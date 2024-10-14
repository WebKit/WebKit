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

#if ENABLE(MEDIA_RECORDER) && USE(AVFOUNDATION)

#import "ContentType.h"
#import "Logging.h"
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#import <wtf/EnumTraits.h>
#import <wtf/NativePromise.h>
#import <wtf/SoftLinking.h>

#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cf/VideoToolboxSoftLink.h>

namespace WebCore {

RefPtr<VideoSampleBufferCompressor> VideoSampleBufferCompressor::create(String mimeType, CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    auto profile = Profile::Baseline;
    for (auto codec : ContentType(mimeType).codecs()) {
        if (startsWithLettersIgnoringASCIICase(codec, "avc1."_s) && codec.length() >= 11) {
            if (codec[5] == '6' && codec[6] == '4')
                profile = Profile::High;
            else if (codec[5] == '4' && (codec[6] == 'd' || codec[6] == 'D'))
                profile = Profile::Main;
            break;
        }
    }

    Ref compressor = adoptRef(*new VideoSampleBufferCompressor(kCMVideoCodecType_H264, profile));
    if (!compressor->initialize(callback, callbackObject))
        return nullptr;
    return compressor;
}

VideoSampleBufferCompressor::VideoSampleBufferCompressor(CMVideoCodecType outputCodecType, Profile profile)
    : m_serialDispatchQueue { WorkQueue::create("com.apple.VideoSampleBufferCompressor"_s) }
    , m_outputCodecType { outputCodecType }
    , m_profile { profile }
{
}

VideoSampleBufferCompressor::~VideoSampleBufferCompressor()
{
    if (m_outputBufferQueue)
        PAL::CMBufferQueueRemoveTrigger(m_outputBufferQueue.get(), m_triggerToken);

    if (m_vtSession) {
        PAL::VTCompressionSessionInvalidate(m_vtSession.get());
        m_vtSession = nullptr;
    }
}

bool VideoSampleBufferCompressor::initialize(CMBufferQueueTriggerCallback callback, void* callbackObject)
{
    CMBufferQueueRef outputBufferQueue;
    if (auto error = PAL::CMBufferQueueCreate(kCFAllocatorDefault, 0, PAL::CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &outputBufferQueue)) {
        RELEASE_LOG_ERROR(MediaStream, "VideoSampleBufferCompressor unable to create buffer queue %d", error);
        return false;
    }
    m_outputBufferQueue = adoptCF(outputBufferQueue);
    PAL::CMBufferQueueInstallTrigger(m_outputBufferQueue.get(), callback, callbackObject, kCMBufferQueueTrigger_WhenDataBecomesReady, PAL::kCMTimeZero, &m_triggerToken);

    m_isEncoding = true;
    return true;
}

void VideoSampleBufferCompressor::setBitsPerSecond(unsigned bitRate)
{
    m_outputBitRate = bitRate;
}

Ref<GenericPromise> VideoSampleBufferCompressor::flushInternal(bool isFinished)
{
    return invokeAsync(m_serialDispatchQueue, [weakThis = ThreadSafeWeakPtr { *this }, this, isFinished] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return GenericPromise::createAndReject();

        auto error = PAL::VTCompressionSessionCompleteFrames(m_vtSession.get(), PAL::kCMTimeInvalid);
        RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTCompressionSessionCompleteFrames failed with %d", error);

        if (!isFinished) {
            m_needsKeyframe = true;
            return GenericPromise::createAndResolve();
        }

        error = PAL::CMBufferQueueMarkEndOfData(m_outputBufferQueue.get());
        RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor CMBufferQueueMarkEndOfData failed with %d", error);

        m_isEncoding = false;
        return GenericPromise::createAndResolve();
    });
}

void VideoSampleBufferCompressor::videoCompressionCallback(void *refCon, void*, OSStatus status, VTEncodeInfoFlags, CMSampleBufferRef buffer)
{
    RELEASE_LOG_ERROR_IF(status, MediaStream, "VideoSampleBufferCompressor videoCompressionCallback status is %d", status);
    if (status != noErr)
        return;

    VideoSampleBufferCompressor *compressor = static_cast<VideoSampleBufferCompressor*>(refCon);

    auto error = PAL::CMBufferQueueEnqueue(compressor->m_outputBufferQueue.get(), buffer);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor CMBufferQueueEnqueue failed with %d", error);
}

static inline OSStatus setCompressionSessionProperty(VTCompressionSessionRef vtSession, CFStringRef key, uint32_t value)
{
    int64_t value64 = value;
    auto cfValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value64));
    OSStatus status = VTSessionSetProperty(vtSession, key, cfValue.get());
    return status;
}

CFStringRef VideoSampleBufferCompressor::vtProfileLevel() const
{
    switch (m_profile) {
    case Profile::Baseline:
        return PAL::kVTProfileLevel_H264_Baseline_AutoLevel;
    case Profile::High:
        return PAL::kVTProfileLevel_H264_High_AutoLevel;
    case Profile::Main:
        return PAL::kVTProfileLevel_H264_Main_AutoLevel;
    }
}

bool VideoSampleBufferCompressor::initCompressionSession(CMVideoFormatDescriptionRef formatDescription)
{
    CMVideoDimensions dimensions = PAL::CMVideoFormatDescriptionGetDimensions(formatDescription);
#if PLATFORM(IOS) || PLATFORM(VISION)
    NSDictionary *encoderSpecifications = nil;
#else
    NSDictionary *encoderSpecifications = @{(__bridge NSString *)PAL::kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder:@YES};
#endif

    VTCompressionSessionRef vtSession;
    auto error = PAL::VTCompressionSessionCreate(kCFAllocatorDefault, dimensions.width, dimensions.height, m_outputCodecType, (__bridge CFDictionaryRef)encoderSpecifications, NULL, NULL, videoCompressionCallback, this, &vtSession);
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "VideoSampleBufferCompressor VTCompressionSessionCreate failed with %d", error);
        return NO;
    }
    m_vtSession = adoptCF(vtSession);

    error = VTSessionSetProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_RealTime failed with %d", error);
    error = setCompressionSessionProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, m_maxKeyFrameIntervalDuration);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration failed with %d", error);
    error = setCompressionSessionProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_MaxKeyFrameInterval, m_maxKeyFrameIntervalDuration * m_expectedFrameRate);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_MaxKeyFrameInterval failed with %d", error);
    error = setCompressionSessionProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_ExpectedFrameRate, m_expectedFrameRate);
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_ExpectedFrameRate failed with %d", error);

    error = VTSessionSetProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_ProfileLevel, vtProfileLevel());
    if (error) {
        RELEASE_LOG_ERROR(MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_ProfileLevel failed with %d for profile %hhu", error, enumToUnderlyingType(m_profile));
        if (m_profile != Profile::Baseline) {
            error = VTSessionSetProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_ProfileLevel, PAL::kVTProfileLevel_H264_Baseline_AutoLevel);
            RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_ProfileLevel failed with %d for default profile", error);
        }
    }

    if (m_outputBitRate) {
        error = setCompressionSessionProperty(m_vtSession.get(), PAL::kVTCompressionPropertyKey_AverageBitRate, *m_outputBitRate);
        RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTSessionSetProperty kVTCompressionPropertyKey_AverageBitRate failed with %d", error);
    }

    error = PAL::VTCompressionSessionPrepareToEncodeFrames(m_vtSession.get());
    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTCompressionSessionPrepareToEncodeFrames failed with %d", error);

    return YES;
}

static CFDictionaryRef forceKeyFrameDictionary()
{
    static NeverDestroyed<RetainPtr<CFDictionaryRef>> dictionary = [] {
        auto key = PAL::kVTEncodeFrameOptionKey_ForceKeyFrame;
        auto value = kCFBooleanTrue;
        return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, (const void**)&key, (const void**)&value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }();

    return dictionary.get().get();
}

void VideoSampleBufferCompressor::processSampleBuffer(CMSampleBufferRef buffer)
{
    if (!m_vtSession) {
        if (!initCompressionSession(PAL::CMSampleBufferGetFormatDescription(buffer)))
            return;
    }

    auto imageBuffer = PAL::CMSampleBufferGetImageBuffer(buffer);
    auto presentationTimeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(buffer);
    auto duration = PAL::CMSampleBufferGetDuration(buffer);
    auto error = PAL::VTCompressionSessionEncodeFrame(m_vtSession.get(), imageBuffer, presentationTimeStamp, duration, m_needsKeyframe ? forceKeyFrameDictionary() : NULL, this, NULL);

    if (m_needsKeyframe)
        m_needsKeyframe = false;

    RELEASE_LOG_ERROR_IF(error, MediaStream, "VideoSampleBufferCompressor VTCompressionSessionEncodeFrame failed with %d", error);
}

void VideoSampleBufferCompressor::addSampleBuffer(CMSampleBufferRef buffer)
{
    m_serialDispatchQueue->dispatch([weakThis = ThreadSafeWeakPtr { *this }, buffer = RetainPtr { buffer }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_isEncoding)
            return;

        protectedThis->processSampleBuffer(buffer.get());
    });
}

CMSampleBufferRef VideoSampleBufferCompressor::getOutputSampleBuffer()
{
    return (CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueGetHead(m_outputBufferQueue.get())));
}

RetainPtr<CMSampleBufferRef> VideoSampleBufferCompressor::takeOutputSampleBuffer()
{
    return adoptCF((CMSampleBufferRef)(const_cast<void*>(PAL::CMBufferQueueDequeueAndRetain(m_outputBufferQueue.get()))));
}

unsigned VideoSampleBufferCompressor::bitRate() const
{
    return m_outputBitRate.value_or(0);
}

}
#endif
