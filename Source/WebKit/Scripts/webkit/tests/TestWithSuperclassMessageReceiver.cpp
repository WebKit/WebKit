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
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
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

void TestWithSuperclass::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithSuperclass::LoadURL::name()) {
        IPC::handleMessage<Messages::TestWithSuperclass::LoadURL>(connection, decoder, this, &TestWithSuperclass::loadURL);
        return;
    }
#if ENABLE(TEST_FEATURE)
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessage::name()) {
        IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessage>(connection, decoder, this, &TestWithSuperclass::testAsyncMessage);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::name()) {
        IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments>(connection, decoder, this, &TestWithSuperclass::testAsyncMessageWithNoArguments);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::name()) {
        IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments>(connection, decoder, this, &TestWithSuperclass::testAsyncMessageWithMultipleArguments);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithConnection::name()) {
        IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithConnection>(connection, decoder, this, &TestWithSuperclass::testAsyncMessageWithConnection);
        return;
    }
#endif
    WebPageBase::didReceiveMessage(connection, decoder);
}

void TestWithSuperclass::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
    if (decoder.messageName() == Messages::TestWithSuperclass::TestSyncMessage::name()) {
        IPC::handleMessageSynchronous<Messages::TestWithSuperclass::TestSyncMessage>(connection, decoder, replyEncoder, this, &TestWithSuperclass::testSyncMessage);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSuperclass::TestSynchronousMessage::name()) {
        IPC::handleMessageSynchronous<Messages::TestWithSuperclass::TestSynchronousMessage>(connection, decoder, replyEncoder, this, &TestWithSuperclass::testSynchronousMessage);
        return;
    }
    WebPageBase::didReceiveSyncMessage(connection, decoder, replyEncoder);
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_LoadURL>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::LoadURL::Arguments>(globalObject, decoder);
}
#if ENABLE(TEST_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArguments>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArguments>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithConnection::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestAsyncMessageWithConnection>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithConnection::ReplyArguments>(globalObject, decoder);
}
#endif
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSyncMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSyncMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestSynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSynchronousMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSuperclass_TestSynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestSynchronousMessage::ReplyArguments>(globalObject, decoder);
}
#if ENABLE(TEST_FEATURE)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithNoArgumentsReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArgumentsReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSuperclass_TestAsyncMessageWithConnectionReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSuperclass::TestAsyncMessageWithConnectionReply::Arguments>(globalObject, decoder);
}
#endif

}

#endif

