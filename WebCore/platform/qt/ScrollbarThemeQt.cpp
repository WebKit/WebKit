/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
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
#include "ScrollbarThemeQt.h"

#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QMenu>

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeQt theme;
    return &theme;
}

ScrollbarThemeQt::~ScrollbarThemeQt()
{
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
 
static QStyleOptionSlider* styleOptionSlider(Scrollbar* scrollbar)
{
    static QStyleOptionSlider opt;
    opt.rect = scrollbar->frameGeometry();
    opt.state = 0;
    if (scrollbar->enabled())
        opt.state |= QStyle::State_Enabled;
    if (scrollbar->controlSize() != RegularScrollbar)
        opt.state |= QStyle::State_Mini;
    opt.orientation = (scrollbar->orientation() == VerticalScrollbar) ? Qt::Vertical : Qt::Horizontal;
    opt.sliderValue = scrollbar->value();
    opt.sliderPosition = opt.sliderValue;
    opt.pageStep = scrollbar->visibleSize();
    opt.singleStep = scrollbar->lineStep();
    opt.minimum = 0;
    opt.maximum = qMax(0, scrollbar->maximum());
    ScrollbarPart pressedPart = scrollbar->pressedPart();
    ScrollbarPart hoveredPart = scrollbar->hoveredPart();
    if (pressedPart != NoPart) {
        opt.activeSubControls = scPart(scrollbar->pressedPart());
        if (pressedPart == BackButtonPart || pressedPart == ForwardButtonPart ||
            pressedPart == ThumbPart)
            opt.state |= QStyle::State_Sunken;
    } else
        opt.activeSubControls = scPart(hoveredPart);
    if (hoveredPart != NoPart)
        opt.state |= QStyle::State_MouseOver;
    return &opt;
}

bool ScrollbarThemeQt::paint(Scrollbar* scrollbar, GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    QRect clip = opt->rect.intersected(damageRect);

    QPainter* p = graphicsContext->platformContext();
    p->save();
    p->setClipRect(clip);
    
    const QPoint topLeft = opt->rect.topLeft();
#ifdef Q_WS_MAC
    QApplication::style()->drawComplexControl(QStyle::CC_ScrollBar, opt, p, 0);
#else
    p->translate(topLeft);
    opt->rect.moveTo(QPoint(0, 0));

    // The QStyle expects the background to be already filled
    p->fillRect(opt->rect, opt->palette.background());

    QApplication::style()->drawComplexControl(QStyle::CC_ScrollBar, opt, p, 0);
    opt->rect.moveTo(topLeft);
#endif
    p->restore();
    
    return true;
}

ScrollbarPart ScrollbarThemeQt::hitTest(Scrollbar* scrollbar, const PlatformMouseEvent& evt)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    const QPoint pos = scrollbar->convertFromContainingWindow(evt.pos());
    opt->rect.moveTo(QPoint(0, 0));
    QStyle::SubControl sc = QApplication::style()->hitTestComplexControl(QStyle::CC_ScrollBar, opt, pos, 0);
    return scrollbarPart(sc);
}

bool ScrollbarThemeQt::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    // Middle click centers slider thumb (if supported)
    return QApplication::style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition) && evt.button() == MiddleButton;
}

void ScrollbarThemeQt::invalidatePart(Scrollbar* scrollbar, ScrollbarPart)
{
    // FIXME: Do more precise invalidation.
    scrollbar->invalidate();
}

int ScrollbarThemeQt::scrollbarThickness(ScrollbarControlSize controlSize)
{
    QStyle* s = QApplication::style();
    QStyleOptionSlider o;
    o.orientation = Qt::Vertical;
    o.state &= ~QStyle::State_Horizontal;
    if (controlSize != RegularScrollbar)
        o.state |= QStyle::State_Mini;
    return s->pixelMetric(QStyle::PM_ScrollBarExtent, &o, 0);
}

int ScrollbarThemeQt::thumbPosition(Scrollbar* scrollbar)
{
    if (scrollbar->enabled())
        return (int)((float)scrollbar->currentPos() * (trackLength(scrollbar) - thumbLength(scrollbar)) / scrollbar->maximum());
    return 0;
}

int ScrollbarThemeQt::thumbLength(Scrollbar* scrollbar)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    IntRect thumb = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, opt, QStyle::SC_ScrollBarSlider, 0);
    return scrollbar->orientation() == HorizontalScrollbar ? thumb.width() : thumb.height();
}

int ScrollbarThemeQt::trackPosition(Scrollbar* scrollbar)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    IntRect track = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, opt, QStyle::SC_ScrollBarGroove, 0);
    return scrollbar->orientation() == HorizontalScrollbar ? track.x() - scrollbar.x() : track.y() - scrollbar.y();
}

int ScrollbarThemeQt::trackLength(Scrollbar* scrollbar)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    IntRect track = QApplication::style()->subControlRect(QStyle::CC_ScrollBar, opt, QStyle::SC_ScrollBarGroove, 0);
    return scrollbar->orientation() == HorizontalScrollbar ? track.width() : track.height();
}

}

