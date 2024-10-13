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
#include "WebPageProxyIdentifier.h"
#include <WebCore/Color.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/InspectorDebuggableType.h>
#include <WebCore/InspectorFrontendAPIDispatcher.h>
#include <WebCore/InspectorFrontendClient.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "InspectorExtensionTypes.h"
#endif

namespace WebCore {
class InspectorController;
class InspectorFrontendHost;
class CertificateInfo;
class FloatRect;
}

namespace WebKit {

class WebPage;
#if ENABLE(INSPECTOR_EXTENSIONS)
class WebInspectorUIExtensionController;
#endif

class WebInspectorUI final
    : public RefCounted<WebInspectorUI>
    , private IPC::Connection::Client
    , public WebCore::InspectorFrontendClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebInspectorUI);
public:
    static Ref<WebInspectorUI> create(WebPage&);
    virtual ~WebInspectorUI();

    // Implemented in generated WebInspectorUIMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override { /* Do nothing, the inspected page process may have crashed and may be getting replaced. */ }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) override { closeWindow(); }

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
    void effectiveAppearanceDidChange(WebCore::InspectorFrontendClient::Appearance);

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() const override;

    bool supportsDockSide(DockSide) override;
    void requestSetDockSide(DockSide) override;
    void changeAttachedWindowHeight(unsigned) override;
    void changeAttachedWindowWidth(unsigned) override;

    void changeSheetRect(const WebCore::FloatRect&) override;

    void openURLExternally(const String& url) override;
    void revealFileExternally(const String& path) override;

    bool canSave(WebCore::InspectorFrontendClient::SaveMode) override;
    void save(Vector<WebCore::InspectorFrontendClient::SaveData>&&, bool forceSaveAs) override;

    bool canLoad() override;
    void load(const WTF::String& path, WTF::CompletionHandler<void(const WTF::String&)>&&) override;

    bool canPickColorFromScreen() override;
    void pickColorFromScreen(WTF::CompletionHandler<void(const std::optional<WebCore::Color>&)>&&) override;

    void inspectedURLChanged(const String&) override;
    void showCertificate(const WebCore::CertificateInfo&) override;

    void setInspectorPageDeveloperExtrasEnabled(bool) override;

#if ENABLE(INSPECTOR_TELEMETRY)
    bool supportsDiagnosticLogging() override;
    bool diagnosticLoggingAvailable() override { return m_diagnosticLoggingAvailable; }
    void logDiagnosticEvent(const WTF::String& eventName, const WebCore::DiagnosticLoggingClient::ValueDictionary&) override;
#endif
        
#if ENABLE(INSPECTOR_EXTENSIONS)
    bool supportsWebExtensions() override;
    void didShowExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&, const WebCore::FrameIdentifier&) override;
    void didHideExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&) override;
    void didNavigateExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&, const URL&) override;
    void inspectedPageDidNavigate(const URL&) override;
#endif

    void sendMessageToBackend(const String&) override;
    WebCore::InspectorFrontendAPIDispatcher& frontendAPIDispatcher() final { return m_frontendAPIDispatcher; }
    WebCore::Page* frontendPage() final;
        
    void pagePaused() override;
    void pageUnpaused() override;

    bool isUnderTest() override { return m_underTest; }

private:
    explicit WebInspectorUI(WebPage&);

    void didEstablishConnection();

    WeakRef<WebPage> m_page;
    Ref<WebCore::InspectorFrontendAPIDispatcher> m_frontendAPIDispatcher;
    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;

    // Keep a pointer to the frontend's inspector controller rather than going through
    // corePage(), since we may need it after the frontend's page has started destruction.
    CheckedPtr<WebCore::InspectorController> m_frontendController;

#if ENABLE(INSPECTOR_EXTENSIONS)
    RefPtr<WebInspectorUIExtensionController> m_extensionController;
#endif

    RefPtr<IPC::Connection> m_backendConnection;

    Markable<WebPageProxyIdentifier> m_inspectedPageIdentifier;
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
