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

#import "CDMFairPlayStreaming.h"
#import "CVUtilities.h"
#import "ISOTrackEncryptionBox.h"
#import "PixelBuffer.h"
#import "PixelBufferConformerCV.h"
#import "ProcessIdentity.h"
#import "VideoFrameCV.h"
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
    commonInit();
}

MediaSampleAVFObjC::MediaSampleAVFObjC(CMSampleBufferRef sample)
    : m_sample(sample)
{
    commonInit();
}

MediaSampleAVFObjC::MediaSampleAVFObjC(CMSampleBufferRef sample, TrackID trackID)
    : m_sample(sample)
    , m_id(trackID)
{
    commonInit();
}

MediaSampleAVFObjC::~MediaSampleAVFObjC() = default;

void MediaSampleAVFObjC::commonInit()
{
    auto presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(m_sample.get());
    if (CMTIME_IS_INVALID(presentationTime))
        presentationTime = PAL::CMSampleBufferGetPresentationTimeStamp(m_sample.get());
    m_presentationTime = PAL::toMediaTime(presentationTime);

    auto decodeTime = PAL::CMSampleBufferGetDecodeTimeStamp(m_sample.get());
    m_decodeTime = !CMTIME_IS_INVALID(decodeTime) ? PAL::toMediaTime(decodeTime) : m_presentationTime;

    auto duration = PAL::CMSampleBufferGetOutputDuration(m_sample.get());
    if (CMTIME_IS_INVALID(duration))
        duration = PAL::CMSampleBufferGetDuration(m_sample.get());
    m_duration = PAL::toMediaTime(duration);

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    auto getKeyIDs = [](CMFormatDescriptionRef description) -> Vector<Ref<SharedBuffer>> {
        if (!description)
            return { };
        if (auto trackEncryptionData = static_cast<CFDataRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("CommonEncryptionTrackEncryptionBox")))) {
            // AVStreamDataParser will attach the 'tenc' box to each sample, not including the leading
            // size and boxType data. Extract the 'tenc' box and use that box to derive the sample's
            // keyID.
            auto length = CFDataGetLength(trackEncryptionData);
            auto ptr = (void*)(CFDataGetBytePtr(trackEncryptionData));
            auto destructorFunction = createSharedTask<void(void*)>([data = WTFMove(trackEncryptionData)] (void*) { UNUSED_PARAM(data); });
            auto trackEncryptionDataBuffer = ArrayBuffer::create(JSC::ArrayBufferContents(ptr, length, std::nullopt, WTFMove(destructorFunction)));

            ISOTrackEncryptionBox trackEncryptionBox;
            auto trackEncryptionView = JSC::DataView::create(WTFMove(trackEncryptionDataBuffer), 0, length);
            if (!trackEncryptionBox.parseWithoutTypeAndSize(trackEncryptionView))
                return { };
            return { SharedBuffer::create(trackEncryptionBox.defaultKID()) };
        }

#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
        if (auto transportStreamData = static_cast<CFDataRef>(PAL::CMFormatDescriptionGetExtension(description, CFSTR("TransportStreamEncryptionInitData")))) {
            // AVStreamDataParser will attach a JSON transport stream encryption
            // description object to each sample. Use a static keyID in this case
            // as MPEG2-TS encryption dose not specify a particular keyID in the
            // stream.
            return CDMPrivateFairPlayStreaming::mptsKeyIDs();
        }
#endif

        return { };
    };
    m_keyIDs = getKeyIDs(PAL::CMSampleBufferGetFormatDescription(m_sample.get()));
#endif
}

MediaTime MediaSampleAVFObjC::presentationTime() const
{
    return m_presentationTime;
}

MediaTime MediaSampleAVFObjC::decodeTime() const
{
    return m_decodeTime;
}

MediaTime MediaSampleAVFObjC::duration() const
{
    return m_duration;
}

size_t MediaSampleAVFObjC::sizeInBytes() const
{
    // Per sample overhead was calculated with `leaks` on a process
    // with MallocStackLogging enabled. This value should be occasionally
    // re-validated and updated when OS changes occurr.
    constexpr size_t EstimatedCMSampleBufferOverhead = 1234;

    return PAL::CMSampleBufferGetTotalSampleSize(m_sample.get())
        + sizeof(MediaSampleAVFObjC)
        + EstimatedCMSampleBufferOverhead;
}

PlatformSample MediaSampleAVFObjC::platformSample() const
{
    PlatformSample sample = { PlatformSample::CMSampleBufferType, { .cmSampleBuffer = m_sample.get() } };
    return sample;
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

bool MediaSampleAVFObjC::isCMSampleBufferNonDisplaying(CMSampleBufferRef sample)
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

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
    if (!m_keyIDs.isEmpty())
        returnValue |= MediaSample::IsProtected;
#endif

    return SampleFlags(returnValue);
}

FloatSize MediaSampleAVFObjC::presentationSize() const
{
    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(m_sample.get());
    if (PAL::CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return FloatSize();
    
    return FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
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
    
    m_presentationTime += offset;
    m_decodeTime += offset;
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
    
    m_presentationTime = presentationTimestamp;
    m_decodeTime = decodeTimestamp;
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

Vector<Ref<MediaSampleAVFObjC>> MediaSampleAVFObjC::divide()
{
    auto numSamples = PAL::CMSampleBufferGetNumSamples(m_sample.get());

    if (numSamples == 1)
        return Vector<Ref<MediaSampleAVFObjC>>::from(Ref { *this });

    Vector<Ref<MediaSampleAVFObjC>> samples;
    samples.reserveInitialCapacity(numSamples);
    PAL::CMSampleBufferCallBlockForEachSample(m_sample.get(), [&] (CMSampleBufferRef sampleBuffer, CMItemCount) -> OSStatus {
        samples.append(MediaSampleAVFObjC::create(sampleBuffer, m_id));
        return noErr;
    });
    return samples;
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

    CMFormatDescriptionRef formatDescription = PAL::CMSampleBufferGetFormatDescription(m_sample.get());
    bool isAudio = PAL::CMFormatDescriptionGetMediaType(formatDescription) == kCMMediaType_Audio;
    const CFStringRef attachmentKey = isAudio ? PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart : PAL::kCMSampleAttachmentKey_DoNotDisplay;

    CFArrayRef attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(newSampleBuffer, true);
    ASSERT(attachmentsArray);
    if (attachmentsArray) {
        for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
            CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
            CFDictionarySetValue(attachments, attachmentKey, kCFBooleanTrue);
        }
    }

    return MediaSampleAVFObjC::create(adoptCF(newSampleBuffer).get(), m_id);
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
        return SampleVector::from(Ref { *this });

    auto count = CFArrayGetCount(attachmentsArray);
    if (count <= 1)
        return SampleVector::from(Ref { *this });

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
        return SampleVector::from(Ref { *this });

    SampleVector samples;
    samples.reserveInitialCapacity(ranges.size());
    for (auto& range : ranges) {
        CMSampleBufferRef rawSample = nullptr;
        if (PAL::CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, m_sample.get(), range, &rawSample) != noErr || !rawSample)
            return { };
        samples.append(MediaSampleAVFObjC::create(adoptCF(rawSample).get(), m_id));
    }
    return samples;
}

}
