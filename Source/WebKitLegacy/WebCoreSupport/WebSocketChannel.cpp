/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebSocketChannel.h"

#include "SocketStreamHandle.h"
#include "SocketStreamHandleImpl.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <WebCore/Blob.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/FileReaderLoader.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LegacyWebSocketInspectorInstrumentation.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Logging.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/Page.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/SocketProvider.h>
#include <WebCore/SocketStreamError.h>
#include <WebCore/UserContentProvider.h>
#include <WebCore/WebSocketChannelClient.h>
#include <WebCore/WebSocketHandshake.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

const Seconds TCPMaximumSegmentLifetime { 2_min };

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSocketChannel);

WebSocketChannel::WebSocketChannel(Document& document, WebSocketChannelClient& client, SocketProvider& provider)
    : m_document(document)
    , m_client(client)
    , m_resumeTimer(*this, &WebSocketChannel::resumeTimerFired)
    , m_closingTimer(*this, &WebSocketChannel::closingTimerFired)
    , m_progressIdentifier(WebSocketChannelIdentifier::generate())
    , m_socketProvider(provider)
{
    LOG(Network, "WebSocketChannel %p ctor, progress identifier %" PRIu64, this, m_progressIdentifier.toUInt64());
}

WebSocketChannel::~WebSocketChannel()
{
    LOG(Network, "WebSocketChannel %p dtor", this);
}

WebSocketChannel::ConnectStatus WebSocketChannel::connect(const URL& requestedURL, const String& protocol)
{
    LOG(Network, "WebSocketChannel %p connect()", this);

    auto validatedURL = validateURL(*m_document, requestedURL);
    if (!validatedURL)
        return ConnectStatus::KO;
    ASSERT(!m_handle);
    ASSERT(!m_suspended);

    if (validatedURL->url != requestedURL) {
        if (RefPtr client = m_client)
            client->didUpgradeURL();
    }

    m_allowCookies = validatedURL->areCookiesAllowed;
    String userAgent = m_document->userAgent(m_document->url());
    String clientOrigin = m_document->securityOrigin().toString();

    bool isAppInitiated = true;
    if (auto* documentLoader = m_document->loader())
        isAppInitiated = documentLoader->lastNavigationWasAppInitiated();

    m_handshake = makeUnique<WebSocketHandshake>(validatedURL->url, protocol, userAgent, clientOrigin, m_allowCookies, isAppInitiated);
    m_handshake->reset();
    m_handshake->addExtensionProcessor(m_deflateFramer.createExtensionProcessor());
    LegacyWebSocketInspectorInstrumentation::didCreateWebSocket(m_document.get(), m_progressIdentifier, validatedURL->url);

    auto* frame = m_document->frame();
    auto* page = m_document->page();
    if (!frame || !page)
        return ConnectStatus::KO;

    ref();
    String partition = m_document->domainForCachePartition();
    bool shouldAcceptInsecureCertificates = false;
    m_handle = SocketStreamHandleImpl::create(m_handshake->url(), *this, page->sessionID(), partition, { }, frame->loader().networkingContext(), shouldAcceptInsecureCertificates);
    return ConnectStatus::OK;
}

Document* WebSocketChannel::document()
{
    return m_document;
}

String WebSocketChannel::subprotocol()
{
    LOG(Network, "WebSocketChannel %p subprotocol()", this);
    if (!m_handshake || m_handshake->mode() != WebSocketHandshake::Connected)
        return emptyString();
    String serverProtocol = m_handshake->serverWebSocketProtocol();
    if (serverProtocol.isNull())
        return emptyString();
    return serverProtocol;
}

String WebSocketChannel::extensions()
{
    LOG(Network, "WebSocketChannel %p extensions()", this);
    if (!m_handshake || m_handshake->mode() != WebSocketHandshake::Connected)
        return emptyString();
    String extensions = m_handshake->acceptedExtensions();
    if (extensions.isNull())
        return emptyString();
    return extensions;
}

void WebSocketChannel::send(CString&& message)
{
    if (m_outgoingFrameQueueStatus != OutgoingFrameQueueOpen)
        return;

    LOG(Network, "WebSocketChannel %p send() Sending String '%s'", this, message.data());
    enqueueTextFrame(WTF::move(message));
    processOutgoingFrameQueue();
}

void WebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (m_outgoingFrameQueueStatus != OutgoingFrameQueueOpen)
        return;

    LOG(Network, "WebSocketChannel %p send() Sending ArrayBuffer %p byteOffset=%u byteLength=%u", this, &binaryData, byteOffset, byteLength);
    enqueueRawFrame(WebSocketFrame::OpCodeBinary, binaryData.span().subspan(byteOffset, byteLength));
    processOutgoingFrameQueue();
}

void WebSocketChannel::send(Blob& binaryData)
{
    if (m_outgoingFrameQueueStatus != OutgoingFrameQueueOpen)
        return;

    LOG(Network, "WebSocketChannel %p send() Sending Blob '%s'", this, binaryData.url().string().utf8().data());
    enqueueBlobFrame(WebSocketFrame::OpCodeBinary, binaryData);
    processOutgoingFrameQueue();
}

void WebSocketChannel::send(std::span<const uint8_t> data)
{
    if (m_outgoingFrameQueueStatus != OutgoingFrameQueueOpen)
        return;

    LOG(Network, "WebSocketChannel %p send() Sending uint8_t* data=%p length=%zu", this, data.data(), data.size());
    enqueueRawFrame(WebSocketFrame::OpCodeBinary, data);
    processOutgoingFrameQueue();
}

void WebSocketChannel::close(int code, const String& reason)
{
    LOG(Network, "WebSocketChannel %p close() code=%d reason='%s'", this, code, reason.utf8().data());
    ASSERT(!m_suspended);
    if (!m_handle)
        return;
    Ref<WebSocketChannel> protectedThis(*this); // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    startClosingHandshake(code, reason);
    if (m_closing && !m_closingTimer.isActive())
        m_closingTimer.startOneShot(TCPMaximumSegmentLifetime * 2);
}

void WebSocketChannel::fail(String&& reason)
{
    RELEASE_LOG(Network, "WebSocketChannel %p fail() reason='%s'", this, reason.utf8().data());
    ASSERT(!m_suspended);
    if (m_document) {
        LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketFrameError(m_document.get(), m_progressIdentifier, reason);

        String consoleMessage;
        if (m_handshake)
            consoleMessage = makeString("WebSocket connection to '"_s, m_handshake->url().stringCenterEllipsizedToLength(), "' failed: "_s, reason);
        else
            consoleMessage = makeString("WebSocket connection failed: "_s, reason);

        m_document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, consoleMessage);
    }

    // Hybi-10 specification explicitly states we must not continue to handle incoming data
    // once the WebSocket connection is failed (section 7.1.7).
    Ref<WebSocketChannel> protectedThis(*this); // The client can close the channel, potentially removing the last reference.
    m_shouldDiscardReceivedData = true;
    if (!m_buffer.isEmpty())
        skipBuffer(m_buffer.size()); // Save memory.
    m_deflateFramer.didFail();
    m_hasContinuousFrame = false;
    m_continuousFrameData.clear();
    if (RefPtr client = m_client)
        client->didReceiveMessageError(WTF::move(reason));

    if (m_handle && !m_closed)
        m_handle->disconnect(); // Will call didCloseSocketStream() but maybe not synchronously.
}

void WebSocketChannel::disconnect()
{
    LOG(Network, "WebSocketChannel %p disconnect()", this);
    if (m_document)
        LegacyWebSocketInspectorInstrumentation::didCloseWebSocket(m_document.get(), m_progressIdentifier);
    m_client = nullptr;
    m_document = nullptr;
    if (m_handle)
        m_handle->disconnect();
}

void WebSocketChannel::suspend()
{
    m_suspended = true;
}

void WebSocketChannel::resume()
{
    m_suspended = false;
    if ((!m_buffer.isEmpty() || m_closed) && m_client.get() && !m_resumeTimer.isActive())
        m_resumeTimer.startOneShot(0_s);
}

void WebSocketChannel::didOpenSocketStream(SocketStreamHandle& handle)
{
    LOG(Network, "WebSocketChannel %p didOpenSocketStream()", this);
    ASSERT(&handle == m_handle);
    if (!m_document)
        return;
    if (LegacyWebSocketInspectorInstrumentation::hasFrontends()) [[unlikely]] {
        auto cookieRequestHeaderFieldValue = [document = m_document] (const URL& url) -> String {
            if (!document || !document->page())
                return { };
            return document->page()->cookieJar().cookieRequestHeaderFieldValue(*document, url);
        };
        LegacyWebSocketInspectorInstrumentation::willSendWebSocketHandshakeRequest(m_document.get(), m_progressIdentifier, m_handshake->clientHandshakeRequest(WTF::move(cookieRequestHeaderFieldValue)));
    }
    auto handshakeMessage = m_handshake->clientHandshakeMessage();
    std::optional<CookieRequestHeaderFieldProxy> cookieRequestHeaderFieldProxy;
    if (m_allowCookies)
        cookieRequestHeaderFieldProxy = CookieJar::cookieRequestHeaderFieldProxy(*m_document, m_handshake->httpURLForAuthenticationAndCookies());
    handle.sendHandshake(WTF::move(handshakeMessage), WTF::move(cookieRequestHeaderFieldProxy), [this, protectedThis = Ref { *this }] (bool success, bool didAccessSecureCookies) {
        if (!success)
            fail("Failed to send WebSocket handshake."_s);

        if (didAccessSecureCookies && m_document)
            m_document->setSecureCookiesAccessed();
    });
}

void WebSocketChannel::didCloseSocketStream(SocketStreamHandle& handle)
{
    LOG(Network, "WebSocketChannel %p didCloseSocketStream()", this);
    if (m_document)
        LegacyWebSocketInspectorInstrumentation::didCloseWebSocket(m_document.get(), m_progressIdentifier);
    ASSERT_UNUSED(handle, &handle == m_handle || !m_handle);
    m_closed = true;
    if (m_closingTimer.isActive())
        m_closingTimer.stop();
    if (m_outgoingFrameQueueStatus != OutgoingFrameQueueClosed)
        abortOutgoingFrameQueue();
    if (m_handle) {
        m_unhandledBufferedAmount = m_handle->bufferedAmount();
        if (m_suspended)
            return;
        RefPtr client = std::exchange(m_client, nullptr);
        m_document = nullptr;
        m_handle = nullptr;
        if (client)
            client->didClose(m_unhandledBufferedAmount, m_receivedClosingHandshake ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete, m_closeEventCode, m_closeEventReason);
    }
    deref();
}

void WebSocketChannel::didReceiveSocketStreamData(SocketStreamHandle& handle, std::span<const uint8_t> data)
{
    LOG(Network, "WebSocketChannel %p didReceiveSocketStreamData() Received %zu bytes", this, data.size());
    Ref<WebSocketChannel> protectedThis(*this); // The client can close the channel, potentially removing the last reference.
    ASSERT(&handle == m_handle);
    if (!m_document) {
        return;
    }
    if (data.empty()) {
        handle.disconnect();
        return;
    }
    if (!m_client.get()) {
        m_shouldDiscardReceivedData = true;
        handle.disconnect();
        return;
    }
    if (m_shouldDiscardReceivedData)
        return;
    if (!appendToBuffer(data)) {
        m_shouldDiscardReceivedData = true;
        fail("Ran out of memory while receiving WebSocket data."_s);
        return;
    }
    while (!m_suspended && m_client.get() && !m_buffer.isEmpty()) {
        if (!processBuffer())
            break;
    }
}

void WebSocketChannel::didFailToReceiveSocketStreamData(SocketStreamHandle& handle)
{
    handle.disconnect();
}

void WebSocketChannel::didUpdateBufferedAmount(SocketStreamHandle&, size_t bufferedAmount)
{
    if (RefPtr client = m_client)
        client->didUpdateBufferedAmount(bufferedAmount);
}

void WebSocketChannel::didFailSocketStream(SocketStreamHandle& handle, const SocketStreamError& error)
{
    LOG(Network, "WebSocketChannel %p didFailSocketStream()", this);
    ASSERT(&handle == m_handle || !m_handle);

    String message;
    if (error.isNull())
        message = "WebSocket network error"_s;
    else if (error.localizedDescription().isNull())
        message = makeString("WebSocket network error: error code "_s, error.errorCode());
    else
        message = makeString("WebSocket network error: "_s, error.localizedDescription());

    if (m_document) {
        LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketFrameError(m_document.get(), m_progressIdentifier, message);
        m_document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, message);
        LOG_ERROR("%s", message.utf8().data());
    }
    m_shouldDiscardReceivedData = true;
    if (RefPtr client = m_client)
        client->didReceiveMessageError(WTF::move(message));
    handle.disconnect();
}

void WebSocketChannel::didStartLoading()
{
    LOG(Network, "WebSocketChannel %p didStartLoading()", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
}

void WebSocketChannel::didReceiveData()
{
    LOG(Network, "WebSocketChannel %p didReceiveData()", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
}

void WebSocketChannel::didFinishLoading()
{
    LOG(Network, "WebSocketChannel %p didFinishLoading()", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoaderStatus = BlobLoaderFinished;
    processOutgoingFrameQueue();
    deref();
}

void WebSocketChannel::didFail(ExceptionCode errorCode)
{
    auto code = static_cast<int>(errorCode);
    LOG(Network, "WebSocketChannel %p didFail() errorCode=%d", this, code);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoader = nullptr;
    m_blobLoaderStatus = BlobLoaderFailed;
    fail(makeString("Failed to load Blob: error code = "_s, code)); // FIXME: Generate human-friendly reason message.
    deref();
}

bool WebSocketChannel::appendToBuffer(std::span<const uint8_t> data)
{
    size_t newBufferSize = m_buffer.size() + data.size();
    if (newBufferSize < m_buffer.size()) {
        LOG(Network, "WebSocketChannel %p appendToBuffer() Buffer overflow (%u bytes already in receive buffer and appending %zu bytes)", this, static_cast<unsigned>(m_buffer.size()), data.size());
        return false;
    }
    m_buffer.append(data);
    return true;
}

void WebSocketChannel::skipBuffer(size_t length)
{
    ASSERT_WITH_SECURITY_IMPLICATION(length <= m_buffer.size());
    memmoveSpan(m_buffer.mutableSpan(), m_buffer.subspan(length));
    m_buffer.shrink(m_buffer.size() - length);
}

bool WebSocketChannel::processBuffer()
{
    ASSERT(!m_suspended);
    ASSERT(m_client.get());
    ASSERT(!m_buffer.isEmpty());
    LOG(Network, "WebSocketChannel %p processBuffer() Receive buffer has %u bytes", this, static_cast<unsigned>(m_buffer.size()));

    if (m_shouldDiscardReceivedData)
        return false;

    if (m_receivedClosingHandshake) {
        skipBuffer(m_buffer.size());
        return false;
    }

    Ref<WebSocketChannel> protectedThis(*this); // The client can close the channel, potentially removing the last reference.

    if (m_handshake->mode() == WebSocketHandshake::Incomplete) {
        int headerLength = m_handshake->readServerHandshake(m_buffer.span());
        if (headerLength <= 0)
            return false;
        if (m_handshake->mode() == WebSocketHandshake::Connected) {
            LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketHandshakeResponse(m_document.get(), m_progressIdentifier, m_handshake->serverHandshakeResponse());
            String serverSetCookie = m_handshake->serverSetCookie();
            if (!serverSetCookie.isEmpty()) {
                if (m_document && m_document->page() && m_document->page()->cookieJar().cookiesEnabled(*m_document))
                    m_document->page()->cookieJar().setCookies(*m_document, m_handshake->httpURLForAuthenticationAndCookies(), serverSetCookie);
            }
            LOG(Network, "WebSocketChannel %p Connected", this);
            skipBuffer(headerLength);
            protect(m_client)->didConnect();
            LOG(Network, "WebSocketChannel %p %u bytes remaining in m_buffer", this, static_cast<unsigned>(m_buffer.size()));
            return !m_buffer.isEmpty();
        }
        ASSERT(m_handshake->mode() == WebSocketHandshake::Failed);
        LOG(Network, "WebSocketChannel %p Connection failed", this);
        skipBuffer(headerLength);
        m_shouldDiscardReceivedData = true;
        fail(m_handshake->failureReason());
        return false;
    }
    if (m_handshake->mode() != WebSocketHandshake::Connected)
        return false;

    return processFrame();
}

void WebSocketChannel::resumeTimerFired()
{
    Ref<WebSocketChannel> protectedThis(*this); // The client can close the channel, potentially removing the last reference.
    while (!m_suspended && m_client.get() && !m_buffer.isEmpty()) {
        if (!processBuffer())
            break;
    }
    if (!m_suspended && m_client.get() && m_closed && m_handle)
        didCloseSocketStream(*m_handle);
}

void WebSocketChannel::startClosingHandshake(int code, const String& reason)
{
    LOG(Network, "WebSocketChannel %p startClosingHandshake() code=%d m_receivedClosingHandshake=%d", this, m_closing, m_receivedClosingHandshake);
    ASSERT(!m_closed);
    if (m_closing)
        return;
    ASSERT(m_handle);

    Vector<uint8_t> buf;
    if (!m_receivedClosingHandshake && code != CloseEventCodeNotSpecified) {
        uint8_t highByte = code >> 8;
        uint8_t lowByte = code;
        buf.append(highByte);
        buf.append(lowByte);
        auto reasonUTF8 = reason.utf8();
        buf.append(reasonUTF8.span());
    }
    enqueueRawFrame(WebSocketFrame::OpCodeClose, buf.span());
    Ref<WebSocketChannel> protectedThis(*this); // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    processOutgoingFrameQueue();

    if (m_closed) {
        // The channel got closed because processOutgoingFrameQueue() failed.
        return;
    }

    m_closing = true;
    if (RefPtr client = m_client)
        client->didStartClosingHandshake();
}

void WebSocketChannel::closingTimerFired()
{
    LOG(Network, "WebSocketChannel %p closingTimerFired()", this);
    if (m_handle)
        m_handle->disconnect();
}


bool WebSocketChannel::processFrame()
{
    ASSERT(!m_buffer.isEmpty());

    WebSocketFrame frame;
    const uint8_t* frameEnd;
    String errorString;
    WebSocketFrame::ParseFrameResult result = WebSocketFrame::parseFrame(m_buffer.mutableSpan(), frame, frameEnd, errorString);
    if (result == WebSocketFrame::FrameIncomplete)
        return false;
    if (result == WebSocketFrame::FrameError) {
        fail(WTF::move(errorString));
        return false;
    }

    ASSERT(m_buffer.begin() < frameEnd);
    ASSERT(frameEnd <= m_buffer.end());

    auto inflateResult = m_deflateFramer.inflate(frame);
    if (!inflateResult->succeeded()) {
        fail(inflateResult->failureReason());
        return false;
    }

    // Validate the frame data.
    if (WebSocketFrame::isReservedOpCode(frame.opCode)) {
        fail(makeString("Unrecognized frame opcode: "_s, static_cast<unsigned>(frame.opCode)));
        return false;
    }

    if (frame.reserved2 || frame.reserved3) {
        fail(makeString("One or more reserved bits are on: reserved2 = "_s, static_cast<unsigned>(frame.reserved2), ", reserved3 = "_s, static_cast<unsigned>(frame.reserved3)));
        return false;
    }

    if (frame.masked) {
        fail("A server must not mask any frames that it sends to the client."_s);
        return false;
    }

    // All control frames must not be fragmented.
    if (WebSocketFrame::isControlOpCode(frame.opCode) && !frame.final) {
        fail(makeString("Received fragmented control frame: opcode = "_s, static_cast<unsigned>(frame.opCode)));
        return false;
    }

    // All control frames must have a payload of 125 bytes or less, which means the frame must not contain
    // the "extended payload length" field.
    if (WebSocketFrame::isControlOpCode(frame.opCode) && WebSocketFrame::needsExtendedLengthField(frame.payload.size())) {
        fail(makeString("Received control frame having too long payload: "_s, frame.payload.size(), " bytes"_s));
        return false;
    }

    // A new data frame is received before the previous continuous frame finishes.
    // Note that control frames are allowed to come in the middle of continuous frames.
    if (m_hasContinuousFrame && frame.opCode != WebSocketFrame::OpCodeContinuation && !WebSocketFrame::isControlOpCode(frame.opCode)) {
        fail("Received new data frame but previous continuous frame is unfinished."_s);
        return false;
    }

    LegacyWebSocketInspectorInstrumentation::didReceiveWebSocketFrame(m_document.get(), m_progressIdentifier, frame);

    switch (frame.opCode) {
    case WebSocketFrame::OpCodeContinuation:
        // An unexpected continuation frame is received without any leading frame.
        if (!m_hasContinuousFrame) {
            fail("Received unexpected continuation frame."_s);
            return false;
        }
        m_continuousFrameData.append(frame.payload);
        skipBuffer(frameEnd - m_buffer.begin());
        if (frame.final) {
            // onmessage handler may eventually call the other methods of this channel,
            // so we should pretend that we have finished to read this frame and
            // make sure that the member variables are in a consistent state before
            // the handler is invoked.
            Vector<uint8_t> continuousFrameData = WTF::move(m_continuousFrameData);
            m_hasContinuousFrame = false;
            if (m_continuousFrameOpCode == WebSocketFrame::OpCodeText) {
                String message;
                if (continuousFrameData.size())
                    message = String::fromUTF8(continuousFrameData.span());
                else
                    message = emptyString();
                if (message.isNull())
                    fail("Could not decode a text frame as UTF-8."_s);
                else
                    protect(m_client)->didReceiveMessage(WTF::move(message));
            } else if (m_continuousFrameOpCode == WebSocketFrame::OpCodeBinary)
                protect(m_client)->didReceiveBinaryData(WTF::move(continuousFrameData));
        }
        break;

    case WebSocketFrame::OpCodeText:
        if (frame.final) {
            String message;
            if (frame.payload.size())
                message = String::fromUTF8(frame.payload);
            else
                message = emptyString();
            skipBuffer(frameEnd - m_buffer.begin());
            if (message.isNull())
                fail("Could not decode a text frame as UTF-8."_s);
            else
                protect(m_client)->didReceiveMessage(WTF::move(message));
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = WebSocketFrame::OpCodeText;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload);
            skipBuffer(frameEnd - m_buffer.begin());
        }
        break;

    case WebSocketFrame::OpCodeBinary:
        if (frame.final) {
            Vector<uint8_t> binaryData(frame.payload);
            skipBuffer(frameEnd - m_buffer.begin());
            protect(m_client)->didReceiveBinaryData(WTF::move(binaryData));
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = WebSocketFrame::OpCodeBinary;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload);
            skipBuffer(frameEnd - m_buffer.begin());
        }
        break;

    case WebSocketFrame::OpCodeClose:
        if (!frame.payload.size())
            m_closeEventCode = CloseEventCodeNoStatusRcvd;
        else if (frame.payload.size() == 1) {
            m_closeEventCode = CloseEventCodeAbnormalClosure;
            fail("Received a broken close frame containing an invalid size body."_s);
            return false;
        } else {
            m_closeEventCode = frame.payload[0] << 8 | frame.payload[1];
            if (m_closeEventCode == CloseEventCodeNoStatusRcvd || m_closeEventCode == CloseEventCodeAbnormalClosure || m_closeEventCode == CloseEventCodeTLSHandshake) {
                m_closeEventCode = CloseEventCodeAbnormalClosure;
                fail("Received a broken close frame containing a reserved status code."_s);
                return false;
            }
        }
        if (frame.payload.size() >= 3)
            m_closeEventReason = String::fromUTF8({ &frame.payload[2], frame.payload.size() - 2 });
        else
            m_closeEventReason = emptyString();
        skipBuffer(frameEnd - m_buffer.begin());
        m_receivedClosingHandshake = true;
        startClosingHandshake(m_closeEventCode, m_closeEventReason);
        if (m_closing) {
            if (m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen)
                m_outgoingFrameQueueStatus = OutgoingFrameQueueClosing;
            processOutgoingFrameQueue();
        }
        break;

    case WebSocketFrame::OpCodePing:
        enqueueRawFrame(WebSocketFrame::OpCodePong, frame.payload);
        skipBuffer(frameEnd - m_buffer.begin());
        processOutgoingFrameQueue();
        break;

    case WebSocketFrame::OpCodePong:
        // A server may send a pong in response to our ping, or an unsolicited pong which is not associated with
        // any specific ping. Either way, there's nothing to do on receipt of pong.
        skipBuffer(frameEnd - m_buffer.begin());
        break;

    default:
        ASSERT_NOT_REACHED();
        skipBuffer(frameEnd - m_buffer.begin());
        break;
    }

    if (m_buffer.isEmpty()) {
        m_buffer.clear();
        return false;
    }
    return true;
}

void WebSocketChannel::enqueueTextFrame(CString&& string)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    auto frame = makeUnique<QueuedFrame>();
    frame->opCode = WebSocketFrame::OpCodeText;
    frame->frameType = QueuedFrameTypeString;
    frame->stringData = WTF::move(string);
    m_outgoingFrameQueue.append(WTF::move(frame));
}

void WebSocketChannel::enqueueRawFrame(WebSocketFrame::OpCode opCode, std::span<const uint8_t> data)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    auto frame = makeUnique<QueuedFrame>();
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeVector;
    frame->vectorData = data;
    m_outgoingFrameQueue.append(WTF::move(frame));
}

void WebSocketChannel::enqueueBlobFrame(WebSocketFrame::OpCode opCode, Blob& blob)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    auto frame = makeUnique<QueuedFrame>();
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeBlob;
    frame->blobData = blob;
    m_outgoingFrameQueue.append(WTF::move(frame));
}

void WebSocketChannel::processOutgoingFrameQueue()
{
    if (m_outgoingFrameQueueStatus == OutgoingFrameQueueClosed)
        return;

    Ref<WebSocketChannel> protectedThis(*this); // Any call to fail() will get the channel closed and dereferenced.

    while (!m_outgoingFrameQueue.isEmpty()) {
        auto frame = m_outgoingFrameQueue.takeFirst();
        switch (frame->frameType) {
        case QueuedFrameTypeString: {
            sendFrame(frame->opCode, byteCast<uint8_t>(frame->stringData.span()), [this, protectedThis = Ref { *this }] (bool success) {
                if (!success)
                    fail("Failed to send WebSocket frame."_s);
            });
            break;
        }

        case QueuedFrameTypeVector:
            sendFrame(frame->opCode, frame->vectorData.span(), [this, protectedThis = Ref { *this }] (bool success) {
                if (!success)
                    fail("Failed to send WebSocket frame."_s);
            });
            break;

        case QueuedFrameTypeBlob: {
            switch (m_blobLoaderStatus) {
            case BlobLoaderNotStarted:
                ref(); // Will be derefed after didFinishLoading() or didFail().
                ASSERT(!m_blobLoader);
                ASSERT(frame->blobData);
                m_blobLoader = FileReaderLoader::create(FileReaderLoader::ReadAsArrayBuffer, this);
                m_blobLoaderStatus = BlobLoaderStarted;
                m_blobLoader->start(m_document.get(), *frame->blobData);
                m_outgoingFrameQueue.prepend(WTF::move(frame));
                return;

            case BlobLoaderStarted:
            case BlobLoaderFailed:
                m_outgoingFrameQueue.prepend(WTF::move(frame));
                return;

            case BlobLoaderFinished: {
                RefPtr<ArrayBuffer> result = m_blobLoader->arrayBufferResult();
                m_blobLoader = nullptr;
                m_blobLoaderStatus = BlobLoaderNotStarted;
                sendFrame(frame->opCode, result->span(), [this, protectedThis = Ref { *this }] (bool success) {
                    if (!success)
                        fail("Failed to send WebSocket frame."_s);
                });
                break;
            }
            }
            break;
        }

        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    ASSERT(m_outgoingFrameQueue.isEmpty());
    if (m_outgoingFrameQueueStatus == OutgoingFrameQueueClosing) {
        m_outgoingFrameQueueStatus = OutgoingFrameQueueClosed;
        m_handle->close();
    }
}

void WebSocketChannel::abortOutgoingFrameQueue()
{
    m_outgoingFrameQueue.clear();
    m_outgoingFrameQueueStatus = OutgoingFrameQueueClosed;
    if (m_blobLoaderStatus == BlobLoaderStarted) {
        m_blobLoader->cancel();
        didFail(ExceptionCode::AbortError);
    }
}

void WebSocketChannel::sendFrame(WebSocketFrame::OpCode opCode, std::span<const uint8_t> data, Function<void(bool)> completionHandler)
{
    ASSERT(m_handle);
    ASSERT(!m_suspended);

    WebSocketFrame frame(opCode, true, false, true, data);
    LegacyWebSocketInspectorInstrumentation::didSendWebSocketFrame(m_document.get(), m_progressIdentifier, frame);

    auto deflateResult = m_deflateFramer.deflate(frame);
    if (!deflateResult->succeeded()) {
        fail(deflateResult->failureReason());
        return completionHandler(false);
    }

    Vector<uint8_t> frameData;
    frame.makeFrameData(frameData);

    m_handle->sendData(frameData.span(), WTF::move(completionHandler));
}

ResourceRequest WebSocketChannel::clientHandshakeRequest(const CookieGetter& cookieRequestHeaderFieldValue) const
{
    return m_handshake->clientHandshakeRequest(cookieRequestHeaderFieldValue);
}

const ResourceResponse& WebSocketChannel::serverHandshakeResponse() const
{
    return m_handshake->serverHandshakeResponse();
}

} // namespace WebCore
