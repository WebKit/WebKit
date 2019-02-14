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

#include "config.h"
#include "WebKitScriptDialogImpl.h"

#include "WebKitScriptDialogPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

struct _WebKitScriptDialogImplPrivate {
    WebKitScriptDialog* dialog;
    GtkWidget* vbox;
    GtkWidget* swindow;
    GtkWidget* title;
    GtkWidget* label;
    GtkWidget* entry;
    GtkWidget* actionArea;
    GtkWidget* defaultButton;
};

WEBKIT_DEFINE_TYPE(WebKitScriptDialogImpl, webkit_script_dialog_impl, WEBKIT_TYPE_WEB_VIEW_DIALOG)

static void webkitScriptDialogImplClose(WebKitScriptDialogImpl* dialog)
{
    webkit_script_dialog_close(dialog->priv->dialog);
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static gboolean webkitScriptDialogImplKeyPressEvent(GtkWidget* widget, GdkEventKey* keyEvent)
{
    if (keyEvent->keyval == GDK_KEY_Escape) {
        webkitScriptDialogImplClose(WEBKIT_SCRIPT_DIALOG_IMPL(widget));
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void webkitScriptDialogImplMap(GtkWidget* widget)
{
    WebKitScriptDialogImplPrivate* priv = WEBKIT_SCRIPT_DIALOG_IMPL(widget)->priv;
    gtk_widget_grab_default(priv->defaultButton);

    switch (priv->dialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        gtk_widget_grab_focus(priv->defaultButton);
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        gtk_widget_grab_focus(priv->entry);
        break;
    }

    GTK_WIDGET_CLASS(webkit_script_dialog_impl_parent_class)->map(widget);
}

static void webkitScriptDialogImplConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_script_dialog_impl_parent_class)->constructed(object);

    auto* dialog = WEBKIT_SCRIPT_DIALOG_IMPL(object);
    WebKitScriptDialogImplPrivate* priv = dialog->priv;

    priv->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(priv->vbox), 0);
    gtk_container_add(GTK_CONTAINER(dialog), priv->vbox);
    gtk_widget_show(priv->vbox);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), GTK_STYLE_CLASS_TITLEBAR);
    gtk_widget_set_size_request(box, -1, 16);
    priv->title = gtk_label_new(nullptr);
    gtk_label_set_ellipsize(GTK_LABEL(priv->title), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_top(priv->title, 6);
    gtk_widget_set_margin_bottom(priv->title, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(priv->title), GTK_STYLE_CLASS_TITLE);
    gtk_box_set_center_widget(GTK_BOX(box), priv->title);
    gtk_widget_show(priv->title);
    gtk_box_pack_start(GTK_BOX(priv->vbox), box, TRUE, FALSE, 0);
    gtk_widget_show(box);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 30);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_box_pack_start(GTK_BOX(priv->vbox), box, TRUE, FALSE, 0);
    gtk_widget_show(box);

    GtkWidget* messageArea = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(box), messageArea, TRUE, TRUE, 0);
    gtk_widget_show(messageArea);

    priv->swindow = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(messageArea), priv->swindow, TRUE, TRUE, 0);
    gtk_widget_show(priv->swindow);

    priv->label = gtk_label_new(nullptr);
    gtk_widget_set_halign(priv->label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(priv->label, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(priv->label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(priv->label), 60);
    gtk_container_add(GTK_CONTAINER(priv->swindow), priv->label);
    gtk_widget_show(priv->label);

    GtkWidget* actionBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(actionBox), "dialog-action-box");
    gtk_box_pack_end(GTK_BOX(priv->vbox), actionBox, FALSE, TRUE, 0);
    gtk_widget_show(actionBox);

    priv->actionArea = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(priv->actionArea), GTK_BUTTONBOX_EXPAND);
    gtk_widget_set_hexpand(priv->actionArea, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(priv->actionArea), "dialog-action-area");
    gtk_box_pack_end(GTK_BOX(actionBox), priv->actionArea, FALSE, TRUE, 0);
    gtk_widget_show(priv->actionArea);
}

static void webkitScriptDialogImplDispose(GObject* object)
{
    auto* dialog = WEBKIT_SCRIPT_DIALOG_IMPL(object);
    if (dialog->priv->dialog) {
        dialog->priv->dialog->nativeDialog = nullptr;
        webkit_script_dialog_unref(dialog->priv->dialog);
        dialog->priv->dialog = nullptr;
    }

    G_OBJECT_CLASS(webkit_script_dialog_impl_parent_class)->dispose(object);
}

static void webkit_script_dialog_impl_class_init(WebKitScriptDialogImplClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitScriptDialogImplConstructed;
    objectClass->dispose = webkitScriptDialogImplDispose;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->key_press_event = webkitScriptDialogImplKeyPressEvent;
    widgetClass->map = webkitScriptDialogImplMap;
    gtk_widget_class_set_accessible_role(widgetClass, ATK_ROLE_ALERT);
}

static void webkitScriptDialogImplSetText(WebKitScriptDialogImpl* dialog, const char* text, GtkRequisition* maxSize)
{
    WebKitScriptDialogImplPrivate* priv = dialog->priv;
    gtk_label_set_text(GTK_LABEL(priv->label), text);
    GtkRequisition naturalRequisition;
    gtk_widget_get_preferred_size(priv->label, nullptr, &naturalRequisition);
    gtk_widget_set_size_request(priv->swindow, std::min(naturalRequisition.width, maxSize->width), std::min(maxSize->height, naturalRequisition.height));
}

static GtkWidget* webkitScriptDialogImplAddButton(WebKitScriptDialogImpl* dialog, const char* text)
{
    WebKitScriptDialogImplPrivate* priv = dialog->priv;
    GtkWidget* button = gtk_button_new_with_label(text);
    gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(button), "text-button");
    gtk_widget_set_can_default(button, TRUE);

    gtk_widget_set_valign(button, GTK_ALIGN_BASELINE);
    gtk_container_add(GTK_CONTAINER(priv->actionArea), button);
    gtk_widget_show(button);

    return button;
}

GtkWidget* webkitScriptDialogImplNew(WebKitScriptDialog* scriptDialog, const char* title, GtkRequisition* maxSize)
{
    auto* dialog = WEBKIT_SCRIPT_DIALOG_IMPL(g_object_new(WEBKIT_TYPE_SCRIPT_DIALOG_IMPL, nullptr));
    dialog->priv->dialog = webkit_script_dialog_ref(scriptDialog);
    dialog->priv->dialog->nativeDialog = GTK_WIDGET(dialog);

    switch (scriptDialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT: {
        gtk_label_set_text(GTK_LABEL(dialog->priv->title), title);

        GtkWidget* button = webkitScriptDialogImplAddButton(dialog, _("_Close"));
        dialog->priv->defaultButton = button;
        g_signal_connect_swapped(button, "clicked", G_CALLBACK(webkitScriptDialogImplCancel), dialog);
        webkitScriptDialogImplSetText(dialog, scriptDialog->message.data(), maxSize);
        break;
    }
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        dialog->priv->entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(dialog->priv->entry), scriptDialog->defaultText.data());
        gtk_container_add(GTK_CONTAINER(dialog->priv->vbox), dialog->priv->entry);
        gtk_entry_set_activates_default(GTK_ENTRY(dialog->priv->entry), TRUE);
        gtk_widget_show(dialog->priv->entry);

        FALLTHROUGH;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM: {
        gtk_label_set_text(GTK_LABEL(dialog->priv->title), title);

        GtkWidget* button = webkitScriptDialogImplAddButton(dialog, _("_Cancel"));
        g_signal_connect_swapped(button, "clicked", G_CALLBACK(webkitScriptDialogImplCancel), dialog);
        button = webkitScriptDialogImplAddButton(dialog, _("_OK"));
        dialog->priv->defaultButton = button;
        g_signal_connect_swapped(button, "clicked", G_CALLBACK(webkitScriptDialogImplConfirm), dialog);
        webkitScriptDialogImplSetText(dialog, scriptDialog->message.data(), maxSize);
        break;
    }
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM: {
        gtk_label_set_text(GTK_LABEL(dialog->priv->title), _("Are you sure you want to leave this page?"));

        GtkWidget* button = webkitScriptDialogImplAddButton(dialog, _("Stay on Page"));
        g_signal_connect_swapped(button, "clicked", G_CALLBACK(webkitScriptDialogImplCancel), dialog);
        button = webkitScriptDialogImplAddButton(dialog, _("Leave Page"));
        dialog->priv->defaultButton = button;
        g_signal_connect_swapped(button, "clicked", G_CALLBACK(webkitScriptDialogImplConfirm), dialog);
        webkitScriptDialogImplSetText(dialog, scriptDialog->message.data(), maxSize);
        break;
    }
    }

    return GTK_WIDGET(dialog);
}

void webkitScriptDialogImplCancel(WebKitScriptDialogImpl* dialog)
{
    switch (dialog->priv->dialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        dialog->priv->dialog->confirmed = false;
        break;
    }
    webkitScriptDialogImplClose(dialog);
}

void webkitScriptDialogImplConfirm(WebKitScriptDialogImpl* dialog)
{
    switch (dialog->priv->dialog->type) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        break;
    case WEBKIT_SCRIPT_DIALOG_PROMPT:
        dialog->priv->dialog->text = gtk_entry_get_text(GTK_ENTRY(dialog->priv->entry));
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
        dialog->priv->dialog->confirmed = true;
        break;
    }
    webkitScriptDialogImplClose(dialog);
}

void webkitScriptDialogImplSetEntryText(WebKitScriptDialogImpl* dialog, const String& text)
{
    if (dialog->priv->dialog->type != WEBKIT_SCRIPT_DIALOG_PROMPT)
        return;

    gtk_entry_set_text(GTK_ENTRY(dialog->priv->entry), text.utf8().data());
}
