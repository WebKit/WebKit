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

#if USE(GTK4)
    GtkWidget* subtitleLabel;
#endif
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

#if USE(GTK4)
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GtkWidget* titleLabel = gtk_label_new(_("Web Inspector"));
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_CENTER);
    gtk_label_set_single_line_mode(GTK_LABEL(titleLabel), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(titleLabel), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(titleLabel, GTK_STYLE_CLASS_TITLE);
    gtk_widget_set_parent(titleLabel, box);

    window->subtitleLabel = gtk_label_new(nullptr);
    gtk_widget_set_halign(window->subtitleLabel, GTK_ALIGN_CENTER);
    gtk_label_set_single_line_mode(GTK_LABEL(window->subtitleLabel), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(window->subtitleLabel), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(window->subtitleLabel, GTK_STYLE_CLASS_SUBTITLE);
    gtk_widget_set_parent(window->subtitleLabel, box);
    gtk_widget_hide(window->subtitleLabel);

    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(window->headerBar), box);
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(window->headerBar), TRUE);
#else
    gtk_header_bar_set_title(GTK_HEADER_BAR(window->headerBar), _("Web Inspector"));
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(window->headerBar), TRUE);
    gtk_widget_show(window->headerBar);
#endif // USE(GTK4)

    gtk_window_set_titlebar(GTK_WINDOW(window), window->headerBar);
}

GtkWidget* webkitInspectorWindowNew()
{
    return GTK_WIDGET(g_object_new(WEBKIT_TYPE_INSPECTOR_WINDOW,
#if !USE(GTK4)
        "type", GTK_WINDOW_TOPLEVEL,
#endif
        "default-width", WebInspectorProxy::initialWindowWidth, "default-height", WebInspectorProxy::initialWindowHeight, nullptr));
}

void webkitInspectorWindowSetSubtitle(WebKitInspectorWindow* window, const char* subtitle)
{
    g_return_if_fail(WEBKIT_IS_INSPECTOR_WINDOW(window));

#if USE(GTK4)
    if (subtitle) {
        gtk_label_set_text(GTK_LABEL(window->subtitleLabel), subtitle);
        gtk_widget_show(window->subtitleLabel);
    } else
        gtk_widget_hide(window->subtitleLabel);
#else
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(window->headerBar), subtitle);
#endif // USE(GTK4)
}
