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
#include "Connection.h"
#include "DataReference.h"
#include "WebConnectionMessages.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

WebConnection::WebConnection(PassRefPtr<CoreIPC::Connection> connection)
    : m_connection(connection)
{
}

WebConnection::~WebConnection()
{
}

void WebConnection::initializeConnectionClient(const WKConnectionClient* client)
{
    m_client.initialize(client);
}

void WebConnection::postMessage(const String& messageName, APIObject* messageBody)
{
    if (!m_connection)
        return;

    OwnPtr<CoreIPC::ArgumentEncoder> messageData = CoreIPC::ArgumentEncoder::create();
    messageData->encode(messageName);
    encodeMessageBody(*messageData, messageBody);

    m_connection->send(Messages::WebConnection::HandleMessage(CoreIPC::DataReference(messageData->buffer(), messageData->bufferSize())), 0);
}

void WebConnection::handleMessage(const CoreIPC::DataReference& messageData)
{
    OwnPtr<CoreIPC::ArgumentDecoder> decoder = CoreIPC::ArgumentDecoder::create(messageData.data(), messageData.size());

    String messageName;
    if (!decoder->decode(messageName))
        return;

    RefPtr<APIObject> messageBody;
    if (!decodeMessageBody(*decoder, messageBody))
        return;

    m_client.didReceiveMessage(this, messageName, messageBody.get());
}

void WebConnection::invalidate()
{
    m_connection->invalidate();
    m_connection = nullptr;
}

} // namespace WebKit
