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
#include "FontCache.h"
#include "MIMETypeRegistry.h"
#include "QualifiedNameCache.h"
#include "ThreadTimers.h"
#include <wtf/MainThread.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Threading.h>
#include <wtf/text/StringImpl.h>

namespace WebCore {

ThreadGlobalData::ThreadGlobalData()
    : m_threadTimers(makeUnique<ThreadTimers>())
#ifndef NDEBUG
    , m_isMainThread(isMainThread())
#endif
{
}

ThreadGlobalData::~ThreadGlobalData() = default;

void ThreadGlobalData::destroy()
{
    m_destroyed = true;

    PAL::ThreadGlobalData::destroy();

    // The ThreadGlobalData destructor is called under the TLS destruction
    // callback, which is later than when the static atom table is destroyed.
    // To avoid AtomStrings being destroyed after the table, we clear objects
    // that have AtomStrings in them.
    m_eventNames = nullptr;
    m_threadTimers = nullptr;
    m_qualifiedNameCache = nullptr;
    m_fontCache = nullptr;
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

void ThreadGlobalData::initializeCachedResourceRequestInitiators()
{
    ASSERT(!m_cachedResourceRequestInitiators);
    m_cachedResourceRequestInitiators = makeUnique<CachedResourceRequestInitiators>();
}

void ThreadGlobalData::initializeEventNames()
{
    ASSERT(!m_eventNames);
    m_eventNames = EventNames::create();
}

void ThreadGlobalData::initializeQualifiedNameCache()
{
    ASSERT(!m_qualifiedNameCache);
    m_qualifiedNameCache = makeUnique<QualifiedNameCache>();
}

void ThreadGlobalData::initializeMimeTypeRegistryThreadGlobalData()
{
    ASSERT(!m_MIMETypeRegistryThreadGlobalData);
    m_MIMETypeRegistryThreadGlobalData = MIMETypeRegistry::createMIMETypeRegistryThreadGlobalData();
}

void ThreadGlobalData::initializeFontCache()
{
    ASSERT(!m_fontCache);
    m_fontCache = makeUnique<FontCache>();
}

} // namespace WebCore

namespace PAL {

ThreadGlobalData& threadGlobalData()
{
    return WebCore::threadGlobalData();
}

} // namespace PAL
