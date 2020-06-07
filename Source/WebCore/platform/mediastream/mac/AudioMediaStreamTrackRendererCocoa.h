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

#if ENABLE(MEDIA_STREAM)

#include "AudioMediaStreamTrackRenderer.h"
#include "Logging.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudioTypes.h>

namespace WebCore {

class AudioSampleDataSource;
class AudioSampleBufferList;
class CAAudioStreamDescription;

class AudioMediaStreamTrackRendererCocoa : public AudioMediaStreamTrackRenderer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AudioMediaStreamTrackRendererCocoa();
    ~AudioMediaStreamTrackRendererCocoa();

private:
    // AudioMediaStreamTrackRenderer
    void pushSamples(const WTF::MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;
    void start() final;
    void stop() final;
    void clear() final;

    static OSStatus inputProc(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 inBusNumber, UInt32 numberOfFrames, AudioBufferList*);
    OSStatus render(UInt32 sampleCount, AudioBufferList&, UInt32 inBusNumber, const AudioTimeStamp&, AudioUnitRenderActionFlags&);

    AudioComponentInstance createAudioUnit(CAAudioStreamDescription&);

    AudioComponentInstance m_remoteIOUnit { nullptr };
    std::unique_ptr<CAAudioStreamDescription> m_outputDescription;

    // Audio threads member
    RefPtr<AudioSampleDataSource> m_dataSource;
    RefPtr<AudioSampleDataSource> m_rendererDataSource;
    bool m_shouldUpdateRendererDataSource { false };
};

}

#endif // ENABLE(MEDIA_STREAM)
