//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferVk.cpp:
//    Implements the class methods for FramebufferVk.
//

#include "libANGLE/renderer/vulkan/FramebufferVk.h"

#include <vulkan/vulkan.h>
#include <array>

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
#include "third_party/trace_event/trace_event.h"

namespace rx
{

namespace
{
// The value to assign an alpha channel that's emulated.  The type is unsigned int, though it will
// automatically convert to the actual data type.
constexpr unsigned int kEmulatedAlphaValue = 1;

constexpr size_t kMinReadPixelsBufferSize = 128000;
// Clear values are only used when loadOp=Clear is set in clearWithRenderPassOp.  When starting a
// new render pass, the clear value is set to an unlikely value (bright pink) to stand out better
// in case of a bug.
constexpr VkClearValue kUninitializedClearValue = {{{0.95, 0.05, 0.95, 0.95}}};

const gl::InternalFormat &GetReadAttachmentInfo(const gl::Context *context,
                                                RenderTargetVk *renderTarget)
{
    GLenum implFormat =
        renderTarget->getImageFormat().imageFormat().fboImplementationInternalFormat;
    return gl::GetSizedInternalFormatInfo(implFormat);
}

bool ClipToRenderTarget(const gl::Rectangle &area,
                        RenderTargetVk *renderTarget,
                        gl::Rectangle *rectOut)
{
    const gl::Extents renderTargetExtents = renderTarget->getExtents();
    gl::Rectangle renderTargetRect(0, 0, renderTargetExtents.width, renderTargetExtents.height);
    return ClipRectangle(area, renderTargetRect, rectOut);
}

bool HasSrcAndDstBlitProperties(RendererVk *renderer,
                                RenderTargetVk *srcRenderTarget,
                                RenderTargetVk *dstRenderTarget)
{
    const VkFormat srcFormat = srcRenderTarget->getImageFormat().vkImageFormat;
    const VkFormat dstFormat = dstRenderTarget->getImageFormat().vkImageFormat;

    // Verifies if the draw and read images have the necessary prerequisites for blitting.
    return renderer->hasImageFormatFeatureBits(srcFormat, VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
           renderer->hasImageFormatFeatureBits(dstFormat, VK_FORMAT_FEATURE_BLIT_DST_BIT);
}

// Special rules apply to VkBufferImageCopy with depth/stencil. The components are tightly packed
// into a depth or stencil section of the destination buffer. See the spec:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkBufferImageCopy.html
const angle::Format &GetDepthStencilImageToBufferFormat(const angle::Format &imageFormat,
                                                        VkImageAspectFlagBits copyAspect)
{
    if (copyAspect == VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        ASSERT(imageFormat.id == angle::FormatID::D24_UNORM_S8_UINT ||
               imageFormat.id == angle::FormatID::D32_FLOAT_S8X24_UINT ||
               imageFormat.id == angle::FormatID::S8_UINT);
        return angle::Format::Get(angle::FormatID::S8_UINT);
    }

    ASSERT(copyAspect == VK_IMAGE_ASPECT_DEPTH_BIT);

    switch (imageFormat.id)
    {
        case angle::FormatID::D16_UNORM:
            return imageFormat;
        case angle::FormatID::D24_UNORM_X8_UINT:
            return imageFormat;
        case angle::FormatID::D24_UNORM_S8_UINT:
            return angle::Format::Get(angle::FormatID::D24_UNORM_X8_UINT);
        case angle::FormatID::D32_FLOAT:
            return imageFormat;
        case angle::FormatID::D32_FLOAT_S8X24_UINT:
            return angle::Format::Get(angle::FormatID::D32_FLOAT);
        default:
            UNREACHABLE();
            return imageFormat;
    }
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
    : FramebufferImpl(state),
      mBackbuffer(backbuffer),
      mActiveColorComponents(0),
      mReadPixelBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, kMinReadPixelsBufferSize, true),
      mBlitPixelBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, kMinReadPixelsBufferSize, true)
{
    mBlitPixelBuffer.init(1, renderer);
    mReadPixelBuffer.init(4, renderer);
}

FramebufferVk::~FramebufferVk() = default;

void FramebufferVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();
    mFramebuffer.release(renderer);

    mReadPixelBuffer.release(renderer);
    mBlitPixelBuffer.release(renderer);
}

angle::Result FramebufferVk::discard(const gl::Context *context,
                                     size_t count,
                                     const GLenum *attachments)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result FramebufferVk::invalidate(const gl::Context *context,
                                        size_t count,
                                        const GLenum *attachments)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result FramebufferVk::invalidateSub(const gl::Context *context,
                                           size_t count,
                                           const GLenum *attachments,
                                           const gl::Rectangle &area)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
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

    mFramebuffer.updateQueueSerial(contextVk->getRenderer()->getCurrentQueueSerial());

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
        if (contextVk->getRenderer()->getFeatures().restartRenderPassAfterLoadOpClear)
        {
            mFramebuffer.finishCurrentCommands(contextVk->getRenderer());
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
    return GetReadAttachmentInfo(context, mRenderTargetCache.getColorRead(mState)).type;
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
    RendererVk *renderer = contextVk->getRenderer();

    gl::Rectangle clippedArea;
    if (!ClipRectangle(area, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }
    gl::Rectangle flippedArea = clippedArea;
    if (contextVk->isViewportFlipEnabledForReadFBO())
    {
        flippedArea.y = fbRect.height - flippedArea.y - flippedArea.height;
    }

    const gl::State &glState            = context->getState();
    const gl::PixelPackState &packState = glState.getPackState();

    const gl::InternalFormat &sizedFormatInfo = gl::GetInternalFormatInfo(format, type);

    GLuint outputPitch = 0;
    ANGLE_VK_CHECK_MATH(contextVk,
                        sizedFormatInfo.computeRowPitch(type, area.width, packState.alignment,
                                                        packState.rowLength, &outputPitch));
    GLuint outputSkipBytes = 0;
    ANGLE_VK_CHECK_MATH(contextVk, sizedFormatInfo.computeSkipBytes(type, outputPitch, 0, packState,
                                                                    false, &outputSkipBytes));

    outputSkipBytes += (clippedArea.x - area.x) * sizedFormatInfo.pixelBytes +
                       (clippedArea.y - area.y) * outputPitch;

    const angle::Format &angleFormat = GetFormatFromFormatType(format, type);

    PackPixelsParams params(flippedArea, angleFormat, outputPitch, packState.reverseRowOrder,
                            glState.getTargetBuffer(gl::BufferBinding::PixelPack), 0);
    if (contextVk->isViewportFlipEnabledForReadFBO())
    {
        params.reverseRowOrder = !params.reverseRowOrder;
    }

    ANGLE_TRY(readPixelsImpl(contextVk, flippedArea, params, VK_IMAGE_ASPECT_COLOR_BIT,
                             getColorReadRenderTarget(),
                             static_cast<uint8_t *>(pixels) + outputSkipBytes));
    mReadPixelBuffer.releaseRetainedBuffers(renderer);
    return angle::Result::Continue;
}

RenderTargetVk *FramebufferVk::getDepthStencilRenderTarget() const
{
    return mRenderTargetCache.getDepthStencil();
}

angle::Result FramebufferVk::blitWithCopy(ContextVk *contextVk,
                                          const gl::Rectangle &copyArea,
                                          RenderTargetVk *readRenderTarget,
                                          RenderTargetVk *drawRenderTarget,
                                          bool blitDepthBuffer,
                                          bool blitStencilBuffer)
{
    VkImageAspectFlags aspectMask =
        vk::GetDepthStencilAspectFlagsForCopy(blitDepthBuffer, blitStencilBuffer);

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &commandBuffer));

    vk::ImageHelper *writeImage = drawRenderTarget->getImageForWrite(&mFramebuffer);
    writeImage->changeLayout(writeImage->getAspectFlags(), vk::ImageLayout::TransferDst,
                             commandBuffer);

    vk::ImageHelper *readImage = readRenderTarget->getImageForRead(
        &mFramebuffer, vk::ImageLayout::TransferSrc, commandBuffer);

    VkImageSubresourceLayers readSubresource = {};
    readSubresource.aspectMask               = aspectMask;
    readSubresource.mipLevel                 = 0;
    readSubresource.baseArrayLayer           = 0;
    readSubresource.layerCount               = 1;

    VkImageSubresourceLayers writeSubresource = readSubresource;

    vk::ImageHelper::Copy(readImage, writeImage, gl::Offset(), gl::Offset(),
                          gl::Extents(copyArea.width, copyArea.height, 1), readSubresource,
                          writeSubresource, commandBuffer);
    return angle::Result::Continue;
}

RenderTargetVk *FramebufferVk::getColorReadRenderTarget() const
{
    RenderTargetVk *renderTarget = mRenderTargetCache.getColorRead(mState);
    ASSERT(renderTarget && renderTarget->getImage().valid());
    return renderTarget;
}

angle::Result FramebufferVk::blitWithReadback(ContextVk *contextVk,
                                              const gl::Rectangle &copyArea,
                                              VkImageAspectFlagBits aspect,
                                              RenderTargetVk *readRenderTarget,
                                              RenderTargetVk *drawRenderTarget)
{
    RendererVk *renderer            = contextVk->getRenderer();
    const angle::Format &readFormat = readRenderTarget->getImageFormat().imageFormat();

    ASSERT(aspect == VK_IMAGE_ASPECT_DEPTH_BIT || aspect == VK_IMAGE_ASPECT_STENCIL_BIT);

    // This path is only currently used for y-flipping depth/stencil blits.
    PackPixelsParams packPixelsParams;
    packPixelsParams.reverseRowOrder = true;
    packPixelsParams.area.width      = copyArea.width;
    packPixelsParams.area.height     = copyArea.height;
    packPixelsParams.area.x          = copyArea.x;
    packPixelsParams.area.y          = copyArea.y;

    // Read back depth values into the destination buffer.
    const angle::Format &copyFormat = GetDepthStencilImageToBufferFormat(readFormat, aspect);
    packPixelsParams.destFormat     = &copyFormat;
    packPixelsParams.outputPitch    = copyFormat.pixelBytes * copyArea.width;

    // Allocate a space in the destination buffer to write to.
    size_t blitAllocationSize = copyFormat.pixelBytes * copyArea.width * copyArea.height;
    uint8_t *destPtr          = nullptr;
    VkBuffer destBufferHandle = VK_NULL_HANDLE;
    VkDeviceSize destOffset   = 0;
    ANGLE_TRY(mBlitPixelBuffer.allocate(contextVk, blitAllocationSize, &destPtr, &destBufferHandle,
                                        &destOffset, nullptr));

    ANGLE_TRY(
        readPixelsImpl(contextVk, copyArea, packPixelsParams, aspect, readRenderTarget, destPtr));

    VkBufferImageCopy copyRegion               = {};
    copyRegion.bufferOffset                    = destOffset;
    copyRegion.bufferImageHeight               = copyArea.height;
    copyRegion.bufferRowLength                 = copyArea.width;
    copyRegion.imageExtent.width               = copyArea.width;
    copyRegion.imageExtent.height              = copyArea.height;
    copyRegion.imageExtent.depth               = 1;
    copyRegion.imageSubresource.mipLevel       = 0;
    copyRegion.imageSubresource.aspectMask     = aspect;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount     = 1;
    copyRegion.imageOffset.x                   = copyArea.x;
    copyRegion.imageOffset.y                   = copyArea.y;
    copyRegion.imageOffset.z                   = 0;

    ANGLE_TRY(mBlitPixelBuffer.flush(contextVk));

    // Reinitialize the commandBuffer after a read pixels because it calls
    // renderer->finish which makes command buffers obsolete.
    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &commandBuffer));

    // We read the bytes of the image in a buffer, now we have to copy them into the
    // destination target.
    vk::ImageHelper *imageForWrite = drawRenderTarget->getImageForWrite(&mFramebuffer);

    imageForWrite->changeLayout(imageForWrite->getAspectFlags(), vk::ImageLayout::TransferDst,
                                commandBuffer);

    commandBuffer->copyBufferToImage(destBufferHandle, imageForWrite->getImage(),
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    mBlitPixelBuffer.releaseRetainedBuffers(renderer);

    return angle::Result::Continue;
}

angle::Result FramebufferVk::blit(const gl::Context *context,
                                  const gl::Rectangle &sourceArea,
                                  const gl::Rectangle &destArea,
                                  GLbitfield mask,
                                  GLenum filter)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    const gl::State &glState                 = context->getState();
    const gl::Framebuffer *sourceFramebuffer = glState.getReadFramebuffer();
    bool blitColorBuffer                     = (mask & GL_COLOR_BUFFER_BIT) != 0;
    bool blitDepthBuffer                     = (mask & GL_DEPTH_BUFFER_BIT) != 0;
    bool blitStencilBuffer                   = (mask & GL_STENCIL_BUFFER_BIT) != 0;

    FramebufferVk *sourceFramebufferVk = vk::GetImpl(sourceFramebuffer);
    bool flipSource                    = contextVk->isViewportFlipEnabledForReadFBO();
    bool flipDest                      = contextVk->isViewportFlipEnabledForDrawFBO();

    gl::Rectangle readRect = sourceArea;
    gl::Rectangle drawRect = destArea;

    if (glState.isScissorTestEnabled())
    {
        const gl::Rectangle scissorRect = glState.getScissor();
        if (!ClipRectangle(sourceArea, scissorRect, &readRect))
        {
            return angle::Result::Continue;
        }

        if (!ClipRectangle(destArea, scissorRect, &drawRect))
        {
            return angle::Result::Continue;
        }
    }

    // After cropping for the scissor, we also want to crop for the size of the buffers.

    if (blitColorBuffer)
    {
        RenderTargetVk *readRenderTarget = sourceFramebufferVk->getColorReadRenderTarget();

        gl::Rectangle readRenderTargetRect;
        if (!ClipToRenderTarget(readRect, readRenderTarget, &readRenderTargetRect))
        {
            return angle::Result::Continue;
        }

        for (size_t colorAttachment : mState.getEnabledDrawBuffers())
        {
            RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorAttachment];
            ASSERT(drawRenderTarget);
            ASSERT(HasSrcAndDstBlitProperties(renderer, readRenderTarget, drawRenderTarget));

            gl::Rectangle drawRenderTargetRect;
            if (!ClipToRenderTarget(drawRect, drawRenderTarget, &drawRenderTargetRect))
            {
                return angle::Result::Continue;
            }

            ANGLE_TRY(blitWithCommand(contextVk, readRenderTargetRect, drawRenderTargetRect,
                                      readRenderTarget, drawRenderTarget, filter, true, false,
                                      false, flipSource, flipDest));
        }
    }

    if (blitDepthBuffer || blitStencilBuffer)
    {
        RenderTargetVk *readRenderTarget = sourceFramebufferVk->getDepthStencilRenderTarget();
        ASSERT(readRenderTarget);

        gl::Rectangle readRenderTargetRect;
        if (!ClipToRenderTarget(readRect, readRenderTarget, &readRenderTargetRect))
        {
            return angle::Result::Continue;
        }

        RenderTargetVk *drawRenderTarget = mRenderTargetCache.getDepthStencil();
        ASSERT(drawRenderTarget);

        gl::Rectangle drawRenderTargetRect;
        if (!ClipToRenderTarget(drawRect, drawRenderTarget, &drawRenderTargetRect))
        {
            return angle::Result::Continue;
        }

        ASSERT(readRenderTargetRect == drawRenderTargetRect);
        ASSERT(filter == GL_NEAREST);

        if (HasSrcAndDstBlitProperties(renderer, readRenderTarget, drawRenderTarget))
        {
            ANGLE_TRY(blitWithCommand(contextVk, readRenderTargetRect, drawRenderTargetRect,
                                      readRenderTarget, drawRenderTarget, filter, false,
                                      blitDepthBuffer, blitStencilBuffer, flipSource, flipDest));
        }
        else
        {
            if (flipSource || flipDest)
            {
                if (blitDepthBuffer)
                {
                    ANGLE_TRY(blitWithReadback(contextVk, readRenderTargetRect,
                                               VK_IMAGE_ASPECT_DEPTH_BIT, readRenderTarget,
                                               drawRenderTarget));
                }
                if (blitStencilBuffer)
                {
                    ANGLE_TRY(blitWithReadback(contextVk, readRenderTargetRect,
                                               VK_IMAGE_ASPECT_STENCIL_BIT, readRenderTarget,
                                               drawRenderTarget));
                }
            }
            else
            {
                ANGLE_TRY(blitWithCopy(contextVk, readRenderTargetRect, readRenderTarget,
                                       drawRenderTarget, blitDepthBuffer, blitStencilBuffer));
            }
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::blitWithCommand(ContextVk *contextVk,
                                             const gl::Rectangle &readRectIn,
                                             const gl::Rectangle &drawRectIn,
                                             RenderTargetVk *readRenderTarget,
                                             RenderTargetVk *drawRenderTarget,
                                             GLenum filter,
                                             bool colorBlit,
                                             bool depthBlit,
                                             bool stencilBlit,
                                             bool flipSource,
                                             bool flipDest)
{
    // Since blitRenderbufferRect is called for each render buffer that needs to be blitted,
    // it should never be the case that both color and depth/stencil need to be blitted at
    // at the same time.
    ASSERT(colorBlit != (depthBlit || stencilBlit));

    vk::ImageHelper *dstImage = drawRenderTarget->getImageForWrite(&mFramebuffer);

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &commandBuffer));

    const vk::Format &readImageFormat = readRenderTarget->getImageFormat();
    VkImageAspectFlags aspectMask =
        colorBlit ? VK_IMAGE_ASPECT_COLOR_BIT
                  : vk::GetDepthStencilAspectFlags(readImageFormat.imageFormat());
    vk::ImageHelper *srcImage = readRenderTarget->getImageForRead(
        &mFramebuffer, vk::ImageLayout::TransferSrc, commandBuffer);

    const gl::Extents sourceFrameBufferExtents = readRenderTarget->getExtents();
    gl::Rectangle readRect                     = readRectIn;

    if (flipSource)
    {
        readRect.y = sourceFrameBufferExtents.height - readRect.y - readRect.height;
    }

    VkImageBlit blit               = {};
    blit.srcOffsets[0]             = {readRect.x0(), flipSource ? readRect.y1() : readRect.y0(), 0};
    blit.srcOffsets[1]             = {readRect.x1(), flipSource ? readRect.y0() : readRect.y1(), 1};
    blit.srcSubresource.aspectMask = aspectMask;
    blit.srcSubresource.mipLevel   = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount     = 1;
    blit.dstSubresource.aspectMask     = aspectMask;
    blit.dstSubresource.mipLevel       = 0;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount     = 1;

    const gl::Extents drawFrameBufferExtents = drawRenderTarget->getExtents();
    gl::Rectangle drawRect                   = drawRectIn;

    if (flipDest)
    {
        drawRect.y = drawFrameBufferExtents.height - drawRect.y - drawRect.height;
    }

    blit.dstOffsets[0] = {drawRect.x0(), flipDest ? drawRect.y1() : drawRect.y0(), 0};
    blit.dstOffsets[1] = {drawRect.x1(), flipDest ? drawRect.y0() : drawRect.y1(), 1};

    // Requirement of the copyImageToBuffer, the dst image must be in
    // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL layout.
    dstImage->changeLayout(aspectMask, vk::ImageLayout::TransferDst, commandBuffer);

    commandBuffer->blitImage(srcImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             dstImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                             gl_vk::GetFilter(filter));

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

angle::Result FramebufferVk::syncState(const gl::Context *context,
                                       const gl::Framebuffer::DirtyBits &dirtyBits)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    ASSERT(dirtyBits.any());
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
                ANGLE_TRY(mRenderTargetCache.updateDepthStencilRenderTarget(context, mState));
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                break;
            default:
            {
                ASSERT(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0 &&
                       dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX);
                size_t colorIndex =
                    static_cast<size_t>(dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                ANGLE_TRY(mRenderTargetCache.updateColorRenderTarget(context, mState, colorIndex));

                // Update cached masks for masked clears.
                RenderTargetVk *renderTarget = mRenderTargetCache.getColors()[colorIndex];
                if (renderTarget)
                {
                    const angle::Format &emulatedFormat =
                        renderTarget->getImageFormat().imageFormat();
                    updateActiveColorMasks(
                        colorIndex, emulatedFormat.redBits > 0, emulatedFormat.greenBits > 0,
                        emulatedFormat.blueBits > 0, emulatedFormat.alphaBits > 0);

                    const angle::Format &sourceFormat =
                        renderTarget->getImageFormat().angleFormat();
                    mEmulatedAlphaAttachmentMask.set(
                        colorIndex, sourceFormat.alphaBits == 0 && emulatedFormat.alphaBits > 0);

                    contextVk->updateColorMask(context->getState().getBlendState());
                }
                else
                {
                    updateActiveColorMasks(colorIndex, false, false, false, false);
                }
                break;
            }
        }
    }

    mActiveColorComponents = gl_vk::GetColorComponentFlags(
        mActiveColorComponentMasksForClear[0].any(), mActiveColorComponentMasksForClear[1].any(),
        mActiveColorComponentMasksForClear[2].any(), mActiveColorComponentMasksForClear[3].any());

    mFramebuffer.release(renderer);

    // Will freeze the current set of dependencies on this FBO. The next time we render we will
    // create a new entry in the command graph.
    mFramebuffer.finishCurrentCommands(renderer);

    // Notify the ContextVk to update the pipeline desc.
    updateRenderPassDesc();
    contextVk->onFramebufferChange(mRenderPassDesc);

    return angle::Result::Continue;
}

void FramebufferVk::updateRenderPassDesc()
{
    mRenderPassDesc = {};
    mRenderPassDesc.setSamples(getSamples());

    // TODO(jmadill): Support gaps in RenderTargets. http://anglebug.com/2394
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndex : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndex];
        ASSERT(colorRenderTarget);
        mRenderPassDesc.packAttachment(colorRenderTarget->getImage().getFormat());
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        mRenderPassDesc.packAttachment(depthStencilRenderTarget->getImage().getFormat());
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
    ANGLE_TRY(contextVk->getRenderer()->getCompatibleRenderPass(contextVk, mRenderPassDesc,
                                                                &compatibleRenderPass));

    // If we've a Framebuffer provided by a Surface (default FBO/backbuffer), query it.
    if (mBackbuffer)
    {
        return mBackbuffer->getCurrentFramebuffer(contextVk, *compatibleRenderPass, framebufferOut);
    }

    // Gather VkImageViews over all FBO attachments, also size of attached region.
    std::vector<VkImageView> attachments;
    gl::Extents attachmentsSize;

    // TODO(jmadill): Support gaps in RenderTargets. http://anglebug.com/2394
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndex : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndex];
        ASSERT(colorRenderTarget);
        attachments.push_back(colorRenderTarget->getDrawImageView()->getHandle());

        ASSERT(attachmentsSize.empty() || attachmentsSize == colorRenderTarget->getExtents());
        attachmentsSize = colorRenderTarget->getExtents();
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        attachments.push_back(depthStencilRenderTarget->getDrawImageView()->getHandle());

        ASSERT(attachmentsSize.empty() ||
               attachmentsSize == depthStencilRenderTarget->getExtents());
        attachmentsSize = depthStencilRenderTarget->getExtents();
    }

    ASSERT(!attachments.empty());

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

    size_t attachmentIndex = 0;

    // Go through clearColorBuffers and set the appropriate loadOp and clear values.
    // TODO: Support gaps in RenderTargets. http://anglebug.com/2394
    for (size_t colorIndex : mState.getEnabledDrawBuffers())
    {
        if (clearColorBuffers.test(colorIndex))
        {
            RenderTargetVk *renderTarget = getColorReadRenderTarget();

            // If the render target doesn't have alpha, but its emulated format has it, clear the
            // alpha to 1.
            VkClearColorValue value = clearColorValue;
            if (mEmulatedAlphaAttachmentMask[colorIndex])
            {
                SetEmulatedAlphaValue(renderTarget->getImageFormat(), &value);
            }

            mFramebuffer.clearRenderPassColorAttachment(attachmentIndex, value);
        }
        ++attachmentIndex;
    }

    // Set the appropriate loadOp and clear values for depth and stencil.
    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        if (clearDepth)
        {
            mFramebuffer.clearRenderPassDepthAttachment(attachmentIndex,
                                                        clearDepthStencilValue.depth);
        }

        if (clearStencil)
        {
            mFramebuffer.clearRenderPassStencilAttachment(attachmentIndex,
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
    RendererVk *renderer = contextVk->getRenderer();

    UtilsVk::ClearFramebufferParameters params = {};
    params.renderPassDesc                      = &getRenderPassDesc();
    params.renderAreaHeight                    = mState.getDimensions().height;
    params.clearArea                           = clearArea;
    params.colorClearValue                     = clearColorValue;
    params.stencilClearValue                   = clearStencilValue;
    params.stencilMask                         = stencilMask;

    params.clearColor   = true;
    params.clearStencil = clearStencil;

    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndex : clearColorBuffers)
    {
        const RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndex];
        ASSERT(colorRenderTarget);

        params.colorFormat          = &colorRenderTarget->getImage().getFormat().imageFormat();
        params.colorAttachmentIndex = colorIndex;
        params.colorMaskFlags       = colorMaskFlags;
        if (mEmulatedAlphaAttachmentMask[colorIndex])
        {
            params.colorMaskFlags &= ~VK_COLOR_COMPONENT_A_BIT;
        }

        ANGLE_TRY(renderer->getUtils().clearFramebuffer(contextVk, this, params));

        // Clear stencil only once!
        params.clearStencil = false;
    }

    // If there was no color clear, clear stencil alone.
    if (params.clearStencil)
    {
        params.clearColor = false;
        ANGLE_TRY(renderer->getUtils().clearFramebuffer(contextVk, this, params));
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::getSamplePosition(const gl::Context *context,
                                               size_t index,
                                               GLfloat *xy) const
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
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

    vk::RenderPassDesc renderPassDesc;

    // Initialize RenderPass info.
    // TODO(jmadill): Support gaps in RenderTargets. http://anglebug.com/2394
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndex : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndex];
        ASSERT(colorRenderTarget);

        ANGLE_TRY(colorRenderTarget->onColorDraw(contextVk, &mFramebuffer, writeCommands,
                                                 &renderPassDesc));

        renderPassAttachmentOps.initWithLoadStore(attachmentClearValues.size(),
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        attachmentClearValues.emplace_back(kUninitializedClearValue);
    }

    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();
    if (depthStencilRenderTarget)
    {
        ANGLE_TRY(depthStencilRenderTarget->onDepthStencilDraw(contextVk, &mFramebuffer,
                                                               writeCommands, &renderPassDesc));

        renderPassAttachmentOps.initWithLoadStore(attachmentClearValues.size(),
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        attachmentClearValues.emplace_back(kUninitializedClearValue);
    }

    return mFramebuffer.beginRenderPass(contextVk, *framebuffer, renderArea, mRenderPassDesc,
                                        renderPassAttachmentOps, attachmentClearValues,
                                        commandBufferOut);
}

void FramebufferVk::updateActiveColorMasks(size_t colorIndex, bool r, bool g, bool b, bool a)
{
    mActiveColorComponentMasksForClear[0].set(colorIndex, r);
    mActiveColorComponentMasksForClear[1].set(colorIndex, g);
    mActiveColorComponentMasksForClear[2].set(colorIndex, b);
    mActiveColorComponentMasksForClear[3].set(colorIndex, a);
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
    TRACE_EVENT0("gpu.angle", "FramebufferVk::readPixelsImpl");

    ANGLE_TRY(renderTarget->ensureImageInitialized(contextVk));

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mFramebuffer.recordCommands(contextVk, &commandBuffer));

    // Note that although we're reading from the image, we need to update the layout below.

    vk::ImageHelper *srcImage =
        renderTarget->getImageForRead(&mFramebuffer, vk::ImageLayout::TransferSrc, commandBuffer);

    const angle::Format *readFormat = &srcImage->getFormat().imageFormat();

    if (copyAspectFlags != VK_IMAGE_ASPECT_COLOR_BIT)
    {
        readFormat = &GetDepthStencilImageToBufferFormat(*readFormat, copyAspectFlags);
    }

    VkBuffer bufferHandle      = VK_NULL_HANDLE;
    uint8_t *readPixelBuffer   = nullptr;
    VkDeviceSize stagingOffset = 0;
    size_t allocationSize      = readFormat->pixelBytes * area.width * area.height;

    ANGLE_TRY(mReadPixelBuffer.allocate(contextVk, allocationSize, &readPixelBuffer, &bufferHandle,
                                        &stagingOffset, nullptr));

    VkBufferImageCopy region               = {};
    region.bufferImageHeight               = area.height;
    region.bufferOffset                    = stagingOffset;
    region.bufferRowLength                 = area.width;
    region.imageExtent.width               = area.width;
    region.imageExtent.height              = area.height;
    region.imageExtent.depth               = 1;
    region.imageOffset.x                   = area.x;
    region.imageOffset.y                   = area.y;
    region.imageOffset.z                   = 0;
    region.imageSubresource.aspectMask     = copyAspectFlags;
    region.imageSubresource.baseArrayLayer = renderTarget->getLayerIndex();
    region.imageSubresource.layerCount     = 1;
    region.imageSubresource.mipLevel       = renderTarget->getLevelIndex();

    commandBuffer->copyImageToBuffer(srcImage->getImage(), srcImage->getCurrentLayout(),
                                     bufferHandle, 1, &region);

    // Triggers a full finish.
    // TODO(jmadill): Don't block on asynchronous readback.
    ANGLE_TRY(contextVk->finishImpl());

    // The buffer we copied to needs to be invalidated before we read from it because its not been
    // created with the host coherent bit.
    ANGLE_TRY(mReadPixelBuffer.invalidate(contextVk));

    PackPixels(packPixelsParams, *readFormat, area.width * readFormat->pixelBytes, readPixelBuffer,
               static_cast<uint8_t *>(pixels));

    return angle::Result::Continue;
}

gl::Extents FramebufferVk::getReadImageExtents() const
{
    ASSERT(getColorReadRenderTarget()->getExtents().width == mState.getDimensions().width);
    ASSERT(getColorReadRenderTarget()->getExtents().height == mState.getDimensions().height);

    return getColorReadRenderTarget()->getExtents();
}

gl::Rectangle FramebufferVk::getCompleteRenderArea() const
{
    return gl::Rectangle(0, 0, mState.getDimensions().width, mState.getDimensions().height);
}

gl::Rectangle FramebufferVk::getScissoredRenderArea(ContextVk *contextVk) const
{
    const gl::Rectangle renderArea(0, 0, mState.getDimensions().width,
                                   mState.getDimensions().height);
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
    mFramebuffer.updateQueueSerial(contextVk->getRenderer()->getCurrentQueueSerial());
    if (mFramebuffer.hasStartedRenderPass() &&
        !mFramebuffer.getRenderPassRenderArea().encloses(scissoredRenderArea))
    {
        mFramebuffer.finishCurrentCommands(contextVk->getRenderer());
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
