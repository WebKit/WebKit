/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "Connection.h"
#include <WebCore/InspectorForwarding.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorFrontendHost.h>

namespace WebKit {

class WebPage;

class WebInspectorUI : public API::ObjectImpl<API::Object::Type::BundleInspectorUI>, public IPC::Connection::Client, public WebCore::InspectorFrontendClient {
public:
    static Ref<WebInspectorUI> create(WebPage*);

    WebPage* page() const { return m_page; }

    // Implemented in generated WebInspectorUIMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override { closeWindow(); }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference) override { closeWindow(); }
    virtual IPC::ProcessType localProcessType() override { return IPC::ProcessType::Web; }
    virtual IPC::ProcessType remoteProcessType() override { return IPC::ProcessType::Web; }

    // Called by WebInspectorUI messages
    void establishConnection(IPC::Attachment connectionIdentifier, uint64_t inspectedPageIdentifier, bool underTest);

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

    void setToolbarHeight(unsigned) override;

    void openInNewTab(const String& url) override;

    bool canSave() override;
    void save(const WTF::String& url, const WTF::String& content, bool base64Encoded, bool forceSaveAs) override;
    void append(const WTF::String& url, const WTF::String& content) override;

    void inspectedURLChanged(const String&) override;

    void sendMessageToBackend(const String&) override;

    bool isUnderTest() override { return m_underTest; }

private:
    explicit WebInspectorUI(WebPage*);

    void evaluateCommandOnLoad(const String& command, const String& argument = String());
    void evaluateCommandOnLoad(const String& command, const ASCIILiteral& argument) { evaluateCommandOnLoad(command, String(argument)); }
    void evaluateCommandOnLoad(const String& command, bool argument);
    void evaluateExpressionOnLoad(const String& expression);
    void evaluatePendingExpressions();

    WebPage* m_page;

    RefPtr<IPC::Connection> m_backendConnection;
    uint64_t m_inspectedPageIdentifier;

    bool m_underTest;
    bool m_frontendLoaded;
    Deque<String> m_queue;

    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;

    DockSide m_dockSide;

#if PLATFORM(COCOA)
    mutable String m_localizedStringsURL;
    mutable bool m_hasLocalizedStringsURL;
#endif
};

} // namespace WebKit

#endif // WebInspectorUI_h
