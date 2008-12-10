/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "ImageObserver.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "SystemTime.h"
#include "Timer.h"
#include <wtf/Vector.h>
#include "MIMETypeRegistry.h"

namespace WebCore {

// Animated images >5MB are considered large enough that we'll only hang on to
// one frame at a time.
const unsigned cLargeAnimationCutoff = 5242880;

// When an animated image is more than five minutes out of date, don't try to
// resync on repaint, so we don't waste CPU cycles on an edge case the user
// doesn't care about.
const double cAnimationResyncCutoff = 5 * 60;

BitmapImage::BitmapImage(ImageObserver* observer)
    : Image(observer)
    , m_currentFrame(0)
    , m_frames(0)
    , m_frameTimer(0)
    , m_repetitionCount(cAnimationNone)
    , m_repetitionCountStatus(Unknown)
    , m_repetitionsComplete(0)
    , m_desiredFrameStartTime(0)
    , m_isSolidColor(false)
    , m_animationFinished(false)
    , m_allDataReceived(false)
    , m_haveSize(false)
    , m_sizeAvailable(false)
    , m_hasUniformFrameSize(true)
    , m_decodedSize(0)
    , m_haveFrameCount(false)
    , m_frameCount(0)
{
    initPlatformData();
}

BitmapImage::~BitmapImage()
{
    invalidatePlatformData();
    stopAnimation();
}

void BitmapImage::destroyDecodedData(bool incremental, bool preserveNearbyFrames)
{
    // Destroy the cached images and release them.
    if (m_frames.size()) {
        int sizeChange = 0;
        int frameSize = m_size.width() * m_size.height() * 4;
        const size_t nextFrame = (preserveNearbyFrames && frameCount()) ? ((m_currentFrame + 1) % frameCount()) : 0;
        for (unsigned i = incremental ? m_frames.size() - 1 : 0; i < m_frames.size(); i++) {
            if (m_frames[i].m_frame && (!preserveNearbyFrames || (i != m_currentFrame && i != nextFrame))) {
                sizeChange -= frameSize;
                m_frames[i].clear();
            }
        }

        // We just always invalidate our platform data, even in the incremental case.
        // This could be better, but it's not a big deal.
        m_isSolidColor = false;
        invalidatePlatformData();
        
        if (sizeChange) {
            m_decodedSize += sizeChange;
            if (imageObserver())
                imageObserver()->decodedSizeChanged(this, sizeChange);
        }
    }
    if (!incremental) {
        // Reset the image source, since Image I/O has an underlying cache that it uses
        // while animating that it seems to never clear.
        m_source.clear();
        if (m_data) 
            m_source.setData(m_data.get(), m_allDataReceived);
    }    
}

void BitmapImage::cacheFrame(size_t index)
{
    size_t numFrames = frameCount();
    ASSERT(m_decodedSize == 0 || numFrames > 1);
    
    if (m_frames.size() < numFrames)
        m_frames.grow(numFrames);

    m_frames[index].m_frame = m_source.createFrameAtIndex(index);
    if (numFrames == 1 && m_frames[index].m_frame)
        checkForSolidColor();

    m_frames[index].m_haveMetadata = true;
    m_frames[index].m_isComplete = m_source.frameIsCompleteAtIndex(index);
    if (repetitionCount(false) != cAnimationNone)
        m_frames[index].m_duration = m_source.frameDurationAtIndex(index);
    m_frames[index].m_hasAlpha = m_source.frameHasAlphaAtIndex(index);

    int sizeChange;
    if (index) {
        IntSize frameSize = m_source.frameSizeAtIndex(index);
        if (frameSize != m_size)
            m_hasUniformFrameSize = false;
        sizeChange = m_frames[index].m_frame ? frameSize.width() * frameSize.height() * 4 : 0;
    } else
        sizeChange = m_frames[index].m_frame ? m_size.width() * m_size.height() * 4 : 0;

    if (sizeChange) {
        m_decodedSize += sizeChange;
        if (imageObserver())
            imageObserver()->decodedSizeChanged(this, sizeChange);
    }
}

IntSize BitmapImage::size() const
{
    if (m_sizeAvailable && !m_haveSize) {
        m_size = m_source.size();
        m_haveSize = true;
    }
    return m_size;
}

IntSize BitmapImage::currentFrameSize() const
{
    if (!m_currentFrame || m_hasUniformFrameSize)
        return size();
    return m_source.frameSizeAtIndex(m_currentFrame);
}

bool BitmapImage::dataChanged(bool allDataReceived)
{
    destroyDecodedData(true);
    
    // Feed all the data we've seen so far to the image decoder.
    m_allDataReceived = allDataReceived;
    m_source.setData(m_data.get(), allDataReceived);
    
    // Clear the frame count.
    m_haveFrameCount = false;

    m_hasUniformFrameSize = true;

    // Image properties will not be available until the first frame of the file
    // reaches kCGImageStatusIncomplete.
    return isSizeAvailable();
}

String BitmapImage::filenameExtension() const
{
    return m_source.filenameExtension();
}

size_t BitmapImage::frameCount()
{
    if (!m_haveFrameCount) {
        m_haveFrameCount = true;
        m_frameCount = m_source.frameCount();
    }
    return m_frameCount;
}

bool BitmapImage::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_source.isSizeAvailable();

    return m_sizeAvailable;
}

NativeImagePtr BitmapImage::frameAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_frame;
}

bool BitmapImage::frameIsCompleteAtIndex(size_t index)
{
    if (index >= frameCount())
        return true;

    if (index >= m_frames.size() || !m_frames[index].m_haveMetadata)
        cacheFrame(index);

    return m_frames[index].m_isComplete;
}

float BitmapImage::frameDurationAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_haveMetadata)
        cacheFrame(index);

    return m_frames[index].m_duration;
}

bool BitmapImage::frameHasAlphaAtIndex(size_t index)
{
    if (index >= frameCount())
        return true;

    if (index >= m_frames.size() || !m_frames[index].m_haveMetadata)
        cacheFrame(index);

    return m_frames[index].m_hasAlpha;
}

int BitmapImage::repetitionCount(bool imageKnownToBeComplete)
{
    if ((m_repetitionCountStatus == Unknown) || ((m_repetitionCountStatus == Uncertain) && imageKnownToBeComplete)) {
        // Snag the repetition count.  If |imageKnownToBeComplete| is false, the
        // repetition count may not be accurate yet for GIFs; in this case the
        // decoder will default to cAnimationLoopOnce, and we'll try and read
        // the count again once the whole image is decoded.
        m_repetitionCount = m_source.repetitionCount();
        m_repetitionCountStatus = (imageKnownToBeComplete || m_repetitionCount == cAnimationNone) ? Certain : Uncertain;
    }
    return m_repetitionCount;
}

bool BitmapImage::shouldAnimate()
{
    return (repetitionCount(false) != cAnimationNone && !m_animationFinished && imageObserver());
}

void BitmapImage::startAnimation(bool catchUpIfNecessary)
{
    if (m_frameTimer || !shouldAnimate() || frameCount() <= 1)
        return;

    // Determine time for next frame to start.  By ignoring paint and timer lag
    // in this calculation, we make the animation appear to run at its desired
    // rate regardless of how fast it's being repainted.
    const double currentDuration = frameDurationAtIndex(m_currentFrame);
    const double time = currentTime();
    if (m_desiredFrameStartTime == 0) {
        m_desiredFrameStartTime = time + currentDuration;
    } else {
        m_desiredFrameStartTime += currentDuration;
        // If we're too far behind, the user probably doesn't care about
        // resyncing and we could burn a lot of time looping through frames
        // below.  Just reset the timings.
        if ((time - m_desiredFrameStartTime) > cAnimationResyncCutoff)
            m_desiredFrameStartTime = time + currentDuration;
    }

    // Don't advance the animation to an incomplete frame.
    size_t nextFrame = (m_currentFrame + 1) % frameCount();
    if (!frameIsCompleteAtIndex(nextFrame))
        return;

    // Don't advance past the last frame if we haven't decoded the whole image
    // yet and our repetition count is potentially unset.  The repetition count
    // in a GIF can potentially come after all the rest of the image data, so
    // wait on it.
    if (!m_allDataReceived && repetitionCount(false) == cAnimationLoopOnce && m_currentFrame >= (frameCount() - 1))
        return;

    // The image may load more slowly than it's supposed to animate, so that by
    // the time we reach the end of the first repetition, we're well behind.
    // Clamp the desired frame start time in this case, so that we don't skip
    // frames (or whole iterations) trying to "catch up".  This is a tradeoff:
    // It guarantees users see the whole animation the second time through and
    // don't miss any repetitions, and is closer to what other browsers do; on
    // the other hand, it makes animations "less accurate" for pages that try to
    // sync an image and some other resource (e.g. audio), especially if users
    // switch tabs (and thus stop drawing the animation, which will pause it)
    // during that initial loop, then switch back later.
    if (nextFrame == 0 && m_repetitionsComplete == 0 && m_desiredFrameStartTime < time)
      m_desiredFrameStartTime = time;

    if (!catchUpIfNecessary || time < m_desiredFrameStartTime) {
        // Haven't yet reached time for next frame to start; delay until then.
        m_frameTimer = new Timer<BitmapImage>(this, &BitmapImage::advanceAnimation);
        m_frameTimer->startOneShot(std::max(m_desiredFrameStartTime - time, 0.));
    } else {
        // We've already reached or passed the time for the next frame to start.
        // See if we've also passed the time for frames after that to start, in
        // case we need to skip some frames entirely.  Remember not to advance
        // to an incomplete frame.
        for (size_t frameAfterNext = (nextFrame + 1) % frameCount(); frameIsCompleteAtIndex(frameAfterNext); frameAfterNext = (nextFrame + 1) % frameCount()) {
            // Should we skip the next frame?
            double frameAfterNextStartTime = m_desiredFrameStartTime + frameDurationAtIndex(nextFrame);
            if (time < frameAfterNextStartTime)
                break;

            // Yes; skip over it without notifying our observers.
            if (!internalAdvanceAnimation(true))
                return;
            m_desiredFrameStartTime = frameAfterNextStartTime;
            nextFrame = frameAfterNext;
        }

        // Draw the next frame immediately.  Note that m_desiredFrameStartTime
        // may be in the past, meaning the next time through this function we'll
        // kick off the next advancement sooner than this frame's duration would
        // suggest.
        if (internalAdvanceAnimation(false)) {
            // The image region has been marked dirty, but once we return to our
            // caller, draw() will clear it, and nothing will cause the
            // animation to advance again.  We need to start the timer for the
            // next frame running, or the animation can hang.  (Compare this
            // with when advanceAnimation() is called, and the region is dirtied
            // while draw() is not in the callstack, meaning draw() gets called
            // to update the region and thus startAnimation() is reached again.)
            // NOTE: For large images with slow or heavily-loaded systems,
            // throwing away data as we go (see destroyDecodedData()) means we
            // can spend so much time re-decoding data above that by the time we
            // reach here we're behind again.  If we let startAnimation() run
            // the catch-up code again, we can get long delays without painting
            // as we race the timer, or even infinite recursion.  In this
            // situation the best we can do is to simply change frames as fast
            // as possible, so force startAnimation() to set a zero-delay timer
            // and bail out if we're not caught up.
            startAnimation(false);
        }
    }
}

void BitmapImage::stopAnimation()
{
    // This timer is used to animate all occurrences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
    delete m_frameTimer;
    m_frameTimer = 0;
}

void BitmapImage::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_desiredFrameStartTime = 0;
    m_animationFinished = false;
    int frameSize = m_size.width() * m_size.height() * 4;
    
    // For extremely large animations, when the animation is reset, we just throw everything away.
    if (frameCount() * frameSize > cLargeAnimationCutoff)
        destroyDecodedData();
}

void BitmapImage::advanceAnimation(Timer<BitmapImage>* timer)
{
    internalAdvanceAnimation(false);
    // At this point the image region has been marked dirty, and if it's
    // onscreen, we'll soon make a call to draw(), which will call
    // startAnimation() again to keep the animation moving.
}

bool BitmapImage::internalAdvanceAnimation(bool skippingFrames)
{
    // Stop the animation.
    stopAnimation();
    
    // See if anyone is still paying attention to this animation.  If not, we don't
    // advance and will remain suspended at the current frame until the animation is resumed.
    if (!skippingFrames && imageObserver()->shouldPauseAnimation(this))
        return false;

    m_currentFrame++;
    if (m_currentFrame >= frameCount()) {
        ++m_repetitionsComplete;
        // Get the repetition count again.  If we weren't able to get a
        // repetition count before, we should have decoded the whole image by
        // now, so it should now be available.
        if (repetitionCount(true) && m_repetitionsComplete >= m_repetitionCount) {
            m_animationFinished = true;
            m_desiredFrameStartTime = 0;
            m_currentFrame--;
            if (skippingFrames) {
                // Uh oh.  We tried to skip past the end of the animation.  We'd
                // better draw this last frame.
                notifyObserverAndTrimDecodedData();
            }
            return false;
        }
        m_currentFrame = 0;
    }

    if (!skippingFrames)
        notifyObserverAndTrimDecodedData();

    return true;
}

void BitmapImage::notifyObserverAndTrimDecodedData()
{
    // Notify our observer that the animation has advanced.
    imageObserver()->animationAdvanced(this);

    // For large animated images, go ahead and throw away frames as we go to
    // save footprint.
    int frameSize = m_size.width() * m_size.height() * 4;
    if (frameCount() * frameSize > cLargeAnimationCutoff) {
        // Destroy all of our frames and just redecode every time.  We save the
        // current frame since we'll need it in draw() anyway.
        destroyDecodedData(false, true);
    }
}

}
