/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebConnection.h"

#include "ArgumentCoders.h"
#include "DataReference.h"
#include "WebConnectionMessages.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

WebConnection::WebConnection()
{
}

WebConnection::~WebConnection()
{
}

void WebConnection::initializeConnectionClient(const WKConnectionClientBase* client)
{
    m_client.initialize(client);
}

void WebConnection::postMessage(const String& messageName, API::Object* messageBody)
{
    if (!hasValidConnection())
        return;

    auto encoder = std::make_unique<IPC::MessageEncoder>(Messages::WebConnection::HandleMessage::receiverName(), Messages::WebConnection::HandleMessage::name(), 0);
    encoder->encode(messageName);
    encodeMessageBody(*encoder, messageBody);

    sendMessage(WTF::move(encoder), 0);
}

void WebConnection::didClose()
{
    m_client.didClose(this);
}

void WebConnection::didReceiveMessage(IPC::Connection* connection, IPC::MessageDecoder& decoder)
{
    didReceiveWebConnectionMessage(connection, decoder);
}

void WebConnection::handleMessage(IPC::MessageDecoder& decoder)
{
    String messageName;
    if (!decoder.decode(messageName))
        return;

    RefPtr<API::Object> messageBody;
    if (!decodeMessageBody(decoder, messageBody))
        return;

    m_client.didReceiveMessage(this, messageName, messageBody.get());
}

} // namespace WebKit
