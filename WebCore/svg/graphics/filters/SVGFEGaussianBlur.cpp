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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEGaussianBlur.h"
#include "SVGRenderTreeAsText.h"
#include "Filter.h"

namespace WebCore {

FEGaussianBlur::FEGaussianBlur(FilterEffect* in, const float& x, const float& y)
    : FilterEffect()
    , m_in(in)
    , m_x(x)
    , m_y(y)
{
}

PassRefPtr<FEGaussianBlur> FEGaussianBlur::create(FilterEffect* in, const float& x, const float& y)
{
    return adoptRef(new FEGaussianBlur(in, x, y));
}

float FEGaussianBlur::stdDeviationX() const
{
    return m_x;
}

void FEGaussianBlur::setStdDeviationX(float x)
{
    m_x = x;
}

float FEGaussianBlur::stdDeviationY() const
{
    return m_y;
}

void FEGaussianBlur::setStdDeviationY(float y)
{
    m_y = y;
}

void FEGaussianBlur::apply(Filter*)
{
}

void FEGaussianBlur::dump()
{
}

TextStream& FEGaussianBlur::externalRepresentation(TextStream& ts) const
{
    ts << "[type=GAUSSIAN-BLUR] ";
    FilterEffect::externalRepresentation(ts);
    ts << " [std dev. x=" << stdDeviationX() << " y=" << stdDeviationY() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
