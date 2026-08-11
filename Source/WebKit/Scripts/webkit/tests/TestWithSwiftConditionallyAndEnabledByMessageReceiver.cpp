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
#include "TestWithSwiftConditionallyAndEnabledBy.h"

#endif // ENABLE(SWIFT_TEST_CONDITION)
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "SharedPreferencesForWebProcess.h" // NOLINT
#include "TestWithSwiftConditionallyAndEnabledByMessages.h" // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

#if ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionallyAndEnabledByMessageForwarder::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
#else // ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionallyAndEnabledBy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
#endif // ENABLE(SWIFT_TEST_CONDITION)
{
#if ENABLE(SWIFT_TEST_CONDITION)
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
    auto sharedPreferences = target->sharedPreferencesForWebProcess(connection);
#else // ENABLE(SWIFT_TEST_CONDITION)
    auto sharedPreferences = sharedPreferencesForWebProcess(connection);
#endif // ENABLE(SWIFT_TEST_CONDITION)
    UNUSED_VARIABLE(sharedPreferences);
    if (decoder.messageName() == Messages::TestWithSwiftConditionallyAndEnabledBy::TestAsyncMessage::name()) {
        if (!(sharedPreferences && sharedPreferences->someFeature)) {
            RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message endpoint", IPC::description(decoder.messageName()).characters());
            decoder.markInvalid();
            return;
        }
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftConditionallyAndEnabledBy::TestAsyncMessage>(connection, decoder, target.get(), &TestWithSwiftConditionallyAndEnabledBy::testAsyncMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftConditionallyAndEnabledBy::TestAsyncMessage>(connection, decoder, this, &TestWithSwiftConditionallyAndEnabledBy::testAsyncMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    UNUSED_PARAM(connection);
    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

#if ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionallyAndEnabledByMessageForwarder::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
#else // ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftConditionallyAndEnabledBy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
#endif // ENABLE(SWIFT_TEST_CONDITION)
{
#if ENABLE(SWIFT_TEST_CONDITION)
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
    auto sharedPreferences = target->sharedPreferencesForWebProcess(connection);
#else // ENABLE(SWIFT_TEST_CONDITION)
    auto sharedPreferences = sharedPreferencesForWebProcess(connection);
#endif // ENABLE(SWIFT_TEST_CONDITION)
    UNUSED_VARIABLE(sharedPreferences);
    if (decoder.messageName() == Messages::TestWithSwiftConditionallyAndEnabledBy::TestSyncMessage::name() && sharedPreferences && sharedPreferences->someFeature) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftConditionallyAndEnabledBy::TestSyncMessage>(connection, decoder, replyEncoder, target.get(), &TestWithSwiftConditionallyAndEnabledBy::testSyncMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftConditionallyAndEnabledBy::TestSyncMessage>(connection, decoder, replyEncoder, this, &TestWithSwiftConditionallyAndEnabledBy::testSyncMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(replyEncoder);
    RELEASE_LOG_ERROR(IPC, "Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}
#if ENABLE(SWIFT_TEST_CONDITION)

static std::unique_ptr<TestWithSwiftConditionallyAndEnabledByWeakRef> makeTestWithSwiftConditionallyAndEnabledByWeakRefUniquePtr(TestWithSwiftConditionallyAndEnabledByWeakRef* _Nonnull handler)
{
    auto newRef = _impl::_impl_TestWithSwiftConditionallyAndEnabledByWeakRef::makeRetained(handler);
    return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftConditionallyAndEnabledByWeakRef>(newRef);
}

TestWithSwiftConditionallyAndEnabledByMessageForwarder::TestWithSwiftConditionallyAndEnabledByMessageForwarder(TestWithSwiftConditionallyAndEnabledByWeakRef* _Nonnull target)
    : m_handler(makeTestWithSwiftConditionallyAndEnabledByWeakRefUniquePtr(target))
{
}

std::unique_ptr<TestWithSwiftConditionallyAndEnabledBy> TestWithSwiftConditionallyAndEnabledByMessageForwarder::getMessageTarget()
{
    auto target = m_handler->getMessageTarget();
    if (target)
        return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftConditionallyAndEnabledBy>(target.get());
    return nullptr;
}

TestWithSwiftConditionallyAndEnabledByMessageForwarder::~TestWithSwiftConditionallyAndEnabledByMessageForwarder()
{
}

#endif // ENABLE(SWIFT_TEST_CONDITION)

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftConditionallyAndEnabledBy_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionallyAndEnabledBy::TestAsyncMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftConditionallyAndEnabledBy_TestAsyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionallyAndEnabledBy::TestAsyncMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftConditionallyAndEnabledBy_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionallyAndEnabledBy::TestSyncMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftConditionallyAndEnabledBy_TestSyncMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionallyAndEnabledBy::TestSyncMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftConditionallyAndEnabledBy_TestAsyncMessageReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftConditionallyAndEnabledBy::TestAsyncMessageReply::Arguments>(globalObject, decoder);
}

}

#endif

