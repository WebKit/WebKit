/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "AudioHardwareListenerMac.h"

#if PLATFORM(MAC)

#include <algorithm>

enum {
    kAudioHardwarePropertyProcessIsRunning = 'prun'
};

namespace WebCore {
    
static AudioHardwareActivityType isAudioHardwareProcessRunning()
{
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyProcessIsRunning,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    
    if (!AudioObjectHasProperty(kAudioObjectSystemObject, &propertyAddress))
        return AudioHardwareActivityType::Unknown;
    
    UInt32 result = 0;
    UInt32 resultSize = sizeof(UInt32);

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, 0, &resultSize, &result))
        return AudioHardwareActivityType::Unknown;

    if (result)
        return AudioHardwareActivityType::IsActive;
    else
        return AudioHardwareActivityType::IsInactive;
}

static bool currentDeviceSupportsLowPowerBufferSize()
{
    AudioDeviceID deviceID = kAudioDeviceUnknown;
    UInt32 descriptorSize = sizeof(deviceID);
    AudioObjectPropertyAddress defaultOutputDeviceDescriptor = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster };

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputDeviceDescriptor, 0, 0, &descriptorSize, (void*)&deviceID))
        return false;

    UInt32 transportType = kAudioDeviceTransportTypeUnknown;
    descriptorSize = sizeof(transportType);
    AudioObjectPropertyAddress tranportTypeDescriptor = {
        kAudioDevicePropertyTransportType,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster,
    };

    if (AudioObjectGetPropertyData(deviceID, &tranportTypeDescriptor, 0, 0, &descriptorSize, &transportType))
        return false;

    // Only allow low-power buffer size when using built-in output device, many external devices perform
    // poorly with a large output buffer.
    return kAudioDeviceTransportTypeBuiltIn == transportType;
}

static const AudioObjectPropertyAddress& processIsRunningPropertyDescriptor()
{
    static const AudioObjectPropertyAddress processIsRunningProperty = {
        kAudioHardwarePropertyProcessIsRunning,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    return processIsRunningProperty;
}

static const AudioObjectPropertyAddress& outputDevicePropertyDescriptor()
{
    static const AudioObjectPropertyAddress outputDeviceProperty = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    return outputDeviceProperty;
}

Ref<AudioHardwareListener> AudioHardwareListener::create(Client& client)
{
    return AudioHardwareListenerMac::create(client);
}

Ref<AudioHardwareListenerMac> AudioHardwareListenerMac::create(Client& client)
{
    return adoptRef(*new AudioHardwareListenerMac(client));
}

AudioHardwareListenerMac::AudioHardwareListenerMac(Client& client)
    : AudioHardwareListener(client)
{
    setHardwareActivity(isAudioHardwareProcessRunning());
    setOutputDeviceSupportsLowPowerMode(currentDeviceSupportsLowPowerBufferSize());

    auto weakThis = makeWeakPtr(*this);
    m_block = Block_copy(^(UInt32 count, const AudioObjectPropertyAddress properties[]) {
        if (weakThis)
            weakThis->propertyChanged(count, properties);
    });

    AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &processIsRunningPropertyDescriptor(), dispatch_get_main_queue(), m_block);
    AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &outputDevicePropertyDescriptor(), dispatch_get_main_queue(), m_block);
}

AudioHardwareListenerMac::~AudioHardwareListenerMac()
{
    AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &processIsRunningPropertyDescriptor(), dispatch_get_main_queue(), m_block);
    AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &outputDevicePropertyDescriptor(), dispatch_get_main_queue(), m_block);
    Block_release(m_block);
}

void AudioHardwareListenerMac::propertyChanged(UInt32 propertyCount, const AudioObjectPropertyAddress properties[])
{
    const AudioObjectPropertyAddress& deviceRunning = processIsRunningPropertyDescriptor();
    const AudioObjectPropertyAddress& outputDevice = outputDevicePropertyDescriptor();

    for (UInt32 i = 0; i < propertyCount; ++i) {
        const AudioObjectPropertyAddress& property = properties[i];

        if (!memcmp(&property, &deviceRunning, sizeof(AudioObjectPropertyAddress)))
            processIsRunningChanged();
        else if (!memcmp(&property, &outputDevice, sizeof(AudioObjectPropertyAddress)))
            outputDeviceChanged();
    }
}

void AudioHardwareListenerMac::processIsRunningChanged()
{
    AudioHardwareActivityType activity = isAudioHardwareProcessRunning();
    if (activity == hardwareActivity())
        return;
    setHardwareActivity(activity);
    
    if (hardwareActivity() == AudioHardwareActivityType::IsActive)
        m_client.audioHardwareDidBecomeActive();
    else if (hardwareActivity() == AudioHardwareActivityType::IsInactive)
        m_client.audioHardwareDidBecomeInactive();
}

void AudioHardwareListenerMac::outputDeviceChanged()
{
    setOutputDeviceSupportsLowPowerMode(currentDeviceSupportsLowPowerBufferSize());
    m_client.audioOutputDeviceChanged();
}

}

#endif
