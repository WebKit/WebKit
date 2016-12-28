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

#include <wtf/CheckedArithmetic.h>

namespace WebCore {

BitmapImage::BitmapImage(ImageObserver* observer)
    : Image(observer)
    , m_animationFinished(false)
    , m_allDataReceived(false)
    , m_haveSize(false)
    , m_sizeAvailable(false)
    , m_haveFrameCount(false)
    , m_animationFinishedWhenCatchingUp(false)
{
}

BitmapImage::BitmapImage(NativeImagePtr&& image, ImageObserver* observer)
    : Image(observer)
    , m_source(image)
    , m_frameCount(1)
    , m_animationFinished(true)
    , m_allDataReceived(true)
    , m_haveSize(true)
    , m_sizeAvailable(true)
    , m_haveFrameCount(true)
    , m_animationFinishedWhenCatchingUp(false)
{
    // Since we don't have a decoder, we can't figure out the image orientation.
    // Set m_sizeRespectingOrientation to be the same as m_size so it's not 0x0.
    m_sizeRespectingOrientation = m_size = NativeImage::size(image);
    m_decodedSize = (m_size.area() * 4).unsafeGet();
    
    m_frames.grow(1);
    m_frames[0].m_hasAlpha = NativeImage::hasAlpha(image);
    m_frames[0].m_haveMetadata = true;
    m_frames[0].m_image = WTFMove(image);
}

BitmapImage::~BitmapImage()
{
    invalidatePlatformData();
    stopAnimation();
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

bool BitmapImage::haveFrameImageAtIndex(size_t index)
{
    if (index >= frameCount())
        return false;

    if (index >= m_frames.size())
        return false;

    return m_frames[index].m_image;
}

bool BitmapImage::hasSingleSecurityOrigin() const
{
    return true;
}

void BitmapImage::destroyDecodedData(bool destroyAll)
{
    unsigned frameBytesCleared = 0;
    const size_t clearBeforeFrame = destroyAll ? m_frames.size() : m_currentFrame;

    // Because we can advance frames without always needing to decode the actual
    // bitmap data, |m_currentFrame| may be larger than m_frames.size();
    // make sure not to walk off the end of the container in this case.
    for (size_t i = 0; i <  std::min(clearBeforeFrame, m_frames.size()); ++i) {
        // The underlying frame isn't actually changing (we're just trying to
        // save the memory for the framebuffer data), so we don't need to clear
        // the metadata.
        unsigned frameBytes = m_frames[i].m_frameBytes;
        if (m_frames[i].clear(false))
            frameBytesCleared += frameBytes;
    }

    m_source.clear(destroyAll, clearBeforeFrame, data(), m_allDataReceived);
    destroyMetadataAndNotify(frameBytesCleared, ClearedSource::Yes);
}

void BitmapImage::destroyDecodedDataIfNecessary(bool destroyAll)
{
    // Animated images over a certain size are considered large enough that we'll only hang on
    // to one frame at a time.
#if PLATFORM(IOS)
    const unsigned largeAnimationCutoff = 2097152;
#else
    const unsigned largeAnimationCutoff = 5242880;
#endif

    // If we have decoded frames but there is no encoded data, we shouldn't destroy
    // the decoded image since we won't be able to reconstruct it later.
    if (!data() && m_frames.size())
        return;

    unsigned allFrameBytes = 0;
    for (size_t i = 0; i < m_frames.size(); ++i)
        allFrameBytes += m_frames[i].usedFrameBytes();

    if (allFrameBytes > largeAnimationCutoff) {
        LOG(Images, "BitmapImage %p destroyDecodedDataIfNecessary destroyingData: allFrameBytes=%u cutoff=%u", this, allFrameBytes, largeAnimationCutoff);
        destroyDecodedData(destroyAll);
    }
}

void BitmapImage::destroyMetadataAndNotify(unsigned frameBytesCleared, ClearedSource clearedSource)
{
    m_solidColor = Nullopt;
    invalidatePlatformData();

    ASSERT(m_decodedSize >= frameBytesCleared);
    m_decodedSize -= frameBytesCleared;

    // Clearing the ImageSource destroys the extra decoded data used for determining image properties.
    if (clearedSource == ClearedSource::Yes) {
        frameBytesCleared += m_decodedPropertiesSize;
        m_decodedPropertiesSize = 0;
    }

    if (frameBytesCleared && imageObserver())
        imageObserver()->decodedSizeChanged(this, -static_cast<long long>(frameBytesCleared));
}

void BitmapImage::cacheFrame(size_t index, SubsamplingLevel subsamplingLevel, ImageFrameCaching frameCaching)
{
    size_t numFrames = frameCount();
    ASSERT(m_decodedSize == 0 || numFrames > 1);
    
    if (m_frames.size() < numFrames)
        m_frames.grow(numFrames);

    if (frameCaching == CacheMetadataAndFrame) {
        m_frames[index].m_image = m_source.createFrameImageAtIndex(index, subsamplingLevel);
        m_frames[index].m_subsamplingLevel = subsamplingLevel;
    }

    m_frames[index].m_orientation = m_source.orientationAtIndex(index);
    m_frames[index].m_haveMetadata = true;
    m_frames[index].m_isComplete = m_source.frameIsCompleteAtIndex(index);

    if (repetitionCount(false) != cAnimationNone)
        m_frames[index].m_duration = m_source.frameDurationAtIndex(index);

    m_frames[index].m_hasAlpha = m_source.frameHasAlphaAtIndex(index);
    m_frames[index].m_frameBytes = m_source.frameBytesAtIndex(index, subsamplingLevel);

    LOG(Images, "BitmapImage %p cacheFrame %lu (%s%u bytes, complete %d)", this, index, frameCaching == CacheMetadataOnly ? "metadata only, " : "", m_frames[index].m_frameBytes, m_frames[index].m_isComplete);

    if (m_frames[index].m_image && WTF::isInBounds<unsigned>(m_frames[index].m_frameBytes + m_decodedSize)) {
        m_decodedSize += m_frames[index].m_frameBytes;
        // The fully-decoded frame will subsume the partially decoded data used
        // to determine image properties.
        long long deltaBytes = static_cast<long long>(m_frames[index].m_frameBytes) - m_decodedPropertiesSize;
        m_decodedPropertiesSize = 0;
        if (imageObserver())
            imageObserver()->decodedSizeChanged(this, deltaBytes);
    }
}

void BitmapImage::didDecodeProperties() const
{
    if (m_decodedSize)
        return;

    unsigned updatedSize = m_source.bytesDecodedToDetermineProperties();
    if (m_decodedPropertiesSize == updatedSize)
        return;

    long long deltaBytes = static_cast<long long>(updatedSize) - m_decodedPropertiesSize;
    m_decodedPropertiesSize = updatedSize;
    if (imageObserver())
        imageObserver()->decodedSizeChanged(this, deltaBytes);
}

void BitmapImage::updateSize() const
{
    if (!m_sizeAvailable || m_haveSize)
        return;

    m_size = m_source.size();
    m_sizeRespectingOrientation = m_source.sizeRespectingOrientation();

    m_haveSize = true;
    didDecodeProperties();
}

FloatSize BitmapImage::size() const
{
    updateSize();
    return m_size;
}

IntSize BitmapImage::sizeRespectingOrientation() const
{
    updateSize();
    return m_sizeRespectingOrientation;
}

Optional<IntPoint> BitmapImage::hotSpot() const
{
    auto result = m_source.hotSpot();
    didDecodeProperties();
    return result;
}

bool BitmapImage::dataChanged(bool allDataReceived)
{
    // Because we're modifying the current frame, clear its (now possibly
    // inaccurate) metadata as well.
#if !PLATFORM(IOS)
    // Clear all partially-decoded frames. For most image formats, there is only
    // one frame, but at least GIF and ICO can have more. With GIFs, the frames
    // come in order and we ask to decode them in order, waiting to request a
    // subsequent frame until the prior one is complete. Given that we clear
    // incomplete frames here, this means there is at most one incomplete frame
    // (even if we use destroyDecodedData() -- since it doesn't reset the
    // metadata), and it is after all the complete frames.
    //
    // With ICOs, on the other hand, we may ask for arbitrary frames at
    // different times (e.g. because we're displaying a higher-resolution image
    // in the content area and using a lower-resolution one for the favicon),
    // and the frames aren't even guaranteed to appear in the file in the same
    // order as in the directory, so an arbitrary number of the frames might be
    // incomplete (if we ask for frames for which we've not yet reached the
    // start of the frame data), and any or none of them might be the particular
    // frame affected by appending new data here. Thus we have to clear all the
    // incomplete frames to be safe.
    unsigned frameBytesCleared = 0;
    for (size_t i = 0; i < m_frames.size(); ++i) {
        // NOTE: Don't call frameIsCompleteAtIndex() here, that will try to
        // decode any uncached (i.e. never-decoded or
        // cleared-on-a-previous-pass) frames!
        unsigned frameBytes = m_frames[i].m_frameBytes;
        if (m_frames[i].m_haveMetadata && !m_frames[i].m_isComplete)
            frameBytesCleared += (m_frames[i].clear(true) ? frameBytes : 0);
    }
    destroyMetadataAndNotify(frameBytesCleared, ClearedSource::No);
#else
    // FIXME: why is this different for iOS?
    int deltaBytes = 0;
    if (!m_frames.isEmpty()) {
        int bytes = m_frames[m_frames.size() - 1].m_frameBytes;
        if (m_frames[m_frames.size() - 1].clear(true)) {
            deltaBytes += bytes;
            deltaBytes += m_decodedPropertiesSize;
            m_decodedPropertiesSize = 0;
        }
    }
    destroyMetadataAndNotify(deltaBytes, ClearedSource::No);
#endif
    
    // Feed all the data we've seen so far to the image decoder.
    m_allDataReceived = allDataReceived;
#if PLATFORM(IOS)
    // FIXME: We should expose a setting to enable/disable progressive loading and make this
    // code conditional on it. Then we can remove the PLATFORM(IOS)-guard.
    static const double chunkLoadIntervals[] = {0, 1, 3, 6, 15};
    double interval = chunkLoadIntervals[std::min(m_progressiveLoadChunkCount, static_cast<uint16_t>(4))];

    bool needsUpdate = false;
    if (currentTime() - m_progressiveLoadChunkTime > interval) { // The first time through, the chunk time will be 0 and the image will get an update.
        needsUpdate = true;
        m_progressiveLoadChunkTime = currentTime();
        ASSERT(m_progressiveLoadChunkCount <= std::numeric_limits<uint16_t>::max());
        ++m_progressiveLoadChunkCount;
    }
    if (needsUpdate || allDataReceived)
        m_source.setData(data(), allDataReceived);
#else
    m_source.setData(data(), allDataReceived);
#endif

    m_haveFrameCount = false;
    m_source.setNeedsUpdateMetadata();
    return isSizeAvailable();
}

String BitmapImage::filenameExtension() const
{
    return m_source.filenameExtension();
}

size_t BitmapImage::frameCount()
{
    if (!m_haveFrameCount) {
        m_frameCount = m_source.frameCount();
        // If decoder is not initialized yet, m_source.frameCount() returns 0.
        if (m_frameCount) {
            didDecodeProperties();
            m_haveFrameCount = true;
        }
    }
    return m_frameCount;
}

bool BitmapImage::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_source.isSizeAvailable();
    didDecodeProperties();

    return m_sizeAvailable;
}

bool BitmapImage::ensureFrameIsCached(size_t index, ImageFrameCaching frameCaching)
{
    if (index >= frameCount())
        return false;

    if (index >= m_frames.size()
        || (frameCaching == CacheMetadataAndFrame && !m_frames[index].m_image)
        || (frameCaching == CacheMetadataOnly && !m_frames[index].m_haveMetadata))
        cacheFrame(index, 0, frameCaching);

    return true;
}

NativeImagePtr BitmapImage::frameImageAtIndex(size_t index, float presentationScaleHint)
{
    if (index >= frameCount())
        return nullptr;

    SubsamplingLevel subsamplingLevel = m_source.subsamplingLevelForScale(presentationScaleHint);
    
    LOG(Images, "BitmapImage %p frameImageAtIndex - subsamplingLevel %d at scale %.4f", this, subsamplingLevel, presentationScaleHint);

    // We may have cached a frame with a higher subsampling level, in which case we need to
    // re-decode with a lower level.
    if (index < m_frames.size() && m_frames[index].m_image && subsamplingLevel < m_frames[index].m_subsamplingLevel) {
        LOG(Images, "  subsamplingLevel was %d, resampling", m_frames[index].m_subsamplingLevel);

        // If the image is already cached, but at too small a size, re-decode a larger version.
        unsigned sizeChange = m_frames[index].m_frameBytes;
        m_frames[index].clear(true);
        invalidatePlatformData();
        if (sizeChange) {
            ASSERT(m_decodedSize >= sizeChange);
            m_decodedSize -= sizeChange;
            if (imageObserver())
                imageObserver()->decodedSizeChanged(this, -static_cast<long long>(sizeChange));
        }
    }

    // If we haven't fetched a frame yet, do so.
    if (index >= m_frames.size() || !m_frames[index].m_image)
        cacheFrame(index, subsamplingLevel, CacheMetadataAndFrame);

    return m_frames[index].m_image;
}

bool BitmapImage::frameIsCompleteAtIndex(size_t index)
{
    if (!ensureFrameIsCached(index, CacheMetadataOnly))
        return false;

    return m_frames[index].m_isComplete;
}

float BitmapImage::frameDurationAtIndex(size_t index)
{
    if (!ensureFrameIsCached(index, CacheMetadataOnly))
        return 0;

    return m_frames[index].m_duration;
}

NativeImagePtr BitmapImage::nativeImageForCurrentFrame()
{
    return frameImageAtIndex(currentFrame());
}

bool BitmapImage::frameHasAlphaAtIndex(size_t index)
{
    if (!ensureFrameIsCached(index, CacheMetadataOnly))
        return true;

    if (m_frames[index].m_haveMetadata)
        return m_frames[index].m_hasAlpha;

    return m_source.frameHasAlphaAtIndex(index);
}

bool BitmapImage::currentFrameKnownToBeOpaque()
{
    return !frameHasAlphaAtIndex(currentFrame());
}

ImageOrientation BitmapImage::frameOrientationAtIndex(size_t index)
{
    if (!ensureFrameIsCached(index, CacheMetadataOnly))
        return ImageOrientation();

    if (m_frames[index].m_haveMetadata)
        return m_frames[index].m_orientation;

    return m_source.orientationAtIndex(index);
}

#if !ASSERT_DISABLED
bool BitmapImage::notSolidColor()
{
    return size().width() != 1 || size().height() != 1 || frameCount() > 1;
}
#endif

int BitmapImage::repetitionCount(bool imageKnownToBeComplete)
{
    if ((m_repetitionCountStatus == Unknown) || ((m_repetitionCountStatus == Uncertain) && imageKnownToBeComplete)) {
        // Snag the repetition count. If |imageKnownToBeComplete| is false, the
        // repetition count may not be accurate yet for GIFs; in this case the
        // decoder will default to cAnimationLoopOnce, and we'll try and read
        // the count again once the whole image is decoded.
        m_repetitionCount = m_source.repetitionCount();
        didDecodeProperties();
        m_repetitionCountStatus = (imageKnownToBeComplete || m_repetitionCount == cAnimationNone) ? Certain : Uncertain;
    }
    return m_repetitionCount;
}

bool BitmapImage::shouldAnimate()
{
    return (repetitionCount(false) != cAnimationNone && !m_animationFinished && imageObserver());
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
    if (!m_allDataReceived && !frameIsCompleteAtIndex(nextFrame))
        return;

    // Don't advance past the last frame if we haven't decoded the whole image
    // yet and our repetition count is potentially unset. The repetition count
    // in a GIF can potentially come after all the rest of the image data, so
    // wait on it.
    if (!m_allDataReceived && repetitionCount(false) == cAnimationLoopOnce && m_currentFrame >= (frameCount() - 1))
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

void BitmapImage::drawPattern(GraphicsContext& ctxt, const FloatRect& tileRect, const AffineTransform& transform,
    const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, const FloatRect& destRect, BlendMode blendMode)
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
        // Note that we don't need to special-case cAnimationLoopOnce here
        // because it is 0 (see comments on its declaration in ImageSource.h).
        if (repetitionCount(true) != cAnimationLoopInfinite && m_repetitionsComplete > m_repetitionCount) {
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

Color BitmapImage::singlePixelSolidColor()
{
    // If the image size is not available yet or if the image will be animating don't use the solid color optimization.
    if (frameCount() != 1)
        return Color();
    
    if (m_solidColor)
        return m_solidColor.value();

    // If the frame image is not loaded, first use the decoder to get the size of the image.
    if (!haveFrameImageAtIndex(0) && m_source.frameSizeAtIndex(0, 0) != IntSize(1, 1)) {
        m_solidColor = Color();
        return m_solidColor.value();
    }

    // Cache the frame image. The size will be calculated from the NativeImagePtr.
    if (!ensureFrameIsCached(0))
        return Color();
    
    ASSERT(m_frames.size());
    m_solidColor = NativeImage::singlePixelSolidColor(m_frames[0].m_image.get());
    
    ASSERT(m_solidColor);
    return m_solidColor.value();
}
    
bool BitmapImage::canAnimate()
{
    return shouldAnimate() && frameCount() > 1;
}

void BitmapImage::dump(TextStream& ts) const
{
    Image::dump(ts);

    ts.dumpProperty("type", m_source.filenameExtension());

    if (isAnimated()) {
        ts.dumpProperty("frame-count", m_frameCount);
        ts.dumpProperty("repetitions", m_repetitionCount);
        ts.dumpProperty("current-frame", m_currentFrame);
    }
    
    if (m_solidColor)
        ts.dumpProperty("solid-color", m_solidColor.value());
    
    m_source.dump(ts);
}

}
