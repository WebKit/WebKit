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

#include <WebGPU/WebGPU.h>
#include <atomic>
#include <cstdint>

// The futures of the WebGPU C API: wgpuInstanceWaitAny() and wgpuInstanceProcessEvents().
namespace TestWebKitAPI {

// A future that does not complete fails the test instead of hanging it.
static constexpr uint64_t waitTimeoutNanoseconds = 10'000'000'000;

static WGPUWaitStatus waitAny(WGPUInstance instance, WGPUFutureWaitInfo* waitInfos, size_t count, uint64_t timeoutNanoseconds = waitTimeoutNanoseconds)
{
    return wgpuInstanceWaitAny(instance, count, waitInfos, timeoutNanoseconds);
}

static WGPUFutureWaitInfo waitInfo(WGPUFuture future)
{
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT;
    info.future = future;
    return info;
}

static WGPUFuture requestAdapter(WGPUInstance instance, WGPUAdapter* adapter)
{
    WGPURequestAdapterCallbackInfo callbackInfo = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView, void* userdata1, void*) {
        EXPECT_EQ(status, WGPURequestAdapterStatus_Success);
        *static_cast<WGPUAdapter*>(userdata1) = adapter;
    };
    callbackInfo.userdata1 = adapter;
    return wgpuInstanceRequestAdapter(instance, nullptr, callbackInfo);
}

static WGPUFuture requestDevice(WGPUAdapter adapter, WGPUDevice* device)
{
    WGPURequestDeviceCallbackInfo callbackInfo = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* userdata1, void*) {
        EXPECT_EQ(status, WGPURequestDeviceStatus_Success);
        *static_cast<WGPUDevice*>(userdata1) = device;
    };
    callbackInfo.userdata1 = device;
    return wgpuAdapterRequestDevice(adapter, nullptr, callbackInfo);
}

// What a work-done callback saw.
struct WorkDone {
    std::atomic<int> callCount { 0 };
    WGPUQueueWorkDoneStatus status { WGPUQueueWorkDoneStatus_Error };
    // The future of the callback, and the status of a wait for it in the callback.
    WGPUInstance instance { nullptr };
    WGPUFuture future { };
    WGPUWaitStatus waitStatusInCallback { WGPUWaitStatus_Error };
};

static WGPUFuture onSubmittedWorkDone(WGPUQueue queue, WorkDone& workDone, WGPUCallbackMode mode = WGPUCallbackMode_AllowProcessEvents)
{
    WGPUQueueWorkDoneCallbackInfo callbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    callbackInfo.mode = mode;
    callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView, void* userdata1, void*) {
        auto& workDone = *static_cast<WorkDone*>(userdata1);
        workDone.status = status;
        ++workDone.callCount;
        if (workDone.instance) {
            auto info = waitInfo(workDone.future);
            workDone.waitStatusInCallback = waitAny(workDone.instance, &info, 1, 0);
        }
    };
    callbackInfo.userdata1 = &workDone;
    return wgpuQueueOnSubmittedWorkDone(queue, callbackInfo);
}

class WebGPUFuture : public testing::Test {
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

        auto adapterInfo = waitInfo(requestAdapter(m_instance, &m_adapter));
        ASSERT_EQ(waitAny(m_instance, &adapterInfo, 1), WGPUWaitStatus_Success);
        ASSERT_TRUE(m_adapter);

        auto deviceInfo = waitInfo(requestDevice(m_adapter, &m_device));
        ASSERT_EQ(waitAny(m_instance, &deviceInfo, 1), WGPUWaitStatus_Success);
        ASSERT_TRUE(m_device);
        m_queue = wgpuDeviceGetQueue(m_device);
    }

    void TearDown() override
    {
        if (m_queue)
            wgpuQueueRelease(m_queue);
        if (m_device)
            wgpuDeviceRelease(m_device);
        if (m_adapter)
            wgpuAdapterRelease(m_adapter);
        wgpuInstanceRelease(m_instance);
    }

protected:
    // Submits a command buffer with no commands, so that the queue has work to complete.
    void submitEmptyCommandBuffer()
    {
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);
        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(m_queue, 1, &commandBuffer);
        wgpuCommandBufferRelease(commandBuffer);
        wgpuCommandEncoderRelease(encoder);
    }

    WGPUInstance m_instance { nullptr };
    WGPUAdapter m_adapter { nullptr };
    WGPUDevice m_device { nullptr };
    WGPUQueue m_queue { nullptr };
};

TEST_F(WebGPUFuture, ProcessEventsRunsTheCallbacks)
{
    WorkDone workDone;
    auto future = onSubmittedWorkDone(m_queue, workDone);
    EXPECT_NE(future.id, 0u);
    // The callback of an idle queue waits for the instance to process its events.
    EXPECT_EQ(workDone.callCount, 0);

    wgpuInstanceProcessEvents(m_instance);
    EXPECT_EQ(workDone.callCount, 1);
    EXPECT_EQ(workDone.status, WGPUQueueWorkDoneStatus_Success);

    // The callback is called once.
    wgpuInstanceProcessEvents(m_instance);
    EXPECT_EQ(workDone.callCount, 1);
}

TEST_F(WebGPUFuture, WaitAnyRunsTheCallbackAndCompletesTheFuture)
{
    WorkDone workDone;
    auto info = waitInfo(onSubmittedWorkDone(m_queue, workDone, WGPUCallbackMode_WaitAnyOnly));
    EXPECT_EQ(waitAny(m_instance, &info, 1, UINT64_MAX), WGPUWaitStatus_Success);
    EXPECT_TRUE(info.completed);
    EXPECT_EQ(workDone.callCount, 1);
    EXPECT_EQ(workDone.status, WGPUQueueWorkDoneStatus_Success);
}

TEST_F(WebGPUFuture, WaitAnyWithAZeroTimeoutRunsTheCallbacksThatAreReady)
{
    WorkDone workDone;
    auto info = waitInfo(onSubmittedWorkDone(m_queue, workDone, WGPUCallbackMode_WaitAnyOnly));
    EXPECT_EQ(waitAny(m_instance, &info, 1, 0), WGPUWaitStatus_Success);
    EXPECT_TRUE(info.completed);
    EXPECT_EQ(workDone.callCount, 1);
}

TEST_F(WebGPUFuture, WaitAnyReportsEveryCompletedFuture)
{
    WorkDone first;
    WorkDone second;
    WGPUFutureWaitInfo infos[] = {
        waitInfo(onSubmittedWorkDone(m_queue, first, WGPUCallbackMode_WaitAnyOnly)),
        waitInfo(onSubmittedWorkDone(m_queue, second, WGPUCallbackMode_WaitAnyOnly)),
    };
    EXPECT_NE(infos[0].future.id, infos[1].future.id);
    EXPECT_EQ(waitAny(m_instance, infos, 2), WGPUWaitStatus_Success);
    EXPECT_TRUE(infos[0].completed);
    EXPECT_TRUE(infos[1].completed);
    EXPECT_EQ(first.callCount, 1);
    EXPECT_EQ(second.callCount, 1);
}

TEST_F(WebGPUFuture, WaitAnyWakesUpForWorkThatCompletesOnTheGPU)
{
    submitEmptyCommandBuffer();
    WorkDone workDone;
    auto info = waitInfo(onSubmittedWorkDone(m_queue, workDone, WGPUCallbackMode_WaitAnyOnly));
    EXPECT_EQ(waitAny(m_instance, &info, 1, UINT64_MAX), WGPUWaitStatus_Success);
    EXPECT_TRUE(info.completed);
    EXPECT_EQ(workDone.callCount, 1);
    EXPECT_EQ(workDone.status, WGPUQueueWorkDoneStatus_Success);
}

TEST_F(WebGPUFuture, WaitAnyTimesOutForAFutureThatDoesNotComplete)
{
    // WGPU_FUTURE_INIT is no future of the instance.
    auto info = waitInfo(WGPU_FUTURE_INIT);
    EXPECT_EQ(waitAny(m_instance, &info, 1, 0), WGPUWaitStatus_TimedOut);
    EXPECT_EQ(waitAny(m_instance, &info, 1, 1'000'000), WGPUWaitStatus_TimedOut);
    EXPECT_FALSE(info.completed);
}

TEST_F(WebGPUFuture, TheFutureCompletesAfterTheCallback)
{
    WorkDone workDone;
    workDone.future = onSubmittedWorkDone(m_queue, workDone);
    workDone.instance = m_instance;
    wgpuInstanceProcessEvents(m_instance);
    EXPECT_EQ(workDone.callCount, 1);
    EXPECT_EQ(workDone.waitStatusInCallback, WGPUWaitStatus_TimedOut);

    auto info = waitInfo(workDone.future);
    EXPECT_EQ(waitAny(m_instance, &info, 1), WGPUWaitStatus_Success);
    EXPECT_TRUE(info.completed);
    EXPECT_EQ(workDone.callCount, 1);
}

TEST_F(WebGPUFuture, MapAsyncCompletesInWaitAny)
{
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    descriptor.size = 16;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(m_device, &descriptor);
    ASSERT_TRUE(buffer);

    WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error;
    WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        *static_cast<WGPUMapAsyncStatus*>(userdata1) = status;
    };
    callbackInfo.userdata1 = &status;
    auto info = waitInfo(wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE, callbackInfo));
    EXPECT_EQ(waitAny(m_instance, &info, 1), WGPUWaitStatus_Success);
    EXPECT_TRUE(info.completed);
    EXPECT_EQ(status, WGPUMapAsyncStatus_Success);
    EXPECT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Mapped);
    EXPECT_TRUE(wgpuBufferGetConstMappedRange(buffer, 0, 16));

    wgpuBufferUnmap(buffer);
    wgpuBufferRelease(buffer);
}

} // namespace TestWebKitAPI

#endif // !PLATFORM(IOS_FAMILY_SIMULATOR)
