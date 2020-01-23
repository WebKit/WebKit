//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferVk.cpp:
//    Implements the class methods for FramebufferVk.
//

#include "libANGLE/renderer/vulkan/FramebufferVk.h"

#include <array>
#include "volk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"

namespace rx
{

namespace
{
// The value to assign an alpha channel that's emulated.  The type is unsigned int, though it will
// automatically convert to the actual data type.
constexpr unsigned int kEmulatedAlphaValue = 1;

constexpr size_t kMinReadPixelsBufferSize = 128000;

// Alignment value to accommodate the largest known, for now, uncompressed Vulkan format
// VK_FORMAT_R64G64B64A64_SFLOAT, while supporting 3-component types such as
// VK_FORMAT_R16G16B16_SFLOAT.
constexpr size_t kReadPixelsBufferAlignment = 32 * 3;

// Clear values are only used when loadOp=Clear is set in clearWithRenderPassOp.  When starting a
// new render pass, the clear value is set to an unlikely value (bright pink) to stand out better
// in case of a bug.
constexpr VkClearValue kUninitializedClearValue = {{{0.95, 0.05, 0.95, 0.95}}};

const gl::InternalFormat &GetReadAttachmentInfo(const gl::Context *context,
                                                RenderTargetVk *renderTarget)
{
    GLenum implFormat =
        renderTarget->getImageFormat().actualImageFormat().fboImplementationInternalFormat;
    return gl::GetSizedInternalFormatInfo(implFormat);
}

bool HasSrcBlitFeature(RendererVk *renderer, RenderTargetVk *srcRenderTarget)
{
    const VkFormat srcFormat = srcRenderTarget->getImageFormat().vkImageFormat;
    return renderer->hasImageFormatFeatureBits(srcFormat, VK_FORMAT_FEATURE_BLIT_SRC_BIT);
}

bool HasDstBlitFeature(RendererVk *renderer, RenderTargetVk *dstRenderTarget)
{
    const VkFormat dstFormat = dstRenderTarget->getImageFormat().vkImageFormat;
    return renderer->hasImageFormatFeatureBits(dstFormat, VK_FORMAT_FEATURE_BLIT_DST_BIT);
}

// Returns false if destination has any channel the source doesn't.  This means that channel was
// emulated and using the Vulkan blit command would overwrite that emulated channel.
bool areSrcAndDstColorChannelsBlitCompatible(RenderTargetVk *srcRenderTarget,
                                             RenderTargetVk *dstRenderTarget)
{
    const angle::Format &srcFormat = srcRenderTarget->getImageFormat().intendedFormat();
    const angle::Format &dstFormat = dstRenderTarget->getImageFormat().intendedFormat();

    // Luminance/alpha formats are not renderable, so they can't have ended up in a framebuffer to
    // participate in a blit.
    ASSERT(!dstFormat.isLUMA() && !srcFormat.isLUMA());

    // All color formats have the red channel.
    ASSERT(dstFormat.redBits > 0 && srcFormat.redBits > 0);

    return (dstFormat.greenBits > 0 || srcFormat.greenBits == 0) &&
           (dstFormat.blueBits > 0 || srcFormat.blueBits == 0) &&
           (dstFormat.alphaBits > 0 || srcFormat.alphaBits == 0);
}

bool areSrcAndDstDepthStencilChannelsBlitCompatible(RenderTargetVk *srcRenderTarget,
                                                    RenderTargetVk *dstRenderTarget)
{
    const angle::Format &srcFormat = srcRenderTarget->getImageFormat().intendedFormat();
    const angle::Format &dstFormat = dstRenderTarget->getImageFormat().intendedFormat();

    return (dstFormat.depthBits > 0 || srcFormat.depthBits == 0) &&
           (dstFormat.stencilBits > 0 || srcFormat.stencilBits == 0);
}

void SetEmulatedAlphaValue(const vk::Format &format, VkClearColorValue *value)
{
    if (format.vkFormatIsInt)
    {
        if (format.vkFormatIsUnsigned)
        {
            value->uint32[3] = kEmulatedAlphaValue;
        }
        else
        {
            value->int32[3] = kEmulatedAlphaValue;
        }
    }
    else
    {
        value->float32[3] = kEmulatedAlphaValue;
    }
}
}  // anonymous namespace

// static
FramebufferVk *FramebufferVk::CreateUserFBO(RendererVk *renderer, const gl::FramebufferState &state)
{
    return new FramebufferVk(renderer, state, nullptr);
}

// static
FramebufferVk *FramebufferVk::CreateDefaultFBO(RendererVk *renderer,
                                               const gl::FramebufferState &state,
                                               WindowSurfaceVk *backbuffer)
{
    return new FramebufferVk(renderer, state, backbuffer);
}

FramebufferVk::FramebufferVk(RendererVk *renderer,
                             const gl::FramebufferState &state,
                             WindowSurfaceVk *backbuffer)
    : FramebufferImpl(state), mBackbuffer(backbuffer), mActiveColorComponents(0)
{
    mReadPixelBuffer.init(renderer, VK_BUFFER_USAGE_TRANSFER_DST_BIT, kReadPixelsBufferAlignment,
                          kMinReadPixelsBufferSize, true);
}

FramebufferVk::~FramebufferVk() = default;

void FramebufferVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    mFramebuffer.release(contextVk);

    mReadPixelBuffer.release(contextVk->getRenderer());
}

angle::Result FramebufferVk::discard(const gl::Context *context,
                                     size_t count,
                                     const GLenum *attachments)
{
    return invalidate(context, count, attachments);
}

angle::Result FramebufferVk::invalidate(const gl::Context *context,
                                        size_t count,
                                        const GLenum *attachments)
{
    ContextVk *contextVk = vk::GetImpl(context);
    mFramebuffer.onGraphAccess(contextVk->getCommandGraph());

    if (mFramebuffer.valid() && mFramebuffer.hasStartedRenderPass())
    {
        invalidateImpl(contextVk, count, attachments);
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::invalidateSub(const gl::Context *context,
                                           size_t count,
                                           const GLenum *attachments,
                                           const gl::Rectangle &area)
{
    ContextVk *contextVk = vk::GetImpl(context);
    mFramebuffer.onGraphAccess(contextVk->getCommandGraph());

    // RenderPass' storeOp cannot be made conditional to a specific region, so we only apply this
    // hint if the requested area encompasses the render area.
    if (mFramebuffer.valid() && mFramebuffer.hasStartedRenderPass() &&
        area.encloses(mFramebuffer.getRenderPassRenderArea()))
    {
        invalidateImpl(contextVk, count, attachments);
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::clear(const gl::Context *context, GLbitfield mask)
{
    ContextVk *contextVk = vk::GetImpl(context);

    bool clearColor   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    bool clearDepth   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    bool clearStencil = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));
    gl::DrawBufferMask clearColorBuffers;
    if (clearColor)
    {
        clearColorBuffers = mState.getEnabledDrawBuffers();
    }

    const VkClearColorValue &clearColorValue = contextVk->getClearColorValue().color;
    const VkClearDepthStencilValue &clearDepthStencilValue =
        contextVk->getClearDepthStencilValue().depthStencil;

    return clearImpl(context, clearColorBuffers, clearDepth, clearStencil, clearColorValue,
                     clearDepthStencilValue);
}

angle::Result FramebufferVk::clearImpl(const gl::Context *context,
                                       gl::DrawBufferMask clearColorBuffers,
                                       bool clearDepth,
                                       bool clearStencil,
                                       const VkClearColorValue &clearColorValue,
                                       const VkClearDepthStencilValue &clearDepthStencilValue)
{
    ContextVk *contextVk = vk::GetImpl(context);

    const gl::Rectangle scissoredRenderArea = getScissoredRenderArea(contextVk);

    // Discard clear altogether if scissor has 0 width or height.
    if (scissoredRenderArea.width == 0 || scissoredRenderArea.height == 0)
    {
        return angle::Result::Continue;
    }

    mFramebuffer.updateCurrentAccessNodes();

    // This function assumes that only enabled attachments are asked to be cleared.
    ASSERT((clearColorBuffers & mState.getEnabledDrawBuffers()) == clearColorBuffers);

    // Adjust clear behavior based on whether the respective attachments are present; if asked to
    // clear a non-existent attachment, don't attempt to clear it.

    VkColorComponentFlags colorMaskFlags = contextVk->getClearColorMask();
    bool clearColor                      = clearColorBuffers.any();

    const gl::FramebufferAttachment *depthAttachment = mState.getDepthAttachment();
    clearDepth                                       = clearDepth && depthAttachment;
    ASSERT(!clearDepth || depthAttachment->isAttached());

    const gl::FramebufferAttachment *stencilAttachment = mState.getStencilAttachment();
    clearStencil                                       = clearStencil && stencilAttachment;
    ASSERT(!clearStencil || stencilAttachment->isAttached());

    uint8_t stencilMask =
        static_cast<uint8_t>(contextVk->getState().getDepthStencilState().stencilWritemask);

    // The front-end should ensure we don't attempt to clear color if all channels are masked.
    ASSERT(!clearColor || colorMaskFlags != 0);
    // The front-end should ensure we don't attempt to clear depth if depth write is disabled.
    ASSERT(!clearDepth || contextVk->getState().getDepthStencilState().depthMask);
    // The front-end should ensure we don't attempt to clear stencil if all bits are masked.
    ASSERT(!clearStencil || stencilMask != 0);

    // If there is nothing to clear, return right away (for example, if asked to clear depth, but
    // there is no depth attachment).
    if (!clearColor && !clearDepth && !clearStencil)
    {
        return angle::Result::Continue;
    }

    VkClearDepthStencilValue modifiedDepthStencilValue = clearDepthStencilValue;

    // We can use render pass load ops if clearing depth, unmasked color or unmasked stencil.  If
    // there's a depth mask, depth clearing is already disabled.
    bool maskedClearColor =
        clearColor && (mActiveColorComponents & colorMaskFlags) != mActiveColorComponents;
    bool maskedClearStencil = stencilMask != 0xFF;

    bool clearColorWithRenderPassLoadOp   = clearColor && !maskedClearColor;
    bool clearStencilWithRenderPassLoadOp = clearStencil && !maskedClearStencil;

    // At least one of color, depth or stencil should be clearable with render pass loadOp for us
    // to use this clear path.
    bool clearAnyWithRenderPassLoadOp =
        clearColorWithRenderPassLoadOp || clearDepth || clearStencilWithRenderPassLoadOp;

    if (clearAnyWithRenderPassLoadOp)
    {
        // Clearing color is indicated by the set bits in this mask.  If not clearing colors with
        // render pass loadOp, the default value of all-zeros means the clear is not done in
        // clearWithRenderPassOp below.  In that case, only clear depth/stencil with render pass
        // loadOp.
        gl::DrawBufferMask clearBuffersWithRenderPassLoadOp;
        if (clearColorWithRenderPassLoadOp)
        {
            clearBuffersWithRenderPassLoadOp = clearColorBuffers;
        }
        ANGLE_TRY(clearWithRenderPassOp(
            contextVk, scissoredRenderArea, clearBuffersWithRenderPassLoadOp, clearDepth,
            clearStencilWithRenderPassLoadOp, clearColorValue, modifiedDepthStencilValue));

        // On some hardware, having inline commands at this point results in corrupted output.  In
        // that case, end the render pass immediately.  http://anglebug.com/2361
        if (contextVk->getRenderer()->getFeatures().restartRenderPassAfterLoadOpClear.enabled)
        {
            mFramebuffer.finishCurrentCommands(contextVk);
        }

        // Fallback to other methods for whatever isn't cleared here.
        clearDepth = false;
        if (clearColorWithRenderPassLoadOp)
        {
            clearColorBuffers.reset();
            clearColor = false;
        }
        if (clearStencilWithRenderPassLoadOp)
        {
            clearStencil = false;
        }

        // If nothing left to clear, early out.
        if (!clearColor && !clearStencil)
        {
            return angle::Result::Continue;
        }
    }

    // Note: depth clear is always done through render pass loadOp.
    ASSERT(clearDepth == false);

    // The most costly clear mode is when we need to mask out specific color channels or stencil
    // bits. This can only be done with a draw call.
    return clearWithDraw(contextVk, scissoredRenderArea, clearColorBuffers, clearStencil,
                         colorMaskFlags, stencilMask, clearColorValue,
                         static_cast<uint8_t>(modifiedDepthStencilValue.stencil));
}

angle::Result FramebufferVk::clearBufferfv(const gl::Context *context,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           const GLfloat *values)
{
    VkClearValue clearValue = {};

    bool clearDepth = false;
    gl::DrawBufferMask clearColorBuffers;

    if (buffer == GL_DEPTH)
    {
        clearDepth                    = true;
        clearValue.depthStencil.depth = values[0];
    }
    else
    {
        clearColorBuffers.set(drawbuffer);
        clearValue.color.float32[0] = values[0];
        clearValue.color.float32[1] = values[1];
        clearValue.color.float32[2] = values[2];
        clearValue.color.float32[3] = values[3];
    }

    return clearImpl(context, clearColorBuffers, clearDepth, false, clearValue.color,
                     clearValue.depthStencil);
}

angle::Result FramebufferVk::clearBufferuiv(const gl::Context *context,
                                            GLenum buffer,
                                            GLint drawbuffer,
                                            const GLuint *values)
{
    VkClearValue clearValue = {};

    gl::DrawBufferMask clearColorBuffers;
    clearColorBuffers.set(drawbuffer);

    clearValue.color.uint32[0] = values[0];
    clearValue.color.uint32[1] = values[1];
    clearValue.color.uint32[2] = values[2];
    clearValue.color.uint32[3] = values[3];

    return clearImpl(context, clearColorBuffers, false, false, clearValue.color,
                     clearValue.depthStencil);
}

angle::Result FramebufferVk::clearBufferiv(const gl::Context *context,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           const GLint *values)
{
    VkClearValue clearValue = {};

    bool clearStencil = false;
    gl::DrawBufferMask clearColorBuffers;

    if (buffer == GL_STENCIL)
    {
        clearStencil = true;
        clearValue.depthStencil.stencil =
            gl::clamp(values[0], 0, std::numeric_limits<uint8_t>::max());
    }
    else
    {
        clearColorBuffers.set(drawbuffer);
        clearValue.color.int32[0] = values[0];
        clearValue.color.int32[1] = values[1];
        clearValue.color.int32[2] = values[2];
        clearValue.color.int32[3] = values[3];
    }

    return clearImpl(context, clearColorBuffers, false, clearStencil, clearValue.color,
                     clearValue.depthStencil);
}

angle::Result FramebufferVk::clearBufferfi(const gl::Context *context,
                                           GLenum buffer,
                                           GLint drawbuffer,
                                           GLfloat depth,
                                           GLint stencil)
{
    VkClearValue clearValue = {};

    clearValue.depthStencil.depth   = depth;
    clearValue.depthStencil.stencil = gl::clamp(stencil, 0, std::numeric_limits<uint8_t>::max());

    return clearImpl(context, gl::DrawBufferMask(), true, true, clearValue.color,
                     clearValue.depthStencil);
}

GLenum FramebufferVk::getImplementationColorReadFormat(const gl::Context *context) const
{
    return GetReadAttachmentInfo(context, mRenderTargetCache.getColorRead(mState)).format;
}

GLenum FramebufferVk::getImplementationColorReadType(const gl::Context *context) const
{
    GLenum readType = GetReadAttachmentInfo(context, mRenderTargetCache.getColorRead(mState)).type;
    if (context->getClientMajorVersion() < 3 && readType == GL_HALF_FLOAT)
    {
        // GL_HALF_FLOAT was not introduced until GLES 3.0, and has a different value from
        // GL_HALF_FLOAT_OES
        readType = GL_HALF_FLOAT_OES;
    }
    return readType;
}

angle::Result FramebufferVk::readPixels(const gl::Context *context,
                                        const gl::Rectangle &area,
                                        GLenum format,
                                        GLenum type,
                                        void *pixels)
{
    // Clip read area to framebuffer.
    const gl::Extents &fbSize = getState().getReadAttachment()->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    ContextVk *contextVk = vk::GetImpl(context);

    gl::Rectangle clippedArea;
    if (!ClipRectangle(area, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }

    const gl::State &glState = contextVk->getState();
    gl::Buffer *packBuffer   = glState.getTargetBuffer(gl::BufferBinding::PixelPack);

    GLuint outputSkipBytes = 0;
    PackPixelsParams params;
    ANGLE_TRY(vk::ImageHelper::GetReadPixelsParams(contextVk, glState.getPackState(), packBuffer,
                                                   format, type, area, clippedArea, &params,
                                                   &outputSkipBytes));

    if (contextVk->isViewportFlipEnabledForReadFBO())
    {
        params.area.y          = fbRect.height - clippedArea.y - clippedArea.height;
        params.reverseRowOrder = !params.reverseRowOrder;
    }

    ANGLE_TRY(readPixelsImpl(contextVk, params.area, params, VK_IMAGE_ASPECT_COLOR_BIT,
                             getColorReadRenderTarget(),
                             static_cast<uint8_t *>(pixels) + outputSkipBytes));
    mReadPixelBuffer.releaseInFlightBuffers(contextVk);
    return angle::Result::Continue;
}

RenderTargetVk *FramebufferVk::getDepthStencilRenderTarget() const
{
    return mRenderTargetCache.getDepthStencil();
}

RenderTargetVk *FramebufferVk::getColorDrawRenderTarget(size_t colorIndex) const
{
    RenderTargetVk *renderTarget = mRenderTargetCache.getColorDraw(mState, colorIndex);
    ASSERT(renderTarget && renderTarget->getImage().valid());
    return renderTarget;
}

RenderTargetVk *FramebufferVk::getColorReadRenderTarget() const
{
    RenderTargetVk *renderTarget = mRenderTargetCache.getColorRead(mState);
    ASSERT(renderTarget && renderTarget->getImage().valid());
    return renderTarget;
}

angle::Result FramebufferVk::blitWithCommand(ContextVk *contextVk,
                                             const gl::Rectangle &sourceArea,
                                             const gl::Rectangle &destArea,
                                             RenderTargetVk *readRenderTarget,
                                             RenderTargetVk *drawRenderTarget,
                                             GLenum filter,
                                             bool colorBlit,
                                             bool depthBlit,
                                             bool stencilBlit,
                                             bool flipX,
                                             bool flipY)
{
    // Since blitRenderbufferRect is called for each render buffer that needs to be blitted,
    // it should never be the case that both color and depth/stencil need to be blitted at
    // at the same time.
    ASSERT(colorBlit != (depthBlit || stencilBlit));

    vk::ImageHelper *srcImage = &readRenderTarget->getImage();
    vk::ImageHelper *dstImage = drawRenderTarget->getImageForWrite(contextVk, &mFramebuffer);

    VkImageAspectFlags imageAspectMask = srcImage->getAspectFlags();
    VkImageAspectFlags blitAspectMask  = imageAspectMask;

    // Remove depth or stencil aspects if they are not requested to be blitted.
    if (!depthBlit)
    {
        blitAspectMask &= ~VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (!stencilBlit)
    {
        blitAspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (srcImage->isLayoutChangeNecessary(vk::ImageLayout::TransferSrc))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(srcImage->recordCommands(contextVk, &srcLayoutChange));
        srcImage->changeLayout(imageAspectMask, vk::ImageLayout::TransferSrc, srcLayoutChange);
    }

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &commandBuffer));

    srcImage->addReadDependency(contextVk, &mFramebuffer);

    VkImageBlit blit                   = {};
    blit.srcSubresource.aspectMask     = blitAspectMask;
    blit.srcSubresource.mipLevel       = readRenderTarget->getLevelIndex();
    blit.srcSubresource.baseArrayLayer = readRenderTarget->getLayerIndex();
    blit.srcSubresource.layerCount     = 1;
    blit.srcOffsets[0]                 = {sourceArea.x0(), sourceArea.y0(), 0};
    blit.srcOffsets[1]                 = {sourceArea.x1(), sourceArea.y1(), 1};
    blit.dstSubresource.aspectMask     = blitAspectMask;
    blit.dstSubresource.mipLevel       = drawRenderTarget->getLevelIndex();
    blit.dstSubresource.baseArrayLayer = drawRenderTarget->getLayerIndex();
    blit.dstSubresource.layerCount     = 1;
    blit.dstOffsets[0]                 = {destArea.x0(), destArea.y0(), 0};
    blit.dstOffsets[1]                 = {destArea.x1(), destArea.y1(), 1};

    // Requirement of the copyImageToBuffer, the dst image must be in
    // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL layout.
    dstImage->changeLayout(imageAspectMask, vk::ImageLayout::TransferDst, commandBuffer);

    commandBuffer->blitImage(srcImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             dstImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                             gl_vk::GetFilter(filter));

    return angle::Result::Continue;
}

angle::Result FramebufferVk::blit(const gl::Context *context,
                                  const gl::Rectangle &sourceAreaIn,
                                  const gl::Rectangle &destAreaIn,
                                  GLbitfield mask,
                                  GLenum filter)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();
    UtilsVk &utilsVk     = contextVk->getUtils();

    const gl::State &glState              = contextVk->getState();
    const gl::Framebuffer *srcFramebuffer = glState.getReadFramebuffer();

    const bool blitColorBuffer   = (mask & GL_COLOR_BUFFER_BIT) != 0;
    const bool blitDepthBuffer   = (mask & GL_DEPTH_BUFFER_BIT) != 0;
    const bool blitStencilBuffer = (mask & GL_STENCIL_BUFFER_BIT) != 0;

    const bool isResolve =
        srcFramebuffer->getCachedSamples(context, gl::AttachmentSampleType::Resource) > 1;

    FramebufferVk *srcFramebufferVk    = vk::GetImpl(srcFramebuffer);
    const bool srcFramebufferFlippedY  = contextVk->isViewportFlipEnabledForReadFBO();
    const bool destFramebufferFlippedY = contextVk->isViewportFlipEnabledForDrawFBO();

    gl::Rectangle sourceArea = sourceAreaIn;
    gl::Rectangle destArea   = destAreaIn;

    // Note: GLES (all 3.x versions) require source and dest area to be identical when
    // resolving.
    ASSERT(!isResolve ||
           (sourceArea.x == destArea.x && sourceArea.y == destArea.y &&
            sourceArea.width == destArea.width && sourceArea.height == destArea.height));

    const gl::Rectangle srcFramebufferDimensions =
        srcFramebufferVk->mState.getDimensions().toRect();

    // If the destination is flipped in either direction, we will flip the source instead so that
    // the destination area is always unflipped.
    sourceArea = sourceArea.flip(destArea.isReversedX(), destArea.isReversedY());
    destArea   = destArea.removeReversal();

    // Calculate the stretch factor prior to any clipping, as it needs to remain constant.
    const float stretch[2] = {
        std::abs(sourceArea.width / static_cast<float>(destArea.width)),
        std::abs(sourceArea.height / static_cast<float>(destArea.height)),
    };

    // First, clip the source area to framebuffer.  That requires transforming the dest area to
    // match the clipped source.
    gl::Rectangle absSourceArea = sourceArea.removeReversal();
    gl::Rectangle clippedSourceArea;
    if (!gl::ClipRectangle(srcFramebufferDimensions, absSourceArea, &clippedSourceArea))
    {
        return angle::Result::Continue;
    }

    // Resize the destination area based on the new size of source.  Note again that stretch is
    // calculated as SrcDimension/DestDimension.
    gl::Rectangle srcClippedDestArea;
    if (isResolve)
    {
        // Source and dest areas are identical in resolve.
        srcClippedDestArea = clippedSourceArea;
    }
    else if (clippedSourceArea == absSourceArea)
    {
        // If there was no clipping, keep dest area as is.
        srcClippedDestArea = destArea;
    }
    else
    {
        // Shift dest area's x0,y0,x1,y1 by as much as the source area's got shifted (taking
        // stretching into account)
        float x0Shift = std::round((clippedSourceArea.x - absSourceArea.x) / stretch[0]);
        float y0Shift = std::round((clippedSourceArea.y - absSourceArea.y) / stretch[1]);
        float x1Shift = std::round((absSourceArea.x1() - clippedSourceArea.x1()) / stretch[0]);
        float y1Shift = std::round((absSourceArea.y1() - clippedSourceArea.y1()) / stretch[1]);

        // If the source area was reversed in any direction, the shift should be applied in the
        // opposite direction as well.
        if (sourceArea.isReversedX())
        {
            std::swap(x0Shift, x1Shift);
        }

        if (sourceArea.isReversedY())
        {
            std::swap(y0Shift, y1Shift);
        }

        srcClippedDestArea.x = destArea.x0() + static_cast<int>(x0Shift);
        srcClippedDestArea.y = destArea.y0() + static_cast<int>(y0Shift);
        int x1               = destArea.x1() - static_cast<int>(x1Shift);
        int y1               = destArea.y1() - static_cast<int>(y1Shift);

        srcClippedDestArea.width  = x1 - srcClippedDestArea.x;
        srcClippedDestArea.height = y1 - srcClippedDestArea.y;
    }

    // If framebuffers are flipped in Y, flip the source and dest area (which define the
    // transformation regardless of clipping), as well as the blit area (which is the clipped
    // dest area).
    if (srcFramebufferFlippedY)
    {
        sourceArea.y      = srcFramebufferDimensions.height - sourceArea.y;
        sourceArea.height = -sourceArea.height;
    }
    if (destFramebufferFlippedY)
    {
        destArea.y      = mState.getDimensions().height - destArea.y;
        destArea.height = -destArea.height;

        srcClippedDestArea.y =
            mState.getDimensions().height - srcClippedDestArea.y - srcClippedDestArea.height;
    }

    const bool flipX = sourceArea.isReversedX() != destArea.isReversedX();
    const bool flipY = sourceArea.isReversedY() != destArea.isReversedY();

    // GLES doesn't allow flipping the parameters of glBlitFramebuffer if performing a resolve.
    ASSERT(!isResolve ||
           (flipX == false && flipY == (srcFramebufferFlippedY != destFramebufferFlippedY)));

    // Again, transfer the destination flip to source, so dest is unflipped.  Note that destArea
    // was not reversed until the final possible Y-flip.
    ASSERT(!destArea.isReversedX());
    sourceArea = sourceArea.flip(false, destArea.isReversedY());
    destArea   = destArea.removeReversal();

    // Clip the destination area to the framebuffer size and scissor.  Note that we don't care
    // about the source area anymore.  The offset translation is done based on the original source
    // and destination rectangles.  The stretch factor is already calculated as well.
    gl::Rectangle blitArea;
    if (!gl::ClipRectangle(getScissoredRenderArea(contextVk), srcClippedDestArea, &blitArea))
    {
        return angle::Result::Continue;
    }

    bool noClip = blitArea == destArea && stretch[0] == 1.0f && stretch[1] == 1.0f;
    bool noFlip = !flipX && !flipY;
    bool disableFlippingBlitWithCommand =
        contextVk->getRenderer()->getFeatures().disableFlippingBlitWithCommand.enabled;

    UtilsVk::BlitResolveParameters params;
    params.srcOffset[0]  = sourceArea.x;
    params.srcOffset[1]  = sourceArea.y;
    params.destOffset[0] = destArea.x;
    params.destOffset[1] = destArea.y;
    params.stretch[0]    = stretch[0];
    params.stretch[1]    = stretch[1];
    params.srcExtents[0] = srcFramebufferDimensions.width;
    params.srcExtents[1] = srcFramebufferDimensions.height;
    params.blitArea      = blitArea;
    params.linear        = filter == GL_LINEAR;
    params.flipX         = flipX;
    params.flipY         = flipY;

    if (blitColorBuffer)
    {
        RenderTargetVk *readRenderTarget = srcFramebufferVk->getColorReadRenderTarget();
        params.srcLayer                  = readRenderTarget->getLayerIndex();

        // Multisampled images are not allowed to have mips.
        ASSERT(!isResolve || readRenderTarget->getLevelIndex() == 0);

        // If there was no clipping and the format capabilities allow us, use Vulkan's builtin blit.
        // The reason clipping is prohibited in this path is that due to rounding errors, it would
        // be hard to guarantee the image stretching remains perfect.  That also allows us not to
        // have to transform back the dest clipping to source.
        //
        // For simplicity, we either blit all render targets with a Vulkan command, or none.
        bool canBlitWithCommand = !isResolve && noClip &&
                                  (noFlip || !disableFlippingBlitWithCommand) &&
                                  HasSrcBlitFeature(renderer, readRenderTarget);
        bool areChannelsBlitCompatible = true;
        for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
        {
            RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
            canBlitWithCommand =
                canBlitWithCommand && HasDstBlitFeature(renderer, drawRenderTarget);
            areChannelsBlitCompatible =
                areChannelsBlitCompatible &&
                areSrcAndDstColorChannelsBlitCompatible(readRenderTarget, drawRenderTarget);
        }

        if (canBlitWithCommand && areChannelsBlitCompatible)
        {
            for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
            {
                RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
                ANGLE_TRY(blitWithCommand(contextVk, sourceArea, destArea, readRenderTarget,
                                          drawRenderTarget, filter, true, false, false, flipX,
                                          flipY));
            }
        }
        // If we're not flipping, use Vulkan's builtin resolve.
        else if (isResolve && !flipX && !flipY && areChannelsBlitCompatible)
        {
            ANGLE_TRY(resolveColorWithCommand(contextVk, params, &readRenderTarget->getImage()));
        }
        // Otherwise use a shader to do blit or resolve.
        else
        {
            const vk::ImageView *readImageView = nullptr;
            ANGLE_TRY(readRenderTarget->getImageView(contextVk, &readImageView));
            readRenderTarget->onImageViewGraphAccess(contextVk);
            ANGLE_TRY(utilsVk.colorBlitResolve(contextVk, this, &readRenderTarget->getImage(),
                                               readImageView, params));
        }
    }

    if (blitDepthBuffer || blitStencilBuffer)
    {
        RenderTargetVk *readRenderTarget = srcFramebufferVk->getDepthStencilRenderTarget();
        RenderTargetVk *drawRenderTarget = mRenderTargetCache.getDepthStencil();
        params.srcLayer                  = readRenderTarget->getLayerIndex();

        // Multisampled images are not allowed to have mips.
        ASSERT(!isResolve || readRenderTarget->getLevelIndex() == 0);

        // Similarly, only blit if there's been no clipping.
        bool canBlitWithCommand = !isResolve && noClip &&
                                  (noFlip || !disableFlippingBlitWithCommand) &&
                                  HasSrcBlitFeature(renderer, readRenderTarget) &&
                                  HasDstBlitFeature(renderer, drawRenderTarget);
        bool areChannelsBlitCompatible =
            areSrcAndDstDepthStencilChannelsBlitCompatible(readRenderTarget, drawRenderTarget);

        if (canBlitWithCommand && areChannelsBlitCompatible)
        {
            ANGLE_TRY(blitWithCommand(contextVk, sourceArea, destArea, readRenderTarget,
                                      drawRenderTarget, filter, false, blitDepthBuffer,
                                      blitStencilBuffer, flipX, flipY));
        }
        else
        {
            // Create depth- and stencil-only views for reading.
            vk::DeviceScoped<vk::ImageView> depthView(contextVk->getDevice());
            vk::DeviceScoped<vk::ImageView> stencilView(contextVk->getDevice());

            vk::ImageHelper *depthStencilImage = &readRenderTarget->getImage();
            uint32_t levelIndex                = readRenderTarget->getLevelIndex();
            uint32_t layerIndex                = readRenderTarget->getLayerIndex();
            gl::TextureType textureType = vk::Get2DTextureType(depthStencilImage->getLayerCount(),
                                                               depthStencilImage->getSamples());

            if (blitDepthBuffer)
            {
                ANGLE_TRY(depthStencilImage->initLayerImageView(
                    contextVk, textureType, VK_IMAGE_ASPECT_DEPTH_BIT, gl::SwizzleState(),
                    &depthView.get(), levelIndex, 1, layerIndex, 1));
            }

            if (blitStencilBuffer)
            {
                ANGLE_TRY(depthStencilImage->initLayerImageView(
                    contextVk, textureType, VK_IMAGE_ASPECT_STENCIL_BIT, gl::SwizzleState(),
                    &stencilView.get(), levelIndex, 1, layerIndex, 1));
            }

            // If shader stencil export is not possible, defer stencil blit/stencil to another pass.
            bool hasShaderStencilExport =
                contextVk->getRenderer()->getFeatures().supportsShaderStencilExport.enabled;

            // Blit depth. If shader stencil export is present, blit stencil as well.
            if (blitDepthBuffer || (blitStencilBuffer && hasShaderStencilExport))
            {
                const vk::ImageView *depth = blitDepthBuffer ? &depthView.get() : nullptr;
                const vk::ImageView *stencil =
                    blitStencilBuffer && hasShaderStencilExport ? &stencilView.get() : nullptr;

                ANGLE_TRY(utilsVk.depthStencilBlitResolve(contextVk, this, depthStencilImage, depth,
                                                          stencil, params));
            }

            // If shader stencil export is not present, blit stencil through a different path.
            if (blitStencilBuffer && !hasShaderStencilExport)
            {
                ANGLE_TRY(utilsVk.stencilBlitResolveNoShaderExport(
                    contextVk, this, depthStencilImage, &stencilView.get(), params));
            }

            vk::ImageView depthViewObject   = depthView.release();
            vk::ImageView stencilViewObject = stencilView.release();

            contextVk->addGarbage(&depthViewObject);
            contextVk->addGarbage(&stencilViewObject);
        }
    }

    return angle::Result::Continue;
}  // namespace rx

angle::Result FramebufferVk::resolveColorWithCommand(ContextVk *contextVk,
                                                     const UtilsVk::BlitResolveParameters &params,
                                                     vk::ImageHelper *srcImage)
{
    if (srcImage->isLayoutChangeNecessary(vk::ImageLayout::TransferSrc))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(srcImage->recordCommands(contextVk, &srcLayoutChange));
        srcImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferSrc,
                               srcLayoutChange);
    }

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &commandBuffer));

    // Source's layout change should happen before rendering
    srcImage->addReadDependency(contextVk, &mFramebuffer);

    VkImageResolve resolveRegion                = {};
    resolveRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    resolveRegion.srcSubresource.mipLevel       = 0;
    resolveRegion.srcSubresource.baseArrayLayer = params.srcLayer;
    resolveRegion.srcSubresource.layerCount     = 1;
    resolveRegion.srcOffset.x                   = params.srcOffset[0];
    resolveRegion.srcOffset.y                   = params.srcOffset[1];
    resolveRegion.srcOffset.z                   = 0;
    resolveRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    resolveRegion.dstSubresource.layerCount     = 1;
    resolveRegion.dstOffset.x                   = params.destOffset[0];
    resolveRegion.dstOffset.y                   = params.destOffset[1];
    resolveRegion.dstOffset.z                   = 0;
    resolveRegion.extent.width                  = params.srcExtents[0];
    resolveRegion.extent.height                 = params.srcExtents[1];
    resolveRegion.extent.depth                  = 1;

    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
        vk::ImageHelper *drawImage = drawRenderTarget->getImageForWrite(contextVk, &mFramebuffer);
        drawImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst,
                                commandBuffer);

        resolveRegion.dstSubresource.mipLevel       = drawRenderTarget->getLevelIndex();
        resolveRegion.dstSubresource.baseArrayLayer = drawRenderTarget->getLayerIndex();

        srcImage->resolve(&drawRenderTarget->getImage(), resolveRegion, commandBuffer);
    }

    return angle::Result::Continue;
}

bool FramebufferVk::checkStatus(const gl::Context *context) const
{
    // if we have both a depth and stencil buffer, they must refer to the same object
    // since we only support packed_depth_stencil and not separate depth and stencil
    if (mState.hasSeparateDepthAndStencilAttachments())
    {
        return false;
    }

    return true;
}

angle::Result FramebufferVk::updateColorAttachment(const gl::Context *context, size_t colorIndexGL)
{
    ContextVk *contextVk = vk::GetImpl(context);

    ANGLE_TRY(mRenderTargetCache.updateColorRenderTarget(context, mState, colorIndexGL));

    // Update cached masks for masked clears.
    RenderTargetVk *renderTarget = mRenderTargetCache.getColors()[colorIndexGL];
    if (renderTarget)
    {
        const angle::Format &actualFormat = renderTarget->getImageFormat().actualImageFormat();
        updateActiveColorMasks(colorIndexGL, actualFormat.redBits > 0, actualFormat.greenBits > 0,
                               actualFormat.blueBits > 0, actualFormat.alphaBits > 0);

        const angle::Format &sourceFormat = renderTarget->getImageFormat().intendedFormat();
        mEmulatedAlphaAttachmentMask.set(colorIndexGL,
                                         sourceFormat.alphaBits == 0 && actualFormat.alphaBits > 0);

        contextVk->updateColorMask(context->getState().getBlendState());
    }
    else
    {
        updateActiveColorMasks(colorIndexGL, false, false, false, false);
    }

    return angle::Result::Continue;
}

void FramebufferVk::invalidateImpl(ContextVk *contextVk, size_t count, const GLenum *attachments)
{
    ASSERT(mFramebuffer.hasStartedRenderPass());

    gl::DrawBufferMask invalidateColorBuffers;
    bool invalidateDepthBuffer   = false;
    bool invalidateStencilBuffer = false;

    for (size_t i = 0; i < count; ++i)
    {
        const GLenum attachment = attachments[i];

        switch (attachment)
        {
            case GL_DEPTH:
            case GL_DEPTH_ATTACHMENT:
                invalidateDepthBuffer = true;
                break;
            case GL_STENCIL:
            case GL_STENCIL_ATTACHMENT:
                invalidateStencilBuffer = true;
                break;
            case GL_DEPTH_STENCIL_ATTACHMENT:
                invalidateDepthBuffer   = true;
                invalidateStencilBuffer = true;
                break;
            default:
                ASSERT(
                    (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) ||
                    (attachment == GL_COLOR));

                invalidateColorBuffers.set(
                    attachment == GL_COLOR ? 0u : (attachment - GL_COLOR_ATTACHMENT0));
        }
    }

    // Set the appropriate storeOp for attachments.
    size_t attachmentIndexVk = 0;
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        if (invalidateColorBuffers.test(colorIndexGL))
        {
            mFramebuffer.invalidateRenderPassColorAttachment(attachmentIndexVk);
        }
        ++attachmentIndexVk;
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        if (invalidateDepthBuffer)
        {
            mFramebuffer.invalidateRenderPassDepthAttachment(attachmentIndexVk);
        }

        if (invalidateStencilBuffer)
        {
            mFramebuffer.invalidateRenderPassStencilAttachment(attachmentIndexVk);
        }
    }

    // NOTE: Possible future optimization is to delay setting the storeOp and only do so if the
    // render pass is closed by itself before another draw call.  Otherwise, in a situation like
    // this:
    //
    //     draw()
    //     invalidate()
    //     draw()
    //
    // We would be discarding the attachments only to load them for the next draw (which is less
    // efficient than keeping the render pass open and not do the discard at all).  While dEQP tests
    // this pattern, this optimization may not be necessary if no application does this.  It is
    // expected that an application would invalidate() when it's done with the framebuffer, so the
    // render pass would have closed either way.
    mFramebuffer.finishCurrentCommands(contextVk);
}

angle::Result FramebufferVk::syncState(const gl::Context *context,
                                       const gl::Framebuffer::DirtyBits &dirtyBits)
{
    ContextVk *contextVk = vk::GetImpl(context);

    ASSERT(dirtyBits.any());
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
                ANGLE_TRY(mRenderTargetCache.updateDepthStencilRenderTarget(context, mState));
                break;
            case gl::Framebuffer::DIRTY_BIT_DEPTH_BUFFER_CONTENTS:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_BUFFER_CONTENTS:
                ANGLE_TRY(mRenderTargetCache.getDepthStencil()->flushStagedUpdates(contextVk));
                break;
            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
                ANGLE_TRY(mRenderTargetCache.update(context, mState, dirtyBits));
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                break;
            default:
            {
                static_assert(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0, "FB dirty bits");
                if (dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX)
                {
                    size_t colorIndexGL = static_cast<size_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                    ANGLE_TRY(updateColorAttachment(context, colorIndexGL));
                }
                else
                {
                    ASSERT(dirtyBit >= gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0 &&
                           dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_MAX);
                    size_t colorIndexGL = static_cast<size_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0);
                    ANGLE_TRY(mRenderTargetCache.getColors()[colorIndexGL]->flushStagedUpdates(
                        contextVk));
                }
            }
        }
    }

    // The FBOs new attachment may have changed the renderable area
    const gl::State &glState = context->getState();
    contextVk->updateScissor(glState);

    mActiveColorComponents = gl_vk::GetColorComponentFlags(
        mActiveColorComponentMasksForClear[0].any(), mActiveColorComponentMasksForClear[1].any(),
        mActiveColorComponentMasksForClear[2].any(), mActiveColorComponentMasksForClear[3].any());

    mFramebuffer.release(contextVk);

    // Will freeze the current set of dependencies on this FBO. The next time we render we will
    // create a new entry in the command graph.
    mFramebuffer.finishCurrentCommands(contextVk);

    // Notify the ContextVk to update the pipeline desc.
    updateRenderPassDesc();

    FramebufferVk *currentDrawFramebuffer = vk::GetImpl(context->getState().getDrawFramebuffer());
    if (currentDrawFramebuffer == this)
    {
        contextVk->onDrawFramebufferChange(this);
    }

    return angle::Result::Continue;
}

void FramebufferVk::updateRenderPassDesc()
{
    mRenderPassDesc = {};
    mRenderPassDesc.setSamples(getSamples());

    const auto &colorRenderTargets              = mRenderTargetCache.getColors();
    const gl::DrawBufferMask enabledDrawBuffers = mState.getEnabledDrawBuffers();
    for (size_t colorIndexGL = 0; colorIndexGL < enabledDrawBuffers.size(); ++colorIndexGL)
    {
        if (enabledDrawBuffers[colorIndexGL])
        {
            RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
            ASSERT(colorRenderTarget);
            mRenderPassDesc.packColorAttachment(
                colorIndexGL, colorRenderTarget->getImage().getFormat().intendedFormatID);
        }
        else
        {
            mRenderPassDesc.packColorAttachmentGap(colorIndexGL);
        }
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        mRenderPassDesc.packDepthStencilAttachment(
            depthStencilRenderTarget->getImage().getFormat().intendedFormatID);
    }
}

angle::Result FramebufferVk::getFramebuffer(ContextVk *contextVk, vk::Framebuffer **framebufferOut)
{
    // If we've already created our cached Framebuffer, return it.
    if (mFramebuffer.valid())
    {
        *framebufferOut = &mFramebuffer.getFramebuffer();
        return angle::Result::Continue;
    }

    vk::RenderPass *compatibleRenderPass = nullptr;
    ANGLE_TRY(contextVk->getCompatibleRenderPass(mRenderPassDesc, &compatibleRenderPass));

    // If we've a Framebuffer provided by a Surface (default FBO/backbuffer), query it.
    if (mBackbuffer)
    {
        return mBackbuffer->getCurrentFramebuffer(contextVk, *compatibleRenderPass, framebufferOut);
    }

    // Gather VkImageViews over all FBO attachments, also size of attached region.
    std::vector<VkImageView> attachments;
    gl::Extents attachmentsSize;

    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(colorRenderTarget->getImageView(contextVk, &imageView));

        attachments.push_back(imageView->getHandle());

        ASSERT(attachmentsSize.empty() || attachmentsSize == colorRenderTarget->getExtents());
        attachmentsSize = colorRenderTarget->getExtents();
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(depthStencilRenderTarget->getImageView(contextVk, &imageView));

        attachments.push_back(imageView->getHandle());

        ASSERT(attachmentsSize.empty() ||
               attachmentsSize == depthStencilRenderTarget->getExtents());
        attachmentsSize = depthStencilRenderTarget->getExtents();
    }

    if (attachmentsSize.empty())
    {
        // No attachments, so use the default values.
        attachmentsSize.height = mState.getDefaultHeight();
        attachmentsSize.width  = mState.getDefaultWidth();
        attachmentsSize.depth  = 0;
    }

    VkFramebufferCreateInfo framebufferInfo = {};

    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.flags           = 0;
    framebufferInfo.renderPass      = compatibleRenderPass->getHandle();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = static_cast<uint32_t>(attachmentsSize.width);
    framebufferInfo.height          = static_cast<uint32_t>(attachmentsSize.height);
    framebufferInfo.layers          = 1;

    ANGLE_TRY(mFramebuffer.init(contextVk, framebufferInfo));

    *framebufferOut = &mFramebuffer.getFramebuffer();
    return angle::Result::Continue;
}

angle::Result FramebufferVk::clearWithRenderPassOp(
    ContextVk *contextVk,
    const gl::Rectangle &clearArea,
    gl::DrawBufferMask clearColorBuffers,
    bool clearDepth,
    bool clearStencil,
    const VkClearColorValue &clearColorValue,
    const VkClearDepthStencilValue &clearDepthStencilValue)
{
    // Start a new render pass if:
    //
    // - no render pass has started,
    // - there is a render pass started but it contains commands; we cannot modify its ops, so new
    // render pass is needed,
    // - the current render area doesn't match the clear area.  We need the render area to be
    // exactly as specified by the scissor for the loadOp to clear only that area.  See
    // onScissorChange for more information.

    if (!mFramebuffer.valid() || !mFramebuffer.renderPassStartedButEmpty() ||
        mFramebuffer.getRenderPassRenderArea() != clearArea)
    {
        vk::CommandBuffer *commandBuffer;
        ANGLE_TRY(startNewRenderPass(contextVk, clearArea, &commandBuffer));
    }

    size_t attachmentIndexVk = 0;

    // Go through clearColorBuffers and set the appropriate loadOp and clear values.
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        if (clearColorBuffers.test(colorIndexGL))
        {
            RenderTargetVk *renderTarget = getColorDrawRenderTarget(colorIndexGL);

            // If the render target doesn't have alpha, but its emulated format has it, clear the
            // alpha to 1.
            VkClearColorValue value = clearColorValue;
            if (mEmulatedAlphaAttachmentMask[colorIndexGL])
            {
                SetEmulatedAlphaValue(renderTarget->getImageFormat(), &value);
            }

            mFramebuffer.clearRenderPassColorAttachment(attachmentIndexVk, value);
        }
        ++attachmentIndexVk;
    }

    // Set the appropriate loadOp and clear values for depth and stencil.
    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        if (clearDepth)
        {
            mFramebuffer.clearRenderPassDepthAttachment(attachmentIndexVk,
                                                        clearDepthStencilValue.depth);
        }

        if (clearStencil)
        {
            mFramebuffer.clearRenderPassStencilAttachment(attachmentIndexVk,
                                                          clearDepthStencilValue.stencil);
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::clearWithDraw(ContextVk *contextVk,
                                           const gl::Rectangle &clearArea,
                                           gl::DrawBufferMask clearColorBuffers,
                                           bool clearStencil,
                                           VkColorComponentFlags colorMaskFlags,
                                           uint8_t stencilMask,
                                           const VkClearColorValue &clearColorValue,
                                           uint8_t clearStencilValue)
{
    UtilsVk::ClearFramebufferParameters params = {};
    params.clearArea                           = clearArea;
    params.colorClearValue                     = clearColorValue;
    params.stencilClearValue                   = clearStencilValue;
    params.stencilMask                         = stencilMask;

    params.clearColor   = true;
    params.clearStencil = clearStencil;

    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : clearColorBuffers)
    {
        const RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        params.colorFormat = &colorRenderTarget->getImage().getFormat().actualImageFormat();
        params.colorAttachmentIndexGL = static_cast<uint32_t>(colorIndexGL);
        params.colorMaskFlags         = colorMaskFlags;
        if (mEmulatedAlphaAttachmentMask[colorIndexGL])
        {
            params.colorMaskFlags &= ~VK_COLOR_COMPONENT_A_BIT;
        }

        ANGLE_TRY(contextVk->getUtils().clearFramebuffer(contextVk, this, params));

        // Clear stencil only once!
        params.clearStencil = false;
    }

    // If there was no color clear, clear stencil alone.
    if (params.clearStencil)
    {
        params.clearColor = false;
        ANGLE_TRY(contextVk->getUtils().clearFramebuffer(contextVk, this, params));
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::getSamplePosition(const gl::Context *context,
                                               size_t index,
                                               GLfloat *xy) const
{
    int sampleCount = getSamples();
    rx::GetSamplePosition(sampleCount, index, xy);
    return angle::Result::Continue;
}

angle::Result FramebufferVk::startNewRenderPass(ContextVk *contextVk,
                                                const gl::Rectangle &renderArea,
                                                vk::CommandBuffer **commandBufferOut)
{
    vk::Framebuffer *framebuffer = nullptr;
    ANGLE_TRY(getFramebuffer(contextVk, &framebuffer));

    vk::AttachmentOpsArray renderPassAttachmentOps;
    std::vector<VkClearValue> attachmentClearValues;

    vk::CommandBuffer *writeCommands = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &writeCommands));

    // Initialize RenderPass info.
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        ANGLE_TRY(colorRenderTarget->onColorDraw(contextVk, &mFramebuffer, writeCommands));

        renderPassAttachmentOps.initWithLoadStore(attachmentClearValues.size(),
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        attachmentClearValues.emplace_back(kUninitializedClearValue);
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        ANGLE_TRY(
            depthStencilRenderTarget->onDepthStencilDraw(contextVk, &mFramebuffer, writeCommands));

        renderPassAttachmentOps.initWithLoadStore(attachmentClearValues.size(),
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        attachmentClearValues.emplace_back(kUninitializedClearValue);
    }

    return mFramebuffer.beginRenderPass(contextVk, *framebuffer, renderArea, mRenderPassDesc,
                                        renderPassAttachmentOps, attachmentClearValues,
                                        commandBufferOut);
}

void FramebufferVk::updateActiveColorMasks(size_t colorIndexGL, bool r, bool g, bool b, bool a)
{
    mActiveColorComponentMasksForClear[0].set(colorIndexGL, r);
    mActiveColorComponentMasksForClear[1].set(colorIndexGL, g);
    mActiveColorComponentMasksForClear[2].set(colorIndexGL, b);
    mActiveColorComponentMasksForClear[3].set(colorIndexGL, a);
}

const gl::DrawBufferMask &FramebufferVk::getEmulatedAlphaAttachmentMask() const
{
    return mEmulatedAlphaAttachmentMask;
}

angle::Result FramebufferVk::readPixelsImpl(ContextVk *contextVk,
                                            const gl::Rectangle &area,
                                            const PackPixelsParams &packPixelsParams,
                                            VkImageAspectFlagBits copyAspectFlags,
                                            RenderTargetVk *renderTarget,
                                            void *pixels)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "FramebufferVk::readPixelsImpl");
    uint32_t level = renderTarget->getLevelIndex();
    uint32_t layer = renderTarget->getLayerIndex();
    return renderTarget->getImage().readPixels(contextVk, area, packPixelsParams, copyAspectFlags,
                                               level, layer, pixels, &mReadPixelBuffer);
}

gl::Extents FramebufferVk::getReadImageExtents() const
{
    RenderTargetVk *readRenderTarget = mRenderTargetCache.getColorRead(mState);

    ASSERT(readRenderTarget->getExtents().width == mState.getDimensions().width);
    ASSERT(readRenderTarget->getExtents().height == mState.getDimensions().height);

    return readRenderTarget->getExtents();
}

gl::Rectangle FramebufferVk::getCompleteRenderArea() const
{
    const gl::Box &dimensions = mState.getDimensions();
    return gl::Rectangle(0, 0, dimensions.width, dimensions.height);
}

gl::Rectangle FramebufferVk::getScissoredRenderArea(ContextVk *contextVk) const
{
    const gl::Box &dimensions = mState.getDimensions();
    const gl::Rectangle renderArea(0, 0, dimensions.width, dimensions.height);
    bool invertViewport = contextVk->isViewportFlipEnabledForDrawFBO();

    return ClipRectToScissor(contextVk->getState(), renderArea, invertViewport);
}

void FramebufferVk::onScissorChange(ContextVk *contextVk)
{
    gl::Rectangle scissoredRenderArea = getScissoredRenderArea(contextVk);

    // If the scissor has grown beyond the previous scissoredRenderArea, make sure the render pass
    // is restarted.  Otherwise, we can continue using the same renderpass area.
    //
    // Without a scissor, the render pass area covers the whole of the framebuffer.  With a
    // scissored clear, the render pass area could be smaller than the framebuffer size.  When the
    // scissor changes, if the scissor area is completely encompassed by the render pass area, it's
    // possible to continue using the same render pass.  However, if the current render pass area
    // is too small, we need to start a new one.  The latter can happen if a scissored clear starts
    // a render pass, the scissor is disabled and a draw call is issued to affect the whole
    // framebuffer.
    mFramebuffer.updateCurrentAccessNodes();
    if (mFramebuffer.hasStartedRenderPass() &&
        !mFramebuffer.getRenderPassRenderArea().encloses(scissoredRenderArea))
    {
        mFramebuffer.finishCurrentCommands(contextVk);
    }
}

RenderTargetVk *FramebufferVk::getFirstRenderTarget() const
{
    for (auto *renderTarget : mRenderTargetCache.getColors())
    {
        if (renderTarget)
        {
            return renderTarget;
        }
    }

    return mRenderTargetCache.getDepthStencil();
}

GLint FramebufferVk::getSamples() const
{
    RenderTargetVk *firstRT = getFirstRenderTarget();
    return firstRT ? firstRT->getImage().getSamples() : 0;
}

}  // namespace rx
