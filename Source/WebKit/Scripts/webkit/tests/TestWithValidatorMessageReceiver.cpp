/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestWithValidator.h"

#include "ArgumentCoders.h" // NOLINT
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "SharedPreferencesForWebProcess.h" // NOLINT
#include "TestWithValidatorMessages.h" // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithValidator::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto sharedPreferences = sharedPreferencesForWebProcess();
    UNUSED_VARIABLE(sharedPreferences);
    if (!sharedPreferences || !sharedPreferences->someOtherFeature) {
        RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message receiver TestWithValidator", IPC::description(decoder.messageName()).characters());
        decoder.markInvalid();
        return;
    }
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithValidator::AlwaysEnabled::name()) {
        IPC::handleMessage<Messages::TestWithValidator::AlwaysEnabled>(connection, decoder, this, &TestWithValidator::alwaysEnabled);
        return;
    }
    if (decoder.messageName() == Messages::TestWithValidator::EnabledIfPassValidation::name()) {
        if (!ValidateFunction(decoder)) {
            RELEASE_LOG_ERROR(IPC, "Message %s fails validation", IPC::description(decoder.messageName()).characters());
            decoder.markInvalid();
            return;
        }
        IPC::handleMessage<Messages::TestWithValidator::EnabledIfPassValidation>(connection, decoder, this, &TestWithValidator::enabledIfPassValidation);
        return;
    }
    if (decoder.messageName() == Messages::TestWithValidator::EnabledIfSomeFeatureEnabledAndPassValidation::name()) {
        if (!(sharedPreferences && sharedPreferences->someFeature)) {
            RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message endpoint", IPC::description(decoder.messageName()).characters());
            decoder.markInvalid();
            return;
        }
        if (!ValidateFunction(decoder)) {
            RELEASE_LOG_ERROR(IPC, "Message %s fails validation", IPC::description(decoder.messageName()).characters());
            decoder.markInvalid();
            return;
        }
        IPC::handleMessage<Messages::TestWithValidator::EnabledIfSomeFeatureEnabledAndPassValidation>(connection, decoder, this, &TestWithValidator::enabledIfSomeFeatureEnabledAndPassValidation);
        return;
    }
    if (decoder.messageName() == Messages::TestWithValidator::MessageWithReply::name()) {
        IPC::handleMessageAsync<Messages::TestWithValidator::MessageWithReply>(connection, decoder, this, &TestWithValidator::messageWithReply);
        return;
    }
    UNUSED_PARAM(connection);
    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

void TestWithValidator::sendCancelReply(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == IPC::ReceiverName::TestWithValidator);
    switch (decoder.messageName()) {
    case IPC::MessageName::TestWithValidator_MessageWithReply: {
        auto arguments = decoder.decode<typename Messages::TestWithValidator::MessageWithReply::Arguments>();
        if (!arguments) [[unlikely]]
            return;
        auto replyID = decoder.decode<IPC::AsyncReplyID>();
        if (!replyID) [[unlikely]]
            return;
        connection.sendAsyncReply<Messages::TestWithValidator::MessageWithReply>(*replyID
            , IPC::AsyncReplyError<String>::create()
            , IPC::AsyncReplyError<double>::create()
        );
        return;
    }
    default:
        // No reply to send.
        return;
    }
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithValidator_AlwaysEnabled>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithValidator::AlwaysEnabled::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithValidator_EnabledIfPassValidation>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithValidator::EnabledIfPassValidation::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithValidator_EnabledIfSomeFeatureEnabledAndPassValidation>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithValidator::EnabledIfSomeFeatureEnabledAndPassValidation::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithValidator_MessageWithReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithValidator::MessageWithReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithValidator_MessageWithReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithValidator::MessageWithReply::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithValidator_MessageWithReplyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithValidator::MessageWithReplyReply::Arguments>(globalObject, decoder);
}

}

#endif

