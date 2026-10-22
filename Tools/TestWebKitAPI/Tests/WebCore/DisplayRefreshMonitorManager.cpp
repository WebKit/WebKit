/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/DisplayRefreshMonitor.h>
#include <WebCore/DisplayRefreshMonitorClient.h>
#include <WebCore/DisplayRefreshMonitorFactory.h>
#include <WebCore/DisplayRefreshMonitorManager.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace TestWebKitAPI {

class TestDisplayRefreshMonitor final : public WebCore::DisplayRefreshMonitor {
public:
    static Ref<TestDisplayRefreshMonitor> create(WebCore::PlatformDisplayID displayID, bool canStart)
    {
        return adoptRef(*new TestDisplayRefreshMonitor(displayID, canStart));
    }

    void setCanStart(bool canStart) { m_canStart = canStart; }

private:
    TestDisplayRefreshMonitor(WebCore::PlatformDisplayID displayID, bool canStart)
        : WebCore::DisplayRefreshMonitor(displayID)
        , m_canStart(canStart)
    {
    }

    bool startNotificationMechanism() final { return m_canStart; }
    void stopNotificationMechanism() final { }

    bool m_canStart { true };
};

class TestDisplayRefreshMonitorFactory final : public WebCore::DisplayRefreshMonitorFactory {
public:
    explicit TestDisplayRefreshMonitorFactory(bool canStart)
        : m_canStart(canStart)
    {
    }

    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID displayID) final
    {
        m_monitor = TestDisplayRefreshMonitor::create(displayID, m_canStart);
        return m_monitor;
    }

    TestDisplayRefreshMonitor* monitor() const { return m_monitor.get(); }

private:
    bool m_canStart { true };
    RefPtr<TestDisplayRefreshMonitor> m_monitor;
};

class TestDisplayRefreshMonitorClient final : public WebCore::DisplayRefreshMonitorClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TestDisplayRefreshMonitorClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(TestDisplayRefreshMonitorClient);
public:
    explicit TestDisplayRefreshMonitorClient(WebCore::DisplayRefreshMonitorFactory& factory)
        : m_factory(factory)
    {
    }

private:
    void displayRefreshFired() final { }
    WebCore::DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() const final { return &m_factory; }

    WebCore::DisplayRefreshMonitorFactory& m_factory;
};

// The manager is a process-wide singleton that caches monitors by display ID, so each test uses its own ID.
constexpr WebCore::PlatformDisplayID successDisplayID = 0x7e570001;
constexpr WebCore::PlatformDisplayID failureDisplayID = 0x7e570002;
constexpr WebCore::PlatformDisplayID recoveryDisplayID = 0x7e570003;

TEST(DisplayRefreshMonitorManager, ScheduleAnimationMarksClientScheduledOnSuccess)
{
    TestDisplayRefreshMonitorFactory factory(true);
    auto client = makeUnique<TestDisplayRefreshMonitorClient>(factory);
    client->setDisplayID(successDisplayID);

    EXPECT_FALSE(client->isScheduled());
    EXPECT_TRUE(WebCore::DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*client));
    EXPECT_TRUE(client->isScheduled());
}

TEST(DisplayRefreshMonitorManager, ScheduleAnimationDoesNotMarkClientScheduledOnFailure)
{
    TestDisplayRefreshMonitorFactory factory(false);
    auto client = makeUnique<TestDisplayRefreshMonitorClient>(factory);
    client->setDisplayID(failureDisplayID);

    EXPECT_FALSE(WebCore::DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*client));
    EXPECT_FALSE(client->isScheduled());
}

TEST(DisplayRefreshMonitorManager, ScheduleAnimationClearsStaleScheduledStateOnFailure)
{
    TestDisplayRefreshMonitorFactory factory(true);
    auto client = makeUnique<TestDisplayRefreshMonitorClient>(factory);
    client->setDisplayID(recoveryDisplayID);

    EXPECT_TRUE(WebCore::DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*client));
    EXPECT_TRUE(client->isScheduled());

    // The monitor is still scheduled, so make the next request fail by stopping it and disabling restart.
    RefPtr monitor = factory.monitor();
    ASSERT_TRUE(monitor);
    monitor->stop();
    factory.monitor()->setCanStart(false);

    EXPECT_FALSE(WebCore::DisplayRefreshMonitorManager::sharedManager().scheduleAnimation(*client));
    EXPECT_FALSE(client->isScheduled());
}

} // namespace TestWebKitAPI
