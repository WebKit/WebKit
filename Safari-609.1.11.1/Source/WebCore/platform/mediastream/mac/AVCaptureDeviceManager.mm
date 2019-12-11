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

#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

@interface WebCoreAVCaptureDeviceManagerObserver : NSObject
{
    AVCaptureDeviceManager* m_callback;
}

-(id)initWithCallback:(AVCaptureDeviceManager*)callback;
-(void)disconnect;
-(void)deviceConnectedDidChange:(NSNotification *)notification;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context;
@end

namespace WebCore {


Vector<CaptureDevice>& AVCaptureDeviceManager::captureDevicesInternal()
{
    if (!isAvailable())
        return m_devices;

    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        refreshCaptureDevices();
        m_notifyWhenDeviceListChanges = true;
    }

    return m_devices;
}

const Vector<CaptureDevice>& AVCaptureDeviceManager::captureDevices()
{
    return captureDevicesInternal();
}

inline static bool deviceIsAvailable(AVCaptureDevice *device)
{
    if (![device isConnected])
        return false;

#if !PLATFORM(IOS_FAMILY)
    if ([device isSuspended])
        return false;
#endif

    return true;
}

void AVCaptureDeviceManager::updateCachedAVCaptureDevices()
{
    auto* currentDevices = [PAL::getAVCaptureDeviceClass() devices];
    auto changedDevices = adoptNS([[NSMutableArray alloc] init]);
    for (AVCaptureDevice *cachedDevice in m_avCaptureDevices.get()) {
        if (![currentDevices containsObject:cachedDevice])
            [changedDevices addObject:cachedDevice];
    }

    if ([changedDevices count]) {
        for (AVCaptureDevice *device in changedDevices.get())
            [device removeObserver:m_objcObserver.get() forKeyPath:@"suspended"];
        [m_avCaptureDevices removeObjectsInArray:changedDevices.get()];
    }

    for (AVCaptureDevice *device in currentDevices) {

        if (![device hasMediaType:AVMediaTypeVideo] && ![device hasMediaType:AVMediaTypeMuxed])
            continue;

        if ([m_avCaptureDevices.get() containsObject:device])
            continue;

        [device addObserver:m_objcObserver.get() forKeyPath:@"suspended" options:NSKeyValueObservingOptionNew context:(void *)nil];
        [m_avCaptureDevices.get() addObject:device];
    }

}

static inline CaptureDevice toCaptureDevice(AVCaptureDevice *device)
{
    CaptureDevice captureDevice { device.uniqueID, CaptureDevice::DeviceType::Camera, device.localizedName };
    captureDevice.setEnabled(deviceIsAvailable(device));
    return captureDevice;
}

bool AVCaptureDeviceManager::isMatchingExistingCaptureDevice(AVCaptureDevice *device)
{
    auto existingCaptureDevice = captureDeviceFromPersistentID(device.uniqueID);
    if (!existingCaptureDevice)
        return false;

    return deviceIsAvailable(device) == existingCaptureDevice.enabled();
}

static inline bool isDefaultVideoCaptureDeviceFirst(const Vector<CaptureDevice>& devices, const String& defaultDeviceID)
{
    if (devices.isEmpty())
        return false;
    return devices[0].persistentId() == defaultDeviceID;
}

static inline bool isVideoDevice(AVCaptureDevice *device)
{
    return [device hasMediaType:AVMediaTypeVideo] || [device hasMediaType:AVMediaTypeMuxed];
}

void AVCaptureDeviceManager::refreshCaptureDevices()
{
    if (!m_avCaptureDevices) {
        m_avCaptureDevices = adoptNS([[NSMutableArray alloc] init]);
        registerForDeviceNotifications();
    }

    updateCachedAVCaptureDevices();

    auto* currentDevices = [PAL::getAVCaptureDeviceClass() devices];
    Vector<CaptureDevice> deviceList;

    auto* defaultVideoDevice = [PAL::getAVCaptureDeviceClass() defaultDeviceWithMediaType: AVMediaTypeVideo];
#if PLATFORM(IOS)
    if ([defaultVideoDevice position] != AVCaptureDevicePositionFront) {
        defaultVideoDevice = nullptr;
        for (AVCaptureDevice *platformDevice in currentDevices) {
            if (!isVideoDevice(platformDevice))
                continue;

            if ([platformDevice position] == AVCaptureDevicePositionFront) {
                defaultVideoDevice = platformDevice;
                break;
            }
        }
    }
#endif

    bool deviceHasChanged = false;
    if (defaultVideoDevice) {
        deviceList.append(toCaptureDevice(defaultVideoDevice));
        deviceHasChanged = !isDefaultVideoCaptureDeviceFirst(captureDevices(), defaultVideoDevice.uniqueID);
    }
    for (AVCaptureDevice *platformDevice in currentDevices) {
        if (!isVideoDevice(platformDevice))
            continue;

        if (!deviceHasChanged && !isMatchingExistingCaptureDevice(platformDevice))
            deviceHasChanged = true;

        if (platformDevice.uniqueID == defaultVideoDevice.uniqueID)
            continue;

        deviceList.append(toCaptureDevice(platformDevice));
    }

    if (deviceHasChanged || m_devices.size() != deviceList.size()) {
        deviceHasChanged = true;
        m_devices = WTFMove(deviceList);
    }

    if (m_notifyWhenDeviceListChanges && deviceHasChanged)
        deviceChanged();
}

bool AVCaptureDeviceManager::isAvailable()
{
    return PAL::isAVFoundationFrameworkAvailable();
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
    for (AVCaptureDevice *device in m_avCaptureDevices.get())
        [device removeObserver:m_objcObserver.get() forKeyPath:@"suspended"];
}

void AVCaptureDeviceManager::registerForDeviceNotifications()
{
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnectedDidChange:) name:AVCaptureDeviceWasConnectedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnectedDidChange:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
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
    m_callback = nil;
}

- (void)deviceConnectedDidChange:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback)
            m_callback->refreshCaptureDevices();
    });
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(context);
    UNUSED_PARAM(change);

    if (!m_callback)
        return;

    if ([keyPath isEqualToString:@"suspended"])
        m_callback->refreshCaptureDevices();
}

@end

#endif // ENABLE(MEDIA_STREAM)
