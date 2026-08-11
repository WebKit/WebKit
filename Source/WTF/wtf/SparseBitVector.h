/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <wtf/BitSet.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WTF {

// A bit vector that only stores the chunks ("elements") that have set bits, so its memory is
// proportional to the number of set bits rather than to the largest index. This is the right
// representation when the universe of indices is large but only a small, clustered subset is set
// at any time -- e.g. per-basic-block liveness sets over a function with many virtual registers,
// where a dense BitVector would cost numBlocks x numIndices bits.
template<unsigned elementBits = 128>
class SparseBitVector {
public:
    SparseBitVector() = default;

    void ensureSize(unsigned) { }

    void clear()
    {
        m_elements.clear();
        m_hint = 0;
    }

    bool isEmpty() const
    {
        ASSERT(allElementsNonEmpty());
        return m_elements.isEmpty();
    }

    bool set(unsigned bit)
    {
        Element& element = findOrInsert(bit / elementBits);
        return !element.bits.testAndSet(bit % elementBits);
    }

    // Matches WTF::BitVector::add(): returns whether the bit transitioned from unset to set. This
    // lets callers template uniformly over BitVector and SparseBitVector.
    bool add(unsigned bit)
    {
        return set(bit);
    }

    bool reset(unsigned bit)
    {
        unsigned elementIndex = bit / elementBits;
        unsigned position = (m_hint < m_elements.size() && m_elements[m_hint].index == elementIndex) ? m_hint : lowerBound(elementIndex);
        if (position >= m_elements.size() || m_elements[position].index != elementIndex)
            return false;

        bool wasSet = m_elements[position].bits.testAndClear(bit % elementBits);
        if (wasSet && m_elements[position].bits.isEmpty()) {
            m_elements.removeAt(position);
            m_hint = 0; // Positions shifted; invalidate the hint.
        } else
            m_hint = position;

        return wasSet;
    }

    bool contains(unsigned bit) const
    {
        const Element* element = find(bit / elementBits);
        return element && element->bits.get(bit % elementBits);
    }

    template<typename Func>
    void forEachSetBit(const Func& func) const
    {
        for (const Element& element : m_elements) {
            unsigned base = element.index * elementBits;
            element.bits.forEachSetBit([&] (size_t bitInElement) {
                func(base + static_cast<unsigned>(bitInElement));
            });
        }
    }

    class iterator {
    public:
        iterator() = default;

        iterator(const SparseBitVector& owner, bool isEnd)
            : m_owner(&owner)
        {
            if (isEnd || owner.m_elements.isEmpty()) {
                m_elementIndex = owner.m_elements.size();
                return;
            }
            m_bitIter = owner.m_elements[0].bits.begin();
        }

        unsigned operator*() const
        {
            const Element& element = m_owner->m_elements[m_elementIndex];
            return element.index * elementBits + static_cast<unsigned>(*m_bitIter);
        }

        iterator& operator++()
        {
            ++m_bitIter;
            if (m_bitIter == m_owner->m_elements[m_elementIndex].bits.end()) {
                ++m_elementIndex;
                if (m_elementIndex < m_owner->m_elements.size())
                    m_bitIter = m_owner->m_elements[m_elementIndex].bits.begin();
                else
                    m_bitIter = { };
            }
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            if (m_elementIndex != other.m_elementIndex)
                return false;
            if (!m_owner || m_elementIndex == m_owner->m_elements.size())
                return true;
            return m_bitIter == other.m_bitIter;
        }

    private:
        const SparseBitVector* m_owner { nullptr };
        size_t m_elementIndex { 0 };
        typename BitSet<elementBits>::iterator m_bitIter;
    };

    iterator begin() const LIFETIME_BOUND { return iterator(*this, false); }
    iterator end() const LIFETIME_BOUND { return iterator(*this, true); }

private:
    struct Element {
        unsigned index { 0 };
        BitSet<elementBits> bits;
    };

#if ASSERT_ENABLED
    bool allElementsNonEmpty() const
    {
        for (const Element& element : m_elements) {
            if (element.bits.isEmpty())
                return false;
        }
        return true;
    }
#endif

    const Element* find(unsigned elementIndex) const
    {
        if (m_hint < m_elements.size() && m_elements[m_hint].index == elementIndex)
            return &m_elements[m_hint];

        unsigned position = lowerBound(elementIndex);
        if (position < m_elements.size() && m_elements[position].index == elementIndex) {
            m_hint = position;
            return &m_elements[position];
        }

        return nullptr;
    }

    Element& findOrInsert(unsigned elementIndex)
    {
        if (m_hint < m_elements.size() && m_elements[m_hint].index == elementIndex)
            return m_elements[m_hint];

        unsigned position = lowerBound(elementIndex);
        if (position < m_elements.size() && m_elements[position].index == elementIndex) {
            m_hint = position;
            return m_elements[position];
        }

        m_elements.insert(position, Element { elementIndex, { } });
        m_hint = position;
        return m_elements[position];
    }

    unsigned lowerBound(unsigned elementIndex) const
    {
        auto iterator = std::ranges::lower_bound(m_elements, elementIndex, { }, &Element::index);
        return iterator - m_elements.begin();
    }

    Vector<Element> m_elements;
    mutable unsigned m_hint { 0 };
};

} // namespace WTF

using WTF::SparseBitVector;
