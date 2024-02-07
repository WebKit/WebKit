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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUDevice.h"
#include "WebGPUPtr.h"
#include "WebGPUQueueImpl.h"
#include <WebGPU/WebGPU.h>
#include <wtf/Deque.h>

namespace WebCore::WebGPU {

class ConvertToBackingContext;
enum class DeviceLostReason : uint8_t;

class DeviceImpl final : public Device {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<DeviceImpl> create(WebGPUPtr<WGPUDevice>&& device, Ref<SupportedFeatures>&& features, Ref<SupportedLimits>&& limits, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new DeviceImpl(WTFMove(device), WTFMove(features), WTFMove(limits), convertToBackingContext));
    }

    virtual ~DeviceImpl();
    void setLastUncapturedError(WGPUErrorType, char const*);

private:
    friend class DowncastConvertToBackingContext;

    DeviceImpl(WebGPUPtr<WGPUDevice>&&, Ref<SupportedFeatures>&&, Ref<SupportedLimits>&&, ConvertToBackingContext&);

    DeviceImpl(const DeviceImpl&) = delete;
    DeviceImpl(DeviceImpl&&) = delete;
    DeviceImpl& operator=(const DeviceImpl&) = delete;
    DeviceImpl& operator=(DeviceImpl&&) = delete;

    WGPUDevice backing() const { return m_backing.get(); }

    Ref<Queue> queue() final;

    void destroy() final;

    Ref<Buffer> createBuffer(const BufferDescriptor&) final;
    Ref<Texture> createTexture(const TextureDescriptor&) final;
    Ref<Sampler> createSampler(const SamplerDescriptor&) final;
    Ref<ExternalTexture> importExternalTexture(const ExternalTextureDescriptor&) final;

    Ref<BindGroupLayout> createBindGroupLayout(const BindGroupLayoutDescriptor&) final;
    Ref<PipelineLayout> createPipelineLayout(const PipelineLayoutDescriptor&) final;
    Ref<BindGroup> createBindGroup(const BindGroupDescriptor&) final;

    Ref<ShaderModule> createShaderModule(const ShaderModuleDescriptor&) final;
    Ref<ComputePipeline> createComputePipeline(const ComputePipelineDescriptor&) final;
    Ref<RenderPipeline> createRenderPipeline(const RenderPipelineDescriptor&) final;
    void createComputePipelineAsync(const ComputePipelineDescriptor&, CompletionHandler<void(RefPtr<ComputePipeline>&&)>&&) final;
    void createRenderPipelineAsync(const RenderPipelineDescriptor&, CompletionHandler<void(RefPtr<RenderPipeline>&&)>&&) final;

    Ref<CommandEncoder> createCommandEncoder(const std::optional<CommandEncoderDescriptor>&) final;
    Ref<RenderBundleEncoder> createRenderBundleEncoder(const RenderBundleEncoderDescriptor&) final;

    Ref<QuerySet> createQuerySet(const QuerySetDescriptor&) final;

    void pushErrorScope(ErrorFilter) final;
    void popErrorScope(CompletionHandler<void(bool, std::optional<Error>&&)>&&) final;
    void resolveUncapturedErrorEvent(CompletionHandler<void(bool, std::optional<WebCore::WebGPU::Error>&&)>&&) final;
    void resolveDeviceLostPromise(CompletionHandler<void(WebCore::WebGPU::DeviceLostReason)>&&) final;

    void setLabelInternal(const String&) final;

    WebGPUPtr<WGPUDevice> m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<QueueImpl> m_queue;
};

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
