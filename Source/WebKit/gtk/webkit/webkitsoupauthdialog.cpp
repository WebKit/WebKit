/*
 * Copyright (C) 2009 Igalia S.L.
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
#include "webkitsoupauthdialog.h"

#include "GtkAuthenticationDialog.h"
#include "webkitmarshal.h"

using namespace WebCore;

/**
 * SECTION:webkitsoupauthdialog
 * @short_description: A #SoupSessionFeature to provide a simple
 * authentication dialog for HTTP basic auth support.
 *
 * #WebKitSoupAuthDialog is a #SoupSessionFeature that you can attach to your
 * #SoupSession to provide a simple authentication dialog while
 * handling HTTP basic auth.
 */

static void webkit_soup_auth_dialog_session_feature_init(SoupSessionFeatureInterface*, gpointer);
static void attach(SoupSessionFeature*, SoupSession*);
static void detach(SoupSessionFeature*, SoupSession*);

enum {
    CURRENT_TOPLEVEL,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE(WebKitSoupAuthDialog, webkit_soup_auth_dialog, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(SOUP_TYPE_SESSION_FEATURE,
                                              webkit_soup_auth_dialog_session_feature_init))

static void webkit_soup_auth_dialog_class_init(WebKitSoupAuthDialogClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);

    /**
     * WebKitSoupAuthDialog::current-toplevel:
     * @authDialog: the object on which the signal is emitted
     * @message: the #SoupMessage being used in the authentication process
     *
     * This signal is emitted by the @authDialog when it needs to know
     * the current toplevel widget in order to correctly set the
     * transiency for the authentication dialog.
     *
     * Return value: (transfer none): the current toplevel #GtkWidget or %NULL if there's none
     *
     * Since: 1.1.1
     */
    signals[CURRENT_TOPLEVEL] =
      g_signal_new("current-toplevel",
                   G_OBJECT_CLASS_TYPE(objectClass),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(WebKitSoupAuthDialogClass, current_toplevel),
                   0, 0,
                   webkit_marshal_OBJECT__OBJECT,
                   GTK_TYPE_WIDGET, 1,
                   SOUP_TYPE_MESSAGE);
}

static void webkit_soup_auth_dialog_init(WebKitSoupAuthDialog*)
{
}

static void webkit_soup_auth_dialog_session_feature_init(SoupSessionFeatureInterface *featureInterface, gpointer)
{
    featureInterface->attach = attach;
    featureInterface->detach = detach;
}

static void sessionAuthenticate(SoupSession* session, SoupMessage* message, SoupAuth* auth, gboolean, SoupSessionFeature* manager)
{
    GtkAuthenticationDialog* authDialog;
    GtkWidget* toplevel = 0;

    /* Get the current toplevel */
    g_signal_emit(manager, signals[CURRENT_TOPLEVEL], 0, message, &toplevel);

    authDialog = new GtkAuthenticationDialog(toplevel ? GTK_WINDOW(toplevel) : 0, session, message, auth);
    authDialog->show();
}

static void attach(SoupSessionFeature* manager, SoupSession* session)
{
    g_signal_connect(session, "authenticate", G_CALLBACK(sessionAuthenticate), manager);
}

static void detach(SoupSessionFeature* manager, SoupSession* session)
{
    g_signal_handlers_disconnect_by_func(session, reinterpret_cast<gpointer>(sessionAuthenticate), manager);
}


