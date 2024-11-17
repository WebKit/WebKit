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

#pragma once

#include "DecodingOptions.h"
#include "ImageTypes.h"
#include "Timer.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

class BitmapImageSource;
class ImageFrame;

class ImageFrameAnimator {
    WTF_MAKE_TZONE_ALLOCATED(ImageFrameAnimator);
public:
    explicit ImageFrameAnimator(BitmapImageSource&);

    void ref() const;
    void deref() const;

    ~ImageFrameAnimator();

    bool imageFrameDecodeAtIndexHasFinished(unsigned index, ImageAnimatingState, DecodingStatus);

    bool startAnimation(SubsamplingLevel, const DecodingOptions&);
    void advanceAnimation();
    void stopAnimation();
    void resetAnimation();
    bool isAnimating() const { return !!m_frameTimer; }
    bool isAnimationAllowed() const;

    bool hasEverAnimated() const { return !!m_desiredFrameStartTime; }
    unsigned currentFrameIndex() const { return m_currentFrameIndex; }

    void dump(TextStream&) const;

private:
    void destroyDecodedData(bool destroyAll);

    void startTimer(Seconds delay);
    void clearTimer();
    void timerFired();

    unsigned nextFrameIndex() const { return (m_currentFrameIndex + 1) % m_frameCount; }

    const char* sourceUTF8() const;

    ThreadSafeWeakPtr<BitmapImageSource> m_source; // Cannot be null.
    unsigned m_frameCount { 0 };
    RepetitionCount m_repetitionCount { RepetitionCountNone };

    std::unique_ptr<Timer> m_frameTimer;
    SubsamplingLevel m_nextFrameSubsamplingLevel { SubsamplingLevel::Default };
    DecodingOptions m_nextFrameOptions { DecodingMode::Asynchronous };

    unsigned m_currentFrameIndex { 0 };
    RepetitionCount m_repetitionsComplete { RepetitionCountNone };
    MonotonicTime m_desiredFrameStartTime;
};

} // namespace WebCore
