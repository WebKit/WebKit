/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/**
 * gtkdrawing.h: GTK widget rendering utilities
 *
 * gtkdrawing provides an API for rendering GTK widgets in the
 * current theme to a pixmap or window, without requiring an actual
 * widget instantiation, similar to the Macintosh Appearance Manager
 * or Windows XP's DrawThemeBackground() API.
 */

#ifndef _GTK_DRAWING_H_
#define _GTK_DRAWING_H_

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*** type definitions ***/
typedef struct {
  guint8 active;
  guint8 focused;
  guint8 inHover;
  guint8 disabled;
  guint8 isDefault;
  guint8 canDefault;
  /* The depressed state is for buttons which remain active for a longer period:
   * activated toggle buttons or buttons showing a popup menu. */
  guint8 depressed;
  gint32 curpos; /* curpos and maxpos are used for scrollbars */
  gint32 maxpos;
} GtkWidgetState;

typedef struct {
  gint slider_width;
  gint trough_border;
  gint stepper_size;
  gint stepper_spacing;
  gint min_slider_size;
  gboolean trough_under_steppers;
  gboolean has_secondary_forward_stepper;
  gboolean has_secondary_backward_stepper;
} MozGtkScrollbarMetrics;

typedef struct _GtkThemeParts {
    GdkColormap* colormap;
    GtkWidget* protoWindow;
    GtkWidget* protoLayout;
    GtkWidget* horizScrollbarWidget;
    GtkWidget* vertScrollbarWidget;
    GtkWidget* scrolledWindowWidget;
} GtkThemeParts;

typedef enum {
  MOZ_GTK_STEPPER_DOWN        = 1 << 0,
  MOZ_GTK_STEPPER_BOTTOM      = 1 << 1,
  MOZ_GTK_STEPPER_VERTICAL    = 1 << 2
} GtkScrollbarButtonFlags;

/* function type for moz_gtk_enable_style_props */
typedef gint (*style_prop_t)(GtkStyle*, const gchar*, gint);

/*** result/error codes ***/
#define MOZ_GTK_SUCCESS 0
#define MOZ_GTK_UNKNOWN_WIDGET -1
#define MOZ_GTK_UNSAFE_THEME -2

/*** widget type constants ***/
typedef enum {
  /* Paints a GtkButton. flags is a GtkReliefStyle. */
  MOZ_GTK_BUTTON,
  /**
   * Paints the button of a GtkScrollbar. flags is a GtkArrowType giving
   * the arrow direction.
   */
  MOZ_GTK_SCROLLBAR_BUTTON,
  /* Paints the trough (track) of a GtkScrollbar. */
  MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL,
  MOZ_GTK_SCROLLBAR_TRACK_VERTICAL,
  /* Paints the slider (thumb) of a GtkScrollbar. */
  MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL,
  MOZ_GTK_SCROLLBAR_THUMB_VERTICAL,
  /* Paints the background of a scrolled window */
  MOZ_GTK_SCROLLED_WINDOW,
} GtkThemeWidgetType;

/*** General library functions ***/
/**
 * Initializes the drawing library.  You must call this function
 * prior to using any other functionality.
 * returns: MOZ_GTK_SUCCESS if there were no errors
 *          MOZ_GTK_UNSAFE_THEME if the current theme engine is known
 *                               to crash with gtkdrawing.
 */
gint moz_gtk_init();

/**
 * Instruct the drawing library to do all rendering based on
 * the given collection of theme parts. If any members of the
 * GtkThemeParts struct are NULL, they will be created lazily.
 */
void
moz_gtk_use_theme_parts(GtkThemeParts* parts);

/**
 * Enable GTK+ 1.2.9+ theme enhancements. You must provide a pointer
 * to the GTK+ 1.2.9+ function "gtk_style_get_prop_experimental".
 * styleGetProp:  pointer to gtk_style_get_prop_experimental
 * 
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_enable_style_props(style_prop_t styleGetProp);

/**
 * Perform cleanup of the drawing library. You should call this function
 * when your program exits, or you no longer need the library.
 *
 * returns: MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_shutdown();

/**
 * Destroy the widgets in the given GtkThemeParts, which should
 * be destroyed before the GtkThemeParts can be freed.
 */
void moz_gtk_destroy_theme_parts_widgets(GtkThemeParts* parts);

/*** Widget drawing ***/
/**
 * Paint a widget in the current theme.
 * widget:    a constant giving the widget to paint
 * rect:      the bounding rectangle for the widget
 * cliprect:  a clipprect rectangle for this painting operation
 * state:     the state of the widget.  ignored for some widgets.
 * flags:     widget-dependant flags; see the GtkThemeWidgetType definition.
 * direction: the text direction, to draw the widget correctly LTR and RTL.
 */
gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, GdkDrawable* drawable,
                     GdkRectangle* rect, GdkRectangle* cliprect,
                     GtkWidgetState* state, gint flags,
                     GtkTextDirection direction);

/*** Widget metrics ***/
/**
 * Get the border size of a widget
 * left/right:  [OUT] the widget's left/right border
 * top/bottom:  [OUT] the widget's top/bottom border
 * direction:   the text direction for the widget
 * inhtml:      boolean indicating whether this widget will be drawn as a HTML form control,
 *              in order to workaround a size issue (MOZ_GTK_BUTTON only, ignored otherwise)
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint moz_gtk_get_widget_border(GtkThemeWidgetType widget, gint* left, gint* top, 
                               gint* right, gint* bottom, GtkTextDirection direction,
                               gboolean inhtml);
/**
 * Get the desired metrics for a GtkScrollbar
 * metrics:          [IN]  struct which will contain the metrics
 *
 * returns:    MOZ_GTK_SUCCESS if there was no error, an error code otherwise
 */
gint
moz_gtk_get_scrollbar_metrics(MozGtkScrollbarMetrics* metrics);

/**
 * Retrieve an actual GTK scrollbar widget for style analysis. It will not
 * be modified.
 */
GtkWidget* moz_gtk_get_scrollbar_widget(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
