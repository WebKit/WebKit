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

#if ENABLE(CONTENT_EXTENSIONS)

#include "Utilities.h"
#include <WebCore/ResourceMonitorThrottler.h>

namespace TestWebKitAPI {

using namespace WebCore;

class ResouceMonitorTest : public testing::Test {
public:
    void SetUp() override
    {
        m_reference = ApproximateTime::now();
    }

protected:
    ApproximateTime m_reference;

    ApproximateTime now()
    {
        auto t = m_reference;
        m_reference += 1_ms;
        return t;
    }

    ApproximateTime later(Seconds delta)
    {
        m_reference += delta;
        return m_reference;
    }
};

TEST_F(ResouceMonitorTest, ThrottlerBasic)
{
    ResourceMonitorThrottler throttler { /* size */ 2, /* duration */ 1_s, /* maxHosts */ 1 };

    auto host = "example.com"_s;

    // first access must be okay.
    EXPECT_TRUE(throttler.tryAccess(host, now()));
    // second one is alse okay.
    EXPECT_TRUE(throttler.tryAccess(host, now()));
    // but third one is not okay because size is 2.
    EXPECT_FALSE(throttler.tryAccess(host, now()));

    // after duration, it should be okay.
    EXPECT_TRUE(throttler.tryAccess(host, later(1_s)));
}

TEST_F(ResouceMonitorTest, ThrottlerMaxHosts)
{
    ResourceMonitorThrottler throttler { /* size */ 2, /* duration */ 1_s, /* maxHosts */ 2 };

    auto host1 = "h1.example.com"_s;
    auto host2 = "h2.example.com"_s;
    auto host3 = "h3.example.com"_s;

    // make host1 inaccessible.
    EXPECT_TRUE(throttler.tryAccess(host1, now()));
    EXPECT_TRUE(throttler.tryAccess(host1, now()));
    EXPECT_FALSE(throttler.tryAccess(host1, now()));

    // host2 is accessible and still host1 is not.
    EXPECT_TRUE(throttler.tryAccess(host2, now()));
    EXPECT_FALSE(throttler.tryAccess(host1, now()));

    // host3 is accessible and host1 is now also accessible because of the max host.
    EXPECT_TRUE(throttler.tryAccess(host3, now()));
    EXPECT_TRUE(throttler.tryAccess(host1, now()));
}

TEST_F(ResouceMonitorTest, ThrottlerLeastRecentAccessedHostWillBeRemoved)
{
    ResourceMonitorThrottler throttler { /* size */ 2, /* duration */ 1_s, /* maxHosts */ 2 };

    auto host1 = "h1.example.com"_s;
    auto host2 = "h2.example.com"_s;
    auto host3 = "h3.example.com"_s;

    // host1 is the oldest access.
    EXPECT_TRUE(throttler.tryAccess(host1, now()));

    // make host2 inaccessible
    EXPECT_TRUE(throttler.tryAccess(host2, now()));
    EXPECT_TRUE(throttler.tryAccess(host2, now()));
    EXPECT_FALSE(throttler.tryAccess(host2, now()));

    // make host1 inaccessible and this is the most recent access.
    EXPECT_TRUE(throttler.tryAccess(host1, now()));
    EXPECT_FALSE(throttler.tryAccess(host1, now()));

    // host3 is accessible. In this access, lest recent host is removed.
    EXPECT_TRUE(throttler.tryAccess(host3, now()));
    // host1 is the oldest, but recent than host2. Still it is blocked.
    EXPECT_FALSE(throttler.tryAccess(host1, now()));
    // host2 is the least recent access and removed in host3 access. So it is accessible.
    EXPECT_TRUE(throttler.tryAccess(host2, now()));
}

TEST_F(ResouceMonitorTest, ThrottlerEmptyHostname)
{
    ResourceMonitorThrottler throttler { /* size */ 2, /* duration */ 1_s, /* maxHosts */ 2 };

    auto emptyHost = ""_s;

    // Accessing with an empty hostname should not crash.
    EXPECT_FALSE(throttler.tryAccess(emptyHost, now()));
}

} // namespace TestWebKitAPI

#endif // ENABLE(CONTENT_EXTENSIONS)
