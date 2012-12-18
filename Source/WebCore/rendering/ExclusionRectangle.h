/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExclusionRectangle_h
#define ExclusionRectangle_h

#include "ExclusionShape.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

namespace WebCore {

class ExclusionRectangle : public ExclusionShape {
public:
    ExclusionRectangle(const FloatRect& bounds, const FloatSize& radii)
        : ExclusionShape()
        , m_x(bounds.x())
        , m_y(bounds.y())
        , m_width(bounds.width())
        , m_height(bounds.height())
        , m_rx(radii.width())
        , m_ry(radii.height())
    {
    }

    virtual FloatRect shapeLogicalBoundingBox() const OVERRIDE { return FloatRect(m_x, m_y, m_width, m_height); }
    virtual bool isEmpty() const OVERRIDE { return m_width <= 0 || m_height <= 0; }
    virtual void getExcludedIntervals(float logicalTop, float logicalHeight, SegmentList&) const OVERRIDE;
    virtual void getIncludedIntervals(float logicalTop, float logicalHeight, SegmentList&) const OVERRIDE;
    virtual bool firstIncludedIntervalLogicalTop(float minLogicalIntervalTop, const FloatSize& minLogicalIntervalSize, float&) const OVERRIDE;

private:
    FloatPoint cornerInterceptForWidth(float width) const;

    float m_x;
    float m_y;
    float m_width;
    float m_height;
    float m_rx; // corner X radius
    float m_ry; // corner Y radius
};

} // namespace WebCore

#endif // ExclusionRectangle_h
