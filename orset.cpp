#include "primer/orset.h"
#include <algorithm>
#include <string>
#include <vector>
#include "common/exception.h"
#include "fmt/format.h"

namespace bustub {

template <typename T>
auto ORSet<T>::Contains(const T &elem) const -> bool {
  // TODO(student): Implement this
  //throw NotImplementedException("ORSet<T>::Contains is not implemented");
  for (const auto &pair : elements) {
    if (pair.first == elem) {
      
      return true;
    }
  }
  return false;
}

template <typename T>
void ORSet<T>::Add(const T &elem, uid_t uid) {
  // TODO(student): Implement this
  //throw NotImplementedException("ORSet<T>::Add is not implemented");
  elements.emplace_back(elem, uid);
}

template <typename T>
void ORSet<T>::Remove(const T &elem) {
  // TODO(student): Implement this
  //throw NotImplementedException("ORSet<T>::Remove is not implemented");
  std::vector<std::pair<T, uid_t>> removedElements;
  for (auto it = elements.begin(); it != elements.end(); ) {
    if (it->first == elem) {
      removedElements.push_back(*it);
      it = elements.erase(it); // 删除匹配的元素
    } else {
      ++it;
    }
  }

  for (const auto &elem : removedElements) {
    tombstones.emplace_back(elem.first, elem.second); // 将匹配的元素和 uid 放入 tombstones
  }
}

template <typename T>
void ORSet<T>::Merge(const ORSet<T> &other) {
  // TODO(student): Implement this
  //throw NotImplementedException("ORSet<T>::Merge is not implemented");
   for (auto it = elements.begin(); it != elements.end(); ) {
    if (std::find(other.tombstones.begin(), other.tombstones.end(), *it) != other.tombstones.end()) {
      it = elements.erase(it);
    } else {
      ++it;
    }
  }
  
  for (const auto &pair : other.elements) {
    if (std::find(tombstones.begin(), tombstones.end(), pair) == tombstones.end()) {
      elements.push_back(pair);
    }
  }

  for (const auto &pair : other.tombstones) {
    if (std::find(tombstones.begin(), tombstones.end(), pair) == tombstones.end()) {
      tombstones.push_back(pair);
    }
  }
}

template <typename T>
auto ORSet<T>::Elements() const -> std::vector<T> {
  // TODO(student): Implement this
  //throw NotImplementedException("ORSet<T>::Elements is not implemented");
   std::vector<T> result;
  for (const auto &pair : elements) {
    bool isTombstoned = false;
    for (const auto &tomb : tombstones) {
      if (tomb.first == pair.first) {
        isTombstoned = true;
        break;
      }
    }
    if (!isTombstoned) {
      result.push_back(pair.first);
    }
  }
  return result;
}

template <typename T>
auto ORSet<T>::ToString() const -> std::string {
  auto elements = Elements();
  std::sort(elements.begin(), elements.end());
  return fmt::format("{{{}}}", fmt::join(elements, ", "));
}

template class ORSet<int>;
template class ORSet<std::string>;

}  // namespace bustub
