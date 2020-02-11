/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollbarThemeWPE.h"

#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"

namespace WebCore {

static const unsigned scrollbarSize = 13;
static const unsigned hoveredScrollbarBorderSize = 1;
static const unsigned thumbBorderSize = 1;
static const unsigned overlayThumbSize = 5;
static const unsigned thumbSize = 6;
static const double scrollbarOpacity = 0.8;
static const Color scrollbarBackgroundColor = makeRGB(252, 252, 252);
static const Color scrollbarBorderColor = makeRGB(220, 223, 227);
static const Color overlayThumbBorderColor = makeRGBA(255, 255, 255, 100);
static const Color overlayThumbColor = makeRGBA(46, 52, 54, 100);
static const Color thumbHoveredColor = makeRGB(86, 91, 92);
static const Color thumbPressedColor = makeRGB(27, 106, 203);
static const Color thumbColor = makeRGB(126, 129, 130);

int ScrollbarThemeWPE::scrollbarThickness(ScrollbarControlSize, ScrollbarExpansionState)
{
    return scrollbarSize;
}

int ScrollbarThemeWPE::minimumThumbLength(Scrollbar&)
{
    return 0;
}

bool ScrollbarThemeWPE::hasButtons(Scrollbar&)
{
    return false;
}

bool ScrollbarThemeWPE::hasThumb(Scrollbar& scrollbar)
{
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeWPE::backButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return { };
}

IntRect ScrollbarThemeWPE::forwardButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return { };
}

IntRect ScrollbarThemeWPE::trackRect(Scrollbar& scrollbar, bool)
{
    return scrollbar.frameRect();
}

bool ScrollbarThemeWPE::paint(Scrollbar& scrollbar, GraphicsContext& graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext.paintingDisabled())
        return false;

    if (!scrollbar.enabled())
        return true;

    IntRect rect = scrollbar.frameRect();
    if (!rect.intersects(damageRect))
        return true;

    double opacity = scrollbar.hoveredPart() == NoPart ? scrollbar.opacity() : scrollbarOpacity;
    if (!opacity)
        return true;

    GraphicsContextStateSaver stateSaver(graphicsContext);
    if (opacity != 1) {
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    if (scrollbar.hoveredPart() != NoPart) {
        graphicsContext.fillRect(rect, scrollbarBackgroundColor);

        IntRect frame = rect;
        if (scrollbar.orientation() == VerticalScrollbar) {
            if (scrollbar.scrollableArea().shouldPlaceBlockDirectionScrollbarOnLeft())
                frame.move(frame.width() - hoveredScrollbarBorderSize, 0);
            frame.setWidth(hoveredScrollbarBorderSize);
        } else
            frame.setHeight(hoveredScrollbarBorderSize);
        graphicsContext.fillRect(frame, scrollbarBorderColor);
    }

    int thumbPos = thumbPosition(scrollbar);
    int thumbLen = thumbLength(scrollbar);
    IntRect thumb = rect;
    if (scrollbar.hoveredPart() == NoPart) {
        if (scrollbar.orientation() == VerticalScrollbar) {
            if (scrollbar.scrollableArea().shouldPlaceBlockDirectionScrollbarOnLeft())
                thumb.move(hoveredScrollbarBorderSize, thumbPos + thumbBorderSize);
            else
                thumb.move(scrollbarSize - (overlayThumbSize + thumbBorderSize) + hoveredScrollbarBorderSize, thumbPos + thumbBorderSize);
            thumb.setWidth(overlayThumbSize);
            thumb.setHeight(thumbLen - thumbBorderSize * 2);
        } else {
            thumb.move(thumbPos + thumbBorderSize, scrollbarSize - (overlayThumbSize + thumbBorderSize) + hoveredScrollbarBorderSize);
            thumb.setWidth(thumbLen - thumbBorderSize * 2);
            thumb.setHeight(overlayThumbSize);
        }
    } else {
        if (scrollbar.orientation() == VerticalScrollbar) {
            if (scrollbar.scrollableArea().shouldPlaceBlockDirectionScrollbarOnLeft())
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2) - hoveredScrollbarBorderSize, thumbPos + thumbBorderSize);
            else
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2), thumbPos + thumbBorderSize);
            thumb.setWidth(thumbSize);
            thumb.setHeight(thumbLen - thumbBorderSize * 2);
        } else {
            thumb.move(thumbPos + thumbBorderSize, scrollbarSize - (scrollbarSize / 2 + thumbSize / 2));
            thumb.setWidth(thumbLen - thumbBorderSize * 2);
            thumb.setHeight(thumbSize);
        }
    }

    FloatSize corner(4, 4);
    Path path;
    if (scrollbar.hoveredPart() == NoPart) {
        path.addRoundedRect(thumb, corner);
        thumb.inflate(-1);
        path.addRoundedRect(thumb, corner);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(overlayThumbBorderColor);
        graphicsContext.fillPath(path);
        path.clear();
    }

    path.addRoundedRect(thumb, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (scrollbar.hoveredPart() == NoPart)
        graphicsContext.setFillColor(overlayThumbColor);
    else if (scrollbar.pressedPart() == ThumbPart)
        graphicsContext.setFillColor(thumbPressedColor);
    else if (scrollbar.hoveredPart() == ThumbPart)
        graphicsContext.setFillColor(thumbHoveredColor);
    else
        graphicsContext.setFillColor(thumbColor);
    graphicsContext.fillPath(path);

    if (opacity != 1)
        graphicsContext.endTransparencyLayer();

    return true;
}

ScrollbarButtonPressAction ScrollbarThemeWPE::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
        // The shift key or middle/right button reverses the sense.
        if (event.shiftKey() || event.button() != LeftButton)
            return ScrollbarButtonPressAction::CenterOnThumb;
        return ScrollbarButtonPressAction::Scroll;
    case ThumbPart:
        if (event.button() != RightButton)
            return ScrollbarButtonPressAction::StartDrag;
        break;
    case BackButtonStartPart:
    case ForwardButtonStartPart:
    case BackButtonEndPart:
    case ForwardButtonEndPart:
        return ScrollbarButtonPressAction::Scroll;
    default:
        break;
    }

    return ScrollbarButtonPressAction::None;
}

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeWPE theme;
    return theme;
}

} // namespace WebCore
