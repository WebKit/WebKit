/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// The WebGPU C++ API.
//
// This is the interface between WebCore / WebKit and a WebGPU implementation: WebGPU::Metal in
// WebGPU.framework (GPU Process and in-process), and the IPC proxies in the Web Process. It
// has declarations and inline code only, so that clients need no symbols from an implementation,
// and it must not include Objective-C or Metal headers.
//
// Enum and flag values are free to change; implementations convert them with switch
// functions.

#pragma once

// The WebGPU_Private module of WebGPU.framework includes every private header, so this header
// must also compile as C and Objective-C.
#ifdef __cplusplus

#include <cstdint>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebGPU {

enum class AddressMode : uint8_t {
    ClampToEdge,
    Repeat,
    MirrorRepeat,
};

enum class BlendFactor : uint8_t {
    Zero,
    One,
    Src,
    OneMinusSrc,
    SrcAlpha,
    OneMinusSrcAlpha,
    Dst,
    OneMinusDst,
    DstAlpha,
    OneMinusDstAlpha,
    SrcAlphaSaturated,
    Constant,
    OneMinusConstant,
};

enum class BlendOperation : uint8_t {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class BufferBindingType : uint8_t {
    Uniform,
    Storage,
    ReadOnlyStorage,
};

enum class BufferUsage : uint16_t {
    MapRead         = 1 << 0,
    MapWrite        = 1 << 1,
    CopySource      = 1 << 2,
    CopyDestination = 1 << 3,
    Index           = 1 << 4,
    Vertex          = 1 << 5,
    Uniform         = 1 << 6,
    Storage         = 1 << 7,
    Indirect        = 1 << 8,
    QueryResolve    = 1 << 9,
};

enum class CanvasAlphaMode : uint8_t {
    Opaque,
    Premultiplied,
};

enum class CanvasToneMappingMode : uint8_t {
    Standard,
    Extended,
};

enum class ColorWrite : uint8_t {
    Red   = 1 << 0,
    Green = 1 << 1,
    Blue  = 1 << 2,
    Alpha = 1 << 3,
};

enum class CompareFunction : uint8_t {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class CompilationMessageType : uint8_t {
    Error,
    Warning,
    Info,
};

enum class CullMode : uint8_t {
    None,
    Front,
    Back,
};

enum class DeviceLostReason : uint8_t {
    Destroyed,
    Unknown,
};

enum class ErrorFilter : uint8_t {
    OutOfMemory,
    Validation,
    Internal,
};

enum class FeatureName : uint8_t {
    DepthClipControl,
    Depth32floatStencil8,
    TextureCompressionBc,
    TextureCompressionBcSliced3d,
    TextureCompressionEtc2,
    TextureCompressionAstc,
    TextureCompressionAstcSliced3d,
    TimestampQuery,
    IndirectFirstInstance,
    ShaderF16,
    Rg11b10ufloatRenderable,
    Bgra8unormStorage,
    Float32Filterable,
    Float32Blendable,
    ClipDistances,
    DualSourceBlending,
    Float16Renderable,
    Float32Renderable,
    CoreFeaturesAndLimits,
    TextureFormatsTier1,
    TextureFormatsTier2,
    PrimitiveIndex,
    Subgroups,
};

enum class FilterMode : uint8_t {
    Nearest,
    Linear,
};

enum class FrontFace : uint8_t {
    CCW,
    CW,
};

enum class IndexFormat : uint8_t {
    Uint16,
    Uint32,
};

enum class LoadOp : uint8_t {
    Load,
    Clear,
};

enum class MapMode : uint8_t {
    Read  = 1 << 0,
    Write = 1 << 1,
};

enum class MipmapFilterMode : uint8_t {
    Nearest,
    Linear,
};

enum class PowerPreference : bool {
    LowPower,
    HighPerformance,
};

enum class PrimitiveTopology : uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class QueryType : uint8_t {
    Occlusion,
    Timestamp,
};

enum class SamplerBindingType : uint8_t {
    Filtering,
    NonFiltering,
    Comparison,
};

enum class ShaderStage : uint8_t {
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
};

enum class StencilOperation : uint8_t {
    Keep,
    Zero,
    Replace,
    Invert,
    IncrementClamp,
    DecrementClamp,
    IncrementWrap,
    DecrementWrap,
};

enum class StorageTextureAccess : uint8_t {
    WriteOnly,
    ReadOnly,
    ReadWrite,
};

enum class StoreOp : uint8_t {
    Store,
    Discard,
};

enum class TextureAspect : uint8_t {
    All,
    StencilOnly,
    DepthOnly,
};

enum class TextureDimension : uint8_t {
    _1d,
    _2d,
    _3d,
};

enum class TextureFormat : uint8_t {
    // 8-bit formats
    R8unorm,
    R8snorm,
    R8uint,
    R8sint,

    // 16-bit formats
    R16unorm,
    R16snorm,
    R16uint,
    R16sint,
    R16float,
    Rg8unorm,
    Rg8snorm,
    Rg8uint,
    Rg8sint,

    // 32-bit formats
    R32uint,
    R32sint,
    R32float,
    Rg16unorm,
    Rg16snorm,
    Rg16uint,
    Rg16sint,
    Rg16float,
    Rgba8unorm,
    Rgba8unormSRGB,
    Rgba8snorm,
    Rgba8uint,
    Rgba8sint,
    Bgra8unorm,
    Bgra8unormSRGB,

    // Packed 32-bit formats
    Rgb9e5ufloat,
    Rgb10a2uint,
    Rgb10a2unorm,
    Rg11b10ufloat,

    // 64-bit formats
    Rg32uint,
    Rg32sint,
    Rg32float,
    Rgba16unorm,
    Rgba16snorm,
    Rgba16uint,
    Rgba16sint,
    Rgba16float,

    // 128-bit formats
    Rgba32uint,
    Rgba32sint,
    Rgba32float,

    // Depth/stencil formats
    Stencil8,
    Depth16unorm,
    Depth24plus,
    Depth24plusStencil8,
    Depth32float,

    // depth32float-stencil8 feature
    Depth32floatStencil8,

    // BC compressed formats usable if texture-compression-bc is both
    // supported by the device/user agent and enabled in requestDevice.
    Bc1RgbaUnorm,
    Bc1RgbaUnormSRGB,
    Bc2RgbaUnorm,
    Bc2RgbaUnormSRGB,
    Bc3RgbaUnorm,
    Bc3RgbaUnormSRGB,
    Bc4RUnorm,
    Bc4RSnorm,
    Bc5RgUnorm,
    Bc5RgSnorm,
    Bc6hRgbUfloat,
    Bc6hRgbFloat,
    Bc7RgbaUnorm,
    Bc7RgbaUnormSRGB,

    // ETC2 compressed formats usable if texture-compression-etc2 is both
    // supported by the device/user agent and enabled in requestDevice.
    Etc2Rgb8unorm,
    Etc2Rgb8unormSRGB,
    Etc2Rgb8a1unorm,
    Etc2Rgb8a1unormSRGB,
    Etc2Rgba8unorm,
    Etc2Rgba8unormSRGB,
    EacR11unorm,
    EacR11snorm,
    EacRg11unorm,
    EacRg11snorm,

    // ASTC compressed formats usable if texture-compression-astc is both
    // supported by the device/user agent and enabled in requestDevice.
    Astc4x4Unorm,
    Astc4x4UnormSRGB,
    Astc5x4Unorm,
    Astc5x4UnormSRGB,
    Astc5x5Unorm,
    Astc5x5UnormSRGB,
    Astc6x5Unorm,
    Astc6x5UnormSRGB,
    Astc6x6Unorm,
    Astc6x6UnormSRGB,
    Astc8x5Unorm,
    Astc8x5UnormSRGB,
    Astc8x6Unorm,
    Astc8x6UnormSRGB,
    Astc8x8Unorm,
    Astc8x8UnormSRGB,
    Astc10x5Unorm,
    Astc10x5UnormSRGB,
    Astc10x6Unorm,
    Astc10x6UnormSRGB,
    Astc10x8Unorm,
    Astc10x8UnormSRGB,
    Astc10x10Unorm,
    Astc10x10UnormSRGB,
    Astc12x10Unorm,
    Astc12x10UnormSRGB,
    Astc12x12Unorm,
    Astc12x12UnormSRGB,
};

enum class TextureSampleType : uint8_t {
    Float,
    UnfilterableFloat,
    Depth,
    Sint,
    Uint,
};

enum class TextureUsage : uint8_t {
    CopySource       = 1 << 0,
    CopyDestination  = 1 << 1,
    TextureBinding   = 1 << 2,
    StorageBinding   = 1 << 3,
    RenderAttachment = 1 << 4,
    Transient        = 1 << 5,

    // Set when the caller passed a bit that is not one of the above, so that the usage can be
    // rejected instead of being silently narrowed to the bits we do recognize.
    Invalid          = 1 << 6,
};

enum class TextureViewDimension : uint8_t {
    _1d,
    _2d,
    _2dArray,
    Cube,
    CubeArray,
    _3d,
};

enum class VertexFormat : uint8_t {
    Uint8,
    Uint8x2,
    Uint8x4,
    Sint8,
    Sint8x2,
    Sint8x4,
    Unorm8,
    Unorm8x2,
    Unorm8x4,
    Snorm8,
    Snorm8x2,
    Snorm8x4,
    Uint16,
    Uint16x2,
    Uint16x4,
    Sint16,
    Sint16x2,
    Sint16x4,
    Unorm16,
    Unorm16x2,
    Unorm16x4,
    Snorm16,
    Snorm16x2,
    Snorm16x4,
    Float16,
    Float16x2,
    Float16x4,
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Uint32x2,
    Uint32x3,
    Uint32x4,
    Sint32,
    Sint32x2,
    Sint32x3,
    Sint32x4,
    Snorm1010102,
    Unorm1010102,
    Unorm8x4Bgra,
};

enum class VertexStepMode : uint8_t {
    Vertex,
    Instance,
};

enum class XREye : uint8_t {
    None,
    Left,
    Right,
};

struct Extent3D {
    uint32_t width { 0 };
    uint32_t height { 1 };
    uint32_t depthOrArrayLayers { 1 };
};

struct Origin3D {
    uint32_t x { 0 };
    uint32_t y { 0 };
    uint32_t z { 0 };
};

struct Color {
    double r { 0 };
    double g { 0 };
    double b { 0 };
    double a { 0 };
};

struct Limits {
    uint32_t maxTextureDimension1D { 0 };
    uint32_t maxTextureDimension2D { 0 };
    uint32_t maxTextureDimension3D { 0 };
    uint32_t maxTextureArrayLayers { 0 };
    uint32_t maxBindGroups { 0 };
    uint32_t maxBindGroupsPlusVertexBuffers { 0 };
    uint32_t maxBindingsPerBindGroup { 0 };
    uint32_t maxDynamicUniformBuffersPerPipelineLayout { 0 };
    uint32_t maxDynamicStorageBuffersPerPipelineLayout { 0 };
    uint32_t maxSampledTexturesPerShaderStage { 0 };
    uint32_t maxSamplersPerShaderStage { 0 };
    uint32_t maxStorageBuffersPerShaderStage { 0 };
    uint32_t maxStorageTexturesPerShaderStage { 0 };
    uint32_t maxUniformBuffersPerShaderStage { 0 };
    uint64_t maxUniformBufferBindingSize { 0 };
    uint64_t maxStorageBufferBindingSize { 0 };
    uint32_t minUniformBufferOffsetAlignment { 0 };
    uint32_t minStorageBufferOffsetAlignment { 0 };
    uint32_t maxVertexBuffers { 0 };
    uint64_t maxBufferSize { 0 };
    uint32_t maxVertexAttributes { 0 };
    uint32_t maxVertexBufferArrayStride { 0 };
    uint32_t maxInterStageShaderVariables { 0 };
    uint32_t maxColorAttachments { 0 };
    uint32_t maxColorAttachmentBytesPerSample { 0 };
    uint32_t maxComputeWorkgroupStorageSize { 0 };
    uint32_t maxComputeInvocationsPerWorkgroup { 0 };
    uint32_t maxComputeWorkgroupSizeX { 0 };
    uint32_t maxComputeWorkgroupSizeY { 0 };
    uint32_t maxComputeWorkgroupSizeZ { 0 };
    uint32_t maxComputeWorkgroupsPerDimension { 0 };
    uint32_t maxStorageBuffersInFragmentStage { 0 };
    uint32_t maxStorageTexturesInFragmentStage { 0 };
    uint32_t maxStorageBuffersInVertexStage { 0 };
    uint32_t maxStorageTexturesInVertexStage { 0 };
};

class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandBuffer;
class CommandEncoder;
class ComputePassEncoder;
class ComputePipeline;
class Device;
class ExternalTexture;
class Instance;
class PipelineLayout;
class PresentationContext;
class QuerySet;
class Queue;
class RenderBundle;
class RenderBundleEncoder;
class RenderPassEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Texture;
class TextureView;
class XRBinding;
class XRProjectionLayer;
class XRSubImage;
class XRView;

} // namespace WebGPU

// Retain and release functions for SWIFT_SHARED_REFERENCE.
inline void refWebGPUAdapter(WebGPU::Adapter*);
inline void derefWebGPUAdapter(WebGPU::Adapter*);
inline void refWebGPUBindGroup(WebGPU::BindGroup*);
inline void derefWebGPUBindGroup(WebGPU::BindGroup*);
inline void refWebGPUBindGroupLayout(WebGPU::BindGroupLayout*);
inline void derefWebGPUBindGroupLayout(WebGPU::BindGroupLayout*);
inline void refWebGPUBuffer(WebGPU::Buffer*);
inline void derefWebGPUBuffer(WebGPU::Buffer*);
inline void refWebGPUCommandBuffer(WebGPU::CommandBuffer*);
inline void derefWebGPUCommandBuffer(WebGPU::CommandBuffer*);
inline void refWebGPUCommandEncoder(WebGPU::CommandEncoder*);
inline void derefWebGPUCommandEncoder(WebGPU::CommandEncoder*);
inline void refWebGPUComputePassEncoder(WebGPU::ComputePassEncoder*);
inline void derefWebGPUComputePassEncoder(WebGPU::ComputePassEncoder*);
inline void refWebGPUComputePipeline(WebGPU::ComputePipeline*);
inline void derefWebGPUComputePipeline(WebGPU::ComputePipeline*);
inline void refWebGPUDevice(WebGPU::Device*);
inline void derefWebGPUDevice(WebGPU::Device*);
inline void refWebGPUExternalTexture(WebGPU::ExternalTexture*);
inline void derefWebGPUExternalTexture(WebGPU::ExternalTexture*);
inline void refWebGPUInstance(WebGPU::Instance*);
inline void derefWebGPUInstance(WebGPU::Instance*);
inline void refWebGPUPipelineLayout(WebGPU::PipelineLayout*);
inline void derefWebGPUPipelineLayout(WebGPU::PipelineLayout*);
inline void refWebGPUPresentationContext(WebGPU::PresentationContext*);
inline void derefWebGPUPresentationContext(WebGPU::PresentationContext*);
inline void refWebGPUQuerySet(WebGPU::QuerySet*);
inline void derefWebGPUQuerySet(WebGPU::QuerySet*);
inline void refWebGPUQueue(WebGPU::Queue*);
inline void derefWebGPUQueue(WebGPU::Queue*);
inline void refWebGPURenderBundle(WebGPU::RenderBundle*);
inline void derefWebGPURenderBundle(WebGPU::RenderBundle*);
inline void refWebGPURenderBundleEncoder(WebGPU::RenderBundleEncoder*);
inline void derefWebGPURenderBundleEncoder(WebGPU::RenderBundleEncoder*);
inline void refWebGPURenderPassEncoder(WebGPU::RenderPassEncoder*);
inline void derefWebGPURenderPassEncoder(WebGPU::RenderPassEncoder*);
inline void refWebGPURenderPipeline(WebGPU::RenderPipeline*);
inline void derefWebGPURenderPipeline(WebGPU::RenderPipeline*);
inline void refWebGPUSampler(WebGPU::Sampler*);
inline void derefWebGPUSampler(WebGPU::Sampler*);
inline void refWebGPUShaderModule(WebGPU::ShaderModule*);
inline void derefWebGPUShaderModule(WebGPU::ShaderModule*);
inline void refWebGPUTexture(WebGPU::Texture*);
inline void derefWebGPUTexture(WebGPU::Texture*);
inline void refWebGPUTextureView(WebGPU::TextureView*);
inline void derefWebGPUTextureView(WebGPU::TextureView*);
inline void refWebGPUXRBinding(WebGPU::XRBinding*);
inline void derefWebGPUXRBinding(WebGPU::XRBinding*);
inline void refWebGPUXRProjectionLayer(WebGPU::XRProjectionLayer*);
inline void derefWebGPUXRProjectionLayer(WebGPU::XRProjectionLayer*);
inline void refWebGPUXRSubImage(WebGPU::XRSubImage*);
inline void derefWebGPUXRSubImage(WebGPU::XRSubImage*);
inline void refWebGPUXRView(WebGPU::XRView*);
inline void derefWebGPUXRView(WebGPU::XRView*);

namespace WebGPU {

class Adapter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Adapter> {
public:
    virtual ~Adapter() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Adapter() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUAdapter, derefWebGPUAdapter) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class BindGroup : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<BindGroup> {
public:
    virtual ~BindGroup() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    BindGroup() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUBindGroup, derefWebGPUBindGroup) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class BindGroupLayout : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<BindGroupLayout> {
public:
    virtual ~BindGroupLayout() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    BindGroupLayout() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUBindGroupLayout, derefWebGPUBindGroupLayout) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class Buffer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Buffer> {
public:
    virtual ~Buffer() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Buffer() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUBuffer, derefWebGPUBuffer) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class CommandBuffer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CommandBuffer> {
public:
    virtual ~CommandBuffer() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    CommandBuffer() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUCommandBuffer, derefWebGPUCommandBuffer) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class CommandEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CommandEncoder> {
public:
    virtual ~CommandEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    CommandEncoder() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUCommandEncoder, derefWebGPUCommandEncoder) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class ComputePassEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ComputePassEncoder> {
public:
    virtual ~ComputePassEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ComputePassEncoder() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUComputePassEncoder, derefWebGPUComputePassEncoder) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class ComputePipeline : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ComputePipeline> {
public:
    virtual ~ComputePipeline() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ComputePipeline() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUComputePipeline, derefWebGPUComputePipeline) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class Device : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Device> {
public:
    virtual ~Device() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Device() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUDevice, derefWebGPUDevice) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class ExternalTexture : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ExternalTexture> {
public:
    virtual ~ExternalTexture() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ExternalTexture() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUExternalTexture, derefWebGPUExternalTexture) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class Instance : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Instance> {
public:
    virtual ~Instance() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Instance() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUInstance, derefWebGPUInstance) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class PipelineLayout : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PipelineLayout> {
public:
    virtual ~PipelineLayout() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    PipelineLayout() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUPipelineLayout, derefWebGPUPipelineLayout) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class PresentationContext : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PresentationContext> {
public:
    virtual ~PresentationContext() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    PresentationContext() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUPresentationContext, derefWebGPUPresentationContext) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class QuerySet : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<QuerySet> {
public:
    virtual ~QuerySet() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    QuerySet() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUQuerySet, derefWebGPUQuerySet) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class Queue : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Queue> {
public:
    virtual ~Queue() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Queue() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUQueue, derefWebGPUQueue) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class RenderBundle : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderBundle> {
public:
    virtual ~RenderBundle() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderBundle() = default;
} SWIFT_SHARED_REFERENCE(refWebGPURenderBundle, derefWebGPURenderBundle) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class RenderBundleEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderBundleEncoder> {
public:
    virtual ~RenderBundleEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderBundleEncoder() = default;
} SWIFT_SHARED_REFERENCE(refWebGPURenderBundleEncoder, derefWebGPURenderBundleEncoder) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class RenderPassEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderPassEncoder> {
public:
    virtual ~RenderPassEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderPassEncoder() = default;
} SWIFT_SHARED_REFERENCE(refWebGPURenderPassEncoder, derefWebGPURenderPassEncoder) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class RenderPipeline : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderPipeline> {
public:
    virtual ~RenderPipeline() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderPipeline() = default;
} SWIFT_SHARED_REFERENCE(refWebGPURenderPipeline, derefWebGPURenderPipeline) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class Sampler : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Sampler> {
public:
    virtual ~Sampler() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Sampler() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUSampler, derefWebGPUSampler) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class ShaderModule : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ShaderModule> {
public:
    virtual ~ShaderModule() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ShaderModule() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUShaderModule, derefWebGPUShaderModule) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class Texture : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Texture> {
public:
    virtual ~Texture() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Texture() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUTexture, derefWebGPUTexture) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class TextureView : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<TextureView> {
public:
    virtual ~TextureView() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    TextureView() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUTextureView, derefWebGPUTextureView) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class XRBinding : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRBinding> {
public:
    virtual ~XRBinding() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRBinding() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUXRBinding, derefWebGPUXRBinding) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class XRProjectionLayer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRProjectionLayer> {
public:
    virtual ~XRProjectionLayer() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRProjectionLayer() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUXRProjectionLayer, derefWebGPUXRProjectionLayer) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class XRSubImage : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRSubImage> {
public:
    virtual ~XRSubImage() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRSubImage() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUXRSubImage, derefWebGPUXRSubImage) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

class XRView : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRView> {
public:
    virtual ~XRView() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRView() = default;
} SWIFT_SHARED_REFERENCE(refWebGPUXRView, derefWebGPUXRView) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

} // namespace WebGPU

inline void refWebGPUAdapter(WebGPU::Adapter* object)
{
    object->ref();
}

inline void derefWebGPUAdapter(WebGPU::Adapter* object)
{
    object->deref();
}

inline void refWebGPUBindGroup(WebGPU::BindGroup* object)
{
    object->ref();
}

inline void derefWebGPUBindGroup(WebGPU::BindGroup* object)
{
    object->deref();
}

inline void refWebGPUBindGroupLayout(WebGPU::BindGroupLayout* object)
{
    object->ref();
}

inline void derefWebGPUBindGroupLayout(WebGPU::BindGroupLayout* object)
{
    object->deref();
}

inline void refWebGPUBuffer(WebGPU::Buffer* object)
{
    object->ref();
}

inline void derefWebGPUBuffer(WebGPU::Buffer* object)
{
    object->deref();
}

inline void refWebGPUCommandBuffer(WebGPU::CommandBuffer* object)
{
    object->ref();
}

inline void derefWebGPUCommandBuffer(WebGPU::CommandBuffer* object)
{
    object->deref();
}

inline void refWebGPUCommandEncoder(WebGPU::CommandEncoder* object)
{
    object->ref();
}

inline void derefWebGPUCommandEncoder(WebGPU::CommandEncoder* object)
{
    object->deref();
}

inline void refWebGPUComputePassEncoder(WebGPU::ComputePassEncoder* object)
{
    object->ref();
}

inline void derefWebGPUComputePassEncoder(WebGPU::ComputePassEncoder* object)
{
    object->deref();
}

inline void refWebGPUComputePipeline(WebGPU::ComputePipeline* object)
{
    object->ref();
}

inline void derefWebGPUComputePipeline(WebGPU::ComputePipeline* object)
{
    object->deref();
}

inline void refWebGPUDevice(WebGPU::Device* object)
{
    object->ref();
}

inline void derefWebGPUDevice(WebGPU::Device* object)
{
    object->deref();
}

inline void refWebGPUExternalTexture(WebGPU::ExternalTexture* object)
{
    object->ref();
}

inline void derefWebGPUExternalTexture(WebGPU::ExternalTexture* object)
{
    object->deref();
}

inline void refWebGPUInstance(WebGPU::Instance* object)
{
    object->ref();
}

inline void derefWebGPUInstance(WebGPU::Instance* object)
{
    object->deref();
}

inline void refWebGPUPipelineLayout(WebGPU::PipelineLayout* object)
{
    object->ref();
}

inline void derefWebGPUPipelineLayout(WebGPU::PipelineLayout* object)
{
    object->deref();
}

inline void refWebGPUPresentationContext(WebGPU::PresentationContext* object)
{
    object->ref();
}

inline void derefWebGPUPresentationContext(WebGPU::PresentationContext* object)
{
    object->deref();
}

inline void refWebGPUQuerySet(WebGPU::QuerySet* object)
{
    object->ref();
}

inline void derefWebGPUQuerySet(WebGPU::QuerySet* object)
{
    object->deref();
}

inline void refWebGPUQueue(WebGPU::Queue* object)
{
    object->ref();
}

inline void derefWebGPUQueue(WebGPU::Queue* object)
{
    object->deref();
}

inline void refWebGPURenderBundle(WebGPU::RenderBundle* object)
{
    object->ref();
}

inline void derefWebGPURenderBundle(WebGPU::RenderBundle* object)
{
    object->deref();
}

inline void refWebGPURenderBundleEncoder(WebGPU::RenderBundleEncoder* object)
{
    object->ref();
}

inline void derefWebGPURenderBundleEncoder(WebGPU::RenderBundleEncoder* object)
{
    object->deref();
}

inline void refWebGPURenderPassEncoder(WebGPU::RenderPassEncoder* object)
{
    object->ref();
}

inline void derefWebGPURenderPassEncoder(WebGPU::RenderPassEncoder* object)
{
    object->deref();
}

inline void refWebGPURenderPipeline(WebGPU::RenderPipeline* object)
{
    object->ref();
}

inline void derefWebGPURenderPipeline(WebGPU::RenderPipeline* object)
{
    object->deref();
}

inline void refWebGPUSampler(WebGPU::Sampler* object)
{
    object->ref();
}

inline void derefWebGPUSampler(WebGPU::Sampler* object)
{
    object->deref();
}

inline void refWebGPUShaderModule(WebGPU::ShaderModule* object)
{
    object->ref();
}

inline void derefWebGPUShaderModule(WebGPU::ShaderModule* object)
{
    object->deref();
}

inline void refWebGPUTexture(WebGPU::Texture* object)
{
    object->ref();
}

inline void derefWebGPUTexture(WebGPU::Texture* object)
{
    object->deref();
}

inline void refWebGPUTextureView(WebGPU::TextureView* object)
{
    object->ref();
}

inline void derefWebGPUTextureView(WebGPU::TextureView* object)
{
    object->deref();
}

inline void refWebGPUXRBinding(WebGPU::XRBinding* object)
{
    object->ref();
}

inline void derefWebGPUXRBinding(WebGPU::XRBinding* object)
{
    object->deref();
}

inline void refWebGPUXRProjectionLayer(WebGPU::XRProjectionLayer* object)
{
    object->ref();
}

inline void derefWebGPUXRProjectionLayer(WebGPU::XRProjectionLayer* object)
{
    object->deref();
}

inline void refWebGPUXRSubImage(WebGPU::XRSubImage* object)
{
    object->ref();
}

inline void derefWebGPUXRSubImage(WebGPU::XRSubImage* object)
{
    object->deref();
}

inline void refWebGPUXRView(WebGPU::XRView* object)
{
    object->ref();
}

inline void derefWebGPUXRView(WebGPU::XRView* object)
{
    object->deref();
}

#endif // __cplusplus
