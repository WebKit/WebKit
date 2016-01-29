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
#include "ScrollAnimatorGtk.h"

#include "ScrollAnimationSmooth.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const double overflowScrollbarsAnimationDuration = 1;
static const double overflowScrollbarsAnimationHideDelay = 2;

std::unique_ptr<ScrollAnimator> ScrollAnimator::create(ScrollableArea& scrollableArea)
{
    return std::make_unique<ScrollAnimatorGtk>(scrollableArea);
}

ScrollAnimatorGtk::ScrollAnimatorGtk(ScrollableArea& scrollableArea)
    : ScrollAnimator(scrollableArea)
    , m_overlayScrollbarAnimationTimer(*this, &ScrollAnimatorGtk::overlayScrollbarAnimationTimerFired)
{
#if ENABLE(SMOOTH_SCROLLING)
    if (scrollableArea.scrollAnimatorEnabled())
        ensureSmoothScrollingAnimation();
#endif
}

ScrollAnimatorGtk::~ScrollAnimatorGtk()
{
}

#if ENABLE(SMOOTH_SCROLLING)
void ScrollAnimatorGtk::ensureSmoothScrollingAnimation()
{
    if (m_animation)
        return;

    m_animation = std::make_unique<ScrollAnimationSmooth>(m_scrollableArea, [this](FloatPoint&& position) {
        FloatSize delta = position - m_currentPosition;
        m_currentPosition = WTFMove(position);
        notifyPositionChanged(delta);
    });
    m_animation->setCurrentPosition(m_currentPosition);
}

bool ScrollAnimatorGtk::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    if (!m_scrollableArea.scrollAnimatorEnabled() || granularity == ScrollByPrecisePixel)
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    ensureSmoothScrollingAnimation();
    return m_animation->scroll(orientation, granularity, step, multiplier);
}

void ScrollAnimatorGtk::scrollToOffsetWithoutAnimation(const FloatPoint& offset)
{
    FloatPoint position = ScrollableArea::scrollPositionFromOffset(offset, toFloatSize(m_scrollableArea.scrollOrigin()));
    if (m_animation)
        m_animation->setCurrentPosition(position);

    FloatSize delta = position - m_currentPosition;
    m_currentPosition = position;
    notifyPositionChanged(delta);
}

void ScrollAnimatorGtk::willEndLiveResize()
{
    if (m_animation)
        m_animation->updateVisibleLengths();
}
#endif

void ScrollAnimatorGtk::didAddVerticalScrollbar(Scrollbar* scrollbar)
{
#if ENABLE(SMOOTH_SCROLLING)
    if (m_animation)
        m_animation->updateVisibleLengths();
#endif
    if (!scrollbar->isOverlayScrollbar())
        return;
    m_verticalOverlayScrollbar = scrollbar;
    if (!m_horizontalOverlayScrollbar)
        m_overlayScrollbarAnimationCurrent = 1;
    m_verticalOverlayScrollbar->setOpacity(m_overlayScrollbarAnimationCurrent);
    hideOverlayScrollbars();
}

void ScrollAnimatorGtk::didAddHorizontalScrollbar(Scrollbar* scrollbar)
{
#if ENABLE(SMOOTH_SCROLLING)
    if (m_animation)
        m_animation->updateVisibleLengths();
#endif
    if (!scrollbar->isOverlayScrollbar())
        return;
    m_horizontalOverlayScrollbar = scrollbar;
    if (!m_verticalOverlayScrollbar)
        m_overlayScrollbarAnimationCurrent = 1;
    m_horizontalOverlayScrollbar->setOpacity(m_overlayScrollbarAnimationCurrent);
    hideOverlayScrollbars();
}

void ScrollAnimatorGtk::willRemoveVerticalScrollbar(Scrollbar* scrollbar)
{
    if (m_verticalOverlayScrollbar != scrollbar)
        return;
    m_verticalOverlayScrollbar = nullptr;
    if (!m_horizontalOverlayScrollbar)
        m_overlayScrollbarAnimationCurrent = 0;
}

void ScrollAnimatorGtk::willRemoveHorizontalScrollbar(Scrollbar* scrollbar)
{
    if (m_horizontalOverlayScrollbar != scrollbar)
        return;
    m_horizontalOverlayScrollbar = nullptr;
    if (!m_verticalOverlayScrollbar)
        m_overlayScrollbarAnimationCurrent = 0;
}

void ScrollAnimatorGtk::updateOverlayScrollbarsOpacity()
{
    if (m_verticalOverlayScrollbar && m_overlayScrollbarAnimationCurrent != m_verticalOverlayScrollbar->opacity()) {
        m_verticalOverlayScrollbar->setOpacity(m_overlayScrollbarAnimationCurrent);
        if (m_verticalOverlayScrollbar->hoveredPart() == NoPart)
            ScrollbarTheme::theme().invalidatePart(*m_verticalOverlayScrollbar, ThumbPart);
    }

    if (m_horizontalOverlayScrollbar && m_overlayScrollbarAnimationCurrent != m_horizontalOverlayScrollbar->opacity()) {
        m_horizontalOverlayScrollbar->setOpacity(m_overlayScrollbarAnimationCurrent);
        if (m_horizontalOverlayScrollbar->hoveredPart() == NoPart)
            ScrollbarTheme::theme().invalidatePart(*m_horizontalOverlayScrollbar, ThumbPart);
    }
}

static inline double easeOutCubic(double t)
{
    double p = t - 1;
    return p * p * p + 1;
}

void ScrollAnimatorGtk::overlayScrollbarAnimationTimerFired()
{
    if (!m_horizontalOverlayScrollbar && !m_verticalOverlayScrollbar)
        return;
    if (m_overlayScrollbarsLocked)
        return;

    double currentTime = monotonicallyIncreasingTime();
    double progress = 1;
    if (currentTime < m_overlayScrollbarAnimationEndTime)
        progress = (currentTime - m_overlayScrollbarAnimationStartTime) / (m_overlayScrollbarAnimationEndTime - m_overlayScrollbarAnimationStartTime);
    progress = m_overlayScrollbarAnimationSource + (easeOutCubic(progress) * (m_overlayScrollbarAnimationTarget - m_overlayScrollbarAnimationSource));
    if (progress != m_overlayScrollbarAnimationCurrent) {
        m_overlayScrollbarAnimationCurrent = progress;
        updateOverlayScrollbarsOpacity();
    }

    if (m_overlayScrollbarAnimationCurrent != m_overlayScrollbarAnimationTarget) {
        static const double frameRate = 60;
        static const double tickTime = 1 / frameRate;
        static const double minimumTimerInterval = .001;
        double deltaToNextFrame = std::max(tickTime - (monotonicallyIncreasingTime() - currentTime), minimumTimerInterval);
        m_overlayScrollbarAnimationTimer.startOneShot(deltaToNextFrame);
    } else
        hideOverlayScrollbars();
}

void ScrollAnimatorGtk::showOverlayScrollbars()
{
    if (m_overlayScrollbarsLocked)
        return;

    if (m_overlayScrollbarAnimationTimer.isActive() && m_overlayScrollbarAnimationTarget == 1)
        return;
    m_overlayScrollbarAnimationTimer.stop();

    if (!m_horizontalOverlayScrollbar && !m_verticalOverlayScrollbar)
        return;

    m_overlayScrollbarAnimationSource = m_overlayScrollbarAnimationCurrent;
    m_overlayScrollbarAnimationTarget = 1;
    if (m_overlayScrollbarAnimationTarget != m_overlayScrollbarAnimationCurrent) {
        m_overlayScrollbarAnimationStartTime = monotonicallyIncreasingTime();
        m_overlayScrollbarAnimationEndTime = m_overlayScrollbarAnimationStartTime + overflowScrollbarsAnimationDuration;
        m_overlayScrollbarAnimationTimer.startOneShot(0);
    } else
        hideOverlayScrollbars();
}

void ScrollAnimatorGtk::hideOverlayScrollbars()
{
    if (m_overlayScrollbarAnimationTimer.isActive() && !m_overlayScrollbarAnimationTarget)
        return;
    m_overlayScrollbarAnimationTimer.stop();

    if (!m_horizontalOverlayScrollbar && !m_verticalOverlayScrollbar)
        return;

    m_overlayScrollbarAnimationSource = m_overlayScrollbarAnimationCurrent;
    m_overlayScrollbarAnimationTarget = 0;
    if (m_overlayScrollbarAnimationTarget == m_overlayScrollbarAnimationCurrent)
        return;
    m_overlayScrollbarAnimationStartTime = monotonicallyIncreasingTime() + overflowScrollbarsAnimationHideDelay;
    m_overlayScrollbarAnimationEndTime = m_overlayScrollbarAnimationStartTime + overflowScrollbarsAnimationDuration + overflowScrollbarsAnimationHideDelay;
    m_overlayScrollbarAnimationTimer.startOneShot(overflowScrollbarsAnimationHideDelay);
}

void ScrollAnimatorGtk::mouseEnteredContentArea()
{
    showOverlayScrollbars();
}

void ScrollAnimatorGtk::mouseExitedContentArea()
{
    hideOverlayScrollbars();
}

void ScrollAnimatorGtk::mouseMovedInContentArea()
{
    showOverlayScrollbars();
}

void ScrollAnimatorGtk::contentAreaDidShow()
{
    showOverlayScrollbars();
}

void ScrollAnimatorGtk::contentAreaDidHide()
{
    if (m_overlayScrollbarsLocked)
        return;
    m_overlayScrollbarAnimationTimer.stop();
    if (m_overlayScrollbarAnimationCurrent) {
        m_overlayScrollbarAnimationCurrent = 0;
        updateOverlayScrollbarsOpacity();
    }
}

void ScrollAnimatorGtk::notifyContentAreaScrolled(const FloatSize&)
{
    showOverlayScrollbars();
}

void ScrollAnimatorGtk::lockOverlayScrollbarStateToHidden(bool shouldLockState)
{
    if (m_overlayScrollbarsLocked == shouldLockState)
        return;
    m_overlayScrollbarsLocked = shouldLockState;

    if (!m_horizontalOverlayScrollbar && !m_verticalOverlayScrollbar)
        return;

    if (m_overlayScrollbarsLocked) {
        m_overlayScrollbarAnimationTimer.stop();
        if (m_horizontalOverlayScrollbar)
            m_horizontalOverlayScrollbar->setOpacity(0);
        if (m_verticalOverlayScrollbar)
            m_verticalOverlayScrollbar->setOpacity(0);
    } else {
        if (m_overlayScrollbarAnimationCurrent == 1)
            updateOverlayScrollbarsOpacity();
        else
            showOverlayScrollbars();
    }
}

} // namespace WebCore

