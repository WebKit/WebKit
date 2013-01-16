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
#include "StyleInheritedData.h"

namespace WebCore {

class RenderBox;
class RenderBoxRegionInfo;
class RenderFlowThread;
class RenderNamedFlowThread;

class RenderRegion : public RenderReplaced {
public:
    explicit RenderRegion(ContainerNode*, RenderFlowThread*);

    virtual bool isRenderRegion() const { return true; }

    virtual void paintReplaced(PaintInfo&, const LayoutPoint&);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    void setFlowThreadPortionRect(const LayoutRect& rect) { m_flowThreadPortionRect = rect; }
    LayoutRect flowThreadPortionRect() const { return m_flowThreadPortionRect; }
    LayoutRect flowThreadPortionOverflowRect() const;

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

    bool isFirstRegion() const;
    bool isLastRegion() const;

    void clearObjectStyleInRegion(const RenderObject*);

    enum RegionState {
        RegionUndefined,
        RegionEmpty,
        RegionFit,
        RegionOverset
    };

    RegionState regionState() const { return isValid() ? m_regionState : RegionUndefined; }
    void setRegionState(RegionState regionState) { m_regionState = regionState; }
    
    // These methods represent the width and height of a "page" and for a RenderRegion they are just the
    // content width and content height of a region. For RenderRegionSets, however, they will be the width and
    // height of a single column or page in the set.
    virtual LayoutUnit pageLogicalWidth() const;
    virtual LayoutUnit pageLogicalHeight() const;

    virtual LayoutUnit minPreferredLogicalWidth() const OVERRIDE;
    virtual LayoutUnit maxPreferredLogicalWidth() const OVERRIDE;
    
    LayoutUnit logicalTopOfFlowThreadContentRect(const LayoutRect&) const;
    LayoutUnit logicalBottomOfFlowThreadContentRect(const LayoutRect&) const;
    LayoutUnit logicalTopForFlowThreadContent() const { return logicalTopOfFlowThreadContentRect(flowThreadPortionRect()); };
    LayoutUnit logicalBottomForFlowThreadContent() const { return logicalBottomOfFlowThreadContentRect(flowThreadPortionRect()); };

    void getRanges(Vector<RefPtr<Range> >&) const;

    // This method represents the logical height of the entire flow thread portion used by the region or set.
    // For RenderRegions it matches logicalPaginationHeight(), but for sets it is the height of all the pages
    // or columns added together.
    virtual LayoutUnit logicalHeightOfAllFlowThreadContent() const;

    bool hasAutoLogicalHeight() const { return m_hasAutoLogicalHeight; }

    bool needsOverrideLogicalContentHeightComputation() const;

    virtual void updateLogicalHeight() OVERRIDE;

    // The top of the nearest page inside the region. For RenderRegions, this is just the logical top of the
    // flow thread portion we contain. For sets, we have to figure out the top of the nearest column or
    // page.
    virtual LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const;
    
    virtual void expandToEncompassFlowThreadContentsIfNeeded() { };

    // Whether or not this region is a set.
    virtual bool isRenderRegionSet() const { return false; }
    
    virtual void repaintFlowThreadContent(const LayoutRect& repaintRect, bool immediate) const;

protected:
    void setRegionObjectsRegionStyle();
    void restoreRegionObjectsOriginalStyle();

    LayoutRect overflowRectForFlowThreadPortion(LayoutRect flowThreadPortionRect, bool isFirstPortion, bool isLastPortion) const;
    void repaintFlowThreadContentRectangle(const LayoutRect& repaintRect, bool immediate, const LayoutRect& flowThreadPortionRect,
        const LayoutRect& flowThreadPortionOverflowRect, const LayoutPoint& regionLocation) const;

private:
    virtual const char* renderName() const { return "RenderRegion"; }

    // FIXME: these functions should be revisited once RenderRegion inherits from RenderBlock
    // instead of RenderReplaced (see https://bugs.webkit.org/show_bug.cgi?id=74132 )
    // When width is auto, use normal block/box sizing code except when inline.
    virtual bool isInlineBlockOrInlineTable() const OVERRIDE { return isInline() && !shouldComputeSizeAsReplaced(); }
    virtual bool shouldComputeSizeAsReplaced() const OVERRIDE { return !style()->logicalWidth().isAuto() && !style()->logicalHeight().isAuto(); }

    bool shouldHaveAutoLogicalHeight() const
    {
        bool hasSpecifiedEndpointsForHeight = style()->logicalTop().isSpecified() && style()->logicalBottom().isSpecified();
        bool hasAnchoredEndpointsForHeight = isOutOfFlowPositioned() && hasSpecifiedEndpointsForHeight;
        return style()->logicalHeight().isAuto() && !hasAnchoredEndpointsForHeight;
    }

    virtual void insertedIntoTree() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;

    virtual void installFlowThread();

    PassRefPtr<RenderStyle> computeStyleInRegion(const RenderObject*);
    void computeChildrenStyleInRegion(const RenderObject*);
    void setObjectStyleInRegion(RenderObject*, PassRefPtr<RenderStyle>, bool objectRegionStyleCached);
    void printRegionObjectsStyles();

    void checkRegionStyle();
    void updateRegionHasAutoLogicalHeightFlag();

protected:
    RenderFlowThread* m_flowThread;

private:
    // If this RenderRegion is displayed as part of another named flow,
    // we need to create a dependency tree, so that layout of the
    // regions is always done before the regions themselves.
    RenderNamedFlowThread* m_parentNamedFlowThread;
    LayoutRect m_flowThreadPortionRect;

    // This map holds unique information about a block that is split across regions.
    // A RenderBoxRegionInfo* tells us about any layout information for a RenderBox that
    // is unique to the region. For now it just holds logical width information for RenderBlocks, but eventually
    // it will also hold a custom style for any box (for region styling).
    typedef HashMap<const RenderBox*, OwnPtr<RenderBoxRegionInfo> > RenderBoxRegionInfoMap;
    RenderBoxRegionInfoMap m_renderBoxRegionInfo;

    struct ObjectRegionStyleInfo {
        // Used to store the original style of the object in region
        // so that the original style is properly restored after paint.
        // Also used to store computed style of the object in region between
        // region paintings, so that the style in region is computed only
        // when necessary.
        RefPtr<RenderStyle> style;
        // True if the computed style in region is cached.
        bool cached;
    };
    typedef HashMap<const RenderObject*, ObjectRegionStyleInfo > RenderObjectRegionStyleMap;
    RenderObjectRegionStyleMap m_renderObjectRegionStyle;

    bool m_isValid : 1;
    bool m_hasCustomRegionStyle : 1;
    bool m_hasAutoLogicalHeight : 1;
    RegionState m_regionState;
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
