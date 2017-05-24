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
#include "WebKitPrivate.h"
#include "WebKitWebView.h"
#include <glib/gi18n-lib.h>
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
    GRefPtr<GtkStyleContext> styleContext;
};

WEBKIT_DEFINE_TYPE(WebKitAuthenticationDialog, webkit_authentication_dialog, GTK_TYPE_EVENT_BOX)

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
    gtk_widget_destroy(GTK_WIDGET(authDialog));
}

static void cancelButtonClicked(GtkButton*, WebKitAuthenticationDialog* authDialog)
{
    webkit_authentication_request_authenticate(authDialog->priv->request.get(), nullptr);
    gtk_widget_destroy(GTK_WIDGET(authDialog));
}

static void authenticationCancelled(WebKitAuthenticationRequest*, WebKitAuthenticationDialog* authDialog)
{
    gtk_widget_destroy(GTK_WIDGET(authDialog));
}

static GtkWidget* createLabelWithLineWrap(const char* text)
{
    GtkWidget* label = gtk_label_new(text);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 40);
    return label;
}

static void webkitAuthenticationDialogInitialize(WebKitAuthenticationDialog* authDialog)
{
    GtkWidget* frame = gtk_frame_new(0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    GtkWidget* vBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vBox), 12);

    GtkWidget* label = gtk_label_new(nullptr);
    // Title of the HTTP authentication dialog.
    GUniquePtr<char> title(g_strdup_printf("<b>%s</b>", _("Authentication Required")));
    gtk_label_set_markup(GTK_LABEL(label), title.get());
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(vBox), label, FALSE, FALSE, 0);

    GtkWidget* buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
    gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 5);
    gtk_box_set_spacing(GTK_BOX(buttonBox), 6);

    GtkWidget* button = gtk_button_new_with_mnemonic(_("_Cancel"));
    g_signal_connect(button, "clicked", G_CALLBACK(cancelButtonClicked), authDialog);
    gtk_box_pack_end(GTK_BOX(buttonBox), button, FALSE, TRUE, 0);
    gtk_widget_show(button);

    WebKitAuthenticationDialogPrivate* priv = authDialog->priv;
    button = gtk_button_new_with_mnemonic(_("_Authenticate"));
    priv->defaultButton = button;
    g_signal_connect(button, "clicked", G_CALLBACK(okButtonClicked), authDialog);
    gtk_widget_set_can_default(button, TRUE);
    gtk_box_pack_end(GTK_BOX(buttonBox), button, FALSE, TRUE, 0);
    gtk_widget_show(button);

    GtkWidget* authBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(authBox), 5);

    const WebCore::AuthenticationChallenge& challenge = webkitAuthenticationRequestGetAuthenticationChallenge(priv->request.get())->core();
    // Prompt on the HTTP authentication dialog.
    GUniquePtr<char> prompt(g_strdup_printf(_("Authentication required by %s:%i"),
        challenge.protectionSpace().host().utf8().data(), challenge.protectionSpace().port()));
    label = createLabelWithLineWrap(prompt.get());
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(authBox), label, FALSE, FALSE, 0);

    String realm = challenge.protectionSpace().realm();
    if (!realm.isEmpty()) {
        // Label on the HTTP authentication dialog. %s is a (probably English) message from the website.
        GUniquePtr<char> message(g_strdup_printf(_("The site says: “%s”"), realm.utf8().data()));
        label = createLabelWithLineWrap(message.get());
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(authBox), label, FALSE, FALSE, 0);
    }

    // Check button on the HTTP authentication dialog.
    priv->rememberCheckButton = gtk_check_button_new_with_mnemonic(_("_Remember password"));
    gtk_label_set_line_wrap(GTK_LABEL(gtk_bin_get_child(GTK_BIN(priv->rememberCheckButton))), TRUE);

    priv->loginEntry = gtk_entry_new();
    gtk_widget_set_hexpand(priv->loginEntry, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(priv->loginEntry), TRUE);
    gtk_widget_show(priv->loginEntry);

    // Entry on the HTTP authentication dialog.
    GtkWidget* loginLabel = gtk_label_new_with_mnemonic(_("_Username"));
    gtk_label_set_mnemonic_widget(GTK_LABEL(loginLabel), priv->loginEntry);
    gtk_widget_set_halign(loginLabel, GTK_ALIGN_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(loginLabel), GTK_STYLE_CLASS_DIM_LABEL);
    gtk_widget_show(loginLabel);

    priv->passwordEntry = gtk_entry_new();
    gtk_widget_set_hexpand(priv->passwordEntry, TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(priv->passwordEntry), TRUE);
    gtk_widget_show(priv->passwordEntry);

    // Entry on the HTTP authentication dialog.
    GtkWidget* passwordLabel = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_label_set_mnemonic_widget(GTK_LABEL(passwordLabel), priv->passwordEntry);
    gtk_widget_set_halign(passwordLabel, GTK_ALIGN_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(passwordLabel), GTK_STYLE_CLASS_DIM_LABEL);
    gtk_widget_show(passwordLabel);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_attach(GTK_GRID(grid), loginLabel, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), priv->loginEntry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), passwordLabel, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), priv->passwordEntry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), priv->rememberCheckButton, 1, 2, 1, 1);
    gtk_widget_show(grid);
    gtk_box_pack_start(GTK_BOX(authBox), grid, FALSE, FALSE, 0);

    gtk_entry_set_visibility(GTK_ENTRY(priv->passwordEntry), FALSE);
    gtk_widget_set_visible(priv->rememberCheckButton, priv->credentialStorageMode != DisallowPersistentStorage && !realm.isEmpty());

    const WebCore::Credential& credentialFromPersistentStorage = challenge.proposedCredential();
    if (!credentialFromPersistentStorage.isEmpty()) {
        gtk_entry_set_text(GTK_ENTRY(priv->loginEntry), credentialFromPersistentStorage.user().utf8().data());
        gtk_entry_set_text(GTK_ENTRY(priv->passwordEntry), credentialFromPersistentStorage.password().utf8().data());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->rememberCheckButton), TRUE);
    }

    gtk_box_pack_start(GTK_BOX(vBox), authBox, TRUE, TRUE, 0);
    gtk_widget_show(authBox);

    gtk_box_pack_end(GTK_BOX(vBox), buttonBox, FALSE, TRUE, 0);
    gtk_widget_show(buttonBox);

    gtk_container_add(GTK_CONTAINER(frame), vBox);
    gtk_widget_show(vBox);

    gtk_container_add(GTK_CONTAINER(authDialog), frame);
    gtk_widget_show(frame);

    authDialog->priv->authenticationCancelledID = g_signal_connect(authDialog->priv->request.get(), "cancelled", G_CALLBACK(authenticationCancelled), authDialog);
}

static gboolean webkitAuthenticationDialogDraw(GtkWidget* widget, cairo_t* cr)
{
    WebKitAuthenticationDialogPrivate* priv = WEBKIT_AUTHENTICATION_DIALOG(widget)->priv;

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_paint(cr);

    if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget))) {
        GtkAllocation allocation;
        gtk_widget_get_allocation(child, &allocation);

        gtk_style_context_save(priv->styleContext.get());
        gtk_style_context_add_class(priv->styleContext.get(), GTK_STYLE_CLASS_BACKGROUND);
        gtk_render_background(priv->styleContext.get(), cr, allocation.x, allocation.y, allocation.width, allocation.height);
        gtk_style_context_restore(priv->styleContext.get());
    }

    GTK_WIDGET_CLASS(webkit_authentication_dialog_parent_class)->draw(widget, cr);

    return FALSE;
}

static void webkitAuthenticationDialogMap(GtkWidget* widget)
{
    WebKitAuthenticationDialogPrivate* priv = WEBKIT_AUTHENTICATION_DIALOG(widget)->priv;
    gtk_widget_grab_focus(priv->loginEntry);
    gtk_widget_grab_default(priv->defaultButton);

    GTK_WIDGET_CLASS(webkit_authentication_dialog_parent_class)->map(widget);
}

static void webkitAuthenticationDialogSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(webkit_authentication_dialog_parent_class)->size_allocate(widget, allocation);

    GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
    if (!child)
        return;

    GtkRequisition naturalSize;
    gtk_widget_get_preferred_size(child, 0, &naturalSize);

    GtkAllocation childAllocation;
    gtk_widget_get_allocation(child, &childAllocation);

    childAllocation.x += (allocation->width - naturalSize.width) / 2;
    childAllocation.y += (allocation->height - naturalSize.height) / 2;
    childAllocation.width = naturalSize.width;
    childAllocation.height = naturalSize.height;
    gtk_widget_size_allocate(child, &childAllocation);
}

static void webkitAuthenticationDialogConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_authentication_dialog_parent_class)->constructed(object);

    WebKitAuthenticationDialogPrivate* priv = WEBKIT_AUTHENTICATION_DIALOG(object)->priv;
    priv->styleContext = adoptGRef(gtk_style_context_new());
    GtkWidgetPath* path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
    gtk_style_context_set_path(priv->styleContext.get(), path);
    gtk_widget_path_free(path);
}

static void webkitAuthenticationDialogDispose(GObject* object)
{
    WebKitAuthenticationDialogPrivate* priv = WEBKIT_AUTHENTICATION_DIALOG(object)->priv;
    if (priv->authenticationCancelledID) {
        g_signal_handler_disconnect(priv->request.get(), priv->authenticationCancelledID);
        priv->authenticationCancelledID = 0;
    }

    G_OBJECT_CLASS(webkit_authentication_dialog_parent_class)->dispose(object);
}

static void webkit_authentication_dialog_class_init(WebKitAuthenticationDialogClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitAuthenticationDialogConstructed;
    objectClass->dispose = webkitAuthenticationDialogDispose;

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->draw = webkitAuthenticationDialogDraw;
    widgetClass->map = webkitAuthenticationDialogMap;
    widgetClass->size_allocate = webkitAuthenticationDialogSizeAllocate;
}

GtkWidget* webkitAuthenticationDialogNew(WebKitAuthenticationRequest* request, CredentialStorageMode mode)
{
    WebKitAuthenticationDialog* authDialog = WEBKIT_AUTHENTICATION_DIALOG(g_object_new(WEBKIT_TYPE_AUTHENTICATION_DIALOG, NULL));
    authDialog->priv->request = request;
    authDialog->priv->credentialStorageMode = mode;
    webkitAuthenticationDialogInitialize(authDialog);
    return GTK_WIDGET(authDialog);
}
