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

#pragma once

#if USE(AVFOUNDATION)

#include <CoreMedia/CMFormatDescription.h>
#include <CoreMedia/CMTime.h>
#include <wtf/BlockPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;
typedef CVPixelBufferRef CVImageBufferRef;
typedef UInt32 VTEncodeInfoFlags;
typedef struct OpaqueVTCompressionSession* VTCompressionSessionRef;

namespace WebCore {

class VideoEncoderVTBSession : public ThreadSafeRefCounted<VideoEncoderVTBSession> {
public:
    static RefPtr<VideoEncoderVTBSession> create(int32_t width, int32_t height, CMVideoCodecType, CFDictionaryRef encoderSpecification, CFDictionaryRef sourceImageBufferAttributes);
    ~VideoEncoderVTBSession();

    bool isHardwareAccelerated() const;
    void setProperty(CFStringRef, CFTypeRef);

    OSStatus prepareToEncodeFrames();
    OSStatus completeFrames(CMTime completeUntilPresentationTimeStamp);

    // Callback may be called on any thread.
    using Callback = BlockPtr<void(OSStatus, VTEncodeInfoFlags, CMSampleBufferRef)>;
    OSStatus encodeFrame(CVImageBufferRef, CMTime presentationTimeStamp, CMTime duration, CFDictionaryRef frameProperties, Callback&&);

private:
    explicit VideoEncoderVTBSession(RetainPtr<VTCompressionSessionRef>&& session)
        : m_compressionSession(WTF::move(session))
    {
    }

    const RetainPtr<VTCompressionSessionRef> m_compressionSession;
};

}

#endif
