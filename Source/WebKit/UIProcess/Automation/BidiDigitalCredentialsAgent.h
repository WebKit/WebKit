/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebDriverBidiProtocolObjects.h"
#include <WebCore/DigitalCredentialPresentationProtocol.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
struct DigitalCredentialsResponseData;
struct ExceptionData;
}

namespace WebKit {

class WebAutomationSession;

struct VirtualWalletBehavior {
    Inspector::Protocol::BidiDigitalCredentials::VirtualWalletAction action;
    WebCore::DigitalCredentialPresentationProtocol protocol { WebCore::DigitalCredentialPresentationProtocol::OrgIsoMdoc };
    String responseJSON;
};

using DigitalCredentialsPickerCompletionHandler = CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>;

class BidiDigitalCredentialsAgent final : public Inspector::BidiDigitalCredentialsBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiDigitalCredentialsAgent);
public:
    BidiDigitalCredentialsAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiDigitalCredentialsAgent() override;

    void setVirtualWalletBehavior(Inspector::Protocol::BidiDigitalCredentials::VirtualWalletAction, const String& optionalContext, std::optional<Inspector::Protocol::BidiDigitalCredentials::DigitalCredentialProtocol>&& optionalProtocol, RefPtr<JSON::Object>&& optionalResponse, Inspector::CommandCallback<void>&&) override;

    std::optional<VirtualWalletBehavior> behaviorForContext(const String& browsingContextID) const;

    void holdPendingHandler(const String& contextID, DigitalCredentialsPickerCompletionHandler&&);
    void releasePendingHandler(const String& contextID);
    void cancelAllPendingRequests();

private:
    void abortPendingHandler(const String& contextID, ASCIILiteral message);

    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiDigitalCredentialsBackendDispatcher> m_digitalCredentialsDomainDispatcher;
    HashMap<String, VirtualWalletBehavior> m_browsingContextToWalletBehaviors;
    std::optional<VirtualWalletBehavior> m_defaultBehavior;
    HashMap<String, DigitalCredentialsPickerCompletionHandler> m_browsingContextToPendingWaitHandlers;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
