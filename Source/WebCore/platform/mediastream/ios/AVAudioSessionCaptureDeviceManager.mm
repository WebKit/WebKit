/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "AVAudioSessionCaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)

#include "AVAudioSessionCaptureDevice.h"
#include "RealtimeMediaSourceCenter.h"
#include <AVFoundation/AVAudioSession.h>
#include <wtf/SoftLinking.h>
#include <wtf/Vector.h>

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVAudioSession)
#define AVAudioSession getAVAudioSessionClass()

void* AvailableInputsContext = &AvailableInputsContext;

@interface WebAVAudioSessionAvailableInputsListener : NSObject {
    WTF::Function<void()> _callback;
}
@end

@implementation WebAVAudioSessionAvailableInputsListener
- (id)initWithCallback:(WTF::Function<void()>&&)callback
{
    self = [super init];
    if (!self)
        return nil;

    _callback = WTFMove(callback);
    return self;
}

- (void)invalidate
{
    _callback = nullptr;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    UNUSED_PARAM(keyPath);
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    if (context == AvailableInputsContext && _callback)
        _callback();
}
@end

namespace WebCore {

AVAudioSessionCaptureDeviceManager& AVAudioSessionCaptureDeviceManager::singleton()
{
    static NeverDestroyed<AVAudioSessionCaptureDeviceManager> manager;
    return manager;
}

AVAudioSessionCaptureDeviceManager::~AVAudioSessionCaptureDeviceManager()
{
    [m_listener invalidate];
    m_listener = nullptr;
}

const Vector<CaptureDevice>& AVAudioSessionCaptureDeviceManager::captureDevices()
{
    if (!m_devices)
        refreshAudioCaptureDevices();
    return m_devices.value();
}

std::optional<CaptureDevice> AVAudioSessionCaptureDeviceManager::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& deviceID)
{
    ASSERT_UNUSED(type, type == CaptureDevice::DeviceType::Microphone);
    for (auto& device : captureDevices()) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return std::nullopt;
}

Vector<AVAudioSessionCaptureDevice>& AVAudioSessionCaptureDeviceManager::audioSessionCaptureDevices()
{
    if (!m_audioSessionCaptureDevices)
        refreshAudioCaptureDevices();
    return m_audioSessionCaptureDevices.value();
}

std::optional<AVAudioSessionCaptureDevice> AVAudioSessionCaptureDeviceManager::audioSessionDeviceWithUID(const String& deviceID)
{
    if (!m_audioSessionCaptureDevices)
        refreshAudioCaptureDevices();

    for (auto& device : audioSessionCaptureDevices()) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return std::nullopt;
}

void AVAudioSessionCaptureDeviceManager::refreshAudioCaptureDevices()
{
    if (!m_listener) {
        m_listener = adoptNS([[WebAVAudioSessionAvailableInputsListener alloc] initWithCallback:[this] {
            refreshAudioCaptureDevices();
        }]);
        [[AVAudioSession sharedInstance] addObserver:m_listener.get() forKeyPath:@"availableInputs" options:0 context:AvailableInputsContext];
    }

    Vector<AVAudioSessionCaptureDevice> newAudioDevices;
    Vector<CaptureDevice> newDevices;

    for (AVAudioSessionPortDescription *portDescription in [AVAudioSession sharedInstance].availableInputs) {
        auto audioDevice = AVAudioSessionCaptureDevice::create(portDescription);
        newDevices.append(audioDevice);
        newAudioDevices.append(WTFMove(audioDevice));
    }

    m_audioSessionCaptureDevices = WTFMove(newAudioDevices);
    m_devices = WTFMove(newDevices);

    deviceChanged();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
