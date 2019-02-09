/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebSocketChannel.h"

#include "Blob.h"
#include "CookieJar.h"
#include "Document.h"
#include "FileError.h"
#include "FileReaderLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "NetworkingContext.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "SocketProvider.h"
#include "SocketStreamError.h"
#include "SocketStreamHandle.h"
#include "UserContentProvider.h"
#include "WebSocketChannelClient.h"
#include "WebSocketHandshake.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

const Seconds TCPMaximumSegmentLifetime { 2_min };

WebSocketChannel::WebSocketChannel(Document& document, WebSocketChannelClient& client, SocketProvider& provider)
    : m_document(&document)
    , m_client(&client)
    , m_resumeTimer(*this, &WebSocketChannel::resumeTimerFired)
    , m_closingTimer(*this, &WebSocketChannel::closingTimerFired)
    , m_socketProvider(provider)
{
    if (Page* page = document.page())
        m_identifier = page->progress().createUniqueIdentifier();

    LOG(Network, "WebSocketChannel %p ctor, identifier %u", this, m_identifier);
}

WebSocketChannel::~WebSocketChannel()
{
    LOG(Network, "WebSocketChannel %p dtor", this);
}

void WebSocketChannel::connect(const URL& requestedURL, const String& protocol)
{
    LOG(Network, "WebSocketChannel %p connect()", this);

    URL url = requestedURL;
    bool allowCookies = true;
#if ENABLE(CONTENT_EXTENSIONS)
    if (auto* page = m_document->page()) {
        if (auto* documentLoader = m_document->loader()) {
            auto blockedStatus = page->userContentProvider().processContentExtensionRulesForLoad(url, ResourceType::Raw, *documentLoader);
            if (blockedStatus.blockedLoad) {
                Ref<WebSocketChannel> protectedThis(*this);
                callOnMainThread([protectedThis = WTFMove(protectedThis)] {
                    if (protectedThis->m_client)
                        protectedThis->m_client->didReceiveMessageError();
                });
                return;
            }
            if (blockedStatus.madeHTTPS) {
                ASSERT(url.protocolIs("ws"));
                url.setProtocol("wss");
                if (m_client)
                    m_client->didUpgradeURL();
            }
            if (blockedStatus.blockedCookies)
                allowCookies = false;
        }
    }
#endif
    
    ASSERT(!m_handle);
    ASSERT(!m_suspended);
    m_handshake = std::make_unique<WebSocketHandshake>(url, protocol, m_document, allowCookies);
    m_handshake->reset();
    if (m_deflateFramer.canDeflate())
        m_handshake->addExtensionProcessor(m_deflateFramer.createExtensionProcessor());
    if (m_identifier)
        InspectorInstrumentation::didCreateWebSocket(m_document, m_identifier, url);

    if (Frame* frame = m_document->frame()) {
        ref();
        Page* page = frame->page();
        PAL::SessionID sessionID = page ? page->sessionID() : PAL::SessionID::defaultSessionID();
        String partition = m_document->domainForCachePartition();
        m_handle = m_socketProvider->createSocketStreamHandle(m_handshake->url(), *this, sessionID, partition, frame->loader().networkingContext());
    }
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

ThreadableWebSocketChannel::SendResult WebSocketChannel::send(const String& message)
{
    LOG(Network, "WebSocketChannel %p send() Sending String '%s'", this, message.utf8().data());
    CString utf8 = message.utf8(StrictConversionReplacingUnpairedSurrogatesWithFFFD);
    enqueueTextFrame(utf8);
    processOutgoingFrameQueue();
    // According to WebSocket API specification, WebSocket.send() should return void instead
    // of boolean. However, our implementation still returns boolean due to compatibility
    // concern (see bug 65850).
    // m_channel->send() may happen later, thus it's not always possible to know whether
    // the message has been sent to the socket successfully. In this case, we have no choice
    // but to return true.
    return ThreadableWebSocketChannel::SendSuccess;
}

ThreadableWebSocketChannel::SendResult WebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    LOG(Network, "WebSocketChannel %p send() Sending ArrayBuffer %p byteOffset=%u byteLength=%u", this, &binaryData, byteOffset, byteLength);
    enqueueRawFrame(WebSocketFrame::OpCodeBinary, static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);
    processOutgoingFrameQueue();
    return ThreadableWebSocketChannel::SendSuccess;
}

ThreadableWebSocketChannel::SendResult WebSocketChannel::send(Blob& binaryData)
{
    LOG(Network, "WebSocketChannel %p send() Sending Blob '%s'", this, binaryData.url().string().utf8().data());
    enqueueBlobFrame(WebSocketFrame::OpCodeBinary, binaryData);
    processOutgoingFrameQueue();
    return ThreadableWebSocketChannel::SendSuccess;
}

bool WebSocketChannel::send(const char* data, int length)
{
    LOG(Network, "WebSocketChannel %p send() Sending char* data=%p length=%d", this, data, length);
    enqueueRawFrame(WebSocketFrame::OpCodeBinary, data, length);
    processOutgoingFrameQueue();
    return true;
}

unsigned WebSocketChannel::bufferedAmount() const
{
    LOG(Network, "WebSocketChannel %p bufferedAmount()", this);
    ASSERT(m_handle);
    ASSERT(!m_suspended);
    return m_handle->bufferedAmount();
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

void WebSocketChannel::fail(const String& reason)
{
    LOG(Network, "WebSocketChannel %p fail() reason='%s'", this, reason.utf8().data());
    ASSERT(!m_suspended);
    if (m_document) {
        InspectorInstrumentation::didReceiveWebSocketFrameError(m_document, m_identifier, reason);

        String consoleMessage;
        if (m_handshake)
            consoleMessage = makeString("WebSocket connection to '", m_handshake->url().stringCenterEllipsizedToLength(), "' failed: ", reason);
        else
            consoleMessage = makeString("WebSocket connection failed: ", reason);

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
    if (m_client)
        m_client->didReceiveMessageError();

    if (m_handle && !m_closed)
        m_handle->disconnect(); // Will call didCloseSocketStream() but maybe not synchronously.
}

void WebSocketChannel::disconnect()
{
    LOG(Network, "WebSocketChannel %p disconnect()", this);
    if (m_identifier && m_document)
        InspectorInstrumentation::didCloseWebSocket(m_document, m_identifier);
    if (m_handshake)
        m_handshake->clearDocument();
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
    if ((!m_buffer.isEmpty() || m_closed) && m_client && !m_resumeTimer.isActive())
        m_resumeTimer.startOneShot(0_s);
}

void WebSocketChannel::didOpenSocketStream(SocketStreamHandle& handle)
{
    LOG(Network, "WebSocketChannel %p didOpenSocketStream()", this);
    ASSERT(&handle == m_handle);
    if (!m_document)
        return;
    if (m_identifier && UNLIKELY(InspectorInstrumentation::hasFrontends()))
        InspectorInstrumentation::willSendWebSocketHandshakeRequest(m_document, m_identifier, m_handshake->clientHandshakeRequest());
    auto handshakeMessage = m_handshake->clientHandshakeMessage();
    auto cookieRequestHeaderFieldProxy = m_handshake->clientHandshakeCookieRequestHeaderFieldProxy();
    handle.sendHandshake(WTFMove(handshakeMessage), WTFMove(cookieRequestHeaderFieldProxy), [this, protectedThis = makeRef(*this)] (bool success, bool didAccessSecureCookies) {
        if (!success)
            fail("Failed to send WebSocket handshake.");

        if (didAccessSecureCookies && m_document)
            m_document->setSecureCookiesAccessed();
    });
}

void WebSocketChannel::didCloseSocketStream(SocketStreamHandle& handle)
{
    LOG(Network, "WebSocketChannel %p didCloseSocketStream()", this);
    if (m_identifier && m_document)
        InspectorInstrumentation::didCloseWebSocket(m_document, m_identifier);
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
        WebSocketChannelClient* client = m_client;
        m_client = nullptr;
        m_document = nullptr;
        m_handle = nullptr;
        if (client)
            client->didClose(m_unhandledBufferedAmount, m_receivedClosingHandshake ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete, m_closeEventCode, m_closeEventReason);
    }
    deref();
}

void WebSocketChannel::didReceiveSocketStreamData(SocketStreamHandle& handle, const char* data, size_t length)
{
    LOG(Network, "WebSocketChannel %p didReceiveSocketStreamData() Received %zu bytes", this, length);
    Ref<WebSocketChannel> protectedThis(*this); // The client can close the channel, potentially removing the last reference.
    ASSERT(&handle == m_handle);
    if (!m_document) {
        return;
    }
    if (!length) {
        handle.disconnect();
        return;
    }
    if (!m_client) {
        m_shouldDiscardReceivedData = true;
        handle.disconnect();
        return;
    }
    if (m_shouldDiscardReceivedData)
        return;
    if (!appendToBuffer(data, length)) {
        m_shouldDiscardReceivedData = true;
        fail("Ran out of memory while receiving WebSocket data.");
        return;
    }
    while (!m_suspended && m_client && !m_buffer.isEmpty()) {
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
    if (m_client)
        m_client->didUpdateBufferedAmount(bufferedAmount);
}

void WebSocketChannel::didFailSocketStream(SocketStreamHandle& handle, const SocketStreamError& error)
{
    LOG(Network, "WebSocketChannel %p didFailSocketStream()", this);
    ASSERT(&handle == m_handle || !m_handle);
    if (m_document) {
        String message;
        if (error.isNull())
            message = "WebSocket network error"_s;
        else if (error.localizedDescription().isNull())
            message = makeString("WebSocket network error: error code ", error.errorCode());
        else
            message = "WebSocket network error: " + error.localizedDescription();
        InspectorInstrumentation::didReceiveWebSocketFrameError(m_document, m_identifier, message);
        m_document->addConsoleMessage(MessageSource::Network, MessageLevel::Error, message);
    }
    m_shouldDiscardReceivedData = true;
    if (m_client)
        m_client->didReceiveMessageError();
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

void WebSocketChannel::didFail(int errorCode)
{
    LOG(Network, "WebSocketChannel %p didFail() errorCode=%d", this, errorCode);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoader = nullptr;
    m_blobLoaderStatus = BlobLoaderFailed;
    fail(makeString("Failed to load Blob: error code = ", errorCode)); // FIXME: Generate human-friendly reason message.
    deref();
}

bool WebSocketChannel::appendToBuffer(const char* data, size_t len)
{
    size_t newBufferSize = m_buffer.size() + len;
    if (newBufferSize < m_buffer.size()) {
        LOG(Network, "WebSocketChannel %p appendToBuffer() Buffer overflow (%u bytes already in receive buffer and appending %u bytes)", this, static_cast<unsigned>(m_buffer.size()), static_cast<unsigned>(len));
        return false;
    }
    m_buffer.append(data, len);
    return true;
}

void WebSocketChannel::skipBuffer(size_t len)
{
    ASSERT_WITH_SECURITY_IMPLICATION(len <= m_buffer.size());
    memmove(m_buffer.data(), m_buffer.data() + len, m_buffer.size() - len);
    m_buffer.shrink(m_buffer.size() - len);
}

bool WebSocketChannel::processBuffer()
{
    ASSERT(!m_suspended);
    ASSERT(m_client);
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
        int headerLength = m_handshake->readServerHandshake(m_buffer.data(), m_buffer.size());
        if (headerLength <= 0)
            return false;
        if (m_handshake->mode() == WebSocketHandshake::Connected) {
            if (m_identifier)
                InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(m_document, m_identifier, m_handshake->serverHandshakeResponse());
            String serverSetCookie = m_handshake->serverSetCookie();
            if (!serverSetCookie.isEmpty()) {
                if (m_document && m_document->page() && m_document->page()->cookieJar().cookiesEnabled(*m_document))
                    m_document->page()->cookieJar().setCookies(*m_document, m_handshake->httpURLForAuthenticationAndCookies(), serverSetCookie);
            }
            LOG(Network, "WebSocketChannel %p Connected", this);
            skipBuffer(headerLength);
            m_client->didConnect();
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
    while (!m_suspended && m_client && !m_buffer.isEmpty())
        if (!processBuffer())
            break;
    if (!m_suspended && m_client && m_closed && m_handle)
        didCloseSocketStream(*m_handle);
}

void WebSocketChannel::startClosingHandshake(int code, const String& reason)
{
    LOG(Network, "WebSocketChannel %p startClosingHandshake() code=%d m_receivedClosingHandshake=%d", this, m_closing, m_receivedClosingHandshake);
    ASSERT(!m_closed);
    if (m_closing)
        return;
    ASSERT(m_handle);

    Vector<char> buf;
    if (!m_receivedClosingHandshake && code != CloseEventCodeNotSpecified) {
        unsigned char highByte = code >> 8;
        unsigned char lowByte = code;
        buf.append(static_cast<char>(highByte));
        buf.append(static_cast<char>(lowByte));
        auto reasonUTF8 = reason.utf8();
        buf.append(reasonUTF8.data(), reasonUTF8.length());
    }
    enqueueRawFrame(WebSocketFrame::OpCodeClose, buf.data(), buf.size());
    Ref<WebSocketChannel> protectedThis(*this); // An attempt to send closing handshake may fail, which will get the channel closed and dereferenced.
    processOutgoingFrameQueue();

    if (m_closed) {
        // The channel got closed because processOutgoingFrameQueue() failed.
        return;
    }

    m_closing = true;
    if (m_client)
        m_client->didStartClosingHandshake();
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
    const char* frameEnd;
    String errorString;
    WebSocketFrame::ParseFrameResult result = WebSocketFrame::parseFrame(m_buffer.data(), m_buffer.size(), frame, frameEnd, errorString);
    if (result == WebSocketFrame::FrameIncomplete)
        return false;
    if (result == WebSocketFrame::FrameError) {
        fail(errorString);
        return false;
    }

    ASSERT(m_buffer.data() < frameEnd);
    ASSERT(frameEnd <= m_buffer.data() + m_buffer.size());

    auto inflateResult = m_deflateFramer.inflate(frame);
    if (!inflateResult->succeeded()) {
        fail(inflateResult->failureReason());
        return false;
    }

    // Validate the frame data.
    if (WebSocketFrame::isReservedOpCode(frame.opCode)) {
        fail(makeString("Unrecognized frame opcode: ", static_cast<unsigned>(frame.opCode)));
        return false;
    }

    if (frame.reserved2 || frame.reserved3) {
        fail(makeString("One or more reserved bits are on: reserved2 = ", static_cast<unsigned>(frame.reserved2), ", reserved3 = ", static_cast<unsigned>(frame.reserved3)));
        return false;
    }

    if (frame.masked) {
        fail("A server must not mask any frames that it sends to the client.");
        return false;
    }

    // All control frames must not be fragmented.
    if (WebSocketFrame::isControlOpCode(frame.opCode) && !frame.final) {
        fail(makeString("Received fragmented control frame: opcode = ", static_cast<unsigned>(frame.opCode)));
        return false;
    }

    // All control frames must have a payload of 125 bytes or less, which means the frame must not contain
    // the "extended payload length" field.
    if (WebSocketFrame::isControlOpCode(frame.opCode) && WebSocketFrame::needsExtendedLengthField(frame.payloadLength)) {
        fail(makeString("Received control frame having too long payload: ", frame.payloadLength, " bytes"));
        return false;
    }

    // A new data frame is received before the previous continuous frame finishes.
    // Note that control frames are allowed to come in the middle of continuous frames.
    if (m_hasContinuousFrame && frame.opCode != WebSocketFrame::OpCodeContinuation && !WebSocketFrame::isControlOpCode(frame.opCode)) {
        fail("Received new data frame but previous continuous frame is unfinished.");
        return false;
    }

    InspectorInstrumentation::didReceiveWebSocketFrame(m_document, m_identifier, frame);

    switch (frame.opCode) {
    case WebSocketFrame::OpCodeContinuation:
        // An unexpected continuation frame is received without any leading frame.
        if (!m_hasContinuousFrame) {
            fail("Received unexpected continuation frame.");
            return false;
        }
        m_continuousFrameData.append(frame.payload, frame.payloadLength);
        skipBuffer(frameEnd - m_buffer.data());
        if (frame.final) {
            // onmessage handler may eventually call the other methods of this channel,
            // so we should pretend that we have finished to read this frame and
            // make sure that the member variables are in a consistent state before
            // the handler is invoked.
            Vector<uint8_t> continuousFrameData = WTFMove(m_continuousFrameData);
            m_hasContinuousFrame = false;
            if (m_continuousFrameOpCode == WebSocketFrame::OpCodeText) {
                String message;
                if (continuousFrameData.size())
                    message = String::fromUTF8(continuousFrameData.data(), continuousFrameData.size());
                else
                    message = emptyString();
                if (message.isNull())
                    fail("Could not decode a text frame as UTF-8.");
                else
                    m_client->didReceiveMessage(message);
            } else if (m_continuousFrameOpCode == WebSocketFrame::OpCodeBinary)
                m_client->didReceiveBinaryData(WTFMove(continuousFrameData));
        }
        break;

    case WebSocketFrame::OpCodeText:
        if (frame.final) {
            String message;
            if (frame.payloadLength)
                message = String::fromUTF8(frame.payload, frame.payloadLength);
            else
                message = emptyString();
            skipBuffer(frameEnd - m_buffer.data());
            if (message.isNull())
                fail("Could not decode a text frame as UTF-8.");
            else
                m_client->didReceiveMessage(message);
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = WebSocketFrame::OpCodeText;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload, frame.payloadLength);
            skipBuffer(frameEnd - m_buffer.data());
        }
        break;

    case WebSocketFrame::OpCodeBinary:
        if (frame.final) {
            Vector<uint8_t> binaryData(frame.payloadLength);
            memcpy(binaryData.data(), frame.payload, frame.payloadLength);
            skipBuffer(frameEnd - m_buffer.data());
            m_client->didReceiveBinaryData(WTFMove(binaryData));
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = WebSocketFrame::OpCodeBinary;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload, frame.payloadLength);
            skipBuffer(frameEnd - m_buffer.data());
        }
        break;

    case WebSocketFrame::OpCodeClose:
        if (!frame.payloadLength)
            m_closeEventCode = CloseEventCodeNoStatusRcvd;
        else if (frame.payloadLength == 1) {
            m_closeEventCode = CloseEventCodeAbnormalClosure;
            fail("Received a broken close frame containing an invalid size body.");
            return false;
        } else {
            unsigned char highByte = static_cast<unsigned char>(frame.payload[0]);
            unsigned char lowByte = static_cast<unsigned char>(frame.payload[1]);
            m_closeEventCode = highByte << 8 | lowByte;
            if (m_closeEventCode == CloseEventCodeNoStatusRcvd || m_closeEventCode == CloseEventCodeAbnormalClosure || m_closeEventCode == CloseEventCodeTLSHandshake) {
                m_closeEventCode = CloseEventCodeAbnormalClosure;
                fail("Received a broken close frame containing a reserved status code.");
                return false;
            }
        }
        if (frame.payloadLength >= 3)
            m_closeEventReason = String::fromUTF8(&frame.payload[2], frame.payloadLength - 2);
        else
            m_closeEventReason = emptyString();
        skipBuffer(frameEnd - m_buffer.data());
        m_receivedClosingHandshake = true;
        startClosingHandshake(m_closeEventCode, m_closeEventReason);
        if (m_closing) {
            if (m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen)
                m_outgoingFrameQueueStatus = OutgoingFrameQueueClosing;
            processOutgoingFrameQueue();
        }
        break;

    case WebSocketFrame::OpCodePing:
        enqueueRawFrame(WebSocketFrame::OpCodePong, frame.payload, frame.payloadLength);
        skipBuffer(frameEnd - m_buffer.data());
        processOutgoingFrameQueue();
        break;

    case WebSocketFrame::OpCodePong:
        // A server may send a pong in response to our ping, or an unsolicited pong which is not associated with
        // any specific ping. Either way, there's nothing to do on receipt of pong.
        skipBuffer(frameEnd - m_buffer.data());
        break;

    default:
        ASSERT_NOT_REACHED();
        skipBuffer(frameEnd - m_buffer.data());
        break;
    }

    return !m_buffer.isEmpty();
}

void WebSocketChannel::enqueueTextFrame(const CString& string)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    auto frame = std::make_unique<QueuedFrame>();
    frame->opCode = WebSocketFrame::OpCodeText;
    frame->frameType = QueuedFrameTypeString;
    frame->stringData = string;
    m_outgoingFrameQueue.append(WTFMove(frame));
}

void WebSocketChannel::enqueueRawFrame(WebSocketFrame::OpCode opCode, const char* data, size_t dataLength)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    auto frame = std::make_unique<QueuedFrame>();
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeVector;
    frame->vectorData.resize(dataLength);
    if (dataLength)
        memcpy(frame->vectorData.data(), data, dataLength);
    m_outgoingFrameQueue.append(WTFMove(frame));
}

void WebSocketChannel::enqueueBlobFrame(WebSocketFrame::OpCode opCode, Blob& blob)
{
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    auto frame = std::make_unique<QueuedFrame>();
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeBlob;
    frame->blobData = &blob;
    m_outgoingFrameQueue.append(WTFMove(frame));
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
            sendFrame(frame->opCode, frame->stringData.data(), frame->stringData.length(), [this, protectedThis = makeRef(*this)] (bool success) {
                if (!success)
                    fail("Failed to send WebSocket frame.");
            });
            break;
        }

        case QueuedFrameTypeVector:
            sendFrame(frame->opCode, frame->vectorData.data(), frame->vectorData.size(), [this, protectedThis = makeRef(*this)] (bool success) {
                if (!success)
                    fail("Failed to send WebSocket frame.");
            });
            break;

        case QueuedFrameTypeBlob: {
            switch (m_blobLoaderStatus) {
            case BlobLoaderNotStarted:
                ref(); // Will be derefed after didFinishLoading() or didFail().
                ASSERT(!m_blobLoader);
                ASSERT(frame->blobData);
                m_blobLoader = std::make_unique<FileReaderLoader>(FileReaderLoader::ReadAsArrayBuffer, this);
                m_blobLoaderStatus = BlobLoaderStarted;
                m_blobLoader->start(m_document, *frame->blobData);
                m_outgoingFrameQueue.prepend(WTFMove(frame));
                return;

            case BlobLoaderStarted:
            case BlobLoaderFailed:
                m_outgoingFrameQueue.prepend(WTFMove(frame));
                return;

            case BlobLoaderFinished: {
                RefPtr<ArrayBuffer> result = m_blobLoader->arrayBufferResult();
                m_blobLoader = nullptr;
                m_blobLoaderStatus = BlobLoaderNotStarted;
                sendFrame(frame->opCode, static_cast<const char*>(result->data()), result->byteLength(), [this, protectedThis = makeRef(*this)] (bool success) {
                    if (!success)
                        fail("Failed to send WebSocket frame.");
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
        didFail(FileError::ABORT_ERR);
    }
}

void WebSocketChannel::sendFrame(WebSocketFrame::OpCode opCode, const char* data, size_t dataLength, WTF::Function<void(bool)> completionHandler)
{
    ASSERT(m_handle);
    ASSERT(!m_suspended);

    WebSocketFrame frame(opCode, true, false, true, data, dataLength);
    InspectorInstrumentation::didSendWebSocketFrame(m_document, m_identifier, frame);

    auto deflateResult = m_deflateFramer.deflate(frame);
    if (!deflateResult->succeeded()) {
        fail(deflateResult->failureReason());
        return completionHandler(false);
    }

    Vector<char> frameData;
    frame.makeFrameData(frameData);

    m_handle->sendData(frameData.data(), frameData.size(), WTFMove(completionHandler));
}

ResourceRequest WebSocketChannel::clientHandshakeRequest()
{
    return m_handshake->clientHandshakeRequest();
}

const ResourceResponse& WebSocketChannel::serverHandshakeResponse() const
{
    return m_handshake->serverHandshakeResponse();
}

WebSocketHandshake::Mode WebSocketChannel::handshakeMode() const
{
    return m_handshake->mode();
}

} // namespace WebCore
