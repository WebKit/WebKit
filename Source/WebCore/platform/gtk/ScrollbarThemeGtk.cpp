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

#include "GRefPtrGtk.h"
#include "PlatformContextCairo.h"
#include "PlatformMouseEvent.h"
#include "ScrollView.h"
#include "Scrollbar.h"
#include <gtk/gtk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TemporaryChange.h>

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeGtk theme;
    return theme;
}

ScrollbarThemeGtk::~ScrollbarThemeGtk()
{
}

bool ScrollbarThemeGtk::hasThumb(Scrollbar& scrollbar)
{
#ifndef GTK_API_VERSION_2
    // This method is just called as a paint-time optimization to see if
    // painting the thumb can be skipped.  We don't have to be exact here.
    return thumbLength(scrollbar) > 0;
#else
    UNUSED_PARAM(scrollbar);
    return false;
#endif
}

IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool painting)
{
#ifndef GTK_API_VERSION_2
    if (part == BackButtonEndPart && !m_hasBackButtonEndPart)
        return IntRect();
    if (part == BackButtonStartPart && !m_hasBackButtonStartPart)
        return IntRect();

    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar, painting ? StyleContextMode::Paint : StyleContextMode::Layout);
    int x = scrollbar.x() + m_cachedProperties.troughBorderWidth;
    int y = scrollbar.y() + m_cachedProperties.troughBorderWidth;
    IntSize size = buttonSize(scrollbar);
    if (part == BackButtonStartPart)
        return IntRect(x, y, size.width(), size.height());

    // BackButtonEndPart (alternate button)
    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(scrollbar.x() + scrollbar.width() - m_cachedProperties.troughBorderWidth - (2 * size.width()), y, size.width(), size.height());

    // VerticalScrollbar alternate button
    return IntRect(x, scrollbar.y() + scrollbar.height() - m_cachedProperties.troughBorderWidth - (2 * size.height()), size.width(), size.height());
#else
    UNUSED_PARAM(scrollbar);
    UNUSED_PARAM(part);
    UNUSED_PARAM(painting);
    return IntRect();
#endif
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool painting)
{
#ifndef GTK_API_VERSION_2
    if (part == ForwardButtonStartPart && !m_hasForwardButtonStartPart)
        return IntRect();
    if (part == ForwardButtonEndPart && !m_hasForwardButtonEndPart)
        return IntRect();

    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar, painting ? StyleContextMode::Paint : StyleContextMode::Layout);
    IntSize size = buttonSize(scrollbar);
    if (scrollbar.orientation() == HorizontalScrollbar) {
        int y = scrollbar.y() + m_cachedProperties.troughBorderWidth;
        if (part == ForwardButtonEndPart)
            return IntRect(scrollbar.x() + scrollbar.width() - size.width() - m_cachedProperties.troughBorderWidth, y, size.width(), size.height());

        // ForwardButtonStartPart (alternate button)
        return IntRect(scrollbar.x() + m_cachedProperties.troughBorderWidth + size.width(), y, size.width(), size.height());
    }

    // VerticalScrollbar
    int x = scrollbar.x() + m_cachedProperties.troughBorderWidth;
    if (part == ForwardButtonEndPart)
        return IntRect(x, scrollbar.y() + scrollbar.height() - size.height() - m_cachedProperties.troughBorderWidth, size.width(), size.height());

    // ForwardButtonStartPart (alternate button)
    return IntRect(x, scrollbar.y() + m_cachedProperties.troughBorderWidth + size.height(), size.width(), size.height());
#else
    UNUSED_PARAM(scrollbar);
    UNUSED_PARAM(part);
    UNUSED_PARAM(painting);
    return IntRect();
#endif
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar& scrollbar, bool painting)
{
#ifndef GTK_API_VERSION_2
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar, painting ? StyleContextMode::Paint : StyleContextMode::Layout);
    // The padding along the thumb movement axis includes the trough border
    // plus the size of stepper spacing (the space between the stepper and
    // the place where the thumb stops). There is often no stepper spacing.
    int movementAxisPadding = m_cachedProperties.troughBorderWidth + m_cachedProperties.stepperSpacing;

    // The fatness of the scrollbar on the non-movement axis.
    int thickness = scrollbarThickness(scrollbar.controlSize());

    int startButtonsOffset = 0;
    int buttonsWidth = 0;
    if (m_hasForwardButtonStartPart) {
        startButtonsOffset += m_cachedProperties.stepperSize;
        buttonsWidth += m_cachedProperties.stepperSize;
    }
    if (m_hasBackButtonStartPart) {
        startButtonsOffset += m_cachedProperties.stepperSize;
        buttonsWidth += m_cachedProperties.stepperSize;
    }
    if (m_hasBackButtonEndPart)
        buttonsWidth += m_cachedProperties.stepperSize;
    if (m_hasForwardButtonEndPart)
        buttonsWidth += m_cachedProperties.stepperSize;

    if (scrollbar.orientation() == HorizontalScrollbar) {
        // Once the scrollbar becomes smaller than the natural size of the
        // two buttons, the track disappears.
        if (scrollbar.width() < 2 * thickness)
            return IntRect();
        return IntRect(scrollbar.x() + movementAxisPadding + startButtonsOffset, scrollbar.y(),
                       scrollbar.width() - (2 * movementAxisPadding) - buttonsWidth, thickness);
    }

    if (scrollbar.height() < 2 * thickness)
        return IntRect();
    return IntRect(scrollbar.x(), scrollbar.y() + movementAxisPadding + startButtonsOffset,
                   thickness, scrollbar.height() - (2 * movementAxisPadding) - buttonsWidth);
#else
    UNUSED_PARAM(scrollbar);
    UNUSED_PARAM(painting);
    return IntRect();
#endif
}

#ifndef GTK_API_VERSION_2
static inline const char* orientationStyleClass(ScrollbarOrientation orientation)
{
    return orientation == VerticalScrollbar ? "vertical" : "horizontal";
}

// The GtkStyleContext returned by this function is cached by ScrollbarThemeGtk::paint for the
// duration of its scope, so a different GtkStyleContext with updated theme properties will be
// used for each call to paint.
GRefPtr<GtkStyleContext> ScrollbarThemeGtk::getOrCreateStyleContext(Scrollbar* scrollbar, StyleContextMode mode)
{
    ASSERT(scrollbar || mode == StyleContextMode::Layout);
    if (m_cachedStyleContext)
        return m_cachedStyleContext;

    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());
    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
#if GTK_CHECK_VERSION(3, 19, 2)
    gtk_widget_path_iter_set_object_name(path.get(), -1, "scrollbar");
    if (m_usesOverlayScrollbars) {
        gtk_widget_path_iter_add_class(path.get(), -1, "overlay-indicator");
        if (mode == StyleContextMode::Layout || scrollbar->hoveredPart() != NoPart)
            gtk_widget_path_iter_add_class(path.get(), -1, "hovering");
    }
#else
    gtk_widget_path_iter_add_class(path.get(), -1, "scrollbar");
#endif
    gtk_widget_path_iter_add_class(path.get(), -1, orientationStyleClass(scrollbar ? scrollbar->orientation() : VerticalScrollbar));
    gtk_style_context_set_path(styleContext.get(), path.get());

    gtk_style_context_get_style(
        styleContext.get(),
        "slider-width", &m_cachedProperties.thumbFatness,
        "trough-border", &m_cachedProperties.troughBorderWidth,
        "stepper-size", &m_cachedProperties.stepperSize,
        "stepper-spacing", &m_cachedProperties.stepperSpacing,
        "trough-under-steppers", &m_cachedProperties.troughUnderSteppers,
        nullptr);

    return styleContext;
}

static GRefPtr<GtkStyleContext> createChildStyleContext(GtkStyleContext* parent, const char* name)
{
    ASSERT(parent);
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_copy(gtk_style_context_get_path(parent)));
    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
#if GTK_CHECK_VERSION(3, 19, 2)
    gtk_widget_path_iter_set_object_name(path.get(), -1, name);
#else
    gtk_widget_path_iter_add_class(path.get(), -1, name);
#endif

    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(styleContext.get(), path.get());
    gtk_style_context_set_parent(styleContext.get(), parent);
    return styleContext;
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
#if GTK_CHECK_VERSION(3, 19, 2)
#if USE(COORDINATED_GRAPHICS_THREADED)
    m_usesOverlayScrollbars = true;
#else
    // FIXME: Disable overlay scrollbars by default for now due to bug #153404.
    // Invert the logic here once bug #153404 is fixed.
    m_usesOverlayScrollbars = !g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "1");
#endif
#endif
    updateThemeProperties();
}

void ScrollbarThemeGtk::themeChanged()
{
    updateThemeProperties();
}

void ScrollbarThemeGtk::updateThemeProperties()
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext();
    gtk_style_context_get_style(
        styleContext.get(),
        "min-slider-length", &m_minThumbLength,
        "has-backward-stepper", &m_hasBackButtonStartPart,
        "has-forward-stepper", &m_hasForwardButtonEndPart,
        "has-secondary-backward-stepper", &m_hasBackButtonEndPart,
        "has-secondary-forward-stepper", &m_hasForwardButtonStartPart,
        nullptr);
}

IntRect ScrollbarThemeGtk::thumbRect(Scrollbar& scrollbar, const IntRect& unconstrainedTrackRect)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
    IntRect trackRect = constrainTrackRectToTrackPieces(scrollbar, unconstrainedTrackRect);
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - m_cachedProperties.thumbFatness) / 2, thumbLength(scrollbar), m_cachedProperties.thumbFatness);

    // VerticalScrollbar
    return IntRect(trackRect.x() + (trackRect.width() - m_cachedProperties.thumbFatness) / 2, trackRect.y() + thumbPos, m_cachedProperties.thumbFatness, thumbLength(scrollbar));
}

static void adjustRectAccordingToMargin(GtkStyleContext* context, IntRect& rect)
{
    GtkBorder margin;
    gtk_style_context_get_margin(context, gtk_style_context_get_state(context), &margin);
    rect.move(margin.left, margin.top);
    rect.contract(margin.left + margin.right, margin.top + margin.bottom);
}

void ScrollbarThemeGtk::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    GRefPtr<GtkStyleContext> parentStyleContext = getOrCreateStyleContext(&scrollbar, StyleContextMode::Paint);
    // Paint the track background. If the trough-under-steppers property is true, this
    // should be the full size of the scrollbar, but if is false, it should only be the
    // track rect.
    IntRect fullScrollbarRect(rect);
    if (m_cachedProperties.troughUnderSteppers)
        fullScrollbarRect = IntRect(scrollbar.x(), scrollbar.y(), scrollbar.width(), scrollbar.height());

    GRefPtr<GtkStyleContext> styleContext = createChildStyleContext(parentStyleContext.get(), "trough");
    adjustRectAccordingToMargin(styleContext.get(), fullScrollbarRect);
    gtk_render_background(styleContext.get(), context.platformContext()->cr(), fullScrollbarRect.x(), fullScrollbarRect.y(), fullScrollbarRect.width(), fullScrollbarRect.height());
    gtk_render_frame(styleContext.get(), context.platformContext()->cr(), fullScrollbarRect.x(), fullScrollbarRect.y(), fullScrollbarRect.width(), fullScrollbarRect.height());
}

void ScrollbarThemeGtk::paintScrollbarBackground(GraphicsContext& context, Scrollbar& scrollbar)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar, StyleContextMode::Paint);
    gtk_render_frame(styleContext.get(), context.platformContext()->cr(), scrollbar.x(), scrollbar.y(), scrollbar.width(), scrollbar.height());
}

void ScrollbarThemeGtk::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    ScrollbarOrientation orientation = scrollbar.orientation();
    GRefPtr<GtkStyleContext> parentStyleContext = getOrCreateStyleContext(&scrollbar, StyleContextMode::Paint);
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(parentStyleContext.get(), "trough");
    GRefPtr<GtkStyleContext> styleContext = createChildStyleContext(troughStyleContext.get(), "slider");

    unsigned flags = 0;
    if (scrollbar.pressedPart() == ThumbPart)
        flags |= GTK_STATE_FLAG_ACTIVE;
    if (scrollbar.hoveredPart() == ThumbPart)
        flags |= GTK_STATE_FLAG_PRELIGHT;
    gtk_style_context_set_state(styleContext.get(), static_cast<GtkStateFlags>(flags));

    IntRect thumbRect(rect);
    if (m_usesOverlayScrollbars && scrollbar.hoveredPart() == NoPart) {
        // When using overlay scrollbars we always claim the size of the scrollbar when hovered, so when
        // drawing the indicator we need to adjust the rectangle to its actual size in indicator mode.
        if (orientation == VerticalScrollbar)
            thumbRect.move(scrollbar.width() - m_cachedProperties.thumbFatness, 0);
        else
            thumbRect.move(0, scrollbar.height() - m_cachedProperties.thumbFatness);
    }
    adjustRectAccordingToMargin(styleContext.get(), thumbRect);
    gtk_render_slider(styleContext.get(), context.platformContext()->cr(), thumbRect.x(), thumbRect.y(), thumbRect.width(), thumbRect.height(),
        orientation == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
}

void ScrollbarThemeGtk::paintButton(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect, ScrollbarPart part)
{
    ScrollbarOrientation orientation = scrollbar.orientation();
    GRefPtr<GtkStyleContext> parentStyleContext = getOrCreateStyleContext(&scrollbar, StyleContextMode::Paint);
    GRefPtr<GtkStyleContext> styleContext = createChildStyleContext(parentStyleContext.get(), "button");

    unsigned flags = 0;
    if ((BackButtonStartPart == part && scrollbar.currentPos())
        || (BackButtonEndPart == part && scrollbar.currentPos())
        || (ForwardButtonEndPart == part && scrollbar.currentPos() != scrollbar.maximum())
        || (ForwardButtonStartPart == part && scrollbar.currentPos() != scrollbar.maximum())) {
        if (part == scrollbar.pressedPart())
            flags |= GTK_STATE_FLAG_ACTIVE;
        if (part == scrollbar.hoveredPart())
            flags |= GTK_STATE_FLAG_PRELIGHT;
    } else
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    gtk_style_context_set_state(styleContext.get(), static_cast<GtkStateFlags>(flags));

    gtk_render_background(styleContext.get(), context.platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(styleContext.get(), context.platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gfloat arrowScaling;
    gtk_style_context_get_style(styleContext.get(), "arrow-scaling", &arrowScaling, nullptr);

    double arrowSize = std::min(rect.width(), rect.height()) * arrowScaling;
    FloatPoint arrowPoint(
        rect.x() + (rect.width() - arrowSize) / 2,
        rect.y() + (rect.height() - arrowSize) / 2);

    if (flags & GTK_STATE_FLAG_ACTIVE) {
        gint arrowDisplacementX, arrowDisplacementY;
        gtk_style_context_get_style(styleContext.get(), "arrow-displacement-x", &arrowDisplacementX, "arrow-displacement-y", &arrowDisplacementY, nullptr);
        arrowPoint.move(arrowDisplacementX, arrowDisplacementY);
    }

    gdouble angle;
    if (orientation == VerticalScrollbar)
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI : 0;
    else
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI / 2 : 3 * (G_PI / 2);

    gtk_render_arrow(styleContext.get(), context.platformContext()->cr(), angle, arrowPoint.x(), arrowPoint.y(), arrowSize);
}

bool ScrollbarThemeGtk::paint(Scrollbar& scrollbar, GraphicsContext& graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext.paintingDisabled())
        return false;

    double opacity = scrollbar.hoveredPart() == NoPart ? scrollbar.opacity() : 1;
    if (!opacity)
        return true;

    // Cache a new GtkStyleContext for the duration of this scope.
    TemporaryChange<GRefPtr<GtkStyleContext>> tempStyleContext(m_cachedStyleContext, getOrCreateStyleContext(&scrollbar, StyleContextMode::Paint));

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

    if (m_cachedProperties.troughUnderSteppers && (scrollMask & BackButtonStartPart
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

    if (m_usesOverlayScrollbars) {
        // FIXME: Remove this check once 3.20 is released.
#if GTK_CHECK_VERSION(3, 19, 8)
        unsigned flags = 0;
        if (scrollbar.hoveredPart() != NoPart)
            flags |= GTK_STATE_FLAG_PRELIGHT;
        if (scrollbar.pressedPart() != NoPart)
            flags |= GTK_STATE_FLAG_ACTIVE;
        double styleOpacity;
        gtk_style_context_set_state(m_cachedStyleContext.get(), static_cast<GtkStateFlags>(flags));
        gtk_style_context_get(m_cachedStyleContext.get(), gtk_style_context_get_state(m_cachedStyleContext.get()), "opacity", &styleOpacity, nullptr);
        opacity *= styleOpacity;
#else
        opacity *= scrollbar.hoveredPart() == NoPart ? 0.4 : 0.7;
#endif
    }
    if (!opacity)
        return true;

    if (opacity != 1) {
        graphicsContext.save();
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    ScrollbarControlPartMask allButtons = BackButtonStartPart | BackButtonEndPart | ForwardButtonStartPart | ForwardButtonEndPart;
    if (scrollMask & TrackBGPart || scrollMask & ThumbPart || scrollMask & allButtons)
        paintScrollbarBackground(graphicsContext, scrollbar);
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

    if (opacity != 1) {
        graphicsContext.endTransparencyLayer();
        graphicsContext.restore();
    }

    return true;
}

bool ScrollbarThemeGtk::shouldCenterOnThumb(Scrollbar&, const PlatformMouseEvent& event)
{
    return (event.shiftKey() && event.button() == LeftButton) || (event.button() == MiddleButton);
}

int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext();
    return m_cachedProperties.thumbFatness + (m_cachedProperties.troughBorderWidth * 2);
}

IntSize ScrollbarThemeGtk::buttonSize(Scrollbar& scrollbar)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
    if (scrollbar.orientation() == VerticalScrollbar)
        return IntSize(m_cachedProperties.thumbFatness, m_cachedProperties.stepperSize);

    // HorizontalScrollbar
    return IntSize(m_cachedProperties.stepperSize, m_cachedProperties.thumbFatness);
}

int ScrollbarThemeGtk::minimumThumbLength(Scrollbar&)
{
    return m_minThumbLength;
}
#endif // GTK_API_VERSION_2

}
