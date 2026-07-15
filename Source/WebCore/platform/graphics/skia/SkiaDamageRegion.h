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
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkMatrix.h>
#include <skia/core/SkRect.h>
#include <skia/core/SkRegion.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Vector.h>

namespace WebCore {

// The damage of a frame in device space, kept both as rects, to split draws by, and as a region, to cull
// and clip with - built once per frame.
class SkiaDamageRegion {
public:
    static SkiaDamageRegion create(const Damage& damage)
    {
        const auto damageRects = damage.rectsForPainting().map([](const IntRect& rect) -> SkIRect {
            return rect;
        });

        SkiaDamageRegion damageRegion;
        damageRegion.m_region.setRects(damageRects.span().data(), damageRects.size());
        damageRegion.m_rects = damageRects.map([](const SkIRect& rect) -> SkRect {
            return SkRect::Make(rect);
        });

        return damageRegion;
    }

    bool isEmpty() const { return m_rects.isEmpty(); }

    // roundOut() applies floor to left/top, and ceil to right/bottom. Make sure a deviceRect touching the
    // region along an edge triggers an intersection - this is conservative, but for sure not wrong.
    bool intersects(const SkRect& deviceRect) const { return m_region.intersects(deviceRect.roundOut()); }

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
            SkRect damaged = rect;
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

    void clipCanvasInDeviceSpace(SkCanvas& canvas) const
    {
        ASSERT(!isEmpty());
        canvas.clipRegion(m_region, SkClipOp::kIntersect);
    }

private:
    SkiaDamageRegion() = default;

    Vector<SkRect> m_rects;
    SkRegion m_region;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
