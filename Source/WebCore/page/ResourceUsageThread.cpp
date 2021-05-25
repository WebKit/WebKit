/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ResourceUsageThread.h"

#if ENABLE(RESOURCE_USAGE)

#include "CommonVM.h"
#include "JSDOMWindow.h"
#include <thread>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>

namespace WebCore {

ResourceUsageThread::ResourceUsageThread()
{
}

ResourceUsageThread& ResourceUsageThread::singleton()
{
    static NeverDestroyed<ResourceUsageThread> resourceUsageThread;
    return resourceUsageThread;
}

void ResourceUsageThread::addObserver(void* key, ResourceUsageCollectionMode mode, std::function<void (const ResourceUsageData&)> function)
{
    auto& resourceUsageThread = ResourceUsageThread::singleton();
    resourceUsageThread.createThreadIfNeeded();

    {
        Locker locker { resourceUsageThread.m_observersLock };
        bool wasEmpty = resourceUsageThread.m_observers.isEmpty();
        resourceUsageThread.m_observers.set(key, std::make_pair(mode, function));

        resourceUsageThread.recomputeCollectionMode();

        if (wasEmpty) {
            resourceUsageThread.platformSaveStateBeforeStarting();
            resourceUsageThread.m_condition.notifyAll();
        }
    }
}

void ResourceUsageThread::removeObserver(void* key)
{
    auto& resourceUsageThread = ResourceUsageThread::singleton();

    {
        Locker locker { resourceUsageThread.m_observersLock };
        resourceUsageThread.m_observers.remove(key);

        resourceUsageThread.recomputeCollectionMode();
    }
}

void ResourceUsageThread::waitUntilObservers()
{
    Locker locker { m_observersLock };
    while (m_observers.isEmpty()) {
        m_condition.wait(m_observersLock);

        // Wait a bit after waking up for the first time.
        WTF::sleep(10_ms);
    }
}

void ResourceUsageThread::notifyObservers(ResourceUsageData&& data)
{
    callOnMainThread([data = WTFMove(data)]() mutable {
        Vector<std::pair<ResourceUsageCollectionMode, std::function<void (const ResourceUsageData&)>>> pairs;

        {
            auto& resourceUsageThread = ResourceUsageThread::singleton();
            Locker locker { resourceUsageThread.m_observersLock };
            pairs = copyToVector(resourceUsageThread.m_observers.values());
        }

        for (auto& pair : pairs)
            pair.second(data);
    });
}

void ResourceUsageThread::recomputeCollectionMode()
{
    m_collectionMode = None;

    for (auto& pair : m_observers.values())
        m_collectionMode = static_cast<ResourceUsageCollectionMode>(m_collectionMode | pair.first);
}

void ResourceUsageThread::createThreadIfNeeded()
{
    if (m_thread)
        return;

    m_vm = &commonVM();
    m_thread = Thread::create("WebCore: ResourceUsage", [this] {
        threadBody();
    });
}

NO_RETURN void ResourceUsageThread::threadBody()
{
    // Wait a bit after waking up for the first time.
    WTF::sleep(10_ms);
    
    while (true) {
        // Only do work if we have observers.
        waitUntilObservers();

        auto start = WallTime::now();

        ResourceUsageData data;
        ResourceUsageCollectionMode mode = m_collectionMode;
        if (mode & CPU)
            platformCollectCPUData(m_vm, data);
        if (mode & Memory)
            platformCollectMemoryData(m_vm, data);

        notifyObservers(WTFMove(data));

        // NOTE: Web Inspector expects this interval to be 500ms (CPU / Memory timelines),
        // so if this interval changes Web Inspector may need to change.
        auto duration = WallTime::now() - start;
        auto difference = 500_ms - duration;
        WTF::sleep(difference);
    }
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
