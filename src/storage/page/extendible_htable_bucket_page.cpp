//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_htable_bucket_page.cpp
//
// Identification: src/storage/page/extendible_htable_bucket_page.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <optional>
#include <utility>

#include "common/exception.h"
#include "storage/page/extendible_htable_bucket_page.h"

namespace bustub {

template <typename K, typename V, typename KC>
void ExtendibleHTableBucketPage<K, V, KC>::Init(uint32_t max_size) {  //初始化一个桶页面
  max_size_ = max_size;
  size_ = 0;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::Lookup(const K &key, V &value, const KC &cmp) const -> bool { //在桶页中查找给定键值的项
  int size = Size();
  if (size == 0) {
    value = {};
    return false;
  }

  uint32_t idx = KeyIndex(key, cmp);
  if (idx >= Size()) {
    return false;
  }

  if (cmp(key, KeyAt(idx)) == 0) {
    value = ValueAt(idx);
    return true;
  }
  return false;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::Insert(const K &key, const V &value, const KC &cmp) -> bool {
  int size = Size();

  if (IsFull()) {
    return false;
  }

  if (size == 0) {
    InsertAt(0, key, value);
    return true;
  }

  uint32_t idx = KeyIndex(key, cmp);
  if (cmp(key, KeyAt(idx)) == 0) {//如果已经有这个键值对了，就返回false
    return false;
  }

  InsertAt(idx, key, value);
  return true;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::Remove(const K &key, const KC &cmp) -> bool {
  int size = Size();
  if (size == 0) {
    return false;
  }

  uint32_t idx = KeyIndex(key, cmp);
  if (cmp(key, KeyAt(idx)) != 0) {
    return false;
  }

  RemoveAt(idx);
  return true;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::KeyIndex(const K &key, const KC &cmp) const -> uint32_t {//确定给定键在数组中的位置（如果已有此键）或是应放入的位置（本来没有此键）
  int size = Size();

  for (int i = 0; i < size; ++i) {//如果找到第一个大于等于给定键的位置，则返回该位置索引
    if (cmp(key, array_[i].first) <= 0) {
      return i;
    }
  }

  return size; // 如果未找到，则返回 size，表示应该插入到最后
}

template <typename K, typename V, typename KC>
void ExtendibleHTableBucketPage<K, V, KC>::InsertAt(uint32_t idx, const K &key, const V &value) {
  int size = Size();
  for (int i = size - 1; i >= static_cast<int>(idx); i--) {
    array_[i + 1] = array_[i];
  }
  array_[idx] = {key, value};
  size_++;
}

template <typename K, typename V, typename KC>
void ExtendibleHTableBucketPage<K, V, KC>::RemoveAt(uint32_t idx) {
  assert(idx < Size());
  int size = Size();
  for (int i = idx; i < size; i++) {
    array_[i] = array_[i + 1];
  }
  size_--;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::KeyAt(uint32_t idx) const -> K {//返回确定位置的键
  return array_[idx].first;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::ValueAt(uint32_t idx) const -> V {
  return array_[idx].second;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::EntryAt(uint32_t idx) const -> const std::pair<K, V> & {
  return array_[idx];
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::Size() const -> uint32_t {
  return size_;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::IsFull() const -> bool {
  return size_ >= max_size_;
}

template <typename K, typename V, typename KC>
auto ExtendibleHTableBucketPage<K, V, KC>::IsEmpty() const -> bool {
  return size_ == 0;
}

template class ExtendibleHTableBucketPage<int, int, IntComparator>;
template class ExtendibleHTableBucketPage<GenericKey<4>, RID, GenericComparator<4>>;
template class ExtendibleHTableBucketPage<GenericKey<8>, RID, GenericComparator<8>>;
template class ExtendibleHTableBucketPage<GenericKey<16>, RID, GenericComparator<16>>;
template class ExtendibleHTableBucketPage<GenericKey<32>, RID, GenericComparator<32>>;
template class ExtendibleHTableBucketPage<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
