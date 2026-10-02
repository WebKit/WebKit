/*
 * Copyright (c) 2022-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import "Adapter.h"
#import "BindGroup.h"
#import "BindGroupLayout.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "CommandEncoder.h"
#import "ComputePassEncoder.h"
#import "ComputePipeline.h"
#import "Device.h"
#import "ExternalTexture.h"
#import "Instance.h"
#import "PipelineLayout.h"
#import "PresentationContext.h"
#import "QuerySet.h"
#import "Queue.h"
#import "RenderBundle.h"
#import "RenderBundleEncoder.h"
#import "RenderPassEncoder.h"
#import "RenderPipeline.h"
#import "Sampler.h"
#import "ShaderModule.h"
#import "Texture.h"
#import "TextureView.h"
#import "WebGPUCppConversions.h"
#import "XRBinding.h"
#import "XRProjectionLayer.h"
#import "XRSubImage.h"
#import "XRView.h"
#import <wtf/BlockPtr.h>
#import <wtf/SwiftBridging.h>
#import <wtf/TypeCasts.h>
#import <wtf/text/WTFString.h>

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Adapter)
    static bool isType(const WGPUAdapterImpl&) { return true; }
    static bool isType(const WebGPU::Adapter&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::BindGroup)
    static bool isType(const WGPUBindGroupImpl&) { return true; }
    static bool isType(const WebGPU::BindGroup&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::BindGroupLayout)
    static bool isType(const WGPUBindGroupLayoutImpl&) { return true; }
    static bool isType(const WebGPU::BindGroupLayout&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Buffer)
    static bool isType(const WGPUBufferImpl&) { return true; }
    static bool isType(const WebGPU::Buffer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::CommandBuffer)
    static bool isType(const WGPUCommandBufferImpl&) { return true; }
    static bool isType(const WebGPU::CommandBuffer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::CommandEncoder)
    static bool isType(const WGPUCommandEncoderImpl&) { return true; }
    static bool isType(const WebGPU::CommandEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ComputePassEncoder)
    static bool isType(const WGPUComputePassEncoderImpl&) { return true; }
    static bool isType(const WebGPU::ComputePassEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ComputePipeline)
    static bool isType(const WGPUComputePipelineImpl&) { return true; }
    static bool isType(const WebGPU::ComputePipeline&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Device)
    static bool isType(const WGPUDeviceImpl&) { return true; }
    static bool isType(const WebGPU::Device&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ExternalTexture)
    static bool isType(const WGPUExternalTextureImpl&) { return true; }
    static bool isType(const WebGPU::ExternalTexture&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Instance)
    static bool isType(const WGPUInstanceImpl&) { return true; }
    static bool isType(const WebGPU::Instance&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::PipelineLayout)
    static bool isType(const WGPUPipelineLayoutImpl&) { return true; }
    static bool isType(const WebGPU::PipelineLayout&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::QuerySet)
    static bool isType(const WGPUQuerySetImpl&) { return true; }
    static bool isType(const WebGPU::QuerySet&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Queue)
    static bool isType(const WGPUQueueImpl&) { return true; }
    static bool isType(const WebGPU::Queue&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderBundle)
    static bool isType(const WGPURenderBundleImpl&) { return true; }
    static bool isType(const WebGPU::RenderBundle&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderBundleEncoder)
    static bool isType(const WGPURenderBundleEncoderImpl&) { return true; }
    static bool isType(const WebGPU::RenderBundleEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderPassEncoder)
    static bool isType(const WGPURenderPassEncoderImpl&) { return true; }
    static bool isType(const WebGPU::RenderPassEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderPipeline)
    static bool isType(const WGPURenderPipelineImpl&) { return true; }
    static bool isType(const WebGPU::RenderPipeline&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Sampler)
    static bool isType(const WGPUSamplerImpl&) { return true; }
    static bool isType(const WebGPU::Sampler&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ShaderModule)
    static bool isType(const WGPUShaderModuleImpl&) { return true; }
    static bool isType(const WebGPU::ShaderModule&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::PresentationContext)
    static bool isType(const WGPUSurfaceImpl&) { return true; }
    static bool isType(const WGPUSwapChainImpl&) { return true; }
    static bool isType(const WebGPU::PresentationContext&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Texture)
    static bool isType(const WGPUTextureImpl&) { return true; }
    static bool isType(const WebGPU::Texture&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::TextureView)
    static bool isType(const WGPUTextureViewImpl&) { return true; }
    static bool isType(const WebGPU::TextureView&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRBinding)
    static bool isType(const WGPUXRBindingImpl&) { return true; }
    static bool isType(const WebGPU::XRBinding&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRSubImage)
    static bool isType(const WGPUXRSubImageImpl&) { return true; }
    static bool isType(const WebGPU::XRSubImage&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRProjectionLayer)
    static bool isType(const WGPUXRProjectionLayerImpl&) { return true; }
    static bool isType(const WebGPU::XRProjectionLayer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRView)
    static bool isType(const WGPUXRViewImpl&) { return true; }
    static bool isType(const WebGPU::XRView&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

namespace WebGPU::Metal {

// A process has only one implementation of the C++ API, so the C++ API objects that WebGPU::Metal
// receives are WebGPU::Metal objects.
inline BindGroupLayout& metal(WebGPU::BindGroupLayout& bindGroupLayout)
{
    return static_cast<BindGroupLayout&>(bindGroupLayout);
}

inline PipelineLayout& metal(WebGPU::PipelineLayout& pipelineLayout)
{
    return static_cast<PipelineLayout&>(pipelineLayout);
}

inline ShaderModule& metal(WebGPU::ShaderModule& shaderModule)
{
    return static_cast<ShaderModule&>(shaderModule);
}

inline Buffer& metal(WebGPU::Buffer& buffer)
{
    return static_cast<Buffer&>(buffer);
}

inline Device& metal(WebGPU::Device& device)
{
    return static_cast<Device&>(device);
}

inline ExternalTexture& metal(WebGPU::ExternalTexture& externalTexture)
{
    return static_cast<ExternalTexture&>(externalTexture);
}

inline QuerySet& metal(WebGPU::QuerySet& querySet)
{
    return static_cast<QuerySet&>(querySet);
}

inline RenderBundle& metal(WebGPU::RenderBundle& renderBundle)
{
    return static_cast<RenderBundle&>(renderBundle);
}

inline Sampler& metal(WebGPU::Sampler& sampler)
{
    return static_cast<Sampler&>(sampler);
}

inline Texture& metal(WebGPU::Texture& texture)
{
    return static_cast<Texture&>(texture);
}

inline TextureView& metal(WebGPU::TextureView& textureView)
{
    return static_cast<TextureView&>(textureView);
}

// For Swift, which cannot call the functions above: it does not see that the WebGPU::Metal classes
// derive from the C++ API classes.
inline Buffer& metal(const Ref<WebGPU::Buffer>& buffer)
{
    return metal(buffer.get());
}

inline QuerySet& metal(const Ref<WebGPU::QuerySet>& querySet)
{
    return metal(querySet.get());
}

inline QuerySet* metalOrNull(const RefPtr<WebGPU::QuerySet>& querySet)
{
    return querySet ? &metal(*querySet) : nullptr;
}

inline Texture& metal(const Ref<WebGPU::Texture>& texture)
{
    return metal(texture.get());
}

// FIXME: It would be cool if we didn't have to list all these overloads, but instead could do something like bridge_cast() in WTF.

inline Adapter& fromAPI(WGPUAdapter adapter)
{
    return downcast<Adapter>(*adapter);
}

inline BindGroup& fromAPI(WGPUBindGroup bindGroup)
{
    return downcast<BindGroup>(*bindGroup);
}

inline BindGroupLayout& fromAPI(WGPUBindGroupLayout bindGroupLayout)
{
    return downcast<BindGroupLayout>(*bindGroupLayout);
}

inline Buffer& fromAPI(WGPUBuffer buffer)
{
    return downcast<Buffer>(*buffer);
}

inline CommandBuffer& fromAPI(WGPUCommandBuffer commandBuffer)
{
    return downcast<CommandBuffer>(*commandBuffer);
}

inline CommandEncoder& fromAPI(WGPUCommandEncoder commandEncoder)
{
    return downcast<CommandEncoder>(*commandEncoder);
}

inline ComputePassEncoder& fromAPI(WGPUComputePassEncoder computePassEncoder)
{
    return downcast<ComputePassEncoder>(*computePassEncoder);
}

inline ComputePipeline& fromAPI(WGPUComputePipeline computePipeline)
{
    return downcast<ComputePipeline>(*computePipeline);
}

inline Device& fromAPI(WGPUDevice device)
{
    return downcast<Device>(*device);
}

inline ExternalTexture& fromAPI(WGPUExternalTexture texture)
{
    return downcast<ExternalTexture>(*texture);
}

inline Instance& fromAPI(WGPUInstance instance)
{
    return downcast<Instance>(*instance);
}

inline PipelineLayout& fromAPI(WGPUPipelineLayout pipelineLayout)
{
    return downcast<PipelineLayout>(*pipelineLayout);
}

inline QuerySet& fromAPI(WGPUQuerySet querySet)
{
    return downcast<QuerySet>(*querySet);
}

inline Queue& fromAPI(WGPUQueue queue)
{
    return downcast<Queue>(*queue);
}

inline RenderBundle& fromAPI(WGPURenderBundle renderBundle)
{
    return downcast<RenderBundle>(*renderBundle);
}

inline RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder renderBundleEncoder)
{
    return downcast<RenderBundleEncoder>(*renderBundleEncoder);
}

inline RenderPassEncoder& fromAPI(WGPURenderPassEncoder renderPassEncoder)
{
    return downcast<RenderPassEncoder>(*renderPassEncoder);
}

inline RenderPipeline& fromAPI(WGPURenderPipeline renderPipeline)
{
    return downcast<RenderPipeline>(*renderPipeline);
}

inline Sampler& fromAPI(WGPUSampler sampler)
{
    return downcast<Sampler>(*sampler);
}

inline ShaderModule& fromAPI(WGPUShaderModule shaderModule)
{
    return downcast<ShaderModule>(*shaderModule);
}

inline PresentationContext& fromAPI(WGPUSurface surface)
{
    return downcast<PresentationContext>(*surface);
}

inline PresentationContext& fromAPI(WGPUSwapChain swapChain)
{
    return downcast<PresentationContext>(*swapChain);
}

inline Texture& fromAPI(WGPUTexture texture)
{
    return downcast<Texture>(*texture);
}

inline TextureView& fromAPI(WGPUTextureView textureView)
{
    return downcast<TextureView>(*textureView);
}

inline XRBinding& fromAPI(WGPUXRBinding binding)
{
    return downcast<XRBinding>(*binding);
}

inline XRSubImage& fromAPI(WGPUXRSubImage subImage)
{
    return downcast<XRSubImage>(*subImage);
}

inline XRProjectionLayer& fromAPI(WGPUXRProjectionLayer layer)
{
    return downcast<XRProjectionLayer>(*layer);
}

inline XRView& fromAPI(WGPUXRView view)
{
    return downcast<XRView>(*view);
}

inline WGPUAdapter toAPI(WebGPU::Adapter& adapter)
{
    return &downcast<Adapter>(adapter);
}

inline WGPUBindGroup toAPI(WebGPU::BindGroup& bindGroup)
{
    return &downcast<BindGroup>(bindGroup);
}

inline WGPUBindGroupLayout toAPI(WebGPU::BindGroupLayout& bindGroupLayout)
{
    return &downcast<BindGroupLayout>(bindGroupLayout);
}

inline WGPUBuffer toAPI(WebGPU::Buffer& buffer)
{
    return &downcast<Buffer>(buffer);
}

inline WGPUCommandBuffer toAPI(WebGPU::CommandBuffer& commandBuffer)
{
    return &downcast<CommandBuffer>(commandBuffer);
}

inline WGPUCommandEncoder toAPI(WebGPU::CommandEncoder& commandEncoder)
{
    return &downcast<CommandEncoder>(commandEncoder);
}

inline WGPUComputePassEncoder toAPI(WebGPU::ComputePassEncoder& computePassEncoder)
{
    return &downcast<ComputePassEncoder>(computePassEncoder);
}

inline WGPUComputePipeline toAPI(WebGPU::ComputePipeline& computePipeline)
{
    return &downcast<ComputePipeline>(computePipeline);
}

inline WGPUDevice toAPI(WebGPU::Device& device)
{
    return &downcast<Device>(device);
}

inline WGPUExternalTexture toAPI(WebGPU::ExternalTexture& externalTexture)
{
    return &downcast<ExternalTexture>(externalTexture);
}

inline WGPUInstance toAPI(WebGPU::Instance& instance)
{
    return &downcast<Instance>(instance);
}

inline WGPUPipelineLayout toAPI(WebGPU::PipelineLayout& pipelineLayout)
{
    return &downcast<PipelineLayout>(pipelineLayout);
}

inline WGPUSurface toAPI(WebGPU::PresentationContext& presentationContext)
{
    return &downcast<PresentationContext>(presentationContext);
}

inline WGPUQuerySet toAPI(WebGPU::QuerySet& querySet)
{
    return &downcast<QuerySet>(querySet);
}

inline WGPUQueue toAPI(WebGPU::Queue& queue)
{
    return &downcast<Queue>(queue);
}

inline WGPURenderBundle toAPI(WebGPU::RenderBundle& renderBundle)
{
    return &downcast<RenderBundle>(renderBundle);
}

inline WGPURenderBundleEncoder toAPI(WebGPU::RenderBundleEncoder& renderBundleEncoder)
{
    return &downcast<RenderBundleEncoder>(renderBundleEncoder);
}

inline WGPURenderPassEncoder toAPI(WebGPU::RenderPassEncoder& renderPassEncoder)
{
    return &downcast<RenderPassEncoder>(renderPassEncoder);
}

inline WGPURenderPipeline toAPI(WebGPU::RenderPipeline& renderPipeline)
{
    return &downcast<RenderPipeline>(renderPipeline);
}

inline WGPUSampler toAPI(WebGPU::Sampler& sampler)
{
    return &downcast<Sampler>(sampler);
}

inline WGPUShaderModule toAPI(WebGPU::ShaderModule& shaderModule)
{
    return &downcast<ShaderModule>(shaderModule);
}

inline WGPUTexture toAPI(WebGPU::Texture& texture)
{
    return &downcast<Texture>(texture);
}

inline WGPUTextureView toAPI(WebGPU::TextureView& textureView)
{
    return &downcast<TextureView>(textureView);
}

inline WGPUXRBinding toAPI(WebGPU::XRBinding& xRBinding)
{
    return &downcast<XRBinding>(xRBinding);
}

inline WGPUXRProjectionLayer toAPI(WebGPU::XRProjectionLayer& xRProjectionLayer)
{
    return &downcast<XRProjectionLayer>(xRProjectionLayer);
}

inline WGPUXRSubImage toAPI(WebGPU::XRSubImage& xRSubImage)
{
    return &downcast<XRSubImage>(xRSubImage);
}

inline WGPUXRView toAPI(WebGPU::XRView& xRView)
{
    return &downcast<XRView>(xRView);
}

inline WGPUSwapChain toAPISwapChain(WebGPU::PresentationContext& presentationContext)
{
    return &downcast<PresentationContext>(presentationContext);
}

// Literals have static storage, so the view can borrow them freely.
inline WGPUStringView toAPI(ASCIILiteral literal)
{
    return { literal.characters(), literal.length() };
}

template<typename R, typename... Args>
inline BlockPtr<R (Args...)> fromAPI(R (^ __strong &&block)(Args...))
{
    return makeBlockPtr(WTF::move(block));
}

template <typename T>
inline T* releaseToAPI(Ref<T>&& pointer)
{
    return &pointer.leakRef();
}

template <typename T>
inline T* releaseToAPI(RefPtr<T>&& pointer)
{
    // FIXME: We shouldn't need this, because invalid objects should be created instead of returning nullptr.
    if (pointer)
        return pointer.leakRef();
    return nullptr;
}

} // namespace WebGPU::Metal
