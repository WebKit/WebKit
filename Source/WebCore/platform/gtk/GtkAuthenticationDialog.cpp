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

#include "GtkVersioning.h"
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

namespace WebCore {

#ifdef GTK_API_VERSION_2
static GtkWidget* addEntryToTable(GtkTable* table, int row, const char* labelText)
#else
static GtkWidget* addEntryToGrid(GtkGrid* grid, int row, const char* labelText)
#endif
{
    GtkWidget* label = gtk_label_new(labelText);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

#ifdef GTK_API_VERSION_2
    gtk_table_attach(table, label, 0, 1, row, row + 1, GTK_FILL, static_cast<GtkAttachOptions>(GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_table_attach_defaults(table, entry, 1, 2, row, row + 1);
#else
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_widget_set_hexpand(label, TRUE);

    gtk_grid_attach(grid, entry, 1, row, 1, 1);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_widget_set_vexpand(entry, TRUE);
#endif

    return entry;
}

static bool sessionCanSavePasswords(SoupSession* session)
{
#ifdef SOUP_TYPE_PASSWORD_MANAGER
    return soup_session_get_feature(session, SOUP_TYPE_PASSWORD_MANAGER);
#else
    return false;
#endif
}

GtkAuthenticationDialog::~GtkAuthenticationDialog()
{
}

GtkAuthenticationDialog::GtkAuthenticationDialog(GtkWindow* parentWindow, SoupSession* session, SoupMessage* message, SoupAuth* auth)
    : m_dialog(gtk_dialog_new())
    , m_session(session)
    , m_message(message)
    , m_auth(auth)
    , m_loginEntry(0)
    , m_passwordEntry(0)
    , m_rememberCheckButton(0)
#ifdef SOUP_TYPE_PASSWORD_MANAGER
    , m_isSavingPassword(false)
    , m_savePasswordHandler(0)
#endif
{
    GtkDialog* dialog = GTK_DIALOG(m_dialog);
    gtk_dialog_add_buttons(dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

    // Set the dialog up with HIG properties.
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
    gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(dialog)), 2); /* 2 * 5 + 2 = 12 */
    gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_action_area(dialog)), 5);
    gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_action_area(dialog)), 6);

    GtkWindow* window = GTK_WINDOW(m_dialog);
    gtk_window_set_resizable(window, FALSE);
    gtk_window_set_title(window, "");
    gtk_window_set_icon_name(window, GTK_STOCK_DIALOG_AUTHENTICATION);

    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_OK);

    if (parentWindow)
        gtk_window_set_transient_for(window, parentWindow);

    // Build contents.
#ifdef GTK_API_VERSION_2
    GtkWidget* hBox = gtk_hbox_new(FALSE, 12);
#else
    GtkWidget* hBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
#endif
    gtk_container_set_border_width(GTK_CONTAINER(hBox), 5);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(dialog)), hBox, TRUE, TRUE, 0);

    GtkWidget* icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);
    gtk_box_pack_start(GTK_BOX(hBox), icon, FALSE, FALSE, 0);

#ifdef GTK_API_VERSION_2
    GtkWidget* mainVBox = gtk_vbox_new(FALSE, 18);
#else
    GtkWidget* mainVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
#endif
    gtk_box_pack_start(GTK_BOX(hBox), mainVBox, TRUE, TRUE, 0);

    SoupURI* uri = soup_message_get_uri(m_message.get());
    GOwnPtr<char>description(g_strdup_printf(_("A username and password are being requested by the site %s"), uri->host));
    GtkWidget* descriptionLabel = gtk_label_new(description.get());
    gtk_misc_set_alignment(GTK_MISC(descriptionLabel), 0.0, 0.5);
    gtk_label_set_line_wrap(GTK_LABEL(descriptionLabel), TRUE);
    gtk_box_pack_start(GTK_BOX(mainVBox), GTK_WIDGET(descriptionLabel), FALSE, FALSE, 0);

#ifdef GTK_API_VERSION_2
    GtkWidget* vBox = gtk_vbox_new(FALSE, 6);
#else
    GtkWidget* vBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
#endif
    gtk_box_pack_start(GTK_BOX(mainVBox), vBox, FALSE, FALSE, 0);

    // The table that holds the entries.
    GtkWidget* entryContainer = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(entryContainer), 0, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(vBox), entryContainer, FALSE, FALSE, 0);

    const char* realm = soup_auth_get_realm(m_auth);
    // Checking that realm is not an empty string.
    bool hasRealm = (realm && realm[0] != '\0');

#ifdef GTK_API_VERSION_2
    GtkWidget* table = gtk_table_new(hasRealm ? 3 : 2, 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 12);
    gtk_table_set_row_spacings(GTK_TABLE(table), 6);
    gtk_container_add(GTK_CONTAINER(entryContainer), table);
#else
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_add(GTK_CONTAINER(entryContainer), grid);
#endif

    if (hasRealm) {
        GtkWidget* serverMessageDescriptionLabel = gtk_label_new(_("Server message:"));
        gtk_misc_set_alignment(GTK_MISC(serverMessageDescriptionLabel), 0.0, 0.5);
        gtk_label_set_line_wrap(GTK_LABEL(serverMessageDescriptionLabel), TRUE);
#ifdef GTK_API_VERSION_2
        gtk_table_attach_defaults(GTK_TABLE(table), serverMessageDescriptionLabel, 0, 1, 0, 1);
#else
        gtk_grid_attach(GTK_GRID(grid), serverMessageDescriptionLabel, 0, 0, 1, 1);
        gtk_widget_set_hexpand(serverMessageDescriptionLabel, TRUE);
        gtk_widget_set_vexpand(serverMessageDescriptionLabel, TRUE);
#endif
        GtkWidget* serverMessageLabel = gtk_label_new(realm);
        gtk_misc_set_alignment(GTK_MISC(serverMessageLabel), 0.0, 0.5);
        gtk_label_set_line_wrap(GTK_LABEL(serverMessageLabel), TRUE);
#ifdef GTK_API_VERSION_2
        gtk_table_attach_defaults(GTK_TABLE(table), serverMessageLabel, 1, 2, 0, 1);
#else
        gtk_grid_attach(GTK_GRID(grid), serverMessageLabel, 1, 0, 1, 1);
        gtk_widget_set_hexpand(serverMessageLabel, TRUE);
        gtk_widget_set_vexpand(serverMessageLabel, TRUE);
#endif
    }

#ifdef GTK_API_VERSION_2
    m_loginEntry = addEntryToTable(GTK_TABLE(table), hasRealm ? 1 : 0, _("Username:"));
    m_passwordEntry = addEntryToTable(GTK_TABLE(table), hasRealm ? 2 : 1, _("Password:"));
#else
    m_loginEntry = addEntryToGrid(GTK_GRID(grid), hasRealm ? 1 : 0, _("Username:"));
    m_passwordEntry = addEntryToGrid(GTK_GRID(grid), hasRealm ? 2 : 1, _("Password:"));
#endif

    gtk_entry_set_visibility(GTK_ENTRY(m_passwordEntry), FALSE);

    if (sessionCanSavePasswords(m_session)) {
#ifdef GTK_API_VERSION_2
        GtkWidget* rememberBox = gtk_vbox_new(FALSE, 6);
#else
        GtkWidget* rememberBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
#endif
        gtk_box_pack_start(GTK_BOX(vBox), rememberBox, FALSE, FALSE, 0);

        m_rememberCheckButton = gtk_check_button_new_with_mnemonic(_("_Remember password"));
        gtk_label_set_line_wrap(GTK_LABEL(gtk_bin_get_child(GTK_BIN(m_rememberCheckButton))), TRUE);
        gtk_box_pack_start(GTK_BOX(rememberBox), m_rememberCheckButton, FALSE, FALSE, 0);
    }
}

static bool getSavedLogin(SoupAuth* auth, const char** username, const char** password)
{
#ifdef SOUP_TYPE_PASSWORD_MANAGER
    GSList* users = soup_auth_get_saved_users(auth);
    if (!users)
        return false;

    *username = static_cast<char*>(users->data);
    *password = soup_auth_get_saved_password(auth, *username);
    g_slist_free(users);

    return *username && *password;
#else
    return false;
#endif
}

void GtkAuthenticationDialog::show()
{
    const char* username = 0;
    const char* password = 0;
    bool haveSavedLogin = getSavedLogin(m_auth, &username, &password);
    soup_session_pause_message(m_session, m_message.get());
    gtk_entry_set_text(GTK_ENTRY(m_loginEntry), username ? username : "");
    gtk_entry_set_text(GTK_ENTRY(m_passwordEntry), password ? password : "");
    if (m_rememberCheckButton && haveSavedLogin)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_rememberCheckButton), TRUE);
    g_signal_connect(m_dialog, "response", G_CALLBACK(authenticationDialogResponseCallback), this);
    gtk_widget_show_all(m_dialog);
}

void GtkAuthenticationDialog::destroy()
{
    bool shouldDelete = true;

    soup_session_unpause_message(m_session, m_message.get());
    gtk_widget_destroy(m_dialog);

#ifdef SOUP_TYPE_PASSWORD_MANAGER
    shouldDelete = !m_isSavingPassword;
#endif

    // Do not delete the object if it's still saving the password,
    // the save password callback will delete it.
    if (shouldDelete)
        delete this;
}

#ifdef SOUP_TYPE_PASSWORD_MANAGER
void GtkAuthenticationDialog::savePasswordCallback(SoupMessage* message, GtkAuthenticationDialog* dialog)
{
    dialog->savePassword();
}

void GtkAuthenticationDialog::savePassword()
{
    ASSERT(!m_username.isNull());
    ASSERT(!m_password.isNull());

    // Anything but 401 and 5xx means the password was accepted.
    if (m_message.get()->status_code != 401 && m_message.get()->status_code < 500)
        soup_auth_save_password(m_auth, m_username.data(), m_password.data());

    // Disconnect the callback. If the authentication succeeded we are done,
    // and if it failed we'll create a new GtkAuthenticationDialog and we'll
    // connect to 'got-headers' again in GtkAuthenticationDialog::authenticate()
    g_signal_handler_disconnect(m_message.get(), m_savePasswordHandler);

    // Dialog has been already destroyed, after saving the password it should be deleted.
    delete this;
}
#endif

void GtkAuthenticationDialog::authenticate()
{
    const char *username = gtk_entry_get_text(GTK_ENTRY(m_loginEntry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(m_passwordEntry));
    soup_auth_authenticate(m_auth, username, password);

#ifdef SOUP_TYPE_PASSWORD_MANAGER
    if (m_rememberCheckButton && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_rememberCheckButton))) {
        m_username = username;
        m_password = password;
        m_isSavingPassword = true;
        m_savePasswordHandler = g_signal_connect(m_message.get(), "got-headers", G_CALLBACK(savePasswordCallback), this);
    }
#endif
}

void GtkAuthenticationDialog::authenticationDialogResponseCallback(GtkWidget*, gint responseID, GtkAuthenticationDialog* dialog)
{
    if (responseID == GTK_RESPONSE_OK)
        dialog->authenticate();

    dialog->destroy();
}

} // namespace WebCore
