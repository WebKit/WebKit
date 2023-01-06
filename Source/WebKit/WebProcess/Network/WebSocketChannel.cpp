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

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkSocketChannelMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/Blob.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/NetworkConnectionIntegrity.h>
#include <WebCore/Page.h>
#include <WebCore/WebSocketChannel.h>
#include <WebCore/WebSocketChannelClient.h>
#include <wtf/CheckedArithmetic.h>

namespace WebKit {
using namespace WebCore;

Ref<WebSocketChannel> WebSocketChannel::create(WebPageProxyIdentifier webPageProxyID, Document& document, WebSocketChannelClient& client)
{
    return adoptRef(*new WebSocketChannel(webPageProxyID, document, client));
}

void WebSocketChannel::notifySendFrame(WebSocketFrame::OpCode opCode, const uint8_t* data, size_t length)
{
    WebSocketFrame frame(opCode, true, false, true, data, length);
    m_inspector.didSendWebSocketFrame(frame);
}

NetworkSendQueue WebSocketChannel::createMessageQueue(Document& document, WebSocketChannel& channel)
{
    return { document, [&channel](auto& utf8String) {
        channel.notifySendFrame(WebSocketFrame::OpCode::OpCodeText, utf8String.dataAsUInt8Ptr(), utf8String.length());
        channel.sendMessage(Messages::NetworkSocketChannel::SendString { IPC::DataReference { utf8String.dataAsUInt8Ptr(), utf8String.length() } }, utf8String.length());
    }, [&channel](auto& span) {
        channel.notifySendFrame(WebSocketFrame::OpCode::OpCodeBinary, span.data(), span.size());
        channel.sendMessage(Messages::NetworkSocketChannel::SendData { IPC::DataReference { span.data(), span.size() } }, span.size());
    }, [&channel](ExceptionCode exceptionCode) {
        auto code = static_cast<int>(exceptionCode);
        channel.fail(makeString("Failed to load Blob: exception code = ", code));
        return NetworkSendQueue::Continue::No;
    } };
}

WebSocketChannel::WebSocketChannel(WebPageProxyIdentifier webPageProxyID, Document& document, WebSocketChannelClient& client)
    : m_document(document)
    , m_client(client)
    , m_messageQueue(createMessageQueue(document, *this))
    , m_inspector(document)
    , m_webPageProxyID(webPageProxyID)
{
    WebProcess::singleton().webSocketChannelManager().addChannel(*this);
}

WebSocketChannel::~WebSocketChannel()
{
    WebProcess::singleton().webSocketChannelManager().removeChannel(*this);
}

IPC::Connection* WebSocketChannel::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

uint64_t WebSocketChannel::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
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

    if (WebProcess::singleton().webSocketChannelManager().hasReachedSocketLimit()) {
        auto reason = "Connection failed: Insufficient resources"_s;
        logErrorMessage(reason);
        if (m_client)
            m_client->didReceiveMessageError(String { reason });
        return ConnectStatus::KO;
    }

    auto request = webSocketConnectRequest(*m_document, url);
    if (!request)
        return ConnectStatus::KO;

    if (request->url() != url && m_client)
        m_client->didUpgradeURL();

    OptionSet<NetworkConnectionIntegrity> networkConnectionIntegrityPolicy;
    bool allowPrivacyProxy { true };
    if (auto* frame = m_document ? m_document->frame() : nullptr) {
        if (auto* mainFrameDocumentLoader = frame->mainFrame().document() ? frame->mainFrame().document()->loader() : nullptr) {
            allowPrivacyProxy = mainFrameDocumentLoader->allowPrivacyProxy();
            networkConnectionIntegrityPolicy = mainFrameDocumentLoader->networkConnectionIntegrityPolicy();
        }
    }

    m_inspector.didCreateWebSocket(url);
    m_url = request->url();
    MessageSender::send(Messages::NetworkConnectionToWebProcess::CreateSocketChannel { *request, protocol, m_identifier, m_webPageProxyID, m_document->clientOrigin(), WebProcess::singleton().hadMainFrameMainResourcePrivateRelayed(), allowPrivacyProxy, networkConnectionIntegrityPolicy });
    return ConnectStatus::OK;
}

bool WebSocketChannel::increaseBufferedAmount(size_t byteLength)
{
    if (!byteLength)
        return true;

    CheckedSize checkedNewBufferedAmount = m_bufferedAmount;
    checkedNewBufferedAmount += byteLength;
    if (UNLIKELY(checkedNewBufferedAmount.hasOverflowed())) {
        fail("Failed to send WebSocket frame: buffer has no more space"_s);
        return false;
    }

    m_bufferedAmount = checkedNewBufferedAmount;
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
    CompletionHandler<void()> completionHandler = [this, protectedThis = Ref { *this }, byteLength] {
        decreaseBufferedAmount(byteLength);
    };
    sendWithAsyncReply(WTFMove(message), WTFMove(completionHandler));
}

WebSocketChannel::SendResult WebSocketChannel::send(CString&& message)
{
    if (!increaseBufferedAmount(message.length()))
        return SendFail;

    m_messageQueue.enqueue(WTFMove(message));
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
    // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    Ref protectedThis { *this };

    m_isClosing = true;
    if (m_client)
        m_client->didStartClosingHandshake();

    ASSERT(code >= 0 || code == WebCore::WebSocketChannel::CloseEventCodeNotSpecified);

    WebSocketFrame closingFrame(WebSocketFrame::OpCodeClose, true, false, true);
    m_inspector.didSendWebSocketFrame(closingFrame);

    MessageSender::send(Messages::NetworkSocketChannel::Close { code, reason });
}

void WebSocketChannel::fail(String&& reason)
{
    // The client can close the channel, potentially removing the last reference.
    Ref protectedThis { *this };

    logErrorMessage(reason);
    if (m_client)
        m_client->didReceiveMessageError(String { reason });

    if (m_isClosing)
        return;

    MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::WebSocketChannel::CloseEventCodeGoingAway, reason });
    didClose(WebCore::WebSocketChannel::CloseEventCodeAbnormalClosure, { });
}

void WebSocketChannel::disconnect()
{
    m_client = nullptr;
    m_document = nullptr;
    m_messageQueue.clear();

    m_inspector.didCloseWebSocket();

    MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::WebSocketChannel::CloseEventCodeGoingAway, { } });
}

void WebSocketChannel::didConnect(String&& subprotocol, String&& extensions)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

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

    m_client->didReceiveMessage(WTFMove(message));
}

void WebSocketChannel::didReceiveBinaryData(IPC::DataReference&& data)
{
    if (m_isClosing)
        return;

    if (!m_client)
        return;

    m_client->didReceiveBinaryData({ data });
}

void WebSocketChannel::didClose(unsigned short code, String&& reason)
{
    if (!m_client)
        return;

    // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    Ref protectedThis { *this };

    bool receivedClosingHandshake = code != WebCore::WebSocketChannel::CloseEventCodeAbnormalClosure;
    if (receivedClosingHandshake)
        m_client->didStartClosingHandshake();

    m_client->didClose(m_bufferedAmount, (m_isClosing || receivedClosingHandshake) ? WebCore::WebSocketChannelClient::ClosingHandshakeComplete : WebCore::WebSocketChannelClient::ClosingHandshakeIncomplete, code, reason);
}

void WebSocketChannel::logErrorMessage(const String& errorMessage)
{
    if (!m_document)
        return;

    String consoleMessage;
    if (!m_url.isNull())
        consoleMessage = makeString("WebSocket connection to '", m_url.string(), "' failed: ", errorMessage);
    else
        consoleMessage = makeString("WebSocket connection failed: ", errorMessage);
    m_document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, consoleMessage);
}

void WebSocketChannel::didReceiveMessageError(String&& errorMessage)
{
    if (!m_client)
        return;

    logErrorMessage(errorMessage);
    m_client->didReceiveMessageError(WTFMove(errorMessage));
}

void WebSocketChannel::networkProcessCrashed()
{
    didReceiveMessageError("WebSocket network error: Network process crashed."_s);
}

void WebSocketChannel::suspend()
{
}

void WebSocketChannel::resume()
{
}

void WebSocketChannel::didSendHandshakeRequest(ResourceRequest&& request)
{
    m_inspector.willSendWebSocketHandshakeRequest(request);
    m_handshakeRequest = WTFMove(request);
}

void WebSocketChannel::didReceiveHandshakeResponse(ResourceResponse&& response)
{
    m_inspector.didReceiveWebSocketHandshakeResponse(response);
    m_handshakeResponse = WTFMove(response);
}

} // namespace WebKit
