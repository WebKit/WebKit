/*
 * Copyright (C) 2018 Igalia S.L.
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_VIEW_DIALOG            (webkit_web_view_dialog_get_type())
#define WEBKIT_WEB_VIEW_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_VIEW_DIALOG, WebKitWebViewDialog))
#define WEBKIT_IS_WEB_VIEW_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_VIEW_DIALOG))
#define WEBKIT_WEB_VIEW_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_VIEW_DIALOG, WebKitWebViewDialogClass))
#define WEBKIT_IS_WEB_VIEW_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_VIEW_DIALOG))
#define WEBKIT_WEB_VIEW_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_VIEW_DIALOG, WebKitWebViewDialogClass))

typedef struct _WebKitWebViewDialog        WebKitWebViewDialog;
typedef struct _WebKitWebViewDialogClass   WebKitWebViewDialogClass;
typedef struct _WebKitWebViewDialogPrivate WebKitWebViewDialogPrivate;

struct _WebKitWebViewDialog {
#if USE(GTK4)
    GtkWidget parent;
#else
    GtkEventBox parent;
#endif

    WebKitWebViewDialogPrivate* priv;
};

struct _WebKitWebViewDialogClass {
#if USE(GTK4)
    GtkWidgetClass parentClass;
#else
    GtkEventBoxClass parentClass;
#endif
};

GType webkit_web_view_dialog_get_type();
#if USE(GTK4)
void webkitWebViewDialogSetChild(WebKitWebViewDialog*, GtkWidget*);
GtkWidget* webkitWebViewDialogGetChild(WebKitWebViewDialog*);
#endif

G_END_DECLS
