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
#include "WebKitAuthenticationDialog.h"

#include "AuthenticationDecisionListener.h"
#include "WebKitAuthenticationRequestPrivate.h"
#include "WebKitCredentialPrivate.h"
#include "WebKitWebView.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

struct _WebKitAuthenticationDialogPrivate {
    GRefPtr<WebKitAuthenticationRequest> request;
    CredentialStorageMode credentialStorageMode;
    GtkWidget* loginEntry;
    GtkWidget* passwordEntry;
    GtkWidget* rememberCheckButton;
    GtkWidget* defaultButton;
    unsigned long authenticationCancelledID;
};

WEBKIT_DEFINE_TYPE(WebKitAuthenticationDialog, webkit_authentication_dialog, WEBKIT_TYPE_WEB_VIEW_DIALOG)

static void webkitAuthenticationDialogDestroy(WebKitAuthenticationDialog* authDialog)
{
#if USE(GTK4)
    gtk_widget_unparent(GTK_WIDGET(authDialog));
#else
    gtk_widget_destroy(GTK_WIDGET(authDialog));
#endif
}

static void okButtonClicked(GtkButton*, WebKitAuthenticationDialog* authDialog)
{
    WebKitAuthenticationDialogPrivate* priv = authDialog->priv;
    const char* username = gtk_entry_get_text(GTK_ENTRY(priv->loginEntry));
    const char* password = gtk_entry_get_text(GTK_ENTRY(priv->passwordEntry));
    bool rememberPassword = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->rememberCheckButton));

    WebCore::CredentialPersistence persistence = rememberPassword && priv->credentialStorageMode == AllowPersistentStorage ?
        WebCore::CredentialPersistencePermanent : WebCore::CredentialPersistenceForSession;

    // FIXME: Use a stack allocated WebKitCredential.
    WebKitCredential* credential = webkitCredentialCreate(WebCore::Credential(String::fromUTF8(username), String::fromUTF8(password), persistence));
    webkit_authentication_request_authenticate(priv->request.get(), credential);
    webkit_credential_free(credential);
    webkitAuthenticationDialogDestroy(authDialog);
}

static void cancelButtonClicked(GtkButton*, WebKitAuthenticationDialog* authDialog)
{
    webkit_authentication_request_authenticate(authDialog->priv->request.get(), nullptr);
    webkitAuthenticationDialogDestroy(authDialog);
}

static void authenticationCancelled(WebKitAuthenticationRequest*, WebKitAuthenticationDialog* authDialog)
{
    webkitAuthenticationDialogDestroy(authDialog);
}

static GtkWidget* createLabelWithLineWrap(const char* text)
{
    GtkWidget* label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 40);
    return label;
}

static void webkitAuthenticationDialogInitialize(WebKitAuthenticationDialog* authDialog)
{
    GtkWidget* vBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(box, GTK_STYLE_CLASS_TITLEBAR);
    gtk_widget_set_size_request(box, -1, 16);
    GtkWidget* title = gtk_label_new(_("Authentication Required"));
    gtk_widget_set_margin_top(title, 6);
    gtk_widget_set_margin_bottom(title, 6);
    gtk_widget_add_css_class(title, GTK_STYLE_CLASS_TITLE);
#if USE(GTK4)
    gtk_widget_set_hexpand(title, TRUE);
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vBox), title);
#else
    gtk_box_set_center_widget(GTK_BOX(box), title);
    gtk_widget_show(title);
    gtk_box_pack_start(GTK_BOX(vBox), box, TRUE, FALSE, 0);
    gtk_widget_show(box);
#endif

#if USE(GTK4)
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(buttonBox), TRUE);
    gtk_widget_set_halign(buttonBox, GTK_ALIGN_FILL);
#else
    GtkWidget* buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EXPAND);
#endif
    gtk_widget_set_hexpand(buttonBox, TRUE);
    gtk_widget_add_css_class(buttonBox, "dialog-action-area");

    GtkWidget* button = gtk_button_new_with_mnemonic(_("_Cancel"));
    g_signal_connect(button, "clicked", G_CALLBACK(cancelButtonClicked), authDialog);
#if USE(GTK4)
    gtk_box_append(GTK_BOX(buttonBox), button);
#else
    gtk_box_pack_start(GTK_BOX(buttonBox), button, FALSE, TRUE, 0);
    gtk_widget_show(button);
#endif

    WebKitAuthenticationDialogPrivate* priv = authDialog->priv;
    button = gtk_button_new_with_mnemonic(_("_Authenticate"));
    priv->defaultButton = button;
    g_signal_connect(button, "clicked", G_CALLBACK(okButtonClicked), authDialog);
#if USE(GTK4)
    gtk_box_append(GTK_BOX(buttonBox), button);
#else
    gtk_widget_set_can_default(button, TRUE);
    gtk_box_pack_end(GTK_BOX(buttonBox), button, FALSE, TRUE, 0);
    gtk_widget_show(button);
#endif

    GtkWidget* authBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(authBox, 10);
    gtk_widget_set_margin_end(authBox, 10);

    const WebCore::AuthenticationChallenge& challenge = webkitAuthenticationRequestGetAuthenticationChallenge(priv->request.get())->core();
    // Prompt on the HTTP authentication dialog.
    GUniquePtr<char> prompt(g_strdup_printf(_("Authentication required by %s:%i"),
        challenge.protectionSpace().host().utf8().data(), challenge.protectionSpace().port()));
    GtkWidget* label = createLabelWithLineWrap(prompt.get());
#if USE(GTK4)
    gtk_box_append(GTK_BOX(authBox), label);
#else
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(authBox), label, FALSE, FALSE, 0);
#endif

    String realm = challenge.protectionSpace().realm();
    if (!realm.isEmpty()) {
        // Label on the HTTP authentication dialog. %s is a (probably English) message from the website.
        GUniquePtr<char> message(g_strdup_printf(_("The site says: “%s”"), realm.utf8().data()));
        label = createLabelWithLineWrap(message.get());
#if USE(GTK4)
        gtk_box_append(GTK_BOX(authBox), label);
#else
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(authBox), label, FALSE, FALSE, 0);
#endif
    }

    // Check button on the HTTP authentication dialog.
    priv->rememberCheckButton = gtk_check_button_new_with_mnemonic(_("_Remember password"));
#if USE(GTK4)
    gtk_label_set_line_wrap(GTK_LABEL(gtk_button_get_child(GTK_BUTTON(priv->rememberCheckButton))), TRUE);
#else
    gtk_label_set_line_wrap(GTK_LABEL(gtk_bin_get_child(GTK_BIN(priv->rememberCheckButton))), TRUE);
#endif

    priv->loginEntry = gtk_entry_new();
    gtk_widget_set_hexpand(priv->loginEntry, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(priv->loginEntry), TRUE);
    gtk_widget_show(priv->loginEntry);

    // Entry on the HTTP authentication dialog.
    GtkWidget* loginLabel = gtk_label_new_with_mnemonic(_("_Username"));
    gtk_label_set_mnemonic_widget(GTK_LABEL(loginLabel), priv->loginEntry);
    gtk_widget_set_halign(loginLabel, GTK_ALIGN_END);
    gtk_widget_add_css_class(loginLabel, GTK_STYLE_CLASS_DIM_LABEL);
    gtk_widget_show(loginLabel);

    priv->passwordEntry = gtk_entry_new();
    gtk_widget_set_hexpand(priv->passwordEntry, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(priv->passwordEntry), TRUE);
    gtk_widget_show(priv->passwordEntry);

    // Entry on the HTTP authentication dialog.
    GtkWidget* passwordLabel = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_label_set_mnemonic_widget(GTK_LABEL(passwordLabel), priv->passwordEntry);
    gtk_widget_set_halign(passwordLabel, GTK_ALIGN_END);
    gtk_widget_add_css_class(passwordLabel, GTK_STYLE_CLASS_DIM_LABEL);
    gtk_widget_show(passwordLabel);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_attach(GTK_GRID(grid), loginLabel, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), priv->loginEntry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), passwordLabel, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), priv->passwordEntry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), priv->rememberCheckButton, 1, 2, 1, 1);
#if USE(GTK4)
    gtk_box_append(GTK_BOX(authBox), grid);
#else
    gtk_widget_show(grid);
    gtk_box_pack_start(GTK_BOX(authBox), grid, FALSE, FALSE, 0);
#endif

    gtk_entry_set_visibility(GTK_ENTRY(priv->passwordEntry), FALSE);
    gtk_widget_set_visible(priv->rememberCheckButton, priv->credentialStorageMode != DisallowPersistentStorage && !realm.isEmpty());

    const WebCore::Credential& credentialFromPersistentStorage = challenge.proposedCredential();
    if (!credentialFromPersistentStorage.isEmpty()) {
        gtk_entry_set_text(GTK_ENTRY(priv->loginEntry), credentialFromPersistentStorage.user().utf8().data());
        gtk_entry_set_text(GTK_ENTRY(priv->passwordEntry), credentialFromPersistentStorage.password().utf8().data());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->rememberCheckButton), TRUE);
    }

#if USE(GTK4)
    gtk_box_append(GTK_BOX(vBox), authBox);
#else
    gtk_box_pack_start(GTK_BOX(vBox), authBox, TRUE, TRUE, 0);
    gtk_widget_show(authBox);
#endif

#if USE(GTK4)
    gtk_box_append(GTK_BOX(vBox), buttonBox);
#else
    gtk_box_pack_end(GTK_BOX(vBox), buttonBox, FALSE, TRUE, 0);
    gtk_widget_show(buttonBox);
#endif

#if USE(GTK4)
    gtk_widget_add_css_class(vBox, "dialog-vbox");
    webkitWebViewDialogSetChild(WEBKIT_WEB_VIEW_DIALOG(authDialog), vBox);
#else
    gtk_container_add(GTK_CONTAINER(authDialog), vBox);
    gtk_widget_show(vBox);
#endif

    authDialog->priv->authenticationCancelledID = g_signal_connect(authDialog->priv->request.get(), "cancelled", G_CALLBACK(authenticationCancelled), authDialog);
}

static void webkitAuthenticationDialogMap(GtkWidget* widget)
{
    WebKitAuthenticationDialogPrivate* priv = WEBKIT_AUTHENTICATION_DIALOG(widget)->priv;
    gtk_widget_grab_focus(priv->loginEntry);
    auto* toplevel = gtk_widget_get_toplevel(widget);
    if (WebCore::widgetIsOnscreenToplevelWindow(toplevel))
        gtk_window_set_default(GTK_WINDOW(toplevel), priv->defaultButton);

    GTK_WIDGET_CLASS(webkit_authentication_dialog_parent_class)->map(widget);
}

static void webkitAuthenticationDialogDispose(GObject* object)
{
    WebKitAuthenticationDialogPrivate* priv = WEBKIT_AUTHENTICATION_DIALOG(object)->priv;
    if (priv->authenticationCancelledID) {
        g_signal_handler_disconnect(priv->request.get(), priv->authenticationCancelledID);
        priv->authenticationCancelledID = 0;
    }

#if USE(GTK4)
    webkitWebViewDialogSetChild(WEBKIT_WEB_VIEW_DIALOG(object), nullptr);
#endif

    G_OBJECT_CLASS(webkit_authentication_dialog_parent_class)->dispose(object);
}

static void webkit_authentication_dialog_class_init(WebKitAuthenticationDialogClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispose = webkitAuthenticationDialogDispose;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->map = webkitAuthenticationDialogMap;
}

GtkWidget* webkitAuthenticationDialogNew(WebKitAuthenticationRequest* request, CredentialStorageMode mode)
{
    WebKitAuthenticationDialog* authDialog = WEBKIT_AUTHENTICATION_DIALOG(g_object_new(WEBKIT_TYPE_AUTHENTICATION_DIALOG, NULL));
    authDialog->priv->request = request;
    authDialog->priv->credentialStorageMode = mode;
    webkitAuthenticationDialogInitialize(authDialog);
    return GTK_WIDGET(authDialog);
}
