//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_extendible_hash_table.cpp
//
// Identification: src/container/disk/hash/disk_extendible_hash_table.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "common/rid.h"
#include "common/util/hash_util.h"
#include "container/disk/hash/disk_extendible_hash_table.h"
#include "storage/index/hash_comparator.h"
#include "storage/page/extendible_htable_bucket_page.h"
#include "storage/page/extendible_htable_directory_page.h"
#include "storage/page/extendible_htable_header_page.h"
#include "storage/page/page_guard.h"

namespace bustub {

template <typename K, typename V, typename KC>
DiskExtendibleHashTable<K, V, KC>::DiskExtendibleHashTable(const std::string &name, BufferPoolManager *bpm,
                                                           const KC &cmp, const HashFunction<K> &hash_fn,
                                                           uint32_t header_max_depth, uint32_t directory_max_depth,
                                                           uint32_t bucket_max_size)
    : bpm_(bpm),
      cmp_(cmp),
      hash_fn_(std::move(hash_fn)),
      header_max_depth_(header_max_depth),
      directory_max_depth_(directory_max_depth),
      bucket_max_size_(bucket_max_size) {
  BasicPageGuard header_guard = bpm_->NewPageGuarded(&header_page_id_);  //这个构造函数创建了一个空的可扩展哈希表，其中暂时只包含一个header表，这三行就是在初始化一个header表
  auto header = header_guard.AsMut<ExtendibleHTableHeaderPage>();
  header->Init(header_max_depth_);
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::GetValue(const K &key, std::vector<V> *result, Transaction *transaction) const
    -> bool {
  if (header_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  //创建header表的临时变量
  ReadPageGuard header_guard = bpm_->FetchPageRead(header_page_id_);
  auto header = header_guard.As<ExtendibleHTableHeaderPage>();

  // 得到相应目录表的id
  uint32_t directory_idx = header->HashToDirectoryIndex(Hash(key));
  page_id_t directory_page_id = header->GetDirectoryPageId(directory_idx);
  header_guard.Drop();
  if (directory_page_id == INVALID_PAGE_ID) {
    return false;
  }
  //创建此目录表的临时变量
  ReadPageGuard directory_guard = bpm_->FetchPageRead(directory_page_id);
  auto directory = directory_guard.As<ExtendibleHTableDirectoryPage>();

  //得到桶表id
  uint32_t bucket_idx = directory->HashToBucketIndex(Hash(key));
  page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);
  directory_guard.Drop();
  if (bucket_page_id == INVALID_PAGE_ID) {
    return false;
  }
  //创建桶表的临时变量
  ReadPageGuard bucket_guard = bpm_->FetchPageRead(bucket_page_id);
  auto bucket = bucket_guard.As<ExtendibleHTableBucketPage<K, V, KC>>();

  // look up the key in the bucket
  V value;
  if (bucket->Lookup(key, value, cmp_)) {
    result->push_back(value);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Insert(const K &key, const V &value, Transaction *transaction) -> bool {
  if (header_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  WritePageGuard header_guard = bpm_->FetchPageWrite(header_page_id_);
  auto header = header_guard.AsMut<ExtendibleHTableHeaderPage>();

  //得到目录表id
  uint32_t directory_idx = header->HashToDirectoryIndex(Hash(key));
  page_id_t directory_page_id = header->GetDirectoryPageId(directory_idx);
  
  //如果这个目录表还没创建，就创建一下，并把header表当中的directory_page_id[]数组更新一下
  if (directory_page_id == INVALID_PAGE_ID) {
    BasicPageGuard directory_guard = bpm_->NewPageGuarded(&directory_page_id);
    auto directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
    directory->Init(directory_max_depth_);
    header->SetDirectoryPageId(directory_idx, directory_page_id);
  }
  header_guard.Drop();

  //取出目录表
  WritePageGuard directory_guard = bpm_->FetchPageWrite(directory_page_id);
  auto directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();

  //得到桶表的id
  uint32_t bucket_idx = directory->HashToBucketIndex(Hash(key));
  page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);

  //如果桶表不存在，则创建一下，并且在目录表中更新bucket_page_id[]数组和其局部深度
  if (bucket_page_id == INVALID_PAGE_ID) {
    BasicPageGuard bucket_guard = bpm_->NewPageGuarded(&bucket_page_id);
    auto bucket = bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
    bucket->Init(bucket_max_size_);
    directory->SetBucketPageId(bucket_idx, bucket_page_id);
    directory->SetLocalDepth(bucket_idx, 0);
  }

  //取出桶表
  WritePageGuard bucket_guard = bpm_->FetchPageWrite(bucket_page_id);
  auto bucket = bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();

  //如果bucket中已经由这个键对应的键值对了，就返回false（项目中有说明键是唯一的）
  V v;
  if (bucket->Lookup(key, v, cmp_)) {
    return false;
  }

  // 如果桶已经满了就分裂
  if (bucket->IsFull()) {
    //如果局部深度已经等于全局深度了，那就要增加全局深度，然后再把相应的局部深度增加
    if (directory->GetLocalDepth(bucket_idx) == directory->GetGlobalDepth()) {
      if (directory->GetGlobalDepth() >= directory->GetMaxDepth()) {
        return false;
      }
      directory->IncrGlobalDepth();
    }
    //把相应的局部深度增加，用于分裂
    directory->IncrLocalDepth(bucket_idx);

    //进行桶的分裂
    if (!SplitBucket(directory, bucket, bucket_idx)) {
      return false;
    }

    directory_guard.Drop();
    bucket_guard.Drop();
    return Insert(key, value, transaction);
  }
  
  //桶没满就直接插入即可
  return bucket->Insert(key, value, cmp_);
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewDirectory(ExtendibleHTableHeaderPage *header, uint32_t directory_idx,
                                                             uint32_t hash, const K &key, const V &value) -> bool {
  // 获取新的目录页ID
  page_id_t directory_page_id = INVALID_PAGE_ID;
  BasicPageGuard directory_guard = bpm_->NewPageGuarded(&directory_page_id);
  if (directory_page_id == INVALID_PAGE_ID) {
    return false;
  }
  // 初始化目录页并更新 header 表中的directory_page_id[]列表
  auto directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
  directory->Init(directory_max_depth_);
  header->SetDirectoryPageId(directory_idx, directory_page_id);
  return true;
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewBucket(ExtendibleHTableDirectoryPage *directory, uint32_t bucket_idx,
                                                          const K &key, const V &value) -> bool {
  // 获取新的桶页ID
  page_id_t bucket_page_id = INVALID_PAGE_ID;
  BasicPageGuard bucket_guard = bpm_->NewPageGuarded(&bucket_page_id);
  if (bucket_page_id == INVALID_PAGE_ID) {
    return false;
  }
  // 初始化桶页并更新目录表中的bucket_page_id[]列表
  auto bucket = bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
  bucket->Init(bucket_max_size_);
  directory->SetBucketPageId(bucket_idx, bucket_page_id);
  directory->SetLocalDepth(bucket_idx, 0);
  return true;
}

template <typename K, typename V, typename KC>
void DiskExtendibleHashTable<K, V, KC>::UpdateDirectoryMapping(ExtendibleHTableDirectoryPage *directory,
                                                               uint32_t new_bucket_idx, page_id_t new_bucket_page_id,
                                                               uint32_t new_local_depth, uint32_t local_depth_mask) {
  // 更新目录表中的映射
  directory->SetBucketPageId(new_bucket_idx, new_bucket_page_id);
  directory->SetLocalDepth(new_bucket_idx, new_local_depth);
}

//添加的用于桶分裂的函数
template <typename K, typename V, typename KC>
  auto DiskExtendibleHashTable<K, V, KC>::SplitBucket(ExtendibleHTableDirectoryPage *directory, ExtendibleHTableBucketPage<K, V, KC> *bucket,
                   uint32_t bucket_idx) -> bool {
    //创建新分裂的桶
    page_id_t split_page_id = INVALID_PAGE_ID;
    BasicPageGuard split_bucket_guard = bpm_->NewPageGuarded(&split_page_id);
    if (split_page_id == INVALID_PAGE_ID) {
      return false;
    }
    auto split_bucket = split_bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
    split_bucket->Init();

    //获取新分裂桶的索引并更新目录表
    uint32_t split_idx = directory->GetSplitImageIndex(bucket_idx);
    uint32_t local_depth = directory->GetLocalDepth(bucket_idx);
    directory->SetBucketPageId(split_idx, split_page_id);
    directory->SetLocalDepth(split_idx, local_depth);

    // 重新分配原有桶中的内容
    page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);
    if (bucket_page_id == INVALID_PAGE_ID) {
      return false;
    }
    //获取原有桶中的内容
    int size = bucket->Size();
    std::list<std::pair<K, V>> entries;
    for (int i = 0; i < size; i++) {
      entries.push_back(bucket->EntryAt(i));
    }
    bucket->Clear();
    //重新分配
    for (const auto &entry : entries) {
      uint32_t target_idx = directory->HashToBucketIndex(Hash(entry.first));
      page_id_t target_page_id = directory->GetBucketPageId(target_idx);
      if (target_page_id == bucket_page_id) {
        bucket->Insert(entry.first, entry.second, cmp_);
      } else if (target_page_id == split_page_id) {
        split_bucket->Insert(entry.first, entry.second, cmp_);
      }
    }
    return true;
  }

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Remove(const K &key, Transaction *transaction) -> bool {
  if (header_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  WritePageGuard header_guard = bpm_->FetchPageWrite(header_page_id_);
  auto header = header_guard.AsMut<ExtendibleHTableHeaderPage>();

  //获取相应的目录表id，并取出
  uint32_t directory_idx = header->HashToDirectoryIndex(Hash(key));
  page_id_t directory_page_id = header->GetDirectoryPageId(directory_idx);
  header_guard.Drop();
  if (directory_page_id == INVALID_PAGE_ID) {
    return false;
  }
  WritePageGuard directory_guard = bpm_->FetchPageWrite(directory_page_id);
  auto directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();

  //获取相应的桶id，并取出
  uint32_t bucket_idx = directory->HashToBucketIndex(Hash(key));
  page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);
  if (bucket_page_id == INVALID_PAGE_ID) {
    return false;
  }
  WritePageGuard bucket_guard = bpm_->FetchPageWrite(bucket_page_id);
  auto bucket = bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();

  //把桶中相应元素删除
  if (!bucket->Remove(key, cmp_)) {
    return false;
  }

  //可能需要桶合并
  MaybeMergeBucket(directory, bucket, bucket_idx);
  while (directory->CanShrink()) {
    directory->DecrGlobalDepth();
  }
  return true;
}

//添加的用于合并桶的函数
template <typename K, typename V, typename KC>
  void DiskExtendibleHashTable<K, V, KC>::MaybeMergeBucket(ExtendibleHTableDirectoryPage *directory, ExtendibleHTableBucketPage<K, V, KC> *bucket,
                        uint32_t bucket_idx) {
    //循环进行合并（如果合并后还能合并，那就继续合并）
    while (true) {
      if (directory->GetLocalDepth(bucket_idx) == 0) {
        return;
      }
      uint32_t split_idx = directory->GetSplitImageIndex(bucket_idx);
      page_id_t split_page_id = directory->GetBucketPageId(split_idx);

      //任务要求：只有具有相同的局部深度时，才能将桶与分裂桶合并。
      if (directory->GetLocalDepth(split_idx) != directory->GetLocalDepth(bucket_idx)) {
        return;
      }

      WritePageGuard split_bucket_guard = bpm_->FetchPageWrite(split_page_id);
      auto split_bucket = split_bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();

      //任务要求：只能合并空桶
      if (!bucket->IsEmpty() && !split_bucket->IsEmpty()) {
        return;
      }

      int size = split_bucket->Size();
      for (int i = 0; i < size; i++) {
        std::pair<K, V> entry = split_bucket->EntryAt(i);
        bucket->Insert(entry.first, entry.second, cmp_);
      }
      split_bucket->Clear();
      split_bucket_guard.Drop();

      //修改原来存放分裂桶位置的相关信息
      page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);
      directory->DecrLocalDepth(bucket_idx);
      uint32_t local_depth = directory->GetLocalDepth(bucket_idx);
      directory->SetBucketPageId(split_idx, bucket_page_id);
      directory->SetLocalDepth(split_idx, local_depth);
    }
  }

template class DiskExtendibleHashTable<int, int, IntComparator>;
template class DiskExtendibleHashTable<GenericKey<4>, RID, GenericComparator<4>>;
template class DiskExtendibleHashTable<GenericKey<8>, RID, GenericComparator<8>>;
template class DiskExtendibleHashTable<GenericKey<16>, RID, GenericComparator<16>>;
template class DiskExtendibleHashTable<GenericKey<32>, RID, GenericComparator<32>>;
template class DiskExtendibleHashTable<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
