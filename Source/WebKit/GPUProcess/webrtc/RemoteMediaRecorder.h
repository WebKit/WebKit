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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_RECORDER)

#include "MediaRecorderIdentifier.h"
#include "MessageReceiver.h"
#include "RemoteVideoFrameIdentifier.h"
#include "SharedCARingBuffer.h"
#include "SharedVideoFrame.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/MediaRecorderPrivateWriterCocoa.h>
#include <wtf/MediaTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakRef.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class WebAudioBufferList;
struct MediaRecorderPrivateOptions;
}

namespace WebKit {

class GPUConnectionToWebProcess;

class RemoteMediaRecorder : private IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaRecorder);
public:
    static std::unique_ptr<RemoteMediaRecorder> create(GPUConnectionToWebProcess&, MediaRecorderIdentifier, bool recordAudio, bool recordVideo, const WebCore::MediaRecorderPrivateOptions&);
    ~RemoteMediaRecorder();

    String mimeType() const { return protectedWriter()->mimeType(); }
    unsigned audioBitRate() const { return protectedWriter()->audioBitRate(); }
    unsigned videoBitRate() const { return protectedWriter()->videoBitRate(); }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

private:
    RemoteMediaRecorder(GPUConnectionToWebProcess&, MediaRecorderIdentifier, Ref<WebCore::MediaRecorderPrivateWriter>&&, bool recordAudio);

    RefPtr<IPC::Connection> connection() const;

    Ref<WebCore::MediaRecorderPrivateWriter> protectedWriter() const { return m_writer; }

    // IPC::MessageReceiver
    void audioSamplesStorageChanged(ConsumerSharedCARingBuffer::Handle&&, const WebCore::CAAudioStreamDescription&);
    void audioSamplesAvailable(MediaTime, uint64_t numberOfFrames);
    void videoFrameAvailable(SharedVideoFrame&&);
    void fetchData(CompletionHandler<void(String mimeType, std::span<const uint8_t>, double)>&&);
    void stopRecording(CompletionHandler<void()>&&);
    void pause(CompletionHandler<void()>&&);
    void resume(CompletionHandler<void()>&&);
    void setSharedVideoFrameSemaphore(IPC::Semaphore&&);
    void setSharedVideoFrameMemory(WebCore::SharedMemory::Handle&&);

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    MediaRecorderIdentifier m_identifier;
    Ref<WebCore::MediaRecorderPrivateWriter> m_writer;

    std::optional<WebCore::CAAudioStreamDescription> m_description;
    std::unique_ptr<ConsumerSharedCARingBuffer> m_ringBuffer;
    std::unique_ptr<WebCore::WebAudioBufferList> m_audioBufferList;
    const bool m_recordAudio;

    SharedVideoFrameReader m_sharedVideoFrameReader;
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_RECORDER)
