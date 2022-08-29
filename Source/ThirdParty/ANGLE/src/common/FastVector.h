//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FastVector.h:
//   A vector class with a initial fixed size and variable growth.
//   Based on FixedVector.
//

#ifndef COMMON_FASTVECTOR_H_
#define COMMON_FASTVECTOR_H_

#include "bitset_utils.h"
#include "common/debug.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <iterator>

namespace angle
{

template <class Iter>
class WrapIter
{
  public:
    typedef Iter iterator_type;
    typedef typename std::iterator_traits<iterator_type>::value_type value_type;
    typedef typename std::iterator_traits<iterator_type>::difference_type difference_type;
    typedef typename std::iterator_traits<iterator_type>::pointer pointer;
    typedef typename std::iterator_traits<iterator_type>::reference reference;
    typedef typename std::iterator_traits<iterator_type>::iterator_category iterator_category;

    WrapIter() : mIter() {}
    WrapIter(const Iter &iter) : mIter(iter) {}
    ~WrapIter() = default;

    WrapIter &operator=(const WrapIter &x)
    {
        mIter = x.mIter;
        return *this;
    }

    bool operator==(const WrapIter &x) const { return mIter == x.mIter; }
    bool operator!=(const WrapIter &x) const { return mIter != x.mIter; }
    bool operator<(const WrapIter &x) const { return mIter < x.mIter; }
    bool operator<=(const WrapIter &x) const { return mIter <= x.mIter; }
    bool operator>(const WrapIter &x) const { return mIter > x.mIter; }
    bool operator>=(const WrapIter &x) const { return mIter >= x.mIter; }

    WrapIter &operator++()
    {
        mIter++;
        return *this;
    }

    WrapIter operator++(int)
    {
        WrapIter tmp(mIter);
        mIter++;
        return tmp;
    }

    WrapIter operator+(difference_type n)
    {
        WrapIter tmp(mIter);
        tmp.mIter += n;
        return tmp;
    }

    WrapIter operator-(difference_type n)
    {
        WrapIter tmp(mIter);
        tmp.mIter -= n;
        return tmp;
    }

    difference_type operator-(const WrapIter &x) const { return mIter - x.mIter; }

    iterator_type operator->() const { return mIter; }

    reference operator*() const { return *mIter; }

  private:
    iterator_type mIter;
};

template <class T, size_t N, class Storage = std::array<T, N>>
class FastVector final
{
  public:
    using value_type      = typename Storage::value_type;
    using size_type       = typename Storage::size_type;
    using reference       = typename Storage::reference;
    using const_reference = typename Storage::const_reference;
    using pointer         = typename Storage::pointer;
    using const_pointer   = typename Storage::const_pointer;
    using iterator        = WrapIter<T *>;
    using const_iterator  = WrapIter<const T *>;

    FastVector();
    FastVector(size_type count, const value_type &value);
    FastVector(size_type count);

    FastVector(const FastVector<T, N, Storage> &other);
    FastVector(FastVector<T, N, Storage> &&other);
    FastVector(std::initializer_list<value_type> init);

    template <class InputIt, std::enable_if_t<!std::is_integral<InputIt>::value, bool> = true>
    FastVector(InputIt first, InputIt last);

    FastVector<T, N, Storage> &operator=(const FastVector<T, N, Storage> &other);
    FastVector<T, N, Storage> &operator=(FastVector<T, N, Storage> &&other);
    FastVector<T, N, Storage> &operator=(std::initializer_list<value_type> init);

    ~FastVector();

    reference at(size_type pos);
    const_reference at(size_type pos) const;

    reference operator[](size_type pos);
    const_reference operator[](size_type pos) const;

    pointer data();
    const_pointer data() const;

    iterator begin();
    const_iterator begin() const;

    iterator end();
    const_iterator end() const;

    bool empty() const;
    size_type size() const;

    void clear();

    void push_back(const value_type &value);
    void push_back(value_type &&value);

    template <typename... Args>
    void emplace_back(Args &&...args);

    void pop_back();

    reference front();
    const_reference front() const;

    reference back();
    const_reference back() const;

    void swap(FastVector<T, N, Storage> &other);

    void resize(size_type count);
    void resize(size_type count, const value_type &value);

    void reserve(size_type count);

    // Specialty function that removes a known element and might shuffle the list.
    void remove_and_permute(const value_type &element);
    void remove_and_permute(iterator pos);

  private:
    void assign_from_initializer_list(std::initializer_list<value_type> init);
    void ensure_capacity(size_t capacity);
    bool uses_fixed_storage() const;

    Storage mFixedStorage;
    pointer mData           = mFixedStorage.data();
    size_type mSize         = 0;
    size_type mReservedSize = N;
};

template <class T, size_t N, class StorageN, size_t M, class StorageM>
bool operator==(const FastVector<T, N, StorageN> &a, const FastVector<T, M, StorageM> &b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

template <class T, size_t N, class StorageN, size_t M, class StorageM>
bool operator!=(const FastVector<T, N, StorageN> &a, const FastVector<T, M, StorageM> &b)
{
    return !(a == b);
}

template <class T, size_t N, class Storage>
ANGLE_INLINE bool FastVector<T, N, Storage>::uses_fixed_storage() const
{
    return mData == mFixedStorage.data();
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::FastVector()
{}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::FastVector(size_type count, const value_type &value)
{
    ensure_capacity(count);
    mSize = count;
    std::fill(begin(), end(), value);
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::FastVector(size_type count)
{
    ensure_capacity(count);
    mSize = count;
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::FastVector(const FastVector<T, N, Storage> &other)
    : FastVector(other.begin(), other.end())
{}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::FastVector(FastVector<T, N, Storage> &&other) : FastVector()
{
    swap(other);
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::FastVector(std::initializer_list<value_type> init)
{
    assign_from_initializer_list(init);
}

template <class T, size_t N, class Storage>
template <class InputIt, std::enable_if_t<!std::is_integral<InputIt>::value, bool>>
FastVector<T, N, Storage>::FastVector(InputIt first, InputIt last)
{
    size_t newSize = last - first;
    ensure_capacity(newSize);
    mSize = newSize;
    std::copy(first, last, begin());
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage> &FastVector<T, N, Storage>::operator=(
    const FastVector<T, N, Storage> &other)
{
    ensure_capacity(other.mSize);
    mSize = other.mSize;
    std::copy(other.begin(), other.end(), begin());
    return *this;
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage> &FastVector<T, N, Storage>::operator=(FastVector<T, N, Storage> &&other)
{
    swap(other);
    return *this;
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage> &FastVector<T, N, Storage>::operator=(
    std::initializer_list<value_type> init)
{
    assign_from_initializer_list(init);
    return *this;
}

template <class T, size_t N, class Storage>
FastVector<T, N, Storage>::~FastVector()
{
    clear();
    if (!uses_fixed_storage())
    {
        delete[] mData;
    }
}

template <class T, size_t N, class Storage>
typename FastVector<T, N, Storage>::reference FastVector<T, N, Storage>::at(size_type pos)
{
    ASSERT(pos < mSize);
    return mData[pos];
}

template <class T, size_t N, class Storage>
typename FastVector<T, N, Storage>::const_reference FastVector<T, N, Storage>::at(
    size_type pos) const
{
    ASSERT(pos < mSize);
    return mData[pos];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::reference FastVector<T, N, Storage>::operator[](
    size_type pos)
{
    ASSERT(pos < mSize);
    return mData[pos];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::const_reference
FastVector<T, N, Storage>::operator[](size_type pos) const
{
    ASSERT(pos < mSize);
    return mData[pos];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::const_pointer
angle::FastVector<T, N, Storage>::data() const
{
    return mData;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::pointer angle::FastVector<T, N, Storage>::data()
{
    return mData;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::iterator FastVector<T, N, Storage>::begin()
{
    return mData;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::const_iterator FastVector<T, N, Storage>::begin()
    const
{
    return mData;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::iterator FastVector<T, N, Storage>::end()
{
    return mData + mSize;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::const_iterator FastVector<T, N, Storage>::end()
    const
{
    return mData + mSize;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE bool FastVector<T, N, Storage>::empty() const
{
    return mSize == 0;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::size_type FastVector<T, N, Storage>::size() const
{
    return mSize;
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::clear()
{
    resize(0);
}

template <class T, size_t N, class Storage>
ANGLE_INLINE void FastVector<T, N, Storage>::push_back(const value_type &value)
{
    if (mSize == mReservedSize)
        ensure_capacity(mSize + 1);
    mData[mSize++] = value;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE void FastVector<T, N, Storage>::push_back(value_type &&value)
{
    emplace_back(std::move(value));
}

template <class T, size_t N, class Storage>
template <typename... Args>
ANGLE_INLINE void FastVector<T, N, Storage>::emplace_back(Args &&...args)
{
    if (mSize == mReservedSize)
        ensure_capacity(mSize + 1);
    mData[mSize++] = std::move(T(std::forward<Args>(args)...));
}

template <class T, size_t N, class Storage>
ANGLE_INLINE void FastVector<T, N, Storage>::pop_back()
{
    ASSERT(mSize > 0);
    mSize--;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::reference FastVector<T, N, Storage>::front()
{
    ASSERT(mSize > 0);
    return mData[0];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::const_reference FastVector<T, N, Storage>::front()
    const
{
    ASSERT(mSize > 0);
    return mData[0];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::reference FastVector<T, N, Storage>::back()
{
    ASSERT(mSize > 0);
    return mData[mSize - 1];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FastVector<T, N, Storage>::const_reference FastVector<T, N, Storage>::back()
    const
{
    ASSERT(mSize > 0);
    return mData[mSize - 1];
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::swap(FastVector<T, N, Storage> &other)
{
    std::swap(mSize, other.mSize);

    pointer tempData = other.mData;
    if (uses_fixed_storage())
        other.mData = other.mFixedStorage.data();
    else
        other.mData = mData;
    if (tempData == other.mFixedStorage.data())
        mData = mFixedStorage.data();
    else
        mData = tempData;
    std::swap(mReservedSize, other.mReservedSize);

    if (uses_fixed_storage() || other.uses_fixed_storage())
        std::swap(mFixedStorage, other.mFixedStorage);
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::resize(size_type count)
{
    if (count > mSize)
    {
        ensure_capacity(count);
    }
    mSize = count;
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::resize(size_type count, const value_type &value)
{
    if (count > mSize)
    {
        ensure_capacity(count);
        std::fill(mData + mSize, mData + count, value);
    }
    mSize = count;
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::reserve(size_type count)
{
    ensure_capacity(count);
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::assign_from_initializer_list(std::initializer_list<value_type> init)
{
    ensure_capacity(init.size());
    mSize        = init.size();
    size_t index = 0;
    for (auto &value : init)
    {
        mData[index++] = value;
    }
}

template <class T, size_t N, class Storage>
ANGLE_INLINE void FastVector<T, N, Storage>::remove_and_permute(const value_type &element)
{
    size_t len = mSize - 1;
    for (size_t index = 0; index < len; ++index)
    {
        if (mData[index] == element)
        {
            mData[index] = std::move(mData[len]);
            break;
        }
    }
    pop_back();
}

template <class T, size_t N, class Storage>
ANGLE_INLINE void FastVector<T, N, Storage>::remove_and_permute(iterator pos)
{
    ASSERT(pos >= begin());
    ASSERT(pos < end());
    size_t len = mSize - 1;
    *pos       = std::move(mData[len]);
    pop_back();
}

template <class T, size_t N, class Storage>
void FastVector<T, N, Storage>::ensure_capacity(size_t capacity)
{
    // We have a minimum capacity of N.
    if (mReservedSize < capacity)
    {
        ASSERT(capacity > N);
        size_type newSize = std::max(mReservedSize, N);
        while (newSize < capacity)
        {
            newSize *= 2;
        }

        pointer newData = new value_type[newSize];

        if (mSize > 0)
        {
            std::move(begin(), end(), newData);
        }

        if (!uses_fixed_storage())
        {
            delete[] mData;
        }

        mData         = newData;
        mReservedSize = newSize;
    }
}

template <class Value, size_t N>
class FastMap final
{
  public:
    FastMap() {}
    ~FastMap() {}

    Value &operator[](uint32_t key)
    {
        if (mData.size() <= key)
        {
            mData.resize(key + 1, {});
        }
        return mData[key];
    }

    const Value &operator[](uint32_t key) const
    {
        ASSERT(key < mData.size());
        return mData[key];
    }

    void clear() { mData.clear(); }

    bool empty() const { return mData.empty(); }
    size_t size() const { return mData.size(); }

    const Value *data() const { return mData.data(); }

    bool operator==(const FastMap<Value, N> &other) const
    {
        return (size() == other.size()) &&
               (memcmp(data(), other.data(), size() * sizeof(Value)) == 0);
    }

  private:
    FastVector<Value, N> mData;
};

template <class Key, class Value, size_t N>
class FlatUnorderedMap final
{
  public:
    using Pair           = std::pair<Key, Value>;
    using Storage        = FastVector<Pair, N>;
    using iterator       = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;

    FlatUnorderedMap()  = default;
    ~FlatUnorderedMap() = default;

    iterator begin() { return mData.begin(); }
    const_iterator begin() const { return mData.begin(); }
    iterator end() { return mData.end(); }
    const_iterator end() const { return mData.end(); }

    iterator find(const Key &key)
    {
        for (auto it = mData.begin(); it != mData.end(); ++it)
        {
            if (it->first == key)
            {
                return it;
            }
        }
        return mData.end();
    }

    const_iterator find(const Key &key) const
    {
        for (auto it = mData.begin(); it != mData.end(); ++it)
        {
            if (it->first == key)
            {
                return it;
            }
        }
        return mData.end();
    }

    Value &operator[](const Key &key)
    {
        iterator it = find(key);
        if (it != end())
        {
            return it->second;
        }

        mData.push_back(Pair(key, {}));
        return mData.back().second;
    }

    void insert(Pair pair)
    {
        ASSERT(!contains(pair.first));
        mData.push_back(std::move(pair));
    }

    void insert(const Key &key, Value value) { insert(Pair(key, value)); }

    void erase(iterator pos) { mData.remove_and_permute(pos); }

    bool contains(const Key &key) const { return find(key) != end(); }

    void clear() { mData.clear(); }

    bool get(const Key &key, Value *value) const
    {
        auto it = find(key);
        if (it != end())
        {
            *value = it->second;
            return true;
        }
        return false;
    }

    bool empty() const { return mData.empty(); }
    size_t size() const { return mData.size(); }

  private:
    FastVector<Pair, N> mData;
};

template <class T, size_t N>
class FlatUnorderedSet final
{
  public:
    using Storage        = FastVector<T, N>;
    using iterator       = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;

    FlatUnorderedSet()  = default;
    ~FlatUnorderedSet() = default;

    iterator begin() { return mData.begin(); }
    const_iterator begin() const { return mData.begin(); }
    iterator end() { return mData.end(); }
    const_iterator end() const { return mData.end(); }

    iterator find(const T &value)
    {
        for (auto it = mData.begin(); it != mData.end(); ++it)
        {
            if (*it == value)
            {
                return it;
            }
        }
        return mData.end();
    }

    const_iterator find(const T &value) const
    {
        for (auto it = mData.begin(); it != mData.end(); ++it)
        {
            if (*it == value)
            {
                return it;
            }
        }
        return mData.end();
    }

    bool empty() const { return mData.empty(); }

    void insert(const T &value)
    {
        ASSERT(!contains(value));
        mData.push_back(value);
    }

    void erase(const T &value)
    {
        ASSERT(contains(value));
        mData.remove_and_permute(value);
    }

    void remove(const T &value) { erase(value); }

    bool contains(const T &value) const { return find(value) != end(); }

    void clear() { mData.clear(); }

    bool operator==(const FlatUnorderedSet<T, N> &other) const { return mData == other.mData; }

  private:
    Storage mData;
};

class FastIntegerSet final
{
  public:
    static constexpr size_t kWindowSize             = 64;
    static constexpr size_t kOneLessThanKWindowSize = kWindowSize - 1;
    static constexpr size_t kShiftForDivision =
        static_cast<size_t>(rx::Log2(static_cast<unsigned int>(kWindowSize)));
    using KeyBitSet = angle::BitSet64<kWindowSize>;

    ANGLE_INLINE FastIntegerSet();
    ANGLE_INLINE ~FastIntegerSet();

    ANGLE_INLINE void ensureCapacity(size_t size)
    {
        if (capacity() <= size)
        {
            reserve(size * 2);
        }
    }

    ANGLE_INLINE void insert(uint64_t key)
    {
        size_t sizedKey = static_cast<size_t>(key);

        ASSERT(!contains(sizedKey));
        ensureCapacity(sizedKey);
        ASSERT(capacity() > sizedKey);

        size_t index  = sizedKey >> kShiftForDivision;
        size_t offset = sizedKey & kOneLessThanKWindowSize;

        mKeyData[index].set(offset, true);
    }

    ANGLE_INLINE bool contains(uint64_t key) const
    {
        size_t sizedKey = static_cast<size_t>(key);

        size_t index  = sizedKey >> kShiftForDivision;
        size_t offset = sizedKey & kOneLessThanKWindowSize;

        return (sizedKey < capacity()) && (mKeyData[index].test(offset));
    }

    ANGLE_INLINE void clear()
    {
        for (KeyBitSet &it : mKeyData)
        {
            it.reset();
        }
    }

    ANGLE_INLINE bool empty() const
    {
        for (const KeyBitSet &it : mKeyData)
        {
            if (it.any())
            {
                return false;
            }
        }
        return true;
    }

    ANGLE_INLINE size_t size() const
    {
        size_t valid_entries = 0;
        for (const KeyBitSet &it : mKeyData)
        {
            valid_entries += it.count();
        }
        return valid_entries;
    }

  private:
    ANGLE_INLINE size_t capacity() const { return kWindowSize * mKeyData.size(); }

    ANGLE_INLINE void reserve(size_t newSize)
    {
        size_t alignedSize = rx::roundUpPow2(newSize, kWindowSize);
        size_t count       = alignedSize >> kShiftForDivision;

        mKeyData.resize(count, KeyBitSet::Zero());
    }

    std::vector<KeyBitSet> mKeyData;
};

// This is needed to accommodate the chromium style guide error -
//      [chromium-style] Complex constructor has an inlined body.
ANGLE_INLINE FastIntegerSet::FastIntegerSet() {}
ANGLE_INLINE FastIntegerSet::~FastIntegerSet() {}

template <typename Value>
class FastIntegerMap final
{
  public:
    FastIntegerMap() {}
    ~FastIntegerMap() {}

    ANGLE_INLINE void ensureCapacity(size_t size)
    {
        // Ensure key set has capacity
        mKeySet.ensureCapacity(size);

        // Ensure value vector has capacity
        ensureCapacityImpl(size);
    }

    ANGLE_INLINE void insert(uint64_t key, Value value)
    {
        // Insert key
        ASSERT(!mKeySet.contains(key));
        mKeySet.insert(key);

        // Insert value
        size_t sizedKey = static_cast<size_t>(key);
        ensureCapacityImpl(sizedKey);
        ASSERT(capacity() > sizedKey);
        mValueData[sizedKey] = value;
    }

    ANGLE_INLINE bool contains(uint64_t key) const { return mKeySet.contains(key); }

    ANGLE_INLINE bool get(uint64_t key, Value *out) const
    {
        if (!mKeySet.contains(key))
        {
            return false;
        }

        size_t sizedKey = static_cast<size_t>(key);
        *out            = mValueData[sizedKey];
        return true;
    }

    ANGLE_INLINE void clear() { mKeySet.clear(); }

    ANGLE_INLINE bool empty() const { return mKeySet.empty(); }

    ANGLE_INLINE size_t size() const { return mKeySet.size(); }

  private:
    ANGLE_INLINE size_t capacity() const { return mValueData.size(); }

    ANGLE_INLINE void ensureCapacityImpl(size_t size)
    {
        if (capacity() <= size)
        {
            reserve(size * 2);
        }
    }

    ANGLE_INLINE void reserve(size_t newSize)
    {
        size_t alignedSize = rx::roundUpPow2(newSize, FastIntegerSet::kWindowSize);
        mValueData.resize(alignedSize);
    }

    FastIntegerSet mKeySet;
    std::vector<Value> mValueData;
};
}  // namespace angle

#endif  // COMMON_FASTVECTOR_H_
