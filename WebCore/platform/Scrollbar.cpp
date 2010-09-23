/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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
#include "Scrollbar.h"

#include "AccessibilityScrollbar.h"
#include "AXObjectCache.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "ScrollbarClient.h"
#include "ScrollbarTheme.h"

#include <algorithm>

using namespace std;

#if (PLATFORM(CHROMIUM) && (OS(LINUX) || OS(FREEBSD))) || PLATFORM(GTK)
// The position of the scrollbar thumb affects the appearance of the steppers, so
// when the thumb moves, we have to invalidate them for painting.
#define THUMB_POSITION_AFFECTS_BUTTONS
#endif

namespace WebCore {

#if !PLATFORM(EFL)
PassRefPtr<Scrollbar> Scrollbar::createNativeScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
{
    return adoptRef(new Scrollbar(client, orientation, size));
}
#endif

int Scrollbar::maxOverlapBetweenPages()
{
    static int maxOverlapBetweenPages = ScrollbarTheme::nativeTheme()->maxOverlapBetweenPages();
    return maxOverlapBetweenPages;
}

Scrollbar::Scrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize,
                     ScrollbarTheme* theme)
    : m_client(client)
    , m_orientation(orientation)
    , m_controlSize(controlSize)
    , m_theme(theme)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_dragOrigin(0)
    , m_lineStep(0)
    , m_pageStep(0)
    , m_pixelStep(1)
    , m_hoveredPart(NoPart)
    , m_pressedPart(NoPart)
    , m_pressedPos(0)
    , m_enabled(true)
    , m_scrollTimer(this, &Scrollbar::autoscrollTimerFired)
    , m_overlapsResizer(false)
    , m_suppressInvalidation(false)
{
    if (!m_theme)
        m_theme = ScrollbarTheme::nativeTheme();

    m_theme->registerScrollbar(this);

    // FIXME: This is ugly and would not be necessary if we fix cross-platform code to actually query for
    // scrollbar thickness and use it when sizing scrollbars (rather than leaving one dimension of the scrollbar
    // alone when sizing).
    int thickness = m_theme->scrollbarThickness(controlSize);
    Widget::setFrameRect(IntRect(0, 0, thickness, thickness));
}

Scrollbar::~Scrollbar()
{
    stopTimerIfNeeded();
    
    m_theme->unregisterScrollbar(this);
}

bool Scrollbar::setValue(int v, ScrollSource source)
{
    v = max(min(v, m_totalSize - m_visibleSize), 0);
    if (value() == v)
        return false; // Our value stayed the same.
    setCurrentPos(v, source);
    return true;
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

bool Scrollbar::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
#if HAVE(ACCESSIBILITY)
    if (AXObjectCache::accessibilityEnabled()) {
        if (parent() && parent()->isFrameView()) {
            Document* document = static_cast<FrameView*>(parent())->frame()->document();
            AXObjectCache* cache = document->axObjectCache();
            AccessibilityScrollbar* axObject = static_cast<AccessibilityScrollbar*>(cache->getOrCreate(ScrollBarRole));
            axObject->setScrollbar(this);
            cache->postNotification(axObject, document, AXObjectCache::AXValueChanged, true);
        }
    }
#endif

    // Ignore perpendicular scrolls.
    if ((m_orientation == HorizontalScrollbar) ? (direction == ScrollUp || direction == ScrollDown) : (direction == ScrollLeft || direction == ScrollRight))
        return false;
    float step = 0;
    switch (granularity) {
    case ScrollByLine:     step = m_lineStep;  break;
    case ScrollByPage:     step = m_pageStep;  break;
    case ScrollByDocument: step = m_totalSize; break;
    case ScrollByPixel:    step = m_pixelStep; break;
    }
    if (direction == ScrollUp || direction == ScrollLeft)
        multiplier = -multiplier;
    if (client())
        return client()->scroll(m_orientation, granularity, step, multiplier);

    return setCurrentPos(max(min(m_currentPos + (step * multiplier), static_cast<float>(m_totalSize - m_visibleSize)), 0.0f), NotFromScrollAnimator);
}

void Scrollbar::updateThumb()
{
#ifdef THUMB_POSITION_AFFECTS_BUTTONS
    invalidate();
#else
    theme()->invalidateParts(this, ForwardTrackPart | BackTrackPart | ThumbPart);
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

void Scrollbar::paint(GraphicsContext* context, const IntRect& damageRect)
{
    if (context->updatingControlTints() && theme()->supportsControlTints()) {
        invalidate();
        return;
    }

    if (context->paintingDisabled() || !frameRect().intersects(damageRect))
        return;

    if (!theme()->paint(this, context, damageRect))
        Widget::paint(context, damageRect);
}

void Scrollbar::autoscrollTimerFired(Timer<Scrollbar>*)
{
    autoscrollPressedPart(theme()->autoscrollTimerDelay());
}

static bool thumbUnderMouse(Scrollbar* scrollbar)
{
    int thumbPos = scrollbar->theme()->trackPosition(scrollbar) + scrollbar->theme()->thumbPosition(scrollbar);
    int thumbLength = scrollbar->theme()->thumbLength(scrollbar);
    return scrollbar->pressedPos() >= thumbPos && scrollbar->pressedPos() < thumbPos + thumbLength;
}

void Scrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse(this)) {
        theme()->invalidatePart(this, m_pressedPart);
        setHoveredPart(ThumbPart);
        return;
    }

    // Handle the arrows and track.
    if (scroll(pressedPartScrollDirection(), pressedPartScrollGranularity()))
        startTimerIfNeeded(delay);
}

void Scrollbar::startTimerIfNeeded(double delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == ThumbPart)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse(this)) {
        theme()->invalidatePart(this, m_pressedPart);
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

void Scrollbar::moveThumb(int pos)
{
    // Drag the thumb.
    int thumbPos = theme()->thumbPosition(this);
    int thumbLen = theme()->thumbLength(this);
    int trackLen = theme()->trackLength(this);
    int maxPos = trackLen - thumbLen;
    int delta = pos - m_pressedPos;
    if (delta > 0)
        delta = min(maxPos - thumbPos, delta);
    else if (delta < 0)
        delta = max(-thumbPos, delta);
    if (delta)
        setCurrentPos(static_cast<float>(thumbPos + delta) * maximum() / (trackLen - thumbLen), NotFromScrollAnimator);
}

bool Scrollbar::setCurrentPos(float pos, ScrollSource source)
{
    if ((source != FromScrollAnimator) && client())
        client()->setScrollPositionAndStopAnimation(m_orientation, pos);

    if (pos == m_currentPos)
        return false;

    int oldValue = value();
    int oldThumbPos = theme()->thumbPosition(this);
    m_currentPos = pos;
    updateThumbPosition();
    if (m_pressedPart == ThumbPart)
        setPressedPos(m_pressedPos + theme()->thumbPosition(this) - oldThumbPos);

    if (value() != oldValue && client())
        client()->valueChanged(this);
    return true;
}

void Scrollbar::setHoveredPart(ScrollbarPart part)
{
    if (part == m_hoveredPart)
        return;

    if ((m_hoveredPart == NoPart || part == NoPart) && theme()->invalidateOnMouseEnterExit())
        invalidate();  // Just invalidate the whole scrollbar, since the buttons at either end change anyway.
    else if (m_pressedPart == NoPart) {  // When there's a pressed part, we don't draw a hovered state, so there's no reason to invalidate.
        theme()->invalidatePart(this, part);
        theme()->invalidatePart(this, m_hoveredPart);
    }
    m_hoveredPart = part;
}

void Scrollbar::setPressedPart(ScrollbarPart part)
{
    if (m_pressedPart != NoPart)
        theme()->invalidatePart(this, m_pressedPart);
    m_pressedPart = part;
    if (m_pressedPart != NoPart)
        theme()->invalidatePart(this, m_pressedPart);
    else if (m_hoveredPart != NoPart)  // When we no longer have a pressed part, we can start drawing a hovered state on the hovered part.
        theme()->invalidatePart(this, m_hoveredPart);
}

bool Scrollbar::mouseMoved(const PlatformMouseEvent& evt)
{
    if (m_pressedPart == ThumbPart) {
        if (theme()->shouldSnapBackToDragOrigin(this, evt))
            setCurrentPos(m_dragOrigin, NotFromScrollAnimator);
        else {
            moveThumb(m_orientation == HorizontalScrollbar ? 
                      convertFromContainingWindow(evt.pos()).x() :
                      convertFromContainingWindow(evt.pos()).y());
        }
        return true;
    }

    if (m_pressedPart != NoPart)
        m_pressedPos = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());

    ScrollbarPart part = theme()->hitTest(this, evt);    
    if (part != m_hoveredPart) {
        if (m_pressedPart != NoPart) {
            if (part == m_pressedPart) {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(theme()->autoscrollTimerDelay());
                theme()->invalidatePart(this, m_pressedPart);
            } else if (m_hoveredPart == m_pressedPart) {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
                theme()->invalidatePart(this, m_pressedPart);
            }
        } 
        
        setHoveredPart(part);
    } 

    return true;
}

bool Scrollbar::mouseExited()
{
    setHoveredPart(NoPart);
    return true;
}

bool Scrollbar::mouseUp()
{
    setPressedPart(NoPart);
    m_pressedPos = 0;
    stopTimerIfNeeded();

    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->frame()->eventHandler()->setMousePressed(false);

    return true;
}

bool Scrollbar::mouseDown(const PlatformMouseEvent& evt)
{
    // Early exit for right click
    if (evt.button() == RightButton)
        return true; // FIXME: Handled as context menu by Qt right now.  Should just avoid even calling this method on a right click though.

    setPressedPart(theme()->hitTest(this, evt));
    int pressedPos = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());
    
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && theme()->shouldCenterOnThumb(this, evt)) {
        setHoveredPart(ThumbPart);
        setPressedPart(ThumbPart);
        m_dragOrigin = m_currentPos;
        int thumbLen = theme()->thumbLength(this);
        int desiredPos = pressedPos;
        // Set the pressed position to the middle of the thumb so that when we do the move, the delta
        // will be from the current pixel position of the thumb to the new desired position for the thumb.
        m_pressedPos = theme()->trackPosition(this) + theme()->thumbPosition(this) + thumbLen / 2;
        moveThumb(desiredPos);
        return true;
    } else if (m_pressedPart == ThumbPart)
        m_dragOrigin = m_currentPos;
    
    m_pressedPos = pressedPos;

    autoscrollPressedPart(theme()->initialAutoscrollTimerDelay());
    return true;
}

void Scrollbar::setFrameRect(const IntRect& rect)
{
    // Get our window resizer rect and see if we overlap.  Adjust to avoid the overlap
    // if necessary.
    IntRect adjustedRect(rect);
    bool overlapsResizer = false;
    ScrollView* view = parent();
    if (view && !rect.isEmpty() && !view->windowResizerRect().isEmpty()) {
        IntRect resizerRect = view->convertFromContainingWindow(view->windowResizerRect());
        if (rect.intersects(resizerRect)) {
            if (orientation() == HorizontalScrollbar) {
                int overlap = rect.right() - resizerRect.x();
                if (overlap > 0 && resizerRect.right() >= rect.right()) {
                    adjustedRect.setWidth(rect.width() - overlap);
                    overlapsResizer = true;
                }
            } else {
                int overlap = rect.bottom() - resizerRect.y();
                if (overlap > 0 && resizerRect.bottom() >= rect.bottom()) {
                    adjustedRect.setHeight(rect.height() - overlap);
                    overlapsResizer = true;
                }
            }
        }
    }
    if (overlapsResizer != m_overlapsResizer) {
        m_overlapsResizer = overlapsResizer;
        if (view)
            view->adjustScrollbarsAvoidingResizerCount(m_overlapsResizer ? 1 : -1);
    }

    Widget::setFrameRect(adjustedRect);
}

void Scrollbar::setParent(ScrollView* parentView)
{
    if (!parentView && m_overlapsResizer && parent())
        parent()->adjustScrollbarsAvoidingResizerCount(-1);
    Widget::setParent(parentView);
}

void Scrollbar::setEnabled(bool e)
{ 
    if (m_enabled == e)
        return;
    m_enabled = e;
    invalidate();
}

bool Scrollbar::isWindowActive() const
{
    return m_client && m_client->isActive();
}
 
void Scrollbar::invalidateRect(const IntRect& rect)
{
    if (suppressInvalidation())
        return;
    if (m_client)
        m_client->invalidateScrollbarRect(this, rect);
}

IntRect Scrollbar::convertToContainingView(const IntRect& localRect) const
{
    if (m_client)
        return m_client->convertFromScrollbarToContainingView(this, localRect);

    return Widget::convertToContainingView(localRect);
}

IntRect Scrollbar::convertFromContainingView(const IntRect& parentRect) const
{
    if (m_client)
        return m_client->convertFromContainingViewToScrollbar(this, parentRect);

    return Widget::convertFromContainingView(parentRect);
}

IntPoint Scrollbar::convertToContainingView(const IntPoint& localPoint) const
{
    if (m_client)
        return m_client->convertFromScrollbarToContainingView(this, localPoint);

    return Widget::convertToContainingView(localPoint);
}

IntPoint Scrollbar::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (m_client)
        return m_client->convertFromContainingViewToScrollbar(this, parentPoint);

    return Widget::convertFromContainingView(parentPoint);
}

}
