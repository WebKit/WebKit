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

#include "Damage.h"
#include "FloatRect.h"
#include "IntRect.h"

#include <optional>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkMatrix.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkRegion.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Vector.h>

namespace WebCore {

// The damage to repaint, in device space, held twice: as rects for the draws that split, and as a region
// for the draws that clip and for culling. Built once per frame, so no draw site rebuilds the region.
// Damage bounds the rect count by its grid cells, which keeps the per-rect work bounded as well.
struct DamageRegion {
    Vector<SkRect> rects;
    SkRegion region;

    // The rects must never overlap, otherwise a split draw would composite translucent content twice.
    // Damage::rectsForPainting() guarantees that.
    static DamageRegion create(const Damage& damage)
    {
        const auto damageRects = damage.rectsForPainting().map([](const IntRect& rect) -> SkIRect {
            return rect;
        });

        // Convert to the Skia rect types once here, never per draw.
        DamageRegion damageRegion;
        damageRegion.region.setRects(damageRects.span().data(), damageRects.size());
        damageRegion.rects = damageRects.map([](const SkIRect& rect) -> SkRect {
            return SkRect::Make(rect);
        });

        return damageRegion;
    }

    bool isEmpty() const { return rects.isEmpty(); }

    // roundOut() applies floor to left/top, and ceil to right/bottom. Make sure a deviceRect touching the
    // region along an edge triggers an intersection - this is conservative, but for sure not wrong.
    bool intersects(const SkRect& deviceRect) const { return region.intersects(deviceRect.roundOut()); }
};

// Plans a frame, given everything the render target being drawn into must repaint to become current. That
// is not the damage of the frame being drawn: the swap chain hands back whichever buffer is free, and one
// that is a frame or more behind still misses every change since it was last presented. So the restriction
// is planned per target, never per frame.
inline std::optional<DamageRegion> planFrameRestriction(const std::optional<Damage>& repaintRegion, const IntSize& surfaceSize)
{
    // Nothing known about the target's contents -> repaint everything.
    if (!repaintRegion)
        return std::nullopt;

    // The target misses nothing -> it is already up-to-date, so there is nothing to repaint.
    if (repaintRegion->isEmpty())
        return DamageRegion { };

    // The damage covers the whole surface -> restricting the draws would only cost time.
    if (repaintRegion->bounds().contains(IntRect { { }, surfaceSize }))
        return std::nullopt;

    // Everything else: restrict the draws to the region's rects.
    return DamageRegion::create(*repaintRegion);
}

// How a single draw (an image or a rect) gets restricted to the damage. The preferred way is to split the
// draw into one sub-draw per damage rect it touches (with no cap on the count!). The sub-draws share a
// paint, so Skia batches them into one operation. A device-space clip is only the fallback, since a clip
// of more than one rect cannot become a scissor: Skia then needs a stencil buffer or a mask texture,
// and batching is lost.
struct DrawRestrictionPlan {
    enum class Mode : uint8_t {
        SkipDraw, // The draw misses the damage.
        PerRect, // One draw per touched damage rect.
        ClipFallback // One draw under a device-space clip (CTM rotated/skewed, misaligning rects).
    };

    Mode mode { Mode::SkipDraw };
    SkMatrix inverse; // Optimization for PerRect mode: maps device rects back to local coordinates.
};

inline DrawRestrictionPlan planDrawRestriction(const SkMatrix& ctm, const SkRect& deviceRect, const DamageRegion& damageRegion)
{
    DrawRestrictionPlan plan;
    if (!damageRegion.intersects(deviceRect))
        return plan;

    if (!ctm.rectStaysRect() || !ctm.invert(&plan.inverse))
        plan.mode = DrawRestrictionPlan::Mode::ClipFallback;
    else
        plan.mode = DrawRestrictionPlan::Mode::PerRect;

    return plan;
}

// Receives a callback 'emit' and calls emit(srcSubRect, dstSubRect) _once per damage rect_ that touches deviceRect,
// narrowing the source sampling region to the damaged sub-rect. deviceRect is dstRectFull under the rectilinear ctm,
// and inverseCtm maps device rects back.
template<typename EmitFunction>
void forEachDamagedSubRect(const SkRect& deviceRect, const SkRect& dstRectFull, const SkRect& srcRectFull, const SkMatrix& inverseCtm, const DamageRegion& damageRegion, NOESCAPE const EmitFunction& emit)
{
    const auto dstToSrc = SkMatrix::RectToRect(dstRectFull, srcRectFull);

    for (const auto& rect : damageRegion.rects) {
        auto damaged = rect;
        if (!damaged.intersect(deviceRect))
            continue;
        if (damaged == deviceRect) {
            emit(srcRectFull, dstRectFull);
            return;
        }
        SkRect dstSubRect;
        inverseCtm.mapRect(&dstSubRect, damaged);
        emit(dstToSrc.mapRect(dstSubRect), dstSubRect);
    }
}

inline void clipToDamageInDeviceSpace(SkCanvas& canvas, const DamageRegion& damageRegion)
{
    // An empty region would clip everything out, which no caller wants.
    ASSERT(!damageRegion.isEmpty());

    // The region is in device space, so it clips without inverting the CTM!
    canvas.clipRegion(damageRegion.region, SkClipOp::kIntersect);
}

// Overwrites the damaged pixels with color, the way a regular clear would. The region is drawn in kSrc mode
// rather than clearing under it as a clip, for the same reason the draws split: a clip of more than one
// rect cannot be a scissor, and Skia would build a clip mask for it. Without a region, clear everything.
inline void clearRestrictedToDamage(SkCanvas& canvas, SkColor color, const DamageRegion* damageRegion)
{
    if (!damageRegion) {
        canvas.clear(color);
        return;
    }

    ASSERT(!damageRegion->isEmpty());

    SkPaint paint;
    paint.setColor(color);
    paint.setBlendMode(SkBlendMode::kSrc);

    // The region is in device space, and the canvas has no transform at this point.
    canvas.drawRegion(damageRegion->region, paint);
}

// Runs draw(srcSubRect, dstSubRect) restricted to the damage, picking the strategy per DrawRestrictionPlan.
template<typename DrawFunction>
void drawRestrictedToDamage(SkCanvas& canvas, const DamageRegion* damageRegion, const SkRect& srcRectFull, const SkRect& dstRectFull, NOESCAPE const DrawFunction& draw)
{
    // Without a region, the draw runs once, unrestricted.
    if (!damageRegion) {
        draw(srcRectFull, dstRectFull);
        return;
    }

    ASSERT(!damageRegion->isEmpty());

    const auto ctm = canvas.getLocalToDeviceAs3x3();
    SkRect deviceRect;
    ctm.mapRect(&deviceRect, dstRectFull);

    const auto plan = planDrawRestriction(ctm, deviceRect, *damageRegion);
    switch (plan.mode) {
    case DrawRestrictionPlan::Mode::SkipDraw:
        return;
    case DrawRestrictionPlan::Mode::PerRect:
        forEachDamagedSubRect(deviceRect, dstRectFull, srcRectFull, plan.inverse, *damageRegion, draw);
        return;
    case DrawRestrictionPlan::Mode::ClipFallback: {
        SkAutoCanvasRestore autoRestore(&canvas, true);
        clipToDamageInDeviceSpace(canvas, *damageRegion);
        draw(srcRectFull, dstRectFull);
        return;
    }
    }
}

inline void drawRectRestrictedToDamage(SkCanvas& canvas, const DamageRegion* damageRegion, const SkRect& localRect, const SkPaint& paint)
{
    drawRestrictedToDamage(canvas, damageRegion, localRect, localRect, [&](const SkRect&, const SkRect& dstSubRect) {
        canvas.drawRect(dstSubRect, paint);
    });
}

inline void drawImageRectRestrictedToDamage(SkCanvas& canvas, const SkImage* image, const SkRect& srcRectFull, const SkRect& dstRectFull, const SkSamplingOptions& sampling, const SkPaint* paint, const DamageRegion* damageRegion)
{
    drawRestrictedToDamage(canvas, damageRegion, srcRectFull, dstRectFull, [&](const SkRect& srcSubRect, const SkRect& dstSubRect) {
        canvas.drawImageRect(image, srcSubRect, dstSubRect, sampling, paint, SkCanvas::kFast_SrcRectConstraint);
    });
}

} // namespace WebCore
