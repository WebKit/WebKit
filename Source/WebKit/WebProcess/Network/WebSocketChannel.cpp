/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or othe r materials provided with the distribution.
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
#include "WebSocketChannel.h"

#include "DataReference.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkSocketChannelMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/WebSocketChannel.h>
#include <WebCore/WebSocketChannelClient.h>
#include <pal/SessionID.h>

namespace WebKit {

Ref<WebSocketChannel> WebSocketChannel::create(WebCore::Document& document, WebCore::WebSocketChannelClient& client)
{
    return adoptRef(*new WebSocketChannel(document, client));
}

WebSocketChannel::WebSocketChannel(WebCore::Document& document, WebCore::WebSocketChannelClient& client)
    : m_document(makeWeakPtr(document))
    , m_client(makeWeakPtr(client))
{
}

WebSocketChannel::~WebSocketChannel()
{
}

IPC::Connection* WebSocketChannel::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

uint64_t WebSocketChannel::messageSenderDestinationID() const
{
    return identifier();
}

String WebSocketChannel::subprotocol()
{
    // FIXME: support subprotocol.
    return emptyString();
}

String WebSocketChannel::extensions()
{
    // FIXME: support extensions.
    return emptyString();
}

WebSocketChannel::ConnectStatus WebSocketChannel::connect(const URL& url, const String& protocol)
{
    if (!m_document)
        return ConnectStatus::KO;

    auto request = webSocketConnectRequest(*m_document, url);
    if (!request)
        return ConnectStatus::KO;

    if (request->url() != url && m_client)
        m_client->didUpgradeURL();

    MessageSender::send(Messages::NetworkConnectionToWebProcess::CreateSocketChannel { m_document->sessionID(), *request, protocol, identifier() });
    return ConnectStatus::OK;
}

WebSocketChannel::SendResult WebSocketChannel::send(const String& message)
{
    auto byteLength = message.sizeInBytes();
    m_bufferedAmount += byteLength;
    if (m_client)
        m_client->didUpdateBufferedAmount(m_bufferedAmount);

    CompletionHandler<void()> completionHandler = [this, protectedThis = makeRef(*this), byteLength] {
        ASSERT(m_bufferedAmount >= byteLength);
        m_bufferedAmount -= byteLength;
        if (m_client)
            m_client->didUpdateBufferedAmount(m_bufferedAmount);
    };
    sendWithAsyncReply(Messages::NetworkSocketChannel::SendString { message }, WTFMove(completionHandler));
    return SendSuccess;
}

WebSocketChannel::SendResult WebSocketChannel::send(const JSC::ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    m_bufferedAmount += byteLength;
    if (m_client)
        m_client->didUpdateBufferedAmount(m_bufferedAmount);

    CompletionHandler<void()> completionHandler = [this, protectedThis = makeRef(*this), byteLength] {
        ASSERT(m_bufferedAmount >= byteLength);
        m_bufferedAmount -= byteLength;
        if (m_client)
            m_client->didUpdateBufferedAmount(m_bufferedAmount);
    };
    sendWithAsyncReply(Messages::NetworkSocketChannel::SendData { IPC::DataReference { static_cast<const uint8_t*>(binaryData.data()) + byteOffset, byteLength } }, WTFMove(completionHandler));
    return SendSuccess;
}

WebSocketChannel::SendResult WebSocketChannel::send(WebCore::Blob&)
{
    notImplemented();
    return SendFail;
}

unsigned WebSocketChannel::bufferedAmount() const
{
    return m_bufferedAmount;
}

void WebSocketChannel::close(int code, const String& reason)
{
    m_isClosing = true;
    if (m_client)
        m_client->didStartClosingHandshake();

    ASSERT(code >= 0 || code == WebCore::WebSocketChannel::CloseEventCodeNotSpecified);

    MessageSender::send(Messages::NetworkSocketChannel::Close { code, reason });
}

void WebSocketChannel::fail(const String& reason)
{
    MessageSender::send(Messages::NetworkSocketChannel::Close { 0, reason });
}

void WebSocketChannel::disconnect()
{
    m_client = nullptr;
    m_document = nullptr;
    m_pendingTasks.clear();

    MessageSender::send(Messages::NetworkSocketChannel::Close { 0, { } });
}

void WebSocketChannel::didConnect()
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this] {
            didConnect();
        });
        return;
    }

    m_client->didConnect();
}

void WebSocketChannel::didReceiveText(const String& message)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, message] {
            didReceiveText(message);
        });
        return;
    }

    m_client->didReceiveMessage(message);
}

void WebSocketChannel::didReceiveBinaryData(const IPC::DataReference& data)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, data = data.vector()]() mutable {
            if (!m_isClosing && m_client)
                m_client->didReceiveBinaryData(WTFMove(data));
        });
        return;
    }
    m_client->didReceiveBinaryData(data.vector());
}

void WebSocketChannel::didClose(unsigned short code, const String& reason)
{
    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, code, reason] {
            didClose(code, reason);
        });
        return;
    }

    if (code == WebCore::WebSocketChannel::CloseEventCodeNormalClosure)
        m_client->didStartClosingHandshake();

    m_client->didClose(m_bufferedAmount, (m_isClosing || code == WebCore::WebSocketChannel::CloseEventCodeNormalClosure) ? WebCore::WebSocketChannelClient::ClosingHandshakeComplete : WebCore::WebSocketChannelClient::ClosingHandshakeIncomplete, code, reason);
}

void WebSocketChannel::didFail()
{
    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this] {
            didFail();
        });
        return;
    }

    m_client->didReceiveMessageError();
}

void WebSocketChannel::networkProcessCrashed()
{
    didFail();
}

void WebSocketChannel::suspend()
{
    m_isSuspended = true;
}

void WebSocketChannel::resume()
{
    m_isSuspended = false;
    while (!m_isSuspended && !m_pendingTasks.isEmpty())
        m_pendingTasks.takeFirst()();
}

void WebSocketChannel::enqueueTask(Function<void()>&& task)
{
    m_pendingTasks.append(WTFMove(task));
}

} // namespace WebKit
