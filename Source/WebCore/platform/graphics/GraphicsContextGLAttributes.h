/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

enum class GraphicsContextGLSimulatedCreationFailure : uint8_t {
    None,
    IPCBufferOOM,
    CreationTimeout,
    FailPlatformContextCreation
};

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
using PlatformGPUID = uint64_t;
#endif

struct GraphicsContextGLAttributes {
    bool alpha { true };
    bool depth { true };
    bool stencil { false };
    bool antialias { true };
    bool premultipliedAlpha { true };
    bool preserveDrawingBuffer { false };
    GraphicsContextGLPowerPreference powerPreference { GraphicsContextGLPowerPreference::Default };
    bool isWebGL2 { false };
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    PlatformGPUID windowGPUID { 0 };
#endif
#if PLATFORM(COCOA)
    bool useMetal { true };
#endif
#if ENABLE(WEBXR)
    bool xrCompatible { false };
#endif
    using SimulatedCreationFailure = GraphicsContextGLSimulatedCreationFailure;
    SimulatedCreationFailure failContextCreationForTesting { SimulatedCreationFailure::None };
};
}

#endif // ENABLE(WEBGL)
