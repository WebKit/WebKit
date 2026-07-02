/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL)

#include <WebCore/GCGLExtension.h>
#include <WebCore/GraphicsContextGLAttributes.h>
#include <WebCore/GraphicsTypesGL.h>
#include <array>
#include <wtf/OptionSet.h>

namespace WebKit {

struct RemoteGraphicsContextGLInitializationState {
    WebCore::GraphicsContextGLAttributes attributes;
    uint64_t knownActiveExtensions { 0 }; // EnumSet<WebCore::GCGLExtension> when EnumSet serialization works.
    uint64_t requestableExtensions { 0 }; // EnumSet<WebCore::GCGLExtension> when EnumSet serialization works.
    GCGLint maxCombinedTextureImageUnits { 0 };
    GCGLint maxVertexAttribs { 0 };
    GCGLint maxTextureSize { 0 };
    GCGLint maxCubeMapTextureSize { 0 };
    GCGLint maxRenderbufferSize { 0 };
    std::array<GCGLint, 2> maxViewportDims { { 0, 0 } };
    GCGLint maxSamples { 0 };
    GCGLint maxTransformFeedbackSeparateAttribs { 0 };
    GCGLint maxUniformBufferBindings { 0 };
    GCGLint uniformBufferOffsetAlignment { 0 };
    GCGLint max3DTextureSize { 0 };
    GCGLint maxArrayTextureLayers { 0 };
};

} // namespace WebKit

#endif
