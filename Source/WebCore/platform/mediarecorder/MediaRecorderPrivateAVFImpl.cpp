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
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.
    // FIXME: We would better to throw an exception to JavaScript if writer creation fails.

    String audioTrackId;
    String videoTrackId;
    const MediaStreamTrackPrivate* audioTrack { nullptr };
    const MediaStreamTrackPrivate* videoTrack { nullptr };
    for (auto& track : stream.tracks()) {
        if (!track->enabled() || track->ended())
            continue;
        switch (track->type()) {
        case RealtimeMediaSource::Type::Video: {
            auto& settings = track->settings();
            if (!videoTrack && settings.supportsWidth() && settings.supportsHeight()) {
                videoTrack = track.get();
                videoTrackId = videoTrack->id();
            }
            break;
        }
        case RealtimeMediaSource::Type::Audio:
            if (!audioTrack) {
                audioTrack = track.get();
                audioTrackId = audioTrack->id();
            }
            break;
        case RealtimeMediaSource::Type::None:
            break;
        }
    }
    auto writer = MediaRecorderPrivateWriter::create(audioTrack, videoTrack);
    if (!writer)
        return nullptr;

    return makeUnique<MediaRecorderPrivateAVFImpl>(writer.releaseNonNull(), WTFMove(audioTrackId), WTFMove(videoTrackId));
}

MediaRecorderPrivateAVFImpl::MediaRecorderPrivateAVFImpl(Ref<MediaRecorderPrivateWriter>&& writer, String&& audioTrackId, String&& videoTrackId)
    : m_writer(WTFMove(writer))
    , m_recordedAudioTrackID(WTFMove(audioTrackId))
    , m_recordedVideoTrackID(WTFMove(videoTrackId))
{
}

void MediaRecorderPrivateAVFImpl::sampleBufferUpdated(const MediaStreamTrackPrivate& track, MediaSample& sampleBuffer)
{
    if (track.id() != m_recordedVideoTrackID)
        return;
    m_writer->appendVideoSampleBuffer(sampleBuffer.platformSample().sample.cmSampleBuffer);
}

void MediaRecorderPrivateAVFImpl::audioSamplesAvailable(const MediaStreamTrackPrivate& track, const WTF::MediaTime& mediaTime, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount)
{
    if (track.id() != m_recordedAudioTrackID)
        return;
    ASSERT(is<WebAudioBufferList>(data));
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    m_writer->appendAudioSampleBuffer(data, description, mediaTime, sampleCount);
}

void MediaRecorderPrivateAVFImpl::stopRecording()
{
    m_writer->stopRecording();
}

void MediaRecorderPrivateAVFImpl::fetchData(CompletionHandler<void(RefPtr<SharedBuffer>&&, const String&)>&& completionHandler)
{
    m_writer->fetchData([completionHandler = WTFMove(completionHandler), mimeType = mimeType()](auto&& buffer) mutable {
        completionHandler(WTFMove(buffer), mimeType);
    });
}

const String& MediaRecorderPrivateAVFImpl::mimeType()
{
    static NeverDestroyed<const String> mp4MimeType(MAKE_STATIC_STRING_IMPL("video/mp4"));
    // FIXME: we will need to support more MIME types.
    return mp4MimeType;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
