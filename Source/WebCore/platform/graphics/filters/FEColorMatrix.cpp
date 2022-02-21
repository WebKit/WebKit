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

#include "FEColorMatrixSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

#if USE(CORE_IMAGE)
#include "FEColorMatrixCoreImageApplier.h"
#endif

namespace WebCore {

Ref<FEColorMatrix> FEColorMatrix::create(ColorMatrixType type, Vector<float>&& values)
{
    return adoptRef(*new FEColorMatrix(type, WTFMove(values)));
}

FEColorMatrix::FEColorMatrix(ColorMatrixType type, Vector<float>&& values)
    : FilterEffect(FilterEffect::Type::FEColorMatrix)
    , m_type(type)
    , m_values(WTFMove(values))
{
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
    components[0] = (0.213 + 0.787 * value);
    components[1] = (0.715 - 0.715 * value);
    components[2] = (0.072 - 0.072 * value);
    components[3] = (0.213 - 0.213 * value);
    components[4] = (0.715 + 0.285 * value);
    components[5] = (0.072 - 0.072 * value);
    components[6] = (0.213 - 0.213 * value);
    components[7] = (0.715 - 0.715 * value);
    components[8] = (0.072 + 0.928 * value);
}

void FEColorMatrix::calculateHueRotateComponents(float* components, float value)
{
    float cosHue = cos(value * piFloat / 180);
    float sinHue = sin(value * piFloat / 180);
    components[0] = 0.213 + cosHue * 0.787 - sinHue * 0.213;
    components[1] = 0.715 - cosHue * 0.715 - sinHue * 0.715;
    components[2] = 0.072 - cosHue * 0.072 + sinHue * 0.928;
    components[3] = 0.213 - cosHue * 0.213 + sinHue * 0.143;
    components[4] = 0.715 + cosHue * 0.285 + sinHue * 0.140;
    components[5] = 0.072 - cosHue * 0.072 - sinHue * 0.283;
    components[6] = 0.213 - cosHue * 0.213 - sinHue * 0.787;
    components[7] = 0.715 - cosHue * 0.715 + sinHue * 0.715;
    components[8] = 0.072 + cosHue * 0.928 + sinHue * 0.072;
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
    return m_type == FECOLORMATRIX_TYPE_LUMINANCETOALPHA;
}

bool FEColorMatrix::supportsAcceleratedRendering() const
{
#if USE(CORE_IMAGE)
    return FEColorMatrixCoreImageApplier::supportsCoreImageRendering(*this);
#else
    return false;
#endif
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

static TextStream& operator<<(TextStream& ts, const ColorMatrixType& type)
{
    switch (type) {
    case FECOLORMATRIX_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FECOLORMATRIX_TYPE_MATRIX:
        ts << "MATRIX";
        break;
    case FECOLORMATRIX_TYPE_SATURATE:
        ts << "SATURATE";
        break;
    case FECOLORMATRIX_TYPE_HUEROTATE:
        ts << "HUEROTATE";
        break;
    case FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
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
