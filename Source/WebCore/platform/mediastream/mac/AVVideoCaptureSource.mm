/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#import "AVVideoCaptureSource.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "ImageBuffer.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceCenterMac.h"
#import "RealtimeMediaSourceSettings.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVCaptureInput.h>
#import <AVFoundation/AVCaptureOutput.h>
#import <AVFoundation/AVCaptureSession.h>
#import <AVFoundation/AVError.h>
#import <objc/runtime.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceTypedef;
typedef AVCaptureDeviceFormat AVCaptureDeviceFormatType;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureVideoDataOutput AVCaptureVideoDataOutputType;
typedef AVFrameRateRange AVFrameRateRangeType;
typedef AVCaptureSession AVCaptureSessionType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceFormat)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVFrameRateRange)
SOFT_LINK_CLASS(AVFoundation, AVCaptureSession)

#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceFormat getAVCaptureDeviceFormatClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVCaptureVideoDataOutput getAVCaptureVideoDataOutputClass()
#define AVFrameRateRange getAVFrameRateRangeClass()

SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset960x540, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset320x240, NSString *)

#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset960x540 getAVCaptureSessionPreset960x540()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPreset320x240 getAVCaptureSessionPreset320x240()

#if PLATFORM(IOS)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset3840x2160, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVCaptureSessionPreset1920x1080, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionRuntimeErrorNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionWasInterruptedNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionInterruptionEndedNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionInterruptionReasonKey, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionErrorKey, NSString *)

#define AVCaptureSessionPreset3840x2160 getAVCaptureSessionPreset3840x2160()
#define AVCaptureSessionPreset1920x1080 getAVCaptureSessionPreset1920x1080()
#define AVCaptureSessionRuntimeErrorNotification getAVCaptureSessionRuntimeErrorNotification()
#define AVCaptureSessionWasInterruptedNotification getAVCaptureSessionWasInterruptedNotification()
#define AVCaptureSessionInterruptionEndedNotification getAVCaptureSessionInterruptionEndedNotification()
#define AVCaptureSessionInterruptionReasonKey getAVCaptureSessionInterruptionReasonKey()
#define AVCaptureSessionErrorKey getAVCaptureSessionErrorKey()
#endif

using namespace WebCore;

@interface WebCoreAVVideoCaptureSourceObserver : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
    AVVideoCaptureSource* m_callback;
}

-(id)initWithCallback:(AVVideoCaptureSource*)callback;
-(void)disconnect;
-(void)addNotificationObservers;
-(void)removeNotificationObservers;
-(void)captureOutput:(AVCaptureOutputType*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType*)connection;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context;
#if PLATFORM(IOS)
-(void)sessionRuntimeError:(NSNotification*)notification;
-(void)beginSessionInterrupted:(NSNotification*)notification;
-(void)endSessionInterrupted:(NSNotification*)notification;
#endif
@end

namespace WebCore {

#if PLATFORM(MAC)
const OSType videoCaptureFormat = kCVPixelFormatType_420YpCbCr8Planar;
#else
const OSType videoCaptureFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
#endif

static dispatch_queue_t globaVideoCaptureSerialQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create_with_target("WebCoreAVVideoCaptureSource video capture queue", DISPATCH_QUEUE_SERIAL, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    });
    return globalQueue;
}

CaptureSourceOrError AVVideoCaptureSource::create(const AtomicString& id, const MediaConstraints* constraints)
{
    AVCaptureDeviceTypedef *device = [getAVCaptureDeviceClass() deviceWithUniqueID:id];
    if (!device)
        return { };

    auto source = adoptRef(*new AVVideoCaptureSource(device, id));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return WTFMove(result.value().first);
    }

    return CaptureSourceOrError(WTFMove(source));
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDeviceTypedef* device, const AtomicString& id)
    : RealtimeMediaSource(id, Type::Video, device.localizedName)
    , m_objcObserver(adoptNS([[WebCoreAVVideoCaptureSourceObserver alloc] initWithCallback:this]))
    , m_device(device)
{
    struct VideoPreset {
        bool symbolAvailable;
        NSString* name;
        int width;
        int height;
    };

    static const VideoPreset presets[] = {
#if PLATFORM(IOS)
        { canLoadAVCaptureSessionPreset3840x2160(), AVCaptureSessionPreset3840x2160, 3840, 2160  },
        { canLoadAVCaptureSessionPreset1920x1080(), AVCaptureSessionPreset1920x1080, 1920, 1080 },
#endif
        { canLoadAVCaptureSessionPreset1280x720(), AVCaptureSessionPreset1280x720, 1280, 720 },
        { canLoadAVCaptureSessionPreset960x540(), AVCaptureSessionPreset960x540, 960, 540 },
        { canLoadAVCaptureSessionPreset640x480(), AVCaptureSessionPreset640x480, 640, 480 },
        { canLoadAVCaptureSessionPreset352x288(), AVCaptureSessionPreset352x288, 352, 288 },
        { canLoadAVCaptureSessionPreset320x240(), AVCaptureSessionPreset320x240, 320, 240 },
    };

    auto* presetsMap = &videoPresets();
    for (auto& preset : presets) {
        if (!preset.symbolAvailable || !preset.name || ![device supportsAVCaptureSessionPreset:preset.name])
            continue;

        presetsMap->add(String(preset.name), IntSize(preset.width, preset.height));
    }

#if PLATFORM(IOS)
    static_assert(static_cast<int>(InterruptionReason::VideoNotAllowedInBackground) == static_cast<int>(AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground), "InterruptionReason::VideoNotAllowedInBackground is not AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground as expected");
    static_assert(static_cast<int>(InterruptionReason::VideoNotAllowedInSideBySide) == AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps, "InterruptionReason::VideoNotAllowedInSideBySide is not AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps as expected");
    static_assert(static_cast<int>(InterruptionReason::VideoInUse) == AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient, "InterruptionReason::VideoInUse is not AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient as expected");
    static_assert(static_cast<int>(InterruptionReason::AudioInUse) == AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient, "InterruptionReason::AudioInUse is not AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient as expected");
#endif

    setPersistentID(String(device.uniqueID));
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenterMac::videoCaptureSourceFactory().unsetActiveSource(*this);
#endif
    [m_objcObserver disconnect];

    if (!m_session)
        return;

    [m_session removeObserver:m_objcObserver.get() forKeyPath:@"rate"];
    if ([m_session isRunning])
        [m_session stopRunning];

}

void AVVideoCaptureSource::startProducingData()
{
    if (!m_session) {
        if (!setupSession())
            return;
    }

    if ([m_session isRunning])
        return;

    [m_objcObserver addNotificationObservers];
    [m_session startRunning];
}

void AVVideoCaptureSource::stopProducingData()
{
    if (!m_session)
        return;

    [m_objcObserver removeNotificationObservers];

    if ([m_session isRunning])
        [m_session stopRunning];

    m_interruption = InterruptionReason::None;
#if PLATFORM(IOS)
    m_session = nullptr;
#endif
}

static void updateSizeMinMax(int& min, int& max, int value)
{
    min = std::min<int>(min, value);
    max = std::max<int>(max, value);
}

static void updateAspectRatioMinMax(double& min, double& max, double value)
{
    min = std::min<double>(min, value);
    max = std::max<double>(max, value);
}

void AVVideoCaptureSource::beginConfiguration()
{
    if (m_session)
        [m_session beginConfiguration];
}

void AVVideoCaptureSource::commitConfiguration()
{
    if (m_session)
        [m_session commitConfiguration];
}

void AVVideoCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    m_currentSettings = std::nullopt;
    RealtimeMediaSource::settingsDidChange(settings);
}

const RealtimeMediaSourceSettings& AVVideoCaptureSource::settings() const
{
    if (m_currentSettings)
        return *m_currentSettings;

    RealtimeMediaSourceSettings settings;
    if ([device() position] == AVCaptureDevicePositionFront)
        settings.setFacingMode(RealtimeMediaSourceSettings::User);
    else if ([device() position] == AVCaptureDevicePositionBack)
        settings.setFacingMode(RealtimeMediaSourceSettings::Environment);
    else
        settings.setFacingMode(RealtimeMediaSourceSettings::Unknown);

    auto maxFrameDuration = [device() activeVideoMaxFrameDuration];
    settings.setFrameRate(maxFrameDuration.timescale / maxFrameDuration.value);
    settings.setWidth(m_width);
    settings.setHeight(m_height);
    settings.setDeviceId(id());

    RealtimeMediaSourceSupportedConstraints supportedConstraints;
    supportedConstraints.setSupportsDeviceId(true);
    supportedConstraints.setSupportsFacingMode([device() position] != AVCaptureDevicePositionUnspecified);
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsFrameRate(true);

    settings.setSupportedConstraints(supportedConstraints);

    m_currentSettings = WTFMove(settings);

    return *m_currentSettings;
}

const RealtimeMediaSourceCapabilities& AVVideoCaptureSource::capabilities() const
{
    if (m_capabilities)
        return *m_capabilities;

    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
    capabilities.setDeviceId(id());
    AVCaptureDeviceTypedef *videoDevice = device();

    if ([videoDevice position] == AVCaptureDevicePositionFront)
        capabilities.addFacingMode(RealtimeMediaSourceSettings::User);
    if ([videoDevice position] == AVCaptureDevicePositionBack)
        capabilities.addFacingMode(RealtimeMediaSourceSettings::Environment);

    Float64 lowestFrameRateRange = std::numeric_limits<double>::infinity();
    Float64 highestFrameRateRange = 0;
    int minimumWidth = std::numeric_limits<int>::infinity();
    int maximumWidth = 0;
    int minimumHeight = std::numeric_limits<int>::infinity();
    int maximumHeight = 0;
    double minimumAspectRatio = std::numeric_limits<double>::infinity();
    double maximumAspectRatio = 0;

    for (AVCaptureDeviceFormatType *format in [videoDevice formats]) {

        for (AVFrameRateRangeType *range in [format videoSupportedFrameRateRanges]) {
            lowestFrameRateRange = std::min<Float64>(lowestFrameRateRange, range.minFrameRate);
            highestFrameRateRange = std::max<Float64>(highestFrameRateRange, range.maxFrameRate);
        }
    }

    for (auto& preset : m_supportedPresets) {
        auto values = preset.value;
        updateSizeMinMax(minimumWidth, maximumWidth, values.width());
        updateSizeMinMax(minimumHeight, maximumHeight, values.height());
        updateAspectRatioMinMax(minimumAspectRatio, maximumAspectRatio, static_cast<double>(values.width()) / values.height());
    }
    capabilities.setFrameRate(CapabilityValueOrRange(lowestFrameRateRange, highestFrameRateRange));
    capabilities.setWidth(CapabilityValueOrRange(minimumWidth, maximumWidth));
    capabilities.setHeight(CapabilityValueOrRange(minimumHeight, maximumHeight));
    capabilities.setAspectRatio(CapabilityValueOrRange(minimumAspectRatio, maximumAspectRatio));

    m_capabilities = WTFMove(capabilities);

    return *m_capabilities;
}

IntSize AVVideoCaptureSource::sizeForPreset(NSString* preset)
{
    if (!preset)
        return { };

    auto& presets = videoPresets();
    auto it = presets.find(String(preset));
    if (it != presets.end())
        return { it->value.width(), it->value.height() };

    return { };
}

bool AVVideoCaptureSource::setPreset(NSString *preset)
{
    if (!session()) {
        m_pendingPreset = preset;
        return true;
    }

    auto size = sizeForPreset(preset);
    if (m_presetSize == size)
        return true;

    m_presetSize = size;

    @try {
        session().sessionPreset = preset;
#if PLATFORM(MAC)
        auto settingsDictionary = @{
            (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(videoCaptureFormat),
            (__bridge NSString *)kCVPixelBufferWidthKey: @(size.width()),
            (__bridge NSString *)kCVPixelBufferHeightKey: @(size.height())
        };
        [m_videoOutput setVideoSettings:settingsDictionary];
#endif
    } @catch(NSException *exception) {
        LOG(Media, "AVVideoCaptureSource::setPreset(%p), exception thrown configuring device: <%s> %s", this, [[exception name] UTF8String], [[exception reason] UTF8String]);
        return false;
    }

    return true;
}

void AVVideoCaptureSource::setFrameRate(double rate)
{
    using namespace PAL;
    double epsilon = 0.00001;
    AVFrameRateRangeType *bestFrameRateRange = nil;
    for (AVFrameRateRangeType *frameRateRange in [[device() activeFormat] videoSupportedFrameRateRanges]) {
        if (rate + epsilon >= [frameRateRange minFrameRate] && rate - epsilon <= [frameRateRange maxFrameRate]) {
            if (!bestFrameRateRange || CMTIME_COMPARE_INLINE([frameRateRange minFrameDuration], >, [bestFrameRateRange minFrameDuration]))
                bestFrameRateRange = frameRateRange;
        }
    }

    if (!bestFrameRateRange || !isFrameRateSupported(rate)) {
        LOG(Media, "AVVideoCaptureSource::setFrameRate(%p), frame rate %f not supported by video device", this, rate);
        return;
    }

    NSError *error = nil;
    @try {
        if ([device() lockForConfiguration:&error]) {
            if (bestFrameRateRange.minFrameRate == bestFrameRateRange.maxFrameRate) {
                [device() setActiveVideoMinFrameDuration:[bestFrameRateRange minFrameDuration]];
                [device() setActiveVideoMaxFrameDuration:[bestFrameRateRange maxFrameDuration]];
            } else {
                [device() setActiveVideoMinFrameDuration: CMTimeMake(1, rate)];
                [device() setActiveVideoMaxFrameDuration: CMTimeMake(1, rate)];
            }
            [device() unlockForConfiguration];
        }
    } @catch(NSException *exception) {
        LOG(Media, "AVVideoCaptureSource::setFrameRate(%p), exception thrown configuring device: <%s> %s", this, [[exception name] UTF8String], [[exception reason] UTF8String]);
        return;
    }

    if (error) {
        LOG(Media, "AVVideoCaptureSource::setFrameRate(%p), failed to lock video device for configuration: %s", this, [[error localizedDescription] UTF8String]);
        return;
    }

    LOG(Media, "AVVideoCaptureSource::setFrameRate(%p) - set frame rate range to %f", this, rate);
    return;
}

void AVVideoCaptureSource::applySizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (width || height)
        setPreset(bestSessionPresetForVideoDimensions(WTFMove(width), WTFMove(height)));

    if (frameRate)
        setFrameRate(frameRate.value());
}

static inline int sensorOrientation(AVCaptureVideoOrientation videoOrientation)
{
#if PLATFORM(IOS)
    switch (videoOrientation) {
    case AVCaptureVideoOrientationPortrait:
        return 180;
    case AVCaptureVideoOrientationPortraitUpsideDown:
        return 0;
    case AVCaptureVideoOrientationLandscapeRight:
        return 90;
    case AVCaptureVideoOrientationLandscapeLeft:
        return -90;
    }
#else
    switch (videoOrientation) {
    case AVCaptureVideoOrientationPortrait:
        return 0;
    case AVCaptureVideoOrientationPortraitUpsideDown:
        return 180;
    case AVCaptureVideoOrientationLandscapeRight:
        return 90;
    case AVCaptureVideoOrientationLandscapeLeft:
        return -90;
    }
#endif
}

static inline int sensorOrientationFromVideoOutput(AVCaptureVideoDataOutputType* videoOutput)
{
    AVCaptureConnectionType* connection = [videoOutput connectionWithMediaType: getAVMediaTypeVideo()];
    return connection ? sensorOrientation([connection videoOrientation]) : 0;
}

bool AVVideoCaptureSource::setupSession()
{
    if (m_session)
        return true;

    m_session = adoptNS([allocAVCaptureSessionInstance() init]);
    [m_session addObserver:m_objcObserver.get() forKeyPath:@"rate" options:NSKeyValueObservingOptionNew context:(void *)nil];

    [m_session beginConfiguration];
    bool success = setupCaptureSession();
    [m_session commitConfiguration];

    if (!success)
        captureFailed();

    return success;
}

bool AVVideoCaptureSource::setupCaptureSession()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenterMac::videoCaptureSourceFactory().setActiveSource(*this);
#endif

    NSError *error = nil;
    RetainPtr<AVCaptureDeviceInputType> videoIn = adoptNS([allocAVCaptureDeviceInputInstance() initWithDevice:device() error:&error]);
    if (error) {
        RELEASE_LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), failed to allocate AVCaptureDeviceInput: %s", this, [[error localizedDescription] UTF8String]);
        return false;
    }

    if (![session() canAddInput:videoIn.get()]) {
        RELEASE_LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video input device", this);
        return false;
    }
    [session() addInput:videoIn.get()];

    m_videoOutput = adoptNS([allocAVCaptureVideoDataOutputInstance() init]);
    auto settingsDictionary = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys: [NSNumber numberWithInt:videoCaptureFormat], kCVPixelBufferPixelFormatTypeKey, nil]);
    if (m_pendingPreset) {
#if PLATFORM(MAC)
        auto size = sizeForPreset(m_pendingPreset.get());
        [settingsDictionary setObject:@(size.width()) forKey:(__bridge NSString *)kCVPixelBufferWidthKey];
        [settingsDictionary setObject:@(size.height()) forKey:(__bridge NSString *)kCVPixelBufferHeightKey];
#endif
    }

    [m_videoOutput setVideoSettings:settingsDictionary.get()];
    [m_videoOutput setAlwaysDiscardsLateVideoFrames:YES];
    [m_videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];

    if (![session() canAddOutput:m_videoOutput.get()]) {
        RELEASE_LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video sample buffer output delegate", this);
        return false;
    }
    [session() addOutput:m_videoOutput.get()];

#if PLATFORM(IOS)
    setPreset(m_pendingPreset.get());
#endif

    m_sensorOrientation = sensorOrientationFromVideoOutput(m_videoOutput.get());
    computeSampleRotation();

    return true;
}

void AVVideoCaptureSource::shutdownCaptureSession()
{
    m_buffer = nullptr;
    m_width = 0;
    m_height = 0;
}

void AVVideoCaptureSource::monitorOrientation(OrientationNotifier& notifier)
{
#if PLATFORM(IOS)
    notifier.addObserver(*this);
    orientationChanged(notifier.orientation());
#else
    UNUSED_PARAM(notifier);
#endif
}

void AVVideoCaptureSource::orientationChanged(int orientation)
{
    ASSERT(orientation == 0 || orientation == 90 || orientation == -90 || orientation == 180);
    m_deviceOrientation = orientation;
    computeSampleRotation();
}

void AVVideoCaptureSource::computeSampleRotation()
{
    bool frontCamera = [device() position] == AVCaptureDevicePositionFront;
    switch (m_sensorOrientation - m_deviceOrientation) {
    case 0:
        m_sampleRotation = MediaSample::VideoRotation::None;
        break;
    case 180:
    case -180:
        m_sampleRotation = MediaSample::VideoRotation::UpsideDown;
        break;
    case 90:
        m_sampleRotation = frontCamera ? MediaSample::VideoRotation::Left : MediaSample::VideoRotation::Right;
        break;
    case -90:
    case -270:
        m_sampleRotation = frontCamera ? MediaSample::VideoRotation::Right : MediaSample::VideoRotation::Left;
        break;
    default:
        ASSERT_NOT_REACHED();
        m_sampleRotation = MediaSample::VideoRotation::None;
    }
}

void AVVideoCaptureSource::processNewFrame(RetainPtr<CMSampleBufferRef> sampleBuffer, RetainPtr<AVCaptureConnectionType> connection)
{
    if (!isProducingData() || muted())
        return;

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer.get());
    if (!formatDescription)
        return;

    m_buffer = sampleBuffer;
    CMVideoDimensions dimensions = PAL::CMVideoFormatDescriptionGetDimensions(formatDescription);
    if (m_sampleRotation == MediaSample::VideoRotation::Left || m_sampleRotation == MediaSample::VideoRotation::Right)
        std::swap(dimensions.width, dimensions.height);

    if (dimensions.width != m_width || dimensions.height != m_height) {
        OptionSet<RealtimeMediaSourceSettings::Flag> changed;
        if (m_width != dimensions.width)
            changed.add(RealtimeMediaSourceSettings::Flag::Width);
        if (m_height != dimensions.height)
            changed.add(RealtimeMediaSourceSettings::Flag::Height);

        m_width = dimensions.width;
        m_height = dimensions.height;
        settingsDidChange(changed);
    }

    videoSampleAvailable(MediaSampleAVFObjC::create(m_buffer.get(), m_sampleRotation, [connection isVideoMirrored]));
}

void AVVideoCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType* captureConnection)
{
    RetainPtr<CMSampleBufferRef> buffer = sampleBuffer;
    RetainPtr<AVCaptureConnectionType> connection = captureConnection;

    scheduleDeferredTask([this, buffer, connection] {
        this->processNewFrame(buffer, connection);
    });
}

NSString* AVVideoCaptureSource::bestSessionPresetForVideoDimensions(std::optional<int> width, std::optional<int> height)
{
    if (!width && !height)
        return nil;

    int widthValue = width ? width.value() : 0;
    int heightValue = height ? height.value() : 0;

    for (auto& preset : videoPresets()) {
        auto size = preset.value;
        NSString* name = preset.key;

        if ((!widthValue || widthValue == size.width()) && (!heightValue || heightValue == size.height()))
            return name;
    }

    return nil;
}

bool AVVideoCaptureSource::isFrameRateSupported(double frameRate)
{
    double epsilon = 0.001;
    for (AVFrameRateRangeType *range in [[device() activeFormat] videoSupportedFrameRateRanges]) {
        if (frameRate + epsilon >= range.minFrameRate && frameRate - epsilon <= range.maxFrameRate)
            return true;
    }
    return false;
}

bool AVVideoCaptureSource::supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (!height && !width && !frameRate)
        return true;

    if ((height || width) && !bestSessionPresetForVideoDimensions(WTFMove(width), WTFMove(height)))
        return false;

    if (!frameRate)
        return true;

    return isFrameRateSupported(frameRate.value());
}

void AVVideoCaptureSource::captureSessionIsRunningDidChange(bool state)
{
    scheduleDeferredTask([this, state] {
        if ((state == m_isRunning) && (state == !muted()))
            return;

        m_isRunning = state;
        notifyMutedChange(!m_isRunning);
    });
}

bool AVVideoCaptureSource::interrupted() const
{
    if (m_interruption != InterruptionReason::None)
        return true;

    return RealtimeMediaSource::interrupted();
}

#if PLATFORM(IOS)
void AVVideoCaptureSource::captureSessionRuntimeError(RetainPtr<NSError> error)
{
    if (!m_isRunning || error.get().code != AVErrorMediaServicesWereReset)
        return;

    // Try to restart the session, but reset m_isRunning immediately so if it fails we won't try again.
    [m_session startRunning];
    m_isRunning = [m_session isRunning];
}

void AVVideoCaptureSource::captureSessionBeginInterruption(RetainPtr<NSNotification> notification)
{
    m_interruption = static_cast<AVVideoCaptureSource::InterruptionReason>([notification.get().userInfo[AVCaptureSessionInterruptionReasonKey] integerValue]);
}

void AVVideoCaptureSource::captureSessionEndInterruption(RetainPtr<NSNotification>)
{
    InterruptionReason reason = m_interruption;

    m_interruption = InterruptionReason::None;
    if (reason != InterruptionReason::VideoNotAllowedInSideBySide || m_isRunning || !m_session)
        return;

    [m_session startRunning];
    m_isRunning = [m_session isRunning];
}
#endif

} // namespace WebCore

@implementation WebCoreAVVideoCaptureSourceObserver

- (id)initWithCallback:(AVVideoCaptureSource*)callback
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
    [self removeNotificationObservers];
    m_callback = nullptr;
}

- (void)addNotificationObservers
{
#if PLATFORM(IOS)
    ASSERT(m_callback);

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    AVCaptureSessionType* session = m_callback->session();

    [center addObserver:self selector:@selector(sessionRuntimeError:) name:AVCaptureSessionRuntimeErrorNotification object:session];
    [center addObserver:self selector:@selector(beginSessionInterrupted:) name:AVCaptureSessionWasInterruptedNotification object:session];
    [center addObserver:self selector:@selector(endSessionInterrupted:) name:AVCaptureSessionInterruptionEndedNotification object:session];
#endif
}

- (void)removeNotificationObservers
{
#if PLATFORM(IOS)
    [[NSNotificationCenter defaultCenter] removeObserver:self];
#endif
}

- (void)captureOutput:(AVCaptureOutputType*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType*)connection
{
    if (!m_callback)
        return;

    m_callback->captureOutputDidOutputSampleBufferFromConnection(captureOutput, sampleBuffer, connection);
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(context);

    if (!m_callback)
        return;

    id newValue = [change valueForKey:NSKeyValueChangeNewKey];

#if !LOG_DISABLED
    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];

    if (willChange)
        LOG(Media, "WebCoreAVVideoCaptureSourceObserver::observeValueForKeyPath(%p) - will change, keyPath = %s", self, [keyPath UTF8String]);
    else {
        RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
        LOG(Media, "WebCoreAVVideoCaptureSourceObserver::observeValueForKeyPath(%p) - did change, keyPath = %s, value = %s", self, [keyPath UTF8String], [valueString.get() UTF8String]);
    }
#endif

    if ([keyPath isEqualToString:@"running"])
        m_callback->captureSessionIsRunningDidChange([newValue boolValue]);
}

#if PLATFORM(IOS)
- (void)sessionRuntimeError:(NSNotification*)notification
{
    NSError *error = notification.userInfo[AVCaptureSessionErrorKey];
    LOG(Media, "WebCoreAVVideoCaptureSourceObserver::sessionRuntimeError(%p) - error = %s", self, [[error localizedDescription] UTF8String]);

    if (m_callback)
        m_callback->captureSessionRuntimeError(error);
}

- (void)beginSessionInterrupted:(NSNotification*)notification
{
    LOG(Media, "WebCoreAVVideoCaptureSourceObserver::beginSessionInterrupted(%p) - reason = %d", self, [notification.userInfo[AVCaptureSessionInterruptionReasonKey] integerValue]);

    if (m_callback)
        m_callback->captureSessionBeginInterruption(notification);
}

- (void)endSessionInterrupted:(NSNotification*)notification
{
    LOG(Media, "WebCoreAVVideoCaptureSourceObserver::endSessionInterrupted(%p)", self);

    if (m_callback)
        m_callback->captureSessionEndInterruption(notification);
}
#endif

@end

#endif // ENABLE(MEDIA_STREAM)
