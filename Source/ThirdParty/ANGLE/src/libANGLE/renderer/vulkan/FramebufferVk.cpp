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

#include "common/debug.h"
#include "common/vulkan/vk_headers.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"

namespace rx
{

namespace
{
// Clear values are only used when loadOp=Clear is set in clearWithRenderPassOp.  When starting a
// new render pass, the clear value is set to an unlikely value (bright pink) to stand out better
// in case of a bug.
constexpr VkClearValue kUninitializedClearValue = {{{0.95, 0.05, 0.95, 0.95}}};

// The value to assign an alpha channel that's emulated.  The type is unsigned int, though it will
// automatically convert to the actual data type.
constexpr unsigned int kEmulatedAlphaValue = 1;

bool HasSrcBlitFeature(RendererVk *renderer, RenderTargetVk *srcRenderTarget)
{
    angle::FormatID srcFormatID = srcRenderTarget->getImageActualFormatID();
    return renderer->hasImageFormatFeatureBits(srcFormatID, VK_FORMAT_FEATURE_BLIT_SRC_BIT);
}

bool HasDstBlitFeature(RendererVk *renderer, RenderTargetVk *dstRenderTarget)
{
    angle::FormatID dstFormatID = dstRenderTarget->getImageActualFormatID();
    return renderer->hasImageFormatFeatureBits(dstFormatID, VK_FORMAT_FEATURE_BLIT_DST_BIT);
}

// Returns false if destination has any channel the source doesn't.  This means that channel was
// emulated and using the Vulkan blit command would overwrite that emulated channel.
bool AreSrcAndDstColorChannelsBlitCompatible(RenderTargetVk *srcRenderTarget,
                                             RenderTargetVk *dstRenderTarget)
{
    const angle::Format &srcFormat = srcRenderTarget->getImageIntendedFormat();
    const angle::Format &dstFormat = dstRenderTarget->getImageIntendedFormat();

    // Luminance/alpha formats are not renderable, so they can't have ended up in a framebuffer to
    // participate in a blit.
    ASSERT(!dstFormat.isLUMA() && !srcFormat.isLUMA());

    // All color formats have the red channel.
    ASSERT(dstFormat.redBits > 0 && srcFormat.redBits > 0);

    return (dstFormat.greenBits > 0 || srcFormat.greenBits == 0) &&
           (dstFormat.blueBits > 0 || srcFormat.blueBits == 0) &&
           (dstFormat.alphaBits > 0 || srcFormat.alphaBits == 0);
}

// Returns false if formats are not identical.  vkCmdResolveImage and resolve attachments both
// require identical formats between source and destination.  vkCmdBlitImage additionally requires
// the same for depth/stencil formats.
bool AreSrcAndDstFormatsIdentical(RenderTargetVk *srcRenderTarget, RenderTargetVk *dstRenderTarget)
{
    angle::FormatID srcFormatID = srcRenderTarget->getImageActualFormatID();
    angle::FormatID dstFormatID = dstRenderTarget->getImageActualFormatID();

    return srcFormatID == dstFormatID;
}

bool AreSrcAndDstDepthStencilChannelsBlitCompatible(RenderTargetVk *srcRenderTarget,
                                                    RenderTargetVk *dstRenderTarget)
{
    const angle::Format &srcFormat = srcRenderTarget->getImageIntendedFormat();
    const angle::Format &dstFormat = dstRenderTarget->getImageIntendedFormat();

    return (dstFormat.depthBits > 0 || srcFormat.depthBits == 0) &&
           (dstFormat.stencilBits > 0 || srcFormat.stencilBits == 0);
}

void EarlyAdjustFlipYForPreRotation(SurfaceRotation blitAngleIn,
                                    SurfaceRotation *blitAngleOut,
                                    bool *blitFlipYOut)
{
    switch (blitAngleIn)
    {
        case SurfaceRotation::Identity:
            // No adjustments needed
            break;
        case SurfaceRotation::Rotated90Degrees:
            *blitAngleOut = SurfaceRotation::Rotated90Degrees;
            *blitFlipYOut = false;
            break;
        case SurfaceRotation::Rotated180Degrees:
            *blitAngleOut = SurfaceRotation::Rotated180Degrees;
            break;
        case SurfaceRotation::Rotated270Degrees:
            *blitAngleOut = SurfaceRotation::Rotated270Degrees;
            *blitFlipYOut = false;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void AdjustBlitAreaForPreRotation(SurfaceRotation framebufferAngle,
                                  const gl::Rectangle &blitAreaIn,
                                  const gl::Rectangle &framebufferDimensions,
                                  gl::Rectangle *blitAreaOut)
{
    switch (framebufferAngle)
    {
        case SurfaceRotation::Identity:
            // No adjustments needed
            break;
        case SurfaceRotation::Rotated90Degrees:
            blitAreaOut->x = blitAreaIn.y;
            blitAreaOut->y = blitAreaIn.x;
            std::swap(blitAreaOut->width, blitAreaOut->height);
            break;
        case SurfaceRotation::Rotated180Degrees:
            blitAreaOut->x = framebufferDimensions.width - blitAreaIn.x - blitAreaIn.width;
            blitAreaOut->y = framebufferDimensions.height - blitAreaIn.y - blitAreaIn.height;
            break;
        case SurfaceRotation::Rotated270Degrees:
            blitAreaOut->x = framebufferDimensions.height - blitAreaIn.y - blitAreaIn.height;
            blitAreaOut->y = framebufferDimensions.width - blitAreaIn.x - blitAreaIn.width;
            std::swap(blitAreaOut->width, blitAreaOut->height);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void AdjustDimensionsAndFlipForPreRotation(SurfaceRotation framebufferAngle,
                                           gl::Rectangle *framebufferDimensions,
                                           bool *flipX,
                                           bool *flipY)
{
    switch (framebufferAngle)
    {
        case SurfaceRotation::Identity:
            // No adjustments needed
            break;
        case SurfaceRotation::Rotated90Degrees:
            std::swap(framebufferDimensions->width, framebufferDimensions->height);
            std::swap(*flipX, *flipY);
            break;
        case SurfaceRotation::Rotated180Degrees:
            break;
        case SurfaceRotation::Rotated270Degrees:
            std::swap(framebufferDimensions->width, framebufferDimensions->height);
            std::swap(*flipX, *flipY);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

// When blitting, the source and destination areas are viewed like UVs.  For example, a 64x64
// texture if flipped should have an offset of 64 in either X or Y which corresponds to U or V of 1.
// On the other hand, when resolving, the source and destination areas are used as fragment
// coordinates to fetch from.  In that case, when flipped, the texture in the above example must
// have an offset of 63.
void AdjustBlitResolveParametersForResolve(const gl::Rectangle &sourceArea,
                                           const gl::Rectangle &destArea,
                                           UtilsVk::BlitResolveParameters *params)
{
    params->srcOffset[0] = sourceArea.x;
    params->srcOffset[1] = sourceArea.y;
    params->dstOffset[0] = destArea.x;
    params->dstOffset[1] = destArea.y;

    if (sourceArea.isReversedX())
    {
        ASSERT(sourceArea.x > 0);
        --params->srcOffset[0];
    }
    if (sourceArea.isReversedY())
    {
        ASSERT(sourceArea.y > 0);
        --params->srcOffset[1];
    }
    if (destArea.isReversedX())
    {
        ASSERT(destArea.x > 0);
        --params->dstOffset[0];
    }
    if (destArea.isReversedY())
    {
        ASSERT(destArea.y > 0);
        --params->dstOffset[1];
    }
}

// Potentially make adjustments for pre-rotatation.  Depending on the angle some of the params need
// to be swapped and/or changes made to which axis are flipped.
void AdjustBlitResolveParametersForPreRotation(SurfaceRotation framebufferAngle,
                                               SurfaceRotation srcFramebufferAngle,
                                               UtilsVk::BlitResolveParameters *params)
{
    switch (framebufferAngle)
    {
        case SurfaceRotation::Identity:
            break;
        case SurfaceRotation::Rotated90Degrees:
            std::swap(params->stretch[0], params->stretch[1]);
            std::swap(params->srcOffset[0], params->srcOffset[1]);
            std::swap(params->rotatedOffsetFactor[0], params->rotatedOffsetFactor[1]);
            std::swap(params->flipX, params->flipY);
            if (srcFramebufferAngle == framebufferAngle)
            {
                std::swap(params->dstOffset[0], params->dstOffset[1]);
                std::swap(params->stretch[0], params->stretch[1]);
            }
            break;
        case SurfaceRotation::Rotated180Degrees:
            // Combine flip info with api flip.
            params->flipX = !params->flipX;
            params->flipY = !params->flipY;
            break;
        case SurfaceRotation::Rotated270Degrees:
            std::swap(params->stretch[0], params->stretch[1]);
            std::swap(params->srcOffset[0], params->srcOffset[1]);
            std::swap(params->rotatedOffsetFactor[0], params->rotatedOffsetFactor[1]);
            if (srcFramebufferAngle == framebufferAngle)
            {
                std::swap(params->stretch[0], params->stretch[1]);
            }
            // Combine flip info with api flip.
            params->flipX = !params->flipX;
            params->flipY = !params->flipY;
            std::swap(params->flipX, params->flipY);

            break;
        default:
            UNREACHABLE();
            break;
    }
}

bool HasResolveAttachment(const gl::AttachmentArray<RenderTargetVk *> &colorRenderTargets,
                          const gl::DrawBufferMask &getEnabledDrawBuffers)
{
    for (size_t colorIndexGL : getEnabledDrawBuffers)
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        if (colorRenderTarget->hasResolveAttachment())
        {
            return true;
        }
    }
    return false;
}

vk::FramebufferNonResolveAttachmentMask MakeUnresolveAttachmentMask(const vk::RenderPassDesc &desc)
{
    vk::FramebufferNonResolveAttachmentMask unresolveMask(
        desc.getColorUnresolveAttachmentMask().bits());
    if (desc.hasDepthUnresolveAttachment() || desc.hasStencilUnresolveAttachment())
    {
        // This mask only needs to know if the depth/stencil attachment needs to be unresolved, and
        // is agnostic of the aspect.
        unresolveMask.set(vk::kUnpackedDepthIndex);
    }
    return unresolveMask;
}

bool IsAnyAttachment3DWithoutAllLayers(const RenderTargetCache<RenderTargetVk> &renderTargetCache,
                                       gl::DrawBufferMask colorAttachmentsMask,
                                       uint32_t framebufferLayerCount)
{
    const auto &colorRenderTargets = renderTargetCache.getColors();
    for (size_t colorIndexGL : colorAttachmentsMask)
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        const vk::ImageHelper &image = colorRenderTarget->getImageForRenderPass();

        if (image.getType() == VK_IMAGE_TYPE_3D && image.getExtents().depth > framebufferLayerCount)
        {
            return true;
        }
    }

    // Depth/stencil attachments cannot be 3D.
    ASSERT(renderTargetCache.getDepthStencil() == nullptr ||
           renderTargetCache.getDepthStencil()->getImageForRenderPass().getType() !=
               VK_IMAGE_TYPE_3D);

    return false;
}
}  // anonymous namespace

FramebufferVk::FramebufferVk(RendererVk *renderer, const gl::FramebufferState &state)
    : FramebufferImpl(state),
      mBackbuffer(nullptr),
      mActiveColorComponentMasksForClear(0),
      mReadOnlyDepthFeedbackLoopMode(false),
      mIsCurrentFramebufferCached(false)
{
    if (mState.isDefault())
    {
        // These are immutable for system default framebuffer.
        mCurrentFramebufferDesc.updateLayerCount(1);
        mCurrentFramebufferDesc.updateIsMultiview(false);
    }
}

FramebufferVk::~FramebufferVk() = default;

void FramebufferVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    releaseCurrentFramebuffer(contextVk);
}

void FramebufferVk::insertCache(ContextVk *contextVk,
                                const vk::FramebufferDesc &desc,
                                vk::FramebufferHelper &&newFramebuffer)
{
    // Add it into per share group cache
    contextVk->getShareGroup()->getFramebufferCache().insert(desc, std::move(newFramebuffer));

    // Create a refcounted cache key object and have each attachment keep a refcount to it so that
    // it can be destroyed promptly if those attachments change.
    const vk::SharedFramebufferCacheKey sharedFramebufferCacheKey =
        vk::CreateSharedFramebufferCacheKey(desc);

    // Ask each attachment to hold a reference to the cache so that when any attachment is
    // released, the cache can be destroyed.
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        colorRenderTargets[colorIndexGL]->onNewFramebuffer(sharedFramebufferCacheKey);
    }

    if (getDepthStencilRenderTarget())
    {
        getDepthStencilRenderTarget()->onNewFramebuffer(sharedFramebufferCacheKey);
    }
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

    ANGLE_TRY(invalidateImpl(contextVk, count, attachments, false,
                             getRotatedCompleteRenderArea(contextVk)));
    return angle::Result::Continue;
}

angle::Result FramebufferVk::invalidateSub(const gl::Context *context,
                                           size_t count,
                                           const GLenum *attachments,
                                           const gl::Rectangle &area)
{
    ContextVk *contextVk = vk::GetImpl(context);

    const gl::Rectangle nonRotatedCompleteRenderArea = getNonRotatedCompleteRenderArea();
    gl::Rectangle rotatedInvalidateArea;
    RotateRectangle(contextVk->getRotationDrawFramebuffer(),
                    contextVk->isViewportFlipEnabledForDrawFBO(),
                    nonRotatedCompleteRenderArea.width, nonRotatedCompleteRenderArea.height, area,
                    &rotatedInvalidateArea);

    // If invalidateSub() covers the whole framebuffer area, make it behave as invalidate().
    // The invalidate area is clipped to the render area for use inside invalidateImpl.
    const gl::Rectangle completeRenderArea = getRotatedCompleteRenderArea(contextVk);
    if (ClipRectangle(rotatedInvalidateArea, completeRenderArea, &rotatedInvalidateArea) &&
        rotatedInvalidateArea == completeRenderArea)
    {
        return invalidate(context, count, attachments);
    }

    // If there are deferred clears, redefer them.  syncState may have accumulated deferred clears,
    // but if the framebuffer's attachments are used after this call not through the framebuffer,
    // those clears wouldn't get flushed otherwise (for example as the destination of
    // glCopyTex[Sub]Image, shader storage image, etc).
    redeferClears(contextVk);

    if (contextVk->hasStartedRenderPass() &&
        rotatedInvalidateArea.encloses(contextVk->getStartedRenderPassCommands().getRenderArea()))
    {
        // Because the render pass's render area is within the invalidated area, it is fine for
        // invalidateImpl() to use a storeOp of DONT_CARE (i.e. fine to not store the contents of
        // the invalidated area).
        ANGLE_TRY(invalidateImpl(contextVk, count, attachments, true, rotatedInvalidateArea));
    }
    else
    {
        ANGLE_VK_PERF_WARNING(
            contextVk, GL_DEBUG_SEVERITY_LOW,
            "InvalidateSubFramebuffer ignored due to area not covering the render area");
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::clear(const gl::Context *context, GLbitfield mask)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "FramebufferVk::clear");
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

    const gl::Rectangle scissoredRenderArea = getRotatedScissoredRenderArea(contextVk);
    ASSERT(scissoredRenderArea.width != 0 && scissoredRenderArea.height != 0);

    // This function assumes that only enabled attachments are asked to be cleared.
    ASSERT((clearColorBuffers & mState.getEnabledDrawBuffers()) == clearColorBuffers);
    ASSERT(!clearDepth || mState.getDepthAttachment() != nullptr);
    ASSERT(!clearStencil || mState.getStencilAttachment() != nullptr);

    gl::BlendStateExt::ColorMaskStorage::Type colorMasks = contextVk->getClearColorMasks();
    bool clearColor                                      = clearColorBuffers.any();

    // When this function is called, there should always be something to clear.
    ASSERT(clearColor || clearDepth || clearStencil);

    const uint8_t stencilMask =
        static_cast<uint8_t>(contextVk->getState().getDepthStencilState().stencilWritemask);

    // The front-end should ensure we don't attempt to clear color if all channels are masked.
    ASSERT(!clearColor || colorMasks != 0);
    // The front-end should ensure we don't attempt to clear depth if depth write is disabled.
    ASSERT(!clearDepth || contextVk->getState().getDepthStencilState().depthMask);
    // The front-end should ensure we don't attempt to clear stencil if all bits are masked.
    ASSERT(!clearStencil || stencilMask != 0);

    // Make sure to close the render pass now if in read-only depth/stencil feedback loop mode and
    // depth/stencil is being cleared.
    if (clearDepth || clearStencil)
    {
        ANGLE_TRY(contextVk->updateRenderPassDepthFeedbackLoopMode(
            clearDepth ? UpdateDepthFeedbackLoopReason::Clear : UpdateDepthFeedbackLoopReason::None,
            clearStencil ? UpdateDepthFeedbackLoopReason::Clear
                         : UpdateDepthFeedbackLoopReason::None));
    }

    const bool scissoredClear = scissoredRenderArea != getRotatedCompleteRenderArea(contextVk);

    // We use the draw path if scissored clear, or color or stencil are masked.  Note that depth
    // clearing is already disabled if there's a depth mask.
    const bool maskedClearColor = clearColor && (mActiveColorComponentMasksForClear & colorMasks) !=
                                                    mActiveColorComponentMasksForClear;
    const bool maskedClearStencil = clearStencil && stencilMask != 0xFF;

    bool clearColorWithDraw   = clearColor && (maskedClearColor || scissoredClear);
    bool clearDepthWithDraw   = clearDepth && scissoredClear;
    bool clearStencilWithDraw = clearStencil && (maskedClearStencil || scissoredClear);

    const bool isMidRenderPassClear = contextVk->hasStartedRenderPassWithCommands();

    if (isMidRenderPassClear)
    {
        // If a render pass is open with commands, it must be for this framebuffer.  Otherwise,
        // either FramebufferVk::syncState() or ContextVk::syncState() would have closed it.
        vk::Framebuffer *currentFramebuffer = nullptr;
        ANGLE_TRY(getFramebuffer(contextVk, &currentFramebuffer, nullptr,
                                 SwapchainResolveMode::Disabled));
        ASSERT(contextVk->hasStartedRenderPassWithFramebuffer(currentFramebuffer));

        // Emit debug-util markers for this mid-render-pass clear
        ANGLE_TRY(
            contextVk->handleGraphicsEventLog(rx::GraphicsEventCmdBuf::InRenderPassCmdBufQueryCmd));
    }
    else
    {
        // Emit debug-util markers for this outside-render-pass clear
        ANGLE_TRY(
            contextVk->handleGraphicsEventLog(rx::GraphicsEventCmdBuf::InOutsideCmdBufQueryCmd));
    }

    const bool preferDrawOverClearAttachments =
        contextVk->getRenderer()->getFeatures().preferDrawClearOverVkCmdClearAttachments.enabled;

    // Merge current clears with the deferred clears, then proceed with only processing deferred
    // clears.  This simplifies the clear paths such that they don't need to consider both the
    // current and deferred clears.  Additionally, it avoids needing to undo an unresolve
    // operation; say attachment A is deferred cleared and multisampled-render-to-texture
    // attachment B is currently cleared.  Assuming a render pass needs to start (because for
    // example attachment C needs to clear with a draw path), starting one with only deferred
    // clears and then applying the current clears won't work as attachment B is unresolved, and
    // there are no facilities to undo that.
    if (preferDrawOverClearAttachments && isMidRenderPassClear)
    {
        // On buggy hardware, prefer to clear with a draw call instead of vkCmdClearAttachments.
        // Note that it's impossible to have deferred clears in the middle of the render pass.
        ASSERT(!mDeferredClears.any());

        clearColorWithDraw   = clearColor;
        clearDepthWithDraw   = clearDepth;
        clearStencilWithDraw = clearStencil;
    }
    else
    {
        gl::DrawBufferMask clearColorDrawBuffersMask;
        if (clearColor && !clearColorWithDraw)
        {
            clearColorDrawBuffersMask = clearColorBuffers;
        }

        mergeClearsWithDeferredClears(clearColorDrawBuffersMask, clearDepth && !clearDepthWithDraw,
                                      clearStencil && !clearStencilWithDraw, clearColorValue,
                                      clearDepthStencilValue);
    }

    // If any deferred clears, we can further defer them, clear them with vkCmdClearAttachments or
    // flush them if necessary.
    if (mDeferredClears.any())
    {
        const bool clearAnyWithDraw =
            clearColorWithDraw || clearDepthWithDraw || clearStencilWithDraw;

        bool isAnyAttachment3DWithoutAllLayers =
            IsAnyAttachment3DWithoutAllLayers(mRenderTargetCache, mState.getColorAttachmentsMask(),
                                              mCurrentFramebufferDesc.getLayerCount());

        // If we are in an active renderpass that has recorded commands and the framebuffer hasn't
        // changed, inline the clear.
        if (isMidRenderPassClear)
        {
            ANGLE_VK_PERF_WARNING(
                contextVk, GL_DEBUG_SEVERITY_LOW,
                "Clear effectively discarding previous draw call results. Suggest earlier Clear "
                "followed by masked color or depth/stencil draw calls instead, or "
                "glInvalidateFramebuffer to discard data instead");

            ASSERT(!preferDrawOverClearAttachments);

            // clearWithCommand will operate on deferred clears.
            clearWithCommand(contextVk, scissoredRenderArea);

            // clearWithCommand will clear only those attachments that have been used in the render
            // pass, and removes them from mDeferredClears.  Any deferred clears that are left can
            // be performed with a renderpass loadOp.
            if (mDeferredClears.any())
            {
                clearWithLoadOp(contextVk);
            }
        }
        else
        {
            if (contextVk->hasStartedRenderPass())
            {
                // Typically, clears are deferred such that it's impossible to have a render pass
                // opened without any additional commands recorded on it.  This is not true for some
                // corner cases, such as with 3D or AHB attachments.  In those cases, a clear can
                // open a render pass that's otherwise empty, and additional clears can continue to
                // be accumulated in the render pass loadOps.
                ASSERT(isAnyAttachment3DWithoutAllLayers || mIsAHBColorAttachments.any());
                clearWithLoadOp(contextVk);
            }

            // This path will defer the current clears along with deferred clears.  This won't work
            // if any attachment needs to be subsequently cleared with a draw call.  In that case,
            // flush deferred clears, which will start a render pass with deferred clear values.
            // The subsequent draw call will then operate on the cleared attachments.
            //
            // Additionally, if the framebuffer is layered, any attachment is 3D and it has a larger
            // depth than the framebuffer layers, clears cannot be deferred.  This is because the
            // clear may later need to be flushed with vkCmdClearColorImage, which cannot partially
            // clear the 3D texture.  In that case, the clears are flushed immediately too.
            //
            // For imported images such as from AHBs, the clears are not deferred so that they are
            // definitely applied before the application uses them outside of the control of ANGLE.
            if (clearAnyWithDraw || isAnyAttachment3DWithoutAllLayers ||
                mIsAHBColorAttachments.any())
            {
                ANGLE_TRY(flushDeferredClears(contextVk));
            }
            else
            {
                redeferClears(contextVk);
            }
        }

        // If nothing left to clear, early out.
        if (!clearAnyWithDraw)
        {
            ASSERT(mDeferredClears.empty());
            return angle::Result::Continue;
        }
    }

    if (!clearColorWithDraw)
    {
        clearColorBuffers.reset();
    }

    // The most costly clear mode is when we need to mask out specific color channels or stencil
    // bits. This can only be done with a draw call.
    return clearWithDraw(contextVk, scissoredRenderArea, clearColorBuffers, clearDepthWithDraw,
                         clearStencilWithDraw, colorMasks, stencilMask, clearColorValue,
                         clearDepthStencilValue);
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
        clearStencil                    = true;
        clearValue.depthStencil.stencil = static_cast<uint8_t>(values[0]);
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
    clearValue.depthStencil.stencil = static_cast<uint8_t>(stencil);

    return clearImpl(context, gl::DrawBufferMask(), true, true, clearValue.color,
                     clearValue.depthStencil);
}

const gl::InternalFormat &FramebufferVk::getImplementationColorReadFormat(
    const gl::Context *context) const
{
    ContextVk *contextVk       = vk::GetImpl(context);
    GLenum sizedFormat         = mState.getReadAttachment()->getFormat().info->sizedInternalFormat;
    const vk::Format &vkFormat = contextVk->getRenderer()->getFormat(sizedFormat);
    GLenum implFormat = vkFormat.getActualRenderableImageFormat().fboImplementationInternalFormat;
    return gl::GetSizedInternalFormatInfo(implFormat);
}

angle::Result FramebufferVk::readPixels(const gl::Context *context,
                                        const gl::Rectangle &area,
                                        GLenum format,
                                        GLenum type,
                                        const gl::PixelPackState &pack,
                                        gl::Buffer *packBuffer,
                                        void *pixels)
{
    // Clip read area to framebuffer.
    const gl::Extents &fbSize = getState().getReadPixelsAttachment(format)->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    ContextVk *contextVk = vk::GetImpl(context);

    gl::Rectangle clippedArea;
    if (!ClipRectangle(area, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }

    // Flush any deferred clears.
    ANGLE_TRY(flushDeferredClears(contextVk));

    GLuint outputSkipBytes = 0;
    PackPixelsParams params;
    ANGLE_TRY(vk::ImageHelper::GetReadPixelsParams(contextVk, pack, packBuffer, format, type, area,
                                                   clippedArea, &params, &outputSkipBytes));

    bool flipY = contextVk->isViewportFlipEnabledForReadFBO();
    switch (params.rotation = contextVk->getRotationReadFramebuffer())
    {
        case SurfaceRotation::Identity:
            // Do not rotate gl_Position (surface matches the device's orientation):
            if (flipY)
            {
                params.area.y = fbRect.height - clippedArea.y - clippedArea.height;
            }
            break;
        case SurfaceRotation::Rotated90Degrees:
            // Rotate gl_Position 90 degrees:
            params.area.x = clippedArea.y;
            params.area.y =
                flipY ? clippedArea.x : fbRect.width - clippedArea.x - clippedArea.width;
            std::swap(params.area.width, params.area.height);
            break;
        case SurfaceRotation::Rotated180Degrees:
            // Rotate gl_Position 180 degrees:
            params.area.x = fbRect.width - clippedArea.x - clippedArea.width;
            params.area.y =
                flipY ? clippedArea.y : fbRect.height - clippedArea.y - clippedArea.height;
            break;
        case SurfaceRotation::Rotated270Degrees:
            // Rotate gl_Position 270 degrees:
            params.area.x = fbRect.height - clippedArea.y - clippedArea.height;
            params.area.y =
                flipY ? fbRect.width - clippedArea.x - clippedArea.width : clippedArea.x;
            std::swap(params.area.width, params.area.height);
            break;
        default:
            UNREACHABLE();
            break;
    }
    if (flipY)
    {
        params.reverseRowOrder = !params.reverseRowOrder;
    }

    ANGLE_TRY(readPixelsImpl(contextVk, params.area, params, getReadPixelsAspectFlags(format),
                             getReadPixelsRenderTarget(format),
                             static_cast<uint8_t *>(pixels) + outputSkipBytes));
    return angle::Result::Continue;
}

RenderTargetVk *FramebufferVk::getDepthStencilRenderTarget() const
{
    return mRenderTargetCache.getDepthStencil();
}

RenderTargetVk *FramebufferVk::getColorDrawRenderTarget(size_t colorIndex) const
{
    RenderTargetVk *renderTarget = mRenderTargetCache.getColorDraw(mState, colorIndex);
    ASSERT(renderTarget && renderTarget->getImageForRenderPass().valid());
    return renderTarget;
}

RenderTargetVk *FramebufferVk::getColorReadRenderTarget() const
{
    RenderTargetVk *renderTarget = mRenderTargetCache.getColorRead(mState);
    ASSERT(renderTarget && renderTarget->getImageForRenderPass().valid());
    return renderTarget;
}

RenderTargetVk *FramebufferVk::getReadPixelsRenderTarget(GLenum format) const
{
    switch (format)
    {
        case GL_DEPTH_COMPONENT:
        case GL_STENCIL_INDEX_OES:
        case GL_DEPTH_STENCIL_OES:
            return getDepthStencilRenderTarget();
        default:
            return getColorReadRenderTarget();
    }
}

VkImageAspectFlagBits FramebufferVk::getReadPixelsAspectFlags(GLenum format) const
{
    switch (format)
    {
        case GL_DEPTH_COMPONENT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case GL_STENCIL_INDEX_OES:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case GL_DEPTH_STENCIL_OES:
            return vk::IMAGE_ASPECT_DEPTH_STENCIL;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
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

    vk::ImageHelper *srcImage = &readRenderTarget->getImageForCopy();
    vk::ImageHelper *dstImage = &drawRenderTarget->getImageForWrite();

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

    vk::CommandBufferAccess access;
    access.onImageTransferRead(imageAspectMask, srcImage);
    access.onImageTransferWrite(drawRenderTarget->getLevelIndex(), 1,
                                drawRenderTarget->getLayerIndex(), 1, imageAspectMask, dstImage);
    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer(access, &commandBuffer));

    VkImageBlit blit               = {};
    blit.srcSubresource.aspectMask = blitAspectMask;
    blit.srcSubresource.mipLevel   = srcImage->toVkLevel(readRenderTarget->getLevelIndex()).get();
    blit.srcSubresource.baseArrayLayer = readRenderTarget->getLayerIndex();
    blit.srcSubresource.layerCount     = 1;
    blit.srcOffsets[0]                 = {sourceArea.x0(), sourceArea.y0(), 0};
    blit.srcOffsets[1]                 = {sourceArea.x1(), sourceArea.y1(), 1};
    blit.dstSubresource.aspectMask     = blitAspectMask;
    blit.dstSubresource.mipLevel = dstImage->toVkLevel(drawRenderTarget->getLevelIndex()).get();
    blit.dstSubresource.baseArrayLayer = drawRenderTarget->getLayerIndex();
    blit.dstSubresource.layerCount     = 1;
    blit.dstOffsets[0]                 = {destArea.x0(), destArea.y0(), 0};
    blit.dstOffsets[1]                 = {destArea.x1(), destArea.y1(), 1};

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

    // We can sometimes end up in a blit with some clear commands saved. Ensure all clear commands
    // are issued before we issue the blit command.
    ANGLE_TRY(flushDeferredClears(contextVk));

    const gl::State &glState              = contextVk->getState();
    const gl::Framebuffer *srcFramebuffer = glState.getReadFramebuffer();
    FramebufferVk *srcFramebufferVk       = vk::GetImpl(srcFramebuffer);

    const bool blitColorBuffer   = (mask & GL_COLOR_BUFFER_BIT) != 0;
    const bool blitDepthBuffer   = (mask & GL_DEPTH_BUFFER_BIT) != 0;
    const bool blitStencilBuffer = (mask & GL_STENCIL_BUFFER_BIT) != 0;

    // If a framebuffer contains a mixture of multisampled and multisampled-render-to-texture
    // attachments, this function could be simultaneously doing a blit on one attachment and resolve
    // on another.  For the most part, this means resolve semantics apply.  However, as the resolve
    // path cannot be taken for multisampled-render-to-texture attachments, the distinction of
    // whether resolve is done for each attachment or blit is made.
    const bool isColorResolve =
        blitColorBuffer &&
        srcFramebufferVk->getColorReadRenderTarget()->getImageForCopy().getSamples() > 1;
    const bool isDepthStencilResolve =
        (blitDepthBuffer || blitStencilBuffer) &&
        srcFramebufferVk->getDepthStencilRenderTarget()->getImageForCopy().getSamples() > 1;
    const bool isResolve = isColorResolve || isDepthStencilResolve;

    bool srcFramebufferFlippedY = contextVk->isViewportFlipEnabledForReadFBO();
    bool dstFramebufferFlippedY = contextVk->isViewportFlipEnabledForDrawFBO();

    gl::Rectangle sourceArea = sourceAreaIn;
    gl::Rectangle destArea   = destAreaIn;

    // Note: GLES (all 3.x versions) require source and destination area to be identical when
    // resolving.
    ASSERT(!isResolve ||
           (sourceArea.x == destArea.x && sourceArea.y == destArea.y &&
            sourceArea.width == destArea.width && sourceArea.height == destArea.height));

    gl::Rectangle srcFramebufferDimensions = srcFramebufferVk->getNonRotatedCompleteRenderArea();
    gl::Rectangle dstFramebufferDimensions = getNonRotatedCompleteRenderArea();

    // If the destination is flipped in either direction, we will flip the source instead so that
    // the destination area is always unflipped.
    sourceArea = sourceArea.flip(destArea.isReversedX(), destArea.isReversedY());
    destArea   = destArea.removeReversal();

    // Calculate the stretch factor prior to any clipping, as it needs to remain constant.
    const double stretch[2] = {
        std::abs(sourceArea.width / static_cast<double>(destArea.width)),
        std::abs(sourceArea.height / static_cast<double>(destArea.height)),
    };

    // Potentially make adjustments for pre-rotatation.  To handle various cases (e.g. clipping)
    // and to not interrupt the normal flow of the code, different adjustments are made in
    // different parts of the code.  These first adjustments are for whether or not to flip the
    // y-axis, and to note the overall rotation (regardless of whether it is the source or
    // destination that is rotated).
    SurfaceRotation srcFramebufferRotation = contextVk->getRotationReadFramebuffer();
    SurfaceRotation dstFramebufferRotation = contextVk->getRotationDrawFramebuffer();
    SurfaceRotation rotation               = SurfaceRotation::Identity;
    // Both the source and destination cannot be rotated (which would indicate both are the default
    // framebuffer (i.e. swapchain image).
    ASSERT((srcFramebufferRotation == SurfaceRotation::Identity) ||
           (dstFramebufferRotation == SurfaceRotation::Identity));
    EarlyAdjustFlipYForPreRotation(srcFramebufferRotation, &rotation, &srcFramebufferFlippedY);
    EarlyAdjustFlipYForPreRotation(dstFramebufferRotation, &rotation, &dstFramebufferFlippedY);

    // First, clip the source area to framebuffer.  That requires transforming the destination area
    // to match the clipped source.
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
        // Source and destination areas are identical in resolve (except rotate it, if appropriate).
        srcClippedDestArea = clippedSourceArea;
        AdjustBlitAreaForPreRotation(dstFramebufferRotation, clippedSourceArea,
                                     dstFramebufferDimensions, &srcClippedDestArea);
    }
    else if (clippedSourceArea == absSourceArea)
    {
        // If there was no clipping, keep destination area as is (except rotate it, if appropriate).
        srcClippedDestArea = destArea;
        AdjustBlitAreaForPreRotation(dstFramebufferRotation, destArea, dstFramebufferDimensions,
                                     &srcClippedDestArea);
    }
    else
    {
        // Shift destination area's x0,y0,x1,y1 by as much as the source area's got shifted (taking
        // stretching into account).  Note that double is used as float doesn't have enough
        // precision near the end of int range.
        double x0Shift = std::round((clippedSourceArea.x - absSourceArea.x) / stretch[0]);
        double y0Shift = std::round((clippedSourceArea.y - absSourceArea.y) / stretch[1]);
        double x1Shift = std::round((absSourceArea.x1() - clippedSourceArea.x1()) / stretch[0]);
        double y1Shift = std::round((absSourceArea.y1() - clippedSourceArea.y1()) / stretch[1]);

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

        // Rotate srcClippedDestArea if the destination is rotated
        if (dstFramebufferRotation != SurfaceRotation::Identity)
        {
            gl::Rectangle originalSrcClippedDestArea = srcClippedDestArea;
            AdjustBlitAreaForPreRotation(dstFramebufferRotation, originalSrcClippedDestArea,
                                         dstFramebufferDimensions, &srcClippedDestArea);
        }
    }

    // If framebuffers are flipped in Y, flip the source and destination area (which define the
    // transformation regardless of clipping), as well as the blit area (which is the clipped
    // destination area).
    if (srcFramebufferFlippedY)
    {
        sourceArea.y      = srcFramebufferDimensions.height - sourceArea.y;
        sourceArea.height = -sourceArea.height;
    }
    if (dstFramebufferFlippedY)
    {
        destArea.y      = dstFramebufferDimensions.height - destArea.y;
        destArea.height = -destArea.height;

        srcClippedDestArea.y =
            dstFramebufferDimensions.height - srcClippedDestArea.y - srcClippedDestArea.height;
    }

    bool flipX = sourceArea.isReversedX() != destArea.isReversedX();
    bool flipY = sourceArea.isReversedY() != destArea.isReversedY();

    // GLES doesn't allow flipping the parameters of glBlitFramebuffer if performing a resolve.
    ASSERT(!isResolve ||
           (flipX == false && flipY == (srcFramebufferFlippedY != dstFramebufferFlippedY)));

    // Again, transfer the destination flip to source, so destination is unflipped.  Note that
    // destArea was not reversed until the final possible Y-flip.
    ASSERT(!destArea.isReversedX());
    sourceArea = sourceArea.flip(false, destArea.isReversedY());
    destArea   = destArea.removeReversal();

    // Now that clipping and flipping is done, rotate certain values that will be used for
    // UtilsVk::BlitResolveParameters
    gl::Rectangle sourceAreaOld = sourceArea;
    gl::Rectangle destAreaOld   = destArea;
    if (srcFramebufferRotation == rotation)
    {
        AdjustBlitAreaForPreRotation(srcFramebufferRotation, sourceAreaOld,
                                     srcFramebufferDimensions, &sourceArea);
        AdjustDimensionsAndFlipForPreRotation(srcFramebufferRotation, &srcFramebufferDimensions,
                                              &flipX, &flipY);
    }
    SurfaceRotation rememberDestFramebufferRotation = dstFramebufferRotation;
    if (srcFramebufferRotation == SurfaceRotation::Rotated90Degrees)
    {
        dstFramebufferRotation = rotation;
    }
    AdjustBlitAreaForPreRotation(dstFramebufferRotation, destAreaOld, dstFramebufferDimensions,
                                 &destArea);
    dstFramebufferRotation = rememberDestFramebufferRotation;

    // Clip the destination area to the framebuffer size and scissor.  Note that we don't care
    // about the source area anymore.  The offset translation is done based on the original source
    // and destination rectangles.  The stretch factor is already calculated as well.
    gl::Rectangle blitArea;
    if (!gl::ClipRectangle(getRotatedScissoredRenderArea(contextVk), srcClippedDestArea, &blitArea))
    {
        return angle::Result::Continue;
    }

    bool noClip = blitArea == destArea && stretch[0] == 1.0f && stretch[1] == 1.0f;
    bool noFlip = !flipX && !flipY;
    bool disableFlippingBlitWithCommand =
        contextVk->getRenderer()->getFeatures().disableFlippingBlitWithCommand.enabled;

    UtilsVk::BlitResolveParameters commonParams;
    commonParams.srcOffset[0]           = sourceArea.x;
    commonParams.srcOffset[1]           = sourceArea.y;
    commonParams.dstOffset[0]           = destArea.x;
    commonParams.dstOffset[1]           = destArea.y;
    commonParams.rotatedOffsetFactor[0] = std::abs(sourceArea.width);
    commonParams.rotatedOffsetFactor[1] = std::abs(sourceArea.height);
    commonParams.stretch[0]             = static_cast<float>(stretch[0]);
    commonParams.stretch[1]             = static_cast<float>(stretch[1]);
    commonParams.srcExtents[0]          = srcFramebufferDimensions.width;
    commonParams.srcExtents[1]          = srcFramebufferDimensions.height;
    commonParams.blitArea               = blitArea;
    commonParams.linear                 = filter == GL_LINEAR && !isResolve;
    commonParams.flipX                  = flipX;
    commonParams.flipY                  = flipY;
    commonParams.rotation               = rotation;

    if (blitColorBuffer)
    {
        RenderTargetVk *readRenderTarget      = srcFramebufferVk->getColorReadRenderTarget();
        UtilsVk::BlitResolveParameters params = commonParams;
        params.srcLayer                       = readRenderTarget->getLayerIndex();

        // Multisampled images are not allowed to have mips.
        ASSERT(!isColorResolve || readRenderTarget->getLevelIndex() == gl::LevelIndex(0));

        // If there was no clipping and the format capabilities allow us, use Vulkan's builtin blit.
        // The reason clipping is prohibited in this path is that due to rounding errors, it would
        // be hard to guarantee the image stretching remains perfect.  That also allows us not to
        // have to transform back the destination clipping to source.
        //
        // Non-identity pre-rotation cases do not use Vulkan's builtin blit.
        //
        // For simplicity, we either blit all render targets with a Vulkan command, or none.
        bool canBlitWithCommand =
            !isColorResolve && noClip && (noFlip || !disableFlippingBlitWithCommand) &&
            HasSrcBlitFeature(renderer, readRenderTarget) && rotation == SurfaceRotation::Identity;
        // If we need to reinterpret the colorspace then the blit must be done through a shader
        bool reinterpretsColorspace =
            mCurrentFramebufferDesc.getWriteControlMode() != gl::SrgbWriteControlMode::Default;
        bool areChannelsBlitCompatible = true;
        bool areFormatsIdentical       = true;
        for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
        {
            RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
            canBlitWithCommand =
                canBlitWithCommand && HasDstBlitFeature(renderer, drawRenderTarget);
            areChannelsBlitCompatible =
                areChannelsBlitCompatible &&
                AreSrcAndDstColorChannelsBlitCompatible(readRenderTarget, drawRenderTarget);
            areFormatsIdentical = AreSrcAndDstFormatsIdentical(readRenderTarget, drawRenderTarget);
        }

        // Now that all flipping is done, adjust the offsets for resolve and prerotation
        if (isColorResolve)
        {
            AdjustBlitResolveParametersForResolve(sourceArea, destArea, &params);
        }
        AdjustBlitResolveParametersForPreRotation(rotation, srcFramebufferRotation, &params);

        if (canBlitWithCommand && areChannelsBlitCompatible && !reinterpretsColorspace)
        {
            for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
            {
                RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
                ANGLE_TRY(blitWithCommand(contextVk, sourceArea, destArea, readRenderTarget,
                                          drawRenderTarget, filter, true, false, false, flipX,
                                          flipY));
            }
        }
        // If we're not flipping or rotating, use Vulkan's builtin resolve.
        else if (isColorResolve && !flipX && !flipY && areChannelsBlitCompatible &&
                 areFormatsIdentical && rotation == SurfaceRotation::Identity &&
                 !reinterpretsColorspace)
        {
            // Resolving with a subpass resolve attachment has a few restrictions:
            // 1.) glBlitFramebuffer() needs to copy the read color attachment to all enabled
            // attachments in the draw framebuffer, but Vulkan requires a 1:1 relationship for
            // multisample attachments to resolve attachments in the render pass subpass.
            // Due to this, we currently only support using resolve attachments when there is a
            // single draw attachment enabled.
            // 2.) Using a subpass resolve attachment relies on using the render pass that performs
            // the draw to still be open, so it can be updated to use the resolve attachment to draw
            // into. If there's no render pass with commands, then the multisampled render pass is
            // already done and whose data is already flushed from the tile (in a tile-based
            // renderer), so there's no chance for the resolve attachment to take advantage of the
            // data already being present in the tile.
            vk::Framebuffer *srcVkFramebuffer = nullptr;
            ANGLE_TRY(srcFramebufferVk->getFramebuffer(contextVk, &srcVkFramebuffer, nullptr,
                                                       SwapchainResolveMode::Disabled));

            // TODO(https://anglebug.com/4968): Support multiple open render passes so we can remove
            //  this hack to 'restore' the finished render pass.
            contextVk->restoreFinishedRenderPass(srcVkFramebuffer);

            // glBlitFramebuffer() needs to copy the read color attachment to all enabled
            // attachments in the draw framebuffer, but Vulkan requires a 1:1 relationship for
            // multisample attachments to resolve attachments in the render pass subpass.  Due to
            // this, we currently only support using resolve attachments when there is a single draw
            // attachment enabled.
            bool canResolveWithSubpass =
                mState.getEnabledDrawBuffers().count() == 1 &&
                mCurrentFramebufferDesc.getLayerCount() == 1 &&
                contextVk->hasStartedRenderPassWithFramebuffer(srcVkFramebuffer);

            // Additionally, when resolving with a resolve attachment, the src and destination
            // offsets must match, the render area must match the resolve area, and there should be
            // no flipping or rotation.  Fortunately, in GLES the blit source and destination areas
            // are already required to be identical.
            ASSERT(params.srcOffset[0] == params.dstOffset[0] &&
                   params.srcOffset[1] == params.dstOffset[1]);
            canResolveWithSubpass =
                canResolveWithSubpass && noFlip && rotation == SurfaceRotation::Identity &&
                blitArea == contextVk->getStartedRenderPassCommands().getRenderArea();

            if (canResolveWithSubpass)
            {
                ANGLE_TRY(resolveColorWithSubpass(contextVk, params));
            }
            else
            {
                ANGLE_TRY(resolveColorWithCommand(contextVk, params,
                                                  &readRenderTarget->getImageForCopy()));
            }
        }
        // Otherwise use a shader to do blit or resolve.
        else
        {
            // Flush the render pass, which may incur a vkQueueSubmit, before taking any views.
            // Otherwise the view serials would not reflect the render pass they are really used in.
            // http://crbug.com/1272266#c22
            ANGLE_TRY(
                contextVk->flushCommandsAndEndRenderPass(RenderPassClosureReason::PrepareForBlit));

            const vk::ImageView *copyImageView = nullptr;
            ANGLE_TRY(readRenderTarget->getCopyImageView(contextVk, &copyImageView));
            ANGLE_TRY(utilsVk.colorBlitResolve(
                contextVk, this, &readRenderTarget->getImageForCopy(), copyImageView, params));
        }
    }

    if (blitDepthBuffer || blitStencilBuffer)
    {
        RenderTargetVk *readRenderTarget      = srcFramebufferVk->getDepthStencilRenderTarget();
        RenderTargetVk *drawRenderTarget      = mRenderTargetCache.getDepthStencil();
        UtilsVk::BlitResolveParameters params = commonParams;
        params.srcLayer                       = readRenderTarget->getLayerIndex();

        // Multisampled images are not allowed to have mips.
        ASSERT(!isDepthStencilResolve || readRenderTarget->getLevelIndex() == gl::LevelIndex(0));

        // Similarly, only blit if there's been no clipping or rotating.
        bool canBlitWithCommand =
            !isDepthStencilResolve && noClip && (noFlip || !disableFlippingBlitWithCommand) &&
            HasSrcBlitFeature(renderer, readRenderTarget) &&
            HasDstBlitFeature(renderer, drawRenderTarget) && rotation == SurfaceRotation::Identity;
        bool areChannelsBlitCompatible =
            AreSrcAndDstDepthStencilChannelsBlitCompatible(readRenderTarget, drawRenderTarget);

        // glBlitFramebuffer requires that depth/stencil blits have matching formats.
        ASSERT(AreSrcAndDstFormatsIdentical(readRenderTarget, drawRenderTarget));

        if (canBlitWithCommand && areChannelsBlitCompatible)
        {
            ANGLE_TRY(blitWithCommand(contextVk, sourceArea, destArea, readRenderTarget,
                                      drawRenderTarget, filter, false, blitDepthBuffer,
                                      blitStencilBuffer, flipX, flipY));
        }
        else
        {
            // Now that all flipping is done, adjust the offsets for resolve and prerotation
            if (isDepthStencilResolve)
            {
                AdjustBlitResolveParametersForResolve(sourceArea, destArea, &params);
            }
            AdjustBlitResolveParametersForPreRotation(rotation, srcFramebufferRotation, &params);

            // Create depth- and stencil-only views for reading.
            vk::DeviceScoped<vk::ImageView> depthView(contextVk->getDevice());
            vk::DeviceScoped<vk::ImageView> stencilView(contextVk->getDevice());

            vk::ImageHelper *depthStencilImage = &readRenderTarget->getImageForCopy();
            vk::LevelIndex levelIndex =
                depthStencilImage->toVkLevel(readRenderTarget->getLevelIndex());
            uint32_t layerIndex         = readRenderTarget->getLayerIndex();
            gl::TextureType textureType = vk::Get2DTextureType(depthStencilImage->getLayerCount(),
                                                               depthStencilImage->getSamples());

            if (blitDepthBuffer)
            {
                ANGLE_TRY(depthStencilImage->initLayerImageView(
                    contextVk, textureType, VK_IMAGE_ASPECT_DEPTH_BIT, gl::SwizzleState(),
                    &depthView.get(), levelIndex, 1, layerIndex, 1,
                    gl::SrgbWriteControlMode::Default, gl::YuvSamplingMode::Default));
            }

            if (blitStencilBuffer)
            {
                ANGLE_TRY(depthStencilImage->initLayerImageView(
                    contextVk, textureType, VK_IMAGE_ASPECT_STENCIL_BIT, gl::SwizzleState(),
                    &stencilView.get(), levelIndex, 1, layerIndex, 1,
                    gl::SrgbWriteControlMode::Default, gl::YuvSamplingMode::Default));
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
                ANGLE_VK_PERF_WARNING(contextVk, GL_DEBUG_SEVERITY_LOW,
                                      "Inefficient BlitFramebuffer operation on the stencil aspect "
                                      "due to lack of shader stencil export support");
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
}

void FramebufferVk::updateColorResolveAttachment(
    uint32_t colorIndexGL,
    vk::ImageOrBufferViewSubresourceSerial resolveImageViewSerial)
{
    mCurrentFramebufferDesc.updateColorResolve(colorIndexGL, resolveImageViewSerial);
    mRenderPassDesc.packColorResolveAttachment(colorIndexGL);
}

void FramebufferVk::removeColorResolveAttachment(uint32_t colorIndexGL)
{
    mCurrentFramebufferDesc.updateColorResolve(colorIndexGL,
                                               vk::kInvalidImageOrBufferViewSubresourceSerial);
    mRenderPassDesc.removeColorResolveAttachment(colorIndexGL);
}

void FramebufferVk::releaseCurrentFramebuffer(ContextVk *contextVk)
{
    if (mIsCurrentFramebufferCached)
    {
        mCurrentFramebuffer.release();
    }
    else
    {
        contextVk->addGarbage(&mCurrentFramebuffer);
    }
}

void FramebufferVk::updateLayerCount()
{
    uint32_t layerCount = std::numeric_limits<uint32_t>::max();

    // Color attachments.
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);
        layerCount = std::min(layerCount, colorRenderTarget->getLayerCount());
    }

    // Depth/stencil attachment.
    RenderTargetVk *depthStencilRenderTarget = getDepthStencilRenderTarget();
    if (depthStencilRenderTarget)
    {
        layerCount = std::min(layerCount, depthStencilRenderTarget->getLayerCount());
    }

    if (layerCount == std::numeric_limits<uint32_t>::max())
    {
        layerCount = mState.getDefaultLayers();
    }

    // While layer count and view count are mutually exclusive, they result in different render
    // passes (and thus framebuffers).  For multiview, layer count is set to view count and a flag
    // signifies that the framebuffer is multiview (as opposed to layered).
    const bool isMultiview = mState.isMultiview();
    if (isMultiview)
    {
        layerCount = mState.getNumViews();
    }

    mCurrentFramebufferDesc.updateLayerCount(layerCount);
    mCurrentFramebufferDesc.updateIsMultiview(isMultiview);
}

angle::Result FramebufferVk::resolveColorWithSubpass(ContextVk *contextVk,
                                                     const UtilsVk::BlitResolveParameters &params)
{
    // Vulkan requires a 1:1 relationship for multisample attachments to resolve attachments in the
    // render pass subpass. Due to this, we currently only support using resolve attachments when
    // there is a single draw attachment enabled.
    ASSERT(mState.getEnabledDrawBuffers().count() == 1);
    uint32_t drawColorIndexGL = static_cast<uint32_t>(*mState.getEnabledDrawBuffers().begin());

    const gl::State &glState              = contextVk->getState();
    const gl::Framebuffer *srcFramebuffer = glState.getReadFramebuffer();
    FramebufferVk *srcFramebufferVk       = vk::GetImpl(srcFramebuffer);
    uint32_t readColorIndexGL             = srcFramebuffer->getState().getReadIndex();

    // Use the draw FBO's color attachments as resolve attachments in the read FBO.
    // - Assign the draw FBO's color attachment Serial to the read FBO's resolve attachment
    // - Deactivate the source Framebuffer, since the description changed
    // - Update the renderpass description to indicate there's a resolve attachment
    vk::ImageOrBufferViewSubresourceSerial resolveImageViewSerial =
        mCurrentFramebufferDesc.getColorImageViewSerial(drawColorIndexGL);
    ASSERT(resolveImageViewSerial.viewSerial.valid());
    srcFramebufferVk->updateColorResolveAttachment(readColorIndexGL, resolveImageViewSerial);
    srcFramebufferVk->releaseCurrentFramebuffer(contextVk);

    // Since the source FBO was updated with a resolve attachment it didn't have when the render
    // pass was started, we need to:
    // 1. Get the new framebuffer
    //   - The draw framebuffer's ImageView will be used as the resolve attachment, so pass it along
    //   in case vkCreateFramebuffer() needs to be called to create a new vkFramebuffer with the new
    //   resolve attachment.
    RenderTargetVk *drawRenderTarget      = mRenderTargetCache.getColors()[drawColorIndexGL];
    const vk::ImageView *resolveImageView = nullptr;
    ANGLE_TRY(drawRenderTarget->getImageView(contextVk, &resolveImageView));
    vk::Framebuffer *newSrcFramebuffer = nullptr;
    ANGLE_TRY(srcFramebufferVk->getFramebuffer(contextVk, &newSrcFramebuffer, resolveImageView,
                                               SwapchainResolveMode::Disabled));
    // 2. Update the RenderPassCommandBufferHelper with the new framebuffer and render pass
    vk::RenderPassCommandBufferHelper &commandBufferHelper =
        contextVk->getStartedRenderPassCommands();
    commandBufferHelper.updateRenderPassForResolve(contextVk, newSrcFramebuffer,
                                                   srcFramebufferVk->getRenderPassDesc());

    // End the render pass now since we don't (yet) support subpass dependencies.
    drawRenderTarget->onColorResolve(contextVk, mCurrentFramebufferDesc.getLayerCount());
    ANGLE_TRY(contextVk->flushCommandsAndEndRenderPass(
        RenderPassClosureReason::AlreadySpecifiedElsewhere));

    // Remove the resolve attachment from the source framebuffer.
    srcFramebufferVk->removeColorResolveAttachment(readColorIndexGL);
    srcFramebufferVk->releaseCurrentFramebuffer(contextVk);

    return angle::Result::Continue;
}

angle::Result FramebufferVk::resolveColorWithCommand(ContextVk *contextVk,
                                                     const UtilsVk::BlitResolveParameters &params,
                                                     vk::ImageHelper *srcImage)
{
    vk::CommandBufferAccess access;
    access.onImageTransferRead(VK_IMAGE_ASPECT_COLOR_BIT, srcImage);

    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
        vk::ImageHelper &dstImage        = drawRenderTarget->getImageForWrite();

        access.onImageTransferWrite(drawRenderTarget->getLevelIndex(), 1,
                                    drawRenderTarget->getLayerIndex(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                    &dstImage);
    }

    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer(access, &commandBuffer));

    VkImageResolve resolveRegion                = {};
    resolveRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    resolveRegion.srcSubresource.mipLevel       = 0;
    resolveRegion.srcSubresource.baseArrayLayer = params.srcLayer;
    resolveRegion.srcSubresource.layerCount     = 1;
    resolveRegion.srcOffset.x                   = params.blitArea.x;
    resolveRegion.srcOffset.y                   = params.blitArea.y;
    resolveRegion.srcOffset.z                   = 0;
    resolveRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    resolveRegion.dstSubresource.layerCount     = 1;
    resolveRegion.dstOffset.x                   = params.blitArea.x;
    resolveRegion.dstOffset.y                   = params.blitArea.y;
    resolveRegion.dstOffset.z                   = 0;
    resolveRegion.extent.width                  = params.blitArea.width;
    resolveRegion.extent.height                 = params.blitArea.height;
    resolveRegion.extent.depth                  = 1;

    angle::VulkanPerfCounters &perfCounters = contextVk->getPerfCounters();
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        RenderTargetVk *drawRenderTarget = mRenderTargetCache.getColors()[colorIndexGL];
        vk::ImageHelper &dstImage        = drawRenderTarget->getImageForWrite();

        vk::LevelIndex levelVk = dstImage.toVkLevel(drawRenderTarget->getLevelIndex());
        resolveRegion.dstSubresource.mipLevel       = levelVk.get();
        resolveRegion.dstSubresource.baseArrayLayer = drawRenderTarget->getLayerIndex();

        srcImage->resolve(&dstImage, resolveRegion, commandBuffer);

        perfCounters.resolveImageCommands++;
    }

    return angle::Result::Continue;
}

gl::FramebufferStatus FramebufferVk::checkStatus(const gl::Context *context) const
{
    // if we have both a depth and stencil buffer, they must refer to the same object
    // since we only support packed_depth_stencil and not separate depth and stencil
    if (mState.hasSeparateDepthAndStencilAttachments())
    {
        return gl::FramebufferStatus::Incomplete(
            GL_FRAMEBUFFER_UNSUPPORTED,
            gl::err::kFramebufferIncompleteUnsupportedSeparateDepthStencilBuffers);
    }

    return gl::FramebufferStatus::Complete();
}

angle::Result FramebufferVk::invalidateImpl(ContextVk *contextVk,
                                            size_t count,
                                            const GLenum *attachments,
                                            bool isSubInvalidate,
                                            const gl::Rectangle &invalidateArea)
{
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

    // Shouldn't try to issue deferred clears if invalidating sub framebuffer.
    ASSERT(mDeferredClears.empty() || !isSubInvalidate);

    // Remove deferred clears for the invalidated attachments.
    if (invalidateDepthBuffer)
    {
        mDeferredClears.reset(vk::kUnpackedDepthIndex);
    }
    if (invalidateStencilBuffer)
    {
        mDeferredClears.reset(vk::kUnpackedStencilIndex);
    }
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        if (invalidateColorBuffers.test(colorIndexGL))
        {
            mDeferredClears.reset(colorIndexGL);
        }
    }

    // If there are still deferred clears, redefer them.  See relevant comment in invalidateSub.
    redeferClears(contextVk);

    const auto &colorRenderTargets           = mRenderTargetCache.getColors();
    RenderTargetVk *depthStencilRenderTarget = mRenderTargetCache.getDepthStencil();

    // If not a partial invalidate, mark the contents of the invalidated attachments as undefined,
    // so their loadOp can be set to DONT_CARE in the following render pass.
    if (!isSubInvalidate)
    {
        for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
        {
            if (invalidateColorBuffers.test(colorIndexGL))
            {
                RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
                ASSERT(colorRenderTarget);

                bool preferToKeepContentsDefined = false;
                colorRenderTarget->invalidateEntireContent(contextVk, &preferToKeepContentsDefined);
                if (preferToKeepContentsDefined)
                {
                    invalidateColorBuffers.reset(colorIndexGL);
                }
            }
        }

        // If we have a depth / stencil render target, invalidate its aspects.
        if (depthStencilRenderTarget)
        {
            if (invalidateDepthBuffer)
            {
                bool preferToKeepContentsDefined = false;
                depthStencilRenderTarget->invalidateEntireContent(contextVk,
                                                                  &preferToKeepContentsDefined);
                if (preferToKeepContentsDefined)
                {
                    invalidateDepthBuffer = false;
                }
            }
            if (invalidateStencilBuffer)
            {
                bool preferToKeepContentsDefined = false;
                depthStencilRenderTarget->invalidateEntireStencilContent(
                    contextVk, &preferToKeepContentsDefined);
                if (preferToKeepContentsDefined)
                {
                    invalidateStencilBuffer = false;
                }
            }
        }
    }

    // To ensure we invalidate the right renderpass we require that the current framebuffer be the
    // same as the current renderpass' framebuffer. E.g. prevent sequence like:
    //- Bind FBO 1, draw
    //- Bind FBO 2, draw
    //- Bind FBO 1, invalidate D/S
    // to invalidate the D/S of FBO 2 since it would be the currently active renderpass.
    vk::Framebuffer *currentFramebuffer = nullptr;
    ANGLE_TRY(
        getFramebuffer(contextVk, &currentFramebuffer, nullptr, SwapchainResolveMode::Disabled));

    if (contextVk->hasStartedRenderPassWithFramebuffer(currentFramebuffer))
    {
        // Mark the invalidated attachments in the render pass for loadOp and storeOp determination
        // at its end.
        vk::PackedAttachmentIndex colorIndexVk(0);
        for (size_t colorIndexGL : mState.getColorAttachmentsMask())
        {
            if (mState.getEnabledDrawBuffers()[colorIndexGL] &&
                invalidateColorBuffers.test(colorIndexGL))
            {
                contextVk->getStartedRenderPassCommands().invalidateRenderPassColorAttachment(
                    contextVk->getState(), colorIndexGL, colorIndexVk, invalidateArea);
            }
            ++colorIndexVk;
        }

        if (depthStencilRenderTarget)
        {
            const gl::DepthStencilState &dsState = contextVk->getState().getDepthStencilState();
            if (invalidateDepthBuffer)
            {
                contextVk->getStartedRenderPassCommands().invalidateRenderPassDepthAttachment(
                    dsState, invalidateArea);
            }

            if (invalidateStencilBuffer)
            {
                contextVk->getStartedRenderPassCommands().invalidateRenderPassStencilAttachment(
                    dsState, invalidateArea);
            }
        }
        if (invalidateColorBuffers.any())
        {
            // Only end the render pass if invalidating at least one color buffer.  Do not end the
            // render pass if only the depth and/or stencil buffer is invalidated.  At least one
            // application invalidates those every frame, disables depth, and then continues to
            // draw only to the color buffer.
            //
            // Since we are not aware of any application that invalidates a color buffer and
            // continues to draw to it, we leave that unoptimized.
            ANGLE_TRY(contextVk->flushCommandsAndEndRenderPass(
                RenderPassClosureReason::ColorBufferInvalidate));
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::updateColorAttachment(const gl::Context *context,
                                                   uint32_t colorIndexGL)
{
    ANGLE_TRY(mRenderTargetCache.updateColorRenderTarget(context, mState, colorIndexGL));

    // Update cached masks for masked clears.
    RenderTargetVk *renderTarget = mRenderTargetCache.getColors()[colorIndexGL];
    if (renderTarget)
    {
        const angle::Format &actualFormat = renderTarget->getImageActualFormat();
        updateActiveColorMasks(colorIndexGL, actualFormat.redBits > 0, actualFormat.greenBits > 0,
                               actualFormat.blueBits > 0, actualFormat.alphaBits > 0);

        const angle::Format &intendedFormat = renderTarget->getImageIntendedFormat();
        mEmulatedAlphaAttachmentMask.set(
            colorIndexGL, intendedFormat.alphaBits == 0 && actualFormat.alphaBits > 0);
    }
    else
    {
        updateActiveColorMasks(colorIndexGL, false, false, false, false);
    }

    const bool enabledColor =
        renderTarget && mState.getColorAttachments()[colorIndexGL].isAttached();
    const bool enabledResolve = enabledColor && renderTarget->hasResolveAttachment();

    if (enabledColor)
    {
        mCurrentFramebufferDesc.updateColor(colorIndexGL, renderTarget->getDrawSubresourceSerial());
        const bool isCreatedWithAHB = mState.getColorAttachments()[colorIndexGL].isCreatedWithAHB();
        mIsAHBColorAttachments.set(colorIndexGL, isCreatedWithAHB);
    }
    else
    {
        mCurrentFramebufferDesc.updateColor(colorIndexGL,
                                            vk::kInvalidImageOrBufferViewSubresourceSerial);
    }

    if (enabledResolve)
    {
        mCurrentFramebufferDesc.updateColorResolve(colorIndexGL,
                                                   renderTarget->getResolveSubresourceSerial());
    }
    else
    {
        mCurrentFramebufferDesc.updateColorResolve(colorIndexGL,
                                                   vk::kInvalidImageOrBufferViewSubresourceSerial);
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::updateDepthStencilAttachment(const gl::Context *context)
{
    ANGLE_TRY(mRenderTargetCache.updateDepthStencilRenderTarget(context, mState));

    ContextVk *contextVk = vk::GetImpl(context);
    updateDepthStencilAttachmentSerial(contextVk);

    return angle::Result::Continue;
}

void FramebufferVk::updateDepthStencilAttachmentSerial(ContextVk *contextVk)
{
    RenderTargetVk *depthStencilRT = getDepthStencilRenderTarget();

    if (depthStencilRT != nullptr)
    {
        mCurrentFramebufferDesc.updateDepthStencil(depthStencilRT->getDrawSubresourceSerial());
    }
    else
    {
        mCurrentFramebufferDesc.updateDepthStencil(vk::kInvalidImageOrBufferViewSubresourceSerial);
    }

    if (depthStencilRT != nullptr && depthStencilRT->hasResolveAttachment())
    {
        mCurrentFramebufferDesc.updateDepthStencilResolve(
            depthStencilRT->getResolveSubresourceSerial());
    }
    else
    {
        mCurrentFramebufferDesc.updateDepthStencilResolve(
            vk::kInvalidImageOrBufferViewSubresourceSerial);
    }
}

angle::Result FramebufferVk::flushColorAttachmentUpdates(const gl::Context *context,
                                                         bool deferClears,
                                                         uint32_t colorIndexGL)
{
    ContextVk *contextVk             = vk::GetImpl(context);
    RenderTargetVk *readRenderTarget = nullptr;
    RenderTargetVk *drawRenderTarget = nullptr;

    // It's possible for the read and draw color attachments to be different if different surfaces
    // are bound, so we need to flush any staged updates to both.

    // Draw
    drawRenderTarget = mRenderTargetCache.getColorDraw(mState, colorIndexGL);
    if (drawRenderTarget)
    {
        if (deferClears)
        {
            ANGLE_TRY(
                drawRenderTarget->flushStagedUpdates(contextVk, &mDeferredClears, colorIndexGL,
                                                     mCurrentFramebufferDesc.getLayerCount()));
        }
        else
        {
            ANGLE_TRY(drawRenderTarget->flushStagedUpdates(
                contextVk, nullptr, 0, mCurrentFramebufferDesc.getLayerCount()));
        }
    }

    // Read
    if (mState.getReadBufferState() != GL_NONE && mState.getReadIndex() == colorIndexGL)
    {
        // Flush staged updates to the read render target as well, but only if it's not the same as
        // the draw render target.  This can happen when the read render target is bound to another
        // surface.
        readRenderTarget = mRenderTargetCache.getColorRead(mState);
        if (readRenderTarget && readRenderTarget != drawRenderTarget)
        {
            ANGLE_TRY(readRenderTarget->flushStagedUpdates(
                contextVk, nullptr, 0, mCurrentFramebufferDesc.getLayerCount()));
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferVk::flushDepthStencilAttachmentUpdates(const gl::Context *context,
                                                                bool deferClears)
{
    ContextVk *contextVk = vk::GetImpl(context);

    RenderTargetVk *depthStencilRT = getDepthStencilRenderTarget();
    if (depthStencilRT == nullptr)
    {
        return angle::Result::Continue;
    }

    if (deferClears)
    {
        return depthStencilRT->flushStagedUpdates(contextVk, &mDeferredClears,
                                                  vk::kUnpackedDepthIndex,
                                                  mCurrentFramebufferDesc.getLayerCount());
    }

    return depthStencilRT->flushStagedUpdates(contextVk, nullptr, 0,
                                              mCurrentFramebufferDesc.getLayerCount());
}

angle::Result FramebufferVk::syncState(const gl::Context *context,
                                       GLenum binding,
                                       const gl::Framebuffer::DirtyBits &dirtyBits,
                                       gl::Command command)
{
    ContextVk *contextVk = vk::GetImpl(context);

    vk::FramebufferDesc priorFramebufferDesc = mCurrentFramebufferDesc;

    // Keep track of which attachments have dirty content and need their staged updates flushed.
    // The respective functions depend on |mCurrentFramebufferDesc::mLayerCount| which is updated
    // after all attachment render targets are updated.
    gl::DrawBufferMask dirtyColorAttachments;
    bool dirtyDepthStencilAttachment = false;

    bool shouldUpdateColorMaskAndBlend    = false;
    bool shouldUpdateLayerCount           = false;
    bool shouldUpdateSrgbWriteControlMode = false;

    // For any updated attachments we'll update their Serials below
    ASSERT(dirtyBits.any());
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_DEPTH_BUFFER_CONTENTS:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_BUFFER_CONTENTS:
                ANGLE_TRY(updateDepthStencilAttachment(context));
                shouldUpdateLayerCount      = true;
                dirtyDepthStencilAttachment = true;
                break;
            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
                ANGLE_TRY(mRenderTargetCache.update(context, mState, dirtyBits));
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
                shouldUpdateColorMaskAndBlend = true;
                shouldUpdateLayerCount        = true;
                break;
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                // Invalidate the cache. If we have performance critical code hitting this path we
                // can add related data (such as width/height) to the cache
                releaseCurrentFramebuffer(contextVk);
                break;
            case gl::Framebuffer::DIRTY_BIT_FRAMEBUFFER_SRGB_WRITE_CONTROL_MODE:
                shouldUpdateSrgbWriteControlMode = true;
                break;
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_LAYERS:
                shouldUpdateLayerCount = true;
                break;
            default:
            {
                static_assert(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0, "FB dirty bits");
                uint32_t colorIndexGL;
                if (dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX)
                {
                    colorIndexGL = static_cast<uint32_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                }
                else
                {
                    ASSERT(dirtyBit >= gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0 &&
                           dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_MAX);
                    colorIndexGL = static_cast<uint32_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0);
                }

                ANGLE_TRY(updateColorAttachment(context, colorIndexGL));

                // Window system framebuffer only have one color attachment and its property should
                // never change unless via DIRTY_BIT_DRAW_BUFFERS bit.
                if (!mState.isDefault())
                {
                    shouldUpdateColorMaskAndBlend = true;
                    shouldUpdateLayerCount        = true;
                }
                dirtyColorAttachments.set(colorIndexGL);

                break;
            }
        }
    }

    if (shouldUpdateSrgbWriteControlMode)
    {
        // Framebuffer colorspace state has been modified, so refresh the current framebuffer
        // descriptor to reflect the new state.
        gl::SrgbWriteControlMode newSrgbWriteControlMode = mState.getWriteControlMode();
        mCurrentFramebufferDesc.setWriteControlMode(newSrgbWriteControlMode);
        mRenderPassDesc.setWriteControlMode(newSrgbWriteControlMode);
    }

    if (shouldUpdateColorMaskAndBlend)
    {
        contextVk->updateColorMasks();
        contextVk->updateBlendFuncsAndEquations();
    }

    if (shouldUpdateLayerCount)
    {
        updateLayerCount();
    }

    // Only defer clears for draw framebuffer ops.  Note that this will result in a render area that
    // completely covers the framebuffer, even if the operation that follows is scissored.
    bool deferClears = binding == GL_DRAW_FRAMEBUFFER;

    // If we are notified that any attachment is dirty, but we have deferred clears for them, a
    // flushDeferredClears() call is missing somewhere.  ASSERT this to catch these bugs.
    vk::ClearValuesArray previousDeferredClears = mDeferredClears;

    for (size_t colorIndexGL : dirtyColorAttachments)
    {
        ASSERT(!previousDeferredClears.test(colorIndexGL));
        ANGLE_TRY(
            flushColorAttachmentUpdates(context, deferClears, static_cast<uint32_t>(colorIndexGL)));
    }
    if (dirtyDepthStencilAttachment)
    {
        ASSERT(!previousDeferredClears.testDepth());
        ASSERT(!previousDeferredClears.testStencil());
        ANGLE_TRY(flushDepthStencilAttachmentUpdates(context, deferClears));
    }

    // No-op redundant changes to prevent closing the RenderPass.
    if (mCurrentFramebufferDesc == priorFramebufferDesc)
    {
        return angle::Result::Continue;
    }

    if (command != gl::Command::Blit)
    {
        // Don't end the render pass when handling a blit to resolve, since we may be able to
        // optimize that path which requires modifying the current render pass.
        // We're deferring the resolve check to FramebufferVk::blit(), since if the read buffer is
        // multisampled-render-to-texture, then srcFramebuffer->getSamples(context) gives > 1, but
        // there's no resolve happening as the read buffer's single sampled image will be used as
        // blit src. FramebufferVk::blit() will handle those details for us.
        ANGLE_TRY(
            contextVk->flushCommandsAndEndRenderPass(RenderPassClosureReason::FramebufferChange));
    }

    updateRenderPassDesc(contextVk);

    // Deactivate Framebuffer
    releaseCurrentFramebuffer(contextVk);

    // Notify the ContextVk to update the pipeline desc.
    return contextVk->onFramebufferChange(this, command);
}

void FramebufferVk::updateRenderPassDesc(ContextVk *contextVk)
{
    mRenderPassDesc = {};
    mRenderPassDesc.setSamples(getSamples());
    mRenderPassDesc.setViewCount(
        mState.isMultiview() && mState.getNumViews() > 1 ? mState.getNumViews() : 0);

    // Color attachments.
    const auto &colorRenderTargets               = mRenderTargetCache.getColors();
    const gl::DrawBufferMask colorAttachmentMask = mState.getColorAttachmentsMask();
    for (size_t colorIndexGL = 0; colorIndexGL < colorAttachmentMask.size(); ++colorIndexGL)
    {
        if (colorAttachmentMask[colorIndexGL])
        {
            RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
            ASSERT(colorRenderTarget);
            mRenderPassDesc.packColorAttachment(
                colorIndexGL, colorRenderTarget->getImageForRenderPass().getActualFormatID());

            // Add the resolve attachment, if any.
            if (colorRenderTarget->hasResolveAttachment())
            {
                mRenderPassDesc.packColorResolveAttachment(colorIndexGL);
            }
        }
        else
        {
            mRenderPassDesc.packColorAttachmentGap(colorIndexGL);
        }
    }

    // Depth/stencil attachment.
    RenderTargetVk *depthStencilRenderTarget = getDepthStencilRenderTarget();
    if (depthStencilRenderTarget)
    {
        mRenderPassDesc.packDepthStencilAttachment(
            depthStencilRenderTarget->getImageForRenderPass().getActualFormatID());

        // Add the resolve attachment, if any.
        if (depthStencilRenderTarget->hasResolveAttachment())
        {
            mRenderPassDesc.packDepthStencilResolveAttachment();
        }
    }

    if (contextVk->isInFramebufferFetchMode())
    {
        mRenderPassDesc.setFramebufferFetchMode(true);
    }

    if (contextVk->getFeatures().enableMultisampledRenderToTexture.enabled)
    {
        // Update descriptions regarding multisampled-render-to-texture use.
        bool isRenderToTexture = false;
        for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
        {
            const gl::FramebufferAttachment *color = mState.getColorAttachment(colorIndexGL);
            ASSERT(color);

            if (color->isRenderToTexture())
            {
                isRenderToTexture = true;
                break;
            }
        }
        const gl::FramebufferAttachment *depthStencil = mState.getDepthStencilAttachment();
        if (depthStencil && depthStencil->isRenderToTexture())
        {
            isRenderToTexture = true;
        }

        mCurrentFramebufferDesc.updateRenderToTexture(isRenderToTexture);
        mRenderPassDesc.updateRenderToTexture(isRenderToTexture);
    }

    mCurrentFramebufferDesc.updateUnresolveMask({});
    mRenderPassDesc.setWriteControlMode(mCurrentFramebufferDesc.getWriteControlMode());
}

angle::Result FramebufferVk::getFramebuffer(ContextVk *contextVk,
                                            vk::Framebuffer **framebufferOut,
                                            const vk::ImageView *resolveImageViewIn,
                                            const SwapchainResolveMode swapchainResolveMode)
{
    ASSERT(mCurrentFramebufferDesc.hasFramebufferFetch() == mRenderPassDesc.hasFramebufferFetch());
    // First return a presently valid Framebuffer
    if (mCurrentFramebuffer.valid())
    {
        *framebufferOut = &mCurrentFramebuffer;
        return angle::Result::Continue;
    }
    // No current FB, so now check for previously cached Framebuffer
    if (contextVk->getShareGroup()->getFramebufferCache().get(contextVk, mCurrentFramebufferDesc,
                                                              mCurrentFramebuffer))
    {
        ASSERT(mCurrentFramebuffer.valid());
        *framebufferOut             = &mCurrentFramebuffer;
        mIsCurrentFramebufferCached = true;
        return angle::Result::Continue;
    }

    vk::RenderPass *compatibleRenderPass = nullptr;
    ANGLE_TRY(contextVk->getCompatibleRenderPass(mRenderPassDesc, &compatibleRenderPass));

    // If we've a Framebuffer provided by a Surface (default FBO/backbuffer), query it.
    if (mBackbuffer)
    {
        return mBackbuffer->getCurrentFramebuffer(
            contextVk,
            mRenderPassDesc.hasFramebufferFetch() ? FramebufferFetchMode::Enabled
                                                  : FramebufferFetchMode::Disabled,
            *compatibleRenderPass, swapchainResolveMode, framebufferOut);
    }

    // Gather VkImageViews over all FBO attachments, also size of attached region.
    std::vector<VkImageView> attachments;
    gl::Extents attachmentsSize = mState.getExtents();
    ASSERT(attachmentsSize.width != 0 && attachmentsSize.height != 0);

    // Color attachments.
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(colorRenderTarget->getImageViewWithColorspace(
            contextVk, mCurrentFramebufferDesc.getWriteControlMode(), &imageView));

        attachments.push_back(imageView->getHandle());
    }

    // Depth/stencil attachment.
    RenderTargetVk *depthStencilRenderTarget = getDepthStencilRenderTarget();
    if (depthStencilRenderTarget)
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(depthStencilRenderTarget->getImageView(contextVk, &imageView));

        attachments.push_back(imageView->getHandle());
    }

    // Color resolve attachments.
    if (resolveImageViewIn)
    {
        ASSERT(!HasResolveAttachment(colorRenderTargets, mState.getEnabledDrawBuffers()));

        // Need to use the passed in ImageView for the resolve attachment, since it came from
        // another Framebuffer.
        attachments.push_back(resolveImageViewIn->getHandle());
    }
    else
    {
        // This Framebuffer owns all of the ImageViews, including its own resolve ImageViews.
        for (size_t colorIndexGL : mState.getColorAttachmentsMask())
        {
            RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
            ASSERT(colorRenderTarget);

            if (colorRenderTarget->hasResolveAttachment())
            {
                const vk::ImageView *resolveImageView = nullptr;
                ANGLE_TRY(colorRenderTarget->getResolveImageView(contextVk, &resolveImageView));

                attachments.push_back(resolveImageView->getHandle());
            }
        }
    }

    // Depth/stencil resolve attachment.
    if (depthStencilRenderTarget && depthStencilRenderTarget->hasResolveAttachment())
    {
        const vk::ImageView *imageView = nullptr;
        ANGLE_TRY(depthStencilRenderTarget->getResolveImageView(contextVk, &imageView));

        attachments.push_back(imageView->getHandle());
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
    if (!mCurrentFramebufferDesc.isMultiview())
    {
        framebufferInfo.layers = std::max(mCurrentFramebufferDesc.getLayerCount(), 1u);
    }

    vk::FramebufferHelper newFramebuffer;
    ANGLE_TRY(newFramebuffer.init(contextVk, framebufferInfo));

    // Check that our description matches our attachments. Can catch implementation bugs.
    ASSERT(static_cast<uint32_t>(attachments.size()) == mCurrentFramebufferDesc.attachmentCount());

    // Since the cache key FramebufferDesc can't distinguish between
    // two FramebufferHelper, if they both have 0 attachment, but their sizes
    // are different, we could have wrong cache hit(new framebufferHelper has
    // a bigger height and width, but get cache hit with framebufferHelper of
    // lower height and width). As a workaround, do not cache the
    // FramebufferHelper if it doesn't have any attachment.
    if (attachments.size() > 0)
    {
        insertCache(contextVk, mCurrentFramebufferDesc, std::move(newFramebuffer));
        bool result = contextVk->getShareGroup()->getFramebufferCache().get(
            contextVk, mCurrentFramebufferDesc, mCurrentFramebuffer);
        ASSERT(result);
        mIsCurrentFramebufferCached = true;
    }
    else
    {
        mCurrentFramebuffer         = std::move(newFramebuffer.getFramebuffer());
        mIsCurrentFramebufferCached = false;
    }
    ASSERT(mCurrentFramebuffer.valid());
    *framebufferOut = &mCurrentFramebuffer;
    return angle::Result::Continue;
}

void FramebufferVk::mergeClearsWithDeferredClears(
    gl::DrawBufferMask clearColorBuffers,
    bool clearDepth,
    bool clearStencil,
    const VkClearColorValue &clearColorValue,
    const VkClearDepthStencilValue &clearDepthStencilValue)
{
    // Apply clears to mDeferredClears.  Note that clears override deferred clears.

    // Color clears.
    for (size_t colorIndexGL : clearColorBuffers)
    {
        ASSERT(mState.getEnabledDrawBuffers().test(colorIndexGL));
        VkClearValue clearValue = getCorrectedColorClearValue(colorIndexGL, clearColorValue);
        mDeferredClears.store(static_cast<uint32_t>(colorIndexGL), VK_IMAGE_ASPECT_COLOR_BIT,
                              clearValue);
    }

    // Depth and stencil clears.
    VkImageAspectFlags dsAspectFlags = 0;
    VkClearValue dsClearValue        = {};
    dsClearValue.depthStencil        = clearDepthStencilValue;
    if (clearDepth)
    {
        dsAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (clearStencil)
    {
        dsAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (dsAspectFlags != 0)
    {
        mDeferredClears.store(vk::kUnpackedDepthIndex, dsAspectFlags, dsClearValue);
    }
}

angle::Result FramebufferVk::clearWithDraw(ContextVk *contextVk,
                                           const gl::Rectangle &clearArea,
                                           gl::DrawBufferMask clearColorBuffers,
                                           bool clearDepth,
                                           bool clearStencil,
                                           gl::BlendStateExt::ColorMaskStorage::Type colorMasks,
                                           uint8_t stencilMask,
                                           const VkClearColorValue &clearColorValue,
                                           const VkClearDepthStencilValue &clearDepthStencilValue)
{
    // All deferred clears should be handled already.
    ASSERT(mDeferredClears.empty());

    UtilsVk::ClearFramebufferParameters params = {};
    params.clearArea                           = clearArea;
    params.colorClearValue                     = clearColorValue;
    params.depthStencilClearValue              = clearDepthStencilValue;
    params.stencilMask                         = stencilMask;

    params.clearColor   = true;
    params.clearDepth   = clearDepth;
    params.clearStencil = clearStencil;

    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    for (size_t colorIndexGL : clearColorBuffers)
    {
        const RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        params.colorFormat = &colorRenderTarget->getImageForRenderPass().getActualFormat();
        params.colorAttachmentIndexGL = static_cast<uint32_t>(colorIndexGL);
        params.colorMaskFlags =
            gl::BlendStateExt::ColorMaskStorage::GetValueIndexed(colorIndexGL, colorMasks);
        if (mEmulatedAlphaAttachmentMask[colorIndexGL])
        {
            params.colorMaskFlags &= ~VK_COLOR_COMPONENT_A_BIT;
        }

        // TODO: implement clear of layered framebuffers.  UtilsVk::clearFramebuffer should add a
        // geometry shader that is instanced layerCount times (or loops layerCount times), each time
        // selecting a different layer.
        // http://anglebug.com/5453
        ASSERT(mCurrentFramebufferDesc.isMultiview() || colorRenderTarget->getLayerCount() == 1);

        ANGLE_TRY(contextVk->getUtils().clearFramebuffer(contextVk, this, params));

        // Clear depth/stencil only once!
        params.clearDepth   = false;
        params.clearStencil = false;
    }

    // If there was no color clear, clear depth/stencil alone.
    if (params.clearDepth || params.clearStencil)
    {
        params.clearColor = false;
        ANGLE_TRY(contextVk->getUtils().clearFramebuffer(contextVk, this, params));
    }

    return angle::Result::Continue;
}

VkClearValue FramebufferVk::getCorrectedColorClearValue(size_t colorIndexGL,
                                                        const VkClearColorValue &clearColor) const
{
    VkClearValue clearValue = {};
    clearValue.color        = clearColor;

    if (!mEmulatedAlphaAttachmentMask[colorIndexGL])
    {
        return clearValue;
    }

    // If the render target doesn't have alpha, but its emulated format has it, clear the alpha
    // to 1.
    RenderTargetVk *renderTarget = getColorDrawRenderTarget(colorIndexGL);
    const angle::Format &format  = renderTarget->getImageActualFormat();

    if (format.isUint())
    {
        clearValue.color.uint32[3] = kEmulatedAlphaValue;
    }
    else if (format.isSint())
    {
        clearValue.color.int32[3] = kEmulatedAlphaValue;
    }
    else
    {
        clearValue.color.float32[3] = kEmulatedAlphaValue;
    }

    return clearValue;
}

void FramebufferVk::redeferClears(ContextVk *contextVk)
{
    ASSERT(!contextVk->hasStartedRenderPass() || !mDeferredClears.any());

    // Set the appropriate loadOp and clear values for depth and stencil.
    VkImageAspectFlags dsAspectFlags  = 0;
    VkClearValue dsClearValue         = {};
    dsClearValue.depthStencil.depth   = mDeferredClears.getDepthValue();
    dsClearValue.depthStencil.stencil = mDeferredClears.getStencilValue();

    if (mDeferredClears.testDepth())
    {
        dsAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        mDeferredClears.reset(vk::kUnpackedDepthIndex);
    }

    if (mDeferredClears.testStencil())
    {
        dsAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        mDeferredClears.reset(vk::kUnpackedStencilIndex);
    }

    // Go through deferred clears and stage the clears for future.
    for (size_t colorIndexGL : mDeferredClears.getColorMask())
    {
        RenderTargetVk *renderTarget = getColorDrawRenderTarget(colorIndexGL);
        gl::ImageIndex imageIndex =
            renderTarget->getImageIndexForClear(mCurrentFramebufferDesc.getLayerCount());
        renderTarget->getImageForWrite().stageClear(imageIndex, VK_IMAGE_ASPECT_COLOR_BIT,
                                                    mDeferredClears[colorIndexGL]);
        mDeferredClears.reset(colorIndexGL);
    }

    if (dsAspectFlags)
    {
        RenderTargetVk *renderTarget = getDepthStencilRenderTarget();
        ASSERT(renderTarget);

        gl::ImageIndex imageIndex =
            renderTarget->getImageIndexForClear(mCurrentFramebufferDesc.getLayerCount());
        renderTarget->getImageForWrite().stageClear(imageIndex, dsAspectFlags, dsClearValue);
    }
}

void FramebufferVk::clearWithCommand(ContextVk *contextVk, const gl::Rectangle &scissoredRenderArea)
{
    // Clear is not affected by viewport, so ContextVk::updateScissor may have decided on a smaller
    // render area.  Grow the render area to the full framebuffer size as this clear path is taken
    // when not scissored.
    vk::RenderPassCommandBufferHelper *renderPassCommands =
        &contextVk->getStartedRenderPassCommands();
    renderPassCommands->growRenderArea(contextVk, scissoredRenderArea);

    gl::AttachmentVector<VkClearAttachment> attachments;

    // Go through deferred clears and add them to the list of attachments to clear.  If any
    // attachment is unused, skip the clear.  clearWithLoadOp will follow and move the remaining
    // clears up to loadOp.
    vk::PackedAttachmentIndex colorIndexVk(0);
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        if (mDeferredClears.getColorMask().test(colorIndexGL))
        {
            if (!renderPassCommands->hasAnyColorAccess(colorIndexVk))
            {
                // Skip this attachment, so we can use a renderpass loadOp to clear it instead.
                // Note that if loadOp=Clear was already used for this color attachment, it will be
                // overriden by the new clear, which is valid because the attachment wasn't used in
                // between.
                ++colorIndexVk;
                continue;
            }

            attachments.emplace_back(VkClearAttachment{VK_IMAGE_ASPECT_COLOR_BIT,
                                                       static_cast<uint32_t>(colorIndexGL),
                                                       mDeferredClears[colorIndexGL]});
            mDeferredClears.reset(colorIndexGL);
            ++contextVk->getPerfCounters().colorClearAttachments;

            renderPassCommands->onColorAccess(colorIndexVk, vk::ResourceAccess::Write);
        }
        ++colorIndexVk;
    }

    // Add depth and stencil to list of attachments as needed.
    VkImageAspectFlags dsAspectFlags  = 0;
    VkClearValue dsClearValue         = {};
    dsClearValue.depthStencil.depth   = mDeferredClears.getDepthValue();
    dsClearValue.depthStencil.stencil = mDeferredClears.getStencilValue();
    if (mDeferredClears.testDepth() && renderPassCommands->hasAnyDepthAccess())
    {
        dsAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        // Explicitly mark a depth write because we are clearing the depth buffer.
        renderPassCommands->onDepthAccess(vk::ResourceAccess::Write);
        mDeferredClears.reset(vk::kUnpackedDepthIndex);
        ++contextVk->getPerfCounters().depthClearAttachments;
    }

    if (mDeferredClears.testStencil() && renderPassCommands->hasAnyStencilAccess())
    {
        dsAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        // Explicitly mark a stencil write because we are clearing the stencil buffer.
        renderPassCommands->onStencilAccess(vk::ResourceAccess::Write);
        mDeferredClears.reset(vk::kUnpackedStencilIndex);
        ++contextVk->getPerfCounters().stencilClearAttachments;
    }

    if (dsAspectFlags != 0)
    {
        attachments.emplace_back(VkClearAttachment{dsAspectFlags, 0, dsClearValue});
        // Because we may have changed the depth stencil access mode, update read only depth mode
        // now.
        updateRenderPassReadOnlyDepthMode(contextVk, renderPassCommands);
    }

    if (attachments.empty())
    {
        return;
    }

    const uint32_t layerCount = mState.isMultiview() ? 1 : mCurrentFramebufferDesc.getLayerCount();

    VkClearRect rect                                     = {};
    rect.rect.extent.width                               = scissoredRenderArea.width;
    rect.rect.extent.height                              = scissoredRenderArea.height;
    rect.layerCount                                      = layerCount;
    vk::RenderPassCommandBuffer *renderPassCommandBuffer = &renderPassCommands->getCommandBuffer();

    renderPassCommandBuffer->clearAttachments(static_cast<uint32_t>(attachments.size()),
                                              attachments.data(), 1, &rect);
    return;
}

void FramebufferVk::clearWithLoadOp(ContextVk *contextVk)
{
    vk::RenderPassCommandBufferHelper *renderPassCommands =
        &contextVk->getStartedRenderPassCommands();

    // Update the render pass loadOps to clear the attachments.
    vk::PackedAttachmentIndex colorIndexVk(0);
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        if (!mDeferredClears.test(colorIndexGL))
        {
            ++colorIndexVk;
            continue;
        }

        ASSERT(!renderPassCommands->hasAnyColorAccess(colorIndexVk));

        renderPassCommands->updateRenderPassColorClear(colorIndexVk, mDeferredClears[colorIndexGL]);

        mDeferredClears.reset(colorIndexGL);

        ++colorIndexVk;
    }

    VkClearValue dsClearValue         = {};
    dsClearValue.depthStencil.depth   = mDeferredClears.getDepthValue();
    dsClearValue.depthStencil.stencil = mDeferredClears.getStencilValue();
    VkImageAspectFlags dsAspects      = 0;

    if (mDeferredClears.testDepth())
    {
        ASSERT(!renderPassCommands->hasAnyDepthAccess());
        dsAspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
        mDeferredClears.reset(vk::kUnpackedDepthIndex);
    }

    if (mDeferredClears.testStencil())
    {
        ASSERT(!renderPassCommands->hasAnyStencilAccess());
        dsAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
        mDeferredClears.reset(vk::kUnpackedStencilIndex);
    }

    if (dsAspects != 0)
    {
        renderPassCommands->updateRenderPassDepthStencilClear(dsAspects, dsClearValue);
        // The render pass can no longer be in read-only depth/stencil mode.
        updateRenderPassReadOnlyDepthMode(contextVk, renderPassCommands);
    }
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
                                                const gl::Rectangle &scissoredRenderArea,
                                                vk::RenderPassCommandBuffer **commandBufferOut,
                                                bool *renderPassDescChangedOut)
{
    ANGLE_TRY(contextVk->flushCommandsAndEndRenderPass(RenderPassClosureReason::NewRenderPass));

    // Initialize RenderPass info.
    vk::AttachmentOpsArray renderPassAttachmentOps;
    vk::PackedClearValuesArray packedClearValues;
    gl::DrawBufferMask previousUnresolveColorMask =
        mRenderPassDesc.getColorUnresolveAttachmentMask();
    const bool hasDeferredClears        = mDeferredClears.any();
    const bool previousUnresolveDepth   = mRenderPassDesc.hasDepthUnresolveAttachment();
    const bool previousUnresolveStencil = mRenderPassDesc.hasStencilUnresolveAttachment();

    // Make sure render pass and framebuffer are in agreement w.r.t unresolve attachments.
    ASSERT(mCurrentFramebufferDesc.getUnresolveAttachmentMask() ==
           MakeUnresolveAttachmentMask(mRenderPassDesc));

    // Color attachments.
    const auto &colorRenderTargets = mRenderTargetCache.getColors();
    vk::PackedAttachmentIndex colorIndexVk(0);
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        ASSERT(colorRenderTarget);

        // Color render targets are never entirely transient.  Only depth/stencil
        // multisampled-render-to-texture textures can be so.
        ASSERT(!colorRenderTarget->isEntirelyTransient());
        const vk::RenderPassStoreOp storeOp = colorRenderTarget->isImageTransient()
                                                  ? vk::RenderPassStoreOp::DontCare
                                                  : vk::RenderPassStoreOp::Store;

        if (mDeferredClears.test(colorIndexGL))
        {
            renderPassAttachmentOps.setOps(colorIndexVk, vk::RenderPassLoadOp::Clear, storeOp);
            packedClearValues.store(colorIndexVk, VK_IMAGE_ASPECT_COLOR_BIT,
                                    mDeferredClears[colorIndexGL]);
            mDeferredClears.reset(colorIndexGL);
        }
        else
        {
            const vk::RenderPassLoadOp loadOp = colorRenderTarget->hasDefinedContent()
                                                    ? vk::RenderPassLoadOp::Load
                                                    : vk::RenderPassLoadOp::DontCare;

            renderPassAttachmentOps.setOps(colorIndexVk, loadOp, storeOp);
            packedClearValues.store(colorIndexVk, VK_IMAGE_ASPECT_COLOR_BIT,
                                    kUninitializedClearValue);
        }
        renderPassAttachmentOps.setStencilOps(colorIndexVk, vk::RenderPassLoadOp::DontCare,
                                              vk::RenderPassStoreOp::DontCare);

        // If there's a resolve attachment, and loadOp needs to be LOAD, the multisampled attachment
        // needs to take its value from the resolve attachment.  In this case, an initial subpass is
        // added for this very purpose which uses the resolve attachment as input attachment.  As a
        // result, loadOp of the multisampled attachment can remain DONT_CARE.
        //
        // Note that this only needs to be done if the multisampled image and the resolve attachment
        // come from the same source.  isImageTransient() indicates whether this should happen.
        if (colorRenderTarget->hasResolveAttachment() && colorRenderTarget->isImageTransient())
        {
            if (renderPassAttachmentOps[colorIndexVk].loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            {
                renderPassAttachmentOps[colorIndexVk].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

                // Update the render pass desc to specify that this attachment should be unresolved.
                mRenderPassDesc.packColorUnresolveAttachment(colorIndexGL);
            }
            else
            {
                mRenderPassDesc.removeColorUnresolveAttachment(colorIndexGL);
            }
        }
        else
        {
            ASSERT(!mRenderPassDesc.getColorUnresolveAttachmentMask().test(colorIndexGL));
        }

        ++colorIndexVk;
    }

    // Depth/stencil attachment.
    vk::PackedAttachmentIndex depthStencilAttachmentIndex = vk::kAttachmentIndexInvalid;
    RenderTargetVk *depthStencilRenderTarget              = getDepthStencilRenderTarget();
    if (depthStencilRenderTarget)
    {
        const bool canExportStencil =
            contextVk->getRenderer()->getFeatures().supportsShaderStencilExport.enabled;

        // depth stencil attachment always immediately follows color attachment
        depthStencilAttachmentIndex = colorIndexVk;

        vk::RenderPassLoadOp depthLoadOp     = vk::RenderPassLoadOp::Load;
        vk::RenderPassLoadOp stencilLoadOp   = vk::RenderPassLoadOp::Load;
        vk::RenderPassStoreOp depthStoreOp   = vk::RenderPassStoreOp::Store;
        vk::RenderPassStoreOp stencilStoreOp = vk::RenderPassStoreOp::Store;

        // If the image data was previously discarded (with no update in between), don't attempt to
        // load the image.  Additionally, if the multisampled image data is transient and there is
        // no resolve attachment, there's no data to load.  The latter is the case with
        // depth/stencil texture attachments per GL_EXT_multisampled_render_to_texture2.
        if (!depthStencilRenderTarget->hasDefinedContent() ||
            depthStencilRenderTarget->isEntirelyTransient())
        {
            depthLoadOp = vk::RenderPassLoadOp::DontCare;
        }
        if (!depthStencilRenderTarget->hasDefinedStencilContent() ||
            depthStencilRenderTarget->isEntirelyTransient())
        {
            stencilLoadOp = vk::RenderPassLoadOp::DontCare;
        }

        // If depth/stencil image is transient, no need to store its data at the end of the render
        // pass.  If shader stencil export is not supported, stencil data cannot be unresolved on
        // the next render pass, so it must be stored/loaded.  If the image is entirely transient,
        // there is no resolve/unresolve and the image data is never stored/loaded.
        if (depthStencilRenderTarget->isImageTransient())
        {
            depthStoreOp = vk::RenderPassStoreOp::DontCare;

            if (canExportStencil || depthStencilRenderTarget->isEntirelyTransient())
            {
                stencilStoreOp = vk::RenderPassStoreOp::DontCare;
            }
        }

        if (mDeferredClears.testDepth() || mDeferredClears.testStencil())
        {
            VkClearValue clearValue = {};

            if (mDeferredClears.testDepth())
            {
                depthLoadOp                   = vk::RenderPassLoadOp::Clear;
                clearValue.depthStencil.depth = mDeferredClears.getDepthValue();
                mDeferredClears.reset(vk::kUnpackedDepthIndex);
            }

            if (mDeferredClears.testStencil())
            {
                stencilLoadOp                   = vk::RenderPassLoadOp::Clear;
                clearValue.depthStencil.stencil = mDeferredClears.getStencilValue();
                mDeferredClears.reset(vk::kUnpackedStencilIndex);
            }

            // Note the aspect is only depth here. That's intentional.
            packedClearValues.store(depthStencilAttachmentIndex, VK_IMAGE_ASPECT_DEPTH_BIT,
                                    clearValue);
        }
        else
        {
            // Note the aspect is only depth here. That's intentional.
            packedClearValues.store(depthStencilAttachmentIndex, VK_IMAGE_ASPECT_DEPTH_BIT,
                                    kUninitializedClearValue);
        }

        const angle::Format &format = depthStencilRenderTarget->getImageIntendedFormat();
        // If the format we picked has stencil but user did not ask for it due to hardware
        // limitations, use DONT_CARE for load/store. The same logic for depth follows.
        if (format.stencilBits == 0)
        {
            stencilLoadOp  = vk::RenderPassLoadOp::DontCare;
            stencilStoreOp = vk::RenderPassStoreOp::DontCare;
        }
        if (format.depthBits == 0)
        {
            depthLoadOp  = vk::RenderPassLoadOp::DontCare;
            depthStoreOp = vk::RenderPassStoreOp::DontCare;
        }

        // Similar to color attachments, if there's a resolve attachment and the multisampled image
        // is transient, depth/stencil data need to be unresolved in an initial subpass.
        //
        // Note that stencil unresolve is currently only possible if shader stencil export is
        // supported.
        if (depthStencilRenderTarget->hasResolveAttachment() &&
            depthStencilRenderTarget->isImageTransient())
        {
            const bool unresolveDepth = depthLoadOp == vk::RenderPassLoadOp::Load;
            const bool unresolveStencil =
                stencilLoadOp == vk::RenderPassLoadOp::Load && canExportStencil;

            if (unresolveDepth)
            {
                depthLoadOp = vk::RenderPassLoadOp::DontCare;
            }

            if (unresolveStencil)
            {
                stencilLoadOp = vk::RenderPassLoadOp::DontCare;
            }

            if (unresolveDepth || unresolveStencil)
            {
                mRenderPassDesc.packDepthStencilUnresolveAttachment(unresolveDepth,
                                                                    unresolveStencil);
            }
            else
            {
                mRenderPassDesc.removeDepthStencilUnresolveAttachment();
            }
        }

        renderPassAttachmentOps.setOps(depthStencilAttachmentIndex, depthLoadOp, depthStoreOp);
        renderPassAttachmentOps.setStencilOps(depthStencilAttachmentIndex, stencilLoadOp,
                                              stencilStoreOp);
    }

    // If render pass description is changed, the previous render pass desc is no longer compatible.
    // Tell the context so that the graphics pipelines can be recreated.
    //
    // Note that render passes are compatible only if the differences are in loadOp/storeOp values,
    // or the existence of resolve attachments in single subpass render passes.  The modification
    // here can add/remove a subpass, or modify its input attachments.
    gl::DrawBufferMask unresolveColorMask = mRenderPassDesc.getColorUnresolveAttachmentMask();
    const bool unresolveDepth             = mRenderPassDesc.hasDepthUnresolveAttachment();
    const bool unresolveStencil           = mRenderPassDesc.hasStencilUnresolveAttachment();
    const bool unresolveChanged           = previousUnresolveColorMask != unresolveColorMask ||
                                  previousUnresolveDepth != unresolveDepth ||
                                  previousUnresolveStencil != unresolveStencil;
    if (unresolveChanged)
    {
        // Make sure framebuffer is recreated.
        releaseCurrentFramebuffer(contextVk);

        mCurrentFramebufferDesc.updateUnresolveMask(MakeUnresolveAttachmentMask(mRenderPassDesc));
    }

    vk::Framebuffer *framebuffer = nullptr;
    ANGLE_TRY(getFramebuffer(contextVk, &framebuffer, nullptr, SwapchainResolveMode::Disabled));

    // If deferred clears were used in the render pass, expand the render area to the whole
    // framebuffer.
    gl::Rectangle renderArea = scissoredRenderArea;
    if (hasDeferredClears)
    {
        renderArea = getRotatedCompleteRenderArea(contextVk);
    }

    ANGLE_TRY(contextVk->beginNewRenderPass(
        *framebuffer, renderArea, mRenderPassDesc, renderPassAttachmentOps, colorIndexVk,
        depthStencilAttachmentIndex, packedClearValues, commandBufferOut));

    // Add the images to the renderpass tracking list (through onColorDraw).
    vk::PackedAttachmentIndex colorAttachmentIndex(0);
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        RenderTargetVk *colorRenderTarget = colorRenderTargets[colorIndexGL];
        colorRenderTarget->onColorDraw(contextVk, mCurrentFramebufferDesc.getLayerCount(),
                                       colorAttachmentIndex);
        ++colorAttachmentIndex;
    }

    if (depthStencilRenderTarget)
    {
        // This must be called after hasDefined*Content() since it will set content to valid.  If
        // the attachment ends up not used in the render pass, contents will be marked undefined at
        // endRenderPass.  The actual layout determination is also deferred until the same time.
        depthStencilRenderTarget->onDepthStencilDraw(contextVk,
                                                     mCurrentFramebufferDesc.getLayerCount());
    }

    const bool anyUnresolve = unresolveColorMask.any() || unresolveDepth || unresolveStencil;
    if (anyUnresolve)
    {
        // Unresolve attachments if any.
        UtilsVk::UnresolveParameters params;
        params.unresolveColorMask = unresolveColorMask;
        params.unresolveDepth     = unresolveDepth;
        params.unresolveStencil   = unresolveStencil;

        ANGLE_TRY(contextVk->getUtils().unresolve(contextVk, this, params));

        // The unresolve subpass has only one draw call.
        ANGLE_TRY(contextVk->startNextSubpass());
    }

    if (unresolveChanged || anyUnresolve)
    {
        contextVk->onDrawFramebufferRenderPassDescChange(this, renderPassDescChangedOut);
    }

    return angle::Result::Continue;
}

void FramebufferVk::updateActiveColorMasks(size_t colorIndexGL, bool r, bool g, bool b, bool a)
{
    gl::BlendStateExt::ColorMaskStorage::SetValueIndexed(
        colorIndexGL, gl::BlendStateExt::PackColorMask(r, g, b, a),
        &mActiveColorComponentMasksForClear);
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
    gl::LevelIndex levelGL = renderTarget->getLevelIndex();
    uint32_t layer         = renderTarget->getLayerIndex();
    return renderTarget->getImageForCopy().readPixels(contextVk, area, packPixelsParams,
                                                      copyAspectFlags, levelGL, layer, pixels);
}

gl::Extents FramebufferVk::getReadImageExtents() const
{
    RenderTargetVk *readRenderTarget = mRenderTargetCache.getColorRead(mState);
    return readRenderTarget->getExtents();
}

// Return the framebuffer's non-rotated render area.  This is a gl::Rectangle that is based on the
// dimensions of the framebuffer, IS NOT rotated, and IS NOT y-flipped
gl::Rectangle FramebufferVk::getNonRotatedCompleteRenderArea() const
{
    const gl::Box &dimensions = mState.getDimensions();
    return gl::Rectangle(0, 0, dimensions.width, dimensions.height);
}

// Return the framebuffer's rotated render area.  This is a gl::Rectangle that is based on the
// dimensions of the framebuffer, IS ROTATED for the draw FBO, and IS NOT y-flipped
//
// Note: Since the rectangle is not scissored (i.e. x and y are guaranteed to be zero), only the
// width and height must be swapped if the rotation is 90 or 270 degrees.
gl::Rectangle FramebufferVk::getRotatedCompleteRenderArea(ContextVk *contextVk) const
{
    gl::Rectangle renderArea = getNonRotatedCompleteRenderArea();
    if (contextVk->isRotatedAspectRatioForDrawFBO())
    {
        // The surface is rotated 90/270 degrees.  This changes the aspect ratio of the surface.
        std::swap(renderArea.width, renderArea.height);
    }
    return renderArea;
}

// Return the framebuffer's scissored and rotated render area.  This is a gl::Rectangle that is
// based on the dimensions of the framebuffer, is clipped to the scissor, IS ROTATED and IS
// Y-FLIPPED for the draw FBO.
//
// Note: Since the rectangle is scissored, it must be fully rotated, and not just have the width
// and height swapped.
gl::Rectangle FramebufferVk::getRotatedScissoredRenderArea(ContextVk *contextVk) const
{
    const gl::Rectangle renderArea = getNonRotatedCompleteRenderArea();
    bool invertViewport            = contextVk->isViewportFlipEnabledForDrawFBO();
    gl::Rectangle scissoredArea    = ClipRectToScissor(contextVk->getState(), renderArea, false);
    gl::Rectangle rotatedScissoredArea;
    RotateRectangle(contextVk->getRotationDrawFramebuffer(), invertViewport, renderArea.width,
                    renderArea.height, scissoredArea, &rotatedScissoredArea);
    return rotatedScissoredArea;
}

GLint FramebufferVk::getSamples() const
{
    const gl::FramebufferAttachment *lastAttachment = nullptr;

    for (size_t colorIndexGL : mState.getEnabledDrawBuffers() & mState.getColorAttachmentsMask())
    {
        const gl::FramebufferAttachment *color = mState.getColorAttachment(colorIndexGL);
        ASSERT(color);

        if (color->isRenderToTexture())
        {
            return color->getSamples();
        }

        lastAttachment = color;
    }
    const gl::FramebufferAttachment *depthStencil = mState.getDepthOrStencilAttachment();
    if (depthStencil)
    {
        if (depthStencil->isRenderToTexture())
        {
            return depthStencil->getSamples();
        }
        lastAttachment = depthStencil;
    }

    // If none of the attachments are multisampled-render-to-texture, take the sample count from the
    // last attachment (any would have worked, as they would all have the same sample count).
    return std::max(lastAttachment ? lastAttachment->getSamples() : 1, 1);
}

angle::Result FramebufferVk::flushDeferredClears(ContextVk *contextVk)
{
    if (mDeferredClears.empty())
    {
        return angle::Result::Continue;
    }

    return contextVk->startRenderPass(getRotatedCompleteRenderArea(contextVk), nullptr, nullptr);
}

void FramebufferVk::updateRenderPassReadOnlyDepthMode(ContextVk *contextVk,
                                                      vk::RenderPassCommandBufferHelper *renderPass)
{
    bool readOnlyDepthStencilMode =
        getDepthStencilRenderTarget() && !getDepthStencilRenderTarget()->hasResolveAttachment() &&
        (mReadOnlyDepthFeedbackLoopMode || !renderPass->hasDepthStencilWriteOrClear());

    // If readOnlyDepthStencil is false, we are switching out of read only mode due to depth write.
    // We must not be in the read only feedback loop mode because the logic in
    // DIRTY_BIT_READ_ONLY_DEPTH_FEEDBACK_LOOP_MODE should ensure we end the previous renderpass and
    // a new renderpass will start with feedback loop disabled.
    ASSERT(readOnlyDepthStencilMode || !mReadOnlyDepthFeedbackLoopMode);

    renderPass->updateStartedRenderPassWithDepthMode(readOnlyDepthStencilMode);
}

void FramebufferVk::switchToFramebufferFetchMode(ContextVk *contextVk, bool hasFramebufferFetch)
{
    // The switch happens once, and is permanent.
    if (mCurrentFramebufferDesc.hasFramebufferFetch() == hasFramebufferFetch)
    {
        return;
    }

    // Make sure framebuffer is recreated.
    releaseCurrentFramebuffer(contextVk);
    mCurrentFramebufferDesc.setFramebufferFetchMode(hasFramebufferFetch);

    mRenderPassDesc.setFramebufferFetchMode(hasFramebufferFetch);
    contextVk->onDrawFramebufferRenderPassDescChange(this, nullptr);

    // Clear the framebuffer cache, as none of the old framebuffers are usable.
    if (contextVk->getFeatures().permanentlySwitchToFramebufferFetchMode.enabled)
    {
        ASSERT(hasFramebufferFetch);
        releaseCurrentFramebuffer(contextVk);
    }
}
}  // namespace rx
