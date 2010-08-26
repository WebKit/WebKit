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
#include "ScrollbarThemeGtk.h"

#include "PlatformMouseEvent.h"
#include "RenderThemeGtk.h"
#include "Scrollbar.h"
#include "gtkdrawing.h"
#include <gtk/gtk.h>

namespace WebCore {

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeGtk theme;
    return &theme;
}

ScrollbarThemeGtk::~ScrollbarThemeGtk()
{
}

bool ScrollbarThemeGtk::hasThumb(Scrollbar* scrollbar)
{
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped.  We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    // We do not support multiple steppers per end yet.
    if (part == BackButtonEndPart)
        return IntRect();

    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);
    IntSize size = buttonSize(scrollbar);
    return IntRect(scrollbar->x() + metrics.trough_border, scrollbar->y() + metrics.trough_border, size.width(), size.height());
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    // We do not support multiple steppers per end yet.
    if (part == ForwardButtonStartPart)
        return IntRect();

    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);

    IntSize size = buttonSize(scrollbar);
    int x, y;
    if (scrollbar->orientation() == HorizontalScrollbar) {
        x = scrollbar->x() + scrollbar->width() - size.width() - metrics.trough_border;
        y = scrollbar->y() + metrics.trough_border;
    } else {
        x = scrollbar->x() + metrics.trough_border;
        y = scrollbar->y() + scrollbar->height() - size.height() - metrics.trough_border;
    }
    return IntRect(x, y, size.width(), size.height());
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar* scrollbar, bool)
{
    // The padding along the thumb movement axis (from outside to in)
    // is the size of trough border plus the size of the stepper (button)
    // plus the size of stepper spacing (the space between the stepper and
    // the place where the thumb stops). There is often no stepper spacing.
    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);
    int movementAxisPadding = metrics.trough_border + metrics.stepper_size + metrics.stepper_spacing;

    // The thickness (of the long part) of the scrollbar. We could also
    // use scrollbarThickness here, but that would mean one more call to
    // moz_gtk_get_scrollbar_metrics.
    int thickness = metrics.slider_width + (metrics.trough_border * 2);

    if (scrollbar->orientation() == HorizontalScrollbar) {
        // Once the scrollbar becomes smaller than the natural size of the
        // two buttons, the track disappears.
        if (scrollbar->width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar->x() + movementAxisPadding, scrollbar->y(), scrollbar->width() - (2 * movementAxisPadding), thickness);
    }

    if (scrollbar->height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar->x(), scrollbar->y() + movementAxisPadding, thickness, scrollbar->height() - (2 * movementAxisPadding));
}

void ScrollbarThemeGtk::paintTrackBackground(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect)
{
    GtkWidgetState state;
    state.focused = FALSE;
    state.isDefault = FALSE;
    state.canDefault = FALSE;
    state.disabled = FALSE;
    state.active = FALSE;
    state.inHover = FALSE;

    IntRect fullScrollbarRect(scrollbar->x(), scrollbar->y(), scrollbar->width(), scrollbar->height());
    GtkThemeWidgetType type = scrollbar->orientation() == VerticalScrollbar ? MOZ_GTK_SCROLLBAR_TRACK_VERTICAL : MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL;
    static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get())->paintMozillaGtkWidget(type, context, fullScrollbarRect, &state, 0);
}

void ScrollbarThemeGtk::paintScrollbarBackground(GraphicsContext* context, Scrollbar* scrollbar)
{
    GtkWidgetState state;
    state.focused = FALSE;
    state.isDefault = FALSE;
    state.canDefault = FALSE;
    state.disabled = FALSE;
    state.active = TRUE;
    state.inHover = FALSE;

    IntRect fullScrollbarRect = IntRect(scrollbar->x(), scrollbar->y(), scrollbar->width(), scrollbar->height());
    static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get())->paintMozillaGtkWidget(MOZ_GTK_SCROLLED_WINDOW, context, fullScrollbarRect, &state, 0);
}

void ScrollbarThemeGtk::paintThumb(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect)
{
    GtkWidgetState state;
    state.focused = FALSE;
    state.isDefault = FALSE;
    state.canDefault = FALSE;
    state.disabled = FALSE;
    state.active = scrollbar->pressedPart() == ThumbPart;
    state.inHover = scrollbar->hoveredPart() == ThumbPart;
    state.maxpos = scrollbar->maximum();
    state.curpos = scrollbar->currentPos();

    GtkThemeWidgetType type = scrollbar->orientation() == VerticalScrollbar ? MOZ_GTK_SCROLLBAR_THUMB_VERTICAL : MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL;
    static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get())->paintMozillaGtkWidget(type, context, rect, &state, 0);
}

IntRect ScrollbarThemeGtk::thumbRect(Scrollbar* scrollbar, const IntRect& unconstrainedTrackRect)
{
    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);

    IntRect trackRect = constrainTrackRectToTrackPieces(scrollbar, unconstrainedTrackRect);
    int thickness = metrics.slider_width;
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar->orientation() == HorizontalScrollbar)
        return IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - thickness) / 2, thumbLength(scrollbar), thickness); 

    // VerticalScrollbar
    return IntRect(trackRect.x() + (trackRect.width() - thickness) / 2, trackRect.y() + thumbPos, thickness, thumbLength(scrollbar));
}

bool ScrollbarThemeGtk::paint(Scrollbar* scrollbar, GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    // Create the ScrollbarControlPartMask based on the damageRect
    ScrollbarControlPartMask scrollMask = NoPart;

    IntRect backButtonStartPaintRect;
    IntRect backButtonEndPaintRect;
    IntRect forwardButtonStartPaintRect;
    IntRect forwardButtonEndPaintRect;
    if (hasButtons(scrollbar)) {
        backButtonStartPaintRect = backButtonRect(scrollbar, BackButtonStartPart, true);
        if (damageRect.intersects(backButtonStartPaintRect))
            scrollMask |= BackButtonStartPart;
        backButtonEndPaintRect = backButtonRect(scrollbar, BackButtonEndPart, true);
        if (damageRect.intersects(backButtonEndPaintRect))
            scrollMask |= BackButtonEndPart;
        forwardButtonStartPaintRect = forwardButtonRect(scrollbar, ForwardButtonStartPart, true);
        if (damageRect.intersects(forwardButtonStartPaintRect))
            scrollMask |= ForwardButtonStartPart;
        forwardButtonEndPaintRect = forwardButtonRect(scrollbar, ForwardButtonEndPart, true);
        if (damageRect.intersects(forwardButtonEndPaintRect))
            scrollMask |= ForwardButtonEndPart;
    }

    IntRect trackPaintRect = trackRect(scrollbar, true);
    if (damageRect.intersects(trackPaintRect))
        scrollMask |= TrackBGPart;

    bool thumbPresent = hasThumb(scrollbar);
    IntRect currentThumbRect;
    if (thumbPresent) {
        IntRect track = trackRect(scrollbar, false);
        currentThumbRect = thumbRect(scrollbar, track);
        if (damageRect.intersects(currentThumbRect))
            scrollMask |= ThumbPart;
    }

    // Paint the scrollbar background (only used by custom CSS scrollbars).
    paintScrollbarBackground(graphicsContext, scrollbar);

    if (scrollMask & TrackBGPart)
        paintTrackBackground(graphicsContext, scrollbar, trackPaintRect);

    // Paint the back and forward buttons.
    if (scrollMask & BackButtonStartPart)
        paintButton(graphicsContext, scrollbar, backButtonStartPaintRect, BackButtonStartPart);
    if (scrollMask & BackButtonEndPart)
        paintButton(graphicsContext, scrollbar, backButtonEndPaintRect, BackButtonEndPart);
    if (scrollMask & ForwardButtonStartPart)
        paintButton(graphicsContext, scrollbar, forwardButtonStartPaintRect, ForwardButtonStartPart);
    if (scrollMask & ForwardButtonEndPart)
        paintButton(graphicsContext, scrollbar, forwardButtonEndPaintRect, ForwardButtonEndPart);

    // Paint the thumb.
    if (scrollMask & ThumbPart)
        paintThumb(graphicsContext, scrollbar, currentThumbRect);

    return true;
}

void ScrollbarThemeGtk::paintButton(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    int flags = 0;
    if (scrollbar->orientation() == VerticalScrollbar)
        flags |= MOZ_GTK_STEPPER_VERTICAL;

    if (part == ForwardButtonEndPart) {
        flags |= MOZ_GTK_STEPPER_DOWN;
        flags |= MOZ_GTK_STEPPER_BOTTOM;
    }

    GtkWidgetState state;
    state.focused = TRUE;
    state.isDefault = TRUE;
    state.canDefault = TRUE;

    if ((BackButtonStartPart == part && scrollbar->currentPos())
        || (ForwardButtonEndPart == part && scrollbar->currentPos() != scrollbar->maximum())) {
        state.disabled = FALSE;
        state.active = part == scrollbar->pressedPart();
        state.inHover = part == scrollbar->hoveredPart();
    } else {
        state.disabled = TRUE;
        state.active = FALSE;
        state.inHover = FALSE;
    }

    static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get())->paintMozillaGtkWidget(MOZ_GTK_SCROLLBAR_BUTTON, context, rect, &state, flags);
}

void ScrollbarThemeGtk::paintScrollCorner(ScrollView* view, GraphicsContext* context, const IntRect& cornerRect)
{
    // ScrollbarThemeComposite::paintScrollCorner incorrectly assumes that the
    // ScrollView is a FrameView (see FramelessScrollView), so we cannot let
    // that code run.  For FrameView's this is correct since we don't do custom
    // scrollbar corner rendering, which ScrollbarThemeComposite supports.
    ScrollbarTheme::paintScrollCorner(view, context, cornerRect);
}

bool ScrollbarThemeGtk::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& event)
{
    return (event.shiftKey() && event.button() == LeftButton) || (event.button() == MiddleButton);
}

int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize)
{
    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);
    return metrics.slider_width + (metrics.trough_border * 2);
}

IntSize ScrollbarThemeGtk::buttonSize(Scrollbar* scrollbar)
{
    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);

    if (scrollbar->orientation() == VerticalScrollbar)
        return IntSize(metrics.slider_width, metrics.stepper_size);

    // HorizontalScrollbar
    return IntSize(metrics.stepper_size, metrics.slider_width);
}

int ScrollbarThemeGtk::minimumThumbLength(Scrollbar* scrollbar)
{
    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);
    return metrics.min_slider_size;
}

}

