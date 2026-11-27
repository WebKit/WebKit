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

#include <WebGPU/WebGPUCAPIHandles.h>
#include <optional>
#include <thread>
#include <wtf/CompletionHandler.h>
#include <wtf/threads/BinarySemaphore.h>

// wgpuDeviceGetLostFuture(), for the devices of the C API and the devices of the C++ API that are
// given to the C API.
namespace TestWebKitAPI {

// A future that does not complete fails the test instead of hanging it.
static constexpr uint64_t waitTimeoutNanoseconds = 10'000'000'000;

// What a wait for one future returned.
struct WaitResult {
    WGPUWaitStatus status;
    bool completed;
};

static WaitResult wait(WGPUInstance instance, WGPUFuture future, uint64_t timeoutNanoseconds = 0)
{
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT;
    info.future = future;
    auto status = wgpuInstanceWaitAny(instance, 1, &info, timeoutNanoseconds);
    return { status, !!info.completed };
}

static void expectPending(WGPUInstance instance, WGPUFuture future)
{
    auto result = wait(instance, future);
    EXPECT_EQ(result.status, WGPUWaitStatus_TimedOut);
    EXPECT_FALSE(result.completed);
}

static void expectCompleted(WGPUInstance instance, WGPUFuture future, uint64_t timeoutNanoseconds = 0)
{
    auto result = wait(instance, future, timeoutNanoseconds);
    EXPECT_EQ(result.status, WGPUWaitStatus_Success);
    EXPECT_TRUE(result.completed);
}

class WebGPUDeviceLostFuture : public testing::Test {
public:
    void SetUp() override
    {
        // A device of the C API.
        static constexpr auto timedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
        WGPUInstanceDescriptor instanceDescriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
        instanceDescriptor.requiredFeatureCount = 1;
        instanceDescriptor.requiredFeatures = &timedWaitAny;
        m_instance = wgpuCreateInstance(&instanceDescriptor);
        ASSERT_TRUE(m_instance);
        m_device = requestDevice(m_adapter);
        ASSERT_TRUE(m_device);

        // A device of the C++ API, which is given to the C API. Without a scheduleWork, the instance
        // runs its work on the thread that schedules it.
        m_cppInstance = WebGPU::createInstance({ });
        ASSERT_TRUE(m_cppInstance);
        BinarySemaphore done;
        protect(m_cppInstance)->requestAdapter({ }, [&](RefPtr<WebGPU::Adapter>&& adapter) {
            m_cppAdapter = WTF::move(adapter);
            done.signal();
        });
        ASSERT_TRUE(done.waitFor(10_s));
        ASSERT_TRUE(m_cppAdapter);
        protect(m_cppAdapter)->requestDevice({ }, [&](std::expected<Ref<WebGPU::Device>, String>&& device) {
            if (device)
                m_cppDevice = WTF::move(*device);
            done.signal();
        });
        ASSERT_TRUE(done.waitFor(10_s));
        ASSERT_TRUE(m_cppDevice);
        m_cppInstanceHandle = WebGPU::retainCAPIHandle(*protect(m_cppInstance));
        m_cppDeviceHandle = WebGPU::retainCAPIHandle(*protect(m_cppDevice));
    }

    void TearDown() override
    {
        if (m_cppDeviceHandle)
            wgpuDeviceRelease(m_cppDeviceHandle);
        if (m_cppInstanceHandle)
            wgpuInstanceRelease(m_cppInstanceHandle);
        m_cppDevice = nullptr;
        m_cppAdapter = nullptr;
        if (RefPtr instance = std::exchange(m_cppInstance, nullptr))
            instance->destroy();

        if (m_device)
            wgpuDeviceRelease(m_device);
        if (m_adapter)
            wgpuAdapterRelease(m_adapter);
        if (m_instance)
            wgpuInstanceRelease(m_instance);
    }

protected:
    // A device of the C API, from a new adapter of m_instance, as an adapter creates one device.
    // The caller releases the adapter.
    WGPUDevice requestDevice(WGPUAdapter& adapter)
    {
        WGPURequestAdapterCallbackInfo adapterCallbackInfo = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
        adapterCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
        adapterCallbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* userdata1, void*) {
            EXPECT_EQ(status, WGPURequestAdapterStatus_Success);
            *static_cast<WGPUAdapter*>(userdata1) = adapter;
        };
        adapterCallbackInfo.userdata1 = &adapter;
        expectCompleted(m_instance, wgpuInstanceRequestAdapter(m_instance, nullptr, adapterCallbackInfo), waitTimeoutNanoseconds);
        if (!adapter)
            return nullptr;

        WGPUDevice device = nullptr;
        WGPURequestDeviceCallbackInfo callbackInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
        callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
        callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
            EXPECT_EQ(status, WGPURequestDeviceStatus_Success);
            *static_cast<WGPUDevice*>(userdata1) = device;
        };
        callbackInfo.userdata1 = &device;
        expectCompleted(m_instance, wgpuAdapterRequestDevice(adapter, nullptr, callbackInfo), waitTimeoutNanoseconds);
        return device;
    }

    WGPUInstance m_instance { nullptr };
    WGPUAdapter m_adapter { nullptr };
    WGPUDevice m_device { nullptr };

    RefPtr<WebGPU::Instance> m_cppInstance;
    RefPtr<WebGPU::Adapter> m_cppAdapter;
    RefPtr<WebGPU::Device> m_cppDevice;
    WGPUInstance m_cppInstanceHandle { nullptr };
    WGPUDevice m_cppDeviceHandle { nullptr };
};

TEST_F(WebGPUDeviceLostFuture, CompletesWhenTheDeviceIsDestroyed)
{
    auto future = wgpuDeviceGetLostFuture(m_device);
    EXPECT_NE(future.id, 0u);
    expectPending(m_instance, future);

    wgpuDeviceDestroy(m_device);
    expectCompleted(m_instance, future);
}

TEST_F(WebGPUDeviceLostFuture, CompletesWhenTheCppAPIDeviceIsDestroyedThroughTheCAPI)
{
    auto future = wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    EXPECT_NE(future.id, 0u);
    expectPending(m_cppInstanceHandle, future);

    wgpuDeviceDestroy(m_cppDeviceHandle);
    EXPECT_TRUE(protect(m_cppDevice)->isLost());
    expectCompleted(m_cppInstanceHandle, future);
}

TEST_F(WebGPUDeviceLostFuture, CompletesWhenTheCppAPIDeviceIsDestroyedThroughTheCppAPI)
{
    auto future = wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    expectPending(m_cppInstanceHandle, future);

    protect(m_cppDevice)->destroy();
    expectCompleted(m_cppInstanceHandle, future);
}

TEST_F(WebGPUDeviceLostFuture, IsTheSameFutureForEveryCall)
{
    for (auto [instance, device] : { std::pair { m_instance, m_device }, std::pair { m_cppInstanceHandle, m_cppDeviceHandle } }) {
        auto future = wgpuDeviceGetLostFuture(device);
        EXPECT_EQ(wgpuDeviceGetLostFuture(device).id, future.id);
        wgpuDeviceDestroy(device);
        EXPECT_EQ(wgpuDeviceGetLostFuture(device).id, future.id);
        expectCompleted(instance, future);
        EXPECT_EQ(wgpuDeviceGetLostFuture(device).id, future.id);
    }
}

TEST_F(WebGPUDeviceLostFuture, StaysCompletedWhileTheDeviceLives)
{
    for (auto [instance, device] : { std::pair { m_instance, m_device }, std::pair { m_cppInstanceHandle, m_cppDeviceHandle } }) {
        auto future = wgpuDeviceGetLostFuture(device);
        wgpuDeviceDestroy(device);
        expectCompleted(instance, future);
        // Unlike the futures of callbacks, waitAny() reports it every time.
        expectCompleted(instance, future);
        expectCompleted(instance, future, waitTimeoutNanoseconds);
    }
}

TEST_F(WebGPUDeviceLostFuture, CompletesAtOnceForADeviceThatIsAlreadyLost)
{
    wgpuDeviceDestroy(m_device);
    expectCompleted(m_instance, wgpuDeviceGetLostFuture(m_device));

    protect(m_cppDevice)->destroy();
    expectCompleted(m_cppInstanceHandle, wgpuDeviceGetLostFuture(m_cppDeviceHandle));
}

TEST_F(WebGPUDeviceLostFuture, CompletesWhenTheDeviceIsGone)
{
    auto future = wgpuDeviceGetLostFuture(m_device);
    expectPending(m_instance, future);
    wgpuDeviceRelease(std::exchange(m_device, nullptr));
    expectCompleted(m_instance, future);

    auto cppFuture = wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    expectPending(m_cppInstanceHandle, cppFuture);
    wgpuDeviceRelease(std::exchange(m_cppDeviceHandle, nullptr));
    m_cppDevice = nullptr;
    expectCompleted(m_cppInstanceHandle, cppFuture);
}

TEST_F(WebGPUDeviceLostFuture, TimedWaitTimesOutWhileTheDeviceLives)
{
    for (auto [instance, device] : { std::pair { m_instance, m_device }, std::pair { m_cppInstanceHandle, m_cppDeviceHandle } }) {
        auto result = wait(instance, wgpuDeviceGetLostFuture(device), 10'000'000);
        EXPECT_EQ(result.status, WGPUWaitStatus_TimedOut);
        EXPECT_FALSE(result.completed);
    }
}

TEST_F(WebGPUDeviceLostFuture, TimedWaitCompletesWhenAnotherThreadDestroysTheDevice)
{
    for (auto [instance, device] : { std::pair { m_instance, m_device }, std::pair { m_cppInstanceHandle, m_cppDeviceHandle } }) {
        auto future = wgpuDeviceGetLostFuture(device);
        std::thread thread([device] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            wgpuDeviceDestroy(device);
        });
        expectCompleted(instance, future, waitTimeoutNanoseconds);
        thread.join();
    }
}

TEST_F(WebGPUDeviceLostFuture, IsPerDevice)
{
    WGPUAdapter otherAdapter = nullptr;
    WGPUDevice otherDevice = requestDevice(otherAdapter);
    ASSERT_TRUE(otherDevice);
    auto future = wgpuDeviceGetLostFuture(m_device);
    auto otherFuture = wgpuDeviceGetLostFuture(otherDevice);
    EXPECT_NE(future.id, otherFuture.id);

    wgpuDeviceDestroy(otherDevice);
    expectPending(m_instance, future);
    expectCompleted(m_instance, otherFuture);

    wgpuDeviceDestroy(m_device);
    expectCompleted(m_instance, future);
    wgpuDeviceRelease(otherDevice);
    wgpuAdapterRelease(otherAdapter);
}

TEST_F(WebGPUDeviceLostFuture, IsOnTheInstanceOfTheDevice)
{
    auto future = wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    wgpuDeviceDestroy(m_cppDeviceHandle);
    // The futures of other instances have other identifiers.
    auto result = wait(m_instance, future);
    EXPECT_FALSE(result.completed);
    expectCompleted(m_cppInstanceHandle, future);
}

// The future takes the device-lost callback of the device of the C++ API.
TEST_F(WebGPUDeviceLostFuture, GettingTheFutureCompletesTheDeviceLostCallback)
{
    Ref device = *m_cppDevice;
    std::optional<WebGPU::DeviceLostReason> reason;
    device->resolveDeviceLostPromise([&](WebGPU::DeviceLostReason lostReason, String&&) {
        reason = lostReason;
    });
    EXPECT_FALSE(reason);

    auto future = wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    // The device is not lost, as setting a callback completes the current one.
    ASSERT_TRUE(reason);
    EXPECT_EQ(*reason, WebGPU::DeviceLostReason::Destroyed);
    EXPECT_FALSE(device->isLost());
    expectPending(m_cppInstanceHandle, future);

    // Getting the future again does not take the callback again.
    reason = std::nullopt;
    wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    expectPending(m_cppInstanceHandle, future);
}

TEST_F(WebGPUDeviceLostFuture, SettingTheDeviceLostCallbackCompletesTheFuture)
{
    Ref device = *m_cppDevice;
    auto future = wgpuDeviceGetLostFuture(m_cppDeviceHandle);
    std::optional<WebGPU::DeviceLostReason> reason;
    device->resolveDeviceLostPromise([&](WebGPU::DeviceLostReason lostReason, String&&) {
        reason = lostReason;
    });
    EXPECT_FALSE(device->isLost());
    expectCompleted(m_cppInstanceHandle, future);

    device->destroy();
    ASSERT_TRUE(reason);
    EXPECT_EQ(*reason, WebGPU::DeviceLostReason::Destroyed);
}

} // namespace TestWebKitAPI

#endif // !PLATFORM(IOS_FAMILY_SIMULATOR)
