#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <namespace.h>
#include <sframe/span.h>
#include <stdexcept>

#ifdef NO_ALLOC

namespace SFRAME_NAMESPACE {

template<typename T, size_t N>
class vector
{
private:
  std::array<T, N> _data;
  size_t _size;

public:
  constexpr vector()
    : _size(N)
  {
    std::fill(_data.begin(), _data.end(), T());
  }

  constexpr vector(size_t size)
  {
    std::fill(_data.begin(), _data.end(), T());
    resize(size);
  }

  constexpr vector(std::initializer_list<uint8_t> content)
  {
    std::fill(_data.begin(), _data.end(), T());
    resize(content.size());
    std::copy(content.begin(), content.end(), _data.begin());
  }

  constexpr vector(span<const T> content)
  {
    std::fill(_data.begin(), _data.end(), T());
    resize(content.size());
    std::copy(content.begin(), content.end(), _data.begin());
  }

  // XXX(RLB) This constructor seems redundant with the prior one, but for some
  // reason the compiler won't auto-convert from vector to span.
  template<size_t M>
  constexpr vector(const vector<T, M>& content)
  {
    std::fill(_data.begin(), _data.end(), T());
    resize(content.size());
    std::copy(content.begin(), content.end(), _data.begin());
  }

  uint8_t* data() { return _data.data(); }

  auto begin() const { return _data.begin(); }
  auto begin() { return _data.begin(); }

  auto end() const { return _data.begin() + _size; }
  auto end() { return _data.begin() + _size; }

  auto size() const { return _size; }
  auto capacity() const { return N; }
  void resize(size_t size)
  {
    if (size > N) {
      throw std::out_of_range("vector out of space");
    }

    _size = size;
  }

  void push(T&& item)
  {
    resize(_size + 1);
    _data.at(_size - 1) = item;
  }

  void append(span<const T> content)
  {
    const auto start = _size;
    resize(_size + content.size());
    std::copy(content.begin(), content.end(), begin() + start);
  }

  auto& operator[](size_t i) { return _data.at(i); }
  const auto& operator[](size_t i) const { return _data.at(i); }

  operator span<const T>() const { return span<const T>(_data.data(), _size); }
  operator span<T>() { return span<T>(_data.data(), _size); }
};

} // namespace SFRAME_NAMESPACE

#else // ifdef NO_ALLOC

#include <vector>

namespace SFRAME_NAMESPACE {

template<typename T, size_t N>
class vector : private std::vector<T>
{
private:
  using parent = std::vector<T>;

public:
  constexpr vector()
    : parent(N)
  {
  }

  constexpr vector(size_t size)
    : parent(size)
  {
  }

  constexpr vector(span<const T> content)
    : parent(content.begin(), content.end())
  {
  }

  template<size_t M>
  constexpr vector(const vector<T, M>& content)
    : parent(content.begin(), content.end())
  {
  }

  T* data() { return parent::data(); }
  const T* data() const { return parent::data(); }

  auto begin() const { return parent::begin(); }
  auto begin() { return parent::begin(); }

  auto end() const { return parent::end(); }
  auto end() { return parent::end(); }

  auto size() const { return parent::size(); }
  auto capacity() const { return parent::capacity(); }
  void resize(size_t size) { parent::resize(size); }

  auto& operator[](size_t i) { return parent::operator[](i); }
  const auto& operator[](size_t i) const { return parent::operator[](i); }

  void append(span<const T> content)
  {
    const auto start = this->size();
    this->resize(start + content.size());
    std::copy(content.begin(), content.end(), this->begin() + start);
  }

  operator span<const T>() const
  {
    return span<const T>(parent::data(), parent::size());
  }

  operator span<T>() { return span<T>(parent::data(), parent::size()); }
};

} // namespace SFRAME_NAMESPACE

#endif // def NO_ALLOC
