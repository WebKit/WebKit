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
#include "CredentialsMessenger.h"

#if ENABLE(WEB_AUTHN)

namespace WebCore {

namespace CredentialsMessengerInternal {

const uint64_t maxMessageId = 0xFFFFFFFFFFFFFF; // 56 bits
const size_t callBackClassifierOffset = 56;

}

void CredentialsMessenger::exceptionReply(uint64_t messageId, const ExceptionData& exception)
{
    using namespace CredentialsMessengerInternal;

    if (!(messageId >> callBackClassifierOffset ^ CallBackClassifier::Creation)) {
        auto handler = takeCreationCompletionHandler(messageId);
        handler(exception.toException());
        return;
    }
    if (!(messageId >> callBackClassifierOffset ^ CallBackClassifier::Request)) {
        auto handler = takeRequestCompletionHandler(messageId);
        handler(exception.toException());
        return;
    }
}

uint64_t CredentialsMessenger::addCreationCompletionHandler(CreationCompletionHandler&& handler)
{
    using namespace CredentialsMessengerInternal;

    uint64_t messageId = m_accumulatedMessageId++;
    ASSERT(messageId <= maxMessageId);
    messageId = messageId | CallBackClassifier::Creation << callBackClassifierOffset;
    auto addResult = m_pendingCreationCompletionHandlers.add(messageId, WTFMove(handler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return messageId;
}

CreationCompletionHandler CredentialsMessenger::takeCreationCompletionHandler(uint64_t messageId)
{
    return m_pendingCreationCompletionHandlers.take(messageId);
}

uint64_t CredentialsMessenger::addRequestCompletionHandler(RequestCompletionHandler&& handler)
{
    using namespace CredentialsMessengerInternal;

    uint64_t messageId = m_accumulatedMessageId++;
    ASSERT(messageId <= maxMessageId);
    messageId = messageId | CallBackClassifier::Request << callBackClassifierOffset;
    auto addResult = m_pendingRequestCompletionHandlers.add(messageId, WTFMove(handler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return messageId;
}

RequestCompletionHandler CredentialsMessenger::takeRequestCompletionHandler(uint64_t messageId)
{
    return m_pendingRequestCompletionHandlers.take(messageId);
}

uint64_t CredentialsMessenger::addQueryCompletionHandler(QueryCompletionHandler&& handler)
{
    using namespace CredentialsMessengerInternal;

    uint64_t messageId = m_accumulatedMessageId++;
    ASSERT(messageId < maxMessageId);
    messageId = messageId | CallBackClassifier::Query << callBackClassifierOffset;
    auto addResult = m_pendingQueryCompletionHandlers.add(messageId, WTFMove(handler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return messageId;
}

QueryCompletionHandler CredentialsMessenger::takeQueryCompletionHandler(uint64_t messageId)
{
    return m_pendingQueryCompletionHandlers.take(messageId);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
