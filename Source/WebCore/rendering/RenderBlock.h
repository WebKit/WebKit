/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include <WebCore/GapRects.h>
#include <WebCore/LineWidth.h>
#include <WebCore/RenderBox.h>
#include <WebCore/TextRun.h>
#include <memory>
#include <wtf/WeakListHashSet.h>

namespace WebCore {

class LayoutScope;
class LogicalSelectionOffsetCaches;
class RenderInline;
class RenderText;

struct PaintInfo;
struct RenderBlockRareData;

using TrackedRendererListHashSet = SingleThreadWeakListHashSet<RenderBox>;

enum CaretType { CursorCaret, DragCaret };
enum class RelayoutChildren : bool { No, Yes };

enum TextRunFlag {
    DefaultTextRunFlags = 0,
    RespectDirection = 1 << 0,
    RespectDirectionOverride = 1 << 1
};

typedef unsigned TextRunFlags;

class RenderBlock : public RenderBox {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderBlock);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderBlock);
public:
    // FIXME: This is temporary to allow us to move code from RenderBlock into RenderBlockFlow that accesses member variables that we haven't moved out of
    // RenderBlock yet.
    friend class LayoutScope;
    friend class RenderBlockFlow;
    virtual ~RenderBlock();

protected:
    RenderBlock(Type, Element&, RenderStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags = { });
    RenderBlock(Type, Document&, RenderStyle&&, OptionSet<TypeFlag>, TypeSpecificFlags = { });

public:
    virtual void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu);

    void addOutOfFlowBox(RenderBox&);
    static void removeOutOfFlowBox(const RenderBox&);
    enum class ContainingBlockState : bool { NewContainingBlock, SameContainingBlock };
    void removeOutOfFlowBoxes(const RenderBlock*, ContainingBlockState = ContainingBlockState::SameContainingBlock);

    TrackedRendererListHashSet* outOfFlowBoxes() const;
    bool hasOutOfFlowBoxes() const
    {
        auto* renderers = outOfFlowBoxes();
        return renderers && !renderers->isEmptyIgnoringNullReferences();
    }
    void addPercentHeightDescendant(RenderBox&);
    static void removePercentHeightDescendant(RenderBox&);
    TrackedRendererListHashSet* percentHeightDescendants() const;
    bool hasPercentHeightDescendants() const
    {
        auto* renderers = percentHeightDescendants();
        return renderers && !renderers->isEmptyIgnoringNullReferences();
    }
    static bool hasPercentHeightContainerMap();
    static void clearPercentHeightDescendantsFrom(RenderBox&);

    bool isContainingBlockAncestorFor(RenderObject&) const;

    void setHasMarginBeforeQuirk(bool b) { setRenderBlockHasMarginBeforeQuirk(b); }
    void setHasMarginAfterQuirk(bool b) { setRenderBlockHasMarginAfterQuirk(b); }
    void setShouldForceRelayoutChildren(bool b) { setRenderBlockShouldForceRelayoutChildren(b); }

    bool hasMarginBeforeQuirk() const { return renderBlockHasMarginBeforeQuirk(); }
    bool hasMarginAfterQuirk() const { return renderBlockHasMarginAfterQuirk(); }
    bool hasBorderOrPaddingLogicalWidthChanged() const { return renderBlockShouldForceRelayoutChildren(); }

    bool hasMarginBeforeQuirk(const RenderBox& child) const;
    bool hasMarginAfterQuirk(const RenderBox& child) const;

    virtual bool shouldChildInlineMarginContributeToContainerIntrinsicSize(Style::MarginTrimSide, const RenderElement&) const { return true; }

    void markOutOfFlowBoxesForLayout();
    void markForPaginationRelayoutIfNeeded() override;
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all of the line layout code has been moved out of RenderBlock
    virtual bool containsFloats() const { return false; }

    inline LayoutUnit availableLogicalWidthForLine(LayoutUnit position, LayoutUnit logicalHeight) const;
    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, LayoutUnit logicalHeight = 0_lu) const;
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, LayoutUnit logicalHeight = 0_lu) const;
    inline LayoutUnit startOffsetForLine(LayoutUnit position, LayoutUnit logicalHeight) const;
    inline LayoutUnit endOffsetForLine(LayoutUnit position, LayoutUnit logicalHeight) const;

    LayoutUnit textIndentOffset() const;

    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) override;

    GapRects selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer);
    LayoutRect logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock, RenderElement* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    LayoutRect logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock, RenderElement* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    void getSelectionGapInfo(HighlightState, bool& leftGap, bool& rightGap);
    bool isSelectionRoot() const;

    LayoutRect logicalRectToPhysicalRect(const LayoutPoint& physicalPosition, const LayoutRect& logicalRect);

    void addContinuationWithOutline(RenderInline*);
#if ASSERT_ENABLED
    bool paintsContinuationOutline(const RenderInline&);
#endif

    bool establishesIndependentFormattingContext() const;
    bool createsNewFormattingContext() const;

    static TextRun constructTextRun(StringView, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior(), TextRunFlags = DefaultTextRunFlags);
    static TextRun constructTextRun(const String&, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior(), TextRunFlags = DefaultTextRunFlags);
    static TextRun constructTextRun(const AtomString&, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior(), TextRunFlags = DefaultTextRunFlags);
    static TextRun constructTextRun(const RenderText&, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior());
    static TextRun constructTextRun(const RenderText&, unsigned offset, unsigned length, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior());
    static TextRun constructTextRun(std::span<const Latin1Character> characters, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior());
    static TextRun constructTextRun(std::span<const char16_t> characters, const RenderStyle&,
        ExpansionBehavior = ExpansionBehavior::defaultBehavior());

    LayoutUnit paginationStrut() const;
    void setPaginationStrut(LayoutUnit);

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset() const;
    void setPageLogicalOffset(LayoutUnit);

    // Fieldset legends that are taller than the fieldset border add in intrinsic border
    // in order to ensure that content gets properly pushed down across all layout systems
    // (flexbox, block, etc.)
    LayoutUnit intrinsicBorderForFieldset() const;
    void setIntrinsicBorderForFieldset(LayoutUnit);

    RectEdges<LayoutUnit> borderWidths() const override;
    LayoutUnit borderTop() const override;
    LayoutUnit borderBottom() const override;
    LayoutUnit borderLeft() const override;
    LayoutUnit borderRight() const override;

    LayoutUnit borderBefore() const override;
    LayoutUnit adjustBorderBoxLogicalHeightForBoxSizing(LayoutUnit height) const override;
    LayoutUnit adjustContentBoxLogicalHeightForBoxSizing(std::optional<LayoutUnit> height) const override;
    LayoutUnit adjustIntrinsicLogicalHeightForBoxSizing(LayoutUnit height) const override;
    void paintExcludedChildrenInBorder(PaintInfo&, const LayoutPoint&);
    
    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    enum ApplyLayoutDeltaMode { ApplyLayoutDelta, DoNotApplyLayoutDelta };
    LayoutUnit logicalWidthForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.width() : child.height(); }
    LayoutUnit logicalHeightForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.height() : child.width(); }
    inline LayoutUnit logicalMarginBoxHeightForChild(const RenderBox& child) const;
    LayoutSize logicalSizeForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.size() : child.size().transposedSize(); }
    LayoutUnit logicalTopForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.y() : child.x(); }
    LayoutUnit logicalLeftForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.x() : child.y(); }
    void setLogicalLeftForChild(RenderBox& child, LayoutUnit logicalLeft, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    void setLogicalTopForChild(RenderBox& child, LayoutUnit logicalTop, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    LayoutUnit marginBeforeForChild(const RenderBoxModelObject& child) const { return child.marginBefore(writingMode()); }
    LayoutUnit marginAfterForChild(const RenderBoxModelObject& child) const { return child.marginAfter(writingMode()); }
    LayoutUnit marginStartForChild(const RenderBoxModelObject& child) const { return child.marginStart(writingMode()); }
    LayoutUnit marginEndForChild(const RenderBoxModelObject& child) const { return child.marginEnd(writingMode()); }
    void setMarginStartForChild(RenderBox& child, LayoutUnit value) const { child.setMarginStart(value, writingMode()); }
    void setMarginEndForChild(RenderBox& child, LayoutUnit value) const { child.setMarginEnd(value, writingMode()); }
    void setMarginBeforeForChild(RenderBox& child, LayoutUnit value) const { child.setMarginBefore(value, writingMode()); }
    void setMarginAfterForChild(RenderBox& child, LayoutUnit value) const { child.setMarginAfter(value, writingMode()); }
    void setTrimmedMarginForChild(RenderBox& child, Style::MarginTrimSide);
    LayoutUnit collapsedMarginBeforeForChild(const RenderBox& child) const;
    LayoutUnit collapsedMarginAfterForChild(const RenderBox& child) const;

    std::pair<RenderObject*, RenderElement*> firstLetterAndContainer(RenderObject* skipThisAsFirstLetter = nullptr);

    virtual void scrollbarsChanged(bool /*horizontalScrollbarChanged*/, bool /*verticalScrollbarChanged*/) { }

    LayoutUnit availableLogicalWidthForContent() const
    { 
        return std::max(0_lu, logicalRightOffsetForContent() - logicalLeftOffsetForContent());
    }
    LayoutUnit logicalLeftOffsetForContent() const;
    LayoutUnit logicalRightOffsetForContent() const;
    LayoutUnit startOffsetForContent() const;
    LayoutUnit endOffsetForContent() const;

    LayoutUnit logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches&);
    LayoutUnit logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches&);

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) const override;

    bool canHaveChildren() const override { return true; }
    virtual bool canDropAnonymousBlockChild() const { return true; }

    RenderFragmentedFlow* cachedEnclosingFragmentedFlow() const;
    void setCachedEnclosingFragmentedFlowNeedsUpdate();
    virtual bool cachedEnclosingFragmentedFlowNeedsUpdate() const;
    void resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(RenderFragmentedFlow* = nullptr) final;

    std::optional<LayoutUnit> availableLogicalHeightForPercentageComputation() const;
    bool hasDefiniteLogicalHeight() const;

    static String updateSecurityDiscCharacters(const RenderStyle&, String&&);

    virtual bool hasLineIfEmpty() const;

    void updateDescendantTransformsAfterLayout();

    virtual bool canPerformSimplifiedLayout() const;

    LayoutUnit offsetFromLogicalTopOfFirstPage() const override;
    RenderFragmentContainer* fragmentAtBlockOffset(LayoutUnit) const;

    void clearLayoutOverflow();

    // Adjust from painting offsets to the local coords of this renderer
    void offsetForContents(LayoutPoint&) const;

    enum FieldsetFindLegendOption { FieldsetIgnoreFloatingOrOutOfFlow, FieldsetIncludeFloatingOrOutOfFlow };
    RenderBox* findFieldsetLegend(FieldsetFindLegendOption = FieldsetIgnoreFloatingOrOutOfFlow) const;
    virtual void layoutExcludedChildren(RelayoutChildren);
    virtual bool computePreferredWidthsForExcludedChildren(LayoutUnit&, LayoutUnit&) const;

    void adjustBorderBoxRectForPainting(LayoutRect&) override;
    LayoutRect paintRectToClipOutFromBorder(const LayoutRect&) override;

    void boundingRects(Vector<LayoutRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    PaintInfo paintInfoForBlockChildren(const PaintInfo&) const;

protected:
    RenderFragmentedFlow* locateEnclosingFragmentedFlow() const override;
    bool establishesIndependentFormattingContextIgnoringDisplayType(const RenderStyle&) const;

    void layout() override;

    void layoutOutOfFlowBoxes(RelayoutChildren, bool fixedPositionObjectsOnly = false);
    virtual void layoutOutOfFlowBox(RenderBox&, RelayoutChildren, bool fixedPositionObjectsOnly);
    
    void markFixedPositionBoxForLayoutIfNeeded(RenderBox& child);

    LayoutUnit marginIntrinsicLogicalWidthForChild(RenderBox&) const;

    void paint(PaintInfo&, const LayoutPoint&) override;
    void paintObject(PaintInfo&, const LayoutPoint&) override;
    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect);
    enum PaintBlockType { PaintAsBlock, PaintAsInlineBlock };
    bool paintChild(RenderBox&, PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect, PaintBlockType paintType = PaintAsBlock);

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;
    
    std::optional<LayoutUnit> firstLineBaseline() const override;
    std::optional<LayoutUnit> lastLineBaseline() const override;

    // Delay updating scrollbars until endAndCommitUpdateScrollInfoAfterLayoutTransaction() is called. These functions are used
    // when a flexbox is laying out its descendants. If multiple calls are made to beginUpdateScrollInfoAfterLayoutTransaction()
    // then endAndCommitUpdateScrollInfoAfterLayoutTransaction() will do nothing until it is called the same number of times.
    void beginUpdateScrollInfoAfterLayoutTransaction();
    void endAndCommitUpdateScrollInfoAfterLayoutTransaction();

    void removeFromUpdateScrollInfoAfterLayoutTransaction();

    void updateScrollInfoAfterLayout();

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

    bool childBoxIsUnsplittableForFragmentation(const RenderBox& child) const;

    static LayoutUnit layoutOverflowLogicalBottom(const RenderBlock&);

    String debugDescription() const override;

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset);

    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, OptionSet<ComputeOverflowOptions> = { });
    void addOverflowFromOutOfFlowBoxes();
    void addVisualOverflowFromTheme();

    void computeFragmentRangeForBoxChild(const RenderBox&) const;

    void estimateFragmentRangeForBoxChild(const RenderBox&) const;
    bool updateFragmentRangeForBoxChild(const RenderBox&) const;

    void updateBlockChildDirtyBitsBeforeLayout(RelayoutChildren, RenderBox&);

    void preparePaginationBeforeBlockLayout(RelayoutChildren&);

    void computeChildPreferredLogicalWidths(RenderBox&, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const;

    virtual void computeChildIntrinsicLogicalWidths(RenderBox&, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const;

    RenderBlockRareData& ensureBlockRareData();
    RenderBlockRareData* blockRareData() const;
    bool recomputeLogicalWidth();

private:
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit) const { return fixedOffset; };
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit) const { return fixedOffset; }
    LayoutUnit adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats) const;
    LayoutUnit adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats) const;

    ASCIILiteral renderName() const override;

    bool isSelfCollapsingBlock() const override;
    virtual bool childrenPreventSelfCollapsing() const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool) { }
    virtual void paintInlineChildren(PaintInfo&, const LayoutPoint&) { }
    void paintContents(PaintInfo&, const LayoutPoint&);
    virtual void paintColumnRules(PaintInfo&, const LayoutPoint&) { };
    void paintSelection(PaintInfo&, const LayoutPoint&);
    void paintCaret(PaintInfo&, const LayoutPoint&, CaretType);
    void paintCarets(PaintInfo&, const LayoutPoint&);

    Node* nodeForHitTest() const override;

    virtual bool hitTestContents(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    // FIXME-BLOCKFLOW: Remove virtualization when all callers have moved to RenderBlockFlow
    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }
    virtual bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction);
    virtual bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction) { return false; }
    bool hitTestExcludedChildrenInBorder(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;
    
    LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const final;
    const RenderStyle& outlineStyleForRepaint() const final;

    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool /*clipToVisibleContent*/) final
    {
        return selectionGapRectsForRepaint(repaintContainer);
    }
    bool shouldPaintSelectionGaps() const final;
    GapRects selectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches&, const PaintInfo* = 0);
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual GapRects inlineSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    GapRects blockSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    LayoutRect blockSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit lastLogicalTop, LayoutUnit lastLogicalLeft, LayoutUnit lastLogicalRight, LayoutUnit logicalBottom, const LogicalSelectionOffsetCaches&, const PaintInfo*);

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void clipOutFloatingBoxes(RenderBlock&, const PaintInfo*, const LayoutPoint&, const LayoutSize&) { };

    void paintContinuationOutlines(PaintInfo&, const LayoutPoint&);

    virtual PositionWithAffinity positionForPointWithInlineChildren(const LayoutPoint&, HitTestSource);

    RenderFragmentedFlow* updateCachedEnclosingFragmentedFlow(RenderFragmentedFlow*) const;

    void absoluteQuadsIgnoringContinuation(const FloatRect&, Vector<FloatQuad>&, bool* wasFixed) const override;

    void paintDebugBoxShadowIfApplicable(GraphicsContext&, const LayoutRect&) const;

    bool contentBoxLogicalWidthChanged(const RenderStyle&, const RenderStyle&);
    bool paddingBoxLogicaHeightChanged(const RenderStyle& oldStyle, const RenderStyle& newStyle);
    bool scrollbarWidthDidChange(const RenderStyle&, const RenderStyle&, ScrollbarOrientation);

private:
    // Used to store state between styleWillChange and styleDidChange
    static bool s_canPropagateFloatIntoSibling;
};

LayoutUnit blockDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock);
LayoutUnit inlineDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock);
PositionWithAffinity positionForPointRespectingEditingBoundaries(RenderBlock&, RenderBox&, const LayoutPoint&, HitTestSource);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderBlock, isRenderBlock())
