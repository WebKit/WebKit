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
static GtkWidget* gToggleButtonWidget;
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
static GtkWidget* gComboBoxEntryWidget;
static GtkWidget* gDropdownEntryWidget;
static GtkWidget* gDropdownButtonWidget;
static GtkWidget* gHandleBoxWidget;
static GtkWidget* gToolbarWidget;
static GtkWidget* gFrameWidget;
static GtkWidget* gStatusbarWidget;
static GtkWidget* gProgressWidget;
static GtkWidget* gTabWidget;
static GtkWidget* gTooltipWidget;
static GtkWidget* gMenuBarWidget;
static GtkWidget* gMenuBarItemWidget;
static GtkWidget* gMenuPopupWidget;
static GtkWidget* gMenuItemWidget;
static GtkWidget* gCheckMenuItemWidget;
static GtkWidget* gTreeViewWidget;
static GtkWidget* gTreeHeaderCellWidget;
static GtkWidget* gTreeHeaderSortArrowWidget;
static GtkWidget* gExpanderWidget;
static GtkWidget* gToolbarSeparatorWidget;
static GtkWidget* gMenuSeparatorWidget;
static GtkWidget* gHPanedWidget;
static GtkWidget* gVPanedWidget;

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
        protoLayout = gtk_fixed_new();
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
ensure_hpaned_widget()
{
    if (!gHPanedWidget) {
        gHPanedWidget = gtk_hpaned_new();
        setup_widget_prototype(gHPanedWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_vpaned_widget()
{
    if (!gVPanedWidget) {
        gVPanedWidget = gtk_vpaned_new();
        setup_widget_prototype(gVPanedWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_toggle_button_widget()
{
    if (!gToggleButtonWidget) {
        gToggleButtonWidget = gtk_toggle_button_new();
        setup_widget_prototype(gToggleButtonWidget);
        /* toggle button must be set active to get the right style on hover. */
        GTK_TOGGLE_BUTTON(gToggleButtonWidget)->active = TRUE;
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
ensure_combo_box_entry_widget()
{
    if (!gComboBoxEntryWidget) {
        gComboBoxEntryWidget = gtk_combo_box_entry_new();
        setup_widget_prototype(gComboBoxEntryWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_dropdown_entry_widget()
{
    if (!gDropdownEntryWidget) {
        ensure_combo_box_entry_widget();

        gDropdownEntryWidget = GTK_BIN(gComboBoxEntryWidget)->child;
        gtk_widget_realize(gDropdownEntryWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static void
moz_gtk_get_dropdown_button(GtkWidget *widget,
                            gpointer client_data)
{
    if (GTK_IS_TOGGLE_BUTTON(widget))
        gDropdownButtonWidget = widget;
}

static gint
ensure_arrow_widget()
{
    if (!gArrowWidget) {
        ensure_combo_box_entry_widget();

        gtk_container_forall(GTK_CONTAINER(gComboBoxEntryWidget),
                             moz_gtk_get_dropdown_button,
                             NULL);

        gArrowWidget = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
        gtk_container_add(GTK_CONTAINER(GTK_BIN(gDropdownButtonWidget)->child),
                          gArrowWidget);
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
ensure_toolbar_separator_widget()
{
    if (!gToolbarSeparatorWidget) {
        ensure_toolbar_widget();
        gToolbarSeparatorWidget = GTK_WIDGET(gtk_separator_tool_item_new());
        setup_widget_prototype(gToolbarSeparatorWidget);
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
ensure_statusbar_widget()
{
    if (!gStatusbarWidget) {
      gStatusbarWidget = gtk_statusbar_new();
      setup_widget_prototype(gStatusbarWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_frame_widget()
{
    if (!gFrameWidget) {
        ensure_statusbar_widget();
        gFrameWidget = gtk_frame_new(NULL);
        gtk_container_add(GTK_CONTAINER(gStatusbarWidget), gFrameWidget);
        gtk_widget_realize(gFrameWidget);
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
ensure_menu_separator_widget()
{
    if (!gMenuSeparatorWidget) {
        ensure_menu_popup_widget();
        gMenuSeparatorWidget = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(gMenuPopupWidget),
                              gMenuSeparatorWidget);
        gtk_widget_realize(gMenuSeparatorWidget);
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

static gint
ensure_tree_view_widget()
{
    if (!gTreeViewWidget) {
        gTreeViewWidget = gtk_tree_view_new();
        setup_widget_prototype(gTreeViewWidget);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_tree_header_cell_widget()
{
    if(!gTreeHeaderCellWidget) {
        GtkTreeViewColumn* treeViewColumn;
        ensure_tree_view_widget();

        treeViewColumn = gtk_tree_view_column_new();
        gtk_tree_view_column_set_title(treeViewColumn, "M");

        gtk_tree_view_append_column(GTK_TREE_VIEW(gTreeViewWidget), treeViewColumn);
        gTreeHeaderCellWidget = treeViewColumn->button;
        gtk_tree_view_column_set_sort_indicator(treeViewColumn, TRUE);
        gTreeHeaderSortArrowWidget = treeViewColumn->arrow;
    }
    return MOZ_GTK_SUCCESS;
}

static gint
ensure_expander_widget()
{
    if (!gExpanderWidget) {
        gExpanderWidget = gtk_expander_new("M");
        setup_widget_prototype(gExpanderWidget);
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

static gint
moz_gtk_button_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state,
                     GtkReliefStyle relief, GtkWidget* widget,
                     GtkTextDirection direction)
{
    GtkShadowType shadow_type;
    GtkStyle* style = widget->style;
    GtkStateType button_state = ConvertGtkState(state);
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;

    gboolean interior_focus;
    gint focus_width, focus_pad;

    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width, &focus_pad);

    if (WINDOW_IS_MAPPED(drawable)) {
        gdk_window_set_back_pixmap(drawable, NULL, TRUE);
        gdk_window_clear_area(drawable, cliprect->x, cliprect->y,
                              cliprect->width, cliprect->height);
    }

    gtk_widget_set_state(widget, button_state);
    gtk_widget_set_direction(widget, direction);

    if (state->isDefault)
        GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_DEFAULT);

    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        width -= 2 * (focus_width + focus_pad);
        height -= 2 * (focus_width + focus_pad);
    }

    shadow_type = button_state == GTK_STATE_ACTIVE ||
                      state->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
 
    if (state->isDefault && GTK_BUTTON(widget)->relief == GTK_RELIEF_NORMAL) {
        gtk_paint_box(style, drawable, button_state, shadow_type, cliprect,
                      widget, "buttondefault", x, y, width, height);                   
    }
 
    if (relief != GTK_RELIEF_NONE || state->depressed ||
           (button_state != GTK_STATE_NORMAL &&
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

gint
moz_gtk_splitter_get_metrics(gint orientation, gint* size)
{
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        ensure_hpaned_widget();
        gtk_widget_style_get(gHPanedWidget, "handle_size", size, NULL);
    } else {
        ensure_vpaned_widget();
        gtk_widget_style_get(gVPanedWidget, "handle_size", size, NULL);
    }
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state,
                     gboolean selected, gboolean isradio,
                     GtkTextDirection direction)
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
    gtk_widget_set_direction(w, direction);
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
                               GtkScrollbarButtonFlags flags,
                               GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = (state->active) ?
        GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GdkRectangle button_rect;
    GdkRectangle arrow_rect;
    GtkStyle* style;
    GtkWidget *scrollbar;
    GtkArrowType arrow_type;
    const char* detail = (flags & MOZ_GTK_STEPPER_VERTICAL) ?
                           "vscrollbar" : "hscrollbar";

    ensure_scrollbar_widget();

    if (flags & MOZ_GTK_STEPPER_VERTICAL)
        scrollbar = gVertScrollbarWidget;
    else
        scrollbar = gHorizScrollbarWidget;

    gtk_widget_set_direction(scrollbar, direction);

    /* Some theme engines (i.e., ClearLooks) check the scrollbar's allocation
       to determine where it should paint rounded corners on the buttons.
       We need to trick them into drawing the buttons the way we want them. */

    scrollbar->allocation.x = rect->x;
    scrollbar->allocation.y = rect->y;
    scrollbar->allocation.width = rect->width;
    scrollbar->allocation.height = rect->height;

    if (flags & MOZ_GTK_STEPPER_VERTICAL) {
        scrollbar->allocation.height *= 5;
        if (flags & MOZ_GTK_STEPPER_DOWN) {
            arrow_type = GTK_ARROW_DOWN;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.y -= 4 * rect->height;
            else
                scrollbar->allocation.y -= rect->height;

        } else {
            arrow_type = GTK_ARROW_UP;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.y -= 3 * rect->height;
        }
    } else {
        scrollbar->allocation.width *= 5;
        if (flags & MOZ_GTK_STEPPER_DOWN) {
            arrow_type = GTK_ARROW_RIGHT;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.x -= 4 * rect->width;
            else
                scrollbar->allocation.x -= rect->width;
        } else {
            arrow_type = GTK_ARROW_LEFT;
            if (flags & MOZ_GTK_STEPPER_BOTTOM)
                scrollbar->allocation.x -= 3 * rect->width;
        }
    }

    style = scrollbar->style;

    ensure_arrow_widget();
  
    calculate_arrow_dimensions(rect, &button_rect);
    TSOffsetStyleGCs(style, button_rect.x, button_rect.y);

    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  scrollbar, detail, button_rect.x, button_rect.y,
                  button_rect.width, button_rect.height);

    arrow_rect.width = button_rect.width / 2;
    arrow_rect.height = button_rect.height / 2;
    arrow_rect.x = button_rect.x + (button_rect.width - arrow_rect.width) / 2;
    arrow_rect.y = button_rect.y +
        (button_rect.height - arrow_rect.height) / 2;  

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
        scrollbar = GTK_SCROLLBAR(gHorizScrollbarWidget);
    else
        scrollbar = GTK_SCROLLBAR(gVertScrollbarWidget);

    gtk_widget_set_direction(GTK_WIDGET(scrollbar), direction);

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
                              GdkRectangle* cliprect, GtkWidgetState* state,
                              GtkTextDirection direction)
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
moz_gtk_spin_paint(GdkDrawable* drawable, GdkRectangle* rect,
                   GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_spin_widget();
    gtk_widget_set_direction(gSpinWidget, direction);
    style = gSpinWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN, NULL,
                  gSpinWidget, "spinbutton",
                  rect->x, rect->y, rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_spin_updown_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          gboolean isDown, GtkWidgetState* state,
                          GtkTextDirection direction)
{
    GdkRectangle arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state_type == GTK_STATE_ACTIVE ?
                                  GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;

    ensure_spin_widget();
    style = gSpinWidget->style;
    gtk_widget_set_direction(gSpinWidget, direction);

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
                    GtkOrientation flags, GtkTextDirection direction)
{
  gint x = 0, y = 0;
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gHScaleWidget : gVScaleWidget);
  gtk_widget_set_direction(widget, direction);

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
                          GtkOrientation flags, GtkTextDirection direction)
{
  GtkStateType state_type = ConvertGtkState(state);
  GtkStyle* style;
  GtkWidget* widget;
  gint thumb_width, thumb_height, x, y;

  ensure_scale_widget();
  widget = ((flags == GTK_ORIENTATION_HORIZONTAL) ? gHScaleWidget : gVScaleWidget);
  gtk_widget_set_direction(widget, direction);

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
                      GdkRectangle* cliprect, GtkWidgetState* state,
                      GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type;
    GtkStyle* style;

    ensure_handlebox_widget();
    gtk_widget_set_direction(gHandleBoxWidget, direction);

    style = gHandleBoxWidget->style;
    shadow_type = GTK_HANDLE_BOX(gHandleBoxWidget)->shadow_type;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, state_type, shadow_type, cliprect,
                  gHandleBoxWidget, "handlebox_bin", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_hpaned_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStateType hpaned_state = ConvertGtkState(state);

    ensure_hpaned_widget();
    gtk_paint_handle(gHPanedWidget->style, drawable, hpaned_state,
                     GTK_SHADOW_NONE, cliprect, gHPanedWidget, "paned",
                     rect->x, rect->y, rect->width, rect->height,
                     GTK_ORIENTATION_VERTICAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_vpaned_paint(GdkDrawable* drawable, GdkRectangle* rect,
                     GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStateType vpaned_state = ConvertGtkState(state);

    ensure_vpaned_widget();
    gtk_paint_handle(gVPanedWidget->style, drawable, vpaned_state,
                     GTK_SHADOW_NONE, cliprect, gVPanedWidget, "paned",
                     rect->x, rect->y, rect->width, rect->height,
                     GTK_ORIENTATION_HORIZONTAL);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_entry_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkWidgetState* state,
                    GtkWidget* widget, GtkTextDirection direction)
{
    gint x, y, width = rect->width, height = rect->height;
    GtkStyle* style;
    gboolean interior_focus;
    gint focus_width;

    gtk_widget_set_direction(widget, direction);

    style = widget->style;

    /* paint the background first */
    x = XTHICKNESS(style);
    y = YTHICKNESS(style);

    /* This gets us a lovely greyish disabledish look */
    gtk_widget_set_sensitive(widget, !state->disabled);

    TSOffsetStyleGCs(style, rect->x + x, rect->y + y);
    gtk_paint_flat_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
                       cliprect, widget, "entry_bg",  rect->x + x,
                       rect->y + y, rect->width - 2*x, rect->height - 2*y);

    gtk_widget_style_get(widget,
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
        GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);

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
                     cliprect, widget, "entry", x, y, width, height);

    if (state->focused && !state->disabled) {
        if (!interior_focus) {
            TSOffsetStyleGCs(style, rect->x, rect->y);
            gtk_paint_focus(style, drawable,  GTK_STATE_NORMAL, cliprect,
                            widget, "entry",
                            rect->x, rect->y, rect->width, rect->height);
        }

        /* Now unset the focus flag. We don't want other entries to look like they're focused too! */
        GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
    }

    return MOZ_GTK_SUCCESS;
}

static gint 
moz_gtk_treeview_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect, GtkWidgetState* state,
                       GtkTextDirection direction)
{
    gint xthickness, ythickness;

    GtkStyle *style;
    GtkStateType state_type;

    ensure_tree_view_widget();
    gtk_widget_set_direction(gTreeViewWidget, direction);

    /* only handle disabled and normal states, otherwise the whole background
     * area will be painted differently with other states */
    state_type = state->disabled ? GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;

    /* In GTK the treeview sets the background of the window
     * which contains the cells to the treeview base color.
     * If we don't set it here the background color will not be correct.*/
    gtk_widget_modify_bg(gTreeViewWidget, state_type,
                         &gTreeViewWidget->style->base[state_type]);

    style = gTreeViewWidget->style;
    xthickness = XTHICKNESS(style);
    ythickness = YTHICKNESS(style);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_flat_box(style, drawable, state_type, GTK_SHADOW_NONE,
                       cliprect, gTreeViewWidget, "treeview",
                       rect->x + xthickness, rect->y + ythickness,
                       rect->width - 2 * xthickness,
                       rect->height - 2 * ythickness);

    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                     cliprect, gTreeViewWidget, "scrolled_window",
                     rect->x, rect->y, rect->width, rect->height); 

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tree_header_cell_paint(GdkDrawable* drawable, GdkRectangle* rect,
                               GdkRectangle* cliprect, GtkWidgetState* state,
                               GtkTextDirection direction)
{
    moz_gtk_button_paint(drawable, rect, cliprect, state, GTK_RELIEF_NORMAL,
                         gTreeHeaderCellWidget, direction);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tree_header_sort_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                     GdkRectangle* cliprect,
                                     GtkWidgetState* state, GtkArrowType flags,
                                     GtkTextDirection direction)
{
    GdkRectangle arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = GTK_SHADOW_IN;
    GtkArrowType arrow_type = flags;
    GtkStyle* style;

    ensure_tree_header_cell_widget();
    gtk_widget_set_direction(gTreeHeaderSortArrowWidget, direction);

    /* hard code these values */
    arrow_rect.width = 11;
    arrow_rect.height = 11;
    arrow_rect.x = rect->x + (rect->width - arrow_rect.width) / 2;
    arrow_rect.y = rect->y + (rect->height - arrow_rect.height) / 2;

    style = gTreeHeaderSortArrowWidget->style;
    TSOffsetStyleGCs(style, arrow_rect.x, arrow_rect.y);

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gTreeHeaderSortArrowWidget, "arrow",  arrow_type, TRUE,
                    arrow_rect.x, arrow_rect.y,
                    arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_treeview_expander_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                GdkRectangle* cliprect, GtkWidgetState* state,
                                GtkExpanderStyle expander_state,
                                GtkTextDirection direction)
{
    GtkStyle *style;
    GtkStateType state_type;

    ensure_tree_view_widget();
    gtk_widget_set_direction(gTreeViewWidget, direction);

    style = gTreeViewWidget->style;

    /* Because the frame we get is of the entire treeview, we can't get the precise
     * event state of one expander, thus rendering hover and active feedback useless. */
    state_type = state->disabled ? GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_expander(style, drawable, state_type, cliprect, gTreeViewWidget, "treeview",
                       rect->x + rect->width / 2, rect->y + rect->height / 2, expander_state);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_expander_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect, GtkWidgetState* state,
                       GtkExpanderStyle expander_state,
                       GtkTextDirection direction)
{
    GtkStyle *style;
    GtkStateType state_type = ConvertGtkState(state);

    ensure_expander_widget();
    gtk_widget_set_direction(gExpanderWidget, direction);

    style = gExpanderWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_expander(style, drawable, state_type, cliprect, gExpanderWidget, "expander",
                       rect->x + rect->width / 2, rect->y + rect->height / 2, expander_state);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_option_menu_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect, GtkWidgetState* state,
                          GtkTextDirection direction)
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
    gtk_widget_set_direction(gOptionMenuWidget, direction);
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
      
    if (direction == GTK_TEXT_DIR_RTL) {
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
moz_gtk_downarrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GdkRectangle arrow_rect;

    ensure_arrow_widget();
    style = gArrowWidget->style;

    arrow_rect.x = rect->x + 1 + XTHICKNESS(style);
    arrow_rect.y = rect->y + 1 + YTHICKNESS(style);
    arrow_rect.width = MAX(1, rect->width - (arrow_rect.x - rect->x) * 2);
    arrow_rect.height = MAX(1, rect->height - (arrow_rect.y - rect->y) * 2);

    TSOffsetStyleGCs(style, arrow_rect.x, arrow_rect.y);
    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gArrowWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    arrow_rect.x, arrow_rect.y, arrow_rect.width, arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_dropdown_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect, GtkWidgetState* state,
                             GtkTextDirection direction)
{
    static gfloat arrow_scaling = 0.7;
    gint real_arrow_padding;
    GdkRectangle arrow_rect, real_arrow_rect;
    GtkStateType state_type = ConvertGtkState(state);
    GtkShadowType shadow_type = state->active ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    GtkStyle* style;

    ensure_arrow_widget();
    gtk_widget_set_direction(gDropdownButtonWidget, direction);

    moz_gtk_button_paint(drawable, rect, cliprect, state, GTK_RELIEF_NORMAL,
                         gDropdownButtonWidget, direction);

    /* This mirrors gtkbutton's child positioning */
    style = gDropdownButtonWidget->style;
    arrow_rect.x = rect->x + 1 + XTHICKNESS(style);
    arrow_rect.y = rect->y + 1 + YTHICKNESS(style);
    arrow_rect.width = MAX(1, rect->width - (arrow_rect.x - rect->x) * 2);
    arrow_rect.height = MAX(1, rect->height - (arrow_rect.y - rect->y) * 2);

    calculate_arrow_dimensions(&arrow_rect, &real_arrow_rect);
    style = gArrowWidget->style;
    TSOffsetStyleGCs(style, real_arrow_rect.x, real_arrow_rect.y);

    real_arrow_rect.width = real_arrow_rect.height =
        MIN (real_arrow_rect.width, real_arrow_rect.height) * arrow_scaling;

    real_arrow_padding = floor((arrow_rect.width - real_arrow_rect.width) / 2 + 0.5);
    real_arrow_rect.x = arrow_rect.x + real_arrow_padding;
    if (direction == GTK_TEXT_DIR_RTL)
        real_arrow_rect.x = arrow_rect.x + arrow_rect.width -
                            real_arrow_rect.width - real_arrow_padding;
    real_arrow_rect.y = floor (arrow_rect.y + ((arrow_rect.height - real_arrow_rect.height) / 2) + 0.5);

    gtk_paint_arrow(style, drawable, state_type, shadow_type, cliprect,
                    gDropdownButtonWidget, "arrow",  GTK_ARROW_DOWN, TRUE,
                    real_arrow_rect.x, real_arrow_rect.y,
                    real_arrow_rect.width, real_arrow_rect.height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_container_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state, 
                        gboolean isradio, GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkStyle* style;
    GtkWidget *widget;
    gboolean interior_focus;
    gint focus_width, focus_pad;

    if (isradio) {
        ensure_radiobutton_widget();
        widget = gRadiobuttonWidget;
    } else {
        ensure_checkbox_widget();
        widget = gCheckboxWidget;
    }
    gtk_widget_set_direction(widget, direction);

    style = widget->style;
    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width,
                             &focus_pad);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    /* The detail argument for the gtk_paint_* calls below are "checkbutton"
       even for radio buttons, to match what gtk does. */

    /* this is for drawing a prelight box */
    if (state_type == GTK_STATE_PRELIGHT || state_type == GTK_STATE_ACTIVE) {
        gtk_paint_flat_box(style, drawable, GTK_STATE_PRELIGHT,
                           GTK_SHADOW_ETCHED_OUT, cliprect, widget,
                           "checkbutton",
                           rect->x, rect->y, rect->width, rect->height);
    }

    if (state_type != GTK_STATE_NORMAL && state_type != GTK_STATE_PRELIGHT)
        state_type = GTK_STATE_NORMAL;

    if (state->focused && !interior_focus) {
        gtk_paint_focus(style, drawable, state_type, cliprect, widget,
                        "checkbutton",
                        rect->x, rect->y, rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_toggle_label_paint(GdkDrawable* drawable, GdkRectangle* rect,
                           GdkRectangle* cliprect, GtkWidgetState* state, 
                           gboolean isradio, GtkTextDirection direction)
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
    gtk_widget_set_direction(widget, direction);

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
                      GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;

    ensure_toolbar_widget();
    gtk_widget_set_direction(gToolbarWidget, direction);

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
moz_gtk_toolbar_separator_paint(GdkDrawable* drawable, GdkRectangle* rect,
                                GdkRectangle* cliprect,
                                GtkTextDirection direction)
{
    GtkStyle* style;
    gint     separator_width;
    gint     paint_width;
    gboolean wide_separators;
    
    /* Defined as constants in GTK+ 2.10.14 */
    const double start_fraction = 0.2;
    const double end_fraction = 0.8;

    ensure_toolbar_separator_widget();
    gtk_widget_set_direction(gToolbarSeparatorWidget, direction);

    style = gToolbarSeparatorWidget->style;

    gtk_widget_style_get(gToolbarWidget,
                         "wide-separators", &wide_separators,
                         "separator-width", &separator_width,
                         NULL);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    if (wide_separators) {
        if (separator_width > rect->width)
            separator_width = rect->width;

        gtk_paint_box(style, drawable,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                      cliprect, gToolbarWidget, "vseparator",
                      rect->x + (rect->width - separator_width) / 2,
                      rect->y + rect->height * start_fraction,
                      separator_width,
                      rect->height * (end_fraction - start_fraction));
                       
    } else {
        paint_width = style->xthickness;
        
        if (paint_width > rect->width)
            paint_width = rect->width;
    
        gtk_paint_vline(style, drawable,
                        GTK_STATE_NORMAL, cliprect, gToolbarSeparatorWidget,
                        "toolbar",
                        rect->y + rect->height * start_fraction,
                        rect->y + rect->height * end_fraction,
                        rect->x + (rect->width - paint_width) / 2);
    }
    
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tooltip_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_tooltip_widget();
    gtk_widget_set_direction(gTooltipWidget, direction);

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
moz_gtk_resizer_paint(GdkDrawable* drawable, GdkRectangle* rect,
                      GdkRectangle* cliprect, GtkWidgetState* state,
                      GtkTextDirection direction)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);

    ensure_window_widget();
    gtk_widget_set_direction(gProtoWindow, direction);

    style = gProtoWindow->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);

    gtk_paint_resize_grip(style, drawable, state_type, cliprect, gProtoWindow,
                          NULL, (direction == GTK_TEXT_DIR_LTR) ?
                          GDK_WINDOW_EDGE_SOUTH_EAST :
                          GDK_WINDOW_EDGE_SOUTH_WEST,
                          rect->x, rect->y, rect->width, rect->height);
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_frame_paint(GdkDrawable* drawable, GdkRectangle* rect,
                    GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;

    ensure_frame_widget();
    gtk_widget_set_direction(gFrameWidget, direction);

    style = gFrameWidget->style;

    gtk_widget_style_get(gStatusbarWidget, "shadow-type", &shadow_type, NULL);

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_shadow(style, drawable, GTK_STATE_NORMAL, shadow_type,
                     cliprect, gFrameWidget, "frame", rect->x, rect->y,
                     rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progressbar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                          GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_progress_widget();
    gtk_widget_set_direction(gProgressWidget, direction);

    style = gProgressWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                  cliprect, gProgressWidget, "trough", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_progress_chunk_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_progress_widget();
    gtk_widget_set_direction(gProgressWidget, direction);

    style = gProgressWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box(style, drawable, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
                  cliprect, gProgressWidget, "bar", rect->x, rect->y,
                  rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_tab_thickness(void)
{
    ensure_tab_widget();
    if (YTHICKNESS(gTabWidget->style) < 2)
        return 2; /* some themes don't set ythickness correctly */

    return YTHICKNESS(gTabWidget->style);
}

static gint
moz_gtk_tab_paint(GdkDrawable* drawable, GdkRectangle* rect,
                  GdkRectangle* cliprect, GtkTabFlags flags,
                  GtkTextDirection direction)
{
    /* When the tab isn't selected, we just draw a notebook extension.
     * When it is selected, we overwrite the adjacent border of the tabpanel
     * touching the tab with a pierced border (called "the gap") to make the
     * tab appear physically attached to the tabpanel; see details below. */

    GtkStyle* style;

    ensure_tab_widget();
    gtk_widget_set_direction(gTabWidget, direction);

    style = gTabWidget->style;
    TSOffsetStyleGCs(style, rect->x, rect->y);

    if ((flags & MOZ_GTK_TAB_SELECTED) == 0) {
        /* Only draw the tab */
        gtk_paint_extension(style, drawable, GTK_STATE_ACTIVE, GTK_SHADOW_OUT,
                            cliprect, gTabWidget, "tab",
                            rect->x, rect->y, rect->width, rect->height,
                            (flags & MOZ_GTK_TAB_BOTTOM) ?
                                GTK_POS_TOP : GTK_POS_BOTTOM );
    } else {
        /* Draw the tab and the gap
         * We want the gap to be positionned exactly on the tabpanel top
         * border; since tabbox.css may set a negative margin so that the tab
         * frame rect already overlaps the tabpanel frame rect, we need to take
         * that into account when drawing. To that effect, nsNativeThemeGTK
         * passes us this negative margin (bmargin in the graphic below) in the
         * lowest bits of |flags|.  We use it to set gap_voffset, the distance
         * between the top of the gap and the bottom of the tab (resp. the
         * bottom of the gap and the top of the tab when we draw a bottom tab),
         * while ensuring that the gap always touches the border of the tab,
         * i.e. 0 <= gap_voffset <= gap_height, to avoid surprinsing results
         * with big negative or positive margins.
         * Here is a graphical explanation in the case of top tabs:
         *             ___________________________
         *            /                           \
         *           |            T A B            |
         * ----------|. . . . . . . . . . . . . . .|----- top of tabpanel
         *           :    ^       bmargin          :  ^
         *           :    | (-negative margin,     :  |
         *  bottom   :    v  passed in flags)      :  |       gap_height
         *    of  -> :.............................:  |    (the size of the
         * the tab   .       part of the gap       .  |  tabpanel top border)
         *           .      outside of the tab     .  v
         * ----------------------------------------------
         *
         * To draw the gap, we use gtk_paint_box_gap(), see comment in
         * moz_gtk_tabpanels_paint(). This box_gap is made 3 * gap_height tall,
         * which should suffice to ensure that the only visible border is the
         * pierced one.  If the tab is in the middle, we make the box_gap begin
         * a bit to the left of the tab and end a bit to the right, adjusting
         * the gap position so it still is under the tab, because we want the
         * rendering of a gap in the middle of a tabpanel.  This is the role of
         * the gints gap_{l,r}_offset. On the contrary, if the tab is the
         * first, we align the start border of the box_gap with the start
         * border of the tab (left if LTR, right if RTL), by setting the
         * appropriate offset to 0.*/
        gint gap_loffset, gap_roffset, gap_voffset, gap_height;

        /* Get height needed by the gap */
        gap_height = moz_gtk_get_tab_thickness();

        /* Extract gap_voffset from the first bits of flags */
        gap_voffset = flags & MOZ_GTK_TAB_MARGIN_MASK;
        if (gap_voffset > gap_height)
            gap_voffset = gap_height;

        /* Set gap_{l,r}_offset to appropriate values */
        gap_loffset = gap_roffset = 20; /* should be enough */
        if (flags & MOZ_GTK_TAB_FIRST) {
            if (direction == GTK_TEXT_DIR_RTL)
                gap_roffset = 0;
            else
                gap_loffset = 0;
        }

        if (flags & MOZ_GTK_TAB_BOTTOM) {
            /* Enlarge the cliprect to have room for the full gap height */
            cliprect->height += gap_height - gap_voffset;
            cliprect->y -= gap_height - gap_voffset;

            /* Draw the tab */
            gtk_paint_extension(style, drawable, GTK_STATE_NORMAL,
                                GTK_SHADOW_OUT, cliprect, gTabWidget, "tab",
                                rect->x, rect->y + gap_voffset, rect->width,
                                rect->height - gap_voffset, GTK_POS_TOP);

            /* Draw the gap; erase with background color before painting in
             * case theme does not */
            gtk_style_apply_default_background(style, drawable, TRUE,
                                               GTK_STATE_NORMAL, cliprect,
                                               rect->x,
                                               rect->y + gap_voffset
                                                       - gap_height,
                                               rect->width, gap_height);
            gtk_paint_box_gap(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                              cliprect, gTabWidget, "notebook",
                              rect->x - gap_loffset,
                              rect->y + gap_voffset - 3 * gap_height,
                              rect->width + gap_loffset + gap_roffset,
                              3 * gap_height, GTK_POS_BOTTOM,
                              gap_loffset, rect->width);
        } else {
            /* Enlarge the cliprect to have room for the full gap height */
            cliprect->height += gap_height - gap_voffset;

            /* Draw the tab */
            gtk_paint_extension(style, drawable, GTK_STATE_NORMAL,
                                GTK_SHADOW_OUT, cliprect, gTabWidget, "tab",
                                rect->x, rect->y, rect->width,
                                rect->height - gap_voffset, GTK_POS_BOTTOM);

            /* Draw the gap; erase with background color before painting in
             * case theme does not */
            gtk_style_apply_default_background(style, drawable, TRUE,
                                               GTK_STATE_NORMAL, cliprect,
                                               rect->x,
                                               rect->y + rect->height
                                                       - gap_voffset,
                                               rect->width, gap_height);
            gtk_paint_box_gap(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                              cliprect, gTabWidget, "notebook",
                              rect->x - gap_loffset,
                              rect->y + rect->height - gap_voffset,
                              rect->width + gap_loffset + gap_roffset,
                              3 * gap_height, GTK_POS_TOP,
                              gap_loffset, rect->width);
        }

    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_tabpanels_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkTextDirection direction)
{
    /* We use gtk_paint_box_gap() to draw the tabpanels widget. gtk_paint_box()
     * draws an all-purpose box, which a lot of themes render differently.
     * A zero-width gap is still visible in most themes, so we hide it to the
     * left (10px should be enough) */
    GtkStyle* style;

    ensure_tab_widget();
    gtk_widget_set_direction(gTabWidget, direction);

    style = gTabWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_box_gap(style, drawable, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
                      cliprect, gTabWidget, "notebook", rect->x, rect->y,
                      rect->width, rect->height,
                      GTK_POS_TOP, -10, 0);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_bar_paint(GdkDrawable* drawable, GdkRectangle* rect,
                       GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;
    ensure_menu_bar_widget();
    gtk_widget_set_direction(gMenuBarWidget, direction);

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
                         GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    ensure_menu_popup_widget();
    gtk_widget_set_direction(gMenuPopupWidget, direction);

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
moz_gtk_menu_separator_paint(GdkDrawable* drawable, GdkRectangle* rect,
                             GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;
    gboolean wide_separators;
    gint separator_height;
    guint horizontal_padding;
    gint paint_height;

    ensure_menu_separator_widget();
    gtk_widget_set_direction(gMenuSeparatorWidget, direction);

    style = gMenuSeparatorWidget->style;

    gtk_widget_style_get(gMenuSeparatorWidget,
                         "wide-separators",    &wide_separators,
                         "separator-height",   &separator_height,
                         "horizontal-padding", &horizontal_padding,
                         NULL);

    TSOffsetStyleGCs(style, rect->x, rect->y);

    if (wide_separators) {
        if (separator_height > rect->height)
            separator_height = rect->height;

        gtk_paint_box(style, drawable,
                      GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
                      cliprect, gMenuSeparatorWidget, "hseparator",
                      rect->x + horizontal_padding + style->xthickness,
                      rect->y + (rect->height - separator_height - style->ythickness) / 2,
                      rect->width - 2 * (horizontal_padding + style->xthickness),
                      separator_height);
    } else {
        paint_height = style->ythickness;
        if (paint_height > rect->height)
            paint_height = rect->height;

        gtk_paint_hline(style, drawable,
                        GTK_STATE_NORMAL, cliprect, gMenuSeparatorWidget,
                        "menuitem",
                        rect->x + horizontal_padding + style->xthickness,
                        rect->x + rect->width - horizontal_padding - style->xthickness - 1,
                        rect->y + (rect->height - style->ythickness) / 2);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_item_paint(GdkDrawable* drawable, GdkRectangle* rect,
                        GdkRectangle* cliprect, GtkWidgetState* state,
                        gint flags, GtkTextDirection direction)
{
    GtkStyle* style;
    GtkShadowType shadow_type;
    GtkWidget* item_widget;

    if (state->inHover && !state->disabled) {
        if (flags & MOZ_TOPLEVEL_MENU_ITEM) {
            ensure_menu_bar_item_widget();
            item_widget = gMenuBarItemWidget;
        } else {
            ensure_menu_item_widget();
            item_widget = gMenuItemWidget;
        }
        gtk_widget_set_direction(item_widget, direction);
        
        style = item_widget->style;
        TSOffsetStyleGCs(style, rect->x, rect->y);
        if (have_menu_shadow_type) {
            gtk_widget_style_get(item_widget, "selected_shadow_type",
                                 &shadow_type, NULL);
        } else {
            shadow_type = GTK_SHADOW_OUT;
        }

        gtk_paint_box(style, drawable, GTK_STATE_PRELIGHT, shadow_type,
                      cliprect, item_widget, "menuitem", rect->x, rect->y,
                      rect->width, rect->height);
    }

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_menu_arrow_paint(GdkDrawable* drawable, GdkRectangle* rect,
                         GdkRectangle* cliprect, GtkWidgetState* state,
                         GtkTextDirection direction)
{
    GtkStyle* style;
    GtkStateType state_type = ConvertGtkState(state);

    ensure_menu_item_widget();
    gtk_widget_set_direction(gMenuItemWidget, direction);

    style = gMenuItemWidget->style;

    TSOffsetStyleGCs(style, rect->x, rect->y);
    gtk_paint_arrow(style, drawable, state_type,
                    (state_type == GTK_STATE_PRELIGHT) ? GTK_SHADOW_IN : GTK_SHADOW_OUT,
                    cliprect, gMenuItemWidget, "menuitem",
                    (direction == GTK_TEXT_DIR_LTR) ? GTK_ARROW_RIGHT : GTK_ARROW_LEFT,
                    TRUE, rect->x, rect->y, rect->width, rect->height);

    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_check_menu_item_paint(GdkDrawable* drawable, GdkRectangle* rect,
                              GdkRectangle* cliprect, GtkWidgetState* state,
                              gboolean checked, gboolean isradio,
                              GtkTextDirection direction)
{
    GtkStateType state_type = ConvertGtkState(state);
    GtkStyle* style;
    GtkShadowType shadow_type = (checked)?GTK_SHADOW_IN:GTK_SHADOW_OUT;
    gint offset;
    gint indicator_size;
    gint x, y;

    moz_gtk_menu_item_paint(drawable, rect, cliprect, state, FALSE, direction);

    ensure_check_menu_item_widget();
    gtk_widget_set_direction(gCheckMenuItemWidget, direction);

    gtk_widget_style_get (gCheckMenuItemWidget,
                          "indicator-size", &indicator_size,
                          NULL);

    if (checked || GTK_CHECK_MENU_ITEM(gCheckMenuItemWidget)->always_show_toggle) {
      style = gCheckMenuItemWidget->style;

      offset = GTK_CONTAINER(gCheckMenuItemWidget)->border_width +
             gCheckMenuItemWidget->style->xthickness + 2;

      /* while normally this "3" would be the horizontal-padding style value, passing it to Gecko
         as the value of menuitem padding causes problems with dropdowns (bug 406129), so in the menu.css
         file this is hardcoded as 3px. Yes it sucks, but we don't really have a choice. */
      x = (direction == GTK_TEXT_DIR_RTL) ?
            rect->width - indicator_size - offset - 3: rect->x + offset + 3;
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
                     GdkRectangle* cliprect, GtkTextDirection direction)
{
    GtkStyle* style;

    ensure_window_widget();
    gtk_widget_set_direction(gProtoWindow, direction);

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
                          gint* right, gint* bottom, GtkTextDirection direction,
                          gboolean inhtml)
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
                moz_gtk_widget_get_focus(gButtonWidget, &interior_focus, &focus_width, &focus_pad);
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
    case MOZ_GTK_TREEVIEW:
        ensure_tree_view_widget();
        w = gTreeViewWidget;
        break;
    case MOZ_GTK_TREE_HEADER_CELL:
        {
            /* A Tree Header in GTK is just a different styled button 
             * It must be placed in a TreeView for getting the correct style
             * assigned.
             * That is why the following code is the same as for MOZ_GTK_BUTTON.  
             * */

            /* Constant in gtkbutton.c */
            static const gint child_spacing = 1;
            gboolean interior_focus;
            gint focus_width, focus_pad;

            ensure_tree_header_cell_widget();
            *left = *top = *right = *bottom = GTK_CONTAINER(gTreeHeaderCellWidget)->border_width;

            moz_gtk_widget_get_focus(gTreeHeaderCellWidget, &interior_focus, &focus_width, &focus_pad);     
            *left += focus_width + focus_pad;
            *right += focus_width + focus_pad;
            *top += focus_width + focus_pad + child_spacing;
            *bottom += focus_width + focus_pad + child_spacing;
            
            *left += gTreeHeaderCellWidget->style->xthickness;
            *right += gTreeHeaderCellWidget->style->xthickness;
            *top += gTreeHeaderCellWidget->style->ythickness;
            *bottom += gTreeHeaderCellWidget->style->ythickness;
            return MOZ_GTK_SUCCESS;
        }
    case MOZ_GTK_TREE_HEADER_SORTARROW:
        ensure_tree_header_cell_widget();
        w = gTreeHeaderSortArrowWidget;
        break;
    case MOZ_GTK_DROPDOWN_ENTRY:
        ensure_dropdown_entry_widget();
        w = gDropdownEntryWidget;
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

            if (direction == GTK_TEXT_DIR_RTL)
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
    case MOZ_GTK_SPINBUTTON_ENTRY:
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
            if (widget == MOZ_GTK_CHECKBUTTON_LABEL) {
                ensure_checkbox_widget();
                moz_gtk_widget_get_focus(gCheckboxWidget, &interior_focus,
                                           &focus_width, &focus_pad);
            }
            else {
                ensure_radiobutton_widget();
                moz_gtk_widget_get_focus(gRadiobuttonWidget, &interior_focus,
                                        &focus_width, &focus_pad);
            }

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
                ensure_checkbox_widget();
                moz_gtk_widget_get_focus(gCheckboxWidget, &interior_focus,
                                           &focus_width, &focus_pad);
                w = gCheckboxWidget;
            } else {
                ensure_radiobutton_widget();
                moz_gtk_widget_get_focus(gRadiobuttonWidget, &interior_focus,
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
        ensure_menu_bar_item_widget();
        w = gMenuItemWidget;
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        ensure_check_menu_item_widget();
        w = gCheckMenuItemWidget;
        break;
    case MOZ_GTK_TAB:
        ensure_tab_widget();
        w = gTabWidget;
        break;
    /* These widgets have no borders, since they are not containers. */
    case MOZ_GTK_SPLITTER_HORIZONTAL:
    case MOZ_GTK_SPLITTER_VERTICAL:
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
    case MOZ_GTK_EXPANDER:
    case MOZ_GTK_TOOLBAR_SEPARATOR:
    case MOZ_GTK_MENUSEPARATOR:
    /* These widgets have no borders.*/
    case MOZ_GTK_SPINBUTTON:
    case MOZ_GTK_TOOLTIP:
    case MOZ_GTK_WINDOW:
    case MOZ_GTK_RESIZER:
    case MOZ_GTK_MENUARROW:
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
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
    const gint min_arrow_size = 15;
    ensure_arrow_widget();

    /*
     * First get the border of the dropdown arrow, then add in the requested
     * size of the arrow.  Note that the minimum arrow size is fixed at
     * 15 pixels.
     */

    *width = 2 * (1 + XTHICKNESS(gDropdownButtonWidget->style));
    *width += min_arrow_size + GTK_MISC(gArrowWidget)->xpad * 2;

    *height = 2 * (1 + YTHICKNESS(gDropdownButtonWidget->style));
    *height += min_arrow_size + GTK_MISC(gArrowWidget)->ypad * 2;

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_toolbar_separator_width(gint* size)
{
    gboolean wide_separators;
    gint separator_width;
    GtkStyle* style;

    ensure_toolbar_widget();

    style = gToolbarWidget->style;

    gtk_widget_style_get(gToolbarWidget,
                         "space-size", size,
                         "wide-separators",  &wide_separators,
                         "separator-width", &separator_width,
                         NULL);

    /* Just in case... */
    *size = MAX(*size, (wide_separators ? separator_width : style->xthickness));

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_expander_size(gint* size)
{
    ensure_expander_widget();
    gtk_widget_style_get(gExpanderWidget,
                         "expander-size", size,
                         NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_treeview_expander_size(gint* size)
{
    ensure_tree_view_widget();
    gtk_widget_style_get(gTreeViewWidget,
                         "expander-size", size,
                         NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_menu_separator_height(gint *size)
{
    gboolean wide_separators;
    gint     separator_height;

    ensure_menu_separator_widget();

    gtk_widget_style_get(gMenuSeparatorWidget,
                          "wide-separators",  &wide_separators,
                          "separator-height", &separator_height,
                          NULL);

    if (wide_separators)
        *size = separator_height + gMenuSeparatorWidget->style->ythickness;
    else
        *size = gMenuSeparatorWidget->style->ythickness * 2;

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
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction)
{
    switch (widget) {
    case MOZ_GTK_BUTTON:
        if (state->depressed) {
            ensure_toggle_button_widget();
            return moz_gtk_button_paint(drawable, rect, cliprect, state,
                                        (GtkReliefStyle) flags,
                                        gToggleButtonWidget, direction);
        }
        ensure_button_widget();
        return moz_gtk_button_paint(drawable, rect, cliprect, state,
                                    (GtkReliefStyle) flags, gButtonWidget,
                                    direction);
        break;
    case MOZ_GTK_CHECKBUTTON:
    case MOZ_GTK_RADIOBUTTON:
        return moz_gtk_toggle_paint(drawable, rect, cliprect, state,
                                    (gboolean) flags,
                                    (widget == MOZ_GTK_RADIOBUTTON),
                                    direction);
        break;
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
    case MOZ_GTK_SCALE_HORIZONTAL:
    case MOZ_GTK_SCALE_VERTICAL:
        return moz_gtk_scale_paint(drawable, rect, cliprect, state,
                                   (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
    case MOZ_GTK_SCALE_THUMB_VERTICAL:
        return moz_gtk_scale_thumb_paint(drawable, rect, cliprect, state,
                                         (GtkOrientation) flags, direction);
        break;
    case MOZ_GTK_SPINBUTTON:
        return moz_gtk_spin_paint(drawable, rect, direction);
        break;
    case MOZ_GTK_SPINBUTTON_UP:
    case MOZ_GTK_SPINBUTTON_DOWN:
        return moz_gtk_spin_updown_paint(drawable, rect,
                                         (widget == MOZ_GTK_SPINBUTTON_DOWN),
                                         state, direction);
        break;
    case MOZ_GTK_SPINBUTTON_ENTRY:
        ensure_spin_widget();
        return moz_gtk_entry_paint(drawable, rect, cliprect, state,
                                   gSpinWidget, direction);
        break;
    case MOZ_GTK_GRIPPER:
        return moz_gtk_gripper_paint(drawable, rect, cliprect, state,
                                     direction);
        break;
    case MOZ_GTK_TREEVIEW:
        return moz_gtk_treeview_paint(drawable, rect, cliprect, state,
                                      direction);
        break;
    case MOZ_GTK_TREE_HEADER_CELL:
        return moz_gtk_tree_header_cell_paint(drawable, rect, cliprect, state,
                                              direction);
        break;
    case MOZ_GTK_TREE_HEADER_SORTARROW:
        return moz_gtk_tree_header_sort_arrow_paint(drawable, rect, cliprect,
                                                    state,
                                                    (GtkArrowType) flags,
                                                    direction);
        break;
    case MOZ_GTK_TREEVIEW_EXPANDER:
        return moz_gtk_treeview_expander_paint(drawable, rect, cliprect, state,
                                               (GtkExpanderStyle) flags, direction);
        break;
    case MOZ_GTK_EXPANDER:
        return moz_gtk_expander_paint(drawable, rect, cliprect, state,
                                      (GtkExpanderStyle) flags, direction);
        break;
    case MOZ_GTK_ENTRY:
        ensure_entry_widget();
        return moz_gtk_entry_paint(drawable, rect, cliprect, state,
                                   gEntryWidget, direction);
        break;
    case MOZ_GTK_DROPDOWN:
        return moz_gtk_option_menu_paint(drawable, rect, cliprect, state,
                                         direction);
        break;
    case MOZ_GTK_DROPDOWN_ARROW:
        return moz_gtk_dropdown_arrow_paint(drawable, rect, cliprect, state,
                                            direction);
        break;
    case MOZ_GTK_DROPDOWN_ENTRY:
        ensure_dropdown_entry_widget();
        return moz_gtk_entry_paint(drawable, rect, cliprect, state,
                                   gDropdownEntryWidget, direction);
        break;
    case MOZ_GTK_CHECKBUTTON_CONTAINER:
    case MOZ_GTK_RADIOBUTTON_CONTAINER:
        return moz_gtk_container_paint(drawable, rect, cliprect, state,
                                       (widget == MOZ_GTK_RADIOBUTTON_CONTAINER),
                                       direction);
        break;
    case MOZ_GTK_CHECKBUTTON_LABEL:
    case MOZ_GTK_RADIOBUTTON_LABEL:
        return moz_gtk_toggle_label_paint(drawable, rect, cliprect, state,
                                          (widget == MOZ_GTK_RADIOBUTTON_LABEL),
                                          direction);
        break;
    case MOZ_GTK_TOOLBAR:
        return moz_gtk_toolbar_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_TOOLBAR_SEPARATOR:
        return moz_gtk_toolbar_separator_paint(drawable, rect, cliprect,
                                               direction);
        break;
    case MOZ_GTK_TOOLTIP:
        return moz_gtk_tooltip_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_FRAME:
        return moz_gtk_frame_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_RESIZER:
        return moz_gtk_resizer_paint(drawable, rect, cliprect, state,
                                     direction);
        break;
    case MOZ_GTK_PROGRESSBAR:
        return moz_gtk_progressbar_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_PROGRESS_CHUNK:
        return moz_gtk_progress_chunk_paint(drawable, rect, cliprect,
                                            direction);
        break;
    case MOZ_GTK_TAB:
        return moz_gtk_tab_paint(drawable, rect, cliprect,
                                 (GtkTabFlags) flags, direction);
        break;
    case MOZ_GTK_TABPANELS:
        return moz_gtk_tabpanels_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_MENUBAR:
        return moz_gtk_menu_bar_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_MENUPOPUP:
        return moz_gtk_menu_popup_paint(drawable, rect, cliprect, direction);
        break;
    case MOZ_GTK_MENUSEPARATOR:
        return moz_gtk_menu_separator_paint(drawable, rect, cliprect,
                                            direction);
        break;
    case MOZ_GTK_MENUITEM:
        return moz_gtk_menu_item_paint(drawable, rect, cliprect, state, flags,
                                       direction);
        break;
    case MOZ_GTK_MENUARROW:
        return moz_gtk_menu_arrow_paint(drawable, rect, cliprect, state,
                                        direction);
        break;
    case MOZ_GTK_TOOLBARBUTTON_ARROW:
        return moz_gtk_downarrow_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_CHECKMENUITEM:
    case MOZ_GTK_RADIOMENUITEM:
        return moz_gtk_check_menu_item_paint(drawable, rect, cliprect, state,
                                             (gboolean) flags,
                                             (widget == MOZ_GTK_RADIOMENUITEM),
                                             direction);
        break;
    case MOZ_GTK_SPLITTER_HORIZONTAL:
        return moz_gtk_vpaned_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_SPLITTER_VERTICAL:
        return moz_gtk_hpaned_paint(drawable, rect, cliprect, state);
        break;
    case MOZ_GTK_WINDOW:
        return moz_gtk_window_paint(drawable, rect, cliprect, direction);
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
    gToggleButtonWidget = NULL;
    gCheckboxWidget = NULL;
    gRadiobuttonWidget = NULL;
    gHorizScrollbarWidget = NULL;
    gVertScrollbarWidget = NULL;
    gSpinWidget = NULL;
    gHScaleWidget = NULL;
    gVScaleWidget = NULL;
    gEntryWidget = NULL;
    gArrowWidget = NULL;
    gOptionMenuWidget = NULL;
    gDropdownButtonWidget = NULL;
    gDropdownEntryWidget = NULL;
    gComboBoxEntryWidget = NULL;
    gHandleBoxWidget = NULL;
    gToolbarWidget = NULL;
    gStatusbarWidget = NULL;
    gFrameWidget = NULL;
    gProgressWidget = NULL;
    gTabWidget = NULL;
    gTooltipWidget = NULL;
    gMenuBarWidget = NULL;
    gMenuBarItemWidget = NULL;
    gMenuPopupWidget = NULL;
    gMenuItemWidget = NULL;
    gCheckMenuItemWidget = NULL;
    gTreeViewWidget = NULL;
    gTreeHeaderCellWidget = NULL;
    gTreeHeaderSortArrowWidget = NULL;
    gExpanderWidget = NULL;
    gToolbarSeparatorWidget = NULL;
    gMenuSeparatorWidget = NULL;
    gHPanedWidget = NULL;
    gVPanedWidget = NULL;

    is_initialized = FALSE;

    return MOZ_GTK_SUCCESS;
}
