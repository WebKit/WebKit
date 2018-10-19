/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

#include "WebPage.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "HandleMessage.h"
#if ENABLE(TEST_FEATURE)
#include "TestTwoStateEnum.h"
#endif
#include "WebPageMessages.h"
#include <wtf/text/WTFString.h>

namespace Messages {

namespace WebPage {

#if ENABLE(TEST_FEATURE)

void TestAsyncMessage::callReply(IPC::Decoder& decoder, CompletionHandler<void(uint64_t&&)>&& completionHandler)
{
    std::optional<uint64_t> result;
    decoder >> result;
    if (!result) {
        ASSERT_NOT_REACHED();
        return;
    }
    completionHandler(WTFMove(*result));
}

void TestAsyncMessage::cancelReply(CompletionHandler<void(uint64_t&&)>&& completionHandler)
{
    completionHandler({ });
}

void TestAsyncMessage::send(std::unique_ptr<IPC::Encoder>&& encoder, IPC::Connection& connection, uint64_t result)
{
    *encoder << result;
    connection.sendSyncReply(WTFMove(encoder));
}

#endif

} // namespace WebPage

} // namespace Messages

namespace WebKit {

void WebPage::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageName() == Messages::WebPage::LoadURL::name()) {
        IPC::handleMessage<Messages::WebPage::LoadURL>(decoder, this, &WebPage::loadURL);
        return;
    }
#if ENABLE(TEST_FEATURE)
    if (decoder.messageName() == Messages::WebPage::TestAsyncMessage::name()) {
        IPC::handleMessageAsync<Messages::WebPage::TestAsyncMessage>(connection, decoder, this, &WebPage::testAsyncMessage);
        return;
    }
#endif
    WebPageBase::didReceiveMessage(connection, decoder);
}

} // namespace WebKit

