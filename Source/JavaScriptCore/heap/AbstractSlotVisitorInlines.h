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

#include "AbstractSlotVisitor.h"
#include "Heap.h"
#include "WeakInlines.h"
#include "WriteBarrier.h"

namespace JSC {

inline AbstractSlotVisitor::Context::Context(AbstractSlotVisitor& visitor, HeapCell* cell)
    : m_visitor(visitor)
    , m_cell(cell)
{
    m_previous = m_visitor.m_context;
    m_visitor.m_context = this;
}

inline AbstractSlotVisitor::Context::~Context()
{
    m_visitor.m_context = m_previous;
}

inline AbstractSlotVisitor::AbstractSlotVisitor(Heap& heap, CString codeName, ConcurrentPtrHashSet& opaqueRoots)
    : m_heap(heap)
    , m_codeName(codeName)
    , m_opaqueRoots(opaqueRoots)
{
}

inline Heap* AbstractSlotVisitor::heap() const
{
    return &m_heap;
}

inline VM& AbstractSlotVisitor::vm()
{
    return m_heap.vm();
}

inline const VM& AbstractSlotVisitor::vm() const
{
    return m_heap.vm();
}

inline bool AbstractSlotVisitor::addOpaqueRoot(void* ptr)
{
    if (!ptr)
        return false;
    if (m_ignoreNewOpaqueRoots)
        return false;
    if (!m_opaqueRoots.add(ptr))
        return false;
    m_visitCount++;
    return true;
}

inline bool AbstractSlotVisitor::containsOpaqueRoot(void* ptr) const
{
    return m_opaqueRoots.contains(ptr);
}

template<typename T>
ALWAYS_INLINE void AbstractSlotVisitor::append(const Weak<T>& weak)
{
    appendUnbarriered(weak.get());
}

template<typename T, typename Traits>
ALWAYS_INLINE void AbstractSlotVisitor::append(const WriteBarrierBase<T, Traits>& slot)
{
    appendUnbarriered(slot.get());
}

template<typename T, typename Traits>
ALWAYS_INLINE void AbstractSlotVisitor::appendHidden(const WriteBarrierBase<T, Traits>& slot)
{
    appendHiddenUnbarriered(slot.get());
}

ALWAYS_INLINE void AbstractSlotVisitor::appendHiddenUnbarriered(JSValue value)
{
    if (value.isCell())
        appendHiddenUnbarriered(value.asCell());
}

template<typename Iterator>
ALWAYS_INLINE void AbstractSlotVisitor::append(Iterator begin, Iterator end)
{
    for (auto it = begin; it != end; ++it)
        append(*it);
}

ALWAYS_INLINE void AbstractSlotVisitor::appendValues(const WriteBarrierBase<Unknown>* barriers, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        append(barriers[i]);
}

ALWAYS_INLINE void AbstractSlotVisitor::appendValuesHidden(const WriteBarrierBase<Unknown>* barriers, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        appendHidden(barriers[i]);
}

ALWAYS_INLINE void AbstractSlotVisitor::appendUnbarriered(JSValue value)
{
    if (value.isCell())
        appendUnbarriered(value.asCell());
}

ALWAYS_INLINE void AbstractSlotVisitor::appendUnbarriered(JSValue* slot, size_t count)
{
    for (size_t i = count; i--;)
        appendUnbarriered(slot[i]);
}

ALWAYS_INLINE HeapCell* AbstractSlotVisitor::parentCell() const
{
    if (!m_context)
        return nullptr;
    return m_context->cell();
}

ALWAYS_INLINE void AbstractSlotVisitor::reset()
{
    m_visitCount = 0;
}

} // namespace JSC
