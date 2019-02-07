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

#import "PixelBufferConformerCV.h"
#import <JavaScriptCore/JSCInlines.h>
#import <JavaScriptCore/TypedArrayInlines.h>
#import <wtf/PrintStream.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

using namespace PAL;

WTF_DECLARE_CF_TYPE_TRAIT(CMSampleBuffer);

namespace WebCore {

static inline void releaseUint8Vector(void *array, const void*)
{
    adoptMallocPtr(static_cast<uint8_t*>(array));
}

RefPtr<MediaSampleAVFObjC> MediaSampleAVFObjC::createImageSample(Vector<uint8_t>&& array, unsigned long width, unsigned long height)
{
    CVPixelBufferRef pixelBuffer = nullptr;
    auto status = CVPixelBufferCreateWithBytes(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, array.data(), width * 4, releaseUint8Vector, array.releaseBuffer().leakPtr(), NULL, &pixelBuffer);
    auto imageBuffer = adoptCF(pixelBuffer);

    ASSERT_UNUSED(status, !status);
    if (!imageBuffer)
        return nullptr;

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, imageBuffer.get(), &formatDescription);
    ASSERT(!status);

    CMSampleTimingInfo sampleTimingInformation = { kCMTimeInvalid, kCMTimeInvalid, kCMTimeInvalid };

    CMSampleBufferRef sampleBuffer;
    status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, imageBuffer.get(), formatDescription, &sampleTimingInformation, &sampleBuffer);
    CFRelease(formatDescription);
    ASSERT(!status);

    auto sample = adoptCF(sampleBuffer);

    CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(sample.get(), true);
    for (CFIndex i = 0; i < CFArrayGetCount(attachmentsArray); ++i) {
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    }
    return create(sample.get());
}

MediaTime MediaSampleAVFObjC::presentationTime() const
{
    return PAL::toMediaTime(CMSampleBufferGetPresentationTimeStamp(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::outputPresentationTime() const
{
    return PAL::toMediaTime(CMSampleBufferGetOutputPresentationTimeStamp(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::decodeTime() const
{
    return PAL::toMediaTime(CMSampleBufferGetDecodeTimeStamp(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::duration() const
{
    return PAL::toMediaTime(CMSampleBufferGetDuration(m_sample.get()));
}

MediaTime MediaSampleAVFObjC::outputDuration() const
{
    return PAL::toMediaTime(CMSampleBufferGetOutputDuration(m_sample.get()));
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

uint32_t MediaSampleAVFObjC::videoPixelFormat() const
{
    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_sample.get()));
    return CVPixelBufferGetPixelFormatType(pixelBuffer);
}

static bool isCMSampleBufferRandomAccess(CMSampleBufferRef sample)
{
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return true;
    
    for (CFIndex i = 0, count = CFArrayGetCount(attachments); i < count; ++i) {
        CFDictionaryRef attachmentDict = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, i));
        if (CFDictionaryContainsKey(attachmentDict, kCMSampleAttachmentKey_NotSync))
            return false;
    }
    return true;
}

static bool isCMSampleBufferNonDisplaying(CMSampleBufferRef sample)
{
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (!attachments)
        return false;
    
    for (CFIndex i = 0; i < CFArrayGetCount(attachments); ++i) {
        CFDictionaryRef attachmentDict = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, i));
        if (CFDictionaryContainsKey(attachmentDict, kCMSampleAttachmentKey_DoNotDisplay))
            return true;
    }

    return false;
}

MediaSample::SampleFlags MediaSampleAVFObjC::flags() const
{
    int returnValue = MediaSample::None;
    
    if (isCMSampleBufferRandomAccess(m_sample.get()))
        returnValue |= MediaSample::IsSync;

    if (isCMSampleBufferNonDisplaying(m_sample.get()))
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
        timing.presentationTimeStamp = PAL::toCMTime(PAL::toMediaTime(timing.presentationTimeStamp) + offset);
        timing.decodeTimeStamp = PAL::toCMTime(PAL::toMediaTime(timing.decodeTimeStamp) + offset);
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
        timing.presentationTimeStamp = PAL::toCMTime(presentationTimestamp);
        timing.decodeTimeStamp = PAL::toCMTime(decodeTimestamp);
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
        if (PAL::toMediaTime(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) >= presentationTime)
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
        CFMutableDictionaryRef attachments = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, i));
        CFDictionarySetValue(attachments, kCMSampleAttachmentKey_DoNotDisplay, kCFBooleanTrue);
    }

    return MediaSampleAVFObjC::create(adoptCF(newSampleBuffer).get(), m_id);
}

RefPtr<JSC::Uint8ClampedArray> MediaSampleAVFObjC::getRGBAImageData() const
{
#if HAVE(CORE_VIDEO)
    const OSType imageFormat = kCVPixelFormatType_32RGBA;
    RetainPtr<CFNumberRef> imageFormatNumber = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &imageFormat));

    RetainPtr<CFMutableDictionaryRef> conformerOptions = adoptCF(CFDictionaryCreateMutable(0, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(conformerOptions.get(), kCVPixelBufferPixelFormatTypeKey, imageFormatNumber.get());
    PixelBufferConformerCV pixelBufferConformer(conformerOptions.get());

    auto pixelBuffer = static_cast<CVPixelBufferRef>(CMSampleBufferGetImageBuffer(m_sample.get()));
    auto rgbaPixelBuffer = pixelBufferConformer.convert(pixelBuffer);
    auto status = CVPixelBufferLockBaseAddress(rgbaPixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(status == noErr);

    void* data = CVPixelBufferGetBaseAddressOfPlane(rgbaPixelBuffer.get(), 0);
    size_t byteLength = CVPixelBufferGetHeight(pixelBuffer) * CVPixelBufferGetWidth(pixelBuffer) * 4;
    auto result = JSC::Uint8ClampedArray::tryCreate(JSC::ArrayBuffer::create(data, byteLength), 0, byteLength);

    status = CVPixelBufferUnlockBaseAddress(rgbaPixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(status == noErr);

    return result;
#else
    return nullptr;
#endif
}

String MediaSampleAVFObjC::toJSONString() const
{
    auto object = JSON::Object::create();

    object->setObject("pts"_s, presentationTime().toJSONObject());
    object->setObject("opts"_s, outputPresentationTime().toJSONObject());
    object->setObject("dts"_s, decodeTime().toJSONObject());
    object->setObject("duration"_s, duration().toJSONObject());
    object->setInteger("flags"_s, static_cast<unsigned>(flags()));
    object->setObject("presentationSize"_s, presentationSize().toJSONObject());

    return object->toJSONString();
}

}
