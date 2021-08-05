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
#import <wtf/BlockPtr.h>
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
    : m_dispatchQueue(WorkQueue::create("com.apple.WebKit.AVAudioSessionCaptureDeviceManager"))
{
    m_dispatchQueue->dispatch([this] {
        createAudioSession();
    });
}

void AVAudioSessionCaptureDeviceManager::createAudioSession()
{
#if !PLATFORM(MACCATALYST)
    m_audioSession = adoptNS([[PAL::getAVAudioSessionClass() alloc] initAuxiliarySession]);
#else
    // FIXME: Figure out if this is correct for Catalyst, where auxiliary session isn't available.
    m_audioSession = [PAL::getAVAudioSessionClass() sharedInstance];
#endif

    NSError *error = nil;
    auto options = AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionMixWithOthers;
    [m_audioSession setCategory:AVAudioSessionCategoryPlayAndRecord mode:AVAudioSessionModeDefault options:options error:&error];
    RELEASE_LOG_ERROR_IF(error, WebRTC, "Failed to set audio session category with error: %@.", error.localizedDescription);
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

std::optional<AVAudioSessionCaptureDevice> AVAudioSessionCaptureDeviceManager::audioSessionDeviceWithUID(const String& deviceID)
{
    if (!m_audioSessionCaptureDevices)
        refreshAudioCaptureDevices();

    for (auto& device : *m_audioSessionCaptureDevices) {
        if (device.persistentId() == deviceID)
            return device;
    }
    return std::nullopt;
}

void AVAudioSessionCaptureDeviceManager::setPreferredAudioSessionDeviceUID(const String& deviceUID)
{
    AVAudioSessionPortDescription *preferredPort = nil;
    NSString *nsDeviceUID = deviceUID;
    for (AVAudioSessionPortDescription *portDescription in [m_audioSession availableInputs]) {
        if ([portDescription.UID isEqualToString:nsDeviceUID]) {
            preferredPort = portDescription;
            break;
        }
    }

    if (!preferredPort) {
        RELEASE_LOG_ERROR(WebRTC, "failed to find preferred input '%{public}s'", deviceUID.ascii().data());
        return;
    }

    NSError *error = nil;
    if (![[PAL::getAVAudioSessionClass() sharedInstance] setPreferredInput:preferredPort error:&error])
        RELEASE_LOG_ERROR(WebRTC, "failed to set preferred input to '%{public}s' with error: %@", deviceUID.ascii().data(), error.localizedDescription);
}

void AVAudioSessionCaptureDeviceManager::scheduleUpdateCaptureDevices()
{
    computeCaptureDevices([] { });
}

void AVAudioSessionCaptureDeviceManager::refreshAudioCaptureDevices()
{
    if (m_audioSessionState == AudioSessionState::Inactive) {
        m_audioSessionState = AudioSessionState::Active;

        m_dispatchQueue->dispatchSync([this] {
            activateAudioSession();
        });
    }

    Vector<AVAudioSessionCaptureDevice> newAudioDevices;
    m_dispatchQueue->dispatchSync([&] {
        newAudioDevices = retrieveAudioSessionCaptureDevices();
    });
    setAudioCaptureDevices(WTFMove(newAudioDevices).isolatedCopy());
}

void AVAudioSessionCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& completion)
{
    if (m_audioSessionState == AudioSessionState::Inactive) {
        m_audioSessionState = AudioSessionState::Active;

        m_dispatchQueue->dispatch([this] {
            activateAudioSession();
        });
    }

    m_dispatchQueue->dispatch([this, completion = WTFMove(completion)] () mutable {
        auto newAudioDevices = retrieveAudioSessionCaptureDevices();
        callOnWebThreadOrDispatchAsyncOnMainThread(makeBlockPtr([this, completion = WTFMove(completion), newAudioDevices = WTFMove(newAudioDevices).isolatedCopy()] () mutable {
            setAudioCaptureDevices(WTFMove(newAudioDevices));
            completion();
        }).get());
    });
}

void AVAudioSessionCaptureDeviceManager::activateAudioSession()
{
    if (!m_listener)
        m_listener = adoptNS([[WebAVAudioSessionAvailableInputsListener alloc] initWithCallback:this audioSession:m_audioSession.get()]);

    NSError *error = nil;
    [m_audioSession setActive:YES withOptions:0 error:&error];
    if (error)
        RELEASE_LOG_ERROR(WebRTC, "Failed to activate audio session with error: %@.", error.localizedDescription);
}

Vector<AVAudioSessionCaptureDevice> AVAudioSessionCaptureDeviceManager::retrieveAudioSessionCaptureDevices() const
{
    auto *defaultInput = [m_audioSession currentRoute].inputs.firstObject;
    auto availableInputs = [m_audioSession availableInputs];

    Vector<AVAudioSessionCaptureDevice> newAudioDevices;
    newAudioDevices.reserveInitialCapacity(availableInputs.count);
    for (AVAudioSessionPortDescription *portDescription in availableInputs)
        newAudioDevices.uncheckedAppend(AVAudioSessionCaptureDevice::create(portDescription, defaultInput));

    return newAudioDevices;
}

void AVAudioSessionCaptureDeviceManager::setAudioCaptureDevices(Vector<AVAudioSessionCaptureDevice>&& newAudioDevices)
{
    bool firstTime = !m_devices;
    bool deviceListChanged = newAudioDevices.size() != m_devices->size();
    bool defaultDeviceChanged = false;
    if (!deviceListChanged && !firstTime) {
        for (auto& newState : newAudioDevices) {

            std::optional<CaptureDevice> oldState;
            for (const auto& device : m_devices.value()) {
                if (device.type() == newState.type() && device.persistentId() == newState.persistentId()) {
                    oldState = device;
                    break;
                }
            }

            if (!oldState.has_value()) {
                deviceListChanged = true;
                break;
            }
            if (newState.isDefault() != oldState.value().isDefault())
                defaultDeviceChanged  = true;

            if (newState.enabled() != oldState.value().enabled()) {
                deviceListChanged = true;
                break;
            }
        }
    }

    if (!deviceListChanged && !firstTime && !defaultDeviceChanged)
        return;

    auto newDevices = copyToVectorOf<CaptureDevice>(newAudioDevices);
    m_audioSessionCaptureDevices = WTFMove(newAudioDevices);
    std::sort(newDevices.begin(), newDevices.end(), [] (auto& first, auto& second) -> bool {
        return first.isDefault() && !second.isDefault();
    });
    m_devices = WTFMove(newDevices);

    if (deviceListChanged && !firstTime)
        deviceChanged();
}

void AVAudioSessionCaptureDeviceManager::enableAllDevicesQuery()
{
    if (m_audioSessionState != AudioSessionState::NotNeeded)
        return;

    m_audioSessionState = AudioSessionState::Inactive;
    refreshAudioCaptureDevices();
}

void AVAudioSessionCaptureDeviceManager::disableAllDevicesQuery()
{
    if (m_audioSessionState == AudioSessionState::NotNeeded)
        return;

    if (m_audioSessionState == AudioSessionState::Active) {
        m_dispatchQueue->dispatch([this] {
            if (m_audioSessionState != AudioSessionState::NotNeeded)
                return;
            NSError *error = nil;
            [m_audioSession setActive:NO withOptions:0 error:&error];
            if (error)
                RELEASE_LOG_ERROR(WebRTC, "Failed to disactivate audio session with error: %@.", error.localizedDescription);
        });
    }

    m_audioSessionState = AudioSessionState::NotNeeded;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
