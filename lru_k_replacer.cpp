//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {  
   std::lock_guard<std::mutex> guard(latch_);
  bool target = false;
    if(!hist_list_.empty()){
        for (auto rit = hist_list_.rbegin(); rit != hist_list_.rend(); ++rit){
            if(node_store_[*rit].is_evictable_){
                *frame_id = *rit;
                hist_list_.erase(std::next(rit).base());
                target = true;
                break;
            }
        }
    }

    if(!target && !cache_list_.empty()){
        for (auto rit = cache_list_.rbegin(); rit != cache_list_.rend(); ++rit){
            if(node_store_[*rit].is_evictable_){
                *frame_id = *rit;
                cache_list_.erase(std::next(rit).base());
                target = true;
                break;
            }
        }
    }
    if(target){
        node_store_.erase(*frame_id);
        --curr_size_;
        return true;
    }
    return false;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
     std::lock_guard<std::mutex> guard(latch_);
    if (frame_id > static_cast<int>(replacer_size_)){
    throw std::invalid_argument(std::string("Invalid frame_id ")+ std::to_string(frame_id));
    }
    size_t new_count = ++node_store_[frame_id].hit_count_;
    if(new_count == 1){ 
    ++curr_size_;
    hist_list_.emplace_front(frame_id);
    node_store_[frame_id].pos_= hist_list_.begin();
    }
    else{
        if (new_count == k_){ 
        hist_list_.erase(node_store_[frame_id].pos_);
        cache_list_.emplace_front(frame_id);
        node_store_[frame_id].pos_= cache_list_.begin();
        }
        else if(new_count >k_){ 
            cache_list_.erase(node_store_[frame_id].pos_);
            cache_list_.emplace_front(frame_id);
            node_store_[frame_id].pos_= cache_list_.begin();
        }
    }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
     std::lock_guard<std::mutex> guard(latch_);
  if(frame_id > static_cast<int>(replacer_size_)){
        throw std::invalid_argument(std::string("Invalid frame_id ")+ std::to_string(frame_id));
    }
    if(node_store_.find(frame_id)== node_store_.end()){
        return;
    }
    if (set_evictable && !node_store_[frame_id].is_evictable_){
        ++curr_size_;
    }
    else if (node_store_[frame_id].is_evictable_ && !set_evictable){
        --curr_size_;
    }
    node_store_[frame_id].is_evictable_ = set_evictable;
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::lock_guard<std::mutex> guard(latch_);
  if(node_store_.find(frame_id) == node_store_.end()){
        return;}
    if(!node_store_[frame_id].is_evictable_){
        throw std::logic_error(std::string("Can't remove an inevictable frame ")+ std::to_string(frame_id));
    }
    if (node_store_[frame_id].hit_count_ < k_) 
        {hist_list_.erase(node_store_[frame_id].pos_);}
    else 
        {cache_list_.erase(node_store_[frame_id].pos_);}
    --curr_size_;
    node_store_.erase(frame_id);
}

auto LRUKReplacer::Size() -> size_t { 
    std::lock_guard<std::mutex> guard(latch_);
  return curr_size_;}

}  // namespace bustub