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

#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkSocketChannelMessages.h"
#include "WebProcess.h"
#include <WebCore/AdvancedPrivacyProtections.h>
#include <WebCore/Blob.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/ThreadableWebSocketChannel.h>
#include <WebCore/WebSocketChannelClient.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

Ref<WebSocketChannel> WebSocketChannel::create(WebPageProxyIdentifier webPageProxyID, Document& document, WebSocketChannelClient& client)
{
    return adoptRef(*new WebSocketChannel(webPageProxyID, document, client));
}

void WebSocketChannel::notifySendFrame(WebSocketFrame::OpCode opCode, std::span<const uint8_t> data)
{
    WebSocketFrame frame(opCode, true, false, true, data);
    m_inspector.didSendWebSocketFrame(frame);
}

Ref<NetworkSendQueue> WebSocketChannel::createMessageQueue(Document& document, WebSocketChannel& channel)
{
    return NetworkSendQueue::create(document, [weakChannel = WeakPtr { channel }](auto& utf8String) {
        RefPtr channel = weakChannel.get();
        if (!channel)
            return;
        auto data = utf8String.span();
        channel->notifySendFrame(WebSocketFrame::OpCode::OpCodeText, byteCast<uint8_t>(data));
        channel->sendMessageInternal(Messages::NetworkSocketChannel::SendString { byteCast<uint8_t>(data) }, utf8String.length());
    }, [weakChannel = WeakPtr { channel }](auto span) {
        RefPtr channel = weakChannel.get();
        if (!channel)
            return;
        channel->notifySendFrame(WebSocketFrame::OpCode::OpCodeBinary, span);
        channel->sendMessageInternal(Messages::NetworkSocketChannel::SendData { span }, span.size());
    }, [weakChannel = WeakPtr { channel }](ExceptionCode exceptionCode) {
        RefPtr channel = weakChannel.get();
        if (!channel)
            return NetworkSendQueue::Continue::No;
        auto code = static_cast<int>(exceptionCode);
        channel->fail(makeString("Failed to load Blob: exception code = "_s, code));
        return NetworkSendQueue::Continue::No;
    });
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
    return identifier().toUInt64();
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
    RefPtr document = m_document.get();
    if (!document)
        return ConnectStatus::KO;

    if (WebProcess::singleton().webSocketChannelManager().hasReachedSocketLimit()) {
        auto reason = "Connection failed: Insufficient resources"_s;
        logErrorMessage(reason);
        if (RefPtr client = m_client.get())
            client->didReceiveMessageError(String { reason });
        return ConnectStatus::KO;
    }

    auto request = webSocketConnectRequest(*document, url);
    if (!request)
        return ConnectStatus::KO;

    if (request->url() != url) {
        if (RefPtr client = m_client.get())
            client->didUpgradeURL();
    }

    OptionSet<AdvancedPrivacyProtections> advancedPrivacyProtections;
    bool allowPrivacyProxy { true };
    std::optional<PageIdentifier> pageID;
    StoredCredentialsPolicy storedCredentialsPolicy { StoredCredentialsPolicy::Use };
    RefPtr frame = document->frame();
    RefPtr mainFrame = document->localMainFrame();
    if (!mainFrame)
        return ConnectStatus::KO;
    auto frameID = frame ? std::optional(frame->frameID()) : std::nullopt;
    pageID = mainFrame->pageID();
    if (RefPtr policySourceDocumentLoader = mainFrame->document() ? mainFrame->protectedDocument()->loader() : nullptr) {
        if (!policySourceDocumentLoader->request().url().hasSpecialScheme() && frame->document()->url().protocolIsInHTTPFamily())
            policySourceDocumentLoader = frame->protectedDocument()->loader();

        if (policySourceDocumentLoader) {
            allowPrivacyProxy = policySourceDocumentLoader->allowPrivacyProxy();
            advancedPrivacyProtections = policySourceDocumentLoader->advancedPrivacyProtections();
        }
    }
    if (auto* page = mainFrame->page())
        storedCredentialsPolicy = page->canUseCredentialStorage() ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    m_inspector.didCreateWebSocket(url);
    m_url = request->url();
    MessageSender::send(Messages::NetworkConnectionToWebProcess::CreateSocketChannel { *request, protocol, identifier(), m_webPageProxyID, frameID, pageID, document->clientOrigin(), WebProcess::singleton().hadMainFrameMainResourcePrivateRelayed(), allowPrivacyProxy, advancedPrivacyProtections, storedCredentialsPolicy });
    return ConnectStatus::OK;
}

bool WebSocketChannel::increaseBufferedAmount(size_t byteLength)
{
    if (!byteLength)
        return true;

    CheckedSize checkedNewBufferedAmount = m_bufferedAmount;
    checkedNewBufferedAmount += byteLength;
    if (checkedNewBufferedAmount.hasOverflowed()) [[unlikely]] {
        fail("Failed to send WebSocket frame: buffer has no more space"_s);
        return false;
    }

    m_bufferedAmount = checkedNewBufferedAmount;
    if (RefPtr client = m_client.get())
        client->didUpdateBufferedAmount(m_bufferedAmount);
    return true;
}

void WebSocketChannel::decreaseBufferedAmount(size_t byteLength)
{
    if (!byteLength)
        return;

    ASSERT(m_bufferedAmount >= byteLength);
    m_bufferedAmount -= byteLength;
    if (RefPtr client = m_client.get())
        client->didUpdateBufferedAmount(m_bufferedAmount);
}

template<typename T> void WebSocketChannel::sendMessageInternal(T&& message, size_t byteLength)
{
    CompletionHandler<void()> completionHandler = [this, protectedThis = Ref { *this }, byteLength] {
        decreaseBufferedAmount(byteLength);
    };
    sendWithAsyncReply(std::forward<T>(message), WTFMove(completionHandler));
}

void WebSocketChannel::send(CString&& message)
{
    if (!increaseBufferedAmount(message.length()))
        return;

    m_messageQueue->enqueue(WTFMove(message));
}

void WebSocketChannel::send(const JSC::ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!increaseBufferedAmount(byteLength))
        return;

    m_messageQueue->enqueue(binaryData, byteOffset, byteLength);
}

void WebSocketChannel::send(Blob& blob)
{
    auto byteLength = blob.size();
    if (!blob.size())
        return send(JSC::ArrayBuffer::create(byteLength, 1), 0, 0);

    if (!increaseBufferedAmount(byteLength))
        return;

    m_messageQueue->enqueue(blob);
}

void WebSocketChannel::close(int code, const String& reason)
{
    // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    Ref protectedThis { *this };

    m_isClosing = true;
    if (RefPtr client = m_client.get())
        client->didStartClosingHandshake();

    ASSERT(code >= 0 || code == WebCore::ThreadableWebSocketChannel::CloseEventCodeNotSpecified);

    WebSocketFrame closingFrame(WebSocketFrame::OpCodeClose, true, false, true);
    m_inspector.didSendWebSocketFrame(closingFrame);

    MessageSender::send(Messages::NetworkSocketChannel::Close { code, reason });
}

void WebSocketChannel::fail(String&& reason)
{
    // The client can close the channel, potentially removing the last reference.
    Ref protectedThis { *this };

    logErrorMessage(reason);
    if (RefPtr client = m_client.get())
        client->didReceiveMessageError(String { reason });

    if (m_isClosing)
        return;

    MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::ThreadableWebSocketChannel::CloseEventCodeGoingAway, reason });
    didClose(WebCore::ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure, { });
}

void WebSocketChannel::disconnect()
{
    m_client = nullptr;
    m_document = nullptr;
    m_messageQueue->clear();

    m_inspector.didCloseWebSocket();

    MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::ThreadableWebSocketChannel::CloseEventCodeGoingAway, { } });
}

void WebSocketChannel::didConnect(String&& subprotocol, String&& extensions)
{
    if (m_isClosing)
        return;

    RefPtr client = m_client.get();
    if (!client)
        return;

    m_subprotocol = WTFMove(subprotocol);
    m_extensions = WTFMove(extensions);
    client->didConnect();
}

void WebSocketChannel::didReceiveText(String&& message)
{
    if (m_isClosing)
        return;

    if (RefPtr client = m_client.get())
        client->didReceiveMessage(WTFMove(message));
}

void WebSocketChannel::didReceiveBinaryData(std::span<const uint8_t> data)
{
    if (m_isClosing)
        return;

    if (RefPtr client = m_client.get())
        client->didReceiveBinaryData({ data });
}

void WebSocketChannel::didClose(unsigned short code, String&& reason)
{
    RefPtr client = m_client.get();
    if (!client)
        return;

    // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    Ref protectedThis { *this };

    bool receivedClosingHandshake = code != WebCore::ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure;
    if (receivedClosingHandshake)
        client->didStartClosingHandshake();

    client->didClose(m_bufferedAmount, (m_isClosing || receivedClosingHandshake) ? WebCore::WebSocketChannelClient::ClosingHandshakeComplete : WebCore::WebSocketChannelClient::ClosingHandshakeIncomplete, code, reason);
}

void WebSocketChannel::logErrorMessage(const String& errorMessage)
{
    RefPtr document = m_document.get();
    if (!document)
        return;

    String consoleMessage;
    if (!m_url.isNull())
        consoleMessage = makeString("WebSocket connection to '"_s, m_url.string(), "' failed: "_s, errorMessage);
    else
        consoleMessage = makeString("WebSocket connection failed: "_s, errorMessage);
    document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, consoleMessage);
}

void WebSocketChannel::didReceiveMessageError(String&& errorMessage)
{
    RefPtr client = m_client.get();
    if (!client)
        return;

    logErrorMessage(errorMessage);
    client->didReceiveMessageError(WTFMove(errorMessage));
}

void WebSocketChannel::networkProcessCrashed()
{
    fail("WebSocket network error: Network process crashed."_s);
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
