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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(COCOA)
#include "RemoteVideoFrameObjectHeapProxyProcessor.h"
#include "RemoteVideoFrameProxy.h"
#include <WebCore/NativeImage.h>
#endif

namespace WebKit {

class GPUProcessConnection;

// Wrapper around RemoteVideoFrameObjectHeapProxyProcessor that will always be destroyed on main thread.
class RemoteVideoFrameObjectHeapProxy : public ThreadSafeRefCounted<RemoteVideoFrameObjectHeapProxy, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<RemoteVideoFrameObjectHeapProxy> create(GPUProcessConnection& connection) { return adoptRef(*new RemoteVideoFrameObjectHeapProxy(connection)); }

#if PLATFORM(COCOA)
    void getVideoFrameBuffer(const RemoteVideoFrameProxy& proxy, RemoteVideoFrameObjectHeapProxyProcessor::Callback&& callback) { m_processor->getVideoFrameBuffer(proxy, WTF::move(callback)); }
    RefPtr<WebCore::NativeImage> getNativeImage(const WebCore::VideoFrame& frame, ShouldApplyRotation shouldApplyRotation = ShouldApplyRotation::No) { return m_processor->getNativeImage(frame, shouldApplyRotation); }

    // Any thread. The frame in the GPU process of this connection:
    // - `frame` itself, if it is a RemoteVideoFrameProxy of this connection whose rotation, mirroring and alpha
    //   still match its frame in the GPU process;
    // - a RemoteVideoFrameProxy derived from it if they do not match. It is cached in `frame`, so that repeated
    //   uses derive once;
    // - otherwise a new RemoteVideoFrameProxy for a copy of `frame` sent to the GPU process.
    // Null if the frame belongs to another GPU process or sending it fails. The consumer messages that carry a
    // read reference of the result can be sent once this returns.
    RefPtr<RemoteVideoFrameProxy> remoteFrame(const WebCore::VideoFrame& frame) { return m_processor->remoteFrame(*this, frame); }
#if USE(LIBWEBRTC)
    // Any thread. A new RemoteVideoFrameProxy for a copy of the pixels of `frame`. Its rotation, mirroring
    // and time are not set: the consumers of these frames send them separately.
    RefPtr<RemoteVideoFrameProxy> remoteFrame(const webrtc::VideoFrame& frame, WebCore::IntSize size) { return m_processor->remoteFrame(*this, frame, size); }
#endif
    // Any thread. A new RemoteVideoFrameProxy for a black frame that the GPU process creates.
    RefPtr<RemoteVideoFrameProxy> remoteBlackFrame(MediaTime presentationTime, WebCore::VideoFrameRotation rotation, WebCore::IntSize size) { return m_processor->remoteBlackFrame(*this, presentationTime, rotation, size); }
#endif

private:
    explicit RemoteVideoFrameObjectHeapProxy(GPUProcessConnection& connection)
#if PLATFORM(COCOA)
        : m_processor(RemoteVideoFrameObjectHeapProxyProcessor::create(connection))
#endif
    {
    }
#if PLATFORM(COCOA)
    const Ref<RemoteVideoFrameObjectHeapProxyProcessor> m_processor;
#endif
};

} // namespace WebKit

#endif
