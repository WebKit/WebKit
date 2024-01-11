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
#include "TestWithSuperclass.h"

#include "ArgumentCoders.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "Message.h" // NOLINT
#include "TestClassName.h" // NOLINT
#if ENABLE(TEST_FEATURE)
#include "TestTwoStateEnum.h" // NOLINT
#endif
#include "TestWithSuperclassMessages.h" // NOLINT
#include <optional> // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithSuperclass::didReceiveMessage(IPC::Connection& connection, IPC::Message&& message)
{
    Ref protectedThis { *this };
    if (message.messageName() == Messages::TestWithSuperclass::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithSuperclass::LoadURL>(connection, WTFMove(message), this, &TestWithSuperclass::loadURL);
#if ENABLE(TEST_FEATURE)
    if (message.messageName() == Messages::TestWithSuperclass::TestAsyncMessage::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessage>(connection, WTFMove(message), this, &TestWithSuperclass::testAsyncMessage);
    if (message.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments>(connection, WTFMove(message), this, &TestWithSuperclass::testAsyncMessageWithNoArguments);
    if (message.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments>(connection, WTFMove(message), this, &TestWithSuperclass::testAsyncMessageWithMultipleArguments);
    if (message.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithConnection::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithConnection>(connection, WTFMove(message), this, &TestWithSuperclass::testAsyncMessageWithConnection);
#endif
    WebPageBase::didReceiveMessage(connection, WTFMove(message));
}

bool TestWithSuperclass::didReceiveSyncMessage(IPC::Connection& connection, IPC::Message& message, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
    if (message.messageName() == Messages::TestWithSuperclass::TestSyncMessage::name())
        return IPC::handleMessageSynchronous<Messages::TestWithSuperclass::TestSyncMessage>(connection, WTFMove(message), replyEncoder, this, &TestWithSuperclass::testSyncMessage);
    if (message.messageName() == Messages::TestWithSuperclass::TestSynchronousMessage::name())
        return IPC::handleMessageSynchronous<Messages::TestWithSuperclass::TestSynchronousMessage>(connection, WTFMove(message), replyEncoder, this, &TestWithSuperclass::testSynchronousMessage);
    UNUSED_PARAM(connection);
    UNUSED_PARAM(message);
    UNUSED_PARAM(replyEncoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return false;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled synchronous message %s to %" PRIu64, description(message.messageName()), message.destinationID);
    return false;
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_LoadURL>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::LoadURL::Arguments>(globalObject, message);
}
#if ENABLE(TEST_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessage::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessage::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithConnection::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithConnection>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithConnection::ReplyArguments>(globalObject, message);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSyncMessage::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSyncMessage::ReplyArguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestSynchronousMessage>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSynchronousMessage::Arguments>(globalObject, message);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestSynchronousMessage>(JSC::JSGlobalObject* globalObject, Message& message)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSynchronousMessage::ReplyArguments>(globalObject, message);
}

}

#endif

