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

#include "config.h"
#include "WebKitScriptDialog.h"

#include "WebKitScriptDialogPrivate.h"

G_DEFINE_BOXED_TYPE(WebKitScriptDialog, webkit_script_dialog, webkit_script_dialog_ref, webkit_script_dialog_unref)

WebKitScriptDialog* webkitScriptDialogCreate(unsigned type, const CString& message, const CString& defaultText, Function<void(bool, const String&)>&& completionHandler)
{
    auto* dialog = static_cast<WebKitScriptDialog*>(fastMalloc(sizeof(WebKitScriptDialog)));
    new (dialog) WebKitScriptDialog(type, message, defaultText, WTFMove(completionHandler));
    return dialog;
}

bool webkitScriptDialogIsRunning(WebKitScriptDialog* scriptDialog)
{
    return !!scriptDialog->completionHandler;
}

/**
 * webkit_script_dialog_ref:
 * @dialog: a #WebKitScriptDialog
 *
 * Atomically increments the reference count of @dialog by one. This
 * function is MT-safe and may be called from any thread.
 *
 * Returns: The passed in #WebKitScriptDialog
 *
 * Since: 2.24
 */
WebKitScriptDialog* webkit_script_dialog_ref(WebKitScriptDialog* dialog)
{
    g_atomic_int_inc(&dialog->referenceCount);
    return dialog;
}

/**
 * webkit_script_dialog_unref:
 * @dialog: a #WebKitScriptDialog
 *
 * Atomically decrements the reference count of @dialog by one. If the
 * reference count drops to 0, all memory allocated by the #WebKitScriptdialog is
 * released. This function is MT-safe and may be called from any
 * thread.
 *
 * Since: 2.24
 */
void webkit_script_dialog_unref(WebKitScriptDialog* dialog)
{
    if (g_atomic_int_dec_and_test(&dialog->referenceCount)) {
        webkit_script_dialog_close(dialog);
        dialog->~WebKitScriptDialog();
        fastFree(dialog);
    }
}

/**
 * webkit_script_dialog_get_dialog_type:
 * @dialog: a #WebKitScriptDialog
 *
 * Get the dialog type of a #WebKitScriptDialog.
 *
 * Returns: the #WebKitScriptDialogType of @dialog
 */
WebKitScriptDialogType webkit_script_dialog_get_dialog_type(WebKitScriptDialog* dialog)
{
    g_return_val_if_fail(dialog, WEBKIT_SCRIPT_DIALOG_ALERT);

    return static_cast<WebKitScriptDialogType>(dialog->type);
}

/**
 * webkit_script_dialog_get_message:
 * @dialog: a #WebKitScriptDialog
 *
 * Get the message of a #WebKitScriptDialog.
 *
 * Returns: the message of @dialog.
 */
const char* webkit_script_dialog_get_message(WebKitScriptDialog* dialog)
{
    g_return_val_if_fail(dialog, 0);

    return dialog->message.data();
}

/**
 * webkit_script_dialog_confirm_set_confirmed:
 * @dialog: a #WebKitScriptDialog
 * @confirmed: whether user confirmed the dialog
 *
 * This method is used for %WEBKIT_SCRIPT_DIALOG_CONFIRM and %WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM dialogs when
 * #WebKitWebView::script-dialog signal is emitted to set whether the user
 * confirmed the dialog or not. The default implementation of #WebKitWebView::script-dialog
 * signal sets %TRUE when the OK or Stay buttons are clicked and %FALSE otherwise.
 * It's an error to use this method with a #WebKitScriptDialog that is not of type
 * %WEBKIT_SCRIPT_DIALOG_CONFIRM or %WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM
 */
void webkit_script_dialog_confirm_set_confirmed(WebKitScriptDialog* dialog, gboolean confirmed)
{
    g_return_if_fail(dialog);
    g_return_if_fail(dialog->type == WEBKIT_SCRIPT_DIALOG_CONFIRM || dialog->type == WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM);

    dialog->confirmed = confirmed;
}

/**
 * webkit_script_dialog_prompt_get_default_text:
 * @dialog: a #WebKitScriptDialog
 *
 * Get the default text of a #WebKitScriptDialog of type %WEBKIT_SCRIPT_DIALOG_PROMPT.
 * It's an error to use this method with a #WebKitScriptDialog that is not of type
 * %WEBKIT_SCRIPT_DIALOG_PROMPT.
 *
 * Returns: the default text of @dialog
 */
const char* webkit_script_dialog_prompt_get_default_text(WebKitScriptDialog* dialog)
{
    g_return_val_if_fail(dialog, 0);
    g_return_val_if_fail(dialog->type == WEBKIT_SCRIPT_DIALOG_PROMPT, 0);

    return dialog->defaultText.data();
}

/**
 * webkit_script_dialog_prompt_set_text:
 * @dialog: a #WebKitScriptDialog
 * @text: the text to set
 *
 * This method is used for %WEBKIT_SCRIPT_DIALOG_PROMPT dialogs when
 * #WebKitWebView::script-dialog signal is emitted to set the text
 * entered by the user. The default implementation of #WebKitWebView::script-dialog
 * signal sets the text of the entry form when OK button is clicked, otherwise %NULL is set.
 * It's an error to use this method with a #WebKitScriptDialog that is not of type
 * %WEBKIT_SCRIPT_DIALOG_PROMPT.
 */
void webkit_script_dialog_prompt_set_text(WebKitScriptDialog* dialog, const char* text)
{
    g_return_if_fail(dialog);
    g_return_if_fail(dialog->type == WEBKIT_SCRIPT_DIALOG_PROMPT);

    dialog->text = text;
}

/**
 * webkit_script_dialog_close:
 * @dialog: a #WebKitScriptDialog
 *
 * Close @dialog. When handling a #WebKitScriptDialog asynchronously (webkit_script_dialog_ref()
 * was called in #WebKitWebView::script-dialog callback), this function needs to be called to notify
 * that we are done with the script dialog. The dialog will be closed on destruction if this function
 * hasn't been called before.
 *
 * Since: 2.24
 */
void webkit_script_dialog_close(WebKitScriptDialog* dialog)
{
    g_return_if_fail(dialog);

    if (!dialog->completionHandler)
        return;

    auto completionHandler = std::exchange(dialog->completionHandler, nullptr);

    switch (dialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        completionHandler(false, emptyString());
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        completionHandler(dialog->confirmed, emptyString());
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        completionHandler(false, String::fromUTF8(dialog->text.data()));
        break;
    }
}
