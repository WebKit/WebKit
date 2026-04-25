/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "config.h"
#include "WebKitScriptDialog.h"

#include "WebKitScriptDialogPrivate.h"

// Callbacks invoked by WebDriver commands
// As WPE has currently no public API to allow the browser to respond to these commands,
// we mimic the expected behavior in these callbacks like one would do in a reference browser.
void webkitScriptDialogAccept(WebKitScriptDialog* dialog)
{
    auto dialog_type = webkit_script_dialog_get_dialog_type(dialog);
    if (dialog_type == WEBKIT_SCRIPT_DIALOG_CONFIRM || dialog_type == WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM)
        webkit_script_dialog_confirm_set_confirmed(dialog, TRUE);
    // W3C WebDriver tests expect an empty string instead of a null one when the prompt is accepted.
    if (dialog_type == WEBKIT_SCRIPT_DIALOG_PROMPT && dialog->text.isNull())
        webkit_script_dialog_prompt_set_text(dialog, dialog->defaultText.isNull() ? "" : dialog->defaultText.data());
    webkit_script_dialog_unref(dialog);
}

void webkitScriptDialogDismiss(WebKitScriptDialog* dialog)
{
    auto dialog_type = webkit_script_dialog_get_dialog_type(dialog);
    if (dialog_type == WEBKIT_SCRIPT_DIALOG_CONFIRM || dialog_type == WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM)
        webkit_script_dialog_confirm_set_confirmed(dialog, FALSE);
    webkit_script_dialog_unref(dialog);
}

void webkitScriptDialogSetUserInput(WebKitScriptDialog* dialog, const String& input)
{
    if (webkit_script_dialog_get_dialog_type(dialog) != WEBKIT_SCRIPT_DIALOG_PROMPT)
        return;

    webkit_script_dialog_prompt_set_text(dialog, input.utf8().data());
}

bool webkitScriptDialogIsUserHandled(WebKitScriptDialog* dialog)
{
    return dialog->isUserHandled;
}
