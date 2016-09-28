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

NativeImagePtr BitmapImage::frameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel)
{
    if (frameHasInvalidNativeImageAtIndex(index, subsamplingLevel)) {
        LOG(Images, "BitmapImage %p frameImageAtIndex - subsamplingLevel was %d, resampling", this, static_cast<int>(frameSubsamplingLevelAtIndex(index)));
        invalidatePlatformData();
    }

    return m_source.frameImageAtIndex(index, subsamplingLevel);
}

NativeImagePtr BitmapImage::nativeImage()
{
    return frameImageAtIndex(0);
}

NativeImagePtr BitmapImage::nativeImageForCurrentFrame()
{
    return frameImageAtIndex(m_currentFrame);
}

#if USE(CG)
NativeImagePtr BitmapImage::nativeImageOfSize(const IntSize& size)
{
    size_t count = frameCount();

    for (size_t i = 0; i < count; ++i) {
        auto image = frameImageAtIndex(i);
        if (image && nativeImageSize(image) == size)
            return image;
    }

    // Fallback to the first frame image if we can't find the right size
    return frameImageAtIndex(0);
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

#if PLATFORM(IOS)
    startAnimation(DoNotCatchUp);
#else
    startAnimation();
#endif

    Color color = singlePixelSolidColor();
    if (color.isValid()) {
        fillWithSolidColor(context, destRect, color, op);
        return;
    }

    float scale = subsamplingScale(context, destRect, srcRect);
    SubsamplingLevel subsamplingLevel = m_source.subsamplingLevelForScale(scale);
    LOG(Images, "BitmapImage %p draw - subsamplingLevel %d at scale %.4f", this, subsamplingLevel, scale);

    auto image = frameImageAtIndex(m_currentFrame, subsamplingLevel);
    if (!image) // If it's too early we won't have an image yet.
        return;

    ImageOrientation orientation(description.imageOrientation());
    if (description.respectImageOrientation() == RespectImageOrientation)
        orientation = frameOrientationAtIndex(m_currentFrame);

    drawNativeImage(image, context, destRect, srcRect, IntSize(size()), op, mode, orientation);

    if (imageObserver())
        imageObserver()->didDraw(this);
}

void BitmapImage::drawPattern(GraphicsContext& ctxt, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, const FloatRect& destRect, BlendMode blendMode)
{
    if (tileRect.isEmpty())
        return;

    if (!ctxt.drawLuminanceMask()) {
        Image::drawPattern(ctxt, tileRect, transform, phase, spacing, op, destRect, blendMode);
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
    m_cachedImage->drawPattern(ctxt, tileRect, transform, phase, spacing, op, destRect, blendMode);
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

void BitmapImage::startAnimation(CatchUpAnimation catchUpIfNecessary)
{
    if (m_frameTimer || !shouldAnimate() || frameCount() <= 1)
        return;

    // If we aren't already animating, set now as the animation start time.
    const double time = monotonicallyIncreasingTime();
    if (!m_desiredFrameStartTime)
        m_desiredFrameStartTime = time;

    // Don't advance the animation to an incomplete frame.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (!m_source.isAllDataReceived() && !frameIsCompleteAtIndex(nextFrame))
        return;

    // Don't advance past the last frame if we haven't decoded the whole image
    // yet and our repetition count is potentially unset. The repetition count
    // in a GIF can potentially come after all the rest of the image data, so
    // wait on it.
    if (!m_source.isAllDataReceived() && repetitionCount() == RepetitionCountOnce && m_currentFrame >= (frameCount() - 1))
        return;

    // Determine time for next frame to start. By ignoring paint and timer lag
    // in this calculation, we make the animation appear to run at its desired
    // rate regardless of how fast it's being repainted.
    const double currentDuration = frameDurationAtIndex(m_currentFrame);
    m_desiredFrameStartTime += currentDuration;

#if !PLATFORM(IOS)
    // When an animated image is more than five minutes out of date, the
    // user probably doesn't care about resyncing and we could burn a lot of
    // time looping through frames below. Just reset the timings.
    const double cAnimationResyncCutoff = 5 * 60;
    if ((time - m_desiredFrameStartTime) > cAnimationResyncCutoff)
        m_desiredFrameStartTime = time + currentDuration;
#else
    // Maintaining frame-to-frame delays is more important than
    // maintaining absolute animation timing, so reset the timings each frame.
    m_desiredFrameStartTime = time + currentDuration;
#endif

    // The image may load more slowly than it's supposed to animate, so that by
    // the time we reach the end of the first repetition, we're well behind.
    // Clamp the desired frame start time in this case, so that we don't skip
    // frames (or whole iterations) trying to "catch up". This is a tradeoff:
    // It guarantees users see the whole animation the second time through and
    // don't miss any repetitions, and is closer to what other browsers do; on
    // the other hand, it makes animations "less accurate" for pages that try to
    // sync an image and some other resource (e.g. audio), especially if users
    // switch tabs (and thus stop drawing the animation, which will pause it)
    // during that initial loop, then switch back later.
    if (nextFrame == 0 && m_repetitionsComplete == 0 && m_desiredFrameStartTime < time)
        m_desiredFrameStartTime = time;

    if (catchUpIfNecessary == DoNotCatchUp || time < m_desiredFrameStartTime) {
        // Haven't yet reached time for next frame to start; delay until then.
        startTimer(std::max<double>(m_desiredFrameStartTime - time, 0));
        return;
    }

    ASSERT(!m_frameTimer);

    // We've already reached or passed the time for the next frame to start.
    // See if we've also passed the time for frames after that to start, in
    // case we need to skip some frames entirely. Remember not to advance
    // to an incomplete frame.

#if !LOG_DISABLED
    size_t startCatchupFrameIndex = nextFrame;
#endif

    for (size_t frameAfterNext = (nextFrame + 1) % frameCount(); frameIsCompleteAtIndex(frameAfterNext); frameAfterNext = (nextFrame + 1) % frameCount()) {
        // Should we skip the next frame?
        double frameAfterNextStartTime = m_desiredFrameStartTime + frameDurationAtIndex(nextFrame);
        if (time < frameAfterNextStartTime)
            break;

        // Yes; skip over it without notifying our observers. If we hit the end while catching up,
        // tell the observer asynchronously.
        if (!internalAdvanceAnimation(SkippingFramesToCatchUp)) {
            m_animationFinishedWhenCatchingUp = true;
            startTimer(0);
            LOG(Images, "BitmapImage %p startAnimation catching up from frame %lu, ended", this, startCatchupFrameIndex);
            return;
        }
        m_desiredFrameStartTime = frameAfterNextStartTime;
        nextFrame = frameAfterNext;
    }

    LOG(Images, "BitmapImage %p startAnimation catching up jumped from from frame %lu to %d", this, startCatchupFrameIndex, (int)nextFrame - 1);

    // Draw the next frame as soon as possible. Note that m_desiredFrameStartTime
    // may be in the past, meaning the next time through this function we'll
    // kick off the next advancement sooner than this frame's duration would suggest.
    startTimer(0);
}

void BitmapImage::advanceAnimation()
{
    internalAdvanceAnimation();
    // At this point the image region has been marked dirty, and if it's
    // onscreen, we'll soon make a call to draw(), which will call
    // startAnimation() again to keep the animation moving.
}

bool BitmapImage::internalAdvanceAnimation(AnimationAdvancement advancement)
{
    clearTimer();

    if (m_animationFinishedWhenCatchingUp) {
        imageObserver()->animationAdvanced(this);
        m_animationFinishedWhenCatchingUp = false;
        return false;
    }

    ++m_currentFrame;
    bool advancedAnimation = true;
    bool destroyAll = false;
    if (m_currentFrame >= frameCount()) {
        ++m_repetitionsComplete;

        // Get the repetition count again. If we weren't able to get a
        // repetition count before, we should have decoded the whole image by
        // now, so it should now be available.
        if (repetitionCount() != RepetitionCountInfinite && m_repetitionsComplete > repetitionCount()) {
            m_animationFinished = true;
            m_desiredFrameStartTime = 0;
            --m_currentFrame;
            advancedAnimation = false;
        } else {
            m_currentFrame = 0;
            destroyAll = true;
        }
    }
    destroyDecodedDataIfNecessary(destroyAll);

    // We need to draw this frame if we advanced to it while not skipping, or if
    // while trying to skip frames we hit the last frame and thus had to stop.
    if (advancement == Normal && advancedAnimation)
        imageObserver()->animationAdvanced(this);

    return advancedAnimation;
}

void BitmapImage::stopAnimation()
{
    // This timer is used to animate all occurrences of this image. Don't invalidate
    // the timer unless all renderers have stopped drawing.
    clearTimer();
}

void BitmapImage::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_desiredFrameStartTime = 0;
    m_animationFinished = false;

    // For extremely large animations, when the animation is reset, we just throw everything away.
    destroyDecodedDataIfNecessary(true);
}

void BitmapImage::dump(TextStream& ts) const
{
    Image::dump(ts);
    
    if (isAnimated())
        ts.dumpProperty("current-frame", m_currentFrame);
    
    m_source.dump(ts);
}

}
