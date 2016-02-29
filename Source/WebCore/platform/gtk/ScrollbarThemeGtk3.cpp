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

#ifndef GTK_API_VERSION_2

#include "GRefPtrGtk.h"
#include "PlatformContextCairo.h"
#include "PlatformMouseEvent.h"
#include "ScrollView.h"
#include "Scrollbar.h"
#include <gtk/gtk.h>
#include <wtf/gobject/GRefPtr.h>

namespace WebCore {

#if !GTK_CHECK_VERSION(3, 19, 2)
// Currently we use a static GtkStyleContext for GTK+ < 3.19, and a bunch of unique
// GtkStyleContexts for GTK+ >= 3.19. This is crazy and definitely should not be necessary, but I
// couldn't find any other way to not break one version or the other. Ideally one of the two
// people on the planet who really understand GTK+ styles would fix this.
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
    DEFINE_STATIC_LOCAL(ScrollbarStyleContext, styleContext, ());
    return styleContext.context();
}
#endif

void ScrollbarThemeGtk::themeChanged()
{
#if !GTK_CHECK_VERSION(3, 19, 2)
    gtk_style_context_invalidate(gtkScrollbarStyleContext());
#endif
    updateThemeProperties();
}

ScrollbarThemeGtk::ScrollbarThemeGtk()
{
    updateThemeProperties();
}

void ScrollbarThemeGtk::updateThemeProperties()
{
#if GTK_CHECK_VERSION(3, 19, 2)
    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 0, "scrollbar");

    gtk_style_context_set_path(styleContext.get(), path.get());
#else
    GRefPtr<GtkStyleContext> styleContext = gtkScrollbarStyleContext();
#endif
    gtk_style_context_get_style(
        styleContext.get(),
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

#if GTK_CHECK_VERSION(3, 19, 2)
static const char* orientationStyleClass(ScrollbarOrientation orientation)
{
    return orientation == VerticalScrollbar ? "vertical" : "horizontal";
}
#else
static void applyScrollbarStyleContextClasses(GtkStyleContext* context, ScrollbarOrientation orientation)
{
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCROLLBAR);
    gtk_style_context_add_class(context, orientation == VerticalScrollbar ?  GTK_STYLE_CLASS_VERTICAL : GTK_STYLE_CLASS_HORIZONTAL);
}
#endif

static void adjustRectAccordingToMargin(GtkStyleContext* context, GtkStateFlags state, IntRect& rect)
{
    GtkBorder margin;
    gtk_style_context_set_state(context, state);
    gtk_style_context_get_margin(context, gtk_style_context_get_state(context), &margin);
    rect.move(margin.left, margin.right);
    rect.contract(margin.left + margin.right, margin.top + margin.bottom);
}

void ScrollbarThemeGtk::paintTrackBackground(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
    // Paint the track background. If the trough-under-steppers property is true, this
    // should be the full size of the scrollbar, but if is false, it should only be the
    // track rect.
    IntRect fullScrollbarRect(rect);
    if (m_troughUnderSteppers)
        fullScrollbarRect = IntRect(scrollbar->x(), scrollbar->y(), scrollbar->width(), scrollbar->height());

#if GTK_CHECK_VERSION(3, 19, 2)
    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 0, "scrollbar");
    gtk_widget_path_iter_add_class(path.get(), 0, orientationStyleClass(scrollbar->orientation()));

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 1, "trough");

    gtk_style_context_set_path(styleContext.get(), path.get());
#else
    GRefPtr<GtkStyleContext> styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext.get());

    applyScrollbarStyleContextClasses(styleContext.get(), scrollbar->orientation());
    gtk_style_context_add_class(styleContext.get(), GTK_STYLE_CLASS_TROUGH);
#endif

    adjustRectAccordingToMargin(styleContext.get(), static_cast<GtkStateFlags>(0), fullScrollbarRect);
    gtk_render_background(styleContext.get(), context->platformContext()->cr(), fullScrollbarRect.x(), fullScrollbarRect.y(), fullScrollbarRect.width(), fullScrollbarRect.height());
    gtk_render_frame(styleContext.get(), context->platformContext()->cr(), fullScrollbarRect.x(), fullScrollbarRect.y(), fullScrollbarRect.width(), fullScrollbarRect.height());

#if !GTK_CHECK_VERSION(3, 19, 2)
    gtk_style_context_restore(styleContext.get());
#endif
}

void ScrollbarThemeGtk::paintScrollbarBackground(GraphicsContext* context, ScrollbarThemeClient* scrollbar)
{
#if GTK_CHECK_VERSION(3, 19, 2)
    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 0, "scrollbar");
    gtk_widget_path_iter_add_class(path.get(), 0, orientationStyleClass(scrollbar->orientation()));

    gtk_style_context_set_path(styleContext.get(), path.get());
#else
    GRefPtr<GtkStyleContext> styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext.get());

    applyScrollbarStyleContextClasses(styleContext.get(), scrollbar->orientation());
    gtk_style_context_add_class(styleContext.get(), "scrolled-window");
#endif

    gtk_render_frame(styleContext.get(), context->platformContext()->cr(), scrollbar->x(), scrollbar->y(), scrollbar->width(), scrollbar->height());

#if !GTK_CHECK_VERSION(3, 19, 2)
    gtk_style_context_restore(styleContext.get());
#endif
}

void ScrollbarThemeGtk::paintThumb(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect)
{
#if GTK_CHECK_VERSION(3, 19, 2)
    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 0, "scrollbar");
    gtk_widget_path_iter_add_class(path.get(), 0, orientationStyleClass(scrollbar->orientation()));

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 1, "trough");

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 2, "slider");

    gtk_style_context_set_path(styleContext.get(), path.get());
#else
    GRefPtr<GtkStyleContext> styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext.get());
#endif

    ScrollbarOrientation orientation = scrollbar->orientation();
#if !GTK_CHECK_VERSION(3, 19, 2)
    applyScrollbarStyleContextClasses(styleContext.get(), orientation);
    gtk_style_context_add_class(styleContext.get(), GTK_STYLE_CLASS_SLIDER);
#endif

    guint flags = 0;
    if (scrollbar->pressedPart() == ThumbPart)
        flags |= GTK_STATE_FLAG_ACTIVE;
    if (scrollbar->hoveredPart() == ThumbPart)
        flags |= GTK_STATE_FLAG_PRELIGHT;
    gtk_style_context_set_state(styleContext.get(), static_cast<GtkStateFlags>(flags));

    IntRect thumbRect(rect);
    adjustRectAccordingToMargin(styleContext.get(), static_cast<GtkStateFlags>(flags), thumbRect);
    gtk_render_slider(styleContext.get(), context->platformContext()->cr(), thumbRect.x(), thumbRect.y(), thumbRect.width(), thumbRect.height(),
        orientation == VerticalScrollbar ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);

#if !GTK_CHECK_VERSION(3, 19, 2)
    gtk_style_context_restore(styleContext.get());
#endif
}

void ScrollbarThemeGtk::paintButton(GraphicsContext* context, ScrollbarThemeClient* scrollbar, const IntRect& rect, ScrollbarPart part)
{
#if GTK_CHECK_VERSION(3, 19, 2)
    GRefPtr<GtkStyleContext> styleContext = adoptGRef(gtk_style_context_new());
    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_new());

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 0, "scrollbar");
    gtk_widget_path_iter_add_class(path.get(), 0, orientationStyleClass(scrollbar->orientation()));

    gtk_widget_path_append_type(path.get(), GTK_TYPE_SCROLLBAR);
    gtk_widget_path_iter_set_object_name(path.get(), 1, "button");

    gtk_style_context_set_path(styleContext.get(), path.get());
#else
    GRefPtr<GtkStyleContext> styleContext = gtkScrollbarStyleContext();
    gtk_style_context_save(styleContext.get());
#endif

    ScrollbarOrientation orientation = scrollbar->orientation();
#if !GTK_CHECK_VERSION(3, 19, 2)
    applyScrollbarStyleContextClasses(styleContext.get(), orientation);
#endif

    guint flags = 0;
    if ((BackButtonStartPart == part && scrollbar->currentPos())
        || (BackButtonEndPart == part && scrollbar->currentPos())
        || (ForwardButtonEndPart == part && scrollbar->currentPos() != scrollbar->maximum())
        || (ForwardButtonStartPart == part && scrollbar->currentPos() != scrollbar->maximum())) {
        if (part == scrollbar->pressedPart())
            flags |= GTK_STATE_FLAG_ACTIVE;
        if (part == scrollbar->hoveredPart())
            flags |= GTK_STATE_FLAG_PRELIGHT;
    } else
        flags |= GTK_STATE_FLAG_INSENSITIVE;
    gtk_style_context_set_state(styleContext.get(), static_cast<GtkStateFlags>(flags));

#if !GTK_CHECK_VERSION(3, 19, 2)
    gtk_style_context_add_class(styleContext.get(), GTK_STYLE_CLASS_BUTTON);
#endif
    gtk_render_background(styleContext.get(), context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());
    gtk_render_frame(styleContext.get(), context->platformContext()->cr(), rect.x(), rect.y(), rect.width(), rect.height());

    gfloat arrowScaling;
    gtk_style_context_get_style(styleContext.get(), "arrow-scaling", &arrowScaling, nullptr);

    double arrowSize = std::min(rect.width(), rect.height()) * arrowScaling;
    FloatPoint arrowPoint(rect.x() + (rect.width() - arrowSize) / 2,
                          rect.y() + (rect.height() - arrowSize) / 2);

    if (flags & GTK_STATE_FLAG_ACTIVE) {
        gint arrowDisplacementX, arrowDisplacementY;
        gtk_style_context_get_style(styleContext.get(), "arrow-displacement-x", &arrowDisplacementX, "arrow-displacement-y", &arrowDisplacementY, nullptr);
        arrowPoint.move(arrowDisplacementX, arrowDisplacementY);
    }

    gdouble angle;
    if (orientation == VerticalScrollbar) {
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI : 0;
    } else {
        angle = (part == ForwardButtonEndPart || part == ForwardButtonStartPart) ? G_PI / 2 : 3 * (G_PI / 2);
    }

    gtk_render_arrow(styleContext.get(), context->platformContext()->cr(), angle, arrowPoint.x(), arrowPoint.y(), arrowSize);

#if !GTK_CHECK_VERSION(3, 19, 2)
    gtk_style_context_restore(styleContext.get());
#endif
}

} // namespace WebCore

#endif // !GTK_API_VERSION_2
