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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRendererIdentifier.h"
#include "GPUProcessConnection.h"
#include "SharedRingBufferStorage.h"
#include <WebCore/AudioMediaStreamTrackRenderer.h>
#include <wtf/MediaTime.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class AudioMediaStreamTrackRenderer final : public WebCore::AudioMediaStreamTrackRenderer, public GPUProcessConnection::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<WebCore::AudioMediaStreamTrackRenderer> create();
    ~AudioMediaStreamTrackRenderer();

    AudioMediaStreamTrackRendererIdentifier identifier() const { return m_identifier; }

private:
    explicit AudioMediaStreamTrackRenderer(Ref<IPC::Connection>&&);

    void initialize();
    void storageChanged(SharedMemory*, const WebCore::CAAudioStreamDescription& format, size_t frameCount);

    // WebCore::AudioMediaStreamTrackRenderer
    void start() final;
    void stop() final;
    void clear() final;
    void setVolume(float) final;
    void pushSamples(const MediaTime&, const WebCore::PlatformAudioData&, const WebCore::AudioStreamDescription&, size_t) final;

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    Ref<IPC::Connection> m_connection;
    AudioMediaStreamTrackRendererIdentifier m_identifier;

    std::unique_ptr<WebCore::CARingBuffer> m_ringBuffer;
    WebCore::CAAudioStreamDescription m_description { };
    int64_t m_numberOfFrames { 0 };
    bool m_isPlaying { false };
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
