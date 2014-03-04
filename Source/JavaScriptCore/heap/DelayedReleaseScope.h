/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DelayedReleaseScope_h
#define DelayedReleaseScope_h

#include "Heap.h"
#include "JSLock.h"
#include "MarkedSpace.h"

namespace JSC {

#if USE(CF)

class DelayedReleaseScope {
public:
    DelayedReleaseScope(MarkedSpace& markedSpace)
        : m_markedSpace(markedSpace)
    {
        ASSERT(!m_markedSpace.m_currentDelayedReleaseScope);
        m_markedSpace.m_currentDelayedReleaseScope = this;
    }

    ~DelayedReleaseScope()
    {
        ASSERT(m_markedSpace.m_currentDelayedReleaseScope == this);
        m_markedSpace.m_currentDelayedReleaseScope = nullptr;

        HeapOperation operationInProgress = NoOperation;
        std::swap(operationInProgress, m_markedSpace.m_heap->m_operationInProgress);

        {
            JSLock::DropAllLocks dropAllLocks(*m_markedSpace.m_heap->vm());
            m_delayedReleaseObjects.clear();
        }

        std::swap(operationInProgress, m_markedSpace.m_heap->m_operationInProgress);
    }

    template <typename T>
    void releaseSoon(RetainPtr<T>&& object)
    {
        m_delayedReleaseObjects.append(std::move(object));
    }

    static bool isInEffectFor(MarkedSpace& markedSpace)
    {
        return markedSpace.m_currentDelayedReleaseScope;
    }

private:
    MarkedSpace& m_markedSpace;
    Vector<RetainPtr<CFTypeRef>> m_delayedReleaseObjects;
};

template <typename T>
inline void MarkedSpace::releaseSoon(RetainPtr<T>&& object)
{
    ASSERT(m_currentDelayedReleaseScope);
    m_currentDelayedReleaseScope->releaseSoon(std::move(object));
}

#else // USE(CF)

class DelayedReleaseScope {
public:
    DelayedReleaseScope(MarkedSpace&)
    {
    }

    static bool isInEffectFor(MarkedSpace&)
    {
        return true;
    }
};

#endif // USE(CF)

} // namespace JSC

#endif // DelayedReleaseScope_h
