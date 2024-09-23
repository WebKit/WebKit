/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && HAVE(AVCAPTUREDEVICE)

#import "FillLightMode.h"
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
#import <AVFoundation/AVCapturePhotoOutput.h>
#import <AVFoundation/AVCaptureSession.h>
#import <AVFoundation/AVError.h>
#import <objc/runtime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Scope.h>
#import <wtf/WorkQueue.h>
#include <wtf/cocoa/VectorCocoa.h>

#import "CoreVideoSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

@interface AVCaptureDeviceFormat (AVCaptureDeviceFormat_New_API)
@property (nonatomic, readonly) NSArray<NSValue *> *supportedMaxPhotoDimensions;
@end

@interface AVCapturePhotoSettings (AVCapturePhotoSettings_New_API)
@property (nonatomic) CMVideoDimensions maxPhotoDimensions;
@end

@interface AVCapturePhotoOutput (AVCapturePhotoOutput_New_API)
@property (nonatomic) CMVideoDimensions maxPhotoDimensions;
@end

@interface NSValue (NSValueCMVideoDimensionsExtensions_New_API)
@property (readonly) CMVideoDimensions CMVideoDimensionsValue;
@end

// FIXME (119325252): Remove staging code for -[AVCaptureSession initWithMediaEnvironment:]
@interface AVCaptureSession(Staging_113653478)
- (instancetype)initWithMediaEnvironment:(NSString *)mediaEnvironment;
@end

@interface WebCoreAVVideoCaptureSourceObserver : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate, AVCapturePhotoCaptureDelegate> {
    ThreadSafeWeakPtr<AVVideoCaptureSource> m_captureSource;
}

-(id)initWithCaptureSource:(AVVideoCaptureSource*)captureSource;
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
- (void)captureOutput:(AVCapturePhotoOutput *)output didFinishProcessingPhoto:(AVCapturePhoto *)photo error:(NSError *)error;
@end

namespace WebCore {

static dispatch_queue_t globaVideoCaptureSerialQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create_with_target("WebCoreAVVideoCaptureSource video capture queue", DISPATCH_QUEUE_SERIAL, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    });
    return globalQueue;
}

static FillLightMode toFillLightMode(AVCaptureTorchMode mode)
{
    switch (mode) {
    case AVCaptureTorchModeOff:
        return FillLightMode::Off;
        break;
    case AVCaptureTorchModeOn:
        return FillLightMode::Flash;
        break;
    case AVCaptureTorchModeAuto:
        return FillLightMode::Auto;
        break;
    }

    ASSERT_NOT_REACHED();
    return FillLightMode::Auto;
}

#if PLATFORM(IOS_FAMILY)
static AVCaptureFlashMode toAVCaptureFlashMode(FillLightMode mode)
{
    switch (mode) {
    case FillLightMode::Off:
        return AVCaptureFlashModeOff;
        break;
    case FillLightMode::Flash:
        return AVCaptureFlashModeOn;
        break;
    case FillLightMode::Auto:
        return AVCaptureFlashModeAuto;
        break;
    }

    ASSERT_NOT_REACHED();
    return AVCaptureFlashModeAuto;
}
#endif

static AVCaptureWhiteBalanceMode whiteBalanceModeFromMeteringMode(MeteringMode mode)
{
    switch (mode) {
    case MeteringMode::Manual:
        return AVCaptureWhiteBalanceModeLocked;
        break;
    case MeteringMode::SingleShot:
    case MeteringMode::None:
        return AVCaptureWhiteBalanceModeAutoWhiteBalance;
        break;
    case MeteringMode::Continuous:
        return AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance;
        break;
    }

    ASSERT_NOT_REACHED();
    return AVCaptureWhiteBalanceModeAutoWhiteBalance;
}

static MeteringMode meteringModeFromAVCaptureWhiteBalanceMode(AVCaptureWhiteBalanceMode mode)
{
    switch (mode) {
    case AVCaptureWhiteBalanceModeLocked:
        return MeteringMode::Manual;
        break;
    case AVCaptureWhiteBalanceModeAutoWhiteBalance:
        return MeteringMode::SingleShot;
        break;
    case AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance:
        return MeteringMode::Continuous;
        break;
    }

    return MeteringMode::None;
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
    return std::min([format videoMaxZoomFactor] / m_zoomScaleFactor, 10.0);
#else
    UNUSED_PARAM(format);
    return { };
#endif
}

static WorkQueue& photoQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("WebKit::AVPhotoCapture Queue"_s);
    return queue.get();
}

CaptureSourceOrError AVVideoCaptureSource::create(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, std::optional<PageIdentifier> pageIdentifier)
{
    auto *avDevice = [PAL::getAVCaptureDeviceClass() deviceWithUniqueID:device.persistentId()];
    if (!avDevice)
        return CaptureSourceOrError({ "No AVVideoCaptureSource device"_s , MediaAccessDenialReason::PermissionDenied });

    Ref<RealtimeMediaSource> source = adoptRef(*new AVVideoCaptureSource(avDevice, device, WTFMove(hashSalts), pageIdentifier));
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return CaptureSourceOrError(CaptureSourceError { result->invalidConstraint });
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

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDevice* avDevice, const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, std::optional<PageIdentifier> pageIdentifier)
    : RealtimeVideoCaptureSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_objcObserver(adoptNS([[WebCoreAVVideoCaptureSourceObserver alloc] initWithCaptureSource:this]))
    , m_device(avDevice)
    , m_zoomScaleFactor(cameraZoomScaleFactor([avDevice deviceType]))
#if PLATFORM(IOS_FAMILY)
    , m_startupTimer(*this, &AVVideoCaptureSource::startupTimerFired)
#endif
    , m_verifyCapturingTimer(*this, &AVVideoCaptureSource::verifyIsCapturing)
    , m_defaultTorchMode((int64_t)[m_device torchMode])
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

    stopSession();
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

#if PLATFORM(IOS_FAMILY)
void AVVideoCaptureSource::startupTimerFired()
{
    if (std::exchange(m_shouldCallNotifyMutedChange, false))
        notifyMutedChange(!m_isRunning);
}
#endif

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

#if PLATFORM(IOS_FAMILY)
    m_shouldCallNotifyMutedChange = false;
    static constexpr Seconds startupTimerInterval = 1_s;
    m_startupTimer.startOneShot(startupTimerInterval);
#endif
}

void AVVideoCaptureSource::stopProducingData()
{
    if (!m_session)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, !![m_session isRunning]);
    [m_objcObserver removeNotificationObservers];
    stopSession();

    m_interrupted = false;

#if PLATFORM(IOS_FAMILY)
    clearSession();
#endif
}

void AVVideoCaptureSource::stopSession()
{
    ASSERT(!m_beginConfigurationCount);

    @try {
        [m_session stopRunning];
    } @catch(NSException *exception) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "error calling -stopRunning ", [[exception name] UTF8String], ", reason : ", exception.reason);
    }

    rejectPendingPhotoRequest("Track stopped"_s);
}

void AVVideoCaptureSource::startApplyingConstraints()
{
    ASSERT(!m_hasBegunConfigurationForConstraints);
    m_hasBegunConfigurationForConstraints = false;
}

void AVVideoCaptureSource::endApplyingConstraints()
{
    if (!m_hasBegunConfigurationForConstraints)
        return;

    m_hasBegunConfigurationForConstraints = false;
    commitConfiguration();
}

void AVVideoCaptureSource::beginConfigurationForConstraintsIfNeeded()
{
    if (m_hasBegunConfigurationForConstraints)
        return;

    m_hasBegunConfigurationForConstraints = true;
    beginConfiguration();
}

void AVVideoCaptureSource::beginConfiguration()
{
    if (++m_beginConfigurationCount > 1)
        return;

    if (m_session)
        [m_session beginConfiguration];
}

void AVVideoCaptureSource::commitConfiguration()
{
    ASSERT(m_beginConfigurationCount);
    if (!m_beginConfigurationCount || --m_beginConfigurationCount > 0)
        return;

    if (m_session)
        [m_session commitConfiguration];
}

void AVVideoCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    m_currentSettings = std::nullopt;

    bool whiteBalanceModeChanged = settings.contains(RealtimeMediaSourceSettings::Flag::WhiteBalanceMode);
    bool torchChanged = settings.contains(RealtimeMediaSourceSettings::Flag::Torch);
    if (!whiteBalanceModeChanged && !torchChanged)
        return;

    scheduleDeferredTask([this, whiteBalanceModeChanged, torchChanged] {
        startApplyingConstraints();
        if (whiteBalanceModeChanged)
            updateWhiteBalanceMode();
        if (torchChanged)
            updateTorch();
        endApplyingConstraints();
    });
}

static bool isZoomSupported(const Vector<VideoPreset>& presets)
{
    return anyOf(presets, [](auto& preset) {
        return preset.isZoomSupported();
    });
}

static Vector<MeteringMode> supportedWhiteBalanceModes(AVCaptureDevice* device)
{
    Vector<MeteringMode> whiteBalanceModes;
    whiteBalanceModes.reserveInitialCapacity(3);

    if ([device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked])
        whiteBalanceModes.append(MeteringMode::Manual);
    if ([device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeAutoWhiteBalance])
        whiteBalanceModes.append(MeteringMode::SingleShot);
    if ([device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance])
        whiteBalanceModes.append(MeteringMode::Continuous);

    return whiteBalanceModes;
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
    settings.setBackgroundBlur(!!device().portraitEffectActive);

    RealtimeMediaSourceSupportedConstraints supportedConstraints;
    supportedConstraints.setSupportsDeviceId(true);
    supportedConstraints.setSupportsGroupId(true);
    supportedConstraints.setSupportsFacingMode([device() position] != AVCaptureDevicePositionUnspecified);
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsFrameRate(true);
    supportedConstraints.setSupportsBackgroundBlur(true);

    if (isZoomSupported(presets())) {
        supportedConstraints.setSupportsZoom(true);
        settings.setZoom(zoom());
    }

    if (!supportedWhiteBalanceModes(device()).isEmpty()) {
        supportedConstraints.setSupportsWhiteBalanceMode(true);
        settings.setWhiteBalanceMode(meteringModeFromAVCaptureWhiteBalanceMode([device() whiteBalanceMode]));
    }

    if ([device() hasTorch]) {
        supportedConstraints.setSupportsTorch(true);
        settings.setTorch([device() torchMode] == AVCaptureTorchModeOn);
    }

#if PLATFORM(IOS_FAMILY)
    supportedConstraints.setSupportsPowerEfficient(true);
    if (canBePowerEfficient())
        settings.setPowerEfficient(m_currentPreset ? m_currentPreset->isEfficient() : false);
#endif
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

    auto supportedConstraints = settings().supportedConstraints();

#if HAVE(AVCAPTUREDEVICE_MINFOCUSLENGTH)
    double minimumFocusDistance = [videoDevice minimumFocusDistance];
    if (minimumFocusDistance != -1.0) {
        ASSERT(minimumFocusDistance >= 0);
        supportedConstraints.setSupportsFocusDistance(true);
        capabilities.setFocusDistance({ minimumFocusDistance / 1000, std::numeric_limits<double>::max() });
    }
#endif // HAVE(AVCAPTUREDEVICE_MINFOCUSLENGTH)

    auto whiteBalanceModes = supportedWhiteBalanceModes(videoDevice);
    if (!whiteBalanceModes.isEmpty()) {
        supportedConstraints.setSupportsWhiteBalanceMode(true);
        capabilities.setWhiteBalanceModes(WTFMove(whiteBalanceModes));
    }

    if ([videoDevice hasTorch]) {
        supportedConstraints.setSupportsTorch(true);
        capabilities.setTorch(true);
    }

    capabilities.setBackgroundBlur(device().portraitEffectActive ? RealtimeMediaSourceCapabilities::BackgroundBlur::On : RealtimeMediaSourceCapabilities::BackgroundBlur::Off);

#if PLATFORM(IOS_FAMILY)
    supportedConstraints.setSupportsPowerEfficient(true);
    capabilities.setPowerEfficient(canBePowerEfficient());
#endif

    capabilities.setSupportedConstraints(supportedConstraints);
    updateCapabilities(capabilities);

    m_capabilities = WTFMove(capabilities);

    return *m_capabilities;
}

AVCapturePhotoOutput* AVVideoCaptureSource::photoOutput()
{
    assertIsCurrent(RunLoop::main());

    if (!m_photoOutput) {
        m_photoOutput = adoptNS([PAL::allocAVCapturePhotoOutputInstance() init]);

        if (!m_photoOutput) {
            ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to allocate AVCapturePhotoOutput");
            return nullptr;
        }

        if (![session() canAddOutput:m_photoOutput.get()]) {
            ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to add photo output");
            return nullptr;
        }
        [session() addOutput:m_photoOutput.get()];
    }

    return m_photoOutput.get();
}

void AVVideoCaptureSource::resolvePendingPhotoRequest(Vector<uint8_t>&& data, const String& mimeType)
{
    Locker lock { m_photoLock };

    if (!m_photoProducer) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "no photo producer");
        return;
    }

    m_photoProducer->resolve(std::make_pair(WTFMove(data), mimeType));
    m_photoProducer = nullptr;
}

void AVVideoCaptureSource::rejectPendingPhotoRequest(const String& error)
{
    Locker lock { m_photoLock };

    if (!m_photoProducer)
        return;

    ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, error);
    m_photoProducer->reject(error);
    m_photoProducer = nullptr;
}

IntSize AVVideoCaptureSource::maxPhotoSizeForCurrentPreset(IntSize requestedSize) const
{
    ASSERT(isMainThread());

    CMVideoDimensions bestMaxPhotoSize;

    auto *format = [m_device activeFormat];
    if ([format respondsToSelector:@selector(supportedMaxPhotoDimensions)]) {
        NSArray<NSValue*> *maxPhotoDimensions = format.supportedMaxPhotoDimensions;
        if (!maxPhotoDimensions.count)
            return { };

        bestMaxPhotoSize = maxPhotoDimensions.firstObject.CMVideoDimensionsValue;
        for (NSValue *value in maxPhotoDimensions) {
            CMVideoDimensions dimensions = value.CMVideoDimensionsValue;
            if (dimensions.width >= requestedSize.width() && dimensions.height >= requestedSize.height()) {
                if (dimensions.width * dimensions.height < bestMaxPhotoSize.width * bestMaxPhotoSize.height)
                    bestMaxPhotoSize = dimensions;
            }
        }
    } else {
        if (!m_currentPreset)
            return { };

        return m_currentPreset->size();
    }

    return { bestMaxPhotoSize.width, bestMaxPhotoSize.height };
}

RetainPtr<AVCapturePhotoSettings> AVVideoCaptureSource::photoConfiguration(const PhotoSettings& photoSettings)
{
    assertIsCurrent(RunLoop::main());

    IntSize requestedPhotoDimensions = { 0, 0 };
    if (photoSettings.imageHeight && photoSettings.imageWidth)
        requestedPhotoDimensions = { static_cast<int>(*photoSettings.imageWidth), static_cast<int>(*photoSettings.imageHeight) };

    AVCapturePhotoSettings* avPhotoSettings = [PAL::getAVCapturePhotoSettingsClass() photoSettingsWithFormat:@{
        AVVideoCodecKey : AVVideoCodecTypeJPEG,
        AVVideoCompressionPropertiesKey : @{ AVVideoQualityKey : @(1) }
    }];

#if PLATFORM(IOS_FAMILY)
    auto* photoOutput = this->photoOutput();
    ASSERT(photoOutput);
    if (!photoOutput)
        return nullptr;

    if (photoSettings.fillLightMode) {
        auto flashMode = toAVCaptureFlashMode(*photoSettings.fillLightMode);
        if ([photoOutput.supportedFlashModes containsObject:@(flashMode)])
            [avPhotoSettings setFlashMode:flashMode];
    }

    if (photoSettings.redEyeReduction && photoOutput.isAutoRedEyeReductionSupported)
        [avPhotoSettings setAutoRedEyeReductionEnabled:!!photoSettings.redEyeReduction.value()];
#endif

    requestedPhotoDimensions = maxPhotoSizeForCurrentPreset(requestedPhotoDimensions);
    if (!requestedPhotoDimensions.isEmpty() && [avPhotoSettings respondsToSelector:@selector(setMaxPhotoDimensions:)])
        [avPhotoSettings setMaxPhotoDimensions: { requestedPhotoDimensions.width(), requestedPhotoDimensions.height() }];

    return avPhotoSettings;
}

auto AVVideoCaptureSource::takePhotoInternal(PhotoSettings&& photoSettings) -> Ref<TakePhotoNativePromise>
{
    assertIsCurrent(RunLoop::main());

    RetainPtr<AVCapturePhotoOutput> photoOutput = this->photoOutput();
    if (!photoOutput)
        return TakePhotoNativePromise::createAndReject("Internal error"_s);

    RefPtr<TakePhotoNativePromise> promise;
    {
        Locker lock { m_photoLock };
        if (m_photoProducer) {
            ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "m_photoProducer should be NULL!");
            return TakePhotoNativePromise::createAndReject("Internal error"_s);
        }

        m_photoProducer = makeUnique<TakePhotoNativePromise::Producer>();
        promise = static_cast<Ref<TakePhotoNativePromise>>(*m_photoProducer);
    }

    auto avPhotoSettings = photoConfiguration(photoSettings);
    if (!avPhotoSettings) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "photoConfiguration() failed");
        return TakePhotoNativePromise::createAndReject("Internal error"_s);
    }

    photoQueue().dispatch([this, protectedThis = Ref { *this }, avPhotoSettings = WTFMove(avPhotoSettings), photoOutput = WTFMove(photoOutput)] {
        ASSERT(!isMainThread());

        if ([avPhotoSettings respondsToSelector:@selector(setMaxPhotoDimensions:)]) {
            auto requestedPhotoDimensions = [avPhotoSettings maxPhotoDimensions];
            if (requestedPhotoDimensions.width && requestedPhotoDimensions.height) {
                auto currentMaxPhotoDimensions = [photoOutput maxPhotoDimensions];
                if (requestedPhotoDimensions.width > currentMaxPhotoDimensions.width || requestedPhotoDimensions.height > currentMaxPhotoDimensions.height)
                    [photoOutput setMaxPhotoDimensions:requestedPhotoDimensions];
            }
        }

        [photoOutput capturePhotoWithSettings:avPhotoSettings.get() delegate:m_objcObserver.get()];
    });

    return promise.releaseNonNull();
}

auto AVVideoCaptureSource::getPhotoCapabilities() -> Ref<PhotoCapabilitiesNativePromise>
{
    if (m_photoCapabilities)
        return PhotoCapabilitiesNativePromise::createAndResolve(*m_photoCapabilities);

    auto capabilities = this->capabilities();
    PhotoCapabilities photoCapabilities;

    auto height = capabilities.height();
    photoCapabilities.imageHeight = { height.max(), height.min(), 1 };

    auto width = capabilities.width();
    photoCapabilities.imageWidth = { width.max(), width.min(), 1 };

    m_photoCapabilities = WTFMove(photoCapabilities);

    return PhotoCapabilitiesNativePromise::createAndResolve(*m_photoCapabilities);
}

auto AVVideoCaptureSource::getPhotoSettings() -> Ref<PhotoSettingsNativePromise>
{
    ASSERT(isMainThread());

    PhotoSettings settings;

    std::optional<FillLightMode> fillLightMode;
    if ([device() hasTorch])
        fillLightMode = { toFillLightMode([device() torchMode]) };

    auto trackSettings = this->settings();
    settings = { fillLightMode, trackSettings.height(), trackSettings.width(), { } };

    return PhotoSettingsNativePromise::createAndResolve(settings);
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

void AVVideoCaptureSource::applyFrameRateAndZoomWithPreset(double requestedFrameRate, double requestedZoom, std::optional<VideoPreset>&& preset)
{
    requestedZoom *= m_zoomScaleFactor;
    if (m_currentFrameRate == requestedFrameRate && m_currentZoom == requestedZoom && preset && m_currentPreset && preset->format() == m_currentPreset->format())
        return;

    beginConfigurationForConstraintsIfNeeded();

    m_currentPreset = WTFMove(preset);
    if (m_currentPreset)
        setIntrinsicSize({ m_currentPreset->size().width(), m_currentPreset->size().height() });

    m_currentFrameRate = requestedFrameRate;
    m_currentZoom = requestedZoom;

    setSessionSizeFrameRateAndZoom();

#if PLATFORM(IOS_FAMILY)
    // Updating the device configuration may switch off the torch. We reenable torch asynchronously if needed.
    if (torch()) {
        scheduleDeferredTask([this] {
            startApplyingConstraints();
            updateTorch();
            endApplyingConstraints();
        });
    }
#endif
}

static bool isFrameRateMatching(double frameRate, AVCaptureDevice* device)
{
    auto activeVideoMinFrameDuration = device.activeVideoMinFrameDuration;
    auto activeVideoMaxFrameDuration = device.activeVideoMaxFrameDuration;
    if (CMTIME_IS_INVALID(activeVideoMinFrameDuration) || CMTIME_IS_INVALID(activeVideoMaxFrameDuration))
        return false;

    auto frameDuration = PAL::CMTimeMake(1, frameRate);
    return PAL::CMTimeCompare(frameDuration, activeVideoMinFrameDuration) >= 0 && PAL::CMTimeCompare(frameDuration, activeVideoMaxFrameDuration) <= 0;
}

bool AVVideoCaptureSource::areSettingsMatching() const
{
    return m_appliedPreset && m_appliedPreset->format() == m_currentPreset->format() &&
#if PLATFORM(IOS_FAMILY)
        device().videoZoomFactor == m_currentZoom &&
#endif
        isFrameRateMatching(m_currentFrameRate, device());
}

void AVVideoCaptureSource::setSessionSizeFrameRateAndZoom()
{
    ASSERT(m_beginConfigurationCount);
    if (!m_session)
        return;

    if (!m_currentPreset)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, SizeFrameRateAndZoom { m_currentPreset->size().width(), m_currentPreset->size().height(), m_currentFrameRate, m_currentZoom }
#if PLATFORM(IOS_FAMILY)
        , " binned: ", !!m_currentPreset->format().isVideoBinned
#endif
    );

    if (areSettingsMatching()) {
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, " settings already match");
        return;
    }

    ASSERT(m_currentPreset->format());

    if (!lockForConfiguration())
        return;

    beginConfiguration();
    @try {
        [device() setActiveFormat:m_currentPreset->format()];

#if PLATFORM(MAC)
        auto settingsDictionary = @{
            (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(preferedPixelBufferFormat()),
            (__bridge NSString *)kCVPixelBufferWidthKey: @(m_currentPreset->size().width()),
            (__bridge NSString *)kCVPixelBufferHeightKey: @(m_currentPreset->size().height()),
            (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ }
        };
        [m_videoOutput setVideoSettings:settingsDictionary];
#endif

        auto* frameRateRange = frameDurationForFrameRate(m_currentFrameRate);
        ASSERT(frameRateRange);
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
        if (m_currentZoom != device().videoZoomFactor) {
            ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "setting zoom to ", m_currentZoom);
            [device() setVideoZoomFactor:m_currentZoom];
        }
#endif

        m_appliedPreset = m_currentPreset;
    } @catch(NSException *exception) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "error configuring device ", exception.name, ", reason : ", exception.reason);
        ASSERT_NOT_REACHED();
    }

    [device() unlockForConfiguration];
    commitConfiguration();
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

bool AVVideoCaptureSource::lockForConfiguration()
{
    ASSERT(device());

    NSError *error = nil;
    if ([device() lockForConfiguration:&error])
        return true;

    ERROR_LOG_IF(loggerPtr() && error, LOGIDENTIFIER, "error locking configuration ", error);
    ERROR_LOG_IF(loggerPtr() && !error, LOGIDENTIFIER, "unknown error locking configuration");

    return false;
}

void AVVideoCaptureSource::updateWhiteBalanceMode()
{
    if (!m_isRunning) {
        m_needsWhiteBalanceReconfiguration = true;
        return;
    }

    beginConfigurationForConstraintsIfNeeded();

    if (!lockForConfiguration())
        return;

    auto* device = this->device();
    @try {
        device.whiteBalanceMode = whiteBalanceModeFromMeteringMode(whiteBalanceMode());
    } @catch(NSException *exception) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "error setting white balance mode ", [[exception name] UTF8String], ", reason : ", exception.reason);
    }

    [device unlockForConfiguration];
}

void AVVideoCaptureSource::updateTorch()
{
    if (!m_isRunning) {
        m_needsTorchReconfiguration = true;
        return;
    }

    beginConfigurationForConstraintsIfNeeded();

    if (!lockForConfiguration())
        return;

    auto* device = this->device();
    @try {
        if (torch()) {
            NSError *error = nil;
            if (![device setTorchModeOnWithLevel:AVCaptureMaxAvailableTorchLevel error:&error]) {
                ERROR_LOG_IF(loggerPtr() && error, LOGIDENTIFIER, "error turning on torch ", error);
                ERROR_LOG_IF(loggerPtr() && !error, LOGIDENTIFIER, "unknown error on torch");
            }
        } else
            [device setTorchMode:(AVCaptureTorchMode)m_defaultTorchMode];

    } @catch(NSException *exception) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "error turning on torch ", exception.name, ", reason : ", exception.reason);
    }

    [device unlockForConfiguration];
}

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

#if ENABLE(EXTENSION_CAPABILITIES)
    String mediaEnvironment = RealtimeMediaSourceCenter::singleton().currentMediaEnvironment();
    WARNING_LOG_IF(loggerPtr() && mediaEnvironment.isEmpty(), "Media environment is empty");
    // FIXME (119325252): Remove staging code for -[AVCaptureSession initWithMediaEnvironment:]
    if (!mediaEnvironment.isEmpty() && [PAL::getAVCaptureSessionClass() instancesRespondToSelector:@selector(initWithMediaEnvironment:)])
        m_session = adoptNS([PAL::allocAVCaptureSessionInstance() initWithMediaEnvironment:mediaEnvironment]);
#endif

#if ENABLE(APP_PRIVACY_REPORT)
    if (!m_session) {
        auto identity = RealtimeMediaSourceCenter::singleton().identity();
        ERROR_LOG_IF(loggerPtr() && !identity, LOGIDENTIFIER, "RealtimeMediaSourceCenter::identity() returned null!");

        if (identity && [PAL::getAVCaptureSessionClass() instancesRespondToSelector:@selector(initWithAssumedIdentity:)])
            m_session = adoptNS([PAL::allocAVCaptureSessionInstance() initWithAssumedIdentity:identity.get()]);
    }
#endif

    if (!m_session) {
#if ENABLE(EXTENSION_CAPABILITIES)
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "allocating AVCaptureSession without media environment nor identity");
#endif
        m_session = adoptNS([PAL::allocAVCaptureSessionInstance() init]);
    }

    if (!m_session) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "failed to allocate AVCaptureSession");
        captureFailed();
        return false;
    }

#if PLATFORM(APPLETV)
    [m_session setMultitaskingCameraAccessEnabled:YES];
#elif PLATFORM(IOS_FAMILY)
    PAL::AVCaptureSessionSetAuthorizedToUseCameraInMultipleForegroundAppLayout(m_session.get());
#endif
    [m_session addObserver:m_objcObserver.get() forKeyPath:@"running" options:NSKeyValueObservingOptionNew context:(void *)nil];

    bool success = setupCaptureSession();
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

    beginConfiguration();
    auto scopeExit = WTF::makeScopeExit([&] { commitConfiguration(); });

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
    auto settingsDictionary = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys: @(preferedPixelBufferFormat()), kCVPixelBufferPixelFormatTypeKey, nil]);

    [m_videoOutput setVideoSettings:settingsDictionary.get()];
    [m_videoOutput setAlwaysDiscardsLateVideoFrames:YES];
    [m_videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];

    if (![session() canAddOutput:m_videoOutput.get()]) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to add video output device");
        return false;
    }
    [session() addOutput:m_videoOutput.get()];

    setSessionSizeFrameRateAndZoom();
    m_needsTorchReconfiguration = m_needsTorchReconfiguration || torch();

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
        m_useSensorAndDeviceOrientation = false;
#endif

    notifier.addObserver(*this);
    orientationChanged(notifier.orientation());
}

void AVVideoCaptureSource::orientationChanged(IntDegrees orientation)
{
    ASSERT(orientation == 0 || orientation == 90 || orientation == -90 || orientation == 180);
    m_deviceOrientation = orientation;
    computeVideoFrameRotation();
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "rotation = ", m_videoFrameRotation, ", orientation = ", m_deviceOrientation);
}

void AVVideoCaptureSource::rotationAngleForHorizonLevelDisplayChanged(const String& devicePersistentId, VideoFrameRotation videoFrameRotation)
{
    if (captureDevice().persistentId() != devicePersistentId)
        return;

    m_useSensorAndDeviceOrientation = false;

    if (videoFrameRotation == m_videoFrameRotation)
        return;

    m_videoFrameRotation = videoFrameRotation;
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "rotation = ", m_videoFrameRotation);
    notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });
}

void AVVideoCaptureSource::computeVideoFrameRotation()
{
    if (!m_useSensorAndDeviceOrientation)
        return;

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

void AVVideoCaptureSource::captureOutputDidFinishProcessingPhoto(RetainPtr<AVCapturePhotoOutput>, RetainPtr<AVCapturePhoto> photo, RetainPtr<NSError> error)
{
    if (error) {
        rejectPendingPhotoRequest("AVCapturePhotoOutput failed"_s);
        RunLoop::main().dispatch([this, protectedThis = Ref { *this }, logIdentifier = LOGIDENTIFIER, error = WTFMove(error) ] {
            ASSERT(isMainThread());
            ALWAYS_LOG_IF(loggerPtr(), logIdentifier, "failed: ", [error code], ", ", error.get());
        });
        return;
    }

    NSData* data = [photo fileDataRepresentation];
    resolvePendingPhotoRequest(makeVector(data), "image/jpeg"_s);
}

void AVVideoCaptureSource::reconfigureIfNeeded()
{
    if (!m_isRunning || (!m_needsTorchReconfiguration && !m_needsWhiteBalanceReconfiguration))
        return;

    startApplyingConstraints();

    if (std::exchange(m_needsTorchReconfiguration, false))
        updateTorch();

    if (std::exchange(m_needsWhiteBalanceReconfiguration, false))
        updateWhiteBalanceMode();

    endApplyingConstraints();
}

void AVVideoCaptureSource::captureSessionIsRunningDidChange(bool state)
{
    scheduleDeferredTask([this, logIdentifier = LOGIDENTIFIER, state] {
        ALWAYS_LOG_IF(loggerPtr(), logIdentifier, state);
        if ((state == m_isRunning) && (state == !muted()))
            return;

        m_isRunning = state;

        reconfigureIfNeeded();

        updateVerifyCapturingTimer();

#if PLATFORM(IOS_FAMILY)
        if (m_startupTimer.isActive()) {
            m_shouldCallNotifyMutedChange = true;
            return;
        }
#endif

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

static bool isFormatPowerEfficient(AVCaptureDeviceFormat* format)
{
#if PLATFORM(IOS_FAMILY)
    return format.isVideoBinned;
#else
    UNUSED_PARAM(format);
    return false;
#endif
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

        VideoPreset preset { size, WTFMove(frameRates), computeMinZoom(), computeMaxZoom(format), isFormatPowerEfficient(format) };
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

- (id)initWithCaptureSource:(AVVideoCaptureSource*)captureSource
{
    self = [super init];
    if (!self)
        return nil;

    m_captureSource = captureSource;

    return self;
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self removeNotificationObservers];
    m_captureSource = nullptr;
}

- (void)addNotificationObservers
{
    auto source = m_captureSource.get();
    ASSERT(source);

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    [center addObserver:self selector:@selector(deviceConnectedDidChange:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];

#if PLATFORM(IOS_FAMILY)
    AVCaptureSession* session = source->session();
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
    if (auto source = m_captureSource.get())
        source->captureOutputDidOutputSampleBufferFromConnection(captureOutput, sampleBuffer, connection);
}

- (void)captureOutput:(AVCapturePhotoOutput *)captureOutput didFinishProcessingPhoto:(AVCapturePhoto *)photo error:(NSError *)error
{
    if (auto source = m_captureSource.get())
        source->captureOutputDidFinishProcessingPhoto(captureOutput, photo, error);
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(context);

    auto source = m_captureSource.get();
    if (!source)
        return;

    id newValue = [change valueForKey:NSKeyValueChangeNewKey];
    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];

#if !RELEASE_LOG_DISABLED
    if (source->loggerPtr()) {
        auto identifier = Logger::LogSiteIdentifier("AVVideoCaptureSource"_s, "observeValueForKeyPath"_s, source->logIdentifier());
        RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
        source->logger().logAlways(source->logChannel(), identifier, willChange ? "will" : "did", " change '", [keyPath UTF8String], "' to ", [valueString UTF8String]);
    }
#endif

    if (willChange)
        return;

    if ([keyPath isEqualToString:@"running"])
        source->captureSessionIsRunningDidChange([newValue boolValue]);
    if ([keyPath isEqualToString:@"suspended"])
        source->captureDeviceSuspendedDidChange();
}

- (void)deviceConnectedDidChange:(NSNotification*)notification
{
    if (auto source = m_captureSource.get())
        source->deviceDisconnected(notification);
}

#if PLATFORM(IOS_FAMILY)
- (void)sessionRuntimeError:(NSNotification*)notification
{
    NSError *error = notification.userInfo[AVCaptureSessionErrorKey];
    if (auto source = m_captureSource.get())
        source->captureSessionRuntimeError(error);
}

- (void)beginSessionInterrupted:(NSNotification*)notification
{
    if (auto source = m_captureSource.get())
        source->captureSessionBeginInterruption(notification);
}

- (void)endSessionInterrupted:(NSNotification*)notification
{
    if (auto source = m_captureSource.get())
        source->captureSessionEndInterruption(notification);
}
#endif

@end

#endif // ENABLE(MEDIA_STREAM) && HAVE(AVCAPTUREDEVICE)
