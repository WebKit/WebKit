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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterProxy.h"
#include "SharedVideoFrame.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCommandEncoderDescriptor.h>
#include <WebCore/WebGPUDevice.h>
#include <wtf/Deque.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include <WebCore/MediaPlayerIdentifier.h>
#endif

namespace WebKit::WebGPU {

class ConvertToBackingContext;
class RemoteQueueProxy;

class RemoteDeviceProxy final : public WebCore::WebGPU::Device {
    WTF_MAKE_TZONE_ALLOCATED(RemoteDeviceProxy);
public:
    static Ref<RemoteDeviceProxy> create(Ref<WebCore::WebGPU::SupportedFeatures>&& features, Ref<WebCore::WebGPU::SupportedLimits>&& limits, RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    {
        return adoptRef(*new RemoteDeviceProxy(WTFMove(features), WTFMove(limits), parent, convertToBackingContext, identifier, queueIdentifier));
    }

    virtual ~RemoteDeviceProxy();

    RemoteAdapterProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }
    WebGPUIdentifier backing() const { return m_backing; }

private:
    friend class DowncastConvertToBackingContext;

    RemoteDeviceProxy(Ref<WebCore::WebGPU::SupportedFeatures>&&, Ref<WebCore::WebGPU::SupportedLimits>&&, RemoteAdapterProxy&, ConvertToBackingContext&, WebGPUIdentifier, WebGPUIdentifier queueIdentifier);

    RemoteDeviceProxy(const RemoteDeviceProxy&) = delete;
    RemoteDeviceProxy(RemoteDeviceProxy&&) = delete;
    RemoteDeviceProxy& operator=(const RemoteDeviceProxy&) = delete;
    RemoteDeviceProxy& operator=(RemoteDeviceProxy&&) = delete;

    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing());
    }
    template<typename T, typename C>
    WARN_UNUSED_RETURN IPC::StreamClientConnection::AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return root().streamClientConnection().sendWithAsyncReply(WTFMove(message), completionHandler, backing());
    }

    Ref<WebCore::WebGPU::Queue> queue() final;

    void destroy() final;

    RefPtr<WebCore::WebGPU::XRBinding> createXRBinding() final;
    RefPtr<WebCore::WebGPU::Buffer> createBuffer(const WebCore::WebGPU::BufferDescriptor&) final;
    RefPtr<WebCore::WebGPU::Texture> createTexture(const WebCore::WebGPU::TextureDescriptor&) final;
    RefPtr<WebCore::WebGPU::Sampler> createSampler(const WebCore::WebGPU::SamplerDescriptor&) final;
    RefPtr<WebCore::WebGPU::ExternalTexture> importExternalTexture(const WebCore::WebGPU::ExternalTextureDescriptor&) final;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    void updateExternalTexture(const WebCore::WebGPU::ExternalTexture&, const WebCore::MediaPlayerIdentifier&) final;
#endif

    RefPtr<WebCore::WebGPU::BindGroupLayout> createBindGroupLayout(const WebCore::WebGPU::BindGroupLayoutDescriptor&) final;
    RefPtr<WebCore::WebGPU::PipelineLayout> createPipelineLayout(const WebCore::WebGPU::PipelineLayoutDescriptor&) final;
    RefPtr<WebCore::WebGPU::BindGroup> createBindGroup(const WebCore::WebGPU::BindGroupDescriptor&) final;

    RefPtr<WebCore::WebGPU::ShaderModule> createShaderModule(const WebCore::WebGPU::ShaderModuleDescriptor&) final;
    RefPtr<WebCore::WebGPU::ComputePipeline> createComputePipeline(const WebCore::WebGPU::ComputePipelineDescriptor&) final;
    RefPtr<WebCore::WebGPU::RenderPipeline> createRenderPipeline(const WebCore::WebGPU::RenderPipelineDescriptor&) final;
    void createComputePipelineAsync(const WebCore::WebGPU::ComputePipelineDescriptor&, CompletionHandler<void(RefPtr<WebCore::WebGPU::ComputePipeline>&&, String&&)>&&) final;
    void createRenderPipelineAsync(const WebCore::WebGPU::RenderPipelineDescriptor&, CompletionHandler<void(RefPtr<WebCore::WebGPU::RenderPipeline>&&, String&&)>&&) final;

    RefPtr<WebCore::WebGPU::CommandEncoder> createCommandEncoder(const std::optional<WebCore::WebGPU::CommandEncoderDescriptor>&) final;
    RefPtr<WebCore::WebGPU::RenderBundleEncoder> createRenderBundleEncoder(const WebCore::WebGPU::RenderBundleEncoderDescriptor&) final;

    RefPtr<WebCore::WebGPU::QuerySet> createQuerySet(const WebCore::WebGPU::QuerySetDescriptor&) final;

    void pushErrorScope(WebCore::WebGPU::ErrorFilter) final;
    void popErrorScope(CompletionHandler<void(bool, std::optional<WebCore::WebGPU::Error>&&)>&&) final;
    void resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<WebCore::WebGPU::Error>&&)>&&) final;

    void setLabelInternal(const String&) final;
    void resolveDeviceLostPromise(CompletionHandler<void(WebCore::WebGPU::DeviceLostReason)>&&) final;

    Deque<CompletionHandler<void(Ref<WebCore::WebGPU::ComputePipeline>&&)>> m_createComputePipelineAsyncCallbacks;
    Deque<CompletionHandler<void(Ref<WebCore::WebGPU::RenderPipeline>&&)>> m_createRenderPipelineAsyncCallbacks;
    Deque<CompletionHandler<void(std::optional<WebCore::WebGPU::Error>&&)>> m_popErrorScopeCallbacks;

    WebGPUIdentifier m_backing;
    WebGPUIdentifier m_queueBacking;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteAdapterProxy> m_parent;
    Ref<RemoteQueueProxy> m_queue;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    WebKit::SharedVideoFrameWriter m_sharedVideoFrameWriter;
#endif
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
