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

#include "DebuggableInfoData.h"
#include "MessageReceiver.h"
#include <WebCore/InspectorFrontendAPIDispatcher.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorFrontendHost.h>
#include <wtf/Deque.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "InspectorExtensionTypes.h"
#endif

namespace WebCore {
class CertificateInfo;
class FloatRect;
}

namespace WebKit {

class WebPage;
#if ENABLE(INSPECTOR_EXTENSIONS)
class WebInspectorUIExtensionController;
#endif

class RemoteWebInspectorUI final
    : public RefCounted<RemoteWebInspectorUI>
    , public IPC::MessageReceiver
    , public WebCore::InspectorFrontendClient {
public:
    static Ref<RemoteWebInspectorUI> create(WebPage&);
    ~RemoteWebInspectorUI();

    // Implemented in generated RemoteWebInspectorUIMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by RemoteWebInspectorUI messages
    void initialize(DebuggableInfoData&&, const String& backendCommandsURL);
    void updateFindString(const String&);
    void didSave(const String& url);
    void didAppend(const String& url);
    void sendMessageToFrontend(const String&);

#if ENABLE(INSPECTOR_TELEMETRY)
    void setDiagnosticLoggingAvailable(bool);
#endif

    // WebCore::InspectorFrontendClient
    void windowObjectCleared() override;
    void frontendLoaded() override;

    void pagePaused() override;
    void pageUnpaused() override;

    void changeSheetRect(const WebCore::FloatRect&) override;
    void startWindowDrag() override;
    void moveWindowBy(float x, float y) override;

    bool isRemote() const final { return true; }
    String localizedStringsURL() const override;
    String backendCommandsURL() const final { return m_backendCommandsURL; }
    Inspector::DebuggableType debuggableType() const override;
    String targetPlatformName() const override;
    String targetBuildVersion() const override;
    String targetProductVersion() const override;
    bool targetIsSimulator() const override;

    void setForcedAppearance(WebCore::InspectorFrontendClient::Appearance) override;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() const override;

    bool supportsDockSide(DockSide) override;

    void bringToFront() override;
    void closeWindow() override;
    void reopen() override;
    void resetState() override;

    void openURLExternally(const String& url) override;
    void save(const String& url, const String& content, bool base64Encoded, bool forceSaveAs) override;
    void append(const String& url, const String& content) override;
    void inspectedURLChanged(const String&) override;
    void showCertificate(const WebCore::CertificateInfo&) override;
    void sendMessageToBackend(const String&) override;
    WebCore::InspectorFrontendAPIDispatcher& frontendAPIDispatcher() override { return m_frontendAPIDispatcher; }
    WebCore::Page* frontendPage() final;

#if ENABLE(INSPECTOR_TELEMETRY)
    bool supportsDiagnosticLogging() override;
    bool diagnosticLoggingAvailable() override { return m_diagnosticLoggingAvailable; }
    void logDiagnosticEvent(const String& eventName, const WebCore::DiagnosticLoggingClient::ValueDictionary&) override;
#endif
        
#if ENABLE(INSPECTOR_EXTENSIONS)
    bool supportsWebExtensions() override;
    void didShowExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&) override;
    void didHideExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&) override;
#endif

    bool canSave() override { return true; }
    bool isUnderTest() override { return false; }
    unsigned inspectionLevel() const override { return 1; }
    void requestSetDockSide(DockSide) override { }
    void changeAttachedWindowHeight(unsigned) override { }
    void changeAttachedWindowWidth(unsigned) override { }

private:
    explicit RemoteWebInspectorUI(WebPage&);

    WebPage& m_page;
    Ref<WebCore::InspectorFrontendAPIDispatcher> m_frontendAPIDispatcher;
    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;
#if ENABLE(INSPECTOR_EXTENSIONS)
    std::unique_ptr<WebInspectorUIExtensionController> m_extensionController;
#endif

    DebuggableInfoData m_debuggableInfo;
    String m_backendCommandsURL;

#if ENABLE(INSPECTOR_TELEMETRY)
    bool m_diagnosticLoggingAvailable { false };
#endif
};

} // namespace WebKit
