//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_htable_directory_page.cpp
//
// Identification: src/storage/page/extendible_htable_directory_page.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/extendible_htable_directory_page.h"

#include <algorithm>
#include <unordered_map>

#include "common/config.h"
#include "common/logger.h"

namespace bustub {

void ExtendibleHTableDirectoryPage::Init(uint32_t max_depth) {//初始化目录表
  max_depth_ = max_depth;
  global_depth_ = 0;
  for(uint64_t i=0;i<HTABLE_DIRECTORY_ARRAY_SIZE;++i)
  {
    bucket_page_ids_[i]=INVALID_PAGE_ID;
    local_depths_[i]=0;
  }
}

auto ExtendibleHTableDirectoryPage::HashToBucketIndex(uint32_t hash) const -> uint32_t {//将哈希值映射到桶索引
  uint32_t mask = GetGlobalDepthMask();
  return hash & mask; //通过与全局深度掩码进行按位与操作，可以得到哈希值对应的桶索引。
}

auto ExtendibleHTableDirectoryPage::GetBucketPageId(uint32_t bucket_idx) const -> page_id_t {
  return bucket_page_ids_[bucket_idx];
}

void ExtendibleHTableDirectoryPage::SetBucketPageId(uint32_t bucket_idx, page_id_t bucket_page_id) {
  bucket_page_ids_[bucket_idx] = bucket_page_id;
}

auto ExtendibleHTableDirectoryPage::GetSplitImageIndex(uint32_t bucket_idx) const -> uint32_t {//分裂时会产生2个桶，要从原有桶索引到另一个分裂的桶
  uint32_t split_idx = bucket_idx ^ (1 << (local_depths_[bucket_idx] - 1));
  return split_idx;
}

auto ExtendibleHTableDirectoryPage::GetGlobalDepthMask() const -> uint32_t { return (1 << global_depth_) - 1; };//返回全局深度的掩码，全局深度代表的后几位为1，其余全为0

auto ExtendibleHTableDirectoryPage::GetGlobalDepth() const -> uint32_t { return global_depth_; }

auto ExtendibleHTableDirectoryPage::GetMaxDepth() const -> uint32_t { return max_depth_; };

void ExtendibleHTableDirectoryPage::IncrGlobalDepth() {
  if (global_depth_ >= max_depth_) {
    return;
  }
  for (int i = 0; i < 1 << global_depth_; i++) {             //全局深度增加之后，会新生成可以存放桶id的位置，把原有位置对应赋值给新位置，使他们都先指向对应的原有的桶
    bucket_page_ids_[(1 << global_depth_) + i] = bucket_page_ids_[i];
    local_depths_[(1 << global_depth_) + i] = local_depths_[i];
  }
  global_depth_++;
}

void ExtendibleHTableDirectoryPage::DecrGlobalDepth() {
  if (global_depth_ <= 0) {
    return;
  }
  global_depth_--;
}

auto ExtendibleHTableDirectoryPage::CanShrink() -> bool {  //检查哈希表是否可以缩小。如果所有桶的局部深度都小于全局深度，则可以进行缩小。
  for (uint32_t i = 0; i < Size(); ++i) {
    if (local_depths_[i] >= global_depth_) {
      return false; // 只要有一个桶的局部深度不小于全局深度，就返回 false
    }
  }
  return true; // 如果所有桶的局部深度都小于全局深度，则返回 true
}

auto ExtendibleHTableDirectoryPage::Size() const -> uint32_t { return 1 << global_depth_; }

auto ExtendibleHTableDirectoryPage::GetLocalDepth(uint32_t bucket_idx) const -> uint32_t {
  return local_depths_[bucket_idx];
}

void ExtendibleHTableDirectoryPage::SetLocalDepth(uint32_t bucket_idx, uint8_t local_depth) {
  local_depths_[bucket_idx] = local_depth;
}

void ExtendibleHTableDirectoryPage::IncrLocalDepth(uint32_t bucket_idx) {
  if (local_depths_[bucket_idx] >= max_depth_) {
    return;
  }
  local_depths_[bucket_idx]++;
}

void ExtendibleHTableDirectoryPage::DecrLocalDepth(uint32_t bucket_idx) {
  if (local_depths_[bucket_idx] == 0) {
    return;
  }
  local_depths_[bucket_idx]--;
}

auto ExtendibleHTableDirectoryPage::MaxSize() const -> uint32_t { return 1 << max_depth_; }
}  // namespace bustub
