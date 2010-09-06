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
#include "ScrollView.h"
#include "Scrollbar.h"
#include "gtkdrawing.h"
#include <gtk/gtk.h>

namespace WebCore {

static HashSet<Scrollbar*>* gScrollbars;
static void gtkStyleSetCallback(GtkWidget*, GtkStyle*, ScrollbarThemeGtk*);

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeGtk theme;
    return &theme;
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
    updateThemeProperties();
    g_signal_connect(static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get())->gtkScrollbar(),
         "style-set", G_CALLBACK(gtkStyleSetCallback), this);
}

ScrollbarThemeGtk::~ScrollbarThemeGtk()
{
}

void ScrollbarThemeGtk::registerScrollbar(Scrollbar* scrollbar)
{
    if (!gScrollbars)
        gScrollbars = new HashSet<Scrollbar*>;
    gScrollbars->add(scrollbar);
}

void ScrollbarThemeGtk::unregisterScrollbar(Scrollbar* scrollbar)
{
    gScrollbars->remove(scrollbar);
    if (gScrollbars->isEmpty()) {
        delete gScrollbars;
        gScrollbars = 0;
    }
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

    if (!gScrollbars)
        return;

    // Update the thickness of every interior frame scrollbar widget. The
    // platform-independent scrollbar them code isn't yet smart enough to get
    // this information when it paints.
    HashSet<Scrollbar*>::iterator end = gScrollbars->end();
    for (HashSet<Scrollbar*>::iterator it = gScrollbars->begin(); it != end; ++it) {
        Scrollbar* scrollbar = (*it);

        // Top-level scrollbar i.e. scrollbars who have a parent ScrollView
        // with no parent are native, and thus do not need to be resized.
        if (!scrollbar->parent() || !scrollbar->parent()->parent())
            return;

        int thickness = scrollbarThickness(scrollbar->controlSize());
        if (scrollbar->orientation() == HorizontalScrollbar)
            scrollbar->setFrameRect(IntRect(0, scrollbar->parent()->height() - thickness, scrollbar->width(), thickness));
        else
            scrollbar->setFrameRect(IntRect(scrollbar->parent()->width() - thickness, 0, thickness, scrollbar->height()));
    }
}

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, ScrollbarThemeGtk* scrollbarTheme)
{
    scrollbarTheme->updateThemeProperties();
}

bool ScrollbarThemeGtk::hasThumb(Scrollbar* scrollbar)
{
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped.  We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    if (part == BackButtonEndPart && !m_hasBackButtonEndPart)
        return IntRect();

    int x = scrollbar->x() + m_troughBorderWidth;
    int y = scrollbar->y() + m_troughBorderWidth;
    IntSize size = buttonSize(scrollbar);
    if (part == BackButtonStartPart)
        return IntRect(x, y, size.width(), size.height());

    // BackButtonEndPart (alternate button)
    if (scrollbar->orientation() == HorizontalScrollbar)
        return IntRect(scrollbar->x() + scrollbar->width() - m_troughBorderWidth - (2 * size.width()), y, size.width(), size.height());

    // VerticalScrollbar alternate button
    return IntRect(x, scrollbar->y() + scrollbar->height() - m_troughBorderWidth - (2 * size.height()), size.width(), size.height());
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar* scrollbar, ScrollbarPart part, bool)
{
    if (part == ForwardButtonStartPart && !m_hasForwardButtonStartPart)
        return IntRect();

    IntSize size = buttonSize(scrollbar);
    if (scrollbar->orientation() == HorizontalScrollbar) {
        int y = scrollbar->y() + m_troughBorderWidth;
        if (part == ForwardButtonEndPart)
            return IntRect(scrollbar->x() + scrollbar->width() - size.width() - m_troughBorderWidth, y, size.width(), size.height());

        // ForwardButtonStartPart (alternate button)
        return IntRect(scrollbar->x() + m_troughBorderWidth + size.width(), y, size.width(), size.height());
    }

    // VerticalScrollbar
    int x = scrollbar->x() + m_troughBorderWidth;
    if (part == ForwardButtonEndPart)
        return IntRect(x, scrollbar->y() + scrollbar->height() - size.height() - m_troughBorderWidth, size.width(), size.height());

    // ForwardButtonStartPart (alternate button)
    return IntRect(x, scrollbar->y() + m_troughBorderWidth + size.height(), size.width(), size.height());
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar* scrollbar, bool)
{
    // The padding along the thumb movement axis (from outside to in)
    // is the size of trough border plus the size of the stepper (button)
    // plus the size of stepper spacing (the space between the stepper and
    // the place where the thumb stops). There is often no stepper spacing.
    int movementAxisPadding = m_troughBorderWidth + m_stepperSize + m_stepperSpacing;

    // The fatness of the scrollbar on the non-movement axis.
    int thickness = scrollbarThickness(scrollbar->controlSize());

    int alternateButtonOffset = 0;
    int alternateButtonWidth = 0;
    if (m_hasForwardButtonStartPart) {
        alternateButtonOffset += m_stepperSize;
        alternateButtonWidth += m_stepperSize;
    }
    if (m_hasBackButtonEndPart)
        alternateButtonWidth += m_stepperSize;

    if (scrollbar->orientation() == HorizontalScrollbar) {
        // Once the scrollbar becomes smaller than the natural size of the
        // two buttons, the track disappears.
        if (scrollbar->width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar->x() + movementAxisPadding + alternateButtonOffset, scrollbar->y(),
                       scrollbar->width() - (2 * movementAxisPadding) - alternateButtonWidth, thickness);
    }

    if (scrollbar->height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar->x(), scrollbar->y() + movementAxisPadding + alternateButtonOffset,
                   thickness, scrollbar->height() - (2 * movementAxisPadding) - alternateButtonWidth);
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
    IntRect trackRect = constrainTrackRectToTrackPieces(scrollbar, unconstrainedTrackRect);
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar->orientation() == HorizontalScrollbar)
        return IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - m_thumbFatness) / 2, thumbLength(scrollbar), m_thumbFatness); 

    // VerticalScrollbar
    return IntRect(trackRect.x() + (trackRect.width() - m_thumbFatness) / 2, trackRect.y() + thumbPos, m_thumbFatness, thumbLength(scrollbar));
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

    if (m_troughUnderSteppers && (scrollMask & BackButtonStartPart
            || scrollMask & BackButtonEndPart
            || scrollMask & ForwardButtonStartPart
            || scrollMask & ForwardButtonEndPart))
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

    if (part == ForwardButtonEndPart)
        flags |= (MOZ_GTK_STEPPER_DOWN | MOZ_GTK_STEPPER_BOTTOM);
    if (part == ForwardButtonStartPart)
        flags |= MOZ_GTK_STEPPER_DOWN;

    GtkWidgetState state;
    state.focused = TRUE;
    state.isDefault = TRUE;
    state.canDefault = TRUE;

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
    return m_thumbFatness + (m_troughBorderWidth * 2);
}

IntSize ScrollbarThemeGtk::buttonSize(Scrollbar* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar)
        return IntSize(m_thumbFatness, m_stepperSize);

    // HorizontalScrollbar
    return IntSize(m_stepperSize, m_thumbFatness);
}

int ScrollbarThemeGtk::minimumThumbLength(Scrollbar* scrollbar)
{
    return m_minThumbLength;
}

}

