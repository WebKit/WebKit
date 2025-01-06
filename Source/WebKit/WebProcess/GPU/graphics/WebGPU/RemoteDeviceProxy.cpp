/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "RemoteDeviceProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBindGroupLayoutProxy.h"
#include "RemoteBindGroupProxy.h"
#include "RemoteBufferProxy.h"
#include "RemoteCommandEncoderProxy.h"
#include "RemoteComputePipelineProxy.h"
#include "RemoteDeviceMessages.h"
#include "RemoteExternalTextureProxy.h"
#include "RemotePipelineLayoutProxy.h"
#include "RemoteQuerySetProxy.h"
#include "RemoteQueueProxy.h"
#include "RemoteRenderBundleEncoderProxy.h"
#include "RemoteRenderPipelineProxy.h"
#include "RemoteSamplerProxy.h"
#include "RemoteShaderModuleProxy.h"
#include "RemoteTextureProxy.h"
#include "RemoteXRBindingProxy.h"
#include "SharedVideoFrame.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDeviceProxy);

Ref<WebCore::WebGPU::CommandEncoder> RemoteDeviceProxy::createInvalidCommandEncoder()
{
    pauseAllErrorReporting(true);
    return createCommandEncoder(std::nullopt).releaseNonNull();
}

static auto makeInvalidRenderPassEncoder(auto& commandEncoder)
{
    WebCore::WebGPU::RenderPassDescriptor descriptor;
    return commandEncoder->beginRenderPass(descriptor).releaseNonNull();
}

static auto makeInvalidCommandBuffer(auto& commandEncoder)
{
    WebCore::WebGPU::CommandBufferDescriptor descriptor;
    return commandEncoder->finish(descriptor).releaseNonNull();
}

RemoteDeviceProxy::RemoteDeviceProxy(Ref<WebCore::WebGPU::SupportedFeatures>&& features, Ref<WebCore::WebGPU::SupportedLimits>&& limits, RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    : Device(WTFMove(features), WTFMove(limits))
    , m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
    , m_queue(RemoteQueueProxy::create(parent, convertToBackingContext, queueIdentifier))
    , m_invalidCommandEncoder(createInvalidCommandEncoder())
    , m_invalidRenderPassEncoder(makeInvalidRenderPassEncoder(m_invalidCommandEncoder))
    , m_invalidComputePassEncoder(Ref { m_invalidCommandEncoder }->beginComputePass(std::nullopt).releaseNonNull())
    , m_invalidCommandBuffer(makeInvalidCommandBuffer(m_invalidCommandEncoder))
{
    Ref { m_invalidRenderPassEncoder }->end();
    Ref { m_invalidComputePassEncoder }->end();
    Ref { m_queue }->submit({ m_invalidCommandBuffer });

    pauseAllErrorReporting(false);
}

RemoteDeviceProxy::~RemoteDeviceProxy()
{
    auto sendResult = send(Messages::RemoteDevice::Destruct());
    UNUSED_PARAM(sendResult);
}

Ref<WebCore::WebGPU::Queue> RemoteDeviceProxy::queue()
{
    return m_queue;
}

void RemoteDeviceProxy::destroy()
{
    auto sendResult = send(Messages::RemoteDevice::Destroy());
    UNUSED_PARAM(sendResult);
}

RefPtr<WebCore::WebGPU::XRBinding> RemoteDeviceProxy::createXRBinding()
{
    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateXRBinding(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteXRBindingProxy::create(*this, protectedConvertToBackingContext(), identifier);
}

RefPtr<WebCore::WebGPU::Buffer> RemoteDeviceProxy::createBuffer(const WebCore::WebGPU::BufferDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBuffer(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteBufferProxy::create(*this, protectedConvertToBackingContext(), identifier, convertedDescriptor->mappedAtCreation);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::Texture> RemoteDeviceProxy::createTexture(const WebCore::WebGPU::TextureDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateTexture(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteTextureProxy::create(protectedRoot(), protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::Sampler> RemoteDeviceProxy::createSampler(const WebCore::WebGPU::SamplerDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateSampler(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteSamplerProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::ExternalTexture> RemoteDeviceProxy::importExternalTexture(const WebCore::WebGPU::ExternalTextureDescriptor& descriptor)
{
    auto identifier = WebGPUIdentifier::generate();

    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

#if PLATFORM(COCOA) && ENABLE(VIDEO)
    if (!convertedDescriptor->mediaIdentifier) {
        auto* videoFrame = std::get_if<RefPtr<WebCore::VideoFrame>>(&descriptor.videoBacking);
        if (videoFrame && videoFrame->get()) {
            convertedDescriptor->sharedFrame = m_sharedVideoFrameWriter.write(*videoFrame->get(), [&](auto& semaphore) {
                auto sendResult = send(Messages::RemoteDevice::SetSharedVideoFrameSemaphore { semaphore });
                UNUSED_VARIABLE(sendResult);
            }, [&](WebCore::SharedMemory::Handle&& handle) {
                auto sendResult = send(Messages::RemoteDevice::SetSharedVideoFrameMemory { WTFMove(handle) });
                UNUSED_VARIABLE(sendResult);
            });
            ASSERT(convertedDescriptor->sharedFrame);
        }
    }

    auto sendResult = send(Messages::RemoteDevice::ImportExternalTextureFromVideoFrame(WTFMove(*convertedDescriptor), identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;
#endif

    auto result = RemoteExternalTextureProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteDeviceProxy::updateExternalTexture(const WebCore::WebGPU::ExternalTexture& externalTexture, const WebCore::MediaPlayerIdentifier& mediaPlayerIdentifier)
{
    auto sendResult = send(Messages::RemoteDevice::UpdateExternalTexture(protectedConvertToBackingContext()->convertToBacking(externalTexture), mediaPlayerIdentifier));
    UNUSED_PARAM(sendResult);
}
#endif

RefPtr<WebCore::WebGPU::BindGroupLayout> RemoteDeviceProxy::createBindGroupLayout(const WebCore::WebGPU::BindGroupLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroupLayout(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteBindGroupLayoutProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::PipelineLayout> RemoteDeviceProxy::createPipelineLayout(const WebCore::WebGPU::PipelineLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreatePipelineLayout(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemotePipelineLayoutProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::BindGroup> RemoteDeviceProxy::createBindGroup(const WebCore::WebGPU::BindGroupDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroup(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteBindGroupProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::ShaderModule> RemoteDeviceProxy::createShaderModule(const WebCore::WebGPU::ShaderModuleDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateShaderModule(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteShaderModuleProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::ComputePipeline> RemoteDeviceProxy::createComputePipeline(const WebCore::WebGPU::ComputePipelineDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateComputePipeline(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteComputePipelineProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::RenderPipeline> RemoteDeviceProxy::createRenderPipeline(const WebCore::WebGPU::RenderPipelineDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderPipeline(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteRenderPipelineProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

void RemoteDeviceProxy::createComputePipelineAsync(const WebCore::WebGPU::ComputePipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<WebCore::WebGPU::ComputePipeline>&&, String&&)>&& callback)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(nullptr, "GPUDevice.createComputePipelineAsync() descriptor is invalid"_s);
        return;
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateComputePipelineAsync(*convertedDescriptor, identifier), [identifier, callback = WTFMove(callback), protectedThis = Ref { *this }, label = WTFMove(convertedDescriptor->label)](auto result, String&& error) mutable {
        if (!result) {
            callback(nullptr, WTFMove(error));
            return;
        }

        auto computePipelineResult = RemoteComputePipelineProxy::create(protectedThis, protectedThis->protectedConvertToBackingContext(), identifier);
        computePipelineResult->setLabel(WTFMove(label));
        callback(WTFMove(computePipelineResult), ""_s);
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::createRenderPipelineAsync(const WebCore::WebGPU::RenderPipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<WebCore::WebGPU::RenderPipeline>&&, String&&)>&& callback)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return callback(nullptr, "GPUDevice.createRenderPipelineAsync() descriptor is invalid"_s);

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateRenderPipelineAsync(*convertedDescriptor, identifier), [identifier, callback = WTFMove(callback), protectedThis = Ref { *this }, label = WTFMove(convertedDescriptor->label)](auto result, String&& error) mutable {
        if (!result) {
            callback(nullptr, WTFMove(error));
            return;
        }

        auto renderPipelineResult = RemoteRenderPipelineProxy::create(protectedThis, protectedThis->protectedConvertToBackingContext(), identifier);
        renderPipelineResult->setLabel(WTFMove(label));
        callback(WTFMove(renderPipelineResult), ""_s);
    });
    UNUSED_PARAM(sendResult);
}

RefPtr<WebCore::WebGPU::CommandEncoder> RemoteDeviceProxy::createCommandEncoder(const std::optional<WebCore::WebGPU::CommandEncoderDescriptor>& descriptor)
{
    std::optional<CommandEncoderDescriptor> convertedDescriptor;
    if (descriptor) {
        convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(*descriptor);
        if (!convertedDescriptor)
            return nullptr;
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateCommandEncoder(WTFMove(convertedDescriptor), identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteCommandEncoderProxy::create(protectedRoot(), protectedConvertToBackingContext(), identifier);
    if (convertedDescriptor)
        result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::RenderBundleEncoder> RemoteDeviceProxy::createRenderBundleEncoder(const WebCore::WebGPU::RenderBundleEncoderDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderBundleEncoder(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteRenderBundleEncoderProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

RefPtr<WebCore::WebGPU::QuerySet> RemoteDeviceProxy::createQuerySet(const WebCore::WebGPU::QuerySetDescriptor& descriptor)
{
    auto convertedDescriptor = protectedConvertToBackingContext()->convertToBacking(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateQuerySet(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteQuerySetProxy::create(*this, protectedConvertToBackingContext(), identifier);
    result->setLabel(WTFMove(convertedDescriptor->label));
    return result;
}

void RemoteDeviceProxy::pushErrorScope(WebCore::WebGPU::ErrorFilter errorFilter)
{
    auto sendResult = send(Messages::RemoteDevice::PushErrorScope(errorFilter));
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::popErrorScope(CompletionHandler<void(bool, std::optional<WebCore::WebGPU::Error>&&)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::PopErrorScope(), [callback = WTFMove(callback)](bool success, auto error) mutable {
        if (!error) {
            callback(success, std::nullopt);
            return;
        }

        WTF::switchOn(WTFMove(*error), [&] (OutOfMemoryError&& outOfMemoryError) {
            callback(success, { WebCore::WebGPU::OutOfMemoryError::create() });
        }, [&] (ValidationError&& validationError) {
            callback(success, { WebCore::WebGPU::ValidationError::create(WTFMove(validationError.message)) });
        }, [&] (InternalError&& internalError) {
            callback(success, { WebCore::WebGPU::InternalError::create(WTFMove(internalError.message)) });
        });
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<WebCore::WebGPU::Error>&&)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::ResolveUncapturedErrorEvent(), [callback = WTFMove(callback)](bool success, auto error) mutable {
        if (!error) {
            callback(success, std::nullopt);
            return;
        }

        WTF::switchOn(WTFMove(*error), [&] (OutOfMemoryError&& outOfMemoryError) {
            callback(success, { WebCore::WebGPU::OutOfMemoryError::create() });
        }, [&] (ValidationError&& validationError) {
            callback(success, { WebCore::WebGPU::ValidationError::create(WTFMove(validationError.message)) });
        }, [&] (InternalError&& internalError) {
            callback(success, { WebCore::WebGPU::InternalError::create(WTFMove(internalError.message)) });
        });
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteDevice::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

void RemoteDeviceProxy::resolveDeviceLostPromise(CompletionHandler<void(WebCore::WebGPU::DeviceLostReason)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::ResolveDeviceLostPromise(), [callback = WTFMove(callback)] (WebCore::WebGPU::DeviceLostReason reason) mutable {
        callback(reason);
    });
    UNUSED_PARAM(sendResult);
}

Ref<ConvertToBackingContext> RemoteDeviceProxy::protectedConvertToBackingContext() const
{
    return m_convertToBackingContext;
}

void RemoteDeviceProxy::pauseAllErrorReporting(bool pause)
{
    auto sendResult = send(Messages::RemoteDevice::PauseAllErrorReporting(pause));
    UNUSED_PARAM(sendResult);
}

Ref<WebCore::WebGPU::CommandEncoder> RemoteDeviceProxy::invalidCommandEncoder()
{
    return m_invalidCommandEncoder;
}

Ref<WebCore::WebGPU::CommandBuffer> RemoteDeviceProxy::invalidCommandBuffer()
{
    return m_invalidCommandBuffer;
}

Ref<WebCore::WebGPU::RenderPassEncoder> RemoteDeviceProxy::invalidRenderPassEncoder()
{
    return m_invalidRenderPassEncoder;
}

Ref<WebCore::WebGPU::ComputePassEncoder> RemoteDeviceProxy::invalidComputePassEncoder()
{
    return m_invalidComputePassEncoder;
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
