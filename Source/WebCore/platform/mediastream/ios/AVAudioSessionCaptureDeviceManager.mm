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

#import "config.h"
#import "AVAudioSessionCaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)

#import "AVAudioSessionCaptureDevice.h"
#import "Logging.h"
#import "RealtimeMediaSourceCenter.h"
#import <AVFoundation/AVAudioSession.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/Vector.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

@interface WebAVAudioSessionAvailableInputsListener : NSObject {
    WebCore::AVAudioSessionCaptureDeviceManager* _callback;
}
@end

@implementation WebAVAudioSessionAvailableInputsListener
- (id)initWithCallback:(WebCore::AVAudioSessionCaptureDeviceManager *)callback audioSession:(AVAudioSession *)session
{
    self = [super init];
    if (!self)
        return nil;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(routeDidChange:) name:PAL::get_AVFoundation_AVAudioSessionRouteChangeNotification() object:session];

    _callback = callback;

    return self;
}

- (void)invalidate
{
    _callback = nullptr;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)routeDidChange:(NSNotification *)notification
{
    if (!_callback)
        return;

    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() mutable {
        if (auto* callback = protectedSelf->_callback)
            callback->scheduleUpdateCaptureDevices();
    });

}

@end

namespace WebCore {

AVAudioSessionCaptureDeviceManager& AVAudioSessionCaptureDeviceManager::singleton()
{
    static NeverDestroyed<AVAudioSessionCaptureDeviceManager> manager;
    return manager;
}

AVAudioSessionCaptureDeviceManager::AVAudioSessionCaptureDeviceManager()
{
#if !PLATFORM(MACCATALYST)
    m_audioSession = adoptNS([[PAL::getAVAudioSessionClass() alloc] initAuxiliarySession]);
#else
    // FIXME: Figure out if this is correct for Catalyst, where auxiliary session isn't available.
    m_audioSession = [PAL::getAVAudioSessionClass() sharedInstance];
#endif
    
    NSError *error = nil;
    auto options = AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionMixWithOthers;
    [m_audioSession setCategory:AVAudioSessionCategoryPlayAndRecord mode:AVAudioSessionModeVideoChat options:options error:&error];
    if (error) {
        RELEASE_LOG_ERROR(WebRTC, "Failed to set audio session category with error: %@.", error.localizedDescription);
        return;
    }
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

Optional<CaptureDevice> AVAudioSessionCaptureDeviceManager::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& deviceID)
{
    ASSERT_UNUSED(type, type == CaptureDevice::DeviceType::Microphone);
    for (auto& device : captureDevices()) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return WTF::nullopt;
}

Vector<AVAudioSessionCaptureDevice>& AVAudioSessionCaptureDeviceManager::audioSessionCaptureDevices()
{
    if (!m_audioSessionCaptureDevices)
        refreshAudioCaptureDevices();
    return m_audioSessionCaptureDevices.value();
}

Optional<AVAudioSessionCaptureDevice> AVAudioSessionCaptureDeviceManager::audioSessionDeviceWithUID(const String& deviceID)
{
    for (auto& device : audioSessionCaptureDevices()) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return WTF::nullopt;
}

void AVAudioSessionCaptureDeviceManager::scheduleUpdateCaptureDevices()
{
    if (m_updateDeviceStateQueue.hasPendingTasks())
        return;

    m_updateDeviceStateQueue.enqueueTask([this] {
        refreshAudioCaptureDevices();
    });
}

void AVAudioSessionCaptureDeviceManager::refreshAudioCaptureDevices()
{
    if (!m_listener)
        m_listener = adoptNS([[WebAVAudioSessionAvailableInputsListener alloc] initWithCallback:this audioSession:m_audioSession.get()]);

    NSError *error = nil;
    [m_audioSession setActive:YES withOptions:0 error:&error];
    if (error)
        RELEASE_LOG_ERROR(WebRTC, "Failed to activate audio session with error: %@.", error.localizedDescription);

    Vector<AVAudioSessionCaptureDevice> newAudioDevices;
    Vector<CaptureDevice> newDevices;
    auto *defaultInput = [m_audioSession currentRoute].inputs.firstObject;
    for (AVAudioSessionPortDescription *portDescription in [m_audioSession availableInputs]) {
        auto audioDevice = AVAudioSessionCaptureDevice::create(portDescription, defaultInput);
        newDevices.append(audioDevice);
        newAudioDevices.append(WTFMove(audioDevice));
    }

    bool firstTime = !m_devices;
    bool haveDeviceChanges = !m_devices || newAudioDevices.size() != m_devices->size();
    if (!haveDeviceChanges) {
        for (size_t i = 0; i < newAudioDevices.size(); ++i) {
            auto& oldState = (*m_devices)[i];
            auto& newState = newAudioDevices[i];
            if (newState.type() != oldState.type() || newState.persistentId() != oldState.persistentId() || newState.enabled() != oldState.enabled() || newState.isDefault() != oldState.isDefault())
                haveDeviceChanges = true;
        }
    }

    if (!haveDeviceChanges && !firstTime)
        return;

    m_audioSessionCaptureDevices = WTFMove(newAudioDevices);
    std::sort(newDevices.begin(), newDevices.end(), [] (auto& first, auto& second) -> bool {
        return first.isDefault() && !second.isDefault();
    });
    m_devices = WTFMove(newDevices);

    if (!firstTime)
        deviceChanged();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
