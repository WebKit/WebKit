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
#include "GtkUtilities.h"
#include <gtk/gtk.h>
#endif

namespace WebCore {

static const unsigned scrollbarSize = 21;
static const unsigned scrollbarBorderSize = 1;
static const unsigned thumbBorderSize = 1;
static const unsigned overlayThumbSize = 3;
static const unsigned minimumThumbSize = 40;
static const unsigned horizThumbMargin = 6;
static const unsigned horizOverlayThumbMargin = 3;
static const unsigned vertThumbMargin = 7;

static constexpr auto scrollbarBackgroundColorLight = Color::white;
static constexpr auto scrollbarBorderColorLight = Color::black.colorWithAlphaByte(38);
static constexpr auto overlayThumbBorderColorLight = Color::white.colorWithAlphaByte(102);
static constexpr auto overlayTroughBorderColorLight = Color::white.colorWithAlphaByte(89);
static constexpr auto overlayTroughColorLight = Color::black.colorWithAlphaByte(25);
static constexpr auto thumbHoveredColorLight = Color::black.colorWithAlphaByte(102);
static constexpr auto thumbPressedColorLight = Color::black.colorWithAlphaByte(153);
static constexpr auto thumbColorLight = Color::black.colorWithAlphaByte(51);

static constexpr auto scrollbarBackgroundColorDark = SRGBA<uint8_t> { 30, 30, 30 };
static constexpr auto scrollbarBorderColorDark = Color::white.colorWithAlphaByte(38);
static constexpr auto overlayThumbBorderColorDark = Color::black.colorWithAlphaByte(51);
static constexpr auto overlayTroughBorderColorDark = Color::black.colorWithAlphaByte(44);
static constexpr auto overlayTroughColorDark = Color::white.colorWithAlphaByte(25);
static constexpr auto thumbHoveredColorDark = Color::white.colorWithAlphaByte(102);
static constexpr auto thumbPressedColorDark = Color::white.colorWithAlphaByte(153);
static constexpr auto thumbColorDark = Color::white.colorWithAlphaByte(51);

void ScrollbarThemeAdwaita::updateScrollbarOverlayStyle(Scrollbar& scrollbar)
{
    scrollbar.invalidate();
}

bool ScrollbarThemeAdwaita::usesOverlayScrollbars() const
{
#if PLATFORM(GTK)
    return shouldUseOverlayScrollbars();
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
        opacity = scrollbar.opacity();
    else
        opacity = 1;
    if (!opacity)
        return true;

    SRGBA<uint8_t> scrollbarBackgroundColor;
    SRGBA<uint8_t> scrollbarBorderColor;
    SRGBA<uint8_t> overlayThumbBorderColor;
    SRGBA<uint8_t> overlayTroughBorderColor;
    SRGBA<uint8_t> overlayTroughColor;
    SRGBA<uint8_t> thumbHoveredColor;
    SRGBA<uint8_t> thumbPressedColor;
    SRGBA<uint8_t> thumbColor;

    if (scrollbar.scrollableArea().useDarkAppearanceForScrollbars()) {
        scrollbarBackgroundColor = scrollbarBackgroundColorDark;
        scrollbarBorderColor = scrollbarBorderColorDark;
        overlayThumbBorderColor = overlayThumbBorderColorDark;
        overlayTroughBorderColor = overlayThumbBorderColorDark;
        overlayTroughColor = overlayTroughColorDark;
        thumbHoveredColor = thumbHoveredColorDark;
        thumbPressedColor = thumbPressedColorDark;
        thumbColor = thumbColorDark;
    } else {
        scrollbarBackgroundColor = scrollbarBackgroundColorLight;
        scrollbarBorderColor = scrollbarBorderColorLight;
        overlayThumbBorderColor = overlayThumbBorderColorLight;
        overlayTroughBorderColor = overlayThumbBorderColorLight;
        overlayTroughColor = overlayTroughColorLight;
        thumbHoveredColor = thumbHoveredColorLight;
        thumbPressedColor = thumbPressedColorLight;
        thumbColor = thumbColorLight;
    }

    GraphicsContextStateSaver stateSaver(graphicsContext);
    if (opacity != 1) {
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    int thumbSize = scrollbarSize - scrollbarBorderSize - horizThumbMargin * 2;

    if (!usesOverlayScrollbars()) {
        graphicsContext.fillRect(rect, scrollbarBackgroundColor);

        IntRect frame = rect;
        if (scrollbar.orientation() == ScrollbarOrientation::Vertical) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                frame.move(frame.width() - scrollbarBorderSize, 0);
            frame.setWidth(scrollbarBorderSize);
        } else
            frame.setHeight(scrollbarBorderSize);
        graphicsContext.fillRect(frame, scrollbarBorderColor);
    } else if (scrollbar.hoveredPart() != NoPart) {
        int thumbCornerSize = thumbSize / 2;
        FloatSize corner(thumbCornerSize, thumbCornerSize);
        FloatSize borderCorner(thumbCornerSize + thumbBorderSize, thumbCornerSize + thumbBorderSize);

        IntRect trough = rect;
        if (scrollbar.orientation() == ScrollbarOrientation::Vertical) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                trough.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2) - scrollbarBorderSize, vertThumbMargin);
            else
                trough.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2), vertThumbMargin);
            trough.setWidth(thumbSize);
            trough.setHeight(rect.height() - vertThumbMargin * 2);
        } else {
            trough.move(vertThumbMargin, scrollbarSize - (scrollbarSize / 2 + thumbSize / 2));
            trough.setWidth(rect.width() - vertThumbMargin * 2);
            trough.setHeight(thumbSize);
        }

        IntRect troughBorder(trough);
        troughBorder.inflate(thumbBorderSize);

        Path path;
        path.addRoundedRect(trough, corner);
        graphicsContext.setFillRule(WindRule::NonZero);
        graphicsContext.setFillColor(overlayTroughColor);
        graphicsContext.fillPath(path);
        path.clear();

        path.addRoundedRect(trough, corner);
        path.addRoundedRect(troughBorder, borderCorner);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(overlayTroughBorderColor);
        graphicsContext.fillPath(path);
    }

    int thumbCornerSize;
    int thumbPos = thumbPosition(scrollbar);
    int thumbLen = thumbLength(scrollbar);
    IntRect thumb = rect;
    if (scrollbar.hoveredPart() == NoPart && usesOverlayScrollbars()) {
        thumbCornerSize = overlayThumbSize / 2;

        if (scrollbar.orientation() == ScrollbarOrientation::Vertical) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                thumb.move(horizOverlayThumbMargin, thumbPos + vertThumbMargin);
            else
                thumb.move(scrollbarSize - overlayThumbSize - horizOverlayThumbMargin, thumbPos + vertThumbMargin);
            thumb.setWidth(overlayThumbSize);
            thumb.setHeight(thumbLen - vertThumbMargin * 2);
        } else {
            thumb.move(thumbPos + vertThumbMargin, scrollbarSize - overlayThumbSize - horizOverlayThumbMargin);
            thumb.setWidth(thumbLen - vertThumbMargin * 2);
            thumb.setHeight(overlayThumbSize);
        }
    } else {
        thumbCornerSize = thumbSize / 2;

        if (scrollbar.orientation() == ScrollbarOrientation::Vertical) {
            if (scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft())
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2) - scrollbarBorderSize, thumbPos + vertThumbMargin);
            else
                thumb.move(scrollbarSize - (scrollbarSize / 2 + thumbSize / 2), thumbPos + vertThumbMargin);
            thumb.setWidth(thumbSize);
            thumb.setHeight(thumbLen - vertThumbMargin * 2);
        } else {
            thumb.move(thumbPos + vertThumbMargin, scrollbarSize - (scrollbarSize / 2 + thumbSize / 2));
            thumb.setWidth(thumbLen - vertThumbMargin * 2);
            thumb.setHeight(thumbSize);
        }
    }

    FloatSize corner(thumbCornerSize, thumbCornerSize);
    FloatSize borderCorner(thumbCornerSize + thumbBorderSize, thumbCornerSize + thumbBorderSize);

    Path path;

    path.addRoundedRect(thumb, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (scrollbar.pressedPart() == ThumbPart)
        graphicsContext.setFillColor(thumbPressedColor);
    else if (scrollbar.hoveredPart() == ThumbPart)
        graphicsContext.setFillColor(thumbHoveredColor);
    else
        graphicsContext.setFillColor(thumbColor);
    graphicsContext.fillPath(path);
    path.clear();

    if (usesOverlayScrollbars()) {
        IntRect thumbBorder(thumb);
        thumbBorder.inflate(thumbBorderSize);

        path.addRoundedRect(thumb, corner);
        path.addRoundedRect(thumbBorder, borderCorner);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(overlayThumbBorderColor);
        graphicsContext.fillPath(path);
    }

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

    IntRect borderRect = IntRect(cornerRect.location(), IntSize(scrollbarBorderSize, scrollbarBorderSize));

    if (scrollableArea.shouldPlaceVerticalScrollbarOnLeft())
        borderRect.move(cornerRect.width() - scrollbarBorderSize, 0);

    graphicsContext.fillRect(cornerRect, scrollbarBackgroundColor);
    graphicsContext.fillRect(borderRect, scrollbarBorderColor);
}

ScrollbarButtonPressAction ScrollbarThemeAdwaita::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    bool warpSlider = false;
    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
#if PLATFORM(GTK)
        warpSlider = [] {
            gboolean gtkWarpsSlader = FALSE;
            g_object_get(gtk_settings_get_default(),
                "gtk-primary-button-warps-slider",
                &gtkWarpsSlader, nullptr);
            return gtkWarpsSlader;
        }();
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
