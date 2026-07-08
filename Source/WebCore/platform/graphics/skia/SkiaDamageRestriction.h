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
#include "GeometryUtilities.h"
#include "IntRect.h"

#include <optional>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkMatrix.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkRegion.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <span>
#include <wtf/Vector.h>

namespace WebCore {

// Above this many damage rects touching a draw, use one clipped draw instead of one draw per rect.
// Only rects that touch the draw count, not the whole frame.
static constexpr unsigned cMaxDamageRectsForPerRectDraw = 4;

inline SkRect mapDamageSourceSubRect(const SkRect& dstSubRect, const SkRect& dstRectFull, const SkRect& srcRectFull)
{
    return mapRect(FloatRect(dstSubRect), FloatRect(dstRectFull), FloatRect(srcRectFull));
}

// The frame's damage rects in surface space and the same rects as an SkRegion. The rects never
// overlap. Built once per frame.
struct DamageRegion {
    Vector<IntRect> rects;
    SkRegion region;
};

// How to paint this frame, decided from the render target's repaint region.
// FullRepaint - the region is unknown or covers the whole surface, so clear and repaint everything.
// SkipPaint - the buffer is already current, so there is nothing to do.
// RestrictToDamage - clear and composite only the damage region.
struct FrameRestrictionPlan {
    enum class Kind : uint8_t {
        FullRepaint,
        SkipPaint,
        RestrictToDamage
    };

    Kind kind { Kind::FullRepaint };
    std::optional<DamageRegion> damageRegion; // Set only for RestrictToDamage.
};

inline FrameRestrictionPlan planFrameRestriction(const std::optional<Damage>& repaintRegion, const IntSize& surfaceSize)
{
    FrameRestrictionPlan plan;
    if (!repaintRegion)
        return plan;

    if (repaintRegion->isEmpty()) {
        plan.kind = FrameRestrictionPlan::Kind::SkipPaint;
        return plan;
    }

    if (repaintRegion->bounds().contains(IntRect { { }, surfaceSize }))
        return plan;

    // Damage::rectsForPainting() returns rects that never overlap, which the restriction relies on.
    // Build the SkRegion here too so clip sites don't have to rebuild it from the rects.
    plan.kind = FrameRestrictionPlan::Kind::RestrictToDamage;
    DamageRegion damageRegion;
    damageRegion.rects = repaintRegion->rectsForPainting();
    Vector<SkIRect> skiaRects;
    skiaRects.reserveInitialCapacity(damageRegion.rects.size());
    for (const auto& rect : damageRegion.rects)
        skiaRects.append(rect);
    damageRegion.region.setRects(skiaRects.span().data(), skiaRects.size());
    plan.damageRegion = WTF::move(damageRegion);
    return plan;
}

// How to limit one draw (a layer's tiles, an image or a rect) to the damage.
// SkipDraw - nothing touches the damage.
// PerRect - one draw per touched rect, narrowed in local coords (this also narrows the image source).
//           Needs a rect-preserving CTM and few rects. Inverse and rects are set only then.
// ClipFallback - rotated or skewed CTM, or too many rects, so use one draw under a device-space clip.
struct DrawRestrictionPlan {
    enum class Kind : uint8_t {
        SkipDraw,
        PerRect,
        ClipFallback
    };

    Kind kind { Kind::SkipDraw };
    SkMatrix inverse;
    Vector<IntRect, cMaxDamageRectsForPerRectDraw> rects;
};

inline DrawRestrictionPlan planDrawRestriction(const SkMatrix& ctm, const SkRect& deviceRect, std::span<const IntRect> damageRects)
{
    DrawRestrictionPlan plan;
    bool tooManyRects = false;
    for (const auto& rect : damageRects) {
        if (!SkRect::Make(rect).intersects(deviceRect))
            continue;
        if (plan.rects.size() == cMaxDamageRectsForPerRectDraw) {
            tooManyRects = true;
            break;
        }
        plan.rects.append(rect);
    }

    if (plan.rects.isEmpty())
        return plan;

    if (tooManyRects || !ctm.rectStaysRect() || !ctm.invert(&plan.inverse)) {
        plan.rects.clear();
        plan.kind = DrawRestrictionPlan::Kind::ClipFallback;
        return plan;
    }

    plan.kind = DrawRestrictionPlan::Kind::PerRect;
    return plan;
}

// Calls emit(srcSubRect, dstSubRect) once per damage rect that touches deviceRect, narrowing the source
// to the damaged sub-rect. deviceRect is dstRectFull under the rectilinear ctm, and inverseCtm maps
// device rects back. A tile fully covered by one rect emits a single full entry. The rects never overlap.
template<typename EmitFunction>
void forEachDamagedSubRect(const SkRect& deviceRect, const SkRect& dstRectFull, const SkRect& srcRectFull, const SkMatrix& inverseCtm, std::span<const IntRect> damageRects, NOESCAPE const EmitFunction& emit)
{
    for (const auto& rect : damageRects) {
        SkRect damaged = SkRect::Make(rect);
        if (!damaged.intersect(deviceRect))
            continue;
        if (damaged == deviceRect) {
            emit(srcRectFull, dstRectFull);
            return;
        }
        SkRect dstSubRect;
        inverseCtm.mapRect(&dstSubRect, damaged);
        emit(mapDamageSourceSubRect(dstSubRect, dstRectFull, srcRectFull), dstSubRect);
    }
}

inline void clipToDamageInDeviceSpace(SkCanvas& canvas, const DamageRegion& damageRegion)
{
    // An empty region would clip everything out, which no caller wants.
    ASSERT(!damageRegion.rects.isEmpty());

    // The region is in device space, so clip with it directly - no CTM inversion or path building.
    canvas.clipRegion(damageRegion.region, SkClipOp::kIntersect);
}

// Replays draw(srcSubRect, dstSubRect) limited to the damage region's rects. The rects never overlap,
// because overlapping rects would composite translucent content more than once.
template<typename DrawFunction>
void drawRestrictedToDamage(SkCanvas& canvas, const std::optional<DamageRegion>& damageRegion, const SkRect& srcRectFull, const SkRect& dstRectFull, NOESCAPE const DrawFunction& draw)
{
    if (!damageRegion) {
        draw(srcRectFull, dstRectFull);
        return;
    }

    ASSERT(!damageRegion->rects.isEmpty());

    const auto ctm = canvas.getLocalToDeviceAs3x3();
    SkRect deviceRect;
    ctm.mapRect(&deviceRect, dstRectFull);

    const auto plan = planDrawRestriction(ctm, deviceRect, damageRegion->rects.span());
    switch (plan.kind) {
    case DrawRestrictionPlan::Kind::SkipDraw:
        return;
    case DrawRestrictionPlan::Kind::PerRect:
        forEachDamagedSubRect(deviceRect, dstRectFull, srcRectFull, plan.inverse, plan.rects.span(), draw);
        return;
    case DrawRestrictionPlan::Kind::ClipFallback: {
        SkAutoCanvasRestore autoRestore(&canvas, true);
        clipToDamageInDeviceSpace(canvas, *damageRegion);
        draw(srcRectFull, dstRectFull);
        return;
    }
    }
}

inline void drawRectRestrictedToDamage(SkCanvas& canvas, const std::optional<DamageRegion>& damageRegion, const SkRect& localRect, const SkPaint& paint)
{
    drawRestrictedToDamage(canvas, damageRegion, localRect, localRect, [&](const SkRect&, const SkRect& dstSubRect) {
        canvas.drawRect(dstSubRect, paint);
    });
}

inline void drawImageRectRestrictedToDamage(SkCanvas& canvas, const SkImage* image, const SkRect& srcRectFull, const SkRect& dstRectFull, const SkSamplingOptions& sampling, const SkPaint* paint, const std::optional<DamageRegion>& damageRegion)
{
    drawRestrictedToDamage(canvas, damageRegion, srcRectFull, dstRectFull, [&](const SkRect& srcSubRect, const SkRect& dstSubRect) {
        canvas.drawImageRect(image, srcSubRect, dstSubRect, sampling, paint, SkCanvas::kFast_SrcRectConstraint);
    });
}

} // namespace WebCore
