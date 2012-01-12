/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#if ENABLE(WEB_SOCKETS)

#include "WebSocketChannel.h"

#include "Blob.h"
#include "CookieJar.h"
#include "Document.h"
#include "FileError.h"
#include "FileReaderLoader.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "SocketStreamError.h"
#include "SocketStreamHandle.h"
#include "WebSocketChannelClient.h"
#include "WebSocketHandshake.h"

#include <wtf/ArrayBuffer.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace std;

namespace WebCore {

const double TCPMaximumSegmentLifetime = 2 * 60.0;

// Constants for hybi-10 frame format.
const unsigned char finalBit = 0x80;
const unsigned char reserved1Bit = 0x40;
const unsigned char reserved2Bit = 0x20;
const unsigned char reserved3Bit = 0x10;
const unsigned char opCodeMask = 0xF;
const unsigned char maskBit = 0x80;
const unsigned char payloadLengthMask = 0x7F;
const size_t maxPayloadLengthWithoutExtendedLengthField = 125;
const size_t payloadLengthWithTwoByteExtendedLengthField = 126;
const size_t payloadLengthWithEightByteExtendedLengthField = 127;
const size_t maskingKeyWidthInBytes = 4;

const WebSocketChannel::OpCode WebSocketChannel::OpCodeContinuation = 0x0;
const WebSocketChannel::OpCode WebSocketChannel::OpCodeText = 0x1;
const WebSocketChannel::OpCode WebSocketChannel::OpCodeBinary = 0x2;
const WebSocketChannel::OpCode WebSocketChannel::OpCodeClose = 0x8;
const WebSocketChannel::OpCode WebSocketChannel::OpCodePing = 0x9;
const WebSocketChannel::OpCode WebSocketChannel::OpCodePong = 0xA;

WebSocketChannel::WebSocketChannel(ScriptExecutionContext* context, WebSocketChannelClient* client)
    : m_context(context)
    , m_client(client)
    , m_buffer(0)
    , m_bufferSize(0)
    , m_resumeTimer(this, &WebSocketChannel::resumeTimerFired)
    , m_suspended(false)
    , m_closing(false)
    , m_receivedClosingHandshake(false)
    , m_closingTimer(this, &WebSocketChannel::closingTimerFired)
    , m_closed(false)
    , m_shouldDiscardReceivedData(false)
    , m_unhandledBufferedAmount(0)
    , m_identifier(0)
    , m_useHixie76Protocol(true)
    , m_hasContinuousFrame(false)
    , m_closeEventCode(CloseEventCodeAbnormalClosure)
    , m_outgoingFrameQueueStatus(OutgoingFrameQueueOpen)
#if ENABLE(BLOB)
    , m_blobLoaderStatus(BlobLoaderNotStarted)
#endif
{
    ASSERT(m_context->isDocument());
    Document* document = static_cast<Document*>(m_context);
    if (Settings* settings = document->settings())
        m_useHixie76Protocol = settings->useHixie76WebSocketProtocol();

    if (Page* page = document->page())
        m_identifier = page->progress()->createUniqueIdentifier();
}

WebSocketChannel::~WebSocketChannel()
{
    fastFree(m_buffer);
}

bool WebSocketChannel::useHixie76Protocol()
{
    return m_useHixie76Protocol;
}

void WebSocketChannel::connect(const KURL& url, const String& protocol)
{
    LOG(Network, "WebSocketChannel %p connect", this);
    ASSERT(!m_handle);
    ASSERT(!m_suspended);
    m_handshake = adoptPtr(new WebSocketHandshake(url, protocol, m_context, m_useHixie76Protocol));
    m_handshake->reset();
    if (m_identifier)
        InspectorInstrumentation::didCreateWebSocket(m_context, m_identifier, url, m_context->url());
    ref();
    m_handle = SocketStreamHandle::create(m_handshake->url(), this);
}

String WebSocketChannel::subprotocol()
{
    LOG(Network, "WebSocketChannel %p subprotocol", this);
    if (!m_handshake || m_handshake->mode() != WebSocketHandshake::Connected)
        return "";
    String serverProtocol = m_handshake->serverWebSocketProtocol();
    if (serverProtocol.isNull())
        return "";
    return serverProtocol;
}

bool WebSocketChannel::send(const String& message)
{
    LOG(Network, "WebSocketChannel %p send %s", this, message.utf8().data());
    if (m_useHixie76Protocol) {
        CString utf8 = message.utf8();
        return sendFrameHixie76(utf8.data(), utf8.length());
    }
    enqueueTextFrame(message);
    // According to WebSocket API specification, WebSocket.send() should return void instead
    // of boolean. However, our implementation still returns boolean due to compatibility
    // concern (see bug 65850).
    // m_channel->send() may happen later, thus it's not always possible to know whether
    // the message has been sent to the socket successfully. In this case, we have no choice
    // but to return true.
    return true;
}

bool WebSocketChannel::send(const ArrayBuffer& binaryData)
{
    LOG(Network, "WebSocketChannel %p send arraybuffer %p", this, &binaryData);
    ASSERT(!m_useHixie76Protocol);
    enqueueRawFrame(OpCodeBinary, static_cast<const char*>(binaryData.data()), binaryData.byteLength());
    return true;
}

bool WebSocketChannel::send(const Blob& binaryData)
{
    LOG(Network, "WebSocketChannel %p send blob %s", this, binaryData.url().string().utf8().data());
    ASSERT(!m_useHixie76Protocol);
    enqueueBlobFrame(OpCodeBinary, binaryData);
    return true;
}

bool WebSocketChannel::send(const char* data, int length)
{
    LOG(Network, "WebSocketChannel %p send binary %p (%dB)", this, data, length);
    ASSERT(!m_useHixie76Protocol);
    enqueueRawFrame(OpCodeBinary, data, length);
    return true;
}

unsigned long WebSocketChannel::bufferedAmount() const
{
    LOG(Network, "WebSocketChannel %p bufferedAmount", this);
    ASSERT(m_handle);
    ASSERT(!m_suspended);
    return m_handle->bufferedAmount();
}

void WebSocketChannel::close(int code, const String& reason)
{
    LOG(Network, "WebSocketChannel %p close", this);
    ASSERT(!m_suspended);
    if (!m_handle)
        return;
    startClosingHandshake(code, reason);
    if (m_closing && !m_closingTimer.isActive())
        m_closingTimer.startOneShot(2 * TCPMaximumSegmentLifetime);
}

void WebSocketChannel::fail(const String& reason)
{
    LOG(Network, "WebSocketChannel %p fail: %s", this, reason.utf8().data());
    ASSERT(!m_suspended);
    if (m_context)
        m_context->addConsoleMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, reason, m_handshake->clientOrigin());
    if (!m_useHixie76Protocol) {
        // Hybi-10 specification explicitly states we must not continue to handle incoming data
        // once the WebSocket connection is failed (section 7.1.7).
        // FIXME: Should we do this in hixie-76 too?
        m_shouldDiscardReceivedData = true;
        if (m_buffer)
            skipBuffer(m_bufferSize); // Save memory.
        m_hasContinuousFrame = false;
        m_continuousFrameData.clear();
    }
    if (m_handle && !m_closed)
        m_handle->disconnect(); // Will call didClose().
}

void WebSocketChannel::disconnect()
{
    LOG(Network, "WebSocketChannel %p disconnect", this);
    if (m_identifier && m_context)
        InspectorInstrumentation::didCloseWebSocket(m_context, m_identifier);
    if (m_handshake)
        m_handshake->clearScriptExecutionContext();
    m_client = 0;
    m_context = 0;
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
    if ((m_buffer || m_closed) && m_client && !m_resumeTimer.isActive())
        m_resumeTimer.startOneShot(0);
}

void WebSocketChannel::didOpenSocketStream(SocketStreamHandle* handle)
{
    LOG(Network, "WebSocketChannel %p didOpenSocketStream", this);
    ASSERT(handle == m_handle);
    if (!m_context)
        return;
    if (m_identifier)
        InspectorInstrumentation::willSendWebSocketHandshakeRequest(m_context, m_identifier, m_handshake->clientHandshakeRequest());
    CString handshakeMessage = m_handshake->clientHandshakeMessage();
    if (!handle->send(handshakeMessage.data(), handshakeMessage.length()))
        fail("Failed to send WebSocket handshake.");
}

void WebSocketChannel::didCloseSocketStream(SocketStreamHandle* handle)
{
    LOG(Network, "WebSocketChannel %p didCloseSocketStream", this);
    if (m_identifier && m_context)
        InspectorInstrumentation::didCloseWebSocket(m_context, m_identifier);
    ASSERT_UNUSED(handle, handle == m_handle || !m_handle);
    m_closed = true;
    if (m_closingTimer.isActive())
        m_closingTimer.stop();
    if (!m_useHixie76Protocol && m_outgoingFrameQueueStatus != OutgoingFrameQueueClosed)
        abortOutgoingFrameQueue();
    if (m_handle) {
        m_unhandledBufferedAmount = m_handle->bufferedAmount();
        if (m_suspended)
            return;
        WebSocketChannelClient* client = m_client;
        m_client = 0;
        m_context = 0;
        m_handle = 0;
        if (client)
            client->didClose(m_unhandledBufferedAmount, m_receivedClosingHandshake ? WebSocketChannelClient::ClosingHandshakeComplete : WebSocketChannelClient::ClosingHandshakeIncomplete, m_closeEventCode, m_closeEventReason);
    }
    deref();
}

void WebSocketChannel::didReceiveSocketStreamData(SocketStreamHandle* handle, const char* data, int len)
{
    LOG(Network, "WebSocketChannel %p didReceiveSocketStreamData %d", this, len);
    RefPtr<WebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.
    ASSERT(handle == m_handle);
    if (!m_context) {
        return;
    }
    if (len <= 0) {
        handle->disconnect();
        return;
    }
    if (!m_client) {
        m_shouldDiscardReceivedData = true;
        handle->disconnect();
        return;
    }
    if (m_shouldDiscardReceivedData)
        return;
    if (!appendToBuffer(data, len)) {
        m_shouldDiscardReceivedData = true;
        fail("Ran out of memory while receiving WebSocket data.");
        return;
    }
    while (!m_suspended && m_client && m_buffer)
        if (!processBuffer())
            break;
}

void WebSocketChannel::didUpdateBufferedAmount(SocketStreamHandle*, size_t bufferedAmount)
{
    if (m_client)
        m_client->didUpdateBufferedAmount(bufferedAmount);
}

void WebSocketChannel::didFailSocketStream(SocketStreamHandle* handle, const SocketStreamError& error)
{
    LOG(Network, "WebSocketChannel %p didFailSocketStream", this);
    ASSERT(handle == m_handle || !m_handle);
    if (m_context) {
        String message;
        if (error.isNull())
            message = "WebSocket network error";
        else if (error.localizedDescription().isNull())
            message = "WebSocket network error: error code " + String::number(error.errorCode());
        else
            message = "WebSocket network error: " + error.localizedDescription();
        String failingURL = error.failingURL();
        ASSERT(failingURL.isNull() || m_handshake->url().string() == failingURL);
        if (failingURL.isNull())
            failingURL = m_handshake->url().string();
        m_context->addConsoleMessage(NetworkMessageSource, LogMessageType, ErrorMessageLevel, message, failingURL);
    }
    m_shouldDiscardReceivedData = true;
    handle->disconnect();
}

void WebSocketChannel::didReceiveAuthenticationChallenge(SocketStreamHandle*, const AuthenticationChallenge&)
{
}

void WebSocketChannel::didCancelAuthenticationChallenge(SocketStreamHandle*, const AuthenticationChallenge&)
{
}

#if ENABLE(BLOB)
void WebSocketChannel::didStartLoading()
{
    LOG(Network, "WebSocketChannel %p didStartLoading", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
}

void WebSocketChannel::didReceiveData()
{
    LOG(Network, "WebSocketChannel %p didReceiveData", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
}

void WebSocketChannel::didFinishLoading()
{
    LOG(Network, "WebSocketChannel %p didFinishLoading", this);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoaderStatus = BlobLoaderFinished;
    processOutgoingFrameQueue();
    deref();
}

void WebSocketChannel::didFail(int errorCode)
{
    LOG(Network, "WebSocketChannel %p didFail %d", this, errorCode);
    ASSERT(m_blobLoader);
    ASSERT(m_blobLoaderStatus == BlobLoaderStarted);
    m_blobLoader.clear();
    m_blobLoaderStatus = BlobLoaderFailed;
    fail("Failed to load Blob: error code = " + String::number(errorCode)); // FIXME: Generate human-friendly reason message.
    deref();
}
#endif

bool WebSocketChannel::appendToBuffer(const char* data, size_t len)
{
    size_t newBufferSize = m_bufferSize + len;
    if (newBufferSize < m_bufferSize) {
        LOG(Network, "WebSocket buffer overflow (%lu+%lu)", static_cast<unsigned long>(m_bufferSize), static_cast<unsigned long>(len));
        return false;
    }
    char* newBuffer = 0;
    if (!tryFastMalloc(newBufferSize).getValue(newBuffer))
        return false;

    if (m_buffer)
        memcpy(newBuffer, m_buffer, m_bufferSize);
    memcpy(newBuffer + m_bufferSize, data, len);
    fastFree(m_buffer);
    m_buffer = newBuffer;
    m_bufferSize = newBufferSize;
    return true;
}

void WebSocketChannel::skipBuffer(size_t len)
{
    ASSERT(len <= m_bufferSize);
    m_bufferSize -= len;
    if (!m_bufferSize) {
        fastFree(m_buffer);
        m_buffer = 0;
        return;
    }
    memmove(m_buffer, m_buffer + len, m_bufferSize);
}

bool WebSocketChannel::processBuffer()
{
    ASSERT(!m_suspended);
    ASSERT(m_client);
    ASSERT(m_buffer);
    LOG(Network, "WebSocketChannel %p processBuffer %lu", this, static_cast<unsigned long>(m_bufferSize));

    if (m_shouldDiscardReceivedData)
        return false;

    if (m_receivedClosingHandshake) {
        skipBuffer(m_bufferSize);
        return false;
    }

    RefPtr<WebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.

    if (m_handshake->mode() == WebSocketHandshake::Incomplete) {
        int headerLength = m_handshake->readServerHandshake(m_buffer, m_bufferSize);
        if (headerLength <= 0)
            return false;
        if (m_handshake->mode() == WebSocketHandshake::Connected) {
            if (m_identifier)
                InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(m_context, m_identifier, m_handshake->serverHandshakeResponse());
            if (!m_handshake->serverSetCookie().isEmpty()) {
                if (m_context->isDocument()) {
                    Document* document = static_cast<Document*>(m_context);
                    if (cookiesEnabled(document)) {
                        ExceptionCode ec; // Exception (for sandboxed documents) ignored.
                        document->setCookie(m_handshake->serverSetCookie(), ec);
                    }
                }
            }
            // FIXME: handle set-cookie2.
            LOG(Network, "WebSocketChannel %p connected", this);
            skipBuffer(headerLength);
            m_client->didConnect();
            LOG(Network, "remaining in read buf %lu", static_cast<unsigned long>(m_bufferSize));
            return m_buffer;
        }
        ASSERT(m_handshake->mode() == WebSocketHandshake::Failed);
        LOG(Network, "WebSocketChannel %p connection failed", this);
        skipBuffer(headerLength);
        m_shouldDiscardReceivedData = true;
        fail(m_handshake->failureReason());
        return false;
    }
    if (m_handshake->mode() != WebSocketHandshake::Connected)
        return false;

    if (m_useHixie76Protocol)
        return processFrameHixie76();

    return processFrame();
}

void WebSocketChannel::resumeTimerFired(Timer<WebSocketChannel>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_resumeTimer);

    RefPtr<WebSocketChannel> protect(this); // The client can close the channel, potentially removing the last reference.
    while (!m_suspended && m_client && m_buffer)
        if (!processBuffer())
            break;
    if (!m_suspended && m_client && m_closed && m_handle)
        didCloseSocketStream(m_handle.get());
}

void WebSocketChannel::startClosingHandshake(int code, const String& reason)
{
    LOG(Network, "WebSocketChannel %p closing %d %d", this, m_closing, m_receivedClosingHandshake);
    if (m_closing)
        return;
    ASSERT(m_handle);
    if (m_useHixie76Protocol) {
        Vector<char> buf;
        buf.append('\xff');
        buf.append('\0');
        if (!m_handle->send(buf.data(), buf.size())) {
            m_handle->disconnect();
            return;
        }
    } else {
        Vector<char> buf;
        if (!m_receivedClosingHandshake && code != CloseEventCodeNotSpecified) {
            unsigned char highByte = code >> 8;
            unsigned char lowByte = code;
            buf.append(static_cast<char>(highByte));
            buf.append(static_cast<char>(lowByte));
            buf.append(reason.utf8().data(), reason.utf8().length());
        }
        enqueueRawFrame(OpCodeClose, buf.data(), buf.size());
    }
    m_closing = true;
    if (m_client)
        m_client->didStartClosingHandshake();
}

void WebSocketChannel::closingTimerFired(Timer<WebSocketChannel>* timer)
{
    LOG(Network, "WebSocketChannel %p closing timer", this);
    ASSERT_UNUSED(timer, &m_closingTimer == timer);
    if (m_handle)
        m_handle->disconnect();
}

WebSocketChannel::ParseFrameResult WebSocketChannel::parseFrame(FrameData& frame)
{
    const char* p = m_buffer;
    const char* bufferEnd = m_buffer + m_bufferSize;

    if (m_bufferSize < 2)
        return FrameIncomplete;

    unsigned char firstByte = *p++;
    unsigned char secondByte = *p++;

    bool final = firstByte & finalBit;
    bool reserved1 = firstByte & reserved1Bit;
    bool reserved2 = firstByte & reserved2Bit;
    bool reserved3 = firstByte & reserved3Bit;
    OpCode opCode = firstByte & opCodeMask;

    bool masked = secondByte & maskBit;
    uint64_t payloadLength64 = secondByte & payloadLengthMask;
    if (payloadLength64 > maxPayloadLengthWithoutExtendedLengthField) {
        int extendedPayloadLengthSize;
        if (payloadLength64 == payloadLengthWithTwoByteExtendedLengthField)
            extendedPayloadLengthSize = 2;
        else {
            ASSERT(payloadLength64 == payloadLengthWithEightByteExtendedLengthField);
            extendedPayloadLengthSize = 8;
        }
        if (bufferEnd - p < extendedPayloadLengthSize)
            return FrameIncomplete;
        payloadLength64 = 0;
        for (int i = 0; i < extendedPayloadLengthSize; ++i) {
            payloadLength64 <<= 8;
            payloadLength64 |= static_cast<unsigned char>(*p++);
        }
    }

    // FIXME: UINT64_C(0x7FFFFFFFFFFFFFFF) should be used but it did not compile on Qt bots.
#if COMPILER(MSVC)
    static const uint64_t maxPayloadLength = 0x7FFFFFFFFFFFFFFFui64;
#else
    static const uint64_t maxPayloadLength = 0x7FFFFFFFFFFFFFFFull;
#endif
    size_t maskingKeyLength = masked ? maskingKeyWidthInBytes : 0;
    if (payloadLength64 > maxPayloadLength || payloadLength64 + maskingKeyLength > numeric_limits<size_t>::max()) {
        fail("WebSocket frame length too large: " + String::number(payloadLength64) + " bytes");
        return FrameError;
    }
    size_t payloadLength = static_cast<size_t>(payloadLength64);

    if (static_cast<size_t>(bufferEnd - p) < maskingKeyLength + payloadLength)
        return FrameIncomplete;

    if (masked) {
        const char* maskingKey = p;
        char* payload = const_cast<char*>(p + maskingKeyWidthInBytes);
        for (size_t i = 0; i < payloadLength; ++i)
            payload[i] ^= maskingKey[i % maskingKeyWidthInBytes]; // Unmask the payload.
    }

    frame.opCode = opCode;
    frame.final = final;
    frame.reserved1 = reserved1;
    frame.reserved2 = reserved2;
    frame.reserved3 = reserved3;
    frame.masked = masked;
    frame.payload = p + maskingKeyLength;
    frame.payloadLength = payloadLength;
    frame.frameEnd = p + maskingKeyLength + payloadLength;
    return FrameOK;
}

bool WebSocketChannel::processFrame()
{
    ASSERT(m_buffer);

    FrameData frame;
    if (parseFrame(frame) != FrameOK)
        return false;

    // Validate the frame data.
    if (isReservedOpCode(frame.opCode)) {
        fail("Unrecognized frame opcode: " + String::number(frame.opCode));
        return false;
    }

    if (frame.reserved1 || frame.reserved2 || frame.reserved3) {
        fail("One or more reserved bits are on: reserved1 = " + String::number(frame.reserved1) + ", reserved2 = " + String::number(frame.reserved2) + ", reserved3 = " + String::number(frame.reserved3));
        return false;
    }

    // All control frames must not be fragmented.
    if (isControlOpCode(frame.opCode) && !frame.final) {
        fail("Received fragmented control frame: opcode = " + String::number(frame.opCode));
        return false;
    }

    // All control frames must have a payload of 125 bytes or less, which means the frame must not contain
    // the "extended payload length" field.
    if (isControlOpCode(frame.opCode) && frame.payloadLength > maxPayloadLengthWithoutExtendedLengthField) {
        fail("Received control frame having too long payload: " + String::number(frame.payloadLength) + " bytes");
        return false;
    }

    // A new data frame is received before the previous continuous frame finishes.
    // Note that control frames are allowed to come in the middle of continuous frames.
    if (m_hasContinuousFrame && frame.opCode != OpCodeContinuation && !isControlOpCode(frame.opCode)) {
        fail("Received new data frame but previous continuous frame is unfinished.");
        return false;
    }

    switch (frame.opCode) {
    case OpCodeContinuation:
        // An unexpected continuation frame is received without any leading frame.
        if (!m_hasContinuousFrame) {
            fail("Received unexpected continuation frame.");
            return false;
        }
        m_continuousFrameData.append(frame.payload, frame.payloadLength);
        skipBuffer(frame.frameEnd - m_buffer);
        if (frame.final) {
            // onmessage handler may eventually call the other methods of this channel,
            // so we should pretend that we have finished to read this frame and
            // make sure that the member variables are in a consistent state before
            // the handler is invoked.
            // Vector<char>::swap() is used here to clear m_continuousFrameData.
            OwnPtr<Vector<char> > continuousFrameData = adoptPtr(new Vector<char>);
            m_continuousFrameData.swap(*continuousFrameData);
            m_hasContinuousFrame = false;
            if (m_continuousFrameOpCode == OpCodeText) {
                String message;
                if (continuousFrameData->size())
                    message = String::fromUTF8(continuousFrameData->data(), continuousFrameData->size());
                else
                    message = "";
                if (message.isNull())
                    fail("Could not decode a text frame as UTF-8.");
                else
                    m_client->didReceiveMessage(message);
            } else if (m_continuousFrameOpCode == OpCodeBinary)
                m_client->didReceiveBinaryData(continuousFrameData.release());
        }
        break;

    case OpCodeText:
        if (frame.final) {
            String message;
            if (frame.payloadLength)
                message = String::fromUTF8(frame.payload, frame.payloadLength);
            else
                message = "";
            skipBuffer(frame.frameEnd - m_buffer);
            if (message.isNull())
                fail("Could not decode a text frame as UTF-8.");
            else
                m_client->didReceiveMessage(message);
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = OpCodeText;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload, frame.payloadLength);
            skipBuffer(frame.frameEnd - m_buffer);
        }
        break;

    case OpCodeBinary:
        if (frame.final) {
            OwnPtr<Vector<char> > binaryData = adoptPtr(new Vector<char>(frame.payloadLength));
            memcpy(binaryData->data(), frame.payload, frame.payloadLength);
            skipBuffer(frame.frameEnd - m_buffer);
            m_client->didReceiveBinaryData(binaryData.release());
        } else {
            m_hasContinuousFrame = true;
            m_continuousFrameOpCode = OpCodeBinary;
            ASSERT(m_continuousFrameData.isEmpty());
            m_continuousFrameData.append(frame.payload, frame.payloadLength);
            skipBuffer(frame.frameEnd - m_buffer);
        }
        break;

    case OpCodeClose:
        if (frame.payloadLength >= 2) {
            unsigned char highByte = static_cast<unsigned char>(frame.payload[0]);
            unsigned char lowByte = static_cast<unsigned char>(frame.payload[1]);
            m_closeEventCode = highByte << 8 | lowByte;
        } else
            m_closeEventCode = CloseEventCodeNoStatusRcvd;
        if (frame.payloadLength >= 3)
            m_closeEventReason = String::fromUTF8(&frame.payload[2], frame.payloadLength - 2);
        else
            m_closeEventReason = "";
        skipBuffer(frame.frameEnd - m_buffer);
        m_receivedClosingHandshake = true;
        startClosingHandshake(m_closeEventCode, m_closeEventReason);
        if (m_closing) {
            m_outgoingFrameQueueStatus = OutgoingFrameQueueClosing;
            processOutgoingFrameQueue();
        }
        break;

    case OpCodePing:
        enqueueRawFrame(OpCodePong, frame.payload, frame.payloadLength);
        skipBuffer(frame.frameEnd - m_buffer);
        break;

    case OpCodePong:
        // A server may send a pong in response to our ping, or an unsolicited pong which is not associated with
        // any specific ping. Either way, there's nothing to do on receipt of pong.
        skipBuffer(frame.frameEnd - m_buffer);
        break;

    default:
        ASSERT_NOT_REACHED();
        skipBuffer(frame.frameEnd - m_buffer);
        break;
    }

    return m_buffer;
}

bool WebSocketChannel::processFrameHixie76()
{
    const char* nextFrame = m_buffer;
    const char* p = m_buffer;
    const char* end = p + m_bufferSize;

    unsigned char frameByte = static_cast<unsigned char>(*p++);
    if ((frameByte & 0x80) == 0x80) {
        size_t length = 0;
        bool errorFrame = false;
        bool lengthFinished = false;
        while (p < end) {
            if (length > numeric_limits<size_t>::max() / 128) {
                LOG(Network, "frame length overflow %lu", static_cast<unsigned long>(length));
                errorFrame = true;
                break;
            }
            size_t newLength = length * 128;
            unsigned char msgByte = static_cast<unsigned char>(*p);
            unsigned int lengthMsgByte = msgByte & 0x7f;
            if (newLength > numeric_limits<size_t>::max() - lengthMsgByte) {
                LOG(Network, "frame length overflow %lu+%u", static_cast<unsigned long>(newLength), lengthMsgByte);
                errorFrame = true;
                break;
            }
            newLength += lengthMsgByte;
            if (newLength < length) { // sanity check
                LOG(Network, "frame length integer wrap %lu->%lu", static_cast<unsigned long>(length), static_cast<unsigned long>(newLength));
                errorFrame = true;
                break;
            }
            length = newLength;
            ++p;
            if (!(msgByte & 0x80)) {
                lengthFinished = true;
                break;
            }
        }
        if (!errorFrame && !lengthFinished)
            return false;
        if (p + length < p) {
            LOG(Network, "frame buffer pointer wrap %p+%lu->%p", p, static_cast<unsigned long>(length), p + length);
            errorFrame = true;
        }
        if (errorFrame) {
            skipBuffer(m_bufferSize); // Save memory.
            m_shouldDiscardReceivedData = true;
            m_client->didReceiveMessageError();
            fail("WebSocket frame length too large");
            return false;
        }
        ASSERT(p + length >= p);
        if (p + length <= end) {
            p += length;
            nextFrame = p;
            ASSERT(nextFrame > m_buffer);
            skipBuffer(nextFrame - m_buffer);
            if (frameByte == 0xff && !length) {
                m_receivedClosingHandshake = true;
                startClosingHandshake(CloseEventCodeNotSpecified, "");
                if (m_closing)
                    m_handle->close(); // close after sending FF 00.
            } else
                m_client->didReceiveMessageError();
            return m_buffer;
        }
        return false;
    }

    const char* msgStart = p;
    while (p < end && *p != '\xff')
        ++p;
    if (p < end && *p == '\xff') {
        int msgLength = p - msgStart;
        ++p;
        nextFrame = p;
        if (frameByte == 0x00) {
            String msg = String::fromUTF8(msgStart, msgLength);
            skipBuffer(nextFrame - m_buffer);
            m_client->didReceiveMessage(msg);
        } else {
            skipBuffer(nextFrame - m_buffer);
            m_client->didReceiveMessageError();
        }
        return m_buffer;
    }
    return false;
}

void WebSocketChannel::enqueueTextFrame(const String& string)
{
    ASSERT(!m_useHixie76Protocol);
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    OwnPtr<QueuedFrame> frame = adoptPtr(new QueuedFrame);
    frame->opCode = OpCodeText;
    frame->frameType = QueuedFrameTypeString;
    frame->stringData = string;
    m_outgoingFrameQueue.append(frame.release());
    processOutgoingFrameQueue();
}

void WebSocketChannel::enqueueRawFrame(OpCode opCode, const char* data, size_t dataLength)
{
    ASSERT(!m_useHixie76Protocol);
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    OwnPtr<QueuedFrame> frame = adoptPtr(new QueuedFrame);
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeVector;
    frame->vectorData.resize(dataLength);
    if (dataLength)
        memcpy(frame->vectorData.data(), data, dataLength);
    m_outgoingFrameQueue.append(frame.release());
    processOutgoingFrameQueue();
}

void WebSocketChannel::enqueueBlobFrame(OpCode opCode, const Blob& blob)
{
    ASSERT(!m_useHixie76Protocol);
    ASSERT(m_outgoingFrameQueueStatus == OutgoingFrameQueueOpen);
    OwnPtr<QueuedFrame> frame = adoptPtr(new QueuedFrame);
    frame->opCode = opCode;
    frame->frameType = QueuedFrameTypeBlob;
    frame->blobData = Blob::create(blob.url(), blob.type(), blob.size());
    m_outgoingFrameQueue.append(frame.release());
    processOutgoingFrameQueue();
}

void WebSocketChannel::processOutgoingFrameQueue()
{
    ASSERT(!m_useHixie76Protocol);
    if (m_outgoingFrameQueueStatus == OutgoingFrameQueueClosed)
        return;

    while (!m_outgoingFrameQueue.isEmpty()) {
        OwnPtr<QueuedFrame> frame = m_outgoingFrameQueue.takeFirst();
        switch (frame->frameType) {
        case QueuedFrameTypeString: {
            CString utf8 = frame->stringData.utf8();
            if (!sendFrame(frame->opCode, utf8.data(), utf8.length()))
                fail("Failed to send WebSocket frame.");
            break;
        }

        case QueuedFrameTypeVector:
            if (!sendFrame(frame->opCode, frame->vectorData.data(), frame->vectorData.size()))
                fail("Failed to send WebSocket frame.");
            break;

        case QueuedFrameTypeBlob: {
#if ENABLE(BLOB)
            switch (m_blobLoaderStatus) {
            case BlobLoaderNotStarted:
                ref(); // Will be derefed after didFinishLoading() or didFail().
                ASSERT(!m_blobLoader);
                m_blobLoader = adoptPtr(new FileReaderLoader(FileReaderLoader::ReadAsArrayBuffer, this));
                m_blobLoaderStatus = BlobLoaderStarted;
                m_blobLoader->start(m_context, frame->blobData.get());
                m_outgoingFrameQueue.prepend(frame.release());
                return;

            case BlobLoaderStarted:
            case BlobLoaderFailed:
                m_outgoingFrameQueue.prepend(frame.release());
                return;

            case BlobLoaderFinished: {
                RefPtr<ArrayBuffer> result = m_blobLoader->arrayBufferResult();
                m_blobLoader.clear();
                m_blobLoaderStatus = BlobLoaderNotStarted;
                if (!sendFrame(frame->opCode, static_cast<const char*>(result->data()), result->byteLength()))
                    fail("Failed to send WebSocket frame.");
                break;
            }
            }
#else
            fail("FileReader is not available. Could not send a Blob as WebSocket binary message.");
#endif
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
    ASSERT(!m_useHixie76Protocol);
    m_outgoingFrameQueue.clear();
    m_outgoingFrameQueueStatus = OutgoingFrameQueueClosed;
#if ENABLE(BLOB)
    if (m_blobLoaderStatus == BlobLoaderStarted) {
        m_blobLoader->cancel();
        didFail(FileError::ABORT_ERR);
    }
#endif
}

bool WebSocketChannel::sendFrame(OpCode opCode, const char* data, size_t dataLength)
{
    ASSERT(m_handle);
    ASSERT(!m_suspended);

    Vector<char> frame;
    ASSERT(!(opCode & ~opCodeMask)); // Checks whether "opCode" fits in the range of opCodes.
    frame.append(finalBit | opCode);
    if (dataLength <= maxPayloadLengthWithoutExtendedLengthField)
        frame.append(maskBit | dataLength);
    else if (dataLength <= 0xFFFF) {
        frame.append(maskBit | payloadLengthWithTwoByteExtendedLengthField);
        frame.append((dataLength & 0xFF00) >> 8);
        frame.append(dataLength & 0xFF);
    } else {
        frame.append(maskBit | payloadLengthWithEightByteExtendedLengthField);
        char extendedPayloadLength[8];
        size_t remaining = dataLength;
        // Fill the length into extendedPayloadLength in the network byte order.
        for (int i = 0; i < 8; ++i) {
            extendedPayloadLength[7 - i] = remaining & 0xFF;
            remaining >>= 8;
        }
        ASSERT(!remaining);
        frame.append(extendedPayloadLength, 8);
    }

    // Mask the frame.
    size_t maskingKeyStart = frame.size();
    frame.grow(frame.size() + maskingKeyWidthInBytes); // Add placeholder for masking key. Will be overwritten.
    size_t payloadStart = frame.size();
    frame.append(data, dataLength);

    cryptographicallyRandomValues(frame.data() + maskingKeyStart, maskingKeyWidthInBytes);
    for (size_t i = 0; i < dataLength; ++i)
        frame[payloadStart + i] ^= frame[maskingKeyStart + i % maskingKeyWidthInBytes];

    return m_handle->send(frame.data(), frame.size());
}

bool WebSocketChannel::sendFrameHixie76(const char* data, size_t dataLength)
{
    ASSERT(m_handle);
    ASSERT(!m_suspended);

    Vector<char> frame;
    frame.append('\0'); // Frame type.
    frame.append(data, dataLength);
    frame.append('\xff'); // Frame end.
    return m_handle->send(frame.data(), frame.size());
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
