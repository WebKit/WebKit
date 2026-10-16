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
#include "HTMLVideoElement.h"
#include "PredefinedColorSpace.h"
#include "WebCodecsVideoFrame.h"
#include "WebGPUExternalTextureDescriptor.h"
#include <wtf/RefPtr.h>

#if PLATFORM(COCOA)
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

class HTMLVideoElement;
#if ENABLE(WEB_CODECS)
using GPUVideoSource = Variant<Ref<HTMLVideoElement>, Ref<WebCodecsVideoFrame>>;
#else
using GPUVideoSource = Ref<HTMLVideoElement>;
#endif

struct GPUExternalTextureDescriptor : public GPUObjectDescriptorBase {

#if ENABLE(VIDEO)
    static WebGPUVideoSourceIdentifier mediaIdentifierForSource(const GPUVideoSource& videoSource)
    {
#if ENABLE(WEB_CODECS)
        return WTF::switchOn(videoSource,
            [&](const Ref<HTMLVideoElement>& videoElement) -> WebGPUVideoSourceIdentifier {
                RefPtr player = videoElement->player();
                // Player needs to run in the GPU process to use the accelerated path
                if (player && player->isHostedInGPUProcess()) {
                    if (auto playerIdentifier = videoElement->playerIdentifier())
                        return playerIdentifier;
                }
                RefPtr<WebCore::VideoFrame> result;
                if (player)
                    result = player->videoFrameForCurrentTime();
                return result;
            },
            [&](const Ref<WebCodecsVideoFrame>& videoFrame) -> WebGPUVideoSourceIdentifier {
                return videoFrame->internalFrame();
            }
        );
#else
        return videoSource->playerIdentifier();
#endif
    }

    // The size the frame is presented at, which textureDimensions() reports and which the pixel buffer
    // travelling to the GPU process does not carry: a WebCodecs frame's display size is whatever its
    // constructor was given, independent of its coded size, and a video element's intrinsic size has
    // already had its pixel aspect ratio applied.
    static IntSize visibleSizeForSource(const GPUVideoSource& videoSource)
    {
#if ENABLE(WEB_CODECS)
        return WTF::switchOn(videoSource,
            [&](const Ref<HTMLVideoElement>& videoElement) {
                return IntSize { static_cast<int>(videoElement->videoWidth()), static_cast<int>(videoElement->videoHeight()) };
            },
            [&](const Ref<WebCodecsVideoFrame>& videoFrame) {
                return IntSize { static_cast<int>(videoFrame->displayWidth()), static_cast<int>(videoFrame->displayHeight()) };
            }
        );
#else
        return IntSize { static_cast<int>(videoSource->videoWidth()), static_cast<int>(videoSource->videoHeight()) };
#endif
    }

    std::optional<WebCore::MediaPlayerIdentifier> mediaIdentifier() const
    {
#if ENABLE(WEB_CODECS)
        return WTF::switchOn(source,
            [&](const Ref<HTMLVideoElement>& videoElement) -> std::optional<WebCore::MediaPlayerIdentifier> {
                RefPtr player = videoElement->player();
                if (!player || !player->isHostedInGPUProcess())
                    return std::nullopt;
                return videoElement->playerIdentifier();
            },
            [&](const Ref<WebCodecsVideoFrame>&) -> std::optional<WebCore::MediaPlayerIdentifier> {
                return std::nullopt;
            }
        );
#else
        return source->playerIdentifier();
#endif
    }
#endif

    WebGPUExternalTextureDescriptor convertToBacking() const
    {
        return {
            { label },
#if ENABLE(VIDEO)
            mediaIdentifierForSource(source),
#else
            { },
#endif
            colorSpace,
#if ENABLE(VIDEO)
            visibleSizeForSource(source),
#else
            { },
#endif
        };
    }

    Ref<JSON::Object> toJSON() const;

#if ENABLE(VIDEO)
    GPUVideoSource source;
#endif
    PredefinedColorSpace colorSpace { PredefinedColorSpace::SRGB };
};

}
