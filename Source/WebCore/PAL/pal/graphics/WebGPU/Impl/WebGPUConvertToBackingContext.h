/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "WebGPUBufferUsage.h"
#include "WebGPUColor.h"
#include "WebGPUColorWrite.h"
#include "WebGPUExtent3D.h"
#include "WebGPUMapMode.h"
#include "WebGPUOrigin2D.h"
#include "WebGPUOrigin3D.h"
#include "WebGPUShaderStage.h"
#include "WebGPUTextureUsage.h"
#include <WebGPU/WebGPU.h>
#include <cstdint>
#include <wtf/RefCounted.h>

namespace PAL::WebGPU {

class Adapter;
enum class AddressMode : uint8_t;
class BindGroup;
class BindGroupLayout;
enum class BlendFactor : uint8_t;
enum class BlendOperation : uint8_t;
class Buffer;
enum class BufferBindingType : uint8_t;
class CommandBuffer;
class CommandEncoder;
enum class CompareFunction : uint8_t;
enum class CompilationMessageType : uint8_t;
enum class ComputePassTimestampLocation : uint8_t;
class ComputePassEncoder;
class ComputePipeline;
enum class CullMode : uint8_t;
class Device;
enum class ErrorFilter : uint8_t;
enum class FeatureName : uint8_t;
enum class FilterMode : uint8_t;
enum class FrontFace : uint8_t;
class GPU;
enum class IndexFormat : uint8_t;
enum class LoadOp : uint8_t;
class PipelineLayout;
enum class PowerPreference : uint8_t;
enum class PrimitiveTopology : uint8_t;
class QuerySet;
enum class QueryType : uint8_t;
class Queue;
class RenderBundleEncoder;
class RenderBundle;
class RenderPassEncoder;
enum class RenderPassTimestampLocation : uint8_t;
class RenderPipeline;
class Sampler;
enum class SamplerBindingType : uint8_t;
class ShaderModule;
enum class StencilOperation : uint8_t;
enum class StorageTextureAccess : uint8_t;
enum class StoreOp : uint8_t;
class Surface;
class SwapChain;
class Texture;
enum class TextureAspect : uint8_t;
enum class TextureDimension : uint8_t;
enum class TextureFormat : uint8_t;
enum class TextureSampleType : uint8_t;
class TextureView;
enum class TextureViewDimension : uint8_t;
enum class VertexFormat : uint8_t;
enum class VertexStepMode : uint8_t;

class ConvertToBackingContext : public RefCounted<ConvertToBackingContext> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ConvertToBackingContext() = default;

    WGPUAddressMode convertToBacking(AddressMode);
    WGPUBlendFactor convertToBacking(BlendFactor);
    WGPUBlendOperation convertToBacking(BlendOperation);
    WGPUBufferBindingType convertToBacking(BufferBindingType);
    WGPUCompareFunction convertToBacking(CompareFunction);
    WGPUCompilationMessageType convertToBacking(CompilationMessageType);
    WGPUComputePassTimestampLocation convertToBacking(ComputePassTimestampLocation);
    WGPUCullMode convertToBacking(CullMode);
    WGPUErrorFilter convertToBacking(ErrorFilter);
    WGPUFeatureName convertToBacking(FeatureName);
    WGPUFilterMode convertToBacking(FilterMode);
    WGPUFrontFace convertToBacking(FrontFace);
    WGPUIndexFormat convertToBacking(IndexFormat);
    WGPULoadOp convertToBacking(LoadOp);
    WGPUPowerPreference convertToBacking(PowerPreference);
    WGPUPrimitiveTopology convertToBacking(PrimitiveTopology);
    WGPUQueryType convertToBacking(QueryType);
    WGPURenderPassTimestampLocation convertToBacking(RenderPassTimestampLocation);
    WGPUSamplerBindingType convertToBacking(SamplerBindingType);
    WGPUStencilOperation convertToBacking(StencilOperation);
    WGPUStorageTextureAccess convertToBacking(StorageTextureAccess);
    WGPUStoreOp convertToBacking(StoreOp);
    WGPUTextureAspect convertToBacking(TextureAspect);
    WGPUTextureDimension convertToBacking(TextureDimension);
    WGPUTextureFormat convertToBacking(TextureFormat);
    WGPUTextureSampleType convertToBacking(TextureSampleType);
    WGPUTextureViewDimension convertToBacking(TextureViewDimension);
    WGPUVertexFormat convertToBacking(VertexFormat);
    WGPUVertexStepMode convertToBacking(VertexStepMode);

    WGPUBufferUsageFlags convertBufferUsageFlagsToBacking(BufferUsageFlags);
    WGPUColorWriteMaskFlags convertColorWriteFlagsToBacking(ColorWriteFlags);
    WGPUMapModeFlags convertMapModeFlagsToBacking(MapModeFlags);
    WGPUShaderStageFlags convertShaderStageFlagsToBacking(ShaderStageFlags);
    WGPUTextureUsageFlags convertTextureUsageFlagsToBacking(TextureUsageFlags);

    WGPUColor convertToBacking(const Color&);
    WGPUExtent3D convertToBacking(const Extent3D&);
    WGPUOrigin3D convertToBacking(const Origin2D&);
    WGPUOrigin3D convertToBacking(const Origin3D&);

    virtual WGPUAdapter convertToBacking(const Adapter&) = 0;
    virtual WGPUBindGroup convertToBacking(const BindGroup&) = 0;
    virtual WGPUBindGroupLayout convertToBacking(const BindGroupLayout&) = 0;
    virtual WGPUBuffer convertToBacking(const Buffer&) = 0;
    virtual WGPUCommandBuffer convertToBacking(const CommandBuffer&) = 0;
    virtual WGPUCommandEncoder convertToBacking(const CommandEncoder&) = 0;
    virtual WGPUComputePassEncoder convertToBacking(const ComputePassEncoder&) = 0;
    virtual WGPUComputePipeline convertToBacking(const ComputePipeline&) = 0;
    virtual WGPUDevice convertToBacking(const Device&) = 0;
    virtual WGPUInstance convertToBacking(const GPU&) = 0;
    virtual WGPUPipelineLayout convertToBacking(const PipelineLayout&) = 0;
    virtual WGPUQuerySet convertToBacking(const QuerySet&) = 0;
    virtual WGPUQueue convertToBacking(const Queue&) = 0;
    virtual WGPURenderBundleEncoder convertToBacking(const RenderBundleEncoder&) = 0;
    virtual WGPURenderBundle convertToBacking(const RenderBundle&) = 0;
    virtual WGPURenderPassEncoder convertToBacking(const RenderPassEncoder&) = 0;
    virtual WGPURenderPipeline convertToBacking(const RenderPipeline&) = 0;
    virtual WGPUSampler convertToBacking(const Sampler&) = 0;
    virtual WGPUShaderModule convertToBacking(const ShaderModule&) = 0;
    virtual WGPUSurface convertToBacking(const Surface&) = 0;
    virtual WGPUSwapChain convertToBacking(const SwapChain&) = 0;
    virtual WGPUTexture convertToBacking(const Texture&) = 0;
    virtual WGPUTextureView convertToBacking(const TextureView&) = 0;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
