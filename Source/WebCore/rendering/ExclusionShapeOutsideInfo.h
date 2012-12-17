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

#ifndef ExclusionShapeOutsideInfo_h
#define ExclusionShapeOutsideInfo_h

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShape.h"
#include "FloatRect.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class RenderBox;

class ExclusionShapeOutsideInfo {
    WTF_MAKE_NONCOPYABLE(ExclusionShapeOutsideInfo); WTF_MAKE_FAST_ALLOCATED;
public:
    ~ExclusionShapeOutsideInfo();

    static PassOwnPtr<ExclusionShapeOutsideInfo> create(RenderBox* box) { return adoptPtr(new ExclusionShapeOutsideInfo(box)); }
    static ExclusionShapeOutsideInfo* infoForRenderBox(const RenderBox*);
    static ExclusionShapeOutsideInfo* ensureInfoForRenderBox(RenderBox*);
    static void removeInfoForRenderBox(const RenderBox*);
    static bool isInfoEnabledForRenderBox(const RenderBox*);

    LayoutUnit shapeLogicalLeft() const
    { 
        return computedShape()->shapeLogicalBoundingBox().x();
    }
    LayoutUnit shapeLogicalRight() const
    { 
        return computedShape()->shapeLogicalBoundingBox().maxX();
    }
    LayoutUnit shapeLogicalTop() const
    { 
        return LayoutUnit::fromFloatCeil(computedShape()->shapeLogicalBoundingBox().y());
    }
    LayoutUnit shapeLogicalBottom() const
    {
        return LayoutUnit::fromFloatFloor(computedShape()->shapeLogicalBoundingBox().maxY());
    }
    LayoutUnit shapeLogicalWidth() const
    { 
        return computedShape()->shapeLogicalBoundingBox().width();
    }
    LayoutUnit shapeLogicalHeight() const
    {
        return computedShape()->shapeLogicalBoundingBox().height();
    }

    void setShapeSize(LayoutUnit logicalWidth, LayoutUnit logicalHeight)
    {
        if (m_logicalWidth == logicalWidth && m_logicalHeight == logicalHeight)
            return;

        dirtyShapeSize();
        m_logicalWidth = logicalWidth;
        m_logicalHeight = logicalHeight;
    }

    void dirtyShapeSize() { m_computedShape.clear(); }

private:
    explicit ExclusionShapeOutsideInfo(RenderBox*);
    const ExclusionShape* computedShape() const;

    RenderBox* m_box;
    mutable OwnPtr<ExclusionShape> m_computedShape;

    LayoutUnit m_logicalWidth;
    LayoutUnit m_logicalHeight;
};

}
#endif
#endif
