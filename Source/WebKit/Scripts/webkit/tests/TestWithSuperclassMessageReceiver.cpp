/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "HandleMessage.h"
#include "TestClassName.h"
#if ENABLE(TEST_FEATURE)
#include "TestTwoStateEnum.h"
#endif
#include "TestWithSuperclassMessages.h"
#include <optional>
#include <wtf/text/WTFString.h>

namespace Messages {

namespace TestWithSuperclass {

#if ENABLE(TEST_FEATURE)

void TestAsyncMessage::callReply(IPC::Decoder& decoder, CompletionHandler<void(uint64_t&&)>&& completionHandler)
{
    std::optional<uint64_t> result;
    decoder >> result;
    if (!result) {
        ASSERT_NOT_REACHED();
        cancelReply(WTFMove(completionHandler));
        return;
    }
    completionHandler(WTFMove(*result));
}

void TestAsyncMessage::cancelReply(CompletionHandler<void(uint64_t&&)>&& completionHandler)
{
    completionHandler(IPC::AsyncReplyError<uint64_t>::create());
}

void TestAsyncMessage::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, uint64_t result)
{
    encoder.get() << result;
    connection.sendSyncReply(WTFMove(encoder));
}

#endif

#if ENABLE(TEST_FEATURE)

void TestAsyncMessageWithNoArguments::callReply(IPC::Decoder& decoder, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void TestAsyncMessageWithNoArguments::cancelReply(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void TestAsyncMessageWithNoArguments::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection)
{
    connection.sendSyncReply(WTFMove(encoder));
}

#endif

#if ENABLE(TEST_FEATURE)

void TestAsyncMessageWithMultipleArguments::callReply(IPC::Decoder& decoder, CompletionHandler<void(bool&&, uint64_t&&)>&& completionHandler)
{
    std::optional<bool> flag;
    decoder >> flag;
    if (!flag) {
        ASSERT_NOT_REACHED();
        cancelReply(WTFMove(completionHandler));
        return;
    }
    std::optional<uint64_t> value;
    decoder >> value;
    if (!value) {
        ASSERT_NOT_REACHED();
        cancelReply(WTFMove(completionHandler));
        return;
    }
    completionHandler(WTFMove(*flag), WTFMove(*value));
}

void TestAsyncMessageWithMultipleArguments::cancelReply(CompletionHandler<void(bool&&, uint64_t&&)>&& completionHandler)
{
    completionHandler(IPC::AsyncReplyError<bool>::create(), IPC::AsyncReplyError<uint64_t>::create());
}

void TestAsyncMessageWithMultipleArguments::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, bool flag, uint64_t value)
{
    encoder.get() << flag;
    encoder.get() << value;
    connection.sendSyncReply(WTFMove(encoder));
}

#endif

#if ENABLE(TEST_FEATURE)

void TestAsyncMessageWithConnection::callReply(IPC::Decoder& decoder, CompletionHandler<void(bool&&)>&& completionHandler)
{
    std::optional<bool> flag;
    decoder >> flag;
    if (!flag) {
        ASSERT_NOT_REACHED();
        cancelReply(WTFMove(completionHandler));
        return;
    }
    completionHandler(WTFMove(*flag));
}

void TestAsyncMessageWithConnection::cancelReply(CompletionHandler<void(bool&&)>&& completionHandler)
{
    completionHandler(IPC::AsyncReplyError<bool>::create());
}

void TestAsyncMessageWithConnection::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, bool flag)
{
    encoder.get() << flag;
    connection.sendSyncReply(WTFMove(encoder));
}

#endif

void TestSyncMessage::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, uint8_t reply)
{
    encoder.get() << reply;
    connection.sendSyncReply(WTFMove(encoder));
}

void TestSynchronousMessage::send(UniqueRef<IPC::Encoder>&& encoder, IPC::Connection& connection, const std::optional<WebKit::TestClassName>& optionalReply)
{
    encoder.get() << optionalReply;
    connection.sendSyncReply(WTFMove(encoder));
}

} // namespace TestWithSuperclass

} // namespace Messages

namespace WebKit {

void TestWithSuperclass::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithSuperclass::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithSuperclass::LoadURL>(decoder, this, &TestWithSuperclass::loadURL);
#if ENABLE(TEST_FEATURE)
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessage::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessage>(connection, decoder, this, &TestWithSuperclass::testAsyncMessage);
#endif
#if ENABLE(TEST_FEATURE)
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithNoArguments>(connection, decoder, this, &TestWithSuperclass::testAsyncMessageWithNoArguments);
#endif
#if ENABLE(TEST_FEATURE)
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments::name())
        return IPC::handleMessageAsync<Messages::TestWithSuperclass::TestAsyncMessageWithMultipleArguments>(connection, decoder, this, &TestWithSuperclass::testAsyncMessageWithMultipleArguments);
#endif
#if ENABLE(TEST_FEATURE)
    if (decoder.messageName() == Messages::TestWithSuperclass::TestAsyncMessageWithConnection::name())
        return IPC::handleMessageAsyncWantsConnection<Messages::TestWithSuperclass::TestAsyncMessageWithConnection>(connection, decoder, this, &TestWithSuperclass::testAsyncMessageWithConnection);
#endif
    WebPageBase::didReceiveMessage(connection, decoder);
}

bool TestWithSuperclass::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithSuperclass::TestSyncMessage::name())
        return IPC::handleMessageSynchronous<Messages::TestWithSuperclass::TestSyncMessage>(connection, decoder, replyEncoder, this, &TestWithSuperclass::testSyncMessage);
    if (decoder.messageName() == Messages::TestWithSuperclass::TestSynchronousMessage::name())
        return IPC::handleMessageSynchronous<Messages::TestWithSuperclass::TestSynchronousMessage>(connection, decoder, replyEncoder, this, &TestWithSuperclass::testSynchronousMessage);
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(replyEncoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return false;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()), decoder.destinationID());
    return false;
}

} // namespace WebKit
