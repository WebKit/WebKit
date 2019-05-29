/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013,  Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "FloatingObjects.h"
#include "LineWidth.h"
#include "RenderBlock.h"
#include "RenderLineBoxList.h"
#include "SimpleLineLayout.h"
#include "TrailingObjects.h"
#include <memory>

namespace WebCore {

class FloatWithRect;
class LineBreaker;
class LineInfo;
class RenderMultiColumnFlow;
class RenderRubyRun;

struct WordMeasurement;

template <class Run> class BidiRunList;
typedef Vector<WordMeasurement, 64> WordMeasurements;

#if ENABLE(TEXT_AUTOSIZING)
enum LineCount {
    NOT_SET = 0, NO_LINE = 1, ONE_LINE = 2, MULTI_LINE = 3
};
#endif

class RenderBlockFlow : public RenderBlock {
    WTF_MAKE_ISO_ALLOCATED(RenderBlockFlow);
public:
    RenderBlockFlow(Element&, RenderStyle&&);
    RenderBlockFlow(Document&, RenderStyle&&);
    virtual ~RenderBlockFlow();
        
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;

protected:
    void willBeDestroyed() override;
    
    // This method is called at the start of layout to wipe away all of the floats in our floating objects list. It also
    // repopulates the list with any floats that intrude from previous siblings or parents. Floats that were added by
    // descendants are gone when this call completes and will get added back later on after the children have gotten
    // a relayout.
    void rebuildFloatingObjectSetFromIntrudingFloats();

    // RenderBlockFlow always contains either lines or paragraphs. When the children are all blocks (e.g. paragraphs), we call layoutBlockChildren.
    // When the children are all inline (e.g., lines), we call layoutInlineChildren.
    void layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom);
    void layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    // RenderBlockFlows override these methods, since they are the only class that supports margin collapsing.
    LayoutUnit collapsedMarginBefore() const final { return maxPositiveMarginBefore() - maxNegativeMarginBefore(); }
    LayoutUnit collapsedMarginAfter() const final { return maxPositiveMarginAfter() - maxNegativeMarginAfter(); }

    void dirtyLinesFromChangedChild(RenderObject& child) final { lineBoxes().dirtyLinesFromChangedChild(*this, child); }

    void paintColumnRules(PaintInfo&, const LayoutPoint&) override;

public:
    class MarginValues {
    public:
        MarginValues(LayoutUnit beforePos, LayoutUnit beforeNeg, LayoutUnit afterPos, LayoutUnit afterNeg)
            : m_positiveMarginBefore(beforePos)
            , m_negativeMarginBefore(beforeNeg)
            , m_positiveMarginAfter(afterPos)
            , m_negativeMarginAfter(afterNeg)
        {
        }
        
        LayoutUnit positiveMarginBefore() const { return m_positiveMarginBefore; }
        LayoutUnit negativeMarginBefore() const { return m_negativeMarginBefore; }
        LayoutUnit positiveMarginAfter() const { return m_positiveMarginAfter; }
        LayoutUnit negativeMarginAfter() const { return m_negativeMarginAfter; }
        
        void setPositiveMarginBefore(LayoutUnit pos) { m_positiveMarginBefore = pos; }
        void setNegativeMarginBefore(LayoutUnit neg) { m_negativeMarginBefore = neg; }
        void setPositiveMarginAfter(LayoutUnit pos) { m_positiveMarginAfter = pos; }
        void setNegativeMarginAfter(LayoutUnit neg) { m_negativeMarginAfter = neg; }
    
    private:
        LayoutUnit m_positiveMarginBefore;
        LayoutUnit m_negativeMarginBefore;
        LayoutUnit m_positiveMarginAfter;
        LayoutUnit m_negativeMarginAfter;
    };
    MarginValues marginValuesForChild(RenderBox& child) const;

    // Allocated only when some of these fields have non-default values
    struct RenderBlockFlowRareData {
        WTF_MAKE_NONCOPYABLE(RenderBlockFlowRareData); WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderBlockFlowRareData(const RenderBlockFlow& block)
            : m_margins(positiveMarginBeforeDefault(block), negativeMarginBeforeDefault(block), positiveMarginAfterDefault(block), negativeMarginAfterDefault(block))
            , m_lineBreakToAvoidWidow(-1)
            , m_discardMarginBefore(false)
            , m_discardMarginAfter(false)
            , m_didBreakAtLineToAvoidWidow(false)
        { 
        }

        static LayoutUnit positiveMarginBeforeDefault(const RenderBlock& block)
        { 
            return std::max<LayoutUnit>(block.marginBefore(), 0);
        }
        static LayoutUnit negativeMarginBeforeDefault(const RenderBlock& block)
        { 
            return std::max<LayoutUnit>(-block.marginBefore(), 0);
        }
        static LayoutUnit positiveMarginAfterDefault(const RenderBlock& block)
        {
            return std::max<LayoutUnit>(block.marginAfter(), 0);
        }
        static LayoutUnit negativeMarginAfterDefault(const RenderBlock& block)
        {
            return std::max<LayoutUnit>(-block.marginAfter(), 0);
        }
        
        MarginValues m_margins;
        int m_lineBreakToAvoidWidow;
        std::unique_ptr<RootInlineBox> m_lineGridBox;

        WeakPtr<RenderMultiColumnFlow> m_multiColumnFlow;
        
        bool m_discardMarginBefore : 1;
        bool m_discardMarginAfter : 1;
        bool m_didBreakAtLineToAvoidWidow : 1;
    };

    class MarginInfo {
        // Collapsing flags for whether we can collapse our margins with our children's margins.
        bool m_canCollapseWithChildren : 1;
        bool m_canCollapseMarginBeforeWithChildren : 1;
        bool m_canCollapseMarginAfterWithChildren : 1;

        // Whether or not we are a quirky container, i.e., do we collapse away top and bottom
        // margins in our container. Table cells and the body are the common examples. We
        // also have a custom style property for Safari RSS to deal with TypePad blog articles.
        bool m_quirkContainer : 1;

        // This flag tracks whether we are still looking at child margins that can all collapse together at the beginning of a block.  
        // They may or may not collapse with the top margin of the block (|m_canCollapseTopWithChildren| tells us that), but they will
        // always be collapsing with one another. This variable can remain set to true through multiple iterations 
        // as long as we keep encountering self-collapsing blocks.
        bool m_atBeforeSideOfBlock : 1;

        // This flag is set when we know we're examining bottom margins and we know we're at the bottom of the block.
        bool m_atAfterSideOfBlock : 1;

        // These variables are used to detect quirky margins that we need to collapse away (in table cells
        // and in the body element).
        bool m_hasMarginBeforeQuirk : 1;
        bool m_hasMarginAfterQuirk : 1;
        bool m_determinedMarginBeforeQuirk : 1;

        bool m_discardMargin : 1;

        // These flags track the previous maximal positive and negative margins.
        LayoutUnit m_positiveMargin;
        LayoutUnit m_negativeMargin;

    public:
        MarginInfo(const RenderBlockFlow&, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding);

        void setAtBeforeSideOfBlock(bool b) { m_atBeforeSideOfBlock = b; }
        void setAtAfterSideOfBlock(bool b) { m_atAfterSideOfBlock = b; }
        void clearMargin()
        {
            m_positiveMargin = 0;
            m_negativeMargin = 0;
        }
        void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
        void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }
        void setDeterminedMarginBeforeQuirk(bool b) { m_determinedMarginBeforeQuirk = b; }
        void setPositiveMargin(LayoutUnit p) { ASSERT(!m_discardMargin); m_positiveMargin = p; }
        void setNegativeMargin(LayoutUnit n) { ASSERT(!m_discardMargin); m_negativeMargin = n; }
        void setPositiveMarginIfLarger(LayoutUnit p)
        {
            ASSERT(!m_discardMargin);
            if (p > m_positiveMargin)
                m_positiveMargin = p;
        }
        void setNegativeMarginIfLarger(LayoutUnit n)
        {
            ASSERT(!m_discardMargin);
            if (n > m_negativeMargin)
                m_negativeMargin = n;
        }

        void setMargin(LayoutUnit p, LayoutUnit n) { ASSERT(!m_discardMargin); m_positiveMargin = p; m_negativeMargin = n; }
        void setCanCollapseMarginAfterWithChildren(bool collapse) { m_canCollapseMarginAfterWithChildren = collapse; }
        void setDiscardMargin(bool value) { m_discardMargin = value; }

        bool atBeforeSideOfBlock() const { return m_atBeforeSideOfBlock; }
        bool canCollapseWithMarginBefore() const { return m_atBeforeSideOfBlock && m_canCollapseMarginBeforeWithChildren; }
        bool canCollapseWithMarginAfter() const { return m_atAfterSideOfBlock && m_canCollapseMarginAfterWithChildren; }
        bool canCollapseMarginBeforeWithChildren() const { return m_canCollapseMarginBeforeWithChildren; }
        bool canCollapseMarginAfterWithChildren() const { return m_canCollapseMarginAfterWithChildren; }
        bool quirkContainer() const { return m_quirkContainer; }
        bool determinedMarginBeforeQuirk() const { return m_determinedMarginBeforeQuirk; }
        bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
        bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }
        LayoutUnit positiveMargin() const { return m_positiveMargin; }
        LayoutUnit negativeMargin() const { return m_negativeMargin; }
        bool discardMargin() const { return m_discardMargin; }
        LayoutUnit margin() const { return m_positiveMargin - m_negativeMargin; }
    };
    LayoutUnit marginOffsetForSelfCollapsingBlock();

    void layoutBlockChild(RenderBox& child, MarginInfo&, LayoutUnit& previousFloatLogicalBottom, LayoutUnit& maxFloatLogicalBottom);
    void adjustPositionedBlock(RenderBox& child, const MarginInfo&);
    void adjustFloatingBlock(const MarginInfo&);

    void setStaticInlinePositionForChild(RenderBox& child, LayoutUnit blockOffset, LayoutUnit inlinePosition);
    void updateStaticInlinePositionForChild(RenderBox& child, LayoutUnit logicalTop, IndentTextOrNot shouldIndentText);

    LayoutUnit collapseMargins(RenderBox& child, MarginInfo&);
    LayoutUnit collapseMarginsWithChildInfo(RenderBox* child, RenderObject* prevSibling, MarginInfo&);

    LayoutUnit clearFloatsIfNeeded(RenderBox& child, MarginInfo&, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos);
    LayoutUnit estimateLogicalTopPosition(RenderBox& child, const MarginInfo&, LayoutUnit& estimateWithoutPagination);
    void marginBeforeEstimateForChild(RenderBox&, LayoutUnit&, LayoutUnit&, bool&) const;
    void handleAfterSideOfBlock(LayoutUnit top, LayoutUnit bottom, MarginInfo&);
    void setCollapsedBottomMargin(const MarginInfo&);

    bool childrenPreventSelfCollapsing() const final;

    bool shouldBreakAtLineToAvoidWidow() const { return hasRareBlockFlowData() && rareBlockFlowData()->m_lineBreakToAvoidWidow >= 0; }
    void clearShouldBreakAtLineToAvoidWidow() const;
    int lineBreakToAvoidWidow() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_lineBreakToAvoidWidow : -1; }
    void setBreakAtLineToAvoidWidow(int);
    void clearDidBreakAtLineToAvoidWidow();
    void setDidBreakAtLineToAvoidWidow();
    bool didBreakAtLineToAvoidWidow() const { return hasRareBlockFlowData() && rareBlockFlowData()->m_didBreakAtLineToAvoidWidow; }
    bool relayoutToAvoidWidows();

    RootInlineBox* lineGridBox() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_lineGridBox.get() : nullptr; }
    void setLineGridBox(std::unique_ptr<RootInlineBox> box)
    {
        ensureRareBlockFlowData().m_lineGridBox = WTFMove(box);
    }
    void layoutLineGridBox();

    RenderMultiColumnFlow* multiColumnFlow() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_multiColumnFlow.get() : nullptr; }
    void setMultiColumnFlow(RenderMultiColumnFlow&);
    void clearMultiColumnFlow();
    bool willCreateColumns(Optional<unsigned> desiredColumnCount = WTF::nullopt) const;
    virtual bool requiresColumns(int) const;

    bool containsFloats() const override { return m_floatingObjects && !m_floatingObjects->set().isEmpty(); }
    bool containsFloat(RenderBox&) const;

    void deleteLines() override;
    void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false) override;
    Position positionForPoint(const LayoutPoint&) override;
    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) override;

    LayoutUnit lowestFloatLogicalBottom(FloatingObject::Type = FloatingObject::FloatLeftRight) const;

    void removeFloatingObjects();
    void markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove = nullptr, bool inLayout = true);
    void markSiblingsWithFloatsForLayout(RenderBox* floatToRemove = nullptr);

    const FloatingObjectSet* floatingObjectSet() const { return m_floatingObjects ? &m_floatingObjects->set() : nullptr; }

    LayoutUnit logicalTopForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.y() : floatingObject.x(); }
    LayoutUnit logicalBottomForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.maxY() : floatingObject.maxX(); }
    LayoutUnit logicalLeftForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.x() : floatingObject.y(); }
    LayoutUnit logicalRightForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.maxX() : floatingObject.maxY(); }
    LayoutUnit logicalWidthForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.width() : floatingObject.height(); }
    LayoutUnit logicalHeightForFloat(const FloatingObject& floatingObject) const { return isHorizontalWritingMode() ? floatingObject.height() : floatingObject.width(); }

    void setLogicalTopForFloat(FloatingObject& floatingObject, LayoutUnit logicalTop)
    {
        if (isHorizontalWritingMode())
            floatingObject.setY(logicalTop);
        else
            floatingObject.setX(logicalTop);
    }
    void setLogicalLeftForFloat(FloatingObject& floatingObject, LayoutUnit logicalLeft)
    {
        if (isHorizontalWritingMode())
            floatingObject.setX(logicalLeft);
        else
            floatingObject.setY(logicalLeft);
    }
    void setLogicalHeightForFloat(FloatingObject& floatingObject, LayoutUnit logicalHeight)
    {
        if (isHorizontalWritingMode())
            floatingObject.setHeight(logicalHeight);
        else
            floatingObject.setWidth(logicalHeight);
    }
    void setLogicalWidthForFloat(FloatingObject& floatingObject, LayoutUnit logicalWidth)
    {
        if (isHorizontalWritingMode())
            floatingObject.setWidth(logicalWidth);
        else
            floatingObject.setHeight(logicalWidth);
    }
    void setLogicalMarginsForFloat(FloatingObject& floatingObject, LayoutUnit logicalLeftMargin, LayoutUnit logicalBeforeMargin)
    {
        if (isHorizontalWritingMode())
            floatingObject.setMarginOffset(LayoutSize(logicalLeftMargin, logicalBeforeMargin));
        else
            floatingObject.setMarginOffset(LayoutSize(logicalBeforeMargin, logicalLeftMargin));
    }

    LayoutPoint flipFloatForWritingModeForChild(const FloatingObject&, const LayoutPoint&) const;

    RenderLineBoxList& lineBoxes() { return m_lineBoxes; }
    const RenderLineBoxList& lineBoxes() const { return m_lineBoxes; }

    RootInlineBox* firstRootBox() const { return downcast<RootInlineBox>(m_lineBoxes.firstLineBox()); }
    RootInlineBox* lastRootBox() const { return downcast<RootInlineBox>(m_lineBoxes.lastLineBox()); }

    bool hasLines() const;
    void invalidateLineLayoutPath() final;

    enum LineLayoutPath { UndeterminedPath = 0, SimpleLinesPath, LineBoxesPath, ForceLineBoxesPath };
    LineLayoutPath lineLayoutPath() const { return static_cast<LineLayoutPath>(renderBlockFlowLineLayoutPath()); }
    void setLineLayoutPath(LineLayoutPath path) { setRenderBlockFlowLineLayoutPath(path); }

    // Helper methods for computing line counts and heights for line counts.
    RootInlineBox* lineAtIndex(int) const;
    int lineCount(const RootInlineBox* = nullptr, bool* = nullptr) const;
    int heightForLineCount(int);
    void clearTruncation();

    void setHasMarkupTruncation(bool b) { setRenderBlockFlowHasMarkupTruncation(b); }
    bool hasMarkupTruncation() const { return renderBlockFlowHasMarkupTruncation(); }

    bool containsNonZeroBidiLevel() const;

    const SimpleLineLayout::Layout* simpleLineLayout() const;
    void deleteLineBoxesBeforeSimpleLineLayout();
    void ensureLineBoxes();
    void generateLineBoxTree();

#if ENABLE(TREE_DEBUGGING)
    void outputLineTreeAndMark(WTF::TextStream&, const InlineBox* markedBox, int depth) const;
#endif

    // Returns the logicalOffset at the top of the next page. If the offset passed in is already at the top of the current page,
    // then nextPageLogicalTop with ExcludePageBoundary will still move to the top of the next page. nextPageLogicalTop with
    // IncludePageBoundary set will not.
    //
    // For a page height of 800px, the first rule will return 800 if the value passed in is 0. The second rule will simply return 0.
    enum PageBoundaryRule { ExcludePageBoundary, IncludePageBoundary };
    LayoutUnit nextPageLogicalTop(LayoutUnit logicalOffset, PageBoundaryRule = ExcludePageBoundary) const;
    LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const;
    LayoutUnit pageLogicalHeightForOffset(LayoutUnit offset) const;
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule = IncludePageBoundary) const;
    LayoutUnit logicalHeightForChildForFragmentation(const RenderBox& child) const;
    bool hasNextPage(LayoutUnit logicalOffset, PageBoundaryRule = ExcludePageBoundary) const;

    void updateColumnProgressionFromStyle(RenderStyle&);
    void updateStylesForColumnChildren();

    bool needsLayoutAfterFragmentRangeChange() const override;
    WEBCORE_EXPORT RenderText* findClosestTextAtAbsolutePoint(const FloatPoint&);

    // A page break is required at some offset due to space shortage in the current fragmentainer.
    void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage);
    // Update minimum page height required to avoid fragmentation where it shouldn't occur (inside
    // unbreakable content, between orphans and widows, etc.). This will be used as a hint to the
    // column balancer to help set a good minimum column height.
    void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight);

    void addFloatsToNewParent(RenderBlockFlow& toBlockFlow) const;

protected:
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    
    bool pushToNextPageWithMinimumLogicalHeight(LayoutUnit& adjustment, LayoutUnit logicalOffset, LayoutUnit minimumLogicalHeight) const;

    // If the child is unsplittable and can't fit on the current page, return the top of the next page/column.
    LayoutUnit adjustForUnsplittableChild(RenderBox& child, LayoutUnit logicalOffset, LayoutUnit beforeMargin = 0_lu, LayoutUnit afterMargin = 0_lu);
    LayoutUnit adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox& child, bool atBeforeSideOfBlock);
    LayoutUnit applyBeforeBreak(RenderBox& child, LayoutUnit logicalOffset); // If the child has a before break, then return a new yPos that shifts to the top of the next page/column.
    LayoutUnit applyAfterBreak(RenderBox& child, LayoutUnit logicalOffset, MarginInfo&); // If the child has an after break, then return a new offset that shifts to the top of the next page/column.

    LayoutUnit maxPositiveMarginBefore() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_margins.positiveMarginBefore() : RenderBlockFlowRareData::positiveMarginBeforeDefault(*this); }
    LayoutUnit maxNegativeMarginBefore() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_margins.negativeMarginBefore() : RenderBlockFlowRareData::negativeMarginBeforeDefault(*this); }
    LayoutUnit maxPositiveMarginAfter() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_margins.positiveMarginAfter() : RenderBlockFlowRareData::positiveMarginAfterDefault(*this); }
    LayoutUnit maxNegativeMarginAfter() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_margins.negativeMarginAfter() : RenderBlockFlowRareData::negativeMarginAfterDefault(*this); }

    void initMaxMarginValues()
    {
        if (!hasRareBlockFlowData())
            return;

        rareBlockFlowData()->m_margins = MarginValues(RenderBlockFlowRareData::positiveMarginBeforeDefault(*this) , RenderBlockFlowRareData::negativeMarginBeforeDefault(*this),
            RenderBlockFlowRareData::positiveMarginAfterDefault(*this), RenderBlockFlowRareData::negativeMarginAfterDefault(*this));
        rareBlockFlowData()->m_discardMarginBefore = false;
        rareBlockFlowData()->m_discardMarginAfter = false;
    }

    void setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg);
    void setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg);

    void setMustDiscardMarginBefore(bool = true);
    void setMustDiscardMarginAfter(bool = true);

    bool mustDiscardMarginBefore() const;
    bool mustDiscardMarginAfter() const;

    bool mustDiscardMarginBeforeForChild(const RenderBox&) const;
    bool mustDiscardMarginAfterForChild(const RenderBox&) const;
    bool mustSeparateMarginBeforeForChild(const RenderBox&) const;
    bool mustSeparateMarginAfterForChild(const RenderBox&) const;

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void createFloatingObjects();

    Optional<int> firstLineBaseline() const override;
    Optional<int> inlineBlockBaseline(LineDirectionMode) const override;

    bool isMultiColumnBlockFlow() const override { return multiColumnFlow(); }
    
    void setComputedColumnCountAndWidth(int, LayoutUnit);

    LayoutUnit computedColumnWidth() const;
    unsigned computedColumnCount() const;
    
    bool isTopLayoutOverflowAllowed() const override;
    bool isLeftLayoutOverflowAllowed() const override;

    virtual void computeColumnCountAndWidth();

    virtual void cachePriorCharactersIfNeeded(const LazyLineBreakIterator&) {};
    
protected:
    // Called to lay out the legend for a fieldset or the ruby text of a ruby run. Also used by multi-column layout to handle
    // the flow thread child.
    void layoutExcludedChildren(bool relayoutChildren) override;
    void addOverflowFromFloats();

private:
    bool recomputeLogicalWidthAndColumnWidth();
    LayoutUnit columnGap() const;
    
    RenderBlockFlow* previousSiblingWithOverhangingFloats(bool& parentHasFloats) const;

    void checkForPaginationLogicalHeightChange(bool& relayoutChildren, LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged);

    void paintInlineChildren(PaintInfo&, const LayoutPoint&) override;
    void paintFloats(PaintInfo&, const LayoutPoint&, bool preservePhase = false) override;

    void repaintOverhangingFloats(bool paintAllDescendants) final;
    void clipOutFloatingObjects(RenderBlock&, const PaintInfo*, const LayoutPoint&, const LayoutSize&) override;

    FloatingObject* insertFloatingObject(RenderBox&);
    void removeFloatingObject(RenderBox&);
    void removeFloatingObjectsBelow(FloatingObject*, int logicalOffset);
    void computeLogicalLocationForFloat(FloatingObject&, LayoutUnit& logicalTopOffset);

    // Called from lineWidth, to position the floats added in the last line.
    // Returns true if and only if it has positioned any floats.
    bool positionNewFloats();

    void clearFloats(Clear);

    LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const override;
    LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const override;

    LayoutUnit logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const;
    LayoutUnit logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const;

    LayoutUnit lowestInitialLetterLogicalBottom() const;
    
    LayoutUnit nextFloatLogicalBottomBelow(LayoutUnit) const;
    LayoutUnit nextFloatLogicalBottomBelowForBlock(LayoutUnit) const;
    
    LayoutUnit addOverhangingFloats(RenderBlockFlow& child, bool makeChildPaintOtherFloats);
    bool hasOverhangingFloat(RenderBox&);
    void addIntrudingFloats(RenderBlockFlow* prev, RenderBlockFlow* container, LayoutUnit xoffset, LayoutUnit yoffset);
    bool hasOverhangingFloats() { return parent() && containsFloats() && lowestFloatLogicalBottom() > logicalHeight(); }
    LayoutUnit getClearDelta(RenderBox& child, LayoutUnit yPos);

    void determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    
    bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset) override;
    bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void addOverflowFromInlineChildren() override;
    
    void fitBorderToLinesIfNeeded(); // Shrink the box in which the border paints if border-fit is set.
    void adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const;

    void markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest = 0);

    GapRects inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches&, const PaintInfo*) override;
    
    Position positionForBox(InlineBox*, bool start = true) const;
    VisiblePosition positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents, const RenderFragmentContainer*) override;
    void addFocusRingRectsForInlineChildren(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*) override;

// FIXME-BLOCKFLOW: These methods have implementations in
// RenderBlockLineLayout. They should be moved to the proper header once the
// line layout code is separated from RenderBlock and RenderBlockFlow.
// START METHODS DEFINED IN RenderBlockLineLayout
public:
    static void appendRunsForObject(BidiRunList<BidiRun>*, int start, int end, RenderObject&, InlineBidiResolver&);
    RootInlineBox* createAndAppendRootInlineBox();

    LayoutUnit startAlignedOffsetForLine(LayoutUnit position, IndentTextOrNot shouldIndentText);
    virtual TextAlignMode textAlignmentForLine(bool endsWithSoftBreak) const;
    virtual void adjustInlineDirectionLineBounds(int /* expansionOpportunityCount */, float& /* logicalLeft */, float& /* logicalWidth */) const { }
    RootInlineBox* constructLine(BidiRunList<BidiRun>&, const LineInfo&);

private:        
    void adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    void layoutLineBoxes(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);
    void layoutSimpleLines(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    virtual std::unique_ptr<RootInlineBox> createRootInlineBox(); // Subclassed by RenderSVGText.
    InlineFlowBox* createLineBoxes(RenderObject*, const LineInfo&, InlineBox* childBox);
    void setMarginsForRubyRun(BidiRun*, RenderRubyRun&, RenderObject*, const LineInfo&);
    void computeInlineDirectionPositionsForLine(RootInlineBox*, const LineInfo&, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&, WordMeasurements&);
    void updateRubyForJustifiedText(RenderRubyRun&, BidiRun&, const Vector<unsigned, 16>& expansionOpportunities, unsigned& expansionOpportunityCount, float& totalLogicalWidth, float availableLogicalWidth, size_t& expansionIndex);
    void computeExpansionForJustifiedText(BidiRun* firstRun, BidiRun* trailingSpaceRun, const Vector<unsigned, 16>& expansionOpportunities, unsigned expansionOpportunityCount, float totalLogicalWidth, float availableLogicalWidth);
    BidiRun* computeInlineDirectionPositionsForSegment(RootInlineBox*, const LineInfo&, TextAlignMode, float& logicalLeft,
        float& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache&, WordMeasurements&);
    void computeBlockDirectionPositionsForLine(RootInlineBox*, BidiRun*, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    BidiRun* handleTrailingSpaces(BidiRunList<BidiRun>&, BidiContext*);
    void appendFloatingObjectToLastLine(FloatingObject&);
    // Helper function for layoutInlineChildren()
    RootInlineBox* createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>&, const InlineIterator& end, LineInfo&, VerticalPositionCache&, BidiRun* trailingSpaceRun, WordMeasurements&);
    void layoutRunsAndFloats(LineLayoutState&, bool hasInlineChild);
    const InlineIterator& restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver&,  const InlineIterator&);
    void layoutRunsAndFloatsInRange(LineLayoutState&, InlineBidiResolver&, const InlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus, unsigned consecutiveHyphenatedLines);
    void reattachCleanLineFloats(RootInlineBox& cleanLine, LayoutUnit delta, bool isFirstCleanLine);
    void linkToEndLineIfNeeded(LineLayoutState&);
    void checkFloatInCleanLine(RootInlineBox& cleanLine, RenderBox& floatBoxOnCleanLine, FloatWithRect& matchingFloatWithRect,
        bool& encounteredNewFloat, bool& dirtiedByFloat);
    RootInlineBox* determineStartPosition(LineLayoutState&, InlineBidiResolver&);
    void determineEndPosition(LineLayoutState&, RootInlineBox* startBox, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus);
    bool checkPaginationAndFloatsAtEndLine(LineLayoutState&);
    bool matchedEndLine(LineLayoutState&, const InlineBidiResolver&, const InlineIterator& endLineStart, const BidiStatus& endLineStatus);
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    // Positions new floats and also adjust all floats encountered on the line if any of them
    // have to move to the next page/column.
    bool positionNewFloatOnLine(const FloatingObject& newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo&, LineWidth&);
    // This function is called to test a line box that has moved in the block direction to see if it has ended up in a new
    // page/column that has a different available line width than the old one. Used to know when you have to dirty a
    // line, i.e., that it can't be re-used.
    bool lineWidthForPaginatedLineChanged(RootInlineBox*, LayoutUnit lineDelta, RenderFragmentedFlow*) const;
    void updateLogicalWidthForAlignment(const TextAlignMode&, const RootInlineBox*, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount);
// END METHODS DEFINED IN RenderBlockLineLayout

    void computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    void adjustInitialLetterPosition(RenderBox& childBox, LayoutUnit& logicalTopOffset, LayoutUnit& marginBeforeOffset);

#if ENABLE(TEXT_AUTOSIZING)
    int m_widthForTextAutosizing;
    unsigned m_lineCountForTextAutosizing : 2;
#endif
    void setSelectionState(SelectionState) final;

    void removeInlineBox(BidiRun&, const RootInlineBox&) const;

public:
    // FIXME-BLOCKFLOW: These can be made protected again once all callers have been moved here.
    void adjustLinePositionForPagination(RootInlineBox*, LayoutUnit& deltaOffset, bool& overflowsFragment, RenderFragmentedFlow*); // Computes a deltaOffset value that put a line at the top of the next page if it doesn't fit on the current page.
    void updateFragmentForLine(RootInlineBox*) const;

    // Pagination routines.
    bool relayoutForPagination();

    bool hasRareBlockFlowData() const { return m_rareBlockFlowData.get(); }
    RenderBlockFlowRareData* rareBlockFlowData() const { ASSERT_WITH_SECURITY_IMPLICATION(hasRareBlockFlowData()); return m_rareBlockFlowData.get(); }
    RenderBlockFlowRareData& ensureRareBlockFlowData();
    void materializeRareBlockFlowData();

#if ENABLE(TEXT_AUTOSIZING)
    int lineCountForTextAutosizing();
    void adjustComputedFontSizes(float size, float visibleWidth);
    void resetComputedFontSize()
    {
        m_widthForTextAutosizing = -1;
        m_lineCountForTextAutosizing = NOT_SET;
    }
#endif

protected:
    std::unique_ptr<FloatingObjects> m_floatingObjects;
    std::unique_ptr<RenderBlockFlowRareData> m_rareBlockFlowData;
    RenderLineBoxList m_lineBoxes;
    std::unique_ptr<SimpleLineLayout::Layout> m_simpleLineLayout;

    friend class LineBreaker;
    friend class LineWidth; // Needs to know FloatingObject
};

inline const SimpleLineLayout::Layout* RenderBlockFlow::simpleLineLayout() const
{
    ASSERT(lineLayoutPath() == SimpleLinesPath || !m_simpleLineLayout);
    return m_simpleLineLayout.get();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderBlockFlow, isRenderBlockFlow())
