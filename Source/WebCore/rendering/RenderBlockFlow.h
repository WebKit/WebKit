/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/FloatingObjects.h>
#include <WebCore/LegacyLineLayout.h>
#include <WebCore/LineWidth.h>
#include <WebCore/RenderBlock.h>
#include <memory>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
struct RenderBlockFlowRareData;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::RenderBlockFlowRareData> : std::true_type { };
}

namespace WebCore {

class FloatingObjects;
class LineBreaker;
class RenderMultiColumnFlow;

namespace LayoutIntegration {
class LineLayout;
}

namespace InlineIterator {
class LineBoxIterator;
}

#if ENABLE(TEXT_AUTOSIZING)
enum LineCount {
    NOT_SET = 0, NO_LINE = 1, ONE_LINE = 2, MULTI_LINE = 3
};
#endif

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

// Allocated only when some of these fields have non-default values
struct RenderBlockFlowRareData {
    WTF_MAKE_TZONE_ALLOCATED(RenderBlockFlowRareData);
    WTF_MAKE_NONCOPYABLE(RenderBlockFlowRareData);
public:
    RenderBlockFlowRareData(const RenderBlockFlow&);
    ~RenderBlockFlowRareData();

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
    LayoutUnit m_alignContentShift; // Caches negative shifts for overflow calculation.

    SingleThreadWeakPtr<RenderMultiColumnFlow> m_multiColumnFlow;

    bool m_didBreakAtLineToAvoidWidow : 1;
};

class RenderBlockFlow : public RenderBlock {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderBlockFlow);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderBlockFlow);
public:
    RenderBlockFlow(Type, Element&, RenderStyle&&, OptionSet<BlockFlowFlag> = { });
    RenderBlockFlow(Type, Document&, RenderStyle&&, OptionSet<BlockFlowFlag> = { });
    virtual ~RenderBlockFlow();
        
    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;

protected:
    void willBeDestroyed() override;

    void layoutBlockWithNoChildren();

    // This method is called at the start of layout to wipe away all of the floats in our floating objects list. It also
    // repopulates the list with any floats that intrude from previous siblings or parents. Floats that were added by
    // descendants are gone when this call completes and will get added back later on after the children have gotten
    // a relayout.
    void rebuildFloatingObjectSetFromIntrudingFloats();

    // RenderBlockFlow always contains either lines or paragraphs. When the children are all blocks (e.g. paragraphs), we call layoutBlockChildren.
    // When the children are all inline (e.g., lines), we call layoutInlineChildren.
    void layoutInFlowChildren(RelayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom, LayoutUnit& maxFloatLogicalBottom);
    void layoutBlockChildren(RelayoutChildren, LayoutUnit& maxFloatLogicalBottom);
    void layoutInlineChildren(RelayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    void simplifiedNormalFlowLayout() override;
    LayoutUnit shiftForAlignContent(LayoutUnit intrinsicLogicalHeight, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    void computeOverflow(LayoutUnit oldClientAfterEdge, OptionSet<ComputeOverflowOptions> = { }) override;
    void addOverflowFromInFlowChildren(OptionSet<ComputeOverflowOptions> = { }) override;

    // RenderBlockFlows override these methods, since they are the only class that supports margin collapsing.
    LayoutUnit collapsedMarginBefore() const final { return maxPositiveMarginBefore() - maxNegativeMarginBefore(); }
    LayoutUnit collapsedMarginAfter() const final { return maxPositiveMarginAfter() - maxNegativeMarginAfter(); }

    void dirtyLineFromChangedChild() final
    {
        if (svgTextLayout() && svgTextLayout()->legacyRootBox())
            svgTextLayout()->legacyRootBox()->markDirty();
    }

    void paintColumnRules(PaintInfo&, const LayoutPoint&) override;

public:
    MarginValues marginValuesForChild(RenderBox& child) const;

    class MarginInfo {
    public:
        enum class IgnoreScrollbarForAfterMargin : bool { No, Yes };
        MarginInfo(const RenderBlockFlow&, IgnoreScrollbarForAfterMargin = IgnoreScrollbarForAfterMargin::Yes);
        MarginInfo(bool canCollapseWithChildren, bool canCollapseMarginBeforeWithChildren, bool canCollapseMarginAfterWithChildren, bool quirkContainer, bool atBeforeSideOfBlock, bool atAfterSideOfBlock,  bool hasMarginBeforeQuirk, bool hasMarginAfterQuirk, bool determinedMarginBeforeQuirk, LayoutUnit positiveMargin, LayoutUnit negativeMargin);
        MarginInfo() = default;

        void setAtBeforeSideOfBlock(bool atBeforeSideOfBlock) { m_atBeforeSideOfBlock = atBeforeSideOfBlock; }
        void setAtAfterSideOfBlock(bool atAfterSideOfBlock) { m_atAfterSideOfBlock = atAfterSideOfBlock; }
        void clearMargin()
        {
            m_positiveMargin = { };
            m_negativeMargin = { };
        }
        void setHasMarginBeforeQuirk(bool hasMarginBeforeQuirk) { m_hasMarginBeforeQuirk = hasMarginBeforeQuirk; }
        void setHasMarginAfterQuirk(bool hasMarginAfterQuirk) { m_hasMarginAfterQuirk = hasMarginAfterQuirk; }
        void setDeterminedMarginBeforeQuirk(bool determinedMarginBeforeQuirk) { m_determinedMarginBeforeQuirk = determinedMarginBeforeQuirk; }
        void setPositiveMargin(LayoutUnit positiveMargin) { m_positiveMargin = positiveMargin; }
        void setNegativeMargin(LayoutUnit negativeMargin) { m_negativeMargin = negativeMargin; }
        void setPositiveMarginIfLarger(LayoutUnit positiveMargin) { m_positiveMargin = std::max(positiveMargin, m_positiveMargin); }
        void setNegativeMarginIfLarger(LayoutUnit negativeMargin) { m_negativeMargin = std::max(negativeMargin, m_negativeMargin); }
        void setMargin(LayoutUnit positiveMargin, LayoutUnit negativeMargin)
        {
            setPositiveMargin(positiveMargin);
            setNegativeMargin(negativeMargin);
        }
        void setCanCollapseMarginAfterWithChildren(bool canCollapse) { m_canCollapseMarginAfterWithChildren = canCollapse; }

        bool atBeforeSideOfBlock() const { return m_atBeforeSideOfBlock; }
        bool atAfterSideOfBlock() const { return m_atAfterSideOfBlock; }
        bool canCollapseWithMarginBefore() const { return m_atBeforeSideOfBlock && m_canCollapseMarginBeforeWithChildren; }
        bool canCollapseWithMarginAfter() const { return m_atAfterSideOfBlock && m_canCollapseMarginAfterWithChildren; }
        bool canCollapseMarginBeforeWithChildren() const { return m_canCollapseMarginBeforeWithChildren; }
        bool canCollapseMarginAfterWithChildren() const { return m_canCollapseMarginAfterWithChildren; }
        bool canCollapseWithChildren() const { return m_canCollapseWithChildren; }
        bool quirkContainer() const { return m_quirkContainer; }
        bool determinedMarginBeforeQuirk() const { return m_determinedMarginBeforeQuirk; }
        bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
        bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }
        LayoutUnit positiveMargin() const { return m_positiveMargin; }
        LayoutUnit negativeMargin() const { return m_negativeMargin; }
        LayoutUnit margin() const { return m_positiveMargin - m_negativeMargin; }

    private:
        // Collapsing flags for whether we can collapse our margins with our children's margins.
        bool m_canCollapseWithChildren : 1 { false };
        bool m_canCollapseMarginBeforeWithChildren : 1 { false };
        bool m_canCollapseMarginAfterWithChildren : 1 { false };

        // Whether or not we are a quirky container, i.e., do we collapse away top and bottom
        // margins in our container. Table cells and the body are the common examples. We
        // also have a custom style property for Safari RSS to deal with TypePad blog articles.
        bool m_quirkContainer : 1 { false };

        // This flag tracks whether we are still looking at child margins that can all collapse together at the beginning of a block.
        // They may or may not collapse with the top margin of the block (|m_canCollapseTopWithChildren| tells us that), but they will
        // always be collapsing with one another. This variable can remain set to true through multiple iterations
        // as long as we keep encountering self-collapsing blocks.
        bool m_atBeforeSideOfBlock : 1 { true };

        // This flag is set when we know we're examining bottom margins and we know we're at the bottom of the block.
        bool m_atAfterSideOfBlock : 1 { false };

        // These variables are used to detect quirky margins that we need to collapse away (in table cells
        // and in the body element).
        bool m_hasMarginBeforeQuirk : 1 { false };
        bool m_hasMarginAfterQuirk : 1 { false };
        bool m_determinedMarginBeforeQuirk : 1 { false };

        // These flags track the previous maximal positive and negative margins.
        LayoutUnit m_positiveMargin;
        LayoutUnit m_negativeMargin;
    };

    bool shouldTrimChildMargin(Style::MarginTrimSide, const RenderBox&) const;
    void performBlockStepSizing(RenderBox& child, LayoutUnit blockStepSizeForChild) const;

    void layoutBlockChild(RenderBox& child, MarginInfo&, LayoutUnit& previousFloatLogicalBottom, LayoutUnit& maxFloatLogicalBottom);
    struct BlockPositionAndMargin {
        LayoutUnit logicalTop;
        MarginInfo marginInfo;
    };
    BlockPositionAndMargin layoutBlockChildFromInlineLayout(RenderBox& child, LayoutUnit blockLogicalTop, MarginInfo); // Called from IFC when laying out block in inline.

    void adjustOutOfFlowBlock(RenderBox& child, const MarginInfo&);
    void adjustFloatingBlock(const MarginInfo&);

    void trimBlockEndChildrenMargins();

    void setStaticInlinePositionForChild(RenderBox& child, LayoutUnit inlinePosition);
    void updateStaticInlinePositionForChild(RenderBox& child, LayoutUnit logicalTop);

    LayoutUnit staticInlinePositionForOriginalDisplayInline(LayoutUnit logicalTop);

    LayoutUnit collapseMargins(RenderBox& child, MarginInfo&);
    LayoutUnit collapseMarginsWithChildInfo(RenderBox* child, MarginInfo&);

    LayoutUnit clearFloatsIfNeeded(RenderBox& child, MarginInfo&, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos);
    LayoutUnit estimateLogicalTopPosition(RenderBox& child, const MarginInfo&, LayoutUnit& estimateWithoutPagination);
    void marginBeforeEstimateForChild(RenderBox&, LayoutUnit&, LayoutUnit&) const;
    LayoutUnit handleAfterSideOfBlock(MarginInfo&, LayoutUnit contentBoxLogicalHeight);
    void setCollapsedBottomMargin(const MarginInfo&);

    bool childrenPreventSelfCollapsing() const final;

    bool shouldBreakAtLineToAvoidWidow() const { return hasRareBlockFlowData() && rareBlockFlowData()->m_lineBreakToAvoidWidow >= 0; }
    void clearShouldBreakAtLineToAvoidWidow() const;
    int lineBreakToAvoidWidow() const { return hasRareBlockFlowData() ? rareBlockFlowData()->m_lineBreakToAvoidWidow : -1; }
    void setBreakAtLineToAvoidWidow(int);
    void clearDidBreakAtLineToAvoidWidow();
    void setDidBreakAtLineToAvoidWidow();
    bool didBreakAtLineToAvoidWidow() const { return hasRareBlockFlowData() && rareBlockFlowData()->m_didBreakAtLineToAvoidWidow; }

    RenderMultiColumnFlow* multiColumnFlow() const { return hasRareBlockFlowData() ? multiColumnFlowSlowCase() : nullptr; }
    RenderMultiColumnFlow* multiColumnFlowSlowCase() const;
    void setMultiColumnFlow(RenderMultiColumnFlow&);
    void clearMultiColumnFlow();
    bool willCreateColumns(std::optional<unsigned> desiredColumnCount = std::nullopt) const;
    virtual bool requiresColumns(int) const;

    bool containsFloats() const override { return m_floatingObjects && !m_floatingObjects->set().isEmpty(); }
    bool containsFloat(const RenderBox&) const;
    bool subtreeContainsFloats() const;
    bool subtreeContainsFloat(const RenderBox&) const;

    Position positionForPoint(const LayoutPoint&, HitTestSource) override;
    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) override;

    LayoutUnit lowestFloatLogicalBottom(FloatingObject::Type = FloatingObject::FloatLeftRight) const;

    void removeFloatingObjects();
    void markAllDescendantsWithFloatsForLayout(RenderBox* floatToRemove = nullptr, bool inLayout = true);
    void markSiblingsWithFloatsForLayout(RenderBox* floatToRemove = nullptr);

    const FloatingObjectSet* floatingObjectSet() const { return m_floatingObjects ? &m_floatingObjects->set() : nullptr; }

    FloatingObject& insertFloatingBox(RenderBox&);

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

    LegacyRootInlineBox* legacyRootBox() const { return svgTextLayout() ? svgTextLayout()->legacyRootBox() : nullptr; }

    void setChildrenInline(bool) final;

    bool hasLines() const;
    bool hasBlocksInInlineLayout() const;

    enum InvalidationReason : uint8_t {
        InternalMove,       // (anon) block is moved or collapsed
        InsertionOrRemoval, // child renderer gets constructed/goes away
        ContentChange       // existing renderer gets changed (text content only atm)
    };
    void invalidateLineLayout(InvalidationReason);
    void computeAndSetLineLayoutPath();

    enum LineLayoutPath { UndeterminedPath = 0, InlinePath, SvgTextPath };
    LineLayoutPath lineLayoutPath() const { return static_cast<LineLayoutPath>(renderBlockFlowLineLayoutPath()); }
    void setLineLayoutPath(LineLayoutPath path) { setRenderBlockFlowLineLayoutPath(path); }

    int lineCount() const;

    bool containsNonZeroBidiLevel() const;

    const LegacyLineLayout* svgTextLayout() const;
    LegacyLineLayout* svgTextLayout();
    const LayoutIntegration::LineLayout* inlineLayout() const;
    LayoutIntegration::LineLayout* inlineLayout();

#if ENABLE(TREE_DEBUGGING)
    void outputFloatingObjects(WTF::TextStream&, int depth) const;
    void outputLineTreeAndMark(WTF::TextStream&, const LegacyInlineBox* markedBox, int depth) const;
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

    void updateColumnProgressionFromStyle(const RenderStyle&);
    void updateStylesForColumnChildren(const RenderStyle* oldStyle);

    bool needsLayoutAfterFragmentRangeChange() const override;
    WEBCORE_EXPORT RenderText* findClosestTextAtAbsolutePoint(const FloatPoint&);

    // A page break is required at some offset due to space shortage in the current fragmentainer.
    void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage);
    // Update minimum page height required to avoid fragmentation where it shouldn't occur (inside
    // unbreakable content, between orphans and widows, etc.). This will be used as a hint to the
    // column balancer to help set a good minimum column height.
    void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight);

    void adjustSizeContainmentChildForPagination(RenderBox& child, LayoutUnit offset);

    void addFloatsToNewParent(RenderBlockFlow& toBlockFlow) const;
    
    inline LayoutUnit endPaddingWidthForCaret() const;

    LayoutUnit adjustEnclosingTopForPrecedingBlock(LayoutUnit top) const;

    std::optional<LayoutUnit> lowestInitialLetterLogicalBottom() const;

protected:
    bool isChildEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox&) const final;

    bool shouldResetLogicalHeightBeforeLayout() const override { return true; }

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
    }

    void setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg);
    void setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg);

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void createFloatingObjects();

    std::optional<LayoutUnit> firstLineBaseline() const override;
    std::optional<LayoutUnit> lastLineBaseline() const override;

    void setComputedColumnCountAndWidth(int, LayoutUnit);

    LayoutUnit computedColumnWidth() const;
    unsigned computedColumnCount() const;
    
    LayoutOptionalOutsets allowedLayoutOverflow() const override;

    virtual void computeColumnCountAndWidth();

    // Called to lay out the legend for a fieldset or the ruby text of a ruby run. Also used by multi-column layout to handle
    // the flow thread child.
    void layoutExcludedChildren(RelayoutChildren) override;

private:
    bool recomputeLogicalWidthAndColumnWidth();
    LayoutUnit columnGap() const;
    
    RenderBlockFlow* previousSiblingWithOverhangingFloats(bool& parentHasFloats) const;

    void checkForPaginationLogicalHeightChange(RelayoutChildren&, LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged);

    void paintInlineChildren(PaintInfo&, const LayoutPoint&) override;
    void paintFloats(PaintInfo&, const LayoutPoint&, bool preservePhase = false) override;

    void repaintOverhangingFloats(bool paintAllDescendants) final;
    void clipOutFloatingBoxes(RenderBlock&, const PaintInfo*, const LayoutPoint&, const LayoutSize&) override;

    void insertFloatingBoxAndMarkForLayout(RenderBox&);
    void removeFloatingBox(RenderBox&);
    void computeLogicalLocationForFloat(FloatingObject&, LayoutUnit& logicalTopOffset);

    // Called from lineWidth, to position the floats added in the last line.
    // Returns true if and only if it has positioned any floats.
    bool positionNewFloats();
    void clearFloats(UsedClear);

    LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const override;
    LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit logicalHeight) const override;

    LayoutUnit logicalRightOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const;
    LayoutUnit logicalLeftOffsetForPositioningFloat(LayoutUnit logicalTop, LayoutUnit fixedOffset, LayoutUnit* heightRemaining) const;
    
    LayoutUnit nextFloatLogicalBottomBelow(LayoutUnit) const;
    LayoutUnit nextFloatLogicalBottomBelowForBlock(LayoutUnit) const;
    
    LayoutUnit addOverhangingFloats(RenderBlockFlow& child, bool makeChildPaintOtherFloats);
    bool hasOverhangingFloat(RenderBox&);
    void addIntrudingFloats(RenderBlockFlow* prev, RenderBlockFlow* container, LayoutUnit xoffset, LayoutUnit yoffset);
    inline bool hasOverhangingFloats() const;
    LayoutUnit computedClearDeltaForChild(RenderBox& child, LayoutUnit yPos);

    void determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    
    bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset) override;
    bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void addOverflowFromInlineChildren();

    GapRects inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches&, const PaintInfo*) override;
    
    PositionWithAffinity positionForPointWithInlineChildren(const LayoutPoint& pointInLogicalContents, HitTestSource) override;

    bool hasSvgTextLayout() const;

    bool hasInlineLayout() const;
    void layoutInlineContent(RelayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);
    bool markInlineContentDirtyForLayout(RelayoutChildren);
    void updateRepaintTopAndBottomAfterLayout(RelayoutChildren, std::optional<LayoutRect> partialRepaintRect, std::pair<float, float> oldContentTopAndBottomIncludingInkOverflow, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);
    std::optional<LayoutUnit> updateLineClampStateAndLogicalHeightAfterLayout();
    bool tryComputePreferredWidthsUsingInlinePath(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth);
    void setStaticPositionsForSimpleOutOfFlowContent();

    void adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;
    void computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;
    void adjustInitialLetterPosition(RenderBox& childBox, LayoutUnit& logicalTopOffset, LayoutUnit& marginBeforeOffset);

    void setTextBoxTrimForSubtree(const RenderBlockFlow* inlineFormattingContextRootForTextBoxTrimEnd = nullptr);
    void adjustTextBoxTrimAfterLayout();
    std::pair<float, float> inlineContentTopAndBottomIncludingInkOverflow() const;

    void dirtyForLayoutFromPercentageHeightDescendants();

#if ENABLE(TEXT_AUTOSIZING)
    int m_widthForTextAutosizing;
    unsigned m_lineCountForTextAutosizing : 2;
#endif
    // FIXME: This is temporary until after we remove the forced "line layout codepath" invalidation.
    std::optional<std::pair<LayoutUnit, LayoutUnit>> m_previousInlineLayoutContentTopAndBottomIncludingInkOverflow;

    std::optional<LayoutUnit> selfCollapsingMarginBeforeWithClear(RenderObject* candidate);

public:
    struct LinePaginationAdjustment {
        LayoutUnit strut { 0_lu };
        bool isFirstAfterPageBreak { false };
    };
    LinePaginationAdjustment computeLineAdjustmentForPagination(const InlineIterator::LineBoxIterator&, LayoutUnit deltaOffset, LayoutUnit floatMinimumBottom = { });
    bool relayoutForPagination();

    bool hasRareBlockFlowData() const { return m_rareBlockFlowData.get(); }
    RenderBlockFlowRareData* rareBlockFlowData() const { ASSERT_WITH_SECURITY_IMPLICATION(hasRareBlockFlowData()); return m_rareBlockFlowData.get(); }
    RenderBlockFlowRareData& ensureRareBlockFlowData();
    void materializeRareBlockFlowData();

#if ENABLE(TEXT_AUTOSIZING)
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

private:
    Variant<
        std::monostate,
        std::unique_ptr<LayoutIntegration::LineLayout>,
        std::unique_ptr<LegacyLineLayout>
    > m_lineLayout;
};

inline bool RenderBlockFlow::hasSvgTextLayout() const
{
    return std::holds_alternative<std::unique_ptr<LegacyLineLayout>>(m_lineLayout);
}

inline const LegacyLineLayout* RenderBlockFlow::svgTextLayout() const
{
    return hasSvgTextLayout() ? std::get<std::unique_ptr<LegacyLineLayout>>(m_lineLayout).get() : nullptr;
}

inline LegacyLineLayout* RenderBlockFlow::svgTextLayout()
{
    return hasSvgTextLayout() ? std::get<std::unique_ptr<LegacyLineLayout>>(m_lineLayout).get() : nullptr;
}

inline bool RenderBlockFlow::hasInlineLayout() const
{
    return std::holds_alternative<std::unique_ptr<LayoutIntegration::LineLayout>>(m_lineLayout);
}

inline const LayoutIntegration::LineLayout* RenderBlockFlow::inlineLayout() const
{
    return hasInlineLayout() ? std::get<std::unique_ptr<LayoutIntegration::LineLayout>>(m_lineLayout).get() : nullptr;
}

inline LayoutIntegration::LineLayout* RenderBlockFlow::inlineLayout()
{
    return hasInlineLayout() ? std::get<std::unique_ptr<LayoutIntegration::LineLayout>>(m_lineLayout).get() : nullptr;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderBlockFlow, isRenderBlockFlow())
