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
#include <WebCore/FileReaderLoader.h>
#include <WebCore/FileReaderLoaderClient.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/WebSocketChannel.h>
#include <WebCore/WebSocketChannelClient.h>
#include <pal/SessionID.h>
#include <wtf/CheckedArithmetic.h>

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

    MessageSender::send(Messages::NetworkConnectionToWebProcess::CreateSocketChannel { m_document->sessionID(), *request, protocol, identifier() });
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

class BlobLoader final : public WebCore::FileReaderLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BlobLoader(WebCore::Document* document, WebCore::Blob& blob, CompletionHandler<void()>&& completionHandler)
        : m_loader(makeUnique<WebCore::FileReaderLoader>(WebCore::FileReaderLoader::ReadAsArrayBuffer, this))
        , m_completionHandler(WTFMove(completionHandler))
    {
        m_loader->start(document, blob);
    }

    ~BlobLoader()
    {
        if (m_loader)
            m_loader->cancel();
    }

    bool isLoading() const { return !!m_loader; }
    const RefPtr<JSC::ArrayBuffer>& result() const { return m_buffer; }
    Optional<int> errorCode() const { return m_errorCode; }

private:
    void didStartLoading() final { }
    void didReceiveData() final { }

    void didFinishLoading() final
    {
        m_buffer = m_loader->arrayBufferResult();
        complete();
    }

    void didFail(int errorCode) final
    {
        m_errorCode = errorCode;
        complete();
    }

    void complete()
    {
        m_loader = nullptr;
        m_completionHandler();
    }

    std::unique_ptr<WebCore::FileReaderLoader> m_loader;
    RefPtr<JSC::ArrayBuffer> m_buffer;
    Optional<int> m_errorCode;
    CompletionHandler<void()> m_completionHandler;
};

class PendingMessage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type { Text, Binary, Blob };

    explicit PendingMessage(const String& message)
        : m_type(Type::Text)
        , m_textMessage(message)
    {
    }

    PendingMessage(const JSC::ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
        : m_type(Type::Binary)
        , m_binaryData(WebCore::SharedBuffer::create(static_cast<const uint8_t*>(binaryData.data()) + byteOffset, byteLength))
    {
    }

    PendingMessage(WebCore::Document* document, WebCore::Blob& blob, CompletionHandler<void()>&& completionHandler)
        : m_type(Type::Blob)
        , m_blobLoader(makeUnique<BlobLoader>(document, blob, WTFMove(completionHandler)))
    {
    }

    ~PendingMessage() = default;

    Type type() const { return m_type; }
    const String& textMessage() const { ASSERT(m_type == Type::Text); return m_textMessage; }
    const WebCore::SharedBuffer& binaryData() const { ASSERT(m_type == Type::Binary); return *m_binaryData; }
    const BlobLoader& blobLoader() const { ASSERT(m_type == Type::Blob); return *m_blobLoader; }

private:
    Type m_type;
    String m_textMessage;
    RefPtr<WebCore::SharedBuffer> m_binaryData;
    std::unique_ptr<BlobLoader> m_blobLoader;
};

WebSocketChannel::SendResult WebSocketChannel::send(const String& message)
{
    auto byteLength = message.sizeInBytes();
    if (!increaseBufferedAmount(byteLength))
        return SendFail;

    if (m_pendingMessages.isEmpty())
        sendMessage(Messages::NetworkSocketChannel::SendString { message }, byteLength);
    else
        m_pendingMessages.append(makeUnique<PendingMessage>(message));

    return SendSuccess;
}

WebSocketChannel::SendResult WebSocketChannel::send(const JSC::ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!increaseBufferedAmount(byteLength))
        return SendFail;

    if (m_pendingMessages.isEmpty())
        sendMessage(Messages::NetworkSocketChannel::SendData { IPC::DataReference { static_cast<const uint8_t*>(binaryData.data()) + byteOffset, byteLength } }, byteLength);
    else
        m_pendingMessages.append(makeUnique<PendingMessage>(binaryData, byteOffset, byteLength));

    return SendSuccess;
}

WebSocketChannel::SendResult WebSocketChannel::send(WebCore::Blob& blob)
{
    // Avoid the Blob queue and loading for empty blobs.
    if (!blob.size())
        return send(JSC::ArrayBuffer::create(blob.size(), 1), 0, 0);

    m_pendingMessages.append(makeUnique<PendingMessage>(m_document.get(), blob, [this] {
        while (!m_pendingMessages.isEmpty()) {
            auto& message = m_pendingMessages.first();

            switch (message->type()) {
            case PendingMessage::Type::Text:
                sendMessage(Messages::NetworkSocketChannel::SendString { message->textMessage() }, message->textMessage().sizeInBytes());
                break;
            case PendingMessage::Type::Binary: {
                const auto& binaryData = message->binaryData();
                sendMessage(Messages::NetworkSocketChannel::SendData { IPC::DataReference { reinterpret_cast<const uint8_t*>(binaryData.data()), binaryData.size() } }, binaryData.size());
                break;
            }
            case PendingMessage::Type::Blob: {
                auto& loader = message->blobLoader();
                if (loader.isLoading())
                    return;

                if (const auto& result = loader.result()) {
                    auto byteLength = result->byteLength();
                    if (increaseBufferedAmount(byteLength))
                        sendMessage(Messages::NetworkSocketChannel::SendData { IPC::DataReference { reinterpret_cast<const uint8_t*>(result->data()), byteLength } }, byteLength);
                } else if (auto errorCode = loader.errorCode())
                    fail(makeString("Failed to load Blob: error code = ", errorCode.value()));
                else
                    ASSERT_NOT_REACHED();
                break;
            }
            }

            m_pendingMessages.removeFirst();
        }
    }));
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
    m_pendingMessages.clear();

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

    // FIXME: do something with errorMessage.
    m_client->didReceiveMessageError();
}

void WebSocketChannel::networkProcessCrashed()
{
    didReceiveMessageError({ });
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
