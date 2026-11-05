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

#include <WebGPU/WebGPU.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// The device-lost callback of the WebGPU C API, WGPUDeviceDescriptor.deviceLostCallbackInfo.
namespace TestWebKitAPI {

struct DeviceLoss {
    WGPUDevice device;
    WGPUDeviceLostReason reason;
    std::string message;
    void* userdata2;
    std::thread::id thread;
};

using DeviceLosses = std::vector<DeviceLoss>;

static int userdata2Marker;

static void waitFor(WGPUInstance instance, WGPUFuture future)
{
    WGPUFutureWaitInfo waitInfo = WGPU_FUTURE_WAIT_INFO_INIT;
    waitInfo.future = future;
    EXPECT_EQ(wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX), WGPUWaitStatus_Success);
    EXPECT_TRUE(waitInfo.completed);
}

static bool isCompleted(WGPUInstance instance, WGPUFuture future, uint64_t timeoutNS = 0)
{
    WGPUFutureWaitInfo waitInfo = WGPU_FUTURE_WAIT_INFO_INIT;
    waitInfo.future = future;
    return wgpuInstanceWaitAny(instance, 1, &waitInfo, timeoutNS) == WGPUWaitStatus_Success && waitInfo.completed;
}

static WGPUInstance createInstance()
{
    // The waits with a timeout are timed waits.
    static constexpr auto timedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
    WGPUInstanceDescriptor descriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
    descriptor.requiredFeatureCount = 1;
    descriptor.requiredFeatures = &timedWaitAny;
    return wgpuCreateInstance(&descriptor);
}

static WGPUAdapter requestAdapter(WGPUInstance instance)
{
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterCallbackInfo callbackInfo = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* userdata1, void*) {
        EXPECT_EQ(status, WGPURequestAdapterStatus_Success);
        *static_cast<WGPUAdapter*>(userdata1) = adapter;
    };
    callbackInfo.userdata1 = &adapter;
    waitFor(instance, wgpuInstanceRequestAdapter(instance, nullptr, callbackInfo));
    return adapter;
}

static WGPUDeviceDescriptor deviceDescriptor(WGPUCallbackMode mode, DeviceLosses& losses)
{
    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    descriptor.deviceLostCallbackInfo.mode = mode;
    descriptor.deviceLostCallbackInfo.callback = [](const WGPUDevice* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2) {
        static_cast<DeviceLosses*>(userdata1)->push_back({ *device, reason, std::string(message.data, message.length), userdata2, std::this_thread::get_id() });
    };
    descriptor.deviceLostCallbackInfo.userdata1 = &losses;
    descriptor.deviceLostCallbackInfo.userdata2 = &userdata2Marker;
    return descriptor;
}

static WGPUDevice requestDevice(WGPUInstance instance, WGPUAdapter adapter, const WGPUDeviceDescriptor& descriptor, WGPURequestDeviceStatus expectedStatus = WGPURequestDeviceStatus_Success)
{
    struct Result {
        WGPURequestDeviceStatus expectedStatus;
        WGPUDevice device;
    } result { expectedStatus, nullptr };
    WGPURequestDeviceCallbackInfo callbackInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
        auto& result = *static_cast<Result*>(userdata1);
        EXPECT_EQ(status, result.expectedStatus);
        result.device = device;
    };
    callbackInfo.userdata1 = &result;
    waitFor(instance, wgpuAdapterRequestDevice(adapter, &descriptor, callbackInfo));
    return result.device;
}

class WebGPUDeviceLostCallback : public testing::Test {
public:
    void SetUp() override
    {
        m_instance = createInstance();
        ASSERT_TRUE(m_instance);
        m_adapter = requestAdapter(m_instance);
        ASSERT_TRUE(m_adapter);
    }

    void TearDown() override
    {
        if (m_adapter)
            wgpuAdapterRelease(m_adapter);
        if (m_instance)
            wgpuInstanceRelease(m_instance);
    }

protected:
    WGPUInstance m_instance { nullptr };
    WGPUAdapter m_adapter { nullptr };
};

TEST_F(WebGPUDeviceLostCallback, IsCalledOnceWhenTheDeviceIsDestroyed)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowSpontaneous, losses));
    ASSERT_TRUE(device);
    EXPECT_TRUE(losses.empty());

    wgpuDeviceDestroy(device);
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, device);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);
    EXPECT_FALSE(losses[0].message.empty());
    EXPECT_EQ(losses[0].userdata2, &userdata2Marker);
    EXPECT_EQ(losses[0].thread, std::this_thread::get_id());
    EXPECT_TRUE(isCompleted(m_instance, wgpuDeviceGetLostFuture(device)));

    wgpuDeviceDestroy(device);
    wgpuDeviceRelease(device);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledOnTheThreadThatLosesTheDevice)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowSpontaneous, losses));
    ASSERT_TRUE(device);

    std::thread::id otherThread;
    std::thread thread([&] {
        otherThread = std::this_thread::get_id();
        wgpuDeviceDestroy(device);
    });
    thread.join();
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].thread, otherThread);

    wgpuDeviceRelease(device);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledWithTheReasonAndMessageOfTheLoss)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowSpontaneous, losses));
    ASSERT_TRUE(device);

    static constexpr char message[] = "The device is lost for testing.";
    wgpuDeviceForceLoss(device, WGPUDeviceLostReason_Unknown, { message, sizeof(message) - 1 });
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, device);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Unknown);
    EXPECT_EQ(losses[0].message, message);

    // A lost device is not lost again.
    wgpuDeviceForceLoss(device, WGPUDeviceLostReason_Unknown, { message, sizeof(message) - 1 });
    wgpuDeviceDestroy(device);
    wgpuDeviceRelease(device);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledWithoutTheDeviceWhenTheLastReferenceIsReleased)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowSpontaneous, losses));
    ASSERT_TRUE(device);

    // An object of the device does not keep the device from being destroyed.
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst;
    bufferDescriptor.size = 4;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    wgpuDeviceAddRef(device);
    wgpuDeviceRelease(device);
    EXPECT_TRUE(losses.empty());

    wgpuDeviceRelease(device);
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, nullptr);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);

    wgpuBufferRelease(buffer);
    wgpuQueueRelease(queue);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledInProcessEventsWithAllowProcessEvents)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowProcessEvents, losses));
    ASSERT_TRUE(device);

    wgpuDeviceDestroy(device);
    EXPECT_TRUE(losses.empty());

    wgpuInstanceProcessEvents(m_instance);
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, device);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);

    wgpuDeviceRelease(device);
    wgpuInstanceProcessEvents(m_instance);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledWhenTheLostFutureIsWaitedFor)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_WaitAnyOnly, losses));
    ASSERT_TRUE(device);
    WGPUFuture lostFuture = wgpuDeviceGetLostFuture(device);
    EXPECT_NE(lostFuture.id, 0u);
    EXPECT_FALSE(isCompleted(m_instance, lostFuture));

    wgpuDeviceDestroy(device);
    EXPECT_TRUE(losses.empty());

    static constexpr uint64_t tenSeconds = 10'000'000'000;
    EXPECT_TRUE(isCompleted(m_instance, lostFuture, tenSeconds));
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, device);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);

    wgpuDeviceRelease(device);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledWithoutTheDeviceWhenTheLastReferenceIsReleasedBeforeTheCallback)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowProcessEvents, losses));
    ASSERT_TRUE(device);

    wgpuDeviceDestroy(device);
    wgpuDeviceRelease(device);
    EXPECT_TRUE(losses.empty());

    wgpuInstanceProcessEvents(m_instance);
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, nullptr);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);
}

TEST_F(WebGPUDeviceLostCallback, IsCancelledWhenTheInstanceIsReleasedBeforeTheCallback)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowProcessEvents, losses));
    ASSERT_TRUE(device);

    wgpuDeviceDestroy(device);
    EXPECT_TRUE(losses.empty());

    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, nullptr);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_CallbackCancelled);
    EXPECT_FALSE(losses[0].message.empty());

    wgpuDeviceRelease(device);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledWithoutTheDeviceWhenAnObjectKeepsTheReleasedDeviceAlive)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowProcessEvents, losses));
    ASSERT_TRUE(device);
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.usage = WGPUBufferUsage_CopyDst;
    bufferDescriptor.size = 4;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    wgpuDeviceDestroy(device);
    wgpuDeviceRelease(device);
    wgpuInstanceProcessEvents(m_instance);
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, nullptr);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);

    wgpuBufferRelease(buffer);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCancelledWhenTheInstanceIsReleasedWhileTheDeviceIsAlive)
{
    WGPUInstance otherInstance = createInstance();
    ASSERT_TRUE(otherInstance);
    WGPUAdapter otherAdapter = requestAdapter(otherInstance);
    ASSERT_TRUE(otherAdapter);

    DeviceLosses processEventsLosses;
    WGPUDevice processEventsDevice = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowProcessEvents, processEventsLosses));
    ASSERT_TRUE(processEventsDevice);
    DeviceLosses waitAnyOnlyLosses;
    WGPUDevice waitAnyOnlyDevice = requestDevice(otherInstance, otherAdapter, deviceDescriptor(WGPUCallbackMode_WaitAnyOnly, waitAnyOnlyLosses));
    ASSERT_TRUE(waitAnyOnlyDevice);

    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    ASSERT_EQ(processEventsLosses.size(), 1u);
    EXPECT_EQ(processEventsLosses[0].device, nullptr);
    EXPECT_EQ(processEventsLosses[0].reason, WGPUDeviceLostReason_CallbackCancelled);
    EXPECT_FALSE(processEventsLosses[0].message.empty());

    wgpuInstanceRelease(otherInstance);
    ASSERT_EQ(waitAnyOnlyLosses.size(), 1u);
    EXPECT_EQ(waitAnyOnlyLosses[0].device, nullptr);
    EXPECT_EQ(waitAnyOnlyLosses[0].reason, WGPUDeviceLostReason_CallbackCancelled);

    // The device is lost once.
    wgpuDeviceDestroy(processEventsDevice);
    wgpuDeviceRelease(processEventsDevice);
    wgpuDeviceRelease(waitAnyOnlyDevice);
    wgpuAdapterRelease(otherAdapter);
    EXPECT_EQ(processEventsLosses.size(), 1u);
    EXPECT_EQ(waitAnyOnlyLosses.size(), 1u);
}

// As in Dawn, which cancels only the callbacks that it can no longer call.
TEST_F(WebGPUDeviceLostCallback, IsNotCancelledWhenTheInstanceIsReleasedWithAllowSpontaneous)
{
    DeviceLosses losses;
    WGPUDevice device = requestDevice(m_instance, m_adapter, deviceDescriptor(WGPUCallbackMode_AllowSpontaneous, losses));
    ASSERT_TRUE(device);

    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    EXPECT_TRUE(losses.empty());

    wgpuDeviceDestroy(device);
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, device);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_Destroyed);

    wgpuDeviceRelease(device);
    EXPECT_EQ(losses.size(), 1u);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledOnceTheDeviceIsLost)
{
    // A lost device pops the error scopes that it does not have. webgpu.h leaves calling the API from
    // a WGPUCallbackMode_AllowSpontaneous callback undefined; this test relies on WebGPU.framework
    // allowing it.
    struct Result {
        bool called { false };
        WGPUPopErrorScopeStatus status { WGPUPopErrorScopeStatus_Error };
    } result;
    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    descriptor.deviceLostCallbackInfo.callback = [](const WGPUDevice* device, WGPUDeviceLostReason, WGPUStringView, void* userdata1, void*) {
        auto& result = *static_cast<Result*>(userdata1);
        result.called = true;
        WGPUPopErrorScopeCallbackInfo callbackInfo = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
        callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        callbackInfo.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType, WGPUStringView, void* userdata1, void*) {
            static_cast<Result*>(userdata1)->status = status;
        };
        callbackInfo.userdata1 = userdata1;
        wgpuDevicePopErrorScope(*device, callbackInfo);
    };
    descriptor.deviceLostCallbackInfo.userdata1 = &result;
    WGPUDevice device = requestDevice(m_instance, m_adapter, descriptor);
    ASSERT_TRUE(device);

    wgpuDeviceDestroy(device);
    EXPECT_TRUE(result.called);
    EXPECT_EQ(result.status, WGPUPopErrorScopeStatus_Success);

    wgpuDeviceRelease(device);
}

TEST_F(WebGPUDeviceLostCallback, IsCalledWithoutTheDeviceWhenTheDeviceCannotBeCreated)
{
    DeviceLosses losses;
    WGPUDeviceDescriptor descriptor = deviceDescriptor(WGPUCallbackMode_AllowSpontaneous, losses);
    WGPULimits limits = WGPU_LIMITS_INIT;
    limits.maxBindGroups = UINT32_MAX - 1;
    descriptor.requiredLimits = &limits;
    EXPECT_FALSE(requestDevice(m_instance, m_adapter, descriptor, WGPURequestDeviceStatus_Error));
    ASSERT_EQ(losses.size(), 1u);
    EXPECT_EQ(losses[0].device, nullptr);
    EXPECT_EQ(losses[0].reason, WGPUDeviceLostReason_FailedCreation);
    EXPECT_FALSE(losses[0].message.empty());
}

} // namespace TestWebKitAPI
