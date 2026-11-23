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

// WebGPU has no adapter on the simulator.
#if !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <WebGPU/WebGPUCpp.h>
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

// WebGPU::Device::isLost() of the WebGPU C++ API, as the device-lost callback sees it.
namespace TestWebKitAPI {

namespace {

// The work that the instance schedules, on any thread, runs on the test's thread.
class DeviceLostScheduledWork final : public ThreadSafeRefCounted<DeviceLostScheduledWork> {
public:
    static Ref<DeviceLostScheduledWork> create() { return adoptRef(*new DeviceLostScheduledWork); }

    void append(Function<void()>&& work)
    {
        Locker locker { m_lock };
        m_work.append(WTF::move(work));
        m_condition.notifyAll();
    }

    // Runs the scheduled work until isDone() holds. Fails the test, rather than hangs it, when no
    // work is scheduled for too long.
    template<typename Predicate>
    void runWorkUntil(const Predicate& isDone)
    {
        while (!isDone()) {
            Deque<Function<void()>> work;
            {
                Locker locker { m_lock };
                bool hasWork = m_condition.waitFor(m_lock, 10_s, [&] {
                    assertIsHeld(m_lock);
                    return !m_work.isEmpty();
                });
                if (!hasWork) {
                    ADD_FAILURE() << "No work was scheduled";
                    return;
                }
                work = std::exchange(m_work, { });
            }
            while (!work.isEmpty())
                work.takeFirst()();
        }
    }

private:
    DeviceLostScheduledWork() = default;

    Lock m_lock;
    Condition m_condition;
    Deque<Function<void()>> m_work WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace

class WebGPUDeviceLost : public testing::Test {
public:
    void SetUp() override
    {
        m_instance = WebGPU::createInstance(WebGPU::InstanceDescriptor {
            .scheduleWork = [scheduledWork = m_scheduledWork](Function<void()>&& work) {
                scheduledWork->append(WTF::move(work));
            },
            .webProcessResourceOwner = std::nullopt,
        });
        ASSERT_TRUE(m_instance);

        bool done = false;
        protect(m_instance)->requestAdapter({ }, [&](RefPtr<WebGPU::Adapter>&& adapter) {
            m_adapter = WTF::move(adapter);
            done = true;
        });
        m_scheduledWork->runWorkUntil([&] {
            return done;
        });
        ASSERT_TRUE(m_adapter);

        done = false;
        protect(m_adapter)->requestDevice({ }, [&](std::expected<Ref<WebGPU::Device>, String>&& device) {
            if (device)
                m_device = WTF::move(*device);
            done = true;
        });
        m_scheduledWork->runWorkUntil([&] {
            return done;
        });
        ASSERT_TRUE(m_device);
    }

    void TearDown() override
    {
        m_device = nullptr;
        m_adapter = nullptr;
        if (RefPtr instance = std::exchange(m_instance, nullptr))
            instance->destroy();
    }

protected:
    // What a device-lost callback saw: the reason, and isLost() of the device while it ran.
    struct DeviceLost {
        WebGPU::DeviceLostReason reason;
        bool isLost;
    };

    // A device-lost callback that records what it saw.
    CompletionHandler<void(WebGPU::DeviceLostReason, String&&)> recordDeviceLost(std::optional<DeviceLost>& result)
    {
        return [this, &result](WebGPU::DeviceLostReason reason, String&&) {
            result = DeviceLost { reason, protect(m_device)->isLost() };
        };
    }

    const Ref<DeviceLostScheduledWork> m_scheduledWork { DeviceLostScheduledWork::create() };
    RefPtr<WebGPU::Instance> m_instance;
    RefPtr<WebGPU::Adapter> m_adapter;
    RefPtr<WebGPU::Device> m_device;
};

TEST_F(WebGPUDeviceLost, CallbackSeesDestroyedDeviceLost)
{
    Ref device = *m_device;
    std::optional<DeviceLost> deviceLost;
    device->resolveDeviceLostPromise(recordDeviceLost(deviceLost));
    EXPECT_FALSE(deviceLost);
    EXPECT_FALSE(device->isLost());

    device->destroy();
    ASSERT_TRUE(deviceLost);
    EXPECT_EQ(deviceLost->reason, WebGPU::DeviceLostReason::Destroyed);
    EXPECT_TRUE(deviceLost->isLost);
    EXPECT_TRUE(device->isLost());
}

TEST_F(WebGPUDeviceLost, ReplacedCallbackSeesDeviceNotLost)
{
    Ref device = *m_device;
    std::optional<DeviceLost> firstDeviceLost;
    device->resolveDeviceLostPromise(recordDeviceLost(firstDeviceLost));

    // Setting another callback completes the first one, without losing the device.
    std::optional<DeviceLost> secondDeviceLost;
    device->resolveDeviceLostPromise(recordDeviceLost(secondDeviceLost));
    ASSERT_TRUE(firstDeviceLost);
    EXPECT_FALSE(firstDeviceLost->isLost);
    EXPECT_FALSE(secondDeviceLost);
    EXPECT_FALSE(device->isLost());

    device->destroy();
    ASSERT_TRUE(secondDeviceLost);
    EXPECT_EQ(secondDeviceLost->reason, WebGPU::DeviceLostReason::Destroyed);
    EXPECT_TRUE(secondDeviceLost->isLost);
}

} // namespace TestWebKitAPI

#endif // !PLATFORM(IOS_FAMILY_SIMULATOR)
