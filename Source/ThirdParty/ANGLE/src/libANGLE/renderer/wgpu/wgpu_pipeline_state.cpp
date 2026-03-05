//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/wgpu_pipeline_state.h"

#include "common/aligned_memory.h"
#include "common/hash_utils.h"
#include "common/span.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"

namespace rx
{
namespace webgpu
{
// Can pack the index format into 1 bit since it has 2 values and Undefined is not used.
static_assert(static_cast<uint32_t>(WGPUIndexFormat_Uint32) == 2U, "Max WGPUIndexFormat is not 2");
static_assert(static_cast<uint32_t>(WGPUIndexFormat_Undefined) == 0,
              "WGPUIndexFormat::Undefined unexpected value");
constexpr uint32_t PackIndexFormat(WGPUIndexFormat unpackedFormat)
{
    ASSERT(static_cast<uint32_t>(unpackedFormat) > 0);
    return static_cast<uint32_t>(unpackedFormat) - 1;
}

constexpr WGPUIndexFormat UnpackIndexFormat(uint32_t packedIndexFormat)
{
    return static_cast<WGPUIndexFormat>(packedIndexFormat + 1);
}

// Can pack the front face into 1 bit since it has 2 values and Undefined is not used.
static_assert(static_cast<uint32_t>(WGPUFrontFace_CW) == 2U, "Max WGPUFrontFace is not 2");
static_assert(static_cast<uint32_t>(WGPUFrontFace_Undefined) == 0,
              "WGPUFrontFace_Undefined unexpected value");
constexpr uint32_t PackFrontFace(WGPUFrontFace unpackedFrontFace)
{
    ASSERT(static_cast<uint32_t>(unpackedFrontFace) > 0);
    return static_cast<uint32_t>(unpackedFrontFace) - 1;
}

constexpr WGPUFrontFace UnpackFrontFace(uint32_t packedFrontFace)
{
    return static_cast<WGPUFrontFace>(packedFrontFace + 1);
}

PackedVertexAttribute::PackedVertexAttribute()
{
    memset(this, 0, sizeof(PackedVertexAttribute));
}

// GraphicsPipelineDesc implementation.
RenderPipelineDesc::RenderPipelineDesc()
{
    (void)mPad0;
    memset(this, 0, sizeof(RenderPipelineDesc));
}

RenderPipelineDesc::~RenderPipelineDesc() = default;

RenderPipelineDesc::RenderPipelineDesc(const RenderPipelineDesc &other)
{
    *this = other;
}

RenderPipelineDesc &RenderPipelineDesc::operator=(const RenderPipelineDesc &other)
{
    memcpy(this, &other, sizeof(*this));
    return *this;
}

bool RenderPipelineDesc::setPrimitiveMode(gl::PrimitiveMode primitiveMode,
                                          gl::DrawElementsType indexTypeOrInvalid)
{
    bool changed = false;

    WGPUPrimitiveTopology topology = gl_wgpu::GetPrimitiveTopology(primitiveMode);
    if (mPrimitiveState.topology != static_cast<uint8_t>(topology))
    {
        SetBitField(mPrimitiveState.topology, topology);
        changed = true;
    }

    uint32_t indexFormat = webgpu::IsStripPrimitiveTopology(topology) &&
                                   indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum
                               ? PackIndexFormat(gl_wgpu::GetIndexFormat(indexTypeOrInvalid))
                               : 0;
    if (mPrimitiveState.stripIndexFormat != static_cast<uint8_t>(indexFormat))
    {
        SetBitField(mPrimitiveState.stripIndexFormat, indexFormat);
        changed = true;
    }

    return changed;
}

bool RenderPipelineDesc::setBlendEnabled(size_t colorIndex, bool enabled)
{
    PackedColorTargetState &colorTarget = mColorTargetStates[colorIndex];
    if (colorTarget.blendEnabled == enabled)
    {
        return false;
    }

    SetBitField(colorTarget.blendEnabled, enabled);
    return true;
}

bool RenderPipelineDesc::setBlendFuncs(size_t colorIndex,
                                       WGPUBlendFactor srcRGB,
                                       WGPUBlendFactor dstRGB,
                                       WGPUBlendFactor srcAlpha,
                                       WGPUBlendFactor dstAlpha)
{
    bool changed                        = false;
    PackedColorTargetState &colorTarget = mColorTargetStates[colorIndex];

    if (colorTarget.colorBlendSrcFactor != static_cast<uint32_t>(srcRGB))
    {
        SetBitField(colorTarget.colorBlendSrcFactor, srcRGB);
        changed = true;
    }

    if (colorTarget.colorBlendDstFactor != static_cast<uint32_t>(dstRGB))
    {
        SetBitField(colorTarget.colorBlendDstFactor, dstRGB);
        changed = true;
    }

    if (colorTarget.alphaBlendSrcFactor != static_cast<uint32_t>(srcAlpha))
    {
        SetBitField(colorTarget.alphaBlendSrcFactor, srcAlpha);
        changed = true;
    }

    if (colorTarget.alphaBlendDstFactor != static_cast<uint32_t>(dstAlpha))
    {
        SetBitField(colorTarget.alphaBlendDstFactor, dstAlpha);
        changed = true;
    }

    return changed;
}

bool RenderPipelineDesc::setBlendEquations(size_t colorIndex,
                                           WGPUBlendOperation rgb,
                                           WGPUBlendOperation alpha)
{
    bool changed                        = false;
    PackedColorTargetState &colorTarget = mColorTargetStates[colorIndex];

    if (colorTarget.colorBlendOp != static_cast<uint32_t>(rgb))
    {
        SetBitField(colorTarget.colorBlendOp, rgb);
        changed = true;
    }

    if (colorTarget.alphaBlendOp != static_cast<uint32_t>(alpha))
    {
        SetBitField(colorTarget.alphaBlendOp, alpha);
        changed = true;
    }

    return changed;
}

void RenderPipelineDesc::setFrontFace(GLenum frontFace)
{
    SetBitField(mPrimitiveState.frontFace, PackFrontFace(gl_wgpu::GetFrontFace(frontFace)));
}

void RenderPipelineDesc::setCullMode(gl::CullFaceMode cullMode, bool cullFaceEnabled)
{
    SetBitField(mPrimitiveState.cullMode, gl_wgpu::GetCullMode(cullMode, cullFaceEnabled));
}

void RenderPipelineDesc::setColorWriteMask(size_t colorIndex, bool r, bool g, bool b, bool a)
{
    PackedColorTargetState &colorTarget = mColorTargetStates[colorIndex];
    SetBitField(colorTarget.writeMask, gl_wgpu::GetColorWriteMask(r, g, b, a));
}

bool RenderPipelineDesc::setVertexAttribute(size_t attribIndex, PackedVertexAttribute &newAttrib)
{
    PackedVertexAttribute &currentAttrib = mVertexAttributes[attribIndex];
    if (memcmp(&currentAttrib, &newAttrib, sizeof(PackedVertexAttribute)) == 0)
    {
        return false;
    }

    memcpy(&currentAttrib, &newAttrib, sizeof(PackedVertexAttribute));
    return true;
}

bool RenderPipelineDesc::setColorAttachmentFormat(size_t colorIndex, WGPUTextureFormat format)
{
    if (mColorTargetStates[colorIndex].format == static_cast<uint8_t>(format))
    {
        return false;
    }

    SetBitField(mColorTargetStates[colorIndex].format, format);
    return true;
}

bool RenderPipelineDesc::setDepthStencilAttachmentFormat(WGPUTextureFormat format)
{
    if (mDepthStencilState.format == static_cast<uint8_t>(format))
    {
        return false;
    }

    SetBitField(mDepthStencilState.format, format);
    return true;
}

bool RenderPipelineDesc::setDepthFunc(WGPUCompareFunction compareFunc)
{
    if (mDepthStencilState.depthCompare == static_cast<uint8_t>(compareFunc))
    {
        return false;
    }
    SetBitField(mDepthStencilState.depthCompare, compareFunc);
    return true;
}

bool RenderPipelineDesc::setStencilFrontFunc(WGPUCompareFunction compareFunc)
{
    if (mDepthStencilState.stencilFrontCompare == static_cast<uint8_t>(compareFunc))
    {
        return false;
    }
    SetBitField(mDepthStencilState.stencilFrontCompare, compareFunc);
    return true;
}

bool RenderPipelineDesc::setStencilFrontOps(WGPUStencilOperation failOp,
                                            WGPUStencilOperation depthFailOp,
                                            WGPUStencilOperation passOp)
{
    if (mDepthStencilState.stencilFrontFailOp == static_cast<uint8_t>(failOp) &&
        mDepthStencilState.stencilFrontDepthFailOp == static_cast<uint8_t>(depthFailOp) &&
        mDepthStencilState.stencilFrontPassOp == static_cast<uint8_t>(passOp))
    {
        return false;
    }
    SetBitField(mDepthStencilState.stencilFrontFailOp, failOp);
    SetBitField(mDepthStencilState.stencilFrontDepthFailOp, depthFailOp);
    SetBitField(mDepthStencilState.stencilFrontPassOp, passOp);
    return true;
}

bool RenderPipelineDesc::setStencilBackFunc(WGPUCompareFunction compareFunc)
{
    if (mDepthStencilState.stencilBackCompare == static_cast<uint8_t>(compareFunc))
    {
        return false;
    }
    SetBitField(mDepthStencilState.stencilBackCompare, compareFunc);
    return true;
}

bool RenderPipelineDesc::setStencilBackOps(WGPUStencilOperation failOp,
                                           WGPUStencilOperation depthFailOp,
                                           WGPUStencilOperation passOp)
{
    if (mDepthStencilState.stencilBackFailOp == static_cast<uint8_t>(failOp) &&
        mDepthStencilState.stencilBackDepthFailOp == static_cast<uint8_t>(depthFailOp) &&
        mDepthStencilState.stencilBackPassOp == static_cast<uint8_t>(passOp))
    {
        return false;
    }
    SetBitField(mDepthStencilState.stencilBackFailOp, failOp);
    SetBitField(mDepthStencilState.stencilBackDepthFailOp, depthFailOp);
    SetBitField(mDepthStencilState.stencilBackPassOp, passOp);
    return true;
}

bool RenderPipelineDesc::setStencilReadMask(uint8_t readMask)
{

    if (mDepthStencilState.stencilReadMask == readMask)
    {
        return false;
    }
    mDepthStencilState.stencilReadMask = readMask;
    return true;
}

bool RenderPipelineDesc::setStencilWriteMask(uint8_t writeMask)
{
    if (mDepthStencilState.stencilWriteMask == writeMask)
    {
        return false;
    }
    mDepthStencilState.stencilWriteMask = writeMask;
    return true;
}

size_t RenderPipelineDesc::hash() const
{
    return angle::ComputeGenericHash(angle::byte_span_from_ref(*this));
}

angle::Result RenderPipelineDesc::createPipeline(ContextWgpu *context,
                                                 const PipelineLayoutHandle &pipelineLayout,
                                                 const gl::ShaderMap<ShaderModuleHandle> &shaders,
                                                 RenderPipelineHandle *pipelineOut) const
{
    const DawnProcTable *wgpu = webgpu::GetProcs(context);

    constexpr const char *kShaderEntryPoint = "wgslMain";

    WGPURenderPipelineDescriptor pipelineDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDesc.layout                       = pipelineLayout.get();

    pipelineDesc.vertex.module        = shaders[gl::ShaderType::Vertex].get();
    pipelineDesc.vertex.entryPoint    = {kShaderEntryPoint, std::strlen(kShaderEntryPoint)};
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants     = nullptr;
    pipelineDesc.vertex.bufferCount   = 0;
    pipelineDesc.vertex.buffers       = nullptr;

    pipelineDesc.primitive.topology = static_cast<WGPUPrimitiveTopology>(mPrimitiveState.topology);
    if (webgpu::IsStripPrimitiveTopology(pipelineDesc.primitive.topology))
    {
        pipelineDesc.primitive.stripIndexFormat =
            UnpackIndexFormat(mPrimitiveState.stripIndexFormat);
    }
    else
    {
        pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    }
    pipelineDesc.primitive.frontFace = UnpackFrontFace(mPrimitiveState.frontFace);
    pipelineDesc.primitive.cullMode  = static_cast<WGPUCullMode>(mPrimitiveState.cullMode);

    size_t attribCount = 0;
    gl::AttribArray<WGPUVertexBufferLayout> vertexBuffers;
    gl::AttribArray<WGPUVertexAttribute> vertexAttribs;

    for (PackedVertexAttribute packedAttrib : mVertexAttributes)
    {
        if (!packedAttrib.enabled)
        {
            continue;
        }

        WGPUVertexAttribute &newAttribute   = vertexAttribs[attribCount];
        newAttribute                        = WGPU_VERTEX_ATTRIBUTE_INIT;
        newAttribute.format                 = static_cast<WGPUVertexFormat>(packedAttrib.format);
        newAttribute.offset                 = packedAttrib.offset;
        newAttribute.shaderLocation         = packedAttrib.shaderLocation;

        WGPUVertexBufferLayout &newBufferLayout   = vertexBuffers[attribCount];
        newBufferLayout                           = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        newBufferLayout.arrayStride               = packedAttrib.stride;
        newBufferLayout.stepMode                  = WGPUVertexStepMode_Undefined;
        newBufferLayout.attributeCount            = 1;
        newBufferLayout.attributes                = &newAttribute;

        attribCount++;
    }

    pipelineDesc.vertex.bufferCount = attribCount;
    pipelineDesc.vertex.buffers     = vertexBuffers.data();

    WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
    std::array<WGPUColorTargetState, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS> colorTargets;
    std::array<WGPUBlendState, gl::IMPLEMENTATION_MAX_DRAW_BUFFERS> blendStates;
    if (shaders[gl::ShaderType::Fragment])
    {
        fragmentState.module        = shaders[gl::ShaderType::Fragment].get();
        fragmentState.entryPoint    = {kShaderEntryPoint, std::strlen(kShaderEntryPoint)};
        fragmentState.constantCount = 0;
        fragmentState.constants     = nullptr;

        size_t colorTargetCount = 0;
        for (size_t colorTargetIndex = 0; colorTargetIndex < gl::IMPLEMENTATION_MAX_DRAW_BUFFERS;
             ++colorTargetIndex)
        {
            const webgpu::PackedColorTargetState &packedColorTarget =
                mColorTargetStates[colorTargetIndex];
            WGPUColorTargetState &outputColorTarget = colorTargets[colorTargetIndex];
            outputColorTarget                       = WGPU_COLOR_TARGET_STATE_INIT;

            outputColorTarget.format = static_cast<WGPUTextureFormat>(packedColorTarget.format);
            if (packedColorTarget.blendEnabled)
            {
                blendStates[colorTargetIndex] = WGPU_BLEND_STATE_INIT;
                blendStates[colorTargetIndex].color.srcFactor =
                    static_cast<WGPUBlendFactor>(packedColorTarget.colorBlendSrcFactor);
                blendStates[colorTargetIndex].color.dstFactor =
                    static_cast<WGPUBlendFactor>(packedColorTarget.colorBlendDstFactor);
                blendStates[colorTargetIndex].color.operation =
                    static_cast<WGPUBlendOperation>(packedColorTarget.colorBlendOp);

                blendStates[colorTargetIndex].alpha.srcFactor =
                    static_cast<WGPUBlendFactor>(packedColorTarget.alphaBlendSrcFactor);
                blendStates[colorTargetIndex].alpha.dstFactor =
                    static_cast<WGPUBlendFactor>(packedColorTarget.alphaBlendDstFactor);
                blendStates[colorTargetIndex].alpha.operation =
                    static_cast<WGPUBlendOperation>(packedColorTarget.alphaBlendOp);

                outputColorTarget.blend = &blendStates[colorTargetIndex];
            }

            outputColorTarget.writeMask =
                static_cast<WGPUColorWriteMask>(packedColorTarget.writeMask);

            if (outputColorTarget.format != WGPUTextureFormat_Undefined)
            {
                colorTargetCount = colorTargetIndex + 1;
            }
        }
        fragmentState.targetCount = colorTargetCount;
        fragmentState.targets     = colorTargets.data();

        pipelineDesc.fragment = &fragmentState;
    }

    WGPUDepthStencilState depthStencilState = WGPU_DEPTH_STENCIL_STATE_INIT;
    if (static_cast<WGPUTextureFormat>(mDepthStencilState.format) != WGPUTextureFormat_Undefined)
    {
        const PackedDepthStencilState &packedDepthStencilState = mDepthStencilState;

        depthStencilState.format = static_cast<WGPUTextureFormat>(packedDepthStencilState.format);
        depthStencilState.depthWriteEnabled =
            static_cast<WGPUOptionalBool>(packedDepthStencilState.depthWriteEnabled);
        depthStencilState.depthCompare =
            static_cast<WGPUCompareFunction>(packedDepthStencilState.depthCompare);

        depthStencilState.stencilFront.compare =
            static_cast<WGPUCompareFunction>(packedDepthStencilState.stencilFrontCompare);
        depthStencilState.stencilFront.failOp =
            static_cast<WGPUStencilOperation>(packedDepthStencilState.stencilFrontFailOp);
        depthStencilState.stencilFront.depthFailOp =
            static_cast<WGPUStencilOperation>(packedDepthStencilState.stencilFrontDepthFailOp);
        depthStencilState.stencilFront.passOp =
            static_cast<WGPUStencilOperation>(packedDepthStencilState.stencilFrontPassOp);

        depthStencilState.stencilBack.compare =
            static_cast<WGPUCompareFunction>(packedDepthStencilState.stencilBackCompare);
        depthStencilState.stencilBack.failOp =
            static_cast<WGPUStencilOperation>(packedDepthStencilState.stencilBackFailOp);
        depthStencilState.stencilBack.depthFailOp =
            static_cast<WGPUStencilOperation>(packedDepthStencilState.stencilBackDepthFailOp);
        depthStencilState.stencilBack.passOp =
            static_cast<WGPUStencilOperation>(packedDepthStencilState.stencilBackPassOp);

        depthStencilState.stencilReadMask  = packedDepthStencilState.stencilReadMask;
        depthStencilState.stencilWriteMask = packedDepthStencilState.stencilWriteMask;

        depthStencilState.depthBias           = packedDepthStencilState.depthBias;
        depthStencilState.depthBiasSlopeScale = packedDepthStencilState.depthBiasSlopeScalef;
        depthStencilState.depthBiasClamp      = packedDepthStencilState.depthBiasClamp;

        pipelineDesc.depthStencil = &depthStencilState;
    }

    DeviceHandle device = context->getDevice();
    ANGLE_WGPU_SCOPED_DEBUG_TRY(
        context, *pipelineOut = RenderPipelineHandle::Acquire(
                     wgpu, wgpu->deviceCreateRenderPipeline(device.get(), &pipelineDesc)));

    return angle::Result::Continue;
}

bool operator==(const RenderPipelineDesc &lhs, const RenderPipelineDesc &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(RenderPipelineDesc)) == 0;
}

// PipelineCache implementation.
PipelineCache::PipelineCache()  = default;
PipelineCache::~PipelineCache() = default;

angle::Result PipelineCache::getRenderPipeline(ContextWgpu *context,
                                               const RenderPipelineDesc &desc,
                                               const PipelineLayoutHandle &pipelineLayout,
                                               const gl::ShaderMap<ShaderModuleHandle> &shaders,
                                               RenderPipelineHandle *pipelineOut)
{
    auto iter = mRenderPipelines.find(desc);
    if (iter != mRenderPipelines.end())
    {
        *pipelineOut = iter->second;
        return angle::Result::Continue;
    }

    ANGLE_TRY(desc.createPipeline(context, pipelineLayout, shaders, pipelineOut));
    mRenderPipelines.insert(std::make_pair(desc, *pipelineOut));

    return angle::Result::Continue;
}

}  // namespace webgpu

}  // namespace rx
