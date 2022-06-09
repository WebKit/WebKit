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
#include "TestWithCVPixelBuffer.h"

#if USE(AVFOUNDATION)
#include "ArgumentCodersCF.h"
#endif
#include "Decoder.h"
#include "HandleMessage.h"
#include "TestWithCVPixelBufferMessages.h"
#if USE(AVFOUNDATION)
#include <WebCore/CVUtilities.h>
#endif
#if USE(AVFOUNDATION)
#include <wtf/RetainPtr.h>
#endif

namespace WebKit {

void TestWithCVPixelBuffer::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    Ref protectedThis { *this };
#if USE(AVFOUNDATION)
    if (decoder.messageName() == Messages::TestWithCVPixelBuffer::SendCVPixelBuffer::name())
        return IPC::handleMessage<Messages::TestWithCVPixelBuffer::SendCVPixelBuffer>(connection, decoder, this, &TestWithCVPixelBuffer::sendCVPixelBuffer);
#endif
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif // ENABLE(IPC_TESTING_API)
    ASSERT_NOT_REACHED_WITH_MESSAGE("Unhandled message %s to %" PRIu64, IPC::description(decoder.messageName()), decoder.destinationID());
}

bool TestWithCVPixelBuffer::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    Ref protectedThis { *this };
#if USE(AVFOUNDATION)
    if (decoder.messageName() == Messages::TestWithCVPixelBuffer::ReceiveCVPixelBuffer::name())
        return IPC::handleMessage<Messages::TestWithCVPixelBuffer::ReceiveCVPixelBuffer>(connection, decoder, *replyEncoder, this, &TestWithCVPixelBuffer::receiveCVPixelBuffer);
#endif
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
