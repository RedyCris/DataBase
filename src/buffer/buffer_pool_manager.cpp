//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"
#include <mutex>

#include "common/exception.h"
#include "common/macros.h"
#include "storage/page/page_guard.h"

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_scheduler_(std::make_unique<DiskScheduler>(disk_manager)), log_manager_(log_manager) {
  // TODO(students): remove this line after you have implemented the buffer pool manager
  // throw NotImplementedException(
  //     "BufferPoolManager is not implemented yet. If you have finished implementing BPM, please remove the throw "
  //     "exception line in `buffer_pool_manager.cpp`.");

  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  Page *page;
  frame_id_t frame_id = -1;
  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (!replacer_->Evict(&frame_id)) {
      return nullptr;
    }
  }
  page = pages_ + frame_id;
  if (page->IsDirty()) {
    auto promise = disk_scheduler_->CreatePromise();
    auto future = promise.get_future();
    disk_scheduler_->Schedule({true, page->GetData(), page->GetPageId(), std::move(promise)});
    future.get();
    page->is_dirty_ = false;
  }
  *page_id = AllocatePage();
  page_table_.erase(page->GetPageId());
  page->page_id_ = *page_id;
  page_table_.emplace(*page_id, frame_id);
  page->pin_count_ = 1;
  page->ResetMemory();
  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);
  return page;
}

auto BufferPoolManager::FetchPage(page_id_t page_id, [[maybe_unused]] AccessType access_type) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  if (page_id == INVALID_PAGE_ID) {
    return nullptr;
  }
  if (page_table_.find(page_id) != page_table_.end()) {
    frame_id_t frame_id = page_table_[page_id];
    auto page = pages_ + frame_id;
    page->pin_count_++;
    replacer_->RecordAccess(frame_id);
    replacer_->SetEvictable(frame_id, false);
    return page;
  }  // 不存在
  // 分配替换帧->脏页面处理->更新基本数据->读取磁盘数据
  Page *page;
  frame_id_t frame_id = -1;
  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (!replacer_->Evict(&frame_id)) {
      return nullptr;
    }
  }
  page = pages_ + frame_id;
  if (page->IsDirty()) {
    auto promise = disk_scheduler_->CreatePromise();
    auto future = promise.get_future();
    disk_scheduler_->Schedule({true, page->GetData(), page->GetPageId(), std::move(promise)});
    future.get();
    page->is_dirty_ = false;
  }
  page_table_.erase(page->GetPageId());
  page->page_id_ = page_id;
  page_table_.emplace(page_id, frame_id);
  page->pin_count_ = 1;
  page->ResetMemory();
  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);

  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();
  disk_scheduler_->Schedule({false, page->GetData(), page->GetPageId(), std::move(promise)});
  future.get();
  return page;
}

auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty, [[maybe_unused]] AccessType access_type) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  if (page_id == INVALID_PAGE_ID) {
    return false;
  }
  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }
  frame_id_t frame_id = page_table_[page_id];
  auto page = pages_ + frame_id;
  if (is_dirty) {
    page->is_dirty_ = is_dirty;
  }
  if (page->GetPinCount() <= 0) {
    return false;
  }

  page->pin_count_--;
  if (page->GetPinCount() == 0) {
    replacer_->SetEvictable(frame_id, true);
  }
  return true;
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  if (page_id == INVALID_PAGE_ID) {
    return false;
  }
  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }  // flush a page to disk
  frame_id_t frame_id = page_table_[page_id];
  auto page = pages_ + frame_id;
  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();
  disk_scheduler_->Schedule({true, page->GetData(), page->GetPageId(), std::move(promise)});
  future.get();
  page->is_dirty_ = false;
  return true;
}

void BufferPoolManager::FlushAllPages() {
  std::scoped_lock<std::mutex> lock(latch_);
  for (const auto &it : page_table_) {
    if (it.first == INVALID_PAGE_ID) {
      continue;
    }
    auto page = pages_ + it.second;
    auto promise = disk_scheduler_->CreatePromise();
    auto future = promise.get_future();
    disk_scheduler_->Schedule({true, page->GetData(), page->GetPageId(), std::move(promise)});
    future.get();
    page->is_dirty_ = false;
  }
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  if (page_id == INVALID_PAGE_ID) {
    return true;
  }
  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    DeallocatePage(page_id);
    return true;
  }
  frame_id_t frame_id = page_table_[page_id];
  auto page = pages_ + frame_id;
  if (page->GetPinCount() > 0) {
    return false;
  }
  page_table_.erase(page_id);
  replacer_->Remove(frame_id);
  free_list_.push_back(frame_id);
  page->ResetMemory();
  page->pin_count_ = 0;
  page->is_dirty_ = false;
  page->page_id_ = INVALID_PAGE_ID;
  DeallocatePage(page_id);
  return true;
}

auto BufferPoolManager::AllocatePage() -> page_id_t { return next_page_id_++; }

auto BufferPoolManager::FetchPageBasic(page_id_t page_id) -> BasicPageGuard {
  auto page = FetchPage(page_id);
  return {this, page};
}

auto BufferPoolManager::FetchPageRead(page_id_t page_id) -> ReadPageGuard {
  auto page = FetchPage(page_id);
  page->RLatch();
  return {this, page};
}

auto BufferPoolManager::FetchPageWrite(page_id_t page_id) -> WritePageGuard {
  auto page = FetchPage(page_id);
  page->WLatch();
  return {this, page};
}

auto BufferPoolManager::NewPageGuarded(page_id_t *page_id) -> BasicPageGuard {
  auto page = NewPage(page_id);
  return {this, page};
}

}  // namespace bustub
