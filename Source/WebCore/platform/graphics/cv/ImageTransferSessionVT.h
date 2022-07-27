/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "IntSize.h"
#include "VideoFrame.h"
#include <wtf/RetainPtr.h>

typedef struct CGImage *CGImageRef;
typedef struct OpaqueVTPixelTransferSession* VTPixelTransferSessionRef;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct __CVPixelBufferPool *CVPixelBufferPoolRef;
typedef struct __IOSurface *IOSurfaceRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class ImageTransferSessionVT {
public:
    static std::unique_ptr<ImageTransferSessionVT> create(uint32_t pixelFormat, bool shouldUseIOSurface = true)
    {
        return std::unique_ptr<ImageTransferSessionVT>(new ImageTransferSessionVT(pixelFormat, shouldUseIOSurface));
    }

    RefPtr<VideoFrame> convertVideoFrame(VideoFrame&, const IntSize&);
    RefPtr<VideoFrame> createVideoFrame(CGImageRef, const MediaTime&, const IntSize&, VideoFrame::Rotation = VideoFrame::Rotation::None, bool mirrored = false);
    RefPtr<VideoFrame> createVideoFrame(CMSampleBufferRef, const MediaTime&, const IntSize&, VideoFrame::Rotation = VideoFrame::Rotation::None, bool mirrored = false);

#if !PLATFORM(MACCATALYST)
    WEBCORE_EXPORT RefPtr<VideoFrame> createVideoFrame(IOSurfaceRef, const MediaTime&, const IntSize&, VideoFrame::Rotation = VideoFrame::Rotation::None, bool mirrored = false);
#endif

    uint32_t pixelFormat() const { return m_pixelFormat; }
    void setMaximumBufferPoolSize(size_t maxBufferPoolSize) { m_maxBufferPoolSize = maxBufferPoolSize; }

private:
    WEBCORE_EXPORT ImageTransferSessionVT(uint32_t pixelFormat, bool shouldUseIOSurface);

#if !PLATFORM(MACCATALYST)
    RetainPtr<CMSampleBufferRef> createCMSampleBuffer(IOSurfaceRef, const MediaTime&, const IntSize&);
#endif

    RetainPtr<CMSampleBufferRef> convertCMSampleBuffer(CMSampleBufferRef, const IntSize&, const MediaTime* = nullptr);
    RetainPtr<CMSampleBufferRef> createCMSampleBuffer(CVPixelBufferRef, const MediaTime&, const IntSize&);
    RetainPtr<CMSampleBufferRef> createCMSampleBuffer(CGImageRef, const MediaTime&, const IntSize&);

    RetainPtr<CVPixelBufferRef> convertPixelBuffer(CVPixelBufferRef, const IntSize&);
    RetainPtr<CVPixelBufferRef> createPixelBuffer(CMSampleBufferRef, const IntSize&);
    RetainPtr<CVPixelBufferRef> createPixelBuffer(CGImageRef, const IntSize&);

    bool setSize(const IntSize&);

    RetainPtr<VTPixelTransferSessionRef> m_transferSession;
    RetainPtr<CVPixelBufferPoolRef> m_outputBufferPool;
    bool m_shouldUseIOSurface { true };
    uint32_t m_pixelFormat;
    IntSize m_size;
    size_t m_maxBufferPoolSize { 0 };
};

}
