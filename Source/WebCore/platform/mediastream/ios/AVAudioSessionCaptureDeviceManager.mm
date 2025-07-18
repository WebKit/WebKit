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
#import "AudioSession.h"
#import "CoreAudioSharedUnit.h"
#import "Logging.h"
#import "RealtimeMediaSourceCenter.h"
#import <AVFoundation/AVAudioSession.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <ranges>
#import <wtf/Assertions.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/Vector.h>

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/AVAudioSessionCaptureDeviceManagerAdditionsIncludes.mm>
#endif

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
    : m_dispatchQueue(WorkQueue::create("com.apple.WebKit.AVAudioSessionCaptureDeviceManager"_s))
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
    // FIXME: Stop using `AVAudioSessionCategoryOptionAllowBluetooth` as it is deprecated (rdar://145294046).
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto options = AVAudioSessionCategoryOptionAllowBluetooth;
    ALLOW_DEPRECATED_DECLARATIONS_END
    [m_audioSession setCategory:AVAudioSessionCategoryPlayAndRecord mode:AVAudioSessionModeDefault options:options error:&error];
    RELEASE_LOG_ERROR_IF(error, WebRTC, "Failed to set audio session category with error: %@.", error.localizedDescription);

    if (!error) {
        [m_listener invalidate];
        m_listener = adoptNS([[WebAVAudioSessionAvailableInputsListener alloc] initWithCallback:this audioSession:m_audioSession.get()]);
    }
}

AVAudioSessionCaptureDeviceManager::~AVAudioSessionCaptureDeviceManager()
{
    [m_listener invalidate];
    m_listener = nullptr;
}

const Vector<CaptureDevice>& AVAudioSessionCaptureDeviceManager::captureDevices()
{
    if (!m_captureDevices)
        refreshAudioCaptureDevices();
    return m_captureDevices.value();
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

void AVAudioSessionCaptureDeviceManager::setPreferredMicrophoneID(const String& microphoneID)
{
    auto previousMicrophoneID = m_preferredMicrophoneID;
    m_preferredMicrophoneID = microphoneID;
    if (!setPreferredAudioSessionDeviceIDs())
        m_preferredMicrophoneID = WTFMove(previousMicrophoneID);
}

void AVAudioSessionCaptureDeviceManager::configurePreferredMicrophone()
{
    ASSERT(!m_preferredMicrophoneID.isEmpty());
    if (!m_preferredMicrophoneID.isEmpty())
        setPreferredAudioSessionDeviceIDs();
}

void AVAudioSessionCaptureDeviceManager::setPreferredSpeakerID(const String& speakerID)
{
    auto previousSpeakerID = m_preferredSpeakerID;
    m_preferredSpeakerID = speakerID;
    if (!setPreferredAudioSessionDeviceIDs())
        m_preferredSpeakerID = WTFMove(previousSpeakerID);
    else if (!m_preferredSpeakerID.isEmpty()) {
#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/AVAudioSessionCaptureDeviceManagerAdditions-2.mm>
#endif
    } else
        m_isReceiverPreferredSpeaker = false;

    AudioSession::singleton().setCategory(AudioSession::singleton().category(), AudioSession::singleton().mode(), AudioSession::singleton().routeSharingPolicy());
}

bool AVAudioSessionCaptureDeviceManager::setPreferredAudioSessionDeviceIDs()
{
    AVAudioSessionPortDescription *preferredInputPort = nil;
    if (!m_preferredMicrophoneID.isEmpty()) {
        RetainPtr nsDeviceUID = m_preferredMicrophoneID.createNSString();
        for (AVAudioSessionPortDescription *portDescription in [m_audioSession availableInputs]) {
            if ([portDescription.UID isEqualToString:nsDeviceUID.get()]) {
                preferredInputPort = portDescription;
                break;
            }
        }
    }
    {
        RELEASE_LOG_INFO(WebRTC, "AVAudioSessionCaptureDeviceManager setting preferred input to '%{public}s'", m_preferredMicrophoneID.ascii().data());

        NSError *error = nil;
        if (![[PAL::getAVAudioSessionClass() sharedInstance] setPreferredInput:preferredInputPort error:&error]) {
            RELEASE_LOG_ERROR(WebRTC, "AVAudioSessionCaptureDeviceManager failed to set preferred input to '%{public}s' with error: %@", m_preferredMicrophoneID.utf8().data(), error.localizedDescription);
            return false;
        }
    }

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/AVAudioSessionCaptureDeviceManagerAdditions-3.mm>
#endif
    return true;
}

void AVAudioSessionCaptureDeviceManager::scheduleUpdateCaptureDevices()
{
    computeCaptureDevices([] { });
}

void AVAudioSessionCaptureDeviceManager::refreshAudioCaptureDevices()
{
    Vector<AVAudioSessionCaptureDevice> newAudioDevices;
    m_dispatchQueue->dispatchSync([&] {
        newAudioDevices = retrieveAudioSessionCaptureDevices();
    });
    setAudioCaptureDevices(crossThreadCopy(WTFMove(newAudioDevices)));
}

void AVAudioSessionCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& completion)
{
    m_dispatchQueue->dispatch([this, completion = WTFMove(completion)] () mutable {
        auto newAudioDevices = retrieveAudioSessionCaptureDevices();
        callOnWebThreadOrDispatchAsyncOnMainThread(makeBlockPtr([this, completion = WTFMove(completion), newAudioDevices = crossThreadCopy(WTFMove(newAudioDevices))] () mutable {
            setAudioCaptureDevices(WTFMove(newAudioDevices));
            completion();
        }).get());
    });
}

Vector<AVAudioSessionCaptureDevice> AVAudioSessionCaptureDeviceManager::retrieveAudioSessionCaptureDevices() const
{
    auto currentInput = [m_audioSession currentRoute].inputs.firstObject;
    if (currentInput) {
        if (currentInput != m_lastDefaultMicrophone.get()) {
            auto device = AVAudioSessionCaptureDevice::createInput(currentInput, currentInput);
            callOnWebThreadOrDispatchAsyncOnMainThread(makeBlockPtr([device = crossThreadCopy(WTFMove(device))] () mutable {
                CoreAudioSharedUnit::singleton().handleNewCurrentMicrophoneDevice(WTFMove(device));
            }).get());
        }
        m_lastDefaultMicrophone = currentInput;
    }

    auto availableInputs = [m_audioSession availableInputs];

    Vector<AVAudioSessionCaptureDevice> newAudioDevices;
    newAudioDevices.reserveInitialCapacity(availableInputs.count);
    for (AVAudioSessionPortDescription *portDescription in availableInputs) {
        auto device = AVAudioSessionCaptureDevice::createInput(portDescription, currentInput);
        newAudioDevices.append(WTFMove(device));
    }

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/AVAudioSessionCaptureDeviceManagerAdditions.mm>
#endif

    return newAudioDevices;
}

void AVAudioSessionCaptureDeviceManager::setAudioCaptureDevices(Vector<AVAudioSessionCaptureDevice>&& newAudioDevices)
{
    bool firstTime = !m_captureDevices;
    bool deviceListChanged = !m_audioSessionCaptureDevices || newAudioDevices.size() != m_audioSessionCaptureDevices->size();
    bool defaultDeviceChanged = false;
    if (!deviceListChanged && !firstTime) {
        for (auto& newState : newAudioDevices) {

            std::optional<CaptureDevice> oldState;
            for (const auto& device : m_audioSessionCaptureDevices.value()) {
                if (device.type() == newState.type() && device.persistentId() == newState.persistentId()) {
                    oldState = device;
                    break;
                }
            }

            if (!oldState || newState.isDefault() != oldState->isDefault() || newState.enabled() != oldState->enabled()) {
                deviceListChanged = true;
                break;
            }
        }
    }

    if (!deviceListChanged && !firstTime && !defaultDeviceChanged)
        return;

    m_audioSessionCaptureDevices = WTFMove(newAudioDevices);

    Vector<CaptureDevice> newCaptureDevices;
    Vector<CaptureDevice> newSpeakerDevices;
    for (auto& device : *m_audioSessionCaptureDevices) {
        if (device.type() == CaptureDevice::DeviceType::Microphone)
            newCaptureDevices.append(device);
        else {
            ASSERT(device.type() == CaptureDevice::DeviceType::Speaker);
            newSpeakerDevices.append(device);
        }
    }

    std::ranges::sort(newCaptureDevices, [] (auto& first, auto& second) -> bool {
        return first.isDefault() && !second.isDefault();
    });
    m_captureDevices = WTFMove(newCaptureDevices);

    std::ranges::sort(newSpeakerDevices, [] (auto& first, auto& second) -> bool {
        return first.isDefault() && !second.isDefault();
    });
    m_speakerDevices = WTFMove(newSpeakerDevices);

    if (deviceListChanged && !firstTime)
        deviceChanged();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
