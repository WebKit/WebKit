/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollAnimatorSmooth.h"

#if ENABLE(SMOOTH_SCROLLING)

#include "ScrollAnimationSmooth.h"
#include "ScrollableArea.h"

namespace WebCore {

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    if (scrollableArea.scrollAnimatorEnabled())
        return std::make_unique<ScrollAnimatorSmooth>(scrollableArea);
    return std::make_unique<ScrollAnimator>(scrollableArea);
}

ScrollAnimatorSmooth::ScrollAnimatorSmooth(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
    , m_animation(std::make_unique<ScrollAnimationSmooth>(scrollableArea, [this](FloatPoint&& position) {
        FloatSize delta = position - m_currentPosition;
        m_currentPosition = WTFMove(position);
        notifyPositionChanged(delta);
    }))
{
}

ScrollAnimatorSmooth::~ScrollAnimatorSmooth()
{
}

bool ScrollAnimatorSmooth::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    if (!m_scrollableArea.scrollAnimatorEnabled() || granularity == ScrollByPrecisePixel)
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    return m_animation->scroll(orientation, granularity, step, multiplier);
}

void ScrollAnimatorSmooth::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(offset, toFloatSize(m_scrollableArea.scrollOrigin()));
    m_animation->setCurrentPosition(position);

    FloatSize delta = position - m_currentPosition;
    m_currentPosition = position;
    notifyPositionChanged(delta);
}

#if !USE(REQUEST_ANIMATION_FRAME_TIMER)
void ScrollAnimatorSmooth::cancelAnimations()
{
    m_animation->stop();
}

void ScrollAnimatorSmooth::serviceScrollAnimations()
{
    m_animation->serviceAnimation();
}
#endif

void ScrollAnimatorSmooth::willEndLiveResize()
{
    m_animation->updateVisibleLengths();
}

void ScrollAnimatorSmooth::didAddVerticalScrollbar(Scrollbar*)
{
    m_animation->updateVisibleLengths();
}

void ScrollAnimatorSmooth::didAddHorizontalScrollbar(Scrollbar*)
{
    m_animation->updateVisibleLengths();
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
