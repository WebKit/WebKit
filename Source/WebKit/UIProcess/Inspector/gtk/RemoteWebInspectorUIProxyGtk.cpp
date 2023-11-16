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
#include "RemoteWebInspectorUIProxy.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "HardwareAccelerationManager.h"
#include "RemoteWebInspectorUIMessages.h"
#include "WebInspectorUIProxy.h"
#include "WebKitInspectorWindow.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageGroup.h"
#include "WebProcessPool.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/GtkVersioning.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using namespace WebCore;

void RemoteWebInspectorUIProxy::updateWindowTitle(const CString& targetName)
{
    if (!m_window)
        return;
    webkitInspectorWindowSetSubtitle(WEBKIT_INSPECTOR_WINDOW(m_window.get()), !targetName.isNull() ? targetName.data() : nullptr);
}

static void remoteInspectorViewDestroyed(RemoteWebInspectorUIProxy* inspectorProxy)
{
    inspectorProxy->closeFromCrash();
}

WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    ASSERT(!m_webView);

    auto preferences = WebPreferences::create(String(), "WebKit2."_s, "WebKit2."_s);
#if ENABLE(DEVELOPER_MODE)
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences->setDeveloperExtrasEnabled(true);
    preferences->setLogsPageMessagesToSystemConsoleEnabled(true);
#endif

    // If hardware acceleration is available and not forced already, force it always for the remote inspector view.
    const auto& hardwareAccelerationManager = HardwareAccelerationManager::singleton();
    if (hardwareAccelerationManager.canUseHardwareAcceleration() && !hardwareAccelerationManager.forceHardwareAcceleration()) {
        preferences->setForceCompositingMode(true);
        preferences->setThreadedScrollingEnabled(true);
    }
    auto pageGroup = WebPageGroup::create(WebKit::defaultInspectorPageGroupIdentifierForPage(nullptr));

    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(&WebKit::defaultInspectorProcessPool(inspectorLevelForPage(nullptr)));
    pageConfiguration->setPreferences(preferences.ptr());
    pageConfiguration->setPageGroup(pageGroup.ptr());
    m_webView.reset(GTK_WIDGET(webkitWebViewBaseCreate(*pageConfiguration.ptr())));
    g_signal_connect_swapped(m_webView.get(), "destroy", G_CALLBACK(remoteInspectorViewDestroyed), this);

    m_window.reset(webkitInspectorWindowNew());
#if USE(GTK4)
    gtk_window_set_child(GTK_WINDOW(m_window.get()), m_webView.get());
#else
    gtk_container_add(GTK_CONTAINER(m_window.get()), m_webView.get());
    gtk_widget_show(m_webView.get());
#endif

    gtk_window_present(GTK_WINDOW(m_window.get()));

    return webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView.get()));
}

void RemoteWebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_webView) {
        if (auto* webPage = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_webView.get())))
            webPage->close();
    }
    if (m_window)
        gtk_widget_destroy(m_window.get());
}

void RemoteWebInspectorUIProxy::platformResetState()
{
}

void RemoteWebInspectorUIProxy::platformBringToFront()
{
    if (m_window)
        gtk_window_present(GTK_WINDOW(m_window.get()));
}

static void remoteFileReplaceContentsCallback(GObject* sourceObject, GAsyncResult* result, gpointer userData)
{
    GFile* file = G_FILE(sourceObject);
    g_file_replace_contents_finish(file, result, nullptr, nullptr);
}

void RemoteWebInspectorUIProxy::platformSave(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    ASSERT(saveDatas.size() == 1);
    UNUSED_PARAM(forceSaveAs);

    GRefPtr<GtkFileChooserNative> dialog = adoptGRef(gtk_file_chooser_native_new("Save File",
        GTK_WINDOW(m_window.get()), GTK_FILE_CHOOSER_ACTION_SAVE, "Save", "Cancel"));

    GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog.get());
#if !USE(GTK4)
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
#endif

    // Some inspector views (Audits for instance) use a custom URI scheme, such
    // as web-inspector. So we can't rely on the URL being a valid file:/// URL
    // unfortunately.
    URL url { saveDatas[0].url };
    // Strip leading / character.
    gtk_file_chooser_set_current_name(chooser, url.path().substring(1).utf8().data());

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog.get())) != GTK_RESPONSE_ACCEPT)
        return;

    Vector<uint8_t> dataVector;
    CString dataString;
    if (saveDatas[0].base64Encoded) {
        auto decodedData = base64Decode(saveDatas[0].content, Base64DecodeMode::DefaultValidatePadding);
        if (!decodedData)
            return;
        decodedData->shrinkToFit();
        dataVector = WTFMove(*decodedData);
    } else
        dataString = saveDatas[0].content.utf8();

    const char* data = !dataString.isNull() ? dataString.data() : reinterpret_cast<char*>(dataVector.data());
    size_t dataLength = !dataString.isNull() ? dataString.length() : dataVector.size();
    GRefPtr<GFile> file = adoptGRef(gtk_file_chooser_get_file(chooser));
    GUniquePtr<char> path(g_file_get_path(file.get()));
    g_file_replace_contents_async(file.get(), data, dataLength, nullptr, false,
        G_FILE_CREATE_REPLACE_DESTINATION, nullptr, remoteFileReplaceContentsCallback, protectedInspectorPage().get());
}

void RemoteWebInspectorUIProxy::platformLoad(const String&, CompletionHandler<void(const String&)>&& completionHandler)
{
    completionHandler(nullString());
}

void RemoteWebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    completionHandler({ });
}

void RemoteWebInspectorUIProxy::platformSetSheetRect(const FloatRect&)
{
}

void RemoteWebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance)
{
}

void RemoteWebInspectorUIProxy::platformStartWindowDrag()
{
}

void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String&)
{
}

void RemoteWebInspectorUIProxy::platformRevealFileExternally(const String&)
{
}

void RemoteWebInspectorUIProxy::platformShowCertificate(const CertificateInfo&)
{
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
