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

#ifndef WrapShapeInfo_h
#define WrapShapeInfo_h

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShape.h"
#include "FloatRect.h"
#include "LayoutTypesInlineMethods.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class RenderBlock;

class WrapShapeInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum LineState {
        LINE_BEFORE_SHAPE,
        LINE_INSIDE_SHAPE,
        LINE_AFTER_SHAPE
    };

    ~WrapShapeInfo();

    static PassOwnPtr<WrapShapeInfo> create(RenderBlock* block) { return adoptPtr(new WrapShapeInfo(block)); }
    static WrapShapeInfo* wrapShapeInfoForRenderBlock(const RenderBlock*);
    static WrapShapeInfo* ensureWrapShapeInfoForRenderBlock(RenderBlock*);
    static void removeWrapShapeInfoForRenderBlock(const RenderBlock*);
    static bool isWrapShapeInfoEnabledForRenderBlock(const RenderBlock*);

    LayoutUnit shapeLogicalTop() const 
    { 
        ASSERT(m_shape);
        return m_shape->shapeLogicalBoundingBox().y();
    }
    bool hasSegments() const;
    const SegmentList& segments() const
    {
        ASSERT(hasSegments());
        return m_segments;
    }
    bool computeSegmentsForLine(LayoutUnit lineTop);
    LineState lineState() const;
    void computeShapeSize(LayoutUnit logicalWidth, LayoutUnit logicalHeight);
    void dirtyWrapShapeSize() { m_wrapShapeSizeDirty = true; }

private:
    WrapShapeInfo(RenderBlock*);

    RenderBlock* m_block;
    OwnPtr<ExclusionShape> m_shape;

    LayoutUnit m_lineTop;
    LayoutUnit m_logicalWidth;
    LayoutUnit m_logicalHeight;

    SegmentList m_segments;
    bool m_wrapShapeSizeDirty;
};

inline WrapShapeInfo::LineState WrapShapeInfo::lineState() const
{
    ASSERT(m_shape);
    FloatRect shapeBounds = m_shape->shapeLogicalBoundingBox();

    if (m_lineTop < shapeBounds.y())
        return LINE_BEFORE_SHAPE;

    if (m_lineTop < shapeBounds.maxY())
        return LINE_INSIDE_SHAPE;

    return LINE_AFTER_SHAPE;
}

}
#endif
#endif
