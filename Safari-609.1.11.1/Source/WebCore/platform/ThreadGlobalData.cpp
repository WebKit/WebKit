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

#include "config.h"
#include "ThreadGlobalData.h"

#include "CachedResourceRequestInitiators.h"
#include "EventNames.h"
#include "QualifiedNameCache.h"
#include "TextCodecICU.h"
#include "ThreadTimers.h"
#include <wtf/MainThread.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Threading.h>
#include <wtf/text/StringImpl.h>

namespace WebCore {

ThreadGlobalData::ThreadGlobalData()
    : m_cachedResourceRequestInitiators(makeUnique<CachedResourceRequestInitiators>())
    , m_eventNames(EventNames::create())
    , m_threadTimers(makeUnique<ThreadTimers>())
    , m_qualifiedNameCache(makeUnique<QualifiedNameCache>())
#ifndef NDEBUG
    , m_isMainThread(isMainThread())
#endif
    , m_cachedConverterICU(makeUnique<ICUConverterWrapper>())
{
    // This constructor will have been called on the main thread before being called on
    // any other thread, and is only called once per thread - this makes this a convenient
    // point to call methods that internally perform a one-time initialization that is not
    // threadsafe.
    Thread::current();
}

ThreadGlobalData::~ThreadGlobalData() = default;

void ThreadGlobalData::destroy()
{
    m_cachedConverterICU = nullptr;

    m_eventNames = nullptr;
    m_threadTimers = nullptr;
    m_qualifiedNameCache = nullptr;
}

#if USE(WEB_THREAD)
static ThreadSpecific<RefPtr<ThreadGlobalData>>* staticData { nullptr };
static ThreadGlobalData* sharedMainThreadStaticData { nullptr };

void ThreadGlobalData::setWebCoreThreadData()
{
    ASSERT(isWebThread());
    ASSERT(&threadGlobalData() != sharedMainThreadStaticData);

    // Set WebThread's ThreadGlobalData object to be the same as the main UI thread.
    **staticData = adoptRef(sharedMainThreadStaticData);

    ASSERT(&threadGlobalData() == sharedMainThreadStaticData);
}

ThreadGlobalData& threadGlobalData()
{
    if (UNLIKELY(!staticData)) {
        staticData = new ThreadSpecific<RefPtr<ThreadGlobalData>>;
        auto& result = **staticData;
        ASSERT(!result);
        result = adoptRef(new ThreadGlobalData);
        // WebThread and main UI thread need to share the same object. Save it in a static
        // here, the WebThread will pick it up in setWebCoreThreadData().
        if (pthread_main_np()) {
            sharedMainThreadStaticData = result.get();
            result->ref();
        }
        return *result;
    }

    auto& result = **staticData;
    if (!result)
        result = adoptRef(new ThreadGlobalData);
    return *result;
}

#else

static ThreadSpecific<ThreadGlobalData>* staticData { nullptr };

ThreadGlobalData& threadGlobalData()
{
    if (UNLIKELY(!staticData))
        staticData = new ThreadSpecific<ThreadGlobalData>;
    return **staticData;
}

#endif

} // namespace WebCore
