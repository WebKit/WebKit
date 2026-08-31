/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "SharedVideoFrame.h"
#include "WebGPUOrigin2D.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <optional>

namespace WebKit::WebGPU {

struct ImageCopyExternalImage {
    std::optional<Origin2D> origin;
    bool flipY { false };
    // Identifies the source ImageBuffer, which already lives in the GPU process. Null for the
    // sources which still take the CPU readback path in GPUQueue::copyExternalImageToTexture.
    std::optional<WebCore::RenderingResourceIdentifier> imageBufferIdentifier;
    // Whether that buffer's pixels are premultiplied by their alpha.
    bool premultipliedAlpha { true };
};

#if PLATFORM(COCOA) && ENABLE(VIDEO)
// The same copy, when the source is a video rather than an ImageBuffer. Unlike an ImageBuffer, a
// decoded frame carries no rendering resource identifier, so it is named the way an external texture
// names one: by the media player holding it, or - for a WebCodecs frame, which has no player - by
// shipping the frame itself through the shared video frame memory.
struct ImageCopyExternalImageVideoSource {
    std::optional<Origin2D> origin;
    bool flipY { false };
    std::optional<WebCore::MediaPlayerIdentifier> mediaIdentifier;
    std::optional<WebKit::SharedVideoFrame> sharedFrame;
};
#endif

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
