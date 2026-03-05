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

#if USE(THEME_ADWAITA)

#include "AdwaitaScrollbarPainter.h"
#include "Color.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "ScrollableArea.h"
#include "ScrollbarInlines.h"
#include "Scrollbar.h"
#include "ThemeAdwaita.h"

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "SystemSettings.h"
#endif

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
#include "ScrollbarsController.h"
#include "ScrollerImpAdwaita.h"
#include <wtf/NeverDestroyed.h>
#endif

namespace WebCore {
using namespace WebCore::AdwaitaScrollbarPainter;

void ScrollbarThemeAdwaita::updateScrollbarOverlayStyle(Scrollbar& scrollbar)
{
    scrollbar.invalidate();
}

bool ScrollbarThemeAdwaita::usesOverlayScrollbars() const
{
#if PLATFORM(GTK) && !USE(GTK4)
    if (!g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "0"))
        return false;
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
    return SystemSettings::singleton().overlayScrolling().value_or(true);
#else
    return true;
#endif
}

int ScrollbarThemeAdwaita::scrollbarThickness(ScrollbarWidth scrollbarWidth, OverlayScrollbarSizeRelevancy overlayRelevancy)
{
    if (scrollbarWidth == ScrollbarWidth::None || (usesOverlayScrollbars() && overlayRelevancy == OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize))
        return 0;
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
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    if (auto scrollableArea = protect(scrollbar.scrollableArea())) {
        if (scrollableArea->usesCompositedScrolling()) {
            // Painting is done by ScrollerCoordinated in the scrolling thread.
            return true;
        }
    }
#endif
    State state {
        .enabled = scrollbar.enabled(),
        .useDarkAppearanceForScrollbars = scrollbar.scrollableArea().useDarkAppearanceForScrollbars(),
        .shouldPlaceVerticalScrollbarOnLeft = scrollbar.scrollableArea().shouldPlaceVerticalScrollbarOnLeft(),
        .usesOverlayScrollbars = usesOverlayScrollbars(),
        .orientation = scrollbar.orientation(),
        .hoveredPart = scrollbar.hoveredPart(),
        .pressedPart = scrollbar.pressedPart(),
        .thumbPosition = thumbPosition(scrollbar),
        .thumbLength = thumbLength(scrollbar),
        .frameRect = scrollbar.frameRect(),
        .opacity = scrollbar.opacity(),
    };
    AdwaitaScrollbarPainter::paint(graphicsContext, damageRect, state);
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
#if PLATFORM(GTK) || PLATFORM(WPE)
        warpSlider = SystemSettings::singleton().primaryButtonWarpsSlider().value_or(true);
#endif
        // The shift key or middle/right button reverses the sense.
        if (event.shiftKey() || event.button() != MouseButton::Left)
            warpSlider = !warpSlider;
        return warpSlider ?
            ScrollbarButtonPressAction::CenterOnThumb:
            ScrollbarButtonPressAction::Scroll;
    case ThumbPart:
        if (event.button() != MouseButton::Right)
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

#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
ScrollerImpAdwaita* ScrollbarThemeAdwaita::scrollerImpForScrollbar(Scrollbar& scrollbar)
{
    if (scrollbar.isCustomScrollbar())
        return nullptr;
    static NeverDestroyed<ScrollerImpAdwaita> scrollerImp;
    return &scrollerImp.get();
}
#endif

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeAdwaita theme;
    return theme;
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
