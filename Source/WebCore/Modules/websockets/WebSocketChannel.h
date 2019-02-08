/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#pragma once

#include "FileReaderLoaderClient.h"
#include "SocketStreamHandleClient.h"
#include "ThreadableWebSocketChannel.h"
#include "Timer.h"
#include "WebSocketDeflateFramer.h"
#include "WebSocketFrame.h"
#include "WebSocketHandshake.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

class Blob;
class Document;
class FileReaderLoader;
class ResourceRequest;
class ResourceResponse;
class SocketProvider;
class SocketStreamHandle;
class SocketStreamError;
class WebSocketChannelClient;

class WebSocketChannel : public RefCounted<WebSocketChannel>, public SocketStreamHandleClient, public ThreadableWebSocketChannel, public FileReaderLoaderClient
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<WebSocketChannel> create(Document& document, WebSocketChannelClient& client, SocketProvider& provider) { return adoptRef(*new WebSocketChannel(document, client, provider)); }
    virtual ~WebSocketChannel();

    bool isWebSocketChannel() const final { return true; }

    bool send(const char* data, int length);

    // ThreadableWebSocketChannel functions.
    void connect(const URL&, const String& protocol) override;
    String subprotocol() override;
    String extensions() override;
    ThreadableWebSocketChannel::SendResult send(const String& message) override;
    ThreadableWebSocketChannel::SendResult send(const JSC::ArrayBuffer&, unsigned byteOffset, unsigned byteLength) override;
    ThreadableWebSocketChannel::SendResult send(Blob&) override;
    unsigned bufferedAmount() const override;
    void close(int code, const String& reason) override; // Start closing handshake.
    void fail(const String& reason) override;
    void disconnect() override;

    void suspend() override;
    void resume() override;

    // SocketStreamHandleClient functions.
    void didOpenSocketStream(SocketStreamHandle&) final;
    void didCloseSocketStream(SocketStreamHandle&) final;
    void didReceiveSocketStreamData(SocketStreamHandle&, const char*, size_t) final;
    void didFailToReceiveSocketStreamData(SocketStreamHandle&) final;
    void didUpdateBufferedAmount(SocketStreamHandle&, size_t bufferedAmount) final;
    void didFailSocketStream(SocketStreamHandle&, const SocketStreamError&) final;

    enum CloseEventCode {
        CloseEventCodeNotSpecified = -1,
        CloseEventCodeNormalClosure = 1000,
        CloseEventCodeGoingAway = 1001,
        CloseEventCodeProtocolError = 1002,
        CloseEventCodeUnsupportedData = 1003,
        CloseEventCodeFrameTooLarge = 1004,
        CloseEventCodeNoStatusRcvd = 1005,
        CloseEventCodeAbnormalClosure = 1006,
        CloseEventCodeInvalidFramePayloadData = 1007,
        CloseEventCodePolicyViolation = 1008,
        CloseEventCodeMessageTooBig = 1009,
        CloseEventCodeMandatoryExt = 1010,
        CloseEventCodeInternalError = 1011,
        CloseEventCodeTLSHandshake = 1015,
        CloseEventCodeMinimumUserDefined = 3000,
        CloseEventCodeMaximumUserDefined = 4999
    };

    // FileReaderLoaderClient functions.
    void didStartLoading() override;
    void didReceiveData() override;
    void didFinishLoading() override;
    void didFail(int errorCode) override;

    unsigned identifier() const { return m_identifier; }
    ResourceRequest clientHandshakeRequest();
    const ResourceResponse& serverHandshakeResponse() const;
    WebSocketHandshake::Mode handshakeMode() const;

    using RefCounted<WebSocketChannel>::ref;
    using RefCounted<WebSocketChannel>::deref;

protected:
    void refThreadableWebSocketChannel() override { ref(); }
    void derefThreadableWebSocketChannel() override { deref(); }

private:
    WEBCORE_EXPORT WebSocketChannel(Document&, WebSocketChannelClient&, SocketProvider&);

    bool appendToBuffer(const char* data, size_t len);
    void skipBuffer(size_t len);
    bool processBuffer();
    void resumeTimerFired();
    void startClosingHandshake(int code, const String& reason);
    void closingTimerFired();

    bool processFrame();

    // It is allowed to send a Blob as a binary frame if hybi-10 protocol is in use. Sending a Blob
    // can be delayed because it must be read asynchronously. Other types of data (String or
    // ArrayBuffer) may also be blocked by preceding sending request of a Blob.
    //
    // To address this situation, messages to be sent need to be stored in a queue. Whenever a new
    // data frame is going to be sent, it first must go to the queue. Items in the queue are processed
    // in the order they were put into the queue. Sending request of a Blob blocks further processing
    // until the Blob is completely read and sent to the socket stream.
    enum QueuedFrameType {
        QueuedFrameTypeString,
        QueuedFrameTypeVector,
        QueuedFrameTypeBlob
    };
    struct QueuedFrame {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        WebSocketFrame::OpCode opCode;
        QueuedFrameType frameType;
        // Only one of the following items is used, according to the value of frameType.
        CString stringData;
        Vector<char> vectorData;
        RefPtr<Blob> blobData;
    };
    void enqueueTextFrame(const CString&);
    void enqueueRawFrame(WebSocketFrame::OpCode, const char* data, size_t dataLength);
    void enqueueBlobFrame(WebSocketFrame::OpCode, Blob&);

    void processOutgoingFrameQueue();
    void abortOutgoingFrameQueue();

    enum OutgoingFrameQueueStatus {
        // It is allowed to put a new item into the queue.
        OutgoingFrameQueueOpen,
        // Close frame has already been put into the queue but may not have been sent yet;
        // m_handle->close() will be called as soon as the queue is cleared. It is not
        // allowed to put a new item into the queue.
        OutgoingFrameQueueClosing,
        // Close frame has been sent or the queue was aborted. It is not allowed to put
        // a new item to the queue.
        OutgoingFrameQueueClosed
    };

    // If you are going to send a hybi-10 frame, you need to use the outgoing frame queue
    // instead of call sendFrame() directly.
    void sendFrame(WebSocketFrame::OpCode, const char* data, size_t dataLength, WTF::Function<void(bool)> completionHandler);

    enum BlobLoaderStatus {
        BlobLoaderNotStarted,
        BlobLoaderStarted,
        BlobLoaderFinished,
        BlobLoaderFailed
    };

    Document* m_document;
    WebSocketChannelClient* m_client;
    std::unique_ptr<WebSocketHandshake> m_handshake;
    RefPtr<SocketStreamHandle> m_handle;
    Vector<char> m_buffer;

    Timer m_resumeTimer;
    bool m_suspended { false };
    bool m_closing { false };
    bool m_receivedClosingHandshake { false };
    Timer m_closingTimer;
    bool m_closed { false };
    bool m_shouldDiscardReceivedData { false };
    unsigned m_unhandledBufferedAmount { 0 };

    unsigned m_identifier { 0 }; // m_identifier == 0 means that we could not obtain a valid identifier.

    // Private members only for hybi-10 protocol.
    bool m_hasContinuousFrame { false };
    WebSocketFrame::OpCode m_continuousFrameOpCode;
    Vector<uint8_t> m_continuousFrameData;
    unsigned short m_closeEventCode { CloseEventCodeAbnormalClosure };
    String m_closeEventReason;

    Deque<std::unique_ptr<QueuedFrame>> m_outgoingFrameQueue;
    OutgoingFrameQueueStatus m_outgoingFrameQueueStatus { OutgoingFrameQueueOpen };

    // FIXME: Load two or more Blobs simultaneously for better performance.
    std::unique_ptr<FileReaderLoader> m_blobLoader;
    BlobLoaderStatus m_blobLoaderStatus { BlobLoaderNotStarted };

    WebSocketDeflateFramer m_deflateFramer;
    Ref<SocketProvider> m_socketProvider;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebSocketChannel)
    static bool isType(const WebCore::ThreadableWebSocketChannel& threadableWebSocketChannel) { return threadableWebSocketChannel.isWebSocketChannel(); }
SPECIALIZE_TYPE_TRAITS_END()
