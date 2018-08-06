/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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
 *
 */

#ifndef ThreadGlobalData_h
#define ThreadGlobalData_h

#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/StringHash.h>

namespace JSC {
class ExecState;
}

namespace WebCore {

    class QualifiedNameCache;
    class ThreadTimers;

    struct CachedResourceRequestInitiators;
    struct EventNames;
    struct ICUConverterWrapper;

#if USE(WEB_THREAD)
    class ThreadGlobalData : public ThreadSafeRefCounted<ThreadGlobalData> {
#else
    class ThreadGlobalData {
#endif
        WTF_MAKE_NONCOPYABLE(ThreadGlobalData);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        WEBCORE_EXPORT ThreadGlobalData();
        WEBCORE_EXPORT ~ThreadGlobalData();
        void destroy(); // called on workers to clean up the ThreadGlobalData before the thread exits.

        const CachedResourceRequestInitiators& cachedResourceRequestInitiators() { return *m_cachedResourceRequestInitiators; }
        EventNames& eventNames() { return *m_eventNames; }
        ThreadTimers& threadTimers() { return *m_threadTimers; }
        QualifiedNameCache& qualifiedNameCache() { return *m_qualifiedNameCache; }

        ICUConverterWrapper& cachedConverterICU() { return *m_cachedConverterICU; }

        JSC::ExecState* currentState() const { return m_currentState; }
        void setCurrentState(JSC::ExecState* state) { m_currentState = state; }

#if USE(WEB_THREAD)
        void setWebCoreThreadData();
#endif

        bool isInRemoveAllEventListeners() const { return m_isInRemoveAllEventListeners; }
        void setIsInRemoveAllEventListeners(bool value) { m_isInRemoveAllEventListeners = value; }

    private:
        std::unique_ptr<CachedResourceRequestInitiators> m_cachedResourceRequestInitiators;
        std::unique_ptr<EventNames> m_eventNames;
        std::unique_ptr<ThreadTimers> m_threadTimers;
        std::unique_ptr<QualifiedNameCache> m_qualifiedNameCache;
        JSC::ExecState* m_currentState { nullptr };

#ifndef NDEBUG
        bool m_isMainThread;
#endif

        bool m_isInRemoveAllEventListeners { false };

        std::unique_ptr<ICUConverterWrapper> m_cachedConverterICU;

        WEBCORE_EXPORT friend ThreadGlobalData& threadGlobalData();
    };

#if USE(WEB_THREAD)
WEBCORE_EXPORT ThreadGlobalData& threadGlobalData();
#else
WEBCORE_EXPORT ThreadGlobalData& threadGlobalData() PURE_FUNCTION;
#endif
    
} // namespace WebCore

#endif // ThreadGlobalData_h
