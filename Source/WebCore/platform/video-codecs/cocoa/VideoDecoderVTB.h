/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include <CoreMedia/CMTime.h>
#include <wtf/BlockPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;
typedef CVPixelBufferRef CVImageBufferRef;
typedef struct OpaqueCMTaggedBufferGroup *CMTaggedBufferGroupRef;
typedef UInt32 VTDecodeInfoFlags;
typedef struct OpaqueVTDecompressionSession*  VTDecompressionSessionRef;
typedef const struct opaqueCMFormatDescription* CMVideoFormatDescriptionRef;

namespace WebCore {

class VideoDecoderVTB : public ThreadSafeRefCounted<VideoDecoderVTB> {
public:
    static RefPtr<VideoDecoderVTB> create(CMVideoFormatDescriptionRef, CFDictionaryRef);
    ~VideoDecoderVTB();

    OSStatus flush();

    bool isHardwareAccelerated() const;
    bool canAccept(CMVideoFormatDescriptionRef) const;
    void setProperty(CFStringRef, CFTypeRef);

    // Callback may be called on any thread.
    using Callback = BlockPtr<void(OSStatus, RetainPtr<CVPixelBufferRef>&&, CMTime)>;
    void decodeFrame(CMSampleBufferRef, VTDecodeInfoFlags, Callback&&);

    // Callback may be called on any thread.
    using CallbackMultiImage = BlockPtr<void(OSStatus, VTDecodeInfoFlags, CVImageBufferRef, CMTaggedBufferGroupRef, CMTime, CMTime)>;
    void decodeMultiImageFrame(CMSampleBufferRef, VTDecodeInfoFlags, CallbackMultiImage&&);

private:
    explicit VideoDecoderVTB(RetainPtr<VTDecompressionSessionRef>&& session)
        : m_decompressionSession(WTF::move(session))
    {
    }

    const RetainPtr<VTDecompressionSessionRef> m_decompressionSession;
};

}

#endif
