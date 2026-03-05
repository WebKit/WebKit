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
#if ENABLE(SWIFT_TEST_CONDITION)
#include "Shared/WebKit-Swift.h" // NOLINT
#else // ENABLE(SWIFT_TEST_CONDITION)
#include "TestWithSwiftConditionally.h"

#endif // ENABLE(SWIFT_TEST_CONDITION)
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "TestWithSwiftConditionallyMessages.h" // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

#if ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionallyMessageForwarder::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
#else // ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionally::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
#endif // ENABLE(SWIFT_TEST_CONDITION)
{
    Ref protectedThis { *this };
#if ENABLE(SWIFT_TEST_CONDITION)
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
#endif // ENABLE(SWIFT_TEST_CONDITION)
    if (decoder.messageName() == Messages::TestWithSwiftConditionally::TestAsyncMessage::name()) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftConditionally::TestAsyncMessage>(connection, decoder, target.get(), &TestWithSwiftConditionally::testAsyncMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftConditionally::TestAsyncMessage>(connection, decoder, this, &TestWithSwiftConditionally::testAsyncMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    UNUSED_PARAM(connection);
    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

#if ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionallyMessageForwarder::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
#else // ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionally::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
#endif // ENABLE(SWIFT_TEST_CONDITION)
{
    Ref protectedThis { *this };
#if ENABLE(SWIFT_TEST_CONDITION)
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
#endif // ENABLE(SWIFT_TEST_CONDITION)
    if (decoder.messageName() == Messages::TestWithSwiftConditionally::TestSyncMessage::name()) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftConditionally::TestSyncMessage>(connection, decoder, replyEncoder, target.get(), &TestWithSwiftConditionally::testSyncMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftConditionally::TestSyncMessage>(connection, decoder, replyEncoder, this, &TestWithSwiftConditionally::testSyncMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(replyEncoder);
    RELEASE_LOG_ERROR(IPC, "Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}
#if ENABLE(SWIFT_TEST_CONDITION)

static std::unique_ptr<TestWithSwiftConditionallyWeakRef> makeTestWithSwiftConditionallyWeakRefUniquePtr(TestWithSwiftConditionallyWeakRef* _Nonnull handler)
{
    auto newRef = _impl::_impl_TestWithSwiftConditionallyWeakRef::makeRetained(handler);
    return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftConditionallyWeakRef>(newRef);
}

TestWithSwiftConditionallyMessageForwarder::TestWithSwiftConditionallyMessageForwarder(TestWithSwiftConditionallyWeakRef* _Nonnull target)
    : m_handler(makeTestWithSwiftConditionallyWeakRefUniquePtr(target))
{
}

std::unique_ptr<TestWithSwiftConditionally> TestWithSwiftConditionallyMessageForwarder::getMessageTarget()
{
    auto target = m_handler->getMessageTarget();
    if (target)
        return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftConditionally>(target.get());
    return nullptr;
}

TestWithSwiftConditionallyMessageForwarder::~TestWithSwiftConditionallyMessageForwarder()
{
}

#endif // ENABLE(SWIFT_TEST_CONDITION)

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftConditionally_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionally::TestAsyncMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftConditionally_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionally::TestAsyncMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftConditionally_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionally::TestSyncMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftConditionally_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionally::TestSyncMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftConditionally_TestAsyncMessageReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionally::TestAsyncMessageReply::Arguments>(globalObject, decoder);
}

}

#endif

