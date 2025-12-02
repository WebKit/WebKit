/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "WorkQueueMessageReceiver.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

using CVPixelBufferPoolRef = struct __CVPixelBufferPool*;

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class NativeImage;
class VideoFrame;
}

namespace WebKit {

class RemoteVideoFrameProxy;

class RemoteVideoFrameObjectHeapProxyProcessor : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::MainRunLoop>, public GPUProcessConnection::Client {
public:
    static Ref<RemoteVideoFrameObjectHeapProxyProcessor> create();
    ~RemoteVideoFrameObjectHeapProxyProcessor();
    void gpuProcessConnectionDidBecomeAvailable(GPUProcessConnection&);

    using Callback = Function<void(RetainPtr<CVPixelBufferRef>&&)>;
    void getVideoFrameBuffer(const RemoteVideoFrameProxy&, bool canUseIOSurfce, Callback&&);
    RefPtr<WebCore::NativeImage> getNativeImage(const WebCore::VideoFrame&);

    ThreadSafeWeakPtrControlBlock& controlBlock() const final { return IPC::WorkQueueMessageReceiver<WTF::DestructionThread::MainRunLoop>::controlBlock(); }
    size_t weakRefCount() const final { return IPC::WorkQueueMessageReceiver<WTF::DestructionThread::MainRunLoop>::weakRefCount(); }

    void ref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::MainRunLoop>::ref(); }
    void deref() const final { IPC::WorkQueueMessageReceiver<WTF::DestructionThread::MainRunLoop>::deref(); }

private:
    explicit RemoteVideoFrameObjectHeapProxyProcessor();
    RefPtr<IPC::Connection> connection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&&);
    void newVideoFrameBuffer(RemoteVideoFrameIdentifier, std::optional<SharedVideoFrame::Buffer>&&);
    void newConvertedVideoFrameBuffer(std::optional<SharedVideoFrame::Buffer>&&);

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&);

    void clearCallbacks();
    Callback takeCallback(RemoteVideoFrameIdentifier);

    Lock m_connectionLock;
    RefPtr<IPC::Connection> m_connection WTF_GUARDED_BY_LOCK(m_connectionLock);
    Lock m_callbacksLock;
    HashMap<RemoteVideoFrameIdentifier, Callback> m_callbacks WTF_GUARDED_BY_LOCK(m_callbacksLock);
    const Ref<WorkQueue> m_queue;
    SharedVideoFrameReader m_sharedVideoFrameReader;

    SharedVideoFrameWriter m_sharedVideoFrameWriter;
    RetainPtr<CVPixelBufferRef> m_convertedBuffer;
    BinarySemaphore m_conversionSemaphore;
};

} // namespace WebKit

#endif
