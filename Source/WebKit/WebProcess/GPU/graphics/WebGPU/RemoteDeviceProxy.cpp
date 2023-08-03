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
#include "SharedVideoFrame.h"
#include "WebGPUCommandEncoderDescriptor.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteDeviceProxy::RemoteDeviceProxy(Ref<WebCore::WebGPU::SupportedFeatures>&& features, Ref<WebCore::WebGPU::SupportedLimits>&& limits, RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    : Device(WTFMove(features), WTFMove(limits))
    , m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
    , m_queue(RemoteQueueProxy::create(parent, convertToBackingContext, queueIdentifier))
{
}

RemoteDeviceProxy::~RemoteDeviceProxy()
{
    auto sendResult = send(Messages::RemoteDevice::Destruct());
    UNUSED_VARIABLE(sendResult);
}

Ref<WebCore::WebGPU::Queue> RemoteDeviceProxy::queue()
{
    return m_queue;
}

void RemoteDeviceProxy::destroy()
{
    auto sendResult = send(Messages::RemoteDevice::Destroy());
    UNUSED_VARIABLE(sendResult);
}

Ref<WebCore::WebGPU::Buffer> RemoteDeviceProxy::createBuffer(const WebCore::WebGPU::BufferDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteBufferProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBuffer(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteBufferProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::Texture> RemoteDeviceProxy::createTexture(const WebCore::WebGPU::TextureDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteTextureProxy::create(root(), m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateTexture(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteTextureProxy::create(root(), m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::Sampler> RemoteDeviceProxy::createSampler(const WebCore::WebGPU::SamplerDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteSamplerProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateSampler(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteSamplerProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::ExternalTexture> RemoteDeviceProxy::importExternalTexture(const WebCore::WebGPU::ExternalTextureDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteExternalTextureProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    if (auto pixelBuffer = descriptor.pixelBuffer) {
        WebKit::SharedVideoFrameWriter sharedVideoFrameWriter;
        constexpr bool canSendIOSurface = false;
        auto sharedVideoFrameBuffer = sharedVideoFrameWriter.writeBuffer(pixelBuffer.get(), [&](auto& semaphore) {
            auto sendResult = send(Messages::RemoteDevice::SetSharedVideoFrameSemaphore { semaphore });
            UNUSED_VARIABLE(sendResult);
        }, [&](auto&& handle) {
            auto sendResult = send(Messages::RemoteDevice::SetSharedVideoFrameMemory { WTFMove(handle) });
            UNUSED_VARIABLE(sendResult);
        }, canSendIOSurface);

        auto sendResult = send(Messages::RemoteDevice::ImportExternalTextureFromPixelBuffer(*convertedDescriptor, WTFMove(sharedVideoFrameBuffer), identifier));
        UNUSED_VARIABLE(sendResult);
    }  else {
        auto sendResult = send(Messages::RemoteDevice::ImportExternalTexture(*convertedDescriptor, identifier));
        UNUSED_VARIABLE(sendResult);
    }
#else
    auto sendResult = send(Messages::RemoteDevice::ImportExternalTexture(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);
#endif

    return RemoteExternalTextureProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::BindGroupLayout> RemoteDeviceProxy::createBindGroupLayout(const WebCore::WebGPU::BindGroupLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteBindGroupLayoutProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroupLayout(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteBindGroupLayoutProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::PipelineLayout> RemoteDeviceProxy::createPipelineLayout(const WebCore::WebGPU::PipelineLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemotePipelineLayoutProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreatePipelineLayout(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemotePipelineLayoutProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::BindGroup> RemoteDeviceProxy::createBindGroup(const WebCore::WebGPU::BindGroupDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteBindGroupProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroup(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteBindGroupProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::ShaderModule> RemoteDeviceProxy::createShaderModule(const WebCore::WebGPU::ShaderModuleDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteShaderModuleProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateShaderModule(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteShaderModuleProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::ComputePipeline> RemoteDeviceProxy::createComputePipeline(const WebCore::WebGPU::ComputePipelineDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteComputePipelineProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateComputePipeline(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteComputePipelineProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::RenderPipeline> RemoteDeviceProxy::createRenderPipeline(const WebCore::WebGPU::RenderPipelineDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteRenderPipelineProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderPipeline(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteRenderPipelineProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteDeviceProxy::createComputePipelineAsync(const WebCore::WebGPU::ComputePipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<WebCore::WebGPU::ComputePipeline>&&)>&& callback)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(nullptr);
        return;
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateComputePipelineAsync(*convertedDescriptor, identifier), [identifier, callback = WTFMove(callback), strongThis = Ref { *this }](auto result) mutable {
        if (!result) {
            callback(nullptr);
            return;
        }

        callback(RemoteComputePipelineProxy::create(strongThis, strongThis->m_convertToBackingContext, identifier));
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::createRenderPipelineAsync(const WebCore::WebGPU::RenderPipelineDescriptor& descriptor, CompletionHandler<void(RefPtr<WebCore::WebGPU::RenderPipeline>&&)>&& callback)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateRenderPipelineAsync(*convertedDescriptor, identifier), [identifier, callback = WTFMove(callback), strongThis = Ref { *this }](auto result) mutable {
        if (!result) {
            callback(nullptr);
            return;
        }

        callback(RemoteRenderPipelineProxy::create(strongThis, strongThis->m_convertToBackingContext, identifier));
    });
    UNUSED_PARAM(sendResult);
}

Ref<WebCore::WebGPU::CommandEncoder> RemoteDeviceProxy::createCommandEncoder(const std::optional<WebCore::WebGPU::CommandEncoderDescriptor>& descriptor)
{
    std::optional<CommandEncoderDescriptor> convertedDescriptor;
    if (descriptor) {
        convertedDescriptor = m_convertToBackingContext->convertToBacking(*descriptor);
        if (!convertedDescriptor) {
            // FIXME: Implement error handling.
            return RemoteCommandEncoderProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
        }
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateCommandEncoder(WTFMove(convertedDescriptor), identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteCommandEncoderProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::RenderBundleEncoder> RemoteDeviceProxy::createRenderBundleEncoder(const WebCore::WebGPU::RenderBundleEncoderDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteRenderBundleEncoderProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderBundleEncoder(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteRenderBundleEncoderProxy::create(*this, m_convertToBackingContext, identifier);
}

Ref<WebCore::WebGPU::QuerySet> RemoteDeviceProxy::createQuerySet(const WebCore::WebGPU::QuerySetDescriptor& descriptor)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    if (!convertedDescriptor) {
        // FIXME: Implement error handling.
        return RemoteQuerySetProxy::create(*this, m_convertToBackingContext, WebGPUIdentifier::generate());
    }

    auto identifier = WebGPUIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateQuerySet(*convertedDescriptor, identifier));
    UNUSED_VARIABLE(sendResult);

    return RemoteQuerySetProxy::create(*this, m_convertToBackingContext, identifier);
}

void RemoteDeviceProxy::pushErrorScope(WebCore::WebGPU::ErrorFilter errorFilter)
{
    auto sendResult = send(Messages::RemoteDevice::PushErrorScope(errorFilter));
    UNUSED_VARIABLE(sendResult);
}

void RemoteDeviceProxy::popErrorScope(CompletionHandler<void(std::optional<WebCore::WebGPU::Error>&&)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::PopErrorScope(), [callback = WTFMove(callback)](auto error) mutable {
        if (!error) {
            callback(std::nullopt);
            return;
        }

        WTF::switchOn(WTFMove(*error), [&] (OutOfMemoryError&& outOfMemoryError) {
            callback({ WebCore::WebGPU::OutOfMemoryError::create() });
        }, [&] (ValidationError&& validationError) {
            callback({ WebCore::WebGPU::ValidationError::create(WTFMove(validationError.message)) });
        });
    });

    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteDevice::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
