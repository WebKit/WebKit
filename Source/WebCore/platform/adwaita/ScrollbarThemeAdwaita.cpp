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

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

namespace WebCore {

static const unsigned scrollbarSize = 15;
static const unsigned hoveredScrollbarBorderSize = 1;
static const unsigned thumbBorderSize = 1;
static const unsigned overlayThumbSize = 5;
static const unsigned minimumThumbSize = 40;
static const unsigned thumbMargin = 3;
static const double scrollbarOpacity = 0.8;

static constexpr auto scrollbarBackgroundColorLight = SRGBA<uint8_t> { 206, 206, 206 };
static constexpr auto scrollbarBorderColorLight = SRGBA<uint8_t> { 205, 199, 194 };
static constexpr auto overlayThumbBorderColorLight = Color::white.colorWithAlphaByte(100);
static constexpr auto overlayThumbColorLight = SRGBA<uint8_t> { 46, 52, 54, 100 };
static constexpr auto thumbHoveredColorLight = SRGBA<uint8_t> { 86, 91, 92 };
static constexpr auto thumbColorLight = SRGBA<uint8_t> { 126, 129, 130 };

static constexpr auto scrollbarBackgroundColorDark = SRGBA<uint8_t> { 49, 49, 49 };
static constexpr auto scrollbarBorderColorDark = SRGBA<uint8_t> { 27, 27, 27 };
static constexpr auto overlayThumbBorderColorDark = Color::black.colorWithAlphaByte(100);
static constexpr auto overlayThumbColorDark = SRGBA<uint8_t> { 238, 238, 236, 100 };
static constexpr auto thumbHoveredColorDark = SRGBA<uint8_t> { 201, 201, 199 };
static constexpr auto thumbColorDark = SRGBA<uint8_t> { 164, 164, 163 };

void ScrollbarThemeAdwaita::updateScrollbarOverlayStyle(Scrollbar& scrollbar)
{
    scrollbar.invalidate();
}

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

    if (!scrollbar.enabled() && usesOverlayScrollbars())
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

    SRGBA<uint8_t> scrollbarBackgroundColor;
    SRGBA<uint8_t> scrollbarBorderColor;
    SRGBA<uint8_t> overlayThumbBorderColor;
    SRGBA<uint8_t> overlayThumbColor;
    SRGBA<uint8_t> thumbHoveredColor;
    SRGBA<uint8_t> thumbColor;

    if (scrollbar.scrollableArea().useDarkAppearanceForScrollbars()) {
        scrollbarBackgroundColor = scrollbarBackgroundColorDark;
        scrollbarBorderColor = scrollbarBorderColorDark;
        overlayThumbBorderColor = overlayThumbBorderColorDark;
        overlayThumbColor = overlayThumbColorDark;
        thumbHoveredColor = thumbHoveredColorDark;
        thumbColor = thumbColorDark;
    } else {
        scrollbarBackgroundColor = scrollbarBackgroundColorLight;
        scrollbarBorderColor = scrollbarBorderColorLight;
        overlayThumbBorderColor = overlayThumbBorderColorLight;
        overlayThumbColor = overlayThumbColorLight;
        thumbHoveredColor = thumbHoveredColorLight;
        thumbColor = thumbColorLight;
    }

    GraphicsContextStateSaver stateSaver(graphicsContext);
    if (opacity != 1) {
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    if (scrollbar.hoveredPart() != NoPart || !usesOverlayScrollbars()) {
        graphicsContext.fillRect(rect, scrollbarBackgroundColor);

        IntRect frame = rect;
        if (scrollbar.orientation() == VerticalScrollbar) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                frame.move(frame.width() - hoveredScrollbarBorderSize, 0);
            frame.setWidth(hoveredScrollbarBorderSize);
        } else
            frame.setHeight(hoveredScrollbarBorderSize);
        graphicsContext.fillRect(frame, scrollbarBorderColor);
    }

    int thumbCornerSize;
    int thumbPos = thumbPosition(scrollbar);
    int thumbLen = thumbLength(scrollbar);
    IntRect thumb = rect;
    if (scrollbar.hoveredPart() == NoPart && usesOverlayScrollbars()) {
        int overlayThumbMargin = thumbMargin - thumbBorderSize;
        thumbCornerSize = overlayThumbSize / 2;

        if (scrollbar.orientation() == VerticalScrollbar) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                thumb.move(0, thumbPos + overlayThumbMargin);
            else
                thumb.move(scrollbarSize - (overlayThumbSize + thumbBorderSize) + hoveredScrollbarBorderSize, thumbPos + overlayThumbMargin);
            thumb.setWidth(overlayThumbSize);
            thumb.setHeight(thumbLen - overlayThumbMargin * 2);
        } else {
            thumb.move(thumbPos + thumbMargin, scrollbarSize - (overlayThumbSize + thumbBorderSize) + hoveredScrollbarBorderSize);
            thumb.setWidth(thumbLen - overlayThumbMargin * 2);
            thumb.setHeight(overlayThumbSize);
        }
    } else {
        int thumbSize = scrollbarSize - hoveredScrollbarBorderSize - thumbMargin * 2;
        thumbCornerSize = thumbSize / 2;

        if (scrollbar.orientation() == VerticalScrollbar) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2) - hoveredScrollbarBorderSize, thumbPos + thumbMargin);
            else
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2), thumbPos + thumbMargin);
            thumb.setWidth(thumbSize);
            thumb.setHeight(thumbLen - thumbMargin * 2);
        } else {
            thumb.move(thumbPos + thumbMargin, scrollbarSize - (scrollbarSize / 2 + thumbSize / 2));
            thumb.setWidth(thumbLen - thumbMargin * 2);
            thumb.setHeight(thumbSize);
        }
    }

    FloatSize corner(thumbCornerSize, thumbCornerSize);
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

void ScrollbarThemeAdwaita::paintScrollCorner(ScrollableArea& scrollableArea, GraphicsContext& graphicsContext, const IntRect& cornerRect)
{
    if (graphicsContext.paintingDisabled())
        return;

    SRGBA<uint8_t> scrollbarBackgroundColor;
    SRGBA<uint8_t> scrollbarBorderColor;

    if (scrollableArea.useDarkAppearanceForScrollbars()) {
        scrollbarBackgroundColor = scrollbarBackgroundColorDark;
        scrollbarBorderColor = scrollbarBorderColorDark;
    } else {
        scrollbarBackgroundColor = scrollbarBackgroundColorLight;
        scrollbarBorderColor = scrollbarBorderColorLight;
    }

    IntRect borderRect = IntRect(cornerRect.location(), IntSize(hoveredScrollbarBorderSize, hoveredScrollbarBorderSize));

    if (scrollableArea.shouldPlaceVerticalScrollbarOnLeft())
        borderRect.move(cornerRect.width() - hoveredScrollbarBorderSize, 0);

    graphicsContext.fillRect(cornerRect, scrollbarBackgroundColor);
    graphicsContext.fillRect(borderRect, scrollbarBorderColor);
}

ScrollbarButtonPressAction ScrollbarThemeAdwaita::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    gboolean warpSlider = FALSE;
    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
#if PLATFORM(GTK)
        g_object_get(gtk_settings_get_default(),
            "gtk-primary-button-warps-slider",
            &warpSlider, nullptr);
#endif
        // The shift key or middle/right button reverses the sense.
        if (event.shiftKey() || event.button() != LeftButton)
            warpSlider = !warpSlider;
        return warpSlider ?
            ScrollbarButtonPressAction::CenterOnThumb:
            ScrollbarButtonPressAction::Scroll;
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
