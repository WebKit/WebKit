/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "WebKitInputMethodContext.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK            (webkit_input_method_context_impl_gtk_get_type())
#define WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK, WebKitInputMethodContextImplGtk))
#define WEBKIT_IS_INPUT_METHOD_CONTEXT_IMPL_GTK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK))
#define WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK, WebKitInputMethodContextImplGtkClass))
#define WEBKIT_IS_INPUT_METHOD_CONTEXT_IMPL_GTK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK))
#define WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_INPUT_METHOD_CONTEXT_IMPL_GTK, WebKitInputMethodContextImplGtkClass))

typedef struct _WebKitInputMethodContextImplGtk        WebKitInputMethodContextImplGtk;
typedef struct _WebKitInputMethodContextImplGtkClass   WebKitInputMethodContextImplGtkClass;
typedef struct _WebKitInputMethodContextImplGtkPrivate WebKitInputMethodContextImplGtkPrivate;

struct _WebKitInputMethodContextImplGtk {
    WebKitInputMethodContext parent;

    WebKitInputMethodContextImplGtkPrivate* priv;
};

struct _WebKitInputMethodContextImplGtkClass {
    WebKitInputMethodContextClass parentClass;
};

GType webkit_input_method_context_impl_gtk_get_type();
WebKitInputMethodContext* webkitInputMethodContextImplGtkNew();
void webkitInputMethodContextImplGtkSetClientWindow(WebKitInputMethodContextImplGtk*, GdkWindow*);

G_END_DECLS

