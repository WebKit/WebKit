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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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

#ifndef RenderFlowThread_h
#define RenderFlowThread_h

#include "LayerFragment.h"
#include "RenderBlockFlow.h"
#include <wtf/HashCountedSet.h>
#include <wtf/ListHashSet.h>

namespace WebCore {

class CurrentRenderRegionMaintainer;
class RenderFlowThread;
class RenderNamedFlowFragment;
class RenderStyle;
class RenderRegion;
class RootInlineBox;

typedef ListHashSet<RenderRegion*> RenderRegionList;
typedef Vector<RenderLayer*> RenderLayerList;
typedef HashMap<RenderNamedFlowFragment*, RenderLayerList> RegionToLayerListMap;
typedef HashMap<RenderLayer*, RenderNamedFlowFragment*> LayerToRegionMap;
typedef HashMap<const RootInlineBox*, RenderRegion*> ContainingRegionMap;

// RenderFlowThread is used to collect all the render objects that participate in a
// flow thread. It will also help in doing the layout. However, it will not render
// directly to screen. Instead, RenderRegion objects will redirect their paint 
// and nodeAtPoint methods to this object. Each RenderRegion will actually be a viewPort
// of the RenderFlowThread.

class RenderFlowThread: public RenderBlockFlow {
public:
    virtual ~RenderFlowThread() { }

    virtual void removeFlowChildInfo(RenderObject*);
#ifndef NDEBUG
    bool hasChildInfo(RenderObject* child) const { return is<RenderBox>(child) && m_regionRangeMap.contains(downcast<RenderBox>(child)); }
#endif

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    bool checkLinesConsistency(const RenderBlockFlow*) const;
#endif
    
    void deleteLines() override;

    virtual void addRegionToThread(RenderRegion*) = 0;
    virtual void removeRegionFromThread(RenderRegion*);
    const RenderRegionList& renderRegionList() const { return m_regionList; }

    void updateLogicalWidth() final;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    bool hasRegions() const { return m_regionList.size(); }
    virtual void regionChangedWritingMode(RenderRegion*) { }

    void validateRegions();
    void invalidateRegions(MarkingBehavior = MarkContainingBlockChain);
    bool hasValidRegionInfo() const { return !m_regionsInvalidated && !m_regionList.isEmpty(); }

    // Some renderers (column spanners) are moved out of the flow thread to live among column
    // sets. If |child| is such a renderer, resolve it to the placeholder that lives at the original
    // location in the tree.
    virtual RenderObject* resolveMovedChild(RenderObject* child) const { return child; }
    // Called when a descendant of the flow thread has been inserted.
    virtual void flowThreadDescendantInserted(RenderObject*) { }
    // Called when a sibling or descendant of the flow thread is about to be removed.
    virtual void flowThreadRelativeWillBeRemoved(RenderObject*) { }
    // Called when a descendant box's layout is finished and it has been positioned within its container.
    virtual void flowThreadDescendantBoxLaidOut(RenderBox*) { }

    static RenderStyle createFlowThreadStyle(const RenderStyle* parentStyle);

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void repaintRectangleInRegions(const LayoutRect&) const;
    
    LayoutPoint adjustedPositionRelativeToOffsetParent(const RenderBoxModelObject&, const LayoutPoint&) const;

    LayoutUnit pageLogicalTopForOffset(LayoutUnit) const;
    LayoutUnit pageLogicalWidthForOffset(LayoutUnit) const;
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule = IncludePageBoundary) const;

    virtual void setPageBreak(const RenderBlock*, LayoutUnit /*offset*/, LayoutUnit /*spaceShortage*/) { }
    virtual void updateMinimumPageHeight(const RenderBlock*, LayoutUnit /*offset*/, LayoutUnit /*minHeight*/) { }

    virtual RenderRegion* regionAtBlockOffset(const RenderBox*, LayoutUnit, bool extendLastRegion = false) const;

    bool regionsHaveUniformLogicalWidth() const { return m_regionsHaveUniformLogicalWidth; }
    bool regionsHaveUniformLogicalHeight() const { return m_regionsHaveUniformLogicalHeight; }

    virtual RenderRegion* mapFromFlowToRegion(TransformState&) const;

    void logicalWidthChangedInRegionsForBlock(const RenderBlock*, bool&);

    LayoutUnit contentLogicalWidthOfFirstRegion() const;
    LayoutUnit contentLogicalHeightOfFirstRegion() const;
    LayoutUnit contentLogicalLeftOfFirstRegion() const;
    
    RenderRegion* firstRegion() const;
    RenderRegion* lastRegion() const;

    bool previousRegionCountChanged() const { return m_previousRegionCount != m_regionList.size(); };
    void updatePreviousRegionCount() { m_previousRegionCount = m_regionList.size(); };

    virtual void setRegionRangeForBox(const RenderBox*, RenderRegion*, RenderRegion*);
    bool getRegionRangeForBox(const RenderBox*, RenderRegion*& startRegion, RenderRegion*& endRegion) const;
    bool computedRegionRangeForBox(const RenderBox*, RenderRegion*& startRegion, RenderRegion*& endRegion) const;
    bool hasCachedRegionRangeForBox(const RenderBox*) const;

    // Check if the object is in region and the region is part of this flow thread.
    bool objectInFlowRegion(const RenderObject*, const RenderRegion*) const;
    
    // Check if the object should be painted in this region and if the region is part of this flow thread.
    bool objectShouldFragmentInFlowRegion(const RenderObject*, const RenderRegion*) const;

    void markAutoLogicalHeightRegionsForLayout();
    void markRegionsForOverflowLayoutIfNeeded();

    virtual bool addForcedRegionBreak(const RenderBlock*, LayoutUnit, RenderBox* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0);
    virtual void applyBreakAfterContent(LayoutUnit) { }

    virtual bool isPageLogicalHeightKnown() const { return true; }
    bool pageLogicalSizeChanged() const { return m_pageLogicalSizeChanged; }

    bool hasAutoLogicalHeightRegions() const { ASSERT(isAutoLogicalHeightRegionsCountConsistent()); return m_autoLogicalHeightRegionsCount; }
    void incrementAutoLogicalHeightRegions();
    void decrementAutoLogicalHeightRegions();

#ifndef NDEBUG
    bool isAutoLogicalHeightRegionsCountConsistent() const;
#endif

    void collectLayerFragments(LayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect);
    LayoutRect fragmentsBoundingBox(const LayoutRect& layerBoundingBox);

    // A flow thread goes through different states during layout.
    enum LayoutPhase {
        LayoutPhaseMeasureContent = 0, // The initial phase, used to measure content for the auto-height regions.
        LayoutPhaseConstrained, // In this phase the regions are laid out using the sizes computed in the normal phase.
        LayoutPhaseOverflow, // In this phase the layout overflow is propagated from the content to the regions.
        LayoutPhaseFinal // In case scrollbars have resized the regions, content is laid out one last time to respect the change.
    };
    bool inMeasureContentLayoutPhase() const { return m_layoutPhase == LayoutPhaseMeasureContent; }
    bool inConstrainedLayoutPhase() const { return m_layoutPhase == LayoutPhaseConstrained; }
    bool inOverflowLayoutPhase() const { return m_layoutPhase == LayoutPhaseOverflow; }
    bool inFinalLayoutPhase() const { return m_layoutPhase == LayoutPhaseFinal; }
    void setLayoutPhase(LayoutPhase phase) { m_layoutPhase = phase; }

    bool needsTwoPhasesLayout() const { return m_needsTwoPhasesLayout; }
    void clearNeedsTwoPhasesLayout() { m_needsTwoPhasesLayout = false; }

    // Whether any of the regions has a compositing descendant.
    bool hasCompositingRegionDescendant() const;

    void setNeedsLayerToRegionMappingsUpdate() { m_layersToRegionMappingsDirty = true; }
    void updateAllLayerToRegionMappingsIfNeeded()
    {
        if (m_layersToRegionMappingsDirty)
            updateAllLayerToRegionMappings();
    }

    const RenderLayerList* getLayerListForRegion(RenderNamedFlowFragment*) const;

    RenderNamedFlowFragment* regionForCompositedLayer(RenderLayer&) const; // By means of getRegionRangeForBox or regionAtBlockOffset.
    RenderNamedFlowFragment* cachedRegionForCompositedLayer(RenderLayer&) const;

    virtual bool collectsGraphicsLayersUnderRegions() const;

    void pushFlowThreadLayoutState(const RenderObject&);
    void popFlowThreadLayoutState();
    LayoutUnit offsetFromLogicalTopOfFirstRegion(const RenderBlock*) const;
    void clearRenderBoxRegionInfoAndCustomStyle(const RenderBox*, const RenderRegion*, const RenderRegion*, const RenderRegion*, const RenderRegion*);

    void addRegionsVisualEffectOverflow(const RenderBox*);
    void addRegionsVisualOverflowFromTheme(const RenderBlock*);
    void addRegionsOverflowFromChild(const RenderBox*, const RenderBox*, const LayoutSize&);
    void addRegionsLayoutOverflow(const RenderBox*, const LayoutRect&);
    void addRegionsVisualOverflow(const RenderBox*, const LayoutRect&);
    void clearRegionsOverflow(const RenderBox*);

    LayoutRect mapFromFlowThreadToLocal(const RenderBox*, const LayoutRect&) const;
    LayoutRect mapFromLocalToFlowThread(const RenderBox*, const LayoutRect&) const;

    void flipForWritingModeLocalCoordinates(LayoutRect&) const;

    // Used to estimate the maximum height of the flow thread.
    static LayoutUnit maxLogicalHeight() { return LayoutUnit::max() / 2; }

    bool regionInRange(const RenderRegion* targetRegion, const RenderRegion* startRegion, const RenderRegion* endRegion) const;

    virtual bool absoluteQuadsForBox(Vector<FloatQuad>&, bool*, const RenderBox*, float, float) const { return false; }

    void layout() override;

    void setCurrentRegionMaintainer(CurrentRenderRegionMaintainer* currentRegionMaintainer) { m_currentRegionMaintainer = currentRegionMaintainer; }
    RenderRegion* currentRegion() const;

    ContainingRegionMap& containingRegionMap();

    bool cachedFlowThreadContainingBlockNeedsUpdate() const override { return false; }

    // FIXME: Eventually as column and region flow threads start nesting, this may end up changing.
    virtual bool shouldCheckColumnBreaks() const { return false; }

private:
    // Always create a RenderLayer for the RenderFlowThread so that we
    // can easily avoid drawing the children directly.
    bool requiresLayer() const final { return true; }

protected:
    RenderFlowThread(Document&, RenderStyle&&);

    RenderFlowThread* locateFlowThreadContainingBlock() const override { return const_cast<RenderFlowThread*>(this); }

    const char* renderName() const override = 0;

    // Overridden by columns/pages to set up an initial logical width of the page width even when
    // no regions have been generated yet.
    virtual LayoutUnit initialLogicalWidth() const { return 0; };
    
    void clearLinesToRegionMap();
    void willBeDestroyed() override;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;

    void updateRegionsFlowThreadPortionRect(const RenderRegion* = nullptr);
    bool shouldRepaint(const LayoutRect&) const;

    bool updateAllLayerToRegionMappings();

    // Triggers a layers' update if a layer has moved from a region to another since the last update.
    void updateLayerToRegionMappings(RenderLayer&, LayerToRegionMap&, RegionToLayerListMap&, bool& needsLayerUpdate);
    void updateRegionForRenderLayer(RenderLayer*, LayerToRegionMap&, RegionToLayerListMap&, bool& needsLayerUpdate);

    void initializeRegionsComputedAutoHeight(RenderRegion* = nullptr);

    inline bool hasCachedOffsetFromLogicalTopOfFirstRegion(const RenderBox*) const;
    inline LayoutUnit cachedOffsetFromLogicalTopOfFirstRegion(const RenderBox*) const;
    inline void setOffsetFromLogicalTopOfFirstRegion(const RenderBox*, LayoutUnit);
    inline void clearOffsetFromLogicalTopOfFirstRegion(const RenderBox*);

    inline const RenderBox* currentActiveRenderBox() const;

    bool getRegionRangeForBoxFromCachedInfo(const RenderBox*, RenderRegion*& startRegion, RenderRegion*& endRegion) const;

    void removeRenderBoxRegionInfo(RenderBox*);
    void removeLineRegionInfo(const RenderBlockFlow*);

    RenderRegionList m_regionList;
    unsigned short m_previousRegionCount;

    class RenderRegionRange {
    public:
        RenderRegionRange()
        {
            setRange(nullptr, nullptr);
        }

        RenderRegionRange(RenderRegion* start, RenderRegion* end)
        {
            setRange(start, end);
        }
        
        void setRange(RenderRegion* start, RenderRegion* end)
        {
            m_startRegion = start;
            m_endRegion = end;
            m_rangeInvalidated = true;
        }

        RenderRegion* startRegion() const { return m_startRegion; }
        RenderRegion* endRegion() const { return m_endRegion; }
        bool rangeInvalidated() const { return m_rangeInvalidated; }
        void clearRangeInvalidated() { m_rangeInvalidated = false; }

    private:
        RenderRegion* m_startRegion;
        RenderRegion* m_endRegion;
        bool m_rangeInvalidated;
    };

    typedef PODInterval<LayoutUnit, RenderRegion*> RegionInterval;
    typedef PODIntervalTree<LayoutUnit, RenderRegion*> RegionIntervalTree;

    class RegionSearchAdapter {
    public:
        RegionSearchAdapter(LayoutUnit offset)
            : m_offset(offset)
            , m_result(nullptr)
        {
        }
        
        const LayoutUnit& lowValue() const { return m_offset; }
        const LayoutUnit& highValue() const { return m_offset; }
        void collectIfNeeded(const RegionInterval&);

        RenderRegion* result() const { return m_result; }

    private:
        LayoutUnit m_offset;
        RenderRegion* m_result;
    };

    // Map a layer to the region in which the layer is painted.
    std::unique_ptr<LayerToRegionMap> m_layerToRegionMap;

    // Map a region to the list of layers that paint in that region.
    std::unique_ptr<RegionToLayerListMap> m_regionToLayerListMap;

    // Map a line to its containing region.
    std::unique_ptr<ContainingRegionMap> m_lineToRegionMap;

    // Map a box to the list of regions in which the box is rendered.
    typedef HashMap<const RenderBox*, RenderRegionRange> RenderRegionRangeMap;
    RenderRegionRangeMap m_regionRangeMap;

    // Map a box with a region break to the auto height region affected by that break. 
    typedef HashMap<RenderBox*, RenderRegion*> RenderBoxToRegionMap;
    RenderBoxToRegionMap m_breakBeforeToRegionMap;
    RenderBoxToRegionMap m_breakAfterToRegionMap;

    typedef Vector<const RenderObject*> RenderObjectStack;
    RenderObjectStack m_activeObjectsStack;

    typedef HashMap<const RenderBox*, LayoutUnit> RenderBoxToOffsetMap;
    RenderBoxToOffsetMap m_boxesToOffsetMap;

    unsigned m_autoLogicalHeightRegionsCount;

    RegionIntervalTree m_regionIntervalTree;

    CurrentRenderRegionMaintainer* m_currentRegionMaintainer;

    bool m_regionsInvalidated : 1;
    bool m_regionsHaveUniformLogicalWidth : 1;
    bool m_regionsHaveUniformLogicalHeight : 1;
    bool m_pageLogicalSizeChanged : 1;
    unsigned m_layoutPhase : 2;
    bool m_needsTwoPhasesLayout : 1;
    bool m_layersToRegionMappingsDirty : 1;
};

// This structure is used by PODIntervalTree for debugging.
#ifndef NDEBUG
template <> struct ValueToString<RenderRegion*> {
    static String string(const RenderRegion* value) { return String::format("%p", value); }
};
#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlowThread, isRenderFlowThread())

#endif // RenderFlowThread_h
