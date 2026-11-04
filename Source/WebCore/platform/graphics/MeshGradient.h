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
#include <WebCore/ColorInterpolationMethod.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/IntSize.h>
#include <WebCore/RectEdges.h>
#include <WebCore/RenderingResource.h>
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

    WEBCORE_EXPORT static Ref<MeshGradient> create(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, const Color& backgroundColor = Color::transparentBlack);
    WEBCORE_EXPORT static std::optional<Ref<MeshGradient>> tryCreate(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, Color&& backgroundColor);

    WEBCORE_EXPORT ~MeshGradient();

    const ColorInterpolationMethod& colorInterpolationMethod() const LIFETIME_BOUND { return m_colorInterpolationMethod; }
    unsigned rows() const { return m_rows; }
    unsigned columns() const { return m_columns; }
    const Vector<Point>& points() const LIFETIME_BOUND { return m_points; }
    const Point& point(unsigned row, unsigned column) const LIFETIME_BOUND { return m_points[index(row, column)]; }
    const Color& backgroundColor() const LIFETIME_BOUND { return m_backgroundColor; }

    WEBCORE_EXPORT SRGBA<float> premultipliedSRGBBackgroundColor() const;

    RefPtr<NativeImage> renderedImage(const IntSize& pixelSize, float scale) const;
    void setRenderedImage(const IntSize& pixelSize, float scale, Ref<NativeImage>&&) const;

    void addObserver(WeakRef<RenderingResourceObserver>&& observer) const
    {
        m_observers.add(WTF::move(observer));
    }

private:
    MeshGradient(ColorInterpolationMethod, unsigned rows, unsigned columns, Vector<Point>&&, const Color& backgroundColor);

    unsigned index(unsigned row, unsigned column) const { return row * m_columns + column; }

    ColorInterpolationMethod m_colorInterpolationMethod;
    unsigned m_rows;
    unsigned m_columns;
    Vector<Point> m_points;
    Color m_backgroundColor;

    mutable Lock m_renderedImageLock;
    mutable IntSize m_renderedImagePixelSize WTF_GUARDED_BY_LOCK(m_renderedImageLock);
    mutable float m_renderedImageScale WTF_GUARDED_BY_LOCK(m_renderedImageLock) { 0 };
    mutable RefPtr<NativeImage> m_renderedImage WTF_GUARDED_BY_LOCK(m_renderedImageLock);

    mutable WeakHashSet<RenderingResourceObserver> m_observers;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const MeshGradient::Point&);
WEBCORE_EXPORT TextStream& operator<<(TextStream&, const MeshGradient&);

} // namespace WebCore
