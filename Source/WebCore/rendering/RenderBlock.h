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

#ifndef RenderBlock_h
#define RenderBlock_h

#include "GapRects.h"
#include "LineWidth.h"
#include "RenderBox.h"
#include "TextRun.h"
#include <memory>
#include <wtf/ListHashSet.h>

namespace WebCore {

class LayoutState;
class LineLayoutState;
class LogicalSelectionOffsetCaches;
class RenderInline;
class RenderText;

struct BidiRun;
struct PaintInfo;

typedef WTF::ListHashSet<RenderBox*> TrackedRendererListHashSet;
typedef WTF::HashMap<const RenderBlock*, std::unique_ptr<TrackedRendererListHashSet>> TrackedDescendantsMap;
typedef WTF::HashMap<const RenderBox*, std::unique_ptr<HashSet<RenderBlock*>>> TrackedContainerMap;

enum CaretType { CursorCaret, DragCaret };
enum ContainingBlockState { NewContainingBlock, SameContainingBlock };

enum TextRunFlag {
    DefaultTextRunFlags = 0,
    RespectDirection = 1 << 0,
    RespectDirectionOverride = 1 << 1
};

typedef unsigned TextRunFlags;

class RenderBlock : public RenderBox {
public:
    friend class LineLayoutState;

protected:
    RenderBlock(Element&, Ref<RenderStyle>&&, BaseTypeFlags);
    RenderBlock(Document&, Ref<RenderStyle>&&, BaseTypeFlags);
    virtual ~RenderBlock();

public:
    // These two functions are overridden for inline-block.
    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    LayoutUnit minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void deleteLines();

    void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;
    void removeChild(RenderObject&) override;

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0);

    virtual void invalidateLineLayoutPath() { }

    void insertPositionedObject(RenderBox&);
    static void removePositionedObject(RenderBox&);
    void removePositionedObjects(RenderBlock*, ContainingBlockState = SameContainingBlock);

    TrackedRendererListHashSet* positionedObjects() const;
    bool hasPositionedObjects() const
    {
        TrackedRendererListHashSet* objects = positionedObjects();
        return objects && !objects->isEmpty();
    }

    void addPercentHeightDescendant(RenderBox&);
    static void removePercentHeightDescendant(RenderBox&);
    TrackedRendererListHashSet* percentHeightDescendants() const;
    static bool hasPercentHeightContainerMap();
    static bool hasPercentHeightDescendant(RenderBox&);
    static void clearPercentHeightDescendantsFrom(RenderBox&);
    static void removePercentHeightDescendantIfNeeded(RenderBox&);

    void setHasMarginBeforeQuirk(bool b) { setRenderBlockHasMarginBeforeQuirk(b); }
    void setHasMarginAfterQuirk(bool b) { setRenderBlockHasMarginAfterQuirk(b); }
    void setHasBorderOrPaddingLogicalWidthChanged(bool b) { setRenderBlockHasBorderOrPaddingLogicalWidthChanged(b); }

    bool hasMarginBeforeQuirk() const { return renderBlockHasMarginBeforeQuirk(); }
    bool hasMarginAfterQuirk() const { return renderBlockHasMarginAfterQuirk(); }
    bool hasBorderOrPaddingLogicalWidthChanged() const { return renderBlockHasBorderOrPaddingLogicalWidthChanged(); }

    bool hasMarginBeforeQuirk(const RenderBox& child) const;
    bool hasMarginAfterQuirk(const RenderBox& child) const;

    bool generatesLineBoxesForInlineChild(RenderObject*);

    void markPositionedObjectsForLayout();
    void markForPaginationRelayoutIfNeeded() override;
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all of the line layout code has been moved out of RenderBlock
    virtual bool containsFloats() const { return false; }

    // Versions that can compute line offsets with the region and page offset passed in. Used for speed to avoid having to
    // compute the region all over again when you already know it.
    LayoutUnit availableLogicalWidthForLineInRegion(LayoutUnit position, IndentTextOrNot shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return std::max<LayoutUnit>(0, logicalRightOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight)
            - logicalLeftOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight));
    }
    LayoutUnit logicalRightOffsetForLineInRegion(LayoutUnit position, IndentTextOrNot shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(region), shouldIndentText, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLineInRegion(LayoutUnit position, IndentTextOrNot shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(region), shouldIndentText, logicalHeight);
    }
    LayoutUnit startOffsetForLineInRegion(LayoutUnit position, IndentTextOrNot shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return style().isLeftToRightDirection() ? logicalLeftOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight);
    }
    LayoutUnit endOffsetForLineInRegion(LayoutUnit position, IndentTextOrNot shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return !style().isLeftToRightDirection() ? logicalLeftOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLineInRegion(position, shouldIndentText, region, logicalHeight);
    }

    LayoutUnit availableLogicalWidthForLine(LayoutUnit position, IndentTextOrNot shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return availableLogicalWidthForLineInRegion(position, shouldIndentText, regionAtBlockOffset(position), logicalHeight);
    }
    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, IndentTextOrNot shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(position), shouldIndentText, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, IndentTextOrNot shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(position), shouldIndentText, logicalHeight);
    }
    LayoutUnit startOffsetForLine(LayoutUnit position, IndentTextOrNot shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return style().isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, logicalHeight);
    }
    LayoutUnit endOffsetForLine(LayoutUnit position, IndentTextOrNot shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return !style().isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, logicalHeight);
    }

    LayoutUnit textIndentOffset() const;

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderRegion*) override;

    GapRects selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer);
    LayoutRect logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        RenderBoxModelObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    LayoutRect logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        RenderBoxModelObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    void getSelectionGapInfo(SelectionState, bool& leftGap, bool& rightGap);
    RenderBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const;

    LayoutRect logicalRectToPhysicalRect(const LayoutPoint& physicalPosition, const LayoutRect& logicalRect);

    void addContinuationWithOutline(RenderInline*);
    bool paintsContinuationOutline(RenderInline*);

    RenderBoxModelObject* virtualContinuation() const final { return continuation(); }
    bool isAnonymousBlockContinuation() const { return isAnonymousBlock() && continuation(); }
    WEBCORE_EXPORT RenderInline* inlineElementContinuation() const;
    RenderBlock* blockElementContinuation() const;

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    static RenderBlock* createAnonymousWithParentRendererAndDisplay(const RenderObject*, EDisplay = BLOCK);
    RenderBlock* createAnonymousBlock(EDisplay display = BLOCK) const { return createAnonymousWithParentRendererAndDisplay(this, display); }
    static void dropAnonymousBoxChild(RenderBlock& parent, RenderBlock& child);

    RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const override;

    static bool shouldSkipCreatingRunsForObject(RenderObject& obj)
    {
        return obj.isFloating() || (obj.isOutOfFlowPositioned() && !obj.style().isOriginalDisplayInlineType() && !obj.container()->isRenderInline());
    }

    static TextRun constructTextRun(StringView, const RenderStyle&,
        ExpansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion, TextRunFlags = DefaultTextRunFlags);
    static TextRun constructTextRun(const String&, const RenderStyle&,
        ExpansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion, TextRunFlags = DefaultTextRunFlags);
    static TextRun constructTextRun(const RenderText*, const RenderStyle&,
        ExpansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion);
    static TextRun constructTextRun(const RenderText*, unsigned offset, unsigned length, const RenderStyle&,
        ExpansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion);
    static TextRun constructTextRun(const LChar* characters, int length, const RenderStyle&,
        ExpansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion);
    static TextRun constructTextRun(const UChar* characters, int length, const RenderStyle&,
        ExpansionBehavior = AllowTrailingExpansion | ForbidLeadingExpansion);
    
    LayoutUnit paginationStrut() const;
    void setPaginationStrut(LayoutUnit);

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset() const;
    void setPageLogicalOffset(LayoutUnit);

    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    enum ApplyLayoutDeltaMode { ApplyLayoutDelta, DoNotApplyLayoutDelta };
    LayoutUnit logicalWidthForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.width() : child.height(); }
    LayoutUnit logicalHeightForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.height() : child.width(); }
    LayoutSize logicalSizeForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.size() : child.size().transposedSize(); }
    LayoutUnit logicalTopForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.y() : child.x(); }
    void setLogicalLeftForChild(RenderBox& child, LayoutUnit logicalLeft, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    void setLogicalTopForChild(RenderBox& child, LayoutUnit logicalTop, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    LayoutUnit marginBeforeForChild(const RenderBoxModelObject& child) const { return child.marginBefore(&style()); }
    LayoutUnit marginAfterForChild(const RenderBoxModelObject& child) const { return child.marginAfter(&style()); }
    LayoutUnit marginStartForChild(const RenderBoxModelObject& child) const { return child.marginStart(&style()); }
    LayoutUnit marginEndForChild(const RenderBoxModelObject& child) const { return child.marginEnd(&style()); }
    void setMarginStartForChild(RenderBox& child, LayoutUnit value) const { child.setMarginStart(value, &style()); }
    void setMarginEndForChild(RenderBox& child, LayoutUnit value) const { child.setMarginEnd(value, &style()); }
    void setMarginBeforeForChild(RenderBox& child, LayoutUnit value) const { child.setMarginBefore(value, &style()); }
    void setMarginAfterForChild(RenderBox& child, LayoutUnit value) const { child.setMarginAfter(value, &style()); }
    LayoutUnit collapsedMarginBeforeForChild(const RenderBox& child) const;
    LayoutUnit collapsedMarginAfterForChild(const RenderBox& child) const;

    virtual void updateFirstLetter();
    void getFirstLetter(RenderObject*& firstLetter, RenderElement*& firstLetterContainer, RenderObject* skipObject = nullptr);

    virtual void scrollbarsChanged(bool /*horizontalScrollbarChanged*/, bool /*verticalScrollbarChanged*/) { }

    LayoutUnit logicalLeftOffsetForContent(RenderRegion*) const;
    LayoutUnit logicalRightOffsetForContent(RenderRegion*) const;
    LayoutUnit availableLogicalWidthForContent(RenderRegion* region) const
    { 
        return std::max<LayoutUnit>(0, logicalRightOffsetForContent(region) - logicalLeftOffsetForContent(region));
    }
    LayoutUnit startOffsetForContent(RenderRegion* region) const
    {
        return style().isLeftToRightDirection() ? logicalLeftOffsetForContent(region) : logicalWidth() - logicalRightOffsetForContent(region);
    }
    LayoutUnit endOffsetForContent(RenderRegion* region) const
    {
        return !style().isLeftToRightDirection() ? logicalLeftOffsetForContent(region) : logicalWidth() - logicalRightOffsetForContent(region);
    }
    LayoutUnit logicalLeftOffsetForContent(LayoutUnit blockOffset) const
    {
        return logicalLeftOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit logicalRightOffsetForContent(LayoutUnit blockOffset) const
    {
        return logicalRightOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit availableLogicalWidthForContent(LayoutUnit blockOffset) const
    {
        return availableLogicalWidthForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit startOffsetForContent(LayoutUnit blockOffset) const
    {
        return startOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit endOffsetForContent(LayoutUnit blockOffset) const
    {
        return endOffsetForContent(regionAtBlockOffset(blockOffset));
    }
    LayoutUnit logicalLeftOffsetForContent() const { return isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }
    LayoutUnit logicalRightOffsetForContent() const { return logicalLeftOffsetForContent() + availableLogicalWidth(); }
    LayoutUnit startOffsetForContent() const { return style().isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }
    LayoutUnit endOffsetForContent() const { return !style().isLeftToRightDirection() ? logicalLeftOffsetForContent() : logicalWidth() - logicalRightOffsetForContent(); }

    LayoutUnit logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches&);
    LayoutUnit logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches&);

    LayoutUnit computeStartPositionDeltaForChildAvoidingFloats(const RenderBox& child, LayoutUnit childMarginStart, RenderRegion* = 0);

#ifndef NDEBUG
    void checkPositionedObjectsNeedLayout();
#endif

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    bool canHaveChildren() const override { return true; }
    virtual bool canDropAnonymousBlockChild() const { return true; }

    RenderFlowThread* cachedFlowThreadContainingBlock() const;
    void setCachedFlowThreadContainingBlockNeedsUpdate();
    virtual bool cachedFlowThreadContainingBlockNeedsUpdate() const;

protected:
    RenderFlowThread* locateFlowThreadContainingBlock() const override;
    void willBeDestroyed() override;

    void layout() override;

    void layoutPositionedObjects(bool relayoutChildren, bool fixedPositionObjectsOnly = false);
    virtual void layoutPositionedObject(RenderBox&, bool relayoutChildren, bool fixedPositionObjectsOnly);
    
    void markFixedPositionObjectForLayoutIfNeeded(RenderObject& child);

    LayoutUnit marginIntrinsicLogicalWidthForChild(RenderBox&) const;

    void paint(PaintInfo&, const LayoutPoint&) override;
    void paintObject(PaintInfo&, const LayoutPoint&) override;
    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect);
    enum PaintBlockType { PaintAsBlock, PaintAsInlineBlock };
    bool paintChild(RenderBox&, PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect, PaintBlockType paintType = PaintAsBlock);
   
    LayoutUnit logicalRightOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalRightOffsetForLine(logicalRightFloatOffsetForLine(logicalTop, fixedOffset, logicalHeight), applyTextIndent);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalLeftOffsetForLine(logicalLeftFloatOffsetForLine(logicalTop, fixedOffset, logicalHeight), applyTextIndent);
    }

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computePreferredLogicalWidths() override;
    
    Optional<int> firstLineBaseline() const override;
    Optional<int> inlineBlockBaseline(LineDirectionMode) const override;

    // Delay updating scrollbars until endAndCommitUpdateScrollInfoAfterLayoutTransaction() is called. These functions are used
    // when a flexbox is laying out its descendants. If multiple calls are made to beginUpdateScrollInfoAfterLayoutTransaction()
    // then endAndCommitUpdateScrollInfoAfterLayoutTransaction() will do nothing until it is called the same number of times.
    void beginUpdateScrollInfoAfterLayoutTransaction();
    void endAndCommitUpdateScrollInfoAfterLayoutTransaction();

    void removeFromUpdateScrollInfoAfterLayoutTransaction();

    void updateScrollInfoAfterLayout();

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual bool hasLineIfEmpty() const;
    
    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

    bool childBoxIsUnsplittableForFragmentation(const RenderBox& child) const;

public:
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false);
    void clearLayoutOverflow();
    
    // Adjust from painting offsets to the local coords of this renderer
    void offsetForContents(LayoutPoint&) const;
    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    RenderBlock* firstLineBlock() const override;

protected:
    virtual void addOverflowFromChildren();
    // FIXME-BLOCKFLOW: Remove virtualization when all callers have moved to RenderBlockFlow
    virtual void addOverflowFromInlineChildren() { }
    void addOverflowFromBlockChildren();
    void addOverflowFromPositionedObjects();
    void addVisualOverflowFromTheme();

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override;
    virtual void addFocusRingRectsForInlineChildren(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);

    void computeRegionRangeForBoxChild(const RenderBox&) const;

    void estimateRegionRangeForBoxChild(const RenderBox&) const;
    bool updateRegionRangeForBoxChild(const RenderBox&) const;

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox&);

    void preparePaginationBeforeBlockLayout(bool&);

private:
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit) const { return fixedOffset; };
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit) const { return fixedOffset; }
    LayoutUnit adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const;
    LayoutUnit adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const;

    const char* renderName() const override;

    bool isInlineBlockOrInlineTable() const final { return isInline() && isReplaced(); }

    void makeChildrenNonInline(RenderObject* insertionPoint = nullptr);
    virtual void removeLeftoverAnonymousBlock(RenderBlock* child);

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void moveAllChildrenIncludingFloatsTo(RenderBlock& toBlock, bool fullRemoveInsert) { moveAllChildrenTo(&toBlock, fullRemoveInsert); }

    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild) override;

    bool isSelfCollapsingBlock() const override;
    virtual bool childrenPreventSelfCollapsing() const;
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual bool hasLines() const { return false; }

    void insertIntoTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*&, TrackedContainerMap*&, bool forceNewEntry = false);
    static void removeFromTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);

    void createFirstLetterRenderer(RenderElement* firstLetterBlock, RenderText* currentTextChild);
    void updateFirstLetterStyle(RenderElement* firstLetterBlock, RenderObject* firstLetterContainer);

    Node* nodeForHitTest() const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool) { }
    virtual void paintInlineChildren(PaintInfo&, const LayoutPoint&) { }
    void paintContents(PaintInfo&, const LayoutPoint&);
    virtual void paintColumnRules(PaintInfo&, const LayoutPoint&) { };
    void paintSelection(PaintInfo&, const LayoutPoint&);
    void paintCaret(PaintInfo&, const LayoutPoint&, CaretType);

    bool avoidsFloats() const override;

    virtual bool hitTestContents(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    // FIXME-BLOCKFLOW: Remove virtualization when all callers have moved to RenderBlockFlow
    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }
    virtual bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction) { return false; }

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset);

    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const final;
    const RenderStyle& outlineStyleForRepaint() const final;

    RenderElement* hoverAncestor() const final;
    void updateDragState(bool dragOn) final;
    void childBecameNonInline(RenderElement&) final;

    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool /*clipToVisibleContent*/) final
    {
        return selectionGapRectsForRepaint(repaintContainer);
    }
    bool shouldPaintSelectionGaps() const final;
    bool isSelectionRoot() const;
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
    virtual void clipOutFloatingObjects(RenderBlock&, const PaintInfo*, const LayoutPoint&, const LayoutSize&) { };
    friend class LogicalSelectionOffsetCaches;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    void paintContinuationOutlines(PaintInfo&, const LayoutPoint&);

    LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = 0) final;
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual VisiblePosition positionForPointWithInlineChildren(const LayoutPoint&, const RenderRegion*);

    RenderPtr<RenderBlock> clone() const;
    RenderBlock* continuationBefore(RenderObject* beforeChild);

    RenderFlowThread* updateCachedFlowThreadContainingBlock(RenderFlowThread*) const;

private:
    bool hasRareData() const;
    
protected:
    void dirtyForLayoutFromPercentageHeightDescendants();

protected:
    bool recomputeLogicalWidth();
    
public:
    LayoutUnit offsetFromLogicalTopOfFirstPage() const override;
    RenderRegion* regionAtBlockOffset(LayoutUnit) const;

    // FIXME: This is temporary to allow us to move code from RenderBlock into RenderBlockFlow that accesses member variables that we haven't moved out of
    // RenderBlock yet.
    friend class RenderBlockFlow;
    // FIXME-BLOCKFLOW: Remove this when the line layout stuff has all moved out of RenderBlock
    friend class LineBreaker;

    // RenderRubyBase objects need to be able to split and merge, moving their children around
    // (calling moveChildTo, moveAllChildrenTo, and makeChildrenNonInline).
    friend class RenderRubyBase;

private:
    // Used to store state between styleWillChange and styleDidChange
    static bool s_canPropagateFloatIntoSibling;
};

LayoutUnit blockDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock);
LayoutUnit inlineDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock);
VisiblePosition positionForPointRespectingEditingBoundaries(RenderBlock&, RenderBox&, const LayoutPoint&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderBlock, isRenderBlock())

#endif // RenderBlock_h
