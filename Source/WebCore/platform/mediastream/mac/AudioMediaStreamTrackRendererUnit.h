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

#if ENABLE(MEDIA_STREAM)

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioSampleDataSource;
class AudioSampleBufferList;
class CAAudioStreamDescription;

class AudioMediaStreamTrackRendererUnit {
public:
    static AudioMediaStreamTrackRendererUnit& singleton();

    AudioMediaStreamTrackRendererUnit() = default;
    ~AudioMediaStreamTrackRendererUnit();

    static OSStatus inputProc(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 inBusNumber, UInt32 numberOfFrames, AudioBufferList*);

    void start();
    void stop();

    void setAudioOutputDevice(const String&);

    void addSource(Ref<AudioSampleDataSource>&&);
    void removeSource(AudioSampleDataSource&);

    CAAudioStreamDescription* formatDescription();

private:
    void createAudioUnitIfNeeded();
    OSStatus render(UInt32 sampleCount, AudioBufferList&, UInt32 inBusNumber, const AudioTimeStamp&, AudioUnitRenderActionFlags&);

    AudioComponentInstance m_remoteIOUnit { nullptr };
    std::unique_ptr<CAAudioStreamDescription> m_outputDescription;
    HashSet<Ref<AudioSampleDataSource>> m_sources;
    Vector<Ref<AudioSampleDataSource>> m_sourcesCopy;
    Vector<Ref<AudioSampleDataSource>> m_renderSources;
    bool m_shouldUpdateRenderSources { false };
    Lock m_sourcesLock;
    bool m_isStarted { false };
#if PLATFORM(MAC)
    uint32_t m_deviceID { 0 };
#endif
};

}

#endif // ENABLE(MEDIA_STREAM)
