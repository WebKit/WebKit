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

#include "WebGPUObjectDescriptorBase.h"
#include "WebGPUPredefinedColorSpace.h"
#include <wtf/Vector.h>

#if ENABLE(VIDEO) && PLATFORM(COCOA)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore::WebGPU {

struct HTMLVideoElementIdentifier {
    uint64_t identifier;
};
struct WebCodecsVideoFrameIdentifier {
    std::pair<uint64_t, uint64_t> identifier;
};

using VideoSourceIdentifier = std::variant<HTMLVideoElementIdentifier, WebCodecsVideoFrameIdentifier>;

struct ExternalTextureDescriptor : public ObjectDescriptorBase {
    VideoSourceIdentifier mediaIdentifier;
    PredefinedColorSpace colorSpace { PredefinedColorSpace::SRGB };
#if ENABLE(VIDEO) && PLATFORM(COCOA)
    RetainPtr<CVPixelBufferRef> pixelBuffer { nullptr };
#else
    void *pixelBuffer { nullptr };
#endif
};

} // namespace WebCore::WebGPU
