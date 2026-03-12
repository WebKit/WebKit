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

constexpr char kVertexMain[] = R"(@vertex
fn vs_main(@builtin(vertex_index) vertex_index : u32) -> @builtin(position) vec4<f32> {
    var pos = array<vec2<f32>, 4>(
        vec2<f32>(-1.0, 1.0),
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(1.0, 1.0),
        vec2<f32>(1.0, -1.0)
    );
    return vec4<f32>(pos[vertex_index], 0.0, 1.0);
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

webgpu::ShaderModuleHandle UtilsWgpu::getShaderModule(ContextWgpu *context, const PipelineKey &key)
{
    std::stringstream ss;
    if (key.op == WgpuPipelineOp::ImageCopy)
    {
        const angle::Format &dstFormat = angle::Format::Get(key.dstActualFormatID);

        ss << kVertexMain;
        ss << "@group(0) @binding(0)\n var t_source: texture_2d<"
           << GetWgslTextureComponentTypeFromGlComponent(key.srcComponentType) << ">;\n";

        ss << "@fragment\nfn " << kFragmentEntryPoint
           << "(@builtin(position) frag_coord: vec4<f32>) -> @location(0) "
              "vec4<"
           << GetWgslTextureComponentTypeFromFormat(dstFormat) << "> {\n";

        ss << "    var texel_coords: vec2<i32> = vec2<i32>(floor(frag_coord.xy));\n";
        ss << "    var srcValue = textureLoad(t_source, texel_coords, 0);\n";
        if (!key.dstIntentedFormatHasAlphaBits)
        {
            ss << "    srcValue.a = 1;\n";
        }
        ss << "    return vec4<" << GetWgslTextureComponentTypeFromFormat(dstFormat)
           << ">(srcValue);\n";
        ss << "}\n";
    }
    else
    {
        UNREACHABLE();
    }

    std::string shaderSource      = ss.str();
    WGPUShaderSourceWGSL wgslDesc = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgslDesc.code                 = {shaderSource.c_str(), shaderSource.length()};

    WGPUShaderModuleDescriptor shaderModuleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shaderModuleDescriptor.nextInChain                = &wgslDesc.chain;

    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    return webgpu::ShaderModuleHandle::Acquire(
        wgpu, wgpu->deviceCreateShaderModule(context->getDevice().get(), &shaderModuleDescriptor));
}

angle::Result UtilsWgpu::getPipeline(ContextWgpu *context,
                                     const PipelineKey &key,
                                     const CachedPipeline **cachedPipelineOut)
{
    auto it = mPipelineCache.find(key);
    if (it != mPipelineCache.end())
    {
        *cachedPipelineOut = &it->second;
        return angle::Result::Continue;
    }

    webgpu::ShaderModuleHandle shaderModule = getShaderModule(context, key);

    WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.primitive.topology           = WGPUPrimitiveTopology_TriangleStrip;
    pipelineDesc.multisample.count            = 1;

    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers     = nullptr;
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
        bglEntry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
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

    CachedPipeline newCachedPipeline;
    newCachedPipeline.pipeline = webgpu::RenderPipelineHandle::Acquire(
        wgpu, wgpu->deviceCreateRenderPipeline(device, &pipelineDesc));
    newCachedPipeline.bindGroupLayout = std::move(bindGroupLayout);

    auto inserted = mPipelineCache.emplace(key, std::move(newCachedPipeline));

    *cachedPipelineOut = &inserted.first->second;

    return angle::Result::Continue;
}

angle::Result UtilsWgpu::copyImage(ContextWgpu *context,
                                   webgpu::TextureViewHandle src,
                                   webgpu::TextureViewHandle dst,
                                   const WGPUExtent3D &size,
                                   bool flipY,
                                   const angle::Format &srcFormat,
                                   angle::FormatID dstIntendedFormatID,
                                   angle::FormatID dstActualFormatID)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(context);
    const angle::Format &dstIntendedFormat = angle::Format::Get(dstIntendedFormatID);
    PipelineKey key           = {};
    key.op                    = WgpuPipelineOp::ImageCopy;
    key.srcComponentType                   = srcFormat.componentType;
    key.dstActualFormatID     = dstActualFormatID;
    key.dstIntentedFormatHasAlphaBits      = dstIntendedFormat.alphaBits != 0;

    const CachedPipeline *cachedPipeline = nullptr;
    ANGLE_TRY(getPipeline(context, key, &cachedPipeline));

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

    webgpu::CommandBuffer &commandBuffer = context->getCommandBuffer();
    commandBuffer.setPipeline(cachedPipeline->pipeline);
    commandBuffer.setBindGroup(0, bindGroup);
    commandBuffer.draw(4, 1, 0, 0);

    ANGLE_TRY(context->endRenderPass(webgpu::RenderPassClosureReason::CopyImage));

    return angle::Result::Continue;
}

}  // namespace webgpu
}  // namespace rx
