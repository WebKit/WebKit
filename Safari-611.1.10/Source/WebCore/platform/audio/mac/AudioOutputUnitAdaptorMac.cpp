/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && PLATFORM(MAC)

#include "AudioBus.h"
#include <CoreAudio/AudioHardware.h>

namespace WebCore {

void AudioOutputUnitAdaptor::configure(float hardwareSampleRate, unsigned numberOfOutputChannels)
{
    // Open and initialize DefaultOutputUnit
    AudioComponent comp;
    AudioComponentDescription desc;

    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    comp = AudioComponentFindNext(0, &desc);

    ASSERT(comp);

    OSStatus result = AudioComponentInstanceNew(comp, &m_outputUnit);
    ASSERT(!result);

    result = AudioUnitInitialize(m_outputUnit);
    ASSERT(!result);

    // Set render callback
    AURenderCallbackStruct input;
    input.inputProc = inputProc;
    input.inputProcRefCon = this;
    result = AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global, 0, &input, sizeof(input));
    ASSERT(!result);

    // Set stream format
    constexpr int bytesPerFloat = sizeof(Float32);
    constexpr int bitsPerByte = 8;

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = hardwareSampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    streamFormat.mBytesPerPacket = bytesPerFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = bytesPerFloat;
    streamFormat.mChannelsPerFrame = numberOfOutputChannels;
    streamFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    result = AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, (void*)&streamFormat, sizeof(AudioStreamBasicDescription));
    ASSERT(!result);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && PLATFORM(MAC)
