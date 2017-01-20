/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BitmapImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "TextStream.h"
#include "Timer.h"
#include <wtf/CurrentTime.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include <limits>
#endif

namespace WebCore {

BitmapImage::BitmapImage(ImageObserver* observer)
    : Image(observer)
    , m_source(this)
{
}

BitmapImage::BitmapImage(NativeImagePtr&& image, ImageObserver* observer)
    : Image(observer)
    , m_source(WTFMove(image))
{
}

BitmapImage::~BitmapImage()
{
    invalidatePlatformData();
    stopAnimation();
}

void BitmapImage::destroyDecodedData(bool destroyAll)
{
    m_source.destroyDecodedData(data(), destroyAll, m_currentFrame);
    invalidatePlatformData();
}

void BitmapImage::destroyDecodedDataIfNecessary(bool destroyAll)
{
    m_source.destroyDecodedDataIfNecessary(data(), destroyAll, m_currentFrame);
}

bool BitmapImage::dataChanged(bool allDataReceived)
{
    return m_source.dataChanged(data(), allDataReceived);
}

NativeImagePtr BitmapImage::frameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const GraphicsContext* targetContext)
{
    if (!frameHasValidNativeImageAtIndex(index, subsamplingLevel)) {
        LOG(Images, "BitmapImage %p %s - subsamplingLevel was %d, resampling", this, __FUNCTION__, static_cast<int>(frameSubsamplingLevelAtIndex(index)));
        invalidatePlatformData();
    }

    return m_source.frameImageAtIndex(index, subsamplingLevel, targetContext);
}

NativeImagePtr BitmapImage::nativeImage(const GraphicsContext* targetContext)
{
    return frameImageAtIndex(0, SubsamplingLevel::Default, targetContext);
}

NativeImagePtr BitmapImage::nativeImageForCurrentFrame(const GraphicsContext* targetContext)
{
    return frameImageAtIndex(m_currentFrame, SubsamplingLevel::Default, targetContext);
}

#if USE(CG)
NativeImagePtr BitmapImage::nativeImageOfSize(const IntSize& size, const GraphicsContext* targetContext)
{
    size_t count = frameCount();

    for (size_t i = 0; i < count; ++i) {
        auto image = frameImageAtIndex(i, SubsamplingLevel::Default, targetContext);
        if (image && nativeImageSize(image) == size)
            return image;
    }

    // Fallback to the first frame image if we can't find the right size
    return frameImageAtIndex(0, SubsamplingLevel::Default, targetContext);
}

Vector<NativeImagePtr> BitmapImage::framesNativeImages()
{
    Vector<NativeImagePtr> images;
    size_t count = frameCount();

    for (size_t i = 0; i < count; ++i) {
        if (auto image = frameImageAtIndex(i))
            images.append(image);
    }

    return images;
}
#endif

#if !ASSERT_DISABLED
bool BitmapImage::notSolidColor()
{
    return size().width() != 1 || size().height() != 1 || frameCount() > 1;
}
#endif

void BitmapImage::draw(GraphicsContext& context, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode mode, ImageOrientationDescription description)
{
    if (destRect.isEmpty() || srcRect.isEmpty())
        return;

    StartAnimationResult result = internalStartAnimation();

    Color color;
    if (result == StartAnimationResult::DecodingActive && showDebugBackground())
        color = Color::yellow;
    else
        color = singlePixelSolidColor();

    if (color.isValid()) {
        fillWithSolidColor(context, destRect, color, op);
        return;
    }

    float scale = subsamplingScale(context, destRect, srcRect);
    m_currentSubsamplingLevel = allowSubsampling() ? m_source.subsamplingLevelForScale(scale) : SubsamplingLevel::Default;
    LOG(Images, "BitmapImage %p draw - subsamplingLevel %d at scale %.4f", this, static_cast<int>(m_currentSubsamplingLevel), scale);

    auto image = frameImageAtIndex(m_currentFrame, m_currentSubsamplingLevel, &context);
    if (!image) // If it's too early we won't have an image yet.
        return;

    ImageOrientation orientation(description.imageOrientation());
    if (description.respectImageOrientation() == RespectImageOrientation)
        orientation = frameOrientationAtIndex(m_currentFrame);

    drawNativeImage(image, context, destRect, srcRect, IntSize(size()), op, mode, orientation);

    if (imageObserver())
        imageObserver()->didDraw(this);
}

void BitmapImage::drawPattern(GraphicsContext& ctxt, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
{
    if (tileRect.isEmpty())
        return;

    if (!ctxt.drawLuminanceMask()) {
        Image::drawPattern(ctxt, destRect, tileRect, transform, phase, spacing, op, blendMode);
        return;
    }

    if (!m_cachedImage) {
        auto buffer = ImageBuffer::createCompatibleBuffer(expandedIntSize(tileRect.size()), ColorSpaceSRGB, ctxt);
        if (!buffer)
            return;

        ImageObserver* observer = imageObserver();

        // Temporarily reset image observer, we don't want to receive any changeInRect() calls due to this relayout.
        setImageObserver(nullptr);

        draw(buffer->context(), tileRect, tileRect, op, blendMode, ImageOrientationDescription());

        setImageObserver(observer);
        buffer->convertToLuminanceMask();

        m_cachedImage = buffer->copyImage(DontCopyBackingStore, Unscaled);
        if (!m_cachedImage)
            return;
    }

    ctxt.setDrawLuminanceMask(false);
    m_cachedImage->drawPattern(ctxt, destRect, tileRect, transform, phase, spacing, op, blendMode);
}

bool BitmapImage::shouldAnimate()
{
    return repetitionCount() && !m_animationFinished && imageObserver();
}

bool BitmapImage::canAnimate()
{
    return shouldAnimate() && frameCount() > 1;
}

void BitmapImage::clearTimer()
{
    m_frameTimer = nullptr;
}

void BitmapImage::startTimer(double delay)
{
    ASSERT(!m_frameTimer);
    m_frameTimer = std::make_unique<Timer>(*this, &BitmapImage::advanceAnimation);
    m_frameTimer->startOneShot(delay);
}

BitmapImage::StartAnimationResult BitmapImage::internalStartAnimation()
{
    if (!canAnimate())
        return StartAnimationResult::CannotStart;

    if (m_frameTimer)
        return StartAnimationResult::TimerActive;
    
    // Don't start a new animation until we draw the frame that is currently being decoded.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (frameIsBeingDecodedAtIndex(nextFrame))
        return StartAnimationResult::DecodingActive;

    if (m_currentFrame >= frameCount() - 1) {
        // Don't advance past the last frame if we haven't decoded the whole image
        // yet and our repetition count is potentially unset. The repetition count
        // in a GIF can potentially come after all the rest of the image data, so
        // wait on it.
        if (!m_source.isAllDataReceived() && repetitionCount() == RepetitionCountOnce)
            return StartAnimationResult::IncompleteData;

        ++m_repetitionsComplete;

        // Check for the end of animation.
        if (repetitionCount() != RepetitionCountInfinite && m_repetitionsComplete >= repetitionCount()) {
            m_animationFinished = true;
            destroyDecodedDataIfNecessary(false);
            return StartAnimationResult::CannotStart;
        }

        destroyDecodedDataIfNecessary(true);
    }

    // Don't advance the animation to an incomplete frame.
    if (!m_source.isAllDataReceived() && !frameIsCompleteAtIndex(nextFrame))
        return StartAnimationResult::IncompleteData;

    double time = monotonicallyIncreasingTime();

    // Handle initial state.
    if (!m_desiredFrameStartTime)
        m_desiredFrameStartTime = time;

    // Setting 'm_desiredFrameStartTime' to 'time' means we are late; otherwise we are early.
    m_desiredFrameStartTime = std::max(time, m_desiredFrameStartTime + frameDurationAtIndex(m_currentFrame));

    // Request async decoding for nextFrame only if this is required. If nextFrame is not in the frameCache,
    // it will be decoded on a separate work queue. When decoding nextFrame finishes, we will be notified
    // through the callback newFrameNativeImageAvailableAtIndex(). Otherwise, advanceAnimation() will be called
    // when the timer fires and m_currentFrame will be advanced to nextFrame since it is not being decoded.
    if ((allowAnimatedImageAsyncDecoding() && m_source.isAsyncDecodingRequired()) || isAsyncDecodingForcedForTesting()) {
        if (!m_source.requestFrameAsyncDecodingAtIndex(nextFrame, m_currentSubsamplingLevel))
            LOG(Images, "BitmapImage %p %s - cachedFrameCount %ld nextFrame %ld", this, __FUNCTION__, ++m_cachedFrameCount, nextFrame);
        m_desiredFrameDecodeTimeForTesting = time + std::max(m_frameDecodingDurationForTesting, 0.0f);
    }

    ASSERT(!m_frameTimer);
    startTimer(m_desiredFrameStartTime - time);
    return StartAnimationResult::Started;
}

void BitmapImage::advanceAnimation()
{
    clearTimer();

    // Pretend as if decoding nextFrame has taken m_frameDecodingDurationForTesting from
    // the time this decoding was requested.
    if (isAsyncDecodingForcedForTesting()) {
        double time = monotonicallyIncreasingTime();
        // Start a timer with the remaining time from now till the m_desiredFrameDecodeTime.
        if (m_desiredFrameDecodeTimeForTesting > std::max(time, m_desiredFrameStartTime)) {
            startTimer(m_desiredFrameDecodeTimeForTesting - time);
            return;
        }
    }
    
    // Don't advance to nextFrame unless its decoding has finished or was not required.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (!frameIsBeingDecodedAtIndex(nextFrame))
        internalAdvanceAnimation();
    else {
        // Force repaint if showDebugBackground() is on.
        if (showDebugBackground())
            imageObserver()->changedInRect(this);
        LOG(Images, "BitmapImage %p %s - lateFrameCount %ld nextFrame %ld", this, __FUNCTION__, ++m_lateFrameCount, nextFrame);
    }
}

void BitmapImage::internalAdvanceAnimation()
{
    m_currentFrame = (m_currentFrame + 1) % frameCount();
    ASSERT(!frameIsBeingDecodedAtIndex(m_currentFrame));

    destroyDecodedDataIfNecessary(false);

    if (imageObserver())
        imageObserver()->animationAdvanced(this);
}

void BitmapImage::stopAnimation()
{
    // This timer is used to animate all occurrences of this image. Don't invalidate
    // the timer unless all renderers have stopped drawing.
    clearTimer();
    m_source.stopAsyncDecodingQueue();
}

void BitmapImage::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = RepetitionCountNone;
    m_desiredFrameStartTime = 0;
    m_animationFinished = false;

    // For extremely large animations, when the animation is reset, we just throw everything away.
    destroyDecodedDataIfNecessary(true);
}

void BitmapImage::newFrameNativeImageAvailableAtIndex(size_t index)
{
    UNUSED_PARAM(index);
    ASSERT(index == (m_currentFrame + 1) % frameCount());

    // Don't advance to nextFrame unless the timer was fired before its decoding finishes.
    if (canAnimate() && !m_frameTimer)
        internalAdvanceAnimation();
    else
        LOG(Images, "BitmapImage %p %s - earlyFrameCount %ld nextFrame %ld", this, __FUNCTION__, ++m_earlyFrameCount, index);
}

void BitmapImage::dump(TextStream& ts) const
{
    Image::dump(ts);
    
    if (isAnimated())
        ts.dumpProperty("current-frame", m_currentFrame);
    
    m_source.dump(ts);
}

}
