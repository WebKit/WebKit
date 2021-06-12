/*
 * Copyright (C) 2016-2021 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashFunctions.h>
#include <wtf/Noncopyable.h>

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SmallSet);

// Functionally, this class is very similar to Variant<Vector<T, SmallArraySize>, HashSet<T>>
// It is optimized primarily for space, but is also quite fast
// Its main limitation is that it has no way to remove elements once they have been added to it
// Also, instead of being fully parameterized by a HashTrait parameter, it always uses -1 (all ones) as its empty value
// Relatedly, it can only store objects of up to 64 bit size (but that particular limitation should be fairly easy to lift if needed)
// Use it whenever you need to store an unbounded but probably small number of unsigned integers or pointers.
template<typename T, typename Hash = PtrHashBase<T, false /* isSmartPtr */>, unsigned SmallArraySize = 8>
class SmallSet {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(SmallSet);
    static_assert(std::is_trivially_destructible<T>::value, "We currently don't support non-trivially destructible types.");
    static_assert(!(SmallArraySize & (SmallArraySize - 1)), "Inline size must be a power of two.");
    static_assert(sizeof(T*) <= SmallArraySize * sizeof(T), "This class has not been tested for m_inline.buffer larger than m_inline.smallStorage");

public:
    SmallSet()
        : m_inline()
    {
        initialize();
    }

    // We take care to have SmallSet have partial move semantics allowable through
    // memcpy. It's partial move semantics because our destructor should not be called
    // on the SmallPtrObject in the old memory we were moved from (otherwise, we might free m_buffer twice)
    // unless that old memory is reset to be isSmall(). See move constructor below.
    // To maintain these semantics, we determine if we're small by checking our size
    // and not our m_buffer pointer. And when we're small, we don't do operations on
    // m_buffer, instead, we perform operations on m_smallStorage directly. The reason we want
    // these semantics is that it's beneficial to have a Vector that contains SmallSet
    // (or an object with SmallSet as a field) be allowed to use memcpy for its move operation.

    SmallSet(SmallSet&& other)
    {
        memcpy(static_cast<void*>(this), static_cast<void*>(&other), sizeof(SmallSet));
        other.initialize();
    }

    SmallSet& operator=(SmallSet&& other)
    {
        this->~SmallSet();
        new (this) SmallSet(WTFMove(other));
        return *this;
    }

    ~SmallSet()
    {
        if (!isSmall())
            SmallSetMalloc::free(m_inline.buffer);
    }

    // We could easily include an iterator in this to fully match the HashSet interface, but currently none of our clients require it.
    struct AddResult {
        bool isNewEntry;
    };

    inline AddResult add(T value)
    {
        ASSERT(isValidEntry(value));

        if (isSmall()) {
            for (unsigned i = 0; i < m_size; i++) {
                if (m_inline.smallStorage[i] == value)
                    return { false };
            }

            if (m_size < SmallArraySize) {
                m_inline.smallStorage[m_size] = value;
                ++m_size;
                return { true };
            }

            grow(std::max(64u, SmallArraySize * 2));
            // Fall through. We're no longer small :(
        }

        // If we're more than 3/4ths full we grow.
        if (UNLIKELY(m_size * 4 >= m_capacity * 3)) {
            grow(m_capacity * 2);
            ASSERT(!(m_capacity & (m_capacity - 1)));
        }

        T* bucket = this->bucket(value);
        if (*bucket != value) {
            *bucket = value;
            ++m_size;
            return { true };
        }
        return { false };
    }

    inline bool contains(T value) const
    {
        ASSERT(isValidEntry(value));
        if (isSmall()) {
            // We only need to search up to m_size because we store things linearly inside m_smallStorage.
            for (unsigned i = 0; i < m_size; i++) {
                if (m_inline.smallStorage[i] == value)
                    return true;
            }
            return false;
        }

        T* bucket = this->bucket(value);
        return *bucket == value;
    }

    class iterator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        iterator& operator++()
        {
            m_index++;
            ASSERT(m_index <= m_capacity);
            while (m_index < m_capacity && m_buffer[m_index] == emptyValue())
                m_index++;
            return *this;
        }
        
        T operator*() const { ASSERT(m_index < m_capacity); return static_cast<T>(m_buffer[m_index]); }
        bool operator==(const iterator& other) const { ASSERT(m_buffer == other.m_buffer); return m_index == other.m_index; }
        bool operator!=(const iterator& other) const { ASSERT(m_buffer == other.m_buffer); return !(*this == other); }

    private:
        template<typename U, typename H, unsigned S> friend class WTF::SmallSet;
        unsigned m_index;
        unsigned m_capacity;
        T* m_buffer;
    };

    iterator begin() const
    {
        iterator it;
        it.m_index = std::numeric_limits<unsigned>::max();
        it.m_capacity = m_capacity;
        if (isSmall())
            it.m_buffer = const_cast<T*>(m_inline.smallStorage);
        else
            it.m_buffer = m_inline.buffer;

        ++it;

        return it;
    }

    iterator end() const
    {
        iterator it;
        it.m_index = m_capacity;
        it.m_capacity = m_capacity;
        if (isSmall())
            it.m_buffer = const_cast<T*>(m_inline.smallStorage);
        else
            it.m_buffer = m_inline.buffer;

        return it;
    }

    inline unsigned size() const { return m_size; }

    inline bool isEmpty() const { return !size(); }

    unsigned memoryUse() const
    {
        unsigned memory = sizeof(SmallSet);
        if (!isSmall())
            memory += m_capacity * sizeof(T);
        return memory;
    }

private:
    constexpr static T emptyValue()
    {
        if constexpr (std::is_pointer<T>::value)
            return static_cast<T>(bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()));
        return std::numeric_limits<T>::max();
    }

    bool isValidEntry(const T value) const
    {
        return value != emptyValue();
    }

    inline bool isSmall() const
    {
        return m_capacity == SmallArraySize;
    }

    inline void initialize()
    {
        m_size = 0;
        m_capacity = SmallArraySize;
        memset(static_cast<void*>(m_inline.smallStorage), -1, sizeof(T) * SmallArraySize);
        ASSERT(isSmall());
    }

    inline void grow(unsigned size)
    {
        // We memset the new buffer with -1, so for consistency emptyValue() must return something which is all 1s.
#if !defined(NDEBUG)
        if constexpr (std::is_pointer<T>::value)
            ASSERT(bitwise_cast<intptr_t>(emptyValue()) == -1ll);
        else if constexpr (sizeof(T) == 8)
            ASSERT(bitwise_cast<int64_t>(emptyValue()) == -1ll);
        else if constexpr (sizeof(T) == 4)
            ASSERT(bitwise_cast<int32_t>(emptyValue()) == -1);
        else if constexpr (sizeof(T) == 2)
            ASSERT(bitwise_cast<int16_t>(emptyValue()) == -1);
        else if constexpr (sizeof(T) == 1)
            ASSERT(bitwise_cast<int8_t>(emptyValue()) == -1);
        else
            RELEASE_ASSERT_NOT_REACHED();
#endif

        size_t allocationSize = sizeof(T) * size;
        bool wasSmall = isSmall();
        T* oldBuffer = wasSmall ? m_inline.smallStorage : m_inline.buffer;
        unsigned oldCapacity = m_capacity;
        T* newBuffer = static_cast<T*>(SmallSetMalloc::malloc(allocationSize));
        memset(static_cast<void*>(newBuffer), -1, allocationSize);
        m_capacity = size;

        for (unsigned i = 0; i < oldCapacity; i++) {
            if (oldBuffer[i] != emptyValue()) {
                T* ptr = bucketInBuffer(newBuffer, static_cast<T>(oldBuffer[i]));
                *ptr = oldBuffer[i];
            }
        }

        if (!wasSmall)
            SmallSetMalloc::free(oldBuffer);

        m_inline.buffer = newBuffer;
    }


    inline T* bucket(T target) const
    {
        ASSERT(!isSmall());
        return bucketInBuffer(m_inline.buffer, target);
    }

    inline T* bucketInBuffer(T* buffer, T target) const
    {
        ASSERT(!(m_capacity & (m_capacity - 1)));
        unsigned bucket = Hash::hash(target) & (m_capacity - 1);
        unsigned index = 0;
        while (true) {
            T* ptr = buffer + bucket;
            if (*ptr == emptyValue())
                return ptr;
            if (*ptr == target)
                return ptr;
            index++;
            bucket = (bucket + index) & (m_capacity - 1);
        }
    }

    unsigned m_size;
    unsigned m_capacity;
    union U {
        T* buffer;
        T smallStorage[SmallArraySize];
        U() { };
    } m_inline;
};

} // namespace WTF

using WTF::SmallSet;
