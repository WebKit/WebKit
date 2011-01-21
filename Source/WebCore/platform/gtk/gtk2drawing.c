/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>  (Original Author)
 *  Pierre Chanial <p_ch@verizon.net>
 *  Michael Ventnor <m.ventnor@gmail.com>
 *  Alp Toker <alp@nuanti.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains painting functions for each of the gtk2 widgets.
 * Adapted from the gtkdrawing.c, and gtk+2.0 source.
 */

#ifdef GTK_API_VERSION_2

#undef GTK_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED

#include <gdk/gdkprivate.h>
#include "gtkdrawing.h"
#include "GtkVersioning.h"
#include <math.h>
#include <string.h>

#define XTHICKNESS(style) (style->xthickness)
#define YTHICKNESS(style) (style->ythickness)

static GtkThemeParts *gParts = NULL;
static style_prop_t style_prop_func;
static gboolean is_initialized;

void
moz_gtk_use_theme_parts(GtkThemeParts* parts)
{
    gParts = parts;
}

/* Because we have such an unconventional way of drawing widgets, signal to the GTK theme engine
   that they are drawing for Mozilla instead of a conventional GTK app so they can do any specific
   things they may want to do. */
static void
moz_gtk_set_widget_name(GtkWidget* widget)
{
    gtk_widget_set_name(widget, "MozillaGtkWidget");
}

gint
moz_gtk_enable_style_props(style_prop_t styleGetProp)
{
    style_prop_func = styleGetProp;
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_window_widget()
{
    if (!gParts->protoWindow) {
        gParts->protoWindow = gtk_window_new(GTK_WINDOW_POPUP);

        if (gParts->colormap)
            gtk_widget_set_colormap(gParts->protoWindow, gParts->colormap);

        gtk_widget_realize(gParts->protoWindow);
        moz_gtk_set_widget_name(gParts->protoWindow);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
setup_widget_prototype(GtkWidget* widget)
{
    ensure_window_widget();
    if (!gParts->protoLayout) {
        gParts->protoLayout = gtk_fixed_new();
        gtk_container_add(GTK_CONTAINER(gParts->protoWindow), gParts->protoLayout);
    }

    gtk_container_add(GTK_CONTAINER(gParts->protoLayout), widget);
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_scrollbar_widget()
{
    if (!gParts->vertScrollbarWidget) {
        gParts->vertScrollbarWidget = gtk_vscrollbar_new(NULL);
        setup_widget_prototype(gParts->vertScrollbarWidget);
    }
    if (!gParts->horizScrollbarWidget) {
        gParts->horizScrollbarWidget = gtk_hscrollbar_new(NULL);
        setup_widget_prototype(gParts->horizScrollbarWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_scrolled_window_widget()
{
    if (!gParts->scrolledWindowWidget) {
        gParts->scrolledWindowWidget = gtk_scrolled_window_new(NULL, NULL);
        setup_widget_prototype(gParts->scrolledWindowWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static GtkStateType
ConvertGtkState(GtkWidgetState* state)
{
    if (state->disabled)
        return GTK_STATE_INSENSITIVE;
    else if (state->depressed)
        return (state->inHover ? GTK_STATE_PRELIGHT : GTK_STATE_ACTIVE);
    else if (state->inHover)
        return (state->active ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT);
    else
        return GTK_STATE_NORMAL;
}

static gint
TSOffsetStyleGCArray(GdkGC** gcs, gint xorigin, gint yorigin)
{
    int i;
    /* there are 5 gc's in each array, for each of the widget states */
    for (i = 0; i < 5; ++i)
        gdk_gc_set_ts_origin(gcs[i], xorigin, yorigin);
    return MOZ_GTK_SUCCESS;
}

static gint
TSOffsetStyleGCs(GtkStyle* style, gint xorigin, gint yorigin)
{
    TSOffsetStyleGCArray(style->fg_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->bg_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->light_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->dark_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->mid_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->text_gc, xorigin, yorigin);
    TSOffsetStyleGCArray(style->base_gc, xorigin, yorigin);
    gdk_gc_set_ts_origin(style->black_gc, xorigin, yorigin);
    gdk_gc_set_ts_origin(style->white_gc, xorigin, yorigin);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_init()
{
    GtkWidgetClass *entry_class;

    is_initialized = TRUE;

    /* Add style property to GtkEntry.
     * Adding the style property to the normal GtkEntry class means that it
     * will work without issues inside GtkComboBox and for Spinbuttons. */
    entry_class = g_type_class_ref(GTK_TYPE_ENTRY);
    gtk_widget_class_install_style_property(entry_class,
        g_param_spec_boolean("honors-transparent-bg-hint",
                             "Transparent BG enabling flag",
                             "If TRUE, the theme is able to draw the GtkEntry on non-prefilled background.",
                             FALSE,
                             G_PARAM_READWRITE));

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrolled_window_paint(GdkDrawable* drawable, GdkRectangle* rect,
                              GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStyle* style;
    GtkAllocation allocation;
    GtkWidget* widget;

    ensure_scrolled_window_widget();
    widget = gParts->scrolledWindowWidget;

    gtk_widget_get_allocation(widget, &allocation);
    allocation.x = rect->x;
    allocation.y = rect->y;
    allocation.width = rect->width;
    allocation.height = rect->height;
    gtk_widget_set_allocation(widget, &allocation);

    style = gtk_widget_get_style(widget);
    TSOffsetStyleGCs(style, rect->x - 1, rect->y - 1);
    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     cliprect, gParts->scrolledWindowWidget, "scrolled_window",
                     rect->x, rect->y, rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkScrollbarButtonFlags flags,
                               GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (state->active) ?
        GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GdkRectangle arrow_rect;
    GtkStyle* style;
    GtkWidget *scrollbar;
    GtkAllocation allocation;
    GtkArrowType arrow_type;
    gint arrow_displacement_x, arrow_displacement_y;
    const char* detail = (flags & MOZ_GTK_STEPPER_VERTICAL) ?
                           "vscrollbar" : "hscrollbar";

    ensure_scrollbar_widget();

    if (flags & MOZ_GTK_STEPPER_VERTICAL)
        scrollbar = gParts->vertScrollbarWidget;
    else
        scrollbar = gParts->horizScrollbarWidget;

    gtk_widget_set_direction(scrollbar, direction);

    /* Some theme engines (i.e., ClearLooks) check the scrollbar's allocation
       to determine where it should paint rounded corners on the buttons.
       We need to trick them into drawing the buttons the way we want them. */

    gtk_widget_get_allocation(scrollbar, &allocation);
    allocation.x = rect->x;
    allocation.y = rect->y;
    allocation.width = rect->width;
    allocation.height = rect->height;

    if (flags & MOZ_GTK_STEPPER_VERTICAL) {
        allocation.height *= 5;
        if (flags & MOZ_GTK_STEPPER_DOWN) {
            arrow_type = GTK_ARROW_DOWN;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                allocation.y -= 4 * rect->height;
            else
                allocation.y -= rect->height;

        } else {
            arrow_type = GTK_ARROW_UP;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                allocation.y -= 3 * rect->height;
        }
    } else {
        allocation.width *= 5;
        if (flags & MOZ_GTK_STEPPER_DOWN) {
            arrow_type = GTK_ARROW_RIGHT;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                allocation.x -= 4 * rect->width;
            else
                allocation.x -= rect->width;
        } else {
            arrow_type = GTK_ARROW_LEFT;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                allocation.x -= 3 * rect->width;
        }
    }

    gtk_widget_set_allocation(scrollbar, &allocation);
    style = gtk_widget_get_style(scrollbar);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  scrollbar, detail, rect->x, rect->y,
                  rect->width, rect->height);

    arrow_rect.width = rect->width / 2;
    arrow_rect.height = rect->height / 2;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;

    if (state_type == GTK_STATE_ACTIVE) {
        gtk_widget_style_get(scrollbar,
                             "arrow-displacement-x", &arrow_displacement_x,
                             "arrow-displacement-y", &arrow_displacement_y,
                             NULL);

        arrow_rect.x += arrow_displacement_x;
        arrow_rect.y += arrow_displacement_y;
    }

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    scrollbar, detail, arrow_type, TRUE, arrow_rect.x,
                    arrow_rect.y, arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_trough_paint(GtkThemeWidgetType widget,
                               GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkTextDirection direction)
{
    GtkStyle* style;
    GtkScrollbar *scrollbar;

    ensure_scrollbar_widget();

    if (widget ==  MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL)
        scrollbar = GTK_SCROLLBAR(gParts->horizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gParts->vertScrollbarWidget);

    gtk_widget_set_direction(GTK_WIDGET(scrollbar), direction);

    style = gtk_widget_get_style(GTK_WIDGET(scrollbar));

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_ACTIVE, GTK_SHADOW_IN, cliprect,
                  GTK_WIDGET(scrollbar), "trough", rect->x, rect->y,
                  rect->width, rect->height);

    if (state->focused) {
        gtk_paint_focus(style, drawable, GTK_STATE_ACTIVE, cliprect,
                        GTK_WIDGET(scrollbar), "trough",
                        rect->x, rect->y, rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_thumb_paint(GtkThemeWidgetType widget,
                              GdkDrawable* drawable, GdkRectangle* rect,
                              GdkRectangle* cliprect, GtkWidgetState* state,
                              GtkTextDirection direction)
{
    GtkStateType state_type = (state->inHover || state->active) ?
        GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;
    GtkShadowType shadow_type = GTK_SHADOW_OUT;
    GtkStyle* style;
    GtkScrollbar *scrollbar;
    GtkAdjustment *adj;
    gboolean activate_slider;

    ensure_scrollbar_widget();

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL)
        scrollbar = GTK_SCROLLBAR(gParts->horizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gParts->vertScrollbarWidget);

    gtk_widget_set_direction(GTK_WIDGET(scrollbar), direction);

    /* Make sure to set the scrollbar range before painting so that
       everything is drawn properly.  At least the bluecurve (and
       maybe other) themes don't draw the top or bottom black line
       surrounding the scrollbar if the theme thinks that it's butted
       up against the scrollbar arrows.  Note the increases of the
       clip rect below. */
    /* Changing the cliprect is pretty bogus. This lets themes draw
       outside the frame, which means we don't invalidate them
       correctly. See bug 297508. But some themes do seem to need
       it. So we modify the frame's overflow area to account for what
       we're doing here; see nsNativeThemeGTK::GetWidgetOverflow. */
    adj = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) {
        cliprect->x -= 1;
        cliprect->width += 2;
        gtk_adjustment_set_page_size(adj, rect->width);
    }
    else {
        cliprect->y -= 1;
        cliprect->height += 2;
        gtk_adjustment_set_page_size(adj, rect->height);
    }

#if GTK_CHECK_VERSION(2, 14, 0)
    gtk_adjustment_configure(adj,
                             state->curpos,
                             0,
                             state->maxpos,
                             gtk_adjustment_get_step_increment(adj),
                             gtk_adjustment_get_page_increment(adj),
                             gtk_adjustment_get_page_size(adj));
#else
    adj->lower = 0;
    adj->value = state->curpos;
    adj->upper = state->maxpos;
    gtk_adjustment_changed(adj);
#endif

    style = gtk_widget_get_style(GTK_WIDGET(scrollbar));
    
    gtk_widget_style_get(GTK_WIDGET(scrollbar), "activate-slider",
                         &activate_slider, NULL);
    
    if (activate_slider && state->active) {
        shadow_type = GTK_SHADOW_IN;
        state_type = GTK_STATE_ACTIVE;
    }

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_slider(style, drawable, state_type, shadow_type, cliprect,
                     GTK_WIDGET(scrollbar), "slider", rect->x, rect->y,
                     rect->width,  rect->height,
                     (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ?
                     GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_widget_border(GtkThemeWidgetType widget, gint* left, gint* top,
                          gint* right, gint* bottom, GtkTextDirection direction,
                          gboolean inhtml)
{
    GtkWidget* w;
    GtkStyle *style;

    switch (widget) {
    /* These widgets have no borders, since they are not containers. */
    case MOZ_GTK_SCROLLBAR_BUTTON:
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        *left = *top = *right = *bottom = 0;
        return MOZ_GTK_SUCCESS;
    default:
        g_warning("Unsupported widget type: %d", widget);
        return MOZ_GTK_UNKNOWN_WIDGET;
    }

    style = gtk_widget_get_style(w);
    *right = *left = XTHICKNESS(style);
    *bottom = *top = YTHICKNESS(style);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_scrollbar_metrics(MozGtkScrollbarMetrics *metrics)
{
    ensure_scrollbar_widget();

    gtk_widget_style_get (gParts->horizScrollbarWidget,
                          "slider_width", &metrics->slider_width,
                          "trough_border", &metrics->trough_border,
                          "stepper_size", &metrics->stepper_size,
                          "stepper_spacing", &metrics->stepper_spacing,
                          "trough_under_steppers", &metrics->trough_under_steppers,
                          "has_secondary_forward_stepper", &metrics->has_secondary_forward_stepper,
                          "has_secondary_backward_stepper", &metrics->has_secondary_backward_stepper,
                          NULL);

    metrics->min_slider_size = gtk_range_get_min_slider_size(GTK_RANGE(gParts->horizScrollbarWidget));

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, GdkDrawable* drawable,
                     GdkRectangle* rect, GdkRectangle* cliprect,
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction)
{
    switch (widget) {
    case MOZ_GTK_SCROLLBAR_BUTTON:
        return moz_gtk_scrollbar_button_paint(drawable, rect, cliprect, state,
                                              (GtkScrollbarButtonFlags) flags,
                                              direction);
        break;
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
        return moz_gtk_scrollbar_trough_paint(widget, drawable, rect,
                                              cliprect, state, direction);
        break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        return moz_gtk_scrollbar_thumb_paint(widget, drawable, rect,
                                             cliprect, state, direction);
        break;
    case MOZ_GTK_SCROLLED_WINDOW:
        return moz_gtk_scrolled_window_paint(drawable, rect, cliprect, state);
        break;
    default:
        g_warning("Unknown widget type: %d", widget);
    }

    return MOZ_GTK_UNKNOWN_WIDGET;
}

GtkWidget* moz_gtk_get_scrollbar_widget(void)
{
    if (!is_initialized)
        return NULL;
    ensure_scrollbar_widget();
    return gParts->horizScrollbarWidget;
}

gint
moz_gtk_shutdown()
{
    GtkWidgetClass *entry_class;
    entry_class = g_type_class_peek(GTK_TYPE_ENTRY);
    g_type_class_unref(entry_class);

    is_initialized = FALSE;

    return MOZ_GTK_SUCCESS;
}

void moz_gtk_destroy_theme_parts_widgets(GtkThemeParts* parts)
{
    if (!parts)
        return;

    if (parts->protoWindow) {
        gtk_widget_destroy(parts->protoWindow);
        parts->protoWindow = NULL;
    }
}

#endif // GTK_API_VERSION_2
