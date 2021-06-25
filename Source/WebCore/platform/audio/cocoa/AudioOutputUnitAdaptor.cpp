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

#include "config.h"
#include "AudioOutputUnitAdaptor.h"

#if ENABLE(WEB_AUDIO)

namespace WebCore {

AudioOutputUnitAdaptor::AudioOutputUnitAdaptor(AudioUnitRenderer& renderer)
    : m_outputUnit(0)
    , m_audioUnitRenderer(renderer)
{
}

AudioOutputUnitAdaptor::~AudioOutputUnitAdaptor()
{
    if (m_outputUnit)
        AudioComponentInstanceDispose(m_outputUnit);
}

OSStatus AudioOutputUnitAdaptor::start()
{
    auto result = AudioOutputUnitStart(m_outputUnit);
    if (result != noErr)
        WTFLogAlways("ERROR: AudioOutputUnitStart() call failed with error code: %ld", static_cast<long>(result));
    return result;
}

OSStatus AudioOutputUnitAdaptor::stop()
{
    return AudioOutputUnitStop(m_outputUnit);
}

// DefaultOutputUnit callback
OSStatus AudioOutputUnitAdaptor::inputProc(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp* timeStamp, UInt32 /*busNumber*/, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    auto* adaptor = static_cast<AudioOutputUnitAdaptor*>(userData);
    double sampleTime = 0.;
    uint64_t hostTime = 0;
    if (timeStamp) {
        sampleTime = timeStamp->mSampleTime;
        hostTime = timeStamp->mHostTime;
    }

    return adaptor->m_audioUnitRenderer.render(sampleTime, hostTime, numberOfFrames, ioData);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
