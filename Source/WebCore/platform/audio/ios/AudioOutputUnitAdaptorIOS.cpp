/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2014, 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioOutputUnitAdaptor.h"

#if ENABLE(WEB_AUDIO) && PLATFORM(IOS_FAMILY)

#include "AudioSession.h"
#include <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

void AudioOutputUnitAdaptor::configure(float hardwareSampleRate, unsigned numberOfOutputChannels)
{
    const int kPreferredBufferSize = 256;

    // Open and initialize DefaultOutputUnit
    AudioComponent comp;
    AudioComponentDescription desc;

    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    comp = PAL::AudioComponentFindNext(0, &desc);

    ASSERT(comp);

    OSStatus result = PAL::AudioComponentInstanceNew(comp, &m_outputUnit);
    ASSERT_UNUSED(result, !result);

    UInt32 flag = 1;
    result = PAL::AudioUnitSetProperty(m_outputUnit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output,
        0,
        &flag,
        sizeof(flag));
    ASSERT_UNUSED(result, !result);

    result = PAL::AudioUnitInitialize(m_outputUnit);
    ASSERT_UNUSED(result, !result);
    // Set render callback
    AURenderCallbackStruct input;
    input.inputProc = inputProc;
    input.inputProcRefCon = this;
    result = PAL::AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
    ASSERT_UNUSED(result, !result);

    // Set stream format
    AudioStreamBasicDescription streamFormat;

    UInt32 size = sizeof(AudioStreamBasicDescription);
    result = PAL::AudioUnitGetProperty(m_outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, (void*)&streamFormat, &size);
    ASSERT_UNUSED(result, !result);

    constexpr int bytesPerFloat = sizeof(Float32);
    constexpr int bitsPerByte = 8;
    streamFormat.mSampleRate = hardwareSampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeFloatPacked) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsNonInterleaved);
    streamFormat.mBytesPerPacket = bytesPerFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = bytesPerFloat;
    streamFormat.mChannelsPerFrame = numberOfOutputChannels;
    streamFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    result = PAL::AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, (void*)&streamFormat, sizeof(AudioStreamBasicDescription));
    ASSERT_UNUSED(result, !result);

    AudioSession::singleton().setPreferredBufferSize(kPreferredBufferSize);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && PLATFORM(IOS_FAMILY)

