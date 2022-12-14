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

enum class GraphicsContextGLPowerPreference : uint8_t {
    Default,
    LowPower,
    HighPerformance
};

enum class GraphicsContextGLWebGLVersion : uint8_t {
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
    bool failPlatformContextCreationForTesting { false };

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
};

}

#endif // ENABLE(WEBGL)
