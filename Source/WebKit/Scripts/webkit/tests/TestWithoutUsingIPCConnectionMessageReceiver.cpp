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
#include "TestWithoutUsingIPCConnection.h"

#include "ArgumentCoders.h" // NOLINT
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "TestWithoutUsingIPCConnectionMessages.h" // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithoutUsingIPCConnection::didReceiveMessageWithReplyHandler(IPC::Decoder& decoder, Function<void(UniqueRef<IPC::Encoder>&&)>&& replyHandler)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithoutUsingIPCConnection::MessageWithoutArgument::name())
        return IPC::handleMessageWithoutUsingIPCConnection<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgument>(decoder, this, &TestWithoutUsingIPCConnection::messageWithoutArgument);
    if (decoder.messageName() == Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndEmptyReply::name())
        return IPC::handleMessageAsyncWithoutUsingIPCConnection<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndEmptyReply>(decoder, WTFMove(replyHandler), this, &TestWithoutUsingIPCConnection::messageWithoutArgumentAndEmptyReply);
    if (decoder.messageName() == Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndReplyWithArgument::name())
        return IPC::handleMessageAsyncWithoutUsingIPCConnection<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndReplyWithArgument>(decoder, WTFMove(replyHandler), this, &TestWithoutUsingIPCConnection::messageWithoutArgumentAndReplyWithArgument);
    if (decoder.messageName() == Messages::TestWithoutUsingIPCConnection::MessageWithArgument::name())
        return IPC::handleMessageWithoutUsingIPCConnection<Messages::TestWithoutUsingIPCConnection::MessageWithArgument>(decoder, this, &TestWithoutUsingIPCConnection::messageWithArgument);
    if (decoder.messageName() == Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndEmptyReply::name())
        return IPC::handleMessageAsyncWithoutUsingIPCConnection<Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndEmptyReply>(decoder, WTFMove(replyHandler), this, &TestWithoutUsingIPCConnection::messageWithArgumentAndEmptyReply);
    if (decoder.messageName() == Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndReplyWithArgument::name())
        return IPC::handleMessageAsyncWithoutUsingIPCConnection<Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndReplyWithArgument>(decoder, WTFMove(replyHandler), this, &TestWithoutUsingIPCConnection::messageWithArgumentAndReplyWithArgument);
    UNUSED_PARAM(decoder);
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()), decoder.destinationID());
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgument>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgument::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndEmptyReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndEmptyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndEmptyReply::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndReplyWithArgument::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithoutArgumentAndReplyWithArgument>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithoutArgumentAndReplyWithArgument::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgument>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithArgument::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndEmptyReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndEmptyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndEmptyReply::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndReplyWithArgument::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithoutUsingIPCConnection_MessageWithArgumentAndReplyWithArgument>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithoutUsingIPCConnection::MessageWithArgumentAndReplyWithArgument::ReplyArguments>(globalObject, decoder);
}

}

#endif

