/*
 * Copyright (C) 2008 Gustavo Noronha Silva
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "InspectorClientGtk.h"

#include "webkitwebview.h"
#include "webkitwebinspector.h"
#include "webkitprivate.h"
#include "InspectorController.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

static void notifyWebViewDestroyed(WebKitWebView* webView, InspectorFrontendClient* inspectorFrontendClient)
{
    inspectorFrontendClient->destroyInspectorWindow();
}

InspectorClient::InspectorClient(WebKitWebView* webView)
    : m_inspectedWebView(webView)
{}

void InspectorClient::inspectorDestroyed()
{
    delete this;
}

void InspectorClient::openInspectorFrontend(InspectorController* controller)
{
    // This g_object_get will ref the inspector. We're not doing an
    // unref if this method succeeds because the inspector object must
    // be alive even after the inspected WebView is destroyed - the
    // close-window and destroy signals still need to be
    // emitted.
    WebKitWebInspector* webInspector = 0;
    g_object_get(m_inspectedWebView, "web-inspector", &webInspector, NULL);
    ASSERT(webInspector);

    WebKitWebView* inspectorWebView = 0;
    g_signal_emit_by_name(webInspector, "inspect-web-view", m_inspectedWebView, &inspectorWebView);

    if (!inspectorWebView) {
        g_object_unref(webInspector);
        return;
    }

    webkit_web_inspector_set_web_view(webInspector, inspectorWebView);

    GOwnPtr<gchar> inspectorURI;

    // Make the Web Inspector work when running tests
    if (g_file_test("WebCore/inspector/front-end/inspector.html", G_FILE_TEST_EXISTS)) {
        GOwnPtr<gchar> currentDirectory(g_get_current_dir());
        GOwnPtr<gchar> fullPath(g_strdup_printf("%s/WebCore/inspector/front-end/inspector.html", currentDirectory.get()));
        inspectorURI.set(g_filename_to_uri(fullPath.get(), NULL, NULL));
    } else
        inspectorURI.set(g_filename_to_uri(DATA_DIR"/webkit-1.0/webinspector/inspector.html", NULL, NULL));

    webkit_web_view_load_uri(inspectorWebView, inspectorURI.get());

    gtk_widget_show(GTK_WIDGET(inspectorWebView));

    Page* inspectorPage = core(inspectorWebView);
    inspectorPage->inspectorController()->setInspectorFrontendClient(new InspectorFrontendClient(m_inspectedWebView, inspectorWebView, webInspector, inspectorPage));
}

void InspectorClient::highlight(Node* node)
{
    notImplemented();
}

void InspectorClient::hideHighlight()
{
    notImplemented();
}

void InspectorClient::populateSetting(const String& key, String* value)
{
    notImplemented();
}

void InspectorClient::storeSetting(const String& key, const String& value)
{
    notImplemented();
}


bool destroyed = TRUE;

InspectorFrontendClient::InspectorFrontendClient(WebKitWebView* inspectedWebView, WebKitWebView* inspectorWebView, WebKitWebInspector* webInspector, Page* inspectorPage)
    : InspectorFrontendClientLocal(core(inspectedWebView)->inspectorController(), inspectorPage)
    , m_inspectorWebView(inspectorWebView)
    , m_inspectedWebView(inspectedWebView)
    , m_webInspector(webInspector)
{
    g_signal_connect(m_inspectorWebView, "destroy",
                     G_CALLBACK(notifyWebViewDestroyed), (gpointer)this);
}

InspectorFrontendClient::~InspectorFrontendClient()
{
    ASSERT(!m_webInspector);
}

void InspectorFrontendClient::destroyInspectorWindow()
{
    if (!m_webInspector)
        return;
    WebKitWebInspector* webInspector = m_webInspector;
    m_webInspector = 0;

    g_signal_handlers_disconnect_by_func(m_inspectorWebView, (gpointer)notifyWebViewDestroyed, (gpointer)this);
    m_inspectorWebView = 0;

    core(m_inspectedWebView)->inspectorController()->disconnectFrontend();

    gboolean handled = FALSE;
    g_signal_emit_by_name(webInspector, "close-window", &handled);
    ASSERT(handled);

    /* we should now dispose our own reference */
    g_object_unref(webInspector);
}

String InspectorFrontendClient::localizedStringsURL()
{
    GOwnPtr<gchar> URL;

    // Make the Web Inspector work when running tests
    if (g_file_test("WebCore/English.lproj/localizedStrings.js", G_FILE_TEST_EXISTS)) {
        GOwnPtr<gchar> currentDirectory(g_get_current_dir());
        GOwnPtr<gchar> fullPath(g_strdup_printf("%s/WebCore/English.lproj/localizedStrings.js", currentDirectory.get()));
        URL.set(g_filename_to_uri(fullPath.get(), NULL, NULL));
    } else
        URL.set(g_filename_to_uri(DATA_DIR"/webkit-1.0/webinspector/localizedStrings.js", NULL, NULL));

    // FIXME: support l10n of localizedStrings.js
    return String::fromUTF8(URL.get());
}

String InspectorFrontendClient::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorFrontendClient::bringToFront()
{
    if (!m_inspectorWebView)
        return;

    gboolean handled = FALSE;
    g_signal_emit_by_name(m_webInspector, "show-window", &handled);
}

void InspectorFrontendClient::closeWindow()
{
    destroyInspectorWindow();
}

void InspectorFrontendClient::attachWindow()
{
    if (!m_inspectorWebView)
        return;

    gboolean handled = FALSE;
    g_signal_emit_by_name(m_webInspector, "attach-window", &handled);
}

void InspectorFrontendClient::detachWindow()
{
    if (!m_inspectorWebView)
        return;

    gboolean handled = FALSE;
    g_signal_emit_by_name(m_webInspector, "detach-window", &handled);
}

void InspectorFrontendClient::setAttachedWindowHeight(unsigned height)
{
    notImplemented();
}

void InspectorFrontendClient::inspectedURLChanged(const String& newURL)
{
    if (!m_inspectorWebView)
        return;

    webkit_web_inspector_set_inspected_uri(m_webInspector, newURL.utf8().data());
}

}

