/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import "APIConversions.h"
#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>

// The conversions between the handles of the C API and the objects behind them. The Metal
// implementation itself never includes the headers of the C API.

namespace WebGPU::CAPI {

// The handles of the C API are the objects of the C++ API, so these conversions do not depend on
// the implementation that created the objects. A WGPUInstance is the instance of the C++ API too.

inline WebGPU::Adapter& fromAPI(WGPUAdapter handle)
{
    return *reinterpret_cast<WebGPU::Adapter*>(handle);
}

inline WGPUAdapter toAPI(WebGPU::Adapter& object)
{
    return reinterpret_cast<WGPUAdapter>(&object);
}

inline WebGPU::BindGroup& fromAPI(WGPUBindGroup handle)
{
    return *reinterpret_cast<WebGPU::BindGroup*>(handle);
}

inline WGPUBindGroup toAPI(WebGPU::BindGroup& object)
{
    return reinterpret_cast<WGPUBindGroup>(&object);
}

inline WebGPU::BindGroupLayout& fromAPI(WGPUBindGroupLayout handle)
{
    return *reinterpret_cast<WebGPU::BindGroupLayout*>(handle);
}

inline WGPUBindGroupLayout toAPI(WebGPU::BindGroupLayout& object)
{
    return reinterpret_cast<WGPUBindGroupLayout>(&object);
}

inline WebGPU::Buffer& fromAPI(WGPUBuffer handle)
{
    return *reinterpret_cast<WebGPU::Buffer*>(handle);
}

inline WGPUBuffer toAPI(WebGPU::Buffer& object)
{
    return reinterpret_cast<WGPUBuffer>(&object);
}

inline WebGPU::CommandBuffer& fromAPI(WGPUCommandBuffer handle)
{
    return *reinterpret_cast<WebGPU::CommandBuffer*>(handle);
}

inline WGPUCommandBuffer toAPI(WebGPU::CommandBuffer& object)
{
    return reinterpret_cast<WGPUCommandBuffer>(&object);
}

inline WebGPU::CommandEncoder& fromAPI(WGPUCommandEncoder handle)
{
    return *reinterpret_cast<WebGPU::CommandEncoder*>(handle);
}

inline WGPUCommandEncoder toAPI(WebGPU::CommandEncoder& object)
{
    return reinterpret_cast<WGPUCommandEncoder>(&object);
}

inline WebGPU::ComputePassEncoder& fromAPI(WGPUComputePassEncoder handle)
{
    return *reinterpret_cast<WebGPU::ComputePassEncoder*>(handle);
}

inline WGPUComputePassEncoder toAPI(WebGPU::ComputePassEncoder& object)
{
    return reinterpret_cast<WGPUComputePassEncoder>(&object);
}

inline WebGPU::ComputePipeline& fromAPI(WGPUComputePipeline handle)
{
    return *reinterpret_cast<WebGPU::ComputePipeline*>(handle);
}

inline WGPUComputePipeline toAPI(WebGPU::ComputePipeline& object)
{
    return reinterpret_cast<WGPUComputePipeline>(&object);
}

inline WebGPU::Device& fromAPI(WGPUDevice handle)
{
    return *reinterpret_cast<WebGPU::Device*>(handle);
}

inline WGPUDevice toAPI(WebGPU::Device& object)
{
    return reinterpret_cast<WGPUDevice>(&object);
}

inline WebGPU::ExternalTexture& fromAPI(WGPUExternalTexture handle)
{
    return *reinterpret_cast<WebGPU::ExternalTexture*>(handle);
}

inline WGPUExternalTexture toAPI(WebGPU::ExternalTexture& object)
{
    return reinterpret_cast<WGPUExternalTexture>(&object);
}

inline WebGPU::Instance& fromAPI(WGPUInstance handle)
{
    return *reinterpret_cast<WebGPU::Instance*>(handle);
}

inline WGPUInstance toAPI(WebGPU::Instance& object)
{
    return reinterpret_cast<WGPUInstance>(&object);
}

inline WebGPU::PipelineLayout& fromAPI(WGPUPipelineLayout handle)
{
    return *reinterpret_cast<WebGPU::PipelineLayout*>(handle);
}

inline WGPUPipelineLayout toAPI(WebGPU::PipelineLayout& object)
{
    return reinterpret_cast<WGPUPipelineLayout>(&object);
}

inline WebGPU::PresentationContext& fromAPI(WGPUSurface handle)
{
    return *reinterpret_cast<WebGPU::PresentationContext*>(handle);
}

inline WGPUSurface toAPI(WebGPU::PresentationContext& object)
{
    return reinterpret_cast<WGPUSurface>(&object);
}

inline WebGPU::QuerySet& fromAPI(WGPUQuerySet handle)
{
    return *reinterpret_cast<WebGPU::QuerySet*>(handle);
}

inline WGPUQuerySet toAPI(WebGPU::QuerySet& object)
{
    return reinterpret_cast<WGPUQuerySet>(&object);
}

inline WebGPU::Queue& fromAPI(WGPUQueue handle)
{
    return *reinterpret_cast<WebGPU::Queue*>(handle);
}

inline WGPUQueue toAPI(WebGPU::Queue& object)
{
    return reinterpret_cast<WGPUQueue>(&object);
}

inline WebGPU::RenderBundle& fromAPI(WGPURenderBundle handle)
{
    return *reinterpret_cast<WebGPU::RenderBundle*>(handle);
}

inline WGPURenderBundle toAPI(WebGPU::RenderBundle& object)
{
    return reinterpret_cast<WGPURenderBundle>(&object);
}

inline WebGPU::RenderBundleEncoder& fromAPI(WGPURenderBundleEncoder handle)
{
    return *reinterpret_cast<WebGPU::RenderBundleEncoder*>(handle);
}

inline WGPURenderBundleEncoder toAPI(WebGPU::RenderBundleEncoder& object)
{
    return reinterpret_cast<WGPURenderBundleEncoder>(&object);
}

inline WebGPU::RenderPassEncoder& fromAPI(WGPURenderPassEncoder handle)
{
    return *reinterpret_cast<WebGPU::RenderPassEncoder*>(handle);
}

inline WGPURenderPassEncoder toAPI(WebGPU::RenderPassEncoder& object)
{
    return reinterpret_cast<WGPURenderPassEncoder>(&object);
}

inline WebGPU::RenderPipeline& fromAPI(WGPURenderPipeline handle)
{
    return *reinterpret_cast<WebGPU::RenderPipeline*>(handle);
}

inline WGPURenderPipeline toAPI(WebGPU::RenderPipeline& object)
{
    return reinterpret_cast<WGPURenderPipeline>(&object);
}

inline WebGPU::Sampler& fromAPI(WGPUSampler handle)
{
    return *reinterpret_cast<WebGPU::Sampler*>(handle);
}

inline WGPUSampler toAPI(WebGPU::Sampler& object)
{
    return reinterpret_cast<WGPUSampler>(&object);
}

inline WebGPU::ShaderModule& fromAPI(WGPUShaderModule handle)
{
    return *reinterpret_cast<WebGPU::ShaderModule*>(handle);
}

inline WGPUShaderModule toAPI(WebGPU::ShaderModule& object)
{
    return reinterpret_cast<WGPUShaderModule>(&object);
}

inline WebGPU::Texture& fromAPI(WGPUTexture handle)
{
    return *reinterpret_cast<WebGPU::Texture*>(handle);
}

inline WGPUTexture toAPI(WebGPU::Texture& object)
{
    return reinterpret_cast<WGPUTexture>(&object);
}

inline WebGPU::TextureView& fromAPI(WGPUTextureView handle)
{
    return *reinterpret_cast<WebGPU::TextureView*>(handle);
}

inline WGPUTextureView toAPI(WebGPU::TextureView& object)
{
    return reinterpret_cast<WGPUTextureView>(&object);
}

// The handle of a new reference, for the C API. The handle of an object is its WebGPU::X.
template <typename T>
inline auto releaseToAPI(Ref<T>&& pointer)
{
    return toAPI(pointer.leakRef());
}

template <typename T>
inline auto releaseToAPI(RefPtr<T>&& pointer) -> decltype(toAPI(*pointer))
{
    // FIXME: We shouldn't need this, because invalid objects should be created instead of returning nullptr.
    if (pointer)
        return toAPI(*pointer.leakRef());
    return nullptr;
}

// The WebGPU::Metal objects of the handles, for the C API functions that use what only the Metal
// implementation has. In WebGPU.framework every C++ API object is a WebGPU::Metal object.

inline Metal::Adapter& metalFromAPI(WGPUAdapter handle)
{
    return downcast<Metal::Adapter>(fromAPI(handle));
}

inline Metal::BindGroup& metalFromAPI(WGPUBindGroup handle)
{
    return downcast<Metal::BindGroup>(fromAPI(handle));
}

inline Metal::BindGroupLayout& metalFromAPI(WGPUBindGroupLayout handle)
{
    return downcast<Metal::BindGroupLayout>(fromAPI(handle));
}

inline Metal::Buffer& metalFromAPI(WGPUBuffer handle)
{
    return downcast<Metal::Buffer>(fromAPI(handle));
}

inline Metal::CommandBuffer& metalFromAPI(WGPUCommandBuffer handle)
{
    return downcast<Metal::CommandBuffer>(fromAPI(handle));
}

inline Metal::CommandEncoder& metalFromAPI(WGPUCommandEncoder handle)
{
    return downcast<Metal::CommandEncoder>(fromAPI(handle));
}

inline Metal::ComputePassEncoder& metalFromAPI(WGPUComputePassEncoder handle)
{
    return downcast<Metal::ComputePassEncoder>(fromAPI(handle));
}

inline Metal::ComputePipeline& metalFromAPI(WGPUComputePipeline handle)
{
    return downcast<Metal::ComputePipeline>(fromAPI(handle));
}

inline Metal::Device& metalFromAPI(WGPUDevice handle)
{
    return downcast<Metal::Device>(fromAPI(handle));
}

inline Metal::ExternalTexture& metalFromAPI(WGPUExternalTexture handle)
{
    return downcast<Metal::ExternalTexture>(fromAPI(handle));
}

inline Metal::Instance& metalFromAPI(WGPUInstance handle)
{
    return downcast<Metal::Instance>(fromAPI(handle));
}

inline Metal::PipelineLayout& metalFromAPI(WGPUPipelineLayout handle)
{
    return downcast<Metal::PipelineLayout>(fromAPI(handle));
}

inline Metal::PresentationContext& metalFromAPI(WGPUSurface handle)
{
    return downcast<Metal::PresentationContext>(fromAPI(handle));
}

inline Metal::QuerySet& metalFromAPI(WGPUQuerySet handle)
{
    return downcast<Metal::QuerySet>(fromAPI(handle));
}

inline Metal::Queue& metalFromAPI(WGPUQueue handle)
{
    return downcast<Metal::Queue>(fromAPI(handle));
}

inline Metal::RenderBundle& metalFromAPI(WGPURenderBundle handle)
{
    return downcast<Metal::RenderBundle>(fromAPI(handle));
}

inline Metal::RenderBundleEncoder& metalFromAPI(WGPURenderBundleEncoder handle)
{
    return downcast<Metal::RenderBundleEncoder>(fromAPI(handle));
}

inline Metal::RenderPassEncoder& metalFromAPI(WGPURenderPassEncoder handle)
{
    return downcast<Metal::RenderPassEncoder>(fromAPI(handle));
}

inline Metal::RenderPipeline& metalFromAPI(WGPURenderPipeline handle)
{
    return downcast<Metal::RenderPipeline>(fromAPI(handle));
}

inline Metal::Sampler& metalFromAPI(WGPUSampler handle)
{
    return downcast<Metal::Sampler>(fromAPI(handle));
}

inline Metal::ShaderModule& metalFromAPI(WGPUShaderModule handle)
{
    return downcast<Metal::ShaderModule>(fromAPI(handle));
}

inline Metal::Texture& metalFromAPI(WGPUTexture handle)
{
    return downcast<Metal::Texture>(fromAPI(handle));
}

inline Metal::TextureView& metalFromAPI(WGPUTextureView handle)
{
    return downcast<Metal::TextureView>(fromAPI(handle));
}

// For the WebGPU::X objects that a WebGPU::Metal object creates, which are WebGPU::Metal::X objects.
template <typename T, typename U>
inline auto releaseToAPIAs(RefPtr<U>&& pointer)
{
    return releaseToAPI(WTF::move(pointer));
}

template <typename T, typename U>
inline auto releaseToAPIAs(Ref<U>&& pointer)
{
    return releaseToAPI(WTF::move(pointer));
}

// A future of the C API on an instance. complete() is called after the callback it stands for
// has run. Without an instance, the future is WGPU_FUTURE_INIT, which nothing waits on.
class Future {
public:
    explicit Future(RefPtr<Metal::Instance>&& instance)
        : m_instance(WTF::move(instance))
        , m_id(m_instance ? m_instance->createFuture() : 0)
    {
    }

    WGPUFuture future() const { return { m_id }; }
    void complete() const
    {
        if (m_instance)
            m_instance->completeFuture(m_id);
    }

private:
    const RefPtr<Metal::Instance> m_instance;
    const uint64_t m_id { 0 };
};

} // namespace WebGPU::CAPI
