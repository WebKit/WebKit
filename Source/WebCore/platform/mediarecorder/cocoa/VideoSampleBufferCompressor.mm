/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "VideoSampleBufferCompressor.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "Logging.h"
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#import <VideoToolbox/VTCompressionSession.h>
#import <wtf/SoftLinking.h>

#import <pal/cf/VideoToolboxSoftLink.h>

using namespace PAL;

namespace WebCore {

std::unique_ptr<VideoSampleBufferCompressor> VideoSampleBufferCompressor::create(CMVideoCodecType outputCodecType, CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    auto compressor = std::unique_ptr<VideoSampleBufferCompressor>(new VideoSampleBufferCompressor(outputCodecType));
    if (!compressor->initialize(callback, callbackObject))
        return nullptr;
    return compressor;
}

VideoSampleBufferCompressor::VideoSampleBufferCompressor(CMVideoCodecType outputCodecType)
    : m_serialDispatchQueue { dispatch_queue_create("com.apple.VideoSampleBufferCompressor", DISPATCH_QUEUE_SERIAL) }
    , m_outputCodecType { outputCodecType }
{
}

VideoSampleBufferCompressor::~VideoSampleBufferCompressor()
{
    if (m_serialDispatchQueue)
        dispatch_release(m_serialDispatchQueue);
}

bool VideoSampleBufferCompressor::initialize(CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    CMBufferQueueRef outputBufferQueue;
    if (auto error = CMBufferQueueCreate(kCFAllocatorDefault, 0, CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &outputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "VideoSampleBufferCompressor unable to create buffer queue %d", error);
        return false;
    }
    m_outputBufferQueue = outputBufferQueue;
    CMBufferQueueInstallTrigger(m_outputBufferQueue.get(), callback, callbackObject, kCMBufferQueueTrigger_WhenDataBecomesReady, kCMTimeZero, NULL);

    m_isEncoding = true;
    return true;
}

void VideoSampleBufferCompressor::finish()
{
    dispatch_sync(m_serialDispatchQueue, ^{
        auto error = VTCompressionSessionCompleteFrames(m_vtSession.get(), kCMTimeInvalid);
        RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTCompressionSessionCompleteFrames failed with %d", error);

        error = CMBufferQueueMarkEndOfData(m_outputBufferQueue.get());
        RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor CMBufferQueueMarkEndOfData failed with %d", error);

        m_isEncoding = false;
    });
}

void VideoSampleBufferCompressor::videoCompressionCallback(void *refCon, void*, OSStatus status, VTEncodeInfoFlags, CMSampleBufferRef buffer)
{
    RELEASE_LOG_ERROR_IF(status, MediaStream, "VideoSampleBufferCompressor videoCompressionCallback status is %d", status);
    if (status != noErr)
        return;

    VideoSampleBufferCompressor *compressor = static_cast<VideoSampleBufferCompressor*>(refCon);

    auto error = CMBufferQueueEnqueue(compressor->m_outputBufferQueue.get(), buffer);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor CMBufferQueueEnqueue failed with %d", error);
}

bool VideoSampleBufferCompressor::initCompressionSession(CMVideoFormatDescriptionRef formatDescription)
{
    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
#if PLATFORM(IOS)
    NSDictionary *encoderSpecifications = nil;
#else
    NSDictionary *encoderSpecifications = @{(__bridge NSString *)kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder:@YES};
#endif

    VTCompressionSessionRef vtSession;
    auto error = VTCompressionSessionCreate(kCFAllocatorDefault, dimensions.width, dimensions.height, m_outputCodecType, (__bridge CFDictionaryRef)encoderSpecifications, NULL, NULL, videoCompressionCallback, this, &vtSession);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "VideoSampleBufferCompressor VTCompressionSessionCreate failed with %d", error);
        return NO;
    }
    m_vtSession = adoptCF(vtSession);

    error = VTSessionSetProperty(m_vtSession.get(), kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_RealTime failed with %d", error);
    error = VTSessionSetProperty(m_vtSession.get(), kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, (__bridge CFTypeRef)@(m_maxKeyFrameIntervalDuration));
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration failed with %d", error);
    error = VTSessionSetProperty(m_vtSession.get(), kVTCompressionPropertyKey_ExpectedFrameRate, (__bridge CFTypeRef)@(m_expectedFrameRate));
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_ExpectedFrameRate failed with %d", error);

    // FIXME: Set video compression rate.
    error = VTCompressionSessionPrepareToEncodeFrames(m_vtSession.get());
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTCompressionSessionPrepareToEncodeFrames failed with %d", error);

    return YES;
}

void VideoSampleBufferCompressor::processSampleBuffer(CMSampleBufferRef buffer)
{
    if (!m_vtSession) {
        if (!initCompressionSession(CMSampleBufferGetFormatDescription(buffer)))
            return;
    }

    auto imageBuffer = CMSampleBufferGetImageBuffer(buffer);
    auto presentationTimeStamp = CMSampleBufferGetPresentationTimeStamp(buffer);
    auto duration = CMSampleBufferGetDuration(buffer);
    auto error = VTCompressionSessionEncodeFrame(m_vtSession.get(), imageBuffer, presentationTimeStamp, duration, NULL, this, NULL);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTCompressionSessionEncodeFrame failed with %d", error);
}

void VideoSampleBufferCompressor::addSampleBuffer(CMSampleBufferRef buffer)
{
    dispatch_sync(m_serialDispatchQueue, ^{
        if (!m_isEncoding)
            return;

        processSampleBuffer(buffer);
    });
}

CMSampleBufferRef VideoSampleBufferCompressor::getOutputSampleBuffer()
{
    return (CMSampleBufferRef)(const_cast<void*>(CMBufferQueueGetHead(m_outputBufferQueue.get())));
}

RetainPtr<CMSampleBufferRef> VideoSampleBufferCompressor::takeOutputSampleBuffer()
{
    return adoptCF((CMSampleBufferRef)(const_cast<void*>(CMBufferQueueDequeueAndRetain(m_outputBufferQueue.get()))));
}

}
#endif
