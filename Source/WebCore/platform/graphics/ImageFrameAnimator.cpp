/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "ImageFrameAnimator.h"

#include "BitmapImageSource.h"
#include "Logging.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageFrameAnimator);

std::unique_ptr<ImageFrameAnimator> ImageFrameAnimator::create(BitmapImageSource& source)
{
    return makeUnique<ImageFrameAnimator>(source);
}

ImageFrameAnimator::ImageFrameAnimator(BitmapImageSource& source)
    : m_source(source)
    , m_frameCount(source.frameCount())
    , m_repetitionCount(source.repetitionCount())
    , m_currentFrameIndex(source.primaryFrameIndex())
{
    ASSERT(m_frameCount > 0);
    ASSERT(m_repetitionCount != RepetitionCountNone);
    ASSERT(m_currentFrameIndex < m_frameCount);
}

ImageFrameAnimator::~ImageFrameAnimator()
{
    clearTimer();
}

void ImageFrameAnimator::destroyDecodedData(bool destroyAll)
{
    // Animated images over a certain size are considered large enough that we'll
    // only hang on to one frame at a time.
    static constexpr unsigned LargeAnimationCutoff = 30 * 1024 * 1024;

    if (m_source.decodedSize() < LargeAnimationCutoff)
        return;

    m_source.destroyDecodedData(destroyAll);
}

void ImageFrameAnimator::startTimer(Seconds delay)
{
    ASSERT(!m_frameTimer);
    m_frameTimer = makeUnique<Timer>(*this, &ImageFrameAnimator::timerFired);
    m_frameTimer->startOneShot(delay);
}

void ImageFrameAnimator::clearTimer()
{
    m_frameTimer = nullptr;
}

void ImageFrameAnimator::timerFired()
{
    clearTimer();

    // Don't advance to nextFrame if the next frame is being decoded.
    if (m_source.isPendingDecodingAtIndex(nextFrameIndex(), m_nextFrameSubsamplingLevel, m_nextFrameOptions))
        return;

    advanceAnimation();
    m_source.imageFrameAtIndexAvailable(m_currentFrameIndex, ImageAnimatingState::Yes, m_source.frameDecodingStatusAtIndex(m_currentFrameIndex));
}

bool ImageFrameAnimator::imageFrameDecodeAtIndexHasFinished(unsigned index, ImageAnimatingState animatingState, DecodingStatus decodingStatus)
{
    if (animatingState != ImageAnimatingState::Yes)
        return false;

    if (index != nextFrameIndex())
        return false;

    if (!hasEverAnimated())
        return false;

    // Don't advance to nextFrame if the timer has not fired yet.
    if (isAnimating())
        return true;

    advanceAnimation();
    m_source.imageFrameAtIndexAvailable(m_currentFrameIndex, animatingState, decodingStatus);
    return true;
}

bool ImageFrameAnimator::startAnimation(SubsamplingLevel subsamplingLevel, const DecodingOptions& options)
{
    if (m_frameTimer)
        return true;

    // ImageObserver may disallow animation.
    if (!m_source.isAnimationAllowed())
        return false;

    m_nextFrameSubsamplingLevel = subsamplingLevel;
    m_nextFrameOptions = options;

    LOG(Images, "ImageFrameAnimator::%s - %p - url: %s. Animation at index = %d will be started.", __FUNCTION__, this, sourceUTF8(), m_currentFrameIndex);

    if (options.decodingMode() == DecodingMode::Asynchronous) {
        LOG(Images, "ImageFrameAnimator::%s - %p - url: %s. Decoding for frame at index = %d will be requested.", __FUNCTION__, this, sourceUTF8(), nextFrameIndex());
        m_source.requestNativeImageAtIndexIfNeeded(nextFrameIndex(), subsamplingLevel, ImageAnimatingState::Yes, options);
    }

    auto time = MonotonicTime::now();

    // Handle initial state.
    if (!m_desiredFrameStartTime)
        m_desiredFrameStartTime = time;

    auto duration = m_source.frameDurationAtIndex(m_currentFrameIndex);

    // Setting 'm_desiredFrameStartTime' to 'time' means we are late; otherwise we are early.
    m_desiredFrameStartTime = std::max(time, m_desiredFrameStartTime + duration);

    startTimer(m_desiredFrameStartTime - time);
    return true;
}

void ImageFrameAnimator::advanceAnimation()
{
    LOG(Images, "ImageFrameAnimator::%s - %p - url: %s. Animation at index = %d will be advanced.", __FUNCTION__, this, sourceUTF8(), m_currentFrameIndex);

    m_currentFrameIndex = nextFrameIndex();
    if (m_currentFrameIndex == m_frameCount - 1) {
        LOG(Images, "ImageFrameAnimator::%s - %p - url: %s. Animation loop %d has ended.", __FUNCTION__, this, sourceUTF8(), m_repetitionsComplete);
        ++m_repetitionsComplete;
    }

    destroyDecodedData(false);
}

void ImageFrameAnimator::stopAnimation()
{
    clearTimer();
}

void ImageFrameAnimator::resetAnimation()
{
    stopAnimation();

    m_currentFrameIndex = m_source.primaryFrameIndex();
    m_repetitionsComplete = RepetitionCountNone;
    m_desiredFrameStartTime = { };

    // For extremely large animations, when the animation is reset, we just throw everything away.
    destroyDecodedData(true);
}

bool ImageFrameAnimator::isAnimationAllowed() const
{
    return m_repetitionCount == RepetitionCountInfinite || m_repetitionsComplete < m_repetitionCount;
}

const char* ImageFrameAnimator::sourceUTF8() const
{
    return m_source.sourceUTF8();
}

void ImageFrameAnimator::dump(TextStream& ts) const
{
    ts.dumpProperty("current-frame-index", m_currentFrameIndex);
    ts.dumpProperty("repetitions-complete", m_repetitionsComplete);
}

} // namespace WebCore
