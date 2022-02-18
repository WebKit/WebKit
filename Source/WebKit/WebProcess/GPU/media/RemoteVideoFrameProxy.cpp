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
#include "RemoteVideoFrameProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
#include "GPUConnectionToWebProcess.h"
#include "RemoteVideoFrameObjectHeapMessages.h"
#include "RemoteVideoFrameObjectHeapProxy.h"

#if PLATFORM(COCOA)
#include <WebCore/CVUtilities.h>
#include <WebCore/RealtimeIncomingVideoSourceCocoa.h>
#include <WebCore/VideoFrameCV.h>
#include <wtf/threads/BinarySemaphore.h>
#endif

namespace WebKit {

RemoteVideoFrameProxy::Properties RemoteVideoFrameProxy::properties(WebKit::RemoteVideoFrameReference&& reference, const WebCore::MediaSample& mediaSample)
{
    return {
        WTFMove(reference),
        mediaSample.presentationTime(),
        mediaSample.videoMirrored(),
        mediaSample.videoRotation(),
        expandedIntSize(mediaSample.presentationSize()),
        mediaSample.videoPixelFormat()
    };
}

Ref<RemoteVideoFrameProxy> RemoteVideoFrameProxy::create(IPC::Connection& connection, RemoteVideoFrameObjectHeapProxy& videoFrameObjectHeapProxy, Properties&& properties)
{
    return adoptRef(*new RemoteVideoFrameProxy(connection, videoFrameObjectHeapProxy, WTFMove(properties)));
}

static void releaseRemoteVideoFrameProxy(IPC::Connection& connection, const RemoteVideoFrameWriteReference& reference)
{
    connection.send(Messages::RemoteVideoFrameObjectHeap::ReleaseVideoFrame(reference), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void RemoteVideoFrameProxy::releaseUnused(IPC::Connection& connection, Properties&& properties)
{
    releaseRemoteVideoFrameProxy(connection, { { properties.reference.identifier(), properties.reference.version() }, 0 });
}

RemoteVideoFrameProxy::RemoteVideoFrameProxy(IPC::Connection& connection, RemoteVideoFrameObjectHeapProxy& videoFrameObjectHeapProxy, Properties&& properties)
    : VideoFrame(properties.presentationTime, properties.isMirrored, properties.rotation)
    , m_connection(connection)
    , m_referenceTracker(properties.reference)
    , m_size(properties.size)
    , m_pixelFormat(properties.pixelFormat)
    , m_videoFrameObjectHeapProxy(&videoFrameObjectHeapProxy)
{
}

RemoteVideoFrameProxy::~RemoteVideoFrameProxy()
{
    releaseRemoteVideoFrameProxy(m_connection, write());
}

RemoteVideoFrameIdentifier RemoteVideoFrameProxy::identifier() const
{
    return m_referenceTracker.identifier();
}

RemoteVideoFrameWriteReference RemoteVideoFrameProxy::write() const
{
    return m_referenceTracker.write();
}

RemoteVideoFrameReadReference RemoteVideoFrameProxy::read() const
{
    return m_referenceTracker.read();
}

uint32_t RemoteVideoFrameProxy::videoPixelFormat() const
{
    return m_pixelFormat;
}

#if PLATFORM(COCOA)
CVPixelBufferRef RemoteVideoFrameProxy::pixelBuffer() const
{
    Locker lock(m_pixelBufferLock);
    if (!m_pixelBuffer && m_videoFrameObjectHeapProxy) {
        BinarySemaphore semaphore;
        m_videoFrameObjectHeapProxy->getVideoFrameBuffer(*this, [this, &semaphore](auto pixelBuffer) {
            m_pixelBuffer = WTFMove(pixelBuffer);
            if (!m_pixelBuffer) {
                // Some code paths do not like empty pixel buffers.
                m_pixelBuffer = WebCore::createBlackPixelBuffer(static_cast<size_t>(m_size.width()), static_cast<size_t>(m_size.height()));
            }
            semaphore.signal();
        });
        semaphore.wait();
        m_videoFrameObjectHeapProxy = nullptr;
    }
    return m_pixelBuffer.get();
}

RefPtr<WebCore::VideoFrameCV> RemoteVideoFrameProxy::asVideoFrameCV()
{
    auto buffer = pixelBuffer();
    if (!buffer)
        return nullptr;
    return VideoFrameCV::create(m_presentationTime, m_isMirrored, m_rotation, RetainPtr { buffer });
}

#endif

TextStream& operator<<(TextStream& ts, const RemoteVideoFrameProxy::Properties& properties)
{
    ts << "{ reference=" << properties.reference
        << ", presentationTime=" << properties.presentationTime
        << ", isMirrored=" << properties.isMirrored
        << ", rotation=" << static_cast<int>(properties.rotation)
        << ", size=" << properties.size
        << ", pixelFormat=" << properties.pixelFormat
        << " }";
    return ts;
}

}
#endif
