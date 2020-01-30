//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_render_utils.mm:
//    Implements the class methods for RenderUtils.
//

#include "libANGLE/renderer/metal/mtl_render_utils.h"

#include <utility>

#include "common/debug.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "libANGLE/renderer/metal/shaders/compiled/mtl_default_shaders.inc"
#include "libANGLE/renderer/metal/shaders/mtl_default_shaders_src_autogen.inc"

namespace rx
{
namespace mtl
{
namespace
{

#define SOURCE_BUFFER_ALIGNED_CONSTANT_NAME @"kSourceBufferAligned"
#define SOURCE_IDX_IS_U8_CONSTANT_NAME @"kSourceIndexIsU8"
#define SOURCE_IDX_IS_U16_CONSTANT_NAME @"kSourceIndexIsU16"
#define SOURCE_IDX_IS_U32_CONSTANT_NAME @"kSourceIndexIsU32"

struct ClearParamsUniform
{
    float clearColor[4];
    float clearDepth;
    float padding[3];
};

struct BlitParamsUniform
{
    // 0: lower left, 1: lower right, 2: upper left, 3: upper right
    float srcTexCoords[4][2];
    int srcLevel         = 0;
    uint8_t srcLuminance = 0;  // source texture is luminance texture
    uint8_t dstFlipY     = 0;
    uint8_t dstLuminance = 0;  // dest texture is luminace
    uint8_t padding1;
    float padding2[2];
};

struct IndexConversionUniform
{
    uint32_t srcOffset;
    uint32_t indexCount;
    uint32_t padding[2];
};

template <typename T>
angle::Result GenTriFanFromClientElements(ContextMtl *contextMtl,
                                          GLsizei count,
                                          const T *indices,
                                          const BufferRef &dstBuffer,
                                          uint32_t dstOffset)
{
    ASSERT(count > 2);
    uint32_t *dstPtr = reinterpret_cast<uint32_t *>(dstBuffer->map(contextMtl) + dstOffset);
    T firstIdx;
    memcpy(&firstIdx, indices, sizeof(firstIdx));
    for (GLsizei i = 2; i < count; ++i)
    {
        T srcPrevIdx, srcIdx;
        memcpy(&srcPrevIdx, indices + i - 1, sizeof(srcPrevIdx));
        memcpy(&srcIdx, indices + i, sizeof(srcIdx));

        uint32_t triIndices[3];
        triIndices[0] = firstIdx;
        triIndices[1] = srcPrevIdx;
        triIndices[2] = srcIdx;

        memcpy(dstPtr + 3 * (i - 2), triIndices, sizeof(triIndices));
    }
    dstBuffer->unmap(contextMtl);

    return angle::Result::Continue;
}
template <typename T>
void GetFirstLastIndicesFromClientElements(GLsizei count,
                                           const T *indices,
                                           uint32_t *firstOut,
                                           uint32_t *lastOut)
{
    *firstOut = 0;
    *lastOut  = 0;
    memcpy(firstOut, indices, sizeof(indices[0]));
    memcpy(lastOut, indices + count - 1, sizeof(indices[0]));
}

}  // namespace

bool RenderUtils::IndexConvesionPipelineCacheKey::operator==(
    const IndexConvesionPipelineCacheKey &other) const
{
    return srcType == other.srcType && srcBufferOffsetAligned == other.srcBufferOffsetAligned;
}
bool RenderUtils::IndexConvesionPipelineCacheKey::operator<(
    const IndexConvesionPipelineCacheKey &other) const
{
    if (!srcBufferOffsetAligned && other.srcBufferOffsetAligned)
    {
        return true;
    }
    if (srcBufferOffsetAligned && !other.srcBufferOffsetAligned)
    {
        return false;
    }
    return static_cast<int>(srcType) < static_cast<int>(other.srcType);
}

RenderUtils::RenderUtils(DisplayMtl *display) : Context(display) {}

RenderUtils::~RenderUtils() {}

angle::Result RenderUtils::initialize()
{
    auto re = initShaderLibrary();
    if (re != angle::Result::Continue)
    {
        return re;
    }

    initClearResources();
    initBlitResources();

    return angle::Result::Continue;
}

void RenderUtils::onDestroy()
{
    mDefaultShaders = nil;

    mClearRenderPipelineCache.clear();
    mBlitRenderPipelineCache.clear();
    mBlitPremultiplyAlphaRenderPipelineCache.clear();
    mBlitUnmultiplyAlphaRenderPipelineCache.clear();

    mIndexConversionPipelineCaches.clear();
    mTriFanFromElemArrayGeneratorPipelineCaches.clear();

    mTriFanFromArraysGeneratorPipeline = nil;
}

// override ErrorHandler
void RenderUtils::handleError(GLenum glErrorCode,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    ERR() << "Metal backend encountered an internal error. Code=" << glErrorCode << ".";
}

void RenderUtils::handleError(NSError *nserror,
                              const char *file,
                              const char *function,
                              unsigned int line)
{
    if (!nserror)
    {
        return;
    }

    std::stringstream errorStream;
    ERR() << "Metal backend encountered an internal error: \n"
          << nserror.localizedDescription.UTF8String;
}

angle::Result RenderUtils::initShaderLibrary()
{
    AutoObjCObj<NSError> err = nil;

#if defined(ANGLE_MTL_DEBUG_INTERNAL_SHADERS)
    mDefaultShaders = CreateShaderLibrary(getDisplay()->getMetalDevice(), default_metallib_src,
                                          sizeof(default_metallib_src), &err);
#else
    mDefaultShaders =
        CreateShaderLibraryFromBinary(getDisplay()->getMetalDevice(), compiled_default_metallib,
                                      compiled_default_metallib_len, &err);
#endif

    if (err && !mDefaultShaders)
    {
        ANGLE_MTL_CHECK(this, false, err.get());
        return angle::Result::Stop;
    }

    return angle::Result::Continue;
}

void RenderUtils::initClearResources()
{
    ANGLE_MTL_OBJC_SCOPE
    {
        // Shader pipeline
        mClearRenderPipelineCache.setVertexShader(
            this, [[mDefaultShaders.get() newFunctionWithName:@"clearVS"] ANGLE_MTL_AUTORELEASE]);
        mClearRenderPipelineCache.setFragmentShader(
            this, [[mDefaultShaders.get() newFunctionWithName:@"clearFS"] ANGLE_MTL_AUTORELEASE]);
    }
}

void RenderUtils::initBlitResources()
{
    ANGLE_MTL_OBJC_SCOPE
    {
        auto shaderLib    = mDefaultShaders.get();
        auto vertexShader = [[shaderLib newFunctionWithName:@"blitVS"] ANGLE_MTL_AUTORELEASE];

        mBlitRenderPipelineCache.setVertexShader(this, vertexShader);
        mBlitRenderPipelineCache.setFragmentShader(
            this, [[shaderLib newFunctionWithName:@"blitFS"] ANGLE_MTL_AUTORELEASE]);

        mBlitPremultiplyAlphaRenderPipelineCache.setVertexShader(this, vertexShader);
        mBlitPremultiplyAlphaRenderPipelineCache.setFragmentShader(
            this,
            [[shaderLib newFunctionWithName:@"blitPremultiplyAlphaFS"] ANGLE_MTL_AUTORELEASE]);

        mBlitUnmultiplyAlphaRenderPipelineCache.setVertexShader(this, vertexShader);
        mBlitUnmultiplyAlphaRenderPipelineCache.setFragmentShader(
            this, [[shaderLib newFunctionWithName:@"blitUnmultiplyAlphaFS"] ANGLE_MTL_AUTORELEASE]);
    }
}

void RenderUtils::clearWithDraw(const gl::Context *context,
                                RenderCommandEncoder *cmdEncoder,
                                const ClearRectParams &params)
{
    auto overridedParams = params;
    // Make sure we don't clear attachment that doesn't exist
    const RenderPassDesc &renderPassDesc = cmdEncoder->renderPassDesc();
    if (renderPassDesc.numColorAttachments == 0)
    {
        overridedParams.clearColor.reset();
    }
    if (!renderPassDesc.depthAttachment.texture)
    {
        overridedParams.clearDepth.reset();
    }
    if (!renderPassDesc.stencilAttachment.texture)
    {
        overridedParams.clearStencil.reset();
    }

    if (!overridedParams.clearColor.valid() && !overridedParams.clearDepth.valid() &&
        !overridedParams.clearStencil.valid())
    {
        return;
    }

    setupClearWithDraw(context, cmdEncoder, overridedParams);

    // Draw the screen aligned quad
    cmdEncoder->draw(MTLPrimitiveTypeTriangle, 0, 6);

    // Invalidate current context's state
    auto contextMtl = GetImpl(context);
    contextMtl->invalidateState(context);
}

void RenderUtils::blitWithDraw(const gl::Context *context,
                               RenderCommandEncoder *cmdEncoder,
                               const BlitParams &params)
{
    if (!params.src)
    {
        return;
    }
    setupBlitWithDraw(context, cmdEncoder, params);

    // Draw the screen aligned quad
    cmdEncoder->draw(MTLPrimitiveTypeTriangle, 0, 6);

    // Invalidate current context's state
    ContextMtl *contextMtl = GetImpl(context);
    contextMtl->invalidateState(context);
}

void RenderUtils::setupClearWithDraw(const gl::Context *context,
                                     RenderCommandEncoder *cmdEncoder,
                                     const ClearRectParams &params)
{
    // Generate render pipeline state
    auto renderPipelineState = getClearRenderPipelineState(context, cmdEncoder, params);
    ASSERT(renderPipelineState);
    // Setup states
    setupDrawCommonStates(cmdEncoder);
    cmdEncoder->setRenderPipelineState(renderPipelineState);

    id<MTLDepthStencilState> dsState = getClearDepthStencilState(context, params);
    cmdEncoder->setDepthStencilState(dsState).setStencilRefVal(params.clearStencil.value());

    // Viewports
    const RenderPassDesc &renderPassDesc = cmdEncoder->renderPassDesc();

    MTLViewport viewport;
    MTLScissorRect scissorRect;

    RenderPassAttachmentDesc renderPassAttachment;

    if (renderPassDesc.numColorAttachments)
    {
        renderPassAttachment = renderPassDesc.colorAttachments[0];
    }
    else if (renderPassDesc.depthAttachment.texture)
    {
        renderPassAttachment = renderPassDesc.depthAttachment;
    }
    else
    {
        ASSERT(renderPassDesc.stencilAttachment.texture);
        renderPassAttachment = renderPassDesc.stencilAttachment;
    }

    auto texture = renderPassAttachment.texture;

    viewport =
        GetViewport(params.clearArea, texture->height(renderPassAttachment.level), params.flipY);

    scissorRect =
        GetScissorRect(params.clearArea, texture->height(renderPassAttachment.level), params.flipY);

    cmdEncoder->setViewport(viewport);
    cmdEncoder->setScissorRect(scissorRect);

    // uniform
    ClearParamsUniform uniformParams;
    uniformParams.clearColor[0] = static_cast<float>(params.clearColor.value().red);
    uniformParams.clearColor[1] = static_cast<float>(params.clearColor.value().green);
    uniformParams.clearColor[2] = static_cast<float>(params.clearColor.value().blue);
    uniformParams.clearColor[3] = static_cast<float>(params.clearColor.value().alpha);
    uniformParams.clearDepth    = params.clearDepth.value();

    cmdEncoder->setVertexData(uniformParams, 0);
    cmdEncoder->setFragmentData(uniformParams, 0);
}

void RenderUtils::setupBlitWithDraw(const gl::Context *context,
                                    RenderCommandEncoder *cmdEncoder,
                                    const BlitParams &params)
{
    ASSERT(cmdEncoder->renderPassDesc().numColorAttachments == 1 && params.src);

    // Generate render pipeline state
    auto renderPipelineState = getBlitRenderPipelineState(context, cmdEncoder, params);
    ASSERT(renderPipelineState);
    // Setup states
    setupDrawCommonStates(cmdEncoder);
    cmdEncoder->setRenderPipelineState(renderPipelineState);
    cmdEncoder->setDepthStencilState(getDisplay()->getStateCache().getNullDepthStencilState(this));

    // Viewport
    const RenderPassDesc &renderPassDesc = cmdEncoder->renderPassDesc();
    const RenderPassColorAttachmentDesc &renderPassColorAttachment =
        renderPassDesc.colorAttachments[0];
    auto texture = renderPassColorAttachment.texture;

    gl::Rectangle dstRect(params.dstOffset.x, params.dstOffset.y, params.srcRect.width,
                          params.srcRect.height);
    MTLViewport viewportMtl =
        GetViewport(dstRect, texture->height(renderPassColorAttachment.level), params.dstFlipY);
    MTLScissorRect scissorRectMtl =
        GetScissorRect(dstRect, texture->height(renderPassColorAttachment.level), params.dstFlipY);
    cmdEncoder->setViewport(viewportMtl);
    cmdEncoder->setScissorRect(scissorRectMtl);

    cmdEncoder->setFragmentTexture(params.src, 0);

    // Uniform
    setupBlitWithDrawUniformData(cmdEncoder, params);
}

void RenderUtils::setupDrawCommonStates(RenderCommandEncoder *cmdEncoder)
{
    cmdEncoder->setCullMode(MTLCullModeNone);
    cmdEncoder->setTriangleFillMode(MTLTriangleFillModeFill);
    cmdEncoder->setDepthBias(0, 0, 0);
}

id<MTLDepthStencilState> RenderUtils::getClearDepthStencilState(const gl::Context *context,
                                                                const ClearRectParams &params)
{
    if (!params.clearDepth.valid() && !params.clearStencil.valid())
    {
        // Doesn't clear depth nor stencil
        return getDisplay()->getStateCache().getNullDepthStencilState(this);
    }

    ContextMtl *contextMtl = GetImpl(context);

    DepthStencilDesc desc;
    desc.reset();

    if (params.clearDepth.valid())
    {
        // Clear depth state
        desc.depthWriteEnabled = true;
    }
    else
    {
        desc.depthWriteEnabled = false;
    }

    if (params.clearStencil.valid())
    {
        // Clear stencil state
        desc.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationReplace;
        desc.frontFaceStencil.writeMask                 = contextMtl->getStencilMask();
        desc.backFaceStencil.depthStencilPassOperation  = MTLStencilOperationReplace;
        desc.backFaceStencil.writeMask                  = contextMtl->getStencilMask();
    }

    return getDisplay()->getStateCache().getDepthStencilState(getDisplay()->getMetalDevice(), desc);
}

id<MTLRenderPipelineState> RenderUtils::getClearRenderPipelineState(
    const gl::Context *context,
    RenderCommandEncoder *cmdEncoder,
    const ClearRectParams &params)
{
    ContextMtl *contextMtl      = GetImpl(context);
    MTLColorWriteMask colorMask = contextMtl->getColorMask();
    if (!params.clearColor.valid())
    {
        colorMask = MTLColorWriteMaskNone;
    }

    RenderPipelineDesc pipelineDesc;
    const RenderPassDesc &renderPassDesc = cmdEncoder->renderPassDesc();

    renderPassDesc.populateRenderPipelineOutputDesc(colorMask, &pipelineDesc.outputDescriptor);

    pipelineDesc.inputPrimitiveTopology = kPrimitiveTopologyClassTriangle;

    return mClearRenderPipelineCache.getRenderPipelineState(contextMtl, pipelineDesc);
}

id<MTLRenderPipelineState> RenderUtils::getBlitRenderPipelineState(const gl::Context *context,
                                                                   RenderCommandEncoder *cmdEncoder,
                                                                   const BlitParams &params)
{
    ContextMtl *contextMtl = GetImpl(context);
    RenderPipelineDesc pipelineDesc;
    const RenderPassDesc &renderPassDesc = cmdEncoder->renderPassDesc();

    renderPassDesc.populateRenderPipelineOutputDesc(params.dstColorMask,
                                                    &pipelineDesc.outputDescriptor);

    pipelineDesc.inputPrimitiveTopology = kPrimitiveTopologyClassTriangle;

    RenderPipelineCache *pipelineCache;
    if (params.unpackPremultiplyAlpha == params.unpackUnmultiplyAlpha)
    {
        pipelineCache = &mBlitRenderPipelineCache;
    }
    else if (params.unpackPremultiplyAlpha)
    {
        pipelineCache = &mBlitPremultiplyAlphaRenderPipelineCache;
    }
    else
    {
        pipelineCache = &mBlitUnmultiplyAlphaRenderPipelineCache;
    }

    return pipelineCache->getRenderPipelineState(contextMtl, pipelineDesc);
}

void RenderUtils::setupBlitWithDrawUniformData(RenderCommandEncoder *cmdEncoder,
                                               const BlitParams &params)
{
    BlitParamsUniform uniformParams;
    uniformParams.dstFlipY     = params.dstFlipY ? 1 : 0;
    uniformParams.srcLevel     = params.srcLevel;
    uniformParams.dstLuminance = params.dstLuminance ? 1 : 0;

    // Compute source texCoords
    auto srcWidth  = params.src->width(params.srcLevel);
    auto srcHeight = params.src->height(params.srcLevel);

    int x0 = params.srcRect.x0();  // left
    int x1 = params.srcRect.x1();  // right
    int y0 = params.srcRect.y0();  // lower
    int y1 = params.srcRect.y1();  // upper
    if (params.srcYFlipped)
    {
        // If source's Y has been flipped, such as default framebuffer, then adjust the real source
        // rectangle.
        y0 = srcHeight - y1;
        y1 = y0 + params.srcRect.height;
        std::swap(y0, y1);
    }

    if (params.unpackFlipY)
    {
        std::swap(y0, y1);
    }

    float u0 = (float)x0 / srcWidth;
    float u1 = (float)x1 / srcWidth;
    float v0 = (float)y0 / srcHeight;
    float v1 = (float)y1 / srcHeight;

    // lower left
    uniformParams.srcTexCoords[0][0] = u0;
    uniformParams.srcTexCoords[0][1] = v0;

    // lower right
    uniformParams.srcTexCoords[1][0] = u1;
    uniformParams.srcTexCoords[1][1] = v0;

    // upper left
    uniformParams.srcTexCoords[2][0] = u0;
    uniformParams.srcTexCoords[2][1] = v1;

    // upper right
    uniformParams.srcTexCoords[3][0] = u1;
    uniformParams.srcTexCoords[3][1] = v1;

    cmdEncoder->setVertexData(uniformParams, 0);
    cmdEncoder->setFragmentData(uniformParams, 0);
}

AutoObjCPtr<id<MTLComputePipelineState>> RenderUtils::getIndexConversionPipeline(
    ContextMtl *context,
    gl::DrawElementsType srcType,
    uint32_t srcOffset)
{
    id<MTLDevice> metalDevice = context->getMetalDevice();
    size_t elementSize        = gl::GetDrawElementsTypeSize(srcType);
    bool aligned              = (srcOffset % elementSize) == 0;

    IndexConvesionPipelineCacheKey key = {srcType, aligned};

    auto &cache = mIndexConversionPipelineCaches[key];

    if (!cache)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            auto shaderLib         = mDefaultShaders.get();
            id<MTLFunction> shader = nil;
            auto funcConstants = [[[MTLFunctionConstantValues alloc] init] ANGLE_MTL_AUTORELEASE];
            NSError *err       = nil;

            [funcConstants setConstantValue:&aligned
                                       type:MTLDataTypeBool
                                   withName:SOURCE_BUFFER_ALIGNED_CONSTANT_NAME];

            switch (srcType)
            {
                case gl::DrawElementsType::UnsignedByte:
                    shader = [shaderLib newFunctionWithName:@"convertIndexU8ToU16"];
                    break;
                case gl::DrawElementsType::UnsignedShort:
                    shader = [shaderLib newFunctionWithName:@"convertIndexU16"
                                             constantValues:funcConstants
                                                      error:&err];
                    break;
                case gl::DrawElementsType::UnsignedInt:
                    shader = [shaderLib newFunctionWithName:@"convertIndexU32"
                                             constantValues:funcConstants
                                                      error:&err];
                    break;
                default:
                    UNREACHABLE();
            }

            if (err && !shader)
            {
                ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
            }
            ASSERT([shader ANGLE_MTL_AUTORELEASE]);

            cache = [[metalDevice newComputePipelineStateWithFunction:shader
                                                                error:&err] ANGLE_MTL_AUTORELEASE];
            if (err && !cache)
            {
                ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
            }
            ASSERT(cache);
        }
    }

    return cache;
}

AutoObjCPtr<id<MTLComputePipelineState>> RenderUtils::getTriFanFromElemArrayGeneratorPipeline(
    ContextMtl *context,
    gl::DrawElementsType srcType,
    uint32_t srcOffset)
{
    id<MTLDevice> metalDevice = context->getMetalDevice();
    size_t elementSize        = gl::GetDrawElementsTypeSize(srcType);
    bool aligned              = (srcOffset % elementSize) == 0;

    IndexConvesionPipelineCacheKey key = {srcType, aligned};

    auto &cache = mTriFanFromElemArrayGeneratorPipelineCaches[key];

    if (!cache)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            auto shaderLib         = mDefaultShaders.get();
            id<MTLFunction> shader = nil;
            auto funcConstants = [[[MTLFunctionConstantValues alloc] init] ANGLE_MTL_AUTORELEASE];
            NSError *err       = nil;

            bool isU8  = false;
            bool isU16 = false;
            bool isU32 = false;

            switch (srcType)
            {
                case gl::DrawElementsType::UnsignedByte:
                    isU8 = true;
                    break;
                case gl::DrawElementsType::UnsignedShort:
                    isU16 = true;
                    break;
                case gl::DrawElementsType::UnsignedInt:
                    isU32 = true;
                    break;
                default:
                    UNREACHABLE();
            }

            [funcConstants setConstantValue:&aligned
                                       type:MTLDataTypeBool
                                   withName:SOURCE_BUFFER_ALIGNED_CONSTANT_NAME];
            [funcConstants setConstantValue:&isU8
                                       type:MTLDataTypeBool
                                   withName:SOURCE_IDX_IS_U8_CONSTANT_NAME];
            [funcConstants setConstantValue:&isU16
                                       type:MTLDataTypeBool
                                   withName:SOURCE_IDX_IS_U16_CONSTANT_NAME];
            [funcConstants setConstantValue:&isU32
                                       type:MTLDataTypeBool
                                   withName:SOURCE_IDX_IS_U32_CONSTANT_NAME];

            shader = [shaderLib newFunctionWithName:@"genTriFanIndicesFromElements"
                                     constantValues:funcConstants
                                              error:&err];
            if (err && !shader)
            {
                ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
            }
            ASSERT([shader ANGLE_MTL_AUTORELEASE]);

            cache = [[metalDevice newComputePipelineStateWithFunction:shader
                                                                error:&err] ANGLE_MTL_AUTORELEASE];
            if (err && !cache)
            {
                ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
            }
            ASSERT(cache);
        }
    }

    return cache;
}

angle::Result RenderUtils::ensureTriFanFromArrayGeneratorInitialized(ContextMtl *context)
{
    if (!mTriFanFromArraysGeneratorPipeline)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            id<MTLDevice> metalDevice = context->getMetalDevice();
            auto shaderLib            = mDefaultShaders.get();
            NSError *err              = nil;
            id<MTLFunction> shader = [shaderLib newFunctionWithName:@"genTriFanIndicesFromArray"];

            [shader ANGLE_MTL_AUTORELEASE];

            mTriFanFromArraysGeneratorPipeline =
                [[metalDevice newComputePipelineStateWithFunction:shader
                                                            error:&err] ANGLE_MTL_AUTORELEASE];
            if (err && !mTriFanFromArraysGeneratorPipeline)
            {
                ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
            }

            ASSERT(mTriFanFromArraysGeneratorPipeline);
        }
    }
    return angle::Result::Continue;
}

angle::Result RenderUtils::convertIndexBuffer(const gl::Context *context,
                                              gl::DrawElementsType srcType,
                                              uint32_t indexCount,
                                              const BufferRef &srcBuffer,
                                              uint32_t srcOffset,
                                              const BufferRef &dstBuffer,
                                              uint32_t dstOffset)
{
    ContextMtl *contextMtl            = GetImpl(context);
    ComputeCommandEncoder *cmdEncoder = contextMtl->getComputeCommandEncoder();
    ASSERT(cmdEncoder);

    AutoObjCPtr<id<MTLComputePipelineState>> pipelineState =
        getIndexConversionPipeline(contextMtl, srcType, srcOffset);

    ASSERT(pipelineState);

    cmdEncoder->setComputePipelineState(pipelineState);

    ASSERT((dstOffset % kIndexBufferOffsetAlignment) == 0);

    IndexConversionUniform uniform;
    uniform.srcOffset  = srcOffset;
    uniform.indexCount = indexCount;

    cmdEncoder->setData(uniform, 0);
    cmdEncoder->setBuffer(srcBuffer, 0, 1);
    cmdEncoder->setBuffer(dstBuffer, dstOffset, 2);

    ANGLE_TRY(dispatchCompute(context, cmdEncoder, pipelineState, indexCount));

    return angle::Result::Continue;
}

angle::Result RenderUtils::generateTriFanBufferFromArrays(const gl::Context *context,
                                                          const TriFanFromArrayParams &params)
{
    ContextMtl *contextMtl            = GetImpl(context);
    ComputeCommandEncoder *cmdEncoder = contextMtl->getComputeCommandEncoder();
    ASSERT(cmdEncoder);
    ANGLE_TRY(ensureTriFanFromArrayGeneratorInitialized(contextMtl));

    ASSERT(params.vertexCount > 2);

    cmdEncoder->setComputePipelineState(mTriFanFromArraysGeneratorPipeline);

    ASSERT((params.dstOffset % kIndexBufferOffsetAlignment) == 0);

    struct TriFanArrayParams
    {
        uint firstVertex;
        uint vertexCountFrom3rd;
        uint padding[2];
    } uniform;

    uniform.firstVertex        = params.firstVertex;
    uniform.vertexCountFrom3rd = params.vertexCount - 2;

    cmdEncoder->setData(uniform, 0);
    cmdEncoder->setBuffer(params.dstBuffer, params.dstOffset, 2);

    ANGLE_TRY(dispatchCompute(context, cmdEncoder, mTriFanFromArraysGeneratorPipeline,
                              uniform.vertexCountFrom3rd));

    return angle::Result::Continue;
}

angle::Result RenderUtils::generateTriFanBufferFromElementsArray(
    const gl::Context *context,
    const IndexGenerationParams &params)
{
    ContextMtl *contextMtl             = GetImpl(context);
    const gl::VertexArray *vertexArray = context->getState().getVertexArray();
    const gl::Buffer *elementBuffer    = vertexArray->getElementArrayBuffer();
    if (elementBuffer)
    {
        size_t srcOffset = reinterpret_cast<size_t>(params.indices);
        ANGLE_CHECK(contextMtl, srcOffset <= std::numeric_limits<uint32_t>::max(),
                    "Index offset is too large", GL_INVALID_VALUE);
        return generateTriFanBufferFromElementsArrayGPU(
            context, params.srcType, params.indexCount,
            GetImpl(elementBuffer)->getCurrentBuffer(context), static_cast<uint32_t>(srcOffset),
            params.dstBuffer, params.dstOffset);
    }
    else
    {
        return generateTriFanBufferFromElementsArrayCPU(context, params);
    }
}

angle::Result RenderUtils::generateTriFanBufferFromElementsArrayGPU(
    const gl::Context *context,
    gl::DrawElementsType srcType,
    uint32_t indexCount,
    const BufferRef &srcBuffer,
    uint32_t srcOffset,
    const BufferRef &dstBuffer,
    // Must be multiples of kIndexBufferOffsetAlignment
    uint32_t dstOffset)
{
    ContextMtl *contextMtl            = GetImpl(context);
    ComputeCommandEncoder *cmdEncoder = contextMtl->getComputeCommandEncoder();
    ASSERT(cmdEncoder);

    AutoObjCPtr<id<MTLComputePipelineState>> pipelineState =
        getTriFanFromElemArrayGeneratorPipeline(contextMtl, srcType, srcOffset);

    ASSERT(pipelineState);

    cmdEncoder->setComputePipelineState(pipelineState);

    ASSERT((dstOffset % kIndexBufferOffsetAlignment) == 0);
    ASSERT(indexCount > 2);

    IndexConversionUniform uniform;
    uniform.srcOffset  = srcOffset;
    uniform.indexCount = indexCount - 2;  // Only start from the 3rd element.

    cmdEncoder->setData(uniform, 0);
    cmdEncoder->setBuffer(srcBuffer, 0, 1);
    cmdEncoder->setBuffer(dstBuffer, dstOffset, 2);

    ANGLE_TRY(dispatchCompute(context, cmdEncoder, pipelineState, uniform.indexCount));

    return angle::Result::Continue;
}

angle::Result RenderUtils::generateTriFanBufferFromElementsArrayCPU(
    const gl::Context *context,
    const IndexGenerationParams &params)
{
    ContextMtl *contextMtl = GetImpl(context);
    switch (params.srcType)
    {
        case gl::DrawElementsType::UnsignedByte:
            return GenTriFanFromClientElements(contextMtl, params.indexCount,
                                               static_cast<const uint8_t *>(params.indices),
                                               params.dstBuffer, params.dstOffset);
        case gl::DrawElementsType::UnsignedShort:
            return GenTriFanFromClientElements(contextMtl, params.indexCount,
                                               static_cast<const uint16_t *>(params.indices),
                                               params.dstBuffer, params.dstOffset);
        case gl::DrawElementsType::UnsignedInt:
            return GenTriFanFromClientElements(contextMtl, params.indexCount,
                                               static_cast<const uint32_t *>(params.indices),
                                               params.dstBuffer, params.dstOffset);
        default:
            UNREACHABLE();
    }

    return angle::Result::Stop;
}

angle::Result RenderUtils::generateLineLoopLastSegment(const gl::Context *context,
                                                       uint32_t firstVertex,
                                                       uint32_t lastVertex,
                                                       const BufferRef &dstBuffer,
                                                       uint32_t dstOffset)
{
    ContextMtl *contextMtl = GetImpl(context);
    uint8_t *ptr           = dstBuffer->map(contextMtl);

    uint32_t indices[2] = {lastVertex, firstVertex};
    memcpy(ptr, indices, sizeof(indices));

    dstBuffer->unmap(contextMtl);

    return angle::Result::Continue;
}

angle::Result RenderUtils::generateLineLoopLastSegmentFromElementsArray(
    const gl::Context *context,
    const IndexGenerationParams &params)
{
    ContextMtl *contextMtl             = GetImpl(context);
    const gl::VertexArray *vertexArray = context->getState().getVertexArray();
    const gl::Buffer *elementBuffer    = vertexArray->getElementArrayBuffer();
    if (elementBuffer)
    {
        size_t srcOffset = reinterpret_cast<size_t>(params.indices);
        ANGLE_CHECK(contextMtl, srcOffset <= std::numeric_limits<uint32_t>::max(),
                    "Index offset is too large", GL_INVALID_VALUE);

        BufferMtl *bufferMtl = GetImpl(elementBuffer);
        std::pair<uint32_t, uint32_t> firstLast;
        ANGLE_TRY(bufferMtl->getFirstLastIndices(context, params.srcType,
                                                 static_cast<uint32_t>(srcOffset),
                                                 params.indexCount, &firstLast));

        return generateLineLoopLastSegment(context, firstLast.first, firstLast.second,
                                           params.dstBuffer, params.dstOffset);
    }
    else
    {
        return generateLineLoopLastSegmentFromElementsArrayCPU(context, params);
    }
}

angle::Result RenderUtils::generateLineLoopLastSegmentFromElementsArrayCPU(
    const gl::Context *context,
    const IndexGenerationParams &params)
{
    uint32_t first, last;

    switch (params.srcType)
    {
        case gl::DrawElementsType::UnsignedByte:
            GetFirstLastIndicesFromClientElements(
                params.indexCount, static_cast<const uint8_t *>(params.indices), &first, &last);
            break;
        case gl::DrawElementsType::UnsignedShort:
            GetFirstLastIndicesFromClientElements(
                params.indexCount, static_cast<const uint16_t *>(params.indices), &first, &last);
            break;
        case gl::DrawElementsType::UnsignedInt:
            GetFirstLastIndicesFromClientElements(
                params.indexCount, static_cast<const uint32_t *>(params.indices), &first, &last);
            break;
        default:
            UNREACHABLE();
            return angle::Result::Stop;
    }

    return generateLineLoopLastSegment(context, first, last, params.dstBuffer, params.dstOffset);
}

angle::Result RenderUtils::dispatchCompute(const gl::Context *context,
                                           ComputeCommandEncoder *cmdEncoder,
                                           id<MTLComputePipelineState> pipelineState,
                                           size_t numThreads)
{
    NSUInteger w                  = pipelineState.threadExecutionWidth;
    MTLSize threadsPerThreadgroup = MTLSizeMake(w, 1, 1);

    if (getDisplay()->getFeatures().hasNonUniformDispatch.enabled)
    {
        MTLSize threads = MTLSizeMake(numThreads, 1, 1);
        cmdEncoder->dispatchNonUniform(threads, threadsPerThreadgroup);
    }
    else
    {
        MTLSize groups = MTLSizeMake((numThreads + w - 1) / w, 1, 1);
        cmdEncoder->dispatch(groups, threadsPerThreadgroup);
    }

    return angle::Result::Continue;
}
}  // namespace mtl
}  // namespace rx
