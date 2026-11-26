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
#include <wtf/CompletionHandler.h>
#include <wtf/threads/BinarySemaphore.h>

// WebGPU::retainCAPIHandle(): objects of the WebGPU C++ API given to the WebGPU C API.
namespace TestWebKitAPI {

// A future that does not complete fails the test instead of hanging it.
static constexpr uint64_t waitTimeoutNanoseconds = 10'000'000'000;

static WGPUFutureWaitInfo waitInfo(WGPUFuture future)
{
    WGPUFutureWaitInfo info = WGPU_FUTURE_WAIT_INFO_INIT;
    info.future = future;
    return info;
}

class WebGPUCAPIHandle : public testing::Test {
public:
    void SetUp() override
    {
        // Without a scheduleWork, the instance runs its work on the thread that schedules it, so
        // its completion handlers run without the help of the waiting thread.
        m_instance = WebGPU::createInstance({ });
        ASSERT_TRUE(m_instance);

        BinarySemaphore done;
        protect(m_instance)->requestAdapter({ }, [&](RefPtr<WebGPU::Adapter>&& adapter) {
            m_adapter = WTF::move(adapter);
            done.signal();
        });
        ASSERT_TRUE(done.waitFor(10_s));
        ASSERT_TRUE(m_adapter);

        protect(m_adapter)->requestDevice({ }, [&](std::expected<Ref<WebGPU::Device>, String>&& device) {
            if (device)
                m_device = WTF::move(*device);
            done.signal();
        });
        ASSERT_TRUE(done.waitFor(10_s));
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
    RefPtr<WebGPU::Texture> createTexture()
    {
        return protect(m_device)->createTexture({
            .label = "texture"_s,
            .usage = { WebGPU::TextureUsage::RenderAttachment, WebGPU::TextureUsage::CopySource },
            .dimension = WebGPU::TextureDimension::_2d,
            .size = { .width = 16, .height = 8, .depthOrArrayLayers = 1 },
            .format = WebGPU::TextureFormat::Rgba8unorm,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormats = { },
        });
    }

    RefPtr<WebGPU::Instance> m_instance;
    RefPtr<WebGPU::Adapter> m_adapter;
    RefPtr<WebGPU::Device> m_device;
};

TEST_F(WebGPUCAPIHandle, HandleHoldsAReference)
{
    Ref instance = *m_instance;
    auto instanceRefCount = instance->refCount();
    WGPUInstance instanceHandle = WebGPU::retainCAPIHandle(instance);
    ASSERT_TRUE(instanceHandle);
    EXPECT_EQ(instance->refCount(), instanceRefCount + 1);
    wgpuInstanceAddRef(instanceHandle);
    EXPECT_EQ(instance->refCount(), instanceRefCount + 2);
    wgpuInstanceRelease(instanceHandle);
    wgpuInstanceRelease(instanceHandle);
    EXPECT_EQ(instance->refCount(), instanceRefCount);

    Ref device = *m_device;
    auto deviceRefCount = device->refCount();
    WGPUDevice deviceHandle = WebGPU::retainCAPIHandle(device);
    ASSERT_TRUE(deviceHandle);
    EXPECT_EQ(device->refCount(), deviceRefCount + 1);
    wgpuDeviceAddRef(deviceHandle);
    EXPECT_EQ(device->refCount(), deviceRefCount + 2);
    wgpuDeviceRelease(deviceHandle);
    wgpuDeviceRelease(deviceHandle);
    EXPECT_EQ(device->refCount(), deviceRefCount);
    // Releasing the references of the C API does not lose the device.
    EXPECT_FALSE(device->isLost());

    RefPtr texture = createTexture();
    ASSERT_TRUE(texture);
    auto textureRefCount = texture->refCount();
    WGPUTexture textureHandle = WebGPU::retainCAPIHandle(*texture);
    ASSERT_TRUE(textureHandle);
    EXPECT_EQ(texture->refCount(), textureRefCount + 1);
    wgpuTextureAddRef(textureHandle);
    EXPECT_EQ(texture->refCount(), textureRefCount + 2);
    wgpuTextureRelease(textureHandle);
    wgpuTextureRelease(textureHandle);
    EXPECT_EQ(texture->refCount(), textureRefCount);
}

TEST_F(WebGPUCAPIHandle, HandleIsTheSameObject)
{
    WGPUInstance instance = WebGPU::retainCAPIHandle(*protect(m_instance));
    WGPUDevice device = WebGPU::retainCAPIHandle(*protect(m_device));

    // The device of the C++ API reaches the instance of the C++ API through the C API.
    WGPUAdapter adapter = wgpuDeviceGetAdapter(device);
    ASSERT_TRUE(adapter);
    WGPUInstance adapterInstance = wgpuAdapterGetInstance(adapter);
    EXPECT_EQ(adapterInstance, instance);
    wgpuInstanceRelease(adapterInstance);
    wgpuAdapterRelease(adapter);

    WGPULimits limits = WGPU_LIMITS_INIT;
    EXPECT_EQ(wgpuDeviceGetLimits(device, &limits), WGPUStatus_Success);
    EXPECT_EQ(limits.maxTextureDimension2D, protect(m_device)->limits().maxTextureDimension2D);
    EXPECT_EQ(limits.maxBindGroups, protect(m_device)->limits().maxBindGroups);

    RefPtr cppTexture = createTexture();
    ASSERT_TRUE(cppTexture);
    WGPUTexture texture = WebGPU::retainCAPIHandle(*cppTexture);
    EXPECT_EQ(wgpuTextureGetWidth(texture), 16u);
    EXPECT_EQ(wgpuTextureGetHeight(texture), 8u);
    EXPECT_EQ(wgpuTextureGetFormat(texture), WGPUTextureFormat_RGBA8Unorm);
    EXPECT_EQ(wgpuTextureGetUsage(texture), WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc);

    // A view of the texture, which the C API creates.
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    EXPECT_TRUE(view);
    wgpuTextureViewRelease(view);

    wgpuTextureRelease(texture);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);
}

TEST_F(WebGPUCAPIHandle, FuturesCompleteOnTheInstance)
{
    WGPUInstance instance = WebGPU::retainCAPIHandle(*protect(m_instance));
    WGPUDevice device = WebGPU::retainCAPIHandle(*protect(m_device));
    WGPUQueue queue = wgpuDeviceGetQueue(device);
    ASSERT_TRUE(queue);

    WGPUQueueWorkDoneStatus workDoneStatus = WGPUQueueWorkDoneStatus_Error;
    WGPUQueueWorkDoneCallbackInfo workDoneCallbackInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    workDoneCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    workDoneCallbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView, void* userdata1, void*) {
        *static_cast<WGPUQueueWorkDoneStatus*>(userdata1) = status;
    };
    workDoneCallbackInfo.userdata1 = &workDoneStatus;
    auto workDoneInfo = waitInfo(wgpuQueueOnSubmittedWorkDone(queue, workDoneCallbackInfo));
    EXPECT_NE(workDoneInfo.future.id, 0u);
    EXPECT_EQ(wgpuInstanceWaitAny(instance, 1, &workDoneInfo, waitTimeoutNanoseconds), WGPUWaitStatus_Success);
    EXPECT_TRUE(workDoneInfo.completed);
    EXPECT_EQ(workDoneStatus, WGPUQueueWorkDoneStatus_Success);

    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bufferDescriptor.size = 16;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    ASSERT_TRUE(buffer);
    WGPUMapAsyncStatus mapStatus = WGPUMapAsyncStatus_Error;
    WGPUBufferMapCallbackInfo mapCallbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    mapCallbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    mapCallbackInfo.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* userdata1, void*) {
        *static_cast<WGPUMapAsyncStatus*>(userdata1) = status;
    };
    mapCallbackInfo.userdata1 = &mapStatus;
    auto mapInfo = waitInfo(wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE, mapCallbackInfo));
    EXPECT_EQ(wgpuInstanceWaitAny(instance, 1, &mapInfo, waitTimeoutNanoseconds), WGPUWaitStatus_Success);
    EXPECT_TRUE(mapInfo.completed);
    EXPECT_EQ(mapStatus, WGPUMapAsyncStatus_Success);
    EXPECT_TRUE(wgpuBufferGetConstMappedRange(buffer, 0, 16));
    wgpuBufferUnmap(buffer);
    wgpuBufferRelease(buffer);

    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);
}

} // namespace TestWebKitAPI

#endif // !PLATFORM(IOS_FAMILY_SIMULATOR)
