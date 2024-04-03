/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/ganesh/geometry/GrQuad.h"

#include "include/core/SkMatrix.h"
#include "include/private/gpu/ganesh/GrTypesPriv.h"

using V4f = skvx::Vec<4, float>;

static bool aa_affects_rect(GrQuadAAFlags edgeFlags, float ql, float qt, float qr, float qb) {
    // Edge coordinates for non-AA edges do not need to be integers; any AA-enabled edge that is
    // at an integer coordinate could be drawn non-AA and be visually identical to non-AA.
    return ((edgeFlags & GrQuadAAFlags::kLeft)   && !SkScalarIsInt(ql)) ||
           ((edgeFlags & GrQuadAAFlags::kRight)  && !SkScalarIsInt(qr)) ||
           ((edgeFlags & GrQuadAAFlags::kTop)    && !SkScalarIsInt(qt)) ||
           ((edgeFlags & GrQuadAAFlags::kBottom) && !SkScalarIsInt(qb));
}

static void map_rect_translate_scale(const SkRect& rect, const SkMatrix& m,
                                     V4f* xs, V4f* ys) {
    SkMatrix::TypeMask tm = m.getType();
    SkASSERT(tm <= (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask));

    V4f r = V4f::Load(&rect);
    if (tm > SkMatrix::kIdentity_Mask) {
        const V4f t{m.getTranslateX(), m.getTranslateY(), m.getTranslateX(), m.getTranslateY()};
        if (tm <= SkMatrix::kTranslate_Mask) {
            r += t;
        } else {
            const V4f s{m.getScaleX(), m.getScaleY(), m.getScaleX(), m.getScaleY()};
            r = r * s + t;
        }
    }
    *xs = skvx::shuffle<0, 0, 2, 2>(r);
    *ys = skvx::shuffle<1, 3, 1, 3>(r);
}

static void map_quad_general(const V4f& qx, const V4f& qy, const SkMatrix& m,
                             V4f* xs, V4f* ys, V4f* ws) {
    *xs = m.getScaleX() * qx + (m.getSkewX() * qy + m.getTranslateX());
    *ys = m.getSkewY() * qx + (m.getScaleY() * qy + m.getTranslateY());
    if (m.hasPerspective()) {
        V4f w = m.getPerspX() * qx + (m.getPerspY() * qy + m.get(SkMatrix::kMPersp2));
        if (ws) {
            // Output the calculated w coordinates
            *ws = w;
        } else {
            // Apply perspective division immediately
            V4f iw = 1.f / w;
            *xs *= iw;
            *ys *= iw;
        }
    } else if (ws) {
        *ws = 1.f;
    }
}

static void map_rect_general(const SkRect& rect, const SkMatrix& matrix,
                             V4f* xs, V4f* ys, V4f* ws) {
    V4f rx{rect.fLeft, rect.fLeft, rect.fRight, rect.fRight};
    V4f ry{rect.fTop, rect.fBottom, rect.fTop, rect.fBottom};
    map_quad_general(rx, ry, matrix, xs, ys, ws);
}

// Rearranges (top-left, top-right, bottom-right, bottom-left) ordered skQuadPts into xs and ys
// ordered (top-left, bottom-left, top-right, bottom-right)
static void rearrange_sk_to_gr_points(const SkPoint skQuadPts[4], V4f* xs, V4f* ys) {
    *xs = V4f{skQuadPts[0].fX, skQuadPts[3].fX, skQuadPts[1].fX, skQuadPts[2].fX};
    *ys = V4f{skQuadPts[0].fY, skQuadPts[3].fY, skQuadPts[1].fY, skQuadPts[2].fY};
}

// If an SkRect is transformed by this matrix, what class of quad is required to represent it.
static GrQuad::Type quad_type_for_transformed_rect(const SkMatrix& matrix) {
    if (matrix.rectStaysRect()) {
        return GrQuad::Type::kAxisAligned;
    } else if (matrix.preservesRightAngles()) {
        return GrQuad::Type::kRectilinear;
    } else if (matrix.hasPerspective()) {
        return GrQuad::Type::kPerspective;
    } else {
        return GrQuad::Type::kGeneral;
    }
}

// Perform minimal analysis of 'pts' (which are suitable for MakeFromSkQuad), and determine a
// quad type that will be as minimally general as possible.
static GrQuad::Type quad_type_for_points(const SkPoint pts[4], const SkMatrix& matrix) {
    if (matrix.hasPerspective()) {
        return GrQuad::Type::kPerspective;
    }
    // If 'pts' was formed by SkRect::toQuad() and not transformed further, it is safe to use the
    // quad type derived from 'matrix'. Otherwise don't waste any more time and assume kStandard
    // (most general 2D quad).
    if ((pts[0].fX == pts[3].fX && pts[1].fX == pts[2].fX) &&
        (pts[0].fY == pts[1].fY && pts[2].fY == pts[3].fY)) {
        return quad_type_for_transformed_rect(matrix);
    } else {
        return GrQuad::Type::kGeneral;
    }
}

GrQuad GrQuad::MakeFromRect(const SkRect& rect, const SkMatrix& m) {
    V4f x, y, w;
    SkMatrix::TypeMask tm = m.getType();
    Type type;
    if (tm <= (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) {
        map_rect_translate_scale(rect, m, &x, &y);
        w = 1.f;
        type = Type::kAxisAligned;
    } else {
        map_rect_general(rect, m, &x, &y, &w);
        type = quad_type_for_transformed_rect(m);
    }
    return GrQuad(x, y, w, type);
}

GrQuad GrQuad::MakeFromSkQuad(const SkPoint pts[4], const SkMatrix& matrix) {
    V4f xs, ys;
    rearrange_sk_to_gr_points(pts, &xs, &ys);
    Type type = quad_type_for_points(pts, matrix);
    if (matrix.isIdentity()) {
        return GrQuad(xs, ys, 1.f, type);
    } else {
        V4f mx, my, mw;
        map_quad_general(xs, ys, matrix, &mx, &my, &mw);
        return GrQuad(mx, my, mw, type);
    }
}

bool GrQuad::aaHasEffectOnRect(GrQuadAAFlags edgeFlags) const {
    SkASSERT(this->quadType() == Type::kAxisAligned);
    // If rect, ws must all be 1s so no need to divide
    return aa_affects_rect(edgeFlags, fX[0], fY[0], fX[3], fY[3]);
}

bool GrQuad::asRect(SkRect* rect) const {
    if (this->quadType() != Type::kAxisAligned) {
        return false;
    }

    *rect = this->bounds();
    // v0 at the geometric top-left is unique amongst axis-aligned vertex orders
    // (90, 180, 270 rotations or axis flips all move v0).
    return fX[0] == rect->fLeft && fY[0] == rect->fTop;
}
