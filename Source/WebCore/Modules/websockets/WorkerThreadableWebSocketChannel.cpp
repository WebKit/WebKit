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

#include "config.h"
#include "WorkerThreadableWebSocketChannel.h"

#include "Blob.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ScriptExecutionContext.h"
#include "SocketProvider.h"
#include "ThreadableWebSocketChannelClientWrapper.h"
#include "WebSocketChannel.h"
#include "WebSocketChannelClient.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerRunLoop.h"
#include "WorkerThread.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WorkerThreadableWebSocketChannel::WorkerThreadableWebSocketChannel(WorkerGlobalScope& context, WebSocketChannelClient& client, const String& taskMode, SocketProvider& provider)
    : m_workerGlobalScope(context)
    , m_workerClientWrapper(ThreadableWebSocketChannelClientWrapper::create(context, client))
    , m_bridge(Bridge::create(m_workerClientWrapper.copyRef(), m_workerGlobalScope.copyRef(), taskMode, provider))
    , m_socketProvider(provider)
{
    m_bridge->initialize();
}

WorkerThreadableWebSocketChannel::~WorkerThreadableWebSocketChannel()
{
    if (m_bridge)
        m_bridge->disconnect();
}

WorkerThreadableWebSocketChannel::ConnectStatus WorkerThreadableWebSocketChannel::connect(const URL& url, const String& protocol)
{
    if (m_bridge)
        m_bridge->connect(url, protocol);
    // connect is called asynchronously, so we do not have any possibility for synchronous errors.
    return ConnectStatus::OK;
}

String WorkerThreadableWebSocketChannel::subprotocol()
{
    return m_workerClientWrapper->subprotocol();
}

String WorkerThreadableWebSocketChannel::extensions()
{
    return m_workerClientWrapper->extensions();
}

ThreadableWebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(const String& message)
{
    if (!m_bridge)
        return ThreadableWebSocketChannel::SendFail;
    return m_bridge->send(message);
}

ThreadableWebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!m_bridge)
        return ThreadableWebSocketChannel::SendFail;
    return m_bridge->send(binaryData, byteOffset, byteLength);
}

ThreadableWebSocketChannel::SendResult WorkerThreadableWebSocketChannel::send(Blob& binaryData)
{
    if (!m_bridge)
        return ThreadableWebSocketChannel::SendFail;
    return m_bridge->send(binaryData);
}

unsigned WorkerThreadableWebSocketChannel::bufferedAmount() const
{
    if (!m_bridge)
        return 0;
    return m_bridge->bufferedAmount();
}

void WorkerThreadableWebSocketChannel::close(int code, const String& reason)
{
    if (m_bridge)
        m_bridge->close(code, reason);
}

void WorkerThreadableWebSocketChannel::fail(const String& reason)
{
    if (m_bridge)
        m_bridge->fail(reason);
}

void WorkerThreadableWebSocketChannel::disconnect()
{
    m_bridge->disconnect();
    m_bridge = nullptr;
}

void WorkerThreadableWebSocketChannel::suspend()
{
    m_workerClientWrapper->suspend();
    if (m_bridge)
        m_bridge->suspend();
}

void WorkerThreadableWebSocketChannel::resume()
{
    m_workerClientWrapper->resume();
    if (m_bridge)
        m_bridge->resume();
}

WorkerThreadableWebSocketChannel::Peer::Peer(Ref<ThreadableWebSocketChannelClientWrapper>&& clientWrapper, WorkerLoaderProxy& loaderProxy, ScriptExecutionContext& context, const String& taskMode, SocketProvider& provider)
    : m_workerClientWrapper(WTFMove(clientWrapper))
    , m_loaderProxy(loaderProxy)
    , m_mainWebSocketChannel(ThreadableWebSocketChannel::create(downcast<Document>(context), *this, provider))
    , m_taskMode(taskMode)
{
    ASSERT(isMainThread());
}

WorkerThreadableWebSocketChannel::Peer::~Peer()
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->disconnect();
}

WorkerThreadableWebSocketChannel::ConnectStatus WorkerThreadableWebSocketChannel::Peer::connect(const URL& url, const String& protocol)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return WorkerThreadableWebSocketChannel::ConnectStatus::KO;
    return m_mainWebSocketChannel->connect(url, protocol);
}

void WorkerThreadableWebSocketChannel::Peer::send(const String& message)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;

    ThreadableWebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(message);
    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, sendRequestResult](ScriptExecutionContext&) mutable {
        workerClientWrapper->setSendRequestResult(sendRequestResult);
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::send(const ArrayBuffer& binaryData)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;

    ThreadableWebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(binaryData, 0, binaryData.byteLength());
    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, sendRequestResult](ScriptExecutionContext&) mutable {
        workerClientWrapper->setSendRequestResult(sendRequestResult);
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::send(Blob& binaryData)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;

    ThreadableWebSocketChannel::SendResult sendRequestResult = m_mainWebSocketChannel->send(binaryData);
    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, sendRequestResult](ScriptExecutionContext&) mutable {
        workerClientWrapper->setSendRequestResult(sendRequestResult);
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::bufferedAmount()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;

    unsigned bufferedAmount = m_mainWebSocketChannel->bufferedAmount();
    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, bufferedAmount](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->setBufferedAmount(bufferedAmount);
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::close(int code, const String& reason)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->close(code, reason);
}

void WorkerThreadableWebSocketChannel::Peer::fail(const String& reason)
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->fail(reason);
}

void WorkerThreadableWebSocketChannel::Peer::disconnect()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->disconnect();
    m_mainWebSocketChannel = nullptr;
}

void WorkerThreadableWebSocketChannel::Peer::suspend()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->suspend();
}

void WorkerThreadableWebSocketChannel::Peer::resume()
{
    ASSERT(isMainThread());
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->resume();
}

void WorkerThreadableWebSocketChannel::Peer::didConnect()
{
    ASSERT(isMainThread());

    String subprotocol = m_mainWebSocketChannel->subprotocol();
    String extensions = m_mainWebSocketChannel->extensions();
    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, subprotocol = subprotocol.isolatedCopy(), extensions = extensions.isolatedCopy()](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->setSubprotocol(subprotocol);
        workerClientWrapper->setExtensions(extensions);
        workerClientWrapper->didConnect();
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessage(const String& message)
{
    ASSERT(isMainThread());

    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, message = message.isolatedCopy()](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->didReceiveMessage(message);
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveBinaryData(Vector<uint8_t>&& binaryData)
{
    ASSERT(isMainThread());

    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, binaryData = WTFMove(binaryData)](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->didReceiveBinaryData(WTFMove(binaryData));
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didUpdateBufferedAmount(unsigned bufferedAmount)
{
    ASSERT(isMainThread());

    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, bufferedAmount](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->didUpdateBufferedAmount(bufferedAmount);
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didStartClosingHandshake()
{
    ASSERT(isMainThread());

    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->didStartClosingHandshake();
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didClose(unsigned unhandledBufferedAmount, ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT(isMainThread());
    m_mainWebSocketChannel = nullptr;

    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper, unhandledBufferedAmount, closingHandshakeCompletion, code, reason = reason.isolatedCopy()](ScriptExecutionContext& context) mutable {
            ASSERT_UNUSED(context, context.isWorkerGlobalScope());
            workerClientWrapper->didClose(unhandledBufferedAmount, closingHandshakeCompletion, code, reason);
        }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didReceiveMessageError()
{
    ASSERT(isMainThread());

    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->didReceiveMessageError();
    }, m_taskMode);
}

void WorkerThreadableWebSocketChannel::Peer::didUpgradeURL()
{
    ASSERT(isMainThread());
    
    m_loaderProxy.postTaskForModeToWorkerGlobalScope([workerClientWrapper = m_workerClientWrapper](ScriptExecutionContext& context) mutable {
        ASSERT_UNUSED(context, context.isWorkerGlobalScope());
        workerClientWrapper->didUpgradeURL();
    }, m_taskMode);
}

WorkerThreadableWebSocketChannel::Bridge::Bridge(Ref<ThreadableWebSocketChannelClientWrapper>&& workerClientWrapper, Ref<WorkerGlobalScope>&& workerGlobalScope, const String& taskMode, Ref<SocketProvider>&& socketProvider)
    : m_workerClientWrapper(WTFMove(workerClientWrapper))
    , m_workerGlobalScope(WTFMove(workerGlobalScope))
    , m_loaderProxy(m_workerGlobalScope->thread().workerLoaderProxy())
    , m_taskMode(taskMode)
    , m_socketProvider(WTFMove(socketProvider))
{
}

WorkerThreadableWebSocketChannel::Bridge::~Bridge()
{
    disconnect();
}

void WorkerThreadableWebSocketChannel::Bridge::mainThreadInitialize(ScriptExecutionContext& context, WorkerLoaderProxy& loaderProxy, Ref<ThreadableWebSocketChannelClientWrapper>&& clientWrapper, const String& taskMode, Ref<SocketProvider>&& provider)
{
    ASSERT(isMainThread());
    ASSERT(context.isDocument());

    bool sent = loaderProxy.postTaskForModeToWorkerGlobalScope({
        ScriptExecutionContext::Task::CleanupTask,
        [clientWrapper, &loaderProxy, peer = makeUnique<Peer>(clientWrapper.copyRef(), loaderProxy, context, taskMode, WTFMove(provider))](ScriptExecutionContext& context) mutable {
            ASSERT_UNUSED(context, context.isWorkerGlobalScope());
            if (clientWrapper->failedWebSocketChannelCreation()) {
                // If Bridge::initialize() quitted earlier, we need to kick mainThreadDestroy() to delete the peer.
                loaderProxy.postTaskToLoader([peer = WTFMove(peer)](ScriptExecutionContext& context) {
                    ASSERT(isMainThread());
                    ASSERT_UNUSED(context, context.isDocument());
                });
            } else
                clientWrapper->didCreateWebSocketChannel(peer.release());
        }
    }, taskMode);

    if (!sent)
        clientWrapper->clearPeer();
}

void WorkerThreadableWebSocketChannel::Bridge::initialize()
{
    ASSERT(!m_peer);
    setMethodNotCompleted();
    Ref<Bridge> protectedThis(*this);

    m_loaderProxy.postTaskToLoader([&loaderProxy = m_loaderProxy, workerClientWrapper = m_workerClientWrapper, taskMode = m_taskMode.isolatedCopy(), provider = m_socketProvider](ScriptExecutionContext& context) mutable {
        mainThreadInitialize(context, loaderProxy, WTFMove(workerClientWrapper), taskMode, WTFMove(provider));
    });
    waitForMethodCompletion();

    // m_peer may be null when the nested runloop exited before a peer has created.
    m_peer = m_workerClientWrapper->peer();
    if (!m_peer)
        m_workerClientWrapper->setFailedWebSocketChannelCreation();
}

void WorkerThreadableWebSocketChannel::Bridge::connect(const URL& url, const String& protocol)
{
    if (!m_peer)
        return;

    m_loaderProxy.postTaskToLoader([peer = m_peer, url = url.isolatedCopy(), protocol = protocol.isolatedCopy()](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT(context.isDocument());
        ASSERT(peer);

        auto& document = downcast<Document>(context);
        
        // FIXME: make this mixed content check equivalent to the document mixed content check currently in WebSocket::connect()
        if (document.frame()) {
            Optional<String> errorString = document.frame()->loader().mixedContentChecker().checkForMixedContentInFrameTree(url);
            if (errorString) {
                peer->fail(errorString.value());
                return;
            }
        }

        if (peer->connect(url, protocol) == ThreadableWebSocketChannel::ConnectStatus::KO)
            peer->didReceiveMessageError();
    });
}

ThreadableWebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const String& message)
{
    if (!m_peer)
        return ThreadableWebSocketChannel::SendFail;
    setMethodNotCompleted();

    m_loaderProxy.postTaskToLoader([peer = m_peer, message = message.isolatedCopy()](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->send(message);
    });

    Ref<Bridge> protectedThis(*this);
    waitForMethodCompletion();
    return m_workerClientWrapper->sendRequestResult();
}

ThreadableWebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(const ArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    if (!m_peer)
        return ThreadableWebSocketChannel::SendFail;

    // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied into Vector<char>.
    Vector<char> data(byteLength);
    if (binaryData.byteLength())
        memcpy(data.data(), static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);
    setMethodNotCompleted();

    m_loaderProxy.postTaskToLoader([peer = m_peer, data = WTFMove(data)](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        auto arrayBuffer = ArrayBuffer::create(data.data(), data.size());
        peer->send(arrayBuffer);
    });

    Ref<Bridge> protectedThis(*this);
    waitForMethodCompletion();
    return m_workerClientWrapper->sendRequestResult();
}

ThreadableWebSocketChannel::SendResult WorkerThreadableWebSocketChannel::Bridge::send(Blob& binaryData)
{
    if (!m_peer)
        return ThreadableWebSocketChannel::SendFail;
    setMethodNotCompleted();

    m_loaderProxy.postTaskToLoader([peer = m_peer, url = binaryData.url().isolatedCopy(), type = binaryData.type().isolatedCopy(), size = binaryData.size()](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->send(Blob::deserialize(&context, url, type, size, { }));
    });

    Ref<Bridge> protectedThis(*this);
    waitForMethodCompletion();
    return m_workerClientWrapper->sendRequestResult();
}

unsigned WorkerThreadableWebSocketChannel::Bridge::bufferedAmount()
{
    if (!m_peer)
        return 0;
    setMethodNotCompleted();

    m_loaderProxy.postTaskToLoader([peer = m_peer](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->bufferedAmount();
    });

    Ref<Bridge> protectedThis(*this);
    waitForMethodCompletion();
    return m_workerClientWrapper->bufferedAmount();
}

void WorkerThreadableWebSocketChannel::Bridge::close(int code, const String& reason)
{
    if (!m_peer)
        return;

    m_loaderProxy.postTaskToLoader([peer = m_peer, code, reason = reason.isolatedCopy()](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->close(code, reason);
    });
}

void WorkerThreadableWebSocketChannel::Bridge::fail(const String& reason)
{
    if (!m_peer)
        return;

    m_loaderProxy.postTaskToLoader([peer = m_peer, reason = reason.isolatedCopy()](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->fail(reason);
    });
}

void WorkerThreadableWebSocketChannel::Bridge::disconnect()
{
    clearClientWrapper();
    if (m_peer) {
        m_loaderProxy.postTaskToLoader([peer = std::unique_ptr<Peer>(m_peer)](ScriptExecutionContext& context) {
            ASSERT(isMainThread());
            ASSERT_UNUSED(context, context.isDocument());
        });
        m_peer = nullptr;
    }
    m_workerGlobalScope = nullptr;
}

void WorkerThreadableWebSocketChannel::Bridge::suspend()
{
    if (!m_peer)
        return;

    m_loaderProxy.postTaskToLoader([peer = m_peer](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->suspend();
    });
}

void WorkerThreadableWebSocketChannel::Bridge::resume()
{
    if (!m_peer)
        return;

    m_loaderProxy.postTaskToLoader([peer = m_peer](ScriptExecutionContext& context) {
        ASSERT(isMainThread());
        ASSERT_UNUSED(context, context.isDocument());
        ASSERT(peer);

        peer->resume();
    });
}

void WorkerThreadableWebSocketChannel::Bridge::clearClientWrapper()
{
    m_workerClientWrapper->clearClient();
}

void WorkerThreadableWebSocketChannel::Bridge::setMethodNotCompleted()
{
    m_workerClientWrapper->clearSyncMethodDone();
}

// Caller of this function should hold a reference to the bridge, because this function may call WebSocket::didClose() in the end,
// which causes the bridge to get disconnected from the WebSocket and deleted if there is no other reference.
void WorkerThreadableWebSocketChannel::Bridge::waitForMethodCompletion()
{
    if (!m_workerGlobalScope)
        return;
    WorkerRunLoop& runLoop = m_workerGlobalScope->thread().runLoop();
    MessageQueueWaitResult result = MessageQueueMessageReceived;
    ThreadableWebSocketChannelClientWrapper* clientWrapper = m_workerClientWrapper.ptr();
    while (m_workerGlobalScope && clientWrapper && !clientWrapper->syncMethodDone() && result != MessageQueueTerminated) {
        result = runLoop.runInMode(m_workerGlobalScope.get(), m_taskMode); // May cause this bridge to get disconnected, which makes m_workerGlobalScope become null.
        clientWrapper = m_workerClientWrapper.ptr();
    }
}

} // namespace WebCore
