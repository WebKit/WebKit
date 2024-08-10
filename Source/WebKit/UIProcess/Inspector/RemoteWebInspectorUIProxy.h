/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "APIObject.h"
#include "MessageReceiver.h"
#include <WebCore/Color.h>
#include <WebCore/FloatRect.h>
#include <WebCore/InspectorFrontendClient.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
OBJC_CLASS NSURL;
OBJC_CLASS NSWindow;
OBJC_CLASS WKInspectorViewController;
OBJC_CLASS WKRemoteWebInspectorUIProxyObjCAdapter;
OBJC_CLASS WKWebView;
#elif PLATFORM(GTK)
#include <wtf/glib/GWeakPtr.h>
#endif

namespace WebCore {
class CertificateInfo;
}

namespace API {
class DebuggableInfo;
class InspectorConfiguration;
}

namespace WebKit {

class RemoteWebInspectorUIProxy;
class WebPageProxy;
class WebView;
#if ENABLE(INSPECTOR_EXTENSIONS)
class WebInspectorUIExtensionControllerProxy;
#endif

class RemoteWebInspectorUIProxyClient : public CanMakeWeakPtr<RemoteWebInspectorUIProxyClient>, public CanMakeCheckedPtr<RemoteWebInspectorUIProxyClient> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteWebInspectorUIProxyClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteWebInspectorUIProxyClient);
public:
    virtual ~RemoteWebInspectorUIProxyClient() { }
    virtual void sendMessageToBackend(const String& message) = 0;
    virtual void closeFromFrontend() = 0;
    virtual Ref<API::InspectorConfiguration> configurationForRemoteInspector(RemoteWebInspectorUIProxy&) = 0;
};

class RemoteWebInspectorUIProxy : public RefCounted<RemoteWebInspectorUIProxy>, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteWebInspectorUIProxy);
public:
    static Ref<RemoteWebInspectorUIProxy> create()
    {
        return adoptRef(*new RemoteWebInspectorUIProxy());
    }

    ~RemoteWebInspectorUIProxy();

    void setClient(RemoteWebInspectorUIProxyClient* client) { m_client = client; }

    bool isUnderTest() const { return false; }

    void setDiagnosticLoggingAvailable(bool);

    void invalidate();

    void initialize(Ref<API::DebuggableInfo>&&, const String& backendCommandsURL);
    void closeFromBackend();
    void show();
    void showConsole();
    void showResources();

    void sendMessageToFrontend(const String& message);

#if ENABLE(INSPECTOR_EXTENSIONS)
    WebInspectorUIExtensionControllerProxy* extensionController() const { return m_extensionController.get(); }
#endif
    
#if PLATFORM(MAC)
    NSWindow *window() const { return m_window.get(); }
    WKWebView *webView() const;

    const WebCore::FloatRect& sheetRect() const { return m_sheetRect; }

    void didBecomeActive();
#endif

#if PLATFORM(GTK)
    void updateWindowTitle(const CString&);
#endif

#if PLATFORM(WIN)
    LRESULT sizeChange();
    LRESULT onClose();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#endif

    void closeFromCrash();

private:
    RemoteWebInspectorUIProxy();
    RefPtr<WebPageProxy> protectedInspectorPage();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // RemoteWebInspectorUIProxy messages.
    void frontendLoaded();
    void frontendDidClose();
    void reopen();
    void resetState();
    void bringToFront();
    void save(Vector<WebCore::InspectorFrontendClient::SaveData>&&, bool forceSaveAs);
    void load(const String& path, CompletionHandler<void(const String&)>&&);
    void pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&&);
    void setSheetRect(const WebCore::FloatRect&);
    void setForcedAppearance(WebCore::InspectorFrontendClient::Appearance);
    void startWindowDrag();
    void openURLExternally(const String& url);
    void revealFileExternally(const String& path);
    void showCertificate(const WebCore::CertificateInfo&);
    void setInspectorPageDeveloperExtrasEnabled(bool);
    void sendMessageToBackend(const String& message);

    void createFrontendPageAndWindow();
    void closeFrontendPageAndWindow();

    // Platform implementations.
    WebPageProxy* platformCreateFrontendPageAndWindow();
    void platformCloseFrontendPageAndWindow();
    void platformResetState();
    void platformBringToFront();
    void platformSave(Vector<WebCore::InspectorFrontendClient::SaveData>&&, bool forceSaveAs);
    void platformLoad(const String& path, CompletionHandler<void(const String&)>&&);
    void platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&&);
    void platformSetSheetRect(const WebCore::FloatRect&);
    void platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance);
    void platformStartWindowDrag();
    void platformOpenURLExternally(const String& url);
    void platformRevealFileExternally(const String& path);
    void platformShowCertificate(const WebCore::CertificateInfo&);

    WeakPtr<RemoteWebInspectorUIProxyClient> m_client;
    WeakPtr<WebPageProxy> m_inspectorPage;

#if ENABLE(INSPECTOR_EXTENSIONS)
    RefPtr<WebInspectorUIExtensionControllerProxy> m_extensionController;
#endif
    
    Ref<API::DebuggableInfo> m_debuggableInfo;
    String m_backendCommandsURL;

#if PLATFORM(MAC)
    RetainPtr<WKInspectorViewController> m_inspectorView;
    RetainPtr<NSWindow> m_window;
    RetainPtr<WKRemoteWebInspectorUIProxyObjCAdapter> m_objCAdapter;
    HashMap<String, RetainPtr<NSURL>> m_suggestedToActualURLMap;
    WebCore::FloatRect m_sheetRect;
#endif
#if PLATFORM(GTK)
    GWeakPtr<GtkWidget> m_webView;
    GWeakPtr<GtkWidget> m_window;
#endif
#if PLATFORM(WIN)
    HWND m_frontendHandle;
    RefPtr<WebView> m_webView;
#endif
};

} // namespace WebKit
