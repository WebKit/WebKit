#pragma once

#include <cstddef>
#include <utility>

#ifdef NO_ALLOC

#include <algorithm>
#include <optional>
#include <sframe/vector.h>
#include <stdexcept>

namespace SFRAME_NAMESPACE {

template<typename K, typename V, size_t N>
class map : private vector<std::optional<std::pair<K, V>>, N>
{
public:
  template<class... Args>
  void emplace(Args&&... args)
  {
    const auto pos = std::find_if(
      this->begin(), this->end(), [&](const auto& pair) { return !pair; });
    if (pos == this->end()) {
      throw std::out_of_range("map out of space");
    }

    pos->emplace(args...);
  }

  auto find(const K& key)
  {
    return std::find_if(this->begin(), this->end(), [&](const auto& pair) {
      return pair && pair.value().first == key;
    });
  }

  auto find(const K& key) const
  {
    return std::find_if(this->begin(), this->end(), [&](const auto& pair) {
      return pair && pair.value().first == key;
    });
  }

  bool contains(const K& key) const { return find(key) != this->end(); }

  const V& at(const K& key) const
  {
    const auto pos = find(key);
    if (pos == this->end()) {
      throw std::out_of_range("map key not found");
    }

    return pos->value().second;
  }

  V& at(const K& key)
  {
    auto pos = find(key);
    if (pos == this->end()) {
      throw std::out_of_range("map key not found");
    }

    return pos->value().second;
  }

  void erase(const K& key)
  {
    erase_if_key([key](const auto& other) { return other == key; });
  }

  template<typename F>
  void erase_if_key(F&& f)
  {
    const auto to_erase = [&f](const auto& maybe_pair) {
      return maybe_pair && f(maybe_pair.value().first);
    };

    std::replace_if(this->begin(), this->end(), to_erase, std::nullopt);
  }
};

} // namespace SFRAME_NAMESPACE

#else // ifdef NO_ALLOC

#include <map>
#include <namespace.h>

namespace SFRAME_NAMESPACE {

template<typename K, typename V, size_t N>
class map : private std::map<K, V>
{
private:
  using parent = std::map<K, V>;

public:
  template<class... Args>
  void emplace(Args&&... args)
  {
    parent::emplace(std::forward<Args>(args)...);
  }

  auto find(const K& key) { return parent::find(key); }
  auto find(const K& key) const { return parent::find(key); }

  bool contains(const K& key) const { return this->count(key) > 0; }

  const V& at(const K& key) const { return parent::at(key); }
  V& at(const K& key) { return parent::at(key); }

  void erase(const K& key) { parent::erase(key); }

  template<typename F>
  void erase_if_key(F&& f)
  {
    for (auto iter = parent::begin(); iter != parent::end();) {
      if (f(iter->first)) {
        iter = parent::erase(iter);
      } else {
        ++iter;
      }
    }
  }
};

} // namespace SFRAME_NAMESPACE

#endif // def NO_ALLOC
