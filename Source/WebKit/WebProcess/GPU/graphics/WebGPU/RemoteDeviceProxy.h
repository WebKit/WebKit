/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUCommandEncoderDescriptor.h>
#include <pal/graphics/WebGPU/WebGPUDevice.h>
#include <wtf/Deque.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;
class RemoteQueueProxy;

class RemoteDeviceProxy final : public PAL::WebGPU::Device {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteDeviceProxy> create(Ref<PAL::WebGPU::SupportedFeatures>&& features, Ref<PAL::WebGPU::SupportedLimits>&& limits, RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    {
        return adoptRef(*new RemoteDeviceProxy(WTFMove(features), WTFMove(limits), parent, convertToBackingContext, identifier, queueIdentifier));
    }

    virtual ~RemoteDeviceProxy();

    RemoteAdapterProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteDeviceProxy(Ref<PAL::WebGPU::SupportedFeatures>&&, Ref<PAL::WebGPU::SupportedLimits>&&, RemoteAdapterProxy&, ConvertToBackingContext&, WebGPUIdentifier, WebGPUIdentifier queueIdentifier);

    RemoteDeviceProxy(const RemoteDeviceProxy&) = delete;
    RemoteDeviceProxy(RemoteDeviceProxy&&) = delete;
    RemoteDeviceProxy& operator=(const RemoteDeviceProxy&) = delete;
    RemoteDeviceProxy& operator=(RemoteDeviceProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    static inline constexpr Seconds defaultSendTimeout = 30_s;
    template<typename T>
    WARN_UNUSED_RETURN bool send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing(), defaultSendTimeout);
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing(), defaultSendTimeout);
    }

    PAL::WebGPU::Queue& queue() final;

    void destroy() final;

    Ref<PAL::WebGPU::Buffer> createBuffer(const PAL::WebGPU::BufferDescriptor&) final;
    Ref<PAL::WebGPU::Texture> createTexture(const PAL::WebGPU::TextureDescriptor&) final;
    Ref<PAL::WebGPU::Texture> createSurfaceTexture(const PAL::WebGPU::TextureDescriptor&, const PAL::WebGPU::Surface&) final;
    Ref<PAL::WebGPU::Surface> createSurface(const PAL::WebGPU::SurfaceDescriptor&) final;
    Ref<PAL::WebGPU::SwapChain> createSwapChain(const PAL::WebGPU::Surface&, const PAL::WebGPU::SwapChainDescriptor&) final;
    Ref<PAL::WebGPU::Sampler> createSampler(const PAL::WebGPU::SamplerDescriptor&) final;
    Ref<PAL::WebGPU::ExternalTexture> importExternalTexture(const PAL::WebGPU::ExternalTextureDescriptor&) final;

    Ref<PAL::WebGPU::BindGroupLayout> createBindGroupLayout(const PAL::WebGPU::BindGroupLayoutDescriptor&) final;
    Ref<PAL::WebGPU::PipelineLayout> createPipelineLayout(const PAL::WebGPU::PipelineLayoutDescriptor&) final;
    Ref<PAL::WebGPU::BindGroup> createBindGroup(const PAL::WebGPU::BindGroupDescriptor&) final;

    Ref<PAL::WebGPU::ShaderModule> createShaderModule(const PAL::WebGPU::ShaderModuleDescriptor&) final;
    Ref<PAL::WebGPU::ComputePipeline> createComputePipeline(const PAL::WebGPU::ComputePipelineDescriptor&) final;
    Ref<PAL::WebGPU::RenderPipeline> createRenderPipeline(const PAL::WebGPU::RenderPipelineDescriptor&) final;
    void createComputePipelineAsync(const PAL::WebGPU::ComputePipelineDescriptor&, CompletionHandler<void(Ref<PAL::WebGPU::ComputePipeline>&&)>&&) final;
    void createRenderPipelineAsync(const PAL::WebGPU::RenderPipelineDescriptor&, CompletionHandler<void(Ref<PAL::WebGPU::RenderPipeline>&&)>&&) final;

    Ref<PAL::WebGPU::CommandEncoder> createCommandEncoder(const std::optional<PAL::WebGPU::CommandEncoderDescriptor>&) final;
    Ref<PAL::WebGPU::RenderBundleEncoder> createRenderBundleEncoder(const PAL::WebGPU::RenderBundleEncoderDescriptor&) final;

    Ref<PAL::WebGPU::QuerySet> createQuerySet(const PAL::WebGPU::QuerySetDescriptor&) final;

    void pushErrorScope(PAL::WebGPU::ErrorFilter) final;
    void popErrorScope(CompletionHandler<void(std::optional<PAL::WebGPU::Error>&&)>&&) final;

    void setLabelInternal(const String&) final;

    Deque<CompletionHandler<void(Ref<PAL::WebGPU::ComputePipeline>&&)>> m_createComputePipelineAsyncCallbacks;
    Deque<CompletionHandler<void(Ref<PAL::WebGPU::RenderPipeline>&&)>> m_createRenderPipelineAsyncCallbacks;
    Deque<CompletionHandler<void(std::optional<PAL::WebGPU::Error>&&)>> m_popErrorScopeCallbacks;

    WebGPUIdentifier m_backing;
    WebGPUIdentifier m_queueBacking;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteAdapterProxy> m_parent;
    Ref<RemoteQueueProxy> m_queue;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
