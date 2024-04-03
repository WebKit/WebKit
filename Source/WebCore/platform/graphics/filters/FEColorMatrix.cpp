/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

void FEColorMatrix::calculateSaturateComponents(float* components, float value)
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

void FEColorMatrix::calculateHueRotateComponents(float* components, float angleInDegrees)
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

bool FEColorMatrix::resultIsAlphaImage(const FilterImageVector&) const
{
    return m_type == ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA;
}

OptionSet<FilterRenderingMode> FEColorMatrix::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if USE(CORE_IMAGE)
    if (FEColorMatrixCoreImageApplier::supportsCoreImageRendering(*this))
        modes.add(FilterRenderingMode::Accelerated);
#endif
#if HAVE(CGSTYLE_COLORMATRIX_BLUR)
    if (m_type == ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX)
        modes.add(FilterRenderingMode::GraphicsContext);
#endif
    return modes;
}

std::unique_ptr<FilterEffectApplier> FEColorMatrix::createAcceleratedApplier() const
{
#if USE(CORE_IMAGE)
    return FilterEffectApplier::create<FEColorMatrixCoreImageApplier>(*this);
#else
    return nullptr;
#endif
}

std::unique_ptr<FilterEffectApplier> FEColorMatrix::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEColorMatrixSoftwareApplier>(*this);
}

std::optional<GraphicsStyle> FEColorMatrix::createGraphicsStyle(const Filter&) const
{
    std::array<float, 20> values;
    std::copy_n(m_values.begin(), std::min<size_t>(m_values.size(), 20), values.begin());
    return GraphicsColorMatrix { values };
}

static TextStream& operator<<(TextStream& ts, const ColorMatrixType& type)
{
    switch (type) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
        ts << "MATRIX";
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
        ts << "SATURATE";
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        ts << "HUEROTATE";
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        ts << "LUMINANCETOALPHA";
        break;
    }
    return ts;
}

TextStream& FEColorMatrix::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feColorMatrix";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " type=\"" << m_type << "\"";
    if (!m_values.isEmpty()) {
        ts << " values=\"";
        Vector<float>::const_iterator ptr = m_values.begin();
        const Vector<float>::const_iterator end = m_values.end();
        while (ptr < end) {
            ts << *ptr;
            ++ptr;
            if (ptr < end)
                ts << " ";
        }
        ts << "\"";
    }

    ts << "]\n";
    return ts;
}

} // namespace WebCore
