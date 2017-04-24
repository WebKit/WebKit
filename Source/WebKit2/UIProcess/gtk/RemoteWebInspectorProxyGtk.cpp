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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
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
#include "RemoteWebInspectorProxy.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "WebInspectorProxy.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageGroup.h"
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

void RemoteWebInspectorProxy::updateWindowTitle(const CString& targetName)
{
    if (!m_window)
        return;

#if GTK_CHECK_VERSION(3, 10, 0)
    GtkHeaderBar* headerBar = GTK_HEADER_BAR(gtk_window_get_titlebar(GTK_WINDOW(m_window)));
    gtk_header_bar_set_title(headerBar, _("Web Inspector"));
    gtk_header_bar_set_subtitle(headerBar, targetName.data());
#else
    GUniquePtr<gchar> title(g_strdup_printf("%s - %s", _("Web Inspector"), targetName.data()));
    gtk_window_set_title(GTK_WINDOW(m_inspectorWindow), title.get());
#endif
}

static void inspectorViewDestroyed(RemoteWebInspectorProxy* inspectorProxy)
{
    inspectorProxy->closeFromCrash();
}

WebPageProxy* RemoteWebInspectorProxy::platformCreateFrontendPageAndWindow()
{
    ASSERT(!m_webView);

    RefPtr<WebPreferences> preferences = WebPreferences::create(String(), "WebKit2.", "WebKit2.");
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif
    RefPtr<WebPageGroup> pageGroup = WebPageGroup::create(inspectorPageGroupIdentifierForPage(nullptr), false, false);

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&inspectorProcessPool(inspectorLevelForPage(nullptr)));
    pageConfiguration->setPreferences(preferences.get());
    pageConfiguration->setPageGroup(pageGroup.get());
    m_webView = GTK_WIDGET(webkitWebViewBaseCreate(*pageConfiguration.ptr()));
    g_signal_connect_swapped(m_webView, "destroy", G_CALLBACK(inspectorViewDestroyed), this);
    g_object_add_weak_pointer(G_OBJECT(m_webView), reinterpret_cast<void**>(&m_webView));

    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

#if GTK_CHECK_VERSION(3, 10, 0)
    GtkWidget* headerBar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerBar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(m_window), headerBar);
    gtk_widget_show(headerBar);
#endif

    gtk_window_set_default_size(GTK_WINDOW(m_window), WebInspectorProxy::initialWindowWidth, WebInspectorProxy::initialWindowHeight);

    gtk_container_add(GTK_CONTAINER(m_window), m_webView);
    gtk_widget_show(m_webView);

    g_object_add_weak_pointer(G_OBJECT(m_window), reinterpret_cast<void**>(&m_window));
    gtk_window_present(GTK_WINDOW(m_window));

    return webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView));
}

void RemoteWebInspectorProxy::platformCloseFrontendPageAndWindow()
{
    if (m_webView) {
        if (auto* webPage = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView)))
            webPage->close();
    }
    if (m_window)
        gtk_widget_destroy(m_window);
}

void RemoteWebInspectorProxy::platformBringToFront()
{
    if (m_window)
        gtk_window_present(GTK_WINDOW(m_window));
}

void RemoteWebInspectorProxy::platformSave(const String&, const String&, bool, bool)
{
}

void RemoteWebInspectorProxy::platformAppend(const String&, const String&)
{
}

void RemoteWebInspectorProxy::platformStartWindowDrag()
{
}

void RemoteWebInspectorProxy::platformOpenInNewTab(const String&)
{
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
