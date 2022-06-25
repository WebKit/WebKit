/*
 * Copyright (C) 2012 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitScriptDialog_h
#define WebKitScriptDialog_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SCRIPT_DIALOG (webkit_script_dialog_get_type())

typedef struct _WebKitScriptDialog WebKitScriptDialog;

/**
 * WebKitScriptDialogType:
 * @WEBKIT_SCRIPT_DIALOG_ALERT: Alert script dialog, used to show a
 * message to the user.
 * @WEBKIT_SCRIPT_DIALOG_CONFIRM: Confirm script dialog, used to ask
 * confirmation to the user.
 * @WEBKIT_SCRIPT_DIALOG_PROMPT: Prompt script dialog, used to ask
 * information to the user.
 * @WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM: Before unload confirm dialog,
 * used to ask confirmation to leave the current page to the user. Since 2.12
 *
 * Enum values used for determining the type of #WebKitScriptDialog
 */
typedef enum {
    WEBKIT_SCRIPT_DIALOG_ALERT,
    WEBKIT_SCRIPT_DIALOG_CONFIRM,
    WEBKIT_SCRIPT_DIALOG_PROMPT,
    WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM
} WebKitScriptDialogType;

WEBKIT_API GType
webkit_script_dialog_get_type                (void);

WEBKIT_API WebKitScriptDialog *
webkit_script_dialog_ref                     (WebKitScriptDialog *dialog);

WEBKIT_API void
webkit_script_dialog_unref                   (WebKitScriptDialog *dialog);

WEBKIT_API WebKitScriptDialogType
webkit_script_dialog_get_dialog_type         (WebKitScriptDialog *dialog);

WEBKIT_API const gchar *
webkit_script_dialog_get_message             (WebKitScriptDialog *dialog);

WEBKIT_API void
webkit_script_dialog_confirm_set_confirmed   (WebKitScriptDialog *dialog,
                                              gboolean            confirmed);

WEBKIT_API const gchar *
webkit_script_dialog_prompt_get_default_text (WebKitScriptDialog *dialog);

WEBKIT_API void
webkit_script_dialog_prompt_set_text         (WebKitScriptDialog *dialog,
                                              const gchar        *text);

WEBKIT_API void
webkit_script_dialog_close                   (WebKitScriptDialog *dialog);

G_END_DECLS

#endif
