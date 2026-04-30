/*
 * Copyright (C) 2026 Igalia S.L.
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

#if USE(SKIA)

#include <memory>
#include <optional>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkBlendMode.h>
#include <skia/core/SkRefCnt.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/GrDirectContext.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

class SkImage;
class SkSurface;
class SkSurfaceProps;
enum GrSurfaceOrigin : int;

namespace WebCore {

class BitmapTexture;
class GLFence;
enum class BlendMode : uint8_t;
enum class CompositeOperator : uint8_t;

namespace SkiaUtilities {

// Creates a GrBackendTexture from a BitmapTexture's GL texture.
GrBackendTexture createBackendTexture(const BitmapTexture&);

// Creates a Skia surface wrapping a BitmapTexture's GL texture.
sk_sp<SkSurface> createSurface(GrDirectContext*, const BitmapTexture&, const SkSurfaceProps&, GrSurfaceOrigin = kTopLeft_GrSurfaceOrigin, unsigned sampleCount = 0);

// Rewraps a GPU-backed SkImage for use in a different GrDirectContext.
// Extracts the backend texture from the source image and creates a new
// SkImage wrapper around it for the target context, preserving the
// original image's color type, alpha type, and color space.
sk_sp<SkImage> rewrapImageForContext(GrDirectContext*, const SkImage&);

// Wraps a GrBackendTexture as a non-owning SkImage for the given context,
// using RGBA8, premultiplied alpha, and sRGB color space.
sk_sp<SkImage> borrowBackendTextureAsImage(GrDirectContext*, const GrBackendTexture&);

// Extracts the GL texture ID from a GPU-backed SkImage, if available.
std::optional<unsigned> retrieveGLTextureID(const SkImage&);

// Flushes the surface, submits pending GPU commands, and creates a GLFence
// for async synchronization. Falls back to synchronous submit if fences
// are not supported or creation fails, and returns nullptr.
std::unique_ptr<GLFence> flushAndSubmitSurfaceWithFence(GrDirectContext*, SkSurface*);

// Flushes the image, submits pending GPU commands, and creates a GLFence
// for async synchronization. Falls back to synchronous submit if fences
// are not supported or creation fails, and returns nullptr.
std::unique_ptr<GLFence> flushAndSubmitImageWithFence(GrDirectContext*, const sk_sp<SkImage>&);

// Flushes and submits pending GPU commands and creates a GLFence for async
// synchronization. Falls back to synchronous submit if fences are not
// supported or creation fails, and returns nullptr.
std::unique_ptr<GLFence> flushAndSubmitWithFence(GrDirectContext*);

SkBlendMode toSkiaBlendMode(BlendMode, std::optional<CompositeOperator> = std::nullopt);

} // namespace SkiaUtilities
} // namespace WebCore

#endif // USE(SKIA)
