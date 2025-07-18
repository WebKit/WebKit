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
#include <wtf/HashMap.h>
#include <wtf/PriorityQueue.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceMonitorPersistence;

class ResourceMonitorThrottler final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(ResourceMonitorThrottler);
public:
    ResourceMonitorThrottler(String&& databaseDirectoryPath, size_t count, Seconds duration, size_t maxHosts);
    ~ResourceMonitorThrottler();

    void openDatabase(String&& path);
    void closeDatabase();
    void recordAccess(const String& host, ContinuousApproximateTime);

    bool tryAccess(const String& host, ContinuousApproximateTime);
    void clearAllData();

    void setCountPerDuration(size_t count, Seconds duration);

    static constexpr size_t defaultThrottleAccessCount = 5;
    static constexpr Seconds defaultThrottleDuration = 24_h;
    static constexpr size_t defaultMaxHosts = 100;

private:
    struct Config {
        size_t count;
        Seconds duration;
        size_t maxHosts;
    };

    class AccessThrottler final {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(AccessThrottler);
    public:
        AccessThrottler() = default;

        bool tryAccessAndUpdateHistory(ContinuousApproximateTime, const Config&);
        bool tryExpire(ContinuousApproximateTime, const Config&);
        ContinuousApproximateTime oldestAccessTime() const;
        ContinuousApproximateTime newestAccessTime() const { return m_newestAccessTime; }

    private:
        void removeExpired(ContinuousApproximateTime);

        PriorityQueue<ContinuousApproximateTime> m_accessTimes;
        ContinuousApproximateTime m_newestAccessTime { -ContinuousApproximateTime::infinity() };
    };

    AccessThrottler& throttlerForHost(const String& host);
    void removeExpiredThrottler();
    void removeOldestThrottler();
    void maintainHosts(ContinuousApproximateTime);

    Config m_config;
    HashMap<String, AccessThrottler> m_throttlersByHost;
    std::unique_ptr<ResourceMonitorPersistence> m_persistence;
};

}
