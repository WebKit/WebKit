/*
 * Copyright (C) 2012, 2013, 2015 Apple Inc. All rights reserved.
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

#ifndef SlotVisitorInlines_h
#define SlotVisitorInlines_h

#include "SlotVisitor.h"
#include "Weak.h"
#include "WeakInlines.h"

namespace JSC {

template<typename T>
inline void SlotVisitor::appendUnbarrieredPointer(T** slot)
{
    ASSERT(slot);
    append(*slot);
}

template<typename T>
inline void SlotVisitor::appendUnbarrieredReadOnlyPointer(T* cell)
{
    append(cell);
}

inline void SlotVisitor::appendUnbarrieredValue(JSValue* slot)
{
    ASSERT(slot);
    append(*slot);
}

inline void SlotVisitor::appendUnbarrieredReadOnlyValue(JSValue value)
{
    append(value);
}

template<typename T>
inline void SlotVisitor::appendUnbarrieredWeak(Weak<T>* weak)
{
    ASSERT(weak);
    append(weak->get());
}

template<typename T>
inline void SlotVisitor::append(WriteBarrierBase<T>* slot)
{
    append(slot->get());
}

template<typename T>
inline void SlotVisitor::appendHidden(WriteBarrierBase<T>* slot)
{
    appendHidden(slot->get());
}

template<typename Iterator>
inline void SlotVisitor::append(Iterator begin, Iterator end)
{
    for (auto it = begin; it != end; ++it)
        append(&*it);
}

inline void SlotVisitor::appendValues(WriteBarrierBase<Unknown>* barriers, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        append(&barriers[i]);
}

inline void SlotVisitor::appendValuesHidden(WriteBarrierBase<Unknown>* barriers, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        appendHidden(&barriers[i]);
}

inline void SlotVisitor::addWeakReferenceHarvester(WeakReferenceHarvester* weakReferenceHarvester)
{
    m_heap.m_weakReferenceHarvesters.addThreadSafe(weakReferenceHarvester);
}

inline void SlotVisitor::addUnconditionalFinalizer(UnconditionalFinalizer* unconditionalFinalizer)
{
    m_heap.m_unconditionalFinalizers.addThreadSafe(unconditionalFinalizer);
}

inline void SlotVisitor::reportExtraMemoryVisited(size_t size)
{
    heap()->reportExtraMemoryVisited(m_currentObjectCellStateBeforeVisiting, size);
}

inline Heap* SlotVisitor::heap() const
{
    return &m_heap;
}

inline VM& SlotVisitor::vm()
{
    return *m_heap.m_vm;
}

inline const VM& SlotVisitor::vm() const
{
    return *m_heap.m_vm;
}

} // namespace JSC

#endif // SlotVisitorInlines_h

