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

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "CoordinatedBackingStoreProxy.h"
#include "FloatRect.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/HashMap.h>

namespace WebCore {
class CoordinatedTileBuffer;

class SkiaBackingStore {
    WTF_MAKE_TZONE_ALLOCATED(SkiaBackingStore);
public:
    SkiaBackingStore() = default;
    ~SkiaBackingStore() = default;

    float scale() const { return m_scale; }

    void update(const FloatSize&, float scale, CoordinatedBackingStoreProxy::Update&&);
    void paintToCanvas(SkCanvas&, const SkPaint&);
    void drawDebugBorders(SkCanvas&, const SkPaint&);

private:
    class Tile {
    public:
        Tile() = default;
        explicit Tile(float scale)
            : m_scale(scale)
        {
        }

        ~Tile() = default;

        void update(const IntRect& dirtyRect, const IntRect& tileRect, CoordinatedTileBuffer&);
        const FloatRect& rect() const LIFETIME_BOUND { return m_rect; }
        sk_sp<SkImage> image();

    private:
        bool tryEnsureSurface(const IntSize&, CoordinatedTileBuffer&);

        float m_scale { 1. };
        FloatRect m_rect;
        sk_sp<SkSurface> m_surface;
        unsigned m_textureID { 0 };
        sk_sp<SkImage> m_cachedImage;
    };

    HashMap<uint32_t, Tile> m_tiles;
    FloatSize m_size;
    float m_scale { 1. };
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
