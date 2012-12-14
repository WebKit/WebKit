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
#include "WebKit2GtkAuthenticationDialog.h"

#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "WebCredential.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <gtk/gtk.h>

namespace WebKit {

// This is necessary because GtkEventBox does not draw a background by default,
// but we want it to have a normal GtkWindow background.
static gboolean drawSignal(GtkWidget* widget, cairo_t* cr, GtkStyleContext* styleContext)
{
    gtk_render_background(styleContext, cr, 0, 0,
        gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
    return FALSE;
}

WebKit2GtkAuthenticationDialog::WebKit2GtkAuthenticationDialog(AuthenticationChallengeProxy* authenticationChallenge, CredentialStorageMode mode)
    : GtkAuthenticationDialog(authenticationChallenge->core(), mode)
    , m_authenticationChallenge(authenticationChallenge)
    , m_styleContext(adoptGRef(gtk_style_context_new()))
{
    m_dialog = gtk_event_box_new();

    GtkWidget* frame = gtk_frame_new(0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(m_dialog), frame);
    createContentsInContainer(frame);

    gtk_style_context_add_class(m_styleContext.get(), GTK_STYLE_CLASS_BACKGROUND);
    GtkWidgetPath* path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
    gtk_style_context_set_path(m_styleContext.get(), path);
    gtk_widget_path_free(path);

    g_signal_connect(m_dialog, "draw", G_CALLBACK(drawSignal), m_styleContext.get());
}

void WebKit2GtkAuthenticationDialog::authenticate(const WebCore::Credential& credential)
{
    RefPtr<WebCredential> webCredential = WebCredential::create(credential);
    m_authenticationChallenge->listener()->useCredential(webCredential.get());
}

} // namespace WebKit
