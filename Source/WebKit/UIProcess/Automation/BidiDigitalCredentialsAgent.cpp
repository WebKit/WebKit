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

#include "config.h"
#include "BidiDigitalCredentialsAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include <WebCore/DigitalCredentialsResponseData.h>
#include <WebCore/ExceptionData.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiDigitalCredentialsAgent);

static WebCore::DigitalCredentialPresentationProtocol toPresentationProtocol(Inspector::Protocol::BidiDigitalCredentials::DigitalCredentialProtocol protocol)
{
    switch (protocol) {
    case Inspector::Protocol::BidiDigitalCredentials::DigitalCredentialProtocol::OrgIsoMdoc:
        return WebCore::DigitalCredentialPresentationProtocol::OrgIsoMdoc;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

BidiDigitalCredentialsAgent::BidiDigitalCredentialsAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_digitalCredentialsDomainDispatcher(BidiDigitalCredentialsBackendDispatcher::create(backendDispatcher, this))
{
}

BidiDigitalCredentialsAgent::~BidiDigitalCredentialsAgent()
{
    cancelAllPendingRequests();
}

void BidiDigitalCredentialsAgent::setVirtualWalletBehavior(Inspector::Protocol::BidiDigitalCredentials::VirtualWalletAction action, const String& optionalContext, std::optional<Inspector::Protocol::BidiDigitalCredentials::DigitalCredentialProtocol>&& optionalProtocol, RefPtr<JSON::Object>&& optionalResponse, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    using VirtualWalletAction = Inspector::Protocol::BidiDigitalCredentials::VirtualWalletAction;

    auto applyBehavior = [&](VirtualWalletBehavior&& behavior) {
        if (!optionalContext.isEmpty())
            m_browsingContextToWalletBehaviors.set(optionalContext, WTF::move(behavior));
        else
            m_defaultBehavior = WTF::move(behavior);
    };

    switch (action) {
    case VirtualWalletAction::Respond: {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!optionalProtocol, InvalidParameter, "Action 'respond' requires a 'protocol' parameter."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!optionalResponse, InvalidParameter, "Action 'respond' requires a 'response' parameter."_s);

        VirtualWalletBehavior behavior { action, toPresentationProtocol(*optionalProtocol), optionalResponse->toJSONString() };
        applyBehavior(WTF::move(behavior));
        break;
    }
    case VirtualWalletAction::Decline:
    case VirtualWalletAction::Wait:
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalProtocol, InvalidParameter, "Only action 'respond' accepts a 'protocol' parameter."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalResponse, InvalidParameter, "Only action 'respond' accepts a 'response' parameter."_s);
        applyBehavior(VirtualWalletBehavior { action, WebCore::DigitalCredentialPresentationProtocol::OrgIsoMdoc, String() });
        break;
    case VirtualWalletAction::Clear:
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalProtocol, InvalidParameter, "Only action 'respond' accepts a 'protocol' parameter."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalResponse, InvalidParameter, "Only action 'respond' accepts a 'response' parameter."_s);
        if (!optionalContext.isEmpty())
            m_browsingContextToWalletBehaviors.remove(optionalContext);
        else
            m_defaultBehavior = std::nullopt;
        break;
    }

    callback({ });
}

std::optional<VirtualWalletBehavior> BidiDigitalCredentialsAgent::behaviorForContext(const String& browsingContextID) const
{
    auto it = m_browsingContextToWalletBehaviors.find(browsingContextID);
    if (it != m_browsingContextToWalletBehaviors.end())
        return it->value;
    return m_defaultBehavior;
}

void BidiDigitalCredentialsAgent::abortPendingHandler(const String& contextID, ASCIILiteral message)
{
    if (auto handler = m_browsingContextToPendingWaitHandlers.take(contextID))
        handler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, message }));
}

void BidiDigitalCredentialsAgent::holdPendingHandler(const String& contextID, DigitalCredentialsPickerCompletionHandler&& handler)
{
    abortPendingHandler(contextID, "Superseded by a new credential request."_s);
    m_browsingContextToPendingWaitHandlers.set(contextID, WTF::move(handler));
}

void BidiDigitalCredentialsAgent::releasePendingHandler(const String& contextID)
{
    abortPendingHandler(contextID, "The credential request was aborted."_s);
}

void BidiDigitalCredentialsAgent::cancelAllPendingRequests()
{
    auto handlers = std::exchange(m_browsingContextToPendingWaitHandlers, { });
    for (auto& handler : handlers.values())
        handler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Automation session ended."_s }));
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
