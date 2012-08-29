/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "CSSWrapShapes.h"

#include <wtf/text/StringBuilder.h>

using namespace WTF;

namespace WebCore {

String CSSWrapShapeRectangle::cssText() const
{
    StringBuilder result;
    result.reserveCapacity(32);

    result.appendLiteral("rectangle(");

    result.append(m_x->cssText());
    result.appendLiteral(", ");

    result.append(m_y->cssText());
    result.appendLiteral(", ");

    result.append(m_width->cssText());
    result.appendLiteral(", ");

    result.append(m_height->cssText());

    if (m_radiusX.get()) {
        result.appendLiteral(", ");
        result.append(m_radiusX->cssText());

        if (m_radiusY.get()) {
            result.appendLiteral(", ");
            result.append(m_radiusY->cssText());
        }
    }

    result.append(')');

    return result.toString();
}

String CSSWrapShapeCircle::cssText() const
{
    StringBuilder result;
    result.reserveCapacity(32);

    result.appendLiteral("circle(");

    result.append(m_centerX->cssText());
    result.appendLiteral(", ");

    result.append(m_centerY->cssText());
    result.appendLiteral(", ");

    result.append(m_radius->cssText());
    result.append(')');

    return result.toString();
}

String CSSWrapShapeEllipse::cssText() const
{
    StringBuilder result;
    result.reserveCapacity(32);
    result.appendLiteral("ellipse(");

    result.append(m_centerX->cssText());
    result.appendLiteral(", ");

    result.append(m_centerY->cssText());
    result.appendLiteral(", ");

    result.append(m_radiusX->cssText());
    result.appendLiteral(", ");

    result.append(m_radiusY->cssText());
    result.append(')');

    return result.toString();
}

String CSSWrapShapePolygon::cssText() const
{
    StringBuilder result;
    result.reserveCapacity(32);

    if (m_windRule == RULE_EVENODD)
        result.appendLiteral("polygon(evenodd, ");
    else
        result.appendLiteral("polygon(nonzero, ");

    ASSERT(!(m_values.size() % 2));

    for (unsigned i = 0; i < m_values.size(); i += 2) {
        if (i)
            result.appendLiteral(", ");
        result.append(m_values.at(i)->cssText());
        result.append(' ');
        result.append(m_values.at(i + 1)->cssText());
    }

    result.append(')');

    return result.toString();
}

} // namespace WebCore

