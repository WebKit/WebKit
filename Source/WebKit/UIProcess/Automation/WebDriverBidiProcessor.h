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

class BidiBrowserAgent;
class BidiBrowsingContextAgent;
class BidiPermissionsAgent;
class BidiScriptAgent;
class BidiStorageAgent;
class WebAutomationSession;
class WebPageProxy;

class WebDriverBidiProcessor final : public Inspector::FrontendChannel {
    WTF_MAKE_TZONE_ALLOCATED(WebDriverBidiProcessor);
public:
    explicit WebDriverBidiProcessor(WebAutomationSession&);
    ~WebDriverBidiProcessor() override;

    void processBidiMessage(const String&);
    void sendBidiMessage(const String&);

    BidiBrowserAgent& browserAgent() const { return m_browserAgent; }

    // Inspector::FrontendChannel methods. Domain events sent via WebDriverBidi domain notifiers are packaged up
    // by FrontendRouter and are then sent back out-of-process via WebAutomationSession::sendBidiMessage().
    Inspector::FrontendChannel::ConnectionType connectionType() const override { return Inspector::FrontendChannel::ConnectionType::Local; };
    void sendMessageToFrontend(const String&) override;

    // Event entry points called from the owning WebAutomationSession.
    Inspector::BidiBrowsingContextFrontendDispatcher& browsingContextDomainNotifier() const { return m_browsingContextDomainNotifier; }
    Inspector::BidiLogFrontendDispatcher& logDomainNotifier() const { return m_logDomainNotifier; }

private:
    WeakPtr<WebAutomationSession> m_session;

    const Ref<Inspector::FrontendRouter> m_frontendRouter;
    const Ref<Inspector::BackendDispatcher> m_backendDispatcher;

    const UniqueRef<BidiBrowserAgent> m_browserAgent;
    const UniqueRef<BidiBrowsingContextAgent> m_browsingContextAgent;
    const UniqueRef<BidiPermissionsAgent> m_permissionsAgent;
    const UniqueRef<BidiScriptAgent> m_scriptAgent;
    const UniqueRef<BidiStorageAgent> m_storageAgent;
    const UniqueRef<Inspector::BidiBrowsingContextFrontendDispatcher> m_browsingContextDomainNotifier;
    const UniqueRef<Inspector::BidiLogFrontendDispatcher> m_logDomainNotifier;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
