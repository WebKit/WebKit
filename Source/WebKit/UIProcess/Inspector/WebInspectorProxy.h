/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "Attachment.h"
#include "DebuggableInfoData.h"
#include "MessageReceiver.h"
#include "WebInspectorUtilities.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <WebCore/FloatRect.h>
#include <WebCore/InspectorClient.h>
#include <WebCore/InspectorFrontendClient.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#include "WKGeometry.h"
#include <WebCore/IntRect.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS NSURL;
OBJC_CLASS NSView;
OBJC_CLASS NSWindow;
OBJC_CLASS WKWebInspectorProxyObjCAdapter;
OBJC_CLASS WKInspectorViewController;
#elif PLATFORM(WIN)
#include "WebView.h"
#endif

namespace WebCore {
class CertificateInfo;
}

namespace API {
class InspectorClient;
}

namespace WebKit {

class WebFrameProxy;
class WebInspectorProxyClient;
class WebPageProxy;
class WebPreferences;

enum class AttachmentSide {
    Bottom,
    Right,
    Left,
};

class WebInspectorProxy
    : public API::ObjectImpl<API::Object::Type::Inspector>
    , public IPC::MessageReceiver
    , public Inspector::FrontendChannel
#if PLATFORM(WIN)
    , public WebCore::WindowMessageListener
#endif
{
public:
    static Ref<WebInspectorProxy> create(WebPageProxy& inspectedPage)
    {
        return adoptRef(*new WebInspectorProxy(inspectedPage));
    }

    explicit WebInspectorProxy(WebPageProxy&);
    virtual ~WebInspectorProxy();

    void invalidate();

    API::InspectorClient& inspectorClient() { return *m_inspectorClient; }
    void setInspectorClient(std::unique_ptr<API::InspectorClient>&&);

    // Public APIs
    WebPageProxy* inspectedPage() const { return m_inspectedPage; }
    WebPageProxy* inspectorPage() const { return m_inspectorPage; }

    bool isConnected() const { return !!m_inspectorPage; }
    bool isVisible() const { return m_isVisible; }
    bool isFront();

    void connect();

    void show();
    void hide();
    void close();
    void closeForCrash();
    void reopen();
    void resetState();

    void reset();
    void updateForNewPageProcess(WebPageProxy&);

#if PLATFORM(MAC)
    enum class InspectionTargetType { Local, Remote };
    static RetainPtr<NSWindow> createFrontendWindow(NSRect savedWindowFrame, InspectionTargetType);

    void didBecomeActive();

    void updateInspectorWindowTitle() const;
    void inspectedViewFrameDidChange(CGFloat = 0);
    void windowFrameDidChange();
    void windowFullScreenDidChange();
    NSWindow *inspectorWindow() const { return m_inspectorWindow.get(); }

    void closeFrontendPage();
    void closeFrontendAfterInactivityTimerFired();

    void attachmentViewDidChange(NSView *oldView, NSView *newView);
    void attachmentWillMoveFromWindow(NSWindow *oldWindow);
    void attachmentDidMoveToWindow(NSWindow *newWindow);

    const WebCore::FloatRect& sheetRect() const { return m_sheetRect; }
#endif

#if PLATFORM(GTK)
    GtkWidget* inspectorView() const { return m_inspectorView; };
    void setClient(std::unique_ptr<WebInspectorProxyClient>&&);
#endif

    void showConsole();
    void showResources();
    void showMainResourceForFrame(WebFrameProxy*);
    void openURLExternally(const String& url);

    AttachmentSide attachmentSide() const { return m_attachmentSide; }
    bool isAttached() const { return m_isAttached; }
    void attachRight();
    void attachLeft();
    void attachBottom();
    void attach(AttachmentSide = AttachmentSide::Bottom);
    void detach();

    void setAttachedWindowHeight(unsigned);
    void setAttachedWindowWidth(unsigned);

    void setSheetRect(const WebCore::FloatRect&);

    void startWindowDrag();

    bool isProfilingPage() const { return m_isProfilingPage; }
    void togglePageProfiling();

    bool isElementSelectionActive() const { return m_elementSelectionActive; }
    void toggleElementSelection();

    bool isUnderTest() const { return m_underTest; }

    void setDiagnosticLoggingAvailable(bool);

    // Browser
    void browserExtensionsEnabled(HashMap<String, String>&&);
    void browserExtensionsDisabled(HashSet<String>&&);

    // Provided by platform WebInspectorProxy implementations.
    static String inspectorPageURL();
    static String inspectorTestPageURL();
    static String inspectorBaseURL();
    static bool isMainOrTestInspectorPage(const URL&);
    static DebuggableInfoData infoForLocalDebuggable();

    static const unsigned minimumWindowWidth;
    static const unsigned minimumWindowHeight;

    static const unsigned initialWindowWidth;
    static const unsigned initialWindowHeight;

    // Testing methods.
    void evaluateInFrontendForTesting(const String&);

private:
    void createFrontendPage();
    void closeFrontendPageAndWindow();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Inspector::FrontendChannel
    void sendMessageToFrontend(const String& message) override;
    ConnectionType connectionType() const override { return ConnectionType::Local; }

    WebPageProxy* platformCreateFrontendPage();
    void platformCreateFrontendWindow();
    void platformCloseFrontendPageAndWindow();

    void platformDidCloseForCrash();
    void platformInvalidate();
    void platformResetState();
    void platformBringToFront();
    void platformBringInspectedPageToFront();
    void platformHide();
    bool platformIsFront();
    void platformAttachAvailabilityChanged(bool);
    void platformSetForcedAppearance(WebCore::InspectorFrontendClient::Appearance);
    void platformOpenURLExternally(const String&);
    void platformInspectedURLChanged(const String&);
    void platformShowCertificate(const WebCore::CertificateInfo&);
    unsigned platformInspectedWindowHeight();
    unsigned platformInspectedWindowWidth();
    void platformAttach();
    void platformDetach();
    void platformSetAttachedWindowHeight(unsigned);
    void platformSetAttachedWindowWidth(unsigned);
    void platformSetSheetRect(const WebCore::FloatRect&);
    void platformStartWindowDrag();
    void platformSave(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs);
    void platformAppend(const String& filename, const String& content);

#if PLATFORM(MAC)
    bool platformCanAttach(bool webProcessCanAttach);
#else
    bool platformCanAttach(bool webProcessCanAttach) { return webProcessCanAttach; }
#endif

    // Called by WebInspectorProxy messages
    void openLocalInspectorFrontend(bool canAttach, bool underTest);
    void setFrontendConnection(IPC::Attachment);

    void sendMessageToBackend(const String&);
    void frontendLoaded();
    void didClose();
    void bringToFront();
    void bringInspectedPageToFront();
    void attachAvailabilityChanged(bool);
    void setForcedAppearance(WebCore::InspectorFrontendClient::Appearance);
    void inspectedURLChanged(const String&);
    void showCertificate(const WebCore::CertificateInfo&);
    void elementSelectionChanged(bool);
    void timelineRecordingChanged(bool);
    void setDeveloperPreferenceOverride(WebCore::InspectorClient::DeveloperPreference, Optional<bool>);

    void save(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs);
    void append(const String& filename, const String& content);

    bool canAttach() const { return m_canAttach; }
    bool shouldOpenAttached();

    void open();

    unsigned inspectionLevel() const;

    WebPreferences& inspectorPagePreferences() const;

#if PLATFORM(MAC)
    void applyForcedAppearance();
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    void updateInspectorWindowTitle() const;
#endif

#if PLATFORM(WIN)
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    bool registerWindowClass();
    void windowReceivedMessage(HWND, UINT, WPARAM, LPARAM) override;
#endif

    WebPageProxy* m_inspectedPage { nullptr };
    WebPageProxy* m_inspectorPage { nullptr };
    std::unique_ptr<API::InspectorClient> m_inspectorClient;

    bool m_underTest { false };
    bool m_isVisible { false };
    bool m_isAttached { false };
    bool m_canAttach { false };
    bool m_isProfilingPage { false };
    bool m_showMessageSent { false };
    bool m_ignoreFirstBringToFront { false };
    bool m_elementSelectionActive { false };
    bool m_ignoreElementSelectionChange { false };
    bool m_isActiveFrontend { false };

    AttachmentSide m_attachmentSide {AttachmentSide::Bottom};

#if PLATFORM(MAC)
    RetainPtr<WKInspectorViewController> m_inspectorViewController;
    RetainPtr<NSWindow> m_inspectorWindow;
    RetainPtr<WKWebInspectorProxyObjCAdapter> m_objCAdapter;
    HashMap<String, RetainPtr<NSURL>> m_suggestedToActualURLMap;
    RunLoop::Timer<WebInspectorProxy> m_closeFrontendAfterInactivityTimer;
    String m_urlString;
    WebCore::FloatRect m_sheetRect;
    WebCore::InspectorFrontendClient::Appearance m_frontendAppearance { WebCore::InspectorFrontendClient::Appearance::System };
    bool m_isObservingContentLayoutRect { false };
#elif PLATFORM(GTK)
    std::unique_ptr<WebInspectorProxyClient> m_client;
    GtkWidget* m_inspectorView { nullptr };
    GtkWidget* m_inspectorWindow { nullptr };
    GtkWidget* m_headerBar { nullptr };
    String m_inspectedURLString;
    bool m_isOpening { false };
#elif PLATFORM(WIN)
    HWND m_inspectedViewWindow { nullptr };
    HWND m_inspectedViewParentWindow { nullptr };
    HWND m_inspectorViewWindow { nullptr };
    HWND m_inspectorDetachWindow { nullptr };
    RefPtr<WebView> m_inspectorView;
#endif
};

} // namespace WebKit
