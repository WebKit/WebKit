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

#include "config.h"
#include "SkiaCompositingLayerImageSetBatch.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexture.h"
#include "CoordinatedTileBuffer.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "SkiaBackingStore.h"
#include "SkiaDamageRestriction.h"

namespace WebCore {

void SkiaCompositingLayerImageSetBatch::updatePaintProperties(SkCanvas& canvas, const sk_sp<SkColorFilter>& colorFilter, const std::optional<SkBlendMode>& blendMode)
{
    if (!m_imageSet.isEmpty() && (m_blendMode != blendMode || m_colorFilter != colorFilter))
        flushIfNeeded(canvas);

    m_blendMode = blendMode;
    m_colorFilter = colorFilter;
}

void SkiaCompositingLayerImageSetBatch::updateSamplingOptions(SkCanvas& canvas, SkSamplingOptions samplingOptions)
{
    if (!m_imageSet.isEmpty() && m_samplingOptions != samplingOptions)
        flushIfNeeded(canvas);

    m_samplingOptions = samplingOptions;
}

void SkiaCompositingLayerImageSetBatch::appendImageSet(Vector<SkCanvas::ImageSetEntry>&& imageSet)
{
    if (m_imageSet.isEmpty())
        m_imageSet = WTF::move(imageSet);
    else
        m_imageSet.appendVector(WTF::move(imageSet));
}

size_t SkiaCompositingLayerImageSetBatch::matrixIndexForDraw(SkCanvas& canvas, const SkMatrix& ctm, const SkSamplingOptions& sampling)
{
    updateSamplingOptions(canvas, sampling);

    if (m_preViewMatrices.isEmpty() || m_preViewMatrices.last() != ctm)
        m_preViewMatrices.append(ctm);

    return m_preViewMatrices.size() - 1;
}

void SkiaCompositingLayerImageSetBatch::addImageSet(SkCanvas& canvas, SkiaBackingStore& backingStore, const SkMatrix& ctm, float opacity, bool enableAntialias)
{
    const auto matrixIndex = matrixIndexForDraw(canvas, ctm, SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone));
    appendImageSet(backingStore.buildImageSet(canvas, ctm, matrixIndex, opacity, enableAntialias));
}

bool SkiaCompositingLayerImageSetBatch::addImageSetRestrictedToDamage(SkCanvas& canvas, SkiaBackingStore& backingStore, const SkMatrix& ctm, float opacity, const Vector<IntRect>& damageRects)
{
    ASSERT(!damageRects.isEmpty());

    SkRect layerDeviceRect;
    ctm.mapRect(&layerDeviceRect, SkRect(FloatRect { { }, backingStore.size() }));

    const auto plan = planDrawRestriction(ctm, layerDeviceRect, damageRects.span());
    switch (plan.kind) {
    case DrawRestrictionPlan::Kind::SkipDraw:
        // Fully outside the damage, so it draws nothing and stays batched.
        return true;
    case DrawRestrictionPlan::Kind::ClipFallback:
        // Can't narrow this to single rects, so let the caller flush and draw the tiles under a damage clip.
        return false;
    case DrawRestrictionPlan::Kind::PerRect:
        break;
    }

    const auto matrixIndex = matrixIndexForDraw(canvas, ctm, SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone));
    appendImageSet(backingStore.buildImageSet(canvas, ctm, matrixIndex, opacity, false, SkiaBackingStore::TilingDamageRestriction { ctm, plan.inverse, plan.rects.span() }));
    return true;
}

void SkiaCompositingLayerImageSetBatch::addImage(SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const FloatRect& clip, const SkMatrix& ctm, float opacity, bool enableAntialias)
{
    const size_t matrixIndex = matrixIndexForDraw(canvas, ctm, SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone));

    if (!clip.isEmpty()) {
        auto quad = SkRect(clip).toQuad();
        for (const auto& point : quad)
            m_dstClips.append(point);
    }

    unsigned aaFlags = enableAntialias ? SkCanvas::kAll_QuadAAFlags : SkCanvas::kNone_QuadAAFlags;
    m_imageSet.append(SkCanvas::ImageSetEntry(image, SkRect::MakeWH(image->width(), image->height()), SkRect(rect), matrixIndex, opacity, aaFlags, !clip.isEmpty()));
}

bool SkiaCompositingLayerImageSetBatch::addImageRestrictedToDamage(SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const SkMatrix& ctm, float opacity, const Vector<IntRect>& damageRects)
{
    ASSERT(!damageRects.isEmpty());

    const SkRect srcRectFull = SkRect::MakeWH(image->width(), image->height());
    const SkRect dstRectFull = SkRect(rect);
    SkRect deviceRect;
    ctm.mapRect(&deviceRect, dstRectFull);

    const auto plan = planDrawRestriction(ctm, deviceRect, damageRects.span());
    switch (plan.kind) {
    case DrawRestrictionPlan::Kind::SkipDraw:
        // Fully outside the damage, so it draws nothing and stays batched.
        return true;
    case DrawRestrictionPlan::Kind::ClipFallback:
        // Can't narrow this to single rects, so let the caller flush and draw the image under a damage clip.
        return false;
    case DrawRestrictionPlan::Kind::PerRect:
        break;
    }

    const auto matrixIndex = matrixIndexForDraw(canvas, ctm, SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone));
    forEachDamagedSubRect(deviceRect, dstRectFull, srcRectFull, plan.inverse, plan.rects.span(), [&](const SkRect& srcSubRect, const SkRect& dstSubRect) {
        m_imageSet.append(SkCanvas::ImageSetEntry(image, srcSubRect, dstSubRect, matrixIndex, opacity, SkCanvas::kNone_QuadAAFlags, false));
    });
    return true;
}

void SkiaCompositingLayerImageSetBatch::flushIfNeeded(SkCanvas& canvas)
{
    if (m_imageSet.isEmpty())
        return;

    SkPaint paint;
    if (m_blendMode)
        paint.setBlendMode(*m_blendMode);
    if (m_colorFilter)
        paint.setColorFilter(m_colorFilter);

    canvas.experimental_DrawEdgeAAImageSet(m_imageSet.span().data(), m_imageSet.size(), m_dstClips.span().data(),
        m_preViewMatrices.span().data(), m_samplingOptions, &paint, SkCanvas::kFast_SrcRectConstraint);

    m_imageSet.clear();
    m_dstClips.clear();
    m_preViewMatrices.clear();
    m_blendMode = std::nullopt;
    m_samplingOptions = { };
}

SkiaCompositingLayerImageSetBatch::ScopedFlush::ScopedFlush(SkCanvas& canvas, SkiaCompositingLayerImageSetBatch& imageSetBatch, Mode mode)
    : m_canvas(canvas)
    , m_imageSetBatch(imageSetBatch)
    , m_mode(mode)
    , m_saveCount(canvas.getSaveCount())
{
    if (m_mode != Mode::DoNothing) {
        m_imageSetBatch.flushIfNeeded(m_canvas);
        canvas.save();
    }
}

SkiaCompositingLayerImageSetBatch::ScopedFlush::~ScopedFlush()
{
    if (m_mode == Mode::FlushBeforeAndAfter)
        m_imageSetBatch.flushIfNeeded(m_canvas);

    if (m_mode != Mode::DoNothing)
        m_canvas.restoreToCount(m_saveCount);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
