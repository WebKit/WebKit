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
#include "TestWithIfMessage.h"

#if PLATFORM(COCOA) || PLATFORM(GTK)
#include "ArgumentCoders.h" // NOLINT
#endif
#include "Decoder.h" // NOLINT
#include "HandleMessage.h" // NOLINT
#include "TestWithIfMessageMessages.h" // NOLINT
#if PLATFORM(COCOA) || PLATFORM(GTK)
#include <wtf/text/WTFString.h> // NOLINT
#endif

#if ENABLE(IPC_TESTING_API)
#include "JSIPCBinding.h"
#endif

namespace WebKit {

void TestWithIfMessage::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    Ref protectedThis { *this };
#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::TestWithIfMessage::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithIfMessage::LoadURL>(connection, decoder, this, &TestWithIfMessage::loadURL);
#endif
#if PLATFORM(GTK)
    if (decoder.messageName() == Messages::TestWithIfMessage::LoadURL::name())
        return IPC::handleMessage<Messages::TestWithIfMessage::LoadURL>(connection, decoder, this, &TestWithIfMessage::loadURL);
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()), static_cast<uint64_t>(decoder.destinationID()));
}

} // namespace WebKit

#if ENABLE(IPC_TESTING_API)

namespace IPC {

#if PLATFORM(COCOA)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithIfMessage_LoadURL>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithIfMessage::LoadURL::Arguments>(globalObject, decoder);
}
#endif
#if PLATFORM(GTK)
template<> std::optional<JSC::JSValue> jsValueForDecodedMessage<MessageName::TestWithIfMessage_LoadURL>(JSC::JSGlobalObject* globalObject, Decoder& decoder)
{
    return jsValueForDecodedArguments<Messages::TestWithIfMessage::LoadURL::Arguments>(globalObject, decoder);
}
#endif

}

#endif

