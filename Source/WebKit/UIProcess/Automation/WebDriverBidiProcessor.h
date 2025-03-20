/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBDRIVER_BIDI)

#include "WebDriverBidiBackendDispatchers.h"
#include "WebDriverBidiFrontendDispatchers.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebAutomationSession;

class WebDriverBidiProcessor final
    : public Inspector::FrontendChannel
    , public Inspector::BidiBrowserBackendDispatcherHandler
    , public Inspector::BidiBrowsingContextBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(WebDriverBidiProcessor);
public:
    explicit WebDriverBidiProcessor(WebAutomationSession&);
    ~WebDriverBidiProcessor() override;

    void processBidiMessage(const String&);
    void sendBidiMessage(const String&);

    // Inspector::FrontendChannel methods. Domain events sent via WebDriverBidi domain notifiers are packaged up
    // by FrontendRouter and are then sent back out-of-process via WebAutomationSession::sendBidiMessage().
    Inspector::FrontendChannel::ConnectionType connectionType() const override { return Inspector::FrontendChannel::ConnectionType::Local; };
    void sendMessageToFrontend(const String&) override;

    // Inspector::BidiBrowsingContextDispatcherHandler methods.
    void navigate(const Inspector::Protocol::BidiBrowsingContext::BrowsingContext&, const String& url, std::optional<Inspector::Protocol::BidiBrowsingContext::ReadinessState>&&, Inspector::CommandCallbackOf<String, String>&&) override;

    // Inspector::BidiBrowserBackendDispatcherHandler methods.
    Inspector::Protocol::ErrorStringOr<void> close() override;

    // Event entry points called from the owning WebAutomationSession.
    void logEntryAdded(const String& level, const String& source, const String& message, double timestamp, const String& type, const String& method);

private:
    Ref<Inspector::FrontendRouter> protectedFrontendRouter() const;
    Ref<Inspector::BackendDispatcher> protectedBackendDispatcher() const;

    WeakPtr<WebAutomationSession> m_session;

    Ref<Inspector::FrontendRouter> m_frontendRouter;
    Ref<Inspector::BackendDispatcher> m_backendDispatcher;

    Ref<Inspector::BidiBrowserBackendDispatcher> m_browserDomainDispatcher;
    Ref<Inspector::BidiBrowsingContextBackendDispatcher> m_browsingContextDomainDispatcher;

    std::unique_ptr<Inspector::BidiLogFrontendDispatcher> m_logDomainNotifier;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
