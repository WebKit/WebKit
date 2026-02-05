// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsWgpu.cpp:
//   Helper methods for the WebGPU backend.
//

#include "libANGLE/renderer/wgpu/UtilsWgpu.h"

#include <sstream>
#include "libANGLE/renderer/wgpu/ContextWgpu.h"

namespace rx
{
namespace
{
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

angle::Result UtilsWgpu::getPipeline(ContextWgpu *context,
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
        ANGLE_TRY(getPipeline(context, key, shaderModule, &newCachedPipeline));
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

}  // namespace webgpu
}  // namespace rx
