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

#pragma once

#include <WebCore/LayoutRect.h>
#include <WebCore/Path.h>
#include <WebCore/StyleBasicShape.h>
#include <WebCore/WritingMode.h>
#include <wtf/RefCounted.h>

namespace WebCore {

struct LineSegment {
    LineSegment() = default;

    LineSegment(float logicalLeft, float logicalRight)
        : logicalLeft(logicalLeft)
        , logicalRight(logicalRight)
        , isValid(true)
    {
    }

    float logicalLeft { 0.f };
    float logicalRight { 0.f };
    bool isValid { false };
};

class Image;
class LayoutRoundedRect;

// A representation of a BasicShape that enables layout code to determine how to break a line up into segments
// that will fit within or around a shape. The line is defined by a pair of logical Y coordinates and the
// computed segments are returned as pairs of logical X coordinates. The BasicShape itself is defined in
// physical coordinates.

class LayoutShape : public RefCounted<LayoutShape> {
public:
    struct DisplayPaths {
        Path shape;
        Path marginShape;
    };

    static Ref<const LayoutShape> createShape(const Style::BasicShape&, const LayoutPoint& borderBoxOffset, const LayoutSize& logicalBoxSize, WritingMode, float logicalMargin);
    static Ref<const LayoutShape> createRasterShape(Image*, float threshold, const LayoutRect& logicalImageRect, const LayoutRect& logicalMarginRect, WritingMode, float logicalMargin);
    static Ref<const LayoutShape> createBoxShape(const LayoutRoundedRect&, WritingMode, float logicalMargin);

    virtual ~LayoutShape() = default;

    virtual LayoutRect shapeMarginLogicalBoundingBox() const = 0;
    virtual bool isEmpty() const = 0;
    virtual LineSegment getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const = 0;

    bool lineOverlapsShapeMarginBounds(LayoutUnit lineTop, LayoutUnit lineHeight) const { return lineOverlapsBoundingBox(lineTop, lineHeight, shapeMarginLogicalBoundingBox()); }

    virtual void buildDisplayPaths(DisplayPaths&) const = 0;

protected:
    float shapeMargin() const { return m_margin; }
    WritingMode writingMode() const { return m_writingMode; }
    static bool shouldFlipStartAndEndPoints(WritingMode);

private:
    bool lineOverlapsBoundingBox(LayoutUnit lineTop, LayoutUnit lineHeight, const LayoutRect& rect) const
    {
        if (rect.isEmpty())
            return false;
        return (lineTop < rect.maxY() && lineTop + lineHeight > rect.y()) || (!lineHeight && lineTop == rect.y());
    }

    WritingMode m_writingMode;
    float m_margin;
};

} // namespace WebCore
