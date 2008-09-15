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
#include "ScrollBar.h"

#include "EventHandler.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "ScrollbarClient.h"
#include "ScrollbarTheme.h"

#include <algorithm>

using std::max;
using std::min;

// FIXME: These constants should come from the ScrollbarTheme.
const double cInitialTimerDelay = 0.25;
const double cNormalTimerDelay = 0.05;

namespace WebCore {

Scrollbar::Scrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize,
                     ScrollbarTheme* theme)
    : m_client(client)
    , m_orientation(orientation)
    , m_controlSize(controlSize)
    , m_theme(theme)
    , m_visibleSize(0)
    , m_totalSize(0)
    , m_currentPos(0)
    , m_lineStep(0)
    , m_pageStep(0)
    , m_pixelStep(1)
    , m_hoveredPart(NoPart)
    , m_pressedPart(NoPart)
    , m_pressedPos(0)
    , m_scrollTimer(this, &Scrollbar::autoscrollTimerFired)
    , m_overlapsResizer(false)
{
    if (!m_theme)
        m_theme = ScrollbarTheme::nativeTheme();
}

Scrollbar::~Scrollbar()
{
    stopTimerIfNeeded();
}

bool Scrollbar::setValue(int v)
{
    v = max(min(v, m_totalSize - m_visibleSize), 0);
    if (value() == v)
        return false; // Our value stayed the same.
    m_currentPos = v;

    updateThumbPosition();

    if (client())
        client()->valueChanged(this);
    
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
    float step = 0;
    if ((direction == ScrollUp && m_orientation == VerticalScrollbar) || (direction == ScrollLeft && m_orientation == HorizontalScrollbar))
        step = -1;
    else if ((direction == ScrollDown && m_orientation == VerticalScrollbar) || (direction == ScrollRight && m_orientation == HorizontalScrollbar)) 
        step = 1;
    
    if (granularity == ScrollByLine)
        step *= m_lineStep;
    else if (granularity == ScrollByPage)
        step *= m_pageStep;
    else if (granularity == ScrollByDocument)
        step *= m_totalSize;
    else if (granularity == ScrollByPixel)
        step *= m_pixelStep;
        
    float newPos = m_currentPos + step * multiplier;
    float maxPos = m_totalSize - m_visibleSize;
    newPos = max(min(newPos, maxPos), 0.0f);

    if (newPos == m_currentPos)
        return false;
    
    int oldValue = value();
    m_currentPos = newPos;
    updateThumbPosition();
    
    if (value() != oldValue && client())
        client()->valueChanged(this);
    
    // return true even if the integer value did not change so that scroll event gets eaten
    return true;
}

void Scrollbar::paint(GraphicsContext* context, const IntRect& damageRect)
{
    if (context->updatingControlTints() && theme()->supportsControlTints()) {
        invalidate();
        return;
    }
        
    if (context->paintingDisabled() || !frameGeometry().intersects(damageRect))
        return;

    if (!theme()->paint(this, context, damageRect))
        Widget::paint(context, damageRect);
}

void Scrollbar::autoscrollTimerFired(Timer<Scrollbar>*)
{
    autoscrollPressedPart(cNormalTimerDelay); // FIXME: Get timer delay from ScrollbarTheme.
}

void Scrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == ThumbPart || m_pressedPart == NoPart)
        return;

    // Handle the track.
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse()) {
        invalidatePart(m_pressedPart);
        m_hoveredPart = ThumbPart;
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
    if ((m_pressedPart == BackTrackPart || m_pressedPart == ForwardTrackPart) && thumbUnderMouse()) {
        invalidatePart(m_pressedPart);
        m_hoveredPart = ThumbPart;
        return;
    }

    // We can't scroll if we've hit the beginning or end.
    ScrollDirection dir = pressedPartScrollDirection();
    if (dir == ScrollUp || dir == ScrollLeft) {
        if (m_currentPos == 0)
            return;
    } else {
        if (m_currentPos == m_totalSize - m_visibleSize)
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
        if (m_pressedPart == BackButtonPart || m_pressedPart == BackTrackPart)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == BackButtonPart || m_pressedPart == BackTrackPart)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity Scrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == BackButtonPart || m_pressedPart == ForwardButtonPart)
        return ScrollByLine;
    return ScrollByPage;
}

bool Scrollbar::handleMouseMoveEvent(const PlatformMouseEvent& evt)
{
    if (m_pressedPart == ThumbPart) {
        // Drag the thumb.
        int thumbPos = theme()->thumbPosition(this);
        int thumbLen = theme()->thumbLength(this);
        int trackLen = theme()->trackLength(this);
        int maxPos = trackLen - thumbLen;
        int delta = 0;
        if (m_orientation == HorizontalScrollbar)
            delta = convertFromContainingWindow(evt.pos()).x() - m_pressedPos;
        else
            delta = convertFromContainingWindow(evt.pos()).y() - m_pressedPos;

        if (delta > 0)
            delta = min(maxPos - thumbPos, delta);
        else if (delta < 0)
            delta = max(-thumbPos, delta);

        if (delta != 0) {
            setValue((float)(thumbPos + delta) * (m_totalSize - m_visibleSize) / (trackLen - thumbLen));
            m_pressedPos += theme()->thumbPosition(this) - thumbPos;
        }

        return true;
    }

    if (m_pressedPart != NoPart)
        m_pressedPos = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());

    ScrollbarPart part = theme()->hitTest(this, evt);    
    if (part != m_hoveredPart) {
        if (m_hoveredPart == NoPart && theme()->invalidateOnMouseEnterExit())
            invalidate();  // Just invalidate the whole scrollbar, since the buttons at either end change anyway.

        if (m_pressedPart != NoPart) {
            if (part == m_pressedPart) {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(cNormalTimerDelay);
                theme()->invalidatePart(this, m_pressedPart);
            } else if (m_hoveredPart == m_pressedPart) {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
                theme()->invalidatePart(this, m_pressedPart);
            }
        } else {
            theme()->invalidatePart(this, part);
            theme()->invalidatePart(this, m_hoveredPart);
        }
        m_hoveredPart = part;
    } 

    return true;
}

bool Scrollbar::handleMouseOutEvent(const PlatformMouseEvent& event)
{
    if (theme()->invalidateOnMouseEnterExit())
        invalidate(); // Just invalidate the whole scrollbar, since the buttons at either end change anyway.
    else
        theme()->invalidatePart(this, m_hoveredPart);
    m_hoveredPart = NoPart;
    return true;
}

bool Scrollbar::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    theme()->invalidatePart(this, m_pressedPart);
    m_pressedPart = NoPart;
    m_pressedPos = 0;
    stopTimerIfNeeded();

    if (parent() && parent()->isFrameView())
        static_cast<FrameView*>(parent())->frame()->eventHandler()->setMousePressed(false);

    return true;
}

bool Scrollbar::handleMousePressEvent(const PlatformMouseEvent& evt)
{
    // Early exit for right click
    if (evt.button() == RightButton)
        return true; // FIXME: Handled as context menu by Qt right now.  Should just avoid even calling this method on a right click though.

    if (theme()->shouldCenterOnThumb(this, evt)) {
        invalidate();
        m_hoveredPart = m_pressedPart = ThumbPart;
        theme()->centerOnThumb(this, evt);
        return true;
    }
    
    m_pressedPart = theme()->hitTest(this, evt);
    m_pressedPos = (orientation() == HorizontalScrollbar ? convertFromContainingWindow(evt.pos()).x() : convertFromContainingWindow(evt.pos()).y());
    theme()->invalidatePart(this, m_pressedPart);
    autoscrollPressedPart(theme()->initialAutoscrollTimerDelay());
    return true;
}

}
