/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
class RenderNamedFlowThread;

class RenderRegion : public RenderReplaced {
public:
    explicit RenderRegion(Node*, RenderFlowThread*);

    virtual bool isRenderRegion() const { return true; }

    virtual void paintReplaced(PaintInfo&, const LayoutPoint&);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void setRegionRect(const LayoutRect& rect) { m_regionRect = rect; }
    LayoutRect regionRect() const { return m_regionRect; }
    LayoutRect regionOverflowRect() const;

    void attachRegion();
    void detachRegion();

    RenderNamedFlowThread* parentNamedFlowThread() const { return m_parentNamedFlowThread; }
    RenderFlowThread* flowThread() const { return m_flowThread; }

    // Valid regions do not create circular dependencies with other flows.
    bool isValid() const { return m_isValid; }
    void setIsValid(bool valid) { m_isValid = valid; }

    bool hasCustomRegionStyle() const { return m_hasCustomRegionStyle; }
    void setHasCustomRegionStyle(bool hasCustomRegionStyle) { m_hasCustomRegionStyle = hasCustomRegionStyle; }

    virtual void layout();

    RenderBoxRegionInfo* renderBoxRegionInfo(const RenderBox*) const;
    RenderBoxRegionInfo* setRenderBoxRegionInfo(const RenderBox*, LayoutUnit logicalLeftInset, LayoutUnit logicalRightInset,
        bool containingBlockChainIsInset);
    PassOwnPtr<RenderBoxRegionInfo> takeRenderBoxRegionInfo(const RenderBox*);
    void removeRenderBoxRegionInfo(const RenderBox*);

    void deleteAllRenderBoxRegionInfo();

    LayoutUnit offsetFromLogicalTopOfFirstPage() const;

    bool isFirstRegion() const;
    bool isLastRegion() const;

    void clearBoxStyleInRegion(const RenderBox*);

    enum RegionState {
        RegionUndefined,
        RegionEmpty,
        RegionFit,
        RegionOverflow
    };

    RegionState regionState() const { return isValid() ? m_regionState : RegionUndefined; }
    void setRegionState(RegionState regionState) { m_regionState = regionState; }
    void setDispatchRegionLayoutUpdateEvent(bool value) { m_dispatchRegionLayoutUpdateEvent = value; }
    bool shouldDispatchRegionLayoutUpdateEvent() { return m_dispatchRegionLayoutUpdateEvent; }
private:
    virtual const char* renderName() const { return "RenderRegion"; }

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

    PassRefPtr<RenderStyle> renderBoxRegionStyle(const RenderBox*);
    PassRefPtr<RenderStyle> computeStyleInRegion(const RenderBox*);
    void setRegionBoxesRegionStyle();
    void restoreRegionBoxesOriginalStyle();

    RenderFlowThread* m_flowThread;

    // If this RenderRegion is displayed as part of another named flow,
    // we need to create a dependency tree, so that layout of the
    // regions is always done before the regions themselves.
    RenderNamedFlowThread* m_parentNamedFlowThread;
    LayoutRect m_regionRect;

    // This map holds unique information about a block that is split across regions.
    // A RenderBoxRegionInfo* tells us about any layout information for a RenderBox that
    // is unique to the region. For now it just holds logical width information for RenderBlocks, but eventually
    // it will also hold a custom style for any box (for region styling).
    typedef HashMap<const RenderBox*, OwnPtr<RenderBoxRegionInfo> > RenderBoxRegionInfoMap;
    RenderBoxRegionInfoMap m_renderBoxRegionInfo;

    typedef HashMap<const RenderBox*, RefPtr<RenderStyle> > RenderBoxRegionStyleMap;
    RenderBoxRegionStyleMap m_renderBoxRegionStyle;

    bool m_isValid;
    bool m_hasCustomRegionStyle;
    RegionState m_regionState;
    bool m_dispatchRegionLayoutUpdateEvent;
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
