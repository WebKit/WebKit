/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "MeshGradient.h"

#include "BezierUtilities.h"
#include "ColorConversion.h"
#include "ColorInterpolation.h"
#include "ColorNormalization.h"
#include "ColorTypes.h"
#include "ColorUtilities.h"
#include "NativeImage.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// MARK: - Color conversion

template<typename MethodColorSpace> static std::optional<size_t> hueComponentIndex()
{
    constexpr auto componentInfo = MethodColorSpace::ColorType::Model::componentInfo;
    for (size_t i = 0; i < 3; ++i) {
        if (componentInfo[i].type == ColorComponentType::Angle)
            return i;
    }
    return std::nullopt;
}

static std::optional<size_t> hueComponentIndex(const ColorInterpolationMethod& method)
{
    return WTF::switchOn(method.colorSpace, [&]<typename MethodColorSpace>(const MethodColorSpace&) {
        return hueComponentIndex<MethodColorSpace>();
    });
}

static std::optional<HueInterpolationMethod> hueInterpolationMethod(const ColorInterpolationMethod& method)
{
    return WTF::switchOn(method.colorSpace, [&]<typename MethodColorSpace>(const MethodColorSpace& methodColorSpace) -> std::optional<HueInterpolationMethod> {
        if constexpr (hasHueInterpolationMethod<MethodColorSpace>)
            return methodColorSpace.hueInterpolationMethod;
        else
            return std::nullopt;
    });
}

static ColorComponents<float, 4> toInterpolationComponents(const ColorInterpolationMethod& method, const Color& color)
{
    // FIXME: Interpolate missing components rather than treating them as zero.
    // CSS Color Module Level 4, Interpolating with Missing Components
    // https://drafts.csswg.org/css-color-4/#interpolation-missing

    auto components = WTF::switchOn(method.colorSpace, [&]<typename MethodColorSpace>(const MethodColorSpace&) {
        return asColorComponents(color.toColorTypeLossy<typename MethodColorSpace::ColorType>().resolved());
    });

    if (method.alphaPremultiplication == AlphaPremultiplication::Premultiplied) {
        auto hueIndex = hueComponentIndex(method);
        for (size_t i = 0; i < 3; ++i) {
            if (i != hueIndex)
                components[i] *= components[3];
        }
    }

    return components;
}

static ResolvedPremultiplied<SRGBA<float>> toPremultipliedSRGB(const ColorInterpolationMethod& method, ColorComponents<float, 4> components)
{
    components[3] = clampedAlpha(components[3]);

    auto hueIndex = hueComponentIndex(method);

    if (method.alphaPremultiplication == AlphaPremultiplication::Premultiplied && components[3] > 0) {
        for (size_t i = 0; i < 3; ++i) {
            if (i != hueIndex)
                components[i] /= components[3];
        }
    }

    // Hues were moved by whole turns before interpolating, so normalize them back.
    if (hueIndex)
        components[*hueIndex] = normalizeHue(components[*hueIndex]);

    auto srgb = WTF::switchOn(method.colorSpace, [&]<typename MethodColorSpace>(const MethodColorSpace&) {
        return convertColor<SRGBA<float>>(makeColorTypeByNormalizingComponents<typename MethodColorSpace::ColorType>(components).resolved());
    });

    return premultiplied(srgb.resolved());
}

// MARK: - Bicubic Bézier patches

// The weights of a cubic Bézier curve's four control points. The 3s are the binomial coefficients of (s + t)^3.
static std::array<float, 4> cubicBernsteinPolynomialsAt(float t)
{
    float s = 1 - t;
    return { s * s * s, 3 * s * s * t, 3 * s * t * t, t * t * t };
}

static FloatPoint weighted(const FloatPoint& value, float weight)
{
    return value.scaled(weight);
}

static ColorComponents<float, 4> weighted(const ColorComponents<float, 4>& value, float weight)
{
    return value * weight;
}

template<typename T> T MeshGradient::BicubicBezierPatch<T>::evaluate(const std::array<float, 4>& basisU, const std::array<float, 4>& basisV) const
{
    T result { };
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned column = 0; column < 4; ++column)
            result += weighted(controlValue(row, column), basisV[row] * basisU[column]);
    }
    return result;
}

// A Coons patch is the tensor-product patch whose interior control points are "implicitly defined by the boundary curves".
//
// PDF 1.7 Section 8.7.4.5.8, Type 7 Shadings (Tensor-Product Patch Meshes)
// https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf
template<typename T> auto MeshGradient::BicubicBezierPatch<T>::coonsPatch(const std::array<T, 4>& top, const std::array<T, 4>& right, const std::array<T, 4>& bottom, const std::array<T, 4>& left) -> BicubicBezierPatch
{
    BicubicBezierPatch patch;
    for (unsigned i = 0; i < 4; ++i) {
        patch.controlValue(0, i) = top[i];
        patch.controlValue(i, 3) = right[i];
        patch.controlValue(3, i) = bottom[i];
        patch.controlValue(i, 0) = left[i];
    }

    // The PDF specification gives the interior control values as equations:
    //
    //     p11 = 1/9 × [-4 × p00 + 6 × (p01 + p10) - 2 × (p03 + p30) + 3 × (p31 + p13) - 1 × p33]
    //     p12 = 1/9 × [-4 × p03 + 6 × (p02 + p13) - 2 × (p00 + p33) + 3 × (p32 + p10) - 1 × p30]
    //     p21 = 1/9 × [-4 × p30 + 6 × (p31 + p20) - 2 × (p33 + p00) + 3 × (p01 + p23) - 1 × p03]
    //     p22 = 1/9 × [-4 × p33 + 6 × (p32 + p23) - 2 × (p30 + p03) + 3 × (p02 + p20) - 1 × p00]
    //
    // Each starts with the near corner, the one next to the interior value, and ends with the far corner, the one
    // diagonally opposite it.
    //
    // The constants come from the surface in 8.7.4.5.7, S = S_C + S_D - S_B. S_C interpolates linearly between the top
    // and bottom edges, S_D does the same between the left and right edges, and S_B interpolates bilinearly between the
    // corners.
    //
    // A straight line written as a cubic has its inner control values a third of the way from each end. So at this
    // control value, the three surfaces are:
    //
    //     S_C: (2 × near edge's value in this column + far edge's value in this column) / 3
    //     S_D: (2 × near edge's value in this row + far edge's value in this row) / 3
    //     S_B: (4 × near corner + 2 × each of the two side corners + far corner) / 9
    //
    // Adding S_C and S_D and subtracting S_B gives the constants below, over 9.
    for (unsigned row : { 1u, 2u }) {
        for (unsigned column : { 1u, 2u }) {
            unsigned nearRow = row == 1 ? 0 : 3;
            unsigned nearColumn = column == 1 ? 0 : 3;
            unsigned farRow = 3 - nearRow;
            unsigned farColumn = 3 - nearColumn;

            T value { };
            value += weighted(patch.controlValue(nearRow, nearColumn), -4);
            value += weighted(patch.controlValue(nearRow, column), 6);
            value += weighted(patch.controlValue(row, nearColumn), 6);
            value += weighted(patch.controlValue(nearRow, farColumn), -2);
            value += weighted(patch.controlValue(farRow, nearColumn), -2);
            value += weighted(patch.controlValue(farRow, column), 3);
            value += weighted(patch.controlValue(row, farColumn), 3);
            value += weighted(patch.controlValue(farRow, farColumn), -1);
            patch.controlValue(row, column) = weighted(value, 1.0f / 9);
        }
    }

    return patch;
}

static BezierSegment edge(const MeshGradient::Point& from, const FloatPoint& fromControlPoint, const MeshGradient::Point& to, const FloatPoint& toControlPoint)
{
    return { from.position, fromControlPoint, toControlPoint, to.position };
}

static std::array<FloatPoint, 4> controlPoints(const BezierSegment& segment)
{
    return { segment.start, segment.controlPoint1, segment.controlPoint2, segment.end };
}

// PCHIP (Piecewise Cubic Hermite Interpolating Polynomial) slopes keep the curve through a sequence of values from
// overshooting them. The slope at `value` is the harmonic mean of the slopes on either side of it, or zero at a peak or
// a trough.
//
// F. N. Fritsch and J. Butland, "A Method for Constructing Local Monotone Piecewise Cubic Interpolants" (1984)
// https://doi.org/10.1137/0905021
static float monotoneSlope(float previous, float value, float next)
{
    float before = value - previous;
    float after = next - value;
    if (before * after <= 0)
        return 0;
    return 2 * before * after / (before + after);
}

// The cubic Bézier curve from `value1` to `value2`. `value0` and `value3` are their neighbors on either side.
// The slope at each end of a cubic Bézier curve is three times the distance to the next control value, so the slopes
// here are divided by 3.
static std::array<float, 4> monotoneSegment(float value0, float value1, float value2, float value3)
{
    return { value1, value1 + monotoneSlope(value0, value1, value2) / 3, value2 - monotoneSlope(value1, value2, value3) / 3, value2 };
}

static std::array<ColorComponents<float, 4>, 4> monotoneSegment(const ColorComponents<float, 4>& color0, const ColorComponents<float, 4>& color1, const ColorComponents<float, 4>& color2, const ColorComponents<float, 4>& color3)
{
    std::array<ColorComponents<float, 4>, 4> segment;
    for (size_t component = 0; component < 4; ++component) {
        auto values = monotoneSegment(color0[component], color1[component], color2[component], color3[component]);
        for (size_t i = 0; i < 4; ++i)
            segment[i][component] = values[i];
    }
    return segment;
}

// MARK: - MeshGradient

WTF_MAKE_TZONE_ALLOCATED_IMPL(MeshGradient);

static bool isValidGrid(unsigned rows, unsigned columns, size_t numberOfPoints)
{
    if (rows < 2 || columns < 2)
        return false;

    CheckedSize expectedNumberOfPoints = rows;
    expectedNumberOfPoints *= columns;
    return !expectedNumberOfPoints.hasOverflowed() && expectedNumberOfPoints.value() == numberOfPoints;
}

Ref<MeshGradient> MeshGradient::create(ColorInterpolationMethod colorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&& points, const Color& backgroundColor)
{
    return adoptRef(*new MeshGradient(colorInterpolationMethod, rows, columns, WTF::move(points), backgroundColor));
}

std::optional<Ref<MeshGradient>> MeshGradient::tryCreate(ColorInterpolationMethod colorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&& points, Color&& backgroundColor)
{
    if (!isValidGrid(rows, columns, points.size()))
        return std::nullopt;

    auto isFinite = [](const FloatPoint& point) {
        return std::isfinite(point.x()) && std::isfinite(point.y());
    };
    for (auto& point : points) {
        if (!isFinite(point.position) || !std::ranges::all_of(allBoxSides, [&](auto side) { return isFinite(point.controlPoints[side]); }))
            return std::nullopt;
    }

    return create(colorInterpolationMethod, rows, columns, WTF::move(points), backgroundColor);
}

MeshGradient::MeshGradient(ColorInterpolationMethod colorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&& points, const Color& backgroundColor)
    : m_colorInterpolationMethod { colorInterpolationMethod }
    , m_rows { rows }
    , m_columns { columns }
    , m_points { WTF::move(points) }
    , m_backgroundColor { backgroundColor }
{
    RELEASE_ASSERT(isValidGrid(m_rows, m_columns, m_points.size()));

    auto colors = m_points.map([&](auto& point) {
        return toInterpolationComponents(m_colorInterpolationMethod, point.color);
    });

    auto hueIndex = hueComponentIndex(m_colorInterpolationMethod);

    // Hues are angles, so moving one by whole turns doesn't change its color. Before interpolating, move each hue so
    // that going from one point to the next takes the arc the hue interpolation method asks for.
    // FIXME: In two dimensions, this cannot be done consistently for every pair of neighbors. For now we go down
    // the first column and then along each row.
    if (hueIndex) {
        auto method = *hueInterpolationMethod(m_colorInterpolationMethod);
        for (unsigned row = 0; row < m_rows; ++row) {
            for (unsigned column = 0; column < m_columns; ++column) {
                if (!row && !column)
                    continue;
                float referenceHue = column ? colors[index(row, column - 1)][*hueIndex] : colors[index(row - 1, 0)][*hueIndex];
                auto& hue = colors[index(row, column)][*hueIndex];
                auto [fixedReferenceHue, fixedHue] = fixupHueComponentsPriorToInterpolation(method, referenceHue, hue);
                hue = fixedHue - (fixedReferenceHue - referenceHue);
            }
        }
    }

    for (unsigned row = 0; row + 1 < m_rows; ++row) {
        for (unsigned column = 0; column + 1 < m_columns; ++column)
            m_patches.append({ makePositionPatch(row, column), makeColorPatch(row, column, colors) });
    }
}

auto MeshGradient::makePositionPatch(unsigned row, unsigned column) const -> PositionPatch
{
    auto& topLeft = point(row, column);
    auto& topRight = point(row, column + 1);
    auto& bottomLeft = point(row + 1, column);
    auto& bottomRight = point(row + 1, column + 1);

    auto top = edge(topLeft, topLeft.controlPoints.right(), topRight, topRight.controlPoints.left());
    auto right = edge(topRight, topRight.controlPoints.bottom(), bottomRight, bottomRight.controlPoints.top());
    auto bottom = edge(bottomLeft, bottomLeft.controlPoints.right(), bottomRight, bottomRight.controlPoints.left());
    auto left = edge(topLeft, topLeft.controlPoints.bottom(), bottomLeft, bottomLeft.controlPoints.top());

    return PositionPatch::coonsPatch(
        controlPoints(top),
        controlPoints(right),
        controlPoints(bottom),
        controlPoints(left)
    );
}

auto MeshGradient::makeColorPatch(unsigned row, unsigned column, const Vector<Components>& colors) const -> ColorPatch
{
    // Each edge is a cubic curve through the colors along its row or column, and the interior is the Coons patch of
    // those edges. Past the edge of the grid the colors are mirrored, so column -1 is column 1. That makes the slope
    // zero at the edge of the mesh.
    auto mirrored = [](int index, unsigned count) -> unsigned {
        int last = static_cast<int>(count) - 1;
        if (index < 0)
            return std::min(-index, last);
        if (index > last)
            return std::max(2 * last - index, 0);
        return index;
    };
    auto color = [&](int row, int column) -> const Components& {
        return colors[index(mirrored(row, m_rows), mirrored(column, m_columns))];
    };

    int r = row;
    int c = column;
    return ColorPatch::coonsPatch(
        monotoneSegment(color(r,     c - 1), color(r,     c    ), color(r,     c + 1), color(r,     c + 2)),
        monotoneSegment(color(r - 1, c + 1), color(r,     c + 1), color(r + 1, c + 1), color(r + 2, c + 1)),
        monotoneSegment(color(r + 1, c - 1), color(r + 1, c    ), color(r + 1, c + 1), color(r + 1, c + 2)),
        monotoneSegment(color(r - 1, c    ), color(r,     c    ), color(r + 1, c    ), color(r + 2, c    ))
    );
}

MeshGradient::~MeshGradient()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyMeshGradient(*this);
}

ResolvedPremultiplied<SRGBA<float>> MeshGradient::premultipliedSRGBBackgroundColor() const
{
    return premultiplied(m_backgroundColor.toColorTypeLossy<SRGBA<float>>().resolved());
}

RefPtr<NativeImage> MeshGradient::renderedImage(const IntSize& pixelSize, float scale) const
{
    Locker locker { m_renderedImageLock };
    if (m_renderedImagePixelSize != pixelSize || m_renderedImageScale != scale)
        return nullptr;
    return m_renderedImage;
}

void MeshGradient::setRenderedImage(const IntSize& pixelSize, float scale, Ref<NativeImage>&& image) const
{
    Locker locker { m_renderedImageLock };
    m_renderedImagePixelSize = pixelSize;
    m_renderedImageScale = scale;
    m_renderedImage = WTF::move(image);
}

unsigned MeshGradient::subdivisions(const Patch& patch, float scale) const
{
    static constexpr float targetTriangleSize = 8;
    static constexpr unsigned maximumSubdivisions = 64;

    // A Bézier patch lies within the bounds of its control points.
    auto [leftmost, rightmost] = std::ranges::minmax(patch.position.controlValues, { }, &FloatPoint::x);
    auto [topmost, bottommost] = std::ranges::minmax(patch.position.controlValues, { }, &FloatPoint::y);

    float size = std::max(rightmost.x() - leftmost.x(), bottommost.y() - topmost.y()) * scale;

    // A position that isn't finite can't be drawn, so there is nothing to subdivide.
    if (!std::isfinite(size))
        return 1;

    return static_cast<unsigned>(std::clamp(std::ceil(size / targetTriangleSize), 1.0f, static_cast<float>(maximumSubdivisions)));
}

auto MeshGradient::triangles(float scale) const -> Vector<Triangle>
{
    unsigned patchRows = m_rows - 1;
    unsigned patchColumns = m_columns - 1;

    // Two neighboring patches have to split the edge they share at the same points. Otherwise, each side approximates a
    // curved edge with a different polyline, and the background shows between them. The patches in a row share edges
    // that run along v, and the patches in a column share edges that run along u.
    Vector<unsigned> subdivisionsAlongV(FillWith { }, patchRows, 1u);
    Vector<unsigned> subdivisionsAlongU(FillWith { }, patchColumns, 1u);
    for (unsigned row = 0; row < patchRows; ++row) {
        for (unsigned column = 0; column < patchColumns; ++column) {
            unsigned subdivisions = this->subdivisions(m_patches[row * patchColumns + column], scale);
            subdivisionsAlongV[row] = std::max(subdivisionsAlongV[row], subdivisions);
            subdivisionsAlongU[column] = std::max(subdivisionsAlongU[column], subdivisions);
        }
    }

    Vector<Triangle> triangles;

    for (unsigned row = 0; row < patchRows; ++row) {
        for (unsigned column = 0; column < patchColumns; ++column) {
            auto& patch = m_patches[row * patchColumns + column];

            // Evaluate the patch on an evenly spaced grid of (u, v), then split each grid cell into two triangles.
            unsigned columnsInGrid = subdivisionsAlongU[column];
            unsigned rowsInGrid = subdivisionsAlongV[row];

            Vector<Vertex> grid;
            for (unsigned j = 0; j <= rowsInGrid; ++j) {
                float v = static_cast<float>(j) / rowsInGrid;
                for (unsigned i = 0; i <= columnsInGrid; ++i) {
                    float u = static_cast<float>(i) / columnsInGrid;

                    auto basisU = cubicBernsteinPolynomialsAt(u);
                    auto basisV = cubicBernsteinPolynomialsAt(v);
                    auto position = patch.position.evaluate(basisU, basisV);
                    auto color = patch.color.evaluate(basisU, basisV);

                    grid.append({ position.scaled(scale), toPremultipliedSRGB(m_colorInterpolationMethod, color) });
                }
            }

            auto gridVertex = [&](unsigned i, unsigned j) -> const Vertex& {
                return grid[j * (columnsInGrid + 1) + i];
            };

            // Each triangle draws over the ones before it, and each patch over the ones before it. So where a patch
            // folds over itself, the point with the largest v shows, and among those, the one with the largest u. This
            // is the rule in 8.7.4.5.7 of the PDF specification, except that there v goes from the bottom of a patch to
            // the top.
            for (unsigned j = 0; j < rowsInGrid; ++j) {
                for (unsigned i = 0; i < columnsInGrid; ++i) {
                    triangles.append(Triangle { gridVertex(i, j), gridVertex(i + 1, j), gridVertex(i, j + 1) });
                    triangles.append(Triangle { gridVertex(i + 1, j), gridVertex(i + 1, j + 1), gridVertex(i, j + 1) });
                }
            }
        }
    }

    return triangles;
}

TextStream& operator<<(TextStream& ts, const MeshGradient::Point& point)
{
    return ts << point.color << " at "_s << point.position << " with "_s << point.controlPoints;
}

TextStream& operator<<(TextStream& ts, const MeshGradient& gradient)
{
    ts.dumpProperty("color-interpolation-method"_s, gradient.colorInterpolationMethod());
    ts.dumpProperty("rows"_s, gradient.rows());
    ts.dumpProperty("columns"_s, gradient.columns());
    ts.dumpProperty("points"_s, gradient.points());
    ts.dumpProperty("background-color"_s, gradient.backgroundColor());
    return ts;
}

} // namespace WebCore
