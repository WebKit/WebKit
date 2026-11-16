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
#include "SkiaImageAtlasLayout.h"
#include "SkiaTextureAtlasPacker.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/HashSet.h>
#include <wtf/Hasher.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

// Builds SkiaImageAtlasLayout objects from raster images collected during tile recording.
// Used by GraphicsContextSkia in RecordingMode.
class SkiaImageAtlasLayoutBuilder {
    WTF_MAKE_TZONE_ALLOCATED(SkiaImageAtlasLayoutBuilder);
public:
    // Configuration constants.
    static constexpr int minAtlasSize = 256;
    static constexpr int maxAtlasSize = 4096;
    static constexpr int minImageSize = 8;
    static constexpr int maxImageSize = 512;
    static constexpr int minImagesForAtlas = 4;
    static constexpr int maxAtlasCount = 4;

    SkiaImageAtlasLayoutBuilder();
    ~SkiaImageAtlasLayoutBuilder();

    // Called during recording when a raster image is drawn.
    void collectRasterImage(const sk_sp<SkImage>&);

    // Check if an image was collected.
    bool isCollected(const SkImage* image) const { return m_collectedSet.contains(image); }

    // Number of collected images.
    size_t imageCount() const { return m_collectedImages.size(); }

    // Fingerprint of all collected images, computed incrementally.
    unsigned imageSetFingerprint() const { return m_hasher.hash(); }

    // Finalize: compute atlas packing, may create multiple atlases.
    // Returns vector of atlas layouts (empty if not enough images for atlasing).
    Vector<Ref<SkiaImageAtlasLayout>> finalize();

private:
    struct CollectedImage {
        sk_sp<SkImage> image;
        IntSize size;
    };

    // Multi-atlas packing when single atlas fails.
    Vector<Ref<SkiaImageAtlasLayout>> packMultipleAtlases(const Vector<IntSize>& sizes);

    // Binary search for max batch size that fits in atlas.
    size_t findMaxPackableBatch(const Vector<IntSize>& sizes, const IntSize& atlasSize);

    // Create atlas layout from packed rectangles.
    Ref<SkiaImageAtlasLayout> createAtlasLayout(const Vector<SkiaTextureAtlasPacker::PackedRect>& packedRects);

    // Calculate optimal atlas size based on total image area.
    static int calculateOptimalAtlasSize(const Vector<IntSize>& sizes);

    Vector<CollectedImage> m_collectedImages;
    HashSet<const SkImage*> m_collectedSet;
    Hasher m_hasher;
    bool m_finalized { false };
};

} // namespace WebCore

#endif // USE(SKIA)
