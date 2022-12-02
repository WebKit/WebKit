/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "VideoFrame.h"
#include "WebCodecsVideoFrame.h"

#if ENABLE(WEB_CODECS)

namespace WebCore {

bool isValidVideoFrameBufferInit(const WebCodecsVideoFrame::BufferInit&);
bool verifyRectOffsetAlignment(VideoPixelFormat, const DOMRectInit&);
bool verifyRectSizeAlignment(VideoPixelFormat, const DOMRectInit&);
ExceptionOr<DOMRectInit> parseVisibleRect(const DOMRectInit&, const std::optional<DOMRectInit>&, size_t codedWidth, size_t codedHeight, VideoPixelFormat);
size_t videoPixelFormatToPlaneCount(VideoPixelFormat);
size_t videoPixelFormatToSampleByteSizePerPlane();
size_t videoPixelFormatToSubSampling(VideoPixelFormat, size_t);

struct CombinedPlaneLayout {
    size_t allocationSize { 0 };
    Vector<ComputedPlaneLayout> computedLayouts;
};

ExceptionOr<CombinedPlaneLayout> computeLayoutAndAllocationSize(const DOMRectInit&, const std::optional<Vector<PlaneLayout>>&, VideoPixelFormat);

ExceptionOr<CombinedPlaneLayout> parseVideoFrameCopyToOptions(const WebCodecsVideoFrame&, const WebCodecsVideoFrame::CopyToOptions&);

void initializeVisibleRectAndDisplaySize(WebCodecsVideoFrame&, const WebCodecsVideoFrame::Init&, const DOMRectInit&, size_t defaultDisplayWidth, size_t defaultDisplayHeight);

VideoColorSpaceInit videoFramePickColorSpace(const std::optional<VideoColorSpaceInit>&, VideoPixelFormat);

bool validateVideoFrameInit(const WebCodecsVideoFrame::Init&, size_t codedWidth, size_t codedHeight, VideoPixelFormat);

}

#endif
