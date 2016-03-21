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
    GtkBorder troughBorder;
    getTroughBorder(scrollbar, &troughBorder);
    int x = scrollbar.x() + troughBorder.left;
    int y = scrollbar.y() + troughBorder.top;
    IntSize size = buttonSize(scrollbar, part);
    if (part == BackButtonStartPart)
        return IntRect(x, y, size.width(), size.height());

    // BackButtonEndPart (alternate button)
    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(scrollbar.x() + scrollbar.width() - troughBorder.left - (2 * size.width()), y, size.width(), size.height());

    // VerticalScrollbar alternate button
    return IntRect(x, scrollbar.y() + scrollbar.height() - troughBorder.top - (2 * size.height()), size.width(), size.height());
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
    GtkBorder troughBorder;
    getTroughBorder(scrollbar, &troughBorder);
    IntSize size = buttonSize(scrollbar, part);
    if (scrollbar.orientation() == HorizontalScrollbar) {
        int y = scrollbar.y() + troughBorder.top;
        if (part == ForwardButtonEndPart)
            return IntRect(scrollbar.x() + scrollbar.width() - size.width() - troughBorder.left, y, size.width(), size.height());

        // ForwardButtonStartPart (alternate button)
        return IntRect(scrollbar.x() + troughBorder.left + size.width(), y, size.width(), size.height());
    }

    // VerticalScrollbar
    int x = scrollbar.x() + troughBorder.left;
    if (part == ForwardButtonEndPart)
        return IntRect(x, scrollbar.y() + scrollbar.height() - size.height() - troughBorder.top, size.width(), size.height());

    // ForwardButtonStartPart (alternate button)
    return IntRect(x, scrollbar.y() + troughBorder.top + size.height(), size.width(), size.height());
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
    GtkBorder troughBorder;
    getTroughBorder(scrollbar, &troughBorder);
    GtkBorder stepperSpacing = { 0, 0, 0, 0 };

    // The fatness of the scrollbar on the non-movement axis.
    int thickness = scrollbarThickness(styleContext.get(), scrollbar.orientation());

    int startButtonsOffset = 0;
    int buttonsWidth = 0;
    if (m_hasForwardButtonStartPart) {
        int buttonSize = stepperSize(scrollbar, ForwardButtonStartPart);
        startButtonsOffset += buttonSize;
        buttonsWidth += buttonSize;
    }
    if (m_hasBackButtonStartPart) {
        int buttonSize = stepperSize(scrollbar, BackButtonStartPart);
        startButtonsOffset += buttonSize;
        buttonsWidth += buttonSize;
        GtkBorder margin;
        getStepperSpacing(scrollbar, BackButtonStartPart, &margin);
        stepperSpacing.left += margin.left;
        stepperSpacing.right += margin.right;
        stepperSpacing.top += margin.top;
        stepperSpacing.bottom += margin.bottom;
    }
    if (m_hasBackButtonEndPart)
        buttonsWidth += stepperSize(scrollbar, BackButtonEndPart);
    if (m_hasForwardButtonEndPart) {
        buttonsWidth += stepperSize(scrollbar, ForwardButtonEndPart);
        GtkBorder margin;
        getStepperSpacing(scrollbar, BackButtonStartPart, &margin);
        stepperSpacing.left += margin.left;
        stepperSpacing.right += margin.right;
        stepperSpacing.top += margin.top;
        stepperSpacing.bottom += margin.bottom;
    }

    if (scrollbar.orientation() == HorizontalScrollbar) {
        // Once the scrollbar becomes smaller than the natural size of the two buttons and the thumb, the track disappears.
        if (scrollbar.width() < buttonsWidth + minimumThumbLength(scrollbar))
            return IntRect();
        return IntRect(scrollbar.x() + troughBorder.left + stepperSpacing.left + startButtonsOffset, scrollbar.y(),
            scrollbar.width() - (troughBorder.left + troughBorder.right) - (stepperSpacing.left + stepperSpacing.right) - buttonsWidth, thickness);
    }

    if (scrollbar.height() < buttonsWidth + minimumThumbLength(scrollbar))
        return IntRect();
    return IntRect(scrollbar.x(), scrollbar.y() + troughBorder.top + stepperSpacing.top + startButtonsOffset,
        thickness, scrollbar.height() - (troughBorder.top + troughBorder.bottom) - (stepperSpacing.top + stepperSpacing.bottom) - buttonsWidth);
#else
    UNUSED_PARAM(scrollbar);
    UNUSED_PARAM(painting);
    return IntRect();
#endif
}

#ifndef GTK_API_VERSION_2
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
    ScrollbarOrientation orientation = scrollbar ? scrollbar->orientation() : VerticalScrollbar;
    gtk_widget_path_iter_add_class(path.get(), -1, orientation == VerticalScrollbar ? "vertical" : "horizontal");
    gtk_widget_path_iter_add_class(path.get(), -1, orientation == VerticalScrollbar ? "right" : "bottom");
    gtk_style_context_set_path(styleContext.get(), path.get());

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
    gtk_widget_path_iter_add_class(path.get(), -1, GTK_STYLE_CLASS_SCROLLBAR);
    gtk_widget_path_iter_add_class(path.get(), -1, name);
#endif

    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(styleContext.get(), path.get());
    gtk_style_context_set_parent(styleContext.get(), parent);
    return styleContext;
}

static void themeChangedCallback()
{
    ScrollbarTheme::theme().themeChanged();
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
#if GTK_CHECK_VERSION(3, 19, 2)
#if USE(COORDINATED_GRAPHICS_THREADED)
    m_usesOverlayScrollbars = true;
#else
    m_usesOverlayScrollbars = g_strcmp0(g_getenv("GTK_OVERLAY_SCROLLING"), "0");
#endif
#endif
    static bool themeMonitorInitialized = false;
    if (!themeMonitorInitialized) {
        g_signal_connect_swapped(gtk_settings_get_default(), "notify::gtk-theme-name", G_CALLBACK(themeChangedCallback), nullptr);
        themeMonitorInitialized = true;
    }

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
    int thumbFat = thumbFatness(scrollbar);
    GtkBorder troughBorder = { 0, 0, 0, 0 };
#if GTK_CHECK_VERSION(3, 19, 11)
    getTroughBorder(scrollbar, &troughBorder);
#endif

    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(trackRect.x() + thumbPos, trackRect.y() + troughBorder.top + (trackRect.height() - thumbFat) / 2, thumbLength(scrollbar), thumbFat);

    // VerticalScrollbar
    return IntRect(trackRect.x() + troughBorder.left + (trackRect.width() - thumbFat) / 2, trackRect.y() + thumbPos, thumbFat, thumbLength(scrollbar));
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
    if (troughUnderSteppers(scrollbar))
        fullScrollbarRect = IntRect(scrollbar.x(), scrollbar.y(), scrollbar.width(), scrollbar.height());

    IntRect adjustedRect = fullScrollbarRect;
    adjustRectAccordingToMargin(parentStyleContext.get(), adjustedRect);
    gtk_render_background(parentStyleContext.get(), context.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
    gtk_render_frame(parentStyleContext.get(), context.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());

    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(parentStyleContext.get(), "contents");
    adjustedRect = fullScrollbarRect;
    adjustRectAccordingToMargin(contentsStyleContext.get(), adjustedRect);
    gtk_render_background(contentsStyleContext.get(), context.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
    gtk_render_frame(contentsStyleContext.get(), context.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());

    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(contentsStyleContext.get(), "trough");
    adjustedRect = fullScrollbarRect;
    adjustRectAccordingToMargin(troughStyleContext.get(), adjustedRect);
    gtk_render_background(troughStyleContext.get(), context.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
    gtk_render_frame(troughStyleContext.get(), context.platformContext()->cr(), adjustedRect.x(), adjustedRect.y(), adjustedRect.width(), adjustedRect.height());
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
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(parentStyleContext.get(), "contents");
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(contentsStyleContext.get(), "trough");
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
            thumbRect.move(scrollbar.width() - scrollbarThickness(parentStyleContext.get(), orientation), 0);
        else
            thumbRect.move(0, scrollbar.height() - scrollbarThickness(parentStyleContext.get(), orientation));
    }
    adjustRectAccordingToMargin(styleContext.get(), thumbRect);
    gtk_render_slider(styleContext.get(), context.platformContext()->cr(), thumbRect.x(), thumbRect.y(), thumbRect.width(), thumbRect.height(),
        orientation == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
}

void ScrollbarThemeGtk::paintButton(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect, ScrollbarPart part)
{
    ScrollbarOrientation orientation = scrollbar.orientation();
    GRefPtr<GtkStyleContext> parentStyleContext = getOrCreateStyleContext(&scrollbar, StyleContextMode::Paint);
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(parentStyleContext.get(), "contents");
    GRefPtr<GtkStyleContext> styleContext = createChildStyleContext(contentsStyleContext.get(), "button");

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

    if (troughUnderSteppers(scrollbar) && (scrollMask & BackButtonStartPart
            || scrollMask & BackButtonEndPart
            || scrollMask & ForwardButtonStartPart
            || scrollMask & ForwardButtonEndPart))
        scrollMask |= TrackBGPart;

    IntRect currentThumbRect;
    if (hasThumb(scrollbar)) {
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
    if (!opacity || scrollMask == NoPart)
        return true;

    if (opacity != 1) {
        graphicsContext.save();
        graphicsContext.clip(damageRect);
        graphicsContext.beginTransparencyLayer(opacity);
    }

    ScrollbarControlPartMask allButtons = BackButtonStartPart | BackButtonEndPart | ForwardButtonStartPart | ForwardButtonEndPart;
    if (scrollMask & TrackBGPart || scrollMask & ThumbPart || scrollMask & allButtons) {
        paintScrollbarBackground(graphicsContext, scrollbar);
        paintTrackBackground(graphicsContext, scrollbar, trackPaintRect);
    }

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

ScrollbarButtonPressAction ScrollbarThemeGtk::handleMousePressEvent(Scrollbar&, const PlatformMouseEvent& event, ScrollbarPart pressedPart)
{
    switch (pressedPart) {
    case BackTrackPart:
    case ForwardTrackPart:
        if (event.button() == LeftButton)
            return ScrollbarButtonPressAction::CenterOnThumb;
        if (event.button() == RightButton)
            return ScrollbarButtonPressAction::Scroll;
        break;
    case ThumbPart:
        if (event.button() != RightButton)
            return ScrollbarButtonPressAction::StartDrag;
        break;
    default:
        break;
    }

    return ScrollbarButtonPressAction::None;
}

int ScrollbarThemeGtk::scrollbarThickness(GtkStyleContext* styleContext, ScrollbarOrientation orientation)
{
    GtkBorder troughBorder;
#if GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(styleContext, "contents");
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(contentsStyleContext.get(), "trough");
    GRefPtr<GtkStyleContext> sliderStyleContext = createChildStyleContext(troughStyleContext.get(), "slider");
    int thumbFat = thumbFatness(sliderStyleContext.get(), orientation);
    getTroughBorder(troughStyleContext.get(), &troughBorder);
    // Since GTK+ 3.19 the scrollbar can have its own border too.
    GtkBorder border;
    gtk_style_context_get_border(styleContext, gtk_style_context_get_state(styleContext), &border);
    troughBorder.left += border.left;
    troughBorder.right += border.right;
    troughBorder.top += border.top;
    troughBorder.bottom += border.bottom;
#else
    int thumbFat = thumbFatness(styleContext, orientation);
    getTroughBorder(styleContext, &troughBorder);
#endif
    if (orientation == VerticalScrollbar)
        return thumbFat + troughBorder.left + troughBorder.right;
    return thumbFat + troughBorder.top + troughBorder.bottom;
}

int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext();
    return scrollbarThickness(styleContext.get());
}

IntSize ScrollbarThemeGtk::buttonSize(Scrollbar& scrollbar, ScrollbarPart buttonPart)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
#if GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(styleContext.get(), "contents");
    GRefPtr<GtkStyleContext> buttonStyleContext = createChildStyleContext(contentsStyleContext.get(), "button");
    switch (buttonPart) {
    case BackButtonStartPart:
    case ForwardButtonStartPart:
        gtk_style_context_add_class(buttonStyleContext.get(), "up");
        break;
    case BackButtonEndPart:
    case ForwardButtonEndPart:
        gtk_style_context_add_class(buttonStyleContext.get(), "down");
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    int minWidth = 0, minHeight = 0;
    gtk_style_context_get(buttonStyleContext.get(), gtk_style_context_get_state(buttonStyleContext.get()),
        "min-width", &minWidth, "min-height", &minHeight, nullptr);
    return IntSize(minWidth, minHeight);
#else
    UNUSED_PARAM(buttonPart);
    int stepperSize;
    gtk_style_context_get_style(styleContext.get(), "stepper-size", &stepperSize, nullptr);
    if (scrollbar.orientation() == VerticalScrollbar)
        return IntSize(thumbFatness(scrollbar), stepperSize);

    // HorizontalScrollbar
    return IntSize(stepperSize, thumbFatness(scrollbar));
#endif
}

int ScrollbarThemeGtk::stepperSize(Scrollbar& scrollbar, ScrollbarPart buttonPart)
{
    IntSize size = buttonSize(scrollbar, buttonPart);
    return scrollbar.orientation() == VerticalScrollbar ? size.height() : size.width();
}

void ScrollbarThemeGtk::getStepperSpacing(Scrollbar& scrollbar, ScrollbarPart buttonPart, GtkBorder* margin)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
#if GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(styleContext.get(), "contents");
    GRefPtr<GtkStyleContext> buttonStyleContext = createChildStyleContext(contentsStyleContext.get(), "button");
    switch (buttonPart) {
    case BackButtonStartPart:
    case ForwardButtonStartPart:
        gtk_style_context_add_class(buttonStyleContext.get(), "up");
        break;
    case BackButtonEndPart:
    case ForwardButtonEndPart:
        gtk_style_context_add_class(buttonStyleContext.get(), "down");
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    gtk_style_context_get_margin(buttonStyleContext.get(), gtk_style_context_get_state(buttonStyleContext.get()), margin);
#else
    UNUSED_PARAM(buttonPart);
    int stepperSpacing = 0;
    gtk_style_context_get_style(styleContext.get(), "stepper-spacing", &stepperSpacing, nullptr);
    margin->left = margin->right = margin->top = margin->bottom = stepperSpacing;
#endif
}

bool ScrollbarThemeGtk::troughUnderSteppers(Scrollbar& scrollbar)
{
#if !GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
    gboolean underSteppers;
    gtk_style_context_get_style(styleContext.get(), "trough-under-steppers", &underSteppers, nullptr);
    return underSteppers;
#else
    UNUSED_PARAM(scrollbar);
    // This is now ignored by GTK+ and considered always true.
    return true;
#endif
}
int ScrollbarThemeGtk::minimumThumbLength(Scrollbar& scrollbar)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
    int minThumbLength = 0;
#if GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(styleContext.get(), "contents");
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(contentsStyleContext.get(), "trough");
    GRefPtr<GtkStyleContext> sliderStyleContext = createChildStyleContext(troughStyleContext.get(), "slider");
    gtk_style_context_get(sliderStyleContext.get(), gtk_style_context_get_state(sliderStyleContext.get()),
        scrollbar.orientation() == VerticalScrollbar ? "min-height" : "min-width", &minThumbLength, nullptr);
#else
    gtk_style_context_get_style(styleContext.get(), "min-slider-length", &minThumbLength, nullptr);
#endif
    return minThumbLength;
}

int ScrollbarThemeGtk::thumbFatness(GtkStyleContext* styleContext, ScrollbarOrientation orientation)
{
    int thumbFat = 0;
#if GTK_CHECK_VERSION(3, 19, 11)
    gtk_style_context_get(styleContext, gtk_style_context_get_state(styleContext),
        orientation == VerticalScrollbar ? "min-width" : "min-height", &thumbFat, nullptr);
    GtkBorder margin;
    gtk_style_context_get_margin(styleContext, gtk_style_context_get_state(styleContext), &margin);
    GtkBorder border;
    gtk_style_context_get_border(styleContext, gtk_style_context_get_state(styleContext), &border);
    if (orientation == VerticalScrollbar)
        thumbFat += margin.left + margin.right + border.left + border.right;
    else
        thumbFat += margin.top + margin.bottom + border.top + border.bottom;
#else
    UNUSED_PARAM(orientation);
    gtk_style_context_get_style(styleContext, "slider-width", &thumbFat, nullptr);
#endif
    return thumbFat;
}

int ScrollbarThemeGtk::thumbFatness(Scrollbar& scrollbar)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
#if GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(styleContext.get(), "contents");
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(contentsStyleContext.get(), "trough");
    GRefPtr<GtkStyleContext> sliderStyleContext = createChildStyleContext(troughStyleContext.get(), "slider");
    return thumbFatness(sliderStyleContext.get(), scrollbar.orientation());
#else
    return thumbFatness(styleContext.get(), scrollbar.orientation());
#endif
}

void ScrollbarThemeGtk::getTroughBorder(GtkStyleContext* styleContext, GtkBorder* border)
{
#if GTK_CHECK_VERSION(3, 19, 11)
    gtk_style_context_get_border(styleContext, gtk_style_context_get_state(styleContext), border);
#else
    int troughBorderWidth = 0;
    gtk_style_context_get_style(styleContext, "trough-border", &troughBorderWidth, nullptr);
    border->top = border->bottom = border->left = border->right = troughBorderWidth;
#endif
}

void ScrollbarThemeGtk::getTroughBorder(Scrollbar& scrollbar, GtkBorder* border)
{
    GRefPtr<GtkStyleContext> styleContext = getOrCreateStyleContext(&scrollbar);
#if GTK_CHECK_VERSION(3, 19, 11)
    GRefPtr<GtkStyleContext> contentsStyleContext = createChildStyleContext(styleContext.get(), "contents");
    GRefPtr<GtkStyleContext> troughStyleContext = createChildStyleContext(contentsStyleContext.get(), "trough");
    getTroughBorder(troughStyleContext.get(), border);
    // Since GTK+ 3.19 the scrollbar can have its own border too.
    GtkBorder scrollbarBorder;
    gtk_style_context_get_border(styleContext.get(), gtk_style_context_get_state(styleContext.get()), &scrollbarBorder);
    border->left += scrollbarBorder.left;
    border->right += scrollbarBorder.right;
    border->top += scrollbarBorder.top;
    border->bottom += scrollbarBorder.bottom;
#else
    getTroughBorder(styleContext.get(), border);
#endif
}

#endif // GTK_API_VERSION_2

}
