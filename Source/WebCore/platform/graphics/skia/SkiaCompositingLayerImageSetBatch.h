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

#include "SkiaDamageRegion.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColorFilter.h>
#include <skia/core/SkImage.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {
class FloatRect;
class SkiaBackingStore;

class SkiaCompositingLayerImageSetBatch {
    WTF_MAKE_NONCOPYABLE(SkiaCompositingLayerImageSetBatch);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    SkiaCompositingLayerImageSetBatch() = default;
    ~SkiaCompositingLayerImageSetBatch() = default;

    void updatePaintProperties(SkCanvas&, const sk_sp<SkColorFilter>&, const std::optional<SkBlendMode>&);

    // A null damage region draws everything. Otherwise the draw is drawn whole when the damage covers it,
    // split by the rects of the region when the transform allows, and otherwise flushed and drawn under a
    // damage clip with fallbackPaint, which is used in that case only.
    void addImageSet(SkCanvas&, SkiaBackingStore&, const SkM44& transform, float opacity, bool enableAntialias, const SkiaDamageRegion*, const SkPaint& fallbackPaint);
    void addImage(SkCanvas&, const sk_sp<SkImage>&, const FloatRect&, const SkM44& transform, float opacity, bool enableAntialias, const SkiaDamageRegion*, const SkPaint& fallbackPaint);

    void flushIfNeeded(SkCanvas&);

    class ScopedFlush {
    public:
        enum class Mode : uint8_t { DoNothing, FlushBefore, FlushBeforeAndAfter };
        ScopedFlush(SkCanvas&, SkiaCompositingLayerImageSetBatch&, Mode);
        ~ScopedFlush();

    private:
        ScopedFlush(ScopedFlush&&) = delete;
        ScopedFlush(const ScopedFlush&) = delete;
        ScopedFlush& operator=(ScopedFlush&&) = delete;
        ScopedFlush& operator=(const ScopedFlush&) = delete;

        SkCanvas& m_canvas;
        SkiaCompositingLayerImageSetBatch& m_imageSetBatch;
        Mode m_mode { Mode::DoNothing };
        int m_saveCount { 0 };
    };

private:
    // What the caller has left to do to limit one draw to the damage.
    enum class RestrictedDraw : uint8_t {
        Done,
        Whole,
        Split
    };

    // Entries here carry no clip quad, so a draw needing a damage clip ends the batch.
    template<typename FallbackDrawFunction>
    RestrictedDraw planRestrictedDraw(SkCanvas& canvas, const SkM44& transform, const SkMatrix& ctm, const SkRect& deviceRect, const SkiaDamageRegion& damageRegion, SkMatrix& inverse, NOESCAPE const FallbackDrawFunction& fallbackDraw)
    {
        ASSERT(!damageRegion.isEmpty());

        if (damageRegion.covers(deviceRect))
            return RestrictedDraw::Whole;

        switch (damageRegion.planDraw(ctm, deviceRect, inverse)) {
        case SkiaDamageRegion::DrawDamageStrategy::Skip:
            return RestrictedDraw::Done;
        case SkiaDamageRegion::DrawDamageStrategy::ClipToDamage: {
            ScopedFlush autoFlush(canvas, *this, ScopedFlush::Mode::FlushBefore);
            canvas.concat(transform);
            damageRegion.clipCanvasInDeviceSpace(canvas);
            fallbackDraw(canvas);
            return RestrictedDraw::Done;
        }
        case SkiaDamageRegion::DrawDamageStrategy::SplitByRect:
            break;
        }

        return RestrictedDraw::Split;
    }

    void updateSamplingOptions(SkCanvas&, SkSamplingOptions);
    bool imageRequiresLinearSampling(const SkCanvas&, const sk_sp<SkImage>&, const FloatRect&, const SkMatrix&) const;
    SkSamplingOptions samplingOptionsForImage(const SkCanvas&, const sk_sp<SkImage>&, const FloatRect&, const SkMatrix&) const;
    size_t matrixIndexForDraw(const SkMatrix& ctm);

    Vector<SkCanvas::ImageSetEntry> m_imageSet;
    Vector<SkMatrix> m_preViewMatrices;
    sk_sp<SkColorFilter> m_colorFilter;
    std::optional<SkBlendMode> m_blendMode;
    SkSamplingOptions m_samplingOptions;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
