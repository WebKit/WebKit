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
#include "SharedPreferencesForWebProcess.h" // NOLINT
#include "TestWithStreamSwiftEnabledByMessages.h" // NOLINT
#include <wtf/text/WTFString.h> // NOLINT

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithStreamSwiftEnabledByMessageForwarder::didReceiveStreamMessage(IPC::StreamServerConnection& connection, IPC::Decoder& decoder)
{
    auto target = getMessageTarget();
    if (!target) {
        decoder.markInvalid();
        return;
    }
    auto sharedPreferences = target->sharedPreferencesForWebProcess();
    UNUSED_VARIABLE(sharedPreferences);
    if (!sharedPreferences || !sharedPreferences->someFeature) {
        RELEASE_LOG_ERROR(IPC, "Message %s received by a disabled message receiver TestWithStreamSwiftEnabledBy", IPC::description(decoder.messageName()).characters());
        decoder.markInvalid();
        return;
    }
    if (decoder.messageName() == Messages::TestWithStreamSwiftEnabledBy::SendString::name()) {
        IPC::handleMessage<Messages::TestWithStreamSwiftEnabledBy::SendString>(connection, decoder, target.get(), &TestWithStreamSwiftEnabledBy::sendString);
        return;
    }
    RELEASE_LOG_ERROR(IPC, "Unhandled stream message %s to %" PRIu64, IPC::description(decoder.messageName()).characters(), decoder.destinationID());
    decoder.markInvalid();
}

static std::unique_ptr<TestWithStreamSwiftEnabledByWeakRef> makeTestWithStreamSwiftEnabledByWeakRefUniquePtr(TestWithStreamSwiftEnabledByWeakRef* _Nonnull handler)
{
    auto newRef = _impl::_impl_TestWithStreamSwiftEnabledByWeakRef::makeRetained(handler);
    return WTF::makeUniqueWithoutFastMallocCheck<TestWithStreamSwiftEnabledByWeakRef>(newRef);
}

TestWithStreamSwiftEnabledByMessageForwarder::TestWithStreamSwiftEnabledByMessageForwarder(TestWithStreamSwiftEnabledByWeakRef* _Nonnull target)
    : m_handler(makeTestWithStreamSwiftEnabledByWeakRefUniquePtr(target))
{
}

std::unique_ptr<TestWithStreamSwiftEnabledBy> TestWithStreamSwiftEnabledByMessageForwarder::getMessageTarget()
{
    auto target = m_handler->getMessageTarget();
    if (target)
        return WTF::makeUniqueWithoutFastMallocCheck<TestWithStreamSwiftEnabledBy>(target.get());
    return nullptr;
}

TestWithStreamSwiftEnabledByMessageForwarder::~TestWithStreamSwiftEnabledByMessageForwarder()
{
}


} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithStreamSwiftEnabledBy_SendString>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithStreamSwiftEnabledBy::SendString::Arguments>(globalObject, decoder);
}

}

#endif

