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
#include <utility>

// The references that the WebGPU C API hands out to adapters and instances, and the futures of the
// objects that a device creates.
namespace TestWebKitAPI {

// A future that does not complete fails the test instead of hanging it.
static constexpr uint64_t waitTimeoutNanoseconds = 10'000'000'000;

static void waitFor(WGPUInstance instance, WGPUFuture future)
{
    WGPUFutureWaitInfo waitInfo = WGPU_FUTURE_WAIT_INFO_INIT;
    waitInfo.future = future;
    EXPECT_EQ(wgpuInstanceWaitAny(instance, 1, &waitInfo, waitTimeoutNanoseconds), WGPUWaitStatus_Success);
    EXPECT_TRUE(waitInfo.completed);
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

static WGPUDevice requestDevice(WGPUInstance instance, WGPUAdapter adapter)
{
    WGPUDevice device = nullptr;
    WGPURequestDeviceCallbackInfo callbackInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
        EXPECT_EQ(status, WGPURequestDeviceStatus_Success);
        *static_cast<WGPUDevice*>(userdata1) = device;
    };
    callbackInfo.userdata1 = &device;
    waitFor(instance, wgpuAdapterRequestDevice(adapter, nullptr, callbackInfo));
    return device;
}

static WGPUBuffer createMappableBuffer(WGPUDevice device)
{
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    descriptor.size = 4;
    return wgpuDeviceCreateBuffer(device, &descriptor);
}

struct WorkDone {
    unsigned callCount { 0 };
    WGPUQueueWorkDoneStatus status { WGPUQueueWorkDoneStatus_Error };
};

static WGPUQueueWorkDoneCallbackInfo workDoneCallbackInfo(WGPUCallbackMode mode, WorkDone& workDone)
{
    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = mode;
    callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView, void* userdata1, void*) {
        auto& workDone = *static_cast<WorkDone*>(userdata1);
        ++workDone.callCount;
        workDone.status = status;
    };
    callbackInfo.userdata1 = &workDone;
    return callbackInfo;
}

class WebGPUReferences : public testing::Test {
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

TEST_F(WebGPUReferences, DeviceGetAdapterReturnsAReferenceToRelease)
{
    WGPUDevice device = requestDevice(m_instance, m_adapter);
    ASSERT_TRUE(device);

    WGPUAdapter adapter = wgpuDeviceGetAdapter(device);
    EXPECT_EQ(adapter, m_adapter);

    // The adapter keeps its instance after its other reference is released.
    wgpuAdapterRelease(std::exchange(m_adapter, nullptr));
    WGPUInstance instance = wgpuAdapterGetInstance(adapter);
    EXPECT_EQ(instance, m_instance);
    wgpuInstanceRelease(instance);

    wgpuAdapterRelease(adapter);
    wgpuDeviceRelease(device);
}

TEST_F(WebGPUReferences, AdapterGetInstanceReturnsAReferenceToRelease)
{
    WGPUInstance instance = wgpuAdapterGetInstance(m_adapter);
    EXPECT_EQ(instance, m_instance);

    // The instance is alive after its other reference is released.
    wgpuInstanceRelease(std::exchange(m_instance, instance));
    WGPUInstance otherReference = wgpuAdapterGetInstance(m_adapter);
    EXPECT_EQ(otherReference, instance);
    wgpuInstanceRelease(otherReference);
}

TEST_F(WebGPUReferences, AdapterGetInstanceIsNullAfterTheInstanceIsReleased)
{
    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    EXPECT_EQ(wgpuAdapterGetInstance(m_adapter), nullptr);
}

class WebGPUFutures : public WebGPUReferences {
public:
    void SetUp() override
    {
        WebGPUReferences::SetUp();
        m_device = requestDevice(m_instance, m_adapter);
        ASSERT_TRUE(m_device);
    }

    void TearDown() override
    {
        if (m_device)
            wgpuDeviceRelease(m_device);
        WebGPUReferences::TearDown();
    }

protected:
    WGPUDevice m_device { nullptr };
};

TEST_F(WebGPUFutures, BufferMapAsyncCompletesInWaitAny)
{
    WGPUBuffer buffer = createMappableBuffer(m_device);
    ASSERT_TRUE(buffer);

    WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
    WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        *static_cast<WGPUMapAsyncStatus*>(userdata1) = status;
    };
    callbackInfo.userdata1 = &status;
    waitFor(m_instance, wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, 4, callbackInfo));
    EXPECT_EQ(status, WGPUMapAsyncStatus_Success);

    wgpuBufferUnmap(buffer);
    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUFutures, QueueOnSubmittedWorkDoneCompletesInWaitAny)
{
    WGPUQueue queue = wgpuDeviceGetQueue(m_device);
    ASSERT_TRUE(queue);

    WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Error;
    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView, void* userdata1, void*) {
        *static_cast<WGPUQueueWorkDoneStatus*>(userdata1) = status;
    };
    callbackInfo.userdata1 = &status;
    waitFor(m_instance, wgpuQueueOnSubmittedWorkDone(queue, callbackInfo));
    EXPECT_EQ(status, WGPUQueueWorkDoneStatus_Success);

    wgpuQueueRelease(queue);
}

TEST_F(WebGPUFutures, ShaderModuleGetCompilationInfoCompletesInWaitAny)
{
    static constexpr char source[] = "@compute @workgroup_size(1) fn main() { }";
    WGPUShaderSourceWGSL wgslSource = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgslSource.code = { source, sizeof(source) - 1 };
    WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &wgslSource.chain;
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_device, &descriptor);
    ASSERT_TRUE(shaderModule);

    WGPUCompilationInfoRequestStatus status = WGPUCompilationInfoRequestStatus_CallbackCancelled;
    WGPUCompilationInfoCallbackInfo callbackInfo = WGPU_COMPILATION_INFO_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo*, void* userdata1, void*) {
        *static_cast<WGPUCompilationInfoRequestStatus*>(userdata1) = status;
    };
    callbackInfo.userdata1 = &status;
    waitFor(m_instance, wgpuShaderModuleGetCompilationInfo(shaderModule, callbackInfo));
    EXPECT_EQ(status, WGPUCompilationInfoRequestStatus_Success);

    wgpuShaderModuleRelease(shaderModule);
}

TEST_F(WebGPUFutures, BufferMapAsyncCallbackIsCalledAfterTheLastDeviceReferenceIsReleased)
{
    WGPUBuffer buffer = createMappableBuffer(m_device);
    ASSERT_TRUE(buffer);
    wgpuDeviceRelease(std::exchange(m_device, nullptr));

    unsigned callCount = 0;
    WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = [](WGPUMapAsyncStatus, WGPUStringView, void* userdata1, void*) {
        ++*static_cast<unsigned*>(userdata1);
    };
    callbackInfo.userdata1 = &callCount;
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, 4, callbackInfo);
    wgpuInstanceProcessEvents(m_instance);
    EXPECT_EQ(callCount, 1u);

    wgpuBufferRelease(buffer);
}

// As in Dawn, which cancels the callbacks that it can no longer call.
TEST_F(WebGPUFutures, QueueOnSubmittedWorkDoneIsCancelledWhenTheInstanceIsReleased)
{
    WGPUQueue queue = wgpuDeviceGetQueue(m_device);
    ASSERT_TRUE(queue);

    // The work of an idle queue is done, and the callback waits for wgpuInstanceProcessEvents().
    WorkDone workDone;
    wgpuQueueOnSubmittedWorkDone(queue, workDoneCallbackInfo(WGPUCallbackMode_AllowProcessEvents, workDone));
    EXPECT_EQ(workDone.callCount, 0u);

    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    EXPECT_EQ(workDone.callCount, 1u);
    EXPECT_EQ(workDone.status, WGPUQueueWorkDoneStatus_CallbackCancelled);

    wgpuQueueRelease(queue);
    EXPECT_EQ(workDone.callCount, 1u);
}

TEST_F(WebGPUFutures, QueueOnSubmittedWorkDoneIsNotCancelledWhenTheInstanceIsReleasedWithAllowSpontaneous)
{
    WGPUQueue queue = wgpuDeviceGetQueue(m_device);
    ASSERT_TRUE(queue);

    WorkDone workDone;
    wgpuQueueOnSubmittedWorkDone(queue, workDoneCallbackInfo(WGPUCallbackMode_AllowSpontaneous, workDone));

    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    EXPECT_EQ(workDone.callCount, 1u);
    EXPECT_EQ(workDone.status, WGPUQueueWorkDoneStatus_Success);

    wgpuQueueRelease(queue);
    EXPECT_EQ(workDone.callCount, 1u);
}

TEST_F(WebGPUFutures, BufferMapAsyncIsCancelledWhenTheInstanceIsReleased)
{
    WGPUBuffer buffer = createMappableBuffer(m_device);
    ASSERT_TRUE(buffer);

    // The buffer is mapped once the work of the queue is done, and the callback waits for
    // wgpuInstanceProcessEvents().
    struct Result {
        unsigned callCount { 0 };
        WGPUMapAsyncStatus status { WGPUMapAsyncStatus_Error };
    } result;
    WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        auto& result = *static_cast<Result*>(userdata1);
        ++result.callCount;
        result.status = status;
    };
    callbackInfo.userdata1 = &result;
    wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, 4, callbackInfo);
    EXPECT_EQ(result.callCount, 0u);

    wgpuInstanceRelease(std::exchange(m_instance, nullptr));
    EXPECT_EQ(result.callCount, 1u);
    EXPECT_EQ(result.status, WGPUMapAsyncStatus_CallbackCancelled);

    wgpuBufferRelease(buffer);
    EXPECT_EQ(result.callCount, 1u);
}

} // namespace TestWebKitAPI
