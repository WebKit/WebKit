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
#include "ResourceMonitorThrottler.h"

#include "Logging.h"
#include "ResourceMonitorPersistence.h"
#include <wtf/MainThread.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(CONTENT_EXTENSIONS)

#define RESOURCEMONITOR_RELEASE_LOG(fmt, ...) RELEASE_LOG(ResourceMonitoring, "ResourceMonitorThrottler::" fmt, ##__VA_ARGS__)

namespace WebCore {

ResourceMonitorThrottler::ResourceMonitorThrottler(String&& path, size_t count, Seconds duration, size_t maxHosts)
    : m_config { count, duration, maxHosts }
{
    ASSERT(!isMainThread());

    ASSERT(maxHosts >= 1);
    RESOURCEMONITOR_RELEASE_LOG("Throttler is launching, count=%zu, duration=%f, maxHosts=%zu", count, duration.value(), maxHosts);

    RESOURCEMONITOR_RELEASE_LOG("Opening persistence for throttler.");
    auto persistence = makeUnique<ResourceMonitorPersistence>();

    if (!persistence->openDatabase(WTFMove(path))) {
        RESOURCEMONITOR_RELEASE_LOG("Failed to setup persistence for throttler.");
        return;
    }

    m_persistence = WTFMove(persistence);
    RESOURCEMONITOR_RELEASE_LOG("Success to setup persistence for throttler.");

    auto now = ContinuousApproximateTime::now();
    m_persistence->deleteExpiredRecords(now, m_config.duration);

    bool changed = false;
    for (const auto& record : m_persistence->importRecords()) {
        auto& throttler = throttlerForHost(record.host);
        changed |= throttler.tryAccessAndUpdateHistory(record.time, m_config);
    }
    if (changed)
        maintainHosts(now);
}

ResourceMonitorThrottler::~ResourceMonitorThrottler()
{
    ASSERT(!isMainThread());

    if (m_persistence) {
        RESOURCEMONITOR_RELEASE_LOG("Closing persistence for throttler.");
        m_persistence->deleteExpiredRecords(ContinuousApproximateTime::now(), m_config.duration);
        m_persistence = nullptr;
    }
}

auto ResourceMonitorThrottler::throttlerForHost(const String& host) -> AccessThrottler&
{
    ASSERT(!isMainThread());

    return m_throttlersByHost.ensure(host, [] {
        return AccessThrottler { };
    }).iterator->value;
}

void ResourceMonitorThrottler::removeOldestThrottler()
{
    ASSERT(!isMainThread());

    auto oldest = ContinuousApproximateTime::infinity();
    String oldestKey;

    for (auto it : m_throttlersByHost) {
        auto time = it.value.newestAccessTime();
        if (time < oldest) {
            oldest = time;
            oldestKey = it.key;
        }
    }
    ASSERT(!oldestKey.isNull());
    m_throttlersByHost.remove(oldestKey);
}

bool ResourceMonitorThrottler::tryAccess(const String& host, ContinuousApproximateTime time)
{
    ASSERT(!isMainThread());
    ASSERT(!host.isEmpty());

    auto& throttler = throttlerForHost(host);
    bool wasGranted = throttler.tryAccessAndUpdateHistory(time, m_config);

    if (wasGranted) {
        RESOURCEMONITOR_RELEASE_LOG("Throttler granted for unloading the resource for %" PUBLIC_LOG_STRING ".", host.utf8().data());

        maintainHosts(time);
        if (m_persistence)
            m_persistence->recordAccess(host, time);
    } else
        RESOURCEMONITOR_RELEASE_LOG("Throttler denied for unloading the resource for %" PUBLIC_LOG_STRING ".", host.utf8().data());

    return wasGranted;
}

void ResourceMonitorThrottler::clearAllData()
{
    ASSERT(!isMainThread());

    if (m_persistence)
        m_persistence->deleteAllRecords();
}

void ResourceMonitorThrottler::maintainHosts(ContinuousApproximateTime time)
{
    ASSERT(!isMainThread());

    if (m_throttlersByHost.size() <= m_config.maxHosts)
        return;

    // Update and remove all expired access times. If no entry in throttler, remove it.
    m_throttlersByHost.removeIf([&](auto& it) -> bool {
        return it.value.tryExpire(time, m_config);
    });

    // If there are still too many hosts, then remove oldest one.
    while (m_throttlersByHost.size() > m_config.maxHosts)
        removeOldestThrottler();

    ASSERT(m_throttlersByHost.size() <= m_config.maxHosts);
}

void ResourceMonitorThrottler::setCountPerDuration(size_t count, Seconds duration)
{
    ASSERT(!isMainThread());

    m_config.count = count;
    m_config.duration = duration;
}

bool ResourceMonitorThrottler::AccessThrottler::tryAccessAndUpdateHistory(ContinuousApproximateTime time, const Config& config)
{
    ASSERT(!isMainThread());

    tryExpire(time, config);
    if (m_accessTimes.size() >= config.count)
        return false;

    m_accessTimes.enqueue(time);
    if (m_newestAccessTime < time)
        m_newestAccessTime = time;

    return true;
}

ContinuousApproximateTime ResourceMonitorThrottler::AccessThrottler::oldestAccessTime() const
{
    ASSERT(!isMainThread());

    return m_accessTimes.peek();
}

bool ResourceMonitorThrottler::AccessThrottler::tryExpire(ContinuousApproximateTime time, const Config& config)
{
    ASSERT(!isMainThread());

    auto expirationTime = time - config.duration;

    while (!m_accessTimes.isEmpty()) {
        if (oldestAccessTime() > expirationTime)
            return false;

        m_accessTimes.dequeue();
    }
    // Tell caller that the queue is empty.
    return true;
}

} // namespace WebCore

#undef RESOURCEMONITOR_RELEASE_LOG

#endif
