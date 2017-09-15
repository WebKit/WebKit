/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "AudioTrackPrivateMediaStream.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>

namespace WebCore {

class AudioSampleDataSource;
class AudioSampleBufferList;
class CAAudioStreamDescription;

class AudioTrackPrivateMediaStreamCocoa final : public AudioTrackPrivateMediaStream, private RealtimeMediaSource::Observer {
    WTF_MAKE_NONCOPYABLE(AudioTrackPrivateMediaStreamCocoa)
public:
    static RefPtr<AudioTrackPrivateMediaStreamCocoa> create(MediaStreamTrackPrivate& streamTrack)
    {
        return adoptRef(*new AudioTrackPrivateMediaStreamCocoa(streamTrack));
    }

    void play();
    void pause();
    bool isPlaying() { return m_isPlaying; }

    void setVolume(float);
    float volume() const { return m_volume; }

    void setMuted(bool muted) { m_muted = muted; }
    bool muted() const { return m_muted; }

private:
    AudioTrackPrivateMediaStreamCocoa(MediaStreamTrackPrivate&);
    ~AudioTrackPrivateMediaStreamCocoa();

    // RealtimeMediaSource::Observer
    void sourceStopped() final;
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    static OSStatus inputProc(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 inBusNumber, UInt32 numberOfFrames, AudioBufferList*);
    OSStatus render(UInt32 sampleCount, AudioBufferList&, UInt32 inBusNumber, const AudioTimeStamp&, AudioUnitRenderActionFlags&);

    AudioComponentInstance createAudioUnit(CAAudioStreamDescription&);
    void cleanup();
    void zeroBufferList(AudioBufferList&, size_t);
    void playInternal();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "AudioTrackPrivateMediaStreamCocoa"; }
#endif


    AudioComponentInstance m_remoteIOUnit { nullptr };
    std::unique_ptr<CAAudioStreamDescription> m_inputDescription;
    std::unique_ptr<CAAudioStreamDescription> m_outputDescription;

    RefPtr<AudioSampleDataSource> m_dataSource;

    float m_volume { 1 };
    bool m_isPlaying { false };
    bool m_autoPlay { false };
    bool m_muted { false };
};

}

#endif // ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)
