/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "HRTFDatabaseLoader.h"

#include "HRTFDatabase.h"
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

// Keeps track of loaders on a per-sample-rate basis.
static HashMap<double, HRTFDatabaseLoader*>& loaderMap()
{
    static NeverDestroyed<HashMap<double, HRTFDatabaseLoader*>> loaderMap;
    return loaderMap;
}

Ref<HRTFDatabaseLoader> HRTFDatabaseLoader::createAndLoadAsynchronouslyIfNecessary(float sampleRate)
{
    ASSERT(isMainThread());

    if (RefPtr<HRTFDatabaseLoader> loader = loaderMap().get(sampleRate)) {
        ASSERT(sampleRate == loader->databaseSampleRate());
        return loader.releaseNonNull();
    }

    auto loader = adoptRef(*new HRTFDatabaseLoader(sampleRate));
    loaderMap().add(sampleRate, loader.ptr());

    loader->loadAsynchronously();

    return loader;
}

HRTFDatabaseLoader::HRTFDatabaseLoader(float sampleRate)
    : m_databaseSampleRate(sampleRate)
{
    ASSERT(isMainThread());
}

HRTFDatabaseLoader::~HRTFDatabaseLoader()
{
    ASSERT(isMainThread());

    waitForLoaderThreadCompletion();
    m_hrtfDatabase = nullptr;

    // Remove ourself from the map.
    loaderMap().remove(m_databaseSampleRate);
}

void HRTFDatabaseLoader::load()
{
    ASSERT(!isMainThread());
    if (!m_hrtfDatabase.get()) {
        // Load the default HRTF database.
        m_hrtfDatabase = makeUnique<HRTFDatabase>(m_databaseSampleRate);
    }
}

void HRTFDatabaseLoader::loadAsynchronously()
{
    ASSERT(isMainThread());

    Locker locker { m_threadLock };
    
    if (!m_hrtfDatabase.get() && !m_databaseLoaderThread) {
        // Start the asynchronous database loading process.
        m_databaseLoaderThread = Thread::create("HRTF database loader", [this] {
            load();
        });
    }
}

bool HRTFDatabaseLoader::isLoaded() const
{
    return m_hrtfDatabase.get();
}

void HRTFDatabaseLoader::waitForLoaderThreadCompletion()
{
    Locker locker { m_threadLock };
    
    // waitForThreadCompletion() should not be called twice for the same thread.
    if (m_databaseLoaderThread)
        m_databaseLoaderThread->waitForCompletion();
    m_databaseLoaderThread = nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
