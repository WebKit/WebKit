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

#pragma once

#include <WebCore/Color.h>
#include <WebCore/ColorComponents.h>
#include <WebCore/ColorInterpolationMethod.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntSize.h>
#include <WebCore/RectEdges.h>
#include <WebCore/RenderingResource.h>
#include <array>
#include <ranges>
#include <span>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class ColorSpace;
class NativeImage;

class MeshGradient final : public ThreadSafeRefCounted<MeshGradient>, public CanMakeThreadSafeCheckedPtr<MeshGradient> {
    WTF_MAKE_TZONE_ALLOCATED(MeshGradient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MeshGradient);
public:
    struct Point {
        FloatPoint position;
        Color color;
        RectEdges<FloatPoint> controlPoints;
    };

    struct Vertex {
        FloatPoint position;
        float u;
        float v;
        uint32_t patchIndex;
    };

    struct Triangle {
        Vertex a;
        Vertex b;
        Vertex c;
    };

    WEBCORE_EXPORT static Ref<MeshGradient> create(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, const Color& backgroundColor = Color::transparentBlack);
    WEBCORE_EXPORT static std::optional<Ref<MeshGradient>> tryCreate(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, Color&& backgroundColor);

    WEBCORE_EXPORT ~MeshGradient();

    const ColorInterpolationMethod& colorInterpolationMethod() const LIFETIME_BOUND { return m_colorInterpolationMethod; }
    unsigned rows() const { return m_rows; }
    unsigned columns() const { return m_columns; }
    const Vector<Point>& points() const LIFETIME_BOUND { return m_points; }
    const Point& point(unsigned row, unsigned column) const LIFETIME_BOUND { return m_points[index(row, column)]; }
    const Color& backgroundColor() const LIFETIME_BOUND { return m_backgroundColor; }

    WEBCORE_EXPORT static std::optional<ColorSpaceName> renderingColorSpace(const ColorSpace&);

    WEBCORE_EXPORT ResolvedPremultiplied<ColorComponents<float, 4>> premultipliedBackgroundColor(ColorSpaceName) const;
    WEBCORE_EXPORT ResolvedPremultiplied<ColorComponents<float, 4>> premultipliedColor(uint32_t patchIndex, float u, float v, ColorSpaceName) const;

    WEBCORE_EXPORT Vector<Triangle> triangles(float scale) const;

    // The 16 control values of each patch, as the terms of a weighted mean in the interpolation color space. Rows run along u.
    class ColorControlValues;
    ColorControlValues colorControlValues() const LIFETIME_BOUND;

    // The same for the points missing their alpha. Empty when no point is missing its alpha.
    ColorControlValues colorControlValuesMissingAlpha() const LIFETIME_BOUND;

    RefPtr<NativeImage> renderedImage(const IntSize& pixelSize, float scale, const ColorSpace&) const;
    void setRenderedImage(const IntSize& pixelSize, float scale, Ref<NativeImage>&&) const;

    void addObserver(WeakRef<RenderingResourceObserver>&& observer) const
    {
        m_observers.add(WTF::move(observer));
    }

private:
    using Components = ColorComponents<float, 4>;

    // The colors between points are a weighted mean of the colors of the points, one component at a time. Each point
    // adds its value times its weight, and its weight, to two sums. Those are interpolated separately and then divided.
    //
    // A point has no weight for a component it is missing. With premultiplied alpha, its weight for every component
    // except alpha and hue is its alpha. A point missing its alpha has to be premultiplied by the interpolated alpha,
    // which isn't known until the mesh is evaluated, so its terms are kept apart, in m_colorPatchesMissingAlpha, until
    // then.
    //
    // Between two points this gives the same colors as interpolateColorComponents().
    //
    // CSS Color Module Level 4, Interpolating with Missing Components
    // https://drafts.csswg.org/css-color-4/#interpolation-missing
    struct WeightedColor {
        Components weightedValues;
        Components weights;

        WeightedColor& operator+=(const WeightedColor& other)
        {
            weightedValues += other.weightedValues;
            weights += other.weights;
            return *this;
        }

        friend WeightedColor weighted(const WeightedColor& color, float weight)
        {
            return { color.weightedValues * weight, color.weights * weight };
        }
    };

    // The value at (u, v) is the sum of `controlValue(i, j) * B_i(v) * B_j(u)`, where B are the cubic Bernstein
    // polynomials. Rows run along u. The first row is the top edge (v = 0), and the last is the bottom edge (v = 1).
    //
    // https://en.wikipedia.org/wiki/B%C3%A9zier_surface
    template<typename T> struct BicubicBezierPatch {
        // Makes the bicubic patch equal to the Coons patch with these four cubic Bézier edges. The top and bottom
        // edges run along u, and the left and right edges run along v.
        static BicubicBezierPatch coonsPatch(const std::array<T, 4>& top, const std::array<T, 4>& right, const std::array<T, 4>& bottom, const std::array<T, 4>& left);

        // `basisU` and `basisV` are the cubic Bernstein polynomials at u and at v.
        T evaluate(const std::array<float, 4>& basisU, const std::array<float, 4>& basisV) const;

        T& controlValue(unsigned row, unsigned column) LIFETIME_BOUND { return controlValues[4 * row + column]; }
        const T& controlValue(unsigned row, unsigned column) const LIFETIME_BOUND { return controlValues[4 * row + column]; }

        std::array<T, 16> controlValues;
    };

    using PositionPatch = BicubicBezierPatch<FloatPoint>;
    using ColorPatch = BicubicBezierPatch<WeightedColor>;

    MeshGradient(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, const Color& backgroundColor);

    unsigned index(unsigned row, unsigned column) const { return row * m_columns + column; }

    PositionPatch makePositionPatch(unsigned row, unsigned column) const;
    ColorPatch makeColorPatch(unsigned row, unsigned column, const Vector<WeightedColor>&) const;

    std::pair<Components, Components> weightedMean(uint32_t patchIndex, float u, float v) const;
    Components interpolatedColor(uint32_t patchIndex, float u, float v) const;

    unsigned subdivisions(const PositionPatch&, float scale) const;

    ColorInterpolationMethod m_colorInterpolationMethod;
    unsigned m_rows;
    unsigned m_columns;
    Vector<Point> m_points;
    Color m_backgroundColor;

    // One patch per cell of the grid, in row order.
    Vector<PositionPatch> m_positionPatches;
    Vector<ColorPatch> m_colorPatches;

    // Empty when no point is missing its alpha.
    Vector<ColorPatch> m_colorPatchesMissingAlpha;

    mutable Lock m_renderedImageLock;
    mutable IntSize m_renderedImagePixelSize WTF_GUARDED_BY_LOCK(m_renderedImageLock);
    mutable float m_renderedImageScale WTF_GUARDED_BY_LOCK(m_renderedImageLock) { 0 };
    mutable RefPtr<NativeImage> m_renderedImage WTF_GUARDED_BY_LOCK(m_renderedImageLock);

    mutable WeakHashSet<RenderingResourceObserver> m_observers;
};

class MeshGradient::ColorControlValues : public std::ranges::view_interface<ColorControlValues> {
public:
    explicit ColorControlValues(const Vector<ColorPatch>& patches LIFETIME_BOUND)
        : m_values { patches | std::views::transform(&ColorPatch::controlValues) | std::views::join }
        , m_size { patches.size() * 16 }
    {
    }

    auto begin() const { return m_values.begin(); }
    auto end() const { return m_values.end(); }
    size_t size() const { return m_size; }

private:
    std::ranges::join_view<std::ranges::transform_view<std::ranges::ref_view<const Vector<ColorPatch>>, decltype(&ColorPatch::controlValues)>> m_values;
    size_t m_size;
};

inline MeshGradient::ColorControlValues MeshGradient::colorControlValues() const
{
    return ColorControlValues { m_colorPatches };
}

inline MeshGradient::ColorControlValues MeshGradient::colorControlValuesMissingAlpha() const
{
    return ColorControlValues { m_colorPatchesMissingAlpha };
}

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const MeshGradient::Point&);
WEBCORE_EXPORT TextStream& operator<<(TextStream&, const MeshGradient&);

} // namespace WebCore
