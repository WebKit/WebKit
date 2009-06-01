/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#ifndef ThreadGlobalData_h
#define ThreadGlobalData_h

#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

    class EventNames;
    struct ICUConverterWrapper;
    struct TECConverterWrapper;
    class ThreadTimers;

    class ThreadGlobalData : Noncopyable {
    public:
        ThreadGlobalData();
        ~ThreadGlobalData();

        EventNames& eventNames() { return *m_eventNames; }
        StringImpl* emptyString() { return m_emptyString; }
        HashSet<StringImpl*>& atomicStringTable() { return *m_atomicStringTable; }
        ThreadTimers& threadTimers() { return *m_threadTimers; }

#if USE(ICU_UNICODE) || USE(GLIB_ICU_UNICODE_HYBRID)
        ICUConverterWrapper& cachedConverterICU() { return *m_cachedConverterICU; }
#endif

#if PLATFORM(MAC)
        TECConverterWrapper& cachedConverterTEC() { return *m_cachedConverterTEC; }
#endif

    private:
        StringImpl* m_emptyString;
        HashSet<StringImpl*>* m_atomicStringTable;
        EventNames* m_eventNames;
        ThreadTimers* m_threadTimers;

#ifndef NDEBUG
        bool m_isMainThread;
#endif

#if USE(ICU_UNICODE) || USE(GLIB_ICU_UNICODE_HYBRID)
        ICUConverterWrapper* m_cachedConverterICU;
#endif

#if PLATFORM(MAC)
        TECConverterWrapper* m_cachedConverterTEC;
#endif
    };

    ThreadGlobalData& threadGlobalData();

} // namespace WebCore

#endif // ThreadGlobalData_h
