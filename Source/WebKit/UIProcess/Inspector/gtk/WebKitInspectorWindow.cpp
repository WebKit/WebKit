/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitInspectorWindow.h"

#include "WebInspectorProxy.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebKit;

struct _WebKitInspectorWindow {
    GtkWindow parent;

    GtkWidget* headerBar;
};

struct _WebKitInspectorWindowClass {
    GtkWindowClass parent;
};

G_DEFINE_TYPE(WebKitInspectorWindow, webkit_inspector_window, GTK_TYPE_WINDOW)

static void webkit_inspector_window_class_init(WebKitInspectorWindowClass*)
{
}

static void webkit_inspector_window_init(WebKitInspectorWindow* window)
{
    window->headerBar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(window->headerBar), _("Web Inspector"));
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(window->headerBar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), window->headerBar);
    gtk_widget_show(window->headerBar);
}

GtkWidget* webkitInspectorWindowNew(GtkWindow* parent)
{
    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_INSPECTOR_WINDOW, "type", GTK_WINDOW_TOPLEVEL, "transient-for", parent,
        "default-width", WebInspectorProxy::initialWindowWidth, "default-height", WebInspectorProxy::initialWindowHeight, nullptr));
}

void webkitInspectorWindowSetSubtitle(WebKitInspectorWindow* window, const char* subtitle)
{
    g_return_if_fail(WEBKIT_IS_INSPECTOR_WINDOW(window));

    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(window->headerBar), subtitle);
}
