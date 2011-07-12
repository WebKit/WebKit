/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
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

#if ENABLE(CSS_EXCLUSIONS)

#include "CSSWrapShapes.h"

using namespace WTF;

namespace WebCore {

String CSSWrapShapeRect::cssText() const
{
    DEFINE_STATIC_LOCAL(const String, rectParen, ("rect("));
    DEFINE_STATIC_LOCAL(const String, comma, (", "));
    
    Vector<UChar> result;
    result.reserveInitialCapacity(32);
    append(result, rectParen);

    append(result, m_left->cssText());
    append(result, comma);

    append(result, m_top->cssText());
    append(result, comma);

    append(result, m_width->cssText());
    append(result, comma);

    append(result, m_height->cssText());
    
    if (m_radiusX.get()) {
        append(result, comma);
        append(result, m_radiusX->cssText());

        if (m_radiusY.get()) {
            append(result, comma);
            append(result, m_radiusY->cssText());
        }
    }
    
    result.append(')');
            
    return String::adopt(result);
}

String CSSWrapShapeCircle::cssText() const
{
    DEFINE_STATIC_LOCAL(const String, circleParen, ("circle("));
    DEFINE_STATIC_LOCAL(const String, comma, (", "));
    
    Vector<UChar> result;
    result.reserveInitialCapacity(32);
    append(result, circleParen);

    append(result, m_left->cssText());
    append(result, comma);

    append(result, m_top->cssText());
    append(result, comma);

    append(result, m_radius->cssText());
    result.append(')');
            
    return String::adopt(result);
}

String CSSWrapShapeEllipse::cssText() const
{
    DEFINE_STATIC_LOCAL(const String, ellipseParen, ("ellipse("));
    DEFINE_STATIC_LOCAL(const String, comma, (", "));
    
    Vector<UChar> result;
    result.reserveInitialCapacity(32);
    append(result, ellipseParen);

    append(result, m_left->cssText());
    append(result, comma);

    append(result, m_top->cssText());
    append(result, comma);

    append(result, m_radiusX->cssText());
    append(result, comma);

    append(result, m_radiusY->cssText());
    result.append(')');
            
    return String::adopt(result);
}

String CSSWrapShapePolygon::cssText() const
{
    DEFINE_STATIC_LOCAL(const String, polygonParenEvenOdd, ("polygon(evenodd, "));
    DEFINE_STATIC_LOCAL(const String, polygonParenNonZero, ("polygon(nonzero, "));
    DEFINE_STATIC_LOCAL(const String, comma, (", "));
    
    Vector<UChar> result;
    result.reserveInitialCapacity(32);
    if (m_windRule == RULE_EVENODD)
        append(result, polygonParenEvenOdd);
    else
        append(result, polygonParenNonZero);

    ASSERT(!(m_values.size() % 2));

    for (unsigned i = 0; i < m_values.size(); i += 2) {
        if (i)
            result.append(' ');
        append(result, m_values.at(i)->cssText());
        append(result, comma);
        append(result, m_values.at(i + 1)->cssText());
    }
    
    result.append(')');
            
    return String::adopt(result);
}

} // namespace WebCore

#endif // ENABLE(CSS_EXCLUSIONS)
