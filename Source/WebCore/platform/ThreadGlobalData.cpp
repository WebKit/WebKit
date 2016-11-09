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
#include "TextCodecICU.h"
#include "ThreadTimers.h"
#include <wtf/MainThread.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Threading.h>
#include <wtf/WTFThreadData.h>
#include <wtf/text/StringImpl.h>

#if PLATFORM(MAC)
#include "TextCodecMac.h"
#endif

namespace WebCore {

ThreadSpecific<ThreadGlobalData>* ThreadGlobalData::staticData;
#if USE(WEB_THREAD)
ThreadGlobalData* ThreadGlobalData::sharedMainThreadStaticData;
#endif

ThreadGlobalData::ThreadGlobalData()
    : m_cachedResourceRequestInitiators(std::make_unique<CachedResourceRequestInitiators>())
    , m_eventNames(EventNames::create())
    , m_threadTimers(std::make_unique<ThreadTimers>())
#ifndef NDEBUG
    , m_isMainThread(isMainThread())
#endif
    , m_cachedConverterICU(std::make_unique<ICUConverterWrapper>())
#if PLATFORM(MAC)
    , m_cachedConverterTEC(std::make_unique<TECConverterWrapper>())
#endif
{
    // This constructor will have been called on the main thread before being called on
    // any other thread, and is only called once per thread - this makes this a convenient
    // point to call methods that internally perform a one-time initialization that is not
    // threadsafe.
    wtfThreadData();
    StringImpl::empty();
}

ThreadGlobalData::~ThreadGlobalData()
{
}

void ThreadGlobalData::destroy()
{
#if PLATFORM(MAC)
    m_cachedConverterTEC = nullptr;
#endif

    m_cachedConverterICU = nullptr;

    m_eventNames = nullptr;
    m_threadTimers = nullptr;
}

#if USE(WEB_THREAD)
void ThreadGlobalData::setWebCoreThreadData()
{
    ASSERT(isWebThread());
    ASSERT(&threadGlobalData() != ThreadGlobalData::sharedMainThreadStaticData);

    // Set WebThread's ThreadGlobalData object to be the same as the main UI thread.
    ThreadGlobalData::staticData->replace(ThreadGlobalData::sharedMainThreadStaticData);

    ASSERT(&threadGlobalData() == ThreadGlobalData::sharedMainThreadStaticData);
}
#endif

ThreadGlobalData& threadGlobalData() 
{
#if USE(WEB_THREAD)
    if (UNLIKELY(!ThreadGlobalData::staticData)) {
        ThreadGlobalData::staticData = new ThreadSpecific<ThreadGlobalData>;
        // WebThread and main UI thread need to share the same object. Save it in a static
        // here, the WebThread will pick it up in setWebCoreThreadData().
        if (pthread_main_np())
            ThreadGlobalData::sharedMainThreadStaticData = *ThreadGlobalData::staticData;
    }
    return **ThreadGlobalData::staticData;
#else
    if (!ThreadGlobalData::staticData)
        ThreadGlobalData::staticData = new ThreadSpecific<ThreadGlobalData>;
    return **ThreadGlobalData::staticData;
#endif
}

} // namespace WebCore
