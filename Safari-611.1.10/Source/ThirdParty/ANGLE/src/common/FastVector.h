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

namespace angle
{
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
    using iterator        = T *;
    using const_iterator  = const T *;

    FastVector();
    FastVector(size_type count, const value_type &value);
    FastVector(size_type count);

    FastVector(const FastVector<T, N, Storage> &other);
    FastVector(FastVector<T, N, Storage> &&other);
    FastVector(std::initializer_list<value_type> init);

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

    void pop_back();

    reference front();
    const_reference front() const;

    reference back();
    const_reference back() const;

    void swap(FastVector<T, N, Storage> &other);

    void resize(size_type count);
    void resize(size_type count, const value_type &value);

    // Specialty function that removes a known element and might shuffle the list.
    void remove_and_permute(const value_type &element);

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
{
    ensure_capacity(other.mSize);
    mSize = other.mSize;
    std::copy(other.begin(), other.end(), begin());
}

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
    swap(*this, other);
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
    if (mSize == mReservedSize)
        ensure_capacity(mSize + 1);
    mData[mSize++] = std::move(value);
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

template <class Key, class Value, size_t N>
class FastUnorderedMap final
{
  public:
    using Pair = std::pair<Key, Value>;

    FastUnorderedMap() {}
    ~FastUnorderedMap() {}

    void insert(Key key, Value value)
    {
        ASSERT(!contains(key));
        mData.push_back(Pair(key, value));
    }

    bool contains(Key key) const
    {
        for (size_t index = 0; index < mData.size(); ++index)
        {
            if (mData[index].first == key)
                return true;
        }
        return false;
    }

    void clear() { mData.clear(); }

    bool get(Key key, Value *value) const
    {
        for (size_t index = 0; index < mData.size(); ++index)
        {
            const Pair &item = mData[index];
            if (item.first == key)
            {
                *value = item.second;
                return true;
            }
        }
        return false;
    }

    bool empty() const { return mData.empty(); }
    size_t size() const { return mData.size(); }

  private:
    FastVector<Pair, N> mData;
};

template <class T, size_t N>
class FastUnorderedSet final
{
  public:
    FastUnorderedSet() {}
    ~FastUnorderedSet() {}

    bool empty() const { return mData.empty(); }

    void insert(T value)
    {
        ASSERT(!contains(value));
        mData.push_back(value);
    }

    bool contains(T needle) const
    {
        for (T value : mData)
        {
            if (value == needle)
                return true;
        }
        return false;
    }

    void clear() { mData.clear(); }

  private:
    FastVector<T, N> mData;
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

    ANGLE_INLINE void clear() { mKeyData.assign(mKeyData.capacity(), KeyBitSet::Zero()); }

    ANGLE_INLINE bool empty() const
    {
        for (KeyBitSet it : mKeyData)
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
        for (KeyBitSet it : mKeyData)
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
