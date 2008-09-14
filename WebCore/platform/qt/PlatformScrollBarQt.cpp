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
#include <QMenu>

using namespace std;

static QString tr(const char* text)
{
    return QCoreApplication::translate("QWebPage", text);
}

namespace WebCore {

const double cInitialTimerDelay = 0.25;
const double cNormalTimerDelay = 0.05;

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize size)
    : Scrollbar(client, orientation, size)
{
    QStyle *s = QApplication::style();

    m_opt.state = QStyle::State_Active | QStyle::State_Enabled;
    m_opt.sliderValue = m_opt.sliderPosition = 0;
    m_opt.upsideDown = false;
    setEnabled(true);
    if (size != RegularScrollbar)
        m_opt.state |= QStyle::State_Mini;
    if (orientation == HorizontalScrollbar) {
        m_opt.rect.setHeight(m_theme->scrollbarThickness(size));
        m_opt.orientation = Qt::Horizontal;
        m_opt.state |= QStyle::State_Horizontal;
    } else {
        m_opt.rect.setWidth(m_theme->scrollbarThickness(size));
        m_opt.orientation = Qt::Vertical;
        m_opt.state &= ~QStyle::State_Horizontal;
    }
}

PlatformScrollbar::~PlatformScrollbar()
{
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

static QStyle::SubControl scPart(const ScrollbarPart& part)
{
    switch (part) {
        case NoPart:
            return QStyle::SC_None;
        case BackButtonPart:
            return QStyle::SC_ScrollBarSubLine;
        case BackTrackPart:
            return QStyle::SC_ScrollBarSubPage;
        case ThumbPart:
            return QStyle::SC_ScrollBarSlider;
        case ForwardTrackPart:
            return QStyle::SC_ScrollBarAddPage;
        case ForwardButtonPart:
            return QStyle::SC_ScrollBarAddLine;
    }
    
    return QStyle::SC_None;
}

static ScrollbarPart scrollbarPart(const QStyle::SubControl& sc)
{
    switch (sc) {
        case QStyle::SC_None:
            return NoPart;
        case QStyle::SC_ScrollBarSubLine:
            return BackButtonPart;
        case QStyle::SC_ScrollBarSubPage:
            return BackTrackPart;
        case QStyle::SC_ScrollBarSlider:
            return ThumbPart;
        case QStyle::SC_ScrollBarAddPage:
            return ForwardTrackPart;
        case QStyle::SC_ScrollBarAddLine:
            return ForwardButtonPart;
    }
    
    return NoPart;
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
        m_opt.rect.setHeight(m_theme->scrollbarThickness(controlSize()));
        m_opt.state |= QStyle::State_Horizontal;
    } else {
        m_opt.rect.setWidth(m_theme->scrollbarThickness(controlSize()));
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
    if (m_pressedPart != NoPart)
        m_opt.activeSubControls = scPart(m_pressedPart);
    else
        m_opt.activeSubControls = scPart(m_hoveredPart);

    const QPoint topLeft = m_opt.rect.topLeft();
#ifdef Q_WS_MAC
    QApplication::style()->drawComplexControl(QStyle::CC_ScrollBar, &m_opt, p, 0);
#else
    p->translate(topLeft);
    m_opt.rect.moveTo(QPoint(0, 0));

    // The QStyle expects the background to be already filled
    p->fillRect(m_opt.rect, m_opt.palette.background());

    QApplication::style()->drawComplexControl(QStyle::CC_ScrollBar, &m_opt, p, 0);
    m_opt.rect.moveTo(topLeft);
#endif
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

int PlatformScrollbar::pixelPosToRangeValue(int pos) const
{
    int thumbLen = thumbLength();

    IntRect track = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, &m_opt,
                                                          QStyle::SC_ScrollBarGroove, 0);
    int thumbMin, thumbMax;
    if (m_orientation == HorizontalScrollbar) {
        thumbMin = track.x();
        thumbMax = track.right() - thumbLen + 1;
    } else {
        thumbMin = track.y();
        thumbMax = track.bottom() - thumbLen + 1;
    }

    return  QStyle::sliderValueFromPosition(0, m_totalSize - m_visibleSize, pos - thumbMin,
                                            thumbMax - thumbMin, m_opt.upsideDown);
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

    if (m_pressedPart == ThumbPart) {
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

    if (m_pressedPart != NoPart)
        m_pressedPos = m_orientation == HorizontalScrollbar ? pos.x() : pos.y();

    ScrollbarPart part = scrollbarPart(sc);
    if (part != m_hoveredPart) {
        if (m_pressedPart != NoPart) {
            if (part == m_pressedPart) {
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
        m_hoveredPart = part;
    }

    return true;
}

bool PlatformScrollbar::handleMouseOutEvent(const PlatformMouseEvent& evt)
{
    m_opt.state &= ~QStyle::State_MouseOver;
    m_opt.state &= ~QStyle::State_Sunken;
    m_hoveredPart = NoPart;
    invalidate();
    return true;
}

bool PlatformScrollbar::handleMousePressEvent(const PlatformMouseEvent& evt)
{
    // Early exit for right click
    if (evt.button() == RightButton)
        return true; // Handled as context menu

    const QPoint pos = convertFromContainingWindow(evt.pos());
    bool midButtonAbsPos = QApplication::style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition);

    // Middle click centers slider thumb, if supported
    if (midButtonAbsPos && evt.button() == MiddleButton) {
        setValue(pixelPosToRangeValue((m_orientation == HorizontalScrollbar ?
                                        pos.x() : pos.y()) - thumbLength() / 2));

    } else { // Left button, or if middle click centering is not supported
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
                m_pressedPart = scrollbarPart(sc);
                break;
            default:
                m_pressedPart = NoPart;
                return false;
        }
        m_pressedPos = m_orientation == HorizontalScrollbar ? pos.x() : pos.y();
        autoscrollPressedPart(cInitialTimerDelay);
        invalidate();
    }

    return true;
}

bool PlatformScrollbar::handleMouseReleaseEvent(const PlatformMouseEvent& evt)
{
    const QPoint pos = convertFromContainingWindow(evt.pos());
    const QPoint topLeft = m_opt.rect.topLeft();
    m_opt.rect.moveTo(QPoint(0, 0));
    QStyle::SubControl scAtMousePoint = QApplication::style()->hitTestComplexControl(QStyle::CC_ScrollBar, &m_opt, pos, 0);
    m_opt.rect.moveTo(topLeft);

    m_hoveredPart = scrollbarPart(scAtMousePoint);
    if (m_hoveredPart == NoPart)
        m_opt.state &= ~QStyle::State_MouseOver;

    m_opt.state &= ~QStyle::State_Sunken;
    m_pressedPart = NoPart;
    m_pressedPos = 0;
    stopTimerIfNeeded();
    invalidate();
    return true;
}

bool PlatformScrollbar::handleContextMenuEvent(const PlatformMouseEvent& event)
{
#ifndef QT_NO_CONTEXTMENU
    bool horizontal = (m_orientation == HorizontalScrollbar);

    QMenu menu;
    QAction* actScrollHere = menu.addAction(tr("Scroll here"));
    menu.addSeparator();

    QAction* actScrollTop = menu.addAction(horizontal ? tr("Left edge") : tr("Top"));
    QAction* actScrollBottom = menu.addAction(horizontal ? tr("Right edge") : tr("Bottom"));
    menu.addSeparator();

    QAction* actPageUp = menu.addAction(horizontal ? tr("Page left") : tr("Page up"));
    QAction* actPageDown = menu.addAction(horizontal ? tr("Page right") : tr("Page down"));
    menu.addSeparator();

    QAction* actScrollUp = menu.addAction(horizontal ? tr("Scroll left") : tr("Scroll up"));
    QAction* actScrollDown = menu.addAction(horizontal ? tr("Scroll right") : tr("Scroll down"));

    const QPoint globalPos = QPoint(event.globalX(), event.globalY());
    QAction* actionSelected = menu.exec(globalPos);

    if (actionSelected == 0)
        /* Do nothing */ ;
    else if (actionSelected == actScrollHere) {
        const QPoint pos = convertFromContainingWindow(event.pos());
        setValue(pixelPosToRangeValue(horizontal ? pos.x() : pos.y()));
    } else if (actionSelected == actScrollTop)
        setValue(0);
    else if (actionSelected == actScrollBottom)
        setValue(m_totalSize - m_visibleSize);
    else if (actionSelected == actPageUp)
        scroll(horizontal ? ScrollLeft: ScrollUp, ScrollByPage, 1);
    else if (actionSelected == actPageDown)
        scroll(horizontal ? ScrollRight : ScrollDown, ScrollByPage, 1);
    else if (actionSelected == actScrollUp)
        scroll(horizontal ? ScrollLeft : ScrollUp, ScrollByLine, 1);
    else if (actionSelected == actScrollDown)
        scroll(horizontal ? ScrollRight : ScrollDown, ScrollByLine, 1);
#endif // QT_NO_CONTEXTMENU
    return true;
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
