/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FloatLine.h"

namespace WebCore {

const FloatPoint FloatLine::pointAtAbsoluteDistance(float absoluteDistance) const
{
    if (!length())
        return m_start;
    auto relativeDistance = absoluteDistance / length();
    return pointAtRelativeDistance(relativeDistance);
}

const FloatPoint FloatLine::pointAtRelativeDistance(float relativeDistance) const
{
    return {
        m_start.x() - (relativeDistance * (m_start.x() - m_end.x())),
        m_start.y() - (relativeDistance * (m_start.y() - m_end.y())),
    };
}

const FloatLine FloatLine::extendedToBounds(const FloatRect& bounds) const
{
    if (fabs(m_start.x() - m_end.x()) <= fabs(m_start.y() - m_end.y())) {
        // The line is roughly vertical, so construct points at the top and bottom of the bounds.
        FloatPoint top = { (((bounds.y() - m_start.y()) * (m_end.x() - m_start.x())) / (m_end.y() - m_start.y())) + m_start.x(), bounds.y() };
        FloatPoint bottom = { (((bounds.y() + bounds.height() - m_start.y()) * (m_end.x() - m_start.x())) / (m_end.y() - m_start.y())) + m_start.x(), bounds.y() + bounds.height() };
        return { top, bottom };
    }
    
    // The line is roughly horizontal, so construct points at the left and right of the bounds.
    FloatPoint left = { bounds.x(), (((bounds.x() - m_start.x()) * (m_end.y() - m_start.y())) / (m_end.x() - m_start.x())) + m_start.y() };
    FloatPoint right = { bounds.x() + bounds.width(), (((bounds.x() + bounds.width() - m_start.x()) * (m_end.y() - m_start.y())) / (m_end.x() - m_start.x())) + m_start.y() };
    return { left, right };
}

const std::optional<FloatPoint> FloatLine::intersectionWith(const FloatLine& otherLine) const
{
    float denominator = ((m_start.x() - m_end.x()) * (otherLine.start().y() - otherLine.end().y())) - ((m_start.y() - m_end.y()) * (otherLine.start().x() - otherLine.end().x()));
    
    // A denominator of zero indicates the lines are parallel or coincident, which means there is no true intersection.
    if (!denominator)
        return std::nullopt;
    
    float thisLineCommonNumeratorFactor = (m_start.x() * m_end.y()) - (m_start.y() * m_end.x());
    float otherLineCommonNumeratorFactor = (otherLine.start().x() * otherLine.end().y()) - (otherLine.start().y() * otherLine.end().x());
    
    return {{
        ((thisLineCommonNumeratorFactor * (otherLine.start().x() - otherLine.end().x())) - (otherLineCommonNumeratorFactor * (m_start.x() - m_end.x()))) / denominator,
        ((thisLineCommonNumeratorFactor * (otherLine.start().y() - otherLine.end().y())) - (otherLineCommonNumeratorFactor * (m_start.y() - m_end.y()))) / denominator,
    }};
}

}
