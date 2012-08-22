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

#ifndef MarkStackInlineMethods_h
#define MarkStackInlineMethods_h

#include "GCThreadSharedData.h"
#include "MarkStack.h"

namespace JSC {

ALWAYS_INLINE void MarkStack::append(JSValue* slot, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        JSValue& value = slot[i];
        if (!value)
            continue;
        internalAppend(value);
    }
}

template<typename T>
inline void MarkStack::appendUnbarrieredPointer(T** slot)
{
    ASSERT(slot);
    JSCell* cell = *slot;
    if (cell)
        internalAppend(cell);
}

ALWAYS_INLINE void MarkStack::append(JSValue* slot)
{
    ASSERT(slot);
    internalAppend(*slot);
}

ALWAYS_INLINE void MarkStack::appendUnbarrieredValue(JSValue* slot)
{
    ASSERT(slot);
    internalAppend(*slot);
}

ALWAYS_INLINE void MarkStack::append(JSCell** slot)
{
    ASSERT(slot);
    internalAppend(*slot);
}

ALWAYS_INLINE void MarkStack::internalAppend(JSValue value)
{
    ASSERT(value);
    if (!value.isCell())
        return;
    internalAppend(value.asCell());
}

inline void MarkStack::addWeakReferenceHarvester(WeakReferenceHarvester* weakReferenceHarvester)
{
    m_shared.m_weakReferenceHarvesters.addThreadSafe(weakReferenceHarvester);
}

inline void MarkStack::addUnconditionalFinalizer(UnconditionalFinalizer* unconditionalFinalizer)
{
    m_shared.m_unconditionalFinalizers.addThreadSafe(unconditionalFinalizer);
}

inline void MarkStack::addOpaqueRoot(void* root)
{
#if ENABLE(PARALLEL_GC)
    if (Options::numberOfGCMarkers() == 1) {
        // Put directly into the shared HashSet.
        m_shared.m_opaqueRoots.add(root);
        return;
    }
    // Put into the local set, but merge with the shared one every once in
    // a while to make sure that the local sets don't grow too large.
    mergeOpaqueRootsIfProfitable();
    m_opaqueRoots.add(root);
#else
    m_opaqueRoots.add(root);
#endif
}

inline bool MarkStack::containsOpaqueRoot(void* root)
{
    ASSERT(!m_isInParallelMode);
#if ENABLE(PARALLEL_GC)
    ASSERT(m_opaqueRoots.isEmpty());
    return m_shared.m_opaqueRoots.contains(root);
#else
    return m_opaqueRoots.contains(root);
#endif
}

inline int MarkStack::opaqueRootCount()
{
    ASSERT(!m_isInParallelMode);
#if ENABLE(PARALLEL_GC)
    ASSERT(m_opaqueRoots.isEmpty());
    return m_shared.m_opaqueRoots.size();
#else
    return m_opaqueRoots.size();
#endif
}

} // namespace JSC

#endif
