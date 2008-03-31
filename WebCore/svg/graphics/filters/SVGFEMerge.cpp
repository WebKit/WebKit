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
#include "SVGFEMerge.h"
#include "TextStream.h"

namespace WebCore {

const Vector<String>& SVGFEMerge::mergeInputs() const
{
    return m_mergeInputs;
}

void SVGFEMerge::setMergeInputs(const Vector<String>& mergeInputs)
{
    m_mergeInputs = mergeInputs;
}

TextStream& SVGFEMerge::externalRepresentation(TextStream& ts) const
{
    ts << "[type=MERGE] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << "[merge inputs=[";
    unsigned x = 0;
    unsigned size = m_mergeInputs.size();
    while (x < size) {
        ts << m_mergeInputs[x];
        x++;
        if (x < m_mergeInputs.size())
            ts << ", ";
    }
    ts << "]]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
