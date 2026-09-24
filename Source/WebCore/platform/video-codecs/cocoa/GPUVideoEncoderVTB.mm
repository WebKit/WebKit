/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "GPUVideoEncoderVTB.h"

#if USE(AVFOUNDATION)

#import "CMUtilities.h"
#include "VideoEncoderVTBSession.h"
#import <algorithm>
#import <cmath>
#import <wtf/Lock.h>
#import <wtf/MonotonicTime.h>
#import <wtf/Seconds.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cf/VideoToolboxSoftLink.h>

namespace WebCore {

// Certain hardware encoders tend to consistently overshoot the bitrate they are configured to encode at.
// This estimates an adjusted bitrate that, when set on the encoder, produces output closer to the desired bitrate.
class GPUVideoEncoderBitrateAdjuster {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GPUVideoEncoderBitrateAdjuster);
public:
    void setTargetBitrateBps(uint32_t bitrateBps)
    {
        Locker locker { m_lock };
        if (!isWithinTolerance(bitrateBps, m_targetBitrateBps) || !isWithinTolerance(bitrateBps, m_lastAdjustedTargetBitrateBps))
            m_adjustedBitrateBps = bitrateBps;
        m_targetBitrateBps = bitrateBps;
    }

    uint32_t adjustedBitrateBps() const
    {
        Locker locker { m_lock };
        return m_adjustedBitrateBps;
    }

    void update(size_t frameSize)
    {
        Locker locker { m_lock };
        auto now = MonotonicTime::now();
        m_bytesSinceLastUpdate += frameSize;
        updateBitrate(now);
    }

private:
    static constexpr Seconds updateInterval = 1_s;
    static constexpr uint32_t updateFrameInterval = 30;
    static constexpr float toleranceRatio = .1;
    static constexpr float minAdjustedRatio = .5;
    static constexpr float maxAdjustedRatio = .95;

    static bool isWithinTolerance(uint32_t bitrateBps, uint32_t targetBitrateBps)
    {
        if (!targetBitrateBps)
            return false;
        float delta = std::abs(static_cast<float>(bitrateBps) - static_cast<float>(targetBitrateBps));
        return (delta / targetBitrateBps) < toleranceRatio;
    }

    void updateBitrate(MonotonicTime now) WTF_REQUIRES_LOCK(m_lock)
    {
        ++m_framesSinceLastUpdate;
        if (!m_lastUpdateTime)
            m_lastUpdateTime = now;
        auto elapsed = now - *m_lastUpdateTime;
        if (elapsed < updateInterval || m_framesSinceLastUpdate < updateFrameInterval)
            return;

        float targetBitrateBps = m_targetBitrateBps;
        float estimatedBitrateBps = elapsed.seconds() > 0 ? (m_bytesSinceLastUpdate * 8) / elapsed.seconds() : targetBitrateBps;
        float error = targetBitrateBps - estimatedBitrateBps;

        // Adjust if we've overshot by any amount or if we've undershot too much.
        if (estimatedBitrateBps > targetBitrateBps || error > toleranceRatio * targetBitrateBps) {
            float adjustedBitrateBps = targetBitrateBps + .5 * error;
            adjustedBitrateBps = std::max(adjustedBitrateBps, minAdjustedRatio * targetBitrateBps);
            adjustedBitrateBps = std::min(adjustedBitrateBps, maxAdjustedRatio * targetBitrateBps);
            m_adjustedBitrateBps = adjustedBitrateBps;
        }

        m_lastUpdateTime = now;
        m_framesSinceLastUpdate = 0;
        m_bytesSinceLastUpdate = 0;
        m_lastAdjustedTargetBitrateBps = m_targetBitrateBps;
    }

    mutable Lock m_lock;
    uint32_t m_targetBitrateBps WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    uint32_t m_adjustedBitrateBps WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    uint32_t m_lastAdjustedTargetBitrateBps WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    size_t m_bytesSinceLastUpdate WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    uint32_t m_framesSinceLastUpdate WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    std::optional<MonotonicTime> m_lastUpdateTime WTF_GUARDED_BY_LOCK(m_lock);
};

GPUVideoEncoderVTB::GPUVideoEncoderVTB(CreationInfo&& creationInfo, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
    : m_creationInfo(WTF::move(creationInfo))
    , m_callback(WTF::move(callback))
    , m_descriptionCallback(WTF::move(descriptionCallback))
    , m_errorCallback(WTF::move(errorCallback))
    , m_bitrateAdjuster(makeUniqueRef<GPUVideoEncoderBitrateAdjuster>())
{
    assertIsCurrent(queue());
}

GPUVideoEncoderVTB::~GPUVideoEncoderVTB() = default;

void GPUVideoEncoderVTB::notifyEncodedFrame(std::span<const uint8_t> data, const GPUVideoEncoderFrameInfo& info)
{
    m_callback(data, info);
}

void GPUVideoEncoderVTB::notifyDescription(std::span<const uint8_t> data, const PlatformVideoColorSpace& colorSpace)
{
    ASSERT(needsToSendDescription());
    setNeedsToSendDescription(false);
    m_descriptionCallback(data, colorSpace);
}

void GPUVideoEncoderVTB::notifyDescriptionIfNeeded(CMSampleBufferRef sampleBuffer, CFStringRef boxName, const PlatformVideoColorSpace& colorSpace)
{
    if (!needsToSendDescription())
        return;

    if (useAnnexB()) {
        notifyDescription({ }, colorSpace);
        return;
    }

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);
    if (RetainPtr sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms))) {
        if (RetainPtr sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict, boxName)))
            notifyDescription(unsafeMakeSpan(CFDataGetBytePtr(sampleExtensions), static_cast<size_t>(CFDataGetLength(sampleExtensions))), colorSpace);
    }
}

void GPUVideoEncoderVTB::notifyError()
{
    m_errorCallback(false);
}

void GPUVideoEncoderVTB::notifyFrameDropped()
{
    m_errorCallback(true);
}

void GPUVideoEncoderVTB::initialize(uint16_t width, uint16_t height, unsigned startBitrateKbps, unsigned, unsigned, uint32_t)
{
    assertIsCurrent(queue());

    m_width = width;
    m_height = height;
    m_targetBitrateBps = startBitrateKbps * 1000;
    resetCompressionSession();
    // FIXME: We might want to check that m_encoder creation went fine, or error the encoder otherwise.
}

CMVideoCodecType GPUVideoEncoderVTB::codecType() const
{
    switch (m_creationInfo.codecType) {
    case VideoCodecType::H264:
        return kCMVideoCodecType_H264;
    case VideoCodecType::H265:
        return kCMVideoCodecType_HEVC;
    case VideoCodecType::VP9:
        return kCMVideoCodecType_VP9;
    case VideoCodecType::AV1:
        return kCMVideoCodecType_AV1;
    }
    ASSERT_NOT_REACHED();
    return kCMVideoCodecType_HEVC;
}

bool GPUVideoEncoderVTB::resetCompressionSession()
{
    assertIsCurrent(queue());

    m_encoder = nullptr;

    RetainPtr encoderSpecification = adoptCF(CFDictionaryCreateMutable(nullptr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
#if PLATFORM(MAC) && !PLATFORM(MACCATALYST)
    CFDictionarySetValue(encoderSpecification, PAL::kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, kCFBooleanTrue);
#endif

    m_encoder = VideoEncoderVTBSession::create(m_width, m_height, codecType(), encoderSpecification, nullptr);
    if (!m_encoder)
        return false;

    setNeedsToSendDescription(true);

    configureCompressionSession();
    return true;
}

void GPUVideoEncoderVTB::configureCompressionSession()
{
    assertIsCurrent(queue());

    ASSERT(m_encoder);
    Ref encoder = *m_encoder;
    encoder->setProperty(PAL::kVTCompressionPropertyKey_RealTime, m_creationInfo.isLowLatencyEnabled ? kCFBooleanTrue : kCFBooleanFalse);
    encoder->setProperty(PAL::kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);

    // FIXME: Enable color space handling for H264 once the H.264 decoder is able to handle it.
    if (codecType() != kCMVideoCodecType_H264) {
        if (m_colorSpace.primaries) {
            if (RetainPtr primaries = convertToCMColorPrimaries(*m_colorSpace.primaries))
                encoder->setProperty(PAL::kVTCompressionPropertyKey_ColorPrimaries, primaries);
        }
        if (m_colorSpace.transfer) {
            if (RetainPtr transferFunction = convertToCMTransferFunction(*m_colorSpace.transfer))
                encoder->setProperty(PAL::kVTCompressionPropertyKey_TransferFunction, transferFunction);
        }
        if (m_colorSpace.matrix) {
            if (RetainPtr matrix = convertToCMYCbCRMatrix(*m_colorSpace.matrix))
                encoder->setProperty(PAL::kVTCompressionPropertyKey_YCbCrMatrix, matrix);
        }
    }

    setEncoderBitrateBps(m_targetBitrateBps);

    // A relatively large value for keyframe emission (7200 frames or 4 minutes).
    int64_t maxKeyFrameInterval = 7200;
    encoder->setProperty(PAL::kVTCompressionPropertyKey_MaxKeyFrameInterval, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt64Type, &maxKeyFrameInterval)));
    double maxKeyFrameIntervalDuration = 240;
    encoder->setProperty(PAL::kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, adoptCF(CFNumberCreate(nullptr, kCFNumberDoubleType, &maxKeyFrameIntervalDuration)));

    if (m_creationInfo.scalabilityMode == VideoEncoderScalabilityMode::L1T2) {
        double baseLayerFrameRateFraction = 0.5;
        encoder->setProperty(PAL::kVTCompressionPropertyKey_BaseLayerFrameRateFraction, adoptCF(CFNumberCreate(nullptr, kCFNumberDoubleType, &baseLayerFrameRateFraction)).get());
    }

    configureAdditionalProperties();

    encoder->prepareToEncodeFrames();
}

void GPUVideoEncoderVTB::setProperty(CFStringRef key, CFTypeRef value)
{
    assertIsCurrent(queue());

    if (RefPtr encoder = m_encoder)
        encoder->setProperty(key, value);
}

void GPUVideoEncoderVTB::setEncoderBitrateBps(uint32_t bitrateBps)
{
    assertIsCurrent(queue());

    m_bitrateAdjuster->setTargetBitrateBps(bitrateBps);
    RefPtr encoder = m_encoder;
    if (!encoder)
        return;
    uint32_t adjustedBitrateBps = m_bitrateAdjuster->adjustedBitrateBps();
    encoder->setProperty(PAL::kVTCompressionPropertyKey_AverageBitRate, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &adjustedBitrateBps)));
}

void GPUVideoEncoderVTB::setRates(uint32_t bitRateKbps, uint32_t frameRate)
{
    assertIsCurrent(queue());

    // FIXME: we should set kVTCompressionPropertyKey_ExpectedFrameRate according frameRate, and may have to use frameRate for duration.
    UNUSED_PARAM(frameRate);

    m_targetBitrateBps = bitRateKbps * 1000;
    setEncoderBitrateBps(m_targetBitrateBps);
}

void GPUVideoEncoderVTB::encodeFrame(CVPixelBufferRef pixelBuffer, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation rotation, bool isKeyframeRequired)
{
    assertIsCurrent(queue());

    PlatformVideoColorSpace colorSpace = computeVideoFrameColorSpace(pixelBuffer);
    // FIXME: Remove this override when enabling color space handling for H264.
    if (codecType() == kCMVideoCodecType_H264)
        colorSpace.fullRange = true;

    if (!m_encoder || colorSpace != m_colorSpace) {
        m_colorSpace = colorSpace;
        if (!resetCompressionSession()) {
            notifyError();
            return;
        }
    }

    RetainPtr<CFDictionaryRef> frameProperties;
    if (isKeyframeRequired) {
        CFTypeRef keys[] = { PAL::kVTEncodeFrameOptionKey_ForceKeyFrame };
        CFTypeRef values[] = { kCFBooleanTrue };
        frameProperties = adoptCF(CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    int64_t captureTimeMS = timeStampNs / 1000000;
    auto presentationTimeStamp = PAL::CMTimeMake(captureTimeMS, 1000);
    uint16_t width = m_width;
    uint16_t height = m_height;

    auto status = protect(m_encoder)->encodeFrame(pixelBuffer, presentationTimeStamp, PAL::kCMTimeInvalid, frameProperties, makeBlockPtr([weakThis = ThreadSafeWeakPtr { *this }, width, height, captureTimeMS, timeStamp, duration, rotation, colorSpace = m_colorSpace](OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (status != noErr) {
            protectedThis->notifyError();
            return;
        }
        if (infoFlags & kVTEncodeInfo_FrameDropped) {
            protectedThis->notifyFrameDropped();
            return;
        }

        bool isKeyframe = true;
        bool isBaseLayer = true;
        if (RetainPtr attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false)) {
            if (CFArrayGetCount(attachments)) {
                if (RetainPtr attachment = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0))) {
                    CFBooleanRef notSync = nullptr;
                    if (CFDictionaryGetValueIfPresent(attachment, PAL::kCMSampleAttachmentKey_NotSync, reinterpret_cast<const void**>(&notSync)))
                        isKeyframe = !CFBooleanGetValue(notSync);
                    if (protectedThis->m_creationInfo.scalabilityMode == VideoEncoderScalabilityMode::L1T2) {
                        CFBooleanRef isDependedOnByOthers = nullptr;
                        if (CFDictionaryGetValueIfPresent(attachment, PAL::kCMSampleAttachmentKey_IsDependedOnByOthers, reinterpret_cast<const void**>(&isDependedOnByOthers)))
                            isBaseLayer = CFBooleanGetValue(isDependedOnByOthers);
                    }
                }
            }
        }

        std::optional<uint8_t> temporalIndex;
        if (protectedThis->m_creationInfo.scalabilityMode == VideoEncoderScalabilityMode::L1T2)
            temporalIndex = isBaseLayer ? 0 : 1;

        GPUVideoEncoderFrameInfo info { width, height, timeStamp, duration, captureTimeMS, isKeyframe, rotation, false, -1, temporalIndex };
        if (!protectedThis->convertAndNotify(sampleBuffer, WTF::move(info), colorSpace)) {
            protectedThis->notifyError();
            return;
        }
        protectedThis->m_bitrateAdjuster->update(PAL::CMSampleBufferGetTotalSampleSize(sampleBuffer));
    }));

    if (status != noErr)
        notifyError();
}

void GPUVideoEncoderVTB::flush()
{
    assertIsCurrent(queue());

    if (RefPtr encoder = m_encoder)
        encoder->completeFrames(PAL::kCMTimeInvalid);
}

std::optional<Vector<uint8_t>> GPUVideoEncoderVTB::toVector(CMSampleBufferRef sampleBuffer)
{
    RetainPtr blockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!blockBuffer)
        return { };

    Vector<uint8_t> buffer;
    size_t size = PAL::CMBlockBufferGetDataLength(blockBuffer);
    buffer.reserveInitialCapacity(size);
    for (size_t currentStart = 0; currentStart < size;) {
        char* data = nullptr;
        size_t length = 0;
        if (PAL::CMBlockBufferGetDataPointer(blockBuffer, currentStart, &length, nullptr, &data) != noErr)
            return { };
        buffer.append(unsafeMakeSpan(reinterpret_cast<const uint8_t*>(data), length));
        currentStart += length;
    }
    return buffer;
}

}

#endif
