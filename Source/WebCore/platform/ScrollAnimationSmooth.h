/*
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScrollAnimation.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FloatPoint;
class TimingFunction;

class ScrollAnimationSmooth final: public ScrollAnimation {
    WTF_MAKE_TZONE_ALLOCATED(ScrollAnimationSmooth);
public:
    ScrollAnimationSmooth(ScrollAnimationClient&);
    virtual ~ScrollAnimationSmooth();

    bool startAnimatedScrollToDestination(const FloatPoint& fromOffset, const FloatPoint& destinationOffset);
    bool retargetActiveAnimation(const FloatPoint& newOffset) final;

    std::optional<FloatPoint> destinationOffset() const final { return m_destinationOffset; }

private:

    void updateScrollExtents() final;
    void serviceAnimation(MonotonicTime) final;
    String debugDescription() const final;

    Seconds durationFromDistance(const FloatSize&) const;

    bool animateScroll(MonotonicTime);

    Seconds m_duration;

    FloatPoint m_startOffset;
    FloatPoint m_destinationOffset;

    RefPtr<TimingFunction> m_timingFunction;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLL_ANIMATION(WebCore::ScrollAnimationSmooth, type() == WebCore::ScrollAnimation::Type::Smooth)
