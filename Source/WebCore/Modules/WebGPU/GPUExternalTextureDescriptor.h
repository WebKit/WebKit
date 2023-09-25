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

#include "GPUObjectDescriptorBase.h"
#include "GPUPredefinedColorSpace.h"
#include "HTMLVideoElement.h"
#include "WebCodecsVideoFrame.h"
#include "WebGPUExternalTextureDescriptor.h"
#include <wtf/RefPtr.h>

typedef struct __CVBuffer* CVPixelBufferRef;

namespace WebCore {

class HTMLVideoElement;
#if ENABLE(WEB_CODECS)
using GPUVideoSource = std::variant<RefPtr<HTMLVideoElement>, RefPtr<WebCodecsVideoFrame>>;
#else
using GPUVideoSource = RefPtr<HTMLVideoElement>;
#endif

struct GPUExternalTextureDescriptor : public GPUObjectDescriptorBase {

#if ENABLE(VIDEO)
    static WebGPU::VideoSourceIdentifier mediaIdentifierForSource(const GPUVideoSource& videoSource, CVPixelBufferRef& outPixelBuffer)
    {
#if ENABLE(WEB_CODECS)
        return WTF::switchOn(videoSource, [] (const RefPtr<HTMLVideoElement> videoElement) -> WebGPU::VideoSourceIdentifier {
            auto playerIdentifier = videoElement->playerIdentifier();
            return WebGPU::HTMLVideoElementIdentifier { playerIdentifier ? playerIdentifier->toUInt64() : 0 };
        }
        , [&outPixelBuffer] (const RefPtr<WebCodecsVideoFrame> videoFrame) -> WebGPU::VideoSourceIdentifier {
#if PLATFORM(COCOA)
            if (auto internalFrame = videoFrame->internalFrame()) {
                if (internalFrame->isRemoteProxy())
                    return WebGPU::WebCodecsVideoFrameIdentifier { internalFrame->resourceIdentifier() };

                outPixelBuffer = internalFrame->pixelBuffer();
            }
#else
            UNUSED_PARAM(videoFrame);
            UNUSED_PARAM(outPixelBuffer);
#endif
            return WebGPU::WebCodecsVideoFrameIdentifier { };
        });
#else
        UNUSED_PARAM(outPixelBuffer);
        auto playerIdentifier = videoSource->playerIdentifier();
        return WebGPU::HTMLVideoElementIdentifier { playerIdentifier ? playerIdentifier->toUInt64() : 0 };
#endif
    }
#endif

    WebGPU::ExternalTextureDescriptor convertToBacking() const
    {
        CVPixelBufferRef pixelBuffer = nullptr;
#if ENABLE(VIDEO)
        auto mediaIdentifier = mediaIdentifierForSource(source, pixelBuffer);
#else
        auto mediaIdentifier = WebGPU::HTMLVideoElementIdentifier { 0 };
#endif
        return {
            { label },
            mediaIdentifier,
            WebCore::convertToBacking(colorSpace),
            pixelBuffer
        };
    }

#if ENABLE(VIDEO)
    GPUVideoSource source;
#endif
    GPUPredefinedColorSpace colorSpace { GPUPredefinedColorSpace::SRGB };
};

}
