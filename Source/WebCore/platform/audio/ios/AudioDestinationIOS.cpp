/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO) && PLATFORM(IOS_FAMILY)

#include "AudioDestinationCocoa.h"

#include "AudioSession.h"
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(AudioToolbox)
SOFT_LINK(AudioToolbox, AudioComponentFindNext, AudioComponent, (AudioComponent inComponent, const AudioComponentDescription *inDesc), (inComponent, inDesc))
SOFT_LINK(AudioToolbox, AudioComponentInstanceNew, OSStatus, (AudioComponent inComponent, AudioComponentInstance *outInstance), (inComponent, outInstance))
SOFT_LINK(AudioToolbox, AudioUnitAddPropertyListener, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitPropertyListenerProc inProc, void *inProcUserData), (inUnit, inID, inProc, inProcUserData))
SOFT_LINK(AudioToolbox, AudioUnitGetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void *outData, UInt32 *ioDataSize), (inUnit, inID, inScope, inElement, outData, ioDataSize))
SOFT_LINK(AudioToolbox, AudioUnitInitialize, OSStatus, (AudioUnit inUnit), (inUnit))
SOFT_LINK(AudioToolbox, AudioUnitSetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, const void *inData, UInt32 inDataSize), (inUnit, inID, inScope, inElement, inData, inDataSize))

namespace WebCore {

void AudioDestinationCocoa::configure()
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
    comp = AudioComponentFindNext(0, &desc);

    ASSERT(comp);

    OSStatus result = AudioComponentInstanceNew(comp, &outputUnit());
    ASSERT(!result);

    UInt32 flag = 1;
    result = AudioUnitSetProperty(outputUnit(),
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output,
        0,
        &flag,
        sizeof(flag));
    ASSERT(!result);

    result = AudioUnitInitialize(outputUnit());
    ASSERT(!result);
    // Set render callback
    AURenderCallbackStruct input;
    input.inputProc = inputProc;
    input.inputProcRefCon = this;
    result = AudioUnitSetProperty(outputUnit(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
    ASSERT(!result);

    // Set stream format
    AudioStreamBasicDescription streamFormat;

    UInt32 size = sizeof(AudioStreamBasicDescription);
    result = AudioUnitGetProperty(outputUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, (void*)&streamFormat, &size);
    ASSERT(!result);

    setAudioStreamBasicDescription(streamFormat);

    result = AudioUnitSetProperty(outputUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, (void*)&streamFormat, sizeof(AudioStreamBasicDescription));
    ASSERT(!result);

    AudioSession::sharedSession().setPreferredBufferSize(kPreferredBufferSize);
}

void AudioDestinationCocoa::processBusAfterRender(AudioBus&, UInt32)
{
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && PLATFORM(IOS_FAMILY)

