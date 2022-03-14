/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)
#include <optional>
#include <wtf/EnumTraits.h>

namespace WebCore {

enum class GraphicsContextGLPowerPreference {
    Default,
    LowPower,
    HighPerformance
};

enum class GraphicsContextGLWebGLVersion {
    WebGL1,
#if ENABLE(WEBGL2)
    WebGL2
#endif
};

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
using PlatformGPUID = uint64_t;
#endif

struct GraphicsContextGLAttributes {
    // WebGLContextAttributes
    bool alpha { true };
    bool depth { true };
    bool stencil { false };
    bool antialias { true };
    bool premultipliedAlpha { true };
    bool preserveDrawingBuffer { false };
    bool failIfMajorPerformanceCaveat { false };
    using PowerPreference = GraphicsContextGLPowerPreference;
    PowerPreference powerPreference { PowerPreference::Default };

    // Additional attributes.
    bool shareResources { true };
    bool noExtensions { false };
    float devicePixelRatio { 1 };
    PowerPreference initialPowerPreference { PowerPreference::Default };
    using WebGLVersion = GraphicsContextGLWebGLVersion;
    WebGLVersion webGLVersion { WebGLVersion::WebGL1 };
    bool forceRequestForHighPerformanceGPU { false };
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    PlatformGPUID windowGPUID { 0 };
#endif
#if PLATFORM(COCOA)
    bool useMetal { true };
#endif
#if ENABLE(WEBXR)
    bool xrCompatible { false };
#endif

    PowerPreference effectivePowerPreference() const
    {
        if (forceRequestForHighPerformanceGPU)
            return PowerPreference::HighPerformance;
        return powerPreference;
    }

#if ENABLE(GPU_PROCESS)
    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<GraphicsContextGLAttributes> decode(Decoder&);
#endif
};

#if ENABLE(GPU_PROCESS)

template<typename Encoder>
void GraphicsContextGLAttributes::encode(Encoder& encoder) const
{
    encoder << alpha
        << depth
        << stencil
        << antialias
        << premultipliedAlpha
        << preserveDrawingBuffer
        << failIfMajorPerformanceCaveat
        << powerPreference
        << shareResources
        << noExtensions
        << devicePixelRatio
        << initialPowerPreference
        << webGLVersion
        << forceRequestForHighPerformanceGPU;
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    encoder << windowGPUID;
#endif
#if PLATFORM(COCOA)
    encoder << useMetal;
#endif
#if ENABLE(WEBXR)
    encoder << xrCompatible;
#endif
}

template<typename Decoder>
std::optional<WebCore::GraphicsContextGLAttributes> GraphicsContextGLAttributes::decode(Decoder& decoder)
{
    auto alpha = decoder.template decode<bool>();
    auto depth = decoder.template decode<bool>();
    auto stencil = decoder.template decode<bool>();
    auto antialias = decoder.template decode<bool>();
    auto premultipliedAlpha = decoder.template decode<bool>();
    auto preserveDrawingBuffer = decoder.template decode<bool>();
    auto failIfMajorPerformanceCaveat = decoder.template decode<bool>();
    auto powerPreference = decoder.template decode<PowerPreference>();
    auto shareResources = decoder.template decode<bool>();
    auto noExtensions = decoder.template decode<bool>();
    auto devicePixelRatio = decoder.template decode<float>();
    auto initialPowerPreference = decoder.template decode<PowerPreference>();
    auto webGLVersion = decoder.template decode<WebGLVersion>();
    auto forceRequestForHighPerformanceGPU = decoder.template decode<bool>();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    auto windowGPUID = decoder.template decode<PlatformGPUID>();
#endif
#if PLATFORM(COCOA)
    auto useMetal = decoder.template decode<bool>();
#endif
#if ENABLE(WEBXR)
    auto xrCompatible = decoder.template decode<bool>();
#endif
    if (!decoder.isValid())
        return std::nullopt;
    return GraphicsContextGLAttributes {
        *alpha,
        *depth,
        *stencil,
        *antialias,
        *premultipliedAlpha,
        *preserveDrawingBuffer,
        *failIfMajorPerformanceCaveat,
        *powerPreference,
        *shareResources,
        *noExtensions,
        *devicePixelRatio,
        *initialPowerPreference,
        *webGLVersion,
        *forceRequestForHighPerformanceGPU,
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        *windowGPUID,
#endif
#if PLATFORM(COCOA)
        *useMetal,
#endif
#if ENABLE(WEBXR)
        *xrCompatible
#endif
    };
}

#endif

}

#if ENABLE(GPU_PROCESS)
namespace WTF {

template <> struct EnumTraits<WebCore::GraphicsContextGLPowerPreference> {
    using values = EnumValues<
    WebCore::GraphicsContextGLPowerPreference,
    WebCore::GraphicsContextGLPowerPreference::Default,
    WebCore::GraphicsContextGLPowerPreference::LowPower,
    WebCore::GraphicsContextGLPowerPreference::HighPerformance
    >;
};

template <> struct EnumTraits<WebCore::GraphicsContextGLWebGLVersion> {
    using values = EnumValues<
    WebCore::GraphicsContextGLWebGLVersion,
    WebCore::GraphicsContextGLWebGLVersion::WebGL1
#if ENABLE(WEBGL2)
    , WebCore::GraphicsContextGLWebGLVersion::WebGL2
#endif
    >;
};

}

#endif

#endif // ENABLE(WEBGL)
