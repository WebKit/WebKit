/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "UserMediaPermissionRequestManagerProxy.h"

#import "MediaPermissionUtilities.h"
#import "SandboxUtilities.h"
#import "UserMediaCaptureManagerProxy.h"
#import "WKWebView.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <WebCore/VideoFrame.h>
#import <pal/spi/cocoa/TCCSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if ENABLE(MEDIA_STREAM) && HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)

static WebCore::VideoFrameRotation computeVideoFrameRotation(int rotation)
{
    switch (rotation) {
    case 0:
        return WebCore::VideoFrame::Rotation::None;
    case 180:
        return WebCore::VideoFrame::Rotation::UpsideDown;
    case 90:
        return WebCore::VideoFrame::Rotation::Right;
    case 270:
        return WebCore::VideoFrame::Rotation::Left;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::VideoFrame::Rotation::None;
    }

    RELEASE_LOG_ERROR(WebRTC, "Unknown video frame rotation value: %d", rotation);
    return WebCore::VideoFrame::Rotation::None;
}

@interface WKRotationCoordinatorObserver : NSObject {
    WeakPtr<WebKit::UserMediaPermissionRequestManagerProxy> _managerProxy;
    HashMap<String, RetainPtr<AVCaptureDeviceRotationCoordinator>> m_coordinators;
}

-(id)initWithRequestManagerProxy:(WeakPtr<WebKit::UserMediaPermissionRequestManagerProxy>&&)managerProxy;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context;
-(bool)isMonitoringCaptureDeviceRotation:(const String&)persistentId;
-(std::optional<WebCore::VideoFrameRotation>)start:(const String&)persistentId layer:(CALayer*)layer;
-(void)stop:(const String&)persistentId;
@end

@implementation WKRotationCoordinatorObserver

- (id)initWithRequestManagerProxy:(WeakPtr<WebKit::UserMediaPermissionRequestManagerProxy>&&)managerProxy {
    if ((self = [super init]))
        _managerProxy = WTFMove(managerProxy);
    return self;
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context {
    UNUSED_PARAM(context);
    UNUSED_PARAM(change);

    if (!_managerProxy)
        return;

    if (![keyPath isEqualToString:@"videoRotationAngleForHorizonLevelPreview"])
        return;

    AVCaptureDeviceRotationCoordinator* coordinator = (AVCaptureDeviceRotationCoordinator*)object;
    String persistentId = [coordinator device].uniqueID;
    auto rotation = computeVideoFrameRotation(clampToInteger([coordinator videoRotationAngleForHorizonLevelPreview]));

    RunLoop::main().dispatch([protectedSelf = retainPtr(self), self, persistentId = WTFMove(persistentId).isolatedCopy(), rotation] {
        if (_managerProxy)
            _managerProxy->rotationAngleForCaptureDeviceChanged(persistentId, rotation);
    });
}

-(bool)isMonitoringCaptureDeviceRotation:(const String&)persistentId {
    return m_coordinators.contains(persistentId);
}

-(std::optional<WebCore::VideoFrameRotation>)start:(const String&)persistentId layer:(CALayer*)layer {
    auto iterator = m_coordinators.add(persistentId, RetainPtr<AVCaptureDeviceRotationCoordinator> { }).iterator;
    if (!iterator->value) {
        if (!PAL::getAVCaptureDeviceRotationCoordinatorClass())
            return { };

        RetainPtr avDevice = [PAL::getAVCaptureDeviceClass() deviceWithUniqueID:persistentId];
        if (!avDevice)
            return { };

        RetainPtr coordinator = adoptNS([PAL::allocAVCaptureDeviceRotationCoordinatorInstance() initWithDevice:avDevice.get() previewLayer:layer]);
        [coordinator addObserver:self forKeyPath:@"videoRotationAngleForHorizonLevelPreview" options:NSKeyValueObservingOptionNew context:(void *)nil];

        iterator->value = WTFMove(coordinator);
    }

    return computeVideoFrameRotation((clampToInteger([iterator->value videoRotationAngleForHorizonLevelPreview])));
}

-(void)stop:(const String&)persistentId {
    if (auto coordinator = m_coordinators.take(persistentId))
        [coordinator removeObserver:self forKeyPath:@"videoRotationAngleForHorizonLevelPreview"];
}

@end

#endif // ENABLE(MEDIA_STREAM) && HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)

namespace WebKit {

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureAudio()
{
#if ENABLE(MEDIA_STREAM)
    return checkSandboxRequirementForType(MediaPermissionType::Audio) && checkUsageDescriptionStringForType(MediaPermissionType::Audio);
#else
    return false;
#endif
}

bool UserMediaPermissionRequestManagerProxy::permittedToCaptureVideo()
{
#if ENABLE(MEDIA_STREAM)
    return checkSandboxRequirementForType(MediaPermissionType::Video) && checkUsageDescriptionStringForType(MediaPermissionType::Video);
#else
    return false;
#endif
}

#if ENABLE(MEDIA_STREAM)
void UserMediaPermissionRequestManagerProxy::requestSystemValidation(const WebPageProxy& page, UserMediaPermissionRequestProxy& request, CompletionHandler<void(bool)>&& callback)
{
    if (page.preferences().mockCaptureDevicesEnabled()) {
        callback(true);
        return;
    }

    // FIXME: Add TCC entitlement check for screensharing.
    auto audioStatus = request.requiresAudioCapture() ? checkAVCaptureAccessForType(MediaPermissionType::Audio) : MediaPermissionResult::Granted;
    if (audioStatus == MediaPermissionResult::Denied) {
        callback(false);
        return;
    }

    auto videoStatus = request.requiresVideoCapture() ? checkAVCaptureAccessForType(MediaPermissionType::Video) : MediaPermissionResult::Granted;
    if (videoStatus == MediaPermissionResult::Denied) {
        callback(false);
        return;
    }

    if (audioStatus == MediaPermissionResult::Unknown) {
        requestAVCaptureAccessForType(MediaPermissionType::Audio, [videoStatus, completionHandler = WTFMove(callback)](bool authorized) mutable {
            if (videoStatus == MediaPermissionResult::Granted) {
                completionHandler(authorized);
                return;
            }
                
            requestAVCaptureAccessForType(MediaPermissionType::Video, WTFMove(completionHandler));
        });
        return;
    }

    if (videoStatus == MediaPermissionResult::Unknown) {
        requestAVCaptureAccessForType(MediaPermissionType::Video, WTFMove(callback));
        return;
    }

    callback(true);
}

#if ENABLE(MEDIA_STREAM) && HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)
bool UserMediaPermissionRequestManagerProxy::isMonitoringCaptureDeviceRotation(const String& persistentId)
{
    if (persistentId.isEmpty())
        return false;
    RetainPtr observer = m_objcObserver;
    return [observer isMonitoringCaptureDeviceRotation:persistentId];
}

void UserMediaPermissionRequestManagerProxy::startMonitoringCaptureDeviceRotation(const String& persistentId)
{
    RefPtr page = this->page();
    if (!page)
        return;

    RetainPtr webView = page->cocoaView();
    auto *layer = [webView layer];
    if (!layer) {
        RELEASE_LOG_ERROR(WebRTC, "UserMediaPermissionRequestManagerProxy unable to start monitoring capture device rotation");
        return;
    }

    if (!m_objcObserver)
        m_objcObserver = adoptNS([[WKRotationCoordinatorObserver alloc] initWithRequestManagerProxy:*this]);

    if (auto currentRotation = [m_objcObserver start:persistentId layer:layer])
        rotationAngleForCaptureDeviceChanged(persistentId, *currentRotation);
}

void UserMediaPermissionRequestManagerProxy::stopMonitoringCaptureDeviceRotation(const String& persistentId)
{
    [m_objcObserver stop:persistentId];
}

void UserMediaPermissionRequestManagerProxy::rotationAngleForCaptureDeviceChanged(const String& persistentId, WebCore::VideoFrameRotation rotation)
{
    if (RefPtr page = this->page())
        page->rotationAngleForCaptureDeviceChanged(persistentId, rotation);
}
#endif // HAVE(AVCAPTUREDEVICEROTATIONCOORDINATOR)

#endif // ENABLE(MEDIA_STREAM)

} // namespace WebKit
