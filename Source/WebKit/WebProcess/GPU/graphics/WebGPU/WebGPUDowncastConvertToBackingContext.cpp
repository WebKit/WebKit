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
#include "RemoteXRBindingProxy.h"
#include "RemoteXRProjectionLayerProxy.h"
#include "RemoteXRSubImageProxy.h"
#include "RemoteXRViewProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DowncastConvertToBackingContext);

WebGPUAdapterIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::Adapter& adapter)
{
    return downcast<RemoteAdapterProxy>(adapter).backing();
}

WebGPUBindGroupIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::BindGroup& bindGroup)
{
    return downcast<RemoteBindGroupProxy>(bindGroup).backing();
}

WebGPUBindGroupLayoutIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::BindGroupLayout& bindGroupLayout)
{
    return downcast<RemoteBindGroupLayoutProxy>(bindGroupLayout).backing();
}

WebGPUBufferIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::Buffer& buffer)
{
    return downcast<RemoteBufferProxy>(buffer).backing();
}

WebGPUCommandBufferIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::CommandBuffer& commandBuffer)
{
    return downcast<RemoteCommandBufferProxy>(commandBuffer).backing();
}

WebGPUCommandEncoderIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::CommandEncoder& commandEncoder)
{
    return downcast<RemoteCommandEncoderProxy>(commandEncoder).backing();
}

WebGPUCompositorIntegrationIdentifier DowncastConvertToBackingContext::convertToBacking(const WebCore::WebGPUCompositorIntegration& compositorIntegration)
{
    return downcast<RemoteCompositorIntegrationProxy>(compositorIntegration).backing();
}

WebGPUComputePassEncoderIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::ComputePassEncoder& computePassEncoder)
{
    return downcast<RemoteComputePassEncoderProxy>(computePassEncoder).backing();
}

WebGPUComputePipelineIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::ComputePipeline& computePipeline)
{
    return downcast<RemoteComputePipelineProxy>(computePipeline).backing();
}

WebGPUDeviceIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::Device& device)
{
    return downcast<RemoteDeviceProxy>(device).backing();
}

WebGPUExternalTextureIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::ExternalTexture& externalTexture)
{
    return downcast<RemoteExternalTextureProxy>(externalTexture).backing();
}

RemoteGPUIdentifier DowncastConvertToBackingContext::convertToBacking(const WebCore::WebGPUIntegration& gpu)
{
    return downcast<RemoteGPUProxy>(gpu).backing();
}

WebGPUPipelineLayoutIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::PipelineLayout& pipelineLayout)
{
    return downcast<RemotePipelineLayoutProxy>(pipelineLayout).backing();
}

WebGPUPresentationContextIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::PresentationContext& presentationContext)
{
    return downcast<RemotePresentationContextProxy>(presentationContext).backing();
}

WebGPUQuerySetIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::QuerySet& querySet)
{
    return downcast<RemoteQuerySetProxy>(querySet).backing();
}

WebGPUQueueIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::Queue& queue)
{
    return downcast<RemoteQueueProxy>(queue).backing();
}

WebGPURenderBundleEncoderIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::RenderBundleEncoder& renderBundleEncoder)
{
    return downcast<RemoteRenderBundleEncoderProxy>(renderBundleEncoder).backing();
}

WebGPURenderBundleIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::RenderBundle& renderBundle)
{
    return downcast<RemoteRenderBundleProxy>(renderBundle).backing();
}

WebGPURenderPassEncoderIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::RenderPassEncoder& renderPassEncoder)
{
    return downcast<RemoteRenderPassEncoderProxy>(renderPassEncoder).backing();
}

WebGPURenderPipelineIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::RenderPipeline& renderPipeline)
{
    return downcast<RemoteRenderPipelineProxy>(renderPipeline).backing();
}

WebGPUSamplerIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::Sampler& sampler)
{
    return downcast<RemoteSamplerProxy>(sampler).backing();
}

WebGPUShaderModuleIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::ShaderModule& shaderModule)
{
    return downcast<RemoteShaderModuleProxy>(shaderModule).backing();
}

WebGPUTextureIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::Texture& texture)
{
    return downcast<RemoteTextureProxy>(texture).backing();
}

WebGPUTextureViewIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::TextureView& textureView)
{
    return downcast<RemoteTextureViewProxy>(textureView).backing();
}

WebGPUXRBindingIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::XRBinding& xrBinding)
{
    return downcast<RemoteXRBindingProxy>(xrBinding).backing();
}

WebGPUXRProjectionLayerIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::XRProjectionLayer& layer)
{
    return downcast<RemoteXRProjectionLayerProxy>(layer).backing();
}

WebGPUXRSubImageIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::XRSubImage& subImage)
{
    return downcast<RemoteXRSubImageProxy>(subImage).backing();
}

WebGPUXRViewIdentifier DowncastConvertToBackingContext::convertToBacking(const ::WebGPU::XRView& view)
{
    return downcast<RemoteXRViewProxy>(view).backing();
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
