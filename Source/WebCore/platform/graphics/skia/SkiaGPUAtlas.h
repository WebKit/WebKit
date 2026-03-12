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
#include "BitmapTexture.h"
#include "IntSize.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
#include <skia/core/SkRect.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class SkiaImageAtlasLayout;

using ImageToRectMap = HashMap<const SkImage*, SkRect>;

// GPU atlas that uploads raster images into a BitmapTexture.
// Uses MemoryMappedGPUBuffer directly for DMA-buf (GBM) path and
// BitmapTexture::updateContents() for GL fallback.
class SkiaGPUAtlas : public ThreadSafeRefCounted<SkiaGPUAtlas, WTF::DestructionThread::Main> {
    WTF_MAKE_TZONE_ALLOCATED(SkiaGPUAtlas);
    WTF_MAKE_NONCOPYABLE(SkiaGPUAtlas);
public:
    static RefPtr<SkiaGPUAtlas> create(const SkiaImageAtlasLayout&, Ref<BitmapTexture>&&);

    // Upload images into the atlas texture. Can run from any thread for DMA-buf backed textures.
    bool uploadImages();

    virtual ~SkiaGPUAtlas();

    const GrBackendTexture& backendTexture() const LIFETIME_BOUND { return m_backendTexture; }
    const ImageToRectMap& imageToRect() const LIFETIME_BOUND { return m_imageToRect; }
    const IntSize& size() const LIFETIME_BOUND { return m_size; }
    BitmapTexture& atlasTexture() const { return m_atlasTexture.get(); }

private:
    SkiaGPUAtlas(Ref<BitmapTexture>&&, GrBackendTexture&&, const SkiaImageAtlasLayout&, const IntSize&);

    Ref<BitmapTexture> m_atlasTexture;
    GrBackendTexture m_backendTexture;
    ImageToRectMap m_imageToRect;
    Ref<const SkiaImageAtlasLayout> m_layout;
    IntSize m_size;
};

} // namespace WebCore

#endif // USE(SKIA)
