// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsWgpu.cpp:
//   Helper methods for the WebGPU backend.
//

#include "libANGLE/renderer/wgpu/UtilsWgpu.h"

#include <sstream>
#include "common/unsafe_buffers.h"
#include "dawn/dawn_proc_table.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/FormatID_autogen.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_format_utils.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"
#include "webgpu/webgpu.h"

namespace rx
{
namespace
{
const bool kLogShaders = false;

constexpr char kVertexEntryPoint[]   = "vs_main";
constexpr char kFragmentEntryPoint[] = "fs_main";

constexpr char kVertexMain[] = R"(
struct VertexInput {
    @location(0) pos: vec2<f32>,
    @location(1) texCoord: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) texCoord: vec2<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4<f32>(in.pos, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}
)";

constexpr char kBlitVertexMain[] = R"(
struct VertexInput {
    @location(0) pos: vec2<f32>,
    @location(1) texScale: vec2<f32>,
    @location(2) texOffset: vec2<f32>,
    @location(3) nearest0: vec4<i32>,
    @location(4) nearest1: vec4<i32>,
    @location(5) nearest2: vec4<i32>,
    @location(6) nearest3: vec4<i32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) @interpolate(flat) texScale: vec2<f32>,
    @location(1) @interpolate(flat) texOffset: vec2<f32>,
    @location(2) @interpolate(flat) nearest0: vec4<i32>,
    @location(3) @interpolate(flat) nearest1: vec4<i32>,
    @location(4) @interpolate(flat) nearest2: vec4<i32>,
    @location(5) @interpolate(flat) nearest3: vec4<i32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4<f32>(in.pos, 0.0, 1.0);
    out.texScale = in.texScale;
    out.texOffset = in.texOffset;
    out.nearest0 = in.nearest0;
    out.nearest1 = in.nearest1;
    out.nearest2 = in.nearest2;
    out.nearest3 = in.nearest3;
    return out;
}

// The GL_NEAREST source texel of the fragment, computed exactly in integers.
fn nearestTexel(in: VertexOutput) -> vec2<i32> {
    let m = vec2<i32>(floor(in.position.xy)) - in.nearest3.zw;
    let t = in.nearest0.xy + m * in.nearest0.zw + (in.nearest1.xy + m * in.nearest1.zw) / in.nearest2.xy;
    return in.nearest2.zw + in.nearest3.xy * t;
}
)";

struct ClearParamsUniforms
{
    float clearColor[4];
    float clearDepth;
    float padding[3];
};

const char *GetWgslTextureComponentTypeFromGlComponent(GLenum componentType)
{
    if (componentType == GL_INT)
    {
        return "i32";
    }
    if (componentType == GL_UNSIGNED_INT)
    {
        return "u32";
    }

    return "f32";
}

const char *GetWgslTextureComponentTypeFromFormat(const angle::Format &format)
{
    return GetWgslTextureComponentTypeFromGlComponent(format.componentType);
}

// 32-bit float formats can only be filtered with the float32-filterable feature.
bool IsUnfilterableFloatFormat(const angle::Format &format)
{
    return format.componentType == GL_FLOAT && format.redBits == 32;
}

WGPUTextureSampleType GetWgpuTextureSampleType(GLenum componentType)
{
    switch (componentType)
    {
        case GL_INT:
            return WGPUTextureSampleType_Sint;
        case GL_UNSIGNED_INT:
            return WGPUTextureSampleType_Uint;
        default:
            return WGPUTextureSampleType_Float;
    }
}

}  // namespace

namespace webgpu
{

UtilsWgpu::UtilsWgpu() = default;

UtilsWgpu::~UtilsWgpu() = default;

webgpu::ShaderModuleHandle UtilsWgpu::getCopyShaderModule(ContextWgpu *context, const CopyKey &key)
{
    std::stringstream ss;

    const angle::Format &dstFormat = angle::Format::Get(key.dstActualFormatID);

    ss << kVertexMain;
    ss << "@group(0) @binding(0)\n var t_source: texture_2d<"
       << GetWgslTextureComponentTypeFromGlComponent(key.srcComponentType) << ">;\n";

    ss << "@fragment\nfn " << kFragmentEntryPoint
       << "(in: VertexOutput) -> @location(0) "
          "vec4<"
       << GetWgslTextureComponentTypeFromFormat(dstFormat) << "> {\n";
    ss << "    var srcValue = textureLoad(t_source, vec2<i32>(floor(in.texCoord)), 0);\n";
    ss << "    var out_rgb = srcValue.rgb;\n";
    if (key.premultiplyAlpha && !key.unmultiplyAlpha)
    {
        ss << "    out_rgb = out_rgb * srcValue.a;\n";
    }
    else if (key.unmultiplyAlpha && !key.premultiplyAlpha)
    {
        ss << "    if (srcValue.a > 0.0) {\n";
        ss << "        out_rgb = out_rgb / srcValue.a;\n";
        ss << "    }\n";
    }
    ss << "    var out_a = srcValue.a;\n";
    if (!key.dstIntentedFormatHasAlphaBits)
    {
        ss << "    out_a = 1.0;\n";
    }
    ss << "    return vec4<" << GetWgslTextureComponentTypeFromFormat(dstFormat)
       << ">(out_rgb, out_a);\n";
    ss << "}\n";

    if (kLogShaders)
    {
        ANGLE_LOG(INFO) << ss.str();
    }

    return getShaderModule(context, ss.str());
}

webgpu::ShaderModuleHandle UtilsWgpu::getBlitShaderModule(ContextWgpu *context, const BlitKey &key)
{
    std::stringstream ss;

    const angle::Format &dstFormat = angle::Format::Get(key.dstActualFormatID);

    ss << kBlitVertexMain;
    ss << "@group(0) @binding(0)\n var t_source: texture_2d<"
       << GetWgslTextureComponentTypeFromGlComponent(key.srcComponentType) << ">;\n";
    ss << "@group(0) @binding(1)\n var t_sampler: sampler;\n";

    ss << "@fragment\nfn " << kFragmentEntryPoint
       << "(in: VertexOutput) -> @location(0) "
          "vec4<"
       << GetWgslTextureComponentTypeFromFormat(dstFormat) << "> {\n";
    // The source coordinate of the pixel center, in texels.
    ss << "    let texCoord = in.texOffset + in.position.xy * in.texScale;\n";

    const bool srcIsInteger =
        key.srcComponentType == GL_INT || key.srcComponentType == GL_UNSIGNED_INT;
    if (key.filter == GL_LINEAR && !srcIsInteger && !key.srcUnfilterable)
    {
        ss << "    let size = vec2<f32>(textureDimensions(t_source));\n";
        ss << "    var srcValue = textureSampleLevel(t_source, t_sampler, texCoord / size, "
              "0.0);\n";
    }
    else if (key.filter == GL_LINEAR && !srcIsInteger)
    {
        // Bilinear filtering in the shader, with the texels clamped to the edge.
        ss << "    let maxCoord = vec2<i32>(textureDimensions(t_source)) - vec2<i32>(1);\n";
        ss << "    let p = texCoord - vec2<f32>(0.5);\n";
        ss << "    let f = fract(p);\n";
        ss << "    let c0 = clamp(vec2<i32>(floor(p)), vec2<i32>(0), maxCoord);\n";
        ss << "    let c1 = clamp(vec2<i32>(floor(p)) + vec2<i32>(1), vec2<i32>(0), maxCoord);\n";
        ss << "    let top = mix(textureLoad(t_source, c0, 0), textureLoad(t_source, "
              "vec2<i32>(c1.x, c0.y), 0), f.x);\n";
        ss << "    let bottom = mix(textureLoad(t_source, vec2<i32>(c0.x, c1.y), 0), "
              "textureLoad(t_source, c1, 0), f.x);\n";
        ss << "    var srcValue = mix(top, bottom, f.y);\n";
    }
    else
    {
        ss << "    let maxCoord = vec2<i32>(textureDimensions(t_source)) - vec2<i32>(1);\n";
        ss << "    let coord = clamp(nearestTexel(in), vec2<i32>(0), maxCoord);\n";
        ss << "    var srcValue = textureLoad(t_source, coord, 0);\n";
    }

    // Formats without alpha read as an alpha of one, also when the actual format stores alpha,
    // and store an alpha of one.
    ss << "    var out_rgb = srcValue.rgb;\n";
    ss << "    var out_a = srcValue.a;\n";
    if (!key.srcIntendedFormatHasAlphaBits || !key.dstIntentedFormatHasAlphaBits)
    {
        ss << "    out_a = " << GetWgslTextureComponentTypeFromGlComponent(key.srcComponentType)
           << "(1);\n";
    }
    ss << "    return vec4<" << GetWgslTextureComponentTypeFromFormat(dstFormat)
       << ">(out_rgb, out_a);\n";
    ss << "}\n";

    if (kLogShaders)
    {
        ANGLE_LOG(INFO) << ss.str();
    }

    return getShaderModule(context, ss.str());
}

webgpu::ShaderModuleHandle UtilsWgpu::getShaderModule(ContextWgpu *context,
                                                      const std::string &shaderSource)
{
    WGPUShaderSourceWGSL wgslDesc = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgslDesc.code                 = {shaderSource.c_str(), shaderSource.length()};

    WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderModuleDescriptor.nextInChain                = &wgslDesc.chain;

    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    return webgpu::ShaderModuleHandle::Acquire(
        wgpu, wgpu->deviceCreateShaderModule(context->getDevice().get(), &shaderModuleDescriptor));
}

angle::Result UtilsWgpu::createPipeline(ContextWgpu *context,
                                        angle::FormatID dstFormatID,
                                        const webgpu::ShaderModuleHandle &shader,
                                        webgpu::BindGroupLayoutHandle bindGroupLayout,
                                        bool blitVertexLayout,
                                        CachedPipeline *cachedPipelineOut)
{
    webgpu::ShaderModuleHandle shaderModule = shader;

    WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.primitive.topology           = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.multisample.count            = 1;

    WGPUVertexAttribute attributes[7];
    for (uint32_t i = 0; i < 7; ++i)
    {
        attributes[i]        = WGPU_VERTEX_ATTRIBUTE_INIT;
        attributes[i].format = i < 3 ? WGPUVertexFormat_Float32x2 : WGPUVertexFormat_Sint32x4;
        attributes[i].shaderLocation = i;
    }

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.attributes             = attributes;
    if (blitVertexLayout)
    {
        attributes[0].offset              = offsetof(BlitVertex, position);
        attributes[1].offset              = offsetof(BlitVertex, texScale);
        attributes[2].offset              = offsetof(BlitVertex, texOffset);
        for (uint32_t i = 0; i < 4; ++i)
        {
            attributes[3 + i].offset = offsetof(BlitVertex, nearest) + i * sizeof(int32_t[4]);
        }
        vertexBufferLayout.arrayStride    = sizeof(BlitVertex);
        vertexBufferLayout.attributeCount = 7;
    }
    else
    {
        attributes[0].offset              = offsetof(CopyVertex, position);
        attributes[1].offset              = offsetof(CopyVertex, texCoord);
        vertexBufferLayout.arrayStride    = sizeof(CopyVertex);
        vertexBufferLayout.attributeCount = 2;
    }

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers     = &vertexBufferLayout;
    pipelineDesc.vertex.module      = shaderModule.get();
    pipelineDesc.vertex.entryPoint  = {kVertexEntryPoint, sizeof(kVertexEntryPoint) - 1};

    WGPUFragmentState fragmentState       = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module                  = shaderModule.get();
    fragmentState.entryPoint              = {kFragmentEntryPoint, sizeof(kFragmentEntryPoint) - 1};
    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;

    colorTargetState.format   = webgpu::GetWgpuTextureFormatFromFormatID(dstFormatID);
    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTargetState;

    pipelineDesc.fragment     = &fragmentState;

    WGPUPipelineLayoutDescriptor plDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    plDesc.bindGroupLayoutCount         = 1;
    plDesc.bindGroupLayouts             = &bindGroupLayout.get();

    WGPUDevice device         = context->getDevice().get();
    const DawnProcTable *wgpu = webgpu::GetProcs(context);

    webgpu::PipelineLayoutHandle pipelineLayout;
    pipelineLayout = webgpu::PipelineLayoutHandle::Acquire(
        wgpu, wgpu->deviceCreatePipelineLayout(device, &plDesc));
    pipelineDesc.layout = pipelineLayout.get();

    cachedPipelineOut->pipeline = webgpu::RenderPipelineHandle::Acquire(
        wgpu, wgpu->deviceCreateRenderPipeline(device, &pipelineDesc));
    cachedPipelineOut->bindGroupLayout = std::move(bindGroupLayout);

    return angle::Result::Continue;
}

angle::Result UtilsWgpu::getCopyPipeline(ContextWgpu *context,
                                         GLenum srcComponentType,
                                         angle::FormatID dstFormatID,
                                         const webgpu::ShaderModuleHandle &shader,
                                         CachedPipeline *cachedPipelineOut)
{
    WGPUBindGroupLayoutEntry bglEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    bglEntry.binding                  = 0;
    bglEntry.visibility               = WGPUShaderStage_Fragment;
    bglEntry.texture.viewDimension    = WGPUTextureViewDimension_2D;
    bglEntry.texture.sampleType       = GetWgpuTextureSampleType(srcComponentType);

    WGPUBindGroupLayoutDescriptor bglDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bglDesc.entryCount                    = 1;
    bglDesc.entries                       = &bglEntry;

    WGPUDevice device         = context->getDevice().get();
    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    webgpu::BindGroupLayoutHandle bindGroupLayout;
    bindGroupLayout = webgpu::BindGroupLayoutHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroupLayout(device, &bglDesc));

    return createPipeline(context, dstFormatID, shader, std::move(bindGroupLayout),
                          /*blitVertexLayout=*/false, cachedPipelineOut);
}

angle::Result UtilsWgpu::getBlitPipeline(ContextWgpu *context,
                                         const BlitKey &key,
                                         const CachedPipeline **cachedPipelineOut)
{
    BlitPipelineCache::iterator it = mBlitPipelineCache.find(key);
    if (it != mBlitPipelineCache.end())
    {
        *cachedPipelineOut = &it->second;
        return angle::Result::Continue;
    }

    WGPUBindGroupLayoutEntry bglEntries[2] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
                                              WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    bglEntries[0].binding                  = 0;
    bglEntries[0].visibility               = WGPUShaderStage_Fragment;
    bglEntries[0].texture.viewDimension    = WGPUTextureViewDimension_2D;
    bglEntries[0].texture.sampleType       = GetWgpuTextureSampleType(key.srcComponentType);
    if (key.srcUnfilterable)
    {
        bglEntries[0].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    }

    bglEntries[1].binding      = 1;
    bglEntries[1].visibility   = WGPUShaderStage_Fragment;
    bglEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;
    if (key.srcComponentType == GL_INT || key.srcComponentType == GL_UNSIGNED_INT ||
        key.srcUnfilterable)
    {
        bglEntries[1].sampler.type = WGPUSamplerBindingType_NonFiltering;
    }

    WGPUBindGroupLayoutDescriptor bglDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bglDesc.entryCount                    = 2;
    bglDesc.entries                       = bglEntries;

    WGPUDevice device         = context->getDevice().get();
    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    webgpu::BindGroupLayoutHandle bindGroupLayout;
    bindGroupLayout = webgpu::BindGroupLayoutHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroupLayout(device, &bglDesc));

    webgpu::ShaderModuleHandle shaderModule = getBlitShaderModule(context, key);
    CachedPipeline newCachedPipeline;
    ANGLE_TRY(createPipeline(context, key.dstActualFormatID, shaderModule,
                             std::move(bindGroupLayout), /*blitVertexLayout=*/true,
                             &newCachedPipeline));
    std::pair<BlitPipelineCache::iterator, bool> inserted =
        mBlitPipelineCache.emplace(key, std::move(newCachedPipeline));
    *cachedPipelineOut = &inserted.first->second;

    return angle::Result::Continue;
}

angle::Result UtilsWgpu::getClearPipeline(ContextWgpu *context,
                                          const ClearPipelineKey &key,
                                          const CachedPipeline **cachedPipelineOut)
{
    ClearPipelineCache::iterator it = mClearPipelineCache.find(key);
    if (it != mClearPipelineCache.end())
    {
        *cachedPipelineOut = &it->second;
        return angle::Result::Continue;
    }

    webgpu::ShaderModuleHandle shaderModule = getClearShaderModule(context, key);

    WGPUBindGroupLayoutEntry bglEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    bglEntry.binding                  = 0;
    bglEntry.visibility               = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bglEntry.buffer.type              = WGPUBufferBindingType_Uniform;
    bglEntry.buffer.minBindingSize    = sizeof(ClearParamsUniforms);
    bglEntry.texture.sampleType       = WGPUTextureSampleType_BindingNotUsed;
    bglEntry.sampler.type             = WGPUSamplerBindingType_BindingNotUsed;
    bglEntry.storageTexture.access    = WGPUStorageTextureAccess_BindingNotUsed;

    WGPUBindGroupLayoutDescriptor bglDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bglDesc.entryCount                    = 1;
    bglDesc.entries                       = &bglEntry;

    WGPUDepthStencilState depthStencilState     = WGPU_DEPTH_STENCIL_STATE_INIT;
    WGPUDepthStencilState *depthStencilStatePtr = nullptr;
    if (key.depthStencilFormat.has_value())
    {
        depthStencilStatePtr = &depthStencilState;

        depthStencilState.format =
            webgpu::GetWgpuTextureFormatFromFormatID(key.depthStencilFormat.value());

        // Enable depth writing if clearing depth. The vertex shader will set the depth value.
        depthStencilState.depthWriteEnabled = static_cast<WGPUOptionalBool>(key.clearDepth);
        depthStencilState.depthCompare      = WGPUCompareFunction_Always;

        if (key.clearStencil)
        {
            depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
            depthStencilState.stencilBack.compare  = WGPUCompareFunction_Always;
            // Defaults to "keep", set it "replace" in order to replace the stencil value if
            // clearing stencil.
            depthStencilState.stencilFront.passOp = WGPUStencilOperation_Replace;
            depthStencilState.stencilBack.passOp  = WGPUStencilOperation_Replace;

            depthStencilState.stencilWriteMask = key.stencilWriteMask.value();
        }
    }

    CachedPipeline newPipeline;

    WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.primitive.topology           = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.multisample.count            = 1;

    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers     = nullptr;
    pipelineDesc.vertex.module      = shaderModule.get();
    pipelineDesc.vertex.entryPoint  = {kVertexEntryPoint, sizeof(kVertexEntryPoint) - 1};

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module            = shaderModule.get();
    fragmentState.entryPoint        = {kFragmentEntryPoint, sizeof(kFragmentEntryPoint) - 1};

    angle::FixedVector<WGPUColorTargetState, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS>
        wgpuColorTargetStates;
    for (size_t i = 0; i < key.actualColorFormats.size(); i++)
    {
        WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
        colorTargetState.format =
            webgpu::GetWgpuTextureFormatFromFormatID(key.actualColorFormats[i]);
        colorTargetState.writeMask = key.colorMasks[i];
        wgpuColorTargetStates.push_back(colorTargetState);
    }
    fragmentState.targetCount = wgpuColorTargetStates.size();
    fragmentState.targets     = wgpuColorTargetStates.data();

    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.depthStencil = depthStencilStatePtr;

    WGPUDevice device         = context->getDevice().get();
    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    webgpu::BindGroupLayoutHandle bindGroupLayout;
    bindGroupLayout = webgpu::BindGroupLayoutHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroupLayout(device, &bglDesc));

    WGPUPipelineLayoutDescriptor plDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    plDesc.bindGroupLayoutCount         = 1;
    plDesc.bindGroupLayouts             = &bindGroupLayout.get();
    webgpu::PipelineLayoutHandle pipelineLayout;
    pipelineLayout = webgpu::PipelineLayoutHandle::Acquire(
        wgpu, wgpu->deviceCreatePipelineLayout(device, &plDesc));
    pipelineDesc.layout = pipelineLayout.get();

    newPipeline.pipeline = webgpu::RenderPipelineHandle::Acquire(
        wgpu, wgpu->deviceCreateRenderPipeline(device, &pipelineDesc));
    newPipeline.bindGroupLayout = std::move(bindGroupLayout);

    std::pair<ClearPipelineCache::iterator, bool> inserted =
        mClearPipelineCache.emplace(key, std::move(newPipeline));

    *cachedPipelineOut = &inserted.first->second;

    return angle::Result::Continue;
}

webgpu::ShaderModuleHandle UtilsWgpu::getClearShaderModule(ContextWgpu *context,
                                                           const ClearPipelineKey &key)
{
    std::stringstream ss;

    const bool hasColorOutputs = key.actualColorFormats.size() != 0;

    ss << R"(struct ClearUniforms {
    color : vec4<f32>,
    depth : f32,
};

@group(0) @binding(0)
var<uniform> clearUniforms : ClearUniforms;

// Vertex shader just draws the whole screen with one triangle
@vertex
fn vs_main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4<f32> {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    return vec4<f32>(pos[vertex_index], clearUniforms.depth, 1.0);
})";

    if (hasColorOutputs)
    {
        ss << "struct Outputs {\n";
        for (size_t i = 0; i < key.actualColorFormats.size(); i++)
        {
            const angle::Format &dstColorFormat = angle::Format::Get(key.actualColorFormats[i]);
            ss << "  @location(" << i << ") output" << i << " : vec4<"
               << GetWgslTextureComponentTypeFromFormat(dstColorFormat) << ">,\n";
        }
        ss << "};\n";

        ss << "@fragment\nfn " << kFragmentEntryPoint
           << "(@builtin(position) frag_coord: vec4<f32>) -> Outputs {\n";

        ss << "    return Outputs(";
        for (size_t i = 0; i < key.actualColorFormats.size(); i++)
        {
            if (i != 0)
            {
                ss << ", ";
            }
            const angle::Format &dstColorFormat = angle::Format::Get(key.actualColorFormats[i]);

            // If the intended format does NOT have alpha bits, but the actual format DOES have
            // alpha bits, set the alpha bits in the actual format to be 1.
            if (!key.intendedColorFormatHasAlphaBits[i] && dstColorFormat.alphaBits != 0)
            {
                // TODO(anglebug.com/474131922):
                // dEQP-GLES2.functional.fbo.render.stencil_clear.tex2d_rgb_stencil_index8 is
                // failing and so is
                // dEQP-GLES2.functional.fbo.render.stencil_clear.rbo_rgb565_stencil_index8.
                ss << "vec4<" << GetWgslTextureComponentTypeFromFormat(dstColorFormat)
                   << ">(bitcast<vec3<" << GetWgslTextureComponentTypeFromFormat(dstColorFormat)
                   << ">>(clearUniforms.color.rgb), 1)";
            }
            else
            {
                // Take the channel's value directly from the uniform.
                // The output may have a component type that isn't f32, but the uniform will always
                // be f32. Just bitcast like C++ does.
                ss << "bitcast<vec4<" << GetWgslTextureComponentTypeFromFormat(dstColorFormat)
                   << ">>(clearUniforms.color)";
            }
        }
        ss << ");\n";
        ss << "}\n";
    }
    else
    {
        // Empty fragment shader if there are no color outputs.
        ss << "@fragment\n fn " << kFragmentEntryPoint << "() {}\n";
    }

    if (kLogShaders)
    {
        ANGLE_LOG(INFO) << ss.str();
    }

    return getShaderModule(context, ss.str());
}

angle::Result UtilsWgpu::copyImage(ContextWgpu *context,
                                   webgpu::TextureViewHandle src,
                                   webgpu::TextureViewHandle dst,
                                   const gl::Rectangle &sourceArea,
                                   const gl::Offset &destOffset,
                                   const WGPUExtent3D &srcSize,
                                   const WGPUExtent3D &dstSize,
                                   bool premultiplyAlpha,
                                   bool unmultiplyAlpha,
                                   bool srcFlipY,
                                   bool dstFlipY,
                                   const angle::Format &srcFormat,
                                   angle::FormatID dstIntendedFormatID,
                                   angle::FormatID dstActualFormatID,
                                   const gl::Rectangle *scissor)
{
    const DawnProcTable *wgpu              = webgpu::GetProcs(context);
    const angle::Format &dstIntendedFormat = angle::Format::Get(dstIntendedFormatID);
    CopyKey key                            = {};
    key.op                                 = WgpuPipelineOp::ImageCopy;
    key.srcComponentType                   = srcFormat.componentType;
    key.dstActualFormatID                  = dstActualFormatID;
    key.dstIntentedFormatHasAlphaBits      = dstIntendedFormat.alphaBits != 0;
    key.premultiplyAlpha                   = premultiplyAlpha;
    key.unmultiplyAlpha                    = unmultiplyAlpha;
    key.srcFlipY                           = srcFlipY;
    key.dstFlipY                           = dstFlipY;

    const CachedPipeline *cachedPipeline = nullptr;
    CopyPipelineCache::iterator it       = mCopyPipelineCache.find(key);
    if (it != mCopyPipelineCache.end())
    {
        cachedPipeline = &it->second;
    }
    else
    {
        webgpu::ShaderModuleHandle shaderModule = getCopyShaderModule(context, key);
        CachedPipeline newCachedPipeline;
        ANGLE_TRY(getCopyPipeline(context, key.srcComponentType, key.dstActualFormatID,
                                  shaderModule, &newCachedPipeline));
        std::pair<CopyPipelineCache::iterator, bool> inserted =
            mCopyPipelineCache.emplace(key, std::move(newCachedPipeline));
        cachedPipeline = &inserted.first->second;
    }

    WGPUBindGroupDescriptor bgDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    WGPUBindGroupEntry bgEntry     = WGPU_BIND_GROUP_ENTRY_INIT;
    bgEntry.binding                = 0;
    bgEntry.textureView            = src.get();
    bgDesc.entryCount              = 1;
    bgDesc.entries                 = &bgEntry;
    bgDesc.layout                  = cachedPipeline->bindGroupLayout.get();

    webgpu::BindGroupHandle bindGroup;
    bindGroup = webgpu::BindGroupHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroup(context->getDevice().get(), &bgDesc));

    gl::Rectangle destArea(destOffset.x, destOffset.y, sourceArea.width, sourceArea.height);

    return drawQuad(context, WgpuPipelineOp::ImageCopy, dst, dstSize, cachedPipeline, bindGroup,
                    destArea, sourceArea, srcSize, srcFlipY, dstFlipY, false, scissor, 0);
}

angle::Result UtilsWgpu::blitColor(ContextWgpu *context,
                                   const BlitParams &params,
                                   const webgpu::TextureViewHandle &src,
                                   uint32_t srcSamples,
                                   angle::FormatID srcIntendedFormatID,
                                   angle::FormatID srcActualFormatID,
                                   const webgpu::TextureViewHandle &dst,
                                   angle::FormatID dstIntendedFormatID,
                                   angle::FormatID dstActualFormatID)
{
    const DawnProcTable *wgpu              = webgpu::GetProcs(context);
    const angle::Format &srcActualFormat   = angle::Format::Get(srcActualFormatID);
    const angle::Format &srcIntendedFormat = angle::Format::Get(srcIntendedFormatID);
    const angle::Format &dstIntendedFormat = angle::Format::Get(dstIntendedFormatID);
    BlitKey key                            = {};

    key.op                            = WgpuPipelineOp::ColorBlit;
    key.srcComponentType              = srcActualFormat.componentType;
    key.srcUnfilterable               = IsUnfilterableFloatFormat(srcActualFormat);
    key.dstActualFormatID             = dstActualFormatID;
    key.srcIntendedFormatHasAlphaBits = srcIntendedFormat.alphaBits != 0;
    key.dstIntentedFormatHasAlphaBits = dstIntendedFormat.alphaBits != 0;
    key.srcSamples                    = srcSamples;
    key.filter                        = params.filter;

    const CachedPipeline *cachedPipeline = nullptr;
    ANGLE_TRY(getBlitPipeline(context, key, &cachedPipeline));

    WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
    samplerDesc.addressModeU          = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV          = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW          = WGPUAddressMode_ClampToEdge;
    samplerDesc.maxAnisotropy         = 1;
    if (params.filter == GL_LINEAR && !key.srcUnfilterable)
    {
        samplerDesc.magFilter = WGPUFilterMode_Linear;
        samplerDesc.minFilter = WGPUFilterMode_Linear;
    }
    else
    {
        samplerDesc.magFilter = WGPUFilterMode_Nearest;
        samplerDesc.minFilter = WGPUFilterMode_Nearest;
    }

    webgpu::SamplerHandle sampler = webgpu::SamplerHandle::Acquire(
        wgpu, wgpu->deviceCreateSampler(context->getDevice().get(), &samplerDesc));

    WGPUBindGroupDescriptor bgDesc  = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    WGPUBindGroupEntry bgEntries[2] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    bgEntries[0].binding            = 0;
    bgEntries[0].textureView        = src.get();
    bgEntries[1].binding            = 1;
    bgEntries[1].sampler            = sampler.get();

    bgDesc.entryCount = 2;
    bgDesc.entries    = bgEntries;
    bgDesc.layout     = cachedPipeline->bindGroupLayout.get();

    webgpu::BindGroupHandle bindGroup;
    bindGroup = webgpu::BindGroupHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroup(context->getDevice().get(), &bgDesc));

    webgpu::PackedRenderPassDescriptor renderPassDesc;
    webgpu::PackedRenderPassColorAttachment colorAttachment;
    colorAttachment.view       = dst;
    colorAttachment.loadOp     = WGPULoadOp_Load;
    colorAttachment.storeOp    = WGPUStoreOp_Store;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    renderPassDesc.colorAttachments.push_back(colorAttachment);

    ANGLE_TRY(startBlitRenderPass(context, params, renderPassDesc));

    webgpu::CommandBuffer &commandBuffer = context->getCommandBuffer();
    commandBuffer.setPipeline(cachedPipeline->pipeline);
    commandBuffer.setBindGroup(0, bindGroup);
    commandBuffer.draw(4, 1, 0, 0);

    return context->endRenderPass(webgpu::RenderPassClosureReason::CopyImage);
}

angle::Result UtilsWgpu::clear(ContextWgpu *context, ClearParams params)
{
    if (params.clearColorBuffers.none() && !params.depthStencilTarget)
    {
        return angle::Result::Continue;
    }

    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    ClearPipelineKey key      = {};

    for (size_t enabledDrawBuffer : params.clearColorBuffers)
    {
        ImageHelper *colorImage = (*params.colorTargets)[enabledDrawBuffer]->getImage();
        const angle::Format &dstIntendedFormat =
            angle::Format::Get(colorImage->getIntendedFormatID());
        key.actualColorFormats.push_back(colorImage->getActualFormatID());
        key.intendedColorFormatHasAlphaBits.push_back(dstIntendedFormat.alphaBits != 0);
        // gl::BlendStateExt::PackColorMask matches WGPUColorWriteMask.
        key.colorMasks.push_back(gl::BlendStateExt::ColorMaskStorage::GetValueIndexed(
            enabledDrawBuffer, params.colorMasks));
    }

    if (params.clearDepthValue || params.clearStencilValue)
    {
        key.depthStencilFormat = params.depthStencilTarget->getImage()->getActualFormatID();
        key.clearDepth         = params.clearDepthValue.has_value();
        key.clearStencil       = params.clearStencilValue.has_value();
        key.stencilWriteMask   = params.stencilWriteMask;
    }

    const CachedPipeline *cachedPipeline = nullptr;
    ANGLE_TRY(getClearPipeline(context, key, &cachedPipeline));

    // Upload the clear color and depth clear value to a new GPU buffer for use as a uniform.
    // TODO(anglebug.com/474131922): cache this. Treat like program uniforms and use dynamic offset.
    webgpu::BufferHelper clearParamsUniformBuffer;

    ANGLE_TRY(clearParamsUniformBuffer.initBuffer(
        wgpu, context->getDevice(), sizeof(ClearParamsUniforms),
        WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, webgpu::MapAtCreation::Yes));

    ASSERT(clearParamsUniformBuffer.valid());

    ClearParamsUniforms *bufferData = reinterpret_cast<ClearParamsUniforms *>(
        clearParamsUniformBuffer.getMapWritePointer(0, sizeof(ClearParamsUniforms)));
    ANGLE_UNSAFE_BUFFERS(
        memcpy(&bufferData->clearColor,
               params.clearColorValue.value_or(gl::ColorF(0.0, 0.0, 0.0, 0.0)).data(),
               sizeof(bufferData->clearColor)));
    bufferData->clearDepth = params.clearDepthValue.value_or(0.0);

    ANGLE_TRY(clearParamsUniformBuffer.unmap());

    // Now create the bind group containing the clear params uniform buffer.
    WGPUBindGroupEntry bgEntry = WGPU_BIND_GROUP_ENTRY_INIT;
    bgEntry.binding            = 0;
    bgEntry.buffer             = clearParamsUniformBuffer.getBuffer().get();
    bgEntry.offset             = 0;
    bgEntry.size               = sizeof(ClearParamsUniforms);

    WGPUBindGroupDescriptor bgDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bgDesc.layout                  = cachedPipeline->bindGroupLayout.get();
    bgDesc.entryCount              = 1;
    bgDesc.entries                 = &bgEntry;

    webgpu::BindGroupHandle bindGroup;
    bindGroup = webgpu::BindGroupHandle::Acquire(
        wgpu, wgpu->deviceCreateBindGroup(context->getDevice().get(), &bgDesc));

    // TODO(anglebug.com/474131922): optimize to use the framebuffer's current render pass if it
    // exists.
    webgpu::PackedRenderPassDescriptor renderPassDesc;

    for (size_t enabledDrawBuffer : params.clearColorBuffers)
    {
        webgpu::PackedRenderPassColorAttachment colorAttachment;
        colorAttachment.view       = (*params.colorTargets)[enabledDrawBuffer]->getTextureView();
        colorAttachment.loadOp     = WGPULoadOp_Load;
        colorAttachment.storeOp    = WGPUStoreOp_Store;
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        renderPassDesc.colorAttachments.push_back(colorAttachment);
    }

    if (params.depthStencilTarget)
    {
        ASSERT(params.clearDepthValue || params.clearStencilValue);

        webgpu::PackedRenderPassDepthStencilAttachment depthStencilAttachment;

        depthStencilAttachment.view = params.depthStencilTarget->getTextureView();

        if (params.clearDepthValue)
        {
            depthStencilAttachment.depthReadOnly = false;
            depthStencilAttachment.depthLoadOp   = WGPULoadOp_Load;
            depthStencilAttachment.depthStoreOp  = WGPUStoreOp_Store;
        }
        else
        {
            depthStencilAttachment.depthReadOnly = true;
        }

        if (params.clearStencilValue)
        {
            depthStencilAttachment.stencilReadOnly = false;
            depthStencilAttachment.stencilLoadOp   = WGPULoadOp_Load;

            depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
        }
        else
        {
            depthStencilAttachment.stencilReadOnly = true;
        }

        renderPassDesc.depthStencilAttachment = std::move(depthStencilAttachment);
    }

    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::ClearWithDraw));
    ANGLE_TRY(context->startRenderPass(renderPassDesc));

    webgpu::CommandBuffer &commandBuffer = context->getCommandBuffer();
    commandBuffer.setPipeline(cachedPipeline->pipeline);
    commandBuffer.setBindGroup(0, bindGroup);
    commandBuffer.setViewport(params.clearArea.x, params.clearArea.y, params.clearArea.width,
                              params.clearArea.height, /*minDepth=*/0, /*maxDepth=*/1);
    commandBuffer.setScissorRect(params.clearArea.x, params.clearArea.y, params.clearArea.width,
                                 params.clearArea.height);
    if (params.clearStencilValue.has_value())
    {
        commandBuffer.setStencilReference(params.clearStencilValue.value());
    }
    commandBuffer.draw(3, 1, 0, 0);

    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::ClearWithDraw));

    return angle::Result::Continue;
}

angle::Result UtilsWgpu::drawQuad(ContextWgpu *context,
                                  WgpuPipelineOp op,
                                  webgpu::TextureViewHandle dst,
                                  const WGPUExtent3D &dstSize,
                                  const CachedPipeline *cachedPipeline,
                                  const webgpu::BindGroupHandle &bindGroup,
                                  const gl::Rectangle &destArea,
                                  const gl::Rectangle &sourceArea,
                                  const WGPUExtent3D &srcSize,
                                  bool srcFlipY,
                                  bool dstFlipY,
                                  bool srcTextureCoordsNormalized,
                                  const gl::Rectangle *scissor,
                                  uint32_t stencilReference)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(context);

    webgpu::PackedRenderPassDescriptor renderPassDesc;
    webgpu::PackedRenderPassColorAttachment colorAttachment;
    colorAttachment.view       = dst;
    colorAttachment.loadOp     = WGPULoadOp_Load;
    colorAttachment.storeOp    = WGPUStoreOp_Store;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    renderPassDesc.colorAttachments.push_back(colorAttachment);

    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::CopyImage));
    ANGLE_TRY(context->startRenderPass(renderPassDesc));

    if (scissor)
    {
        gl::Rectangle clippedScissor;
        const gl::Rectangle framebufferRect(0, 0, dstSize.width, dstSize.height);
        if (ClipRectangle(*scissor, framebufferRect, &clippedScissor))
        {
            if (dstFlipY)
            {
                clippedScissor.y = dstSize.height - clippedScissor.y - clippedScissor.height;
            }
            context->getCommandBuffer().setScissorRect(clippedScissor.x, clippedScissor.y,
                                                       clippedScissor.width, clippedScissor.height);
        }
        else
        {
            context->getCommandBuffer().setScissorRect(0, 0, 0, 0);
        }
    }
    else
    {
        context->getCommandBuffer().setScissorRect(0, 0, dstSize.width, dstSize.height);
    }

    float dstX1 = destArea.x;
    float dstY1 = destArea.y;
    float dstX2 = destArea.x + destArea.width;
    float dstY2 = destArea.y + destArea.height;

    float srcX1 = sourceArea.x;
    float srcY1 = sourceArea.y;
    float srcX2 = sourceArea.x + sourceArea.width;
    float srcY2 = sourceArea.y + sourceArea.height;

    // WebGPU's texture coordinate system has (0,0) in the top-left corner.
    // Normalized device coordinates (NDC) has y pointing up.
    // The viewport transform in WGPU maps NDC Y=1 to window Y=0.

    // Surfaces (dstFlipY=true) are presented flipped by ANGLE, so they should be drawn
    // right-side up in WGPU (GL bottom -> NDC bottom).
    // FBOs (dstFlipY=false) are stored upside-down in WGPU memory, so they should be
    // drawn flipped in WGPU (GL bottom -> NDC top).
    float dstNormX1 = dstX1 / dstSize.width * 2.0f - 1.0f;
    float dstNormX2 = dstX2 / dstSize.width * 2.0f - 1.0f;
    float dstNormY1 = dstY1 / dstSize.height * 2.0f - 1.0f;
    float dstNormY2 = dstY2 / dstSize.height * 2.0f - 1.0f;

    if (!dstFlipY)
    {
        dstNormY1 = -dstNormY1;
        dstNormY2 = -dstNormY2;
    }

    float srcU1, srcV1, srcU2, srcV2;
    if (srcTextureCoordsNormalized)
    {
        // Normalized coordinates for float textures
        srcU1 = srcX1 / srcSize.width;
        srcV1 = srcY1 / srcSize.height;
        srcU2 = srcX2 / srcSize.width;
        srcV2 = srcY2 / srcSize.height;
    }
    else
    {
        srcU1 = srcX1;
        srcV1 = srcY1;
        srcU2 = srcX2;
        srcV2 = srcY2;
    }

    if (srcFlipY)
    {
        if (srcTextureCoordsNormalized)
        {
            srcV1 = 1.0f - srcV1;
            srcV2 = 1.0f - srcV2;
        }
        else
        {
            srcV1 = static_cast<float>(srcSize.height) - srcV1;
            srcV2 = static_cast<float>(srcSize.height) - srcV2;
        }
    }

    webgpu::CommandBuffer &commandBuffer = context->getCommandBuffer();
    commandBuffer.setPipeline(cachedPipeline->pipeline);
    commandBuffer.setBindGroup(0, bindGroup);
    CopyVertex vertices[4] = {
        {{dstNormX1, dstNormY1}, {srcU1, srcV1}},
        {{dstNormX2, dstNormY1}, {srcU2, srcV1}},
        {{dstNormX1, dstNormY2}, {srcU1, srcV2}},
        {{dstNormX2, dstNormY2}, {srcU2, srcV2}},
    };

    WGPUBufferDescriptor bufferDesc   = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDesc.size                   = sizeof(vertices);
    bufferDesc.usage                  = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    webgpu::BufferHandle vertexBuffer = webgpu::BufferHandle::Acquire(
        wgpu, wgpu->deviceCreateBuffer(context->getDevice().get(), &bufferDesc));

    wgpu->queueWriteBuffer(context->getQueue().get(), vertexBuffer.get(), 0, vertices,
                           sizeof(vertices));
    commandBuffer.setVertexBuffer(0, vertexBuffer, 0, sizeof(vertices));

    commandBuffer.draw(4, 1, 0, 0);

    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::CopyImage));

    return angle::Result::Continue;
}

angle::Result UtilsWgpu::startBlitRenderPass(
    ContextWgpu *context,
    const BlitParams &params,
    const webgpu::PackedRenderPassDescriptor &renderPassDesc)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(context);

    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::CopyImage));
    ANGLE_TRY(context->startRenderPass(renderPassDesc));

    // Normalized device coordinates have y up, and y = 1 is texel row 0 of the destination.
    const float dstWidth  = static_cast<float>(params.dstSize.width);
    const float dstHeight = static_cast<float>(params.dstSize.height);
    const float x0        = params.dstArea.x0() / dstWidth * 2.0f - 1.0f;
    const float x1        = params.dstArea.x1() / dstWidth * 2.0f - 1.0f;
    const float y0        = 1.0f - params.dstArea.y0() / dstHeight * 2.0f;
    const float y1        = 1.0f - params.dstArea.y1() / dstHeight * 2.0f;

    // The source coordinate, in texels, of the destination pixel position p is
    // offset + p * scale. A reversed source rectangle flips.
    const double scaleX      = static_cast<double>(params.srcArea.width) / params.dstArea.width;
    const double scaleY      = static_cast<double>(params.srcArea.height) / params.dstArea.height;
    const float texScale[2]  = {static_cast<float>(scaleX), static_cast<float>(scaleY)};
    const float texOffset[2] = {static_cast<float>(params.srcArea.x - params.dstArea.x * scaleX),
                                static_cast<float>(params.srcArea.y - params.dstArea.y * scaleY)};

    const BlitParams::NearestAxis &nx = params.nearestX;
    const BlitParams::NearestAxis &ny = params.nearestY;
    const int32_t nearest[4][4]       = {{nx.q0, ny.q0, nx.a, ny.a},
                                         {nx.r0, ny.r0, nx.b, ny.b},
                                         {nx.denominator, ny.denominator, nx.srcBase, ny.srcBase},
                                         {nx.srcSign, ny.srcSign, nx.dstOrigin, ny.dstOrigin}};

    BlitVertex vertices[4];
    const float positions[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
    for (size_t i = 0; i < 4; ++i)
    {
        vertices[i] = {{positions[i][0], positions[i][1]},
                       {texScale[0], texScale[1]},
                       {texOffset[0], texOffset[1]},
                       {}};
        for (size_t row = 0; row < 4; ++row)
        {
            for (size_t column = 0; column < 4; ++column)
            {
                vertices[i].nearest[row][column] = nearest[row][column];
            }
        }
    }

    WGPUBufferDescriptor bufferDesc   = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDesc.size                   = sizeof(vertices);
    bufferDesc.usage                  = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    webgpu::BufferHandle vertexBuffer = webgpu::BufferHandle::Acquire(
        wgpu, wgpu->deviceCreateBuffer(context->getDevice().get(), &bufferDesc));
    wgpu->queueWriteBuffer(context->getQueue().get(), vertexBuffer.get(), 0, vertices,
                           sizeof(vertices));

    webgpu::CommandBuffer &commandBuffer = context->getCommandBuffer();
    commandBuffer.setVertexBuffer(0, vertexBuffer, 0, sizeof(vertices));
    commandBuffer.setViewport(0, 0, dstWidth, dstHeight, /*minDepth=*/0, /*maxDepth=*/1);
    commandBuffer.setScissorRect(params.dstScissor.x, params.dstScissor.y, params.dstScissor.width,
                                 params.dstScissor.height);

    return angle::Result::Continue;
}

}  // namespace webgpu
}  // namespace rx
