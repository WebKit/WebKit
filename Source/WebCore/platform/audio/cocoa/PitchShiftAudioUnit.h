/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/SystemFree.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

#if TARGET_OS_IPHONE || (defined(AUDIOCOMPONENT_NOCARBONINSTANCES) && AUDIOCOMPONENT_NOCARBONINSTANCES)
typedef struct OpaqueAudioComponentInstance* AudioComponentInstance;
#else
typedef struct ComponentInstanceRecord* AudioComponentInstance;
#endif

typedef AudioComponentInstance AudioUnit;
typedef UInt32 AudioUnitRenderActionFlags;
typedef struct AudioTimeStamp AudioTimeStamp;
typedef struct AudioBufferList AudioBufferList;

namespace WebCore {

class AudioBus;
class CAAudioStreamDescription;
class WebAudioBufferList;

class PitchShiftAudioUnit {
    WTF_MAKE_NONCOPYABLE(PitchShiftAudioUnit);
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(PitchShiftAudioUnit, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT explicit PitchShiftAudioUnit(const CAAudioStreamDescription&);
    WEBCORE_EXPORT ~PitchShiftAudioUnit();

    WEBCORE_EXPORT void setRate(double);
    double rate() const { return m_rate; }
    WEBCORE_EXPORT void setPitch(double);
    double pitch() const { return m_pitch; }

    using InputCallback = Function<bool(AudioBus&, size_t numberOfFrames)>;
    WEBCORE_EXPORT void setInputCallback(InputCallback&&);

    WEBCORE_EXPORT bool render(AudioBus& destinationBus, size_t numberOfFrames);

private:
    static OSStatus renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData);

    AudioUnit m_audioUnit { nullptr };
    const std::unique_ptr<AudioBufferList, WTF::SystemFree<AudioBufferList>> m_renderBuffer;
    const Ref<AudioBus> m_renderBus;
    InputCallback m_inputCallback;
    double m_rate { 1.0 };
    double m_pitch { 0. };
    double m_framesSoFar { 0. };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
