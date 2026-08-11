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

#pragma once

#include <wtf/CompletionHandler.h>
#include <wtf/ContinuousApproximateTime.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceMonitorThrottler;

class ResourceMonitorThrottlerHolder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ResourceMonitorThrottlerHolder, WTF::DestructionThread::Main> {
public:
    WEBCORE_EXPORT static Ref<ResourceMonitorThrottlerHolder> create();
    WEBCORE_EXPORT static Ref<ResourceMonitorThrottlerHolder> create(size_t count, Seconds duration, size_t maxHosts);
    WEBCORE_EXPORT static Ref<ResourceMonitorThrottlerHolder> create(const String& databaseDirectoryPath);
    WEBCORE_EXPORT static Ref<ResourceMonitorThrottlerHolder> create(const String& databaseDirectoryPath, size_t count, Seconds duration, size_t maxHosts);

    WEBCORE_EXPORT ~ResourceMonitorThrottlerHolder();

    WEBCORE_EXPORT void tryAccess(const String& host, ContinuousApproximateTime, CompletionHandler<void(bool)>&&);
    WEBCORE_EXPORT void clearAllData(CompletionHandler<void()>&&);

    WEBCORE_EXPORT void setCountPerDuration(size_t, Seconds);

private:
    // Shared WorkQueue is used to prevent race condition when delete and create Throttler for same database file.
    static Ref<WorkQueue> sharedWorkQueueSingleton();

    ResourceMonitorThrottlerHolder(const String& databasePath, size_t count, Seconds duration, size_t maxHosts);

    std::unique_ptr<ResourceMonitorThrottler> m_throttler;
};

}
