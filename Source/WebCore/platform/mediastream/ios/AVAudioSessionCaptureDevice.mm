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

#import "config.h"
#import "AVAudioSessionCaptureDevice.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)

#import <AVFoundation/AVAudioSession.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

AVAudioSessionCaptureDevice AVAudioSessionCaptureDevice::createInput(AVAudioSessionPortDescription* deviceInput, AVAudioSessionPortDescription *defaultInput)
{
    return AVAudioSessionCaptureDevice(deviceInput, defaultInput, CaptureDevice::DeviceType::Microphone);
}

AVAudioSessionCaptureDevice AVAudioSessionCaptureDevice::createOutput(AVAudioSessionPortDescription *deviceOutput, AVAudioSessionPortDescription *defaultOutput)
{
    return AVAudioSessionCaptureDevice(deviceOutput, defaultOutput, CaptureDevice::DeviceType::Speaker);
}

AVAudioSessionCaptureDevice::AVAudioSessionCaptureDevice(AVAudioSessionPortDescription *device, AVAudioSessionPortDescription *defaultDevice, CaptureDevice::DeviceType deviceType)
    : CaptureDevice(device.UID, deviceType, device.portName)
    , m_description(device)
{
    setEnabled(true);
    setIsDefault(defaultDevice && [defaultDevice.UID isEqualToString:device.UID]);
}

bool AVAudioSessionCaptureDevice::isBuiltin() const
{
    if (type() == CaptureDevice::DeviceType::Microphone)
        return [m_description portType] == AVAudioSessionPortBuiltInMic;

    return [m_description portType] == AVAudioSessionPortBuiltInReceiver || [m_description portType] == AVAudioSessionPortBuiltInSpeaker;
}

bool AVAudioSessionCaptureDevice::isLineInOrOut() const
{
    return [m_description portType] == (type() == CaptureDevice::DeviceType::Microphone ? AVAudioSessionPortLineIn : AVAudioSessionPortLineOut);
}

bool AVAudioSessionCaptureDevice::isHeadset() const
{
    return [m_description portType] == (type() == CaptureDevice::DeviceType::Microphone ? AVAudioSessionPortHeadsetMic : AVAudioSessionPortHeadphones);
}

AVAudioSessionCaptureDevice::AVAudioSessionCaptureDevice(const String& persistentId, DeviceType type, const String& label, const String& groupId, bool isEnabled, bool isDefault, bool isMock, RetainPtr<AVAudioSessionPortDescription>&& description)
    : CaptureDevice(persistentId, type, label, groupId, isEnabled, isDefault, isMock)
    , m_description(WTFMove(description))
{
}

AVAudioSessionCaptureDevice AVAudioSessionCaptureDevice::isolatedCopy() &&
{
    return {
        WTFMove(m_persistentId).isolatedCopy(),
        m_type,
        WTFMove(m_label).isolatedCopy(),
        WTFMove(m_groupId).isolatedCopy(),
        m_enabled,
        m_default,
        m_isMockDevice,
        WTFMove(m_description)
    };
}

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
