/*
 * Copyright (C) 2017 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitPrintCustomWidget_h
#define WebKitPrintCustomWidget_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_PRINT_CUSTOM_WIDGET            (webkit_print_custom_widget_get_type())
#define WEBKIT_PRINT_CUSTOM_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_PRINT_CUSTOM_WIDGET, WebKitPrintCustomWidget))
#define WEBKIT_IS_PRINT_CUSTOM_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_PRINT_CUSTOM_WIDGET))
#define WEBKIT_PRINT_CUSTOM_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_PRINT_CUSTOM_WIDGET, WebKitPrintCustomWidgetClass))
#define WEBKIT_IS_PRINT_CUSTOM_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_PRINT_CUSTOM_WIDGET))
#define WEBKIT_PRINT_CUSTOM_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_PRINT_CUSTOM_WIDGET, WebKitPrintCustomWidgetClass))

typedef struct _WebKitPrintCustomWidget        WebKitPrintCustomWidget;
typedef struct _WebKitPrintCustomWidgetClass   WebKitPrintCustomWidgetClass;
typedef struct _WebKitPrintCustomWidgetPrivate WebKitPrintCustomWidgetPrivate;

struct _WebKitPrintCustomWidget {
    GObject parent;

    WebKitPrintCustomWidgetPrivate *priv;
};

struct _WebKitPrintCustomWidgetClass {
    GObjectClass parent_class;

    void    (* apply)               (WebKitPrintCustomWidget *print_custom_widget,
                                     GtkWidget               *widget);
    void    (* update)              (WebKitPrintCustomWidget *print_custom_widget,
                                     GtkWidget               *widget,
                                     GtkPageSetup            *page_setup,
                                     GtkPrintSettings        *print_settings);

    void    (*_webkit_reserved0) (void);
    void    (*_webkit_reserved1) (void);
    void    (*_webkit_reserved2) (void);
    void    (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_print_custom_widget_get_type   (void);

WEBKIT_API WebKitPrintCustomWidget *
webkit_print_custom_widget_new        (GtkWidget               *widget,
                                       const char              *title);

WEBKIT_API GtkWidget *
webkit_print_custom_widget_get_widget (WebKitPrintCustomWidget *print_custom_widget);

WEBKIT_API const gchar *
webkit_print_custom_widget_get_title  (WebKitPrintCustomWidget *print_custom_widget);

G_END_DECLS

#endif
