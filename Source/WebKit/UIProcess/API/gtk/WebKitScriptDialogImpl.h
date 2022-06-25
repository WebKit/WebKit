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

#include "WebKitScriptDialog.h"
#include "WebKitWebViewDialog.h"
#include <wtf/text/WTFString.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SCRIPT_DIALOG_IMPL            (webkit_script_dialog_impl_get_type())
#define WEBKIT_SCRIPT_DIALOG_IMPL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_SCRIPT_DIALOG_IMPL, WebKitScriptDialogImpl))
#define WEBKIT_IS_SCRIPT_DIALOG_IMPL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_SCRIPT_DIALOG_IMPL))
#define WEBKIT_SCRIPT_DIALOG_IMPL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_SCRIPT_DIALOG_IMPL, WebKitScriptDialogImplClass))
#define WEBKIT_IS_SCRIPT_DIALOG_IMPL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_SCRIPT_DIALOG_IMPL))
#define WEBKIT_SCRIPT_DIALOG_IMPL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_SCRIPT_DIALOG_IMPL, WebKitScriptDialogImplClass))

typedef struct _WebKitScriptDialogImpl        WebKitScriptDialogImpl;
typedef struct _WebKitScriptDialogImplClass   WebKitScriptDialogImplClass;
typedef struct _WebKitScriptDialogImplPrivate WebKitScriptDialogImplPrivate;

struct _WebKitScriptDialogImpl {
    WebKitWebViewDialog parent;

    WebKitScriptDialogImplPrivate* priv;
};

struct _WebKitScriptDialogImplClass {
    WebKitWebViewDialogClass parentClass;
};

GType webkit_script_dialog_impl_get_type();
GtkWidget* webkitScriptDialogImplNew(WebKitScriptDialog*, const char* title, GtkRequisition* maxSize);
void webkitScriptDialogImplCancel(WebKitScriptDialogImpl*);
void webkitScriptDialogImplConfirm(WebKitScriptDialogImpl*);
void webkitScriptDialogImplSetEntryText(WebKitScriptDialogImpl*, const String&);

G_END_DECLS
