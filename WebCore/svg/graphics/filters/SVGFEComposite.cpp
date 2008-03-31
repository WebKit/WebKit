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
#include "SVGFEComposite.h"
#include "TextStream.h"

namespace WebCore {

SVGFEComposite::SVGFEComposite(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_operation(SVG_FECOMPOSITE_OPERATOR_UNKNOWN)
    , m_k1(0.0f)
    , m_k2(0.0f)
    , m_k3(0.0f)
    , m_k4(0.0f)
{
}

String SVGFEComposite::in2() const
{
    return m_in2;
}

void SVGFEComposite::setIn2(const String& in2)
{
    m_in2 = in2;
}

SVGCompositeOperationType SVGFEComposite::operation() const
{
    return m_operation;
}

void SVGFEComposite::setOperation(SVGCompositeOperationType oper)
{
    m_operation = oper;
}

float SVGFEComposite::k1() const
{
    return m_k1;
}

void SVGFEComposite::setK1(float k1)
{
    m_k1 = k1;
}

float SVGFEComposite::k2() const
{
    return m_k2;
}

void SVGFEComposite::setK2(float k2)
{
    m_k2 = k2;
}

float SVGFEComposite::k3() const
{
    return m_k3;
}

void SVGFEComposite::setK3(float k3)
{
    m_k3 = k3;
}

float SVGFEComposite::k4() const
{
    return m_k4;
}

void SVGFEComposite::setK4(float k4)
{
    m_k4 = k4;
}

TextStream& SVGFEComposite::externalRepresentation(TextStream& ts) const
{
    ts << "[type=COMPOSITE] ";
    SVGFilterEffect::externalRepresentation(ts);
    if (!in2().isEmpty())
        ts << " [in2=\"" << in2() << "\"]";
    ts << " [k1=" << k1() << " k2=" << k2() << " k3=" << k3() << " k4=" << k4() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
