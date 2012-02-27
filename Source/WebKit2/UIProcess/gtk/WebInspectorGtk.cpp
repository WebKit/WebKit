/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include <WebCore/NotImplemented.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static const char* inspectorFilesBasePath()
{
    const gchar* environmentPath = g_getenv("WEBKIT_INSPECTOR_PATH");
    if (environmentPath && g_file_test(environmentPath, G_FILE_TEST_IS_DIR))
        return environmentPath;

    static const char* inspectorFilesPath = DATA_DIR""G_DIR_SEPARATOR_S
                                            "webkitgtk-"WEBKITGTK_API_VERSION_STRING""G_DIR_SEPARATOR_S
                                            "webinspector"G_DIR_SEPARATOR_S;
    return inspectorFilesPath;
}

static gboolean inspectorWindowDestroyed(GtkWidget* window, GdkEvent*, gpointer userData)
{
    WebInspectorProxy* inspectorProxy = static_cast<WebInspectorProxy*>(userData);

    // Inform WebProcess about webinspector closure. Not doing so,
    // results in failure of subsequent invocation of webinspector.
    inspectorProxy->close();
    inspectorProxy->windowDestroyed();

    return FALSE;
}

void WebInspectorProxy::windowDestroyed()
{
    ASSERT(m_inspectorView);
    ASSERT(m_inspectorWindow);
    m_inspectorView = 0;
    m_inspectorWindow = 0;
}

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(m_page);
    ASSERT(!m_inspectorView);
    m_inspectorView = GTK_WIDGET(webkitWebViewBaseCreate(page()->process()->context(), inspectorPageGroup()));
    return webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_inspectorView));
}

void WebInspectorProxy::platformOpen()
{
    ASSERT(!m_inspectorWindow);
    m_inspectorWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(m_inspectorWindow), _("Web Inspector"));
    gtk_window_set_default_size(GTK_WINDOW(m_inspectorWindow), initialWindowWidth, initialWindowHeight);
    g_signal_connect(m_inspectorWindow, "delete-event", G_CALLBACK(inspectorWindowDestroyed), this);

    gtk_container_add(GTK_CONTAINER(m_inspectorWindow), m_inspectorView);
    gtk_widget_show(m_inspectorView);
    gtk_widget_show(m_inspectorWindow);
}

void WebInspectorProxy::platformDidClose()
{
    if (m_inspectorWindow) {
        gtk_widget_destroy(m_inspectorWindow);
        m_inspectorWindow = 0;
        m_inspectorView = 0;
    }
}

void WebInspectorProxy::platformBringToFront()
{
    notImplemented();
}

bool WebInspectorProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorProxy::platformInspectedURLChanged(const String& url)
{
    GOwnPtr<gchar> title(g_strdup_printf("%s - %s", _("Web Inspector"), url.utf8().data()));
    gtk_window_set_title(GTK_WINDOW(m_inspectorWindow), title.get());
}

String WebInspectorProxy::inspectorPageURL() const
{
    GOwnPtr<gchar> filePath(g_build_filename(inspectorFilesBasePath(), "inspector.html", NULL));
    GOwnPtr<gchar> fileURI(g_filename_to_uri(filePath.get(), 0, 0));
    return WebCore::filenameToString(fileURI.get());
}

String WebInspectorProxy::inspectorBaseURL() const
{
    GOwnPtr<gchar> fileURI(g_filename_to_uri(inspectorFilesBasePath(), 0, 0));
    return WebCore::filenameToString(fileURI.get());
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    notImplemented();
    return 0;
}

void WebInspectorProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
