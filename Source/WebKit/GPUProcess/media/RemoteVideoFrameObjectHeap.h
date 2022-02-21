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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
#include "RemoteVideoFrameIdentifier.h"
#include "ThreadSafeObjectHeap.h"
#include <WebCore/MediaSample.h>
#include <wtf/ThreadAssertions.h>

#if PLATFORM(COCOA)
#include "SharedVideoFrame.h"
#endif

namespace WebKit {
class GPUConnectionToWebProcess;

// Holds references to all VideoFrame instances that are mapped from Web process to GPU process two processes.
// As currently there is no special state for RemoteVideoFrame, the object heap stores VideoFrame instances (MediaSample).
// As currently there is no RemoteVideoFrame, responsible for dispatching the VideoFrame methods to the objects.
// Consume thread is the thread that uses the VideoFrame. For GPUP this is the media player thread, the main thread.
// FIXME: currently VideoFrame instances are MediaSample instances.
class RemoteVideoFrameObjectHeap final : public ThreadSafeObjectHeap<RemoteVideoFrameIdentifier, RefPtr<WebCore::MediaSample>>, private IPC::MessageReceiver {
public:
    static Ref<RemoteVideoFrameObjectHeap> create(GPUConnectionToWebProcess&);
    ~RemoteVideoFrameObjectHeap();

    RemoteVideoFrameIdentifier createRemoteVideoFrame(Ref<WebCore::MediaSample>&&);

    void stopListeningForIPC(Ref<RemoteVideoFrameObjectHeap>&& refFromConnection);

private:
    RemoteVideoFrameObjectHeap(GPUConnectionToWebProcess&);

    // IPC::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages.
    void releaseVideoFrame(RemoteVideoFrameWriteReference&&);
#if PLATFORM(COCOA)
    void getVideoFrameBuffer(RemoteVideoFrameReadReference&&);
#endif

    GPUConnectionToWebProcess* m_gpuConnectionToWebProcess WTF_GUARDED_BY_LOCK(m_consumeThread);
    const Ref<IPC::Connection> m_connection;
    ThreadAssertion m_consumeThread NO_UNIQUE_ADDRESS;
#if PLATFORM(COCOA)
    SharedVideoFrameWriter m_sharedVideoFrameWriter;
#endif
};



}
#endif
