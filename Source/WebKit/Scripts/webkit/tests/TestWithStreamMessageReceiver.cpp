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
#if PLATFORM(COCOA)
#include "ArgumentCodersDarwin.h" // NOLINT
#endif
#include "HandleMessage.h" // NOLINT
#include "Message.h" // NOLINT
#include "TestWithStreamMessages.h" // NOLINT
#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h> // NOLINT
#endif
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithStream::didReceiveStreamMessage(IPC::StreamServerConnection& connection, IPC::Message&& message)
{
    if (message.messageName() == Messages::TestWithStream::SendString::name())
        return IPC::handleMessage<Messages::TestWithStream::SendString>(connection.protectedConnection(), WTFMove(message), this, &TestWithStream::sendString);
    if (message.messageName() == Messages::TestWithStream::SendStringAsync::name())
        return IPC::handleMessageAsync<Messages::TestWithStream::SendStringAsync>(connection.protectedConnection(), WTFMove(message), this, &TestWithStream::sendStringAsync);
    if (message.messageName() == Messages::TestWithStream::CallWithIdentifier::name())
        return IPC::handleMessageAsyncWithReplyID<Messages::TestWithStream::CallWithIdentifier>(connection.protectedConnection(), WTFMove(message), this, &TestWithStream::callWithIdentifier);
#if PLATFORM(COCOA)
    if (message.messageName() == Messages::TestWithStream::SendMachSendRight::name())
        return IPC::handleMessage<Messages::TestWithStream::SendMachSendRight>(connection.protectedConnection(), WTFMove(message), this, &TestWithStream::sendMachSendRight);
#endif
    if (message.messageName() == Messages::TestWithStream::SendStringSync::name())
        return IPC::handleMessageSynchronous<Messages::TestWithStream::SendStringSync>(connection, WTFMove(message), this, &TestWithStream::sendStringSync);
#if PLATFORM(COCOA)
    if (message.messageName() == Messages::TestWithStream::ReceiveMachSendRight::name())
        return IPC::handleMessageSynchronous<Messages::TestWithStream::ReceiveMachSendRight>(connection, WTFMove(message), this, &TestWithStream::receiveMachSendRight);
    if (message.messageName() == Messages::TestWithStream::SendAndReceiveMachSendRight::name())
        return IPC::handleMessageSynchronous<Messages::TestWithStream::SendAndReceiveMachSendRight>(connection, WTFMove(message), this, &TestWithStream::sendAndReceiveMachSendRight);
#endif
    UNUSED_PARAM(message);
    UNUSED_PARAM(connection);
#if ENABLE(IPC_TESTING_API)
    if (connection.protectedConnection()->ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled stream message %s to %" PRIu64, IPC::description(message.messageName()), message.destinationID);
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendString>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendString::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendStringAsync>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringAsync::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringAsync>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringAsync::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendStringSync>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringSync::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_SendStringSync>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendStringSync::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_CallWithIdentifier>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::CallWithIdentifier::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_CallWithIdentifier>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::CallWithIdentifier::ReplyArguments>(globalObject, message);
}
#if PLATFORM(COCOA)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendMachSendRight>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendMachSendRight::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_ReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::ReceiveMachSendRight::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_ReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::ReceiveMachSendRight::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStream_SendAndReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendAndReceiveMachSendRight::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithStream_SendAndReceiveMachSendRight>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithStream::SendAndReceiveMachSendRight::ReplyArguments>(globalObject, message);
}
#endif

}

#endif

