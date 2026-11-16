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
#include "TestWithSwiftTypesOptIn.h"

#endif // ENABLE(SWIFT_TEST_CONDITION)
#include "ArgumentCoders.h" // NOLINT
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "SharedPreferencesForWebProcess.h" // NOLINT
#include "TestSwiftStruct.h" // NOLINT
#include "TestWithSwiftTypesOptInMessages.h" // NOLINT
#include <optional> // NOLINT
#include <wtf/Vector.h> // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

#if ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftTypesOptInMessageForwarder::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
#else // ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftTypesOptIn::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
#endif // ENABLE(SWIFT_TEST_CONDITION)
{
#if ENABLE(SWIFT_TEST_CONDITION)
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
    auto sharedPreferences = target->sharedPreferencesForWebProcess();
#else // ENABLE(SWIFT_TEST_CONDITION)
    auto sharedPreferences = sharedPreferencesForWebProcess();
#endif // ENABLE(SWIFT_TEST_CONDITION)
    UNUSED_VARIABLE(sharedPreferences);
    if (!sharedPreferences || !sharedPreferences->someFeature) {
        RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message receiver TestWithSwiftTypesOptIn", IPC::description(decoder.messageName()).characters());
        decoder.markInvalid();
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypesOptIn::CxxTypedMessage::name()) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftTypesOptIn::CxxTypedMessage>(connection, decoder, m_handler.get(), &TestWithSwiftTypesOptInWeakRef::dispatchCxxTypedMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftTypesOptIn::CxxTypedMessage>(connection, decoder, this, &TestWithSwiftTypesOptIn::cxxTypedMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypesOptIn::SwiftTypedMessage::name()) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleSwiftMessageAsync<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessage>(connection, decoder, m_handler.get(), &TestWithSwiftTypesOptInWeakRef::dispatchSwiftTypedMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageAsync<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessage>(connection, decoder, this, &TestWithSwiftTypesOptIn::swiftTypedMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypesOptIn::SwiftTypedMessageWithoutReply::name()) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleSwiftMessage<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessageWithoutReply>(connection, decoder, m_handler.get(), &TestWithSwiftTypesOptInWeakRef::dispatchSwiftTypedMessageWithoutReply);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessage<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessageWithoutReply>(connection, decoder, this, &TestWithSwiftTypesOptIn::swiftTypedMessageWithoutReply);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    UNUSED_PARAM(connection);
    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

#if ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftTypesOptInMessageForwarder::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
#else // ENABLE(SWIFT_TEST_CONDITION)
void TestWithSwiftTypesOptIn::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
#endif // ENABLE(SWIFT_TEST_CONDITION)
{
#if ENABLE(SWIFT_TEST_CONDITION)
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
    auto sharedPreferences = target->sharedPreferencesForWebProcess();
#else // ENABLE(SWIFT_TEST_CONDITION)
    auto sharedPreferences = sharedPreferencesForWebProcess();
#endif // ENABLE(SWIFT_TEST_CONDITION)
    UNUSED_VARIABLE(sharedPreferences);
    if (!sharedPreferences || !sharedPreferences->someFeature) {
        RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message receiver TestWithSwiftTypesOptIn", IPC::description(decoder.messageName()).characters());
        decoder.markInvalid();
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypesOptIn::SwiftTypedSynchronousMessage::name() && sharedPreferences && sharedPreferences->someOtherFeature) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleSwiftMessageSynchronous<Messages::TestWithSwiftTypesOptIn::SwiftTypedSynchronousMessage>(connection, decoder, replyEncoder, m_handler.get(), &TestWithSwiftTypesOptInWeakRef::dispatchSwiftTypedSynchronousMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftTypesOptIn::SwiftTypedSynchronousMessage>(connection, decoder, replyEncoder, this, &TestWithSwiftTypesOptIn::swiftTypedSynchronousMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypesOptIn::CxxTypedSynchronousMessage::name()) {
#if ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftTypesOptIn::CxxTypedSynchronousMessage>(connection, decoder, replyEncoder, m_handler.get(), &TestWithSwiftTypesOptInWeakRef::dispatchCxxTypedSynchronousMessage);
#else // ENABLE(SWIFT_TEST_CONDITION)
        IPC::handleMessageSynchronous<Messages::TestWithSwiftTypesOptIn::CxxTypedSynchronousMessage>(connection, decoder, replyEncoder, this, &TestWithSwiftTypesOptIn::cxxTypedSynchronousMessage);
#endif // ENABLE(SWIFT_TEST_CONDITION)
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(replyEncoder);
    RELEASE_LOG_ERROR(IPC, "Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}
#if ENABLE(SWIFT_TEST_CONDITION)

static std::unique_ptr<TestWithSwiftTypesOptInWeakRef> makeTestWithSwiftTypesOptInWeakRefUniquePtr(TestWithSwiftTypesOptInWeakRef* WTF_NONNULL handler)
{
    auto newRef = _impl::_impl_TestWithSwiftTypesOptInWeakRef::makeRetained(handler);
    return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftTypesOptInWeakRef>(newRef);
}

TestWithSwiftTypesOptInMessageForwarder::TestWithSwiftTypesOptInMessageForwarder(TestWithSwiftTypesOptInWeakRef* WTF_NONNULL target)
    : m_handler(makeTestWithSwiftTypesOptInWeakRefUniquePtr(target))
{
}

std::unique_ptr<TestWithSwiftTypesOptIn> TestWithSwiftTypesOptInMessageForwarder::getMessageTarget()
{
    auto target = m_handler->getMessageTarget();
    if (target)
        return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftTypesOptIn>(target.get());
    return nullptr;
}

TestWithSwiftTypesOptInMessageForwarder::~TestWithSwiftTypesOptInMessageForwarder()
{
}

#endif // ENABLE(SWIFT_TEST_CONDITION)

} // namespace WebKit
#if ENABLE(SWIFT_TEST_CONDITION)

namespace CompletionHandlers {
namespace TestWithSwiftTypesOptIn {

void completeWithDefaultReply(CxxTypedMessageCompletionHandler& completionHandler)
{
    IPC::Connection::cancelReply<Messages::TestWithSwiftTypesOptIn::CxxTypedMessage>(*completionHandler);
}

void complete(CxxTypedMessageCompletionHandler& completionHandler, uint8_t reply)
{
    (*completionHandler)(reply);
}

} // namespace TestWithSwiftTypesOptIn
} // namespace CompletionHandlers
#endif // ENABLE(SWIFT_TEST_CONDITION)

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_CxxTypedMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::CxxTypedMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypesOptIn_CxxTypedMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::CxxTypedMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessageWithoutReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessageWithoutReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_SwiftTypedSynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::SwiftTypedSynchronousMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypesOptIn_SwiftTypedSynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::SwiftTypedSynchronousMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_CxxTypedSynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::CxxTypedSynchronousMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypesOptIn_CxxTypedSynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::CxxTypedSynchronousMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_CxxTypedMessageReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::CxxTypedMessageReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypesOptIn_SwiftTypedMessageReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypesOptIn::SwiftTypedMessageReply::Arguments>(globalObject, decoder);
}

}

#endif

