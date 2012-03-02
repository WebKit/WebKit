/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "ScrollbarThemeQStyle.h"

#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "RenderThemeQStyle.h"
#include "RenderThemeQtMobile.h"
#include "ScrollView.h"
#include "Scrollbar.h"

#include <QApplication>
#ifdef Q_WS_MAC
#include <QMacStyle>
#endif
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

namespace WebCore {

ScrollbarThemeQStyle::~ScrollbarThemeQStyle()
{
}

static QStyle::SubControl scPart(const ScrollbarPart& part)
{
    switch (part) {
    case NoPart:
        return QStyle::SC_None;
    case BackButtonStartPart:
    case BackButtonEndPart:
        return QStyle::SC_ScrollBarSubLine;
    case BackTrackPart:
        return QStyle::SC_ScrollBarSubPage;
    case ThumbPart:
        return QStyle::SC_ScrollBarSlider;
    case ForwardTrackPart:
        return QStyle::SC_ScrollBarAddPage;
    case ForwardButtonStartPart:
    case ForwardButtonEndPart:
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
        return BackButtonStartPart;
    case QStyle::SC_ScrollBarSubPage:
        return BackTrackPart;
    case QStyle::SC_ScrollBarSlider:
        return ThumbPart;
    case QStyle::SC_ScrollBarAddPage:
        return ForwardTrackPart;
    case QStyle::SC_ScrollBarAddLine:
        return ForwardButtonStartPart;
    }
    return NoPart;
}

static QStyleOptionSlider* styleOptionSlider(ScrollbarThemeClient* scrollbar, QWidget* widget = 0)
{
    static QStyleOptionSlider opt;
    if (widget)
        opt.initFrom(widget);
    else
        opt.state |= QStyle::State_Active;

    opt.state &= ~QStyle::State_HasFocus;

    opt.rect = scrollbar->frameRect();
    if (scrollbar->enabled())
        opt.state |= QStyle::State_Enabled;
    if (scrollbar->controlSize() != RegularScrollbar)
        opt.state |= QStyle::State_Mini;
    opt.orientation = (scrollbar->orientation() == VerticalScrollbar) ? Qt::Vertical : Qt::Horizontal;

    if (scrollbar->orientation() == HorizontalScrollbar)
        opt.state |= QStyle::State_Horizontal;
    else
        opt.state &= ~QStyle::State_Horizontal;

    opt.sliderValue = scrollbar->value();
    opt.sliderPosition = opt.sliderValue;
    opt.pageStep = scrollbar->pageStep();
    opt.singleStep = scrollbar->lineStep();
    opt.minimum = 0;
    opt.maximum = qMax(0, scrollbar->maximum());
    ScrollbarPart pressedPart = scrollbar->pressedPart();
    ScrollbarPart hoveredPart = scrollbar->hoveredPart();
    if (pressedPart != NoPart) {
        opt.activeSubControls = scPart(scrollbar->pressedPart());
        if (pressedPart == BackButtonStartPart || pressedPart == ForwardButtonStartPart
            || pressedPart == BackButtonEndPart || pressedPart == ForwardButtonEndPart
            || pressedPart == ThumbPart)
            opt.state |= QStyle::State_Sunken;
    } else
        opt.activeSubControls = scPart(hoveredPart);
    if (hoveredPart != NoPart)
        opt.state |= QStyle::State_MouseOver;
    return &opt;
}

bool ScrollbarThemeQStyle::paint(ScrollbarThemeClient* scrollbar, GraphicsContext* graphicsContext, const IntRect& dirtyRect)
{
    if (graphicsContext->updatingControlTints()) {
       scrollbar->invalidateRect(dirtyRect);
       return false;
    }

    StylePainterQStyle p(this, graphicsContext);
    if (!p.isValid())
      return true;

    p.painter->save();
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar, p.widget);

    p.painter->setClipRect(opt->rect.intersected(dirtyRect), Qt::IntersectClip);

#ifdef Q_WS_MAC
    // FIXME: We also need to check the widget style but today ScrollbarTheme is not aware of the page so we
    // can't get the widget.
    if (qobject_cast<QMacStyle*>(style()))
        p.drawComplexControl(QStyle::CC_ScrollBar, *opt);
    else
#endif
    {
        // The QStyle expects the background to be already filled.
        p.painter->fillRect(opt->rect, opt->palette.background());

        const QPoint topLeft = opt->rect.topLeft();
        p.painter->translate(topLeft);
        opt->rect.moveTo(QPoint(0, 0));
        p.drawComplexControl(QStyle::CC_ScrollBar, *opt);
        opt->rect.moveTo(topLeft);
    }
    p.painter->restore();

    return true;
}

ScrollbarPart ScrollbarThemeQStyle::hitTest(ScrollbarThemeClient* scrollbar, const PlatformMouseEvent& evt)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    const QPoint pos = scrollbar->convertFromContainingWindow(evt.position());
    opt->rect.moveTo(QPoint(0, 0));
    QStyle::SubControl sc = style()->hitTestComplexControl(QStyle::CC_ScrollBar, opt, pos, 0);
    return scrollbarPart(sc);
}

bool ScrollbarThemeQStyle::shouldCenterOnThumb(ScrollbarThemeClient*, const PlatformMouseEvent& evt)
{
    // Middle click centers slider thumb (if supported).
    return style()->styleHint(QStyle::SH_ScrollBar_MiddleClickAbsolutePosition) && evt.button() == MiddleButton;
}

void ScrollbarThemeQStyle::invalidatePart(ScrollbarThemeClient* scrollbar, ScrollbarPart)
{
    // FIXME: Do more precise invalidation.
    scrollbar->invalidate();
}

int ScrollbarThemeQStyle::scrollbarThickness(ScrollbarControlSize controlSize)
{
    QStyleOptionSlider o;
    o.orientation = Qt::Vertical;
    o.state &= ~QStyle::State_Horizontal;
    if (controlSize != RegularScrollbar)
        o.state |= QStyle::State_Mini;
    return style()->pixelMetric(QStyle::PM_ScrollBarExtent, &o, 0);
}

int ScrollbarThemeQStyle::thumbPosition(ScrollbarThemeClient* scrollbar)
{
    if (scrollbar->enabled()) {
        float pos = (float)scrollbar->currentPos() * (trackLength(scrollbar) - thumbLength(scrollbar)) / scrollbar->maximum();
        return (pos < 1 && pos > 0) ? 1 : pos;
    }
    return 0;
}

int ScrollbarThemeQStyle::thumbLength(ScrollbarThemeClient* scrollbar)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    IntRect thumb = style()->subControlRect(QStyle::CC_ScrollBar, opt, QStyle::SC_ScrollBarSlider, 0);
    return scrollbar->orientation() == HorizontalScrollbar ? thumb.width() : thumb.height();
}

int ScrollbarThemeQStyle::trackPosition(ScrollbarThemeClient* scrollbar)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    IntRect track = style()->subControlRect(QStyle::CC_ScrollBar, opt, QStyle::SC_ScrollBarGroove, 0);
    return scrollbar->orientation() == HorizontalScrollbar ? track.x() - scrollbar->x() : track.y() - scrollbar->y();
}

int ScrollbarThemeQStyle::trackLength(ScrollbarThemeClient* scrollbar)
{
    QStyleOptionSlider* opt = styleOptionSlider(scrollbar);
    IntRect track = style()->subControlRect(QStyle::CC_ScrollBar, opt, QStyle::SC_ScrollBarGroove, 0);
    return scrollbar->orientation() == HorizontalScrollbar ? track.width() : track.height();
}

void ScrollbarThemeQStyle::paintScrollCorner(ScrollView*, GraphicsContext* context, const IntRect& rect)
{
    StylePainterQStyle p(this, context);
    if (!p.isValid())
        return;

    QStyleOption option;
    option.rect = rect;
    p.drawPrimitive(QStyle::PE_PanelScrollAreaCorner, option);
}

QStyle* ScrollbarThemeQStyle::style() const
{
    return QApplication::style();
}

}

