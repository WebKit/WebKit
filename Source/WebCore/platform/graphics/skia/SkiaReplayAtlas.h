/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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

#include "IntSize.h"
#include <optional>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
#include <skia/core/SkRect.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SkiaGPUAtlas;

// Per-worker atlas wrapper for GPU texture access during replay.
// Takes a pre-uploaded GPU atlas (from SkiaRecordingResult) and rewraps
// the texture for the worker thread's GrDirectContext.
class SkiaReplayAtlas {
    WTF_MAKE_TZONE_ALLOCATED(SkiaReplayAtlas);
public:
    // Create atlas wrapper from pre-prepared GPU atlas, rewraps the texture for the current context.
    static std::unique_ptr<SkiaReplayAtlas> create(const SkiaGPUAtlas&);

    ~SkiaReplayAtlas();

    const sk_sp<SkImage>& atlasTexture() const { return m_rewrappedTexture; }

    // Lookup: original raster image -> rect within atlas texture.
    // Returns nullopt if image is not in this atlas.
    std::optional<SkRect> rectForImage(const SkImage&) const;

private:
    SkiaReplayAtlas(Ref<const SkiaGPUAtlas>&&, sk_sp<SkImage>&&);

    Ref<const SkiaGPUAtlas> m_gpuAtlas;
    sk_sp<SkImage> m_rewrappedTexture;
};

} // namespace WebCore

#endif // USE(SKIA)
