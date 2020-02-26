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
#include <WebCore/Blob.h>
#include <WebCore/Document.h>
#include <WebCore/WebSocketChannel.h>
#include <WebCore/WebSocketChannelClient.h>
#include <wtf/CheckedArithmetic.h>

using namespace WebCore;

namespace WebKit {

Ref<WebSocketChannel> WebSocketChannel::create(Document& document, WebSocketChannelClient& client)
{
    return adoptRef(*new WebSocketChannel(document, client));
}

NetworkSendQueue WebSocketChannel::createMessageQueue(Document& document, WebSocketChannel& channel)
{
    return { document, [&channel](auto& string) {
        auto byteLength = string.sizeInBytes();
        channel.sendMessage(Messages::NetworkSocketChannel::SendString { string }, byteLength);
    }, [&channel](const char* data, size_t byteLength) {
        channel.sendMessage(Messages::NetworkSocketChannel::SendData { IPC::DataReference { reinterpret_cast<const uint8_t*>(data), byteLength } }, byteLength);
    }, [&channel](auto errorCode) {
        channel.fail(makeString("Failed to load Blob: error code = ", errorCode));
        return NetworkSendQueue::Continue::No;
    } };
}

WebSocketChannel::WebSocketChannel(Document& document, WebSocketChannelClient& client)
    : m_document(makeWeakPtr(document))
    , m_client(makeWeakPtr(client))
    , m_messageQueue(createMessageQueue(document, *this))
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
    return m_subprotocol.isNull() ? emptyString() : m_subprotocol;
}

String WebSocketChannel::extensions()
{
    return m_extensions.isNull() ? emptyString() : m_extensions;
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

    MessageSender::send(Messages::NetworkConnectionToWebProcess::CreateSocketChannel { *request, protocol, identifier() });
    return ConnectStatus::OK;
}

bool WebSocketChannel::increaseBufferedAmount(size_t byteLength)
{
    if (!byteLength)
        return true;

    Checked<size_t, RecordOverflow> checkedNewBufferedAmount = m_bufferedAmount;
    checkedNewBufferedAmount += byteLength;
    if (UNLIKELY(checkedNewBufferedAmount.hasOverflowed())) {
        fail("Failed to send WebSocket frame: buffer has no more space");
        return false;
    }

    m_bufferedAmount = checkedNewBufferedAmount.unsafeGet();
    if (m_client)
        m_client->didUpdateBufferedAmount(m_bufferedAmount);
    return true;
}

void WebSocketChannel::decreaseBufferedAmount(size_t byteLength)
{
    if (!byteLength)
        return;

    ASSERT(m_bufferedAmount >= byteLength);
    m_bufferedAmount -= byteLength;
    if (m_client)
        m_client->didUpdateBufferedAmount(m_bufferedAmount);
}

template<typename T> void WebSocketChannel::sendMessage(T&& message, size_t byteLength)
{
    CompletionHandler<void()> completionHandler = [this, protectedThis = makeRef(*this), byteLength] {
        decreaseBufferedAmount(byteLength);
    };
    sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler));
}

WebSocketChannel::SendResult WebSocketChannel::send(const String& message)
{
    auto byteLength = message.sizeInBytes();
    if (!increaseBufferedAmount(byteLength))
        return SendFail;

    m_messageQueue.enqueue(message);
    return SendSuccess;
}

WebSocketChannel::SendResult WebSocketChannel::send(const JSC::ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!increaseBufferedAmount(byteLength))
        return SendFail;

    m_messageQueue.enqueue(binaryData, byteOffset, byteLength);
    return SendSuccess;
}

WebSocketChannel::SendResult WebSocketChannel::send(Blob& blob)
{
    auto byteLength = blob.size();
    if (!blob.size())
        return send(JSC::ArrayBuffer::create(byteLength, 1), 0, 0);

    if (!increaseBufferedAmount(byteLength))
        return SendFail;

    m_messageQueue.enqueue(blob);
    return SendSuccess;
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
    if (m_client)
        m_client->didReceiveMessageError();

    if (!m_isClosing)
        MessageSender::send(Messages::NetworkSocketChannel::Close { 0, reason });
}

void WebSocketChannel::disconnect()
{
    m_client = nullptr;
    m_document = nullptr;
    m_pendingTasks.clear();
    m_messageQueue.clear();

    MessageSender::send(Messages::NetworkSocketChannel::Close { 0, { } });
}

void WebSocketChannel::didConnect(String&& subprotocol, String&& extensions)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, subprotocol = WTFMove(subprotocol), extensions = WTFMove(extensions)] () mutable {
            didConnect(WTFMove(subprotocol), WTFMove(extensions));
        });
        return;
    }

    m_subprotocol = WTFMove(subprotocol);
    m_extensions = WTFMove(extensions);
    m_client->didConnect();
}

void WebSocketChannel::didReceiveText(String&& message)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, message = WTFMove(message)] () mutable {
            didReceiveText(WTFMove(message));
        });
        return;
    }

    m_client->didReceiveMessage(message);
}

void WebSocketChannel::didReceiveBinaryData(IPC::DataReference&& data)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, data = data.vector()] () mutable {
            if (!m_isClosing && m_client)
                m_client->didReceiveBinaryData(WTFMove(data));
        });
        return;
    }
    m_client->didReceiveBinaryData(data.vector());
}

void WebSocketChannel::didClose(unsigned short code, String&& reason)
{
    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, code, reason = WTFMove(reason)] () mutable {
            didClose(code, WTFMove(reason));
        });
        return;
    }

    if (code == WebCore::WebSocketChannel::CloseEventCodeNormalClosure)
        m_client->didStartClosingHandshake();

    m_client->didClose(m_bufferedAmount, (m_isClosing || code == WebCore::WebSocketChannel::CloseEventCodeNormalClosure) ? WebCore::WebSocketChannelClient::ClosingHandshakeComplete : WebCore::WebSocketChannelClient::ClosingHandshakeIncomplete, code, reason);
}

void WebSocketChannel::didReceiveMessageError(String&& errorMessage)
{
    if (!m_client)
        return;

    if (m_isSuspended) {
        enqueueTask([this, errorMessage = WTFMove(errorMessage)] () mutable {
            didReceiveMessageError(WTFMove(errorMessage));
        });
        return;
    }

    if (m_document)
        m_document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, errorMessage);

    m_client->didReceiveMessageError();
}

void WebSocketChannel::networkProcessCrashed()
{
    didReceiveMessageError("WebSocket network error: Network process crashed."_s);
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
