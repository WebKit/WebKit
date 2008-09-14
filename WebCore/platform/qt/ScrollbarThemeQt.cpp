/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

static QStyleOptionSlider* styleOptionSlider(Scrollbar* scrollbar)
{
    static QStyleOptionSlider opt;
    opt.rect = scrollbar->frameGeometry();
    if (scrollbar->isEnabled()) {
        opt.state |= QStyle::State_Enabled;
    else
        opt.state &= ~QStyle::State_Enabled;
    if (scrollbar->controlSize() != RegularScrollbar)
        opt.state |= QStyle::State_Mini;
    else
        opt.state &= ~QStyle::State_Mini;
    opt.orientation = (scrollbar->orientation() == VerticalScrollbar) ? Qt::Vertical : Qt::Horizontal;
    opt.sliderValue = scrollbar->value();
    opt.sliderPosition = opt.sliderValue;
    opt.pageStep = scrollbar->visibleSize();
    opt.singleStep = scrollbar->lineStep();
    opt.minimum = 0;
    opt.maximum = qMax(0, scrollbar->maximum());
    if (scrollbar->pressedPart() != NoPart)
        opt.activeSubControls = scPart(scrollbar->pressedPart());
    else
        opt.activeSubControls = scPart(scrollbar->hoveredPart());
    return &opt;
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

bool ScrollbarThemeQt::paint(Scrollbar* scrollbar, GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    QOptionSlider* opt = styleOptionSlider(scrollbar);
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

}

