/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#ifndef WebInspectorUI_h
#define WebInspectorUI_h

#include "Connection.h"
#include "WebInspectorFrontendAPIDispatcher.h"
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorFrontendHost.h>

namespace WebCore {
class InspectorController;
}

namespace WebKit {

class WebPage;

class WebInspectorUI : public RefCounted<WebInspectorUI>, public IPC::Connection::Client, public WebCore::InspectorFrontendClient {
public:
    static Ref<WebInspectorUI> create(WebPage&);

    // Implemented in generated WebInspectorUIMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override { closeWindow(); }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference) override { closeWindow(); }
    IPC::ProcessType localProcessType() override { return IPC::ProcessType::Web; }
    IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::Web; }

    // Called by WebInspectorUI messages
    void establishConnection(IPC::Attachment connectionIdentifier, uint64_t inspectedPageIdentifier, bool underTest, unsigned inspectionLevel);

    void showConsole();
    void showResources();

    void showMainResourceForFrame(const String& frameIdentifier);

    void startPageProfiling();
    void stopPageProfiling();

    void attachedBottom() { setDockSide(DockSide::Bottom); }
    void attachedRight() { setDockSide(DockSide::Right); }
    void detached() { setDockSide(DockSide::Undocked); }

    void setDockSide(DockSide);
    void setDockingUnavailable(bool);

    void didSave(const String& url);
    void didAppend(const String& url);

    void sendMessageToFrontend(const String&);

    // WebCore::InspectorFrontendClient
    void windowObjectCleared() override;
    void frontendLoaded() override;

    void startWindowDrag() override;
    void moveWindowBy(float x, float y) override;

    String localizedStringsURL() override;

    void bringToFront() override;
    void closeWindow() override;

    void requestSetDockSide(DockSide) override;
    void changeAttachedWindowHeight(unsigned) override;
    void changeAttachedWindowWidth(unsigned) override;

    void openInNewTab(const String& url) override;

    bool canSave() override;
    void save(const WTF::String& url, const WTF::String& content, bool base64Encoded, bool forceSaveAs) override;
    void append(const WTF::String& url, const WTF::String& content) override;

    void inspectedURLChanged(const String&) override;

    void sendMessageToBackend(const String&) override;

    bool isUnderTest() override { return m_underTest; }
    unsigned inspectionLevel() const override { return m_inspectionLevel; }

private:
    explicit WebInspectorUI(WebPage&);

    WebPage& m_page;
    WebInspectorFrontendAPIDispatcher m_frontendAPIDispatcher;
    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;
    RefPtr<IPC::Connection> m_backendConnection;

    // Keep a pointer to the frontend's inspector controller rather than going through
    // corePage(), since we may need it after the frontend's page has started destruction.
    WebCore::InspectorController* m_frontendController { nullptr };

    uint64_t m_inspectedPageIdentifier { 0 };
    bool m_underTest { false };
    bool m_dockingUnavailable { false };
    DockSide m_dockSide { DockSide::Undocked };
    unsigned m_inspectionLevel { 1 };

#if PLATFORM(COCOA)
    mutable String m_localizedStringsURL;
    mutable bool m_hasLocalizedStringsURL { false };
#endif
};

} // namespace WebKit

#endif // WebInspectorUI_h
