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
#include "Shared/WebKit-Swift.h" // NOLINT
#include "ArgumentCoders.h" // NOLINT
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "SessionState.h" // NOLINT
#include "TestSwiftEnum.h" // NOLINT
#include "TestSwiftOptionSet.h" // NOLINT
#include "TestSwiftRefCountedClass.h" // NOLINT
#include "TestSwiftStruct.h" // NOLINT
#include "TestWithSwiftTypesMessages.h" // NOLINT
#include <WebCore/FrameIdentifier.h> // NOLINT
#include <optional> // NOLINT
#include <wtf/Markable.h> // NOLINT
#include <wtf/OptionSet.h> // NOLINT
#include <wtf/Ref.h> // NOLINT
#include <wtf/RefCounted.h> // NOLINT
#include <wtf/Vector.h> // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithSwiftTypesMessageForwarder::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithoutReply::name()) {
        IPC::handleSwiftMessage<Messages::TestWithSwiftTypes::MessageWithoutReply>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithoutReply);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithoutParameters::name()) {
        IPC::handleSwiftMessage<Messages::TestWithSwiftTypes::MessageWithoutParameters>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithoutParameters);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithReply::name()) {
        IPC::handleSwiftMessageAsync<Messages::TestWithSwiftTypes::MessageWithReply>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithReply);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithMultipleReplies::name()) {
        IPC::handleSwiftMessageAsync<Messages::TestWithSwiftTypes::MessageWithMultipleReplies>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithMultipleReplies);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithEmptyReply::name()) {
        IPC::handleSwiftMessageAsync<Messages::TestWithSwiftTypes::MessageWithEmptyReply>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithEmptyReply);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithRefs::name()) {
        IPC::handleSwiftMessageAsync<Messages::TestWithSwiftTypes::MessageWithRefs>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithRefs);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::MessageWithCxxTypes::name()) {
        IPC::handleMessageAsync<Messages::TestWithSwiftTypes::MessageWithCxxTypes>(connection, decoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchMessageWithCxxTypes);
        return;
    }
    UNUSED_PARAM(connection);
    RELEASE_LOG_ERROR(IPC, "Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

void TestWithSwiftTypesMessageForwarder::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    auto target = getMessageTarget();
    if (!target) {
        FATAL("Something is keeping a reference to the message forwarder");
        decoder.markInvalid();
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::SynchronousMessage::name()) {
        IPC::handleSwiftMessageSynchronous<Messages::TestWithSwiftTypes::SynchronousMessage>(connection, decoder, replyEncoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchSynchronousMessage);
        return;
    }
    if (decoder.messageName() == Messages::TestWithSwiftTypes::SynchronousMessageWithoutParameters::name()) {
        IPC::handleSwiftMessageSynchronous<Messages::TestWithSwiftTypes::SynchronousMessageWithoutParameters>(connection, decoder, replyEncoder, m_handler.get(), &TestWithSwiftTypesWeakRef::dispatchSynchronousMessageWithoutParameters);
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(replyEncoder);
    RELEASE_LOG_ERROR(IPC, "Unhandled synchronous message %s to %" PRIu64, description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

static std::unique_ptr<TestWithSwiftTypesWeakRef> makeTestWithSwiftTypesWeakRefUniquePtr(TestWithSwiftTypesWeakRef* WTF_NONNULL handler)
{
    auto newRef = _impl::_impl_TestWithSwiftTypesWeakRef::makeRetained(handler);
    return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftTypesWeakRef>(newRef);
}

TestWithSwiftTypesMessageForwarder::TestWithSwiftTypesMessageForwarder(TestWithSwiftTypesWeakRef* WTF_NONNULL target)
    : m_handler(makeTestWithSwiftTypesWeakRefUniquePtr(target))
{
}

std::unique_ptr<TestWithSwiftTypes> TestWithSwiftTypesMessageForwarder::getMessageTarget()
{
    auto target = m_handler->getMessageTarget();
    if (target)
        return WTF::makeUniqueWithoutFastMallocCheck<TestWithSwiftTypes>(target.get());
    return nullptr;
}

TestWithSwiftTypesMessageForwarder::~TestWithSwiftTypesMessageForwarder()
{
}


} // namespace WebKit

namespace CompletionHandlers {
namespace TestWithSwiftTypes {

void completeWithDefaultReply(MessageWithCxxTypesCompletionHandler& completionHandler)
{
    IPC::Connection::cancelReply<Messages::TestWithSwiftTypes::MessageWithCxxTypes>(*completionHandler);
}

void complete(MessageWithCxxTypesCompletionHandler& completionHandler, uint8_t reply)
{
    (*completionHandler)(reply);
}

} // namespace TestWithSwiftTypes
} // namespace CompletionHandlers

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithoutReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithoutReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithoutParameters>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithoutParameters::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_MessageWithReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithReply::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithMultipleReplies>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithMultipleReplies::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_MessageWithMultipleReplies>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithMultipleReplies::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithEmptyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithEmptyReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_MessageWithEmptyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithEmptyReply::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithRefs>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithRefs::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_MessageWithRefs>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithRefs::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_SynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::SynchronousMessage::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_SynchronousMessage>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::SynchronousMessage::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_SynchronousMessageWithoutParameters>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::SynchronousMessageWithoutParameters::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_SynchronousMessageWithoutParameters>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::SynchronousMessageWithoutParameters::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithCxxTypes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithCxxTypes::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessageReply<MessageName::TestWithSwiftTypes_MessageWithCxxTypes>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithCxxTypes::ReplyArguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithReplyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithReplyReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithMultipleRepliesReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithMultipleRepliesReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithEmptyReplyReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithEmptyReplyReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithRefsReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithRefsReply::Arguments>(globalObject, decoder);
}
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithSwiftTypes_MessageWithCxxTypesReply>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithSwiftTypes::MessageWithCxxTypesReply::Arguments>(globalObject, decoder);
}

}

#endif

