/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGFilterEffect.h"

#include "SVGRenderTreeAsText.h"
#include "TextStream.h"

namespace WebCore {

FloatRect SVGFilterEffect::subRegion() const
{
    return m_subRegion;
}

void SVGFilterEffect::setSubRegion(const FloatRect& subRegion)
{
    m_subRegion = subRegion;
}

String SVGFilterEffect::in() const
{
    return m_in;
}

void SVGFilterEffect::setIn(const String& in)
{
    m_in = in;
}

String SVGFilterEffect::result() const
{
    return m_result;
}

void SVGFilterEffect::setResult(const String& result)
{
    m_result = result;
}

TextStream& SVGFilterEffect::externalRepresentation(TextStream& ts) const
{
    if (!in().isEmpty())
        ts << "[in=\"" << in() << "\"]";
    if (!result().isEmpty())
        ts << " [result=\"" << result() << "\"]";
    if (!subRegion().isEmpty())
        ts << " [subregion=\"" << subRegion() << "\"]";
    return ts;
}

TextStream& operator<<(TextStream& ts, const SVGFilterEffect& e)
{
    return e.externalRepresentation(ts);
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
