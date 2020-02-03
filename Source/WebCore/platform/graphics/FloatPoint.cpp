/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FloatPoint.h"

#include "AffineTransform.h"
#include "FloatConversion.h"
#include "IntPoint.h"
#include "TransformationMatrix.h"
#include <limits>
#include <math.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FloatPoint::FloatPoint(const IntPoint& p) : m_x(p.x()), m_y(p.y())
{
}

FloatPoint FloatPoint::constrainedBetween(const FloatPoint& min, const FloatPoint& max) const
{
    return {
        std::max(min.x(), std::min(max.x(), m_x)),
        std::max(min.y(), std::min(max.y(), m_y))
    };
}

void FloatPoint::normalize()
{
    float tempLength = length();

    if (tempLength) {
        m_x /= tempLength;
        m_y /= tempLength;
    }
}

float FloatPoint::slopeAngleRadians() const
{
    return atan2f(m_y, m_x);
}

FloatPoint FloatPoint::matrixTransform(const AffineTransform& transform) const
{
    double newX, newY;
    transform.map(static_cast<double>(m_x), static_cast<double>(m_y), newX, newY);
    return narrowPrecision(newX, newY);
}

FloatPoint FloatPoint::matrixTransform(const TransformationMatrix& transform) const
{
    double newX, newY;
    transform.map(static_cast<double>(m_x), static_cast<double>(m_y), newX, newY);
    return narrowPrecision(newX, newY);
}

FloatPoint FloatPoint::narrowPrecision(double x, double y)
{
    return FloatPoint(narrowPrecisionToFloat(x), narrowPrecisionToFloat(y));
}

TextStream& operator<<(TextStream& ts, const FloatPoint& p)
{
    // FIXME: callers should use the NumberRespectingIntegers flag.
    return ts << "(" << TextStream::FormatNumberRespectingIntegers(p.x()) << "," << TextStream::FormatNumberRespectingIntegers(p.y()) << ")";
}

}
