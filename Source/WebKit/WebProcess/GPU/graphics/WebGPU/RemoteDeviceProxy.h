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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterProxy.h"
#include "SharedVideoFrame.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>
#include <WebCore/WebGPURenderBundleEncoderDescriptor.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include <WebCore/MediaPlayerIdentifier.h>
#endif

namespace WebKit::WebGPU {

class ConvertToBackingContext;
class RemoteQueueProxy;

class RemoteDeviceProxy final : public ::WebGPU::Device {
    WTF_MAKE_TZONE_ALLOCATED(RemoteDeviceProxy);
public:
    static Ref<RemoteDeviceProxy> create(Vector<::WebGPU::FeatureName>&& features, const ::WebGPU::Limits& limits, RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, WebGPUIdentifier queueIdentifier)
    {
        return adoptRef(*new RemoteDeviceProxy(WTF::move(features), limits, parent, convertToBackingContext, identifier, queueIdentifier));
    }

    virtual ~RemoteDeviceProxy();

    RemoteAdapterProxy& parent() const { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

    // Called by RemoteGPUProxy, which implements the device commands that take WebCore sources.
    RefPtr<::WebGPU::ExternalTexture> importExternalTexture(const WebCore::WebGPUExternalTextureDescriptor&);
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    void updateExternalTexture(const ::WebGPU::ExternalTexture&, const WebCore::MediaPlayerIdentifier&);
#endif
    WebGPUIdentifier backing() const { return m_backing; }

    Vector<::WebGPU::FeatureName> features() const final { return m_features; }
    const ::WebGPU::Limits& limits() const LIFETIME_BOUND final { return m_limits; }
    Ref<::WebGPU::Queue> NODELETE queue() final;
    void destroy() final;

    RefPtr<::WebGPU::Buffer> createBuffer(const ::WebGPU::BufferDescriptor&) final;
    RefPtr<::WebGPU::Texture> createTexture(const ::WebGPU::TextureDescriptor&) final;
    RefPtr<::WebGPU::Sampler> createSampler(const ::WebGPU::SamplerDescriptor&) final;
#if PLATFORM(COCOA)
    RefPtr<::WebGPU::ExternalTexture> importExternalTexture(const ::WebGPU::ExternalTextureDescriptor&) final;
#endif
    RefPtr<::WebGPU::BindGroupLayout> createBindGroupLayout(const ::WebGPU::BindGroupLayoutDescriptor&) final;
    RefPtr<::WebGPU::PipelineLayout> createPipelineLayout(const ::WebGPU::PipelineLayoutDescriptor&) final;
    RefPtr<::WebGPU::BindGroup> createBindGroup(const ::WebGPU::BindGroupDescriptor&) final;
    RefPtr<::WebGPU::ShaderModule> createShaderModule(const ::WebGPU::ShaderModuleDescriptor&) final;
    RefPtr<::WebGPU::ComputePipeline> createComputePipeline(const ::WebGPU::ComputePipelineDescriptor&) final;
    RefPtr<::WebGPU::RenderPipeline> createRenderPipeline(const ::WebGPU::RenderPipelineDescriptor&) final;
    void createComputePipelineAsync(const ::WebGPU::ComputePipelineDescriptor&, CompletionHandler<void(std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&&)>&&) final;
    void createRenderPipelineAsync(const ::WebGPU::RenderPipelineDescriptor&, CompletionHandler<void(std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&&)>&&) final;
    void createComputePipelineWithPipelineLayoutFromPipelineAsync(const ::WebGPU::ComputePipelineDescriptor&, const ::WebGPU::ComputePipeline&, CompletionHandler<void(std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&&)>&&) final;
    void createRenderPipelineWithPipelineLayoutFromPipelineAsync(const ::WebGPU::RenderPipelineDescriptor&, const ::WebGPU::RenderPipeline&, CompletionHandler<void(std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&&)>&&) final;
    RefPtr<::WebGPU::CommandEncoder> createCommandEncoder(const ::WebGPU::CommandEncoderDescriptor&) final;
    RefPtr<::WebGPU::RenderBundleEncoder> createRenderBundleEncoder(const ::WebGPU::RenderBundleEncoderDescriptor&) final;
    RefPtr<::WebGPU::QuerySet> createQuerySet(const ::WebGPU::QuerySetDescriptor&) final;
    RefPtr<::WebGPU::XRBinding> createXRBinding() final;

    void pushErrorScope(::WebGPU::ErrorFilter) final;
    void popErrorScope(CompletionHandler<void(bool, std::optional<::WebGPU::Error>&&)>&&) final;
    void resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<::WebGPU::Error>&&)>&&) final;
    void resolveDeviceLostPromise(CompletionHandler<void(::WebGPU::DeviceLostReason, String&&)>&&) final;
    void pauseAllErrorReporting(bool pause) final;
    void setLabel(String&&) final;
    bool isValid() const final;

private:
    friend class DowncastConvertToBackingContext;

    RemoteDeviceProxy(Vector<::WebGPU::FeatureName>&&, const ::WebGPU::Limits&, RemoteAdapterProxy&, ConvertToBackingContext&, WebGPUIdentifier, WebGPUIdentifier queueIdentifier);

    RemoteDeviceProxy(const RemoteDeviceProxy&) = delete;
    RemoteDeviceProxy(RemoteDeviceProxy&&) = delete;
    RemoteDeviceProxy& operator=(const RemoteDeviceProxy&) = delete;
    RemoteDeviceProxy& operator=(RemoteDeviceProxy&&) = delete;

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }
    template<typename T, typename C>
    [[nodiscard]] std::optional<IPC::StreamClientConnection::AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return protect(root().streamClientConnection())->sendWithAsyncReply(std::forward<T>(message), std::forward<C>(completionHandler), backing());
    }

    WebGPUIdentifier m_backing;
    const Vector<::WebGPU::FeatureName> m_features;
    const ::WebGPU::Limits m_limits;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteAdapterProxy> m_parent;
    const Ref<RemoteQueueProxy> m_queue;
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    WebKit::SharedVideoFrameWriter m_sharedVideoFrameWriter;
#endif
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteDeviceProxy)
    // In the Web Process, every WebGPU::Device is a RemoteDeviceProxy.
    static bool isType(const ::WebGPU::Device&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
