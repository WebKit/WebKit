/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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
#include "RemoteBindGroupLayoutProxy.h"
#include "RemoteBindGroupProxy.h"
#include "RemoteBufferProxy.h"
#include "RemoteCommandBufferProxy.h"
#include "RemoteCommandEncoderProxy.h"
#include "RemoteCompositorIntegrationProxy.h"
#include "RemoteComputePassEncoderProxy.h"
#include "RemoteComputePipelineProxy.h"
#include "RemoteDeviceProxy.h"
#include "RemoteExternalTextureProxy.h"
#include "RemoteGPUProxy.h"
#include "RemotePipelineLayoutProxy.h"
#include "RemotePresentationContextProxy.h"
#include "RemoteQuerySetProxy.h"
#include "RemoteQueueProxy.h"
#include "RemoteRenderBundleEncoderProxy.h"
#include "RemoteRenderBundleProxy.h"
#include "RemoteRenderPassEncoderProxy.h"
#include "RemoteRenderPipelineProxy.h"
#include "RemoteSamplerProxy.h"
#include "RemoteShaderModuleProxy.h"
#include "RemoteTextureProxy.h"
#include "RemoteTextureViewProxy.h"
#include "RemoteXRBindingProxy.h"
#include "RemoteXRProjectionLayerProxy.h"
#include "RemoteXRSubImageProxy.h"
#include "RemoteXRViewProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>

namespace WebKit::WebGPU {

// In the Web Process, every WebGPU object is a Remote*Proxy, whose backing is its identifier in the GPU Process.

inline WebGPUAdapterIdentifier identifier(const ::WebGPU::Adapter& adapter)
{
    return downcast<RemoteAdapterProxy>(adapter).backing();
}

inline WebGPUBindGroupIdentifier identifier(const ::WebGPU::BindGroup& bindGroup)
{
    return downcast<RemoteBindGroupProxy>(bindGroup).backing();
}

inline WebGPUBindGroupLayoutIdentifier identifier(const ::WebGPU::BindGroupLayout& bindGroupLayout)
{
    return downcast<RemoteBindGroupLayoutProxy>(bindGroupLayout).backing();
}

inline WebGPUBufferIdentifier identifier(const ::WebGPU::Buffer& buffer)
{
    return downcast<RemoteBufferProxy>(buffer).backing();
}

inline WebGPUCommandBufferIdentifier identifier(const ::WebGPU::CommandBuffer& commandBuffer)
{
    return downcast<RemoteCommandBufferProxy>(commandBuffer).backing();
}

inline WebGPUCommandEncoderIdentifier identifier(const ::WebGPU::CommandEncoder& commandEncoder)
{
    return downcast<RemoteCommandEncoderProxy>(commandEncoder).backing();
}

inline WebGPUCompositorIntegrationIdentifier identifier(const WebCore::WebGPUCompositorIntegration& compositorIntegration)
{
    return downcast<RemoteCompositorIntegrationProxy>(compositorIntegration).backing();
}

inline WebGPUComputePassEncoderIdentifier identifier(const ::WebGPU::ComputePassEncoder& computePassEncoder)
{
    return downcast<RemoteComputePassEncoderProxy>(computePassEncoder).backing();
}

inline WebGPUComputePipelineIdentifier identifier(const ::WebGPU::ComputePipeline& computePipeline)
{
    return downcast<RemoteComputePipelineProxy>(computePipeline).backing();
}

inline WebGPUDeviceIdentifier identifier(const ::WebGPU::Device& device)
{
    return downcast<RemoteDeviceProxy>(device).backing();
}

inline WebGPUExternalTextureIdentifier identifier(const ::WebGPU::ExternalTexture& externalTexture)
{
    return downcast<RemoteExternalTextureProxy>(externalTexture).backing();
}

inline RemoteGPUIdentifier identifier(const WebCore::WebGPUIntegration& gpu)
{
    return downcast<RemoteGPUProxy>(gpu).backing();
}

inline WebGPUPipelineLayoutIdentifier identifier(const ::WebGPU::PipelineLayout& pipelineLayout)
{
    return downcast<RemotePipelineLayoutProxy>(pipelineLayout).backing();
}

inline WebGPUPresentationContextIdentifier identifier(const ::WebGPU::PresentationContext& presentationContext)
{
    return downcast<RemotePresentationContextProxy>(presentationContext).backing();
}

inline WebGPUQuerySetIdentifier identifier(const ::WebGPU::QuerySet& querySet)
{
    return downcast<RemoteQuerySetProxy>(querySet).backing();
}

inline WebGPUQueueIdentifier identifier(const ::WebGPU::Queue& queue)
{
    return downcast<RemoteQueueProxy>(queue).backing();
}

inline WebGPURenderBundleEncoderIdentifier identifier(const ::WebGPU::RenderBundleEncoder& renderBundleEncoder)
{
    return downcast<RemoteRenderBundleEncoderProxy>(renderBundleEncoder).backing();
}

inline WebGPURenderBundleIdentifier identifier(const ::WebGPU::RenderBundle& renderBundle)
{
    return downcast<RemoteRenderBundleProxy>(renderBundle).backing();
}

inline WebGPURenderPassEncoderIdentifier identifier(const ::WebGPU::RenderPassEncoder& renderPassEncoder)
{
    return downcast<RemoteRenderPassEncoderProxy>(renderPassEncoder).backing();
}

inline WebGPURenderPipelineIdentifier identifier(const ::WebGPU::RenderPipeline& renderPipeline)
{
    return downcast<RemoteRenderPipelineProxy>(renderPipeline).backing();
}

inline WebGPUSamplerIdentifier identifier(const ::WebGPU::Sampler& sampler)
{
    return downcast<RemoteSamplerProxy>(sampler).backing();
}

inline WebGPUShaderModuleIdentifier identifier(const ::WebGPU::ShaderModule& shaderModule)
{
    return downcast<RemoteShaderModuleProxy>(shaderModule).backing();
}

inline WebGPUTextureIdentifier identifier(const ::WebGPU::Texture& texture)
{
    return downcast<RemoteTextureProxy>(texture).backing();
}

inline WebGPUTextureViewIdentifier identifier(const ::WebGPU::TextureView& textureView)
{
    return downcast<RemoteTextureViewProxy>(textureView).backing();
}

inline WebGPUXRBindingIdentifier identifier(const ::WebGPU::XRBinding& xrBinding)
{
    return downcast<RemoteXRBindingProxy>(xrBinding).backing();
}

inline WebGPUXRProjectionLayerIdentifier identifier(const ::WebGPU::XRProjectionLayer& layer)
{
    return downcast<RemoteXRProjectionLayerProxy>(layer).backing();
}

inline WebGPUXRSubImageIdentifier identifier(const ::WebGPU::XRSubImage& subImage)
{
    return downcast<RemoteXRSubImageProxy>(subImage).backing();
}

inline WebGPUXRViewIdentifier identifier(const ::WebGPU::XRView& view)
{
    return downcast<RemoteXRViewProxy>(view).backing();
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
