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
#include "MediaRecorderPrivateMock.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamTrackPrivate.h"
#include "SharedBuffer.h"
#include "Timer.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {

MediaRecorderPrivateMock::MediaRecorderPrivateMock(MediaStreamPrivate& stream)
{
    auto selectedTracks = MediaRecorderPrivate::selectTracks(stream);
    if (selectedTracks.audioTrack) {
        m_audioTrackID = selectedTracks.audioTrack->id();
        setAudioSource(&selectedTracks.audioTrack->source());
    }
    if (selectedTracks.videoTrack) {
        m_videoTrackID = selectedTracks.videoTrack->id();
        setVideoSource(&selectedTracks.videoTrack->source());
    }
}

MediaRecorderPrivateMock::~MediaRecorderPrivateMock()
{
}

void MediaRecorderPrivateMock::stopRecording(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void MediaRecorderPrivateMock::pauseRecording(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void MediaRecorderPrivateMock::resumeRecording(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

void MediaRecorderPrivateMock::videoSampleAvailable(MediaSample&)
{
    Locker locker { m_bufferLock };
    m_buffer.append("Video Track ID: ");
    m_buffer.append(m_videoTrackID);
    generateMockCounterString();
}

void MediaRecorderPrivateMock::audioSamplesAvailable(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t)
{
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    Locker locker { m_bufferLock };
    m_buffer.append("Audio Track ID: ");
    m_buffer.append(m_audioTrackID);
    generateMockCounterString();
}

void MediaRecorderPrivateMock::generateMockCounterString()
{
    m_buffer.append(" Counter: ", ++m_counter, "\r\n---------\r\n");
}

void MediaRecorderPrivateMock::fetchData(FetchDataCallback&& completionHandler)
{
    RefPtr<SharedBuffer> buffer;
    {
        Locker locker { m_bufferLock };
        Vector<uint8_t> value { m_buffer.characters8(), m_buffer.length() };
        m_buffer.clear();
        buffer = SharedBuffer::create(WTFMove(value));
    }

    // Delay calling the completion handler a bit to mimick real writer behavior.
    Timer::schedule(50_ms, [completionHandler = WTFMove(completionHandler), buffer = WTFMove(buffer), mimeType = mimeType(), timeCode = MonotonicTime::now().secondsSinceEpoch().value()]() mutable {
        completionHandler(WTFMove(buffer), mimeType, timeCode);
    });
}

const String& MediaRecorderPrivateMock::mimeType() const
{
    static NeverDestroyed<const String> textPlainMimeType(MAKE_STATIC_STRING_IMPL("text/plain"));
    return textPlainMimeType;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
