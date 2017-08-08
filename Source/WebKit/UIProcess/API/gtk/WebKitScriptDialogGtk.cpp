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
#include "WebKitWebViewPrivate.h"
#include <WebCore/GtkUtilities.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

static GtkWidget* webkitWebViewCreateJavaScriptDialog(WebKitWebView* webView, GtkMessageType type, GtkButtonsType buttons, int defaultResponse, const char* primaryText, const char* secondaryText = nullptr)
{
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(webView));
    GtkWidget* dialog = gtk_message_dialog_new(WebCore::widgetIsOnscreenToplevelWindow(parent) ? GTK_WINDOW(parent) : nullptr,
        GTK_DIALOG_DESTROY_WITH_PARENT, type, buttons, "%s", primaryText);
    if (secondaryText)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondaryText);
    GUniquePtr<char> title(g_strdup_printf("JavaScript - %s", webkitWebViewGetPage(webView).pageLoadState().url().utf8().data()));
    gtk_window_set_title(GTK_WINDOW(dialog), title.get());
    if (buttons != GTK_BUTTONS_NONE)
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), defaultResponse);

    return dialog;
}

void webkitScriptDialogRun(WebKitScriptDialog* scriptDialog, WebKitWebView* webView)
{
    GtkWidget* dialog = nullptr;

    switch (scriptDialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, GTK_RESPONSE_CLOSE, scriptDialog->message.data());
        scriptDialog->nativeDialog = dialog;
        gtk_dialog_run(GTK_DIALOG(dialog));
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, scriptDialog->message.data());
        scriptDialog->nativeDialog = dialog;
        scriptDialog->confirmed = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT: {
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, GTK_RESPONSE_OK, scriptDialog->message.data());
        scriptDialog->nativeDialog = dialog;
        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), scriptDialog->defaultText.data());
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_widget_show(entry);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
            scriptDialog->text = gtk_entry_get_text(GTK_ENTRY(entry));
        break;
    }
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        dialog = webkitWebViewCreateJavaScriptDialog(webView, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, GTK_RESPONSE_OK,
            _("Are you sure you want to leave this page?"), scriptDialog->message.data());
        scriptDialog->nativeDialog = dialog;
        gtk_dialog_add_buttons(GTK_DIALOG(dialog), _("Stay on Page"), GTK_RESPONSE_CLOSE, _("Leave Page"), GTK_RESPONSE_OK, nullptr);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
        scriptDialog->confirmed = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
        break;
    }

    gtk_widget_destroy(dialog);
    scriptDialog->nativeDialog = nullptr;
}

bool webkitScriptDialogIsRunning(WebKitScriptDialog* scriptDialog)
{
    return !!scriptDialog->nativeDialog;
}

void webkitScriptDialogAccept(WebKitScriptDialog* scriptDialog)
{
    int response = 0;
    switch (scriptDialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        response = GTK_RESPONSE_CLOSE;
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        response = GTK_RESPONSE_OK;
        break;
    }
    ASSERT(scriptDialog->nativeDialog);
    gtk_dialog_response(GTK_DIALOG(scriptDialog->nativeDialog), response);
}

void webkitScriptDialogDismiss(WebKitScriptDialog* scriptDialog)
{
    ASSERT(scriptDialog->nativeDialog);
    gtk_dialog_response(GTK_DIALOG(scriptDialog->nativeDialog), GTK_RESPONSE_CLOSE);
}

void webkitScriptDialogSetUserInput(WebKitScriptDialog* scriptDialog, const String& userInput)
{
    if (scriptDialog->type != WEBKIT_SCRIPT_DIALOG_PROMPT)
        return;

    ASSERT(scriptDialog->nativeDialog);
    GtkWidget* dialogContentArea = gtk_dialog_get_content_area(GTK_DIALOG(scriptDialog->nativeDialog));
    GUniquePtr<GList> children(gtk_container_get_children(GTK_CONTAINER(dialogContentArea)));
    for (GList* child = children.get(); child; child = g_list_next(child)) {
        GtkWidget* childWidget = GTK_WIDGET(child->data);
        if (GTK_IS_ENTRY(childWidget)) {
            gtk_entry_set_text(GTK_ENTRY(childWidget), userInput.utf8().data());
            break;
        }
    }
}
