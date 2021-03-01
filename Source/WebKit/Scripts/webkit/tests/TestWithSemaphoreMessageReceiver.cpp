/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#include "TestWithSemaphore.h"

#include "Decoder.h"
#include "HandleMessage.h"
#include "IPCSemaphore.h"
#include "TestWithSemaphoreMessages.h"

namespace WebKit {

void TestWithSemaphore::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithSemaphore::SendSemaphore::name()) {
        IPC::handleMessage<Messages::TestWithSemaphore::SendSemaphore>(decoder, this, &TestWithSemaphore::sendSemaphore);
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    ASSERT_NOT_REACHED();
}

void TestWithSemaphore::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    auto protectedThis = makeRef(*this);
    if (decoder.messageName() == Messages::TestWithSemaphore::ReceiveSemaphore::name()) {
        IPC::handleMessage<Messages::TestWithSemaphore::ReceiveSemaphore>(decoder, *replyEncoder, this, &TestWithSemaphore::receiveSemaphore);
        return;
    }
    UNUSED_PARAM(connection);
    UNUSED_PARAM(decoder);
    UNUSED_PARAM(replyEncoder);
    ASSERT_NOT_REACHED();
}

} // namespace WebKit
