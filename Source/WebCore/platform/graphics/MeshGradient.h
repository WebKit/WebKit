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
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

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
        ResolvedPremultiplied<SRGBA<float>> color;
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

    WEBCORE_EXPORT ResolvedPremultiplied<SRGBA<float>> premultipliedSRGBBackgroundColor() const;

    WEBCORE_EXPORT Vector<Triangle> triangles(float scale) const;

    RefPtr<NativeImage> renderedImage(const IntSize& pixelSize, float scale) const;
    void setRenderedImage(const IntSize& pixelSize, float scale, Ref<NativeImage>&&) const;

    void addObserver(WeakRef<RenderingResourceObserver>&& observer) const
    {
        m_observers.add(WTF::move(observer));
    }

private:
    using Components = ColorComponents<float, 4>;

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
    using ColorPatch = BicubicBezierPatch<Components>;

    struct Patch {
        PositionPatch position;
        ColorPatch color;
    };

    MeshGradient(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, const Color& backgroundColor);

    unsigned index(unsigned row, unsigned column) const { return row * m_columns + column; }

    PositionPatch makePositionPatch(unsigned row, unsigned column) const;
    ColorPatch makeColorPatch(unsigned row, unsigned column, const Vector<Components>&) const;

    unsigned subdivisions(const Patch&, float scale) const;

    ColorInterpolationMethod m_colorInterpolationMethod;
    unsigned m_rows;
    unsigned m_columns;
    Vector<Point> m_points;
    Color m_backgroundColor;

    // One patch per cell of the grid, in row order.
    Vector<Patch> m_patches;

    mutable Lock m_renderedImageLock;
    mutable IntSize m_renderedImagePixelSize WTF_GUARDED_BY_LOCK(m_renderedImageLock);
    mutable float m_renderedImageScale WTF_GUARDED_BY_LOCK(m_renderedImageLock) { 0 };
    mutable RefPtr<NativeImage> m_renderedImage WTF_GUARDED_BY_LOCK(m_renderedImageLock);

    mutable WeakHashSet<RenderingResourceObserver> m_observers;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const MeshGradient::Point&);
WEBCORE_EXPORT TextStream& operator<<(TextStream&, const MeshGradient&);

} // namespace WebCore
