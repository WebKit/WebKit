/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AVCaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVVideoCaptureSource.h"
#import "AudioSourceProvider.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "RealtimeMediaSource.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeMediaSourceSupportedConstraints.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVCaptureSession.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>

typedef AVCaptureDevice AVCaptureDeviceTypedef;
typedef AVCaptureSession AVCaptureSessionType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureSession)

SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeMuxed, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVCaptureDeviceWasConnectedNotification, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVCaptureDeviceWasDisconnectedNotification, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeMuxed getAVMediaTypeMuxed()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVCaptureDeviceWasConnectedNotification getAVCaptureDeviceWasConnectedNotification()
#define AVCaptureDeviceWasDisconnectedNotification getAVCaptureDeviceWasDisconnectedNotification()

using namespace WebCore;

@interface WebCoreAVCaptureDeviceManagerObserver : NSObject
{
    AVCaptureDeviceManager* m_callback;
}

-(id)initWithCallback:(AVCaptureDeviceManager*)callback;
-(void)disconnect;
-(void)deviceDisconnected:(NSNotification *)notification;
-(void)deviceConnected:(NSNotification *)notification;
@end

namespace WebCore {


Vector<CaptureDevice>& AVCaptureDeviceManager::captureDevicesInternal()
{
    if (!isAvailable())
        return m_devices;

    static bool firstTime = true;
    if (firstTime && !m_devices.size()) {
        firstTime = false;
        refreshCaptureDevices();
        registerForDeviceNotifications();
    }

    return m_devices;
}

const Vector<CaptureDevice>& AVCaptureDeviceManager::captureDevices()
{
    return captureDevicesInternal();
}

inline static bool deviceIsAvailable(AVCaptureDeviceTypedef *device)
{
    if (![device isConnected])
        return false;

#if !PLATFORM(IOS_FAMILY)
    if ([device isSuspended] || [device isInUseByAnotherApplication])
        return false;
#endif

    return true;
}

void AVCaptureDeviceManager::refreshAVCaptureDevicesOfType(CaptureDevice::DeviceType type)
{
    ASSERT(type == CaptureDevice::DeviceType::Camera || type == CaptureDevice::DeviceType::Microphone);

    NSString *platformType = (type == CaptureDevice::DeviceType::Camera) ? AVMediaTypeVideo : AVMediaTypeAudio;

    for (AVCaptureDeviceTypedef *platformDevice in [getAVCaptureDeviceClass() devices]) {

        bool hasMatchingType = [platformDevice hasMediaType:platformType] || [platformDevice hasMediaType:AVMediaTypeMuxed];
        if (!hasMatchingType)
            continue;

        CaptureDevice existingCaptureDevice = captureDeviceFromPersistentID(platformDevice.uniqueID);
        if (existingCaptureDevice && existingCaptureDevice.type() == type)
            continue;

        CaptureDevice captureDevice(platformDevice.uniqueID, type, platformDevice.localizedName);
        captureDevice.setEnabled(deviceIsAvailable(platformDevice));
        m_devices.append(captureDevice);
    }
}

void AVCaptureDeviceManager::refreshCaptureDevices()
{
    refreshAVCaptureDevicesOfType(CaptureDevice::DeviceType::Camera);
}

bool AVCaptureDeviceManager::isAvailable()
{
    return AVFoundationLibrary();
}

AVCaptureDeviceManager& AVCaptureDeviceManager::singleton()
{
    static NeverDestroyed<AVCaptureDeviceManager> manager;
    return manager;
}

AVCaptureDeviceManager::AVCaptureDeviceManager()
    : m_objcObserver(adoptNS([[WebCoreAVCaptureDeviceManagerObserver alloc] initWithCallback: this]))
{
}

AVCaptureDeviceManager::~AVCaptureDeviceManager()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver disconnect];
}

void AVCaptureDeviceManager::registerForDeviceNotifications()
{
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnected:) name:AVCaptureDeviceWasConnectedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceDisconnected:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
}

void AVCaptureDeviceManager::deviceConnected()
{
    refreshCaptureDevices();
    deviceChanged();
}

void AVCaptureDeviceManager::deviceDisconnected(AVCaptureDeviceTypedef* device)
{
    auto& devices = captureDevicesInternal();

    size_t count = devices.size();
    if (!count)
        return;

    String deviceID = device.uniqueID;
    for (size_t i = 0; i < count; ++i) {
        if (devices[i].persistentId() == deviceID) {
            LOG(Media, "AVCaptureDeviceManager::deviceDisconnected(%p), device %d disabled", this, i);
            devices[i].setEnabled(false);
        }
    }

    deviceChanged();
}

} // namespace WebCore

@implementation WebCoreAVCaptureDeviceManagerObserver

- (id)initWithCallback:(AVCaptureDeviceManager*)callback
{
    self = [super init];
    if (!self)
        return nil;
    m_callback = callback;
    return self;
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    m_callback = 0;
}

- (void)deviceDisconnected:(NSNotification *)notification
{
    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback) {
            AVCaptureDeviceTypedef *device = [notification object];
            m_callback->deviceDisconnected(device);
        }
    });
}

- (void)deviceConnected:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback)
            m_callback->deviceConnected();
    });
}

@end

#endif // ENABLE(MEDIA_STREAM)
