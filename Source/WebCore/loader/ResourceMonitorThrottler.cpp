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
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

#if ENABLE(CONTENT_EXTENSIONS)

#define RESOURCEMONITOR_RELEASE_LOG(fmt, ...) RELEASE_LOG(ResourceLoading, "%p - ResourceMonitorThrottler::" fmt, this, ##__VA_ARGS__)

namespace WebCore {

static constexpr size_t defaultThrottleAccessCount = 5;
static constexpr Seconds defaultThrottleDuration = 24_h;
static constexpr size_t defaultMaxHosts = 100;

Ref<ResourceMonitorThrottler> ResourceMonitorThrottler::create()
{
    return create(defaultThrottleAccessCount, defaultThrottleDuration, defaultMaxHosts);
}

Ref<ResourceMonitorThrottler> ResourceMonitorThrottler::create(size_t count, Seconds duration, size_t maxHosts)
{
    return adoptRef(*new ResourceMonitorThrottler(count, duration, maxHosts));
}

ResourceMonitorThrottler::ResourceMonitorThrottler(size_t count, Seconds duration, size_t maxHosts)
    : m_config { count, duration, maxHosts }
{
    ASSERT(maxHosts >= 1);
    RESOURCEMONITOR_RELEASE_LOG("Initialized with count: %zu, duration: %.f, maxHosts: %zu", count, duration.value(), maxHosts);
}

auto ResourceMonitorThrottler::throttlerForHost(const String& host) -> AccessThrottler&
{
    return m_throttlersByHost.ensure(host, [] {
        return AccessThrottler { };
    }).iterator->value;
}

void ResourceMonitorThrottler::removeOldestThrottler()
{
    auto oldest = ApproximateTime::infinity();
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

bool ResourceMonitorThrottler::tryAccess(const String& host, ApproximateTime time)
{
    if (host.isEmpty())
        return false;

    auto& throttler = throttlerForHost(host);
    auto result = throttler.tryAccessAndUpdateHistory(time, m_config);

    if (m_throttlersByHost.size() > m_config.maxHosts) {
        // Update and remove all expired access times. If no entry in throttler, remove it.
        m_throttlersByHost.removeIf([protectedThis = Ref { *this }, &time](auto& it) -> bool {
            return it.value.tryExpire(time, protectedThis->m_config);
        });

        // If there are still too many hosts, then remove oldest one.
        while (m_throttlersByHost.size() > m_config.maxHosts)
            removeOldestThrottler();
    }
    ASSERT(m_throttlersByHost.size() <= m_config.maxHosts);

    return result;
}

bool ResourceMonitorThrottler::AccessThrottler::tryAccessAndUpdateHistory(ApproximateTime time, const Config& config)
{
    tryExpire(time, config);
    if (m_accessTimes.size() >= config.count)
        return false;

    m_accessTimes.enqueue(time);
    if (m_newestAccessTime < time)
        m_newestAccessTime = time;

    return true;
}

ApproximateTime ResourceMonitorThrottler::AccessThrottler::oldestAccessTime() const
{
    return m_accessTimes.peek();
}

bool ResourceMonitorThrottler::AccessThrottler::tryExpire(ApproximateTime time, const Config& config)
{
    auto expirationTime = time - config.duration;

    while (!m_accessTimes.isEmpty()) {
        if (oldestAccessTime() > expirationTime)
            return false;

        m_accessTimes.dequeue();
    }
    // Tell caller that the queue is empty.
    return true;
}

void ResourceMonitorThrottler::setCountPerDuration(size_t count, Seconds duration)
{
    m_config.count = count;
    m_config.duration = duration;
}

} // namespace WebCore

#undef RESOURCEMONITOR_RELEASE_LOG

#endif
