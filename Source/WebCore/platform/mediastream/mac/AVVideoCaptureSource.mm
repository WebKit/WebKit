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

#import "AVCaptureDeviceManager.h"
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "PixelBufferConformerCV.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceCenterMac.h"
#import "RealtimeMediaSourceSettings.h"
#import "WebActionDisablingCALayerDelegate.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVCaptureInput.h>
#import <AVFoundation/AVCaptureOutput.h>
#import <AVFoundation/AVCaptureSession.h>
#import <AVFoundation/AVCaptureVideoPreviewLayer.h>
#import <objc/runtime.h>

#if PLATFORM(IOS)
#include "WebCoreThread.h"
#include "WebCoreThreadRun.h"
#endif

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceTypedef;
typedef AVCaptureDeviceFormat AVCaptureDeviceFormatType;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureVideoDataOutput AVCaptureVideoDataOutputType;
typedef AVFrameRateRange AVFrameRateRangeType;
typedef AVCaptureVideoPreviewLayer AVCaptureVideoPreviewLayerType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceFormat)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoPreviewLayer)
SOFT_LINK_CLASS(AVFoundation, AVFrameRateRange)

#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceFormat getAVCaptureDeviceFormatClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVCaptureVideoDataOutput getAVCaptureVideoDataOutputClass()
#define AVCaptureVideoPreviewLayer getAVCaptureVideoPreviewLayerClass()
#define AVFrameRateRange getAVFrameRateRangeClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionPreset960x540, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionPreset320x240, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionPresetLow, NSString *)

#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset960x540 getAVCaptureSessionPreset960x540()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPreset320x240 getAVCaptureSessionPreset320x240()
#define AVCaptureSessionPresetLow getAVCaptureSessionPresetLow()

using namespace WebCore;

namespace WebCore {

#if PLATFORM(MAC)
const OSType videoCaptureFormat = kCVPixelFormatType_420YpCbCr8Planar;
#else
const OSType videoCaptureFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
#endif

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
    : AVMediaCaptureSource(device, id, Type::Video)
{
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenterMac::videoCaptureSourceFactory().unsetActiveSource(*this);
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

void AVVideoCaptureSource::initializeCapabilities(RealtimeMediaSourceCapabilities& capabilities)
{
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

        if (AVCaptureSessionPreset1280x720 && [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset1280x720]) {
            updateSizeMinMax(minimumWidth, maximumWidth, 1280);
            updateSizeMinMax(minimumHeight, maximumHeight, 720);
            updateAspectRatioMinMax(minimumAspectRatio, maximumAspectRatio, 1280.0 / 720);
        }
        if (AVCaptureSessionPreset960x540 && [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset960x540]) {
            updateSizeMinMax(minimumWidth, maximumWidth, 960);
            updateSizeMinMax(minimumHeight, maximumHeight, 540);
            updateAspectRatioMinMax(minimumAspectRatio, maximumAspectRatio, 960 / 540);
        }
        if (AVCaptureSessionPreset640x480 && [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset640x480]) {
            updateSizeMinMax(minimumWidth, maximumWidth, 640);
            updateSizeMinMax(minimumHeight, maximumHeight, 480);
            updateAspectRatioMinMax(minimumAspectRatio, maximumAspectRatio, 640 / 480);
        }
        if (AVCaptureSessionPreset352x288 && [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset352x288]) {
            updateSizeMinMax(minimumWidth, maximumWidth, 352);
            updateSizeMinMax(minimumHeight, maximumHeight, 288);
            updateAspectRatioMinMax(minimumAspectRatio, maximumAspectRatio, 352 / 288);
        }
        if (AVCaptureSessionPreset320x240 && [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset320x240]) {
            updateSizeMinMax(minimumWidth, maximumWidth, 320);
            updateSizeMinMax(minimumHeight, maximumHeight, 240);
            updateAspectRatioMinMax(minimumAspectRatio, maximumAspectRatio, 320 / 240);
        }
    }

    capabilities.setFrameRate(CapabilityValueOrRange(lowestFrameRateRange, highestFrameRateRange));
    capabilities.setWidth(CapabilityValueOrRange(minimumWidth, maximumWidth));
    capabilities.setHeight(CapabilityValueOrRange(minimumHeight, maximumHeight));
    capabilities.setAspectRatio(CapabilityValueOrRange(minimumAspectRatio, maximumAspectRatio));
}

void AVVideoCaptureSource::initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints& supportedConstraints)
{
    supportedConstraints.setSupportsFacingMode([device() position] != AVCaptureDevicePositionUnspecified);
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsFrameRate(true);
}

void AVVideoCaptureSource::updateSettings(RealtimeMediaSourceSettings& settings)
{
    settings.setDeviceId(id());

    if ([device() position] == AVCaptureDevicePositionFront)
        settings.setFacingMode(RealtimeMediaSourceSettings::User);
    else if ([device() position] == AVCaptureDevicePositionBack)
        settings.setFacingMode(RealtimeMediaSourceSettings::Environment);
    else
        settings.setFacingMode(RealtimeMediaSourceSettings::Unknown);

    // FIXME: Observe frame rate changes.
    auto maxFrameDuration = [device() activeVideoMaxFrameDuration];
    settings.setFrameRate(maxFrameDuration.timescale / maxFrameDuration.value);
    settings.setWidth(m_width);
    settings.setHeight(m_height);
    settings.setAspectRatio(static_cast<float>(m_width) / m_height);
}

bool AVVideoCaptureSource::applySize(const IntSize& size)
{
    NSString *preset = bestSessionPresetForVideoDimensions(size.width(), size.height());
    if (!preset || ![session() canSetSessionPreset:preset]) {
        LOG(Media, "AVVideoCaptureSource::applySize(%p), unable find or set preset for width: %i, height: %i", this, size.width(), size.height());
        return false;
    }

    return setPreset(preset);
}

static IntSize sizeForPreset(NSString* preset)
{
    if (!preset)
        return { };

    if (AVCaptureSessionPreset1280x720 && [preset isEqualToString:AVCaptureSessionPreset1280x720])
        return { 1280, 720 };

    if (AVCaptureSessionPreset960x540 && [preset isEqualToString:AVCaptureSessionPreset960x540])
        return { 960, 540 };

    if (AVCaptureSessionPreset640x480 && [preset isEqualToString:AVCaptureSessionPreset640x480])
        return { 640, 480 };

    if (AVCaptureSessionPreset352x288 && [preset isEqualToString:AVCaptureSessionPreset352x288])
        return { 352, 288 };

    if (AVCaptureSessionPreset320x240 && [preset isEqualToString:AVCaptureSessionPreset320x240])
        return { 320, 240 };
    
    return { };
    
}

bool AVVideoCaptureSource::setPreset(NSString *preset)
{
    if (!session()) {
        m_pendingPreset = preset;
        return true;
    }

    auto size = sizeForPreset(preset);
    if (size.width() == m_width && size.height() == m_height)
        return true;

    @try {
        session().sessionPreset = preset;
#if PLATFORM(MAC)
        auto settingsDictionary = @{ (NSString*)kCVPixelBufferPixelFormatTypeKey: @(videoCaptureFormat), (NSString*)kCVPixelBufferWidthKey: @(size.width()), (NSString*)kCVPixelBufferHeightKey: @(size.height()), };
        [m_videoOutput setVideoSettings:settingsDictionary];
#endif
    } @catch(NSException *exception) {
        LOG(Media, "AVVideoCaptureSource::applySize(%p), exception thrown configuring device: <%s> %s", this, [[exception name] UTF8String], [[exception reason] UTF8String]);
        return false;
    }

    return true;
}

bool AVVideoCaptureSource::applyFrameRate(double rate)
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
        LOG(Media, "AVVideoCaptureSource::applyFrameRate(%p), frame rate %f not supported by video device", this, rate);
        return false;
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
        LOG(Media, "AVVideoCaptureSource::applyFrameRate(%p), exception thrown configuring device: <%s> %s", this, [[exception name] UTF8String], [[exception reason] UTF8String]);
        return false;
    }

    if (error) {
        LOG(Media, "AVVideoCaptureSource::applyFrameRate(%p), failed to lock video device for configuration: %s", this, [[error localizedDescription] UTF8String]);
        return false;
    }

    LOG(Media, "AVVideoCaptureSource::applyFrameRate(%p) - set frame rate range to %f", this, rate);
    return true;
}

void AVVideoCaptureSource::applySizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double> frameRate)
{
    if (width || height)
        setPreset(bestSessionPresetForVideoDimensions(WTFMove(width), WTFMove(height)));

    if (frameRate)
        applyFrameRate(frameRate.value());
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

bool AVVideoCaptureSource::setupCaptureSession()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenterMac::videoCaptureSourceFactory().setActiveSource(*this);
#endif

    NSError *error = nil;
    RetainPtr<AVCaptureDeviceInputType> videoIn = adoptNS([allocAVCaptureDeviceInputInstance() initWithDevice:device() error:&error]);
    if (error) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), failed to allocate AVCaptureDeviceInput: %s", this, [[error localizedDescription] UTF8String]);
        return false;
    }

    if (![session() canAddInput:videoIn.get()]) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video input device", this);
        return false;
    }
    [session() addInput:videoIn.get()];

    m_videoOutput = adoptNS([allocAVCaptureVideoDataOutputInstance() init]);
    auto settingsDictionary = adoptNS([[NSMutableDictionary alloc] initWithObjectsAndKeys: [NSNumber numberWithInt:videoCaptureFormat], kCVPixelBufferPixelFormatTypeKey, nil]);
    if (m_pendingPreset) {
#if PLATFORM(MAC)
        auto size = sizeForPreset(m_pendingPreset.get());
        [settingsDictionary.get() setObject:[NSNumber numberWithInt:size.width()] forKey:(NSString*)kCVPixelBufferWidthKey];
        [settingsDictionary.get() setObject:[NSNumber numberWithInt:size.height()] forKey:(NSString*)kCVPixelBufferHeightKey];
#endif
    }

    [m_videoOutput setVideoSettings:settingsDictionary.get()];
    [m_videoOutput setAlwaysDiscardsLateVideoFrames:YES];
    setVideoSampleBufferDelegate(m_videoOutput.get());

    if (![session() canAddOutput:m_videoOutput.get()]) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video sample buffer output delegate", this);
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
        m_width = dimensions.width;
        m_height = dimensions.height;

        settingsDidChange();
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

NSString* AVVideoCaptureSource::bestSessionPresetForVideoDimensions(std::optional<int> width, std::optional<int> height) const
{
    if (!width && !height)
        return nil;

    AVCaptureDeviceTypedef *videoDevice = device();
    if ((!width || width.value() == 1280) && (!height || height.value() == 720) && AVCaptureSessionPreset1280x720)
        return [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset1280x720] ? AVCaptureSessionPreset1280x720 : nil;

    if ((!width || width.value() == 960) && (!height || height.value() == 540) && AVCaptureSessionPreset960x540)
        return [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset960x540] ? AVCaptureSessionPreset960x540 : nil;

    if ((!width || width.value() == 640) && (!height || height.value() == 480 ) && AVCaptureSessionPreset640x480)
        return [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset640x480] ? AVCaptureSessionPreset640x480 : nil;

    if ((!width || width.value() == 352) && (!height || height.value() == 288 ) && AVCaptureSessionPreset352x288)
        return [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset352x288] ? AVCaptureSessionPreset352x288 : nil;

    if ((!width || width.value() == 320) && (!height || height.value() == 240 ) && AVCaptureSessionPreset320x240)
        return [videoDevice supportsAVCaptureSessionPreset:AVCaptureSessionPreset320x240] ? AVCaptureSessionPreset320x240 : nil;

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

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
