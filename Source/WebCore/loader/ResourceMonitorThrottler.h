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

#include <wtf/ApproximateTime.h>
#include <wtf/HashMap.h>
#include <wtf/PriorityQueue.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceMonitorThrottler final : public RefCounted<ResourceMonitorThrottler> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT static Ref<ResourceMonitorThrottler> create();
    WEBCORE_EXPORT static Ref<ResourceMonitorThrottler> create(size_t count, Seconds duration, size_t maxHosts);

    WEBCORE_EXPORT bool tryAccess(const String& host, ApproximateTime = ApproximateTime::now());

    WEBCORE_EXPORT void setCountPerDuration(size_t, Seconds);

private:
    struct Config {
        size_t count;
        Seconds duration;
        size_t maxHosts;
    };

    class AccessThrottler final {
    public:
        AccessThrottler() = default;

        bool tryAccessAndUpdateHistory(ApproximateTime, const Config&);
        bool tryExpire(ApproximateTime, const Config&);
        ApproximateTime oldestAccessTime() const;
        ApproximateTime newestAccessTime() const { return m_newestAccessTime; }

    private:
        void removeExpired(ApproximateTime);

        PriorityQueue<ApproximateTime> m_accessTimes;
        ApproximateTime m_newestAccessTime { -ApproximateTime::infinity() };
    };

    ResourceMonitorThrottler(size_t count, Seconds duration, size_t maxHosts);

    AccessThrottler& throttlerForHost(const String& host);
    void removeExpiredThrottler();
    void removeOldestThrottler();

    Config m_config;
    HashMap<String, AccessThrottler> m_throttlersByHost;
};

}
