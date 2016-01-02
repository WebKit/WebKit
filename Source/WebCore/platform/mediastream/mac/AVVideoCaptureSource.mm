/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#import "BlockExceptions.h"
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "IntRect.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "NotImplemented.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>

#import "CoreMediaSoftLink.h"

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceType;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureVideoDataOutput AVCaptureVideoDataOutputType;
typedef AVCaptureVideoPreviewLayer AVCaptureVideoPreviewLayerType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreVideo)

SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoPreviewLayer)

#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVCaptureVideoDataOutput getAVCaptureVideoDataOutputClass()
#define AVCaptureVideoPreviewLayer getAVCaptureVideoPreviewLayerClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPresetLow, NSString *)

#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPresetLow getAVCaptureSessionPresetLow()

SOFT_LINK(CoreVideo, CVPixelBufferGetWidth, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetHeight, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetBaseAddress, void*, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetBytesPerRow, size_t, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferGetPixelFormatType, OSType, (CVPixelBufferRef pixelBuffer), (pixelBuffer))
SOFT_LINK(CoreVideo, CVPixelBufferLockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))
SOFT_LINK(CoreVideo, CVPixelBufferUnlockBaseAddress, CVReturn, (CVPixelBufferRef pixelBuffer, CVOptionFlags lockFlags), (pixelBuffer, lockFlags))

SOFT_LINK_POINTER(CoreVideo, kCVPixelBufferPixelFormatTypeKey, NSString *)
#define kCVPixelBufferPixelFormatTypeKey getkCVPixelBufferPixelFormatTypeKey()

namespace WebCore {

RefPtr<AVMediaCaptureSource> AVVideoCaptureSource::create(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
{
    return adoptRef(new AVVideoCaptureSource(device, id, constraint));
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
    : AVMediaCaptureSource(device, id, RealtimeMediaSource::Video, constraint)
{
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
}

void AVVideoCaptureSource::initializeCapabilities(RealtimeMediaSourceCapabilities&)
{
    // FIXME: finish this implementation
}

void AVVideoCaptureSource::initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints& supportedConstraints)
{
    supportedConstraints.setSupportsWidth(true);
    supportedConstraints.setSupportsHeight(true);
    supportedConstraints.setSupportsAspectRatio(true);
    supportedConstraints.setSupportsFrameRate(true);
    supportedConstraints.setSupportsFacingMode(true);
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
    
    settings.setFrameRate(m_frameRate);
    settings.setWidth(m_width);
    settings.setHeight(m_height);
    settings.setAspectRatio(static_cast<float>(m_width) / m_height);
}

bool AVVideoCaptureSource::setFrameRateConstraint(float minFrameRate, float maxFrameRate)
{
    AVFrameRateRange *bestFrameRateRange = 0;

    for (AVFrameRateRange *frameRateRange in [[device() activeFormat] videoSupportedFrameRateRanges]) {
        if (!maxFrameRate) {
            if (minFrameRate == [frameRateRange minFrameRate])
                bestFrameRateRange = frameRateRange;
        } else if (minFrameRate >= [frameRateRange minFrameRate] && maxFrameRate <= [frameRateRange maxFrameRate]) {
            if (CMTIME_COMPARE_INLINE([frameRateRange minFrameDuration], >, [bestFrameRateRange minFrameDuration]))
                bestFrameRateRange = frameRateRange;
        }
    }
    
    if (!bestFrameRateRange) {
        LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p), frame rate range %f..%f not supported by video device", this, minFrameRate, maxFrameRate);
        return false;
    }
    
    NSError *error = nil;
    @try {
        if ([device() lockForConfiguration:&error]) {
            [device() setActiveVideoMinFrameDuration:[bestFrameRateRange minFrameDuration]];
            if (maxFrameRate)
                [device() setActiveVideoMaxFrameDuration:[bestFrameRateRange maxFrameDuration]];
            [device() unlockForConfiguration];
        }
    } @catch(NSException *exception) {
        LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p), exception thrown configuring device: <%s> %s", this, [[exception name] UTF8String], [[exception reason] UTF8String]);
        return false;
    }
    
    if (error) {
        LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p), failed to lock video device for configuration: %s", this, [[error localizedDescription] UTF8String]);
        return false;
    }

    LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p) - set frame rate range to %f..%f", this, minFrameRate, maxFrameRate);
    return true;
}

bool AVVideoCaptureSource::applyConstraints(MediaConstraints* constraints)
{
    ASSERT(constraints);

    const RealtimeMediaSourceSupportedConstraints& supportedConstraints = RealtimeMediaSourceCenter::singleton().supportedConstraints();
    String widthConstraintValue;
    String heightConstraintValue;
    String widthConstraintName = supportedConstraints.nameForConstraint(MediaConstraintType::Width);
    String heightConstraintName = supportedConstraints.nameForConstraint(MediaConstraintType::Height);

    constraints->getMandatoryConstraintValue(widthConstraintName, widthConstraintValue);
    constraints->getMandatoryConstraintValue(heightConstraintName, heightConstraintValue);

    int width = widthConstraintValue.toInt();
    int height = heightConstraintValue.toInt();
    if (!width && !height) {
        constraints->getOptionalConstraintValue(widthConstraintName, widthConstraintValue);
        constraints->getOptionalConstraintValue(heightConstraintName, heightConstraintValue);
        width = widthConstraintValue.toInt();
        height = heightConstraintValue.toInt();
    }
    
    if (width || height) {
        NSString *preset = AVCaptureSessionInfo(session()).bestSessionPresetForVideoDimensions(width, height);
        if (!preset || ![session() canSetSessionPreset:preset])
            return false;
        
        [session() setSessionPreset:preset];
    }

    String frameRateConstraintValue;
    String frameRateConstraintName = supportedConstraints.nameForConstraint(MediaConstraintType::FrameRate);
    constraints->getMandatoryConstraintValue(frameRateConstraintName, frameRateConstraintValue);
    float frameRate = frameRateConstraintValue.toFloat();
    if (!frameRate) {
        constraints->getOptionalConstraintValue(frameRateConstraintName, frameRateConstraintValue);
        frameRate = frameRateConstraintValue.toFloat();
    }
    if (frameRate && !setFrameRateConstraint(frameRate, 0))
        return false;

    return true;
}

void AVVideoCaptureSource::setupCaptureSession()
{
    RetainPtr<AVCaptureDeviceInputType> videoIn = adoptNS([allocAVCaptureDeviceInputInstance() initWithDevice:device() error:nil]);

    if (![session() canAddInput:videoIn.get()]) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video input device", this);
        return;
    }

    [session() addInput:videoIn.get()];

    if (constraints())
        applyConstraints(constraints());

    RetainPtr<AVCaptureVideoDataOutputType> videoOutput = adoptNS([allocAVCaptureVideoDataOutputInstance() init]);
    RetainPtr<NSDictionary> settingsDictionary = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
                                                         [NSNumber numberWithInt:kCVPixelFormatType_32BGRA], kCVPixelBufferPixelFormatTypeKey
                                                         , nil]);
    [videoOutput setVideoSettings:settingsDictionary.get()];
    setVideoSampleBufferDelegate(videoOutput.get());

    if (![session() canAddOutput:videoOutput.get()]) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add video sample buffer output delegate", this);
        return;
    }
    [session() addOutput:videoOutput.get()];
}

void AVVideoCaptureSource::shutdownCaptureSession()
{
    m_videoPreviewLayer = nullptr;
    m_buffer = nullptr;
    m_lastImage = nullptr;
    m_videoFrameTimeStamps.clear();
    m_frameRate = 0;
    m_width = 0;
    m_height = 0;
}

bool AVVideoCaptureSource::updateFramerate(CMSampleBufferRef sampleBuffer)
{
    CMTime sampleTime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    if (!CMTIME_IS_NUMERIC(sampleTime))
        return false;

    Float64 frameTime = CMTimeGetSeconds(sampleTime);
    Float64 oneSecondAgo = frameTime - 1;
    
    m_videoFrameTimeStamps.append(frameTime);
    
    while (m_videoFrameTimeStamps[0] < oneSecondAgo)
        m_videoFrameTimeStamps.remove(0);

    Float64 frameRate = m_frameRate;
    m_frameRate = (m_frameRate + m_videoFrameTimeStamps.size()) / 2;

    return frameRate != m_frameRate;
}

void AVVideoCaptureSource::processNewFrame(RetainPtr<CMSampleBufferRef> sampleBuffer)
{
    // Ignore frames delivered when the session is not running, we want to hang onto the last image
    // delivered before it stopped.
    if (m_lastImage && (!isProducingData() || muted()))
        return;

    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer.get());
    if (!formatDescription)
        return;

    updateFramerate(sampleBuffer.get());

    bool settingsChanged = false;

    m_buffer = sampleBuffer;
    m_lastImage = nullptr;

    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
    if (dimensions.width != m_width || dimensions.height != m_height) {
        m_width = dimensions.width;
        m_height = dimensions.height;
        settingsChanged = true;
    }

    if (settingsChanged)
        this->settingsDidChanged();
}

void AVVideoCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType*)
{
    RetainPtr<CMSampleBufferRef> buffer = sampleBuffer;

    scheduleDeferredTask([this, buffer] {
        this->processNewFrame(buffer);
    });
}

RefPtr<Image> AVVideoCaptureSource::currentFrameImage()
{
    if (!currentFrameCGImage())
        return nullptr;

    FloatRect imageRect(0, 0, m_width, m_height);
    std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(imageRect.size(), Unaccelerated);

    if (!imageBuffer)
        return nullptr;

    paintCurrentFrameInContext(imageBuffer->context(), imageRect);

    return ImageBuffer::sinkIntoImage(WTFMove(imageBuffer));
}

RetainPtr<CGImageRef> AVVideoCaptureSource::currentFrameCGImage()
{
    if (m_lastImage)
        return m_lastImage;

    if (!m_buffer)
        return nullptr;

    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_buffer.get()));
    ASSERT(CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_32BGRA);

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);

    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateWithData(NULL, baseAddress, bytesPerRow * height, NULL));
    m_lastImage = adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, sRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst, provider.get(), NULL, true, kCGRenderingIntentDefault));

    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    return m_lastImage;
}

void AVVideoCaptureSource::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled() || !currentFrameCGImage())
        return;

    GraphicsContextStateSaver stateSaver(context);
    context.translate(rect.x(), rect.y() + rect.height());
    context.scale(FloatSize(1, -1));
    context.setImageInterpolationQuality(InterpolationLow);
    IntRect paintRect(IntPoint(0, 0), IntSize(rect.width(), rect.height()));
    CGContextDrawImage(context.platformContext(), CGRectMake(0, 0, paintRect.width(), paintRect.height()), m_lastImage.get());
}

PlatformLayer* AVVideoCaptureSource::platformLayer() const
{
    if (m_videoPreviewLayer)
        return m_videoPreviewLayer.get();

    m_videoPreviewLayer = adoptNS([allocAVCaptureVideoPreviewLayerInstance() initWithSession:session()]);
#ifndef NDEBUG
    m_videoPreviewLayer.get().name = @"AVVideoCaptureSource preview layer";
#endif

    return m_videoPreviewLayer.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
