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

#if !__has_feature(objc_arc)
#error This header requires ARC. Include it only from files compiled with -fobjc-arc.
#endif

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
#import "XRBinding.h"
#import "XRProjectionLayer.h"
#import "XRSubImage.h"
#import "XRView.h"
#import <algorithm>
#import <wtf/BlockPtr.h>
#import <wtf/SwiftBridging.h>
#import <wtf/TypeCasts.h>
#import <wtf/text/WTFString.h>

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Adapter)
    static bool isType(const WebGPU::Adapter&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::BindGroup)
    static bool isType(const WebGPU::BindGroup&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::BindGroupLayout)
    static bool isType(const WebGPU::BindGroupLayout&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Buffer)
    static bool isType(const WebGPU::Buffer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::CommandBuffer)
    static bool isType(const WebGPU::CommandBuffer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::CommandEncoder)
    static bool isType(const WebGPU::CommandEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ComputePassEncoder)
    static bool isType(const WebGPU::ComputePassEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ComputePipeline)
    static bool isType(const WebGPU::ComputePipeline&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Device)
    static bool isType(const WebGPU::Device&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ExternalTexture)
    static bool isType(const WebGPU::ExternalTexture&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Instance)
    static bool isType(const WebGPU::Instance&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::PipelineLayout)
    static bool isType(const WebGPU::PipelineLayout&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::QuerySet)
    static bool isType(const WebGPU::QuerySet&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Queue)
    static bool isType(const WebGPU::Queue&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderBundle)
    static bool isType(const WebGPU::RenderBundle&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderBundleEncoder)
    static bool isType(const WebGPU::RenderBundleEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderPassEncoder)
    static bool isType(const WebGPU::RenderPassEncoder&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::RenderPipeline)
    static bool isType(const WebGPU::RenderPipeline&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Sampler)
    static bool isType(const WebGPU::Sampler&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::ShaderModule)
    static bool isType(const WebGPU::ShaderModule&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::PresentationContext)
    static bool isType(const WebGPU::PresentationContext&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRBinding)
    static bool isType(const WebGPU::XRBinding&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRSubImage)
    static bool isType(const WebGPU::XRSubImage&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRProjectionLayer)
    static bool isType(const WebGPU::XRProjectionLayer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::XRView)
    static bool isType(const WebGPU::XRView&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

namespace WebGPU::Metal {

// A process has only one implementation of the C++ API, so the C++ API objects that WebGPU::Metal
// receives are WebGPU::Metal objects.
inline BindGroupLayout& metal(WebGPU::BindGroupLayout& bindGroupLayout)
{
    return downcast<BindGroupLayout>(bindGroupLayout);
}

inline PipelineLayout& metal(WebGPU::PipelineLayout& pipelineLayout)
{
    return downcast<PipelineLayout>(pipelineLayout);
}

inline ShaderModule& metal(WebGPU::ShaderModule& shaderModule)
{
    return downcast<ShaderModule>(shaderModule);
}

inline Buffer& metal(WebGPU::Buffer& buffer)
{
    return downcast<Buffer>(buffer);
}

inline Device& metal(WebGPU::Device& device)
{
    return downcast<Device>(device);
}

inline ExternalTexture& metal(WebGPU::ExternalTexture& externalTexture)
{
    return downcast<ExternalTexture>(externalTexture);
}

inline QuerySet& metal(WebGPU::QuerySet& querySet)
{
    return downcast<QuerySet>(querySet);
}

inline RenderBundle& metal(WebGPU::RenderBundle& renderBundle)
{
    return downcast<RenderBundle>(renderBundle);
}

inline Sampler& metal(WebGPU::Sampler& sampler)
{
    return downcast<Sampler>(sampler);
}

inline Texture& metal(WebGPU::Texture& texture)
{
    return downcast<Texture>(texture);
}

inline TextureView& metal(WebGPU::TextureView& textureView)
{
    return downcast<TextureView>(textureView);
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

template<typename R, typename... Args>
inline BlockPtr<R (Args...)> fromAPI(R (^ __strong &&block)(Args...))
{
    return makeBlockPtr(WTF::move(block));
}

} // namespace WebGPU::Metal
