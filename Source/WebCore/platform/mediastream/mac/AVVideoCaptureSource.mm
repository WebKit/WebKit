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
#import "RealtimeVideoSource.h"
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

class AVVideoPreset : public VideoPreset {
public:
    static Ref<AVVideoPreset> create(IntSize size, Vector<FrameRateRange>&& frameRateRanges, AVCaptureDeviceFormat* format)
    {
        return adoptRef(*new AVVideoPreset(size, WTFMove(frameRateRanges), format));
    }

    AVVideoPreset(IntSize size, Vector<FrameRateRange>&& frameRateRanges, AVCaptureDeviceFormat* format)
        : VideoPreset(size, WTFMove(frameRateRanges), AVCapture)
        , format(format)
    {
    }

    RetainPtr<AVCaptureDeviceFormat> format;
};

CaptureSourceOrError AVVideoCaptureSource::create(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier pageIdentifier)
{
    auto *avDevice = [PAL::getAVCaptureDeviceClass() deviceWithUniqueID:device.persistentId()];
    if (!avDevice)
        return { "No AVVideoCaptureSource device"_s };

    auto source = adoptRef(*new AVVideoCaptureSource(avDevice, device, WTFMove(hashSalts), pageIdentifier));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return WTFMove(result.value().badConstraint);
    }

    return CaptureSourceOrError(RealtimeVideoSource::create(WTFMove(source)));
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDevice* avDevice, const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
    : RealtimeVideoCaptureSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_objcObserver(adoptNS([[WebCoreAVVideoCaptureSourceObserver alloc] initWithCallback:this]))
    , m_device(avDevice)
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

const RealtimeMediaSourceSettings& AVVideoCaptureSource::settings()
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

    settings.setLabel(name());
    settings.setFrameRate(frameRate());

    auto size = this->size();
    if (m_videoFrameRotation == VideoFrame::Rotation::Left || m_videoFrameRotation == VideoFrame::Rotation::Right)
        size = size.transposedSize();
    
    settings.setWidth(size.width());
    settings.setHeight(size.height());
    settings.setDeviceId(hashedId());

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

const RealtimeMediaSourceCapabilities& AVVideoCaptureSource::capabilities()
{
    if (m_capabilities)
        return *m_capabilities;

    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
    capabilities.setDeviceId(hashedId());

    AVCaptureDevice *videoDevice = device();
    if ([videoDevice position] == AVCaptureDevicePositionFront)
        capabilities.addFacingMode(RealtimeMediaSourceSettings::User);
    if ([videoDevice position] == AVCaptureDevicePositionBack)
        capabilities.addFacingMode(RealtimeMediaSourceSettings::Environment);

    updateCapabilities(capabilities);

    m_capabilities = WTFMove(capabilities);

    return *m_capabilities;
}

double AVVideoCaptureSource::facingModeFitnessDistanceAdjustment() const
{
    ASSERT(isMainThread());

    if ([device() position] != AVCaptureDevicePositionBack)
        return 0;

    static NSMutableArray *devicePriorities;
    if (!devicePriorities) {
        devicePriorities = [[NSMutableArray alloc] init];

        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInTripleCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInTripleCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeBuiltInDualWideCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
        [devicePriorities addObject:AVCaptureDeviceTypeBuiltInUltraWideCamera];
        [devicePriorities addObject:AVCaptureDeviceTypeBuiltInDualCamera];
        [devicePriorities addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
        [devicePriorities addObject:AVCaptureDeviceTypeBuiltInTelephotoCamera];
        if (PAL::canLoad_AVFoundation_AVCaptureDeviceTypeDeskViewCamera())
            [devicePriorities addObject:AVCaptureDeviceTypeDeskViewCamera];
    }

    auto relativePriority = [devicePriorities indexOfObject:[device() deviceType]];
    if (relativePriority == NSNotFound)
        relativePriority = devicePriorities.count;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, captureDevice().label(), " has priority ", relativePriority);

    return relativePriority;
}

bool AVVideoCaptureSource::prefersPreset(VideoPreset& preset)
{
#if PLATFORM(IOS_FAMILY)
    return [static_cast<AVVideoPreset*>(&preset)->format.get() isVideoBinned];
#else
    UNUSED_PARAM(preset);
#endif

    return true;
}

void AVVideoCaptureSource::setFrameRateWithPreset(double requestedFrameRate, RefPtr<VideoPreset> preset)
{
    auto* avPreset = preset ? downcast<AVVideoPreset>(preset.get()) : nullptr;
    m_currentPreset = avPreset;
    m_currentFrameRate = requestedFrameRate;

    setSessionSizeAndFrameRate();
}

void AVVideoCaptureSource::setSessionSizeAndFrameRate()
{
    if (!m_session)
        return;

    auto* avPreset = m_currentPreset.get();
    if (!avPreset)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, SizeAndFrameRate { m_currentPreset->size.width(), m_currentPreset->size.height(), m_currentFrameRate });

    auto* frameRateRange = frameDurationForFrameRate(m_currentFrameRate);
    ASSERT(frameRateRange);

    if (m_appliedPreset && m_appliedPreset->format.get() == m_currentPreset->format.get() && m_appliedFrameRateRange.get() == frameRateRange) {
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, " settings already match");
        return;
    }

    ASSERT(avPreset->format);

    NSError *error = nil;
    [m_session beginConfiguration];
    @try {
        if ([device() lockForConfiguration:&error]) {
            [device() setActiveFormat:avPreset->format.get()];

#if PLATFORM(MAC)
            auto settingsDictionary = @{
                (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(avVideoCapturePixelBufferFormat()),
                (__bridge NSString *)kCVPixelBufferWidthKey: @(avPreset->size.width()),
                (__bridge NSString *)kCVPixelBufferHeightKey: @(avPreset->size.height()),
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

            [device() unlockForConfiguration];
            m_appliedFrameRateRange = frameRateRange;
            m_appliedPreset = m_currentPreset;
        }
    } @catch(NSException *exception) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "error configuring device ", [[exception name] UTF8String], ", reason : ", [[exception reason] UTF8String]);
        [device() unlockForConfiguration];
        ASSERT_NOT_REACHED();
    }
    [m_session commitConfiguration];

    ERROR_LOG_IF(error && loggerPtr(), LOGIDENTIFIER, [[error localizedDescription] UTF8String]);
}

static inline int sensorOrientation(AVCaptureVideoOrientation videoOrientation)
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

static inline int sensorOrientationFromVideoOutput(AVCaptureVideoDataOutput* videoOutput)
{
    AVCaptureConnection* connection = [videoOutput connectionWithMediaType:AVMediaTypeVideo];
    return connection ? sensorOrientation([connection videoOrientation]) : 0;
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
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "failed to allocate AVCaptureDeviceInput ", [[error localizedDescription] UTF8String]);
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

    setSessionSizeAndFrameRate();

    m_sensorOrientation = sensorOrientationFromVideoOutput(m_videoOutput.get());
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
    Vector<Ref<VideoPreset>> presets;
    for (AVCaptureDeviceFormat* format in [device() formats]) {

        CMVideoDimensions dimensions = PAL::CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        IntSize size = { dimensions.width, dimensions.height };
        auto index = presets.findIf([&size](auto& preset) {
            return size == preset->size;
        });
        if (index != notFound)
            continue;

        Vector<FrameRateRange> frameRates;
        for (AVFrameRateRange* range in [format videoSupportedFrameRateRanges])
            frameRates.append({ range.minFrameRate, range.maxFrameRate});

        presets.append(AVVideoPreset::create(size, WTFMove(frameRates), format));
    }

    setSupportedPresets(WTFMove(presets));
}

#if PLATFORM(IOS_FAMILY)
void AVVideoCaptureSource::captureSessionRuntimeError(RetainPtr<NSError> error)
{
    auto identifier = LOGIDENTIFIER;
    ERROR_LOG_IF(loggerPtr(), identifier, [error code], ", ", [[error localizedDescription] UTF8String]);

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
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, [notification.get().userInfo[AVCaptureSessionInterruptionReasonKey] integerValue]);
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
