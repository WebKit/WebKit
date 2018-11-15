/*
 * Copyright (C) 2010, 2014, 2015 Apple Inc. All rights reserved.
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
#include "Connection.h"
#include "MessageReceiver.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPage;

class WebInspector : public API::ObjectImpl<API::Object::Type::BundleInspector>, private IPC::Connection::Client, public Inspector::FrontendChannel {
public:
    static Ref<WebInspector> create(WebPage*);

    WebPage* page() const { return m_page; }

    void updateDockingAvailability();

    void sendMessageToFrontend(const String& message) override;
    ConnectionType connectionType() const override { return ConnectionType::Local; }

    // Implemented in generated WebInspectorMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // IPC::Connection::Client
    void didClose(IPC::Connection&) override { close(); }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference, IPC::StringReference) override { close(); }

    void show();
    void close();

    void openInNewTab(const String& urlString);

    void canAttachWindow(bool& result);

    void showConsole();
    void showResources();
    void showTimelines();

    void showMainResourceForFrame(uint64_t frameIdentifier);

    void setAttached(bool attached) { m_attached = attached; }

    void evaluateScriptForTest(const String& script);

    void startPageProfiling();
    void stopPageProfiling();

    void startElementSelection();
    void stopElementSelection();
    void elementSelectionChanged(bool);

    void sendMessageToBackend(const String&);

    void disconnectFromPage() { close(); }

private:
    friend class WebInspectorClient;

    explicit WebInspector(WebPage*);
    virtual ~WebInspector();

    bool canAttachWindow();

    // Called from WebInspectorClient
    void openFrontendConnection(bool underTest);
    void closeFrontendConnection();

    void bringToFront();

    WebPage* m_page;

    RefPtr<IPC::Connection> m_frontendConnection;

    bool m_attached { false };
    bool m_previousCanAttach { false };
};

} // namespace WebKit
