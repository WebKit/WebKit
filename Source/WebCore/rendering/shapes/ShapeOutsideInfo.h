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

#ifndef ShapeOutsideInfo_h
#define ShapeOutsideInfo_h

#if ENABLE(CSS_SHAPES)

#include "FloatRect.h"
#include "LayoutSize.h"
#include "LayoutUnit.h"
#include "RenderStyle.h"
#include "Shape.h"
#include "ShapeValue.h"
#include <memory>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;
class FloatingObject;

class ShapeOutsideInfo final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShapeOutsideInfo(const RenderBox& renderer)
        : m_renderer(renderer)
        , m_lineOverlapsShape(false)
    {
    }

    static bool isEnabledFor(const RenderBox&);

    LayoutUnit leftMarginBoxDelta() const { return m_leftMarginBoxDelta; }
    LayoutUnit rightMarginBoxDelta() const { return m_rightMarginBoxDelta; }
    bool lineOverlapsShape() const { return m_lineOverlapsShape; }

    void updateDeltasForContainingBlockLine(const RenderBlockFlow&, const FloatingObject&, LayoutUnit lineTop, LayoutUnit lineHeight);

    void setReferenceBoxLogicalSize(LayoutSize);

    LayoutUnit shapeLogicalTop() const { return computedShape().shapeMarginLogicalBoundingBox().y() + logicalTopOffset(); }
    LayoutUnit shapeLogicalBottom() const { return computedShape().shapeMarginLogicalBoundingBox().maxY() + logicalTopOffset(); }
    LayoutUnit shapeLogicalLeft() const { return computedShape().shapeMarginLogicalBoundingBox().x() + logicalLeftOffset(); }
    LayoutUnit shapeLogicalRight() const { return computedShape().shapeMarginLogicalBoundingBox().maxX() + logicalLeftOffset(); }
    LayoutUnit shapeLogicalWidth() const { return computedShape().shapeMarginLogicalBoundingBox().width(); }
    LayoutUnit shapeLogicalHeight() const { return computedShape().shapeMarginLogicalBoundingBox().height(); }

    LayoutUnit logicalLineTop() const { return m_referenceBoxLineTop + logicalTopOffset(); }
    LayoutUnit logicalLineBottom() const { return m_referenceBoxLineTop + m_lineHeight + logicalTopOffset(); }
    LayoutUnit logicalLineBottom(LayoutUnit lineHeight) const { return m_referenceBoxLineTop + lineHeight + logicalTopOffset(); }

    void markShapeAsDirty() { m_shape = nullptr; }
    bool isShapeDirty() { return !m_shape; }

    LayoutRect computedShapePhysicalBoundingBox() const;
    FloatPoint shapeToRendererPoint(const FloatPoint&) const;
    FloatSize shapeToRendererSize(const FloatSize&) const;

    const Shape& computedShape() const;

    static ShapeOutsideInfo& ensureInfo(const RenderBox& key)
    {
        InfoMap& infoMap = ShapeOutsideInfo::infoMap();
        if (ShapeOutsideInfo* info = infoMap.get(&key))
            return *info;
        auto result = infoMap.add(&key, std::make_unique<ShapeOutsideInfo>(key));
        return *result.iterator->value;
    }
    static void removeInfo(const RenderBox& key) { infoMap().remove(&key); }
    static ShapeOutsideInfo* info(const RenderBox& key) { return infoMap().get(&key); }

private:
    SegmentList computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight) const;

    LayoutUnit logicalTopOffset() const;
    LayoutUnit logicalLeftOffset() const;

    typedef HashMap<const RenderBox*, std::unique_ptr<ShapeOutsideInfo>> InfoMap;
    static InfoMap& infoMap()
    {
        DEPRECATED_DEFINE_STATIC_LOCAL(InfoMap, staticInfoMap, ());
        return staticInfoMap;
    }

    const RenderBox& m_renderer;

    mutable std::unique_ptr<Shape> m_shape;
    LayoutSize m_referenceBoxLogicalSize;
    LayoutUnit m_referenceBoxLineTop;
    LayoutUnit m_lineHeight;

    LayoutUnit m_leftMarginBoxDelta;
    LayoutUnit m_rightMarginBoxDelta;
    LayoutUnit m_borderBoxLineTop;
    bool m_lineOverlapsShape;
};

}
#endif
#endif
