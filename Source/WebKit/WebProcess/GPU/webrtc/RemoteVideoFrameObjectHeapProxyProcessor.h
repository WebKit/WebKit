/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(VIDEO)

#include "Connection.h"
#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SharedVideoFrame.h"

#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/WorkQueue.h>

using CVPixelBufferPoolRef = struct __CVPixelBufferPool*;

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class RemoteVideoFrameProxy;

class RemoteVideoFrameObjectHeapProxyProcessor : public IPC::Connection::WorkQueueMessageReceiver, public GPUProcessConnection::Client {
public:
    static Ref<RemoteVideoFrameObjectHeapProxyProcessor> create(GPUProcessConnection&);
    ~RemoteVideoFrameObjectHeapProxyProcessor();

    using Callback = Function<void(RetainPtr<CVPixelBufferRef>&&)>;
    void getVideoFrameBuffer(const RemoteVideoFrameProxy&, Callback&&);

private:
    explicit RemoteVideoFrameObjectHeapProxyProcessor(GPUProcessConnection&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(const SharedMemory::IPCHandle&);
    void videoFrameBufferNotFound(RemoteVideoFrameIdentifier);
    void newVideoFrameBuffer(RemoteVideoFrameIdentifier);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&);

    void clearCallbacks();
    Callback takeCallback(RemoteVideoFrameIdentifier);

private:
    Lock m_connectionLock;
    IPC::Connection::UniqueID m_connectionID WTF_GUARDED_BY_LOCK(m_connectionLock);
    Lock m_callbacksLock;
    HashMap<RemoteVideoFrameIdentifier, Callback> m_callbacks WTF_GUARDED_BY_LOCK(m_callbacksLock);
    Ref<WorkQueue> m_queue;
    SharedVideoFrameReader m_sharedVideoFrameReader;
};

} // namespace WebKit

#endif
