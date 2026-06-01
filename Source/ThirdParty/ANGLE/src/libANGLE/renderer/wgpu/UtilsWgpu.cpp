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

constexpr char kCopyVertexMain[] = R"(
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

}  // namespace

namespace webgpu
{

UtilsWgpu::UtilsWgpu() = default;

UtilsWgpu::~UtilsWgpu() = default;

webgpu::ShaderModuleHandle UtilsWgpu::getCopyShaderModule(ContextWgpu *context, const CopyKey &key)
{
    std::stringstream ss;

    const angle::Format &dstFormat = angle::Format::Get(key.dstActualFormatID);

    ss << kCopyVertexMain;
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

angle::Result UtilsWgpu::getCopyPipeline(ContextWgpu *context,
                                         const CopyKey &key,
                                         const webgpu::ShaderModuleHandle &shader,
                                         CachedPipeline *cachedPipelineOut)
{
    webgpu::ShaderModuleHandle shaderModule = shader;

    WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.primitive.topology           = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.multisample.count            = 1;

    WGPUVertexAttribute attributes[2] = {};
    attributes[0].format              = WGPUVertexFormat_Float32x2;
    attributes[0].offset              = offsetof(CopyVertex, position);
    attributes[0].shaderLocation      = 0;
    attributes[1].format              = WGPUVertexFormat_Float32x2;
    attributes[1].offset              = offsetof(CopyVertex, texCoord);
    attributes[1].shaderLocation      = 1;

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride            = sizeof(CopyVertex);
    vertexBufferLayout.attributeCount         = 2;
    vertexBufferLayout.attributes             = attributes;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers     = &vertexBufferLayout;
    pipelineDesc.vertex.module      = shaderModule.get();
    pipelineDesc.vertex.entryPoint  = {kVertexEntryPoint, sizeof(kVertexEntryPoint) - 1};

    WGPUFragmentState fragmentState       = WGPU_FRAGMENT_STATE_INIT;
    fragmentState.module                  = shaderModule.get();
    fragmentState.entryPoint              = {kFragmentEntryPoint, sizeof(kFragmentEntryPoint) - 1};
    WGPUColorTargetState colorTargetState = WGPU_COLOR_TARGET_STATE_INIT;
    colorTargetState.format   = webgpu::GetWgpuTextureFormatFromFormatID(key.dstActualFormatID);
    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTargetState;
    pipelineDesc.fragment     = &fragmentState;

    WGPUBindGroupLayoutEntry bglEntry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    bglEntry.binding                  = 0;
    bglEntry.visibility               = WGPUShaderStage_Fragment;
    bglEntry.texture.viewDimension    = WGPUTextureViewDimension_2D;

    if (key.srcComponentType == GL_INT)
    {
        bglEntry.texture.sampleType = WGPUTextureSampleType_Sint;
    }
    else if (key.srcComponentType == GL_UNSIGNED_INT)
    {
        bglEntry.texture.sampleType = WGPUTextureSampleType_Uint;
    }
    else
    {
        bglEntry.texture.sampleType = WGPUTextureSampleType_Float;
    }

    WGPUBindGroupLayoutDescriptor bglDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bglDesc.entryCount                    = 1;
    bglDesc.entries                       = &bglEntry;

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

    cachedPipelineOut->pipeline = webgpu::RenderPipelineHandle::Acquire(
        wgpu, wgpu->deviceCreateRenderPipeline(device, &pipelineDesc));
    cachedPipelineOut->bindGroupLayout = std::move(bindGroupLayout);

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
                                   angle::FormatID dstActualFormatID)
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
    auto it                              = mCopyPipelineCache.find(key);
    if (it != mCopyPipelineCache.end())
    {
        cachedPipeline = &it->second;
    }
    else
    {
        webgpu::ShaderModuleHandle shaderModule = getCopyShaderModule(context, key);
        CachedPipeline newCachedPipeline;
        ANGLE_TRY(getCopyPipeline(context, key, shaderModule, &newCachedPipeline));
        auto inserted  = mCopyPipelineCache.emplace(key, std::move(newCachedPipeline));
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

    webgpu::PackedRenderPassDescriptor renderPassDesc;
    webgpu::PackedRenderPassColorAttachment colorAttachment;
    colorAttachment.view       = dst;
    colorAttachment.loadOp     = WGPULoadOp_Load;
    colorAttachment.storeOp    = WGPUStoreOp_Store;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    renderPassDesc.colorAttachments.push_back(colorAttachment);
    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::CopyImage));
    ANGLE_TRY(context->startRenderPass(renderPassDesc));

    float dstX1 = destOffset.x;
    float dstY1 = destOffset.y;
    float dstX2 = destOffset.x + sourceArea.width;
    float dstY2 = destOffset.y + sourceArea.height;

    float srcX1 = sourceArea.x;
    float srcY1 = sourceArea.y;
    float srcX2 = sourceArea.x + sourceArea.width;
    float srcY2 = sourceArea.y + sourceArea.height;

    if (srcFlipY != dstFlipY)
    {
        std::swap(srcY1, srcY2);
    }

    // WebGPU's texture coordinate system has (0,0) in the top-left corner.
    // Normalized device coordinates (NDC) has y pointing up.
    // The following vertex positions are in NDC. The viewport is not flipped.
    float dstNormX1 = dstX1 / dstSize.width * 2.0f - 1.0f;
    float dstNormY1 = -(dstY1 / dstSize.height * 2.0f - 1.0f);
    float dstNormX2 = dstX2 / dstSize.width * 2.0f - 1.0f;
    float dstNormY2 = -(dstY2 / dstSize.height * 2.0f - 1.0f);

    webgpu::CommandBuffer &commandBuffer = context->getCommandBuffer();
    commandBuffer.setPipeline(cachedPipeline->pipeline);
    commandBuffer.setBindGroup(0, bindGroup);
    CopyVertex vertices[4] = {
        {{dstNormX1, dstNormY2}, {srcX1, srcY2}},
        {{dstNormX2, dstNormY2}, {srcX2, srcY2}},
        {{dstNormX1, dstNormY1}, {srcX1, srcY1}},
        {{dstNormX2, dstNormY1}, {srcX2, srcY1}},
    };

    WGPUBufferDescriptor bufferDesc   = {};
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

angle::Result UtilsWgpu::getClearPipeline(ContextWgpu *context,
                                          const ClearPipelineKey &key,
                                          const CachedPipeline **cachedPipelineOut)
{
    auto it = mClearPipelineCache.find(key);
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

    auto inserted = mClearPipelineCache.emplace(key, std::move(newPipeline));

    *cachedPipelineOut = &inserted.first->second;

    return angle::Result::Continue;
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

}  // namespace webgpu
}  // namespace rx
