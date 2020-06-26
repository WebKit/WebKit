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

#include "MediaRecorderIdentifier.h"
#include "SharedRingBufferStorage.h"

#include <WebCore/MediaRecorderPrivate.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class MediaStreamPrivate;
}

namespace WebKit {

class MediaRecorderPrivate final
    : public WebCore::MediaRecorderPrivate
    , public SharedRingBufferStorage::Client
    , public CanMakeWeakPtr<MediaRecorderPrivate> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit MediaRecorderPrivate(WebCore::MediaStreamPrivate&);
    ~MediaRecorderPrivate();

private:
    // WebCore::MediaRecorderPrivate
    void videoSampleAvailable(WebCore::MediaSample&) final;
    void fetchData(CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&, const String& mimeType)>&&) final;
    void stopRecording() final;
    void audioSamplesAvailable(const WTF::MediaTime&, const WebCore::PlatformAudioData&, const WebCore::AudioStreamDescription&, size_t) final;

    // SharedRingBufferStorage::Client
    void storageChanged(SharedMemory*);

    MediaRecorderIdentifier m_identifier;

    Ref<IPC::Connection> m_connection;
    String m_recordedAudioTrackID;
    String m_recordedVideoTrackID;

    std::unique_ptr<WebCore::CARingBuffer> m_ringBuffer;
    WebCore::CAAudioStreamDescription m_description { };
    int64_t m_numberOfFrames { 0 };
};

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

