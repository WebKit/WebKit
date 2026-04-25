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

#import "CMUtilities.h"
#import "CVUtilities.h"
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

namespace WebCore {

MediaSampleAVFObjC::MediaSampleAVFObjC(RetainPtr<CMSampleBufferRef>&& sample)
    : m_sample(WTF::move(sample))
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
    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(m_sample.get());
    m_keyIDs = getKeyIDs(formatDescription.get());
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
    return PlatformSample { m_sample };
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
    RetainPtr attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;

    if (CFArrayGetCount(attachments.get()) < 1)
        return true;

    RetainPtr firstAttachment = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments.get(), 0));
    return isCMSampleBufferAttachmentRandomAccess(firstAttachment.get());
}

static bool isCMSampleBufferAttachmentNonDisplaying(CFDictionaryRef attachmentDict)
{
    return CFDictionaryContainsKey(attachmentDict, PAL::kCMSampleAttachmentKey_DoNotDisplay);
}

bool MediaSampleAVFObjC::isCMSampleBufferNonDisplaying(CMSampleBufferRef sample)
{
    RetainPtr attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return false;
    
    for (CFIndex i = 0; i < CFArrayGetCount(attachments.get()); ++i) {
        RetainPtr sampleDictionary = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments.get(), i));
        if (isCMSampleBufferAttachmentNonDisplaying(sampleDictionary.get()))
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
    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(m_sample.get());
    if (PAL::CMFormatDescriptionGetMediaType(formatDescription.get()) != kCMMediaType_Video)
        return FloatSize();
    
    return FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(formatDescription.get(), true, true));
}

void MediaSampleAVFObjC::offsetTimestampsBy(const MediaTime& offset)
{
    CMItemCount itemCount = 0;
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), 0, nullptr, &itemCount))
        return;
    
    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), itemCount, timingInfoArray.mutableSpan().data(), nullptr))
        return;
    
    for (auto& timing : timingInfoArray) {
        timing.presentationTimeStamp = PAL::toCMTime(PAL::toMediaTime(timing.presentationTimeStamp) + offset);
        timing.decodeTimeStamp = PAL::toCMTime(PAL::toMediaTime(timing.decodeTimeStamp) + offset);
    }
    
    CMSampleBufferRef newSample;
    if (noErr != PAL::CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, m_sample.get(), itemCount, timingInfoArray.span().data(), &newSample))
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
    if (noErr != PAL::CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), itemCount, timingInfoArray.mutableSpan().data(), nullptr))
        return;
    
    for (auto& timing : timingInfoArray) {
        timing.presentationTimeStamp = PAL::toCMTime(presentationTimestamp);
        timing.decodeTimeStamp = PAL::toCMTime(decodeTimestamp);
    }
    
    CMSampleBufferRef newSample;
    if (noErr != PAL::CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, m_sample.get(), itemCount, timingInfoArray.span().data(), &newSample))
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
    PAL::CMSampleBufferCallBlockForEachSample(m_sample.get(), [&samples, id = m_id] (CMSampleBufferRef sampleBuffer, CMItemCount) -> OSStatus {
        samples.append(MediaSampleAVFObjC::create(sampleBuffer, id));
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

    RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(m_sample.get());
    bool isAudio = PAL::CMFormatDescriptionGetMediaType(formatDescription.get()) == kCMMediaType_Audio;
    const RetainPtr attachmentKey = isAudio ? PAL::kCMSampleBufferAttachmentKey_TrimDurationAtStart : PAL::kCMSampleAttachmentKey_DoNotDisplay;

    RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(newSampleBuffer, true);
    ASSERT(attachmentsArray);
    if (attachmentsArray) {
        for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray.get()); ++i) {
            RetainPtr attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), i));
            CFDictionarySetValue(attachments.get(), attachmentKey.get(), kCFBooleanTrue);
        }
    }

    return MediaSampleAVFObjC::create(adoptCF(newSampleBuffer).get(), m_id);
}

bool MediaSampleAVFObjC::isHomogeneous() const
{
    RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(m_sample.get(), true);
    if (!attachmentsArray)
        return true;

    auto count = CFArrayGetCount(attachmentsArray.get());
    if (count <= 1)
        return true;

    RetainPtr firstAttachment = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), 0));
    bool isSync = isCMSampleBufferAttachmentRandomAccess(firstAttachment.get());
    bool isNonDisplaying = isCMSampleBufferAttachmentNonDisplaying(firstAttachment.get());

    for (CFIndex i = 1; i < count; ++i) {
        RetainPtr attachmentDict = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), i));
        if (isSync != isCMSampleBufferAttachmentRandomAccess(attachmentDict.get()))
            return false;

        if (isNonDisplaying != isCMSampleBufferAttachmentNonDisplaying(attachmentDict.get()))
            return false;
    };

    return true;
}

Vector<Ref<MediaSampleAVFObjC>> MediaSampleAVFObjC::divideIntoHomogeneousSamples()
{
    using SampleVector = Vector<Ref<MediaSampleAVFObjC>>;

    RetainPtr attachmentsArray = PAL::CMSampleBufferGetSampleAttachmentsArray(m_sample.get(), true);
    if (!attachmentsArray)
        return SampleVector::from(Ref { *this });

    auto count = CFArrayGetCount(attachmentsArray.get());
    if (count <= 1)
        return SampleVector::from(Ref { *this });

    RetainPtr firstAttachment = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), 0));
    bool isSync = isCMSampleBufferAttachmentRandomAccess(firstAttachment.get());
    bool isNonDisplaying = isCMSampleBufferAttachmentNonDisplaying(firstAttachment.get());
    Vector<CFRange> ranges;
    CFIndex currentRangeStart = 0;
    CFIndex currentRangeLength = 1;

    for (CFIndex i = 1; i < count; ++i, ++currentRangeLength) {
        RetainPtr attachmentDict = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray.get(), i));
        if (isSync == isCMSampleBufferAttachmentRandomAccess(attachmentDict.get()) && isNonDisplaying == isCMSampleBufferAttachmentNonDisplaying(attachmentDict.get()))
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
