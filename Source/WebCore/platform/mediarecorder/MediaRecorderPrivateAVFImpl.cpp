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


#include "config.h"
#include "MediaRecorderPrivateAVFImpl.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioStreamDescription.h"
#include "MediaSample.h"
#include "MediaStreamPrivate.h"
#include "SharedBuffer.h"
#include "WebAudioBufferList.h"

namespace WebCore {

std::unique_ptr<MediaRecorderPrivateAVFImpl> MediaRecorderPrivateAVFImpl::create(const MediaStreamPrivate& stream)
{
    auto instance = std::unique_ptr<MediaRecorderPrivateAVFImpl>(new MediaRecorderPrivateAVFImpl(stream));
    if (!instance->m_isWriterReady)
        return nullptr;
    return instance;
}

MediaRecorderPrivateAVFImpl::MediaRecorderPrivateAVFImpl(const MediaStreamPrivate& stream)
{
    if (!m_writer.setupWriter())
        return;
    auto tracks = stream.tracks();
    bool videoSelected = false;
    bool audioSelected = false;
    for (auto& track : tracks) {
        if (!track->enabled() || track->ended())
            continue;
        switch (track->type()) {
        case RealtimeMediaSource::Type::Video: {
            auto& settings = track->settings();
            if (videoSelected || !settings.supportsWidth() || !settings.supportsHeight())
                break;
            // FIXME: we will need to implement support for multiple video tracks, currently we only choose the first track as the recorded track.
            // FIXME: we would better to throw an exception to JavaScript if setVideoInput failed
            if (!m_writer.setVideoInput(settings.width(), settings.height()))
                return;
            m_recordedVideoTrackID = track->id();
            videoSelected = true;
            break;
        }
        case RealtimeMediaSource::Type::Audio: {
            if (audioSelected)
                break;
            // FIXME: we will need to implement support for multiple audio tracks, currently we only choose the first track as the recorded track.
            // FIXME: we would better to throw an exception to JavaScript if setAudioInput failed
            if (!m_writer.setAudioInput())
                return;
            m_recordedAudioTrackID = track->id();
            audioSelected = true;
            break;
        }
        case RealtimeMediaSource::Type::None:
            break;
        }
    }
    m_isWriterReady = true;
}

void MediaRecorderPrivateAVFImpl::sampleBufferUpdated(MediaStreamTrackPrivate& track, MediaSample& sampleBuffer)
{
    if (track.id() != m_recordedVideoTrackID)
        return;
    m_writer.appendVideoSampleBuffer(sampleBuffer.platformSample().sample.cmSampleBuffer);
}

void MediaRecorderPrivateAVFImpl::audioSamplesAvailable(MediaStreamTrackPrivate& track, const WTF::MediaTime& mediaTime, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount)
{
    if (track.id() != m_recordedAudioTrackID)
        return;
    ASSERT(is<WebAudioBufferList>(data));
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    m_writer.appendAudioSampleBuffer(data, description, mediaTime, sampleCount);
}

void MediaRecorderPrivateAVFImpl::stopRecording()
{
    m_writer.stopRecording();
}

RefPtr<SharedBuffer> MediaRecorderPrivateAVFImpl::fetchData()
{
    return m_writer.fetchData();
}

const String& MediaRecorderPrivateAVFImpl::mimeType()
{
    static NeverDestroyed<const String> mp4MimeType(MAKE_STATIC_STRING_IMPL("video/mp4"));
    // FIXME: we will need to support more MIME types.
    return mp4MimeType;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
