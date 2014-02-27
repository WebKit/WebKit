/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2013 Gustavo Noronha Silva <gns@gnome.org>.
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
#include "WebInspectorProxy.h"

#if ENABLE(INSPECTOR)

#include "WebKitWebViewBasePrivate.h"
#include "WebProcessProxy.h"
#include <WebCore/FileSystem.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/NotImplemented.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static void inspectorViewDestroyed(GtkWidget*, gpointer userData)
{
    WebInspectorProxy* inspectorProxy = static_cast<WebInspectorProxy*>(userData);

    // Inform WebProcess about webinspector closure. Not doing so,
    // results in failure of subsequent invocation of webinspector.
    inspectorProxy->close();
}

void WebInspectorProxy::initializeInspectorClientGtk(const WKInspectorClientGtkBase* inspectorClient)
{
    m_client.initialize(inspectorClient);
}

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(m_page);
    ASSERT(!m_inspectorView);
    m_inspectorView = GTK_WIDGET(webkitWebViewBaseCreate(&page()->process().context(), inspectorPageGroup(), m_page));
    g_object_add_weak_pointer(G_OBJECT(m_inspectorView), reinterpret_cast<void**>(&m_inspectorView));
    return webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_inspectorView));
}

void WebInspectorProxy::createInspectorWindow()
{
    if (m_client.openWindow(this))
        return;

    ASSERT(!m_inspectorWindow);
    m_inspectorWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    GtkWidget* inspectedViewParent = gtk_widget_get_toplevel(m_page->viewWidget());
    if (WebCore::widgetIsOnscreenToplevelWindow(inspectedViewParent))
        gtk_window_set_transient_for(GTK_WINDOW(m_inspectorWindow), GTK_WINDOW(inspectedViewParent));

    gtk_window_set_title(GTK_WINDOW(m_inspectorWindow), _("Web Inspector"));
    gtk_window_set_default_size(GTK_WINDOW(m_inspectorWindow), initialWindowWidth, initialWindowHeight);

    gtk_container_add(GTK_CONTAINER(m_inspectorWindow), m_inspectorView);
    gtk_widget_show(m_inspectorView);

    g_object_add_weak_pointer(G_OBJECT(m_inspectorWindow), reinterpret_cast<void**>(&m_inspectorWindow));
    gtk_window_present(GTK_WINDOW(m_inspectorWindow));
}

void WebInspectorProxy::platformOpen()
{
    ASSERT(!m_inspectorWindow);
    ASSERT(m_inspectorView);

    if (m_isAttached)
        platformAttach();
    else
        createInspectorWindow();
    g_signal_connect(m_inspectorView, "destroy", G_CALLBACK(inspectorViewDestroyed), this);
}

void WebInspectorProxy::platformDidClose()
{
    if (m_inspectorView)
        g_signal_handlers_disconnect_by_func(m_inspectorView, reinterpret_cast<void*>(inspectorViewDestroyed), this);

    m_client.didClose(this);

    if (m_inspectorWindow) {
        gtk_widget_destroy(m_inspectorWindow);
        m_inspectorWindow = 0;
    }
    m_inspectorView = 0;
}

void WebInspectorProxy::platformHide()
{
    notImplemented();
}

void WebInspectorProxy::platformBringToFront()
{
    if (m_client.bringToFront(this))
        return;

    GtkWidget* parent = gtk_widget_get_toplevel(m_inspectorView);
    if (WebCore::widgetIsOnscreenToplevelWindow(parent))
        gtk_window_present(GTK_WINDOW(parent));
}

bool WebInspectorProxy::platformIsFront()
{
    GtkWidget* parent = gtk_widget_get_toplevel(m_inspectorView);
    if (WebCore::widgetIsOnscreenToplevelWindow(parent))
        return m_isVisible && gtk_window_is_active(GTK_WINDOW(parent));
    return false;
}

void WebInspectorProxy::platformInspectedURLChanged(const String& url)
{
    m_client.inspectedURLChanged(this, url);

    if (!m_inspectorWindow)
        return;
    GUniquePtr<gchar> title(g_strdup_printf("%s - %s", _("Web Inspector"), url.utf8().data()));
    gtk_window_set_title(GTK_WINDOW(m_inspectorWindow), title.get());
}

String WebInspectorProxy::inspectorPageURL() const
{
    return String("resource:///org/webkitgtk/inspector/UserInterface/Main.html");
}

String WebInspectorProxy::inspectorTestPageURL() const
{
    return String("resource:///org/webkitgtk/inspector/UserInterface/Test.html");
}

String WebInspectorProxy::inspectorBaseURL() const
{
    return String("resource:///org/webkitgtk/inspector/UserInterface/");
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    return gtk_widget_get_allocated_height(m_page->viewWidget());
}

unsigned WebInspectorProxy::platformInspectedWindowWidth()
{
    return gtk_widget_get_allocated_width(m_page->viewWidget());
}

void WebInspectorProxy::platformAttach()
{
    GRefPtr<GtkWidget> inspectorView = m_inspectorView;
    if (m_inspectorWindow) {
        gtk_container_remove(GTK_CONTAINER(m_inspectorWindow), m_inspectorView);
        gtk_widget_destroy(m_inspectorWindow);
        m_inspectorWindow = 0;
    }

    // Set a default attached size based on InspectorFrontendClientLocal.
    static const unsigned defaultAttachedSize = 300;
    if (m_attachmentSide == AttachmentSideBottom) {
        unsigned maximumAttachedHeight = platformInspectedWindowHeight() * 3 / 4;
        platformSetAttachedWindowHeight(std::max(minimumAttachedHeight, std::min(defaultAttachedSize, maximumAttachedHeight)));
    } else {
        unsigned maximumAttachedWidth = platformInspectedWindowWidth() * 3 / 4;
        platformSetAttachedWindowWidth(std::max(minimumAttachedWidth, std::min(defaultAttachedSize, maximumAttachedWidth)));
    }

    if (m_client.attach(this))
        return;

    webkitWebViewBaseAddWebInspector(WEBKIT_WEB_VIEW_BASE(m_page->viewWidget()), m_inspectorView, m_attachmentSide);
    gtk_widget_show(m_inspectorView);
}

void WebInspectorProxy::platformDetach()
{
    if (!m_page->isValid())
        return;

    GRefPtr<GtkWidget> inspectorView = m_inspectorView;
    if (!m_client.detach(this)) {
        GtkWidget* parent = gtk_widget_get_parent(m_inspectorView);
        ASSERT(parent);
        gtk_container_remove(GTK_CONTAINER(parent), m_inspectorView);
    }

    if (!m_isVisible)
        return;

    createInspectorWindow();
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    m_client.didChangeAttachedHeight(this, height);
    webkitWebViewBaseSetInspectorViewSize(WEBKIT_WEB_VIEW_BASE(m_page->viewWidget()), height);
}

void WebInspectorProxy::platformSetAttachedWindowWidth(unsigned width)
{
    if (!m_isAttached)
        return;

    m_client.didChangeAttachedWidth(this, width);
    webkitWebViewBaseSetInspectorViewSize(WEBKIT_WEB_VIEW_BASE(m_page->viewWidget()), width);
}

void WebInspectorProxy::platformSetToolbarHeight(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformSave(const String&, const String&, bool, bool)
{
    notImplemented();
}

void WebInspectorProxy::platformAppend(const String&, const String&)
{
    notImplemented();
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
