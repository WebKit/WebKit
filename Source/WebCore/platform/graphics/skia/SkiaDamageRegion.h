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

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)

#include "Damage.h"
#include "IntRect.h"
#include <optional>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkMatrix.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkRegion.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Vector.h>

namespace WebCore {

// The damage of a frame in device space, kept both as rects, to split draws by, and as a region, to cull
// and clip with - built once per frame.
class SkiaDamageRegion {
public:
    static std::optional<SkiaDamageRegion> create(const Damage& repaintRegion, const IntSize& surfaceSize)
    {
        if (repaintRegion.isEmpty())
            return SkiaDamageRegion { };

        auto damageRects = repaintRegion.rectsForPainting().map([](const IntRect& rect) -> SkIRect {
            return rect;
        });

        SkiaDamageRegion damageRegion;
        damageRegion.m_region.setRects(damageRects.span().data(), damageRects.size());
        damageRegion.m_rects = WTF::move(damageRects);

        // Ask the region, not its bounds, because two rects in opposite corners of the surface span a
        // bounding box that covers it while the rects themselves cover almost none of it.
        if (damageRegion.m_region.contains(SkIRect::MakeWH(surfaceSize.width(), surfaceSize.height())))
            return std::nullopt;

        return damageRegion;
    }

    SkiaDamageRegion(const SkiaDamageRegion&) = default;
    SkiaDamageRegion& operator=(const SkiaDamageRegion&) = default;

    // SkRegion declares a copy constructor and a destructor, so it has no move constructor and would deep
    // copy its run array on every move. swap() exchanges pointers instead.
    SkiaDamageRegion(SkiaDamageRegion&& other)
    {
        *this = WTF::move(other);
    }

    SkiaDamageRegion& operator=(SkiaDamageRegion&& other)
    {
        m_rects = WTF::move(other.m_rects);
        m_region.swap(other.m_region);
        return *this;
    }

    bool isEmpty() const { return m_rects.isEmpty(); }

    // roundOut() applies floor to left/top, and ceil to right/bottom. Make sure a deviceRect touching the
    // region along an edge triggers an intersection - this is conservative, but for sure not wrong.
    bool intersects(const SkRect& deviceRect) const { return m_region.intersects(deviceRect.roundOut()); }

    // roundOut() grows the rect, so coverage is under-reported, never over-reported.
    bool covers(const SkRect& deviceRect) const { return m_region.contains(deviceRect.roundOut()); }

    // How to limit one draw to the damage. A draw is split into as many rects as touch it, with no limit,
    // because the parts share a paint and Skia batches them into one op. Clipping is the slow path instead:
    // a clip of more than one rect cannot be a scissor, so it costs a stencil buffer or a mask texture.
    enum class DrawDamageStrategy : uint8_t {
        Skip, // Nothing touches the damage.
        SplitByRect, // One draw per touched rect, narrowed in local coords, which narrows the image source too.
        ClipToDamage // The CTM is rotated or skewed, so one draw under a device-space clip.
    };

    // On SplitByRect, sets inverse to the matrix that maps device rects back to local coordinates. inverse
    // is left untouched for the other strategies.
    DrawDamageStrategy planDraw(const SkMatrix& ctm, const SkRect& deviceRect, SkMatrix& inverse) const
    {
        if (!intersects(deviceRect))
            return DrawDamageStrategy::Skip;

        if (!ctm.rectStaysRect() || !ctm.invert(&inverse))
            return DrawDamageStrategy::ClipToDamage;

        return DrawDamageStrategy::SplitByRect;
    }

    // Calls emit(srcSubRect, dstSubRect) once per damage rect that touches deviceRect. deviceRect is
    // dstRectFull mapped by the ctm, and inverseCtm maps device rects back to local coordinates.
    template<typename EmitFunction>
    void forEachDamagedSubRect(const SkRect& deviceRect, const SkRect& dstRectFull, const SkRect& srcRectFull, const SkMatrix& inverseCtm, NOESCAPE const EmitFunction& emit) const
    {
        const auto dstToSrc = SkMatrix::RectToRect(dstRectFull, srcRectFull);

        for (const auto& rect : m_rects) {
            auto damaged = SkRect::Make(rect);
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

    template<typename DrawFunction>
    void restrictDraw(SkCanvas& canvas, const SkRect& srcRectFull, const SkRect& dstRectFull, NOESCAPE const DrawFunction& draw) const
    {
        ASSERT(!isEmpty());

        const auto ctm = canvas.getLocalToDeviceAs3x3();
        SkRect deviceRect;
        ctm.mapRect(&deviceRect, dstRectFull);

        if (covers(deviceRect)) {
            draw(srcRectFull, dstRectFull);
            return;
        }

        SkMatrix inverse;
        switch (planDraw(ctm, deviceRect, inverse)) {
        case DrawDamageStrategy::Skip:
            return;
        case DrawDamageStrategy::SplitByRect:
            forEachDamagedSubRect(deviceRect, dstRectFull, srcRectFull, inverse, draw);
            return;
        case DrawDamageStrategy::ClipToDamage: {
            SkAutoCanvasRestore autoRestore(&canvas, true);
            clipCanvasInDeviceSpace(canvas);
            draw(srcRectFull, dstRectFull);
            return;
        }
        }
    }

    void fillCanvasInDeviceSpace(SkCanvas& canvas, const SkPaint& paint) const
    {
        // Unlike clipRegion(), drawRegion() maps the region by the CTM, so the canvas must carry none
        // for the region to land in device space.
        ASSERT(!isEmpty());
        ASSERT(canvas.getLocalToDeviceAs3x3().isIdentity());
        canvas.drawRegion(m_region, paint);
    }

    void clipCanvasInDeviceSpace(SkCanvas& canvas) const
    {
        ASSERT(!isEmpty());
        canvas.clipRegion(m_region, SkClipOp::kIntersect);
    }

private:
    SkiaDamageRegion() = default;

    Vector<SkIRect> m_rects;
    SkRegion m_region;
};

inline void drawRectRestricted(SkCanvas& canvas, const SkiaDamageRegion* damageRegion, const SkRect& rect, const SkPaint& paint)
{
    if (!damageRegion) {
        canvas.drawRect(rect, paint);
        return;
    }

    damageRegion->restrictDraw(canvas, rect, rect, [&](const SkRect&, const SkRect& dstSubRect) {
        canvas.drawRect(dstSubRect, paint);
    });
}

inline void drawImageRectRestricted(SkCanvas& canvas, const SkiaDamageRegion* damageRegion, const SkImage* image, const SkRect& srcRectFull, const SkRect& dstRectFull, const SkSamplingOptions& sampling, const SkPaint* paint)
{
    if (!damageRegion) {
        canvas.drawImageRect(image, srcRectFull, dstRectFull, sampling, paint, SkCanvas::kFast_SrcRectConstraint);
        return;
    }

    damageRegion->restrictDraw(canvas, srcRectFull, dstRectFull, [&](const SkRect& srcSubRect, const SkRect& dstSubRect) {
        canvas.drawImageRect(image, srcSubRect, dstSubRect, sampling, paint, SkCanvas::kFast_SrcRectConstraint);
    });
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
