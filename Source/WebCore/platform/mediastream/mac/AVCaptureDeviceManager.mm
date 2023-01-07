/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

static NSMutableArray<NSString*>* cameraCaptureDeviceTypes()
{
    ASSERT(isMainThread());
    NSMutableArray<NSString*>* deviceTypes = [[NSMutableArray alloc] initWithArray:
        @[AVCaptureDeviceTypeBuiltInWideAngleCamera,
          AVCaptureDeviceTypeBuiltInTelephotoCamera,
          AVCaptureDeviceTypeBuiltInUltraWideCamera,
#if PLATFORM(MAC)
          AVCaptureDeviceTypeExternalUnknown,
#endif
        ]
    ];
    if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeDeskViewCamera())
        [deviceTypes addObject:AVCaptureDeviceTypeDeskViewCamera];
    if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInDualWideCamera())
        [deviceTypes addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
    if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInTripleCamera())
        [deviceTypes addObject:AVCaptureDeviceTypeBuiltInTripleCamera];

    return deviceTypes;
}

void AVCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& callback)
{
    if (!m_isInitialized) {
        refreshCaptureDevices([this, callback = WTFMove(callback)]() mutable {
            m_isInitialized = true;
            callback();
        });
        return;
    }
    callback();
}

const Vector<CaptureDevice>& AVCaptureDeviceManager::captureDevices()
{
    ASSERT(m_isInitialized);
    RELEASE_LOG_ERROR_IF(!m_isInitialized, WebRTC, "Retrieving AVCaptureDeviceManager list before initialization");
    return m_devices;
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

RetainPtr<NSArray> AVCaptureDeviceManager::currentCameras()
{
    AVCaptureDeviceDiscoverySession *discoverySession = [PAL::getAVCaptureDeviceDiscoverySessionClass()
        discoverySessionWithDeviceTypes:m_avCaptureDeviceTypes.get()
        mediaType:AVMediaTypeVideo
        position:AVCaptureDevicePositionUnspecified
    ];

    return discoverySession.devices;
}

void AVCaptureDeviceManager::updateCachedAVCaptureDevices()
{
    ASSERT(!isMainThread());
    auto currentDevices = currentCameras();
    auto removedDevices = adoptNS([[NSMutableArray alloc] init]);
    for (AVCaptureDevice *cachedDevice in m_avCaptureDevices.get()) {
        if (![currentDevices containsObject:cachedDevice])
            [removedDevices addObject:cachedDevice];
    }

    if ([removedDevices count]) {
        for (AVCaptureDevice *device in removedDevices.get())
            [device removeObserver:m_objcObserver.get() forKeyPath:@"suspended"];
        [m_avCaptureDevices removeObjectsInArray:removedDevices.get()];
    }

    for (AVCaptureDevice *device in currentDevices.get()) {

        if (![device hasMediaType:AVMediaTypeVideo] && ![device hasMediaType:AVMediaTypeMuxed])
            continue;

        if ([m_avCaptureDevices containsObject:device])
            continue;

        [device addObserver:m_objcObserver.get() forKeyPath:@"suspended" options:NSKeyValueObservingOptionNew context:(void *)nil];
        [m_avCaptureDevices addObject:device];
    }

}

static inline CaptureDevice toCaptureDevice(AVCaptureDevice *device, bool isDefault = false)
{
    CaptureDevice captureDevice { device.uniqueID, CaptureDevice::DeviceType::Camera, device.localizedName };
    captureDevice.setEnabled(deviceIsAvailable(device));
    captureDevice.setIsDefault(isDefault);

#if HAVE(CONTINUITY_CAMEARA)
    if ([PAL::getAVCaptureDeviceClass() respondsToSelector:@selector(systemPreferredCamera)] && [device respondsToSelector:@selector(isContinuityCamera)])
        captureDevice.setIsEphemeral(device.isContinuityCamera && [PAL::getAVCaptureDeviceClass() systemPreferredCamera] != device);
#endif

    return captureDevice;
}

static inline bool isVideoDevice(AVCaptureDevice *device)
{
    return [device hasMediaType:AVMediaTypeVideo] || [device hasMediaType:AVMediaTypeMuxed];
}

Vector<CaptureDevice> AVCaptureDeviceManager::retrieveCaptureDevices()
{
    ASSERT(!isMainThread());
    if (!isAvailable())
        return { };

    if (!m_avCaptureDevices) {
        m_avCaptureDevices = adoptNS([[NSMutableArray alloc] init]);
        registerForDeviceNotifications();
    }

    updateCachedAVCaptureDevices();

    auto currentDevices = currentCameras();
    Vector<CaptureDevice> deviceList;

    auto* defaultVideoDevice = [PAL::getAVCaptureDeviceClass() defaultDeviceWithMediaType: AVMediaTypeVideo];
#if PLATFORM(IOS)
    if ([defaultVideoDevice position] != AVCaptureDevicePositionFront) {
        defaultVideoDevice = nullptr;
        for (AVCaptureDevice *platformDevice in currentDevices.get()) {
            if (!isVideoDevice(platformDevice))
                continue;

            if ([platformDevice position] == AVCaptureDevicePositionFront) {
                defaultVideoDevice = platformDevice;
                break;
            }
        }
    }
#endif

    if (defaultVideoDevice)
        deviceList.append(toCaptureDevice(defaultVideoDevice, true));

    for (AVCaptureDevice *platformDevice in currentDevices.get()) {
        if (isVideoDevice(platformDevice) && platformDevice.uniqueID != defaultVideoDevice.uniqueID)
            deviceList.append(toCaptureDevice(platformDevice));
    }
    return deviceList;
}

void AVCaptureDeviceManager::refreshCaptureDevices(CompletionHandler<void()>&& callback)
{
    m_dispatchQueue->dispatch([this, callback = WTFMove(callback)]() mutable {
        RunLoop::main().dispatch([this, callback = WTFMove(callback), deviceList = crossThreadCopy(retrieveCaptureDevices())]() mutable {
            bool deviceHasChanged = m_devices.size() != deviceList.size();
            if (!deviceHasChanged) {
                for (size_t cptr = 0; cptr < deviceList.size(); ++cptr) {
                    if (m_devices[cptr].persistentId() != deviceList[cptr].persistentId() || m_devices[cptr].enabled() != deviceList[cptr].enabled()) {
                        deviceHasChanged = true;
                        break;
                    }
                }
            }

            if (deviceHasChanged) {
                m_devices = WTFMove(deviceList);
                if (m_isInitialized)
                    deviceChanged();
            }
            callback();
        });
    });
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
    : m_objcObserver(adoptNS([[WebCoreAVCaptureDeviceManagerObserver alloc] initWithCallback:this]))
    , m_avCaptureDeviceTypes(adoptNS(cameraCaptureDeviceTypes()))
    , m_dispatchQueue(WorkQueue::create("com.apple.WebKit.AVCaptureDeviceManager"))
{
}

AVCaptureDeviceManager::~AVCaptureDeviceManager()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver disconnect];
    for (AVCaptureDevice *device in m_avCaptureDevices.get())
        [device removeObserver:m_objcObserver.get() forKeyPath:@"suspended"];
    [PAL::getAVCaptureDeviceClass() removeObserver:m_objcObserver.get() forKeyPath:@"systemPreferredCamera"];
    [PAL::getAVCaptureDeviceDiscoverySessionClass() removeObserver:m_objcObserver.get() forKeyPath:@"devices"];
}

void AVCaptureDeviceManager::registerForDeviceNotifications()
{
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnectedDidChange:) name:AVCaptureDeviceWasConnectedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnectedDidChange:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
    [PAL::getAVCaptureDeviceClass() addObserver:m_objcObserver.get() forKeyPath:@"systemPreferredCamera" options:(NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew) context:nil];
    [PAL::getAVCaptureDeviceDiscoverySessionClass() addObserver:m_objcObserver.get() forKeyPath:@"devices" options:(NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew) context:nil];
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

    RunLoop::main().dispatch([self, protectedSelf = retainPtr(self)] {
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

    if (![keyPath isEqualToString:@"suspended"] && ![keyPath isEqualToString:@"systemPreferredCamera"] && ![keyPath isEqualToString:@"devices"])
        return;

    RunLoop::main().dispatch([self, protectedSelf = retainPtr(self)] {
        if (m_callback)
            m_callback->refreshCaptureDevices();
    });
}

@end

#endif // ENABLE(MEDIA_STREAM)
