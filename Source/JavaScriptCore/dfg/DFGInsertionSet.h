/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef DFGInsertionSet_h
#define DFGInsectionSet_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include <wtf/Vector.h>

namespace JSC { namespace DFG {

template<typename ElementType>
class Insertion {
public:
    Insertion() { }
    
    Insertion(size_t index, const ElementType& element)
        : m_index(index)
        , m_element(element)
    {
    }
    
    size_t index() const { return m_index; }
    const ElementType& element() const { return m_element; }
private:
    size_t m_index;
    ElementType m_element;
};

template<typename ElementType>
class InsertionSet {
public:
    InsertionSet() { }
    
    void append(const Insertion<ElementType>& insertion)
    {
        ASSERT(!m_insertions.size() || m_insertions.last().index() <= insertion.index());
        m_insertions.append(insertion);
    }
    
    void append(size_t index, const ElementType& element)
    {
        append(Insertion<ElementType>(index, element));
    }
    
    template<typename CollectionType>
    void execute(CollectionType& collection)
    {
        if (!m_insertions.size())
            return;
        collection.grow(collection.size() + m_insertions.size());
        size_t lastIndex = collection.size();
        for (size_t indexInInsertions = m_insertions.size(); indexInInsertions--;) {
            Insertion<ElementType>& insertion = m_insertions[indexInInsertions];
            size_t firstIndex = insertion.index() + indexInInsertions;
            size_t indexOffset = indexInInsertions + 1;
            for (size_t i = lastIndex; i-- > firstIndex;)
                collection[i] = collection[i - indexOffset];
            collection[firstIndex] = insertion.element();
            lastIndex = firstIndex;
        }
        m_insertions.resize(0);
    }
private:
    Vector<Insertion<ElementType>, 8> m_insertions;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGInsertionSet_h

