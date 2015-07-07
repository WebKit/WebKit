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

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVVideoCaptureSource.h"

#import "AVCaptureDeviceManager.h"
#import "BlockExceptions.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaStreamSourceStates.h"
#import "NotImplemented.h"
#import "SoftLinking.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceType;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureVideoDataOutput AVCaptureVideoDataOutputType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)

SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoDataOutput)

#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVCaptureVideoDataOutput getAVCaptureVideoDataOutputClass()

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

SOFT_LINK(CoreMedia, CMSampleBufferGetFormatDescription, CMFormatDescriptionRef, (CMSampleBufferRef sbuf), (sbuf));
SOFT_LINK(CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf));
SOFT_LINK(CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
SOFT_LINK(CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
SOFT_LINK(CoreMedia, CMVideoFormatDescriptionGetDimensions, CMVideoDimensions, (CMVideoFormatDescriptionRef videoDesc), (videoDesc));

namespace WebCore {

RefPtr<AVMediaCaptureSource> AVVideoCaptureSource::create(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
{
    return adoptRef(new AVVideoCaptureSource(device, id, constraint));
}

AVVideoCaptureSource::AVVideoCaptureSource(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
    : AVMediaCaptureSource(device, id, MediaStreamSource::Video, constraint)
    , m_frameRate(0)
    , m_width(0)
    , m_height(0)
{
    currentStates()->setSourceId(id);
    currentStates()->setSourceType(MediaStreamSourceStates::Camera);
}

AVVideoCaptureSource::~AVVideoCaptureSource()
{
}

RefPtr<MediaStreamSourceCapabilities> AVVideoCaptureSource::capabilities() const
{
    notImplemented();
    return 0;
}

void AVVideoCaptureSource::updateStates()
{
    MediaStreamSourceStates* states = currentStates();

    if ([device() position] == AVCaptureDevicePositionFront)
        states->setFacingMode(MediaStreamSourceStates::User);
    else if ([device() position] == AVCaptureDevicePositionBack)
        states->setFacingMode(MediaStreamSourceStates::Environment);
    else
        states->setFacingMode(MediaStreamSourceStates::Unknown);
    
    states->setFrameRate(m_frameRate);
    states->setWidth(m_width);
    states->setHeight(m_height);
    states->setAspectRatio(static_cast<float>(m_width) / m_height);
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

NSLog(@"set frame rate to %f", [bestFrameRateRange minFrameRate]);
    LOG(Media, "AVVideoCaptureSource::setFrameRateConstraint(%p) - set frame rate range to %f..%f", this, minFrameRate, maxFrameRate);
    return true;
}

bool AVVideoCaptureSource::applyConstraints(MediaConstraints* constraints)
{
    ASSERT(constraints);

    const Vector<AtomicString>& constraintNames = AVCaptureDeviceManager::validConstraintNames();
    String widthConstraint;
    String heightConstraint;

    constraints->getMandatoryConstraintValue(constraintNames[AVCaptureDeviceManager::Width], widthConstraint);
    constraints->getMandatoryConstraintValue(constraintNames[AVCaptureDeviceManager::Height], heightConstraint);

    int width = widthConstraint.toInt();
    int height = heightConstraint.toInt();
    if (!width && !height) {
        constraints->getOptionalConstraintValue(constraintNames[AVCaptureDeviceManager::Width], widthConstraint);
        constraints->getOptionalConstraintValue(constraintNames[AVCaptureDeviceManager::Height], heightConstraint);
        width = widthConstraint.toInt();
        height = heightConstraint.toInt();
    }
    
    if (width || height) {
        NSString *preset = AVCaptureDeviceManager::bestSessionPresetForVideoSize(session(), width, height);
        if (!preset || ![session() canSetSessionPreset:preset])
            return false;
        
        [session() setSessionPreset:preset];
    }

    String frameRateConstraint;
    constraints->getMandatoryConstraintValue(constraintNames[AVCaptureDeviceManager::FrameRate], frameRateConstraint);
    float frameRate = frameRateConstraint.toFloat();
    if (!frameRate) {
        constraints->getOptionalConstraintValue(constraintNames[AVCaptureDeviceManager::FrameRate], frameRateConstraint);
        frameRate = frameRateConstraint.toFloat();
    }
    if (frameRate && !setFrameRateConstraint(frameRate, 0))
        return false;

    return true;
}

void AVVideoCaptureSource::setupCaptureSession()
{
    RetainPtr<AVCaptureDeviceInputType> videoIn = adoptNS([[AVCaptureDeviceInput alloc] initWithDevice:device() error:nil]);
    ASSERT([session() canAddInput:videoIn.get()]);
    if ([session() canAddInput:videoIn.get()])
        [session() addInput:videoIn.get()];
    
    if (constraints())
        applyConstraints(constraints());

    RetainPtr<AVCaptureVideoDataOutputType> videoOutput = adoptNS([[AVCaptureVideoDataOutput alloc] init]);
    setVideoSampleBufferDelegate(videoOutput.get());
    ASSERT([session() canAddOutput:videoOutput.get()]);
    if ([session() canAddOutput:videoOutput.get()])
        [session() addOutput:videoOutput.get()];
    
    m_videoConnection = adoptNS([videoOutput.get() connectionWithMediaType:AVMediaTypeVideo]);
}

void AVVideoCaptureSource::calculateFramerate(CMSampleBufferRef sampleBuffer)
{
    CMTime sampleTime = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    if (!CMTIME_IS_NUMERIC(sampleTime))
        return;

    Float64 frameTime = CMTimeGetSeconds(sampleTime);
    Float64 oneSecondAgo = frameTime - 1;
    
    m_videoFrameTimeStamps.append(frameTime);
    
    while (m_videoFrameTimeStamps[0] < oneSecondAgo)
        m_videoFrameTimeStamps.remove(0);
    
    m_frameRate = (m_frameRate + m_videoFrameTimeStamps.size()) / 2;
}
    
void AVVideoCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType*)
{
    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
    if (!formatDescription)
        return;

    CFRetain(formatDescription);
    m_videoFormatDescription = adoptCF(formatDescription);
    calculateFramerate(sampleBuffer);

    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
    m_width = dimensions.width;
    m_height = dimensions.height;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
