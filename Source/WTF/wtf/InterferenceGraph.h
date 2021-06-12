/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/BitVector.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/LikelyDenseUnsignedIntegerSet.h>
#include <wtf/Vector.h>

namespace WTF {

// This file offers multiple implementations of a simple interference graph, with varying performance characteristics.
// Conceptually they are just sets of pairs of integers
// All of them have the same interface:
//   bool contains(IndexType u, IndexType v)
//   bool addAndReturnIsNewEntry(IndexType u, IndexType v)
//   void add(IndexType u, IndexType v)
//   void clear()
//   void mayClear(IndexType u)
//   void setMaxIndex(unsigned n)
//   void forEach(const Functor& functor)
//   unsigned size()
//   unsigned memoryUse()
//   void dumpMemoryUseInKB()
// Three useful type aliases are defined:
//   SmallInterferenceGraph, which is the fastest but has quadratic memory use in the maximum vertex index (best for n < 400 as it is already 20kB by that point)
//   LargeInterferenceGraph, as long as the indices fit in a uint16_t
//   HugeInterferenceGraph otherwise
// If you need to iterate over the indices that interfere with a given index, you must use the following:
//   SmallIterableInterferenceGraph
//   LargeIterableInterferenceGraph
//   HugeIterableInterferenceGraph
// The large and huge versions are 2x the memory of their non-iterable versions, but offer an additional operator[] method, which returns an iterable object

template <typename T>
class InterferenceBitVector {
public:
    using IndexType = T;
    bool contains(IndexType u, IndexType v)
    {
        return m_bitVector.quickGet(index(u, v));
    }

    bool addAndReturnIsNewEntry(IndexType u, IndexType v)
    {
        bool alreadyIn = m_bitVector.quickSet(index(u, v));
        m_size += !alreadyIn;
        return !alreadyIn;
    }

    void add(IndexType u, IndexType v)
    {
        (void) addAndReturnIsNewEntry(u, v);
    }

    void clear()
    {
        m_bitVector.clearAll();
        m_size = 0;
    }

    void mayClear(IndexType) { }

    void setMaxIndex(unsigned n)
    {
        ASSERT(n < std::numeric_limits<IndexType>::max());
        m_numElements = n;
        m_bitVector.ensureSize(n*n);
    }

    template <typename Functor>
    void forEach(const Functor& functor)
    {
        for (IndexType i = 0; i < m_numElements; ++i) {
            for (IndexType j = i + 1; j < m_numElements; ++j) {
                if (m_bitVector.quickGet(index(i, j)))
                    functor({ i, j });
            }
        }
    }

    unsigned size() const { return m_size; }

    unsigned memoryUse() const { return (m_bitVector.size() + 7) >> 3; }

    void dumpMemoryUseInKB() const { dataLog(memoryUse() / 1024); }

    struct Iterable {
        struct iterator {
            iterator& operator++()
            {
                m_index = m_iterable.m_bitVector.findBit(m_index + 1, true);
                return *this;
            }

            IndexType operator*() const
            {
                return static_cast<IndexType>(m_index - m_iterable.m_startingIndex);
            }

            bool operator==(const iterator& other) const
            {
                // We cannot just use a normal comparison beyond m_iterable.m_end, because the findBit in operator++ may send us pretty much anywhere
                // Worse: it is possible for the user of this iterator to add elements to the interference graph between calls to ++
                // This won't invalidate us/cause problems directly, as the user won't affect the slice of the bit vector that we're iterating over.
                // But they may well set bits beyond it. So we cannot compute an "ideal" end iterator (by looking for the first bit set after the slice we care about) ahead of time.
                // Instead we set m_end to index(u+1, 0) (so just after the slice of interest), and we use this slightly weird comparison operator to still stop in time once we go beyond it.
                if (m_index >= m_iterable.m_end && other.m_index >= m_iterable.m_end)
                    return true;
                return m_index == other.m_index;
            }

            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }

            const Iterable& m_iterable;
            unsigned m_index;
        };

        iterator begin() const { return { *this, m_begin }; }
        iterator end() const { return { *this, m_end }; }

        const BitVector& m_bitVector;
        unsigned m_startingIndex;
        unsigned m_begin;
        unsigned m_end;
    };

    Iterable operator[](IndexType u) const
    {
        unsigned begin = m_bitVector.findBit(index(u, 0), true);
        unsigned end = index(u + 1, 0);
        return { m_bitVector, index(u, 0), begin, end };
    }

private:
    unsigned index(IndexType i, IndexType j) const
    {
        return i * m_numElements + j;
    }

    BitVector m_bitVector;
    unsigned m_size {0};
    IndexType m_numElements;
};

template <typename T, typename IT>
class InterferenceVector {
public:
    using IndexType = IT;
    bool contains(IndexType u, IndexType v)
    {
        return m_vector[u].contains(v);
    }

    bool addAndReturnIsNewEntry(IndexType u, IndexType v)
    {
        bool isNewEntry = m_vector[u].add(v).isNewEntry;
        m_size += isNewEntry;
        return isNewEntry;
    }

    void add(IndexType u, IndexType v)
    {
        (void) addAndReturnIsNewEntry(u, v);
    }

    void clear()
    {
        m_vector.clear();
        m_size = 0;
    }

    void mayClear(IndexType u) { m_vector[u].clear(); }

    void setMaxIndex(unsigned n)
    {
        ASSERT(n < std::numeric_limits<IndexType>::max());
        m_vector.resize(n);
    }

    template <typename Functor>
    void forEach(const Functor& functor)
    {
        for (unsigned i = 0; i < m_vector.size() ; ++i) {
            for (IndexType j : m_vector[i])
                functor({ i, j });
        }
    }

    unsigned size() const { return m_size; }

    unsigned memoryUse() const
    {
        unsigned memory = 0;
        for (const T& set : m_vector)
            memory += set.memoryUse();
        return memory;
    }

    void dumpMemoryUseInKB() const { dataLog(memoryUse() / 1024); }

    struct Iterable {
        using iterator = typename T::iterator;
        iterator begin() const { return m_set.begin(); }
        iterator end() const { return m_set.end(); }
        const T& m_set;
    };

    Iterable operator[](IndexType u) const { return { m_vector[u] }; }

private:
    Vector<T> m_vector;
    unsigned m_size {0};
};

template<typename T>
class UndirectedEdgesDuplicatingAdapter {
public:
    using IndexType = typename T::IndexType;
    using Iterable = typename T::Iterable;
    bool contains(IndexType u, IndexType v) { return m_underlying.contains(u, v); }

    bool addAndReturnIsNewEntry(IndexType u, IndexType v)
    {
        bool isNewEntry1 = m_underlying.addAndReturnIsNewEntry(u, v);
        bool isNewEntry2 = m_underlying.addAndReturnIsNewEntry(v, u);
        ASSERT_UNUSED(isNewEntry2, isNewEntry1 == isNewEntry2);
        return isNewEntry1;
    }

    void add(IndexType u, IndexType v)
    {
        m_underlying.add(u, v);
        m_underlying.add(v, u);
    }

    void clear() { m_underlying.clear(); }
    void mayClear(IndexType u) { m_underlying.mayClear(u); }
    void setMaxIndex(unsigned n) { m_underlying.setMaxIndex(n); }
    template <typename Functor>
    void forEach(const Functor& functor) { m_underlying.forEach(functor); }
    unsigned size() const { return m_underlying.size(); }
    unsigned memoryUse() const { return m_underlying.memoryUse(); }
    void dumpMemoryUseInKB() const { m_underlying.dumpMemoryUseInKB(); }
    Iterable operator[](IndexType u) const { return m_underlying[u]; }

private:
    T m_underlying;
};

// This is generally better than the Duplicating version, there are two reasons not to use it though:
// - For BitVector, it has no effect on memory use, and adds a branch to every call to contains/addAndReturnIsNewEntry
// - It makes it impossible to iterate over the indices that interfere with a particular index. This can be required for some uses (see AirAllocateStackByGraphColoring)
template<typename T>
class UndirectedEdgesDedupAdapter {
public:
    using IndexType = typename T::IndexType;
    bool contains(IndexType u, IndexType v)
    {
        if (v < u)
            std::swap(u, v);
        return m_underlying.contains(u, v);
    }

    bool addAndReturnIsNewEntry(IndexType u, IndexType v)
    {
        if (v < u)
            std::swap(u, v);
        return m_underlying.addAndReturnIsNewEntry(u, v);
    }

    void add(IndexType u, IndexType v)
    {
        if (v < u)
            std::swap(u, v);
        m_underlying.add(u, v);
    }

    void clear() { m_underlying.clear(); }
    void mayClear(IndexType u) { m_underlying.mayClear(u); }
    void setMaxIndex(unsigned n) { m_underlying.setMaxIndex(n); }
    template <typename Functor>
    void forEach(const Functor& functor) { m_underlying.forEach(functor); }
    unsigned size() const { return m_underlying.size(); }
    unsigned memoryUse() const { return m_underlying.memoryUse(); }
    void dumpMemoryUseInKB() const { m_underlying.dumpMemoryUseInKB(); }

private:
    T m_underlying;
};

// The bit vector approach uses n*n bits, or 20kB for n = 400.
constexpr unsigned maxSizeForSmallInterferenceGraph = 400;
using DefaultSmallInterferenceGraph = UndirectedEdgesDuplicatingAdapter<InterferenceBitVector<uint16_t>>;
using DefaultSmallIterableInterferenceGraph = DefaultSmallInterferenceGraph;
using DefaultLargeInterferenceGraph = UndirectedEdgesDedupAdapter<InterferenceVector<LikelyDenseUnsignedIntegerSet<uint16_t>, uint16_t>>;
using DefaultHugeInterferenceGraph = UndirectedEdgesDedupAdapter<InterferenceVector<LikelyDenseUnsignedIntegerSet<uint32_t>, uint32_t>>;
using DefaultLargeIterableInterferenceGraph = UndirectedEdgesDuplicatingAdapter<InterferenceVector<LikelyDenseUnsignedIntegerSet<uint16_t>, uint16_t>>;
using DefaultHugeIterableInterferenceGraph = UndirectedEdgesDuplicatingAdapter<InterferenceVector<LikelyDenseUnsignedIntegerSet<uint32_t>, uint32_t>>;

// Set to one to run a reference implementation in parallel with the optimized datastructure, verifying that they agree on every method call.
// Beware: this is naturally much slower and memory hungry
#define TEST_OPTIMIZED_INTERFERENCE_GRAPH 0
#if !TEST_OPTIMIZED_INTERFERENCE_GRAPH

using SmallInterferenceGraph = DefaultSmallInterferenceGraph;
using LargeInterferenceGraph = DefaultLargeInterferenceGraph;
using HugeInterferenceGraph = DefaultHugeInterferenceGraph;

using SmallIterableInterferenceGraph = DefaultSmallIterableInterferenceGraph;
using LargeIterableInterferenceGraph = DefaultLargeIterableInterferenceGraph;
using HugeIterableInterferenceGraph = DefaultHugeIterableInterferenceGraph;

#else // TEST_OPTIMIZED_INTERFERENCE_GRAPH

// This is an unoptimized reference implementation, for use with InstrumentedInterferenceGraph below
template <typename T>
class InterferenceHashSet {
public:
    using IndexType = T;
    using InterferenceEdge = std::pair<IndexType, IndexType>;
    bool contains(IndexType u, IndexType v)
    {
        return m_set.contains({ u, v });
    }

    bool addAndReturnIsNewEntry(IndexType u, IndexType v)
    {
        return m_set.add({ u, v }).isNewEntry;
    }

    void add(IndexType u, IndexType v)
    {
        m_set.add({ u, v });
    }

    void clear()
    {
        m_set.clear();
    }

    void setMaxIndex(unsigned n)
    {
        ASSERT_UNUSED(n, n < std::numeric_limits<IndexType>::max());
    }

    template <typename Functor>
    void forEach(const Functor& functor)
    {
        for (auto edge : m_set)
            functor(edge);
    }

    unsigned size() const { return m_set.size(); }

    unsigned memoryUse() const { return m_set.capacity() * sizeof(InterferenceEdge); }

    void dumpMemoryUseInKB() const { dataLog(memoryUse() / 1024); }

private:
    HashSet<InterferenceEdge> m_set;
};

template <typename T, typename U>
class InstrumentedInterferenceGraph {
public:
    using IndexType = typename T::IndexType;
    static_assert(std::is_same<IndexType, typename U::IndexType>::value);
    bool contains(IndexType u, IndexType v)
    {
        bool containsInA = m_setA.contains(u, v);
        bool containsInB = m_setB.contains(u, v);
        RELEASE_ASSERT(containsInA == containsInB);
        return containsInA;
    }

    bool addAndReturnIsNewEntry(IndexType u, IndexType v)
    {
        bool isNewEntryA = m_setA.addAndReturnIsNewEntry(u, v);
        bool isNewEntryB = m_setB.addAndReturnIsNewEntry(u, v);
        RELEASE_ASSERT(isNewEntryA == isNewEntryB);
        return isNewEntryA;
    }

    void add(IndexType u, IndexType v)
    {
        m_setA.add(u, v);
        m_setB.add(u, v);
    }

    void clear()
    {
        m_setA.clear();
        m_setB.clear();
    }

    void mayClear(IndexType u)
    {
        m_setA.mayClear(u);
        m_setB.mayClear(u);
    }

    void setMaxIndex(unsigned n)
    {
        ASSERT(n < std::numeric_limits<IndexType>::max());
        m_setA.setMaxIndex(n);
        m_setB.setMaxIndex(n);
    }

    template <typename Functor>
    void forEach(const Functor& functor)
    {
        m_setA.forEach(functor);
    }

    unsigned size() const
    {
        unsigned sizeA = m_setA.size();
        unsigned sizeB = m_setB.size();
        RELEASE_ASSERT(sizeA == sizeB);
        return sizeA;
    }

    unsigned memoryUse() const
    {
        return m_setA.memoryUse() + m_setB.memoryUse();
    }

    void dumpMemoryUseInKB() const
    {
        dataLog(m_setA.memoryUse() / 1024, " -> ", m_setB.memoryUse() / 1024);
    }

protected:
    T m_setA;
    U m_setB;
};

template <typename T, typename U>
class InstrumentedIterableInterferenceGraph : public InstrumentedInterferenceGraph<T, U> {
public:
    using Base = InstrumentedInterferenceGraph<T, U>;
    using IndexType = typename Base::IndexType;

    struct Iterable {
        Iterable(typename T::Iterable iterableA, typename U::Iterable iterableB)
        {
            for (IndexType value : iterableA)
                m_values.append(value);
            Vector<IndexType> valuesB;
            for (IndexType value : iterableB)
                valuesB.append(value);
            RELEASE_ASSERT(m_values.size() == valuesB.size());
            std::sort(m_values.begin(), m_values.end());
            std::sort(valuesB.begin(), valuesB.end());
            for (unsigned i = 0; i < m_values.size(); ++i)
                RELEASE_ASSERT(m_values[i] == valuesB[i]);
        }
        using iterator = typename Vector<IndexType>::const_iterator;

        iterator begin() const
        {
            return m_values.begin();
        }
        iterator end() const
        {
            return m_values.end();
        }

        Vector<IndexType> m_values;
    };

    Iterable operator[](IndexType u) const
    {
        return { Base::m_setA[u], Base::m_setB[u] };
    }
};

using ReferenceInterferenceGraph = UndirectedEdgesDedupAdapter<InterferenceHashSet<uint16_t>>;
using HugeReferenceInterferenceGraph = UndirectedEdgesDedupAdapter<InterferenceHashSet<uint32_t>>;

using SmallInterferenceGraph = InstrumentedInterferenceGraph<ReferenceInterferenceGraph, DefaultSmallInterferenceGraph>;
using LargeInterferenceGraph = InstrumentedInterferenceGraph<ReferenceInterferenceGraph, DefaultLargeInterferenceGraph>;
using HugeInterferenceGraph = InstrumentedInterferenceGraph<HugeReferenceInterferenceGraph, DefaultHugeInterferenceGraph>;

using ReferenceIterableInterferenceGraph = UndirectedEdgesDuplicatingAdapter<InterferenceVector<HashSet<uint16_t, IntHash<uint16_t>, UnsignedWithZeroKeyHashTraits<uint16_t>>, uint16_t>>;
using HugeReferenceIterableInterferenceGraph = UndirectedEdgesDuplicatingAdapter<InterferenceVector<HashSet<uint32_t, IntHash<uint32_t>, UnsignedWithZeroKeyHashTraits<uint32_t>>, uint32_t>>;

using SmallIterableInterferenceGraph = InstrumentedIterableInterferenceGraph<ReferenceIterableInterferenceGraph, DefaultSmallIterableInterferenceGraph>;
using LargeIterableInterferenceGraph = InstrumentedIterableInterferenceGraph<ReferenceIterableInterferenceGraph, DefaultLargeIterableInterferenceGraph>;
using HugeIterableInterferenceGraph = InstrumentedIterableInterferenceGraph<HugeReferenceIterableInterferenceGraph, DefaultHugeIterableInterferenceGraph>;

#endif // TEST_OPTIMIZED_INTERFERENCE_GRAPH

} // namespace WTF

using WTF::SmallInterferenceGraph;
using WTF::LargeInterferenceGraph;
using WTF::HugeInterferenceGraph;
using WTF::SmallIterableInterferenceGraph;
using WTF::LargeIterableInterferenceGraph;
using WTF::HugeIterableInterferenceGraph;
