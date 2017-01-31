/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import <wtf/PrintStream.h>

#import "CoreMediaSoftLink.h"

namespace WebCore {

MediaTime MediaSampleAVFObjC::presentationTime() const
{
    return toMediaTime(CMSampleBufferGetPresentationTimeStamp(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::outputPresentationTime() const
{
    return toMediaTime(CMSampleBufferGetOutputPresentationTimeStamp(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::decodeTime() const
{
    return toMediaTime(CMSampleBufferGetDecodeTimeStamp(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::duration() const
{
    return toMediaTime(CMSampleBufferGetDuration(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::outputDuration() const
{
    return toMediaTime(CMSampleBufferGetOutputDuration(m_sample.get()));
}

size_t MediaSampleAVFObjC::sizeInBytes() const
{
    return CMSampleBufferGetTotalSampleSize(m_sample.get());
}

PlatformSample MediaSampleAVFObjC::platformSample()
{
    PlatformSample sample = { PlatformSample::CMSampleBufferType, { .cmSampleBuffer = m_sample.get() } };
    return sample;
}

static bool CMSampleBufferIsRandomAccess(CMSampleBufferRef sample)
{
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;
    
    for (CFIndex i = 0, count = CFArrayGetCount(attachments); i < count; ++i) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, i);
        if (CFDictionaryContainsKey(attachmentDict, kCMSampleAttachmentKey_NotSync))
            return false;
    }
    return true;
}

static bool CMSampleBufferIsNonDisplaying(CMSampleBufferRef sample)
{
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return false;
    
    for (CFIndex i = 0; i < CFArrayGetCount(attachments); ++i) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, i);
        if (CFDictionaryContainsKey(attachmentDict, kCMSampleAttachmentKey_DoNotDisplay))
            return true;
    }

    return false;
}

MediaSample::SampleFlags MediaSampleAVFObjC::flags() const
{
    int returnValue = MediaSample::None;
    
    if (CMSampleBufferIsRandomAccess(m_sample.get()))
        returnValue |= MediaSample::IsSync;

    if (CMSampleBufferIsNonDisplaying(m_sample.get()))
        returnValue |= MediaSample::IsNonDisplaying;
    
    return SampleFlags(returnValue);
}

FloatSize MediaSampleAVFObjC::presentationSize() const
{
    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(m_sample.get());
    if (CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
        return FloatSize();
    
    return FloatSize(CMVideoFormatDescriptionGetPresentationDimensions(formatDescription, true, true));
}

void MediaSampleAVFObjC::dump(PrintStream& out) const
{
    out.print("{PTS(", presentationTime(), "), OPTS(", outputPresentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(", (int)flags(), "), presentationSize(", presentationSize().width(), "x", presentationSize().height(), ")}");
}

void MediaSampleAVFObjC::offsetTimestampsBy(const MediaTime& offset)
{
    CMItemCount itemCount = 0;
    if (noErr != CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), 0, nullptr, &itemCount))
        return;
    
    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), itemCount, timingInfoArray.data(), nullptr))
        return;
    
    for (auto& timing : timingInfoArray) {
        timing.presentationTimeStamp = toCMTime(toMediaTime(timing.presentationTimeStamp) + offset);
        timing.decodeTimeStamp = toCMTime(toMediaTime(timing.decodeTimeStamp) + offset);
    }
    
    CMSampleBufferRef newSample;
    if (noErr != CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, m_sample.get(), itemCount, timingInfoArray.data(), &newSample))
        return;
    
    m_sample = adoptCF(newSample);
}

void MediaSampleAVFObjC::setTimestamps(const WTF::MediaTime &presentationTimestamp, const WTF::MediaTime &decodeTimestamp)
{
    CMItemCount itemCount = 0;
    if (noErr != CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), 0, nullptr, &itemCount))
        return;
    
    Vector<CMSampleTimingInfo> timingInfoArray;
    timingInfoArray.grow(itemCount);
    if (noErr != CMSampleBufferGetSampleTimingInfoArray(m_sample.get(), itemCount, timingInfoArray.data(), nullptr))
        return;
    
    for (auto& timing : timingInfoArray) {
        timing.presentationTimeStamp = toCMTime(presentationTimestamp);
        timing.decodeTimeStamp = toCMTime(decodeTimestamp);
    }
    
    CMSampleBufferRef newSample;
    if (noErr != CMSampleBufferCreateCopyWithNewTiming(kCFAllocatorDefault, m_sample.get(), itemCount, timingInfoArray.data(), &newSample))
        return;
    
    m_sample = adoptCF(newSample);
}

bool MediaSampleAVFObjC::isDivisable() const
{
    if (CMSampleBufferGetNumSamples(m_sample.get()) == 1)
        return false;

    if (CMSampleBufferGetSampleSizeArray(m_sample.get(), 0, nullptr, nullptr) == kCMSampleBufferError_BufferHasNoSampleSizes)
        return false;

    return true;
}

std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> MediaSampleAVFObjC::divide(const MediaTime& presentationTime)
{
    if (!isDivisable())
        return { nullptr, nullptr };

    CFIndex samplesBeforePresentationTime = 0;

    CMSampleBufferCallBlockForEachSample(m_sample.get(), [&] (CMSampleBufferRef sampleBuffer, CMItemCount) -> OSStatus {
        if (toMediaTime(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) >= presentationTime)
            return 1;
        ++samplesBeforePresentationTime;
        return noErr;
    });

    if (!samplesBeforePresentationTime)
        return { nullptr, this };

    CMItemCount sampleCount = CMSampleBufferGetNumSamples(m_sample.get());
    if (samplesBeforePresentationTime >= sampleCount)
        return { this, nullptr };

    CMSampleBufferRef rawSampleBefore = nullptr;
    CFRange rangeBefore = CFRangeMake(0, samplesBeforePresentationTime);
    if (CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, m_sample.get(), rangeBefore, &rawSampleBefore) != noErr)
        return { nullptr, nullptr };
    RetainPtr<CMSampleBufferRef> sampleBefore = adoptCF(rawSampleBefore);

    CMSampleBufferRef rawSampleAfter = nullptr;
    CFRange rangeAfter = CFRangeMake(samplesBeforePresentationTime, sampleCount - samplesBeforePresentationTime);
    if (CMSampleBufferCopySampleBufferForRange(kCFAllocatorDefault, m_sample.get(), rangeAfter, &rawSampleAfter) != noErr)
        return { nullptr, nullptr };
    RetainPtr<CMSampleBufferRef> sampleAfter = adoptCF(rawSampleAfter);

    return { MediaSampleAVFObjC::create(sampleBefore.get(), m_id), MediaSampleAVFObjC::create(sampleAfter.get(), m_id) };
}

Ref<MediaSample> MediaSampleAVFObjC::createNonDisplayingCopy() const
{
    CMSampleBufferRef newSampleBuffer = 0;
    CMSampleBufferCreateCopy(kCFAllocatorDefault, m_sample.get(), &newSampleBuffer);
    ASSERT(newSampleBuffer);

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(newSampleBuffer, true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachmentsArray, i);
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);
    }

    return MediaSampleAVFObjC::create(adoptCF(newSampleBuffer).get(), m_id);
}

}
