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
#define PREMULTIPLY_ALPHA_CONSTANT_NAME @"kPremultiplyAlpha"
#define UNMULTIPLY_ALPHA_CONSTANT_NAME @"kUnmultiplyAlpha"
#define SOURCE_TEXTURE_TYPE_CONSTANT_NAME @"kSourceTextureType"

// See libANGLE/renderer/metal/shaders/clear.metal
struct ClearParamsUniform
{
    float clearColor[4];
    float clearDepth;
    float padding[3];
};

// See libANGLE/renderer/metal/shaders/blit.metal
struct BlitParamsUniform
{
    // 0: lower left, 1: lower right, 2: upper left
    float srcTexCoords[3][2];
    int srcLevel         = 0;
    int srcLayer         = 0;
    uint8_t dstFlipX     = 0;
    uint8_t dstFlipY     = 0;
    uint8_t dstLuminance = 0;  // dest texture is luminace
    uint8_t padding1;
    float padding2[3];
};

struct IndexConversionUniform
{
    uint32_t srcOffset;
    uint32_t indexCount;
    uint32_t padding[2];
};

void GetBlitTexCoords(uint32_t srcWidth,
                      uint32_t srcHeight,
                      const gl::Rectangle &srcRect,
                      bool srcYFlipped,
                      bool unpackFlipY,
                      float *u0,
                      float *v0,
                      float *u1,
                      float *v1)
{
    int x0 = srcRect.x0();  // left
    int x1 = srcRect.x1();  // right
    int y0 = srcRect.y0();  // lower
    int y1 = srcRect.y1();  // upper
    if (srcYFlipped)
    {
        // If source's Y has been flipped, such as default framebuffer, then adjust the real source
        // rectangle.
        y0 = srcHeight - y1;
        y1 = y0 + srcRect.height;
        std::swap(y0, y1);
    }

    if (unpackFlipY)
    {
        std::swap(y0, y1);
    }

    *u0 = static_cast<float>(x0) / srcWidth;
    *u1 = static_cast<float>(x1) / srcWidth;
    *v0 = static_cast<float>(y0) / srcHeight;
    *v1 = static_cast<float>(y1) / srcHeight;
}

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

int GetShaderTextureType(const TextureRef &texture)
{
    if (!texture)
    {
        return -1;
    }
    switch (texture->textureType())
    {
        case MTLTextureType2D:
            return mtl_shader::kTextureType2D;
        case MTLTextureType2DArray:
            return mtl_shader::kTextureType2DArray;
        case MTLTextureType2DMultisample:
            return mtl_shader::kTextureType2DMultisample;
        case MTLTextureTypeCube:
            return mtl_shader::kTextureTypeCube;
        case MTLTextureType3D:
            return mtl_shader::kTextureType3D;
        default:
            UNREACHABLE();
    }

    return 0;
}

ANGLE_INLINE
void EnsureComputePipelineInitialized(DisplayMtl *display,
                                      NSString *functionName,
                                      AutoObjCPtr<id<MTLComputePipelineState>> *pipelineOut)
{
    AutoObjCPtr<id<MTLComputePipelineState>> &pipeline = *pipelineOut;
    if (pipeline)
    {
        return;
    }

    ANGLE_MTL_OBJC_SCOPE
    {
        id<MTLDevice> metalDevice = display->getMetalDevice();
        id<MTLLibrary> shaderLib  = display->getDefaultShadersLib();
        NSError *err              = nil;
        id<MTLFunction> shader    = [shaderLib newFunctionWithName:functionName];

        [shader ANGLE_MTL_AUTORELEASE];

        pipeline = [[metalDevice newComputePipelineStateWithFunction:shader
                                                               error:&err] ANGLE_MTL_AUTORELEASE];
        if (err && !pipeline)
        {
            ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
        }

        ASSERT(pipeline);
    }
}

ANGLE_INLINE
void EnsureSpecializedComputePipelineInitialized(
    DisplayMtl *display,
    NSString *functionName,
    MTLFunctionConstantValues *funcConstants,
    AutoObjCPtr<id<MTLComputePipelineState>> *pipelineOut)
{
    if (!funcConstants)
    {
        // Non specialized constants provided, use default creation function.
        EnsureComputePipelineInitialized(display, functionName, pipelineOut);
        return;
    }

    AutoObjCPtr<id<MTLComputePipelineState>> &pipeline = *pipelineOut;
    if (pipeline)
    {
        return;
    }

    ANGLE_MTL_OBJC_SCOPE
    {
        id<MTLDevice> metalDevice = display->getMetalDevice();
        id<MTLLibrary> shaderLib  = display->getDefaultShadersLib();
        NSError *err              = nil;

        id<MTLFunction> shader = [shaderLib newFunctionWithName:functionName
                                                 constantValues:funcConstants
                                                          error:&err];
        if (err && !shader)
        {
            ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
        }
        ASSERT([shader ANGLE_MTL_AUTORELEASE]);

        pipeline = [[metalDevice newComputePipelineStateWithFunction:shader
                                                               error:&err] ANGLE_MTL_AUTORELEASE];
        if (err && !pipeline)
        {
            ERR() << "Internal error: " << err.localizedDescription.UTF8String << "\n";
        }
        ASSERT(pipeline);
    }
}

template <typename T>
void ClearRenderPipelineCacheArray(T *pipelineCacheArray)
{
    for (RenderPipelineCache &pipelineCache : *pipelineCacheArray)
    {
        pipelineCache.clear();
    }
}

template <typename T>
void ClearPipelineStateArray(T *pipelineCacheArray)
{
    for (auto &pipeline : *pipelineCacheArray)
    {
        pipeline = nil;
    }
}

template <typename T>
void ClearPipelineState2DArray(T *pipelineCache2DArray)
{
    for (auto &level1Array : *pipelineCache2DArray)
    {
        ClearPipelineStateArray(&level1Array);
    }
}

void DispatchCompute(ContextMtl *contextMtl,
                     ComputeCommandEncoder *cmdEncoder,
                     id<MTLComputePipelineState> pipelineState,
                     size_t numThreads)
{
    NSUInteger w = std::min<NSUInteger>(pipelineState.threadExecutionWidth, numThreads);
    MTLSize threadsPerThreadgroup = MTLSizeMake(w, 1, 1);

    if (contextMtl->getDisplay()->getFeatures().hasNonUniformDispatch.enabled)
    {
        MTLSize threads = MTLSizeMake(numThreads, 1, 1);
        cmdEncoder->dispatchNonUniform(threads, threadsPerThreadgroup);
    }
    else
    {
        MTLSize groups = MTLSizeMake((numThreads + w - 1) / w, 1, 1);
        cmdEncoder->dispatch(groups, threadsPerThreadgroup);
    }
}

void SetupFullscreenDrawCommonStates(RenderCommandEncoder *cmdEncoder)
{
    cmdEncoder->setCullMode(MTLCullModeNone);
    cmdEncoder->setTriangleFillMode(MTLTriangleFillModeFill);
    cmdEncoder->setDepthBias(0, 0, 0);
}

void SetupBlitWithDrawUniformData(RenderCommandEncoder *cmdEncoder, const BlitParams &params)
{

    BlitParamsUniform uniformParams;
    uniformParams.dstFlipY     = params.dstFlipY ? 1 : 0;
    uniformParams.srcLevel     = params.srcLevel;
    uniformParams.dstLuminance = params.dstLuminance ? 1 : 0;

    // Compute source texCoords
    uint32_t srcWidth = 0, srcHeight = 0;
    if (params.src)
    {
        srcWidth  = params.src->width(params.srcLevel);
        srcHeight = params.src->height(params.srcLevel);
    }
    else
    {
        UNREACHABLE();
    }

    float u0, v0, u1, v1;
    GetBlitTexCoords(srcWidth, srcHeight, params.srcRect, params.srcYFlipped, params.unpackFlipY,
                     &u0, &v0, &u1, &v1);

    float du = u1 - u0;
    float dv = v1 - v0;

    // lower left
    uniformParams.srcTexCoords[0][0] = u0;
    uniformParams.srcTexCoords[0][1] = v0;

    // lower right
    uniformParams.srcTexCoords[1][0] = u1 + du;
    uniformParams.srcTexCoords[1][1] = v0;

    // upper left
    uniformParams.srcTexCoords[2][0] = u0;
    uniformParams.srcTexCoords[2][1] = v1 + dv;

    cmdEncoder->setVertexData(uniformParams, 0);
    cmdEncoder->setFragmentData(uniformParams, 0);
}
}  // namespace

// RenderUtils implementation
RenderUtils::RenderUtils(DisplayMtl *display) : Context(display) {}

RenderUtils::~RenderUtils() {}

angle::Result RenderUtils::initialize()
{
    return angle::Result::Continue;
}

void RenderUtils::onDestroy()
{
    mIndexUtils.onDestroy();
    mClearUtils.onDestroy();
    mColorBlitUtils.onDestroy();
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

// Clear current framebuffer
angle::Result RenderUtils::clearWithDraw(const gl::Context *context,
                                         RenderCommandEncoder *cmdEncoder,
                                         const ClearRectParams &params)
{
    return mClearUtils.clearWithDraw(context, cmdEncoder, params);
}

// Blit texture data to current framebuffer
angle::Result RenderUtils::blitWithDraw(const gl::Context *context,
                                        RenderCommandEncoder *cmdEncoder,
                                        const BlitParams &params)
{
    return mColorBlitUtils.blitWithDraw(context, cmdEncoder, params);
}

angle::Result RenderUtils::blitWithDraw(const gl::Context *context,
                                        RenderCommandEncoder *cmdEncoder,
                                        const TextureRef &srcTexture)
{
    if (!srcTexture)
    {
        return angle::Result::Continue;
    }
    BlitParams params;
    params.src     = srcTexture;
    params.srcRect = gl::Rectangle(0, 0, srcTexture->width(), srcTexture->height());
    return blitWithDraw(context, cmdEncoder, params);
}

angle::Result RenderUtils::convertIndexBufferGPU(ContextMtl *contextMtl,
                                                 const IndexConversionParams &params)
{
    return mIndexUtils.convertIndexBufferGPU(contextMtl, params);
}
angle::Result RenderUtils::generateTriFanBufferFromArrays(ContextMtl *contextMtl,
                                                          const TriFanFromArrayParams &params)
{
    return mIndexUtils.generateTriFanBufferFromArrays(contextMtl, params);
}
angle::Result RenderUtils::generateTriFanBufferFromElementsArray(
    ContextMtl *contextMtl,
    const IndexGenerationParams &params)
{
    return mIndexUtils.generateTriFanBufferFromElementsArray(contextMtl, params);
}

angle::Result RenderUtils::generateLineLoopLastSegment(ContextMtl *contextMtl,
                                                       uint32_t firstVertex,
                                                       uint32_t lastVertex,
                                                       const BufferRef &dstBuffer,
                                                       uint32_t dstOffset)
{
    return mIndexUtils.generateLineLoopLastSegment(contextMtl, firstVertex, lastVertex, dstBuffer,
                                                   dstOffset);
}
angle::Result RenderUtils::generateLineLoopLastSegmentFromElementsArray(
    ContextMtl *contextMtl,
    const IndexGenerationParams &params)
{
    return mIndexUtils.generateLineLoopLastSegmentFromElementsArray(contextMtl, params);
}

// ClearUtils implementation
ClearUtils::ClearUtils() = default;

void ClearUtils::onDestroy()
{
    mClearRenderPipelineCache.clear();
}

void ClearUtils::ensureRenderPipelineStateCacheInitialized(ContextMtl *ctx)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        id<MTLLibrary> shaderLib = ctx->getDisplay()->getDefaultShadersLib();

        mClearRenderPipelineCache.setVertexShader(
            ctx, [[shaderLib newFunctionWithName:@"clearVS"] ANGLE_MTL_AUTORELEASE]);
        mClearRenderPipelineCache.setFragmentShader(
            ctx, [[shaderLib newFunctionWithName:@"clearFS"] ANGLE_MTL_AUTORELEASE]);
    }
}

id<MTLDepthStencilState> ClearUtils::getClearDepthStencilState(const gl::Context *context,
                                                               const ClearRectParams &params)
{
    ContextMtl *contextMtl = GetImpl(context);

    if (!params.clearDepth.valid() && !params.clearStencil.valid())
    {
        // Doesn't clear depth nor stencil
        return contextMtl->getDisplay()->getStateCache().getNullDepthStencilState(contextMtl);
    }

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

    return contextMtl->getDisplay()->getStateCache().getDepthStencilState(
        contextMtl->getMetalDevice(), desc);
}

id<MTLRenderPipelineState> ClearUtils::getClearRenderPipelineState(const gl::Context *context,
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

    ensureRenderPipelineStateCacheInitialized(contextMtl);

    return mClearRenderPipelineCache.getRenderPipelineState(contextMtl, pipelineDesc);
}

void ClearUtils::setupClearWithDraw(const gl::Context *context,
                                    RenderCommandEncoder *cmdEncoder,
                                    const ClearRectParams &params)
{
    // Generate render pipeline state
    id<MTLRenderPipelineState> renderPipelineState =
        getClearRenderPipelineState(context, cmdEncoder, params);
    ASSERT(renderPipelineState);
    // Setup states
    SetupFullscreenDrawCommonStates(cmdEncoder);
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

    TextureRef texture = renderPassAttachment.texture;

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

angle::Result ClearUtils::clearWithDraw(const gl::Context *context,
                                        RenderCommandEncoder *cmdEncoder,
                                        const ClearRectParams &params)
{
    ClearRectParams overridedParams = params;
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
        return angle::Result::Continue;
    }

    setupClearWithDraw(context, cmdEncoder, overridedParams);

    // Draw the screen aligned triangle
    cmdEncoder->draw(MTLPrimitiveTypeTriangle, 0, 3);

    // Invalidate current context's state
    ContextMtl *contextMtl = GetImpl(context);
    contextMtl->invalidateState(context);

    return angle::Result::Continue;
}

// ColorBlitUtils implementation
ColorBlitUtils::ColorBlitUtils() = default;

void ColorBlitUtils::onDestroy()
{
    ClearRenderPipelineCacheArray(&mBlitRenderPipelineCache);
    ClearRenderPipelineCacheArray(&mBlitPremultiplyAlphaRenderPipelineCache);
    ClearRenderPipelineCacheArray(&mBlitUnmultiplyAlphaRenderPipelineCache);
}

void ColorBlitUtils::ensureRenderPipelineStateCacheInitialized(ContextMtl *ctx,
                                                               int alphaPremultiplyType,
                                                               int textureType,
                                                               RenderPipelineCache *cacheOut)
{
    RenderPipelineCache &pipelineCache = *cacheOut;
    if (pipelineCache.getVertexShader() && pipelineCache.getFragmentShader())
    {
        // Already initialized.
        return;
    }

    ANGLE_MTL_OBJC_SCOPE
    {
        NSError *err             = nil;
        id<MTLLibrary> shaderLib = ctx->getDisplay()->getDefaultShadersLib();
        id<MTLFunction> vertexShader =
            [[shaderLib newFunctionWithName:@"blitVS"] ANGLE_MTL_AUTORELEASE];
        MTLFunctionConstantValues *funcConstants =
            [[[MTLFunctionConstantValues alloc] init] ANGLE_MTL_AUTORELEASE];

        constexpr BOOL multiplyAlphaFlags[][2] = {// premultiply, unmultiply

                                                  // Normal blit
                                                  {NO, NO},
                                                  // Blit premultiply-alpha
                                                  {YES, NO},
                                                  // Blit unmultiply alpha
                                                  {NO, YES}};

        // Set alpha multiply flags
        [funcConstants setConstantValue:&multiplyAlphaFlags[alphaPremultiplyType][0]
                                   type:MTLDataTypeBool
                               withName:PREMULTIPLY_ALPHA_CONSTANT_NAME];
        [funcConstants setConstantValue:&multiplyAlphaFlags[alphaPremultiplyType][1]
                                   type:MTLDataTypeBool
                               withName:UNMULTIPLY_ALPHA_CONSTANT_NAME];

        // Set texture type constant
        [funcConstants setConstantValue:&textureType
                                   type:MTLDataTypeInt
                               withName:SOURCE_TEXTURE_TYPE_CONSTANT_NAME];

        id<MTLFunction> fragmentShader =
            [[shaderLib newFunctionWithName:@"blitFS" constantValues:funcConstants
                                      error:&err] ANGLE_MTL_AUTORELEASE];

        ASSERT(vertexShader);
        ASSERT(fragmentShader);
        pipelineCache.setVertexShader(ctx, vertexShader);
        pipelineCache.setFragmentShader(ctx, fragmentShader);
    }
}

id<MTLRenderPipelineState> ColorBlitUtils::getBlitRenderPipelineState(
    const gl::Context *context,
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
    int alphaPremultiplyType;
    int textureType = GetShaderTextureType(params.src);
    if (params.unpackPremultiplyAlpha == params.unpackUnmultiplyAlpha)
    {
        alphaPremultiplyType = 0;
        pipelineCache        = &mBlitRenderPipelineCache[textureType];
    }
    else if (params.unpackPremultiplyAlpha)
    {
        alphaPremultiplyType = 1;
        pipelineCache        = &mBlitPremultiplyAlphaRenderPipelineCache[textureType];
    }
    else
    {
        alphaPremultiplyType = 2;
        pipelineCache        = &mBlitUnmultiplyAlphaRenderPipelineCache[textureType];
    }

    ensureRenderPipelineStateCacheInitialized(contextMtl, alphaPremultiplyType, textureType,
                                              pipelineCache);

    return pipelineCache->getRenderPipelineState(contextMtl, pipelineDesc);
}

void ColorBlitUtils::setupBlitWithDraw(const gl::Context *context,
                                       RenderCommandEncoder *cmdEncoder,
                                       const BlitParams &params)
{
    ASSERT(cmdEncoder->renderPassDesc().numColorAttachments == 1 && params.src);

    ContextMtl *contextMtl = mtl::GetImpl(context);

    // Generate render pipeline state
    id<MTLRenderPipelineState> renderPipelineState =
        getBlitRenderPipelineState(context, cmdEncoder, params);
    ASSERT(renderPipelineState);
    // Setup states
    SetupFullscreenDrawCommonStates(cmdEncoder);
    cmdEncoder->setRenderPipelineState(renderPipelineState);
    cmdEncoder->setDepthStencilState(
        contextMtl->getDisplay()->getStateCache().getNullDepthStencilState(contextMtl));

    // Viewport
    const RenderPassDesc &renderPassDesc = cmdEncoder->renderPassDesc();
    const RenderPassColorAttachmentDesc &renderPassColorAttachment =
        renderPassDesc.colorAttachments[0];
    mtl::TextureRef texture = renderPassColorAttachment.texture;

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
    SetupBlitWithDrawUniformData(cmdEncoder, params);
}

angle::Result ColorBlitUtils::blitWithDraw(const gl::Context *context,
                                           RenderCommandEncoder *cmdEncoder,
                                           const BlitParams &params)
{
    if (!params.src)
    {
        return angle::Result::Continue;
    }
    ContextMtl *contextMtl = GetImpl(context);
    setupBlitWithDraw(context, cmdEncoder, params);

    // Draw the screen aligned triangle
    cmdEncoder->draw(MTLPrimitiveTypeTriangle, 0, 3);

    // Invalidate current context's state
    contextMtl->invalidateState(context);

    return angle::Result::Continue;
}

// IndexGeneratorUtils implementation
void IndexGeneratorUtils::onDestroy()
{
    ClearPipelineState2DArray(&mIndexConversionPipelineCaches);
    ClearPipelineState2DArray(&mTriFanFromElemArrayGeneratorPipelineCaches);

    mTriFanFromArraysGeneratorPipeline = nil;
}

AutoObjCPtr<id<MTLComputePipelineState>> IndexGeneratorUtils::getIndexConversionPipeline(
    ContextMtl *contextMtl,
    gl::DrawElementsType srcType,
    uint32_t srcOffset)
{
    size_t elementSize = gl::GetDrawElementsTypeSize(srcType);
    BOOL aligned       = (srcOffset % elementSize) == 0;
    int srcTypeKey     = static_cast<int>(srcType);
    AutoObjCPtr<id<MTLComputePipelineState>> &cache =
        mIndexConversionPipelineCaches[srcTypeKey][aligned ? 1 : 0];

    if (!cache)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            auto funcConstants = [[[MTLFunctionConstantValues alloc] init] ANGLE_MTL_AUTORELEASE];

            [funcConstants setConstantValue:&aligned
                                       type:MTLDataTypeBool
                                   withName:SOURCE_BUFFER_ALIGNED_CONSTANT_NAME];

            NSString *shaderName = nil;
            switch (srcType)
            {
                case gl::DrawElementsType::UnsignedByte:
                    // No need for specialized shader
                    funcConstants = nil;
                    shaderName    = @"convertIndexU8ToU16";
                    break;
                case gl::DrawElementsType::UnsignedShort:
                    shaderName = @"convertIndexU16";
                    break;
                case gl::DrawElementsType::UnsignedInt:
                    shaderName = @"convertIndexU32";
                    break;
                default:
                    UNREACHABLE();
            }

            EnsureSpecializedComputePipelineInitialized(contextMtl->getDisplay(), shaderName,
                                                        funcConstants, &cache);
        }
    }

    return cache;
}

AutoObjCPtr<id<MTLComputePipelineState>>
IndexGeneratorUtils::getTriFanFromElemArrayGeneratorPipeline(ContextMtl *contextMtl,
                                                             gl::DrawElementsType srcType,
                                                             uint32_t srcOffset)
{
    size_t elementSize = gl::GetDrawElementsTypeSize(srcType);
    BOOL aligned       = (srcOffset % elementSize) == 0;
    int srcTypeKey     = static_cast<int>(srcType);

    AutoObjCPtr<id<MTLComputePipelineState>> &cache =
        mTriFanFromElemArrayGeneratorPipelineCaches[srcTypeKey][aligned ? 1 : 0];

    if (!cache)
    {
        ANGLE_MTL_OBJC_SCOPE
        {
            auto funcConstants = [[[MTLFunctionConstantValues alloc] init] ANGLE_MTL_AUTORELEASE];

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

            EnsureSpecializedComputePipelineInitialized(
                contextMtl->getDisplay(), @"genTriFanIndicesFromElements", funcConstants, &cache);
        }
    }

    return cache;
}

void IndexGeneratorUtils::ensureTriFanFromArrayGeneratorInitialized(ContextMtl *contextMtl)
{
    EnsureComputePipelineInitialized(contextMtl->getDisplay(), @"genTriFanIndicesFromArray",
                                     &mTriFanFromArraysGeneratorPipeline);
}

angle::Result IndexGeneratorUtils::convertIndexBufferGPU(ContextMtl *contextMtl,
                                                         const IndexConversionParams &params)
{
    ComputeCommandEncoder *cmdEncoder = contextMtl->getComputeCommandEncoder();
    ASSERT(cmdEncoder);

    AutoObjCPtr<id<MTLComputePipelineState>> pipelineState =
        getIndexConversionPipeline(contextMtl, params.srcType, params.srcOffset);

    ASSERT(pipelineState);

    cmdEncoder->setComputePipelineState(pipelineState);

    ASSERT((params.dstOffset % kIndexBufferOffsetAlignment) == 0);

    IndexConversionUniform uniform;
    uniform.srcOffset  = params.srcOffset;
    uniform.indexCount = params.indexCount;

    cmdEncoder->setData(uniform, 0);
    cmdEncoder->setBuffer(params.srcBuffer, 0, 1);
    cmdEncoder->setBufferForWrite(params.dstBuffer, params.dstOffset, 2);

    DispatchCompute(contextMtl, cmdEncoder, pipelineState, params.indexCount);

    return angle::Result::Continue;
}

angle::Result IndexGeneratorUtils::generateTriFanBufferFromArrays(
    ContextMtl *contextMtl,
    const TriFanFromArrayParams &params)
{
    ComputeCommandEncoder *cmdEncoder = contextMtl->getComputeCommandEncoder();
    ASSERT(cmdEncoder);
    ensureTriFanFromArrayGeneratorInitialized(contextMtl);

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
    cmdEncoder->setBufferForWrite(params.dstBuffer, params.dstOffset, 2);

    DispatchCompute(contextMtl, cmdEncoder, mTriFanFromArraysGeneratorPipeline,
                    uniform.vertexCountFrom3rd);

    return angle::Result::Continue;
}

angle::Result IndexGeneratorUtils::generateTriFanBufferFromElementsArray(
    ContextMtl *contextMtl,
    const IndexGenerationParams &params)
{
    const gl::VertexArray *vertexArray = contextMtl->getState().getVertexArray();
    const gl::Buffer *elementBuffer    = vertexArray->getElementArrayBuffer();
    if (elementBuffer)
    {
        BufferMtl *elementBufferMtl = GetImpl(elementBuffer);
        size_t srcOffset            = reinterpret_cast<size_t>(params.indices);
        ANGLE_CHECK(contextMtl, srcOffset <= std::numeric_limits<uint32_t>::max(),
                    "Index offset is too large", GL_INVALID_VALUE);
        return generateTriFanBufferFromElementsArrayGPU(
            contextMtl, params.srcType, params.indexCount, elementBufferMtl->getCurrentBuffer(),
            static_cast<uint32_t>(srcOffset), params.dstBuffer, params.dstOffset);
    }
    else
    {
        return generateTriFanBufferFromElementsArrayCPU(contextMtl, params);
    }
}

angle::Result IndexGeneratorUtils::generateTriFanBufferFromElementsArrayGPU(
    ContextMtl *contextMtl,
    gl::DrawElementsType srcType,
    uint32_t indexCount,
    const BufferRef &srcBuffer,
    uint32_t srcOffset,
    const BufferRef &dstBuffer,
    // Must be multiples of kIndexBufferOffsetAlignment
    uint32_t dstOffset)
{
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
    cmdEncoder->setBufferForWrite(dstBuffer, dstOffset, 2);

    DispatchCompute(contextMtl, cmdEncoder, pipelineState, uniform.indexCount);

    return angle::Result::Continue;
}

angle::Result IndexGeneratorUtils::generateTriFanBufferFromElementsArrayCPU(
    ContextMtl *contextMtl,
    const IndexGenerationParams &params)
{
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

angle::Result IndexGeneratorUtils::generateLineLoopLastSegment(ContextMtl *contextMtl,
                                                               uint32_t firstVertex,
                                                               uint32_t lastVertex,
                                                               const BufferRef &dstBuffer,
                                                               uint32_t dstOffset)
{
    uint8_t *ptr = dstBuffer->map(contextMtl) + dstOffset;

    uint32_t indices[2] = {lastVertex, firstVertex};
    memcpy(ptr, indices, sizeof(indices));

    dstBuffer->unmap(contextMtl);

    return angle::Result::Continue;
}

angle::Result IndexGeneratorUtils::generateLineLoopLastSegmentFromElementsArray(
    ContextMtl *contextMtl,
    const IndexGenerationParams &params)
{
    const gl::VertexArray *vertexArray = contextMtl->getState().getVertexArray();
    const gl::Buffer *elementBuffer    = vertexArray->getElementArrayBuffer();
    if (elementBuffer)
    {
        size_t srcOffset = reinterpret_cast<size_t>(params.indices);
        ANGLE_CHECK(contextMtl, srcOffset <= std::numeric_limits<uint32_t>::max(),
                    "Index offset is too large", GL_INVALID_VALUE);

        BufferMtl *bufferMtl = GetImpl(elementBuffer);
        std::pair<uint32_t, uint32_t> firstLast;
        ANGLE_TRY(bufferMtl->getFirstLastIndices(params.srcType, static_cast<uint32_t>(srcOffset),
                                                 params.indexCount, &firstLast));

        return generateLineLoopLastSegment(contextMtl, firstLast.first, firstLast.second,
                                           params.dstBuffer, params.dstOffset);
    }
    else
    {
        return generateLineLoopLastSegmentFromElementsArrayCPU(contextMtl, params);
    }
}

angle::Result IndexGeneratorUtils::generateLineLoopLastSegmentFromElementsArrayCPU(
    ContextMtl *contextMtl,
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

    return generateLineLoopLastSegment(contextMtl, first, last, params.dstBuffer, params.dstOffset);
}

}  // namespace mtl
}  // namespace rx
