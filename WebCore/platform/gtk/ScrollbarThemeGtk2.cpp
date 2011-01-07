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

#ifdef GTK_API_VERSION_2

#include "PlatformMouseEvent.h"
#include "RenderThemeGtk.h"
#include "ScrollView.h"
#include "Scrollbar.h"
#include "WidgetRenderingContext.h"
#include "gtkdrawing.h"
#include <gtk/gtk.h>

namespace WebCore {

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, ScrollbarThemeGtk* scrollbarTheme)
{
    scrollbarTheme->updateThemeProperties();
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
    updateThemeProperties();
    g_signal_connect(static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get())->gtkScrollbar(),
         "style-set", G_CALLBACK(gtkStyleSetCallback), this);
}

void ScrollbarThemeGtk::updateThemeProperties()
{
    MozGtkScrollbarMetrics metrics;
    moz_gtk_get_scrollbar_metrics(&metrics);

    m_thumbFatness = metrics.slider_width;
    m_troughBorderWidth = metrics.trough_border;
    m_stepperSize = metrics.stepper_size;
    m_stepperSpacing = metrics.stepper_spacing;
    m_minThumbLength = metrics.min_slider_size;
    m_troughUnderSteppers = metrics.trough_under_steppers;
    m_hasForwardButtonStartPart = metrics.has_secondary_forward_stepper;
    m_hasBackButtonEndPart = metrics.has_secondary_backward_stepper;

    updateScrollbarsFrameThickness();
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

    // Paint the track background. If the trough-under-steppers property is true, this
    // should be the full size of the scrollbar, but if is false, it should only be the
    // track rect.
    IntRect fullScrollbarRect = rect;
    if (m_troughUnderSteppers)
        fullScrollbarRect = IntRect(scrollbar->x(), scrollbar->y(), scrollbar->width(), scrollbar->height());

    GtkThemeWidgetType type = scrollbar->orientation() == VerticalScrollbar ? MOZ_GTK_SCROLLBAR_TRACK_VERTICAL : MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL;
    WidgetRenderingContext widgetContext(context, fullScrollbarRect);
    widgetContext.paintMozillaWidget(type, &state, 0);
}

void ScrollbarThemeGtk::paintScrollbarBackground(GraphicsContext* context, Scrollbar* scrollbar)
{
    // This is unused by the moz_gtk_scrollecd_window_paint.
    GtkWidgetState state;
    IntRect fullScrollbarRect = IntRect(scrollbar->x(), scrollbar->y(), scrollbar->width(), scrollbar->height());
    WidgetRenderingContext widgetContext(context, fullScrollbarRect);
    widgetContext.paintMozillaWidget(MOZ_GTK_SCROLLED_WINDOW, &state, 0);
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
    WidgetRenderingContext widgetContext(context, rect);
    widgetContext.paintMozillaWidget(type, &state, 0);
}

void ScrollbarThemeGtk::paintButton(GraphicsContext* context, Scrollbar* scrollbar, const IntRect& rect, ScrollbarPart part)
{
    int flags = 0;
    if (scrollbar->orientation() == VerticalScrollbar)
        flags |= MOZ_GTK_STEPPER_VERTICAL;

    if (part == ForwardButtonEndPart)
        flags |= (MOZ_GTK_STEPPER_DOWN | MOZ_GTK_STEPPER_BOTTOM);
    if (part == ForwardButtonStartPart)
        flags |= MOZ_GTK_STEPPER_DOWN;

    GtkWidgetState state;
    state.focused = TRUE;
    state.isDefault = TRUE;
    state.canDefault = TRUE;
    state.depressed = FALSE;

    if ((BackButtonStartPart == part && scrollbar->currentPos())
        || (BackButtonEndPart == part && scrollbar->currentPos())
        || (ForwardButtonEndPart == part && scrollbar->currentPos() != scrollbar->maximum())
        || (ForwardButtonStartPart == part && scrollbar->currentPos() != scrollbar->maximum())) {
        state.disabled = FALSE;
        state.active = part == scrollbar->pressedPart();
        state.inHover = part == scrollbar->hoveredPart();
    } else {
        state.disabled = TRUE;
        state.active = FALSE;
        state.inHover = FALSE;
    }

    WidgetRenderingContext widgetContext(context, rect);
    widgetContext.paintMozillaWidget(MOZ_GTK_SCROLLBAR_BUTTON, &state, flags);
}

} // namespace WebCore

#endif // GTK_API_VERSION_2
