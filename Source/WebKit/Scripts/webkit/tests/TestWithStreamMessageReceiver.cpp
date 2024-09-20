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
#include "TestWithStream.h"

#include "ArgumentCoders.h" // NOLINT
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "TestWithStreamMessages.h" // NOLINT
#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h> // NOLINT
#endif
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithStream::didReceiveStreamMessage(IPC::StreamServerConnection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::TestWithStream::SendString::name()) {
        IPC::handleMessage<Messages::TestWithStream::SendString>(connection, decoder, this, &TestWithStream::sendString);
        return;
    }
    if (decoder.messageName() == Messages::TestWithStream::SendStringAsync::name()) {
        IPC::handleMessageWithReply<Messages::TestWithStream::SendStringAsync>(connection, decoder, this, &TestWithStream::sendStringAsync);
        return;
    }
    if (decoder.messageName() == Messages::TestWithStream::SendStringSync::name()) {
        IPC::handleMessageWithReply<Messages::TestWithStream::SendStringSync>(connection, decoder, this, &TestWithStream::sendStringSync);
        return;
    }
    if (decoder.messageName() == Messages::TestWithStream::CallWithIdentifier::name()) {
        IPC::handleMessageWithReplyID<Messages::TestWithStream::CallWithIdentifier>(connection, decoder, this, &TestWithStream::callWithIdentifier);
        return;
    }
#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::TestWithStream::SendMachSendRight::name()) {
        IPC::handleMessage<Messages::TestWithStream::SendMachSendRight>(connection, decoder, this, &TestWithStream::sendMachSendRight);
        return;
    }
    if (decoder.messageName() == Messages::TestWithStream::ReceiveMachSendRight::name()) {
        IPC::handleMessageWithReply<Messages::TestWithStream::ReceiveMachSendRight>(connection, decoder, this, &TestWithStream::receiveMachSendRight);
        return;
    }
    if (decoder.messageName() == Messages::TestWithStream::SendAndReceiveMachSendRight::name()) {
        IPC::handleMessageWithReply<Messages::TestWithStream::SendAndReceiveMachSendRight>(connection, decoder, this, &TestWithStream::sendAndReceiveMachSendRight);
        return;
    }
#endif
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(connection);
#if ENABLE(IPC_TESTING_API)
    if (connection.protectedConnection()->ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled stream message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendString>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendString::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendStringAsync>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringAsync::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringAsync>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringAsync::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendStringSync>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringSync::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringSync>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringSync::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_CallWithIdentifier>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::CallWithIdentifier::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_CallWithIdentifier>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::CallWithIdentifier::ReplyArguments>(globalObject, decoder);
}
#if PLATFORM(COCOA)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendMachSendRight>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendMachSendRight::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_ReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::ReceiveMachSendRight::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_ReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::ReceiveMachSendRight::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendAndReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendAndReceiveMachSendRight::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_SendAndReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendAndReceiveMachSendRight::ReplyArguments>(globalObject, decoder);
}
#endif

}

#endif

