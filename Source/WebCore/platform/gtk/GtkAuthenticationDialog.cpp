/*
 * Copyright (C) 2009, 2011 Igalia S.L.
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
#include "GtkAuthenticationDialog.h"

#include "CredentialBackingStore.h"
#include "GtkVersioning.h"
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WebCore {

static const int gLayoutColumnSpacing = 12;
static const int gLayoutRowSpacing = 6;
static const int gButtonSpacing = 5;

GtkAuthenticationDialog::GtkAuthenticationDialog(const AuthenticationChallenge& challenge, CredentialStorageMode mode)
    : m_dialog(0)
    , m_loginEntry(0)
    , m_passwordEntry(0)
    , m_rememberCheckButton(0)
    , m_challenge(challenge)
    , m_credentialStorageMode(mode)
{
}

GtkAuthenticationDialog::GtkAuthenticationDialog(GtkWindow* parentWindow, const AuthenticationChallenge& challenge, CredentialStorageMode mode)
    : m_dialog(gtk_dialog_new())
    , m_loginEntry(0)
    , m_passwordEntry(0)
    , m_rememberCheckButton(0)
    , m_challenge(challenge)
    , m_credentialStorageMode(mode)
{
    GtkWidget* contentArea = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));
    gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(m_dialog)), 5);
    gtk_box_set_spacing(GTK_BOX(contentArea), 2); /* 2 * 5 + 2 = 12 */

    GtkWindow* window = GTK_WINDOW(m_dialog);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_title(window, "");
    gtk_window_set_icon_name(window, GTK_STOCK_DIALOG_AUTHENTICATION);

    if (parentWindow)
        gtk_window_set_transient_for(window, parentWindow);

    createContentsInContainer(contentArea);
}

#ifdef GTK_API_VERSION_2
static void packTwoColumnLayoutInBox(GtkWidget* box, ...)
{
    va_list argumentList;
    va_start(argumentList, box);

    GtkWidget* table = gtk_table_new(1, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), gLayoutColumnSpacing);
    gtk_table_set_row_spacings(GTK_TABLE(table), gLayoutRowSpacing);

    GtkWidget* firstColumnWidget = va_arg(argumentList, GtkWidget*);
    int rowNumber = 0;
    while (firstColumnWidget) {
        if (rowNumber)
            gtk_table_resize(GTK_TABLE(table), rowNumber + 1, 2);

        GtkWidget* secondColumnWidget = va_arg(argumentList, GtkWidget*);
        GtkAttachOptions attachOptions = static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL);
        gtk_table_attach(GTK_TABLE(table), firstColumnWidget,
            0, secondColumnWidget ? 1 : 2,
            rowNumber, rowNumber + 1,
            attachOptions, attachOptions,
            0, 0);

        if (secondColumnWidget)
            gtk_table_attach_defaults(GTK_TABLE(table), secondColumnWidget, 1, 2, rowNumber, rowNumber + 1);

        firstColumnWidget = va_arg(argumentList, GtkWidget*);
        rowNumber++;
    }

    va_end(argumentList);

    gtk_box_pack_start(GTK_BOX(box), table, FALSE, FALSE, 0);
}
#else
static void packTwoColumnLayoutInBox(GtkWidget* box, ...)
{
    va_list argumentList;
    va_start(argumentList, box);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), gLayoutRowSpacing);
    gtk_grid_set_row_spacing(GTK_GRID(grid), gLayoutRowSpacing);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    GtkWidget* firstColumnWidget = va_arg(argumentList, GtkWidget*);
    int rowNumber = 0;
    while (firstColumnWidget) {
        GtkWidget* secondColumnWidget = va_arg(argumentList, GtkWidget*);
        int firstWidgetWidth = secondColumnWidget ? 1 : 2;

        gtk_grid_attach(GTK_GRID(grid), firstColumnWidget, 0, rowNumber, firstWidgetWidth, 1);
        gtk_widget_set_hexpand(firstColumnWidget, TRUE);
        gtk_widget_set_vexpand(firstColumnWidget, TRUE);

        if (secondColumnWidget) {
            gtk_grid_attach(GTK_GRID(grid), secondColumnWidget, 1, rowNumber, 1, 1);
            gtk_widget_set_hexpand(secondColumnWidget, TRUE);
            gtk_widget_set_vexpand(secondColumnWidget, TRUE);
        }

        firstColumnWidget = va_arg(argumentList, GtkWidget*);
        rowNumber++;
    }

    va_end(argumentList);

    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
}
#endif

static GtkWidget* createDialogLabel(const char* labelString, int horizontalPadding = 0)
{
    GtkWidget* label = gtk_label_new(labelString);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    if (horizontalPadding)
        gtk_misc_set_padding(GTK_MISC(label), 0, horizontalPadding);
    return label;
}

static GtkWidget* createDialogEntry(GtkWidget** member)
{
    *member = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(*member), TRUE);
    return *member;
}

void GtkAuthenticationDialog::createContentsInContainer(GtkWidget* container)
{
#ifdef GTK_API_VERSION_2
    GtkWidget* hBox = gtk_hbox_new(FALSE, gLayoutColumnSpacing);
#else
    GtkWidget* hBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, gLayoutColumnSpacing);
#endif
    gtk_container_set_border_width(GTK_CONTAINER(hBox), gButtonSpacing);
    gtk_container_add(GTK_CONTAINER(container), hBox);

    GtkWidget* icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0);
    gtk_box_pack_start(GTK_BOX(hBox), icon, FALSE, FALSE, 0);

    GOwnPtr<char> prompt(g_strdup_printf(
        _("The site %s:%i requests a username and password"),
        m_challenge.protectionSpace().host().utf8().data(),
        m_challenge.protectionSpace().port()));

    m_rememberCheckButton = gtk_check_button_new_with_mnemonic(_("_Remember password"));
    gtk_label_set_line_wrap(GTK_LABEL(gtk_bin_get_child(GTK_BIN(m_rememberCheckButton))), TRUE);
    gtk_widget_set_no_show_all(m_rememberCheckButton, m_credentialStorageMode == DisallowPersistentStorage);


    // We are adding the button box here manually instead of using the ready-made GtkDialog buttons.
    // This is so that we can share the code with implementations that do not use GtkDialog.
#ifdef GTK_API_VERSION_2
    GtkWidget* buttonBox = gtk_hbutton_box_new();
#else
    GtkWidget* buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
#endif
    gtk_box_set_spacing(GTK_BOX(buttonBox), gButtonSpacing);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);

    m_okayButton = gtk_button_new_from_stock(GTK_STOCK_OK);
    m_cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_box_pack_start(GTK_BOX(buttonBox), m_cancelButton, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttonBox), m_okayButton, FALSE, TRUE, 0);
    g_signal_connect(m_okayButton, "clicked", G_CALLBACK(buttonClickedCallback), this);
    g_signal_connect(m_cancelButton, "clicked", G_CALLBACK(buttonClickedCallback), this);

    String realm = m_challenge.protectionSpace().realm();
    if (!realm.isEmpty()) {
        packTwoColumnLayoutInBox(hBox,
            createDialogLabel(prompt.get(), gLayoutRowSpacing), NULL,
            createDialogLabel(_("Server message:")), createDialogLabel(realm.utf8().data()),
            createDialogLabel(_("Username:")), createDialogEntry(&m_loginEntry),
            createDialogLabel(_("Password:")), createDialogEntry(&m_passwordEntry),
            m_rememberCheckButton, NULL,
            buttonBox, NULL, NULL);

    } else {
        packTwoColumnLayoutInBox(hBox,
            createDialogLabel(prompt.get(), gLayoutRowSpacing), NULL,
            createDialogLabel(_("Username:")), createDialogEntry(&m_loginEntry),
            createDialogLabel(_("Password:")), createDialogEntry(&m_passwordEntry),
            m_rememberCheckButton, NULL, NULL,
            buttonBox, NULL, NULL);
    }

    gtk_entry_set_visibility(GTK_ENTRY(m_passwordEntry), FALSE);
    const Credential& credentialFromPersistentStorage = m_challenge.proposedCredential();
    if (!credentialFromPersistentStorage.isEmpty()) {
        gtk_entry_set_text(GTK_ENTRY(m_loginEntry), credentialFromPersistentStorage.user().utf8().data());
        gtk_entry_set_text(GTK_ENTRY(m_passwordEntry), credentialFromPersistentStorage.password().utf8().data());
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_rememberCheckButton), TRUE);
    }
}

void GtkAuthenticationDialog::show()
{
    gtk_widget_set_can_default(m_okayButton, TRUE);
    gtk_widget_grab_default(m_okayButton);
    gtk_widget_grab_focus(m_loginEntry);

    gtk_widget_show_all(m_dialog);
}

void GtkAuthenticationDialog::destroy()
{
    gtk_widget_destroy(m_dialog);
    delete this;
}

void GtkAuthenticationDialog::authenticate(const Credential& credential)
{
    if (credential.isEmpty())
        m_challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(m_challenge);
    else
        m_challenge.authenticationClient()->receivedCredential(m_challenge, credential);
}

void GtkAuthenticationDialog::buttonClickedCallback(GtkWidget* button, GtkAuthenticationDialog* dialog)
{
    Credential credential;
    if (button == dialog->m_okayButton) {
        const char *username = gtk_entry_get_text(GTK_ENTRY(dialog->m_loginEntry));
        const char *password = gtk_entry_get_text(GTK_ENTRY(dialog->m_passwordEntry));
        bool rememberPassword = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->m_rememberCheckButton));
        CredentialPersistence persistence;

        if (rememberPassword && dialog->m_credentialStorageMode == AllowPersistentStorage)
            persistence = CredentialPersistencePermanent;
        else
            persistence = CredentialPersistenceForSession;

        credential = Credential(String::fromUTF8(username), String::fromUTF8(password), persistence);
    }

    dialog->authenticate(credential);
    dialog->destroy();
}

} // namespace WebCore
