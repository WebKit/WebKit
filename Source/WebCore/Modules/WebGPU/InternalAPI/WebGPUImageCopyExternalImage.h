/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include <WebCore/ImageBuffer.h>
#include <WebCore/WebGPUExternalTextureDescriptor.h>
#include <WebCore/WebGPUOrigin2D.h>
#include <optional>
#include <wtf/RefPtr.h>

namespace WebCore::WebGPU {

struct ImageCopyExternalImage {
    std::optional<Origin2D> origin;
    bool flipY { false };
    // The source, when it is backed by an ImageBuffer whose pixels are already GPU-resident
    // (a canvas, an OffscreenCanvas, or an ImageBitmap). Null for the sources which still take
    // the CPU readback path in GPUQueue::copyExternalImageToTexture.
    RefPtr<WebCore::ImageBuffer> imageBuffer;
    // Whether those pixels are premultiplied by their alpha. True for anything a 2D context
    // composited, but an ImageBitmap created with premultiplyAlpha: "none" holds them straight.
    bool premultipliedAlpha { true };
    // Set in place of imageBuffer when the source is a video element or a WebCodecs frame. The
    // decoded frame already lives in the GPU process, so it is named rather than read back, the
    // same way importExternalTexture() names one.
    std::optional<VideoSourceIdentifier> videoSource;
#if ENABLE(VIDEO)
    // How that frame has to be transformed to be presented, which its pixels are not stored with:
    // a horizontal mirror if videoSourceIsMirrored, then a clockwise rotation. A frame a media
    // player decoded has already been transformed, so both are inert for one; a WebCodecs frame
    // carries its transform as state instead.
    VideoFrameRotation videoSourceRotation { VideoFrameRotation::None };
    bool videoSourceIsMirrored { false };
#endif
};

} // namespace WebCore::WebGPU
