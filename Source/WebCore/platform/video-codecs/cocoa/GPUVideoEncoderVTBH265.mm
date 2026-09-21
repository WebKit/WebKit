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
#import "GPUVideoEncoderVTBH265.h"

#if USE(AVFOUNDATION)

#import <WebCore/HEVCUtilitiesCocoa.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUVideoEncoderVTBH265);

GPUVideoEncoderVTBH265::GPUVideoEncoderVTBH265(CreationInfo&& info, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
    : GPUVideoEncoderVTB(WTF::move(info), WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback))
{
}

bool GPUVideoEncoderVTBH265::convertAndNotify(RetainPtr<CMSampleBufferRef>&& sampleBuffer, GPUVideoEncoderFrameInfo&& info)
{
    if (useAnnexB()) {
        // FIXME: We need to call notifyDescription to provide the right color space.
        auto annexBBuffer = convertHEVCCMSampleBufferToAnnexB(sampleBuffer, info.isKeyFrame);
        if (annexBBuffer.isEmpty())
            return false;

        {
            m_bitstreamParser.parseBitstream(annexBBuffer.span());
            if (auto qp = m_bitstreamParser.lastSliceQP())
                info.qp = *qp;
        }

        notifyEncodedFrame(annexBBuffer.span(), info);
        return true;
    }

    RetainPtr blockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
    if (!blockBuffer)
        return false;

    Vector<uint8_t> buffer;
    size_t size = PAL::CMBlockBufferGetDataLength(blockBuffer);
    buffer.reserveInitialCapacity(size);
    for (size_t currentStart = 0; currentStart < size;) {
        char* data = nullptr;
        size_t length = 0;
        if (PAL::CMBlockBufferGetDataPointer(blockBuffer, currentStart, &length, nullptr, &data) != noErr)
            return false;
        buffer.append(unsafeMakeSpan(reinterpret_cast<const uint8_t*>(data), length));
        currentStart += length;
    }

    if (needsToSendDescription()) {
        RetainPtr formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);
        if (RetainPtr sampleExtensionsDict = dynamic_cf_cast<CFDictionaryRef>(PAL::CMFormatDescriptionGetExtension(formatDescription, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms))) {
            if (RetainPtr sampleExtensions = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(sampleExtensionsDict, CFSTR("hvcC")))) {
                setNeedsToSendDescription(false);
                notifyDescription(unsafeMakeSpan(CFDataGetBytePtr(sampleExtensions), static_cast<size_t>(CFDataGetLength(sampleExtensions))));
            }
        }
    }

    notifyEncodedFrame(buffer.span(), info);
    return true;
}

}

#endif // USE(AVFOUNDATION)
