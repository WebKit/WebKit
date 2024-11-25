/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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

#include "WebProcessPool.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "APIAutomationClient.h"
#include "APIAutomationSessionClient.h"
#include "APIUIClient.h"
#include "WebView.h"
#include <JavaScriptCore/RemoteInspectorServer.h>
#endif

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)

class AutomationSessionClient final : public API::AutomationSessionClient {
public:
    explicit AutomationSessionClient(const String&);

    String sessionIdentifier() const override { return m_sessionIdentifier; }

    void requestNewPageWithOptions(WebKit::WebAutomationSession&, API::AutomationSessionBrowsingContextOptions, CompletionHandler<void(WebKit::WebPageProxy*)>&&) override;
    void didDisconnectFromRemote(WebKit::WebAutomationSession&) override;

    void retainWebView(Ref<WebView>&&);
    void releaseWebView(WebPageProxy*);

private:
    String m_sessionIdentifier;

    static void close(WKPageRef, const void*);
    HashSet<Ref<WebView>> m_webViews;
};

class AutomationClient final : public API::AutomationClient, Inspector::RemoteInspector::Client {
public:
    explicit AutomationClient(WebProcessPool&);
    ~AutomationClient();

private:
    // API::AutomationClient
    bool allowsRemoteAutomation(WebKit::WebProcessPool*) final { return remoteAutomationAllowed(); }
    void didRequestAutomationSession(WebKit::WebProcessPool*, const String& sessionIdentifier) final { }

    // RemoteInspector::Client
    bool remoteAutomationAllowed() const override { return true; }

    // FIXME: should use valid value
    String browserName() const override { return "MiniBrowser"_s; }
    String browserVersion() const override { return "1.0"_s; }

    RefPtr<WebProcessPool> protectedProcessPool() const;

    void requestAutomationSession(const String&, const Inspector::RemoteInspector::Client::SessionCapabilities&) override;
    void closeAutomationSession() override;

    WeakPtr<WebProcessPool> m_processPool;
};

#endif

} // namespace WebKit
