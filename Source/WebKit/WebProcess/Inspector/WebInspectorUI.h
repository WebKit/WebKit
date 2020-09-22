/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "DebuggableInfoData.h"
#include "WebInspectorFrontendAPIDispatcher.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/InspectorDebuggableType.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorFrontendHost.h>

namespace WebCore {
class InspectorController;
class CertificateInfo;
class FloatRect;
}

namespace WebKit {

class WebPage;

class WebInspectorUI : public RefCounted<WebInspectorUI>, private IPC::Connection::Client, public WebCore::InspectorFrontendClient {
public:
    static Ref<WebInspectorUI> create(WebPage&);

    static void enableFrontendFeatures();

    // Implemented in generated WebInspectorUIMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override { /* Do nothing, the inspected page process may have crashed and may be getting replaced. */ }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override { closeWindow(); }

    // Called by WebInspectorUI messages
    void establishConnection(WebPageProxyIdentifier inspectedPageIdentifier, const DebuggableInfoData&, bool underTest, unsigned inspectionLevel);
    void updateConnection();

    void showConsole();
    void showResources();

    void showMainResourceForFrame(const String& frameIdentifier);

    void startPageProfiling();
    void stopPageProfiling();

    void startElementSelection();
    void stopElementSelection();

    void attachedBottom() { setDockSide(DockSide::Bottom); }
    void attachedRight() { setDockSide(DockSide::Right); }
    void attachedLeft() { setDockSide(DockSide::Left); }
    void detached() { setDockSide(DockSide::Undocked); }

    void setDockSide(DockSide);
    void setDockingUnavailable(bool);

    void setIsVisible(bool);

    void updateFindString(const String&);

    void didSave(const String& url);
    void didAppend(const String& url);

    void sendMessageToFrontend(const String& message);
    void evaluateInFrontendForTesting(const String& expression);

#if ENABLE(INSPECTOR_TELEMETRY)
    void setDiagnosticLoggingAvailable(bool);
#endif

    // WebCore::InspectorFrontendClient
    void windowObjectCleared() override;
    void frontendLoaded() override;

    void startWindowDrag() override;
    void moveWindowBy(float x, float y) override;

    bool isRemote() const final { return false; }
    String localizedStringsURL() const override;
    String backendCommandsURL() const final { return String(); }
    Inspector::DebuggableType debuggableType() const final { return Inspector::DebuggableType::WebPage; }
    String targetPlatformName() const override;
    String targetBuildVersion() const override;
    String targetProductVersion() const override;
    bool targetIsSimulator() const final { return false; }
    unsigned inspectionLevel() const override { return m_inspectionLevel; }

    void bringToFront() override;
    void closeWindow() override;
    void reopen() override;
    void resetState() override;

    void setForcedAppearance(WebCore::InspectorFrontendClient::Appearance) override;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() const override;

    bool supportsDockSide(DockSide) override;
    void requestSetDockSide(DockSide) override;
    void changeAttachedWindowHeight(unsigned) override;
    void changeAttachedWindowWidth(unsigned) override;

    void changeSheetRect(const WebCore::FloatRect&) override;

    void openURLExternally(const String& url) override;

    bool canSave() override;
    void save(const WTF::String& url, const WTF::String& content, bool base64Encoded, bool forceSaveAs) override;
    void append(const WTF::String& url, const WTF::String& content) override;

    void inspectedURLChanged(const String&) override;
    void showCertificate(const WebCore::CertificateInfo&) override;

#if ENABLE(INSPECTOR_TELEMETRY)
    bool supportsDiagnosticLogging() override;
    bool diagnosticLoggingAvailable() override { return m_diagnosticLoggingAvailable; }
    void logDiagnosticEvent(const WTF::String& eventName, const WebCore::DiagnosticLoggingClient::ValueDictionary&) override;
#endif

    void sendMessageToBackend(const String&) override;

    void pagePaused() override;
    void pageUnpaused() override;

    bool isUnderTest() override { return m_underTest; }

private:
    explicit WebInspectorUI(WebPage&);

    WebPage& m_page;
    WebInspectorFrontendAPIDispatcher m_frontendAPIDispatcher;
    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;

    // Keep a pointer to the frontend's inspector controller rather than going through
    // corePage(), since we may need it after the frontend's page has started destruction.
    WebCore::InspectorController* m_frontendController { nullptr };

    WebPageProxyIdentifier m_inspectedPageIdentifier;
    bool m_underTest { false };
    DebuggableInfoData m_debuggableInfo;
    bool m_dockingUnavailable { false };
    bool m_isVisible { false };
#if ENABLE(INSPECTOR_TELEMETRY)
    bool m_diagnosticLoggingAvailable { false };
#endif

    DockSide m_dockSide { DockSide::Undocked };
    unsigned m_inspectionLevel { 1 };
};

} // namespace WebKit
