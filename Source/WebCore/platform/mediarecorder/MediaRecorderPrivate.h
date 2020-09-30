/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include "Exception.h"
#include "MediaRecorderPrivateOptions.h"
#include "RealtimeMediaSource.h"

#if ENABLE(MEDIA_STREAM)

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class MediaSample;
class MediaStreamPrivate;
class MediaStreamTrackPrivate;
class PlatformAudioData;
class SharedBuffer;

class MediaRecorderPrivate
    : public RealtimeMediaSource::AudioSampleObserver
    , public RealtimeMediaSource::VideoSampleObserver {
public:
    ~MediaRecorderPrivate();

    struct AudioVideoSelectedTracks {
        MediaStreamTrackPrivate* audioTrack { nullptr };
        MediaStreamTrackPrivate* videoTrack { nullptr };
    };
    WEBCORE_EXPORT static AudioVideoSelectedTracks selectTracks(MediaStreamPrivate&);

    using FetchDataCallback = CompletionHandler<void(RefPtr<SharedBuffer>&&, const String& mimeType)>;
    virtual void fetchData(FetchDataCallback&&) = 0;
    virtual void stopRecording() = 0;

    using ErrorCallback = CompletionHandler<void(Optional<Exception>&&)>;
    virtual void startRecording(ErrorCallback&& callback) { callback({ }); }

    WEBCORE_EXPORT virtual const String& mimeType() const;

protected:
    void setAudioSource(RefPtr<RealtimeMediaSource>&&);
    void setVideoSource(RefPtr<RealtimeMediaSource>&&);

private:
    RefPtr<RealtimeMediaSource> m_audioSource;
    RefPtr<RealtimeMediaSource> m_videoSource;
};

inline void MediaRecorderPrivate::setAudioSource(RefPtr<RealtimeMediaSource>&& audioSource)
{
    if (m_audioSource)
        m_audioSource->removeAudioSampleObserver(*this);

    m_audioSource = WTFMove(audioSource);

    if (m_audioSource)
        m_audioSource->addAudioSampleObserver(*this);
}

inline void MediaRecorderPrivate::setVideoSource(RefPtr<RealtimeMediaSource>&& videoSource)
{
    if (m_videoSource)
        m_videoSource->removeVideoSampleObserver(*this);

    m_videoSource = WTFMove(videoSource);

    if (m_videoSource)
        m_videoSource->addVideoSampleObserver(*this);
}

inline MediaRecorderPrivate::~MediaRecorderPrivate()
{
    // Subclasses should stop observing sonner than here. Otherwise they might be called from a background thread while half destroyed
    ASSERT(!m_audioSource);
    ASSERT(!m_videoSource);
    if (m_audioSource)
        m_audioSource->removeAudioSampleObserver(*this);
    if (m_videoSource)
        m_videoSource->removeVideoSampleObserver(*this);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
