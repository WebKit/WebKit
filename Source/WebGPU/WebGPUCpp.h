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
//
// Swift C++ interop constrains the types that Swift code takes:
// - Swift 6.3 and 6.4 crash (swift-frontend) when Swift code copies a struct that holds a
//   WTF::Variant. std::variant copies
//   without problems, so the types that Swift takes hold std::variant.
// - Swift 6.3 makes no C++ thunk for an @_expose(Cxx) Swift function that takes a
//   SWIFT_NONCOPYABLE type, borrowing or not. The function is silently left out of the generated
//   header. Swift 6.4 makes the thunk for a borrowing parameter. So the types that Swift takes are
//   not SWIFT_NONCOPYABLE. SWIFT_NONESCAPABLE types work with both.
// The alternative is to hold WTF::Variant, to not use SWIFT_NONCOPYABLE, to take the types as
// borrowing in Swift and to never copy them there. Copying one, such as with `copy`, then crashes
// the compiler.

#pragma once

// The WebGPU_Private module of WebGPU.framework includes every private header, so this header
// must also compile as C and Objective-C.
#ifdef __cplusplus

#include <cstdint>
#include <optional>
#include <span>
#include <variant> // NOLINT: See the Swift C++ interop constraints above.
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>

typedef struct __CVBuffer* CVPixelBufferRef;
typedef struct __IOSurface* IOSurfaceRef;
#endif

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

enum class ErrorType : uint8_t {
    Validation,
    OutOfMemory,
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
    CCW, // NOLINT
    CW, // NOLINT
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

enum class PipelineErrorReason : uint8_t {
    Validation,
    Internal,
};

enum class PowerPreference : bool {
    LowPower,
    HighPerformance,
};

enum class PredefinedColorSpace : uint8_t {
    SRGB, // NOLINT
    SRGBLinear,
    DisplayP3,
    DisplayP3Linear,
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

// The clockwise rotation that presents a video frame.
enum class VideoFrameRotation : uint8_t {
    None,
    Right,
    UpsideDown,
    Left,
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

struct Extent2D {
    uint32_t width { 0 };
    uint32_t height { 0 };
};

struct Origin2D {
    uint32_t x { 0 };
    uint32_t y { 0 };
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

// Descriptors are call parameters only. Implementations must not store them.

// https://gpuweb.github.io/gpuweb/#dictdef-gpubufferdescriptor
struct BufferDescriptor {
    String label;
    OptionSet<BufferUsage> usage;
    uint64_t size { 0 };
    bool mappedAtCreation { false };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpuquerysetdescriptor
struct QuerySetDescriptor {
    String label;
    QueryType type { QueryType::Occlusion };
    uint32_t count { 0 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpusamplerdescriptor
struct SamplerDescriptor {
    String label;
    AddressMode addressModeU { AddressMode::ClampToEdge };
    AddressMode addressModeV { AddressMode::ClampToEdge };
    AddressMode addressModeW { AddressMode::ClampToEdge };
    FilterMode magFilter { FilterMode::Nearest };
    FilterMode minFilter { FilterMode::Nearest };
    MipmapFilterMode mipmapFilter { MipmapFilterMode::Nearest };
    float lodMinClamp { 0 };
    float lodMaxClamp { 32 };
    std::optional<CompareFunction> compare; // std::nullopt: not a comparison sampler.
    uint16_t maxAnisotropy { 1 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gputexturedescriptor
struct TextureDescriptor {
    String label;
    OptionSet<TextureUsage> usage;
    TextureDimension dimension { TextureDimension::_2d };
    Extent3D size;
    TextureFormat format { TextureFormat::R8unorm };
    uint32_t mipLevelCount { 1 };
    uint32_t sampleCount { 1 };
    std::span<const TextureFormat> viewFormats; // Borrowed for the duration of the call.
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gputextureviewdescriptor
// The std::nullopt members take their values from the texture, as described in
// https://gpuweb.github.io/gpuweb/#abstract-opdef-resolving-gputextureviewdescriptor-defaults.
struct TextureViewDescriptor {
    String label;
    std::optional<TextureFormat> format;
    std::optional<TextureViewDimension> dimension;
    uint32_t baseMipLevel { 0 };
    std::optional<uint32_t> mipLevelCount;
    uint32_t baseArrayLayer { 0 };
    std::optional<uint32_t> arrayLayerCount;
    TextureAspect aspect { TextureAspect::All };
    OptionSet<TextureUsage> usage; // Empty: the usage of the texture.
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpushadermodulecompilationhint
struct ShaderModuleCompilationHint {
    String entryPoint;
    Ref<PipelineLayout> layout;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpushadermoduledescriptor
struct ShaderModuleDescriptor {
    String label;
    String code; // WGSL.
    std::span<const ShaderModuleCompilationHint> hints; // Borrowed for the duration of the call.
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpucommandencoderdescriptor
struct CommandEncoderDescriptor {
    String label;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpucommandbufferdescriptor
struct CommandBufferDescriptor {
    String label;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderpasstimestampwrites
// https://gpuweb.github.io/gpuweb/#dictdef-gpucomputepasstimestampwrites
struct PassTimestampWrites {
    Ref<QuerySet> querySet;
    std::optional<uint32_t> beginningOfPassWriteIndex;
    std::optional<uint32_t> endOfPassWriteIndex;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpucomputepassdescriptor
struct ComputePassDescriptor {
    String label;
    std::optional<PassTimestampWrites> timestampWrites;
};

// A texture as a render pass attachment is its default view. std::variant, not WTF::Variant, because
// Swift takes RenderPassDescriptor. See the Swift C++ interop constraints above.
using RenderPassAttachmentView = std::variant<Ref<TextureView>, Ref<Texture>>; // NOLINT

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderpasscolorattachment
struct RenderPassColorAttachment {
    RenderPassAttachmentView view;
    std::optional<uint32_t> depthSlice;
    std::optional<RenderPassAttachmentView> resolveTarget;
    Color clearValue;
    LoadOp loadOp { LoadOp::Load };
    StoreOp storeOp { StoreOp::Store };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderpassdepthstencilattachment
struct RenderPassDepthStencilAttachment {
    RenderPassAttachmentView view;
    float depthClearValue { 0 };
    std::optional<LoadOp> depthLoadOp;
    std::optional<StoreOp> depthStoreOp;
    bool depthReadOnly { false };
    uint32_t stencilClearValue { 0 };
    std::optional<LoadOp> stencilLoadOp;
    std::optional<StoreOp> stencilStoreOp;
    bool stencilReadOnly { false };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderpassdescriptor
struct RenderPassDescriptor {
    String label;
    // Borrowed for the duration of the call. std::nullopt: no color attachment in that slot.
    std::span<const std::optional<RenderPassColorAttachment>> colorAttachments;
    std::optional<RenderPassDepthStencilAttachment> depthStencilAttachment;
    RefPtr<QuerySet> occlusionQuerySet;
    std::optional<PassTimestampWrites> timestampWrites;
    std::optional<uint64_t> maxDrawCount;

    size_t colorAttachmentCount() const { return colorAttachments.size(); }
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#gpuerror
struct Error {
    ErrorType type { ErrorType::Validation };
    String message;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpurequestadapteroptions
struct RequestAdapterOptions {
    std::optional<PowerPreference> powerPreference;
    bool forceFallbackAdapter { false };
    bool xrCompatible { false };
};

// https://gpuweb.github.io/gpuweb/#gpuadapterinfo
struct AdapterInfo {
    String name;
    bool isFallbackAdapter { false };
    uint32_t subgroupMinSize { 0 };
    uint32_t subgroupMaxSize { 0 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpudevicedescriptor
struct DeviceDescriptor {
    String label;
    std::span<const FeatureName> requiredFeatures; // Borrowed for the duration of the call.
    std::optional<Limits> requiredLimits; // std::nullopt: the default limits.
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpucanvasconfiguration, with the size of the canvas, which
// the canvas knows and the configuration does not.
struct CanvasConfiguration {
    Ref<Device> device;
    TextureFormat format { TextureFormat::Bgra8unorm };
    OptionSet<TextureUsage> usage { TextureUsage::RenderAttachment };
    std::span<const TextureFormat> viewFormats; // Borrowed for the duration of the call.
    PredefinedColorSpace colorSpace { PredefinedColorSpace::SRGB };
    CanvasToneMappingMode toneMappingMode { CanvasToneMappingMode::Standard };
    CanvasAlphaMode compositingAlphaMode { CanvasAlphaMode::Opaque };
    bool reportValidationErrors { true };
    uint32_t width { 0 };
    uint32_t height { 0 };
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpucopyexternalimagedestinfo
struct ImageCopyTextureTagged {
    Ref<Texture> texture;
    uint32_t mipLevel { 0 };
    Origin3D origin;
    TextureAspect aspect { TextureAspect::All };
    PredefinedColorSpace colorSpace { PredefinedColorSpace::SRGB };
    bool premultipliedAlpha { false };
};

#if PLATFORM(COCOA)
// https://gpuweb.github.io/gpuweb/#dictdef-gpuexternaltexturedescriptor, with the pixel buffer of the
// video source.
struct ExternalTextureDescriptor {
    String label;
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    PredefinedColorSpace colorSpace { PredefinedColorSpace::SRGB };
    // The size the source presents the frame at, which the pixel buffer does not carry. Zero when
    // the source could not say, and then the decoded size of the frame stands in for it.
    Extent2D visibleSize;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpucopyexternalimagesourceinfo, with the IOSurface of an
// image or canvas, or the pixel buffer of a video frame, as the source. Exactly one of them is set.
struct ImageCopyExternalImage {
    RetainPtr<IOSurfaceRef> source;
    // The format of the single plane of the IOSurface. An accelerated 2D canvas can be backed by it.
    std::optional<TextureFormat> sourceFormat;
    // The logical extent of the IOSurface, which may be larger.
    Extent2D sourceSize;
    // A video frame carries its own extent, crop and primaries, and it is treated as opaque.
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    // The display transform of the frame: a horizontal mirror, then a clockwise rotation.
    VideoFrameRotation pixelBufferRotation { VideoFrameRotation::None };
    bool pixelBufferIsMirrored { false };
    Origin2D origin;
    bool flipY { false };
    // False when the alpha channel of the source carries no data, as for an opaque canvas.
    bool hasAlpha { true };
    bool premultipliedAlpha { true };
    PredefinedColorSpace colorSpace { PredefinedColorSpace::SRGB };
};
#endif

// https://immersive-web.github.io/WebXR-WebGPU-Binding/#dictdef-xrgpuprojectionlayerinit
struct XRProjectionLayerDescriptor {
    TextureFormat colorFormat { TextureFormat::Bgra8unorm };
    std::optional<TextureFormat> depthStencilFormat;
    OptionSet<TextureUsage> textureUsage { TextureUsage::RenderAttachment };
    double scaleFactor { 1 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderbundleencoderdescriptor
struct RenderBundleEncoderDescriptor {
    String label;
    // Borrowed for the duration of the call. std::nullopt: no color attachment in that slot.
    std::span<const std::optional<TextureFormat>> colorFormats;
    std::optional<TextureFormat> depthStencilFormat;
    uint32_t sampleCount { 1 };
    bool depthReadOnly { false };
    bool stencilReadOnly { false };
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderbundledescriptor
struct RenderBundleDescriptor {
    String label;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gputexelcopybufferlayout
struct TexelCopyBufferLayout {
    uint64_t offset { 0 };
    std::optional<uint32_t> bytesPerRow;
    std::optional<uint32_t> rowsPerImage;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gputexelcopybufferinfo
struct TexelCopyBufferInfo {
    TexelCopyBufferLayout layout;
    Ref<Buffer> buffer;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gputexelcopytextureinfo
struct TexelCopyTextureInfo {
    Ref<Texture> texture;
    uint32_t mipLevel { 0 };
    Origin3D origin;
    TextureAspect aspect { TextureAspect::All };
};

// https://gpuweb.github.io/gpuweb/#dom-gpuprogrammablestage-constants
struct ConstantEntry {
    String key;
    double value { 0 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpuprogrammablestage
struct ProgrammableStage {
    Ref<ShaderModule> module;
    String entryPoint; // A null string: the only entry point of the module for the stage.
    std::span<const ConstantEntry> constants; // Borrowed for the duration of the call.
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpucomputepipelinedescriptor
struct ComputePipelineDescriptor {
    String label;
    RefPtr<PipelineLayout> layout;
    ProgrammableStage compute;
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpuvertexattribute
struct VertexAttribute {
    VertexFormat format { VertexFormat::Uint8x2 };
    uint64_t offset { 0 };
    uint32_t shaderLocation { 0 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpuvertexbufferlayout
struct VertexBufferLayout {
    uint64_t arrayStride { 0 };
    VertexStepMode stepMode { VertexStepMode::Vertex };
    std::span<const VertexAttribute> attributes; // Borrowed for the duration of the call.
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpuvertexstate
struct VertexState {
    ProgrammableStage stage;
    // Borrowed for the duration of the call. std::nullopt: no vertex buffer in that slot.
    std::span<const std::optional<VertexBufferLayout>> buffers;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpublendcomponent
struct BlendComponent {
    BlendOperation operation { BlendOperation::Add };
    BlendFactor srcFactor { BlendFactor::One };
    BlendFactor dstFactor { BlendFactor::Zero };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpublendstate
struct BlendState {
    BlendComponent color;
    BlendComponent alpha;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpucolortargetstate
struct ColorTargetState {
    TextureFormat format { TextureFormat::R8unorm };
    std::optional<BlendState> blend;
    OptionSet<ColorWrite> writeMask { ColorWrite::Red, ColorWrite::Green, ColorWrite::Blue, ColorWrite::Alpha };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpufragmentstate
struct FragmentState {
    ProgrammableStage stage;
    // Borrowed for the duration of the call. std::nullopt: no color target in that slot.
    std::span<const std::optional<ColorTargetState>> targets;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpuprimitivestate
struct PrimitiveState {
    PrimitiveTopology topology { PrimitiveTopology::TriangleList };
    std::optional<IndexFormat> stripIndexFormat;
    FrontFace frontFace { FrontFace::CCW };
    CullMode cullMode { CullMode::None };
    bool unclippedDepth { false };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpustencilfacestate
struct StencilFaceState {
    CompareFunction compare { CompareFunction::Always };
    StencilOperation failOp { StencilOperation::Keep };
    StencilOperation depthFailOp { StencilOperation::Keep };
    StencilOperation passOp { StencilOperation::Keep };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpudepthstencilstate
struct DepthStencilState {
    TextureFormat format { TextureFormat::Depth24plus };
    std::optional<bool> depthWriteEnabled;
    std::optional<CompareFunction> depthCompare;
    StencilFaceState stencilFront;
    StencilFaceState stencilBack;
    uint32_t stencilReadMask { 0xFFFFFFFF };
    uint32_t stencilWriteMask { 0xFFFFFFFF };
    int32_t depthBias { 0 };
    float depthBiasSlopeScale { 0 };
    float depthBiasClamp { 0 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpumultisamplestate
struct MultisampleState {
    uint32_t count { 1 };
    uint32_t mask { 0xFFFFFFFF };
    bool alphaToCoverageEnabled { false };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpurenderpipelinedescriptor
struct RenderPipelineDescriptor {
    String label;
    RefPtr<PipelineLayout> layout; // nullptr: a layout that the pipeline generates from its shaders.
    VertexState vertex;
    PrimitiveState primitive;
    std::optional<DepthStencilState> depthStencil;
    MultisampleState multisample;
    std::optional<FragmentState> fragment;
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#gpupipelineerror
struct PipelineError {
    PipelineErrorReason reason { PipelineErrorReason::Validation };
    String message;
};

// https://gpuweb.github.io/gpuweb/#gpucompilationmessage
struct CompilationMessage {
    String message;
    CompilationMessageType type { CompilationMessageType::Error };
    uint64_t lineNum { 0 };
    uint64_t linePos { 0 };
    uint64_t offset { 0 };
    uint64_t length { 0 };
};

// https://gpuweb.github.io/gpuweb/#gpucompilationinfo
struct CompilationInfo {
    Vector<CompilationMessage> messages;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpubufferbindinglayout
struct BufferBindingLayout {
    BufferBindingType type { BufferBindingType::Uniform };
    bool hasDynamicOffset { false };
    uint64_t minBindingSize { 0 };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpusamplerbindinglayout
struct SamplerBindingLayout {
    SamplerBindingType type { SamplerBindingType::Filtering };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gputexturebindinglayout
struct TextureBindingLayout {
    TextureSampleType sampleType { TextureSampleType::Float };
    TextureViewDimension viewDimension { TextureViewDimension::_2d };
    bool multisampled { false };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpustoragetexturebindinglayout
struct StorageTextureBindingLayout {
    StorageTextureAccess access { StorageTextureAccess::WriteOnly };
    TextureFormat format { TextureFormat::R8unorm };
    TextureViewDimension viewDimension { TextureViewDimension::_2d };
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpuexternaltexturebindinglayout
struct ExternalTextureBindingLayout {
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpubindgrouplayoutentry
// A valid entry has exactly one of the binding layout members. The implementation validates this,
// as the specification requires.
struct BindGroupLayoutEntry {
    uint32_t binding { 0 };
    OptionSet<ShaderStage> visibility;
    std::optional<BufferBindingLayout> buffer;
    std::optional<SamplerBindingLayout> sampler;
    std::optional<TextureBindingLayout> texture;
    std::optional<StorageTextureBindingLayout> storageTexture;
    std::optional<ExternalTextureBindingLayout> externalTexture;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpubindgrouplayoutdescriptor
struct BindGroupLayoutDescriptor {
    String label;
    std::span<const BindGroupLayoutEntry> entries; // Borrowed for the duration of the call.
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpupipelinelayoutdescriptor
struct PipelineLayoutDescriptor {
    String label;
    // Borrowed for the duration of the call. std::nullopt makes a layout that the pipeline
    // generates from its shaders.
    std::optional<std::span<const Ref<BindGroupLayout>>> bindGroupLayouts;
} SWIFT_NONESCAPABLE;

// https://gpuweb.github.io/gpuweb/#dictdef-gpubufferbinding
struct BufferBinding {
    Ref<Buffer> buffer;
    uint64_t offset { 0 };
    std::optional<uint64_t> size; // std::nullopt: the rest of the buffer after the offset.
};

// https://gpuweb.github.io/gpuweb/#typedefdef-gpubindingresource
using BindingResource = Variant<Ref<Sampler>, Ref<Texture>, Ref<TextureView>, BufferBinding, Ref<ExternalTexture>>;

// https://gpuweb.github.io/gpuweb/#dictdef-gpubindgroupentry
struct BindGroupEntry {
    uint32_t binding { 0 };
    BindingResource resource;
};

// https://gpuweb.github.io/gpuweb/#dictdef-gpubindgroupdescriptor
struct BindGroupDescriptor {
    String label;
    Ref<BindGroupLayout> layout;
    std::span<const BindGroupEntry> entries; // Borrowed for the duration of the call.
} SWIFT_NONESCAPABLE;

} // namespace WebGPU

// Retain and release functions for SWIFT_SHARED_REFERENCE.
#if !ENABLE(SWIFT_BASE_CLASS_ANNOTATIONS)
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
#endif

namespace WebGPU {

class Adapter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Adapter> {
public:
    virtual ~Adapter() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Adapter() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUAdapter, derefWebGPUAdapter);

class BindGroup : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<BindGroup> {
public:
    virtual ~BindGroup() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    BindGroup() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUBindGroup, derefWebGPUBindGroup);

class BindGroupLayout : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<BindGroupLayout> {
public:
    virtual ~BindGroupLayout() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    BindGroupLayout() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUBindGroupLayout, derefWebGPUBindGroupLayout);

class Buffer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Buffer> {
public:
    virtual ~Buffer() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Buffer() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUBuffer, derefWebGPUBuffer);

class CommandBuffer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CommandBuffer> {
public:
    virtual ~CommandBuffer() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    CommandBuffer() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUCommandBuffer, derefWebGPUCommandBuffer);

class CommandEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<CommandEncoder> {
public:
    virtual ~CommandEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    CommandEncoder() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUCommandEncoder, derefWebGPUCommandEncoder);

class ComputePassEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ComputePassEncoder> {
public:
    virtual ~ComputePassEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ComputePassEncoder() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUComputePassEncoder, derefWebGPUComputePassEncoder);

class ComputePipeline : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ComputePipeline> {
public:
    virtual ~ComputePipeline() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ComputePipeline() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUComputePipeline, derefWebGPUComputePipeline);

class Device : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Device> {
public:
    virtual ~Device() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Device() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUDevice, derefWebGPUDevice);

class ExternalTexture : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ExternalTexture> {
public:
    virtual ~ExternalTexture() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ExternalTexture() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUExternalTexture, derefWebGPUExternalTexture);

class Instance : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Instance> {
public:
    virtual ~Instance() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Instance() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUInstance, derefWebGPUInstance);

class PipelineLayout : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PipelineLayout> {
public:
    virtual ~PipelineLayout() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    PipelineLayout() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUPipelineLayout, derefWebGPUPipelineLayout);

class PresentationContext : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PresentationContext> {
public:
    virtual ~PresentationContext() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    PresentationContext() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUPresentationContext, derefWebGPUPresentationContext);

class QuerySet : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<QuerySet> {
public:
    virtual ~QuerySet() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    QuerySet() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUQuerySet, derefWebGPUQuerySet);

class Queue : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Queue> {
public:
    virtual ~Queue() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Queue() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUQueue, derefWebGPUQueue);

class RenderBundle : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderBundle> {
public:
    virtual ~RenderBundle() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderBundle() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPURenderBundle, derefWebGPURenderBundle);

class RenderBundleEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderBundleEncoder> {
public:
    virtual ~RenderBundleEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderBundleEncoder() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPURenderBundleEncoder, derefWebGPURenderBundleEncoder);

class RenderPassEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderPassEncoder> {
public:
    virtual ~RenderPassEncoder() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderPassEncoder() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPURenderPassEncoder, derefWebGPURenderPassEncoder);

class RenderPipeline : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RenderPipeline> {
public:
    virtual ~RenderPipeline() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    RenderPipeline() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPURenderPipeline, derefWebGPURenderPipeline);

class Sampler : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Sampler> {
public:
    virtual ~Sampler() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Sampler() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUSampler, derefWebGPUSampler);

class ShaderModule : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ShaderModule> {
public:
    virtual ~ShaderModule() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    ShaderModule() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUShaderModule, derefWebGPUShaderModule);

class Texture : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Texture> {
public:
    virtual ~Texture() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    Texture() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUTexture, derefWebGPUTexture);

class TextureView : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<TextureView> {
public:
    virtual ~TextureView() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    TextureView() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUTextureView, derefWebGPUTextureView);

class XRBinding : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRBinding> {
public:
    virtual ~XRBinding() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRBinding() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUXRBinding, derefWebGPUXRBinding);

class XRProjectionLayer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRProjectionLayer> {
public:
    virtual ~XRProjectionLayer() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRProjectionLayer() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUXRProjectionLayer, derefWebGPUXRProjectionLayer);

class XRSubImage : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRSubImage> {
public:
    virtual ~XRSubImage() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRSubImage() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUXRSubImage, derefWebGPUXRSubImage);

class XRView : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<XRView> {
public:
    virtual ~XRView() = default;

    virtual void setLabel(String&&) = 0;
    virtual bool isValid() const = 0;

protected:
    XRView() = default;
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(refWebGPUXRView, derefWebGPUXRView);

} // namespace WebGPU

#if !ENABLE(SWIFT_BASE_CLASS_ANNOTATIONS)
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
#endif

#endif // __cplusplus
