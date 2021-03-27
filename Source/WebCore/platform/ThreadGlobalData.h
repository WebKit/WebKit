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
class CallFrame;
class JSGlobalObject;
}

namespace WebCore {

class QualifiedNameCache;
class ThreadTimers;

struct CachedResourceRequestInitiators;
struct EventNames;
struct ICUConverterWrapper;
struct MIMETypeRegistryThreadGlobalData;

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

    const CachedResourceRequestInitiators& cachedResourceRequestInitiators()
    {
        if (UNLIKELY(!m_cachedResourceRequestInitiators))
            initializeCachedResourceRequestInitiators();
        return *m_cachedResourceRequestInitiators;
    }
    EventNames& eventNames()
    {
        if (UNLIKELY(!m_eventNames))
            initializeEventNames();
        return *m_eventNames;
    }
    QualifiedNameCache& qualifiedNameCache()
    {
        if (UNLIKELY(!m_qualifiedNameCache))
            initializeQualifiedNameCache();
        return *m_qualifiedNameCache;
    }
    const MIMETypeRegistryThreadGlobalData& mimeTypeRegistryThreadGlobalData()
    {
        if (UNLIKELY(!m_MIMETypeRegistryThreadGlobalData))
            initializeMimeTypeRegistryThreadGlobalData();
        return *m_MIMETypeRegistryThreadGlobalData;
    }

    ThreadTimers& threadTimers() { return *m_threadTimers; }
    ICUConverterWrapper& cachedConverterICU() { return *m_cachedConverterICU; }

    JSC::JSGlobalObject* currentState() const { return m_currentState; }
    void setCurrentState(JSC::JSGlobalObject* state) { m_currentState = state; }

#if USE(WEB_THREAD)
    void setWebCoreThreadData();
#endif

    bool isInRemoveAllEventListeners() const { return m_isInRemoveAllEventListeners; }
    void setIsInRemoveAllEventListeners(bool value) { m_isInRemoveAllEventListeners = value; }

private:
    WEBCORE_EXPORT void initializeCachedResourceRequestInitiators();
    WEBCORE_EXPORT void initializeEventNames();
    WEBCORE_EXPORT void initializeQualifiedNameCache();
    WEBCORE_EXPORT void initializeMimeTypeRegistryThreadGlobalData();

    std::unique_ptr<CachedResourceRequestInitiators> m_cachedResourceRequestInitiators;
    std::unique_ptr<EventNames> m_eventNames;
    std::unique_ptr<ThreadTimers> m_threadTimers;
    std::unique_ptr<QualifiedNameCache> m_qualifiedNameCache;
    JSC::JSGlobalObject* m_currentState { nullptr };
    std::unique_ptr<ICUConverterWrapper> m_cachedConverterICU;
    std::unique_ptr<MIMETypeRegistryThreadGlobalData> m_MIMETypeRegistryThreadGlobalData;

#ifndef NDEBUG
    bool m_isMainThread;
#endif

    bool m_isInRemoveAllEventListeners { false };

    WEBCORE_EXPORT friend ThreadGlobalData& threadGlobalData();
};

#if USE(WEB_THREAD)
WEBCORE_EXPORT ThreadGlobalData& threadGlobalData();
#else
WEBCORE_EXPORT ThreadGlobalData& threadGlobalData() PURE_FUNCTION;
#endif

} // namespace WebCore

#endif // ThreadGlobalData_h
