/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ResourceMonitorThrottlerHolder.h"

#include "ResourceMonitorThrottler.h"
#include "SQLiteDatabase.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {

Ref<WorkQueue> ResourceMonitorThrottlerHolder::sharedWorkQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue = WorkQueue::create("ResourceMonitorThrottlerHolder Work Queue"_s);
    return workQueue.get();
}

Ref<ResourceMonitorThrottlerHolder> ResourceMonitorThrottlerHolder::create()
{
    return create(ResourceMonitorThrottler::defaultThrottleAccessCount, ResourceMonitorThrottler::defaultThrottleDuration, ResourceMonitorThrottler::defaultMaxHosts);
}

Ref<ResourceMonitorThrottlerHolder> ResourceMonitorThrottlerHolder::create(size_t count, Seconds duration, size_t maxHosts)
{
    return create(emptyString(), count, duration, maxHosts);
}

Ref<ResourceMonitorThrottlerHolder> ResourceMonitorThrottlerHolder::create(const String& databaseDirectoryPath)
{
    return create(databaseDirectoryPath, ResourceMonitorThrottler::defaultThrottleAccessCount, ResourceMonitorThrottler::defaultThrottleDuration, ResourceMonitorThrottler::defaultMaxHosts);
}

Ref<ResourceMonitorThrottlerHolder> ResourceMonitorThrottlerHolder::create(const String& databaseDirectoryPath, size_t count, Seconds duration, size_t maxHosts)
{
    return adoptRef(*new ResourceMonitorThrottlerHolder(databaseDirectoryPath, count, duration, maxHosts));
}

ResourceMonitorThrottlerHolder::ResourceMonitorThrottlerHolder(const String& databaseDirectoryPath, size_t count, Seconds duration, size_t maxHosts)
{
    ASSERT(isMainThread());

    sharedWorkQueueSingleton()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, path = crossThreadCopy(databaseDirectoryPath), count, duration, maxHosts] mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_throttler = makeUnique<ResourceMonitorThrottler>(WTFMove(path), count, duration, maxHosts);
    });
}

ResourceMonitorThrottlerHolder::~ResourceMonitorThrottlerHolder()
{
    ASSERT(isMainThread());

    sharedWorkQueueSingleton()->dispatch([container = WTFMove(m_throttler)] { });
}

void ResourceMonitorThrottlerHolder::tryAccess(const String& host, ContinuousApproximateTime time, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(isMainThread());

    if (host.isEmpty()) {
        completionHandler(false);
        return;
    }

    sharedWorkQueueSingleton()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, host = crossThreadCopy(host), time, completionHandler = WTFMove(completionHandler)] mutable {
        RefPtr protectedThis = weakThis.get();
        bool wasGranted = protectedThis && protectedThis->m_throttler->tryAccess(host, time);

        callOnMainThread([wasGranted, completionHandler = WTFMove(completionHandler)] mutable {
            completionHandler(wasGranted);
        });
    });
}

void ResourceMonitorThrottlerHolder::clearAllData(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());

    sharedWorkQueueSingleton()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTFMove(completionHandler)] mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_throttler->clearAllData();

        callOnMainThread(WTFMove(completionHandler));
    });
}

void ResourceMonitorThrottlerHolder::setCountPerDuration(size_t count, Seconds duration)
{
    ASSERT(isMainThread());

    sharedWorkQueueSingleton()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, count, duration] mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_throttler->setCountPerDuration(count, duration);
    });
}

} // namespace WebCore

#endif
