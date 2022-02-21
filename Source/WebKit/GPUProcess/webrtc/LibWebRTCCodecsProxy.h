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

#if USE(LIBWEBRTC) && PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "DataReference.h"
#include "RTCDecoderIdentifier.h"
#include "RTCEncoderIdentifier.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SharedMemory.h"
#include "SharedVideoFrame.h"
#include <wtf/Lock.h>

namespace IPC {
class Connection;
class Decoder;
class Semaphore;
}

namespace WebCore {
class RemoteVideoSample;
}

namespace webrtc {
using LocalDecoder = void*;
using LocalEncoder = void*;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteVideoFrameObjectHeap;
class SharedVideoFrameReader;

class LibWebRTCCodecsProxy : public IPC::Connection::ThreadMessageReceiverRefCounted {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<LibWebRTCCodecsProxy> create(GPUConnectionToWebProcess& process) { return adoptRef(*new LibWebRTCCodecsProxy(process)); }
    ~LibWebRTCCodecsProxy();

    void close();

    bool allowsExitUnderMemoryPressure() const;

private:
    explicit LibWebRTCCodecsProxy(GPUConnectionToWebProcess&);

    // IPC::Connection::ThreadMessageReceiver
    void dispatchToThread(Function<void()>&&) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void createH264Decoder(RTCDecoderIdentifier, bool useRemoteFrames);
    void createH265Decoder(RTCDecoderIdentifier, bool useRemoteFrames);
    void createVP9Decoder(RTCDecoderIdentifier, bool useRemoteFrames);
    void releaseDecoder(RTCDecoderIdentifier);
    void decodeFrame(RTCDecoderIdentifier, uint32_t timeStamp, const IPC::DataReference&);
    void setFrameSize(RTCDecoderIdentifier, uint16_t width, uint16_t height);

    void createEncoder(RTCEncoderIdentifier, const String&, const Vector<std::pair<String, String>>&, bool useLowLatency);
    void releaseEncoder(RTCEncoderIdentifier);
    void initializeEncoder(RTCEncoderIdentifier, uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate);
    void encodeFrame(RTCEncoderIdentifier, WebCore::RemoteVideoSample&&, uint32_t timeStamp, bool shouldEncodeAsKeyFrame, std::optional<RemoteVideoFrameReadReference>);
    void setEncodeRates(RTCEncoderIdentifier, uint32_t bitRate, uint32_t frameRate);
    void setSharedVideoFrameSemaphore(RTCEncoderIdentifier, IPC::Semaphore&&);
    void setSharedVideoFrameMemory(RTCEncoderIdentifier, const SharedMemory::IPCHandle&);
    void setRTCLoggingLevel(WTFLogLevel);

    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;

    struct Encoder {
        webrtc::LocalEncoder webrtcEncoder { nullptr };
        std::unique_ptr<SharedVideoFrameReader> frameReader;
    };
    Encoder* findEncoderWithoutLock(RTCEncoderIdentifier);

    mutable Lock m_lock;
    HashMap<RTCDecoderIdentifier, webrtc::LocalDecoder> m_decoders WTF_GUARDED_BY_LOCK(m_lock); // Only modified on the libWebRTCCodecsQueue but may get accessed from the main thread.
    HashMap<RTCEncoderIdentifier, Encoder> m_encoders WTF_GUARDED_BY_LOCK(m_lock); // Only modified on the libWebRTCCodecsQueue but may get accessed from the main thread.
    Ref<WorkQueue> m_queue;
    Ref<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
};

}

#endif
