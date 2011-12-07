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

#ifndef RenderRegion_h
#define RenderRegion_h

#include "RenderReplaced.h"

namespace WebCore {

class RenderBox;
class RenderBoxRegionInfo;
class RenderFlowThread;

class RenderRegion : public RenderReplaced {
public:
    explicit RenderRegion(Node*, RenderFlowThread*);
    virtual ~RenderRegion();

    virtual bool isRenderRegion() const { return true; }

    virtual void paintReplaced(PaintInfo&, const LayoutPoint&);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void setRegionRect(const LayoutRect& rect) { m_regionRect = rect; }
    LayoutRect regionRect() const { return m_regionRect; }
    LayoutRect regionOverflowRect() const;

    void attachRegion();
    void detachRegion();

    RenderFlowThread* parentFlowThread() const { return m_parentFlowThread; }

    // Valid regions do not create circular dependencies with other flows.
    bool isValid() const { return m_isValid; }
    void setIsValid(bool valid) { m_isValid = valid; }

    bool hasCustomRegionStyle() const { return m_hasCustomRegionStyle; }
    void setHasCustomRegionStyle(bool hasCustomRegionStyle) { m_hasCustomRegionStyle = hasCustomRegionStyle; }

    virtual void layout();

    RenderBoxRegionInfo* renderBoxRegionInfo(const RenderBox*) const;
    RenderBoxRegionInfo* setRenderBoxRegionInfo(const RenderBox*, LayoutUnit logicalLeftInset, LayoutUnit logicalRightInset,
        bool containingBlockChainIsInset);
    RenderBoxRegionInfo* takeRenderBoxRegionInfo(const RenderBox*);
    void removeRenderBoxRegionInfo(const RenderBox*);

    void deleteAllRenderBoxRegionInfo();

    LayoutUnit offsetFromLogicalTopOfFirstPage() const;

    bool isFirstRegion() const;
    bool isLastRegion() const;

    RenderStyle* renderObjectRegionStyle(const RenderObject*) const;
    void computeStyleInRegion(const RenderObject*);
    void clearObjectStyleInRegion(const RenderObject*);
private:
    virtual const char* renderName() const { return "RenderRegion"; }

    RenderFlowThread* m_flowThread;

    // If this RenderRegion is displayed as part of another flow,
    // we need to create a dependency tree, so that layout of the
    // regions is always done before the regions themselves.
    RenderFlowThread* m_parentFlowThread;
    LayoutRect m_regionRect;

    // This map holds unique information about a block that is split across regions.
    // A RenderBoxRegionInfo* tells us about any layout information for a RenderBox that
    // is unique to the region. For now it just holds logical width information for RenderBlocks, but eventually
    // it will also hold a custom style for any box (for region styling).
    HashMap<const RenderBox*, RenderBoxRegionInfo*> m_renderBoxRegionInfo;

    // This map holds information about the region style associated with the render objects that
    // are displayed into this region.
    typedef HashMap<const RenderObject*, RefPtr<RenderStyle> > RenderObjectRegionStyleMap;
    RenderObjectRegionStyleMap m_renderObjectRegionStyle;

    bool m_isValid;
    bool m_hasCustomRegionStyle;
};

inline RenderRegion* toRenderRegion(RenderObject* object)
{
    ASSERT(!object || object->isRenderRegion());
    return static_cast<RenderRegion*>(object);
}

inline const RenderRegion* toRenderRegion(const RenderObject* object)
{
    ASSERT(!object || object->isRenderRegion());
    return static_cast<const RenderRegion*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderRegion(const RenderRegion*);

} // namespace WebCore

#endif // RenderRegion_h
