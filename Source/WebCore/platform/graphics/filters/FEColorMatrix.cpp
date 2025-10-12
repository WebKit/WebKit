/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FEColorMatrix.h"

#include "ColorMatrix.h"
#include "FEColorMatrixSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

#if USE(CORE_IMAGE)
#include "FEColorMatrixCoreImageApplier.h"
#endif

#if USE(SKIA)
#include "FEColorMatrixSkiaApplier.h"
#endif

namespace WebCore {

Ref<FEColorMatrix> FEColorMatrix::create(ColorMatrixType type, Vector<float>&& values, DestinationColorSpace colorSpace)
{
    return adoptRef(*new FEColorMatrix(type, WTFMove(values), colorSpace));
}

FEColorMatrix::FEColorMatrix(ColorMatrixType type, Vector<float>&& values, DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::FEColorMatrix, colorSpace)
    , m_type(type)
    , m_values(WTFMove(values))
{
}

bool FEColorMatrix::operator==(const FEColorMatrix& other) const
{
    return FilterEffect::operator==(other)
        && m_type == other.m_type
        && m_values == other.m_values;
}

bool FEColorMatrix::setType(ColorMatrixType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

bool FEColorMatrix::setValues(const Vector<float> &values)
{
    if (m_values == values)
        return false;
    m_values = values;
    return true;
}

void FEColorMatrix::calculateSaturateComponents(std::span<float, 9> components, float value)
{
    auto saturationMatrix = saturationColorMatrix(value);

    components[0] = saturationMatrix.at(0, 0);
    components[1] = saturationMatrix.at(0, 1);
    components[2] = saturationMatrix.at(0, 2);

    components[3] = saturationMatrix.at(1, 0);
    components[4] = saturationMatrix.at(1, 1);
    components[5] = saturationMatrix.at(1, 2);

    components[6] = saturationMatrix.at(2, 0);
    components[7] = saturationMatrix.at(2, 1);
    components[8] = saturationMatrix.at(2, 2);
}

void FEColorMatrix::calculateHueRotateComponents(std::span<float, 9> components, float angleInDegrees)
{
    auto hueRotateMatrix = hueRotateColorMatrix(angleInDegrees);

    components[0] = hueRotateMatrix.at(0, 0);
    components[1] = hueRotateMatrix.at(0, 1);
    components[2] = hueRotateMatrix.at(0, 2);

    components[3] = hueRotateMatrix.at(1, 0);
    components[4] = hueRotateMatrix.at(1, 1);
    components[5] = hueRotateMatrix.at(1, 2);

    components[6] = hueRotateMatrix.at(2, 0);
    components[7] = hueRotateMatrix.at(2, 1);
    components[8] = hueRotateMatrix.at(2, 2);
}

Vector<float> FEColorMatrix::normalizedFloats(const Vector<float>& values)
{
    Vector<float> normalizedValues(values.size());
    for (size_t i = 0; i < values.size(); ++i)
        normalizedValues[i] = normalizedFloat(values[i]);
    return normalizedValues;
}

bool FEColorMatrix::resultIsAlphaImage(std::span<const Ref<FilterImage>>) const
{
    return m_type == ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA;
}

OptionSet<FilterRenderingMode> FEColorMatrix::supportedFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes) const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if USE(CORE_IMAGE)
    if (FEColorMatrixCoreImageApplier::supportsCoreImageRendering(*this))
        modes.add(FilterRenderingMode::Accelerated);
#endif
#if USE(SKIA)
    modes.add(FilterRenderingMode::Accelerated);
#endif
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
    if (m_type == ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX
        || m_type == ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE
        || m_type == ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE)
        modes.add(FilterRenderingMode::GraphicsContext);
#endif
    return modes & preferredFilterRenderingModes;
}

std::unique_ptr<FilterEffectApplier> FEColorMatrix::createAcceleratedApplier() const
{
#if USE(CORE_IMAGE)
    return FilterEffectApplier::create<FEColorMatrixCoreImageApplier>(*this);
#elif USE(SKIA)
    return FilterEffectApplier::create<FEColorMatrixSkiaApplier>(*this);
#else
    return nullptr;
#endif
}

std::unique_ptr<FilterEffectApplier> FEColorMatrix::createSoftwareApplier() const
{
#if USE(SKIA)
    return FilterEffectApplier::create<FEColorMatrixSkiaApplier>(*this);
#else
    return FilterEffectApplier::create<FEColorMatrixSoftwareApplier>(*this);
#endif
}

std::optional<GraphicsStyle> FEColorMatrix::createGraphicsStyle(GraphicsContext&, const Filter&) const
{
    switch (m_type) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX: {
        RELEASE_ASSERT(m_values.size() == 20);
        GraphicsColorMatrix result;
        std::copy_n(m_values.begin(), std::min<size_t>(m_values.size(), 20), result.values.begin());
        return result;
    }
    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
        return GraphicsColorMatrix { ColorMatrix<5, 4>(saturationColorMatrix(m_values[0])).data() };

    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        return GraphicsColorMatrix { ColorMatrix<5, 4>(hueRotateColorMatrix(m_values[0])).data() };

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return { };
}

static TextStream& operator<<(TextStream& ts, const ColorMatrixType& type)
{
    switch (type) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
        ts << "UNKNOWN"_s;
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
        ts << "MATRIX"_s;
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
        ts << "SATURATE"_s;
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        ts << "HUEROTATE"_s;
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        ts << "LUMINANCETOALPHA"_s;
        break;
    }
    return ts;
}

TextStream& FEColorMatrix::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feColorMatrix"_s;
    FilterEffect::externalRepresentation(ts, representation);

    ts << " type=\"" << m_type << '"';
    if (!m_values.isEmpty()) {
        ts << " values=\""_s;
        bool isFirst = true;
        for (auto value : m_values) {
            if (isFirst)
                isFirst = false;
            else
                ts << ' ';
            ts << value;
        }
        ts << '"';
    }

    ts << "]\n"_s;
    return ts;
}

} // namespace WebCore
