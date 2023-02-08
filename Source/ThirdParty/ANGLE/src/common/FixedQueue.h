//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FixedQueue.h:
//   An array based fifo queue class that supports concurrent push and pop.
//

#ifndef COMMON_FIXEDQUEUE_H_
#define COMMON_FIXEDQUEUE_H_

#include "common/debug.h"

#include <algorithm>
#include <array>

namespace angle
{
// class FixedQueue: An array based fix storage fifo queue class that supports concurrent push and
// pop. Caller must ensure queue is not empty before pop and not full before push. This class
// supports concurrent push and pop from different threads. If caller want to push from two
// different threads, proper mutex must be used to ensure the access is serialized.
template <class T, size_t N, class Storage = std::array<T, N>>
class FixedQueue final : angle::NonCopyable
{
  public:
    using value_type      = typename Storage::value_type;
    using size_type       = typename Storage::size_type;
    using reference       = typename Storage::reference;
    using const_reference = typename Storage::const_reference;

    class iterator
    {
      public:
        iterator() : mData(nullptr), mIndex(-1) {}

        T &operator*() const { return mData[mIndex % N]; }
        T *operator->() const { return &mData[mIndex % N]; }

        bool operator==(const iterator &rhs) const { return mIndex == rhs.mIndex; }
        bool operator!=(const iterator &rhs) const { return mIndex != rhs.mIndex; }

        iterator &operator++()
        {
            mIndex++;
            return *this;
        }

      private:
        Storage &mData;
        size_type mIndex;
        friend class FixedQueue<T, N, Storage>;
        iterator(Storage &data, size_type index) : mData(data), mIndex(index) {}
    };
    class const_iterator
    {
      public:
        const_iterator() : mData(nullptr), mIndex(-1) {}

        const T &operator*() const { return mData[mIndex % N]; }
        const T *operator->() const { return &mData[mIndex % N]; }

        bool operator==(const iterator &rhs) const { mIndex == rhs.mIndex; }
        bool operator!=(const iterator &rhs) const { mIndex != rhs.mIndex; }

        const_iterator &operator++()
        {
            mIndex++;
            return *this;
        }

      private:
        const Storage &mData;
        size_type mIndex;
        friend class FixedQueue<T, N, Storage>;
        const_iterator(const Storage &data, size_type index) : mData(data), mIndex(index) {}
    };

    FixedQueue();
    ~FixedQueue();

    size_type size() const;
    bool empty() const;
    bool full() const;

    reference operator[](size_type pos);
    const_reference operator[](size_type pos) const;

    reference front();
    const_reference front() const;

    void push(const value_type &value);
    void push(value_type &&value);

    reference back();
    const_reference back() const;

    void pop();
    void clear();

    iterator begin();
    const_iterator begin() const;

    iterator end();
    const_iterator end() const;

  private:
    Storage mData;
    // The front and back indices are virtual indices (think about queue sizd is infinite). They
    // will never wrap around when hit N. The wrap around occur when element is referenced. Virtual
    // index for current head
    size_type mFrontIndex;
    // Virtual index for next write.
    size_type mBackIndex;
    // Atomic so that we can support concurrent push and pop.
    std::atomic<size_type> mSize;
};

template <class T, size_t N, class Storage>
FixedQueue<T, N, Storage>::FixedQueue() : mFrontIndex(0), mBackIndex(0), mSize(0)
{}

template <class T, size_t N, class Storage>
FixedQueue<T, N, Storage>::~FixedQueue()
{}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FixedQueue<T, N, Storage>::size_type FixedQueue<T, N, Storage>::size() const
{
    return mSize;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE bool FixedQueue<T, N, Storage>::empty() const
{
    return mSize == 0;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE bool FixedQueue<T, N, Storage>::full() const
{
    return mSize >= N;
}

template <class T, size_t N, class Storage>
typename FixedQueue<T, N, Storage>::reference FixedQueue<T, N, Storage>::operator[](size_type pos)
{
    ASSERT(pos < mSize);
    return mData[(pos + mFrontIndex) % N];
}

template <class T, size_t N, class Storage>
typename FixedQueue<T, N, Storage>::const_reference FixedQueue<T, N, Storage>::operator[](
    size_type pos) const
{
    ASSERT(pos < mSize);
    return mData[(pos + mFrontIndex) % N];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FixedQueue<T, N, Storage>::reference FixedQueue<T, N, Storage>::front()
{
    ASSERT(mSize > 0);
    return mData[mFrontIndex % N];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FixedQueue<T, N, Storage>::const_reference FixedQueue<T, N, Storage>::front()
    const
{
    ASSERT(mSize > 0);
    return mData[mFrontIndex % N];
}

template <class T, size_t N, class Storage>
void FixedQueue<T, N, Storage>::push(const value_type &value)
{
    ASSERT(mSize < N);
    mData[mBackIndex % N] = value;
    mBackIndex++;
    // We must increment size last, after we wrote data. That way if another thread is doing
    // `if(!dq.empty()){ s = dq.front(); }`, it will only see not empty until element is fully
    // pushed.
    mSize++;
}

template <class T, size_t N, class Storage>
void FixedQueue<T, N, Storage>::push(value_type &&value)
{
    ASSERT(mSize < N);
    mData[mBackIndex % N] = std::move(value);
    mBackIndex++;
    // We must increment size last, after we wrote data. That way if another thread is doing
    // `if(!dq.empty()){ s = dq.front(); }`, it will only see not empty until element is fully
    // pushed.
    mSize++;
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FixedQueue<T, N, Storage>::reference FixedQueue<T, N, Storage>::back()
{
    ASSERT(mSize > 0);
    return mData[(mBackIndex + (N - 1)) % N];
}

template <class T, size_t N, class Storage>
ANGLE_INLINE typename FixedQueue<T, N, Storage>::const_reference FixedQueue<T, N, Storage>::back()
    const
{
    ASSERT(mSize > 0);
    return mData[(mBackIndex + (N - 1)) % N];
}

template <class T, size_t N, class Storage>
void FixedQueue<T, N, Storage>::pop()
{
    ASSERT(mSize > 0);
    mData[mFrontIndex % N] = value_type();
    mFrontIndex++;
    // We must decrement size last, after we wrote data. That way if another thread is doing
    // `if(!dq.full()){ dq.push; }`, it will only see not full until element is fully popped.
    mSize--;
}

template <class T, size_t N, class Storage>
void FixedQueue<T, N, Storage>::clear()
{
    for (size_type i = 0; i < mSize; i++)
    {
        pop();
    }
}

template <class T, size_t N, class Storage>
typename FixedQueue<T, N, Storage>::iterator FixedQueue<T, N, Storage>::begin()
{
    return iterator(mData, mFrontIndex);
}

template <class T, size_t N, class Storage>
typename FixedQueue<T, N, Storage>::const_iterator FixedQueue<T, N, Storage>::begin() const
{
    return const_iterator(mData, mFrontIndex);
}

template <class T, size_t N, class Storage>
typename FixedQueue<T, N, Storage>::iterator FixedQueue<T, N, Storage>::end()
{
    return iterator(mData, mFrontIndex + mSize);
}

template <class T, size_t N, class Storage>
typename FixedQueue<T, N, Storage>::const_iterator FixedQueue<T, N, Storage>::end() const
{
    return const_iterator(mData, mFrontIndex + mSize);
}
}  // namespace angle

#endif  // COMMON_FIXEDQUEUE_H_
