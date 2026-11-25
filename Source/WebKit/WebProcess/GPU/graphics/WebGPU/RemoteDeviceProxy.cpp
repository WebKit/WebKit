/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include "RemoteCommandBufferProxy.h"
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
#include "WebGPUDescriptorSerializers.h"
#include "WebGPUProxyIdentifiers.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDeviceProxy);

static ::WebGPU::PipelineError invalidDescriptorError(String&& message)
{
    return { .reason = ::WebGPU::PipelineErrorReason::Validation, .message = WTF::move(message) };
}

RemoteDeviceProxy::RemoteDeviceProxy(Vector<::WebGPU::FeatureName>&& features, const ::WebGPU::Limits& limits, RemoteAdapterProxy& parent, WebGPUDeviceIdentifier identifier, WebGPUQueueIdentifier queueIdentifier)
    : m_backing(identifier)
    , m_features(WTF::move(features))
    , m_limits(limits)
    , m_parent(parent)
    , m_queue(RemoteQueueProxy::create(parent, queueIdentifier))
{
    m_queue->setDevice(*this);
}

RemoteDeviceProxy::~RemoteDeviceProxy()
{
    auto sendResult = send(Messages::RemoteDevice::Destruct());
    UNUSED_PARAM(sendResult);
}

Ref<::WebGPU::Queue> RemoteDeviceProxy::queue()
{
    return m_queue;
}

void RemoteDeviceProxy::destroy()
{
    // Destroying the device loses it.
    m_isLost = true;
    auto sendResult = send(Messages::RemoteDevice::Destroy());
    UNUSED_PARAM(sendResult);
}

RefPtr<::WebGPU::XRBinding> RemoteDeviceProxy::createXRBinding()
{
    auto identifier = WebGPUXRBindingIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateXRBinding(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteXRBindingProxy::create(*this, identifier);
}

RefPtr<::WebGPU::Buffer> RemoteDeviceProxy::createBuffer(const ::WebGPU::BufferDescriptor& descriptor)
{
    auto identifier = WebGPUBufferIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBuffer(descriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteBufferProxy::create(*this, identifier, descriptor);
    result->setLabel(String { descriptor.label });
    return result;
}

RefPtr<::WebGPU::Texture> RemoteDeviceProxy::createTexture(const ::WebGPU::TextureDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUTextureIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateTexture(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteTextureProxy::create(*this, identifier, {
        .size = descriptor.size,
        .mipLevelCount = descriptor.mipLevelCount,
        .sampleCount = descriptor.sampleCount,
        .dimension = descriptor.dimension,
        .format = descriptor.format,
        .usage = descriptor.usage,
    });
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::Sampler> RemoteDeviceProxy::createSampler(const ::WebGPU::SamplerDescriptor& descriptor)
{
    auto identifier = WebGPUSamplerIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateSampler(descriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteSamplerProxy::create(*this, identifier);
    result->setLabel(String { descriptor.label });
    return result;
}

RefPtr<::WebGPU::ExternalTexture> RemoteDeviceProxy::importExternalTexture(const WebCore::WebGPUExternalTextureDescriptor& descriptor)
{
    auto identifier = WebGPUExternalTextureIdentifier::generate();

    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

#if PLATFORM(COCOA) && ENABLE(VIDEO)
    if (!convertedDescriptor->mediaIdentifier) {
        auto* videoFrame = std::get_if<RefPtr<WebCore::VideoFrame>>(&descriptor.videoBacking);
        if (videoFrame && videoFrame->get()) {
            convertedDescriptor->sharedFrame = m_sharedVideoFrameWriter.write(*videoFrame->get(), protect(root().gpuProcessIPCConnection()), [this, protectedThis = protect(*this)](auto& semaphore) {
                auto sendResult = send(Messages::RemoteDevice::SetSharedVideoFrameSemaphore { semaphore });
                UNUSED_VARIABLE(sendResult);
            }, [this, protectedThis = protect(*this)](WebCore::SharedMemory::Handle&& handle) {
                auto sendResult = send(Messages::RemoteDevice::SetSharedVideoFrameMemory { WTF::move(handle) });
                UNUSED_VARIABLE(sendResult);
            });
            ASSERT(convertedDescriptor->sharedFrame);
        }
    }

    auto sendResult = send(Messages::RemoteDevice::ImportExternalTextureFromVideoFrame(WTF::move(*convertedDescriptor), identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;
#endif

    auto result = RemoteExternalTextureProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

#if PLATFORM(COCOA)
RefPtr<::WebGPU::ExternalTexture> RemoteDeviceProxy::importExternalTexture(const ::WebGPU::ExternalTextureDescriptor&)
{
    // The Web Process cannot send a pixel buffer to the GPU process. RemoteGPUProxy sends the
    // WebCore source instead, through the overload that takes it.
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteDeviceProxy::updateExternalTexture(const ::WebGPU::ExternalTexture& externalTexture, const WebCore::MediaPlayerIdentifier& mediaPlayerIdentifier)
{
    auto sendResult = send(Messages::RemoteDevice::UpdateExternalTexture(identifier(externalTexture), mediaPlayerIdentifier));
    UNUSED_PARAM(sendResult);
}
#endif

RefPtr<::WebGPU::BindGroupLayout> RemoteDeviceProxy::createBindGroupLayout(const ::WebGPU::BindGroupLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUBindGroupLayoutIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroupLayout(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteBindGroupLayoutProxy::create(protect(root()), identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::PipelineLayout> RemoteDeviceProxy::createPipelineLayout(const ::WebGPU::PipelineLayoutDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUPipelineLayoutIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreatePipelineLayout(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemotePipelineLayoutProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::BindGroup> RemoteDeviceProxy::createBindGroup(const ::WebGPU::BindGroupDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUBindGroupIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateBindGroup(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteBindGroupProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::ShaderModule> RemoteDeviceProxy::createShaderModule(const ::WebGPU::ShaderModuleDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUShaderModuleIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateShaderModule(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteShaderModuleProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::ComputePipeline> RemoteDeviceProxy::createComputePipeline(const ::WebGPU::ComputePipelineDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPUComputePipelineIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateComputePipeline(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteComputePipelineProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::RenderPipeline> RemoteDeviceProxy::createRenderPipeline(const ::WebGPU::RenderPipelineDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPURenderPipelineIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderPipeline(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteRenderPipelineProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

void RemoteDeviceProxy::createComputePipelineAsync(const ::WebGPU::ComputePipelineDescriptor& descriptor, CompletionHandler<void(std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&&)>&& callback)
{
    auto convertedDescriptor = serialize(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor) {
        callback(makeUnexpected(invalidDescriptorError("GPUDevice.createComputePipelineAsync() descriptor is invalid"_s)));
        return;
    }

    auto identifier = WebGPUComputePipelineIdentifier::generate();
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateComputePipelineAsync(*convertedDescriptor, identifier), [identifier, callback = WTF::move(callback), protectedThis = protect(*this), label = WTF::move(convertedDescriptor->label)](auto result, String&& error) mutable {
        if (!result) {
            callback(makeUnexpected(::WebGPU::PipelineError { .reason = ::WebGPU::PipelineErrorReason::Validation, .message = WTF::move(error) }));
            return;
        }

        auto computePipelineResult = RemoteComputePipelineProxy::create(protectedThis, identifier);
        computePipelineResult->setLabel(WTF::move(label));
        callback(Ref<::WebGPU::ComputePipeline> { WTF::move(computePipelineResult) });
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::createRenderPipelineAsync(const ::WebGPU::RenderPipelineDescriptor& descriptor, CompletionHandler<void(std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&&)>&& callback)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return callback(makeUnexpected(invalidDescriptorError("GPUDevice.createRenderPipelineAsync() descriptor is invalid"_s)));

    auto identifier = WebGPURenderPipelineIdentifier::generate();
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateRenderPipelineAsync(*convertedDescriptor, identifier), [identifier, callback = WTF::move(callback), protectedThis = protect(*this), label = WTF::move(convertedDescriptor->label)](auto result, String&& error) mutable {
        if (!result) {
            callback(makeUnexpected(::WebGPU::PipelineError { .reason = ::WebGPU::PipelineErrorReason::Validation, .message = WTF::move(error) }));
            return;
        }

        auto renderPipelineResult = RemoteRenderPipelineProxy::create(protectedThis, identifier);
        renderPipelineResult->setLabel(WTF::move(label));
        callback(Ref<::WebGPU::RenderPipeline> { WTF::move(renderPipelineResult) });
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::createComputePipelineWithPipelineLayoutFromPipelineAsync(const ::WebGPU::ComputePipelineDescriptor& descriptor, const ::WebGPU::ComputePipeline& pipelineToReplace, CompletionHandler<void(std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&&)>&& callback)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor) {
        callback(makeUnexpected(invalidDescriptorError({ })));
        return;
    }

    auto identifier = WebGPUComputePipelineIdentifier::generate();
    auto pipelineToReplaceIdentifier = WebGPU::identifier(pipelineToReplace);
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateComputePipelineWithPipelineLayoutFromPipeline(*convertedDescriptor, identifier, pipelineToReplaceIdentifier), [identifier, callback = WTF::move(callback), protectedThis = protect(*this), label = WTF::move(convertedDescriptor->label)](bool success) mutable {
        if (!success) {
            callback(makeUnexpected(invalidDescriptorError({ })));
            return;
        }

        Ref result = RemoteComputePipelineProxy::create(protectedThis, identifier);
        result->setLabel(WTF::move(label));
        callback(Ref<::WebGPU::ComputePipeline> { WTF::move(result) });
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::createRenderPipelineWithPipelineLayoutFromPipelineAsync(const ::WebGPU::RenderPipelineDescriptor& descriptor, const ::WebGPU::RenderPipeline& pipelineToReplace, CompletionHandler<void(std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&&)>&& callback)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor) {
        callback(makeUnexpected(invalidDescriptorError({ })));
        return;
    }

    auto identifier = WebGPURenderPipelineIdentifier::generate();
    auto pipelineToReplaceIdentifier = WebGPU::identifier(pipelineToReplace);
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::CreateRenderPipelineWithPipelineLayoutFromPipeline(*convertedDescriptor, identifier, pipelineToReplaceIdentifier), [identifier, callback = WTF::move(callback), protectedThis = protect(*this), label = WTF::move(convertedDescriptor->label)](bool success) mutable {
        if (!success) {
            callback(makeUnexpected(invalidDescriptorError({ })));
            return;
        }

        Ref result = RemoteRenderPipelineProxy::create(protectedThis, identifier);
        result->setLabel(WTF::move(label));
        callback(Ref<::WebGPU::RenderPipeline> { WTF::move(result) });
    });
    UNUSED_PARAM(sendResult);
}

RefPtr<::WebGPU::CommandEncoder> RemoteDeviceProxy::createCommandEncoder(const ::WebGPU::CommandEncoderDescriptor& descriptor)
{
    auto identifier = WebGPUCommandEncoderIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateCommandEncoder(descriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteCommandEncoderProxy::create(protect(root()), identifier);
    result->setLabel(String { descriptor.label });
    return result;
}

RefPtr<::WebGPU::RenderBundleEncoder> RemoteDeviceProxy::createRenderBundleEncoder(const ::WebGPU::RenderBundleEncoderDescriptor& descriptor)
{
    auto convertedDescriptor = serialize(descriptor);
    if (!convertedDescriptor)
        return nullptr;

    auto identifier = WebGPURenderBundleEncoderIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateRenderBundleEncoder(*convertedDescriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteRenderBundleEncoderProxy::create(*this, identifier);
    result->setLabel(WTF::move(convertedDescriptor->label));
    return result;
}

RefPtr<::WebGPU::QuerySet> RemoteDeviceProxy::createQuerySet(const ::WebGPU::QuerySetDescriptor& descriptor)
{
    auto identifier = WebGPUQuerySetIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateQuerySet(descriptor, identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    auto result = RemoteQuerySetProxy::create(*this, identifier, descriptor);
    result->setLabel(String { descriptor.label });
    return result;
}

void RemoteDeviceProxy::pushErrorScope(::WebGPU::ErrorFilter errorFilter)
{
    auto sendResult = send(Messages::RemoteDevice::PushErrorScope(errorFilter));
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::popErrorScope(CompletionHandler<void(bool, std::optional<::WebGPU::Error>&&)>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::PopErrorScope(), [callback = WTF::move(callback)](bool success, auto error) mutable {
        callback(success, WTF::move(error));
    });
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::setUncapturedErrorCallback(Function<void(::WebGPU::Error&&)>&& callback)
{
    bool wasListening = !!m_uncapturedErrorCallback;
    m_uncapturedErrorCallback = callback ? RefPtr { createSharedTask<void(::WebGPU::Error&&)>(WTF::move(callback)) } : nullptr;
    bool listens = !!m_uncapturedErrorCallback;
    if (listens == wasListening)
        return;

    // The GPU process sends each uncaptured error to RemoteGPUProxy, as soon as the device generates it.
    Ref root = this->root();
    if (listens)
        root->addUncapturedErrorListener(m_backing, *this);
    else
        root->removeUncapturedErrorListener(m_backing);
    auto sendResult = send(Messages::RemoteDevice::SetListensForUncapturedErrors(listens));
    UNUSED_PARAM(sendResult);
}

void RemoteDeviceProxy::didGenerateUncapturedError(::WebGPU::Error&& error)
{
    if (RefPtr callback = m_uncapturedErrorCallback)
        callback->run(WTF::move(error));
}

void RemoteDeviceProxy::setLabel(String&& label)
{
    auto sendResult = send(Messages::RemoteDevice::SetLabel(WTF::move(label)));
    UNUSED_VARIABLE(sendResult);
}

bool RemoteDeviceProxy::isValid() const
{
    // The Web Process cannot know. RemoteGPU::isValid() answers it for tests.
    RELEASE_ASSERT_NOT_REACHED();
}

void RemoteDeviceProxy::resolveDeviceLostPromise(CompletionHandler<void(::WebGPU::DeviceLostReason, String&&)>&& callback)
{
    if (protect(root())->isLost()) {
        m_isLost = true;
        callback(::WebGPU::DeviceLostReason::Unknown, { });
        return;
    }

    // The GPU process does not send the message, so the device is lost with an empty one. The reply
    // says whether the device is lost: setting another callback completes this one without losing
    // the device.
    auto sendResult = sendWithAsyncReply(Messages::RemoteDevice::ResolveDeviceLostPromise(), [weakThis = ThreadSafeWeakPtr { *this }, callback = WTF::move(callback)](::WebGPU::DeviceLostReason reason, bool isLost) mutable {
        if (RefPtr protectedThis = weakThis; protectedThis && isLost)
            protectedThis->m_isLost = true;
        callback(reason, { });
    });
    UNUSED_PARAM(sendResult);
}

bool RemoteDeviceProxy::isLost() const
{
    // The proxy learns that the GPU Process lost the device from the reply of
    // ResolveDeviceLostPromise, which says whether the device is lost, so asynchronously, and only
    // while there is a callback, as GPUDevice always sets. It knows synchronously when it is
    // destroyed and when the GPU Process is gone.
    return m_isLost || protect(m_parent->root())->isLost();
}

void RemoteDeviceProxy::pauseAllErrorReporting(bool pause)
{
    auto sendResult = send(Messages::RemoteDevice::PauseAllErrorReporting(pause));
    UNUSED_PARAM(sendResult);
}

RefPtr<::WebGPU::BindGroup> RemoteDeviceProxy::createInvalidBindGroup()
{
    auto identifier = WebGPUBindGroupIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidBindGroup(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteBindGroupProxy::create(*this, identifier);
}

RefPtr<::WebGPU::BindGroupLayout> RemoteDeviceProxy::createInvalidBindGroupLayout()
{
    auto identifier = WebGPUBindGroupLayoutIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidBindGroupLayout(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteBindGroupLayoutProxy::create(protect(root()), identifier);
}

RefPtr<::WebGPU::Buffer> RemoteDeviceProxy::createInvalidBuffer()
{
    auto identifier = WebGPUBufferIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidBuffer(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteBufferProxy::create(*this, identifier, ::WebGPU::BufferDescriptor { });
}

RefPtr<::WebGPU::CommandBuffer> RemoteDeviceProxy::createInvalidCommandBuffer()
{
    auto identifier = WebGPUCommandBufferIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidCommandBuffer(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteCommandBufferProxy::create(protect(root()), identifier);
}

RefPtr<::WebGPU::CommandEncoder> RemoteDeviceProxy::createInvalidCommandEncoder()
{
    auto identifier = WebGPUCommandEncoderIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidCommandEncoder(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteCommandEncoderProxy::create(protect(root()), identifier);
}

RefPtr<::WebGPU::ComputePipeline> RemoteDeviceProxy::createInvalidComputePipeline()
{
    auto identifier = WebGPUComputePipelineIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidComputePipeline(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteComputePipelineProxy::create(*this, identifier);
}

RefPtr<::WebGPU::PipelineLayout> RemoteDeviceProxy::createInvalidPipelineLayout()
{
    auto identifier = WebGPUPipelineLayoutIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidPipelineLayout(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemotePipelineLayoutProxy::create(*this, identifier);
}

RefPtr<::WebGPU::QuerySet> RemoteDeviceProxy::createInvalidQuerySet()
{
    auto identifier = WebGPUQuerySetIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidQuerySet(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteQuerySetProxy::createInvalid(*this, identifier);
}

RefPtr<::WebGPU::RenderBundleEncoder> RemoteDeviceProxy::createInvalidRenderBundleEncoder(String&& error)
{
    auto identifier = WebGPURenderBundleEncoderIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidRenderBundleEncoder(WTF::move(error), identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteRenderBundleEncoderProxy::create(*this, identifier);
}

RefPtr<::WebGPU::RenderPipeline> RemoteDeviceProxy::createInvalidRenderPipeline()
{
    auto identifier = WebGPURenderPipelineIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidRenderPipeline(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteRenderPipelineProxy::create(*this, identifier);
}

RefPtr<::WebGPU::Sampler> RemoteDeviceProxy::createInvalidSampler()
{
    auto identifier = WebGPUSamplerIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidSampler(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteSamplerProxy::create(*this, identifier);
}

RefPtr<::WebGPU::ShaderModule> RemoteDeviceProxy::createInvalidShaderModule()
{
    auto identifier = WebGPUShaderModuleIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidShaderModule(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    return RemoteShaderModuleProxy::create(*this, identifier);
}

RefPtr<::WebGPU::Texture> RemoteDeviceProxy::createInvalidTexture()
{
    auto identifier = WebGPUTextureIdentifier::generate();
    auto sendResult = send(Messages::RemoteDevice::CreateInvalidTexture(identifier));
    if (sendResult != IPC::Error::NoError)
        return nullptr;

    // The properties of the Metal invalid texture.
    return RemoteTextureProxy::create(*this, identifier, {
        .size = { 0, 0, 0 },
        .mipLevelCount = 0,
        .sampleCount = 0,
        .dimension = ::WebGPU::TextureDimension::_2d,
        .format = std::nullopt,
        .usage = { },
    });
}

void RemoteDeviceProxy::generateAValidationError(String&& message)
{
    auto sendResult = send(Messages::RemoteDevice::GenerateAValidationError(WTF::move(message)));
    UNUSED_PARAM(sendResult);
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
