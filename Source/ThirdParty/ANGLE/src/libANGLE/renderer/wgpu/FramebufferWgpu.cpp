//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferWgpu.cpp:
//    Implements the class methods for FramebufferWgpu.
//

#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"
#include "common/unsafe_buffers.h"

#include <__config>
#include <limits>

#include "common/Color.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"
#include "webgpu/webgpu.h"

namespace rx
{

namespace
{

using DoubleRectangle = gl::RectangleImpl<double>;

int64_t FloorDivide(int64_t numerator, int64_t denominator)
{
    ASSERT(denominator > 0);
    const int64_t quotient = numerator / denominator;
    return quotient * denominator > numerator ? quotient - 1 : quotient;
}

int64_t CeilDivide(int64_t numerator, int64_t denominator)
{
    return -FloorDivide(-numerator, denominator);
}

// Computes, in one axis, the GL window coordinates [*beginOut, *endOut) of the pixels that a blit
// writes: the pixels in the destination rectangle and the draw framebuffer whose sample point,
// srcStart + (g + 0.5 - dstStart) * srcExtent / dstExtent, is inside the read framebuffer, as
// WebGL 2.0 requires. Returns false if the computation overflows, which only rectangles with
// coordinates near the limits of GLint can make it do.
bool GetBlitWrittenRange(int srcStart,
                         int srcExtent,
                         int srcSize,
                         int dstStart,
                         int dstExtent,
                         int dstSize,
                         int *beginOut,
                         int *endOut)
{
    ASSERT(srcExtent != 0 && dstExtent != 0);

    // The sample point of pixel g is (a + g * b) / denominator, which is inside the read
    // framebuffer when 0 <= a + g * b < limit.
    angle::CheckedNumeric<int64_t> a =
        angle::CheckedNumeric<int64_t>(2) * srcStart * dstExtent +
        (angle::CheckedNumeric<int64_t>(1) - angle::CheckedNumeric<int64_t>(2) * dstStart) *
            srcExtent;
    angle::CheckedNumeric<int64_t> b           = angle::CheckedNumeric<int64_t>(2) * srcExtent;
    angle::CheckedNumeric<int64_t> denominator = angle::CheckedNumeric<int64_t>(2) * dstExtent;
    if (dstExtent < 0)
    {
        a           = -a;
        b           = -b;
        denominator = -denominator;
    }
    const angle::CheckedNumeric<int64_t> limit = denominator * srcSize;
    int64_t aValue, bValue, limitValue;
    if (!a.AssignIfValid(&aValue) || !b.AssignIfValid(&bValue) ||
        !limit.AssignIfValid(&limitValue) || !(limit - a).IsValid() || !(a - limit).IsValid())
    {
        return false;
    }

    int64_t begin = std::max<int64_t>(std::min(dstStart, dstStart + dstExtent), 0);
    int64_t end   = std::min<int64_t>(std::max(dstStart, dstStart + dstExtent), dstSize);
    if (bValue > 0)
    {
        begin = std::max(begin, CeilDivide(-aValue, bValue));
        end   = std::min(end, CeilDivide(limitValue - aValue, bValue));
    }
    else
    {
        begin = std::max(begin, FloorDivide(aValue - limitValue, -bValue) + 1);
        end   = std::min(end, FloorDivide(aValue, -bValue) + 1);
    }
    *beginOut = static_cast<int>(begin);
    *endOut   = static_cast<int>(std::max(begin, end));
    return true;
}

int32_t SaturateToInt32(int64_t value)
{
    return static_cast<int32_t>(std::clamp<int64_t>(value, std::numeric_limits<int32_t>::min(),
                                                    std::numeric_limits<int32_t>::max()));
}

// GL_NEAREST takes, for the destination pixel center g + 0.5 in GL window coordinates, the source
// texel floor(srcStart + (g + 0.5 - dstStart) * srcExtent / dstExtent), where the starts and the
// signed extents are those of the blit call. Computes that in integers, in one axis, for the
// destination texels from dstOrigin on. Texel j of a flipped subresource is GL's size - 1 - j.
webgpu::UtilsWgpu::BlitParams::NearestAxis GetNearestAxis(int srcStart,
                                                          int srcExtent,
                                                          int dstStart,
                                                          int dstExtent,
                                                          bool srcFlip,
                                                          int srcSize,
                                                          bool dstFlip,
                                                          int dstSize,
                                                          int dstOrigin)
{
    webgpu::UtilsWgpu::BlitParams::NearestAxis axis;
    axis.dstOrigin = dstOrigin;
    axis.srcBase   = srcFlip ? srcSize - 1 : 0;
    axis.srcSign   = srcFlip ? -1 : 1;

    // The source coordinate of GL pixel g is numerator(g) / denominator. The GL pixel of
    // destination texel dstOrigin + m is g0 + m * gStep.
    const int64_t g0    = dstFlip ? dstSize - 1 - int64_t(dstOrigin) : dstOrigin;
    const int64_t gStep = dstFlip ? -1 : 1;
    int64_t denominator = 2 * int64_t(dstExtent);
    int64_t numerator0  = 2 * int64_t(srcStart) * dstExtent +
                         (2 * g0 + 1 - 2 * int64_t(dstStart)) * int64_t(srcExtent);
    int64_t step = 2 * gStep * int64_t(srcExtent);
    if (denominator < 0)
    {
        denominator = -denominator;
        numerator0  = -numerator0;
        step        = -step;
    }

    const int64_t q0 = FloorDivide(numerator0, denominator);
    const int64_t a  = FloorDivide(step, denominator);
    int64_t r0       = numerator0 - q0 * denominator;
    int64_t b        = step - a * denominator;

    // Keep r0 + m * b in 32 bits for any destination size. The mapping is approximate, off by up to
    // about a texel, only for destination extents of more than 32768 pixels, more than twice the
    // largest framebuffer.
    constexpr int64_t kMaxDenominator = int64_t(1) << 16;
    if (denominator > kMaxDenominator)
    {
        const int64_t divisor = (denominator + kMaxDenominator - 1) / kMaxDenominator;
        r0 /= divisor;
        b /= divisor;
        denominator = (denominator + divisor - 1) / divisor;
    }

    axis.q0          = SaturateToInt32(q0);
    axis.a           = SaturateToInt32(a);
    axis.r0          = static_cast<int32_t>(r0);
    axis.b           = static_cast<int32_t>(b);
    axis.denominator = static_cast<int32_t>(denominator);
    return axis;
}

// Converts a blit from GL window coordinates to texels of the source and destination
// subresources, which have row 0 first. A framebuffer with flipY has GL's row 0 last.
webgpu::UtilsWgpu::BlitParams GetBlitParams(const gl::Rectangle &dstArea,
                                            const gl::Rectangle &dstScissor,
                                            const DoubleRectangle &srcArea,
                                            const gl::Rectangle &callSrcArea,
                                            const gl::Rectangle &callDstArea,
                                            bool srcFlipY,
                                            const WGPUExtent3D &srcSize,
                                            bool dstFlipY,
                                            const WGPUExtent3D &dstSize,
                                            GLenum filter)
{
    webgpu::UtilsWgpu::BlitParams params;
    params.dstArea    = dstArea;
    params.dstScissor = dstScissor;
    params.dstSize    = dstSize;
    params.filter     = filter;

    double srcY0 = srcArea.y0();
    double srcY1 = srcArea.y1();
    if (srcFlipY)
    {
        srcY0 = srcSize.height - srcY0;
        srcY1 = srcSize.height - srcY1;
    }
    if (dstFlipY)
    {
        const int dstHeight = static_cast<int>(dstSize.height);
        params.dstArea.y    = dstHeight - dstArea.y1();
        params.dstScissor.y = dstHeight - dstScissor.y1();
        // The first destination row in texels is the last one in GL.
        std::swap(srcY0, srcY1);
    }
    params.srcArea = gl::RectangleImpl<float>(
        static_cast<float>(srcArea.x), static_cast<float>(srcY0), static_cast<float>(srcArea.width),
        static_cast<float>(srcY1 - srcY0));

    params.nearestX =
        GetNearestAxis(callSrcArea.x, callSrcArea.width, callDstArea.x, callDstArea.width, false,
                       srcSize.width, false, dstSize.width, params.dstArea.x);
    params.nearestY =
        GetNearestAxis(callSrcArea.y, callSrcArea.height, callDstArea.y, callDstArea.height,
                       srcFlipY, srcSize.height, dstFlipY, dstSize.height, params.dstArea.y);
    return params;
}

// Whether a blit maps each destination texel to the source texel at the same offset, so that it
// can be a copy.
bool IsBlitUnscaledAndUnflipped(const webgpu::UtilsWgpu::BlitParams &params)
{
    return params.srcArea.width == params.dstArea.width &&
           params.srcArea.height == params.dstArea.height &&
           params.srcArea.x == std::floor(params.srcArea.x) &&
           params.srcArea.y == std::floor(params.srcArea.y);
}

}  // namespace

FramebufferWgpu::FramebufferWgpu(const gl::FramebufferState &state) : FramebufferImpl(state)
{
    mCurrentColorAttachmentFormats.fill(WGPUTextureFormat_Undefined);
}

FramebufferWgpu::~FramebufferWgpu() {}

angle::Result FramebufferWgpu::discard(const gl::Context *context,
                                       size_t count,
                                       const GLenum *attachments)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::invalidate(const gl::Context *context,
                                          size_t count,
                                          const GLenum *attachments)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::invalidateSub(const gl::Context *context,
                                             size_t count,
                                             const GLenum *attachments,
                                             const gl::Rectangle &area)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clear(const gl::Context *context, GLbitfield mask)
{
    const bool clearColor   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    const bool clearDepth   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    const bool clearStencil = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));

    gl::ColorF clearColorValue = context->getState().getColorClearValue();
    gl::DrawBufferMask clearColorBuffers =
        clearColor ? mState.getEnabledDrawBuffers() : gl::DrawBufferMask();

    uint32_t clearStencilValue = static_cast<uint32_t>(context->getState().getStencilClearValue());

    float clearDepthValue = context->getState().getDepthClearValue();

    return clearImpl(context, clearColorBuffers, clearDepth, clearStencil, clearColorValue,
                     clearDepthValue, clearStencilValue);
}

gl::ColorF FramebufferWgpu::getClearColorWithCorrectAlpha(const gl::ColorF &clearValue,
                                                          size_t drawBufferIndex)
{
    webgpu::ImageHelper *colorImage = mRenderTargetCache.getColors()[drawBufferIndex]->getImage();
    const angle::Format &dstIntendedFormat = angle::Format::Get(colorImage->getIntendedFormatID());
    const angle::Format &dstActualFormat   = angle::Format::Get(colorImage->getActualFormatID());
    // If the intended format does not have alpha bits, but the texture is backed by an actual
    // format with alpha bits, ensure the alpha bits are cleared to 1.0.
    if (dstIntendedFormat.alphaBits == 0 && dstActualFormat.alphaBits != 0)
    {
        return gl::ColorF(clearValue.red, clearValue.green, clearValue.blue, 1.0);
    }

    return clearValue;
}

angle::Result FramebufferWgpu::clearImpl(const gl::Context *context,
                                         gl::DrawBufferMask clearColorBuffers,
                                         bool clearDepth,
                                         bool clearStencil,
                                         const gl::ColorF &clearColorValue,
                                         float clearDepthValue,
                                         uint32_t clearStencilValue)
{
    const bool clearColor = clearColorBuffers.any();

    ASSERT(clearDepth || clearStencil || clearColor);

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    // This function assumes that only enabled attachments are asked to be cleared.
    ASSERT((clearColorBuffers & mState.getEnabledDrawBuffers()) == clearColorBuffers);
    ASSERT(!clearDepth || mState.getDepthAttachment() != nullptr);
    ASSERT(!clearStencil || mState.getStencilAttachment() != nullptr);

    // The front-end should ensure we don't attempt to clear color if all channels are masked.
    ASSERT(!clearColor || context->getState().getBlendStateExt().getColorMaskBits() != 0);
    // The front-end should ensure we don't attempt to clear depth if depth write is disabled.
    ASSERT(!clearDepth || context->getState().getDepthStencilState().depthMask);
    // The front-end should ensure we don't attempt to clear stencil if all bits are masked.
    ASSERT(!clearStencil ||
           static_cast<uint8_t>(context->getState().getDepthStencilState().stencilWritemask) != 0);

    gl::Rectangle renderArea(0, 0, mState.getDimensions().width, mState.getDimensions().height);
    gl::Rectangle scissoredRenderArea = ClipRectToScissor(context->getState(), renderArea, false);
    if (scissoredRenderArea.empty())
    {
        return angle::Result::Continue;
    }
    const bool scissoredClear = scissoredRenderArea != renderArea;
    // TODO(anglebug.com/474131922): could avoid a clearWithDraw if a masked out channel is not
    // present in the `internalFormat` that's being cleared. Vulkan does this.
    const bool maskedClearColor =
        clearColor && (context->getState().getBlendStateExt().getColorMaskBits() !=
                       context->getState().getBlendStateExt().getAllColorMaskBits());
    GLuint allStencilBits =
        angle::BitMask<GLuint>(context->getState().getDrawFramebuffer()->getStencilBitCount());
    const bool maskedClearStencil =
        clearStencil && ((context->getState().getDepthStencilState().stencilWritemask &
                          allStencilBits) != allStencilBits);
    const bool clearWithDraw = scissoredClear || maskedClearColor || maskedClearStencil;

    if (clearWithDraw)
    {
        // Flush any deferred clears so that they do not overwrite this clearWithDraw.
        // TODO(anglebug.com/474131922): in the future this should just start a render pass for the
        // draw call to be added to.
        ANGLE_TRY(flushDeferredClears(contextWgpu));

        // If a scissor, need to flip the clearArea if this framebuffer is flipped.
        if (mFlipY)
        {
            scissoredRenderArea.y =
                mState.getDimensions().height - scissoredRenderArea.y - scissoredRenderArea.height;
        }

        webgpu::UtilsWgpu::ClearParams clearParams{
            .clearArea         = scissoredRenderArea,
            .colorMasks        = context->getState().getBlendStateExt().getColorMaskBits(),
            .clearColorBuffers = clearColor ? clearColorBuffers : gl::DrawBufferMask(),
            // RGB textures backed by the RGBA format will have their alpha cleared to 1.0 by the
            // draw.
            .clearColorValue =
                clearColor ? std::optional<gl::ColorF>(clearColorValue) : std::nullopt,
            .clearDepthValue = clearDepth ? std::optional<float>(clearDepthValue) : std::nullopt,
            .clearStencilValue =
                clearStencil ? std::optional<uint32_t>(clearStencilValue) : std::nullopt,
            .stencilWriteMask =
                clearStencil ? std::optional<uint32_t>(
                                   context->getState().getDepthStencilState().stencilWritemask)
                             : std::nullopt,
            .colorTargets = clearColor ? &mRenderTargetCache.getColors() : nullptr,
            .depthStencilTarget =
                clearDepth || clearStencil ? mRenderTargetCache.getDepthStencil() : nullptr,
        };

        return contextWgpu->getUtils()->clear(contextWgpu, std::move(clearParams));
    }

    webgpu::PackedRenderPassDescriptor clearRenderPassDesc;

    for (size_t enabledDrawBuffer : clearColorBuffers)
    {
        clearRenderPassDesc.colorAttachments.push_back(webgpu::CreateNewClearColorAttachment(
            getClearColorWithCorrectAlpha(clearColorValue, enabledDrawBuffer),
            WGPU_DEPTH_SLICE_UNDEFINED,
            mRenderTargetCache.getColorDraw(mState, enabledDrawBuffer)->getTextureView()));
    }

    if (clearDepth || clearStencil)
    {
        clearRenderPassDesc.depthStencilAttachment = webgpu::CreateNewClearDepthStencilAttachment(
            clearDepthValue, clearStencilValue,
            mRenderTargetCache.getDepthStencil()->getTextureView(), clearDepth, clearStencil);
    }

    // Attempt to end a render pass if one has already been started.
    bool isActiveRenderPass =
        mCurrentRenderPassDesc != clearRenderPassDesc || contextWgpu->hasActiveRenderPass();

    if (mDeferredClears.any())
    {
        // Merge the current clear command with any deferred clears. This is to keep the clear paths
        // simpler so they only need to consider the current or the deferred clears.
        mergeClearWithDeferredClears(clearColorValue, clearColorBuffers, clearDepthValue,
                                     clearStencilValue, clearColor, clearDepth, clearStencil);
        if (isActiveRenderPass)
        {
            ANGLE_TRY(flushDeferredClears(contextWgpu));
        }
        else
        {
            for (size_t colorIndexGL : mDeferredClears.getColorMask())
            {
                RenderTargetWgpu *renderTarget =
                    mRenderTargetCache.getColorDraw(mState, colorIndexGL);
                webgpu::ClearValues deferredClearValue;
                deferredClearValue = mDeferredClears[colorIndexGL];
                renderTarget->getImage()->stageClear(renderTarget->getGlLevel(), deferredClearValue,
                                                     false, false);
            }
            if (mDeferredClears.hasDepth() || mDeferredClears.hasStencil())
            {
                webgpu::ClearValues dsClearValue = {};
                dsClearValue.depthValue          = mDeferredClears.getDepthValue();
                dsClearValue.stencilValue        = mDeferredClears.getStencilValue();
                RenderTargetWgpu *renderTarget   = mRenderTargetCache.getDepthStencil();
                renderTarget->getImage()->stageClear(renderTarget->getGlLevel(), dsClearValue,
                                                     mDeferredClears.hasDepth(),
                                                     mDeferredClears.hasStencil());
            }
            mDeferredClears.reset();
        }
        return angle::Result::Continue;
    }

    if (isActiveRenderPass)
    {
        ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));
    }

    mCurrentRenderPassDesc = std::move(clearRenderPassDesc);
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clearBufferfv(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             const GLfloat *values)
{
    bool clearDepth       = false;
    float clearDepthValue = 0.0;

    gl::DrawBufferMask clearColorBuffers;
    gl::ColorF clearColorValue;

    if (buffer == GL_DEPTH)
    {
        clearDepth      = true;
        clearDepthValue = values[0];
    }
    else
    {
        clearColorBuffers.set(drawbuffer);
        clearColorValue = ANGLE_UNSAFE_TODO(gl::ColorF(values[0], values[1], values[2], values[3]));
    }

    return clearImpl(context, clearColorBuffers, clearDepth, /*clearStencil=*/false,
                     clearColorValue, clearDepthValue, /*clearStencilValue=*/0);
}

angle::Result FramebufferWgpu::clearBufferuiv(const gl::Context *context,
                                              GLenum buffer,
                                              GLint drawbuffer,
                                              const GLuint *values)
{
    gl::DrawBufferMask clearColorBuffers;
    gl::ColorF clearColorValue;

    clearColorBuffers.set(drawbuffer);
    clearColorValue =
        gl::ColorF(gl::bitCast<float>(values[0]), gl::bitCast<float>(ANGLE_UNSAFE_TODO(values[1])),
                   gl::bitCast<float>(ANGLE_UNSAFE_TODO(values[2])),
                   gl::bitCast<float>(ANGLE_UNSAFE_TODO(values[3])));

    return clearImpl(context, clearColorBuffers, /*clearDepth=*/false, /*clearStencil=*/false,
                     clearColorValue, /*clearDepthValue=*/0.0, /*clearStencilValue=*/0);
}

angle::Result FramebufferWgpu::clearBufferiv(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             const GLint *values)
{
    bool clearStencil          = false;
    uint32_t clearStencilValue = 0;

    gl::DrawBufferMask clearColorBuffers;
    gl::ColorF clearColorValue;

    if (buffer == GL_STENCIL)
    {
        clearStencil      = true;
        clearStencilValue = static_cast<uint32_t>(values[0]);
    }
    else
    {
        clearColorBuffers.set(drawbuffer);
        clearColorValue = gl::ColorF(gl::bitCast<float>(values[0]),
                                     gl::bitCast<float>(ANGLE_UNSAFE_TODO(values[1])),
                                     gl::bitCast<float>(ANGLE_UNSAFE_TODO(values[2])),
                                     gl::bitCast<float>(ANGLE_UNSAFE_TODO(values[3])));
    }

    return clearImpl(context, clearColorBuffers, /*clearDepth=*/false, clearStencil,
                     clearColorValue, /*clearDepthValue=*/0.0, clearStencilValue);
}

angle::Result FramebufferWgpu::clearBufferfi(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             GLfloat depth,
                                             GLint stencil)
{
    return clearImpl(context, gl::DrawBufferMask(), /*clearDepth=*/true, /*clearStencil=*/true,
                     gl::ColorF(), depth, static_cast<uint32_t>(stencil));
}

angle::Result FramebufferWgpu::readPixels(const gl::Context *context,
                                          const gl::Rectangle &origArea,
                                          GLenum format,
                                          GLenum type,
                                          const gl::PixelPackState &pack,
                                          gl::Buffer *packBuffer,
                                          void *ptrOrOffset)
{
    // Get the pointer to write to from the argument or the pack buffer
    GLubyte *pixels = nullptr;
    if (packBuffer != nullptr)
    {
        UNREACHABLE();
    }
    else
    {
        pixels = reinterpret_cast<GLubyte *>(ptrOrOffset);
    }

    // Clip read area to framebuffer.
    const gl::Extents fbSize = getState().getReadPixelsAttachment(format)->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    gl::Rectangle clippedArea;
    if (!ClipRectangle(origArea, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }
    gl::Rectangle flippedArea = getReadArea(context, clippedArea);

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    ANGLE_TRY(flushDeferredClears(contextWgpu));

    GLuint outputSkipBytes = 0;
    PackPixelsParams params;
    ANGLE_TRY(webgpu::ImageHelper::getReadPixelsParams(contextWgpu, pack, packBuffer, format, type,
                                                       origArea, clippedArea, &params,
                                                       &outputSkipBytes));

    if (mFlipY)
    {
        params.reverseRowOrder = !params.reverseRowOrder;
    }

    // Does not handle reading from depth/stencil buffer(s).
    ASSERT(format != GL_DEPTH_COMPONENT && format != GL_STENCIL_INDEX);

    RenderTargetWgpu *readRT = getReadPixelsRenderTarget();
    uint32_t layer           = readRT->getLayer();

    webgpu::ImageHelper *sourceImageHelper = readRT->getImage();
    ANGLE_UNSAFE_TODO(ANGLE_TRY(
        sourceImageHelper->readPixels(contextWgpu, flippedArea, params, readRT->getLevelIndex(),
                                      layer, static_cast<uint8_t *>(pixels) + outputSkipBytes)));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::blit(const gl::Context *context,
                                    const gl::Rectangle &sourceArea,
                                    const gl::Rectangle &destArea,
                                    GLbitfield mask,
                                    GLenum filter)
{
    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);
    bool blitColor           = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    bool blitDepth           = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    bool blitStencil         = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));

    const gl::State &glState               = context->getState();
    const gl::Framebuffer *readFramebuffer = glState.getReadFramebuffer();
    FramebufferWgpu *readFBOWgpu           = GetImplAs<FramebufferWgpu>(readFramebuffer);
    bool srcFlipY                          = readFBOWgpu->flipY();

    // Flush any deferred clears on the read and draw framebuffers.
    ANGLE_TRY(readFBOWgpu->flushDeferredClears(contextWgpu));
    ANGLE_TRY(flushDeferredClears(contextWgpu));

    if (sourceArea.width == 0 || sourceArea.height == 0 || destArea.width == 0 ||
        destArea.height == 0)
    {
        return angle::Result::Continue;
    }

    // Destination pixels whose source pixel is outside the read framebuffer are not written, as
    // WebGL 2.0 requires. Find the written pixels exactly, and the source rectangle that maps to
    // them, which is reversed where the blit flips, so that the destination isn't.
    const gl::Extents srcExtents = readFramebuffer->getState().getExtents();
    const gl::Extents dstExtents = mState.getExtents();
    int dstX0, dstX1, dstY0, dstY1;
    if (!GetBlitWrittenRange(sourceArea.x, sourceArea.width, srcExtents.width, destArea.x,
                             destArea.width, dstExtents.width, &dstX0, &dstX1) ||
        !GetBlitWrittenRange(sourceArea.y, sourceArea.height, srcExtents.height, destArea.y,
                             destArea.height, dstExtents.height, &dstY0, &dstY1) ||
        dstX0 == dstX1 || dstY0 == dstY1)
    {
        return angle::Result::Continue;
    }
    auto sourceEdge = [](int dstEdge, int srcStart, int srcExtent, int dstStart, int dstExtent) {
        return srcStart + (static_cast<double>(dstEdge) - dstStart) * srcExtent / dstExtent;
    };
    const double srcX0 =
        sourceEdge(dstX0, sourceArea.x, sourceArea.width, destArea.x, destArea.width);
    const double srcX1 =
        sourceEdge(dstX1, sourceArea.x, sourceArea.width, destArea.x, destArea.width);
    const double srcY0 =
        sourceEdge(dstY0, sourceArea.y, sourceArea.height, destArea.y, destArea.height);
    const double srcY1 =
        sourceEdge(dstY1, sourceArea.y, sourceArea.height, destArea.y, destArea.height);

    BlitGeometry geometry;
    geometry.callSrcArea = sourceArea;
    geometry.callDstArea = destArea;
    geometry.dstArea     = gl::Rectangle(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);
    geometry.srcArea     = DoubleRectangle(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);

    const gl::Rectangle &scissor =
        glState.isScissorTestEnabled() ? glState.getScissor() : geometry.dstArea;
    if (!gl::ClipRectangle(geometry.dstArea, scissor, &geometry.dstScissor))
    {
        return angle::Result::Continue;
    }

    if (blitColor)
    {
        RenderTargetWgpu *readRenderTarget = readFBOWgpu->getReadPixelsRenderTarget();
        ASSERT(readRenderTarget);

        const gl::DrawBufferMask &drawBufferMask = mState.getEnabledDrawBuffers();
        for (size_t drawBufferIdx : drawBufferMask)
        {
            RenderTargetWgpu *drawRenderTarget =
                mRenderTargetCache.getColorDraw(mState, drawBufferIdx);
            ASSERT(drawRenderTarget);

            ANGLE_TRY(blitColorAttachment(context, geometry, readRenderTarget, srcFlipY,
                                          drawRenderTarget, filter));
        }
    }

    if (blitDepth || blitStencil)
    {
        RenderTargetWgpu *readRT = readFBOWgpu->mRenderTargetCache.getDepthStencil();
        RenderTargetWgpu *drawRT = mRenderTargetCache.getDepthStencil();

        if (readRT && drawRT)
        {
            WGPUTextureAspect aspect = WGPUTextureAspect_All;
            if (blitDepth && blitStencil)
            {
                aspect = WGPUTextureAspect_All;
            }
            else if (blitDepth)
            {
                aspect = WGPUTextureAspect_DepthOnly;
            }
            else if (blitStencil)
            {
                aspect = WGPUTextureAspect_StencilOnly;
            }

            ANGLE_TRY(
                blitDepthStencilAttachment(context, geometry, readRT, srcFlipY, drawRT, aspect));
        }
    }

    return angle::Result::Continue;
}

gl::FramebufferStatus FramebufferWgpu::checkStatus(const gl::Context *context) const
{
    return gl::FramebufferStatus::Complete();
}

angle::Result FramebufferWgpu::syncState(const gl::Context *context,
                                         GLenum binding,
                                         const gl::Framebuffer::DirtyBits &dirtyBits,
                                         gl::Command command)
{
    ContextWgpu *contextWgpu         = webgpu::GetImpl(context);
    bool dirtyDepthStencilAttachment = false;
    ASSERT(dirtyBits.any());

    gl::DrawBufferMask dirtyColorAttachments;
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_DEPTH_BUFFER_CONTENTS:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_BUFFER_CONTENTS:
            {
                ANGLE_TRY(mRenderTargetCache.updateDepthStencilRenderTarget(context, mState));
                dirtyDepthStencilAttachment = true;
                // Update the current depth stencil texture format let the context know if this
                // framebuffer is bound for draw
                RenderTargetWgpu *rt       = mRenderTargetCache.getDepthStencil();
                mCurrentDepthStencilFormat = (rt && rt->getImage())
                                                 ? rt->getImage()->toWgpuTextureFormat()
                                                 : WGPUTextureFormat_Undefined;
                if (binding == GL_DRAW_FRAMEBUFFER)
                {
                    contextWgpu->setDepthStencilFormat(mCurrentDepthStencilFormat);
                }

                break;
            }

            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
                ANGLE_TRY(mRenderTargetCache.update(context, mState, dirtyBits));
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
                // The context needs to set the render pipeline to mask out the disabled draw
                // buffers, or reset the color masks of the now-enabled draw buffers.
                contextWgpu->updatePipelineColorMasks();
                break;
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            case gl::Framebuffer::DIRTY_BIT_FRAMEBUFFER_SRGB_WRITE_CONTROL_MODE:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_LAYERS:
            case gl::Framebuffer::DIRTY_BIT_FOVEATION:
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

                bool readColorTargetUpdated = false;
                ANGLE_TRY(mRenderTargetCache.updateColorRenderTarget(context, mState, colorIndexGL,
                                                                     &readColorTargetUpdated));

                // Window system framebuffer only have one color attachment and its property should
                // never change unless via DIRTY_BIT_DRAW_BUFFERS bit.
                if (!mState.isDefault())
                {
                    contextWgpu->updatePipelineColorMasks();
                }

                // Update the current color texture formats let the context know if this framebuffer
                // is bound for draw
                RenderTargetWgpu *rt = mRenderTargetCache.getColorDraw(mState, colorIndexGL);
                mCurrentColorAttachmentFormats[colorIndexGL] =
                    (rt && rt->getImage()) ? rt->getImage()->toWgpuTextureFormat()
                                           : WGPUTextureFormat_Undefined;
                if (binding == GL_DRAW_FRAMEBUFFER)
                {
                    contextWgpu->setColorAttachmentFormat(
                        colorIndexGL, mCurrentColorAttachmentFormats[colorIndexGL]);
                }

                dirtyColorAttachments.set(colorIndexGL);
                break;
            }
        }
    }

    // Like in Vulkan, defer clears for draw framebuffer ops as well as clears to read framebuffer
    // attachments that are not taking part in a blit operation.
    ASSERT(mDeferredClears.empty());

    const bool isBlitCommand     = command >= gl::Command::Blit && command <= gl::Command::BlitAll;
    bool deferColorClears        = binding == GL_DRAW_FRAMEBUFFER;
    bool deferDepthStencilClears = binding == GL_DRAW_FRAMEBUFFER;
    if (binding == GL_READ_FRAMEBUFFER && isBlitCommand)
    {
        uint32_t blitMask =
            static_cast<uint32_t>(command) - static_cast<uint32_t>(gl::Command::Blit);
        if ((blitMask & gl::CommandBlitBufferColor) == 0)
        {
            deferColorClears = true;
        }
        if ((blitMask & (gl::CommandBlitBufferDepth | gl::CommandBlitBufferStencil)) == 0)
        {
            deferDepthStencilClears = true;
        }
    }

    ANGLE_TRY(flushAttachmentUpdates(context, dirtyColorAttachments, dirtyDepthStencilAttachment,
                                     deferColorClears, deferDepthStencilClears));

    // Notify the ContextWgpu to update the pipeline desc or restart the renderpass
    ANGLE_TRY(contextWgpu->onFramebufferChange(this, command));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::getSamplePosition(const gl::Context *context,
                                                 size_t index,
                                                 GLfloat *xy) const
{
    return angle::Result::Continue;
}

RenderTargetWgpu *FramebufferWgpu::getReadPixelsRenderTarget() const
{
    return mRenderTargetCache.getColorRead(mState);
}

void FramebufferWgpu::addNewColorAttachments(
    std::vector<webgpu::PackedRenderPassColorAttachment> newColorAttachments)
{
    if (!mNewRenderPassDesc.has_value())
    {
        mNewRenderPassDesc = webgpu::PackedRenderPassDescriptor();
    }

    for (webgpu::PackedRenderPassColorAttachment &colorAttachment : newColorAttachments)
    {
        mNewRenderPassDesc->colorAttachments.push_back(colorAttachment);
    }
}

void FramebufferWgpu::updateDepthStencilAttachment(
    webgpu::PackedRenderPassDepthStencilAttachment newRenderPassDepthStencilAttachment)
{
    if (!mNewRenderPassDesc.has_value())
    {
        mNewRenderPassDesc = webgpu::PackedRenderPassDescriptor();
    }
    mNewRenderPassDesc->depthStencilAttachment = std::move(newRenderPassDepthStencilAttachment);
}

angle::Result FramebufferWgpu::flushOneColorAttachmentUpdate(const gl::Context *context,
                                                             bool deferClears,
                                                             uint32_t colorIndexGL)
{
    ContextWgpu *contextWgpu           = GetImplAs<ContextWgpu>(context);
    RenderTargetWgpu *drawRenderTarget = nullptr;
    RenderTargetWgpu *readRenderTarget = nullptr;

    drawRenderTarget = mRenderTargetCache.getColorDraw(mState, colorIndexGL);
    if (drawRenderTarget)
    {
        if (deferClears)
        {
            ANGLE_TRY(drawRenderTarget->flushImageStagedUpdates(contextWgpu, &mDeferredClears,
                                                                colorIndexGL));
        }
        else
        {
            ANGLE_TRY(drawRenderTarget->flushImageStagedUpdates(contextWgpu, nullptr, 0));
        }
    }

    if (mState.getReadBufferState() != GL_NONE && mState.getReadIndex() == colorIndexGL)
    {
        readRenderTarget = mRenderTargetCache.getColorRead(mState);
        if (readRenderTarget && readRenderTarget != drawRenderTarget)
        {
            ANGLE_TRY(readRenderTarget->flushImageStagedUpdates(contextWgpu, nullptr, 0));
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::flushAttachmentUpdates(const gl::Context *context,
                                                      gl::DrawBufferMask dirtyColorAttachments,
                                                      bool dirtyDepthStencilAttachment,
                                                      bool deferColorClears,
                                                      bool deferDepthStencilClears)
{
    for (size_t colorIndexGL : dirtyColorAttachments)
    {
        ANGLE_TRY(flushOneColorAttachmentUpdate(context, deferColorClears,
                                                static_cast<uint32_t>(colorIndexGL)));
    }

    ContextWgpu *contextWgpu         = GetImplAs<ContextWgpu>(context);
    RenderTargetWgpu *depthStencilRt = mRenderTargetCache.getDepthStencil();

    if (depthStencilRt && dirtyDepthStencilAttachment)
    {
        if (deferDepthStencilClears)
        {
            // The underlying ImageHelper will check if a clear has a stencil value and store the
            // deferred clear in the correct index.
            ANGLE_TRY(depthStencilRt->flushImageStagedUpdates(contextWgpu, &mDeferredClears,
                                                              webgpu::kUnpackedDepthIndex));
        }
        else
        {
            ANGLE_TRY(depthStencilRt->flushImageStagedUpdates(contextWgpu, nullptr, 0));
        }
    }

    // If we added any new attachments, we start a render pass to fully flush the updates.
    if (mNewRenderPassDesc.has_value())
    {
        ANGLE_TRY(startRenderPassNewAttachments(contextWgpu));
    }
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::flushDeferredClears(ContextWgpu *contextWgpu)
{
    if (mDeferredClears.empty())
    {
        return angle::Result::Continue;
    }
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    webgpu::PackedRenderPassDescriptor clearRenderPassDesc;
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        if (!mDeferredClears.test(colorIndexGL))
        {
            continue;
        }
        clearRenderPassDesc.colorAttachments.push_back(webgpu::CreateNewClearColorAttachment(
            mDeferredClears[colorIndexGL].clearColor, mDeferredClears[colorIndexGL].depthSlice,
            mRenderTargetCache.getColorDraw(mState, colorIndexGL)->getTextureView()));
    }
    if (mRenderTargetCache.getDepthStencil() &&
        (mDeferredClears.hasDepth() || mDeferredClears.hasStencil()))
    {
        clearRenderPassDesc.depthStencilAttachment = webgpu::CreateNewClearDepthStencilAttachment(
            mDeferredClears.getDepthValue(), mDeferredClears.getStencilValue(),
            mRenderTargetCache.getDepthStencil()->getTextureView(), !mDeferredClears.hasDepth(),
            !mDeferredClears.hasStencil());
    }

    mCurrentRenderPassDesc = std::move(clearRenderPassDesc);
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    mDeferredClears.reset();

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::startRenderPassNewAttachments(ContextWgpu *contextWgpu)
{
    // Flush out a render pass if there is an active one.
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    ASSERT(mNewRenderPassDesc.has_value());
    mCurrentRenderPassDesc = std::move(mNewRenderPassDesc.value());
    mNewRenderPassDesc.reset();

    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::startNewRenderPass(ContextWgpu *contextWgpu)
{
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    webgpu::PackedRenderPassDescriptor newRenderPass;
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        webgpu::PackedRenderPassColorAttachment colorAttachment;
        colorAttachment.view =
            mRenderTargetCache.getColorDraw(mState, colorIndexGL)->getTextureView();
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAttachment.storeOp    = WGPUStoreOp_Store;

        if (mDeferredClears.test(colorIndexGL))
        {
            colorAttachment.loadOp     = WGPULoadOp_Clear;
            colorAttachment.clearValue = mDeferredClears[colorIndexGL].clearColor;
            mDeferredClears.reset(colorIndexGL);
        }
        else
        {
            colorAttachment.loadOp = WGPULoadOp_Load;
        }

        newRenderPass.colorAttachments.push_back(colorAttachment);
    }
    if (mRenderTargetCache.getDepthStencil())
    {
        webgpu::PackedRenderPassDepthStencilAttachment dsAttachment =
            webgpu::CreateNewDepthStencilAttachment(
                mRenderTargetCache.getDepthStencil()->getTextureView(), mState.hasDepth(),
                mState.hasStencil());

        if (mDeferredClears.hasDepth())
        {
            dsAttachment.depthLoadOp     = WGPULoadOp_Clear;
            dsAttachment.depthClearValue = mDeferredClears.getDepthValue();
            mDeferredClears.reset(webgpu::kUnpackedDepthIndex);
        }
        if (mDeferredClears.hasStencil())
        {
            dsAttachment.stencilLoadOp     = WGPULoadOp_Clear;
            dsAttachment.stencilClearValue = mDeferredClears.getStencilValue();
            mDeferredClears.reset(webgpu::kUnpackedStencilIndex);
        }

        newRenderPass.depthStencilAttachment = dsAttachment;
    }

    mCurrentRenderPassDesc = std::move(newRenderPass);
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));

    return angle::Result::Continue;
}

void FramebufferWgpu::mergeClearWithDeferredClears(const gl::ColorF &clearValue,
                                                   gl::DrawBufferMask clearColorBuffers,
                                                   float depthValue,
                                                   uint32_t stencilValue,
                                                   bool clearColor,
                                                   bool clearDepth,
                                                   bool clearStencil)
{
    for (size_t enabledDrawBuffer : clearColorBuffers)
    {
        mDeferredClears.store(static_cast<uint32_t>(enabledDrawBuffer),
                              {getClearColorWithCorrectAlpha(clearValue, enabledDrawBuffer),
                               WGPU_DEPTH_SLICE_UNDEFINED, 0, 0});
    }
    if (clearDepth)
    {
        mDeferredClears.store(webgpu::kUnpackedDepthIndex,
                              {gl::ColorF(), WGPU_DEPTH_SLICE_UNDEFINED, depthValue, 0});
    }
    if (clearStencil)
    {
        mDeferredClears.store(webgpu::kUnpackedStencilIndex,
                              {gl::ColorF(), WGPU_DEPTH_SLICE_UNDEFINED, 0, stencilValue});
    }
}

gl::Rectangle FramebufferWgpu::getReadArea(const gl::Context *context,
                                           const gl::Rectangle &glArea) const
{
    RenderTargetWgpu *readRT = getReadPixelsRenderTarget();
    if (!readRT)
    {
        readRT = mRenderTargetCache.getDepthStencil();
    }
    ASSERT(readRT);
    gl::Rectangle flippedArea = glArea;
    if (mFlipY)
    {
        flippedArea.y = readRT->getImage()->getLevelSize(readRT->getLevelIndex()).height -
                        flippedArea.y - flippedArea.height;
    }

    return flippedArea;
}

angle::Result FramebufferWgpu::blitColorAttachment(const gl::Context *context,
                                                   const BlitGeometry &geometry,
                                                   RenderTargetWgpu *readRenderTarget,
                                                   bool srcFlipY,
                                                   RenderTargetWgpu *drawRenderTarget,
                                                   GLenum filter)
{
    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);
    webgpu::ImageHelper *srcImage;
    webgpu::ImageHelper *dstImage;
    WGPUExtent3D srcLevelSize;
    WGPUExtent3D dstLevelSize;

    ANGLE_TRY(getBlitImageAndSize(contextWgpu, readRenderTarget, &srcImage, &srcLevelSize));
    ANGLE_TRY(getBlitImageAndSize(contextWgpu, drawRenderTarget, &dstImage, &dstLevelSize));

    const webgpu::UtilsWgpu::BlitParams params =
        GetBlitParams(geometry.dstArea, geometry.dstScissor, geometry.srcArea, geometry.callSrcArea,
                      geometry.callDstArea, srcFlipY, srcLevelSize, flipY(), dstLevelSize, filter);

    // A copy can't convert between formats. Formats without alpha may be stored in a format with
    // alpha, whose stored alpha must not be read, and in which an alpha of one must be stored, so
    // a copy into such a format would carry the source's stored alpha.
    const bool srcHasAlpha    = angle::Format::Get(srcImage->getIntendedFormatID()).alphaBits != 0;
    const bool dstHasAlpha    = angle::Format::Get(dstImage->getIntendedFormatID()).alphaBits != 0;
    const bool dstStoresAlpha = angle::Format::Get(dstImage->getActualFormatID()).alphaBits != 0;
    if (srcImage->getActualFormatID() == dstImage->getActualFormatID() &&
        (!dstStoresAlpha || (srcHasAlpha && dstHasAlpha)) && srcImage->getSamples() <= 1 &&
        IsBlitUnscaledAndUnflipped(params))
    {
        return blitWithDirectCopy(contextWgpu, readRenderTarget, drawRenderTarget, params,
                                  WGPUTextureAspect_All);
    }

    webgpu::TextureViewHandle dstView;
    angle::FormatID dstViewFormatID = dstImage->getActualFormatID();

    // Handle GL_FRAMEBUFFER_SRGB
    if (!context->getState().getFramebufferSRGB() && angle::Format::Get(dstViewFormatID).isSRGB)
    {
        // If sRGB is disabled but the texture is sRGB, we need to write to a linear view.
        dstViewFormatID = dstImage->getIntendedFormatID();

        ANGLE_TRY(dstImage->createTextureViewSingleLevel(
            drawRenderTarget->getGlLevel(), drawRenderTarget->getLayer(), dstView,
            WGPUTextureAspect_All, webgpu::GetWgpuTextureFormatFromFormatID(dstViewFormatID)));
    }
    else
    {
        ANGLE_TRY(dstImage->createTextureViewSingleLevel(
            drawRenderTarget->getGlLevel(), drawRenderTarget->getLayer(), dstView,
            WGPUTextureAspect_All, WGPUTextureFormat_Undefined));
    }

    webgpu::TextureViewHandle srcView;
    ANGLE_TRY(srcImage->createTextureViewSingleLevel(
        readRenderTarget->getGlLevel(), readRenderTarget->getLayer(), srcView,
        WGPUTextureAspect_All, WGPUTextureFormat_Undefined));

    return contextWgpu->getUtils()->blitColor(
        contextWgpu, params, srcView, srcImage->getSamples(), srcImage->getIntendedFormatID(),
        srcImage->getActualFormatID(), dstView, dstImage->getIntendedFormatID(), dstViewFormatID);
}

angle::Result FramebufferWgpu::blitDepthStencilAttachment(const gl::Context *context,
                                                          const BlitGeometry &geometry,
                                                          RenderTargetWgpu *readRenderTarget,
                                                          bool srcFlipY,
                                                          RenderTargetWgpu *drawRenderTarget,
                                                          WGPUTextureAspect aspect)
{
    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);
    webgpu::ImageHelper *srcImage;
    webgpu::ImageHelper *dstImage;
    WGPUExtent3D srcLevelSize;
    WGPUExtent3D dstLevelSize;

    ANGLE_TRY(getBlitImageAndSize(contextWgpu, readRenderTarget, &srcImage, &srcLevelSize));
    ANGLE_TRY(getBlitImageAndSize(contextWgpu, drawRenderTarget, &dstImage, &dstLevelSize));

    const webgpu::UtilsWgpu::BlitParams params = GetBlitParams(
        geometry.dstArea, geometry.dstScissor, geometry.srcArea, geometry.callSrcArea,
        geometry.callDstArea, srcFlipY, srcLevelSize, flipY(), dstLevelSize, GL_NEAREST);

    const WGPUTextureFormat wgpuFormat = srcImage->toWgpuTextureFormat();
    if (wgpuFormat != WGPUTextureFormat_Depth24Plus &&
        wgpuFormat != WGPUTextureFormat_Depth24PlusStencil8 &&
        srcImage->getActualFormatID() == dstImage->getActualFormatID() &&
        srcImage->getSamples() <= 1 && IsBlitUnscaledAndUnflipped(params))
    {
        return blitWithDirectCopy(contextWgpu, readRenderTarget, drawRenderTarget, params, aspect);
    }

    UNIMPLEMENTED();
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::blitWithDirectCopy(ContextWgpu *contextWgpu,
                                                  RenderTargetWgpu *readRenderTarget,
                                                  RenderTargetWgpu *drawRenderTarget,
                                                  const webgpu::UtilsWgpu::BlitParams &params,
                                                  WGPUTextureAspect aspect)
{
    webgpu::ImageHelper *srcImage = readRenderTarget->getImage();
    webgpu::ImageHelper *dstImage = drawRenderTarget->getImage();

    // Copy the part of the destination inside the scissor.
    const gl::Rectangle &dstScissor = params.dstScissor;
    gl::Box sourceBox(static_cast<int>(params.srcArea.x) + dstScissor.x - params.dstArea.x,
                      static_cast<int>(params.srcArea.y) + dstScissor.y - params.dstArea.y, 0,
                      dstScissor.width, dstScissor.height, 1);
    gl::Offset dstOffset(dstScissor.x, dstScissor.y, drawRenderTarget->getLayer());

    gl::ImageIndex dstIndex = gl::ImageIndex::Make2D(drawRenderTarget->getGlLevel().get());

    ANGLE_TRY(dstImage->CopyImage(contextWgpu, srcImage, dstIndex, dstOffset,
                                  readRenderTarget->getGlLevel(), readRenderTarget->getLayer(),
                                  sourceBox, aspect));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::getBlitImageAndSize(ContextWgpu *contextWgpu,
                                                   RenderTargetWgpu *renderTarget,
                                                   webgpu::ImageHelper **imageOut,
                                                   WGPUExtent3D *levelSizeOut)
{
    webgpu::ImageHelper *image = renderTarget->getImage();

    ANGLE_TRY(image->flushStagedUpdates(contextWgpu));

    *imageOut = image;

    *levelSizeOut = image->getLevelSize(image->toWgpuLevel(renderTarget->getGlLevel()));

    return angle::Result::Continue;
}

}  // namespace rx
