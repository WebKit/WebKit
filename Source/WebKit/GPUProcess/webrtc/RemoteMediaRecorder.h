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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "DataReference.h"
#include "MediaRecorderIdentifier.h"
#include "MessageReceiver.h"
#include "SharedMemory.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/MediaRecorderPrivateWriterCocoa.h>
#include <wtf/MediaTime.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class CARingBuffer;
class ImageTransferSessionVT;
class RemoteVideoSample;
class WebAudioBufferList;
struct MediaRecorderPrivateOptions;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class SharedRingBufferStorage;

class RemoteMediaRecorder : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<RemoteMediaRecorder> create(GPUConnectionToWebProcess&, MediaRecorderIdentifier, bool recordAudio, bool recordVideo, const WebCore::MediaRecorderPrivateOptions&);
    ~RemoteMediaRecorder();

    String mimeType() const { return m_writer->mimeType(); }
    unsigned audioBitRate() const { return m_writer->audioBitRate(); }
    unsigned videoBitRate() const { return m_writer->videoBitRate(); }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    RemoteMediaRecorder(GPUConnectionToWebProcess&, MediaRecorderIdentifier, Ref<WebCore::MediaRecorderPrivateWriter>&&, bool recordAudio);

    // IPC::MessageReceiver
    void audioSamplesStorageChanged(const SharedMemory::IPCHandle&, const WebCore::CAAudioStreamDescription&, uint64_t numberOfFrames);
    void audioSamplesAvailable(MediaTime, uint64_t numberOfFrames);
    void videoSampleAvailable(WebCore::RemoteVideoSample&&);
    void fetchData(CompletionHandler<void(IPC::DataReference&&, double)>&&);
    void stopRecording(CompletionHandler<void()>&&);
    void pause(CompletionHandler<void()>&&);
    void resume(CompletionHandler<void()>&&);

    GPUConnectionToWebProcess& m_gpuConnectionToWebProcess;
    MediaRecorderIdentifier m_identifier;
    Ref<WebCore::MediaRecorderPrivateWriter> m_writer;

    WebCore::CAAudioStreamDescription m_description;
    std::unique_ptr<WebCore::CARingBuffer> m_ringBuffer;
    std::unique_ptr<WebCore::WebAudioBufferList> m_audioBufferList;
    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
