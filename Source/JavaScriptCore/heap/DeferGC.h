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

#ifndef DeferGC_h
#define DeferGC_h

#include "Heap.h"
#include <wtf/Noncopyable.h>
#include <wtf/ThreadSpecific.h>

namespace JSC {

class DeferGC {
    WTF_MAKE_NONCOPYABLE(DeferGC);
public:
    DeferGC(Heap& heap)
        : m_heap(heap)
    {
        m_heap.incrementDeferralDepth();
    }
    
    ~DeferGC()
    {
        m_heap.decrementDeferralDepthAndGCIfNeeded();
    }

private:
    Heap& m_heap;
};

class DeferGCForAWhile {
    WTF_MAKE_NONCOPYABLE(DeferGCForAWhile);
public:
    DeferGCForAWhile(Heap& heap)
        : m_heap(heap)
    {
        m_heap.incrementDeferralDepth();
    }
    
    ~DeferGCForAWhile()
    {
        m_heap.decrementDeferralDepth();
    }

private:
    Heap& m_heap;
};

#ifndef NDEBUG
class DisallowGC {
    WTF_MAKE_NONCOPYABLE(DisallowGC);
public:
    DisallowGC()
    {
        WTF::threadSpecificSet(s_isGCDisallowedOnCurrentThread, reinterpret_cast<void*>(true));
    }

    ~DisallowGC()
    {
        WTF::threadSpecificSet(s_isGCDisallowedOnCurrentThread, reinterpret_cast<void*>(false));
    }

    static bool isGCDisallowedOnCurrentThread()
    {
        return !!WTF::threadSpecificGet(s_isGCDisallowedOnCurrentThread);
    }
    static void initialize()
    {
        WTF::threadSpecificKeyCreate(&s_isGCDisallowedOnCurrentThread, 0);
    }

    JS_EXPORT_PRIVATE static WTF::ThreadSpecificKey s_isGCDisallowedOnCurrentThread;
};
#endif // NDEBUG

} // namespace JSC

#endif // DeferGC_h

