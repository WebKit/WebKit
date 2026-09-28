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

#include "Connection.h"
#include "MessageSenderInlines.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkSocketChannelMessages.h"
#include "WebProcess.h"
#include "WebSocketChannelMessages.h"
#include <WebCore/AdvancedPrivacyProtections.h>
#include <WebCore/Blob.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/MixedContentChecker.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/ThreadableWebSocketChannel.h>
#include <WebCore/WebSocketChannelClient.h>
#include <WebCore/WorkerGlobalScope.h>
#include <WebCore/WorkerLoaderProxy.h>
#include <WebCore/WorkerThread.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
using namespace WebCore;

Ref<WebSocketChannel> WebSocketChannel::create(WebPageProxyIdentifier webPageProxyID, Document& document, WebSocketChannelClient& client, IsInitiatedByDedicatedWorker isInitiatedByDedicatedWorker)
{
    return adoptRef(*new WebSocketChannel(webPageProxyID, document, client, isInitiatedByDedicatedWorker, nullptr));
}

Ref<WebSocketChannel> WebSocketChannel::create(WebPageProxyIdentifier webPageProxyID, WorkerGlobalScope& scope, WebSocketChannelClient& client, IsInitiatedByDedicatedWorker isInitiatedByDedicatedWorker, Ref<IPC::Connection>&& connection)
{
    ASSERT(!isMainRunLoop());
    return adoptRef(*new WebSocketChannel(webPageProxyID, scope, client, isInitiatedByDedicatedWorker, WTF::move(connection)));
}

void WebSocketChannel::notifySendFrame(WebSocketFrame::OpCode opCode, std::span<const uint8_t> data)
{
    WebSocketFrame frame(opCode, true, false, true, data);
    m_inspector.didSendWebSocketFrame(frame);
}

Ref<NetworkSendQueue> WebSocketChannel::createMessageQueue(ScriptExecutionContext& context, WebSocketChannel& channel)
{
    return NetworkSendQueue::create(context, [weakChannel = ThreadSafeWeakPtr<WebSocketChannel> { channel }](auto& utf8String) {
        RefPtr channel = weakChannel.get();
        if (!channel)
            return;
        auto data = utf8String.span();
        channel->notifySendFrame(WebSocketFrame::OpCode::OpCodeText, asByteSpan(data));
        channel->sendMessageInternal(Messages::NetworkSocketChannel::SendString { asByteSpan(data) }, utf8String.length());
    }, [weakChannel = ThreadSafeWeakPtr<WebSocketChannel> { channel }](auto span) {
        RefPtr channel = weakChannel.get();
        if (!channel)
            return;
        channel->notifySendFrame(WebSocketFrame::OpCode::OpCodeBinary, span);
        channel->sendMessageInternal(Messages::NetworkSocketChannel::SendData { span }, span.size());
    }, [weakChannel = ThreadSafeWeakPtr<WebSocketChannel> { channel }](ExceptionCode exceptionCode) {
        RefPtr channel = weakChannel.get();
        if (!channel)
            return NetworkSendQueue::Continue::No;
        auto code = static_cast<int>(exceptionCode);
        channel->fail(makeString("Failed to load Blob: exception code = "_s, code));
        return NetworkSendQueue::Continue::No;
    });
}

WebSocketChannel::WebSocketChannel(WebPageProxyIdentifier webPageProxyID, ScriptExecutionContext& context, WebSocketChannelClient& client, IsInitiatedByDedicatedWorker isInitiatedByDedicatedWorker, RefPtr<IPC::Connection>&& connection)
    : m_context(context)
    , m_connection(WTF::move(connection))
    , m_workerContextIdentifier(m_connection ? std::make_optional(m_context->identifier()) : std::nullopt)
    , m_client(client)
    , m_messageQueue(createMessageQueue(context, *this))
    , m_inspector(context)
    , m_webPageProxyID(webPageProxyID)
    , m_isInitiatedByDedicatedWorker(isInitiatedByDedicatedWorker)
{
    WebProcess::singleton().webSocketChannelManager().addChannel(*this);
}

WebSocketChannel::~WebSocketChannel()
{
    // Ensure the network process tears down its NetworkSocketChannel even when
    // none of the explicit close/disconnect paths ran. This happens for worker
    // WebSockets on a peer-initiated close: WorkerThreadableWebSocketChannel::Peer::didClose
    // nulls its RefPtr<WebSocketChannel> before the worker can call disconnect(),
    // so without this the Close IPC would never be sent and the network-side
    // channel would leak. m_needsToCallClose is only true once connect()
    // successfully sent a CreateSocketChannel IPC and Close hasn't been sent
    // since, so the destructor doesn't send a stray Close for channels the
    // network process never knew about.
    if (m_needsToCallClose)
        MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::ThreadableWebSocketChannel::CloseEventCodeGoingAway, { } });

    removeMessageReceiverIfNeeded();

    WebProcess::singleton().webSocketChannelManager().removeChannel(*this);
}

void WebSocketChannel::addMessageReceiverIfNeeded(ScriptExecutionContextIdentifier workerContextIdentifier)
{
    ASSERT(isMainRunLoop());
    if (!m_connection || m_messageReceiverAdded)
        return;

    m_messageReceiverAdded = true;
    m_connection->addScriptExecutionContextMessageReceiver(Messages::WebSocketChannel::messageReceiverName(), workerContextIdentifier, *this, identifier().toUInt64());
}

void WebSocketChannel::removeMessageReceiverIfNeeded()
{
    if (!m_connection || !m_messageReceiverAdded)
        return;

    m_connection->removeScriptExecutionContextMessageReceiver(Messages::WebSocketChannel::messageReceiverName(), identifier().toUInt64());
    m_messageReceiverAdded = false;
}

IPC::Connection* WebSocketChannel::messageSenderConnection() const
{
    if (m_connection)
        return m_connection.get();

    ASSERT(isMainRunLoop());
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

auto WebSocketChannel::createConnectParameters(Document& document, const URL& url, WebSocketChannelIdentifier progressIdentifier) -> std::optional<ConnectParameters>
{
    ASSERT(isMainRunLoop());

    auto request = webSocketConnectRequest(document, url);
    if (!request)
        return std::nullopt;

    RefPtr frame = document.frame();
    if (!frame)
        return std::nullopt;

    ConnectParameters parameters;
    parameters.didUpgradeURL = request->url() != url;

    if (RefPtr page = frame->page())
        parameters.storedCredentialsPolicy = page->canUseCredentialStorage() ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    Ref mainFrame = frame->mainFrame();
    Ref policySourceFrame = [&] -> Ref<Frame> {
        if (!WTF::URLParser::isSpecialScheme(mainFrame->frameURLProtocol()) && document.url().protocolIsInHTTPFamily())
            return *frame;
        return mainFrame;
    }();

    WebSocketChannelInspector::didCreateWebSocket(document, progressIdentifier, url);
    WebSocketChannelInspector::willSendWebSocketHandshakeRequest(document, progressIdentifier, *request);

    parameters.request = WTF::move(*request);
    parameters.clientOrigin = document.clientOrigin();
    parameters.frameIdentifier = frame->frameID();
    parameters.pageIdentifier = frame->pageID();
    parameters.advancedPrivacyProtections = policySourceFrame->advancedPrivacyProtections();
    parameters.hadMainFrameMainResourcePrivateRelayed = WebProcess::singleton().hadMainFrameMainResourcePrivateRelayed();
    parameters.allowPrivacyProxy = policySourceFrame->allowPrivacyProxy();

    return parameters;
}

auto WebSocketChannel::createConnectParametersForWorker(Document& document, const URL& url, WebSocketChannelIdentifier progressIdentifier) -> std::optional<ConnectParameters>
{
    auto parameters = createConnectParameters(document, url, progressIdentifier);
    if (!parameters)
        return std::nullopt;

    return ConnectParameters {
        WTF::move(parameters->request).isolatedCopy(),
        WTF::move(parameters->clientOrigin).isolatedCopy(),
        parameters->frameIdentifier,
        parameters->pageIdentifier,
        parameters->storedCredentialsPolicy,
        parameters->advancedPrivacyProtections,
        parameters->hadMainFrameMainResourcePrivateRelayed,
        parameters->allowPrivacyProxy,
        parameters->didUpgradeURL
    };
}

WebSocketChannel::ConnectStatus WebSocketChannel::connect(const URL& url, const String& protocol)
{
    RefPtr context = m_context.get();
    if (!context)
        return ConnectStatus::KO;

    if (is<WorkerGlobalScope>(context.get())) {
        CheckedPtr loaderProxy = downcast<WorkerGlobalScope>(context.get())->thread()->workerLoaderProxy();
        if (!loaderProxy)
            return ConnectStatus::KO;

        // The only main thread work a worker channel does: snapshot what the handshake needs from
        // the Document, then hand it back so the channel can talk to the network process itself.
        loaderProxy->postTaskToLoader([weakThis = ThreadSafeWeakPtr<WebSocketChannel> { *this }, contextIdentifier = *m_workerContextIdentifier, url = url.isolatedCopy(), protocol = protocol.isolatedCopy(), progressIdentifier = progressIdentifier()](ScriptExecutionContext& context) mutable {
            ASSERT(isMainRunLoop());

            RefPtr channel = weakThis.get();
            if (!channel)
                return;

            channel->addMessageReceiverIfNeeded(contextIdentifier);

            auto& document = downcast<Document>(context);

            if (RefPtr frame = document.frame(); frame && MixedContentChecker::shouldBlockRequest(*frame, url)) {
                auto errorMessage = makeString("The page at "_s, document.url().stringCenterEllipsizedToLength(), " was blocked from connecting insecurely to "_s, url.stringCenterEllipsizedToLength(), " either because the protocol is insecure or the page is embedded from an insecure page."_s);
                ScriptExecutionContext::postTaskTo(contextIdentifier, [weakThis = WTF::move(weakThis), errorMessage = WTF::move(errorMessage).isolatedCopy()](ScriptExecutionContext&) mutable {
                    if (RefPtr channel = weakThis.get())
                        channel->fail(WTF::move(errorMessage));
                });
                return;
            }

            if (WebProcess::singleton().webSocketChannelManager().hasReachedSocketLimit()) {
                String errorMessage = "Connection failed: Insufficient resources"_s;
                if (RefPtr channel = weakThis.get())
                    channel->logErrorMessage(context, errorMessage);

                ScriptExecutionContext::postTaskTo(contextIdentifier, [weakThis = WTF::move(weakThis), errorMessage = WTF::move(errorMessage).isolatedCopy()](ScriptExecutionContext&) mutable {
                    RefPtr channel = weakThis.get();
                    if (!channel)
                        return;

                    if (RefPtr client = channel->m_client.get())
                        client->didReceiveMessageError(WTF::move(errorMessage));
                });
                return;
            }

            auto parameters = WebSocketChannel::createConnectParametersForWorker(document, url, progressIdentifier);
            ScriptExecutionContext::postTaskTo(contextIdentifier, [weakThis = WTF::move(weakThis), parameters = WTF::move(parameters), protocol = WTF::move(protocol).isolatedCopy()](ScriptExecutionContext&) mutable {
                RefPtr channel = weakThis.get();
                if (!channel)
                    return;

                if (parameters)
                    channel->connectWithParameters(WTF::move(*parameters), protocol);
                else
                    channel->didReceiveMessageError({ });
            });
        });

        // connect is asynchronous for worker channels, so failures are reported through the client.
        return ConnectStatus::OK;
    }

    RefPtr document = dynamicDowncast<Document>(context.get());
    if (!document)
        return ConnectStatus::KO;

    if (WebProcess::singleton().webSocketChannelManager().hasReachedSocketLimit()) {
        auto reason = "Connection failed: Insufficient resources"_s;
        logErrorMessage(*context, reason);
        if (RefPtr client = m_client.get())
            client->didReceiveMessageError(String { reason });
        return ConnectStatus::KO;
    }

    auto parameters = createConnectParameters(*document, url, progressIdentifier());
    if (!parameters)
        return ConnectStatus::KO;

    connectWithParameters(WTF::move(*parameters), protocol);
    return ConnectStatus::OK;
}

void WebSocketChannel::connectWithParameters(ConnectParameters&& parameters, const String& protocol)
{
    if (parameters.didUpgradeURL) {
        if (RefPtr client = m_client.get())
            client->didUpgradeURL();
    }

    m_url = parameters.request.url();

    MessageSender::send(Messages::NetworkConnectionToWebProcess::CreateSocketChannel { parameters.request, protocol, identifier(), m_webPageProxyID, parameters.frameIdentifier, parameters.pageIdentifier, parameters.clientOrigin, parameters.hadMainFrameMainResourcePrivateRelayed, parameters.allowPrivacyProxy, parameters.advancedPrivacyProtections, parameters.storedCredentialsPolicy, m_isInitiatedByDedicatedWorker });
    m_needsToCallClose = true;
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
    CompletionHandler<void()> completionHandler = [weakThis = ThreadSafeWeakPtr<WebSocketChannel> { *this }, byteLength] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->decreaseBufferedAmount(byteLength);
    };

    if (m_connection) {
        RefPtr context = m_context.get();
        ASSERT(context);
        m_connection->sendWithAsyncReplyOnDispatcher(std::forward<T>(message), protect(context->nativePromiseDispatcher()), WTF::move(completionHandler), messageSenderDestinationID());
    } else
        sendWithAsyncReply(std::forward<T>(message), WTF::move(completionHandler));
}

void WebSocketChannel::send(UTF8CString&& message)
{
    if (!increaseBufferedAmount(message.length()))
        return;

    ASSERT(m_messageQueue);
    protect(m_messageQueue)->enqueue(WTF::move(message));
}

void WebSocketChannel::send(const JSC::ArrayBuffer& binaryData, size_t byteOffset, size_t byteLength)
{
    if (!increaseBufferedAmount(byteLength))
        return;

    ASSERT(m_messageQueue);
    protect(m_messageQueue)->enqueue(binaryData, byteOffset, byteLength);
}

void WebSocketChannel::send(Blob& blob)
{
    auto byteLength = blob.size();
    if (!blob.size())
        return send(JSC::ArrayBuffer::create(byteLength, 1), 0, 0);

    if (!increaseBufferedAmount(byteLength))
        return;

    ASSERT(m_messageQueue);
    protect(m_messageQueue)->enqueue(blob);
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
    m_needsToCallClose = false;
}

void WebSocketChannel::fail(String&& reason)
{
    // The client can close the channel, potentially removing the last reference.
    Ref protectedThis { *this };

    if (RefPtr context = m_context)
        logErrorMessage(*context, reason);
    if (RefPtr client = m_client.get())
        client->didReceiveMessageError(String { reason });

    if (m_isClosing)
        return;

    MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::ThreadableWebSocketChannel::CloseEventCodeGoingAway, reason });
    m_needsToCallClose = false;
    didClose(WebCore::ThreadableWebSocketChannel::CloseEventCodeAbnormalClosure, { });
}

void WebSocketChannel::disconnect()
{
    m_client = nullptr;
    m_context = nullptr;

    // NetworkSendQueue is a ContextDestructionObserver, whose destructor has to run on the context thread.
    if (RefPtr messageQueue = std::exchange(m_messageQueue, nullptr))
        messageQueue->clear();

    m_inspector.didCloseWebSocket();

    if (m_needsToCallClose) {
        MessageSender::send(Messages::NetworkSocketChannel::Close { WebCore::ThreadableWebSocketChannel::CloseEventCodeGoingAway, { } });
        m_needsToCallClose = false;
    }
}

void WebSocketChannel::didConnect(String&& subprotocol, String&& extensions)
{
    if (m_isClosing)
        return;

    RefPtr client = m_client.get();
    if (!client)
        return;

    m_subprotocol = WTF::move(subprotocol);
    m_extensions = WTF::move(extensions);
    client->didConnect();
}

void WebSocketChannel::didReceiveText(String&& message)
{
    if (m_isClosing)
        return;

    if (RefPtr client = m_client.get())
        client->didReceiveMessage(WTF::move(message));
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

void WebSocketChannel::logErrorMessage(WebCore::ScriptExecutionContext& context, const String& errorMessage)
{
    String consoleMessage;
    if (!m_url.isNull())
        consoleMessage = makeString("WebSocket connection to '"_s, m_url.string(), "' failed: "_s, errorMessage);
    else
        consoleMessage = makeString("WebSocket connection failed: "_s, errorMessage);

    if (is<WorkerGlobalScope>(context)) {
        CheckedPtr loaderProxy = downcast<WorkerGlobalScope>(context).thread()->workerLoaderProxy();
        if (!loaderProxy)
            return;

        loaderProxy->postTaskToLoader([consoleMessage = WTF::move(consoleMessage).isolatedCopy()](ScriptExecutionContext& context) {
            context.addConsoleMessage(MessageSource::Network, MessageLevel::Error, consoleMessage);
        });
    } else
        context.addConsoleMessage(MessageSource::Network, MessageLevel::Error, consoleMessage);
}

void WebSocketChannel::didReceiveMessageError(String&& errorMessage)
{
    RefPtr client = m_client.get();
    if (!client)
        return;

    if (RefPtr context = m_context)
        logErrorMessage(*context, errorMessage);
    client->didReceiveMessageError(WTF::move(errorMessage));
}

void WebSocketChannel::networkProcessCrashed()
{
    String errorMessage = "WebSocket network error: Network process crashed."_s;
    if (m_workerContextIdentifier) {
        ScriptExecutionContext::postTaskTo(*m_workerContextIdentifier, [weakThis = ThreadSafeWeakPtr<WebSocketChannel> { *this }, errorMessage = WTF::move(errorMessage).isolatedCopy()](ScriptExecutionContext&) mutable {
            if (RefPtr channel = weakThis.get())
                channel->fail(WTF::move(errorMessage));
        });
    } else
        fail(WTF::move(errorMessage));
}

void WebSocketChannel::suspend()
{
}

void WebSocketChannel::resume()
{
}

void WebSocketChannel::didSendHandshakeRequest(ResourceRequest&& request)
{
    m_inspector.didSendWebSocketHandshakeRequest(request);
    m_handshakeRequest = WTF::move(request);
}

void WebSocketChannel::didReceiveHandshakeResponse(ResourceResponse&& response)
{
    m_inspector.didReceiveWebSocketHandshakeResponse(response);
    m_handshakeResponse = WTF::move(response);
}

} // namespace WebKit
