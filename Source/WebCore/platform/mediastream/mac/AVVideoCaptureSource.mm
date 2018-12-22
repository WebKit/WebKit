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
#import "ImageTransferSessionVT.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeVideoUtilities.h"
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

SOFT_LINK_CONSTANT(AVFoundation, AVCaptureDeviceWasDisconnectedNotification, NSString *)
#define AVCaptureDeviceWasDisconnectedNotification getAVCaptureDeviceWasDisconnectedNotification()

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionRuntimeErrorNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionWasInterruptedNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionInterruptionEndedNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionInterruptionReasonKey, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionErrorKey, NSString *)

#define AVCaptureSessionRuntimeErrorNotification getAVCaptureSessionRuntimeErrorNotification()
#define AVCaptureSessionWasInterruptedNotification getAVCaptureSessionWasInterruptedNotification()
#define AVCaptureSessionInterruptionEndedNotification getAVCaptureSessionInterruptionEndedNotification()
#define AVCaptureSessionInterruptionReasonKey getAVCaptureSessionInterruptionReasonKey()
#define AVCaptureSessionErrorKey getAVCaptureSessionErrorKey()
#endif

using namespace WebCore;
using namespace PAL;

@interface WebCoreAVVideoCaptureSourceObserver : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
    AVVideoCaptureSource* m_callback;
}

-(id)initWithCallback:(AVVideoCaptureSource*)callback;
-(void)disconnect;
-(void)addNotificationObservers;
-(void)removeNotificationObservers;
-(void)captureOutput:(AVCaptureOutputType*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType*)connection;
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
    // FIXME: Use preferedPixelBufferFormat() once rdar://problem/44391444 is fixed.
#if PLATFORM(MAC)
    return kCVPixelFormatType_420YpCbCr8Planar;
#else
    return preferedPixelBufferFormat();
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
    static Ref<AVVideoPreset> create(IntSize size, Vector<FrameRateRange>&& frameRateRanges, AVCaptureDeviceFormatType* format)
    {
        return adoptRef(*new AVVideoPreset(size, WTFMove(frameRateRanges), format));
    }

    AVVideoPreset(IntSize size, Vector<FrameRateRange>&& frameRateRanges, AVCaptureDeviceFormatType* format)
        : VideoPreset(size, WTFMove(frameRateRanges), AVCapture)
        , format(format)
    {
    }

    RetainPtr<AVCaptureDeviceFormatType> format;
};

CaptureSourceOrError AVVideoCaptureSource::create(String&& id, String&& hashSalt, const MediaConstraints* constraints)
{
    AVCaptureDeviceTypedef *device = [getAVCaptureDeviceClass() deviceWithUniqueID:id];
    if (!device)
        return { };

    auto source = adoptRef(*new AVVideoCaptureSource(device, WTFMove(id), WTFMove(hashSalt)));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return WTFMove(result.value().first);
    }

    return CaptureSourceOrError(WTFMove(source));
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDeviceTypedef* device, String&& id, String&& hashSalt)
    : RealtimeVideoSource(device.localizedName, WTFMove(id), WTFMove(hashSalt))
    , m_objcObserver(adoptNS([[WebCoreAVVideoCaptureSourceObserver alloc] initWithCallback:this]))
    , m_device(device)
{
#if PLATFORM(IOS_FAMILY)
    static_assert(static_cast<int>(InterruptionReason::VideoNotAllowedInBackground) == static_cast<int>(AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground), "InterruptionReason::VideoNotAllowedInBackground is not AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground as expected");
    static_assert(static_cast<int>(InterruptionReason::VideoNotAllowedInSideBySide) == AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps, "InterruptionReason::VideoNotAllowedInSideBySide is not AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps as expected");
    static_assert(static_cast<int>(InterruptionReason::VideoInUse) == AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient, "InterruptionReason::VideoInUse is not AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient as expected");
    static_assert(static_cast<int>(InterruptionReason::AudioInUse) == AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient, "InterruptionReason::AudioInUse is not AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient as expected");
#endif
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
    [m_objcObserver disconnect];

    if (!m_session)
        return;

    [m_session removeObserver:m_objcObserver.get() forKeyPath:@"running"];
    [m_device removeObserver:m_objcObserver.get() forKeyPath:@"suspended"];
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
#if PLATFORM(IOS_FAMILY)
    m_session = nullptr;
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
    m_currentSettings = WTF::nullopt;
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

    settings.setFrameRate(frameRate());
    auto& size = this->size();
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

    AVCaptureDeviceTypedef *videoDevice = device();
    if ([videoDevice position] == AVCaptureDevicePositionFront)
        capabilities.addFacingMode(RealtimeMediaSourceSettings::User);
    if ([videoDevice position] == AVCaptureDevicePositionBack)
        capabilities.addFacingMode(RealtimeMediaSourceSettings::Environment);

    updateCapabilities(capabilities);

    m_capabilities = WTFMove(capabilities);

    return *m_capabilities;
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

void AVVideoCaptureSource::setSizeAndFrameRateWithPreset(IntSize requestedSize, double requestedFrameRate, RefPtr<VideoPreset> preset)
{
    auto* avPreset = preset ? downcast<AVVideoPreset>(preset.get()) : nullptr;

    if (!m_session) {
        m_pendingPreset = avPreset;
        m_pendingSize = requestedSize;
        m_pendingFrameRate = requestedFrameRate;
        return;
    }

    m_pendingPreset = nullptr;
    m_pendingFrameRate = 0;

    auto* frameRateRange = frameDurationForFrameRate(requestedFrameRate);
    ASSERT(frameRateRange);
    if (!frameRateRange)
        return;

    if (!avPreset)
        return;

    ASSERT(avPreset->format);

    NSError *error = nil;
    [m_session beginConfiguration];
    @try {
        if ([device() lockForConfiguration:&error]) {
            if (!m_currentPreset || ![m_currentPreset->format.get() isEqual:avPreset->format.get()]) {
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
            }
            [device() setActiveVideoMinFrameDuration:[frameRateRange minFrameDuration]];
            [device() setActiveVideoMaxFrameDuration:[frameRateRange maxFrameDuration]];
            [device() unlockForConfiguration];
        }
    } @catch(NSException *exception) {
        RELEASE_LOG(Media, "AVVideoCaptureSource::setFrameRate - exception thrown configuring device: <%s> %s", [[exception name] UTF8String], [[exception reason] UTF8String]);
    }
    [m_session commitConfiguration];

    m_currentPreset = avPreset;

    if (error)
        RELEASE_LOG(Media, "AVVideoCaptureSource::setFrameRate - failed to lock video device for configuration: %s", [[error localizedDescription] UTF8String]);
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
    [m_session addObserver:m_objcObserver.get() forKeyPath:@"running" options:NSKeyValueObservingOptionNew context:(void *)nil];
    [m_device addObserver:m_objcObserver.get() forKeyPath:@"suspended" options:NSKeyValueObservingOptionNew context:(void *)nil];

    [m_session beginConfiguration];
    bool success = setupCaptureSession();
    [m_session commitConfiguration];

    if (!success)
        captureFailed();

    return success;
}

AVFrameRateRangeType* AVVideoCaptureSource::frameDurationForFrameRate(double rate)
{
    AVFrameRateRangeType *bestFrameRateRange = nil;
    for (AVFrameRateRangeType *frameRateRange in [[device() activeFormat] videoSupportedFrameRateRanges]) {
        if (frameRateRangeIncludesRate({ [frameRateRange minFrameRate], [frameRateRange maxFrameRate] }, rate)) {
            if (!bestFrameRateRange || CMTIME_COMPARE_INLINE([frameRateRange minFrameDuration], >, [bestFrameRateRange minFrameDuration]))
                bestFrameRateRange = frameRateRange;
        }
    }

    if (!bestFrameRateRange)
        RELEASE_LOG(Media, "AVVideoCaptureSource::frameDurationForFrameRate, no frame rate range for rate %g", rate);

    return bestFrameRateRange;
}

bool AVVideoCaptureSource::setupCaptureSession()
{
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
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
    auto settingsDictionary = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys: [NSNumber numberWithInt:avVideoCapturePixelBufferFormat()], kCVPixelBufferPixelFormatTypeKey, nil]);

    [m_videoOutput setVideoSettings:settingsDictionary.get()];
    [m_videoOutput setAlwaysDiscardsLateVideoFrames:YES];
    [m_videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];

    if (![session() canAddOutput:m_videoOutput.get()]) {
        RELEASE_LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video sample buffer output delegate", this);
        return false;
    }
    [session() addOutput:m_videoOutput.get()];

    if (m_pendingPreset || m_pendingFrameRate)
        setSizeAndFrameRateWithPreset(m_pendingSize, m_pendingFrameRate, m_pendingPreset);

    m_sensorOrientation = sensorOrientationFromVideoOutput(m_videoOutput.get());
    computeSampleRotation();

    return true;
}

void AVVideoCaptureSource::shutdownCaptureSession()
{
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

void AVVideoCaptureSource::processNewFrame(Ref<MediaSample>&& sample)
{
    if (!isProducingData() || muted())
        return;

    m_buffer = &sample.get();
    setIntrinsicSize(expandedIntSize(sample->presentationSize()));
    dispatchMediaSampleToObservers(WTFMove(sample));
}

void AVVideoCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType* captureConnection)
{
    if (m_framesToDropAtStartup && m_framesToDropAtStartup--)
        return;

    auto sample = MediaSampleAVFObjC::create(sampleBuffer, m_sampleRotation, [captureConnection isVideoMirrored]);
    scheduleDeferredTask([this, sample = WTFMove(sample)] () mutable {
        processNewFrame(WTFMove(sample));
    });
}

void AVVideoCaptureSource::captureSessionIsRunningDidChange(bool state)
{
    scheduleDeferredTask([this, state] {
        if ((state == m_isRunning) && (state == !muted()))
            return;

        m_isRunning = state;
        if (m_isRunning)
            m_framesToDropAtStartup = 4;

        notifyMutedChange(!m_isRunning);
    });
}

void AVVideoCaptureSource::captureDeviceSuspendedDidChange()
{
#if !PLATFORM(IOS_FAMILY)
    scheduleDeferredTask([this] {
        auto isSuspended = [m_device isSuspended];
        if (isSuspended == muted())
            return;

        notifyMutedChange(isSuspended);
    });
#endif
}

bool AVVideoCaptureSource::interrupted() const
{
    if (m_interruption != InterruptionReason::None)
        return true;

    return RealtimeMediaSource::interrupted();
}

void AVVideoCaptureSource::generatePresets()
{
    Vector<Ref<VideoPreset>> presets;
    for (AVCaptureDeviceFormatType* format in [device() formats]) {

        CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
        IntSize size = { dimensions.width, dimensions.height };
        auto index = presets.findMatching([&size](auto& preset) {
            return size == preset->size;
        });
        if (index != notFound)
            continue;

        Vector<FrameRateRange> frameRates;
        for (AVFrameRateRangeType *range in [format videoSupportedFrameRateRanges])
            frameRates.append({ range.minFrameRate, range.maxFrameRate});

        presets.append(AVVideoPreset::create(size, WTFMove(frameRates), format));
    }

    setSupportedPresets(WTFMove(presets));
}

#if PLATFORM(IOS_FAMILY)
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

void AVVideoCaptureSource::deviceDisconnected(RetainPtr<NSNotification> notification)
{
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
    AVCaptureSessionType* session = m_callback->session();
    [center addObserver:self selector:@selector(sessionRuntimeError:) name:AVCaptureSessionRuntimeErrorNotification object:session];
    [center addObserver:self selector:@selector(beginSessionInterrupted:) name:AVCaptureSessionWasInterruptedNotification object:session];
    [center addObserver:self selector:@selector(endSessionInterrupted:) name:AVCaptureSessionInterruptionEndedNotification object:session];
#endif
}

- (void)removeNotificationObservers
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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

    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];
#if !LOG_DISABLED

    if (willChange)
        LOG(Media, "WebCoreAVVideoCaptureSourceObserver::observeValueForKeyPath(%p) - will change, keyPath = %s", self, [keyPath UTF8String]);
    else {
        RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
        LOG(Media, "WebCoreAVVideoCaptureSourceObserver::observeValueForKeyPath(%p) - did change, keyPath = %s, value = %s", self, [keyPath UTF8String], [valueString.get() UTF8String]);
    }
#endif

    if (!willChange && [keyPath isEqualToString:@"running"])
        m_callback->captureSessionIsRunningDidChange([newValue boolValue]);
    if (!willChange && [keyPath isEqualToString:@"suspended"])
        m_callback->captureDeviceSuspendedDidChange();
}

- (void)deviceConnectedDidChange:(NSNotification*)notification
{
    LOG(Media, "WebCoreAVVideoCaptureSourceObserver::deviceConnectedDidChange(%p)", self);

    if (m_callback)
        m_callback->deviceDisconnected(notification);
}

#if PLATFORM(IOS_FAMILY)
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
