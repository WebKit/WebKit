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

void ResourceUsageThread::addObserver(void* key, std::function<void (const ResourceUsageData&)> function)
{
    auto& resourceUsageThread = ResourceUsageThread::singleton();
    resourceUsageThread.createThreadIfNeeded();

    {
        LockHolder locker(resourceUsageThread.m_lock);
        bool wasEmpty = resourceUsageThread.m_observers.isEmpty();
        resourceUsageThread.m_observers.set(key, function);

        if (wasEmpty)
            resourceUsageThread.m_condition.notifyAll();
    }
}

void ResourceUsageThread::removeObserver(void* key)
{
    auto& resourceUsageThread = ResourceUsageThread::singleton();

    {
        LockHolder locker(resourceUsageThread.m_lock);
        resourceUsageThread.m_observers.remove(key);
    }
}

void ResourceUsageThread::waitUntilObservers()
{
    LockHolder locker(m_lock);
    while (m_observers.isEmpty())
        m_condition.wait(m_lock);
}

void ResourceUsageThread::notifyObservers(ResourceUsageData&& data)
{
    callOnMainThread([data = WTFMove(data)]() mutable {
        Vector<std::function<void (const ResourceUsageData&)>> functions;
        
        {
            auto& resourceUsageThread = ResourceUsageThread::singleton();
            LockHolder locker(resourceUsageThread.m_lock);
            copyValuesToVector(resourceUsageThread.m_observers, functions);
        }

        for (auto& function : functions)
            function(data);
    });
}

void ResourceUsageThread::createThreadIfNeeded()
{
    if (m_threadIdentifier)
        return;

    m_vm = &JSDOMWindow::commonVM();
    m_threadIdentifier = createThread(threadCallback, this, "WebCore: ResourceUsage");
}

void ResourceUsageThread::threadCallback(void* resourceUsageThread)
{
    static_cast<ResourceUsageThread*>(resourceUsageThread)->threadBody();
}

NO_RETURN void ResourceUsageThread::threadBody()
{
    while (true) {
        // Only do work if we have observers.
        waitUntilObservers();

        auto start = std::chrono::system_clock::now();

        ResourceUsageData data;
        platformThreadBody(m_vm, data);
        notifyObservers(WTFMove(data));

        auto duration = std::chrono::system_clock::now() - start;
        auto difference = 500ms - duration;
        std::this_thread::sleep_for(difference);
    }
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_USAGE)
