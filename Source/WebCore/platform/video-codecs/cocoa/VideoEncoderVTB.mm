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
#import "VideoEncoderVTB.h"

#if USE(AVFOUNDATION)

#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cf/VideoToolboxSoftLink.h>

namespace WebCore {

RefPtr<VideoEncoderVTB> VideoEncoderVTB::create(int32_t width, int32_t height, CMVideoCodecType codecType, CFDictionaryRef encoderSpecification, CFDictionaryRef sourceImageBufferAttributes)
{
    VTCompressionSessionRef compressionSession = nullptr;
    auto result = PAL::VTCompressionSessionCreate(kCFAllocatorDefault, width, height, codecType, encoderSpecification, sourceImageBufferAttributes, nullptr, nullptr, nullptr, &compressionSession);
    if (result != noErr)
        return nullptr;
    RetainPtr session = adoptCF(compressionSession);
    return adoptRef(*new VideoEncoderVTB(WTF::move(session)));
}

VideoEncoderVTB::~VideoEncoderVTB()
{
    PAL::VTCompressionSessionInvalidate(m_compressionSession.get());
}

bool VideoEncoderVTB::isHardwareAccelerated() const
{
    CFBooleanRef isHardwareAccelerated = nullptr;
    VTSessionCopyProperty(m_compressionSession.get(), PAL::kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, kCFAllocatorDefault, &isHardwareAccelerated);
    return isHardwareAccelerated && isHardwareAccelerated == kCFBooleanTrue;
}

void VideoEncoderVTB::setProperty(CFStringRef key, CFTypeRef value)
{
    VTSessionSetProperty(m_compressionSession.get(), key, value);
}

OSStatus VideoEncoderVTB::prepareToEncodeFrames()
{
    return PAL::VTCompressionSessionPrepareToEncodeFrames(m_compressionSession.get());
}

OSStatus VideoEncoderVTB::completeFrames(CMTime completeUntilPresentationTimeStamp)
{
    return PAL::VTCompressionSessionCompleteFrames(m_compressionSession.get(), completeUntilPresentationTimeStamp);
}

OSStatus VideoEncoderVTB::encodeFrame(CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime duration, CFDictionaryRef frameProperties, Callback&& callback)
{
    return PAL::VTCompressionSessionEncodeFrameWithOutputHandler(m_compressionSession.get(), imageBuffer, presentationTimeStamp, duration, frameProperties, nullptr, makeBlockPtr([callback = WTF::move(callback)](OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer) mutable {
        callback(status, infoFlags, sampleBuffer);
    }).get());
}

} // namespace WebCore

#endif
