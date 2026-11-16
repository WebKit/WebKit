/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatRect.h>
#include <WebCore/LayoutShape.h>

namespace WebCore {

// shape-outside from an arbitrary path, shape() or path() held as the flattened outline
class PathLayoutShape final : public LayoutShape {
    WTF_MAKE_NONCOPYABLE(PathLayoutShape);
public:
    PathLayoutShape(Vector<Vector<FloatPoint>>&& polylines, const FloatRect& bounds)
        : m_polylines(WTF::move(polylines))
        , m_bounds(bounds)
    {
    }

    LayoutRect shapeMarginLogicalBoundingBox() const override;
    bool isEmpty() const override { return m_polylines.isEmpty(); }
    LineSegment getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const override;

    void buildDisplayPaths(DisplayPaths&) const override;

private:
    Vector<Vector<FloatPoint>> m_polylines;
    FloatRect m_bounds;
};

} // namespace WebCore
