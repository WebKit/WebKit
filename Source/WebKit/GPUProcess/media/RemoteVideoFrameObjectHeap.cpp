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
#include "RemoteVideoFrameObjectHeap.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
#include "GPUConnectionToWebProcess.h"
#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameObjectHeapProxyProcessorMessages.h"
#include "RemoteVideoFrameProxy.h"

#if PLATFORM(COCOA)
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/VideoFrameCV.h>
#include <pal/cf/CoreMediaSoftLink.h>
#endif

namespace WebKit {

Ref<RemoteVideoFrameObjectHeap> RemoteVideoFrameObjectHeap::create(GPUConnectionToWebProcess& connectionToWebProcess)
{
    return adoptRef(*new RemoteVideoFrameObjectHeap(connectionToWebProcess));
}

RemoteVideoFrameObjectHeap::RemoteVideoFrameObjectHeap(GPUConnectionToWebProcess& connectionToWebProcess)
    : m_gpuConnectionToWebProcess(&connectionToWebProcess)
    , m_connection(m_gpuConnectionToWebProcess->connection())
{
    m_gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName(), *this);
}

RemoteVideoFrameObjectHeap::~RemoteVideoFrameObjectHeap()
{
    ASSERT(!m_gpuConnectionToWebProcess);
    m_consumeThread.reset();
}

void RemoteVideoFrameObjectHeap::stopListeningForIPC(Ref<RemoteVideoFrameObjectHeap>&& refFromConnection)
{
    assertIsCurrent(m_consumeThread);
#if PLATFORM(COCOA)
    m_sharedVideoFrameWriter.disable();
#endif

    if (auto* gpuConnectionToWebProcess = std::exchange(m_gpuConnectionToWebProcess, nullptr)) {
        gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteVideoFrameObjectHeap::messageReceiverName());
        // Clients might hold on to the ref after this happens. They should also stop themselves, but if they do not,
        // avoid big memory leaks by clearing the frames. The clients should fail gracefully (do nothing) in case they fail to look up
        // frames.
        clear();
        // TODO: add can happen after stopping.
    }
}

RemoteVideoFrameIdentifier RemoteVideoFrameObjectHeap::createRemoteVideoFrame(Ref<WebCore::MediaSample>&& frame)
{
    auto identifier = RemoteVideoFrameIdentifier::generateThreadSafe();
    add(identifier, WTFMove(frame));
    return identifier;
}

void RemoteVideoFrameObjectHeap::releaseVideoFrame(RemoteVideoFrameWriteReference&& write)
{
    assertIsCurrent(m_consumeThread);
    retireRemove(WTFMove(write));
}

#if PLATFORM(COCOA)
void RemoteVideoFrameObjectHeap::getVideoFrameBuffer(RemoteVideoFrameReadReference&& read)
{
    auto identifier = read.identifier();
    auto videoFrame = retire(WTFMove(read), 0_s);

    if (!videoFrame)
        m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::VideoFrameBufferNotFound { identifier }, 0);
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (is<VideoFrameCV>(videoFrame))
        pixelBuffer = downcast<VideoFrameCV>(*videoFrame).pixelBuffer();
    else if (is<MediaSampleAVFObjC>(*videoFrame))
        pixelBuffer = downcast<MediaSampleAVFObjC>(*videoFrame).pixelBuffer();
    else {
        ASSERT_NOT_REACHED();
        return;
    }

    m_sharedVideoFrameWriter.write(pixelBuffer.get(),
        [&](auto& semaphore) { m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameSemaphore { semaphore }, 0); },
        [&](auto& handle) { m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::SetSharedVideoFrameMemory { handle }, 0); }
    );
    m_connection->send(Messages::RemoteVideoFrameObjectHeapProxyProcessor::NewVideoFrameBuffer { identifier }, 0);
}
#endif

}

#endif
