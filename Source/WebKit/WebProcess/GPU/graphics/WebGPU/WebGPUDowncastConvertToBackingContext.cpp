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
#include "WebGPUDowncastConvertToBackingContext.h"

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

namespace WebKit::WebGPU {

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::Adapter& adapter)
{
    return static_cast<const RemoteAdapterProxy&>(adapter).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::BindGroup& bindGroup)
{
    return static_cast<const RemoteBindGroupProxy&>(bindGroup).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::BindGroupLayout& bindGroupLayout)
{
    return static_cast<const RemoteBindGroupLayoutProxy&>(bindGroupLayout).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::Buffer& buffer)
{
    return static_cast<const RemoteBufferProxy&>(buffer).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::CommandBuffer& commandBuffer)
{
    return static_cast<const RemoteCommandBufferProxy&>(commandBuffer).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::CommandEncoder& commandEncoder)
{
    return static_cast<const RemoteCommandEncoderProxy&>(commandEncoder).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::CompositorIntegration& compositorIntegration)
{
    return static_cast<const RemoteCompositorIntegrationProxy&>(compositorIntegration).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::ComputePassEncoder& computePassEncoder)
{
    return static_cast<const RemoteComputePassEncoderProxy&>(computePassEncoder).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::ComputePipeline& computePipeline)
{
    return static_cast<const RemoteComputePipelineProxy&>(computePipeline).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::Device& device)
{
    return static_cast<const RemoteDeviceProxy&>(device).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::ExternalTexture& externalTexture)
{
    return static_cast<const RemoteExternalTextureProxy&>(externalTexture).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::GPU& gpu)
{
    return static_cast<const RemoteGPUProxy&>(gpu).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::PipelineLayout& pipelineLayout)
{
    return static_cast<const RemotePipelineLayoutProxy&>(pipelineLayout).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::PresentationContext& presentationContext)
{
    return static_cast<const RemotePresentationContextProxy&>(presentationContext).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::QuerySet& querySet)
{
    return static_cast<const RemoteQuerySetProxy&>(querySet).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::Queue& queue)
{
    return static_cast<const RemoteQueueProxy&>(queue).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::RenderBundleEncoder& renderBundleEncoder)
{
    return static_cast<const RemoteRenderBundleEncoderProxy&>(renderBundleEncoder).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::RenderBundle& renderBundle)
{
    return static_cast<const RemoteRenderBundleProxy&>(renderBundle).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::RenderPassEncoder& renderPassEncoder)
{
    return static_cast<const RemoteRenderPassEncoderProxy&>(renderPassEncoder).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::RenderPipeline& renderPipeline)
{
    return static_cast<const RemoteRenderPipelineProxy&>(renderPipeline).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::Sampler& sampler)
{
    return static_cast<const RemoteSamplerProxy&>(sampler).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::ShaderModule& shaderModule)
{
    return static_cast<const RemoteShaderModuleProxy&>(shaderModule).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::Texture& texture)
{
    return static_cast<const RemoteTextureProxy&>(texture).backing();
}

WebGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const PAL::WebGPU::TextureView& textureView)
{
    return static_cast<const RemoteTextureViewProxy&>(textureView).backing();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
