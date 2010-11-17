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

#ifndef GTK_API_VERSION_2

#include <gdk/gdkprivate.h>
#include "gtkdrawing.h"
#include "GtkVersioning.h"
#include <math.h>
#include <string.h>

#define XTHICKNESS(style) (style->xthickness)
#define YTHICKNESS(style) (style->ythickness)

static GtkThemeParts *gParts = NULL;
static style_prop_t style_prop_func;
static gboolean have_arrow_scaling;
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
ensure_button_widget()
{
    if (!gParts->buttonWidget) {
        gParts->buttonWidget = gtk_button_new_with_label("M");
        setup_widget_prototype(gParts->buttonWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_toggle_button_widget()
{
    if (!gParts->toggleButtonWidget) {
        gParts->toggleButtonWidget = gtk_toggle_button_new();
        setup_widget_prototype(gParts->toggleButtonWidget);
        /* toggle button must be set active to get the right style on hover. */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gParts->toggleButtonWidget), TRUE);
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_button_arrow_widget()
{
    if (!gParts->buttonArrowWidget) {
        ensure_toggle_button_widget();

        gParts->buttonArrowWidget = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
        gtk_container_add(GTK_CONTAINER(gParts->toggleButtonWidget), gParts->buttonArrowWidget);
        gtk_widget_realize(gParts->buttonArrowWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_checkbox_widget()
{
    if (!gParts->checkboxWidget) {
        gParts->checkboxWidget = gtk_check_button_new_with_label("M");
        setup_widget_prototype(gParts->checkboxWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_radiobutton_widget()
{
    if (!gParts->radiobuttonWidget) {
        gParts->radiobuttonWidget = gtk_radio_button_new_with_label(NULL, "M");
        setup_widget_prototype(gParts->radiobuttonWidget);
    }
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
ensure_scale_widget()
{
  if (!gParts->hScaleWidget) {
    gParts->hScaleWidget = gtk_hscale_new(NULL);
    setup_widget_prototype(gParts->hScaleWidget);
  }
  if (!gParts->vScaleWidget) {
    gParts->vScaleWidget = gtk_vscale_new(NULL);
    setup_widget_prototype(gParts->vScaleWidget);
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_entry_widget()
{
    if (!gParts->entryWidget) {
        gParts->entryWidget = gtk_entry_new();
        setup_widget_prototype(gParts->entryWidget);
    }
    return MOZ_GTK_SUCCESS;
}

/* We need to have pointers to the inner widgets (button, separator, arrow)
 * of the ComboBox to get the correct rendering from theme engines which
 * special cases their look. Since the inner layout can change, we ask GTK
 * to NULL our pointers when they are about to become invalid because the
 * corresponding widgets don't exist anymore. It's the role of
 * g_object_add_weak_pointer().
 * Note that if we don't find the inner widgets (which shouldn't happen), we
 * fallback to use generic "non-inner" widgets, and they don't need that kind
 * of weak pointer since they are explicit children of gParts->protoWindow and as
 * such GTK holds a strong reference to them. */
static void
moz_gtk_get_combo_box_inner_button(GtkWidget *widget, gpointer client_data)
{
    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        gParts->comboBoxButtonWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxButtonWidget);
        gtk_widget_realize(widget);
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    }
}

static void
moz_gtk_get_combo_box_button_inner_widgets(GtkWidget *widget,
                                           gpointer client_data)
{
    if (GTK_IS_SEPARATOR(widget)) {
        gParts->comboBoxSeparatorWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxSeparatorWidget);
    } else if (GTK_IS_ARROW(widget)) {
        gParts->comboBoxArrowWidget = widget;
        g_object_add_weak_pointer(G_OBJECT(widget),
                                  (gpointer) &gParts->comboBoxArrowWidget);
    } else
        return;
    gtk_widget_realize(widget);
    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
}

static gint
ensure_combo_box_widgets()
{
    GtkWidget* buttonChild;

    if (gParts->comboBoxButtonWidget && gParts->comboBoxArrowWidget)
        return MOZ_GTK_SUCCESS;

    /* Create a ComboBox if needed */
    if (!gParts->comboBoxWidget) {
        gParts->comboBoxWidget = gtk_combo_box_new();
        setup_widget_prototype(gParts->comboBoxWidget);
    }

    /* Get its inner Button */
    gtk_container_forall(GTK_CONTAINER(gParts->comboBoxWidget),
                         moz_gtk_get_combo_box_inner_button,
                         NULL);

    if (gParts->comboBoxButtonWidget) {
        /* Get the widgets inside the Button */
        buttonChild = gtk_bin_get_child(GTK_BIN(gParts->comboBoxButtonWidget));
        if (GTK_IS_HBOX(buttonChild)) {
            /* appears-as-list = FALSE, cell-view = TRUE; the button
             * contains an hbox. This hbox is there because the ComboBox
             * needs to place a cell renderer, a separator, and an arrow in
             * the button when appears-as-list is FALSE. */
            gtk_container_forall(GTK_CONTAINER(buttonChild),
                                 moz_gtk_get_combo_box_button_inner_widgets,
                                 NULL);
        } else if(GTK_IS_ARROW(buttonChild)) {
            /* appears-as-list = TRUE, or cell-view = FALSE;
             * the button only contains an arrow */
            gParts->comboBoxArrowWidget = buttonChild;
            g_object_add_weak_pointer(G_OBJECT(buttonChild), (gpointer)
                                      &gParts->comboBoxArrowWidget);
            gtk_widget_realize(gParts->comboBoxArrowWidget);
            g_object_set_data(G_OBJECT(gParts->comboBoxArrowWidget),
                              "transparent-bg-hint", GINT_TO_POINTER(TRUE));
        }
    } else {
        /* Shouldn't be reached with current internal gtk implementation; we
         * use a generic toggle button as last resort fallback to avoid
         * crashing. */
        ensure_toggle_button_widget();
        gParts->comboBoxButtonWidget = gParts->toggleButtonWidget;
    }

    if (!gParts->comboBoxArrowWidget) {
        /* Shouldn't be reached with current internal gtk implementation;
         * we gParts->buttonArrowWidget as last resort fallback to avoid
         * crashing. */
        ensure_button_arrow_widget();
        gParts->comboBoxArrowWidget = gParts->buttonArrowWidget;
    }

    /* We don't test the validity of gParts->comboBoxSeparatorWidget since there
     * is none when "appears-as-list" = TRUE or "cell-view" = FALSE; if it
     * is invalid we just won't paint it. */

    return MOZ_GTK_SUCCESS;
}

static gint
ensure_progress_widget()
{
    if (!gParts->progresWidget) {
        gParts->progresWidget = gtk_progress_bar_new();
        setup_widget_prototype(gParts->progresWidget);
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
moz_gtk_button_paint(cairo_t* cr, GdkRectangle* rect,
                     GtkWidgetState* state, GtkReliefStyle relief,
                     GtkWidget* widget, GtkTextDirection direction)
{
    GtkShadowType shadow_type;
    GtkStyle* style = gtk_widget_get_style(widget);
    GtkStateType button_state = ConvertGtkState(state);
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;
    GdkWindow* window = gtk_widget_get_window(widget);

    gboolean interior_focus;
    gint focus_width, focus_pad;

    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width, &focus_pad);

    if (window && gdk_window_is_visible(window)) {
        gdk_window_set_background_pattern(window, NULL);
    }

    gtk_widget_set_state(widget, button_state);
    gtk_widget_set_direction(widget, direction);
    gtk_button_set_relief(GTK_BUTTON(widget), relief);

    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        width -= 2 * (focus_width + focus_pad);
        height -= 2 * (focus_width + focus_pad);
    }

    shadow_type = button_state == GTK_STATE_ACTIVE ||
                      state->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

    if (state->isDefault && relief == GTK_RELIEF_NORMAL) {
        gtk_paint_box(style, cr, button_state, shadow_type,
                      widget, "buttondefault", x, y, width, height);
    }

    if (relief != GTK_RELIEF_NONE || state->depressed ||
           (button_state != GTK_STATE_NORMAL &&
            button_state != GTK_STATE_INSENSITIVE)) {
        /* the following line can trigger an assertion (Crux theme)
           file ../../gdk/gdkwindow.c: line 1846 (gdk_window_clear_area):
           assertion `GDK_IS_WINDOW (window)' failed */
        gtk_paint_box(style, cr, button_state, shadow_type,
                      widget, "button", x, y, width, height);
    }

    if (state->focused) {
        if (interior_focus) {
            GtkStyle* style = gtk_widget_get_style(widget);
            x += style->xthickness + focus_pad;
            y += style->ythickness + focus_pad;
            width -= 2 * (style->xthickness + focus_pad);
            height -= 2 * (style->ythickness + focus_pad);
        } else {
            x -= focus_width + focus_pad;
            y -= focus_width + focus_pad;
            width += 2 * (focus_width + focus_pad);
            height += 2 * (focus_width + focus_pad);
        }

        gtk_paint_focus(style, cr, button_state,
                        widget, "button", x, y, width, height);
    }

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_init()
{
    GtkWidgetClass *entry_class;

    is_initialized = TRUE;
    have_arrow_scaling = (gtk_major_version > 2 ||
                          (gtk_major_version == 2 && gtk_minor_version >= 12));

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

gint
moz_gtk_checkbox_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    ensure_checkbox_widget();

    gtk_widget_style_get (gParts->checkboxWidget,
                          "indicator_size", indicator_size,
                          "indicator_spacing", indicator_spacing,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_radio_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    ensure_radiobutton_widget();

    gtk_widget_style_get (gParts->radiobuttonWidget,
                          "indicator_size", indicator_size,
                          "indicator_spacing", indicator_spacing,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_widget_get_focus(GtkWidget* widget, gboolean* interior_focus,
                         gint* focus_width, gint* focus_pad)
{
    gtk_widget_style_get (widget,
                          "interior-focus", interior_focus,
                          "focus-line-width", focus_width,
                          "focus-padding", focus_pad,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_button_get_inner_border(GtkWidget* widget, GtkBorder* inner_border)
{
    static const GtkBorder default_inner_border = { 1, 1, 1, 1 };
    GtkBorder *tmp_border;

    gtk_widget_style_get (widget, "inner-border", &tmp_border, NULL);

    if (tmp_border) {
        *inner_border = *tmp_border;
        gtk_border_free(tmp_border);
    }
    else
        *inner_border = default_inner_border;

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_paint(cairo_t* cr, GdkRectangle* rect,
                     GtkWidgetState* state, gboolean selected,
                     gboolean inconsistent, gboolean isradio,
                     GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (selected)?GTK_SHADOW_IN:GTK_SHADOW_OUT;
    gint indicator_size, indicator_spacing;
    gint x, y, width, height;
    gint focus_x, focus_y, focus_width, focus_height;
    GtkWidget *w;
    GtkStyle *style;

    if (isradio) {
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
        w = gParts->radiobuttonWidget;
    } else {
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);
        w = gParts->checkboxWidget;
    }

    // "GetMinimumWidgetSize was ignored"
    // FIXME: This assert causes a build failure in WebKitGTK+ debug
    // builds, because it uses 'false' in its definition. We may want
    // to force this file to be built with g++, by renaming it.
    // ASSERT(rect->width == indicator_size);

    /*
     * vertically center in the box, since XUL sometimes ignores our
     * GetMinimumWidgetSize in the vertical dimension
     */
    x = rect->x;
    y = rect->y + (rect->height - indicator_size) / 2;
    width = indicator_size;
    height = indicator_size;

    focus_x = x - indicator_spacing;
    focus_y = y - indicator_spacing;
    focus_width = width + 2 * indicator_spacing;
    focus_height = height + 2 * indicator_spacing;

    style = gtk_widget_get_style(w);

    gtk_widget_set_sensitive(w, !state->disabled);
    gtk_widget_set_direction(w, direction);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), selected);

    if (isradio) {
        gtk_paint_option(style, cr, state_type, shadow_type,
                         gParts->radiobuttonWidget, "radiobutton", x, y,
                         width, height);
        if (state->focused) {
            gtk_paint_focus(style, cr, GTK_STATE_ACTIVE,
                            gParts->radiobuttonWidget, "radiobutton", focus_x, focus_y,
                            focus_width, focus_height);
        }
    }
    else {
       /*
        * 'indeterminate' type on checkboxes. In GTK, the shadow type
        * must also be changed for the state to be drawn.
        */
        if (inconsistent) {
            gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(gParts->checkboxWidget), TRUE);
            shadow_type = GTK_SHADOW_ETCHED_IN;
        } else {
            gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(gParts->checkboxWidget), FALSE);
        }

        gtk_paint_check(style, cr, state_type, shadow_type,
                        gParts->checkboxWidget, "checkbutton", x, y, width, height);
        if (state->focused) {
            gtk_paint_focus(style, cr, GTK_STATE_ACTIVE,
                            gParts->checkboxWidget, "checkbutton", focus_x, focus_y,
                            focus_width, focus_height);
        }
    }

    return MOZ_GTK_SUCCESS;
}

static gint
calculate_button_inner_rect(GtkWidget* button, GdkRectangle* rect,
                            GdkRectangle* inner_rect,
                            GtkTextDirection direction,
                            gboolean ignore_focus)
{
    GtkBorder inner_border;
    gboolean interior_focus;
    gint focus_width, focus_pad;
    GtkStyle* style;

    style = gtk_widget_get_style(button);

    /* This mirrors gtkbutton's child positioning */
    moz_gtk_button_get_inner_border(button, &inner_border);
    moz_gtk_widget_get_focus(button, &interior_focus,
                             &focus_width, &focus_pad);

    if (ignore_focus)
        focus_width = focus_pad = 0;

    inner_rect->x = rect->x + XTHICKNESS(style) + focus_width + focus_pad;
    inner_rect->x += direction == GTK_TEXT_DIR_LTR ?
                        inner_border.left : inner_border.right;
    inner_rect->y = rect->y + inner_border.top + YTHICKNESS(style) +
                    focus_width + focus_pad;
    inner_rect->width = MAX(1, rect->width - inner_border.left -
       inner_border.right - (XTHICKNESS(style) + focus_pad + focus_width) * 2);
    inner_rect->height = MAX(1, rect->height - inner_border.top -
       inner_border.bottom - (YTHICKNESS(style) + focus_pad + focus_width) * 2);

    return MOZ_GTK_SUCCESS;
}


static gint
calculate_arrow_rect(GtkWidget* arrow, GdkRectangle* rect,
                     GdkRectangle* arrow_rect, GtkTextDirection direction)
{
    /* defined in gtkarrow.c */
    gfloat arrow_scaling = 0.7;
    gfloat xalign, xpad;
    gint extent;
    GtkMisc* misc = GTK_MISC(arrow);
    gfloat misc_xalign, misc_yalign;
    gint misc_xpad, misc_ypad;

    if (have_arrow_scaling)
        gtk_widget_style_get(arrow, "arrow_scaling", &arrow_scaling, NULL);

    gtk_misc_get_padding(misc, &misc_xpad, &misc_ypad);
    gtk_misc_get_alignment(misc, &misc_xalign, &misc_yalign);

    extent = MIN((rect->width - misc_xpad * 2),
                 (rect->height - misc_ypad * 2)) * arrow_scaling;

    xalign = direction == GTK_TEXT_DIR_LTR ? misc_xalign : 1.0 - misc_xalign;
    xpad = misc_xpad + (rect->width - extent) * xalign;

    arrow_rect->x = direction == GTK_TEXT_DIR_LTR ?
                        floor(rect->x + xpad) : ceil(rect->x + xpad);
    arrow_rect->y = floor(rect->y + misc_ypad +
                          ((rect->height - extent) * misc_yalign));

    arrow_rect->width = arrow_rect->height = extent;

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrolled_window_paint(cairo_t* cr, GdkRectangle* rect,
                              GtkWidgetState* state)
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
    gtk_paint_shadow(style, cr, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     widget, "scrolled_window", rect->x , rect->y,
                     rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_button_paint(cairo_t* cr, GdkRectangle* rect,
                               GtkWidgetState* state,
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

    gtk_paint_box(style, cr, state_type, shadow_type,
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

    gtk_paint_arrow(style, cr, state_type, shadow_type,
                    scrollbar, detail, arrow_type, TRUE, arrow_rect.x,
                    arrow_rect.y, arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_trough_paint(GtkThemeWidgetType widget,
                               cairo_t* cr, GdkRectangle* rect,
                               GtkWidgetState* state,
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

    gtk_paint_box(style, cr, GTK_STATE_ACTIVE, GTK_SHADOW_IN,
                  GTK_WIDGET(scrollbar), "trough", rect->x, rect->y,
                  rect->width, rect->height);

    if (state->focused) {
        gtk_paint_focus(style, cr, GTK_STATE_ACTIVE,
                        GTK_WIDGET(scrollbar), "trough",
                        rect->x, rect->y, rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_thumb_paint(GtkThemeWidgetType widget,
                              cairo_t* cr, GdkRectangle* rect,
                              GtkWidgetState* state,
                              GtkTextDirection direction)
{
    GtkStateType state_type = (state->inHover || state->active) ?
        GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;
    GtkShadowType shadow_type = GTK_SHADOW_OUT;
    GtkStyle* style;
    GtkScrollbar *scrollbar;
    GtkAdjustment *adj;

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
    adj = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) {
        gtk_adjustment_set_page_size(adj, rect->width);
    }
    else {
        gtk_adjustment_set_page_size(adj, rect->height);
    }

    gtk_adjustment_configure(adj,
                             state->curpos,
                             0,
                             state->maxpos,
                             gtk_adjustment_get_step_increment(adj),
                             gtk_adjustment_get_page_increment(adj),
                             gtk_adjustment_get_page_size(adj));

    style = gtk_widget_get_style(GTK_WIDGET(scrollbar));

    gtk_paint_slider(style, cr, state_type, shadow_type,
                     GTK_WIDGET(scrollbar), "slider", rect->x, rect->y,
                     rect->width,  rect->height,
                     (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ?
                     GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_paint(cairo_t* cr, GdkRectangle* rect,
                    GtkWidgetState* state, GtkOrientation flags,
                    GtkTextDirection direction)
{
  gint x = 0, y = 0;
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gParts->hScaleWidget : gParts->vScaleWidget);
  gtk_widget_set_direction(widget, direction);

  style = gtk_widget_get_style(widget);

  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    x = XTHICKNESS(style);
    y++;
  }
  else {
    x++;
    y = YTHICKNESS(style);
  }

  gtk_style_apply_default_background(style, cr, gtk_widget_get_window (widget),
                                     GTK_STATE_NORMAL,
                                     rect->x, rect->y,
                                     rect->width, rect->height);

  gtk_paint_box(style, cr, GTK_STATE_ACTIVE, GTK_SHADOW_IN,
                widget, "trough", rect->x + x, rect->y + y,
                rect->width - 2*x, rect->height - 2*y);

  if (state->focused)
    gtk_paint_focus(style, cr, state_type, widget, "trough",
                    rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_thumb_paint(cairo_t* cr, GdkRectangle* rect,
                          GtkWidgetState* state, GtkOrientation flags,
                          GtkTextDirection direction)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;
  gint thumb_width, thumb_height, x, y;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gParts->hScaleWidget : gParts->vScaleWidget);
  gtk_widget_set_direction(widget, direction);

  style = gtk_widget_get_style(widget);

  /* determine the thumb size, and position the thumb in the center in the opposite axis */
  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL, &thumb_width, &thumb_height);
    x = rect->x;
    y = rect->y + (rect->height - thumb_height) / 2;
  }
  else {
    moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_height, &thumb_width);
    x = rect->x + (rect->width - thumb_width) / 2;
    y = rect->y;
  }

  gtk_paint_slider(style, cr, state_type, GTK_SHADOW_OUT,
                   widget, (flags == GTK_ORIENTATION_HORIZONTAL) ? "hscale" : "vscale",
                   x, y, thumb_width, thumb_height, flags);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_entry_paint(cairo_t* cr, GdkRectangle* rect,
                    GtkWidgetState* state, GtkWidget* widget,
                    GtkTextDirection direction)
{
    GtkStateType bg_state = state->disabled ?
                                GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;
    gint x, y, width = rect->width, height = rect->height;
    GtkStyle* style;
    gboolean interior_focus;
    gboolean theme_honors_transparency = FALSE;
    gint focus_width;

    gtk_widget_set_direction(widget, direction);

    style = gtk_widget_get_style(widget);

    gtk_widget_style_get(widget,
                         "interior-focus", &interior_focus,
                         "focus-line-width", &focus_width,
                         "honors-transparent-bg-hint", &theme_honors_transparency,
                         NULL);

    /* gtkentry.c uses two windows, one for the entire widget and one for the
     * text area inside it. The background of both windows is set to the "base"
     * color of the new state in gtk_entry_state_changed, but only the inner
     * textarea window uses gtk_paint_flat_box when exposed */

    /* This gets us a lovely greyish disabledish look */
    gtk_widget_set_sensitive(widget, !state->disabled);

    /* GTK fills the outer widget window with the base color before drawing the widget.
     * Some older themes rely on this behavior, but many themes nowadays use rounded
     * corners on their widgets. While most GTK apps are blissfully unaware of this
     * problem due to their use of the default window background, we render widgets on
     * many kinds of backgrounds on the web.
     * If the theme is able to cope with transparency, then we can skip pre-filling
     * and notify the theme it will paint directly on the canvas. */
    if (theme_honors_transparency) {
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));
    } else {
        cairo_save(cr);
        gdk_cairo_set_source_color(cr, (const GdkColor*)&style->base[bg_state]);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
        cairo_fill(cr);
        cairo_restore(cr);
        g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(FALSE));
    }

    /* Get the position of the inner window, see _gtk_entry_get_borders */
    x = XTHICKNESS(style);
    y = YTHICKNESS(style);

    if (!interior_focus) {
        x += focus_width;
        y += focus_width;
    }

    /* Simulate an expose of the inner window */
    gtk_paint_flat_box(style, cr, bg_state, GTK_SHADOW_NONE,
                       widget, "entry_bg",  rect->x + x,
                       rect->y + y, rect->width - 2*x, rect->height - 2*y);

    /* Now paint the shadow and focus border.
     * We do like in gtk_entry_draw_frame, we first draw the shadow, a tad
     * smaller when focused if the focus is not interior, then the focus. */
    x = rect->x;
    y = rect->y;

    if (state->focused && !state->disabled) {
        /* This will get us the lit borders that focused textboxes enjoy on
         * some themes. */
        if (!interior_focus) {
            /* Indent the border a little bit if we have exterior focus
               (this is what GTK does to draw native entries) */
            x += focus_width;
            y += focus_width;
            width -= 2 * focus_width;
            height -= 2 * focus_width;
        }
    }

    gtk_paint_shadow(style, cr, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     widget, "entry", x, y, width, height);

    if (state->focused && !state->disabled) {
        if (!interior_focus) {
            gtk_paint_focus(style, cr,  GTK_STATE_NORMAL,
                            widget, "entry",
                            rect->x, rect->y, rect->width, rect->height);
        }
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_combo_box_paint(cairo_t* cr, GdkRectangle* rect,
                        GtkWidgetState* state, gboolean ishtml,
                        GtkTextDirection direction)
{
    GdkRectangle arrow_rect, real_arrow_rect;
    gint /* arrow_size, */ separator_width;
    gboolean wide_separators;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;
    GtkRequisition arrow_req;

    ensure_combo_box_widgets();

    /* Also sets the direction on gParts->comboBoxButtonWidget, which is then
     * inherited by the separator and arrow */
    moz_gtk_button_paint(cr, rect, state, GTK_RELIEF_NORMAL,
                         gParts->comboBoxButtonWidget, direction);

    calculate_button_inner_rect(gParts->comboBoxButtonWidget,
                                rect, &arrow_rect, direction, ishtml);
    /* Now arrow_rect contains the inner rect ; we want to correct the width
     * to what the arrow needs (see gtk_combo_box_size_allocate) */
    gtk_widget_get_preferred_size(gParts->comboBoxArrowWidget, &arrow_req, NULL);
    if (direction == GTK_TEXT_DIR_LTR)
        arrow_rect.x += arrow_rect.width - arrow_req.width;
    arrow_rect.width = arrow_req.width;

    calculate_arrow_rect(gParts->comboBoxArrowWidget,
                         &arrow_rect, &real_arrow_rect, direction);

    style = gtk_widget_get_style(gParts->comboBoxArrowWidget);

    gtk_widget_size_allocate(gParts->comboBoxWidget, rect);

    gtk_paint_arrow(style, cr, state_type, shadow_type,
                    gParts->comboBoxArrowWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    real_arrow_rect.x, real_arrow_rect.y,
                    real_arrow_rect.width, real_arrow_rect.height);


    /* If there is no separator in the theme, there's nothing left to do. */
    if (!gParts->comboBoxSeparatorWidget)
        return MOZ_GTK_SUCCESS;

    style = gtk_widget_get_style(gParts->comboBoxSeparatorWidget);

    gtk_widget_style_get(gParts->comboBoxSeparatorWidget,
                         "wide-separators", &wide_separators,
                         "separator-width", &separator_width,
                         NULL);

    if (wide_separators) {
        if (direction == GTK_TEXT_DIR_LTR)
            arrow_rect.x -= separator_width;
        else
            arrow_rect.x += arrow_rect.width;

        gtk_paint_box(style, cr,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                      gParts->comboBoxSeparatorWidget, "vseparator",
                      arrow_rect.x, arrow_rect.y,
                      separator_width, arrow_rect.height);
    } else {
        if (direction == GTK_TEXT_DIR_LTR)
            arrow_rect.x -= XTHICKNESS(style);
        else
            arrow_rect.x += arrow_rect.width;

        gtk_paint_vline(style, cr, GTK_STATE_NORMAL,
                        gParts->comboBoxSeparatorWidget, "vseparator",
                        arrow_rect.y, arrow_rect.y + arrow_rect.height,
                        arrow_rect.x);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progressbar_paint(cairo_t* cr, GdkRectangle* rect,
                          GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_progress_widget();
    gtk_widget_set_direction(gParts->progresWidget, direction);

    style = gtk_widget_get_style(gParts->progresWidget);

    gtk_paint_box(style, cr, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                  gParts->progresWidget, "trough", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progress_chunk_paint(cairo_t* cr, GdkRectangle* rect,
                             GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_progress_widget();
    gtk_widget_set_direction(gParts->progresWidget, direction);

    style = gtk_widget_get_style(gParts->progresWidget);

    gtk_paint_box(style, cr, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
                  gParts->progresWidget, "bar", rect->x, rect->y,
                  rect->width, rect->height);

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
    case MOZ_GTK_BUTTON:
        {
            GtkBorder inner_border;
            gboolean interior_focus;
            gint focus_width, focus_pad;
            GtkStyle *style;

            ensure_button_widget();
            *left = *top = *right = *bottom = gtk_container_get_border_width(GTK_CONTAINER(gParts->buttonWidget));

            /* Don't add this padding in HTML, otherwise the buttons will
               become too big and stuff the layout. */
            if (!inhtml) {
                moz_gtk_widget_get_focus(gParts->buttonWidget, &interior_focus, &focus_width, &focus_pad);
                moz_gtk_button_get_inner_border(gParts->buttonWidget, &inner_border);
                *left += focus_width + focus_pad + inner_border.left;
                *right += focus_width + focus_pad + inner_border.right;
                *top += focus_width + focus_pad + inner_border.top;
                *bottom += focus_width + focus_pad + inner_border.bottom;
            }

            style = gtk_widget_get_style(gParts->buttonWidget);
            *left += style->xthickness;
            *right += style->xthickness;
            *top += style->ythickness;
            *bottom += style->ythickness;
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_ENTRY:
        ensure_entry_widget();
        w = gParts->entryWidget;
        break;
    case MOZ_GTK_DROPDOWN:
        {
            /* We need to account for the arrow on the dropdown, so text
             * doesn't come too close to the arrow, or in some cases spill
             * into the arrow. */
            gboolean ignored_interior_focus, wide_separators;
            gint focus_width, focus_pad, separator_width;
            GtkRequisition arrow_req;
            GtkStyle* style;

            ensure_combo_box_widgets();

            *left = gtk_container_get_border_width(GTK_CONTAINER(gParts->comboBoxButtonWidget));

            if (!inhtml) {
                moz_gtk_widget_get_focus(gParts->comboBoxButtonWidget,
                                         &ignored_interior_focus,
                                         &focus_width, &focus_pad);
                *left += focus_width + focus_pad;
            }

            style = gtk_widget_get_style(gParts->comboBoxButtonWidget);
            *top = *left + style->ythickness;
            *left += style->xthickness;

            *right = *left; *bottom = *top;

            /* If there is no separator, don't try to count its width. */
            separator_width = 0;
            if (gParts->comboBoxSeparatorWidget) {
                gtk_widget_style_get(gParts->comboBoxSeparatorWidget,
                                     "wide-separators", &wide_separators,
                                     "separator-width", &separator_width,
                                     NULL);

                if (!wide_separators)
                    separator_width =
                        XTHICKNESS(style);
            }

            gtk_widget_get_preferred_size(gParts->comboBoxArrowWidget, &arrow_req, NULL);
            if (direction == GTK_TEXT_DIR_RTL)
                *left += separator_width + arrow_req.width;
            else
                *right += separator_width + arrow_req.width;

            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_PROGRESSBAR:
        ensure_progress_widget();
        w = gParts->progresWidget;
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
        ensure_scale_widget();
        w = gParts->hScaleWidget;
        break;
    case MOZ_GTK_SCALE_VERTICAL:
        ensure_scale_widget();
        w = gParts->vScaleWidget;
        break;
    /* These widgets have no borders, since they are not containers. */
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
    case MOZ_GTK_SCROLLBAR_BUTTON:
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
    case MOZ_GTK_PROGRESS_CHUNK:
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
moz_gtk_get_scalethumb_metrics(GtkOrientation orient, gint* thumb_length, gint* thumb_height)
{
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((orient == GTK_ORIENTATION_HORIZONTAL) ? gParts->hScaleWidget : gParts->vScaleWidget);

  gtk_widget_style_get (widget,
                        "slider_length", thumb_length,
                        "slider_width", thumb_height,
                        NULL);

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
moz_gtk_widget_paint(GtkThemeWidgetType widget, cairo_t* cr,
                     GdkRectangle* rect, GtkWidgetState* state,
                     gint flags, GtkTextDirection direction)
{
    switch (widget) {
    case MOZ_GTK_BUTTON:
        if (state->depressed) {
            ensure_toggle_button_widget();
            return moz_gtk_button_paint(cr, rect, state,
                                        (GtkReliefStyle) flags,
                                        gParts->toggleButtonWidget, direction);
        }
        ensure_button_widget();
        return moz_gtk_button_paint(cr, rect, state,
                                    (GtkReliefStyle) flags, gParts->buttonWidget,
                                    direction);
        break;
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
        return moz_gtk_toggle_paint(cr, rect, state,
                                    !!(flags & MOZ_GTK_WIDGET_CHECKED),
                                    !!(flags & MOZ_GTK_WIDGET_INCONSISTENT),
                                    (widget == MOZ_GTK_RADIOBUTTON),
                                    direction);
        break;
    case MOZ_GTK_SCROLLBAR_BUTTON:
        return moz_gtk_scrollbar_button_paint(cr, rect, state,
                                              (GtkScrollbarButtonFlags) flags,
                                              direction);
        break;
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
        return moz_gtk_scrollbar_trough_paint(widget, cr, rect,
                                              state, direction);
        break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        return moz_gtk_scrollbar_thumb_paint(widget, cr, rect,
                                             state, direction);
        break;
    case MOZ_GTK_SCROLLED_WINDOW:
        return moz_gtk_scrolled_window_paint(cr, rect, state);
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
        return moz_gtk_scale_paint(cr, rect, state,
                                   (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
        return moz_gtk_scale_thumb_paint(cr, rect, state,
                                         (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_ENTRY:
        ensure_entry_widget();
        return moz_gtk_entry_paint(cr, rect, state,
                                   gParts->entryWidget, direction);
        break;
    case MOZ_GTK_DROPDOWN:
        return moz_gtk_combo_box_paint(cr, rect, state,
                                       (gboolean) flags, direction);
        break;
    case MOZ_GTK_PROGRESSBAR:
        return moz_gtk_progressbar_paint(cr, rect, direction);
        break;
    case MOZ_GTK_PROGRESS_CHUNK:
        return moz_gtk_progress_chunk_paint(cr, rect, direction);
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

GtkWidget* moz_gtk_get_progress_widget()
{
    if (!is_initialized)
        return NULL;
    ensure_progress_widget();
    return gParts->progresWidget;
}

#endif // GTK_API_VERSION_2
