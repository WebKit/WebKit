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
#include "RenderScrollbarTheme.h"
#include "RenderScrollbar.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

RenderScrollbarTheme* RenderScrollbarTheme::renderScrollbarTheme()
{
    static NeverDestroyed<RenderScrollbarTheme> theme;
    return &theme.get();
}

void RenderScrollbarTheme::buttonSizesAlongTrackAxis(Scrollbar& scrollbar, int& beforeSize, int& afterSize)
{
    IntRect firstButton = backButtonRect(scrollbar, BackButtonStartPart);
    IntRect secondButton = forwardButtonRect(scrollbar, ForwardButtonStartPart);
    IntRect thirdButton = backButtonRect(scrollbar, BackButtonEndPart);
    IntRect fourthButton = forwardButtonRect(scrollbar, ForwardButtonEndPart);
    if (scrollbar.orientation() == HorizontalScrollbar) {
        beforeSize = firstButton.width() + secondButton.width();
        afterSize = thirdButton.width() + fourthButton.width();
    } else {
        beforeSize = firstButton.height() + secondButton.height();
        afterSize = thirdButton.height() + fourthButton.height();
    }
}

bool RenderScrollbarTheme::hasButtons(Scrollbar& scrollbar)
{
    int startSize;
    int endSize;
    buttonSizesAlongTrackAxis(scrollbar, startSize, endSize);
    return (startSize + endSize) <= (scrollbar.orientation() == HorizontalScrollbar ? scrollbar.width() : scrollbar.height());
}

bool RenderScrollbarTheme::hasThumb(Scrollbar& scrollbar)
{
    return trackLength(scrollbar) - thumbLength(scrollbar) >= 0;
}

int RenderScrollbarTheme::minimumThumbLength(Scrollbar& scrollbar)
{
    return downcast<RenderScrollbar>(scrollbar).minimumThumbLength();
}

IntRect RenderScrollbarTheme::backButtonRect(Scrollbar& scrollbar, ScrollbarPart partType, bool)
{
    return downcast<RenderScrollbar>(scrollbar).buttonRect(partType);
}

IntRect RenderScrollbarTheme::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart partType, bool)
{
    return downcast<RenderScrollbar>(scrollbar).buttonRect(partType);
}

IntRect RenderScrollbarTheme::trackRect(Scrollbar& scrollbar, bool)
{
    if (!hasButtons(scrollbar))
        return scrollbar.frameRect();
    
    int startLength;
    int endLength;
    buttonSizesAlongTrackAxis(scrollbar, startLength, endLength);
    
    return downcast<RenderScrollbar>(scrollbar).trackRect(startLength, endLength);
}

IntRect RenderScrollbarTheme::constrainTrackRectToTrackPieces(Scrollbar& scrollbar, const IntRect& rect)
{ 
    IntRect backRect = downcast<RenderScrollbar>(scrollbar).trackPieceRectWithMargins(BackTrackPart, rect);
    IntRect forwardRect = downcast<RenderScrollbar>(scrollbar).trackPieceRectWithMargins(ForwardTrackPart, rect);
    IntRect result = rect;
    if (scrollbar.orientation() == HorizontalScrollbar) {
        result.setX(backRect.x());
        result.setWidth(forwardRect.maxX() - backRect.x());
    } else {
        result.setY(backRect.y());
        result.setHeight(forwardRect.maxY() - backRect.y());
    }
    return result;
}

void RenderScrollbarTheme::willPaintScrollbar(GraphicsContext& context, Scrollbar& scrollbar)
{
    float opacity = downcast<RenderScrollbar>(scrollbar).opacity();
    if (opacity != 1) {
        context.save();
        context.clip(scrollbar.frameRect());
        context.beginTransparencyLayer(opacity);
    }
}

void RenderScrollbarTheme::didPaintScrollbar(GraphicsContext& context, Scrollbar& scrollbar)
{
    float opacity = downcast<RenderScrollbar>(scrollbar).opacity();
    if (opacity != 1) {
        context.endTransparencyLayer();
        context.restore();
    }
}

void RenderScrollbarTheme::paintScrollCorner(ScrollableArea& area, GraphicsContext& context, const IntRect& cornerRect)
{
    ScrollbarTheme::theme().paintScrollCorner(area, context, cornerRect);
}

void RenderScrollbarTheme::paintScrollbarBackground(GraphicsContext& context, Scrollbar& scrollbar)
{
    downcast<RenderScrollbar>(scrollbar).paintPart(context, ScrollbarBGPart, scrollbar.frameRect());
}

void RenderScrollbarTheme::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    downcast<RenderScrollbar>(scrollbar).paintPart(context, TrackBGPart, rect);
}

void RenderScrollbarTheme::paintTrackPiece(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect, ScrollbarPart part)
{
    downcast<RenderScrollbar>(scrollbar).paintPart(context, part, rect);
}

void RenderScrollbarTheme::paintButton(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect, ScrollbarPart part)
{
    downcast<RenderScrollbar>(scrollbar).paintPart(context, part, rect);
}

void RenderScrollbarTheme::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    downcast<RenderScrollbar>(scrollbar).paintPart(context, ThumbPart, rect);
}

void RenderScrollbarTheme::paintTickmarks(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    ScrollbarTheme::theme().paintTickmarks(context, scrollbar, rect);
}

}
