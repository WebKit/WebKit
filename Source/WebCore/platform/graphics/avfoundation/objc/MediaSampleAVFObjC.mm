/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "MediaSampleAVFObjC.h"

#import "PixelBuffer.h"
#import "PixelBufferConformerCV.h"
#import <JavaScriptCore/JSCInlines.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <wtf/PrintStream.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

// Equivalent to WTF_DECLARE_CF_TYPE_TRAIT(CMSampleBuffer);
// Needed due to requirement of specifying the PAL namespace.
template <>
struct WTF::CFTypeTrait<CMSampleBufferRef> {
    static inline CFTypeID typeID(void) { return PAL::CMSampleBufferGetTypeID(); }
};

namespace WebCore {

MediaSampleAVFObjC::MediaSampleAVFObjC(RetainPtr<CMSampleBufferRef>&& sample)
    : m_sample(WTFMove(sample))
{
}
MediaSampleAVFObjC::MediaSampleAVFObjC(CMSampleBufferRef sample)
    : m_sample(sample)
{
}
MediaSampleAVFObjC::MediaSampleAVFObjC(CMSampleBufferRef sample, AtomString trackID)
    : m_sample(sample)
    , m_id(trackID)
{
}
MediaSampleAVFObjC::MediaSampleAVFObjC(CMSampleBufferRef sample, uint64_t trackID)
    : m_sample(sample)
    , m_id(AtomString::number(trackID))
{
}
MediaSampleAVFObjC::MediaSampleAVFObjC(CMSampleBufferRef sample, VideoRotation rotation, bool mirrored)
    : m_sample(sample)
    , m_rotation(rotation)
    , m_mirrored(mirrored)
{
}

MediaSampleAVFObjC::~MediaSampleAVFObjC() = default;

RefPtr<MediaSampleAVFObjC> MediaSampleAVFObjC::createImageSample(PixelBuffer&& pixelBuffer)
{
    auto size = pixelBuffer.size();
    auto width = size.width();
    auto height = size.height();

    auto data = pixelBuffer.takeData();
    auto dataBaseAddress = data->data();
    auto leakedData = &data.leakRef();
    
    auto derefBuffer = [] (void* context, const void*) {
        static_cast<JSC::Uint8ClampedArray*>(context)->deref();
    };

    CVPixelBufferRef cvPixelBufferRaw = nullptr;
    auto status = CVPixelBufferCreateWithBytes(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, dataBaseAddress, width * 4, derefBuffer, leakedData, nullptr, &cvPixelBufferRaw);

    auto cvPixelBuffer = adoptCF(cvPixelBufferRaw);
    if (!cvPixelBuffer) {
        derefBuffer(leakedData, nullptr);
        return nullptr;
    }
    ASSERT_UNUSED(status, !status);

    CMVideoFormatDescriptionRef formatDescriptionRaw = nullptr;
    status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, cvPixelBuffer.get(), &formatDescriptionRaw);
    ASSERT(!status);
    auto formatDescription = adoptCF(formatDescriptionRaw);

    CMSampleTimingInfo sampleTimingInformation = { PAL::kCMTimeInvalid, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid };
    CMSampleBufferRef sampleBufferRaw = nullptr;
    status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, cvPixelBuffer.get(), formatDescription.get(), &sampleTimingInformation, &sampleBufferRaw);
    ASSERT(!status);
    auto sampleBuffer = adoptCF(sampleBufferRaw);

    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer.get(), true);
    for (CFIndex i = 0, count = CFArrayGetCount(attachmentsArray); i < count; ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }
    return create(sampleBuffer.get());
}

MediaTime MediaSampleAVFObjC::presentationTime() const
{
    auto timeStamp = PAL::CMSampleBufferGetOutputPresentationTimeStamp(m_sample.get());
    if (CMTIME_IS_INVALID(timeStamp))
        timeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(m_sample.get());
    return PAL::toMediaTime(timeStamp);
}

MediaTime MediaSampleAVFObjC::decodeTime() const
{
    auto timeStamp = PAL::CMSampleBufferGetDecodeTimeStamp(m_sample.get());
    if (CMTIME_IS_INVALID(timeStamp))
        return presentationTime();
    return PAL::toMediaTime(timeStamp);
}

MediaTime MediaSampleAVFObjC::duration() const
{
    auto duration = PAL::CMSampleBufferGetOutputDuration(m_sample.get());
    if (CMTIME_IS_INVALID(duration))
        duration = PAL::CMSampleBufferGetDuration(m_sample.get());
    return PAL::toMediaTime(duration);
}

size_t MediaSampleAVFObjC::sizeInBytes() const
{
    return PAL::CMSampleBufferGetTotalSampleSize(m_sample.get());
}

PlatformSample MediaSampleAVFObjC::platformSample()
{
    PlatformSample sample = { PlatformSample::CMSampleBufferType, { .cmSampleBuffer = m_sample.get() } };
    return sample;
}

uint32_t MediaSampleAVFObjC::videoPixelFormat() const
{
    auto pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(m_sample.get()));
    return CVPixelBufferGetPixelFormatType(pixelBuffer);
}

static bool isCMSampleBufferAttachmentRandomAccess(CFDictionaryRef attachmentDict)
{
    return !CFDictionaryContainsKey(attachmentDict, PAL::kCMSampleAttachmentKey_NotSync);
}

static bool doesCMSampleBufferHaveSyncInfo(CMSampleBufferRef sample)
{
    return PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
}

static bool isCMSampleBufferRandomAccess(CMSampleBufferRef sample)
{
    CFArrayRef attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;
    
    for (CFIndex i = 0, count = CFArrayGetCount(attachments); i < count; ++i) {
        if (!isCMSampleBufferAttachmentRandomAccess(checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, i))))
            return false;
    }
    return true;
}

static bool isCMSampleBufferAttachmentNonDisplaying(CFDictionaryRef attachmentDict)
{
    return CFDictionaryContainsKey(attachmentDict, PAL::kCMSampleAttachmentKey_DoNotDisplay);
}

static bool isCMSampleBufferNonDisplaying(CMSampleBufferRef sample)
{
    CFArrayRef attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return false;
    
    for (CFIndex i = 0; i < CFArrayGetCount(attachments); ++i) {
        if (isCMSampleBufferAttachmentNonDisplaying(checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, i))))
            return true;
    }

    return false;
}

MediaSample::SampleFlags MediaSampleAVFObjC::flags() const
{
    int returnValue = MediaSample::None;

    if (doesCMSampleBufferHaveSyncInfo(m_sample.get()))
        returnValue |= MediaSample::HasSyncInfo;

    if (isCMSampleBufferRandomAccess(m_sample.get()))
        returnValue |= MediaSample::IsSync;

    if (isCMSampleBufferNonDisplaying(m_sample.get()))
        returnValue |= MediaSample::IsNonDisplaying;

    return SampleFlags(returnValue);
}

FloatSize MediaSampleAVFObjC::presentationSize() const
{
    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(m_sample.get());
    if (PAL::CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return FloatSize();
    
    return FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
}

void MediaSampleAVFObjC::dump(PrintStream& out) const
{
    out.print("{PTS(", presentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(", (int)flags(), "), presentationSize(", presentationSize().width(), "x", presentationSize().height(), ")}");
}

void MediaSampleAVFObjC::offsetTimestampsBy(const MediaTime& offset)
{
    CMItemCount itemCount = 0;
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), 0, nullptr, &itemCount))
        return;
    
    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), itemCount, timingInfoArray.data(), nullptr))
        return;
    
    for (auto& timing : timingInfoArray) {
        timing.presentationTimeStamp = PAL::toCMTime(PAL::toMediaTime(timing.presentationTimeStamp) + offset);
        timing.decodeTimeStamp = PAL::toCMTime(PAL::toMediaTime(timing.decodeTimeStamp) + offset);
    }
    
    CMSampleBufferRef newSample;
    if (noErr != PAL::CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, m_sample.get(), itemCount, timingInfoArray.data(), &newSample))
        return;
    
    m_sample = adoptCF(newSample);
}

void MediaSampleAVFObjC::setTimestamps(const WTF::MediaTime &presentationTimestamp, const WTF::MediaTime &decodeTimestamp)
{
    CMItemCount itemCount = 0;
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), 0, nullptr, &itemCount))
        return;
    
    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), itemCount, timingInfoArray.data(), nullptr))
        return;
    
    for (auto& timing : timingInfoArray) {
        timing.presentationTimeStamp = PAL::toCMTime(presentationTimestamp);
        timing.decodeTimeStamp = PAL::toCMTime(decodeTimestamp);
    }
    
    CMSampleBufferRef newSample;
    if (noErr != PAL::CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, m_sample.get(), itemCount, timingInfoArray.data(), &newSample))
        return;
    
    m_sample = adoptCF(newSample);
}

bool MediaSampleAVFObjC::isDivisable() const
{
    if (PAL::CMSampleBufferGetNumSamples(m_sample.get()) == 1)
        return false;

    if (PAL::CMSampleBufferGetSampleSizeArray(m_sample.get(), 0, nullptr, nullptr) == kCMSampleBufferError_BufferHasNoSampleSizes)
        return false;

    return true;
}

std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> MediaSampleAVFObjC::divide(const MediaTime& presentationTime, UseEndTime useEndTime)
{
    if (!isDivisable())
        return { nullptr, nullptr };

    CFIndex samplesBeforePresentationTime = 0;

    PAL::CMSampleBufferCallBlockForEachSample(m_sample.get(), [&] (CMSampleBufferRef sampleBuffer, CMItemCount) -> OSStatus {
        auto timeStamp = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sampleBuffer);
        if (CMTIME_IS_INVALID(timeStamp))
            timeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

        if (useEndTime == UseEndTime::Use) {
            auto duration = PAL::CMSampleBufferGetOutputDuration(sampleBuffer);
            if (CMTIME_IS_INVALID(duration))
                duration = PAL::CMSampleBufferGetDuration(sampleBuffer);

            if (PAL::toMediaTime(PAL::CMTimeAdd(timeStamp, duration)) > presentationTime)
                return 1;
        } else if (PAL::toMediaTime(timeStamp) >= presentationTime)
            return 1;
        ++samplesBeforePresentationTime;
        return noErr;
    });

    if (!samplesBeforePresentationTime)
        return { nullptr, this };

    CMItemCount sampleCount = PAL::CMSampleBufferGetNumSamples(m_sample.get());
    if (samplesBeforePresentationTime >= sampleCount)
        return { this, nullptr };

    CMSampleBufferRef rawSampleBefore = nullptr;
    CFRange rangeBefore = CFRangeMake(0, samplesBeforePresentationTime);
    if (PAL::CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, m_sample.get(), rangeBefore, &rawSampleBefore) != noErr)
        return { nullptr, nullptr };
    RetainPtr<CMSampleBufferRef> sampleBefore = adoptCF(rawSampleBefore);

    CMSampleBufferRef rawSampleAfter = nullptr;
    CFRange rangeAfter = CFRangeMake(samplesBeforePresentationTime, sampleCount - samplesBeforePresentationTime);
    if (PAL::CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, m_sample.get(), rangeAfter, &rawSampleAfter) != noErr)
        return { nullptr, nullptr };
    RetainPtr<CMSampleBufferRef> sampleAfter = adoptCF(rawSampleAfter);

    return { MediaSampleAVFObjC::create(sampleBefore.get(), m_id), MediaSampleAVFObjC::create(sampleAfter.get(), m_id) };
}

Ref<MediaSample> MediaSampleAVFObjC::createNonDisplayingCopy() const
{
    CMSampleBufferRef newSampleBuffer = 0;
    PAL::CMSampleBufferCreateCopy(kCFAllocatorDefault, m_sample.get(), &newSampleBuffer);
    ASSERT(newSampleBuffer);

    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(newSampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);
    }

    return MediaSampleAVFObjC::create(adoptCF(newSampleBuffer).get(), m_id);
}

RefPtr<JSC::Uint8ClampedArray> MediaSampleAVFObjC::getRGBAImageData() const
{
    const OSType imageFormat = kCVPixelFormatType_32RGBA;
    RetainPtr<CFNumberRef> imageFormatNumber = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &imageFormat));

    RetainPtr<CFMutableDictionaryRef> conformerOptions = adoptCF(CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(conformerOptions.get(), kCVPixelBufferPixelFormatTypeKey, imageFormatNumber.get());
    PixelBufferConformerCV pixelBufferConformer(conformerOptions.get());

    auto pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(m_sample.get()));
    auto rgbaPixelBuffer = pixelBufferConformer.convert(pixelBuffer);
    auto status = CVPixelBufferLockBaseAddress(rgbaPixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(status == noErr);

    void* data = CVPixelBufferGetBaseAddressOfPlane(rgbaPixelBuffer.get(), 0);
    size_t byteLength = CVPixelBufferGetHeight(pixelBuffer) * CVPixelBufferGetWidth(pixelBuffer) * 4;
    auto result = JSC::Uint8ClampedArray::tryCreate(JSC::ArrayBuffer::create(data, byteLength), 0, byteLength);

    status = CVPixelBufferUnlockBaseAddress(rgbaPixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(status == noErr);

    return result;
}

static inline void setSampleBufferAsDisplayImmediately(CMSampleBufferRef sampleBuffer)
{
    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, PAL::kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }
}

void MediaSampleAVFObjC::setAsDisplayImmediately(MediaSample& sample)
{
    setSampleBufferAsDisplayImmediately(sample.platformSample().sample.cmSampleBuffer);
}

bool MediaSampleAVFObjC::isHomogeneous() const
{
    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(m_sample.get(), true);
    if (!attachmentsArray)
        return true;

    auto count = CFArrayGetCount(attachmentsArray);
    if (count <= 1)
        return true;

    CFDictionaryRef firstAttachment = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, 0));
    bool isSync = isCMSampleBufferAttachmentRandomAccess(firstAttachment);
    bool isNonDisplaying = isCMSampleBufferAttachmentNonDisplaying(firstAttachment);

    for (CFIndex i = 1; i < count; ++i) {
        auto attachmentDict = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        if (isSync != isCMSampleBufferAttachmentRandomAccess(attachmentDict))
            return false;

        if (isNonDisplaying != isCMSampleBufferAttachmentNonDisplaying(attachmentDict))
            return false;
    };

    return true;
}

Vector<Ref<MediaSampleAVFObjC>> MediaSampleAVFObjC::divideIntoHomogeneousSamples()
{
    using SampleVector = Vector<Ref<MediaSampleAVFObjC>>;

    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(m_sample.get(), true);
    if (!attachmentsArray)
        return SampleVector::from(makeRef(*this));

    auto count = CFArrayGetCount(attachmentsArray);
    if (count <= 1)
        return SampleVector::from(makeRef(*this));

    CFDictionaryRef firstAttachment = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, 0));
    bool isSync = isCMSampleBufferAttachmentRandomAccess(firstAttachment);
    bool isNonDisplaying = isCMSampleBufferAttachmentNonDisplaying(firstAttachment);
    Vector<CFRange> ranges;
    CFIndex currentRangeStart = 0;
    CFIndex currentRangeLength = 1;

    for (CFIndex i = 1; i < count; ++i, ++currentRangeLength) {
        CFDictionaryRef attachmentDict = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        if (isSync == isCMSampleBufferAttachmentRandomAccess(attachmentDict) && isNonDisplaying == isCMSampleBufferAttachmentNonDisplaying(attachmentDict))
            continue;

        ranges.append(CFRangeMake(currentRangeStart, currentRangeLength));
        currentRangeStart = i;
        currentRangeLength = 0;
    }
    ranges.append(CFRangeMake(currentRangeStart, currentRangeLength));

    if (ranges.size() == 1)
        return SampleVector::from(makeRef(*this));

    SampleVector samples;
    samples.reserveInitialCapacity(ranges.size());
    for (auto& range : ranges) {
        CMSampleBufferRef rawSample = nullptr;
        if (PAL::CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, m_sample.get(), range, &rawSample) != noErr || !rawSample)
            return { };
        samples.uncheckedAppend(MediaSampleAVFObjC::create(adoptCF(rawSample).get(), m_id));
    }
    return samples;
}

RetainPtr<CMSampleBufferRef> MediaSampleAVFObjC::cloneSampleBufferAndSetAsDisplayImmediately(CMSampleBufferRef sample)
{
    auto pixelBuffer = static_cast<CVImageBufferRef>(PAL::CMSampleBufferGetImageBuffer(sample));
    if (!pixelBuffer)
        return nullptr;

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    auto status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, &formatDescription);
    if (status)
        return nullptr;
    auto retainedFormatDescription = adoptCF(formatDescription);

    CMItemCount itemCount = 0;
    status = PAL::CMSampleBufferGetSampleTimingInfoArray(sample, 0, nullptr, &itemCount);
    if (status)
        return nullptr;

    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    status = PAL::CMSampleBufferGetSampleTimingInfoArray(sample, itemCount, timingInfoArray.data(), nullptr);
    if (status)
        return nullptr;

    CMSampleBufferRef newSampleBuffer;
    status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, pixelBuffer, formatDescription, timingInfoArray.data(), &newSampleBuffer);
    if (status)
        return nullptr;

    setSampleBufferAsDisplayImmediately(newSampleBuffer);

    return adoptCF(newSampleBuffer);
}

static CFStringRef byteRangeOffsetAttachmentKey()
{
    static CFStringRef key = CFSTR("WebKitMediaSampleByteRangeOffset");
    return key;
}

std::optional<MediaSample::ByteRange> MediaSampleAVFObjC::byteRange() const
{
    return byteRangeForAttachment(byteRangeOffsetAttachmentKey());
}

void MediaSampleAVFObjC::setByteRangeOffset(size_t byteOffset)
{
    int64_t checkedOffset = CheckedInt64(byteOffset);
    auto offsetNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &checkedOffset));
    PAL::CMSetAttachment(m_sample.get(), byteRangeOffsetAttachmentKey(), offsetNumber.get(), kCMAttachmentMode_ShouldPropagate);
}

std::optional<MediaSample::ByteRange> MediaSampleAVFObjC::byteRangeForAttachment(CFStringRef key) const
{
    auto byteOffsetCF = dynamic_cf_cast<CFNumberRef>(PAL::CMGetAttachment(m_sample.get(), key, nullptr));
    if (!byteOffsetCF)
        return std::nullopt;

    int64_t byteOffset = 0;
    if (!CFNumberGetValue(byteOffsetCF, kCFNumberSInt64Type, &byteOffset))
        return std::nullopt;

    CMItemCount sizeArrayEntries = 0;
    PAL::CMSampleBufferGetSampleSizeArray(m_sample.get(), 0, nullptr, &sizeArrayEntries);
    if (sizeArrayEntries != 1)
        return std::nullopt;

    size_t singleSizeEntry = 0;
    PAL::CMSampleBufferGetSampleSizeArray(m_sample.get(), 1, &singleSizeEntry, nullptr);
    return { { CheckedSize(byteOffset), singleSizeEntry } };
}

}
