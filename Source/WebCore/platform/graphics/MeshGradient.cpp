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

#include "ColorConversion.h"
#include "ColorUtilities.h"
#include "NativeImage.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

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
}

MeshGradient::~MeshGradient()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyMeshGradient(*this);
}

SRGBA<float> MeshGradient::premultipliedSRGBBackgroundColor() const
{
    return premultiplied(m_backgroundColor.toColorTypeLossy<SRGBA<float>>());
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
