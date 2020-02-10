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

#include "RemoteWebInspectorUIMessages.h"
#include "WebInspectorProxy.h"
#include "WebKitInspectorWindow.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageGroup.h"
#include <WebCore/CertificateInfo.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using namespace WebCore;

void RemoteWebInspectorProxy::updateWindowTitle(const CString& targetName)
{
    if (!m_window)
        return;
    webkitInspectorWindowSetSubtitle(WEBKIT_INSPECTOR_WINDOW(m_window), !targetName.isNull() ? targetName.data() : nullptr);
}

static void remoteInspectorViewDestroyed(RemoteWebInspectorProxy* inspectorProxy)
{
    inspectorProxy->closeFromCrash();
}

WebPageProxy* RemoteWebInspectorProxy::platformCreateFrontendPageAndWindow()
{
    ASSERT(!m_webView);

    auto preferences = WebPreferences::create(String(), "WebKit2.", "WebKit2.");
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif
    auto pageGroup = WebPageGroup::create(inspectorPageGroupIdentifierForPage(nullptr));

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&inspectorProcessPool(inspectorLevelForPage(nullptr)));
    pageConfiguration->setPreferences(preferences.ptr());
    pageConfiguration->setPageGroup(pageGroup.ptr());
    m_webView = GTK_WIDGET(webkitWebViewBaseCreate(*pageConfiguration.ptr()));
    g_signal_connect_swapped(m_webView, "destroy", G_CALLBACK(remoteInspectorViewDestroyed), this);
    g_object_add_weak_pointer(G_OBJECT(m_webView), reinterpret_cast<void**>(&m_webView));

    m_window = webkitInspectorWindowNew();
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

void RemoteWebInspectorProxy::platformResetState()
{
}

void RemoteWebInspectorProxy::platformBringToFront()
{
    if (m_window)
        gtk_window_present(GTK_WINDOW(m_window));
}

static void remoteFileReplaceContentsCallback(GObject* sourceObject, GAsyncResult* result, gpointer userData)
{
    GFile* file = G_FILE(sourceObject);
    if (!g_file_replace_contents_finish(file, result, nullptr, nullptr))
        return;

    auto* page = static_cast<WebPageProxy*>(userData);
    GUniquePtr<char> path(g_file_get_path(file));
    page->process().send(Messages::RemoteWebInspectorUI::DidSave(path.get()), page->webPageID());
}

void RemoteWebInspectorProxy::platformSave(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    UNUSED_PARAM(forceSaveDialog);

    GRefPtr<GtkFileChooserNative> dialog = adoptGRef(gtk_file_chooser_native_new("Save File",
        GTK_WINDOW(m_window), GTK_FILE_CHOOSER_ACTION_SAVE, "Save", "Cancel"));

    GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog.get());
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    // Some inspector views (Audits for instance) use a custom URI scheme, such
    // as web-inspector. So we can't rely on the URL being a valid file:/// URL
    // unfortunately.
    URL url(URL(), suggestedURL);
    // Strip leading / character.
    gtk_file_chooser_set_current_name(chooser, url.path().substring(1).utf8().data());

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog.get())) != GTK_RESPONSE_ACCEPT)
        return;

    Vector<char> dataVector;
    CString dataString;
    if (base64Encoded) {
        if (!base64Decode(content, dataVector, Base64ValidatePadding))
            return;
        dataVector.shrinkToFit();
    } else
        dataString = content.utf8();

    const char* data = !dataString.isNull() ? dataString.data() : dataVector.data();
    size_t dataLength = !dataString.isNull() ? dataString.length() : dataVector.size();
    GRefPtr<GFile> file = adoptGRef(gtk_file_chooser_get_file(chooser));
    GUniquePtr<char> path(g_file_get_path(file.get()));
    g_file_replace_contents_async(file.get(), data, dataLength, nullptr, false,
        G_FILE_CREATE_REPLACE_DESTINATION, nullptr, remoteFileReplaceContentsCallback, m_inspectorPage);
}

void RemoteWebInspectorProxy::platformAppend(const String&, const String&)
{
}

void RemoteWebInspectorProxy::platformSetSheetRect(const FloatRect&)
{
}

void RemoteWebInspectorProxy::platformStartWindowDrag()
{
}

void RemoteWebInspectorProxy::platformOpenInNewTab(const String&)
{
}

void RemoteWebInspectorProxy::platformShowCertificate(const CertificateInfo&)
{
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
