/*
 * Copyright (C) 2014, 2020 Igalia S.L.
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
#include "ScrollbarThemeAdwaita.h"

#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "ScrollableArea.h"
#include "Scrollbar.h"
#include "ThemeAdwaita.h"

namespace WebCore {

static const unsigned scrollbarSize = 13;
static const unsigned hoveredScrollbarBorderSize = 1;
static const unsigned thumbBorderSize = 1;
static const unsigned overlayThumbSize = 5;
static const unsigned minimumThumbSize = 40;
static const unsigned thumbSize = 6;
static const double scrollbarOpacity = 0.8;
static constexpr auto scrollbarBackgroundColor = SRGBA<uint8_t> { 252, 252, 252 };
static constexpr auto scrollbarBorderColor = SRGBA<uint8_t> { 220, 223, 227 };
static constexpr auto overlayThumbBorderColor = Color::white.colorWithAlphaByte(100);
static constexpr auto overlayThumbColor = SRGBA<uint8_t> { 46, 52, 54, 100 };
static constexpr auto thumbHoveredColor = SRGBA<uint8_t> { 86, 91, 92 };
static constexpr auto thumbPressedColor = SRGBA<uint8_t> { 27, 106, 203 };
static constexpr auto thumbColor = SRGBA<uint8_t> { 126, 129, 130 };

bool ScrollbarThemeAdwaita::usesOverlayScrollbars() const
{
#if PLATFORM(GTK)
    static bool shouldUuseOverlayScrollbars = g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "0");
    return shouldUuseOverlayScrollbars;
#else
    return true;
#endif
}

int ScrollbarThemeAdwaita::scrollbarThickness(ScrollbarControlSize, ScrollbarExpansionState)
{
    return scrollbarSize;
}

int ScrollbarThemeAdwaita::minimumThumbLength(Scrollbar&)
{
    return minimumThumbSize;
}

bool ScrollbarThemeAdwaita::hasButtons(Scrollbar&)
{
    return false;
}

bool ScrollbarThemeAdwaita::hasThumb(Scrollbar& scrollbar)
{
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeAdwaita::backButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return { };
}

IntRect ScrollbarThemeAdwaita::forwardButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return { };
}

IntRect ScrollbarThemeAdwaita::trackRect(Scrollbar& scrollbar, bool)
{
    return scrollbar.frameRect();
}

bool ScrollbarThemeAdwaita::paint(Scrollbar& scrollbar, GraphicsContext& graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext.paintingDisabled())
        return false;

    if (!scrollbar.enabled())
        return true;

    IntRect rect = scrollbar.frameRect();
    if (!rect.intersects(damageRect))
        return true;

    double opacity;
    if (usesOverlayScrollbars())
        opacity = scrollbar.hoveredPart() == NoPart ? scrollbar.opacity() : scrollbarOpacity;
    else
        opacity = 1;
    if (!opacity)
        return true;

    GraphicsContextStateSaver stateSaver(graphicsContext);
    if (opacity != 1) {
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    if (scrollbar.hoveredPart() != NoPart || !usesOverlayScrollbars()) {
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
    if (scrollbar.hoveredPart() == NoPart && usesOverlayScrollbars()) {
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
    if (scrollbar.hoveredPart() == NoPart && usesOverlayScrollbars()) {
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
    if (scrollbar.hoveredPart() == NoPart && usesOverlayScrollbars())
        graphicsContext.setFillColor(overlayThumbColor);
    else if (scrollbar.pressedPart() == ThumbPart)
        graphicsContext.setFillColor(static_cast<ThemeAdwaita&>(Theme::singleton()).activeSelectionBackgroundColor());
    else if (scrollbar.hoveredPart() == ThumbPart)
        graphicsContext.setFillColor(thumbHoveredColor);
    else
        graphicsContext.setFillColor(thumbColor);
    graphicsContext.fillPath(path);

    if (opacity != 1)
        graphicsContext.endTransparencyLayer();

    return true;
}

ScrollbarButtonPressAction ScrollbarThemeAdwaita::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
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

#if !PLATFORM(GTK) || USE(GTK4)
ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeAdwaita theme;
    return theme;
}
#endif

} // namespace WebCore
