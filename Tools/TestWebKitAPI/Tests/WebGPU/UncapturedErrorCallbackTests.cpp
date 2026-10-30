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
#include <vector>

// The uncaptured error callback of the WebGPU C API, WGPUDeviceDescriptor.uncapturedErrorCallbackInfo.
namespace TestWebKitAPI {

struct UncapturedError {
    WGPUDevice device;
    WGPUErrorType type;
    std::string message;
    void* userdata2;
    std::thread::id thread;
};

using UncapturedErrors = std::vector<UncapturedError>;

static int userdata2Marker;

// A future that does not complete fails the test instead of hanging it.
static constexpr uint64_t waitTimeoutNanoseconds = 10'000'000'000;

static void waitFor(WGPUInstance instance, WGPUFuture future)
{
    WGPUFutureWaitInfo waitInfo = WGPU_FUTURE_WAIT_INFO_INIT;
    waitInfo.future = future;
    EXPECT_EQ(wgpuInstanceWaitAny(instance, 1, &waitInfo, waitTimeoutNanoseconds), WGPUWaitStatus_Success);
    EXPECT_TRUE(waitInfo.completed);
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

static WGPUDevice requestDevice(WGPUInstance instance, WGPUAdapter adapter, UncapturedErrors* errors)
{
    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    if (errors) {
        descriptor.uncapturedErrorCallbackInfo.callback = [](const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
            static_cast<UncapturedErrors*>(userdata1)->push_back({ *device, type, std::string(message.data, message.length), userdata2, std::this_thread::get_id() });
        };
        descriptor.uncapturedErrorCallbackInfo.userdata1 = errors;
        descriptor.uncapturedErrorCallbackInfo.userdata2 = &userdata2Marker;
    }

    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo callbackInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
        EXPECT_EQ(status, WGPURequestDeviceStatus_Success);
        *static_cast<WGPUDevice*>(userdata1) = device;
    };
    callbackInfo.userdata1 = &device;
    waitFor(instance, wgpuAdapterRequestDevice(adapter, &descriptor, callbackInfo));
    return device;
}

// A buffer that is both readable and writable by mapping is a validation error.
static void createInvalidBuffer(WGPUDevice device)
{
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite;
    descriptor.size = 4;
    wgpuBufferRelease(wgpuDeviceCreateBuffer(device, &descriptor));
}

static WGPUErrorType popErrorScope(WGPUInstance instance, WGPUDevice device)
{
    WGPUErrorType type = WGPUErrorType_NoError;
    WGPUPopErrorScopeCallbackInfo callbackInfo = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView, void* userdata1, void*) {
        EXPECT_EQ(status, WGPUPopErrorScopeStatus_Success);
        *static_cast<WGPUErrorType*>(userdata1) = type;
    };
    callbackInfo.userdata1 = &type;
    waitFor(instance, wgpuDevicePopErrorScope(device, callbackInfo));
    return type;
}

class WebGPUUncapturedErrorCallback : public testing::Test {
public:
    void SetUp() override
    {
        // The waits with a timeout are timed waits.
        static constexpr auto timedWaitAny = WGPUInstanceFeatureName_TimedWaitAny;
        WGPUInstanceDescriptor instanceDescriptor = WGPU_INSTANCE_DESCRIPTOR_INIT;
        instanceDescriptor.requiredFeatureCount = 1;
        instanceDescriptor.requiredFeatures = &timedWaitAny;
        m_instance = wgpuCreateInstance(&instanceDescriptor);
        ASSERT_TRUE(m_instance);
        m_adapter = requestAdapter(m_instance);
        ASSERT_TRUE(m_adapter);
    }

    void TearDown() override
    {
        wgpuAdapterRelease(m_adapter);
        wgpuInstanceRelease(m_instance);
    }

protected:
    WGPUInstance m_instance { nullptr };
    WGPUAdapter m_adapter { nullptr };
};

TEST_F(WebGPUUncapturedErrorCallback, IsCalledInTheCallThatGeneratesTheError)
{
    UncapturedErrors errors;
    WGPUDevice device = requestDevice(m_instance, m_adapter, &errors);
    ASSERT_TRUE(device);

    createInvalidBuffer(device);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].device, device);
    EXPECT_EQ(errors[0].type, WGPUErrorType_Validation);
    EXPECT_FALSE(errors[0].message.empty());
    EXPECT_EQ(errors[0].userdata2, &userdata2Marker);

    EXPECT_EQ(errors[0].thread, std::this_thread::get_id());

    // The callback is called for every error.
    createInvalidBuffer(device);
    EXPECT_EQ(errors.size(), 2u);

    wgpuDeviceRelease(device);
}

TEST_F(WebGPUUncapturedErrorCallback, IsCalledOnTheThreadThatGeneratesTheError)
{
    UncapturedErrors errors;
    WGPUDevice device = requestDevice(m_instance, m_adapter, &errors);
    ASSERT_TRUE(device);

    std::thread::id otherThread;
    std::thread thread([&] {
        otherThread = std::this_thread::get_id();
        createInvalidBuffer(device);
        createInvalidBuffer(device);
    });
    thread.join();
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0].thread, otherThread);
    EXPECT_EQ(errors[1].thread, otherThread);

    createInvalidBuffer(device);
    ASSERT_EQ(errors.size(), 3u);
    EXPECT_EQ(errors[2].thread, std::this_thread::get_id());

    wgpuDeviceRelease(device);
}

TEST_F(WebGPUUncapturedErrorCallback, IsNotCalledForTheErrorsThatAnErrorScopeCatches)
{
    UncapturedErrors errors;
    WGPUDevice device = requestDevice(m_instance, m_adapter, &errors);
    ASSERT_TRUE(device);

    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    createInvalidBuffer(device);
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(popErrorScope(m_instance, device), WGPUErrorType_Validation);

    // A scope with another filter does not catch the error.
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_OutOfMemory);
    createInvalidBuffer(device);
    EXPECT_EQ(errors.size(), 1u);
    EXPECT_EQ(popErrorScope(m_instance, device), WGPUErrorType_NoError);

    wgpuDeviceRelease(device);
}

TEST_F(WebGPUUncapturedErrorCallback, IsNotCalledAfterTheDeviceIsLost)
{
    UncapturedErrors errors;
    WGPUDevice device = requestDevice(m_instance, m_adapter, &errors);
    ASSERT_TRUE(device);

    // An invalid error filter is a validation error, also on a lost device.
    auto invalidErrorFilter = static_cast<WGPUErrorFilter>(0x7FFF);
    wgpuDevicePushErrorScope(device, invalidErrorFilter);
    EXPECT_EQ(errors.size(), 1u);

    wgpuDeviceDestroy(device);
    wgpuDevicePushErrorScope(device, invalidErrorFilter);
    createInvalidBuffer(device);
    EXPECT_EQ(errors.size(), 1u);

    wgpuDeviceRelease(device);
}

TEST_F(WebGPUUncapturedErrorCallback, IsOptional)
{
    WGPUDevice device = requestDevice(m_instance, m_adapter, nullptr);
    ASSERT_TRUE(device);
    createInvalidBuffer(device);
    wgpuDeviceRelease(device);
}

} // namespace TestWebKitAPI
