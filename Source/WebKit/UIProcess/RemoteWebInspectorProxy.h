/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <WebCore/FloatRect.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
OBJC_CLASS NSURL;
OBJC_CLASS NSWindow;
OBJC_CLASS WKInspectorViewController;
OBJC_CLASS WKRemoteWebInspectorProxyObjCAdapter;
OBJC_CLASS WKWebView;
#endif

namespace WebCore {
class CertificateInfo;
}

namespace API {
class DebuggableInfo;
}

namespace WebKit {

class WebPageProxy;
class WebView;

class RemoteWebInspectorProxyClient {
public:
    virtual ~RemoteWebInspectorProxyClient() { }
    virtual void sendMessageToBackend(const String& message) = 0;
    virtual void closeFromFrontend() = 0;
};

class RemoteWebInspectorProxy : public RefCounted<RemoteWebInspectorProxy>, public IPC::MessageReceiver {
public:
    static Ref<RemoteWebInspectorProxy> create()
    {
        return adoptRef(*new RemoteWebInspectorProxy());
    }

    ~RemoteWebInspectorProxy();

    void setClient(RemoteWebInspectorProxyClient* client) { m_client = client; }

    bool isUnderTest() const { return false; }

    void setDiagnosticLoggingAvailable(bool);

    void invalidate();

    void load(Ref<API::DebuggableInfo>&&, const String& backendCommandsURL);
    void closeFromBackend();
    void show();

    void sendMessageToFrontend(const String& message);

#if PLATFORM(MAC)
    NSWindow *window() const { return m_window.get(); }
    WKWebView *webView() const;

    const WebCore::FloatRect& sheetRect() const { return m_sheetRect; }
#endif

#if PLATFORM(GTK)
    void updateWindowTitle(const CString&);
#endif

#if PLATFORM(WIN_CAIRO)
    LRESULT sizeChange();
    LRESULT onClose();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#endif

    void closeFromCrash();

private:
    RemoteWebInspectorProxy();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // RemoteWebInspectorProxy messages.
    void frontendDidClose();
    void reopen();
    void resetState();
    void bringToFront();
    void save(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs);
    void append(const String& filename, const String& content);
    void setSheetRect(const WebCore::FloatRect&);
    void startWindowDrag();
    void openInNewTab(const String& url);
    void showCertificate(const WebCore::CertificateInfo&);
    void sendMessageToBackend(const String& message);

    void createFrontendPageAndWindow();
    void closeFrontendPageAndWindow();

    // Platform implementations.
    WebPageProxy* platformCreateFrontendPageAndWindow();
    void platformCloseFrontendPageAndWindow();
    void platformResetState();
    void platformBringToFront();
    void platformSave(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs);
    void platformAppend(const String& filename, const String& content);
    void platformSetSheetRect(const WebCore::FloatRect&);
    void platformStartWindowDrag();
    void platformOpenInNewTab(const String& url);
    void platformShowCertificate(const WebCore::CertificateInfo&);

    RemoteWebInspectorProxyClient* m_client { nullptr };
    WebPageProxy* m_inspectorPage { nullptr };

    Ref<API::DebuggableInfo> m_debuggableInfo;
    String m_backendCommandsURL;

#if PLATFORM(MAC)
    RetainPtr<WKInspectorViewController> m_inspectorView;
    RetainPtr<NSWindow> m_window;
    RetainPtr<WKRemoteWebInspectorProxyObjCAdapter> m_objCAdapter;
    HashMap<String, RetainPtr<NSURL>> m_suggestedToActualURLMap;
    WebCore::FloatRect m_sheetRect;
#endif
#if PLATFORM(GTK)
    GtkWidget* m_webView { nullptr };
    GtkWidget* m_window { nullptr };
#endif
#if PLATFORM(WIN_CAIRO)
    HWND m_frontendHandle;
    RefPtr<WebView> m_webView;
#endif
};

} // namespace WebKit
