/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#import "ImageTransferSessionVT.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeVideoUtilities.h"
#import "VideoFrameCV.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVCaptureInput.h>
#import <AVFoundation/AVCaptureOutput.h>
#import <AVFoundation/AVCaptureSession.h>
#import <AVFoundation/AVError.h>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>

#import "CoreVideoSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

@interface WebCoreAVVideoCaptureSourceObserver : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
    AVVideoCaptureSource* m_callback;
}

-(id)initWithCallback:(AVVideoCaptureSource*)callback;
-(void)disconnect;
-(void)addNotificationObservers;
-(void)removeNotificationObservers;
-(void)captureOutput:(AVCaptureOutput*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context;
#if PLATFORM(IOS_FAMILY)
-(void)sessionRuntimeError:(NSNotification*)notification;
-(void)beginSessionInterrupted:(NSNotification*)notification;
-(void)endSessionInterrupted:(NSNotification*)notification;
-(void)deviceConnectedDidChange:(NSNotification*)notification;
#endif
@end

namespace WebCore {

static inline OSType avVideoCapturePixelBufferFormat()
{
#if HAVE(DISPLAY_LAYER_BIPLANAR_SUPPORT)
    return preferedPixelBufferFormat();
#else
    return kCVPixelFormatType_420YpCbCr8Planar;
#endif
}

static dispatch_queue_t globaVideoCaptureSerialQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create_with_target("WebCoreAVVideoCaptureSource video capture queue", DISPATCH_QUEUE_SERIAL, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    });
    return globalQueue;
}

std::optional<double> AVVideoCaptureSource::computeMinZoom() const
{
#if PLATFORM(IOS_FAMILY)
    if (m_zoomScaleFactor == 1.0)
        return { };
    return 1.0 / m_zoomScaleFactor;
#else
    return { };
#endif
}

std::optional<double> AVVideoCaptureSource::computeMaxZoom(AVCaptureDeviceFormat* format) const
{
#if PLATFORM(IOS_FAMILY)
    // We restrict zoom for now as it might require elevated permissions.
    return std::min([format videoMaxZoomFactor], 4.0) / m_zoomScaleFactor;
#else
    UNUSED_PARAM(format);
    return { };
#endif
}

CaptureSourceOrError AVVideoCaptureSource::create(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    auto *avDevice = [PAL::getAVCaptureDeviceClass() deviceWithUniqueID:device.persistentId()];
    if (!avDevice)
        return CaptureSourceOrError({ "No AVVideoCaptureSource device"_s , MediaAccessDenialReason::PermissionDenied });

    Ref<RealtimeMediaSource> source = adoptRef(*new AVVideoCaptureSource(avDevice, device, WTFMove(hashSalts), pageIdentifier));
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return CaptureSourceOrError({ WTFMove(result->badConstraint), MediaAccessDenialReason::InvalidConstraint });
    }

    return WTFMove(source);
}

static double cameraZoomScaleFactor(AVCaptureDeviceType deviceType)
{
#if PLATFORM(IOS_FAMILY)
    return (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInTripleCamera() && deviceType == AVCaptureDeviceTypeBuiltInTripleCamera)
        || (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInDualWideCamera() && deviceType == AVCaptureDeviceTypeBuiltInDualWideCamera) ? 2.0 : 1.0;
#else
    UNUSED_PARAM(deviceType);
    return 1.0;
#endif
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDevice* avDevice, const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : RealtimeVideoCaptureSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_objcObserver(adoptNS([[WebCoreAVVideoCaptureSourceObserver alloc] initWithCallback:this]))
    , m_device(avDevice)
    , m_zoomScaleFactor(cameraZoomScaleFactor([avDevice deviceType]))
    , m_verifyCapturingTimer(*this, &AVVideoCaptureSource::verifyIsCapturing)
{
    [m_device addObserver:m_objcObserver.get() forKeyPath:@"suspended" options:NSKeyValueObservingOptionNew context:(void *)nil];
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    [m_objcObserver disconnect];
    [m_device removeObserver:m_objcObserver.get() forKeyPath:@"suspended"];

    if (!m_session)
        return;

    if ([m_session isRunning])
        [m_session stopRunning];

    clearSession();
}

void AVVideoCaptureSource::verifyIsCapturing()
{
    ASSERT(m_isRunning);
    if (m_lastFramesCount != m_framesCount) {
        m_lastFramesCount = m_framesCount;
        return;
    }

    RELEASE_LOG_ERROR(WebRTC, "AVVideoCaptureSource::verifyIsCapturing - no frame received in %d seconds, failing", static_cast<int>(m_verifyCapturingTimer.repeatInterval().value()));
    captureFailed();
}

void AVVideoCaptureSource::updateVerifyCapturingTimer()
{
    if (!m_isRunning || m_interrupted) {
        m_verifyCapturingTimer.stop();
        return;
    }

    if (m_verifyCapturingTimer.isActive())
        return;

    m_verifyCapturingTimer.startRepeating(verifyCaptureInterval);
    m_framesCount = 0;
    m_lastFramesCount = 0;
}

void AVVideoCaptureSource::clearSession()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    ASSERT(m_session);
    [m_session removeObserver:m_objcObserver.get() forKeyPath:@"running"];
    m_session = nullptr;
}

void AVVideoCaptureSource::startProducingData()
{
    if (!m_session) {
        if (!setupSession())
            return;
    }

    bool isRunning = !![m_session isRunning];
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, isRunning);
    if (isRunning)
        return;

    [m_objcObserver addNotificationObservers];
    [m_session startRunning];
}

void AVVideoCaptureSource::stopProducingData()
{
    if (!m_session)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, !![m_session isRunning]);
    [m_objcObserver removeNotificationObservers];
    [m_session stopRunning];

    m_interrupted = false;

#if PLATFORM(IOS_FAMILY)
    clearSession();
#endif
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

void AVVideoCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>)
{
    m_currentSettings = std::nullopt;
}

static bool isZoomSupported(const Vector<VideoPreset>& presets)
{
    return anyOf(presets, [](auto& preset) {
        return preset.isZoomSupported();
    });
}

const RealtimeMediaSourceSettings& AVVideoCaptureSource::settings()
{
    if (m_currentSettings)
        return *m_currentSettings;

    RealtimeMediaSourceSettings settings;
    if ([device() position] == AVCaptureDevicePositionFront)
        settings.setFacingMode(VideoFacingMode::User);
    else if ([device() position] == AVCaptureDevicePositionBack)
        settings.setFacingMode(VideoFacingMode::Environment);
    else
        settings.setFacingMode(VideoFacingMode::Unknown);

    settings.setLabel(name());
    settings.setFrameRate(frameRate());

    auto size = this->size();
    if (m_videoFrameRotation == VideoFrame::Rotation::Left || m_videoFrameRotation == VideoFrame::Rotation::Right)
        size = size.transposedSize();
    
    settings.setWidth(size.width());
    settings.setHeight(size.height());
    settings.setDeviceId(hashedId());
    settings.setGroupId(captureDevice().groupId());

    RealtimeMediaSourceSupportedConstraints supportedConstraints;
    supportedConstraints.setSupportsDeviceId(true);
    supportedConstraints.setSupportsGroupId(true);
    supportedConstraints.setSupportsFacingMode([device() position] != AVCaptureDevicePositionUnspecified);
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsFrameRate(true);

    if (isZoomSupported(presets())) {
        supportedConstraints.setSupportsZoom(true);
        settings.setZoom(zoom());
    }

    settings.setSupportedConstraints(supportedConstraints);

    m_currentSettings = WTFMove(settings);

    return *m_currentSettings;
}

const RealtimeMediaSourceCapabilities& AVVideoCaptureSource::capabilities()
{
    if (m_capabilities)
        return *m_capabilities;

    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
    capabilities.setDeviceId(hashedId());

    AVCaptureDevice *videoDevice = device();
    if ([videoDevice position] == AVCaptureDevicePositionFront)
        capabilities.addFacingMode(VideoFacingMode::User);
    if ([videoDevice position] == AVCaptureDevicePositionBack)
        capabilities.addFacingMode(VideoFacingMode::Environment);

#if HAVE(AVCAPTUREDEVICE_MINFOCUSLENGTH)
    double minimumFocusDistance = [videoDevice minimumFocusDistance];
    if (minimumFocusDistance != -1.0) {
        ASSERT(minimumFocusDistance >= 0);
        auto supportedConstraints = settings().supportedConstraints();
        supportedConstraints.setSupportsFocusDistance(true);
        capabilities.setFocusDistance({ minimumFocusDistance / 1000, std::numeric_limits<double>::max() });
        capabilities.setSupportedConstraints(supportedConstraints);
    }
#endif // HAVE(AVCAPTUREDEVICE_MINFOCUSLENGTH)

    updateCapabilities(capabilities);

    m_capabilities = WTFMove(capabilities);

    return *m_capabilities;
}

NSMutableArray* AVVideoCaptureSource::cameraCaptureDeviceTypes()
{
    ASSERT(isMainThread());
    static NSMutableArray *devicePriorities;
    if (!devicePriorities) {
        devicePriorities = [[NSMutableArray alloc] initWithCapacity:8];

        // This order is important as it is used to select the preferred back camera.
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInTripleCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInTripleCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInDualWideCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInUltraWideCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInUltraWideCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInDualCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInDualCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInWideAngleCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInTelephotoCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInTelephotoCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeDeskViewCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeDeskViewCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeExternalUnknown())
            [devicePriorities addObject:AVCaptureDeviceTypeExternalUnknown];
    }
    return devicePriorities;
}

double AVVideoCaptureSource::facingModeFitnessScoreAdjustment() const
{
    ASSERT(isMainThread());

    if ([device() position] != AVCaptureDevicePositionBack)
        return 0;

    auto relativePriority = [cameraCaptureDeviceTypes() indexOfObject:[device() deviceType]];
    if (relativePriority == NSNotFound)
        relativePriority = cameraCaptureDeviceTypes().count;

    auto fitnessScore = cameraCaptureDeviceTypes().count - relativePriority;
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, captureDevice().label(), " has fitness adjustment ", fitnessScore);

    return fitnessScore;
}

bool AVVideoCaptureSource::prefersPreset(const VideoPreset& preset)
{
#if PLATFORM(IOS_FAMILY)
    return [preset.format() isVideoBinned];
#else
    UNUSED_PARAM(preset);
#endif

    return true;
}

void AVVideoCaptureSource::setFrameRateAndZoomWithPreset(double requestedFrameRate, double requestedZoom, std::optional<VideoPreset>&& preset)
{
    m_currentPreset = WTFMove(preset);
    if (m_currentPreset)
        setIntrinsicSize({ m_currentPreset->size().width(), m_currentPreset->size().height() });

    m_currentFrameRate = requestedFrameRate;
    m_currentZoom = m_zoomScaleFactor * requestedZoom;

    setSessionSizeFrameRateAndZoom();
}

void AVVideoCaptureSource::setSessionSizeFrameRateAndZoom()
{
    if (!m_session)
        return;

    if (!m_currentPreset)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, SizeFrameRateAndZoom { m_currentPreset->size().width(), m_currentPreset->size().height(), m_currentFrameRate, m_currentZoom });

    auto* frameRateRange = frameDurationForFrameRate(m_currentFrameRate);
    ASSERT(frameRateRange);

    if (m_appliedPreset && m_appliedPreset->format() == m_currentPreset->format() && m_appliedFrameRateRange.get() == frameRateRange && m_appliedZoom == m_currentZoom) {
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, " settings already match");
        return;
    }

    ASSERT(m_currentPreset->format());

    NSError *error = nil;
    [m_session beginConfiguration];
    @try {
        if ([device() lockForConfiguration:&error]) {
            [device() setActiveFormat:m_currentPreset->format()];

#if PLATFORM(MAC)
            auto settingsDictionary = @{
                (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(avVideoCapturePixelBufferFormat()),
                (__bridge NSString *)kCVPixelBufferWidthKey: @(m_currentPreset->size().width()),
                (__bridge NSString *)kCVPixelBufferHeightKey: @(m_currentPreset->size().height()),
                (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ }
            };
            [m_videoOutput setVideoSettings:settingsDictionary];
#endif

            if (frameRateRange) {
                m_currentFrameRate = clampTo(m_currentFrameRate, frameRateRange.minFrameRate, frameRateRange.maxFrameRate);

                auto frameDuration = PAL::CMTimeMake(1, m_currentFrameRate);
                if (PAL::CMTimeCompare(frameDuration, frameRateRange.minFrameDuration) < 0)
                    frameDuration = frameRateRange.minFrameDuration;
                else if (PAL::CMTimeCompare(frameDuration, frameRateRange.maxFrameDuration) > 0)
                    frameDuration = frameRateRange.maxFrameDuration;

                ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "setting frame rate to ", m_currentFrameRate, ", duration ", PAL::toMediaTime(frameDuration));

                [device() setActiveVideoMinFrameDuration: frameDuration];
                [device() setActiveVideoMaxFrameDuration: frameDuration];
            } else
                ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "cannot find proper frame rate range for the selected preset\n");

#if PLATFORM(IOS_FAMILY)
            if (m_currentZoom != m_appliedZoom) {
                ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "setting zoom to ", m_currentZoom);
                [device() setVideoZoomFactor:m_currentZoom];
                m_appliedZoom = m_currentZoom;
            }
#endif

            [device() unlockForConfiguration];
            m_appliedFrameRateRange = frameRateRange;
            m_appliedPreset = m_currentPreset;
        }
    } @catch(NSException *exception) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "error configuring device ", exception.name, ", reason : ", exception.reason);
        [device() unlockForConfiguration];
        ASSERT_NOT_REACHED();
    }
    [m_session commitConfiguration];

    ERROR_LOG_IF(error && loggerPtr(), LOGIDENTIFIER, error);
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static inline IntDegrees sensorOrientation(AVCaptureVideoOrientation videoOrientation)
{
#if PLATFORM(IOS_FAMILY)
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
ALLOW_DEPRECATED_DECLARATIONS_END

IntDegrees AVVideoCaptureSource::sensorOrientationFromVideoOutput()
{
    if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeExternalUnknown() && [device() deviceType] == AVCaptureDeviceTypeExternalUnknown)
        return 0;

    AVCaptureConnection* connection = [m_videoOutput connectionWithMediaType:AVMediaTypeVideo];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return connection ? sensorOrientation([connection videoOrientation]) : 0;
ALLOW_DEPRECATED_DECLARATIONS_END
}

bool AVVideoCaptureSource::setupSession()
{
    if (m_session)
        return true;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

#if ENABLE(APP_PRIVACY_REPORT)
    auto identity = RealtimeMediaSourceCenter::singleton().identity();
    ERROR_LOG_IF(loggerPtr() && !identity, LOGIDENTIFIER, "RealtimeMediaSourceCenter::identity() returned null!");

    if (identity && [PAL::getAVCaptureSessionClass() instancesRespondToSelector:@selector(initWithAssumedIdentity:)])
        m_session = adoptNS([PAL::allocAVCaptureSessionInstance() initWithAssumedIdentity:identity.get()]);

    if (!m_session)
#endif
        m_session = adoptNS([PAL::allocAVCaptureSessionInstance() init]);

    if (!m_session) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "failed to allocate AVCaptureSession");
        captureFailed();
        return false;
    }

#if PLATFORM(IOS_FAMILY)
    PAL::AVCaptureSessionSetAuthorizedToUseCameraInMultipleForegroundAppLayout(m_session.get());
#endif
    [m_session addObserver:m_objcObserver.get() forKeyPath:@"running" options:NSKeyValueObservingOptionNew context:(void *)nil];

    [m_session beginConfiguration];
    bool success = setupCaptureSession();
    [m_session commitConfiguration];

    if (!success)
        captureFailed();

    return success;
}

AVFrameRateRange* AVVideoCaptureSource::frameDurationForFrameRate(double rate)
{
    using namespace PAL; // For CMTIME_COMPARE_INLINE

    AVFrameRateRange *bestFrameRateRange = nil;
    for (AVFrameRateRange *frameRateRange in [[device() activeFormat] videoSupportedFrameRateRanges]) {
        if (frameRateRangeIncludesRate({ [frameRateRange minFrameRate], [frameRateRange maxFrameRate] }, rate)) {
            if (!bestFrameRateRange || CMTIME_COMPARE_INLINE([frameRateRange minFrameDuration], >, [bestFrameRateRange minFrameDuration]))
                bestFrameRateRange = frameRateRange;
        }
    }

    if (!bestFrameRateRange)
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "no frame rate range for rate ", rate);

    return bestFrameRateRange;
}

bool AVVideoCaptureSource::setupCaptureSession()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    NSError *error = nil;
    RetainPtr<AVCaptureDeviceInput> videoIn = adoptNS([PAL::allocAVCaptureDeviceInputInstance() initWithDevice:device() error:&error]);
    if (error) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "failed to allocate AVCaptureDeviceInput ", error);
        return false;
    }

    if (![session() canAddInput:videoIn.get()]) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to add video input device");
        return false;
    }
    [session() addInput:videoIn.get()];

    m_videoOutput = adoptNS([PAL::allocAVCaptureVideoDataOutputInstance() init]);
    auto settingsDictionary = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys: @(avVideoCapturePixelBufferFormat()), kCVPixelBufferPixelFormatTypeKey, nil]);

    [m_videoOutput setVideoSettings:settingsDictionary.get()];
    [m_videoOutput setAlwaysDiscardsLateVideoFrames:YES];
    [m_videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];

    if (![session() canAddOutput:m_videoOutput.get()]) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to add video output device");
        return false;
    }
    [session() addOutput:m_videoOutput.get()];

    setSessionSizeFrameRateAndZoom();

    m_sensorOrientation = sensorOrientationFromVideoOutput();
    computeVideoFrameRotation();

    return true;
}

void AVVideoCaptureSource::shutdownCaptureSession()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    m_buffer = nullptr;
}

void AVVideoCaptureSource::monitorOrientation(OrientationNotifier& notifier)
{
#if PLATFORM(IOS_FAMILY)
    if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeExternalUnknown() && [device() deviceType] == AVCaptureDeviceTypeExternalUnknown)
        return;

    notifier.addObserver(*this);
    orientationChanged(notifier.orientation());
#else
    UNUSED_PARAM(notifier);
#endif
}

void AVVideoCaptureSource::orientationChanged(IntDegrees orientation)
{
    ASSERT(orientation == 0 || orientation == 90 || orientation == -90 || orientation == 180);
    m_deviceOrientation = orientation;
    computeVideoFrameRotation();
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "rotation = ", m_videoFrameRotation, ", orientation = ", m_deviceOrientation);
}

void AVVideoCaptureSource::computeVideoFrameRotation()
{
    bool frontCamera = [device() position] == AVCaptureDevicePositionFront;
    VideoFrame::Rotation videoFrameRotation;
    switch (m_sensorOrientation - m_deviceOrientation) {
    case 0:
        videoFrameRotation = VideoFrame::Rotation::None;
        break;
    case 180:
    case -180:
        videoFrameRotation = VideoFrame::Rotation::UpsideDown;
        break;
    case 90:
    case -270:
        videoFrameRotation = frontCamera ? VideoFrame::Rotation::Left : VideoFrame::Rotation::Right;
        break;
    case -90:
    case 270:
        videoFrameRotation = frontCamera ? VideoFrame::Rotation::Right : VideoFrame::Rotation::Left;
        break;
    default:
        ASSERT_NOT_REACHED();
        videoFrameRotation = VideoFrame::Rotation::None;
    }
    if (videoFrameRotation == m_videoFrameRotation)
        return;

    m_videoFrameRotation = videoFrameRotation;
    notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });
}

void AVVideoCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutput*, CMSampleBufferRef sampleBuffer, AVCaptureConnection* captureConnection)
{
    if (++m_framesCount <= framesToDropWhenStarting)
        return;

    auto videoFrame = VideoFrameCV::create(sampleBuffer, [captureConnection isVideoMirrored], m_videoFrameRotation);
    m_buffer = &videoFrame.get();
    setIntrinsicSize(expandedIntSize(videoFrame->presentationSize()));
    VideoFrameTimeMetadata metadata;
    metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();
    dispatchVideoFrameToObservers(WTFMove(videoFrame), metadata);
}

void AVVideoCaptureSource::captureSessionIsRunningDidChange(bool state)
{
    scheduleDeferredTask([this, logIdentifier = LOGIDENTIFIER, state] {
        ALWAYS_LOG_IF(loggerPtr(), logIdentifier, state);
        if ((state == m_isRunning) && (state == !muted()))
            return;

        m_isRunning = state;

        updateVerifyCapturingTimer();
        notifyMutedChange(!m_isRunning);
    });
}

void AVVideoCaptureSource::captureDeviceSuspendedDidChange()
{
#if !PLATFORM(IOS_FAMILY)
    scheduleDeferredTask([this, logIdentifier = LOGIDENTIFIER] {
        m_interrupted = [m_device isSuspended];
        ALWAYS_LOG_IF(loggerPtr(), logIdentifier, !!m_interrupted);

        updateVerifyCapturingTimer();

        if (m_interrupted != muted())
            notifyMutedChange(m_interrupted);
    });
#endif
}

bool AVVideoCaptureSource::interrupted() const
{
    if (m_interrupted)
        return true;

    return RealtimeMediaSource::interrupted();
}

void AVVideoCaptureSource::generatePresets()
{
    Vector<VideoPreset> presets;
    for (AVCaptureDeviceFormat* format in [device() formats]) {

        CMVideoDimensions dimensions = PAL::CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        IntSize size = { dimensions.width, dimensions.height };
        auto index = presets.findIf([&size](auto& preset) {
            return size == preset.size();
        });
        if (index != notFound)
            continue;

        Vector<FrameRateRange> frameRates;
        for (AVFrameRateRange* range in [format videoSupportedFrameRateRanges])
            frameRates.append({ range.minFrameRate, range.maxFrameRate});

        VideoPreset preset { size, WTFMove(frameRates), computeMinZoom(), computeMaxZoom(format) };
        preset.setFormat(format);
        presets.append(WTFMove(preset));
    }

    setSupportedPresets(WTFMove(presets));
}

#if PLATFORM(IOS_FAMILY)
void AVVideoCaptureSource::captureSessionRuntimeError(RetainPtr<NSError> error)
{
    auto identifier = LOGIDENTIFIER;
    ERROR_LOG_IF(loggerPtr(), identifier, [error code], ", ", error.get());

    if (!m_isRunning || error.get().code != AVErrorMediaServicesWereReset)
        return;

    scheduleDeferredTask([this, identifier] {
        // Try to restart the session, but reset m_isRunning immediately so if it fails we won't try again.
        ERROR_LOG_IF(loggerPtr(), identifier, "restarting session");
        [m_session startRunning];
        m_isRunning = [m_session isRunning];
    });
}

void AVVideoCaptureSource::captureSessionBeginInterruption(RetainPtr<NSNotification> notification)
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, [notification userInfo][AVCaptureSessionInterruptionReasonKey]);
    m_interrupted = true;
}

void AVVideoCaptureSource::captureSessionEndInterruption(RetainPtr<NSNotification>)
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    m_interrupted = false;
}
#endif

void AVVideoCaptureSource::deviceDisconnected(RetainPtr<NSNotification> notification)
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    if (this->device() == [notification object])
        captureFailed();
}

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
    ASSERT(m_callback);

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    [center addObserver:self selector:@selector(deviceConnectedDidChange:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];

#if PLATFORM(IOS_FAMILY)
    AVCaptureSession* session = m_callback->session();
    [center addObserver:self selector:@selector(sessionRuntimeError:) name:AVCaptureSessionRuntimeErrorNotification object:session];
    [center addObserver:self selector:@selector(beginSessionInterrupted:) name:AVCaptureSessionWasInterruptedNotification object:session];
    [center addObserver:self selector:@selector(endSessionInterrupted:) name:AVCaptureSessionInterruptionEndedNotification object:session];
#endif
}

- (void)removeNotificationObservers
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)captureOutput:(AVCaptureOutput*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection
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
    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];

#if !RELEASE_LOG_DISABLED
    if (m_callback->loggerPtr()) {
        auto identifier = Logger::LogSiteIdentifier("AVVideoCaptureSource", "observeValueForKeyPath", m_callback->logIdentifier());
        RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
        m_callback->logger().logAlways(m_callback->logChannel(), identifier, willChange ? "will" : "did", " change '", [keyPath UTF8String], "' to ", [valueString UTF8String]);
    }
#endif

    if (willChange)
        return;

    if ([keyPath isEqualToString:@"running"])
        m_callback->captureSessionIsRunningDidChange([newValue boolValue]);
    if ([keyPath isEqualToString:@"suspended"])
        m_callback->captureDeviceSuspendedDidChange();
}

- (void)deviceConnectedDidChange:(NSNotification*)notification
{
    if (m_callback)
        m_callback->deviceDisconnected(notification);
}

#if PLATFORM(IOS_FAMILY)
- (void)sessionRuntimeError:(NSNotification*)notification
{
    NSError *error = notification.userInfo[AVCaptureSessionErrorKey];
    if (m_callback)
        m_callback->captureSessionRuntimeError(error);
}

- (void)beginSessionInterrupted:(NSNotification*)notification
{
    if (m_callback)
        m_callback->captureSessionBeginInterruption(notification);
}

- (void)endSessionInterrupted:(NSNotification*)notification
{
    if (m_callback)
        m_callback->captureSessionEndInterruption(notification);
}
#endif

@end

#endif // ENABLE(MEDIA_STREAM)
