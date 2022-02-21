/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include "RemoteVideoFrameObjectHeapProxy.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameObjectHeapProxyProcessorMessages.h"
#include "RemoteVideoFrameProxy.h"

namespace WebKit {

Ref<RemoteVideoFrameObjectHeapProxyProcessor> RemoteVideoFrameObjectHeapProxyProcessor::create(GPUProcessConnection& connection)
{
    return adoptRef(*new RemoteVideoFrameObjectHeapProxyProcessor(connection));
}

RemoteVideoFrameObjectHeapProxyProcessor::RemoteVideoFrameObjectHeapProxyProcessor(GPUProcessConnection& connection)
    : m_connectionID(connection.connection().uniqueID())
    , m_queue(WorkQueue::create("RemoteVideoFrameObjectHeapProxy", WorkQueue::QOS::UserInteractive))
{
    connection.addClient(*this);
    connection.connection().addWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeapProxyProcessor::messageReceiverName(), m_queue, this);
}

RemoteVideoFrameObjectHeapProxyProcessor::~RemoteVideoFrameObjectHeapProxyProcessor()
{
    ASSERT(isMainRunLoop());
    clearCallbacks();
}

void RemoteVideoFrameObjectHeapProxyProcessor::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    {
        Locker lock(m_connectionLock);
        m_connectionID = { };
    }
    connection.connection().removeWorkQueueMessageReceiver(Messages::RemoteVideoFrameObjectHeapProxyProcessor::messageReceiverName());
    clearCallbacks();
}

void RemoteVideoFrameObjectHeapProxyProcessor::clearCallbacks()
{
    HashMap<RemoteVideoFrameIdentifier, Callback> callbacks;
    {
        Locker lock(m_callbacksLock);
        callbacks = std::exchange(m_callbacks, { });
    }

    m_queue->dispatch([queue = m_queue, callbacks = WTFMove(callbacks)]() mutable {
        for (auto& callback : callbacks.values())
            callback({ });
    });
}

void RemoteVideoFrameObjectHeapProxyProcessor::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTFMove(semaphore));
}

void RemoteVideoFrameObjectHeapProxyProcessor::setSharedVideoFrameMemory(const SharedMemory::IPCHandle& ipcHandle)
{
    m_sharedVideoFrameReader.setSharedMemory(ipcHandle);
}

RemoteVideoFrameObjectHeapProxyProcessor::Callback RemoteVideoFrameObjectHeapProxyProcessor::takeCallback(RemoteVideoFrameIdentifier identifier)
{
    Locker lock(m_callbacksLock);
    return m_callbacks.take(identifier);
}

void RemoteVideoFrameObjectHeapProxyProcessor::videoFrameBufferNotFound(RemoteVideoFrameIdentifier identifier)
{
    if (auto callback = takeCallback(identifier))
        callback(nullptr);
}

void RemoteVideoFrameObjectHeapProxyProcessor::newVideoFrameBuffer(RemoteVideoFrameIdentifier identifier)
{
    auto result = m_sharedVideoFrameReader.read();
    if (auto callback = takeCallback(identifier))
        callback(WTFMove(result));
}

void RemoteVideoFrameObjectHeapProxyProcessor::getVideoFrameBuffer(const RemoteVideoFrameProxy& frame, Callback&& callback)
{
    {
        Locker lock(m_callbacksLock);
        ASSERT(!m_callbacks.contains(frame.identifier()));
        m_callbacks.add(frame.identifier(), WTFMove(callback));
    }
    Locker lock(m_connectionLock);
    if (!m_connectionID) {
        videoFrameBufferNotFound(frame.identifier());
        return;
    }
    IPC::Connection::send(m_connectionID, Messages::RemoteVideoFrameObjectHeap::GetVideoFrameBuffer(frame.read()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

}

#endif
