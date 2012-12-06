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

#include "config.h"
#include "ExclusionShapeInsideInfo.h"

#if ENABLE(CSS_EXCLUSIONS)

#include "NotImplemented.h"
#include "RenderBlock.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

typedef HashMap<const RenderBlock*, OwnPtr<ExclusionShapeInsideInfo> > ExclusionShapeInsideInfoMap;

static ExclusionShapeInsideInfoMap& exclusionShapeInsideInfoMap()
{
    DEFINE_STATIC_LOCAL(ExclusionShapeInsideInfoMap, staticExclusionShapeInsideInfoMap, ());
    return staticExclusionShapeInsideInfoMap;
}

ExclusionShapeInsideInfo::ExclusionShapeInsideInfo(RenderBlock* block)
    : m_block(block)
    , m_shapeSizeDirty(true)
{
}

ExclusionShapeInsideInfo::~ExclusionShapeInsideInfo()
{
}

ExclusionShapeInsideInfo* ExclusionShapeInsideInfo::ensureExclusionShapeInsideInfoForRenderBlock(RenderBlock* block)
{
    ExclusionShapeInsideInfoMap::AddResult result = exclusionShapeInsideInfoMap().add(block, create(block));
    return result.iterator->value.get();
}

ExclusionShapeInsideInfo* ExclusionShapeInsideInfo::exclusionShapeInsideInfoForRenderBlock(const RenderBlock* block)
{
    ASSERT(block->style()->shapeInside());
    return exclusionShapeInsideInfoMap().get(block);
}

bool ExclusionShapeInsideInfo::isExclusionShapeInsideInfoEnabledForRenderBlock(const RenderBlock* block)
{
    // FIXME: Bug 89707: Enable shape inside for non-rectangular shapes
    ExclusionShapeValue* shapeValue = block->style()->shapeInside();
    BasicShape* shape = (shapeValue && shapeValue->type() == ExclusionShapeValue::SHAPE) ? shapeValue->shape() : 0;
    return shape && (shape->type() == BasicShape::BASIC_SHAPE_RECTANGLE || shape->type() == BasicShape::BASIC_SHAPE_POLYGON);
}

void ExclusionShapeInsideInfo::removeExclusionShapeInsideInfoForRenderBlock(const RenderBlock* block)
{
    if (!block->style() || !block->style()->shapeInside())
        return;
    exclusionShapeInsideInfoMap().remove(block);
}

void ExclusionShapeInsideInfo::computeShapeSize(LayoutUnit logicalWidth, LayoutUnit logicalHeight)
{
    if (!m_shapeSizeDirty && logicalWidth == m_logicalWidth && logicalHeight == m_logicalHeight)
        return;

    m_shapeSizeDirty = false;
    m_logicalWidth = logicalWidth;
    m_logicalHeight = logicalHeight;

    // FIXME: Bug 89993: The wrap shape may come from the parent object
    ExclusionShapeValue* shapeValue = m_block->style()->shapeInside();
    BasicShape* shape = (shapeValue && shapeValue->type() == ExclusionShapeValue::SHAPE) ? shapeValue->shape() : 0;

    ASSERT(shape);

    m_shape = ExclusionShape::createExclusionShape(shape, logicalWidth, logicalHeight, m_block->style()->writingMode());
    ASSERT(m_shape);
}

bool ExclusionShapeInsideInfo::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight)
{
    ASSERT(lineHeight >= 0);
    m_lineTop = lineTop;
    m_lineHeight = lineHeight;
    m_segments.clear();
    m_segmentRanges.clear();

    if (lineOverlapsShapeBounds()) {
        ASSERT(m_shape);
        m_shape->getIncludedIntervals(lineTop, std::min(lineHeight, shapeLogicalBottom() - lineTop), m_segments);
    }
    return m_segments.size();
}

bool ExclusionShapeInsideInfo::adjustLogicalLineTop(float minSegmentWidth)
{
    if (!m_shape || m_lineHeight <= 0 || m_lineTop > shapeLogicalBottom())
        return false;

    float floatNewLineTop;
    if (m_shape->firstIncludedIntervalLogicalTop(m_lineTop, FloatSize(minSegmentWidth, m_lineHeight), floatNewLineTop)) {
        // Use fromFloatCeil() to ensure that the returned LayoutUnit value is within the shape's bounds.
        LayoutUnit newLineTop = LayoutUnit::fromFloatCeil(floatNewLineTop);
        if (newLineTop > m_lineTop) {
            m_lineTop = newLineTop;
            return true;
        }
    }

    return false;
}

}
#endif
