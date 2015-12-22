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

#include "PlatformContextCairo.h"
#include "PlatformMouseEvent.h"
#include "ScrollView.h"
#include "Scrollbar.h"
#include <gtk/gtk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GRefPtr.h>

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

IntRect ScrollbarThemeGtk::backButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool)
{
#ifndef GTK_API_VERSION_2
    if (part == BackButtonEndPart && !m_hasBackButtonEndPart)
        return IntRect();
    if (part == BackButtonStartPart && !m_hasBackButtonStartPart)
        return IntRect();

    int x = scrollbar.x() + m_troughBorderWidth;
    int y = scrollbar.y() + m_troughBorderWidth;
    IntSize size = buttonSize(scrollbar);
    if (part == BackButtonStartPart)
        return IntRect(x, y, size.width(), size.height());

    // BackButtonEndPart (alternate button)
    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(scrollbar.x() + scrollbar.width() - m_troughBorderWidth - (2 * size.width()), y, size.width(), size.height());

    // VerticalScrollbar alternate button
    return IntRect(x, scrollbar.y() + scrollbar.height() - m_troughBorderWidth - (2 * size.height()), size.width(), size.height());
#else
    UNUSED_PARAM(scrollbar);
    UNUSED_PARAM(part);
    return IntRect();
#endif
}

IntRect ScrollbarThemeGtk::forwardButtonRect(Scrollbar& scrollbar, ScrollbarPart part, bool)
{
#ifndef GTK_API_VERSION_2
    if (part == ForwardButtonStartPart && !m_hasForwardButtonStartPart)
        return IntRect();
    if (part == ForwardButtonEndPart && !m_hasForwardButtonEndPart)
        return IntRect();

    IntSize size = buttonSize(scrollbar);
    if (scrollbar.orientation() == HorizontalScrollbar) {
        int y = scrollbar.y() + m_troughBorderWidth;
        if (part == ForwardButtonEndPart)
            return IntRect(scrollbar.x() + scrollbar.width() - size.width() - m_troughBorderWidth, y, size.width(), size.height());

        // ForwardButtonStartPart (alternate button)
        return IntRect(scrollbar.x() + m_troughBorderWidth + size.width(), y, size.width(), size.height());
    }

    // VerticalScrollbar
    int x = scrollbar.x() + m_troughBorderWidth;
    if (part == ForwardButtonEndPart)
        return IntRect(x, scrollbar.y() + scrollbar.height() - size.height() - m_troughBorderWidth, size.width(), size.height());

    // ForwardButtonStartPart (alternate button)
    return IntRect(x, scrollbar.y() + m_troughBorderWidth + size.height(), size.width(), size.height());
#else
    UNUSED_PARAM(scrollbar);
    UNUSED_PARAM(part);
    return IntRect();
#endif
}

IntRect ScrollbarThemeGtk::trackRect(Scrollbar& scrollbar, bool)
{
#ifndef GTK_API_VERSION_2
    // The padding along the thumb movement axis includes the trough border
    // plus the size of stepper spacing (the space between the stepper and
    // the place where the thumb stops). There is often no stepper spacing.
    int movementAxisPadding = m_troughBorderWidth + m_stepperSpacing;

    // The fatness of the scrollbar on the non-movement axis.
    int thickness = scrollbarThickness(scrollbar.controlSize());

    int startButtonsOffset = 0;
    int buttonsWidth = 0;
    if (m_hasForwardButtonStartPart) {
        startButtonsOffset += m_stepperSize;
        buttonsWidth += m_stepperSize;
    }
    if (m_hasBackButtonStartPart) {
        startButtonsOffset += m_stepperSize;
        buttonsWidth += m_stepperSize;
    }
    if (m_hasBackButtonEndPart)
        buttonsWidth += m_stepperSize;
    if (m_hasForwardButtonEndPart)
        buttonsWidth += m_stepperSize;

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
    return IntRect();
#endif
}

#ifndef GTK_API_VERSION_2
class ScrollbarStyleContext {
    WTF_MAKE_NONCOPYABLE(ScrollbarStyleContext); WTF_MAKE_FAST_ALLOCATED;
public:
    ScrollbarStyleContext()
        : m_context(adoptGRef(gtk_style_context_new()))
    {
        GtkWidgetPath* path = gtk_widget_path_new();
        gtk_widget_path_append_type(path, GTK_TYPE_SCROLLBAR);
        gtk_widget_path_iter_add_class(path, 0, GTK_STYLE_CLASS_SCROLLBAR);
        gtk_style_context_set_path(m_context.get(), path);
        gtk_widget_path_free(path);
    }

    ~ScrollbarStyleContext()
    {
    }

    GtkStyleContext* context() const { return m_context.get(); }

private:
    GRefPtr<GtkStyleContext> m_context;
};

static GtkStyleContext* gtkScrollbarStyleContext()
{
    static NeverDestroyed<ScrollbarStyleContext> styleContext;
    return styleContext.get().context();
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
    updateThemeProperties();
}

void ScrollbarThemeGtk::themeChanged()
{
    gtk_style_context_invalidate(gtkScrollbarStyleContext());
    updateThemeProperties();
}

void ScrollbarThemeGtk::updateThemeProperties()
{
    gtk_style_context_get_style(
        gtkScrollbarStyleContext(),
        "min-slider-length", &m_minThumbLength,
        "slider-width", &m_thumbFatness,
        "trough-border", &m_troughBorderWidth,
        "stepper-size", &m_stepperSize,
        "stepper-spacing", &m_stepperSpacing,
        "trough-under-steppers", &m_troughUnderSteppers,
        "has-backward-stepper", &m_hasBackButtonStartPart,
        "has-forward-stepper", &m_hasForwardButtonEndPart,
        "has-secondary-backward-stepper", &m_hasBackButtonEndPart,
        "has-secondary-forward-stepper", &m_hasForwardButtonStartPart,
        nullptr);
    updateScrollbarsFrameThickness();
}

typedef HashSet<Scrollbar*> ScrollbarMap;

static ScrollbarMap& scrollbarMap()
{
    static NeverDestroyed<ScrollbarMap> map;
    return map;
}

void ScrollbarThemeGtk::registerScrollbar(Scrollbar& scrollbar)
{
    scrollbarMap().add(&scrollbar);
}

void ScrollbarThemeGtk::unregisterScrollbar(Scrollbar& scrollbar)
{
    scrollbarMap().remove(&scrollbar);
}

void ScrollbarThemeGtk::updateScrollbarsFrameThickness()
{
    if (scrollbarMap().isEmpty())
        return;

    // Update the thickness of every interior frame scrollbar widget. The
    // platform-independent scrollbar them code isn't yet smart enough to get
    // this information when it paints.
    for (const auto& scrollbar : scrollbarMap()) {
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

IntRect ScrollbarThemeGtk::thumbRect(Scrollbar& scrollbar, const IntRect& unconstrainedTrackRect)
{
    IntRect trackRect = constrainTrackRectToTrackPieces(scrollbar, unconstrainedTrackRect);
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar.orientation() == HorizontalScrollbar)
        return IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - m_thumbFatness) / 2, thumbLength(scrollbar), m_thumbFatness); 

    // VerticalScrollbar
    return IntRect(trackRect.x() + (trackRect.width() - m_thumbFatness) / 2, trackRect.y() + thumbPos, m_thumbFatness, thumbLength(scrollbar));
}

static void applyScrollbarStyleContextClasses(GtkStyleContext* context, ScrollbarOrientation orientation)
{
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCROLLBAR);
    gtk_style_context_add_class(context, orientation == VerticalScrollbar ?  GTK_STYLE_CLASS_VERTICAL : GTK_STYLE_CLASS_HORIZONTAL);
}

static void adjustRectAccordingToMargin(GtkStyleContext* context, GtkStateFlags state, IntRect& rect)
{
    GtkBorder margin;
    gtk_style_context_set_state(context, state);
    gtk_style_context_get_margin(context, gtk_style_context_get_state(context), &margin);
    rect.move(margin.left, margin.right);
    rect.contract(margin.left + margin.right, margin.top + margin.bottom);
}

void ScrollbarThemeGtk::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    // Paint the track background. If the trough-under-steppers property is true, this
    // should be the full size of the scrollbar, but if is false, it should only be the
    // track rect.
    IntRect fullScrollbarRect(rect);
    if (m_troughUnderSteppers)
        fullScrollbarRect = IntRect(scrollbar.x(), scrollbar.y(), scrollbar.width(), scrollbar.height());

    GtkStyleContext* styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext);

    applyScrollbarStyleContextClasses(styleContext, scrollbar.orientation());
    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_TROUGH);

    adjustRectAccordingToMargin(styleContext, static_cast<GtkStateFlags>(0), fullScrollbarRect);
    gtk_render_background(styleContext, context.platformContext()->cr(), fullScrollbarRect.x(), fullScrollbarRect.y(), fullScrollbarRect.width(), fullScrollbarRect.height());
    gtk_render_frame(styleContext, context.platformContext()->cr(), fullScrollbarRect.x(), fullScrollbarRect.y(), fullScrollbarRect.width(), fullScrollbarRect.height());

    gtk_style_context_restore(styleContext);
}

void ScrollbarThemeGtk::paintScrollbarBackground(GraphicsContext& context, Scrollbar& scrollbar)
{
    GtkStyleContext* styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext);

    applyScrollbarStyleContextClasses(styleContext, scrollbar.orientation());
    gtk_style_context_add_class(styleContext, "scrolled-window");
    gtk_render_frame(styleContext, context.platformContext()->cr(), scrollbar.x(), scrollbar.y(), scrollbar.width(), scrollbar.height());

    gtk_style_context_restore(styleContext);
}

void ScrollbarThemeGtk::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    GtkStyleContext* styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext);

    ScrollbarOrientation orientation = scrollbar.orientation();
    applyScrollbarStyleContextClasses(styleContext, orientation);
    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_SLIDER);

    guint flags = 0;
    if (scrollbar.pressedPart() == ThumbPart)
        flags |= GTK_STATE_FLAG_ACTIVE;
    if (scrollbar.hoveredPart() == ThumbPart)
        flags |= GTK_STATE_FLAG_PRELIGHT;
    gtk_style_context_set_state(styleContext, static_cast<GtkStateFlags>(flags));

    IntRect thumbRect(rect);
    adjustRectAccordingToMargin(styleContext, static_cast<GtkStateFlags>(flags), thumbRect);
    gtk_render_slider(styleContext, context.platformContext()->cr(), thumbRect.x(), thumbRect.y(), thumbRect.width(), thumbRect.height(),
        orientation == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);

    gtk_style_context_restore(styleContext);
}

void ScrollbarThemeGtk::paintButton(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect, ScrollbarPart part)
{
    GtkStyleContext* styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext);

    ScrollbarOrientation orientation = scrollbar.orientation();
    applyScrollbarStyleContextClasses(styleContext, orientation);

    guint flags = 0;
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
    gtk_style_context_set_state(styleContext, static_cast<GtkStateFlags>(flags));

    gtk_style_context_add_class(styleContext, GTK_STYLE_CLASS_BUTTON);
    gtk_render_background(styleContext, context.platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(styleContext, context.platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gfloat arrowScaling;
    gtk_style_context_get_style(styleContext, "arrow-scaling", &arrowScaling, nullptr);

    double arrowSize = std::min(rect.width(), rect.height()) * arrowScaling;
    FloatPoint arrowPoint(
        rect.x() + (rect.width() - arrowSize) / 2,
        rect.y() + (rect.height() - arrowSize) / 2);

    if (flags & GTK_STATE_FLAG_ACTIVE) {
        gint arrowDisplacementX, arrowDisplacementY;
        gtk_style_context_get_style(styleContext, "arrow-displacement-x", &arrowDisplacementX, "arrow-displacement-y", &arrowDisplacementY, nullptr);
        arrowPoint.move(arrowDisplacementX, arrowDisplacementY);
    }

    gdouble angle;
    if (orientation == VerticalScrollbar)
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI : 0;
    else
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI / 2 : 3 * (G_PI / 2);

    gtk_render_arrow(styleContext, context.platformContext()->cr(), angle, arrowPoint.x(), arrowPoint.y(), arrowSize);

    gtk_style_context_restore(styleContext);
}

bool ScrollbarThemeGtk::paint(Scrollbar& scrollbar, GraphicsContext& graphicsContext, const IntRect& damageRect)
{
    if (graphicsContext.paintingDisabled())
        return false;

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

    return true;
}

bool ScrollbarThemeGtk::shouldCenterOnThumb(Scrollbar&, const PlatformMouseEvent& event)
{
    return (event.shiftKey() && event.button() == LeftButton) || (event.button() == MiddleButton);
}

int ScrollbarThemeGtk::scrollbarThickness(ScrollbarControlSize)
{
    return m_thumbFatness + (m_troughBorderWidth * 2);
}

IntSize ScrollbarThemeGtk::buttonSize(Scrollbar& scrollbar)
{
    if (scrollbar.orientation() == VerticalScrollbar)
        return IntSize(m_thumbFatness, m_stepperSize);

    // HorizontalScrollbar
    return IntSize(m_stepperSize, m_thumbFatness);
}

int ScrollbarThemeGtk::minimumThumbLength(Scrollbar&)
{
    return m_minThumbLength;
}
#endif // GTK_API_VERSION_2

}
