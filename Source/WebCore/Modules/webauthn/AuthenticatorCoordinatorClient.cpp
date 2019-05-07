/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "AuthenticatorCoordinatorClient.h"

#if ENABLE(WEB_AUTHN)

#include "PublicKeyCredentialData.h"

namespace WebCore {

void AuthenticatorCoordinatorClient::requestReply(uint64_t messageId, const WebCore::PublicKeyCredentialData& data, const WebCore::ExceptionData& exception)
{
    if (messageId != m_accumulatedRequestMessageId - 1)
        return;
    m_pendingCompletionHandler(data, exception);
}

void AuthenticatorCoordinatorClient::isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool result)
{
    auto handler = m_pendingQueryCompletionHandlers.take(messageId);
    handler(result);
}

uint64_t AuthenticatorCoordinatorClient::setRequestCompletionHandler(RequestCompletionHandler&& handler)
{
    if (m_pendingCompletionHandler)
        m_pendingCompletionHandler({ }, { NotAllowedError, "This request has been voided by a new request."_s });

    m_pendingCompletionHandler = WTFMove(handler);
    return m_accumulatedRequestMessageId++;
}

uint64_t AuthenticatorCoordinatorClient::addQueryCompletionHandler(QueryCompletionHandler&& handler)
{
    uint64_t messageId = m_accumulatedQueryMessageId++;
    auto addResult = m_pendingQueryCompletionHandlers.add(messageId, WTFMove(handler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return messageId;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
