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
#include "TestWithStream.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "HandleMessage.h"
#include "TestWithStreamMessages.h"
#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#endif
#include <wtf/text/WTFString.h>

namespace WebKit {

void TestWithStream::didReceiveStreamMessage(IPC::StreamServerConnectionBase& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::TestWithStream::SendString::name())
        return IPC::handleMessage<Messages::TestWithStream::SendString>(decoder, this, &TestWithStream::sendString);
#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::TestWithStream::SendMachSendRight::name())
        return IPC::handleMessage<Messages::TestWithStream::SendMachSendRight>(decoder, this, &TestWithStream::sendMachSendRight);
#endif
    if (decoder.messageName() == Messages::TestWithStream::SendStringSynchronized::name())
        return IPC::handleMessage<Messages::TestWithStream::SendStringSynchronized>(decoder, this, &TestWithStream::sendStringSynchronized);
#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::TestWithStream::ReceiveMachSendRight::name())
        return IPC::handleMessage<Messages::TestWithStream::ReceiveMachSendRight>(decoder, this, &TestWithStream::receiveMachSendRight);
#endif
#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::TestWithStream::SendAndReceiveMachSendRight::name())
        return IPC::handleMessage<Messages::TestWithStream::SendAndReceiveMachSendRight>(decoder, this, &TestWithStream::sendAndReceiveMachSendRight);
#endif
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(connection);
#if ENABLE(IPC_TESTING_API)
    if (connection.connection().ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled stream message %s to %" PRIu64, description(decoder.messageName()), decoder.destinationID());
}

} // namespace WebKit
