#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/comparison_expression.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/seq_scan_plan.h"
#include "optimizer/optimizer.h"
#include "execution/expressions/logic_expression.h"

namespace bustub {
/*
auto Optimizer::OptimizeSeqScanAsIndexScan(const bustub::AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // TODO(student): implement seq scan with predicate -> index scan optimizer rule
  // The Filter Predicate Pushdown has been enabled for you in optimizer.cpp when forcing starter rule
  return plan;
}*/

auto Optimizer::OptimizeSeqScanAsIndexScan(const bustub::AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  // 创建一个存储优化后子节点的容器
  std::vector<AbstractPlanNodeRef> children;
  // 创建储存有逻辑连接情况下的那些逻辑表达式中子节点的优化节点
  std::vector<AbstractPlanNodeRef> logic_children;
  // 遍历计划的所有子节点，并对每个子节点进行优化
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeSeqScanAsIndexScan(child));
  }
  // 使用优化后的子节点重新克隆优化后的计划
  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  // 如果优化后的计划是SeqScanPlanNode类型
  if (optimized_plan->GetType() == PlanType::SeqScan) {
    // 获取SeqScanPlanNode节点
    const auto &seq_scan = dynamic_cast<const SeqScanPlanNode &>(*optimized_plan);
    // 如果SeqScan节点具有过滤谓词
    if (seq_scan.filter_predicate_ != nullptr) {
      // 获取逻辑表达式
      const auto *logic_expr = dynamic_cast<const LogicExpression *>(seq_scan.filter_predicate_.get());

      //判断逻辑表达式是否他们的子表达式都是针对同一列的
      // 如果是逻辑连接操作
      if (logic_expr != nullptr && logic_expr->logic_type_ == LogicType::Or) {
        // 获取第一个子表达式
        const auto *first_cmp_expr = dynamic_cast<const ComparisonExpression *>(logic_expr->children_[0].get());
        if (first_cmp_expr != nullptr) {
          // 获取第一个子表达式涉及的列
          auto *first_column_value_expr = dynamic_cast<ColumnValueExpression *>(first_cmp_expr->children_[0].get());
          if (first_column_value_expr == nullptr) {
            first_column_value_expr = dynamic_cast<ColumnValueExpression *>(first_cmp_expr->children_[1].get());
          }
          if (first_column_value_expr != nullptr) {
            uint32_t first_col_idx = first_column_value_expr->GetColIdx();

            // 遍历所有子表达式，检查是否涉及同一列
            for (size_t i = 0; i < logic_expr->children_.size(); ++i) {
              const auto *cmp_expr = dynamic_cast<const ComparisonExpression *>(logic_expr->children_[i].get());
              if (cmp_expr != nullptr) {
                auto *column_value_expr = dynamic_cast<ColumnValueExpression *>(cmp_expr->children_[0].get());
                if (column_value_expr == nullptr) {
                  column_value_expr = dynamic_cast<ColumnValueExpression *>(cmp_expr->children_[1].get());
                }
                if (column_value_expr != nullptr) {
                  uint32_t col_idx = column_value_expr->GetColIdx();
                  // 如果子表达式涉及的列和第一个子表达式涉及的列不同，直接返回原始计划
                  if (col_idx != first_col_idx) {
                    return optimized_plan;
                  }
                }
              }
            }
          }
        }
      }

      // 如果是逻辑连接操作
      if (logic_expr != nullptr && logic_expr->logic_type_ == LogicType::Or) {
        // 遍历所有子表达式
        for (const auto &child_expr : logic_expr->children_) {
          // 如果子表达式是比较表达式
          const auto *cmp_expr = dynamic_cast<const ComparisonExpression *>(child_expr.get());
          if (cmp_expr != nullptr && cmp_expr->comp_type_ == ComparisonType::Equal) {
            // 获取表信息
            const auto *table_info = catalog_.GetTable(seq_scan.GetTableOid());
            // 获取表的所有索引
            const auto indices = catalog_.GetTableIndexes(table_info->name_);
            // 获取过滤列的值表达式
            auto *column_value_expr = dynamic_cast<ColumnValueExpression *>(cmp_expr->children_[0].get());
            // 如果过滤列的值表达式为空，尝试交换操作数（对应实现1=v1这样的逻辑）
            if (column_value_expr == nullptr) {
              column_value_expr = dynamic_cast<ColumnValueExpression *>(cmp_expr->children_[1].get());
            }

            if (column_value_expr != nullptr) {
              // 遍历所有索引
              for (const auto *index : indices) {
                // 获取索引关联的列
                const auto &columns = index->index_->GetKeyAttrs();
                // 设置过滤列的id
                std::vector<uint32_t> filter_column_ids = {column_value_expr->GetColIdx()};
                // 如果过滤列和索引的列一致
                if (filter_column_ids == columns) {
                  // 在logic_children中加上这个子节点的IndexScanPlanNode节点
                  logic_children.emplace_back(std::make_shared<IndexScanPlanNode>(optimized_plan->output_schema_, table_info->oid_,
                                                           index->index_oid_, seq_scan.filter_predicate_));
                  break;
                }
              }
            }
          }
        }
        auto optimized_plan_node = plan->CloneWithChildren(std::move(logic_children));
        return optimized_plan_node;
      } else if (logic_expr == nullptr) { // 如果过滤谓词不是逻辑连接操作
        // 如果过滤谓词是单个比较表达式
        const auto *cmp_expr = dynamic_cast<const ComparisonExpression *>(seq_scan.filter_predicate_.get());
        if (cmp_expr != nullptr && cmp_expr->comp_type_ == ComparisonType::Equal) {
          // 获取表信息
          const auto *table_info = catalog_.GetTable(seq_scan.GetTableOid());
          // 获取表的所有索引
          const auto indices = catalog_.GetTableIndexes(table_info->name_);
          // 获取过滤列的值表达式
          auto *column_value_expr = dynamic_cast<ColumnValueExpression *>(cmp_expr->children_[0].get());
            // 如果过滤列的值表达式为空，尝试交换操作数（对应实现1=v1这样的逻辑）
          if (column_value_expr == nullptr) {
            column_value_expr = dynamic_cast<ColumnValueExpression *>(cmp_expr->children_[1].get());
          }

          if (column_value_expr != nullptr) {
            // 遍历所有索引
            for (const auto *index : indices) {
              // 获取索引关联的列
              const auto &columns = index->index_->GetKeyAttrs();
              // 设置过滤列的id
              std::vector<uint32_t> filter_column_ids = {column_value_expr->GetColIdx()};
              // 如果过滤列和索引的列一致
              if (filter_column_ids == columns) {
                // 返回一个新的IndexScanPlanNode节点
                return std::make_shared<IndexScanPlanNode>(optimized_plan->output_schema_, table_info->oid_,
                                                           index->index_oid_, seq_scan.filter_predicate_);
              }
            }
          }
        }
      }
    }
  }
  // 返回优化后的计划
  return optimized_plan;
}

}  // namespace bustub
