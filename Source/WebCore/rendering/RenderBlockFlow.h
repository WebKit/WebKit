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

#ifndef RenderBlockFlow_h
#define RenderBlockFlow_h

#include "FloatingObjects.h"
#include "RenderBlock.h"
#include "RenderLineBoxList.h"
#include "SimpleLineLayout.h"
#include "TrailingObjects.h"

namespace WebCore {

class LayoutStateMaintainer;
class LineBreaker;
class LineInfo;
class LineWidth;
class RenderMultiColumnFlowThread;
class RenderNamedFlowFragment;
class RenderRubyRun;

struct FloatWithRect;
struct WordMeasurement;

template <class Run> class BidiRunList;
typedef Vector<WordMeasurement, 64> WordMeasurements;

#if ENABLE(IOS_TEXT_AUTOSIZING)
enum LineCount {
    NOT_SET = 0, NO_LINE = 1, ONE_LINE = 2, MULTI_LINE = 3
};
#endif

class RenderBlockFlow : public RenderBlock {
public:
    RenderBlockFlow(Element&, PassRef<RenderStyle>);
    RenderBlockFlow(Document&, PassRef<RenderStyle>);
    virtual ~RenderBlockFlow();
        
    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

protected:
    virtual void insertedIntoTree() override;
    virtual void willBeDestroyed() override;
    
    // This method is called at the start of layout to wipe away all of the floats in our floating objects list. It also
    // repopulates the list with any floats that intrude from previous siblings or parents. Floats that were added by
    // descendants are gone when this call completes and will get added back later on after the children have gotten
    // a relayout.
    void rebuildFloatingObjectSetFromIntrudingFloats();

    // RenderBlockFlow always contains either lines or paragraphs. When the children are all blocks (e.g. paragraphs), we call layoutBlockChildren.
    // When the children are are all inline (e.g., lines), we call layoutInlineChildren.
    void layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom);
    void layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    // RenderBlockFlows override these methods, since they are the only class that supports margin collapsing.
    virtual LayoutUnit collapsedMarginBefore() const override final { return maxPositiveMarginBefore() - maxNegativeMarginBefore(); }
    virtual LayoutUnit collapsedMarginAfter() const override final { return maxPositiveMarginAfter() - maxNegativeMarginAfter(); }

    virtual void dirtyLinesFromChangedChild(RenderObject* child) override final { lineBoxes().dirtyLinesFromChangedChild(this, child); }
    virtual void updateLogicalHeight() override;

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
            , m_renderNamedFlowFragment(nullptr)
            , m_multiColumnFlowThread(nullptr)
            , m_discardMarginBefore(false)
            , m_discardMarginAfter(false)
            , m_didBreakAtLineToAvoidWidow(false)
        { 
        }

        ~RenderBlockFlowRareData()
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
        RenderNamedFlowFragment* m_renderNamedFlowFragment;

        RenderMultiColumnFlowThread* m_multiColumnFlowThread;
        
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
        MarginInfo(RenderBlockFlow&, LayoutUnit beforeBorderPadding, LayoutUnit afterBorderPadding);

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
    void updateStaticInlinePositionForChild(RenderBox& child, LayoutUnit logicalTop);

    LayoutUnit collapseMargins(RenderBox& child, MarginInfo&);
    LayoutUnit clearFloatsIfNeeded(RenderBox& child, MarginInfo&, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos);
    LayoutUnit estimateLogicalTopPosition(RenderBox& child, const MarginInfo&, LayoutUnit& estimateWithoutPagination);
    void marginBeforeEstimateForChild(RenderBox&, LayoutUnit&, LayoutUnit&, bool&) const;
    void handleAfterSideOfBlock(LayoutUnit top, LayoutUnit bottom, MarginInfo&);
    void setCollapsedBottomMargin(const MarginInfo&);

    bool shouldBreakAtLineToAvoidWidow() const { return hasRareBlockFlowData() && rareBlockFlowData()->m_lineBreakToAvoidWidow >= 0; }
    void clearShouldBreakAtLineToAvoidWidow() const;
    int lineBreakToAvoidWidow() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_lineBreakToAvoidWidow : -1; }
    void setBreakAtLineToAvoidWidow(int);
    void clearDidBreakAtLineToAvoidWidow();
    void setDidBreakAtLineToAvoidWidow();
    bool didBreakAtLineToAvoidWidow() const { return hasRareBlockFlowData() && rareBlockFlowData()->m_didBreakAtLineToAvoidWidow; }
    bool relayoutToAvoidWidows(LayoutStateMaintainer&);

    virtual bool canHaveGeneratedChildren() const override;

    RootInlineBox* lineGridBox() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_lineGridBox.get() : nullptr; }
    void setLineGridBox(std::unique_ptr<RootInlineBox> box)
    {
        ensureRareBlockFlowData().m_lineGridBox = std::move(box);
    }
    void layoutLineGridBox();

    virtual bool canCollapseAnonymousBlockChild() const override { return !renderNamedFlowFragment(); }
    RenderNamedFlowFragment* renderNamedFlowFragment() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_renderNamedFlowFragment : nullptr; }
    void setRenderNamedFlowFragment(RenderNamedFlowFragment*);

    RenderMultiColumnFlowThread* multiColumnFlowThread() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_multiColumnFlowThread : nullptr; }
    void setMultiColumnFlowThread(RenderMultiColumnFlowThread*);
    
    bool containsFloats() const override { return m_floatingObjects && !m_floatingObjects->set().isEmpty(); }
    bool containsFloat(RenderBox&) const;

    virtual void deleteLines() override;
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false) override;
    virtual VisiblePosition positionForPoint(const LayoutPoint&) override;

    void removeFloatingObjects();
    void markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove = nullptr, bool inLayout = true);
    void markSiblingsWithFloatsForLayout(RenderBox* floatToRemove = nullptr);

    LayoutUnit logicalTopForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->y() : floatingObject->x(); }
    LayoutUnit logicalBottomForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->maxY() : floatingObject->maxX(); }
    LayoutUnit logicalLeftForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->x() : floatingObject->y(); }
    LayoutUnit logicalRightForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->maxX() : floatingObject->maxY(); }
    LayoutUnit logicalWidthForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->width() : floatingObject->height(); }
    LayoutUnit logicalHeightForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->height() : floatingObject->width(); }
    LayoutSize logicalSizeForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? LayoutSize(floatingObject->width(), floatingObject->height()) : LayoutSize(floatingObject->height(), floatingObject->width()); }

    int pixelSnappedLogicalTopForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedY() : floatingObject->frameRect().pixelSnappedX(); }
    int pixelSnappedLogicalBottomForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedMaxY() : floatingObject->frameRect().pixelSnappedMaxX(); }
    int pixelSnappedLogicalLeftForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedX() : floatingObject->frameRect().pixelSnappedY(); }
    int pixelSnappedLogicalRightForFloat(const FloatingObject* floatingObject) const { return isHorizontalWritingMode() ? floatingObject->frameRect().pixelSnappedMaxX() : floatingObject->frameRect().pixelSnappedMaxY(); }

    void setLogicalTopForFloat(FloatingObject* floatingObject, LayoutUnit logicalTop)
    {
        if (isHorizontalWritingMode())
            floatingObject->setY(logicalTop);
        else
            floatingObject->setX(logicalTop);
    }
    void setLogicalLeftForFloat(FloatingObject* floatingObject, LayoutUnit logicalLeft)
    {
        if (isHorizontalWritingMode())
            floatingObject->setX(logicalLeft);
        else
            floatingObject->setY(logicalLeft);
    }
    void setLogicalHeightForFloat(FloatingObject* floatingObject, LayoutUnit logicalHeight)
    {
        if (isHorizontalWritingMode())
            floatingObject->setHeight(logicalHeight);
        else
            floatingObject->setWidth(logicalHeight);
    }
    void setLogicalWidthForFloat(FloatingObject* floatingObject, LayoutUnit logicalWidth)
    {
        if (isHorizontalWritingMode())
            floatingObject->setWidth(logicalWidth);
        else
            floatingObject->setHeight(logicalWidth);
    }

    LayoutUnit xPositionForFloatIncludingMargin(const FloatingObject* child) const { return isHorizontalWritingMode() ? child->x() + child->renderer().marginLeft() : child->x() + marginBeforeForChild(child->renderer()); }
    LayoutUnit yPositionForFloatIncludingMargin(const FloatingObject* child) const { return isHorizontalWritingMode() ? child->y() + marginBeforeForChild(child->renderer()) : child->y() + child->renderer().marginTop(); }

    LayoutPoint flipFloatForWritingModeForChild(const FloatingObject*, const LayoutPoint&) const;

    RenderLineBoxList& lineBoxes() { return m_lineBoxes; }
    const RenderLineBoxList& lineBoxes() const { return m_lineBoxes; }

    RootInlineBox* firstRootBox() const { return toRootInlineBox(m_lineBoxes.firstLineBox()); }
    RootInlineBox* lastRootBox() const { return toRootInlineBox(m_lineBoxes.lastLineBox()); }

    virtual bool hasLines() const override final;

    // Helper methods for computing line counts and heights for line counts.
    RootInlineBox* lineAtIndex(int) const;
    int lineCount(const RootInlineBox* = nullptr, bool* = nullptr) const;
    int heightForLineCount(int);
    void clearTruncation();

    void setHasMarkupTruncation(bool b) { m_hasMarkupTruncation = b; }
    bool hasMarkupTruncation() const { return m_hasMarkupTruncation; }

    bool containsNonZeroBidiLevel() const;

    const SimpleLineLayout::Layout* simpleLineLayout() const { return m_simpleLineLayout.get(); }
    void deleteLineBoxesBeforeSimpleLineLayout();
    void ensureLineBoxes();

#ifndef NDEBUG
    virtual void showLineTreeAndMark(const InlineBox* = nullptr, const char* = nullptr, const InlineBox* = nullptr, const char* = nullptr, const RenderObject* = nullptr) const override;
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
    bool hasNextPage(LayoutUnit logicalOffset, PageBoundaryRule = ExcludePageBoundary) const;

    void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;
    
    void createMultiColumnFlowThread();
    void destroyMultiColumnFlowThread();
    
    virtual void updateColumnProgressionFromStyle(RenderStyle*) override;

protected:
    // A page break is required at some offset due to space shortage in the current fragmentainer.
    void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage);

    // Update minimum page height required to avoid fragmentation where it shouldn't occur (inside
    // unbreakable content, between orphans and widows, etc.). This will be used as a hint to the
    // column balancer to help set a good minimum column height.
    void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight);
    bool pushToNextPageWithMinimumLogicalHeight(LayoutUnit& adjustment, LayoutUnit logicalOffset, LayoutUnit minimumLogicalHeight) const;

    // If the child is unsplittable and can't fit on the current page, return the top of the next page/column.
    LayoutUnit adjustForUnsplittableChild(RenderBox& child, LayoutUnit logicalOffset, bool includeMargins = false);
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

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void createFloatingObjects();

    virtual int firstLineBaseline() const override;
    virtual int inlineBlockBaseline(LineDirectionMode) const override;

    virtual bool isMultiColumnBlockFlow() const override { return multiColumnFlowThread(); }
    
    virtual void setComputedColumnCountAndWidth(int, LayoutUnit) override;

    virtual LayoutUnit computedColumnWidth() const override;
    virtual unsigned computedColumnCount() const override;
    
    bool isTopLayoutOverflowAllowed() const override;
    bool isLeftLayoutOverflowAllowed() const override;

    void moveFloatsTo(RenderBlockFlow* toBlock);

private:
    // Called to lay out the legend for a fieldset or the ruby text of a ruby run. Also used by multi-column layout to handle
    // the flow thread child.
    virtual RenderObject* layoutSpecialExcludedChild(bool /*relayoutChildren*/);

    void checkForPaginationLogicalHeightChange(LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged, bool& hasSpecifiedPageLogicalHeight);
    
    virtual void paintInlineChildren(PaintInfo&, const LayoutPoint&) override;
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool preservePhase = false) override;

    virtual void moveAllChildrenIncludingFloatsTo(RenderBlock* toBlock, bool fullRemoveInsert) override;
    virtual void repaintOverhangingFloats(bool paintAllDescendants) override final;
    virtual void clipOutFloatingObjects(RenderBlock&, const PaintInfo*, const LayoutPoint&, const LayoutSize&) override;

    FloatingObject* insertFloatingObject(RenderBox&);
    void removeFloatingObject(RenderBox&);
    void removeFloatingObjectsBelow(FloatingObject*, int logicalOffset);
    LayoutPoint computeLogicalLocationForFloat(const FloatingObject*, LayoutUnit logicalTopOffset) const;

    // Called from lineWidth, to position the floats added in the last line.
    // Returns true if and only if it has positioned any floats.
    bool positionNewFloats();

    void clearFloats(EClear);

    virtual LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const override;
    virtual LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const override;

    LayoutUnit logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const;
    LayoutUnit logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining) const;

    LayoutUnit lowestFloatLogicalBottom(FloatingObject::Type = FloatingObject::FloatLeftRight) const; 
    LayoutUnit nextFloatLogicalBottomBelow(LayoutUnit) const;
    LayoutUnit nextFloatLogicalBottomBelowForBlock(LayoutUnit) const;
    
    LayoutUnit addOverhangingFloats(RenderBlockFlow& child, bool makeChildPaintOtherFloats);
    bool hasOverhangingFloat(RenderBox&);
    void addIntrudingFloats(RenderBlockFlow* prev, LayoutUnit xoffset, LayoutUnit yoffset);
    bool hasOverhangingFloats() { return parent() && !hasColumns() && containsFloats() && lowestFloatLogicalBottom() > logicalHeight(); }
    LayoutUnit getClearDelta(RenderBox& child, LayoutUnit yPos);

    void determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    
    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset) override;
    virtual bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void addOverflowFromFloats();
    virtual void addOverflowFromInlineChildren();
    
    void fitBorderToLinesIfNeeded(); // Shrink the box in which the border paints if border-fit is set.
    void adjustForBorderFit(LayoutUnit x, LayoutUnit& left, LayoutUnit& right) const;

    void markLinesDirtyInBlockRange(LayoutUnit logicalTop, LayoutUnit logicalBottom, RootInlineBox* highest = 0);

    virtual GapRects inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches&, const PaintInfo*) override;
    
    Position positionForBox(InlineBox*, bool start = true) const;
    virtual VisiblePosition positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents) override;
    virtual void addFocusRingRectsForInlineChildren(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*) override;

// FIXME-BLOCKFLOW: These methods have implementations in
// RenderBlockLineLayout. They should be moved to the proper header once the
// line layout code is separated from RenderBlock and RenderBlockFlow.
// START METHODS DEFINED IN RenderBlockLineLayout
public:
    static void appendRunsForObject(BidiRunList<BidiRun>&, int start, int end, RenderObject*, InlineBidiResolver&);
    RootInlineBox* createAndAppendRootInlineBox();

    LayoutUnit startAlignedOffsetForLine(LayoutUnit position, bool shouldIndentText);
    virtual ETextAlign textAlignmentForLine(bool endsWithSoftBreak) const;
    virtual void adjustInlineDirectionLineBounds(int /* expansionOpportunityCount */, float& /* logicalLeft */, float& /* logicalWidth */) const { }

private:
    void layoutLineBoxes(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);
    void layoutSimpleLines(LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    virtual std::unique_ptr<RootInlineBox> createRootInlineBox(); // Subclassed by RenderSVGText.
    InlineFlowBox* createLineBoxes(RenderObject*, const LineInfo&, InlineBox* childBox, bool startsNewSegment);
    RootInlineBox* constructLine(BidiRunList<BidiRun>&, const LineInfo&);
    void setMarginsForRubyRun(BidiRun*, RenderRubyRun&, RenderObject*, const LineInfo&);
    void computeInlineDirectionPositionsForLine(RootInlineBox*, const LineInfo&, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&, WordMeasurements&);
    BidiRun* computeInlineDirectionPositionsForSegment(RootInlineBox*, const LineInfo&, ETextAlign, float& logicalLeft,
        float& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache&, WordMeasurements&);
    void computeBlockDirectionPositionsForLine(RootInlineBox*, BidiRun*, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    BidiRun* handleTrailingSpaces(BidiRunList<BidiRun>&, BidiContext*);
    void appendFloatingObjectToLastLine(FloatingObject*);
    // Helper function for layoutInlineChildren()
    RootInlineBox* createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>&, const InlineIterator& end, LineInfo&, VerticalPositionCache&, BidiRun* trailingSpaceRun, WordMeasurements&);
    void layoutRunsAndFloats(LineLayoutState&, bool hasInlineChild);
    const InlineIterator& restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver&,  const InlineIterator&);
    void layoutRunsAndFloatsInRange(LineLayoutState&, InlineBidiResolver&, const InlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus, unsigned consecutiveHyphenatedLines);
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    void updateShapeAndSegmentsForCurrentLine(ShapeInsideInfo*&, const LayoutSize&, LineLayoutState&);
    void updateShapeAndSegmentsForCurrentLineInFlowThread(ShapeInsideInfo*&, LineLayoutState&);
    bool adjustLogicalLineTopAndLogicalHeightIfNeeded(ShapeInsideInfo*, LayoutUnit, LineLayoutState&, InlineBidiResolver&, FloatingObject*, InlineIterator&, WordMeasurements&);
#endif
    void linkToEndLineIfNeeded(LineLayoutState&);
    static void repaintDirtyFloats(Vector<FloatWithRect>& floats);
    void checkFloatsInCleanLine(RootInlineBox*, Vector<FloatWithRect>&, size_t& floatIndex, bool& encounteredNewFloat, bool& dirtiedByFloat);
    RootInlineBox* determineStartPosition(LineLayoutState&, InlineBidiResolver&);
    void determineEndPosition(LineLayoutState&, RootInlineBox* startBox, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus);
    bool checkPaginationAndFloatsAtEndLine(LineLayoutState&);
    bool matchedEndLine(LineLayoutState&, const InlineBidiResolver&, const InlineIterator& endLineStart, const BidiStatus& endLineStatus);
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    // Positions new floats and also adjust all floats encountered on the line if any of them
    // have to move to the next page/column.
    bool positionNewFloatOnLine(FloatingObject* newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo&, LineWidth&);
    // This function is called to test a line box that has moved in the block direction to see if it has ended up in a new
    // region/page/column that has a different available line width than the old one. Used to know when you have to dirty a
    // line, i.e., that it can't be re-used.
    bool lineWidthForPaginatedLineChanged(RootInlineBox*, LayoutUnit lineDelta, RenderFlowThread*) const;
    void updateLogicalWidthForAlignment(const ETextAlign&, const RootInlineBox*, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount);

// END METHODS DEFINED IN RenderBlockLineLayout

    bool namedFlowFragmentNeedsUpdate() const;
    virtual bool canHaveChildren() const override;

#if ENABLE(IOS_TEXT_AUTOSIZING)
    int m_widthForTextAutosizing;
    unsigned m_lineCountForTextAutosizing : 2;
#endif

public:
    // FIXME-BLOCKFLOW: These can be made protected again once all callers have been moved here.
    void adjustLinePositionForPagination(RootInlineBox*, LayoutUnit& deltaOffset, RenderFlowThread*); // Computes a deltaOffset value that put a line at the top of the next page if it doesn't fit on the current page.
    void updateRegionForLine(RootInlineBox*) const;
    void createRenderNamedFlowFragmentIfNeeded();

    // Pagination routines.
    virtual bool relayoutForPagination(bool hasSpecifiedPageLogicalHeight, LayoutUnit pageLogicalHeight, LayoutStateMaintainer&);

    bool hasRareBlockFlowData() const { return m_rareBlockFlowData.get(); }
    RenderBlockFlowRareData* rareBlockFlowData() const { ASSERT_WITH_SECURITY_IMPLICATION(hasRareBlockFlowData()); return m_rareBlockFlowData.get(); }
    RenderBlockFlowRareData& ensureRareBlockFlowData();
    void materializeRareBlockFlowData();

#if ENABLE(IOS_TEXT_AUTOSIZING)
    int immediateLineCount();
    void adjustComputedFontSizes(float size, float visibleWidth);
    void resetComputedFontSize()
    {
        m_widthForTextAutosizing = -1;
        m_lineCountForTextAutosizing = NOT_SET;
    }
#endif

protected:
    OwnPtr<FloatingObjects> m_floatingObjects;
    std::unique_ptr<RenderBlockFlowRareData> m_rareBlockFlowData;
    RenderLineBoxList m_lineBoxes;
    std::unique_ptr<SimpleLineLayout::Layout> m_simpleLineLayout;

    friend class LineBreaker;
    friend class LineWidth; // Needs to know FloatingObject
};

RENDER_OBJECT_TYPE_CASTS(RenderBlockFlow, isRenderBlockFlow())

inline bool RenderElement::isRenderNamedFlowFragmentContainer() const
{
    return isRenderBlockFlow() && toRenderBlockFlow(this)->renderNamedFlowFragment();
}

} // namespace WebCore

#endif // RenderBlockFlow_h
