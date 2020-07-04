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

#if ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "AudioStreamDescription.h"
#include "MediaRecorderPrivateWriterCocoa.h"
#include "MediaSample.h"
#include "MediaStreamPrivate.h"
#include "SharedBuffer.h"
#include "WebAudioBufferList.h"

namespace WebCore {

std::unique_ptr<MediaRecorderPrivateAVFImpl> MediaRecorderPrivateAVFImpl::create(MediaStreamPrivate& stream)
{
    // FIXME: we will need to implement support for multiple audio/video tracks
    // Currently we only choose the first track as the recorded track.
    // FIXME: We would better to throw an exception to JavaScript if writer creation fails.

    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream);

    auto writer = MediaRecorderPrivateWriter::create(!!selectedTracks.audioTrack, !!selectedTracks.videoTrack);
    if (!writer)
        return nullptr;

    auto recorder = makeUnique<MediaRecorderPrivateAVFImpl>(writer.releaseNonNull());
    if (selectedTracks.audioTrack)
        recorder->setAudioSource(&selectedTracks.audioTrack->source());
    if (selectedTracks.videoTrack)
        recorder->setVideoSource(&selectedTracks.videoTrack->source());
    return recorder;
}

MediaRecorderPrivateAVFImpl::MediaRecorderPrivateAVFImpl(Ref<MediaRecorderPrivateWriter>&& writer)
    : m_writer(WTFMove(writer))
{
}

MediaRecorderPrivateAVFImpl::~MediaRecorderPrivateAVFImpl()
{
    setAudioSource(nullptr);
    setVideoSource(nullptr);
}

void MediaRecorderPrivateAVFImpl::videoSampleAvailable(MediaSample& sampleBuffer)
{
    m_writer->appendVideoSampleBuffer(sampleBuffer.platformSample().sample.cmSampleBuffer);
}

void MediaRecorderPrivateAVFImpl::audioSamplesAvailable(const WTF::MediaTime& mediaTime, const PlatformAudioData& data, const AudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(is<WebAudioBufferList>(data));
    ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
    m_writer->appendAudioSampleBuffer(data, description, mediaTime, sampleCount);
}

void MediaRecorderPrivateAVFImpl::stopRecording()
{
    setAudioSource(nullptr);
    setVideoSource(nullptr);
    m_writer->stopRecording();
}

void MediaRecorderPrivateAVFImpl::fetchData(FetchDataCallback&& completionHandler)
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

#endif // ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)
