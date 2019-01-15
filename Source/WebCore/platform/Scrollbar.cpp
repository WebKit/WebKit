/*
 * Copyright (C) 2004, 2006, 2008, 2015 Apple Inc. All rights reserved.
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
#include "Scrollbar.h"

#include "FrameView.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "ScrollAnimator.h"
#include "ScrollView.h"
#include "ScrollableArea.h"
#include "ScrollbarTheme.h"
#include <algorithm>

#if PLATFORM(GTK)
// The position of the scrollbar thumb affects the appearance of the steppers, so
// when the thumb moves, we have to invalidate them for painting.
#define THUMB_POSITION_AFFECTS_BUTTONS
#endif

namespace WebCore {

Ref<Scrollbar> Scrollbar::createNativeScrollbar(ScrollableArea& scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize size)
{
    return adoptRef(*new Scrollbar(scrollableArea, orientation, size));
}

int Scrollbar::maxOverlapBetweenPages()
{
    static int maxOverlapBetweenPages = ScrollbarTheme::theme().maxOverlapBetweenPages();
    return maxOverlapBetweenPages;
}

Scrollbar::Scrollbar(ScrollableArea& scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize controlSize, ScrollbarTheme* customTheme)
    : m_scrollableArea(scrollableArea)
    , m_orientation(orientation)
    , m_controlSize(controlSize)
    , m_theme(customTheme ? *customTheme : ScrollbarTheme::theme())
    , m_scrollTimer(*this, &Scrollbar::autoscrollTimerFired)
{
    theme().registerScrollbar(*this);

    // FIXME: This is ugly and would not be necessary if we fix cross-platform code to actually query for
    // scrollbar thickness and use it when sizing scrollbars (rather than leaving one dimension of the scrollbar
    // alone when sizing).
    int thickness = theme().scrollbarThickness(controlSize);
    Widget::setFrameRect(IntRect(0, 0, thickness, thickness));

    m_currentPos = static_cast<float>(m_scrollableArea.scrollOffset(m_orientation));
}

Scrollbar::~Scrollbar()
{
    stopTimerIfNeeded();
    
    theme().unregisterScrollbar(*this);
}

int Scrollbar::occupiedWidth() const
{
    return isOverlayScrollbar() ? 0 : width();
}

int Scrollbar::occupiedHeight() const
{
    return isOverlayScrollbar() ? 0 : height();
}

void Scrollbar::offsetDidChange()
{
    float position = static_cast<float>(m_scrollableArea.scrollOffset(m_orientation));
    if (position == m_currentPos)
        return;

    int oldThumbPosition = theme().thumbPosition(*this);
    m_currentPos = position;
    updateThumbPosition();
    if (m_pressedPart == ThumbPart)
        setPressedPos(m_pressedPos + theme().thumbPosition(*this) - oldThumbPosition);
}

void Scrollbar::setProportion(int visibleSize, int totalSize)
{
    if (visibleSize == m_visibleSize && totalSize == m_totalSize)
        return;

    m_visibleSize = visibleSize;
    m_totalSize = totalSize;

    updateThumbProportion();
}

void Scrollbar::setSteps(int lineStep, int pageStep, int pixelsPerStep)
{
    m_lineStep = lineStep;
    m_pageStep = pageStep;
    m_pixelStep = 1.0f / pixelsPerStep;
}

void Scrollbar::updateThumb()
{
#ifdef THUMB_POSITION_AFFECTS_BUTTONS
    invalidate();
#else
    theme().invalidateParts(*this, ForwardTrackPart | BackTrackPart | ThumbPart);
#endif
}

void Scrollbar::updateThumbPosition()
{
    updateThumb();
}

void Scrollbar::updateThumbProportion()
{
    updateThumb();
}

void Scrollbar::paint(GraphicsContext& context, const IntRect& damageRect, Widget::SecurityOriginPaintPolicy)
{
    if (context.invalidatingControlTints() && theme().supportsControlTints()) {
        invalidate();
        return;
    }

    if (context.paintingDisabled() || !frameRect().intersects(damageRect))
        return;

    if (!theme().paint(*this, context, damageRect))
        Widget::paint(context, damageRect);
}

void Scrollbar::autoscrollTimerFired()
{
    autoscrollPressedPart(theme().autoscrollTimerDelay());
}

static bool thumbUnderMouse(Scrollbar* scrollbar)
{
    int thumbPos = scrollbar->theme().trackPosition(*scrollbar) + scrollbar->theme().thumbPosition(*scrollbar);
    int thumbLength = scrollbar->theme().thumbLength(*scrollbar);
    return scrollbar->pressedPos() >= thumbPos && scrollbar->pressedPos() < thumbPos + thumbLength;
}

void Scrollbar::autoscrollPressedPart(Seconds delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse(this)) {
        theme().invalidatePart(*this, m_pressedPart);
        setHoveredPart(ThumbPart);
        return;
    }

    // Handle the arrows and track.
    if (m_scrollableArea.scroll(pressedPartScrollDirection(), pressedPartScrollGranularity()))
        startTimerIfNeeded(delay);
}

void Scrollbar::startTimerIfNeeded(Seconds delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == ThumbPart)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse(this)) {
        theme().invalidatePart(*this, m_pressedPart);
        setHoveredPart(ThumbPart);
        return;
    }

    // We can't scroll if we've hit the beginning or end.
    ScrollDirection dir = pressedPartScrollDirection();
    if (dir == ScrollUp || dir == ScrollLeft) {
        if (m_currentPos == 0)
            return;
    } else {
        if (m_currentPos == maximum())
            return;
    }

    m_scrollTimer.startOneShot(delay);
}

void Scrollbar::stopTimerIfNeeded()
{
    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

ScrollDirection Scrollbar::pressedPartScrollDirection()
{
    if (m_orientation == HorizontalScrollbar) {
        if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart || m_pressedPart == BackTrackPart)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity Scrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == BackButtonStartPart || m_pressedPart == BackButtonEndPart ||  m_pressedPart == ForwardButtonStartPart || m_pressedPart == ForwardButtonEndPart)
        return ScrollByLine;
    return ScrollByPage;
}

void Scrollbar::moveThumb(int pos, bool draggingDocument)
{
    int delta = pos - m_pressedPos;

    if (draggingDocument) {
        if (m_draggingDocument)
            delta = pos - m_documentDragPos;
        m_draggingDocument = true;
        FloatPoint currentPosition = m_scrollableArea.scrollAnimator().currentPosition();
        int destinationPosition = (m_orientation == HorizontalScrollbar ? currentPosition.x() : currentPosition.y()) + delta;
        if (delta > 0)
            destinationPosition = std::min(destinationPosition + delta, maximum());
        else if (delta < 0)
            destinationPosition = std::max(destinationPosition + delta, 0);
        m_scrollableArea.scrollToOffsetWithoutAnimation(m_orientation, destinationPosition);
        m_documentDragPos = pos;
        return;
    }

    if (m_draggingDocument) {
        delta += m_pressedPos - m_documentDragPos;
        m_draggingDocument = false;
    }

    // Drag the thumb.
    int thumbPos = theme().thumbPosition(*this);
    int thumbLen = theme().thumbLength(*this);
    int trackLen = theme().trackLength(*this);
    int maxPos = trackLen - thumbLen;
    if (delta > 0)
        delta = std::min(maxPos - thumbPos, delta);
    else if (delta < 0)
        delta = std::max(-thumbPos, delta);
    
    if (delta) {
        float newPosition = static_cast<float>(thumbPos + delta) * maximum() / (trackLen - thumbLen);
        m_scrollableArea.scrollToOffsetWithoutAnimation(m_orientation, newPosition);
    }
}

void Scrollbar::setHoveredPart(ScrollbarPart part)
{
    if (part == m_hoveredPart)
        return;

    if ((m_hoveredPart == NoPart || part == NoPart) && theme().invalidateOnMouseEnterExit())
        invalidate();  // Just invalidate the whole scrollbar, since the buttons at either end change anyway.
    else if (m_pressedPart == NoPart) {  // When there's a pressed part, we don't draw a hovered state, so there's no reason to invalidate.
        theme().invalidatePart(*this, part);
        theme().invalidatePart(*this, m_hoveredPart);
    }
    m_hoveredPart = part;
}

void Scrollbar::setPressedPart(ScrollbarPart part)
{
    if (m_pressedPart != NoPart)
        theme().invalidatePart(*this, m_pressedPart);
    m_pressedPart = part;
    if (m_pressedPart != NoPart)
        theme().invalidatePart(*this, m_pressedPart);
    else if (m_hoveredPart != NoPart)  // When we no longer have a pressed part, we can start drawing a hovered state on the hovered part.
        theme().invalidatePart(*this, m_hoveredPart);
}

#if !PLATFORM(IOS_FAMILY)
bool Scrollbar::mouseMoved(const PlatformMouseEvent& evt)
{
    if (m_pressedPart == ThumbPart) {
        if (theme().shouldSnapBackToDragOrigin(*this, evt))
            m_scrollableArea.scrollToOffsetWithoutAnimation(m_orientation, m_dragOrigin);
        else {
            moveThumb(m_orientation == HorizontalScrollbar ? 
                      convertFromContainingWindow(evt.position()).x() :
                      convertFromContainingWindow(evt.position()).y(), theme().shouldDragDocumentInsteadOfThumb(*this, evt));
        }
        return true;
    }

    if (m_pressedPart != NoPart)
        m_pressedPos = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.position()).x() : convertFromContainingWindow(evt.position()).y());

    ScrollbarPart part = theme().hitTest(*this, evt.position());
    if (part != m_hoveredPart) {
        if (m_pressedPart != NoPart) {
            if (part == m_pressedPart) {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(theme().autoscrollTimerDelay());
                theme().invalidatePart(*this, m_pressedPart);
            } else if (m_hoveredPart == m_pressedPart) {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
                theme().invalidatePart(*this, m_pressedPart);
            }
        } 
        
        setHoveredPart(part);
    } 

    return true;
}
#endif

void Scrollbar::mouseEntered()
{
    m_scrollableArea.mouseEnteredScrollbar(this);
}

bool Scrollbar::mouseExited()
{
    m_scrollableArea.mouseExitedScrollbar(this);
    setHoveredPart(NoPart);
    return true;
}

bool Scrollbar::mouseUp(const PlatformMouseEvent& mouseEvent)
{
    setPressedPart(NoPart);
    m_pressedPos = 0;
    m_draggingDocument = false;
    stopTimerIfNeeded();

    m_scrollableArea.mouseIsDownInScrollbar(this, false);

    // m_hoveredPart won't be updated until the next mouseMoved or mouseDown, so we have to hit test
    // to really know if the mouse has exited the scrollbar on a mouseUp.
    ScrollbarPart part = theme().hitTest(*this, mouseEvent.position());
    if (part == NoPart)
        m_scrollableArea.mouseExitedScrollbar(this);

    return true;
}

bool Scrollbar::mouseDown(const PlatformMouseEvent& evt)
{
    ScrollbarPart pressedPart = theme().hitTest(*this, evt.position());
    auto action = theme().handleMousePressEvent(*this, evt, pressedPart);
    if (action == ScrollbarButtonPressAction::None)
        return true;

    m_scrollableArea.mouseIsDownInScrollbar(this, true);
    setPressedPart(pressedPart);

    int pressedPosition = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.position()).x() : convertFromContainingWindow(evt.position()).y());
    if (action == ScrollbarButtonPressAction::CenterOnThumb) {
        setHoveredPart(ThumbPart);
        setPressedPart(ThumbPart);
        m_dragOrigin = m_currentPos;
        // Set the pressed position to the middle of the thumb so that when we do the move, the delta
        // will be from the current pixel position of the thumb to the new desired position for the thumb.
        m_pressedPos = theme().trackPosition(*this) + theme().thumbPosition(*this) + theme().thumbLength(*this) / 2;
        moveThumb(pressedPosition);
        return true;
    }

    m_pressedPos = pressedPosition;

    if (action == ScrollbarButtonPressAction::StartDrag)
        m_dragOrigin = m_currentPos;

    if (action == ScrollbarButtonPressAction::Scroll)
        autoscrollPressedPart(theme().initialAutoscrollTimerDelay());

    return true;
}

void Scrollbar::setEnabled(bool e)
{ 
    if (m_enabled == e)
        return;
    m_enabled = e;
    theme().updateEnabledState(*this);
    invalidate();
}

bool Scrollbar::isOverlayScrollbar() const
{
    return theme().usesOverlayScrollbars();
}

bool Scrollbar::shouldParticipateInHitTesting()
{
    // Non-overlay scrollbars should always participate in hit testing.
    if (!isOverlayScrollbar())
        return true;
    return m_scrollableArea.scrollAnimator().shouldScrollbarParticipateInHitTesting(this);
}

bool Scrollbar::isWindowActive() const
{
    return m_scrollableArea.isActive();
}

void Scrollbar::invalidateRect(const IntRect& rect)
{
    if (suppressInvalidation())
        return;

    m_scrollableArea.invalidateScrollbar(*this, rect);
}

IntRect Scrollbar::convertToContainingView(const IntRect& localRect) const
{
    return m_scrollableArea.convertFromScrollbarToContainingView(*this, localRect);
}

IntRect Scrollbar::convertFromContainingView(const IntRect& parentRect) const
{
    return m_scrollableArea.convertFromContainingViewToScrollbar(*this, parentRect);
}

IntPoint Scrollbar::convertToContainingView(const IntPoint& localPoint) const
{
    return m_scrollableArea.convertFromScrollbarToContainingView(*this, localPoint);
}

IntPoint Scrollbar::convertFromContainingView(const IntPoint& parentPoint) const
{
    return m_scrollableArea.convertFromContainingViewToScrollbar(*this, parentPoint);
}

bool Scrollbar::supportsUpdateOnSecondaryThread() const
{
    // It's unfortunate that this needs to be done with an ifdef. Ideally there would be a way to feature-detect
    // the necessary support within AppKit.
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)
    return !m_scrollableArea.forceUpdateScrollbarsOnMainThreadForPerformanceTesting()
        && (m_scrollableArea.hasLayerForVerticalScrollbar() || m_scrollableArea.hasLayerForHorizontalScrollbar())
        && m_scrollableArea.usesAsyncScrolling();
#else
    return false;
#endif
}

} // namespace WebCore
