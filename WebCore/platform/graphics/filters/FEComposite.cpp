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
#include "FEComposite.h"

namespace WebCore {

FEComposite::FEComposite(FilterEffect* in, FilterEffect* in2, const CompositeOperationType& type,
    const float& k1, const float& k2, const float& k3, const float& k4)
    : FilterEffect()
    , m_in(in)
    , m_in2(in2)
    , m_type(type)
    , m_k1(k1)
    , m_k2(k2)
    , m_k3(k3)
    , m_k4(k4)
{
}

PassRefPtr<FEComposite> FEComposite::create(FilterEffect* in, FilterEffect* in2, const CompositeOperationType& type,
    const float& k1, const float& k2, const float& k3, const float& k4)
{
    return adoptRef(new FEComposite(in, in2, type, k1, k2, k3, k4));
}

CompositeOperationType FEComposite::operation() const
{
    return m_type;
}

void FEComposite::setOperation(CompositeOperationType type)
{
    m_type = type;
}

float FEComposite::k1() const
{
    return m_k1;
}

void FEComposite::setK1(float k1)
{
    m_k1 = k1;
}

float FEComposite::k2() const
{
    return m_k2;
}

void FEComposite::setK2(float k2)
{
    m_k2 = k2;
}

float FEComposite::k3() const
{
    return m_k3;
}

void FEComposite::setK3(float k3)
{
    m_k3 = k3;
}

float FEComposite::k4() const
{
    return m_k4;
}

void FEComposite::setK4(float k4)
{
    m_k4 = k4;
}

void FEComposite::apply()
{
}

void FEComposite::dump()
{
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
