/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FEColorMatrix.h"

namespace WebCore {

FEColorMatrix::FEColorMatrix(FilterEffect* in, ColorMatrixType type, const Vector<float>& values)
    : FilterEffect()
    , m_in(in)
    , m_type(type)
    , m_values(values)
{
}

PassRefPtr<FEColorMatrix> FEColorMatrix::create(FilterEffect* in, ColorMatrixType type, const Vector<float>& values)
{
    return adoptRef(new FEColorMatrix(in, type, values));
}

ColorMatrixType FEColorMatrix::type() const
{
    return m_type;
}

void FEColorMatrix::setType(ColorMatrixType type)
{
    m_type = type;
}

const Vector<float>& FEColorMatrix::values() const
{
    return m_values;
}

void FEColorMatrix::setValues(const Vector<float> &values)
{
    m_values = values;
}

void FEColorMatrix::apply()
{
}

void FEColorMatrix::dump()
{
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
