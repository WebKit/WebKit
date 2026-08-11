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

#include "config.h"
#include "PitchShiftAudioUnit.h"

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

#include "AudioBus.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"
#include <AudioToolbox/AudioComponent.h>
#include <AudioToolbox/AudioUnit.h>
#include <AudioToolbox/AudioUnitParameters.h>
#include <AudioToolbox/AudioUnitProperties.h>
#include <pal/cf/CoreAudioExtras.h>
#include <wtf/StdLibExtras.h>

#include <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PitchShiftAudioUnit);

PitchShiftAudioUnit::PitchShiftAudioUnit(const CAAudioStreamDescription& format)
    : m_renderBuffer { PAL::createAudioBufferList(format.numberOfChannelStreams(), PAL::ShouldZeroMemory::Yes) }
    , m_renderBus { AudioBus::create(format.numberOfChannelStreams(), 0, false) }
{
    AudioComponentDescription acd {
        kAudioUnitType_FormatConverter,
        kAudioUnitSubType_NewTimePitch,
        kAudioUnitManufacturer_Apple,
        0,
        0
    };

    AudioComponent component = PAL::AudioComponentFindNext(nullptr, &acd);
    if (!component)
        return;

    OSStatus status = PAL::AudioComponentInstanceNew(component, &m_audioUnit);
    if (status != noErr || !m_audioUnit)
        return;

    AudioStreamBasicDescription asbd = format.streamDescription();
    PAL::AudioUnitSetProperty(m_audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, sizeof(AudioStreamBasicDescription));

    PAL::AudioUnitSetProperty(m_audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof(AudioStreamBasicDescription));

    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = renderCallback;
    callbackStruct.inputProcRefCon = this;
    PAL::AudioUnitSetProperty(m_audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callbackStruct, sizeof(callbackStruct));

    PAL::AudioUnitInitialize(m_audioUnit);
}

PitchShiftAudioUnit::~PitchShiftAudioUnit()
{
    if (m_audioUnit) {
        PAL::AudioUnitUninitialize(m_audioUnit);
        PAL::AudioComponentInstanceDispose(m_audioUnit);
    }
}

void PitchShiftAudioUnit::setRate(double rate)
{
    if (m_rate == rate)
        return;

    m_rate = rate;

    if (!m_audioUnit)
        return;

    Float32 rateFloat = rate;
    PAL::AudioUnitSetParameter(m_audioUnit, kNewTimePitchParam_Rate, kAudioUnitScope_Global, 0, rateFloat, 0);
}

void PitchShiftAudioUnit::setPitch(double pitch)
{
    if (m_pitch == pitch)
        return;

    m_pitch = pitch;

    if (!m_audioUnit)
        return;

    Float32 pitchFloat = pitch;
    PAL::AudioUnitSetParameter(m_audioUnit, kNewTimePitchParam_Pitch, kAudioUnitScope_Global, 0, pitchFloat, 0);
}

void PitchShiftAudioUnit::setInputCallback(InputCallback&& callback)
{
    m_inputCallback = WTF::move(callback);
}

bool PitchShiftAudioUnit::render(AudioBus& bus, size_t numberOfFrames)
{
    if (!m_audioUnit || !m_renderBuffer)
        return false;

    auto renderBuffers = span(*m_renderBuffer);
    for (unsigned i = 0; i < bus.numberOfChannels() && i < renderBuffers.size(); ++i) {
        AudioChannel* channel = bus.channel(i);
        renderBuffers[i].mNumberChannels = 1;
        renderBuffers[i].mDataByteSize = numberOfFrames * sizeof(float);
        renderBuffers[i].mData = channel->mutableData();
    }

    AudioTimeStamp timeStamp { };
    FillOutAudioTimeStampWithSampleTime(timeStamp, m_framesSoFar);
    m_framesSoFar += numberOfFrames;

    AudioUnitRenderActionFlags flags = 0;
    OSStatus status = PAL::AudioUnitRender(m_audioUnit, &flags, &timeStamp, 0, numberOfFrames, m_renderBuffer.get());

    for (unsigned i = 0; i < renderBuffers.size(); ++i) {
        renderBuffers[i].mDataByteSize = 0;
        renderBuffers[i].mData = nullptr;
    }

    if (status != noErr) {
        bus.zero();
        return false;
    }

    return true;
}

OSStatus PitchShiftAudioUnit::renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
    UNUSED_PARAM(ioActionFlags);
    UNUSED_PARAM(inTimeStamp);
    UNUSED_PARAM(inBusNumber);

    auto* self = static_cast<PitchShiftAudioUnit*>(inRefCon);
    if (!self)
        return kAudioUnitErr_NoConnection;

    if (!self->m_inputCallback)
        return kAudioUnitErr_NoConnection;

    auto ioDataBuffers = span(*ioData);

    for (unsigned i = 0; i < ioDataBuffers.size(); ++i) {
        auto channelData = mutableSpan<float>(ioDataBuffers[i]);
        self->m_renderBus->setChannelMemory(i, channelData.first(inNumberFrames));
    }
    self->m_renderBus->setLength(inNumberFrames);

    if (!self->m_inputCallback(self->m_renderBus, inNumberFrames))
        return kAudioUnitErr_TooManyFramesToProcess;

    return noErr;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
