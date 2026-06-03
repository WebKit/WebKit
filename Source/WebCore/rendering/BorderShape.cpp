/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "BorderShape.h"

#include "BorderData.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "LayoutRoundedRect.h"
#include "Path.h"
#include "RenderStyle+GettersInlines.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace WebCore {

static constexpr float kRoundExponent = 2.0f;
static constexpr float kBevelExponent = 1.0f;
static constexpr float kStraightThreshold = 16.0f;   // exponent >= this => straight
static constexpr float kNotchThreshold = 1.0f / kStraightThreshold; // exponent <= this => notch

static float extractCornerExponent(const Style::CornerShapeValue& shape)
{
    // CornerShapeValue stores the spec `superellipse parameter` (log2 of the
    // curve exponent). Convert it to a curve exponent here for the path builder.
    auto e = shape.exponent();
    if (std::isnan(e))
        return kRoundExponent;
    auto f = static_cast<float>(e);
    if (!std::isfinite(f))
        return std::numeric_limits<float>::infinity();
    if (f <= 0.0f)
        return 0.0f;
    return f;
}

static std::array<float, 4> extractCornerExponents(const RenderStyle& style)
{
    const auto& shapes = style.border().cornerShapes;
    return {
        extractCornerExponent(shapes.topLeft()),
        extractCornerExponent(shapes.topRight()),
        extractCornerExponent(shapes.bottomRight()),
        extractCornerExponent(shapes.bottomLeft()),
    };
}

static void zeroRadiiForOpenEdges(LayoutRoundedRectRadii& radii, RectEdges<bool> closedEdges)
{
    if (!closedEdges.top()) {
        radii.setTopLeft({ });
        radii.setTopRight({ });
    }
    if (!closedEdges.right()) {
        radii.setTopRight({ });
        radii.setBottomRight({ });
    }
    if (!closedEdges.bottom()) {
        radii.setBottomRight({ });
        radii.setBottomLeft({ });
    }
    if (!closedEdges.left()) {
        radii.setBottomLeft({ });
        radii.setTopLeft({ });
    }
}

static RectEdges<LayoutUnit> applyClosedEdges(const RectEdges<LayoutUnit>& widths, RectEdges<bool> closedEdges)
{
    return {
        LayoutUnit(closedEdges.top() ? widths.top() : 0_lu),
        LayoutUnit(closedEdges.right() ? widths.right() : 0_lu),
        LayoutUnit(closedEdges.bottom() ? widths.bottom() : 0_lu),
        LayoutUnit(closedEdges.left() ? widths.left() : 0_lu),
    };
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, RectEdges<bool> closedEdges)
{
    auto borderWidths = RectEdges<LayoutUnit>::map(style.usedBorderWidths(), [&](auto width) {
        return Style::evaluate<LayoutUnit>(width, Style::ZoomNeeded { });
    });
    return shapeForBorderRect(style, borderRect, borderWidths, closedEdges);
}

BorderShape BorderShape::shapeForBorderRect(const RenderStyle& style, const LayoutRect& borderRect, const RectEdges<LayoutUnit>& overrideBorderWidths, RectEdges<bool> closedEdges)
{
    auto usedBorderWidths = applyClosedEdges(overrideBorderWidths, closedEdges);
    auto exponents = extractCornerExponents(style);

    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());
        radii.scale(calcBorderRadiiConstraintScaleFor(borderRect, radii));
        zeroRadiiForOpenEdges(radii, closedEdges);

        if (!radii.areRenderableInRect(borderRect))
            radii.makeRenderableInRect(borderRect);

        return BorderShape { borderRect, usedBorderWidths, radii, exponents };
    }

    return BorderShape { borderRect, usedBorderWidths };
}

BorderShape BorderShape::shapeForOffsetRect(const RenderStyle& style, const LayoutRect& borderRect, const LayoutRect& offsetRect, const RectEdges<LayoutUnit>& edgeWidths, RectEdges<bool> closedEdges)
{
    auto usedEdgeWidths = applyClosedEdges(edgeWidths, closedEdges);
    auto exponents = extractCornerExponents(style);

    if (style.border().hasBorderRadius()) {
        auto radii = Style::evaluate<LayoutRoundedRectRadii>(style.borderRadii(), borderRect.size(), style.usedZoomForLength());

        auto leftDelta = borderRect.x() - offsetRect.x();
        auto topDelta = borderRect.y() - offsetRect.y();
        auto rightDelta = offsetRect.maxX() - borderRect.maxX();
        auto bottomDelta = offsetRect.maxY() - borderRect.maxY();

        radii.expand(topDelta, bottomDelta, leftDelta, rightDelta);
        zeroRadiiForOpenEdges(radii, closedEdges);

        if (!radii.areRenderableInRect(offsetRect))
            radii.makeRenderableInRect(offsetRect);

        return BorderShape { offsetRect, usedEdgeWidths, radii, exponents };
    }

    return BorderShape { offsetRect, usedEdgeWidths };
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths)
    : m_borderRect(borderRect)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
{
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const LayoutRoundedRectRadii& radii)
    : m_borderRect(borderRect, radii)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
{
    // The caller should have adjusted the radii already.
    ASSERT(m_borderRect.isRenderable());
}

BorderShape::BorderShape(const LayoutRect& borderRect, const RectEdges<LayoutUnit>& borderWidths, const LayoutRoundedRectRadii& radii, const std::array<float, 4>& cornerExponents)
    : m_borderRect(borderRect, radii)
    , m_innerEdgeRect(computeInnerEdgeRoundedRect(m_borderRect, borderWidths))
    , m_borderWidths(borderWidths)
    , m_cornerExponents(cornerExponents)
{
    ASSERT(m_borderRect.isRenderable());
}

BorderShape BorderShape::shapeWithBorderWidths(const RectEdges<LayoutUnit>& borderWidths) const
{
    return BorderShape(m_borderRect.rect(), borderWidths, m_borderRect.radii(), m_cornerExponents);
}

bool BorderShape::hasNonRoundCornerShapes() const
{
    for (auto e : m_cornerExponents) {
        if (e != kRoundExponent)
            return true;
    }
    return false;
}

LayoutRoundedRect BorderShape::deprecatedRoundedRect() const
{
    return m_borderRect;
}

LayoutRoundedRect BorderShape::deprecatedInnerRoundedRect() const
{
    return m_innerEdgeRect;
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedRoundedRect(float deviceScaleFactor) const
{
    return m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRoundedRect BorderShape::deprecatedPixelSnappedInnerRoundedRect(float deviceScaleFactor) const
{
    return m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
}

FloatRect BorderShape::snappedOuterRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(m_borderRect.rect(), deviceScaleFactor);
}

FloatRect BorderShape::snappedInnerRect(float deviceScaleFactor) const
{
    return snapRectToDevicePixels(innerEdgeRect(), deviceScaleFactor);
}

bool BorderShape::innerShapeContains(const LayoutRect& rect) const
{
    return m_innerEdgeRect.contains(rect);
}

bool BorderShape::outerShapeContains(const LayoutRect& rect) const
{
    return m_borderRect.contains(rect);
}

bool BorderShape::allCornersClippedOut(const LayoutRect& rect) const
{
    if (!isRounded())
        return true;

    auto borderRect = m_borderRect.rect();
    if (rect.contains(borderRect))
        return false;

    auto radii = m_borderRect.radii();

    LayoutRect topLeftRect(borderRect.location(), radii.topLeft());
    if (rect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(borderRect.location(), radii.topRight());
    topRightRect.setX(borderRect.maxX() - topRightRect.width());
    if (rect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(borderRect.location(), radii.bottomLeft());
    bottomLeftRect.setY(borderRect.maxY() - bottomLeftRect.height());
    if (rect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(borderRect.location(), radii.bottomRight());
    bottomRightRect.setX(borderRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(borderRect.maxY() - bottomRightRect.height());
    if (rect.intersects(bottomRightRect))
        return false;

    return true;
}

bool BorderShape::outerShapeIsRectangular() const
{
    return !m_borderRect.isRounded();
}

bool BorderShape::innerShapeIsRectangular() const
{
    return !m_innerEdgeRect.isRounded();
}

void BorderShape::move(LayoutSize offset)
{
    m_borderRect.move(offset);
    m_innerEdgeRect.move(offset);
}

void BorderShape::inflate(LayoutUnit amount)
{
    m_borderRect.inflateWithRadii(amount);
    m_innerEdgeRect = computeInnerEdgeRoundedRect(m_borderRect, m_borderWidths);
}

static void addRoundedRectToPath(const FloatRoundedRect& roundedRect, Path& path)
{
    if (roundedRect.isRounded())
        path.addRoundedRect(roundedRect);
    else
        path.addRect(roundedRect.rect());
}

// ---------------------------------------------------------------------------
// Contoured-rect path construction (corner-shape rendering).
//
// A "contoured rect" is a FloatRoundedRect plus a per-corner superellipse
// exponent. Exponent semantics (consistent with WebKit's stored value):
//   * exponent == 2.0       => round (default; uses standard ellipse arc)
//   * exponent == 1.0       => bevel (straight diagonal chamfer)
//   * 0 < exponent < 1      => concave (e.g. 0.5 == scoop)
//   * exponent == 0         => notch (inverted right-angle)
//   * exponent >= kStraightThreshold => straight (corner stays rectangular)
//
// For convex shapes other than round/bevel/straight we approximate the
// superellipse with two cubic Béziers meeting at the 45° point. This mirrors
// Chromium's path_builder.cc AddCurvedCorner() implementation.
// ---------------------------------------------------------------------------

namespace {

struct CornerVertices {
    FloatPoint start;   // point on the previous straight edge where the curve begins
    FloatPoint outer;   // the actual rectangle corner (would-be sharp tip)
    FloatPoint end;     // point on the next straight edge where the curve ends
    FloatPoint center;  // inner intersection of the two radii (opposite of outer)
};

static float clampExponent(float exponent)
{
    if (std::isnan(exponent) || exponent < 0.0f)
        return 0.0f;
    if (exponent > kStraightThreshold)
        return kStraightThreshold;
    return exponent;
}

static float halfCornerForExponent(float exponent)
{
    // The (x,y) coordinate on the unit superellipse at t = 0.5 (45 degrees).
    return std::pow(0.5f, 1.0f / std::max(exponent, 1e-3f));
}

// Returns three normalised vectors describing a cubic-Bézier approximation
// of a half quarter of a superellipse with the supplied (convex) exponent,
// going from (0,1) clockwise to (halfCorner, halfCorner). Ported from
// blink::ApproximateSuperellipseHalfCornerAsBezierCurve in path_builder.cc.
static std::array<FloatPoint, 3> approximateConvexHalfCorner(float exponent)
{
    static constexpr std::array<double, 7> p = {
        1.2430920942724248, 2.010479023614843, 0.32922901179443753,
        0.2823023142212073, 1.3473704261055421, 2.9149468637949814,
        0.9106507102917086
    };
    const double s = std::log2(std::max(exponent, 1.0f));
    const double slope = p[0] + (p[6] - p[0]) * 0.5 * (1.0 + std::tanh(p[5] * (s - p[1])));
    const double base = 1.0 / (1.0 + std::exp(-slope * (0.0 - p[1])));
    const double logistic = 1.0 / (1.0 + std::exp(-slope * (s - p[1])));
    const double a = (logistic - base) / (1.0 - base);
    const double b = p[2] * std::exp(-p[3] * std::pow(s, p[4]));
    const float halfCorner = halfCornerForExponent(exponent);
    return {
        FloatPoint(static_cast<float>(a), 1.0f),
        FloatPoint(halfCorner - static_cast<float>(b), halfCorner + static_cast<float>(b)),
        FloatPoint(halfCorner, halfCorner)
    };
}

static FloatPoint mapNormalisedPointToCorner(const CornerVertices& v, FloatPoint n)
{
    // Local basis: x along (outer - start), y along (start - center).
    auto v1 = FloatPoint(v.outer.x() - v.start.x(), v.outer.y() - v.start.y());
    auto v4 = FloatPoint(v.start.x() - v.center.x(), v.start.y() - v.center.y());
    return FloatPoint(
        v.center.x() + v1.x() * n.x() + v4.x() * n.y(),
        v.center.y() + v1.y() * n.x() + v4.y() * n.y());
}

// Builds the four CornerVertices for a given outer rect + radii.
// Order: { topLeft, topRight, bottomRight, bottomLeft }, each laid out so
// `start` is on the edge entering the corner clockwise and `end` is on the
// edge leaving it. This matches the {TL, TR, BR, BL} traversal order used by
// the rest of WebKit's painters.
static std::array<CornerVertices, 4> computeCornerVertices(const FloatRect& rect, const CornerRadii& radii)
{
    auto tl = radii.topLeft();
    auto tr = radii.topRight();
    auto br = radii.bottomRight();
    auto bl = radii.bottomLeft();

    auto left = rect.x();
    auto right = rect.maxX();
    auto top = rect.y();
    auto bottom = rect.maxY();

    return {{
        // top-left
        {
            { left, top + tl.height() },
            { left, top },
            { left + tl.width(), top },
            { left + tl.width(), top + tl.height() }
        },
        // top-right
        {
            { right - tr.width(), top },
            { right, top },
            { right, top + tr.height() },
            { right - tr.width(), top + tr.height() }
        },
        // bottom-right
        {
            { right, bottom - br.height() },
            { right, bottom },
            { right - br.width(), bottom },
            { right - br.width(), bottom - br.height() }
        },
        // bottom-left
        {
            { left + bl.width(), bottom },
            { left, bottom },
            { left, bottom - bl.height() },
            { left + bl.width(), bottom - bl.height() }
        }
    }};
}

static bool cornerIsEmpty(const CornerVertices& v)
{
    return v.start == v.end;
}

// Computes an inner corner aligned perpendicular to the outer corner's curve so
// that the resulting border thickness stays visually uniform along the curve.
// Ported from blink::ContouredRect::Corner::AlignedToOrigin in chromium's
// platform/geometry/contoured_rect.cc.
//
// For notch (concave 90°) and straight (rectangular) corners the chromium
// algorithm degenerates (the perpendicular offset reduces to zero in one or
// both axes), which leaves the inner corner anchored to the outer box's edges
// instead of the inner box's edges. For those two extremes we take a simpler
// route and translate the entire corner diagonally inward by `shift`, so the
// inner notch's adjacent legs land on the inner box's straight edges.
//
// When the outer corner is degenerate (zero radii in either direction), the
// algorithm cannot perpendicular-offset anything meaningful, so we return the
// already-computed inner corner verbatim. Same when border thickness is zero.
static CornerVertices alignedInnerCorner(const CornerVertices& outer, const CornerVertices& inner, float exponent, float thicknessStart, float thicknessEnd, FloatSize shift)
{
    auto vectorLength = [](FloatPoint a, FloatPoint b) {
        return std::hypot(a.x() - b.x(), a.y() - b.y());
    };
    // Outer is degenerate if either of its two side-vectors is zero-length.
    if (vectorLength(outer.start, outer.outer) <= 0.0f || vectorLength(outer.outer, outer.end) <= 0.0f)
        return inner;
    if (thicknessStart <= 0.0f && thicknessEnd <= 0.0f)
        return outer;

    // Notch (≈0) and straight (≈∞): translate diagonally so the inner notch's
    // legs sit on the inner box edges (matches the visual: each leg is the
    // outer leg perpendicular-offset inward by the matching border width).
    auto translate = [&](FloatPoint p) {
        return FloatPoint { p.x() + shift.width(), p.y() + shift.height() };
    };
    if (exponent <= kNotchThreshold || exponent >= kStraightThreshold) {
        return CornerVertices {
            translate(outer.start),
            translate(outer.outer),
            translate(outer.end),
            translate(outer.center),
        };
    }

    auto normalize = [](FloatSize v) {
        float len = std::hypot(v.width(), v.height());
        if (len <= 0.0f)
            return FloatSize { 0.0f, 0.0f };
        return FloatSize { v.width() / len, v.height() / len };
    };

    float c = std::clamp(exponent, 0.5f, kRoundExponent);
    float halfCorner = halfCornerForExponent(c);

    FloatSize hullPre { halfCorner * 2.0f - 0.5f, (1.0f - halfCorner) * 2.0f - 0.5f };
    FloatSize hull = normalize(hullPre);
    FloatSize adjusted { hull.width(), -hull.height() };

    FloatSize v1 { outer.outer.x() - outer.start.x(), outer.outer.y() - outer.start.y() };
    FloatSize v2 { outer.end.x() - outer.outer.x(), outer.end.y() - outer.outer.y() };
    FloatSize v3 { outer.center.x() - outer.end.x(), outer.center.y() - outer.end.y() };
    FloatSize v4 { outer.start.x() - outer.center.x(), outer.start.y() - outer.center.y() };

    auto nv1 = normalize(v1);
    auto nv2 = normalize(v2);
    auto nv3 = normalize(v3);
    auto nv4 = normalize(v4);

    auto scale = [](FloatSize v, float s) { return FloatSize { v.width() * s, v.height() * s }; };

    auto o1 = scale(nv1, thicknessStart * adjusted.height());
    auto o2 = scale(nv2, thicknessStart * adjusted.width());
    auto o3 = scale(nv3, thicknessEnd * adjusted.width());
    auto o4 = scale(nv4, thicknessEnd * adjusted.height());

    auto addPoint = [](FloatPoint p, FloatSize a, FloatSize b) {
        return FloatPoint { p.x() + a.width() + b.width(), p.y() + a.height() + b.height() };
    };

    return CornerVertices {
        addPoint(outer.start, o1, o2),
        addPoint(outer.outer, o2, o3),
        addPoint(outer.end, o3, o4),
        addPoint(outer.center, o4, o1),
    };
}

static void addCornerToPath(Path& path, const CornerVertices& vIn, float exponent)
{
    if (cornerIsEmpty(vIn)) {
        path.addLineTo(vIn.outer);
        return;
    }

    float c = clampExponent(exponent);
    CornerVertices v = vIn;

    // Concave (exponent < 1): mirror through the center diagonal and use the
    // equivalent convex exponent. Geometrically this swaps `outer` and `center`.
    if (c < kBevelExponent && c > 0.0f) {
        std::swap(v.outer, v.center);
        c = 1.0f / c;
    }

    // Connect any preceding straight edge to the curve start.
    path.addLineTo(v.start);

    if (c >= kStraightThreshold) {
        // Straight: keep the original rectangular corner.
        path.addLineTo(v.outer);
        path.addLineTo(v.end);
        return;
    }

    if (c <= 0.0f) {
        // Notch: inverted right angle reaching the center.
        path.addLineTo(v.center);
        path.addLineTo(v.end);
        return;
    }

    if (c == kBevelExponent) {
        // Bevel: straight diagonal across the corner.
        path.addLineTo(v.end);
        return;
    }

    if (c == kRoundExponent) {
        // Round: standard cubic-Bézier approximation of a quarter ellipse.
        // Magic constant k = (4/3) * (sqrt(2) - 1).
        constexpr float k = 0.5522847498307933984022516f;
        auto v1x = v.outer.x() - v.start.x();
        auto v1y = v.outer.y() - v.start.y();
        auto v2x = v.end.x() - v.outer.x();
        auto v2y = v.end.y() - v.outer.y();
        FloatPoint c1 { v.start.x() + k * v1x, v.start.y() + k * v1y };
        FloatPoint c2 { v.end.x() - k * v2x, v.end.y() - k * v2y };
        path.addBezierCurveTo(c1, c2, v.end);
        return;
    }

    // General convex superellipse (1 < c < kStraightThreshold): approximate
    // with two cubic Béziers meeting at the 45° point (t = 0.5).
    auto controls = approximateConvexHalfCorner(c);
    auto cp1a = mapNormalisedPointToCorner(v, controls[0]);
    auto cp2a = mapNormalisedPointToCorner(v, controls[1]);
    auto mid  = mapNormalisedPointToCorner(v, controls[2]);
    // Second half is the same control points transposed across y=x.
    FloatPoint t1 { controls[1].y(), controls[1].x() };
    FloatPoint t0 { controls[0].y(), controls[0].x() };
    auto cp1b = mapNormalisedPointToCorner(v, t1);
    auto cp2b = mapNormalisedPointToCorner(v, t0);

    path.addBezierCurveTo(cp1a, cp2a, mid);
    path.addBezierCurveTo(cp1b, cp2b, v.end);
}

} // namespace

static void addContouredCornersToPath(const std::array<CornerVertices, 4>& corners, const std::array<float, 4>& exponents, Path& path)
{
    // Trace the outline clockwise starting just after the TL corner on the top edge.
    path.moveTo(corners[0].end);
    addCornerToPath(path, corners[1], exponents[1]); // top-right
    addCornerToPath(path, corners[2], exponents[2]); // bottom-right
    addCornerToPath(path, corners[3], exponents[3]); // bottom-left
    addCornerToPath(path, corners[0], exponents[0]); // top-left
    path.closeSubpath();
}

static void addContouredRectToPath(const FloatRoundedRect& roundedRect, const std::array<float, 4>& exponents, Path& path)
{
    if (!roundedRect.isRounded()) {
        path.addRect(roundedRect.rect());
        return;
    }
    auto corners = computeCornerVertices(roundedRect.rect(), roundedRect.radii());
    addContouredCornersToPath(corners, exponents, path);
}

void BorderShape::addOuterShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded())
        addContouredRectToPath(pixelSnappedRect, m_cornerExponents, path);
    else
        addRoundedRectToPath(pixelSnappedRect, path);
}

void BorderShape::addInnerShapeToPath(Path& path, float deviceScaleFactor) const
{
    auto pixelSnappedInner = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedInner.isRenderable());

    // If any corner shape is non-round, the standard concentric inner ellipse
    // doesn't yield uniform border thickness along chamfered/scooped corners.
    // Compute each inner corner perpendicular-aligned to the outer corner, like
    // chromium's ContouredRect::Corner::AlignedToOrigin.
    auto pixelSnappedOuter = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShapes() && pixelSnappedOuter.isRounded()) {
        auto outerCorners = computeCornerVertices(pixelSnappedOuter.rect(), pixelSnappedOuter.radii());
        // The inner rounded rect may have all radii shrunk to zero already (when
        // border-width >= radius); we still need a valid inner CornerVertices
        // for the fallback path in alignedInnerCorner() so build it from the
        // inner rect with its own (possibly-zero) radii.
        auto innerCornersBase = computeCornerVertices(pixelSnappedInner.rect(), pixelSnappedInner.radii());
        auto outerSnappedRect = pixelSnappedOuter.rect();
        auto innerSnappedRect = pixelSnappedInner.rect();
        // Per-side thicknesses in the snapped coordinate space.
        float tTop = innerSnappedRect.y() - outerSnappedRect.y();
        float tRight = outerSnappedRect.maxX() - innerSnappedRect.maxX();
        float tBottom = outerSnappedRect.maxY() - innerSnappedRect.maxY();
        float tLeft = innerSnappedRect.x() - outerSnappedRect.x();
        // Diagonal-inward shift per corner, used by alignedInnerCorner() for
        // notch / straight shapes (where perpendicular-to-curve is degenerate).
        std::array<FloatSize, 4> shift = {{
            { tLeft, tTop },     // TL: shift right+down
            { -tRight, tTop },   // TR: shift left+down
            { -tRight, -tBottom }, // BR: shift left+up
            { tLeft, -tBottom }, // BL: shift right+up
        }};
        std::array<CornerVertices, 4> aligned = {
            alignedInnerCorner(outerCorners[0], innerCornersBase[0], m_cornerExponents[0], tLeft, tTop, shift[0]),    // TL
            alignedInnerCorner(outerCorners[1], innerCornersBase[1], m_cornerExponents[1], tTop, tRight, shift[1]),   // TR
            alignedInnerCorner(outerCorners[2], innerCornersBase[2], m_cornerExponents[2], tRight, tBottom, shift[2]), // BR
            alignedInnerCorner(outerCorners[3], innerCornersBase[3], m_cornerExponents[3], tBottom, tLeft, shift[3]),  // BL
        };
        addContouredCornersToPath(aligned, m_cornerExponents, path);
        return;
    }

    addRoundedRectToPath(pixelSnappedInner, path);
}

Path BorderShape::pathForOuterShape(float deviceScaleFactor) const
{
    Path path;
    addOuterShapeToPath(path, deviceScaleFactor);
    return path;
}

Path BorderShape::pathForInnerShape(float deviceScaleFactor) const
{
    Path path;
    addInnerShapeToPath(path, deviceScaleFactor);
    return path;
}

Path BorderShape::pathForBorderArea(float deviceScaleFactor) const
{
    Path path;
    addOuterShapeToPath(path, deviceScaleFactor);
    addInnerShapeToPath(path, deviceScaleFactor);
    return path;
}

void BorderShape::clipToOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded()) {
        Path path;
        addContouredRectToPath(pixelSnappedRect, m_cornerExponents, path);
        context.clipPath(path);
        return;
    }
    if (pixelSnappedRect.isRounded())
        context.clipRoundedRect(pixelSnappedRect);
    else
        context.clip(pixelSnappedRect.rect());
}

void BorderShape::clipToInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded()) {
        Path path;
        addInnerShapeToPath(path, deviceScaleFactor);
        context.clipPath(path);
        return;
    }
    if (pixelSnappedRect.isRounded())
        context.clipRoundedRect(pixelSnappedRect);
    else
        context.clip(pixelSnappedRect.rect());
}

void BorderShape::clipOutOuterShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded()) {
        Path path;
        addContouredRectToPath(pixelSnappedRect, m_cornerExponents, path);
        context.clipOut(path);
        return;
    }
    if (pixelSnappedRect.isRounded())
        context.clipOutRoundedRect(pixelSnappedRect);
    else
        context.clipOut(pixelSnappedRect.rect());
}

void BorderShape::clipOutInnerShape(GraphicsContext& context, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (pixelSnappedRect.isEmpty())
        return;

    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded()) {
        Path path;
        addInnerShapeToPath(path, deviceScaleFactor);
        context.clipOut(path);
        return;
    }
    if (pixelSnappedRect.isRounded())
        context.clipOutRoundedRect(pixelSnappedRect);
    else
        context.clipOut(pixelSnappedRect.rect());
}

void BorderShape::fillOuterShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded()) {
        Path path;
        addContouredRectToPath(pixelSnappedRect, m_cornerExponents, path);
        context.setFillColor(color);
        context.fillPath(path);
        return;
    }
    if (pixelSnappedRect.isRounded())
        context.fillRoundedRect(pixelSnappedRect, color);
    else
        context.fillRect(pixelSnappedRect.rect(), color);
}

void BorderShape::fillInnerShape(GraphicsContext& context, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(pixelSnappedRect.isRenderable());
    if (hasNonRoundCornerShapes() && pixelSnappedRect.isRounded()) {
        Path path;
        addInnerShapeToPath(path, deviceScaleFactor);
        context.setFillColor(color);
        context.fillPath(path);
        return;
    }
    if (pixelSnappedRect.isRounded())
        context.fillRoundedRect(pixelSnappedRect, color);
    else
        context.fillRect(pixelSnappedRect.rect(), color);
}

void BorderShape::fillRectWithInnerHoleShape(GraphicsContext& context, const LayoutRect& outerRect, const Color& color, float deviceScaleFactor) const
{
    auto pixelSnappedOuterRect = snapRectToDevicePixels(outerRect, deviceScaleFactor);
    auto innerSnappedRoundedRect = m_innerEdgeRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
    ASSERT(innerSnappedRoundedRect.isRenderable());
    if (hasNonRoundCornerShapes() && innerSnappedRoundedRect.isRounded()) {
        // Fill outerRect with the colour, then punch out the inner contoured shape.
        Path path;
        path.addRect(pixelSnappedOuterRect);
        addInnerShapeToPath(path, deviceScaleFactor);
        context.setFillRule(WindRule::EvenOdd);
        context.setFillColor(color);
        context.fillPath(path);
        context.setFillRule(WindRule::NonZero);
        return;
    }
    context.fillRectWithRoundedHole(pixelSnappedOuterRect, innerSnappedRoundedRect, color);
}

LayoutRoundedRect BorderShape::computeInnerEdgeRoundedRect(const LayoutRoundedRect& borderRoundedRect, const RectEdges<LayoutUnit>& borderWidths)
{
    auto borderRect = borderRoundedRect.rect();
    auto width = std::max(0_lu, borderRect.width() - borderWidths.left() - borderWidths.right());
    auto height = std::max(0_lu, borderRect.height() - borderWidths.top() - borderWidths.bottom());
    auto innerRect = LayoutRect {
        borderRect.x() + borderWidths.left(),
        borderRect.y() + borderWidths.top(),
        width,
        height
    };

    auto innerEdgeRect = LayoutRoundedRect { innerRect };
    if (borderRoundedRect.isRounded()) {
        auto innerRadii = borderRoundedRect.radii();
        innerRadii.shrink(borderWidths.top(), borderWidths.bottom(), borderWidths.left(), borderWidths.right());
        innerEdgeRect.setRadii(innerRadii);

        if (!innerEdgeRect.isRenderable())
            innerEdgeRect.adjustRadii();
    }

    return innerEdgeRect;
}

} // namespace WebCore
