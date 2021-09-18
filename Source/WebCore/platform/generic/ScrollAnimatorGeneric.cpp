/*
 * Copyright (c) 2016 Igalia S.L.
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
#include "ScrollAnimatorGeneric.h"

#include "ScrollAnimationKinetic.h"
#include "ScrollAnimationSmooth.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"

namespace WebCore {

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return makeUnique<ScrollAnimatorGeneric>(scrollableArea);
}

ScrollAnimatorGeneric::ScrollAnimatorGeneric(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
    , m_kineticAnimation(makeUnique<ScrollAnimationKinetic>(*this))
{
}

ScrollAnimatorGeneric::~ScrollAnimatorGeneric() = default;

bool ScrollAnimatorGeneric::scrollToPositionWithoutAnimation(const FloatPoint& position, ScrollClamping clamping)
{
    m_kineticAnimation->stop();
    return ScrollAnimator::scrollToPositionWithoutAnimation(position, clamping);
}

bool ScrollAnimatorGeneric::handleWheelEvent(const PlatformWheelEvent& event)
{
    m_kineticAnimation->stop();

#if ENABLE(KINETIC_SCROLLING)
    m_kineticAnimation->appendToScrollHistory(event);

    if (event.isEndOfNonMomentumScroll()) {
        m_kineticAnimation->startAnimatedScrollWithInitialVelocity(m_currentPosition, m_kineticAnimation->computeVelocity(), m_scrollableArea.horizontalScrollbar(), m_scrollableArea.verticalScrollbar());
        return true;
    }
    if (event.isTransitioningToMomentumScroll()) {
        m_kineticAnimation->clearScrollHistory();
        m_kineticAnimation->startAnimatedScrollWithInitialVelocity(m_currentPosition, event.swipeVelocity(), m_scrollableArea.horizontalScrollbar(), m_scrollableArea.verticalScrollbar());
        return true;
    }
#endif

    return ScrollAnimator::handleWheelEvent(event);
}

void ScrollAnimatorGeneric::updatePosition(const FloatPoint& position)
{
    FloatSize delta = position - m_currentPosition;
    m_currentPosition = position;
    notifyPositionChanged(delta);
    updateActiveScrollSnapIndexForOffset();
}

// FIXME: Can we just use the base class implementation?
void ScrollAnimatorGeneric::scrollAnimationDidUpdate(ScrollAnimation& animation, const FloatPoint& position)
{
    if (&animation == m_kineticAnimation.get()) {
        // FIXME: Clarify how animations interact. There should never be more than one active at a time.
        m_scrollAnimation->stop();
        updatePosition(position);
        return;
    }

    ScrollAnimator::scrollAnimationDidUpdate(animation, position);
}

} // namespace WebCore
