/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGTextLayoutAttributes.h"

#include <stdio.h>
#include <wtf/text/CString.h>

namespace WebCore {

SVGTextLayoutAttributes::SVGTextLayoutAttributes()
{
}

void SVGTextLayoutAttributes::fillWithEmptyValues(unsigned length)
{
    m_xValues.fill(emptyValue(), length);
    m_yValues.fill(emptyValue(), length);
    m_dxValues.fill(emptyValue(), length);
    m_dyValues.fill(emptyValue(), length);
    m_rotateValues.fill(emptyValue(), length);
}

float SVGTextLayoutAttributes::emptyValue()
{
    static float s_emptyValue = std::numeric_limits<float>::max() - 1;
    return s_emptyValue;
}

static inline void dumpLayoutVector(Vector<float>& values)
{
    if (values.isEmpty()) {
        fprintf(stderr, "empty");
        return;
    }

    unsigned size = values.size();
    for (unsigned i = 0; i < size; ++i) {
        float value = values.at(i);
        if (value == SVGTextLayoutAttributes::emptyValue())
            fprintf(stderr, "x ");
        else
            fprintf(stderr, "%lf ", value);
    }
}

void SVGTextLayoutAttributes::dump()
{
    fprintf(stderr, "x values: ");
    dumpLayoutVector(m_xValues);
    fprintf(stderr, "\n");

    fprintf(stderr, "y values: ");
    dumpLayoutVector(m_yValues);
    fprintf(stderr, "\n");

    fprintf(stderr, "dx values: ");
    dumpLayoutVector(m_dxValues);
    fprintf(stderr, "\n");

    fprintf(stderr, "dy values: ");
    dumpLayoutVector(m_dyValues);
    fprintf(stderr, "\n");

    fprintf(stderr, "rotate values: ");
    dumpLayoutVector(m_rotateValues);
    fprintf(stderr, "\n");

    fprintf(stderr, "character data values:\n");
    Vector<CharacterData>::iterator end = m_characterDataValues.end();
    for (Vector<CharacterData>::iterator it = m_characterDataValues.begin(); it != end; ++it) {
        CharacterData& data = *it;
        fprintf(stderr, "| {spansCharacters=%i, glyphName='%s', unicodeString='%s', width=%lf, height=%lf}\n",
                data.spansCharacters, data.glyphName.utf8().data(), data.unicodeString.utf8().data(), data.width, data.height);
    }
    fprintf(stderr, "\n");
}

}

#endif // ENABLE(SVG)
