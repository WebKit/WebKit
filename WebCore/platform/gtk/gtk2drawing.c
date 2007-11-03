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

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <string.h>
#include "gtkdrawing.h"

#include <math.h>

#define XTHICKNESS(style) (style->xthickness)
#define YTHICKNESS(style) (style->ythickness)
#define WINDOW_IS_MAPPED(window) ((window) && GDK_IS_WINDOW(window) && gdk_window_is_visible(window))

static GtkWidget* gProtoWindow;
static GtkWidget* gButtonWidget;
static GtkWidget* gCheckboxWidget;
static GtkWidget* gRadiobuttonWidget;
static GtkWidget* gHorizScrollbarWidget;
static GtkWidget* gVertScrollbarWidget;
static GtkWidget* gSpinWidget;
static GtkWidget* gHScaleWidget;
static GtkWidget* gVScaleWidget;
static GtkWidget* gEntryWidget;
static GtkWidget* gArrowWidget;
static GtkWidget* gOptionMenuWidget;
static GtkWidget* gDropdownButtonWidget;
static GtkWidget* gHandleBoxWidget;
static GtkWidget* gToolbarWidget;
static GtkWidget* gFrameWidget;
static GtkWidget* gProgressWidget;
static GtkWidget* gTabWidget;
static GtkWidget* gTooltipWidget;
static GtkWidget* gMenuBarWidget;
static GtkWidget* gMenuBarItemWidget;
static GtkWidget* gMenuPopupWidget;
static GtkWidget* gMenuItemWidget;
static GtkWidget* gCheckMenuItemWidget;

static GtkShadowType gMenuBarShadowType;
static GtkShadowType gToolbarShadowType;

static style_prop_t style_prop_func;
static gboolean have_menu_shadow_type;
static gboolean is_initialized;

gint
moz_gtk_enable_style_props(style_prop_t styleGetProp)
{
    style_prop_func = styleGetProp;
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_window_widget()
{
    if (!gProtoWindow) {
        gProtoWindow = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_realize(gProtoWindow);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
setup_widget_prototype(GtkWidget* widget)
{
    static GtkWidget* protoLayout;
    ensure_window_widget();
    if (!protoLayout) {
        protoLayout = gtk_hbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(gProtoWindow), protoLayout);
    }

    gtk_container_add(GTK_CONTAINER(protoLayout), widget);
    gtk_widget_realize(widget);
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_button_widget()
{
    if (!gButtonWidget) {
        gButtonWidget = gtk_button_new_with_label("M");
        setup_widget_prototype(gButtonWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_checkbox_widget()
{
    if (!gCheckboxWidget) {
        gCheckboxWidget = gtk_check_button_new_with_label("M");
        setup_widget_prototype(gCheckboxWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_radiobutton_widget()
{
    if (!gRadiobuttonWidget) {
        gRadiobuttonWidget = gtk_radio_button_new_with_label(NULL, "M");
        setup_widget_prototype(gRadiobuttonWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_scrollbar_widget()
{
    if (!gVertScrollbarWidget) {
        gVertScrollbarWidget = gtk_vscrollbar_new(NULL);
        setup_widget_prototype(gVertScrollbarWidget);
    }
    if (!gHorizScrollbarWidget) {
        gHorizScrollbarWidget = gtk_hscrollbar_new(NULL);
        setup_widget_prototype(gHorizScrollbarWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_spin_widget()
{
  if (!gSpinWidget) {
    gSpinWidget = gtk_spin_button_new(NULL, 1, 0);
    setup_widget_prototype(gSpinWidget);
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_scale_widget()
{
  if (!gHScaleWidget) {
    gHScaleWidget = gtk_hscale_new(NULL);
    setup_widget_prototype(gHScaleWidget);
  }
  if (!gVScaleWidget) {
    gVScaleWidget = gtk_vscale_new(NULL);
    setup_widget_prototype(gVScaleWidget);
  }
  return MOZ_GTK_SUCCESS;
}

static gint
ensure_entry_widget()
{
    if (!gEntryWidget) {
        gEntryWidget = gtk_entry_new();
        setup_widget_prototype(gEntryWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_option_menu_widget()
{
    if (!gOptionMenuWidget) {
        gOptionMenuWidget = gtk_option_menu_new();
        setup_widget_prototype(gOptionMenuWidget);        
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_arrow_widget()
{
    if (!gArrowWidget) {
        gDropdownButtonWidget = gtk_button_new();
        setup_widget_prototype(gDropdownButtonWidget);
        gArrowWidget = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
        gtk_container_add(GTK_CONTAINER(gDropdownButtonWidget), gArrowWidget);
        gtk_widget_realize(gArrowWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_handlebox_widget()
{
    if (!gHandleBoxWidget) {
        gHandleBoxWidget = gtk_handle_box_new();
        setup_widget_prototype(gHandleBoxWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_toolbar_widget()
{
    if (!gToolbarWidget) {
        ensure_handlebox_widget();
        gToolbarWidget = gtk_toolbar_new();
        gtk_container_add(GTK_CONTAINER(gHandleBoxWidget), gToolbarWidget);
        gtk_widget_realize(gToolbarWidget);
        gtk_widget_style_get(gToolbarWidget, "shadow_type", &gToolbarShadowType,
                             NULL);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tooltip_widget()
{
    if (!gTooltipWidget) {
        gTooltipWidget = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_realize(gTooltipWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tab_widget()
{
    if (!gTabWidget) {
        gTabWidget = gtk_notebook_new();
        setup_widget_prototype(gTabWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_progress_widget()
{
    if (!gProgressWidget) {
        gProgressWidget = gtk_progress_bar_new();
        setup_widget_prototype(gProgressWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_frame_widget()
{
    if (!gFrameWidget) {
        gFrameWidget = gtk_frame_new(NULL);
        setup_widget_prototype(gFrameWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_bar_widget()
{
    if (!gMenuBarWidget) {
        gMenuBarWidget = gtk_menu_bar_new();
        setup_widget_prototype(gMenuBarWidget);
       gtk_widget_style_get(gMenuBarWidget, "shadow_type", &gMenuBarShadowType,
                            NULL);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_bar_item_widget()
{
    if (!gMenuBarItemWidget) {
        ensure_menu_bar_widget();
        gMenuBarItemWidget = gtk_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(gMenuBarWidget),
                              gMenuBarItemWidget);
        gtk_widget_realize(gMenuBarItemWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_popup_widget()
{
    if (!gMenuPopupWidget) {
        ensure_menu_bar_item_widget();
        gMenuPopupWidget = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(gMenuBarItemWidget),
                                  gMenuPopupWidget);
        gtk_widget_realize(gMenuPopupWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_menu_item_widget()
{
    if (!gMenuItemWidget) {
        ensure_menu_popup_widget();
        gMenuItemWidget = gtk_menu_item_new_with_label("M");
        gtk_menu_shell_append(GTK_MENU_SHELL(gMenuPopupWidget),
                              gMenuItemWidget);
        gtk_widget_realize(gMenuItemWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_check_menu_item_widget()
{
    if (!gCheckMenuItemWidget) {
        ensure_menu_popup_widget();
        gCheckMenuItemWidget = gtk_check_menu_item_new_with_label("M");
        gtk_menu_shell_append(GTK_MENU_SHELL(gMenuPopupWidget),
                              gCheckMenuItemWidget);
        gtk_widget_realize(gCheckMenuItemWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static GtkStateType
ConvertGtkState(GtkWidgetState* state)
{
    if (state->disabled)
        return GTK_STATE_INSENSITIVE;
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

static gint
moz_gtk_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state,
                     GtkReliefStyle relief, GtkWidget* widget)
{
    GtkShadowType shadow_type;
    GtkStyle* style = widget->style;
    GtkStateType button_state = ConvertGtkState(state);
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;

    gboolean interior_focus;
    gint focus_width, focus_pad;

    moz_gtk_button_get_focus(&interior_focus, &focus_width, &focus_pad);

    if (WINDOW_IS_MAPPED(drawable)) {
        gdk_window_set_back_pixmap(drawable, NULL, TRUE);
        gdk_window_clear_area(drawable, cliprect->x, cliprect->y,
                              cliprect->width, cliprect->height);
    }

    gtk_widget_set_state(widget, button_state);

    if (state->isDefault)
        GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_DEFAULT);

    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        width -= 2 * (focus_width + focus_pad);
        height -= 2 * (focus_width + focus_pad);
    }

    shadow_type = button_state == GTK_STATE_ACTIVE ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

    if (relief != GTK_RELIEF_NONE || (button_state != GTK_STATE_NORMAL &&
                                      button_state != GTK_STATE_INSENSITIVE)) {
        TSOffsetStyleGCs(style, x, y);
        /* the following line can trigger an assertion (Crux theme)
           file ../../gdk/gdkwindow.c: line 1846 (gdk_window_clear_area):
           assertion `GDK_IS_WINDOW (window)' failed */
        gtk_paint_box(style, drawable, button_state, shadow_type, cliprect,
                      widget, "button", x, y, width, height);
    }

    if (state->focused) {
        if (interior_focus) {
            x += widget->style->xthickness + focus_pad;
            y += widget->style->ythickness + focus_pad;
            width -= 2 * (widget->style->xthickness + focus_pad);
            height -= 2 * (widget->style->ythickness + focus_pad);
        } else {
            x -= focus_width + focus_pad;
            y -= focus_width + focus_pad;
            width += 2 * (focus_width + focus_pad);
            height += 2 * (focus_width + focus_pad);
        }

        TSOffsetStyleGCs(style, x, y);
        gtk_paint_focus(style, drawable, button_state, cliprect,
                        widget, "button", x, y, width, height);
    }

    GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_DEFAULT);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_init()
{
    is_initialized = TRUE;
    have_menu_shadow_type =
        (gtk_major_version > 2 ||
         (gtk_major_version == 2 && gtk_minor_version >= 1));

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_checkbox_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    ensure_checkbox_widget();

    gtk_widget_style_get (gCheckboxWidget,
                          "indicator_size", indicator_size,
                          "indicator_spacing", indicator_spacing,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_radio_get_metrics(gint* indicator_size, gint* indicator_spacing)
{
    ensure_radiobutton_widget();

    gtk_widget_style_get (gRadiobuttonWidget,
                          "indicator_size", indicator_size,
                          "indicator_spacing", indicator_spacing,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_checkbox_get_focus(gboolean* interior_focus,
                           gint* focus_width, gint* focus_pad)
{
    ensure_checkbox_widget();

    gtk_widget_style_get (gCheckboxWidget,
                          "interior-focus", interior_focus,
                          "focus-line-width", focus_width,
                          "focus-padding", focus_pad,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_radio_get_focus(gboolean* interior_focus,
                        gint* focus_width, gint* focus_pad)
{
    ensure_radiobutton_widget();

    gtk_widget_style_get (gRadiobuttonWidget,
                          "interior-focus", interior_focus,
                          "focus-line-width", focus_width,
                          "focus-padding", focus_pad,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_button_get_focus(gboolean* interior_focus,
                         gint* focus_width, gint* focus_pad)
{
    ensure_button_widget();

    gtk_widget_style_get (gButtonWidget,
                          "interior-focus", interior_focus,
                          "focus-line-width", focus_width,
                          "focus-padding", focus_pad,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_option_menu_get_metrics(gboolean* interior_focus,
                                GtkRequisition* indicator_size,
                                GtkBorder* indicator_spacing,
                                gint* focus_width,
                                gint* focus_pad)
{
    static const GtkRequisition default_indicator_size = { 7, 13 };
    static const GtkBorder default_indicator_spacing = { 7, 5, 2, 2 };
    /* these default values are not used in gtkoptionmenu.c
    static const gboolean default_interior_focus = TRUE;
    static const gint default_focus_width = 1;
    static const gint default_focus_pad = 0; */
    GtkRequisition *tmp_indicator_size;
    GtkBorder *tmp_indicator_spacing;

    gtk_widget_style_get(gOptionMenuWidget,
                         "interior_focus", interior_focus,
                         "indicator_size", &tmp_indicator_size,
                         "indicator_spacing", &tmp_indicator_spacing,
                         "focus_line_width", focus_width,
                         "focus_padding", focus_pad,
                         NULL);

    if (tmp_indicator_size)
        *indicator_size = *tmp_indicator_size;
    else
        *indicator_size = default_indicator_size;
    if (tmp_indicator_spacing)
        *indicator_spacing = *tmp_indicator_spacing;
    else
        *indicator_spacing = default_indicator_spacing;

    gtk_requisition_free(tmp_indicator_size);
    gtk_border_free(tmp_indicator_spacing);
 
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state,
                     gboolean selected, gboolean isradio)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (selected)?GTK_SHADOW_IN:GTK_SHADOW_OUT;
    gint indicator_size, indicator_spacing;
    gint x, y, width, height;
    GtkWidget *w;
    GtkStyle *style;

    if (isradio) {
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
        w = gRadiobuttonWidget;
    } else {
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);
        w = gCheckboxWidget;
    }

    /* offset by indicator_spacing, and centered vertically within the rect */
    x = rect->x + indicator_spacing;
    y = rect->y + (rect->height - indicator_size) / 2;
    width = indicator_size;
    height = indicator_size;
  
    style = w->style;
    TSOffsetStyleGCs(style, x, y);

    gtk_widget_set_sensitive(w, !state->disabled);
    GTK_TOGGLE_BUTTON(w)->active = selected;
      
    if (isradio) {
        gtk_paint_option(style, drawable, state_type, shadow_type, cliprect,
                         gRadiobuttonWidget, "radiobutton", x, y,
                         width, height);
        if (state->focused) {
            gtk_paint_focus(style, drawable, GTK_STATE_ACTIVE, cliprect,
                            gRadiobuttonWidget, "radiobutton", rect->x, rect->y,
                            rect->width, rect->height);
        }
    }
    else {
        gtk_paint_check(style, drawable, state_type, shadow_type, cliprect, 
                        gCheckboxWidget, "checkbutton", x, y, width, height);
        if (state->focused) {
            gtk_paint_focus(style, drawable, GTK_STATE_ACTIVE, cliprect,
                            gCheckboxWidget, "checkbutton", rect->x, rect->y,
                            rect->width, rect->height);
        }
    }

    return MOZ_GTK_SUCCESS;
}

static gint
calculate_arrow_dimensions(GdkRectangle* rect, GdkRectangle* arrow_rect)
{
    GtkMisc* misc = GTK_MISC(gArrowWidget);

    gint extent = MIN(rect->width - misc->xpad * 2,
                      rect->height - misc->ypad * 2);

    arrow_rect->x = ((rect->x + misc->xpad) * (1.0 - misc->xalign) +
                     (rect->x + rect->width - extent - misc->xpad) *
                     misc->xalign);

    arrow_rect->y = ((rect->y + misc->ypad) * (1.0 - misc->yalign) +
                     (rect->y + rect->height - extent - misc->ypad) *
                     misc->yalign);

    arrow_rect->width = arrow_rect->height = extent;

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkArrowType type)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (state->active) ?
        GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GdkRectangle button_rect;
    GdkRectangle arrow_rect;
    GtkStyle* style;
    GtkAdjustment *adj;
    GtkScrollbar *scrollbar;

    ensure_scrollbar_widget();

    if (type < 2)
        scrollbar = GTK_SCROLLBAR(gVertScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gHorizScrollbarWidget);

    /* Some theme engines (i.e., ClearLooks) check the scrollbar's allocation
       to determine where it should paint rounded corners on the buttons.
       We need to trick them into drawing the buttons the way we want them. */

    GTK_WIDGET(scrollbar)->allocation.x = rect->x;
    GTK_WIDGET(scrollbar)->allocation.y = rect->y;
    GTK_WIDGET(scrollbar)->allocation.width = rect->width;
    GTK_WIDGET(scrollbar)->allocation.height = rect->height;

    if (type < 2) {
        GTK_WIDGET(scrollbar)->allocation.height *= 3;
        if (type == GTK_ARROW_DOWN)
            GTK_WIDGET(scrollbar)->allocation.y -= 2 * rect->height;
    } else {
        GTK_WIDGET(scrollbar)->allocation.width *= 3;
        if (type == GTK_ARROW_RIGHT)
            GTK_WIDGET(scrollbar)->allocation.x -= 2 * rect->width;
    }

    style = GTK_WIDGET(scrollbar)->style;

    ensure_arrow_widget();
  
    calculate_arrow_dimensions(rect, &button_rect);
    TSOffsetStyleGCs(style, button_rect.x, button_rect.y);

    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  GTK_WIDGET(scrollbar),
                  (type < 2) ? "vscrollbar" : "hscrollbar",
                  button_rect.x, button_rect.y, button_rect.width,
                  button_rect.height);

    arrow_rect.width = button_rect.width / 2;
    arrow_rect.height = button_rect.height / 2;
    arrow_rect.x = button_rect.x + (button_rect.width - arrow_rect.width) / 2;
    arrow_rect.y = button_rect.y +
        (button_rect.height - arrow_rect.height) / 2;  

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    GTK_WIDGET(scrollbar), (type < 2) ?
                    "vscrollbar" : "hscrollbar", 
                    type, TRUE, arrow_rect.x, arrow_rect.y, arrow_rect.width,
                    arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scrollbar_trough_paint(GtkThemeWidgetType widget,
                               GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStyle* style;
    GtkScrollbar *scrollbar;

    ensure_scrollbar_widget();

    if (widget ==  MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL)
        scrollbar = GTK_SCROLLBAR(gHorizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gVertScrollbarWidget);

    style = GTK_WIDGET(scrollbar)->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_ACTIVE,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);

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
                              GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStateType state_type = (state->inHover || state->active) ?
        GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;
    GtkStyle* style;
    GtkScrollbar *scrollbar;
    GtkAdjustment *adj;

    ensure_scrollbar_widget();

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL)
        scrollbar = GTK_SCROLLBAR(gHorizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gVertScrollbarWidget);

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
        adj->page_size = rect->width;
    }
    else {
        cliprect->y -= 1;
        cliprect->height += 2;
        adj->page_size = rect->height;
    }

    adj->lower = 0;
    adj->value = state->curpos;
    adj->upper = state->maxpos;
    gtk_adjustment_changed(adj);

    style = GTK_WIDGET(scrollbar)->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_slider(style, drawable, state_type, GTK_SHADOW_OUT, cliprect,
                     GTK_WIDGET(scrollbar), "slider", rect->x, rect->y,
                     rect->width,  rect->height,
                     (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ?
                     GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_spin_paint(GdkDrawable* drawable, GdkRectangle* rect, gboolean isDown,
                   GtkWidgetState* state)
{
    GdkRectangle arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state_type == GTK_STATE_ACTIVE ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;

    ensure_spin_widget();
    style = gSpinWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, state_type, shadow_type, NULL, gSpinWidget,
                  isDown ? "spinbutton_down" : "spinbutton_up",
                  rect->x, rect->y, rect->width, rect->height);

    /* hard code these values */
    arrow_rect.width = 6;
    arrow_rect.height = 6;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;
    arrow_rect.y += isDown ? -1 : 1;

    gtk_paint_arrow(style, drawable, state_type, shadow_type, NULL,
                    gSpinWidget, "spinbutton",
                    isDown ? GTK_ARROW_DOWN : GTK_ARROW_UP, TRUE,
                    arrow_rect.x, arrow_rect.y,
                    arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkWidgetState* state,
                    GtkOrientation flags)
{
  gint x = 0, y = 0;
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gHScaleWidget : gVScaleWidget);
  style = widget->style;

  if (flags == GTK_ORIENTATION_HORIZONTAL) {
    x = XTHICKNESS(style);
    y++;
  }
  else {
    x++;
    y = YTHICKNESS(style);
  }

  TSOffsetStyleGCs(style, rect->x, rect->y);
  gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_NORMAL,
                                     cliprect, rect->x, rect->y,
                                     rect->width, rect->height);

  gtk_paint_box(style, drawable, GTK_STATE_ACTIVE, GTK_SHADOW_IN, cliprect,
                widget, "trough", rect->x + x, rect->y + y,
                rect->width - 2*x, rect->height - 2*y);

  if (state->focused)
    gtk_paint_focus(style, drawable, state_type, cliprect, widget, "trough",
                    rect->x, rect->y, rect->width, rect->height);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_scale_thumb_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect, GtkWidgetState* state,
                          GtkOrientation flags)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;
  gint thumb_width, thumb_height, x, y;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gHScaleWidget : gVScaleWidget);
  style = widget->style;

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

  TSOffsetStyleGCs(style, rect->x, rect->y);
  gtk_paint_slider(style, drawable, state_type, GTK_SHADOW_OUT, cliprect,
                   widget, (flags == GTK_ORIENTATION_HORIZONTAL) ? "hscale" : "vscale",
                   x, y, thumb_width, thumb_height, flags);

  return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_gripper_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type;
    GtkStyle* style;

    ensure_handlebox_widget();
    style = gHandleBoxWidget->style;
    shadow_type = GTK_HANDLE_BOX(gHandleBoxWidget)->shadow_type;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  gHandleBoxWidget, "handlebox_bin", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_entry_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkWidgetState* state)
{
    gint x, y, width = rect->width, height = rect->height;
    GtkStyle* style;
    gboolean interior_focus;
    gint focus_width;

    ensure_entry_widget();
    style = gEntryWidget->style;

    /* paint the background first */
    x = XTHICKNESS(style);
    y = YTHICKNESS(style);

    /* This gets us a lovely greyish disabledish look */
    gtk_widget_set_sensitive(gEntryWidget, !state->disabled);

    TSOffsetStyleGCs(style, rect->x + x, rect->y + y);
    gtk_paint_flat_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                       cliprect, gEntryWidget, "entry_bg",  rect->x + x,
                       rect->y + y, rect->width - 2*x, rect->height - 2*y);

    gtk_widget_style_get(gEntryWidget,
                         "interior-focus", &interior_focus,
                         "focus-line-width", &focus_width,
                         NULL);

    /*
     * Now paint the shadow and focus border.
     *
     * gtk+ is able to draw over top of the entry when it gains focus,
     * so the non-focused text border is implicitly already drawn when
     * the entry is drawn in a focused state.
     *
     * Gecko doesn't quite work this way, so always draw the non-focused
     * shadow, then draw the shadow again, inset, if we're focused.
     */

    x = rect->x;
    y = rect->y;

    if (state->focused && !state->disabled) {
         /* This will get us the lit borders that focused textboxes enjoy on some themes. */
        GTK_WIDGET_SET_FLAGS(gEntryWidget, GTK_HAS_FOCUS);

        if (!interior_focus) {
            /* Indent the border a little bit if we have exterior focus 
               (this is what GTK does to draw native entries) */
            x += focus_width;
            y += focus_width;
            width -= 2 * focus_width;
            height -= 2 * focus_width;
        }
    }

    TSOffsetStyleGCs(style, x, y);
    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     cliprect, gEntryWidget, "entry", x, y, width, height);

    if (state->focused && !state->disabled) {
        if (!interior_focus) {
            TSOffsetStyleGCs(style, rect->x, rect->y);
            gtk_paint_focus(style, drawable,  GTK_STATE_NORMAL, cliprect,
                            gEntryWidget, "entry",
                            rect->x, rect->y, rect->width, rect->height);
        }

        /* Now unset the focus flag. We don't want other entries to look like they're focused too! */
        GTK_WIDGET_UNSET_FLAGS(gEntryWidget, GTK_HAS_FOCUS);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_option_menu_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;
    gint tab_x, tab_y;
    gboolean interior_focus;
    GtkRequisition indicator_size;
    GtkBorder indicator_spacing;
    gint focus_width;
    gint focus_pad;

    ensure_option_menu_widget();
    moz_gtk_option_menu_get_metrics(&interior_focus, &indicator_size,
                                    &indicator_spacing, &focus_width,
                                    &focus_pad);

    style = gOptionMenuWidget->style;

    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        width -= 2 * (focus_width + focus_pad);
        height -= 2 * (focus_width + focus_pad);
    }

    TSOffsetStyleGCs(style, x, y);
    gtk_paint_box(style, drawable, state_type, GTK_SHADOW_OUT,
                  cliprect, gOptionMenuWidget, "optionmenu",
                  x, y, width, height);
      
    if (gtk_widget_get_direction(gOptionMenuWidget) == GTK_TEXT_DIR_RTL) {
        tab_x = x + indicator_spacing.right + XTHICKNESS(style);
    } else {
        tab_x = x + width - indicator_size.width - indicator_spacing.right -
                XTHICKNESS(style);
    }
    tab_y = y + (height - indicator_size.height) / 2;

    TSOffsetStyleGCs(style, tab_x, tab_y);
    gtk_paint_tab(style, drawable, state_type, GTK_SHADOW_OUT, cliprect,
                  gOptionMenuWidget, "optionmenutab", tab_x, tab_y, 
                  indicator_size.width, indicator_size.height);
      
    if (state->focused) {
      if (interior_focus) {
          x += XTHICKNESS(style) + focus_pad;
          y += YTHICKNESS(style) + focus_pad;
          /* Standard GTK combos have their focus ring around the entire
             control, not just the text bit */
          width -= 2 * (XTHICKNESS(style) + focus_pad);
          height -= 2 * (YTHICKNESS(style) + focus_pad);
      } else {
          x -= focus_width + focus_pad;
          y -= focus_width + focus_pad;
          width += 2 * (focus_width + focus_pad);
          height += 2 * (focus_width + focus_pad);
      }
        
      TSOffsetStyleGCs(style, x, y);
      gtk_paint_focus (style, drawable, state_type, cliprect, gOptionMenuWidget,
                       "button", x, y,  width, height);
    }
    
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_dropdown_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect, GtkWidgetState* state)
{
    GdkRectangle arrow_rect, real_arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;

    ensure_arrow_widget();
    moz_gtk_button_paint(drawable, rect, cliprect, state, GTK_RELIEF_NORMAL,
                         gDropdownButtonWidget);

    /* This mirrors gtkbutton's child positioning */
    style = gDropdownButtonWidget->style;
    arrow_rect.x = rect->x + 1 + XTHICKNESS(gDropdownButtonWidget->style);
    arrow_rect.y = rect->y + 1 + YTHICKNESS(gDropdownButtonWidget->style);
    arrow_rect.width = MAX(1, rect->width - (arrow_rect.x - rect->x) * 2);
    arrow_rect.height = MAX(1, rect->height - (arrow_rect.y - rect->y) * 2);

    calculate_arrow_dimensions(&arrow_rect, &real_arrow_rect);
    style = gArrowWidget->style;
    TSOffsetStyleGCs(style, real_arrow_rect.x, real_arrow_rect.y);

    real_arrow_rect.width = real_arrow_rect.height =
        MIN (real_arrow_rect.width, real_arrow_rect.height) * 0.9; 

    real_arrow_rect.x = floor (arrow_rect.x + ((arrow_rect.width - real_arrow_rect.width) / 2) + 0.5);
    real_arrow_rect.y = floor (arrow_rect.y + ((arrow_rect.height - real_arrow_rect.height) / 2) + 0.5);

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gHorizScrollbarWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    real_arrow_rect.x, real_arrow_rect.y,
                    real_arrow_rect.width, real_arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_container_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state, 
                        gboolean isradio)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkStyle* style;
    gboolean interior_focus;
    gint focus_width, focus_pad;

    if (isradio) {
        ensure_radiobutton_widget();
        style = gRadiobuttonWidget->style;  
        moz_gtk_radio_get_focus(&interior_focus, &focus_width, &focus_pad);
    } else {
        ensure_checkbox_widget();
        style = gCheckboxWidget->style;
        moz_gtk_checkbox_get_focus(&interior_focus, &focus_width, &focus_pad);
    }

    TSOffsetStyleGCs(style, rect->x, rect->y);

    /* The detail argument for the gtk_paint_* calls below are "checkbutton"
       even for radio buttons, to match what gtk does. */

    /* this is for drawing a prelight box */
    if (state_type == GTK_STATE_PRELIGHT || state_type == GTK_STATE_ACTIVE) {
        gtk_paint_flat_box(style, drawable, GTK_STATE_PRELIGHT, GTK_SHADOW_ETCHED_OUT,
                           cliprect, gCheckboxWidget,
                           "checkbutton",
                           rect->x, rect->y, rect->width, rect->height);
    }

    if (state_type != GTK_STATE_NORMAL && state_type != GTK_STATE_PRELIGHT)
        state_type = GTK_STATE_NORMAL;

    if (state->focused && !interior_focus) {
        gtk_paint_focus(style, drawable, state_type, cliprect, gCheckboxWidget,
                        "checkbutton",
                        rect->x, rect->y, rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_label_paint(GdkDrawable* drawable, GdkRectangle* rect,
                           GdkRectangle* cliprect, GtkWidgetState* state, 
                           gboolean isradio)
{
    GtkStateType state_type;
    GtkStyle *style;
    GtkWidget *widget;
    gboolean interior_focus;

    if (!state->focused)
        return MOZ_GTK_SUCCESS;

    if (isradio) {
        ensure_radiobutton_widget();
        widget = gRadiobuttonWidget;
    } else {
        ensure_checkbox_widget();
        widget = gCheckboxWidget;
    }

    gtk_widget_style_get(widget, "interior-focus", &interior_focus, NULL);
    if (!interior_focus)
        return MOZ_GTK_SUCCESS;

    state_type = ConvertGtkState(state);

    style = widget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    /* Always "checkbutton" to match gtkcheckbutton.c */
    gtk_paint_focus(style, drawable, state_type, cliprect, widget,
                    "checkbutton",
                    rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toolbar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect)
{
    GtkStyle* style;
    GtkShadowType shadow_type;

    ensure_toolbar_widget();
    style = gToolbarWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_style_apply_default_background(style, drawable, TRUE,
                                       GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);

    gtk_paint_box (style, drawable, GTK_STATE_NORMAL, gToolbarShadowType,
                   cliprect, gToolbarWidget, "toolbar",
                   rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tooltip_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect)
{
    GtkStyle* style;

    ensure_tooltip_widget();
    style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                      "gtk-tooltips", "GtkWindow",
                                      GTK_TYPE_WINDOW);

    style = gtk_style_attach(style, gTooltipWidget->window);
    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_flat_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                       cliprect, gTooltipWidget, "tooltip",
                       rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_frame_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect)
{
    GtkStyle* style = gProtoWindow->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_flat_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                       NULL, gProtoWindow, "base", rect->x, rect->y,
                       rect->width, rect->height);

    ensure_frame_widget();
    style = gFrameWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     cliprect, gFrameWidget, "frame", rect->x, rect->y,
                     rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progressbar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect)
{
    GtkStyle* style;

    ensure_progress_widget();
    style = gProgressWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                  cliprect, gProgressWidget, "trough", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progress_chunk_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect)
{
    GtkStyle* style;

    ensure_progress_widget();
    style = gProgressWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
                  cliprect, gProgressWidget, "bar", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tab_paint(GdkDrawable* drawable, GdkRectangle* rect,
                  GdkRectangle* cliprect, gint flags)
{
    /*
     * In order to get the correct shadows and highlights, GTK paints
     * tabs right-to-left (end-to-beginning, to be generic), leaving
     * out the active tab, and then paints the current tab once
     * everything else is painted.  In addition, GTK uses a 2-pixel
     * overlap between adjacent tabs (this value is hard-coded in
     * gtknotebook.c).  For purposes of mapping to gecko's frame
     * positions, we put this overlap on the far edge of the frame
     * (i.e., for a horizontal/top tab strip, we shift the left side
     * of each tab 2px to the left, into the neighboring tab's frame
     * rect.  The right 2px * of a tab's frame will be referred to as
     * the "overlap area".
     *
     * Since we can't guarantee painting order with gecko, we need to
     * manage the overlap area manually. There are three types of tab
     * boundaries we need to handle:
     *
     * * two non-active tabs: In this case, we just have both tabs
     *   paint normally.
     *
     * * non-active to active tab: Here, we need the tab on the left to paint
     *                             itself normally, then paint the edge of the
     *                             active tab in its overlap area.
     *
     * * active to non-active tab: In this case, we just have both tabs paint
     *                             normally.
     *
     * We need to make an exception for the first tab - since there is
     * no tab to the left to paint the overlap area, we do _not_ shift
     * the tab left by 2px.
     */

    GtkStyle* style;
    ensure_tab_widget();

    if (!(flags & MOZ_GTK_TAB_FIRST)) {
        rect->x -= 2;
        rect->width += 2;
    }

    style = gTabWidget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_extension(style, drawable,
                        ((flags & MOZ_GTK_TAB_SELECTED) ?
                         GTK_STATE_NORMAL : GTK_STATE_ACTIVE),
                        GTK_SHADOW_OUT, cliprect, gTabWidget, "tab", rect->x,
                        rect->y, rect->width, rect->height, GTK_POS_BOTTOM);

    if (flags & MOZ_GTK_TAB_BEFORE_SELECTED) {
        gboolean before_selected = ((flags & MOZ_GTK_TAB_BEFORE_SELECTED)!=0);
        cliprect->y -= 2;
        cliprect->height += 2;

        TSOffsetStyleGCs(style, rect->x + rect->width - 2,
                         rect->y - (2 * before_selected));

        gtk_paint_extension(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                            cliprect, gTabWidget, "tab",
                            rect->x + rect->width - 2,
                            rect->y - (2 * before_selected), rect->width,
                            rect->height + (2 * before_selected),
                            GTK_POS_BOTTOM);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tabpanels_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect)
{
    GtkStyle* style;

    ensure_tab_widget();
    style = gTabWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                  cliprect, gTabWidget, "notebook", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_bar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect)
{
    GtkStyle* style;
    GtkShadowType shadow_type;
    ensure_menu_bar_widget();
    style = gMenuBarWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, gMenuBarShadowType,
                  cliprect, gMenuBarWidget, "menubar", rect->x, rect->y,
                  rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_popup_paint(GdkDrawable* drawable, GdkRectangle* rect,
                         GdkRectangle* cliprect)
{
    GtkStyle* style;
    ensure_menu_popup_widget();
    style = gMenuPopupWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE, GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT, 
                  cliprect, gMenuPopupWidget, "menu",
                  rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_item_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStyle* style;
    GtkShadowType shadow_type;

    if (state->inHover && !state->disabled) {
        ensure_menu_item_widget();

        style = gMenuItemWidget->style;
        TSOffsetStyleGCs(style, rect->x, rect->y);
        if (have_menu_shadow_type) {
            gtk_widget_style_get(gMenuItemWidget, "selected_shadow_type",
                                 &shadow_type, NULL);
        } else {
            shadow_type = GTK_SHADOW_OUT;
        }

        gtk_paint_box(style, drawable, GTK_STATE_PRELIGHT, shadow_type,
                      cliprect, gMenuItemWidget, "menuitem", rect->x, rect->y,
                      rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_check_menu_item_paint(GdkDrawable* drawable, GdkRectangle* rect,
                              GdkRectangle* cliprect, GtkWidgetState* state,
                              gboolean checked, gboolean isradio)
{
    GtkStateType state_type;
    GtkStyle* style;
    GtkShadowType shadow_type = (checked)?GTK_SHADOW_IN:GTK_SHADOW_OUT;
    gint offset;
    gint indicator_size = 8; /* it's a fixed value in gtk 2.2 */
    gint x, y;
    
    moz_gtk_menu_item_paint(drawable, rect, cliprect, state);
    
    ensure_check_menu_item_widget();

    if (checked || GTK_CHECK_MENU_ITEM(gCheckMenuItemWidget)->always_show_toggle) {
      style = gCheckMenuItemWidget->style;
      
      if (state->inHover && !state->disabled) {
        state_type = GTK_STATE_PRELIGHT;
      } else {
        state_type = GTK_STATE_NORMAL;
      }
      
      offset = GTK_CONTAINER(gCheckMenuItemWidget)->border_width + 
             gCheckMenuItemWidget->style->xthickness + 2;
      
      x = rect->x + offset;
      y = rect->y + (rect->height - indicator_size) / 2;
                                                                                
      TSOffsetStyleGCs(style, x, y);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gCheckMenuItemWidget),
                                     checked);
      
      if (isradio) {
        gtk_paint_option(style, drawable, state_type, shadow_type, cliprect,
                         gCheckMenuItemWidget, "option",
                         x, y, indicator_size, indicator_size);
      } else {
        gtk_paint_check(style, drawable, state_type, shadow_type, cliprect,
                        gCheckMenuItemWidget, "check",
                        x, y, indicator_size, indicator_size);
      }
    }
    
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_window_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect)
{
    GtkStyle* style;

    ensure_window_widget();
    style = gProtoWindow->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_style_apply_default_background(style, drawable, TRUE,
                                       GTK_STATE_NORMAL,
                                       cliprect, rect->x, rect->y,
                                       rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_widget_border(GtkThemeWidgetType widget, gint* left, gint* top,
                          gint* right, gint* bottom, gboolean inhtml)
{
    GtkWidget* w;

    switch (widget) {
    case MOZ_GTK_BUTTON:
        {
            /* Constant in gtkbutton.c */
            static const gint child_spacing = 1;
            gboolean interior_focus;
            gint focus_width, focus_pad;

            ensure_button_widget();
            *left = *top = *right = *bottom = GTK_CONTAINER(gButtonWidget)->border_width;

            /* Don't add this padding in HTML, otherwise the buttons will
               become too big and stuff the layout. */
            if (!inhtml) {
                moz_gtk_button_get_focus(&interior_focus, &focus_width, &focus_pad);
                *left += focus_width + focus_pad + child_spacing;
                *right += focus_width + focus_pad + child_spacing;
                *top += focus_width + focus_pad + child_spacing;
                *bottom += focus_width + focus_pad + child_spacing;
            }

            *left += gButtonWidget->style->xthickness;
            *right += gButtonWidget->style->xthickness;
            *top += gButtonWidget->style->ythickness;
            *bottom += gButtonWidget->style->ythickness;
            return MOZ_GTK_SUCCESS;
        }

    case MOZ_GTK_TOOLBAR:
        ensure_toolbar_widget();
        w = gToolbarWidget;
        break;
    case MOZ_GTK_ENTRY:
        ensure_entry_widget();
        w = gEntryWidget;
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        ensure_arrow_widget();
        w = gDropdownButtonWidget;
        break;
    case MOZ_GTK_DROPDOWN:
        {
            /* We need to account for the arrow on the dropdown, so text doesn't
               come too close to the arrow, or in some cases spill into the arrow. */
            gboolean interior_focus;
            GtkRequisition indicator_size;
            GtkBorder indicator_spacing;
            gint focus_width, focus_pad;

            ensure_option_menu_widget();
            *right = *left = gOptionMenuWidget->style->xthickness;
            *bottom = *top = gOptionMenuWidget->style->ythickness;
            moz_gtk_option_menu_get_metrics(&interior_focus, &indicator_size,
                                            &indicator_spacing, &focus_width, &focus_pad);

            if (gtk_widget_get_direction(gOptionMenuWidget) == GTK_TEXT_DIR_RTL)
                *left += indicator_spacing.left + indicator_size.width + indicator_spacing.right;
            else
                *right += indicator_spacing.left + indicator_size.width + indicator_spacing.right;
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TABPANELS:
        ensure_tab_widget();
        w = gTabWidget;
        break;
    case MOZ_GTK_PROGRESSBAR:
        ensure_progress_widget();
        w = gProgressWidget;
        break;
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        ensure_spin_widget();
        w = gSpinWidget;
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
        ensure_scale_widget();
        w = gHScaleWidget;
        break;
    case MOZ_GTK_SCALE_VERTICAL:
        ensure_scale_widget();
        w = gVScaleWidget;
        break;
    case MOZ_GTK_FRAME:
        ensure_frame_widget();
        w = gFrameWidget;
        break;
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
        {
            gboolean interior_focus;
            gint focus_width, focus_pad;

            /* If the focus is interior, then the label has a border of
               (focus_width + focus_pad). */
            if (widget == MOZ_GTK_CHECKBUTTON_LABEL)
                moz_gtk_checkbox_get_focus(&interior_focus,
                                           &focus_width, &focus_pad);
            else
                moz_gtk_radio_get_focus(&interior_focus,
                                        &focus_width, &focus_pad);

            if (interior_focus)
                *left = *top = *right = *bottom = (focus_width + focus_pad);
            else
                *left = *top = *right = *bottom = 0;

            return MOZ_GTK_SUCCESS;
        }

    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        {
            gboolean interior_focus;
            gint focus_width, focus_pad;

            /* If the focus is _not_ interior, then the container has a border
               of (focus_width + focus_pad). */
            if (widget == MOZ_GTK_CHECKBUTTON_CONTAINER) {
                moz_gtk_checkbox_get_focus(&interior_focus,
                                           &focus_width, &focus_pad);
                w = gCheckboxWidget;
            } else {
                moz_gtk_radio_get_focus(&interior_focus,
                                        &focus_width, &focus_pad);
                w = gRadiobuttonWidget;
            }

            *left = *top = *right = *bottom = GTK_CONTAINER(w)->border_width;

            if (!interior_focus) {
                *left += (focus_width + focus_pad);
                *right += (focus_width + focus_pad);
                *top += (focus_width + focus_pad);
                *bottom += (focus_width + focus_pad);
            }

            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_MENUBAR:
        ensure_menu_bar_widget();
        w = gMenuBarWidget;
        break;
    case MOZ_GTK_MENUPOPUP:
        ensure_menu_popup_widget();
        w = gMenuPopupWidget;
        break;
    case MOZ_GTK_MENUITEM:
        ensure_menu_item_widget();
        w = gMenuItemWidget;
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        ensure_check_menu_item_widget();
        w = gCheckMenuItemWidget;
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
    case MOZ_GTK_GRIPPER:
    case MOZ_GTK_PROGRESS_CHUNK:
    case MOZ_GTK_TAB:
    /* These widgets have no borders.*/
    case MOZ_GTK_TOOLTIP:
    case MOZ_GTK_WINDOW:
        *left = *top = *right = *bottom = 0;
        return MOZ_GTK_SUCCESS;
    default:
        g_warning("Unsupported widget type: %d", widget);
        return MOZ_GTK_UNKNOWN_WIDGET;
    }

    *right = *left = XTHICKNESS(w->style);
    *bottom = *top = YTHICKNESS(w->style);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_dropdown_arrow_size(gint* width, gint* height)
{
    ensure_arrow_widget();

    /*
     * First get the border of the dropdown arrow, then add in the requested
     * size of the arrow.  Note that the minimum arrow size is fixed at
     * 11 pixels.
     */

    *width = 2 * (1 + XTHICKNESS(gDropdownButtonWidget->style));
    *width += 11 + GTK_MISC(gArrowWidget)->xpad * 2;

    *height = 2 * (1 + YTHICKNESS(gDropdownButtonWidget->style));
    *height += 11 + GTK_MISC(gArrowWidget)->ypad * 2;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_scalethumb_metrics(GtkOrientation orient, gint* thumb_length, gint* thumb_height)
{
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((orient == GTK_ORIENTATION_HORIZONTAL) ? gHScaleWidget : gVScaleWidget);

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

    gtk_widget_style_get (gHorizScrollbarWidget,
                          "slider_width", &metrics->slider_width,
                          "trough_border", &metrics->trough_border,
                          "stepper_size", &metrics->stepper_size,
                          "stepper_spacing", &metrics->stepper_spacing,
                          NULL);

    metrics->min_slider_size =
        GTK_RANGE(gHorizScrollbarWidget)->min_slider_size;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, GdkDrawable* drawable,
                     GdkRectangle* rect, GdkRectangle* cliprect,
                     GtkWidgetState* state, gint flags)
{
    switch (widget) {
    case MOZ_GTK_BUTTON:
        ensure_button_widget();
        return moz_gtk_button_paint(drawable, rect, cliprect, state,
                                    (GtkReliefStyle) flags, gButtonWidget);
        break;
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
        return moz_gtk_toggle_paint(drawable, rect, cliprect, state,
                                    (gboolean) flags,
                                    (widget == MOZ_GTK_RADIOBUTTON));
        break;
    case MOZ_GTK_SCROLLBAR_BUTTON:
        return moz_gtk_scrollbar_button_paint(drawable, rect, cliprect,
                                              state, (GtkArrowType) flags);
        break;
    case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
        return moz_gtk_scrollbar_trough_paint(widget, drawable, rect,
                                              cliprect, state);
        break;
    case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
    case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
        return moz_gtk_scrollbar_thumb_paint(widget, drawable, rect,
                                             cliprect, state);
        break;
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
        return moz_gtk_scale_paint(drawable, rect, cliprect, state, (GtkOrientation) flags);
        break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
        return moz_gtk_scale_thumb_paint(drawable, rect, cliprect, state, (GtkOrientation) flags);
        break;
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        return moz_gtk_spin_paint(drawable, rect,
                                  (widget == MOZ_GTK_SPINBUTTON_DOWN), state);
        break;
    case MOZ_GTK_GRIPPER:
        return moz_gtk_gripper_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_ENTRY:
        return moz_gtk_entry_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_DROPDOWN:
        return moz_gtk_option_menu_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        return moz_gtk_dropdown_arrow_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        return moz_gtk_container_paint(drawable, rect, cliprect, state,
                                       (widget == MOZ_GTK_RADIOBUTTON_CONTAINER));
        break;
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
        return moz_gtk_toggle_label_paint(drawable, rect, cliprect, state,
                                          (widget == MOZ_GTK_RADIOBUTTON_LABEL));
        break;
    case MOZ_GTK_TOOLBAR:
        return moz_gtk_toolbar_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_TOOLTIP:
        return moz_gtk_tooltip_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_FRAME:
        return moz_gtk_frame_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_PROGRESSBAR:
        return moz_gtk_progressbar_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_PROGRESS_CHUNK:
        return moz_gtk_progress_chunk_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_TAB:
        return moz_gtk_tab_paint(drawable, rect, cliprect, flags);
        break;
    case MOZ_GTK_TABPANELS:
        return moz_gtk_tabpanels_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_MENUBAR:
        return moz_gtk_menu_bar_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_MENUPOPUP:
        return moz_gtk_menu_popup_paint(drawable, rect, cliprect);
        break;
    case MOZ_GTK_MENUITEM:
        return moz_gtk_menu_item_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        return moz_gtk_check_menu_item_paint(drawable, rect, cliprect, state,
                                             (gboolean) flags,
                                             (widget == MOZ_GTK_RADIOMENUITEM));
        break;
    case MOZ_GTK_WINDOW:
        return moz_gtk_window_paint(drawable, rect, cliprect);
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
    return gHorizScrollbarWidget;
}

gint
moz_gtk_shutdown()
{
    if (gTooltipWidget)
        gtk_widget_destroy(gTooltipWidget);
    /* This will destroy all of our widgets */
    if (gProtoWindow)
        gtk_widget_destroy(gProtoWindow);

    gProtoWindow = NULL;
    gButtonWidget = NULL;
    gCheckboxWidget = NULL;
    gRadiobuttonWidget = NULL;
    gHorizScrollbarWidget = NULL;
    gVertScrollbarWidget = NULL;
    gHScaleWidget = NULL;
    gVScaleWidget = NULL;
    gEntryWidget = NULL;
    gArrowWidget = NULL;
    gDropdownButtonWidget = NULL;
    gHandleBoxWidget = NULL;
    gToolbarWidget = NULL;
    gFrameWidget = NULL;
    gProgressWidget = NULL;
    gTabWidget = NULL;
    gTooltipWidget = NULL;
    gMenuBarWidget = NULL;
    gMenuBarItemWidget = NULL;
    gMenuPopupWidget = NULL;
    gMenuItemWidget = NULL;
    gCheckMenuItemWidget = NULL;

    is_initialized = FALSE;

    return MOZ_GTK_SUCCESS;
}
