/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebAuthDialog.h"

#include <WebCore/GtkAuthenticationDialog.h>

typedef struct {
    GObject parent;
} WebAuthDialog;

typedef struct {
    GObjectClass parent;
} WebAuthDialogClass;

static void webAuthDialogSessionFeatureInit(SoupSessionFeatureInterface*, gpointer);

G_DEFINE_TYPE_WITH_CODE(WebAuthDialog, web_auth_dialog, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(SOUP_TYPE_SESSION_FEATURE,
                                              webAuthDialogSessionFeatureInit))

static void web_auth_dialog_class_init(WebAuthDialogClass*)
{
}

static void web_auth_dialog_init(WebAuthDialog*)
{
}

static void sessionAuthenticate(SoupSession* session, SoupMessage* message, SoupAuth* auth, gboolean, gpointer)
{
    WebCore::GtkAuthenticationDialog* authDialog = new WebCore::GtkAuthenticationDialog(0, session, message, auth);
    authDialog->show();
}

static void attach(SoupSessionFeature*, SoupSession* session)
{
    g_signal_connect(session, "authenticate", G_CALLBACK(sessionAuthenticate), 0);
}

static void detach(SoupSessionFeature*, SoupSession* session)
{
    g_signal_handlers_disconnect_by_func(session, reinterpret_cast<gpointer>(sessionAuthenticate), 0);
}

static void webAuthDialogSessionFeatureInit(SoupSessionFeatureInterface* feature, gpointer)
{
    feature->attach = attach;
    feature->detach = detach;
}
