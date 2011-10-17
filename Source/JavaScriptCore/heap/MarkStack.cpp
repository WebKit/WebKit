/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MarkStack.h"

#include "ConservativeRoots.h"
#include "Heap.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSObject.h"
#include "ScopeChain.h"
#include "Structure.h"
#include "WriteBarrier.h"

namespace JSC {

MarkStackArray::MarkStackArray()
    : m_top(0)
    , m_allocated(pageSize())
{
    m_data = static_cast<const JSCell**>(MarkStack::allocateStack(m_allocated));
    m_capacity = m_allocated / sizeof(JSCell*);
}

MarkStackArray::~MarkStackArray()
{
    MarkStack::releaseStack(m_data, m_allocated);
}

void MarkStackArray::expand()
{
    size_t oldAllocation = m_allocated;
    m_allocated *= 2;
    m_capacity = m_allocated / sizeof(JSCell*);
    void* newData = MarkStack::allocateStack(m_allocated);
    memcpy(newData, m_data, oldAllocation);
    MarkStack::releaseStack(m_data, oldAllocation);
    m_data = static_cast<const JSCell**>(newData);
}

void MarkStackArray::shrinkAllocation(size_t size)
{
    ASSERT(size <= m_allocated);
    ASSERT(isPageAligned(size));
    if (size == m_allocated)
        return;
#if OS(WINDOWS)
    // We cannot release a part of a region with VirtualFree. To get around this,
    // we'll release the entire region and reallocate the size that we want.
    MarkStack::releaseStack(m_data, m_allocated);
    m_data = static_cast<const JSCell**>(MarkStack::allocateStack(size));
#else
    MarkStack::releaseStack(reinterpret_cast<char*>(m_data) + size, m_allocated - size);
#endif
    m_allocated = size;
    m_capacity = m_allocated / sizeof(JSCell*);
}

void MarkStack::reset()
{
    m_visitCount = 0;
    m_stack.shrinkAllocation(pageSize());
    m_opaqueRoots.clear();
}

void MarkStack::append(ConservativeRoots& conservativeRoots)
{
    JSCell** roots = conservativeRoots.roots();
    size_t size = conservativeRoots.size();
    for (size_t i = 0; i < size; ++i)
        internalAppend(roots[i]);
}

ALWAYS_INLINE static void visitChildren(SlotVisitor& visitor, const JSCell* cell, void* jsFinalObjectVPtr, void* jsArrayVPtr, void* jsStringVPtr)
{
#if ENABLE(SIMPLE_HEAP_PROFILING)
    m_visitedTypeCounts.count(cell);
#endif

    ASSERT(Heap::isMarked(cell));

    if (cell->vptr() == jsStringVPtr)
        return;

    if (cell->vptr() == jsFinalObjectVPtr) {
        JSObject::visitChildren(const_cast<JSCell*>(cell), visitor);
        return;
    }

    if (cell->vptr() == jsArrayVPtr) {
        JSArray::visitChildren(const_cast<JSCell*>(cell), visitor);
        return;
    }

    cell->methodTable()->visitChildren(const_cast<JSCell*>(cell), visitor);
}

void SlotVisitor::drain()
{
    void* jsFinalObjectVPtr = m_jsFinalObjectVPtr;
    void* jsArrayVPtr = m_jsArrayVPtr;
    void* jsStringVPtr = m_jsStringVPtr;

    while (!m_stack.isEmpty())
        visitChildren(*this, m_stack.removeLast(), jsFinalObjectVPtr, jsArrayVPtr, jsStringVPtr);
}

void SlotVisitor::harvestWeakReferences()
{
    while (m_firstWeakReferenceHarvester) {
        WeakReferenceHarvester* current = m_firstWeakReferenceHarvester;
        WeakReferenceHarvester* next = reinterpret_cast<WeakReferenceHarvester*>(current->m_nextAndFlag & ~1);
        current->m_nextAndFlag = 0;
        m_firstWeakReferenceHarvester = next;
        current->visitWeakReferences(*this);
    }
}

#if ENABLE(GC_VALIDATION)
void MarkStack::validate(JSCell* cell)
{
    if (!cell)
        CRASH();

    if (!cell->structure())
        CRASH();

    // Both the cell's structure, and the cell's structure's structure should be the Structure Structure.
    // I hate this sentence.
    if (cell->structure()->structure()->JSCell::classInfo() != cell->structure()->JSCell::classInfo())
        CRASH();
}
#else
void MarkStack::validate(JSCell*)
{
}
#endif

} // namespace JSC
