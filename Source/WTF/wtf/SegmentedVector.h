/*
 * Copyright (C) 2008, 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Vector.h>

namespace WTF {

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SegmentedVector);

    // An iterator for SegmentedVector. It supports only the pre ++ operator
    template <typename T, size_t SegmentSize, size_t InlineCapacity, SegmentedVectorGrowthPolicy GrowthPolicy, typename Malloc> class SegmentedVector;
    template <typename T, size_t SegmentSize = 8, size_t InlineCapacity = 0, SegmentedVectorGrowthPolicy GrowthPolicy = SegmentedVectorGrowthPolicy::Constant, typename Malloc = SegmentedVectorMalloc> class SegmentedVectorIterator {
        WTF_MAKE_CONFIGURABLE_ALLOCATED(FastMalloc);
    private:
        friend class SegmentedVector<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc>;
    public:
        typedef SegmentedVectorIterator<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc> Iterator;

        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        ~SegmentedVectorIterator() { }

        T& operator*() const { return m_vector.at(m_index); }
        T* operator->() const { return &m_vector.at(m_index); }

        // Only prefix ++ operator supported
        Iterator& operator++()
        {
            m_index++;
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            return m_index == other.m_index && &m_vector == &other.m_vector;
        }

        SegmentedVectorIterator& operator=(const SegmentedVectorIterator<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc>& other)
        {
            m_vector = other.m_vector;
            m_index = other.m_index;
            return *this;
        }

    private:
        SegmentedVectorIterator(SegmentedVector<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc>& vector, size_t index)
            : m_vector(vector)
            , m_index(index)
        {
        }

        SegmentedVector<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc>& m_vector;
        size_t m_index;
    };

    // SegmentedVector is just like Vector, but it doesn't move the values
    // stored in its buffer when it grows. Therefore, it is safe to keep
    // pointers into a SegmentedVector. The default tuning values are
    // optimized for segmented vectors that get large; you may want to use
    // the inline storage option if you don't expect a lot of entries.
    //
    // When InlineCapacity > 0, the first InlineCapacity elements are stored
    // inline within the vector object itself, avoiding heap allocation for
    // small vectors. Additional elements are stored in heap-allocated segments
    // of SegmentSize elements each. Vectors with inline storage are not
    // movable, as moving would invalidate pointers to elements stored inline.
    template <typename T, size_t SegmentSize, size_t InlineCapacity, SegmentedVectorGrowthPolicy GrowthPolicy, typename Malloc>
    class SegmentedVector final {
        friend class SegmentedVectorIterator<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc>;
        WTF_MAKE_NONCOPYABLE(SegmentedVector);
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(SegmentedVector);

        static constexpr bool hasInlineStorage = InlineCapacity > 0;

    public:
        using Iterator = SegmentedVectorIterator<T, SegmentSize, InlineCapacity, GrowthPolicy, Malloc>;

        using value_type = T;
        using iterator = Iterator;

        SegmentedVector() = default;

        ~SegmentedVector()
        {
            destroyAllItems();
        }

        SegmentedVector(SegmentedVector&&) requires (hasInlineStorage) = delete;
        SegmentedVector& operator=(SegmentedVector&&) requires (hasInlineStorage) = delete;

        SegmentedVector(SegmentedVector&& other) requires (!hasInlineStorage)
            : m_size(std::exchange(other.m_size, 0))
            , m_segments(WTF::move(other.m_segments))
        {
        }

        SegmentedVector& operator=(SegmentedVector&& other) requires (!hasInlineStorage)
        {
            destroyAllItems();
            m_segments = WTF::move(other.m_segments);
            m_size = std::exchange(other.m_size, 0);
            return *this;
        }

        size_t size() const { return m_size; }
        bool isEmpty() const { return !size(); }

        ALWAYS_INLINE T& at(size_t index) LIFETIME_BOUND
        {
            ASSERT_WITH_SECURITY_IMPLICATION(index < m_size);
            return *addressAt(index);
        }

        ALWAYS_INLINE const T& at(size_t index) const LIFETIME_BOUND
        {
            return const_cast<SegmentedVector*>(this)->at(index);
        }

        T& operator[](size_t index) LIFETIME_BOUND { return at(index); }
        const T& operator[](size_t index) const LIFETIME_BOUND { return at(index); }

        T& first() LIFETIME_BOUND
        {
            ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
            return at(0);
        }
        const T& first() const LIFETIME_BOUND
        {
            ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
            return at(0);
        }
        ALWAYS_INLINE T& last() LIFETIME_BOUND
        {
            ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
            return at(size() - 1);
        }
        ALWAYS_INLINE const T& last() const LIFETIME_BOUND
        {
            ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
            return at(size() - 1);
        }

        T takeLast()
        {
            ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
            T result = WTF::move(last());
            --m_size;
            return result;
        }

        template<typename... Args>
        ALWAYS_INLINE T& alloc(Args&&... args)
        {
            size_t newIndex = m_size++;
            if constexpr (hasInlineStorage) {
                if (newIndex >= InlineCapacity && !heapSegmentExistsFor(newIndex)) [[unlikely]]
                    allocateSegment();
            } else {
                if (!segmentExistsFor(newIndex))
                    allocateSegment();
            }
            T* ptr = addressAt(newIndex);
            new (NotNull, ptr) T(std::forward<Args>(args)...);
            return *ptr;
        }

        template<typename... Args>
        ALWAYS_INLINE void append(Args&&... args)
        {
            alloc(std::forward<Args>(args)...);
        }

        template<typename... Args>
        ALWAYS_INLINE void constructAndAppend(Args&&... args)
        {
            alloc(std::forward<Args>(args)...);
        }

        ALWAYS_INLINE void removeLast()
        {
            ASSERT_WITH_SECURITY_IMPLICATION(!isEmpty());
            --m_size;
            std::destroy_at(addressAt(m_size));
        }

        void grow(size_t size)
        {
            ASSERT(size > m_size);
            ensureSegmentsFor(size);
            size_t oldSize = m_size;
            m_size = size;
            for (size_t i = oldSize; i < m_size; ++i)
                new (NotNull, addressAt(i)) T();
        }

        void clear()
        {
            destroyAllItems();
            m_segments.clear();
            m_size = 0;
        }

        Iterator begin() LIFETIME_BOUND { return Iterator(*this, 0); }
        Iterator end() LIFETIME_BOUND { return Iterator(*this, m_size); }

        void shrinkToFit() { m_segments.shrinkToFit(); }

        unsigned removeAllMatching(NOESCAPE const Invocable<bool(T&)> auto& matches)
        {
            unsigned writeIndex = 0;
            unsigned matchCount = 0;
            for (unsigned readIndex = 0; readIndex < m_size; ++readIndex) {
                T* readPtr = addressAt(readIndex);
                if (matches(*readPtr)) {
                    std::destroy_at(readPtr);
                    ++matchCount;
                } else {
                    if (writeIndex != readIndex) {
                        T* writePtr = addressAt(writeIndex);
                        new (NotNull, writePtr) T(WTF::move(*readPtr));
                        std::destroy_at(readPtr);
                    }
                    ++writeIndex;
                }
            }
            m_size = writeIndex;
            shrinkToFit();
            return matchCount;
        }

    private:
        struct SegmentLocation {
            size_t segmentIndex;
            size_t offsetInSegment;
        };

        ALWAYS_INLINE static SegmentLocation segmentLocationFor(size_t index)
        {
            if constexpr (GrowthPolicy == SegmentedVectorGrowthPolicy::Doubling) {
                size_t n = index / SegmentSize + 1;
                size_t segmentIndex = (sizeof(size_t) * CHAR_BIT - 1) - WTF::clz(n);
                size_t offset = index - (SegmentSize * ((static_cast<size_t>(1) << segmentIndex) - 1));
                return { segmentIndex, offset };
            } else
                return { index / SegmentSize, index % SegmentSize };
        }

        ALWAYS_INLINE static size_t sizeOfSegment(size_t segmentIndex)
        {
            if constexpr (GrowthPolicy == SegmentedVectorGrowthPolicy::Doubling)
                return SegmentSize << segmentIndex;
            else
                return SegmentSize;
        }

        class Segment {
        public:
            std::span<T, SegmentSize> entries() requires (GrowthPolicy == SegmentedVectorGrowthPolicy::Constant)
            {
                return unsafeMakeSpan<T, SegmentSize>(m_entries, SegmentSize);
            }
            T* data() { return m_entries; }

        private:
            T m_entries[0];
        };

        using SegmentPtr = std::unique_ptr<Segment, NonDestructingDeleter<Segment, Malloc>>;

        struct EmptyInlineStorage { };
        struct InlineStorageData { AlignedStorage<T> m_data[InlineCapacity]; };

        ALWAYS_INLINE T* inlineStorage() LIFETIME_BOUND
        {
            static_assert(hasInlineStorage);
            return m_inlineStorageMember.m_data[0].get();
        }

        ALWAYS_INLINE const T* inlineStorage() const LIFETIME_BOUND
        {
            static_assert(hasInlineStorage);
            return m_inlineStorageMember.m_data[0].get();
        }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

        ALWAYS_INLINE T* addressAt(size_t index) LIFETIME_BOUND
        {
            if constexpr (hasInlineStorage) {
                if (index < InlineCapacity) [[likely]]
                    return &inlineStorage()[index];
                index -= InlineCapacity;
            }
            auto [segmentIndex, offset] = segmentLocationFor(index);
            return &m_segments[segmentIndex].get()->data()[offset];
        }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        void destroyAllItems()
        {
            for (size_t i = 0; i < m_size; ++i)
                std::destroy_at(addressAt(i));
        }

        ALWAYS_INLINE bool segmentExistsFor(size_t index)
        {
            if constexpr (hasInlineStorage) {
                if (index < InlineCapacity)
                    return true;
                return heapSegmentExistsFor(index);
            } else
                return segmentLocationFor(index).segmentIndex < m_segments.size();
        }

        ALWAYS_INLINE bool heapSegmentExistsFor(size_t index)
        {
            if constexpr (hasInlineStorage) {
                ASSERT(index >= InlineCapacity);
                return segmentLocationFor(index - InlineCapacity).segmentIndex < m_segments.size();
            } else
                return segmentLocationFor(index).segmentIndex < m_segments.size();
        }

        void ensureSegmentsFor(size_t size)
        {
            if constexpr (hasInlineStorage) {
                if (size <= InlineCapacity)
                    return;
                size_t currentSegmentCount = m_segments.size();
                size_t requiredSegmentCount = segmentLocationFor(size - InlineCapacity - 1).segmentIndex + 1;
                for (size_t i = currentSegmentCount; i < requiredSegmentCount; ++i)
                    allocateSegment();
            } else {
                size_t segmentCount = m_size ? segmentLocationFor(m_size - 1).segmentIndex + 1 : 0;
                size_t requiredSegmentCount = segmentLocationFor(size - 1).segmentIndex + 1;
                for (size_t i = segmentCount ? segmentCount - 1 : 0; i < requiredSegmentCount; ++i)
                    ensureSegment(i);
            }
        }

        void ensureSegment(size_t segmentIndex)
        {
            ASSERT_WITH_SECURITY_IMPLICATION(segmentIndex <= m_segments.size());
            if (segmentIndex == m_segments.size())
                allocateSegment();
        }

        void allocateSegment()
        {
            size_t segSize = sizeOfSegment(m_segments.size());
            auto* ptr = static_cast<Segment*>(Malloc::malloc(sizeof(T) * segSize));
            m_segments.append(SegmentPtr(ptr, { }));
        }

        size_t m_size { 0 };
        NO_UNIQUE_ADDRESS std::conditional_t<hasInlineStorage, InlineStorageData, EmptyInlineStorage> m_inlineStorageMember;
        Vector<SegmentPtr, 0, CrashOnOverflow, 16, Malloc> m_segments;
    };

} // namespace WTF

using WTF::SegmentedVector;
using WTF::SegmentedVectorGrowthPolicy;
