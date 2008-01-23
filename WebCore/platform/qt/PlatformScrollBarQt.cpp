/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2007 Trolltech ASA
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

#include "PlatformScrollBar.h"

#include "EventHandler.h"
#include "FrameView.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformMouseEvent.h"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QStyle>

using namespace std;

namespace WebCore {

const double cInitialTimerDelay = 0.25;
const double cNormalTimerDelay = 0.05;

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    : Scrollbar(client, orientation, size)
    , m_pressedPos(0)
    , m_pressedPart(QStyle::SC_None)
    , m_hoveredPart(QStyle::SC_None)
    , m_scrollTimer(this, &PlatformScrollbar::autoscrollTimerFired)
{
    QStyle *s = QApplication::style();

    m_opt.state = QStyle::State_Active | QStyle::State_Enabled;
    m_opt.sliderValue = m_opt.sliderPosition = 0;
    m_opt.upsideDown = false;
    setEnabled(true);
    if (size != RegularScrollbar)
        m_opt.state |= QStyle::State_Mini;
    if (orientation == HorizontalScrollbar) {
        m_opt.rect.setHeight(horizontalScrollbarHeight(size));
        m_opt.orientation = Qt::Horizontal;
        m_opt.state |= QStyle::State_Horizontal;
    } else {
        m_opt.rect.setWidth(verticalScrollbarWidth(size));
        m_opt.orientation = Qt::Vertical;
        m_opt.state &= ~QStyle::State_Horizontal;
    }
}

PlatformScrollbar::~PlatformScrollbar()
{
    stopTimerIfNeeded();
}

void PlatformScrollbar::updateThumbPosition()
{
    invalidate();
}

void PlatformScrollbar::updateThumbProportion()
{
    invalidate();
}

int PlatformScrollbar::width() const
{
    return m_opt.rect.width();
}

int PlatformScrollbar::height() const
{
    return m_opt.rect.height();
}

void PlatformScrollbar::setRect(const IntRect& rect)
{
    setFrameGeometry(rect);
}

IntRect PlatformScrollbar::frameGeometry() const
{
    return m_opt.rect;
}

void PlatformScrollbar::setFrameGeometry(const IntRect& rect)
{
    m_opt.rect = rect;
}

bool PlatformScrollbar::isEnabled() const
{
    return m_opt.state & QStyle::State_Enabled;
}

void PlatformScrollbar::setEnabled(bool enabled)
{
    if (enabled != isEnabled()) {
        if (enabled) {
            m_opt.state |= QStyle::State_Enabled;
        } else {
            m_opt.state &= ~QStyle::State_Enabled;
        }
        invalidate();
    }
}

void PlatformScrollbar::paint(GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    if (controlSize() != RegularScrollbar) {
        m_opt.state |= QStyle::State_Mini;
    } else {
        m_opt.state &= ~QStyle::State_Mini;
    }
    m_opt.orientation = (orientation() == VerticalScrollbar) ? Qt::Vertical : Qt::Horizontal;
    QStyle *s = QApplication::style();
    if (orientation() == HorizontalScrollbar) {
        m_opt.rect.setHeight(horizontalScrollbarHeight(controlSize()));
        m_opt.state |= QStyle::State_Horizontal;
    } else {
        m_opt.rect.setWidth(verticalScrollbarWidth(controlSize()));
        m_opt.state &= ~QStyle::State_Horizontal;
    }

    if (graphicsContext->paintingDisabled() || !m_opt.rect.isValid())
        return;

    QRect clip = m_opt.rect.intersected(damageRect);
    // Don't paint anything if the scrollbar doesn't intersect the damage rect.
    if (clip.isEmpty())
        return;

    QPainter *p = graphicsContext->platformContext();
    p->save();
    p->setClipRect(clip);
    m_opt.sliderValue = value();
    m_opt.sliderPosition = value();
    m_opt.pageStep = m_visibleSize;
    m_opt.singleStep = m_lineStep;
    m_opt.minimum = 0;
    m_opt.maximum = qMax(0, m_totalSize - m_visibleSize);
    if (m_pressedPart != QStyle::SC_None) {
        m_opt.activeSubControls = m_pressedPart;
    } else {
        m_opt.activeSubControls = m_hoveredPart;
    }

    const QPoint topLeft = m_opt.rect.topLeft();
    p->translate(topLeft);
    m_opt.rect.moveTo(QPoint(0, 0));
    QApplication::style()->drawComplexControl(QStyle::CC_ScrollBar, &m_opt, p, 0);
    m_opt.rect.moveTo(topLeft);
    p->restore();
}

int PlatformScrollbar::thumbPosition() const
{
    if (isEnabled())
        return (int)((float)m_currentPos * (trackLength() - thumbLength()) / (m_totalSize - m_visibleSize));
    return 0;
}

int PlatformScrollbar::thumbLength() const
{
    IntRect thumb = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, &m_opt, QStyle::SC_ScrollBarSlider, 0);
    return m_orientation == HorizontalScrollbar ? thumb.width() : thumb.height();
}

int PlatformScrollbar::trackLength() const
{
    IntRect track = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, &m_opt, QStyle::SC_ScrollBarGroove, 0);
    return m_orientation == HorizontalScrollbar ? track.width() : track.height();
}

bool PlatformScrollbar::handleMouseMoveEvent(const PlatformMouseEvent& evt)
{
    const QPoint pos = convertFromContainingWindow(evt.pos());
    //qDebug() << "PlatformScrollbar::handleMouseMoveEvent" << m_opt.rect << pos << evt.pos();

    m_opt.state |= QStyle::State_MouseOver;
    const QPoint topLeft = m_opt.rect.topLeft();
    m_opt.rect.moveTo(QPoint(0, 0));
    QStyle::SubControl sc = QApplication::style()->hitTestComplexControl(QStyle::CC_ScrollBar, &m_opt, pos, 0);
    m_opt.rect.moveTo(topLeft);

    if (sc == m_pressedPart) {
        m_opt.state |= QStyle::State_Sunken;
    } else {
        m_opt.state &= ~QStyle::State_Sunken;
    }

    if (m_pressedPart == QStyle::SC_ScrollBarSlider) {
        // Drag the thumb.
        int thumbPos = thumbPosition();
        int thumbLen = thumbLength();
        int trackLen = trackLength();
        int maxPos = trackLen - thumbLen;
        int delta = 0;
        if (m_orientation == HorizontalScrollbar)
            delta = pos.x() - m_pressedPos;
        else
            delta = pos.y() - m_pressedPos;

        if (delta > 0)
            // The mouse moved down/right.
            delta = min(maxPos - thumbPos, delta);
        else if (delta < 0)
            // The mouse moved up/left.
            delta = max(-thumbPos, delta);

        if (delta != 0) {
            setValue((int)((float)(thumbPos + delta) * (m_totalSize - m_visibleSize) / (trackLen - thumbLen)));
            m_pressedPos += thumbPosition() - thumbPos;
        }
        
        return true;
    }

    if (m_pressedPart != QStyle::SC_None)
        m_pressedPos = m_orientation == HorizontalScrollbar ? pos.x() : pos.y();

    if (sc != m_hoveredPart) {
        if (m_pressedPart != QStyle::SC_None) {
            if (sc == m_pressedPart) {
                // The mouse is moving back over the pressed part.  We
                // need to start up the timer action again.
                startTimerIfNeeded(cNormalTimerDelay);
                invalidate();
            } else if (m_hoveredPart == m_pressedPart) {
                // The mouse is leaving the pressed part.  Kill our timer
                // if needed.
                stopTimerIfNeeded();
                invalidate();
            }
        } else {
            invalidate();
        }
        m_hoveredPart = sc;
    } 

    return true;
}

bool PlatformScrollbar::handleMouseOutEvent(const PlatformMouseEvent& evt)
{
    m_opt.state &= ~QStyle::State_MouseOver;
    m_opt.state &= ~QStyle::State_Sunken;
    invalidate();
    return true;
}

bool PlatformScrollbar::handleMousePressEvent(const PlatformMouseEvent& evt)
{
    const QPoint pos = convertFromContainingWindow(evt.pos());
    //qDebug() << "PlatformScrollbar::handleMousePressEvent" << m_opt.rect << pos << evt.pos();

    const QPoint topLeft = m_opt.rect.topLeft();
    m_opt.rect.moveTo(QPoint(0, 0));
    QStyle::SubControl sc = QApplication::style()->hitTestComplexControl(QStyle::CC_ScrollBar, &m_opt, pos, 0);
    m_opt.rect.moveTo(topLeft);
    switch (sc) {
        case QStyle::SC_ScrollBarAddLine:
        case QStyle::SC_ScrollBarSubLine:
        case QStyle::SC_ScrollBarSlider:
            m_opt.state |= QStyle::State_Sunken;
        case QStyle::SC_ScrollBarAddPage:
        case QStyle::SC_ScrollBarSubPage:
        case QStyle::SC_ScrollBarGroove:
            m_pressedPart = sc;
            break;
        default:
            m_pressedPart = QStyle::SC_None;
            return false;
    }
    m_pressedPos = m_orientation == HorizontalScrollbar ? pos.x() : pos.y();
    autoscrollPressedPart(cInitialTimerDelay);
    invalidate();
    return true;
}

bool PlatformScrollbar::handleMouseReleaseEvent(const PlatformMouseEvent&)
{
    m_opt.state &= ~QStyle::State_Sunken;
    m_pressedPart = QStyle::SC_None;
    m_pressedPos = 0;
    stopTimerIfNeeded();
    invalidate();
    return true;
}

void PlatformScrollbar::startTimerIfNeeded(double delay)
{
    // Don't do anything for the thumb.
    if (m_pressedPart == QStyle::SC_ScrollBarSlider)
        return;

    // Handle the track.  We halt track scrolling once the thumb is level
    // with us.
    if (m_pressedPart == QStyle::SC_ScrollBarGroove && thumbUnderMouse()) {
        invalidate();
        m_hoveredPart = QStyle::SC_ScrollBarSlider;
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

void PlatformScrollbar::stopTimerIfNeeded()
{
    if (m_scrollTimer.isActive())
        m_scrollTimer.stop();
}

void PlatformScrollbar::autoscrollPressedPart(double delay)
{
    // Don't do anything for the thumb or if nothing was pressed.
    if (m_pressedPart == QStyle::SC_ScrollBarSlider || m_pressedPart == QStyle::SC_None)
        return;

    // Handle the track.
    if (m_pressedPart == QStyle::SC_ScrollBarGroove && thumbUnderMouse()) {
        invalidate();
        m_hoveredPart = QStyle::SC_ScrollBarSlider;
        return;
    }

    // Handle the arrows and track.
    if (scroll(pressedPartScrollDirection(), pressedPartScrollGranularity()))
        startTimerIfNeeded(delay);
}

void PlatformScrollbar::autoscrollTimerFired(Timer<PlatformScrollbar>*)
{
    autoscrollPressedPart(cNormalTimerDelay);
}

ScrollDirection PlatformScrollbar::pressedPartScrollDirection()
{
    if (m_orientation == HorizontalScrollbar) {
        if (m_pressedPart == QStyle::SC_ScrollBarSubLine || m_pressedPart == QStyle::SC_ScrollBarSubPage)
            return ScrollLeft;
        return ScrollRight;
    } else {
        if (m_pressedPart == QStyle::SC_ScrollBarSubLine || m_pressedPart == QStyle::SC_ScrollBarSubPage)
            return ScrollUp;
        return ScrollDown;
    }
}

ScrollGranularity PlatformScrollbar::pressedPartScrollGranularity()
{
    if (m_pressedPart == QStyle::SC_ScrollBarSubLine || m_pressedPart == QStyle::SC_ScrollBarAddLine)
        return ScrollByLine;
    return ScrollByPage;
}

bool PlatformScrollbar::thumbUnderMouse()
{
    // Construct a rect.
    IntRect thumb = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, &m_opt, QStyle::SC_ScrollBarSlider, 0);
    thumb.move(-m_opt.rect.x(), -m_opt.rect.y());
    int begin = (m_orientation == HorizontalScrollbar) ? thumb.x() : thumb.y();
    int end = (m_orientation == HorizontalScrollbar) ? thumb.right() : thumb.bottom();
    return (begin <= m_pressedPos && m_pressedPos < end);
}

int PlatformScrollbar::horizontalScrollbarHeight(ScrollbarControlSize controlSize)
{
    QStyle *s = QApplication::style();
    QStyleOptionSlider o;
    o.orientation = Qt::Horizontal;
    o.state |= QStyle::State_Horizontal;
    if (controlSize != RegularScrollbar)
        o.state |= QStyle::State_Mini;
    return s->pixelMetric(QStyle::PM_ScrollBarExtent, &o, 0);
}

int PlatformScrollbar::verticalScrollbarWidth(ScrollbarControlSize controlSize)
{
    QStyle *s = QApplication::style();
    QStyleOptionSlider o;
    o.orientation = Qt::Vertical;
    o.state &= ~QStyle::State_Horizontal;
    if (controlSize != RegularScrollbar)
        o.state |= QStyle::State_Mini;
    return s->pixelMetric(QStyle::PM_ScrollBarExtent, &o, 0);
}

void PlatformScrollbar::invalidate()
{
    // Get the root widget.
    ScrollView* outermostView = topLevel();
    if (!outermostView)
        return;

    IntRect windowRect = convertToContainingWindow(IntRect(0, 0, width(), height()));
    outermostView->addToDirtyRegion(windowRect);
}

}

// vim: ts=4 sw=4 et
