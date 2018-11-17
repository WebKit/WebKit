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

#include "MessageReceiver.h"
#include "WebInspectorFrontendAPIDispatcher.h"
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorFrontendHost.h>
#include <wtf/Deque.h>

namespace WebCore {
class CertificateInfo;
}

namespace WebKit {

class WebPage;

class RemoteWebInspectorUI final : public RefCounted<RemoteWebInspectorUI>, public IPC::MessageReceiver, public WebCore::InspectorFrontendClient {
public:
    static Ref<RemoteWebInspectorUI> create(WebPage&);

    // Implemented in generated RemoteWebInspectorUIMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Called by RemoteWebInspectorUI messages
    void initialize(const String& debuggableType, const String& backendCommandsURL);
    void didSave(const String& url);
    void didAppend(const String& url);
    void sendMessageToFrontend(const String&);

    // WebCore::InspectorFrontendClient
    void windowObjectCleared() override;
    void frontendLoaded() override;
    void startWindowDrag() override;
    void moveWindowBy(float x, float y) override;

    String localizedStringsURL() override;
    String backendCommandsURL() override { return m_backendCommandsURL; }
    String debuggableType() override { return m_debuggableType; }

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() const override;

    void bringToFront() override;
    void closeWindow() override;

    void openInNewTab(const String& url) override;
    void save(const String& url, const String& content, bool base64Encoded, bool forceSaveAs) override;
    void append(const String& url, const String& content) override;
    void inspectedURLChanged(const String&) override;
    void showCertificate(const WebCore::CertificateInfo&) override;
    void sendMessageToBackend(const String&) override;

    bool canSave() override { return true; }
    bool isUnderTest() override { return false; }
    unsigned inspectionLevel() const override { return 1; }
    void requestSetDockSide(DockSide) override { }
    void changeAttachedWindowHeight(unsigned) override { }
    void changeAttachedWindowWidth(unsigned) override { }

private:
    explicit RemoteWebInspectorUI(WebPage&);

    WebPage& m_page;
    WebInspectorFrontendAPIDispatcher m_frontendAPIDispatcher;
    RefPtr<WebCore::InspectorFrontendHost> m_frontendHost;
    String m_debuggableType;
    String m_backendCommandsURL;
};

} // namespace WebKit
