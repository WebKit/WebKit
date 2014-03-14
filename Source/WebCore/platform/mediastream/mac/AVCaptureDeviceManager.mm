/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVCaptureDeviceManager.h"

#import "AVAudioCaptureSource.h"
#import "AVMediaCaptureSource.h"
#import "AVVideoCaptureSource.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaStreamSource.h"
#import "MediaStreamSourceStates.h"
#import "NotImplemented.h"
#import "SoftLinking.h"
#import "UUID.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

typedef AVCaptureDevice AVCaptureDeviceType;
typedef AVCaptureSession AVCaptureSessionType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureSession)

#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureSession getAVCaptureSessionClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeMuxed, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPresetLow, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureDeviceWasConnectedNotification, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureDeviceWasDisconnectedNotification, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeMuxed getAVMediaTypeMuxed()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPresetLow getAVCaptureSessionPresetLow()
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

static void refreshCaptureDeviceList();

class CaptureDevice {
public:
    CaptureDevice()
        :m_enabled(false)
    {
    }

    String m_captureDeviceID;

    String m_audioSourceId;
    RefPtr<AVMediaCaptureSource> m_audioSource;

    String m_videoSourceId;
    RefPtr<AVMediaCaptureSource> m_videoSource;

    bool m_enabled;
};

static Vector<CaptureDevice>& captureDeviceList()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Vector<CaptureDevice>, captureDeviceList, ());
    static bool firstTime = true;

    if (firstTime && !captureDeviceList.size()) {
        firstTime = false;
        refreshCaptureDeviceList();
        AVCaptureDeviceManager::shared().registerForDeviceNotifications();
    }

    return captureDeviceList;
}

static bool captureDeviceFromDeviceID(const String& captureDeviceID, CaptureDevice& source)
{
    Vector<CaptureDevice>& devices = captureDeviceList();
    
    size_t count = devices.size();
    for (size_t i = 0; i < count; ++i) {
        if (devices[i].m_captureDeviceID == captureDeviceID) {
            source = devices[i];
            return true;
        }
    }
    
    return false;
}

static void refreshCaptureDeviceList()
{
    Vector<CaptureDevice>& devices = captureDeviceList();
    
    for (AVCaptureDeviceType *device in [AVCaptureDevice devices]) {
        CaptureDevice source;
        
        if (!captureDeviceFromDeviceID(device.uniqueID, source)) {
            // An AVCaptureDevice has a unique ID, but we can't use it for the source ID because:
            // 1. if it provides both audio and video we will need to create two sources for it
            // 2. the unique ID persists on one system across device connections, disconnections,
            //    application restarts, and reboots, so it could be used to figerprint a user.
            source.m_captureDeviceID = device.uniqueID;
            source.m_enabled = true;
            if ([device hasMediaType:AVMediaTypeAudio] || [device hasMediaType:AVMediaTypeMuxed])
                source.m_audioSourceId = createCanonicalUUIDString();

            if ([device hasMediaType:AVMediaTypeVideo] || [device hasMediaType:AVMediaTypeMuxed])
                source.m_videoSourceId = createCanonicalUUIDString();

            devices.append(source);
        } else if (!source.m_enabled) {
            source.m_enabled = true;
            if (source.m_audioSource)
                source.m_audioSource->setEnabled(true);
            if (source.m_videoSource)
                source.m_videoSource->setEnabled(true);
        }
    }
}

bool AVCaptureDeviceManager::isAvailable()
{
    return AVFoundationLibrary();
}

AVCaptureDeviceManager& AVCaptureDeviceManager::shared()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(AVCaptureDeviceManager, manager, ());
    return manager;
}

AVCaptureDeviceManager::AVCaptureDeviceManager()
    : m_objcObserver(adoptNS([[WebCoreAVCaptureDeviceManagerObserver alloc] initWithCallback:this]))
{
}

AVCaptureDeviceManager::~AVCaptureDeviceManager()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver disconnect];
}

String AVCaptureDeviceManager::bestSessionPresetForVideoSize(AVCaptureSessionType *captureSession, int width, int height)
{
    ASSERT(width >= 0);
    ASSERT(height >= 0);
    
    if (width > 1280 || height > 720)
        // FIXME: this restriction could be adjusted with the videoMaxScaleAndCropFactor property.
        return emptyString();
    
    if (width > 640 || height > 480) {
        if (![captureSession canSetSessionPreset:AVCaptureSessionPreset1280x720])
            emptyString();
        return AVCaptureSessionPreset1280x720;
    }
    
    if (width > 352 || height > 288) {
        if (![captureSession canSetSessionPreset:AVCaptureSessionPreset640x480])
            emptyString();
        return AVCaptureSessionPreset640x480;
    }
    
    if ([captureSession canSetSessionPreset:AVCaptureSessionPreset352x288])
        return AVCaptureSessionPreset352x288;
    
    if ([captureSession canSetSessionPreset:AVCaptureSessionPresetLow])
        return AVCaptureSessionPresetLow;
    
    return emptyString();
}

bool AVCaptureDeviceManager::deviceSupportsFacingMode(AVCaptureDeviceType *device, MediaStreamSourceStates::VideoFacingMode facingMode)
{
    if (![device hasMediaType:AVMediaTypeVideo])
        return false;
    
    switch (facingMode) {
    case MediaStreamSourceStates::User:
        if ([device position] == AVCaptureDevicePositionFront)
            return true;
        break;
    case MediaStreamSourceStates::Environment:
        if ([device position] == AVCaptureDevicePositionBack)
            return true;
        break;
    case MediaStreamSourceStates::Left:
    case MediaStreamSourceStates::Right:
    case MediaStreamSourceStates::Unknown:
        return false;
    }
    
    return false;
}

CaptureDevice* AVCaptureDeviceManager::bestDeviceForFacingMode(MediaStreamSourceStates::VideoFacingMode facingMode)
{
    Vector<CaptureDevice>& devices = captureDeviceList();
    
    size_t count = devices.size();
    for (size_t i = 0; i < count; ++i) {
        AVCaptureDeviceType *device = [AVCaptureDevice deviceWithUniqueID:devices[i].m_captureDeviceID];
        ASSERT(device);
        
        if (device && deviceSupportsFacingMode(device, facingMode))
            return &devices[i];
    }
    
    return 0;
}

bool AVCaptureDeviceManager::sessionSupportsConstraint(AVCaptureSessionType *session, MediaStreamSource::Type type, const String& name, const String& value)
{
    size_t constraint = validConstraintNames().find(name);
    if (constraint == notFound)
        return false;
    
    switch (constraint) {
    case Width:
        if (type == MediaStreamSource::Audio)
            return false;

        return !bestSessionPresetForVideoSize(session, value.toInt(), 0).isEmpty();
    case Height:
        if (type == MediaStreamSource::Audio)
            return false;

        return !bestSessionPresetForVideoSize(session, 0, value.toInt()).isEmpty();
    case FrameRate: {
        if (type == MediaStreamSource::Audio)
            return false;
        
        // It would make sense to use [AVCaptureConnection videoMinFrameDuration] and
        // [AVCaptureConnection videoMaxFrameDuration], but they only work with a "live" AVCaptureConnection.
        float rate = value.toFloat();
        return rate > 0 && rate <= 60;
    }
    case Gain: {
        if (type != MediaStreamSource::Audio)
            return false;
        
        float level = value.toFloat();
        return level > 0 && level <= 1;
    }
    case FacingMode: {
        if (type == MediaStreamSource::Audio)
            return false;

        size_t facingMode =  validFacingModes().find(value);
        if (facingMode != notFound)
            return false;
        return bestDeviceForFacingMode(static_cast<MediaStreamSourceStates::VideoFacingMode>(facingMode));
    }
    }
    
    return false;
}

bool AVCaptureDeviceManager::isValidConstraint(MediaStreamSource::Type type, const String& name)
{
    size_t constraint = validConstraintNames().find(name);
    if (constraint == notFound)
        return false;

    if (constraint == Gain)
        return type == MediaStreamSource::Audio;

    return true;
}

Vector<RefPtr<TrackSourceInfo>> AVCaptureDeviceManager::getSourcesInfo(const String& requestOrigin)
{
    UNUSED_PARAM(requestOrigin);
    Vector<RefPtr<TrackSourceInfo>> sourcesInfo;

    if (!isAvailable())
        return sourcesInfo;

    Vector<CaptureDevice>& devices = captureDeviceList();
    size_t count = devices.size();
    for (size_t i = 0; i < count; ++i) {
        AVCaptureDeviceType *device = [AVCaptureDevice deviceWithUniqueID:devices[i].m_captureDeviceID];
        ASSERT(device);

        if (!devices[i].m_enabled)
            continue;

        if (devices[i].m_videoSource)
            sourcesInfo.append(TrackSourceInfo::create(devices[i].m_videoSourceId, TrackSourceInfo::Video, device.localizedName));
        if (devices[i].m_audioSource)
            sourcesInfo.append(TrackSourceInfo::create(devices[i].m_audioSourceId, TrackSourceInfo::Audio, device.localizedName));
    }
    
    LOG(Media, "AVCaptureDeviceManager::getSourcesInfo(%p), found %d active devices", this, sourcesInfo.size());

    return sourcesInfo;
}

bool AVCaptureDeviceManager::verifyConstraintsForMediaType(MediaStreamSource::Type type, MediaConstraints* constraints, String& invalidConstraint)
{
    if (!isAvailable())
        return false;

    if (!constraints)
        return true;

    Vector<MediaConstraint> mandatoryConstraints;
    constraints->getMandatoryConstraints(mandatoryConstraints);
    if (mandatoryConstraints.size()) {
        RetainPtr<AVCaptureSessionType> session = adoptNS([[AVCaptureSession alloc] init]);
        for (size_t i = 0; i < mandatoryConstraints.size(); ++i) {
            const MediaConstraint& constraint = mandatoryConstraints[i];
            if (!sessionSupportsConstraint(session.get(), type, constraint.m_name, constraint.m_value)) {
                invalidConstraint = constraint.m_name;
                return false;
            }
        }
    }
    
    Vector<MediaConstraint> optionalConstraints;
    constraints->getOptionalConstraints(optionalConstraints);
    if (!optionalConstraints.size())
        return true;

    for (size_t i = 0; i < optionalConstraints.size(); ++i) {
        const MediaConstraint& constraint = optionalConstraints[i];
        if (!isValidConstraint(type, constraint.m_name)) {
            invalidConstraint = constraint.m_name;
            return false;
        }
    }

    return true;
}

RefPtr<MediaStreamSource> AVCaptureDeviceManager::bestSourceForTypeAndConstraints(MediaStreamSource::Type type, PassRefPtr<MediaConstraints> constraints)
{
    if (!isAvailable())
        return 0;

    Vector<CaptureDevice>& devices = captureDeviceList();
    size_t count = devices.size();
    for (size_t i = 0; i < count; ++i) {
        if (!devices[i].m_enabled)
            continue;

        // FIXME: consider the constraints when choosing among multiple devices. For now just select the first available
        // device of the appropriate type.
        if (type == MediaStreamSource::Audio && !devices[i].m_audioSourceId.isEmpty()) {
            if (!devices[i].m_audioSource) {
                AVCaptureDeviceType *device = [AVCaptureDevice deviceWithUniqueID:devices[i].m_captureDeviceID];
                ASSERT(device);
                devices[i].m_audioSource = AVAudioCaptureSource::create(device, devices[i].m_audioSourceId, constraints);
            }
            devices[i].m_audioSource->setReadyState(MediaStreamSource::Live);
            return devices[i].m_audioSource;
        }

        if (type == MediaStreamSource::Video && !devices[i].m_videoSourceId.isEmpty()) {
            if (!devices[i].m_videoSource) {
                AVCaptureDeviceType *device = [AVCaptureDevice deviceWithUniqueID:devices[i].m_captureDeviceID];
                ASSERT(device);
                devices[i].m_videoSource = AVVideoCaptureSource::create(device, devices[i].m_videoSourceId, constraints);
            }
            devices[i].m_videoSource->setReadyState(MediaStreamSource::Live);
            return devices[i].m_videoSource;
        }
    }

    return 0;
}

void AVCaptureDeviceManager::registerForDeviceNotifications()
{
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceConnected:) name:AVCaptureDeviceWasConnectedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(deviceDisconnected:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
}

void AVCaptureDeviceManager::deviceConnected()
{
    refreshCaptureDeviceList();
}

void AVCaptureDeviceManager::deviceDisconnected(AVCaptureDeviceType* device)
{
    Vector<CaptureDevice>& devices = captureDeviceList();

    size_t count = devices.size();
    if (!count)
        return;

    String deviceID = device.uniqueID;
    for (size_t i = 0; i < count; ++i) {
        if (devices[i].m_captureDeviceID == deviceID) {
            LOG(Media, "AVCaptureDeviceManager::deviceDisconnected(%p), device %d disabled", this, i);
            devices[i].m_enabled = false;
            if (devices[i].m_audioSource)
                devices[i].m_audioSource->setEnabled(false);
            if (devices[i].m_videoSource)
                devices[i].m_videoSource->setEnabled(false);
        }
    }
}

const Vector<AtomicString>& AVCaptureDeviceManager::validConstraintNames()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Vector<AtomicString>, constraints, ());
    static NeverDestroyed<AtomicString> heightConstraint("height", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> widthConstraint("width", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> frameRateConstraint("frameRate", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> facingModeConstraint("facingMode", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> gainConstraint("gain", AtomicString::ConstructFromLiteral);
    
    if (!constraints.size()) {
        constraints.insert(Width, widthConstraint);
        constraints.insert(Height, heightConstraint);
        constraints.insert(FrameRate, frameRateConstraint);
        constraints.insert(FacingMode, facingModeConstraint);
        constraints.insert(Gain, gainConstraint);
    }
    
    return constraints;
}

const Vector<AtomicString>& AVCaptureDeviceManager::validFacingModes()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(Vector<AtomicString>, modes, ());
    
    if (!modes.size()) {
        modes.insert(MediaStreamSourceStates::User, MediaStreamSourceStates::facingMode(MediaStreamSourceStates::User));
        modes.insert(MediaStreamSourceStates::Environment, MediaStreamSourceStates::facingMode(MediaStreamSourceStates::Environment));
        modes.insert(MediaStreamSourceStates::Left, MediaStreamSourceStates::facingMode(MediaStreamSourceStates::Left));
        modes.insert(MediaStreamSourceStates::Right, MediaStreamSourceStates::facingMode(MediaStreamSourceStates::Right));
    }
    
    return modes;
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
            AVCaptureDeviceType *device = [notification object];
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
