/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#if ENABLE(WEB_AUDIO)

#if PLATFORM(IOS)

#include "AudioDestinationIOS.h"

#include "AudioIOCallback.h"
#include "FloatConversion.h"
#include "Logging.h"
#include "Page.h"
#include "SoftLinking.h"
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioServices.h>
#include <WebCore/RuntimeApplicationChecksIOS.h>
#include <wtf/HashSet.h>

SOFT_LINK_FRAMEWORK(AudioToolbox)
SOFT_LINK(AudioToolbox, AudioComponentFindNext, AudioComponent, (AudioComponent inComponent, const AudioComponentDescription *inDesc), (inComponent, inDesc))
SOFT_LINK(AudioToolbox, AudioComponentInstanceDispose, OSStatus, (AudioComponentInstance inInstance), (inInstance))
SOFT_LINK(AudioToolbox, AudioComponentInstanceNew, OSStatus, (AudioComponent inComponent, AudioComponentInstance *outInstance), (inComponent, outInstance))
SOFT_LINK(AudioToolbox, AudioOutputUnitStart, OSStatus, (AudioUnit ci), (ci))
SOFT_LINK(AudioToolbox, AudioOutputUnitStop, OSStatus, (AudioUnit ci), (ci))
SOFT_LINK(AudioToolbox, AudioSessionInitialize, OSStatus, (CFRunLoopRef inRunLoop, CFStringRef inRunLoopMode, AudioSessionInterruptionListener inInterruptionListener, void *inClientData), (inRunLoop, inRunLoopMode, inInterruptionListener, inClientData))
SOFT_LINK(AudioToolbox, AudioSessionGetProperty, OSStatus, (AudioSessionPropertyID inID, UInt32 *ioDataSize, void *outData), (inID, ioDataSize, outData))
SOFT_LINK(AudioToolbox, AudioSessionSetActive, OSStatus, (Boolean active), (active))
SOFT_LINK(AudioToolbox, AudioSessionSetProperty, OSStatus, (AudioSessionPropertyID inID, UInt32 inDataSize, const void *inData), (inID, inDataSize, inData))
SOFT_LINK(AudioToolbox, AudioUnitAddPropertyListener, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitPropertyListenerProc inProc, void *inProcUserData), (inUnit, inID, inProc, inProcUserData))
SOFT_LINK(AudioToolbox, AudioUnitGetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void *outData, UInt32 *ioDataSize), (inUnit, inID, inScope, inElement, outData, ioDataSize))
SOFT_LINK(AudioToolbox, AudioUnitInitialize, OSStatus, (AudioUnit inUnit), (inUnit))
SOFT_LINK(AudioToolbox, AudioUnitSetProperty, OSStatus, (AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, const void *inData, UInt32 inDataSize), (inUnit, inID, inScope, inElement, inData, inDataSize))

namespace WebCore {

const int kRenderBufferSize = 128;
const int kPreferredBufferSize = 256;

typedef HashSet<AudioDestinationIOS*> AudioDestinationSet;
static AudioDestinationSet& audioDestinations()
{
    DEFINE_STATIC_LOCAL(AudioDestinationSet, audioDestinationSet, ());
    return audioDestinationSet;
}

// Factory method: iOS-implementation
PassOwnPtr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    // FIXME: Add support for multi-channel (> stereo) output.
    if (numberOfOutputChannels != 2)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    return adoptPtr(new AudioDestinationIOS(callback, sampleRate));
}

// iOS 7.0/Innsbruck will deprecate AudioSession APIs. For now continue to use the old API.
// <rdar://problem/11701792> AudioSession should move to using AVAudioSession (Innsbruck)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

float AudioDestination::hardwareSampleRate()
{
    AudioDestinationIOS::initializeAudioSession();

    double sampleRate = 0;
    UInt32 sampleRateSize = sizeof(sampleRate);

    AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate, &sampleRateSize, &sampleRate);
    return sampleRate;
}

unsigned long AudioDestination::maxChannelCount()
{
    // FIXME: query the default audio hardware device to return the actual number
    // of channels of the device. Also see corresponding FIXME in create().
    // There is a small amount of code which assumes stereo in AudioDestinationIOS which
    // can be upgraded.
    return 0;
}

AudioDestinationIOS::AudioDestinationIOS(AudioIOCallback& callback, double sampleRate)
    : m_outputUnit(0)
    , m_callback(callback)
    , m_renderBus(2, kRenderBufferSize, false)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
    , m_interruptedOnPlayback(false)
{
    initializeAudioSession();

    audioDestinations().add(this);
    if (audioDestinations().size() == 1)
        AudioSessionSetActive(true);

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

    OSStatus result = AudioComponentInstanceNew(comp, &m_outputUnit);
    ASSERT(!result);

    UInt32 flag = 1;
    result = AudioUnitSetProperty(m_outputUnit, 
                                  kAudioOutputUnitProperty_EnableIO, 
                                  kAudioUnitScope_Output, 
                                  0,
                                  &flag, 
                                  sizeof(flag));
    ASSERT(!result);

    result = AudioUnitAddPropertyListener(m_outputUnit, kAudioUnitProperty_MaximumFramesPerSlice, frameSizeChangedProc, this);
    ASSERT(!result);

    result = AudioUnitInitialize(m_outputUnit);
    ASSERT(!result);

    configure();
}

AudioDestinationIOS::~AudioDestinationIOS()
{
    audioDestinations().remove(this);
    if (audioDestinations().size() == 0)
        AudioSessionSetActive(false);

    if (m_outputUnit)
        AudioComponentInstanceDispose(m_outputUnit);
}

void AudioDestinationIOS::configure()
{
    // Set render callback
    AURenderCallbackStruct input;
    input.inputProc = inputProc;
    input.inputProcRefCon = this;
    OSStatus result = AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
    ASSERT(!result);

    // Set stream format
    AudioStreamBasicDescription streamFormat;

    UInt32 size = sizeof(AudioStreamBasicDescription);
    result = AudioUnitGetProperty(m_outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, (void*)&streamFormat, &size);
    ASSERT(!result);

    const int bytesPerFloat = sizeof(Float32);
    const int bitsPerByte = 8;
    streamFormat.mSampleRate = m_sampleRate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    streamFormat.mBytesPerPacket = bytesPerFloat;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = bytesPerFloat;
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBitsPerChannel = bitsPerByte * bytesPerFloat;

    result = AudioUnitSetProperty(m_outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, (void*)&streamFormat, sizeof(AudioStreamBasicDescription));
    ASSERT(!result);

    Float32 duration = narrowPrecisionToFloat(kPreferredBufferSize / m_sampleRate);
    AudioSessionSetProperty(kAudioSessionProperty_PreferredHardwareIOBufferDuration, sizeof(duration), &duration);
}

void AudioDestinationIOS::audioDestinationInterruptionListener(void *userData, UInt32 interruptionState)
{
    ASSERT_UNUSED(userData, userData);
    for (AudioDestinationSet::iterator i = audioDestinations().begin(); i != audioDestinations().end(); ++i) {
        if (interruptionState == kAudioSessionBeginInterruption)
            (*i)->beganAudioInterruption();
        else
            (*i)->endedAudioInterruption();
    }
}

void AudioDestinationIOS::initializeAudioSession()
{
    static bool audioSessionWasInitialized = false;
    if (audioSessionWasInitialized)
        return;

    audioSessionWasInitialized = true;
    AudioSessionInitialize(0, 0, &audioDestinationInterruptionListener, 0);
    Page::setAudioSessionCategory(kAudioSessionCategory_AmbientSound);
}

#pragma GCC diagnostic pop

void AudioDestinationIOS::start()
{
    OSStatus result = AudioOutputUnitStart(m_outputUnit);

    if (!result)
        m_isPlaying = true;
}

void AudioDestinationIOS::stop()
{
    OSStatus result = AudioOutputUnitStop(m_outputUnit);

    if (!result)
        m_isPlaying = false;
}

void AudioDestinationIOS::beganAudioInterruption()
{
    if (!m_isPlaying)
        return;

    stop();
    m_interruptedOnPlayback = true;
}

void AudioDestinationIOS::endedAudioInterruption()
{
    if (!m_interruptedOnPlayback)
        return;

    m_interruptedOnPlayback = false;
    start();
}

// Pulls on our provider to get rendered audio stream.
OSStatus AudioDestinationIOS::render(UInt32 numberOfFrames, AudioBufferList* ioData)
{
    AudioBuffer* buffers = ioData->mBuffers;
    for (UInt32 frameOffset = 0; frameOffset + kRenderBufferSize <= numberOfFrames; frameOffset += kRenderBufferSize) {
        UInt32 remainingFrames = MIN(kRenderBufferSize, numberOfFrames - frameOffset);
        for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
            UInt32 bytesPerFrame = buffers[i].mDataByteSize / numberOfFrames;
            UInt32 byteOffset = frameOffset * bytesPerFrame;
            float* memory = (float*)((char*)buffers[i].mData + byteOffset);
            m_renderBus.setChannelMemory(i, memory, remainingFrames);
        }
        m_callback.render(0, &m_renderBus, remainingFrames);
    }

    return noErr;
}

// DefaultOutputUnit callback
OSStatus AudioDestinationIOS::inputProc(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32 /*busNumber*/, UInt32 numberOfFrames, AudioBufferList* ioData)
{
    AudioDestinationIOS* audioOutput = static_cast<AudioDestinationIOS*>(userData);
    return audioOutput->render(numberOfFrames, ioData);
}

void AudioDestinationIOS::frameSizeChangedProc(void *inRefCon, AudioUnit, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement)
{
    AudioDestinationIOS* audioOutput = static_cast<AudioDestinationIOS*>(inRefCon);
    UInt32 bufferSize = 0;
    UInt32 dataSize = sizeof(bufferSize);
    AudioUnitGetProperty(audioOutput->m_outputUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, (void*)&bufferSize, &dataSize);
    fprintf(stderr, ">>>> frameSizeChanged = %lu\n", static_cast<unsigned long>(bufferSize));
}

} // namespace WebCore

#endif // PLATFORM(IOS)

#endif // ENABLE(WEB_AUDIO)
