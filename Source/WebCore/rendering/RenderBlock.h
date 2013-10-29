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

#include "ColumnInfo.h"
#include "FloatingObjects.h"
#include "GapRects.h"
#include "PODIntervalTree.h"
#include "RenderBox.h"
#include "RootInlineBox.h"
#include "TextBreakIterator.h"
#include "TextRun.h"
#include <wtf/OwnPtr.h>
#include <wtf/ListHashSet.h>

#if ENABLE(CSS_SHAPES)
#include "ShapeInsideInfo.h"
#include "ShapeValue.h"
#endif

namespace WebCore {

class BidiContext;
class InlineIterator;
class LayoutStateMaintainer;
class LineLayoutState;
class LineWidth;
class LogicalSelectionOffsetCaches;
class RenderInline;
class RenderText;

struct BidiRun;
struct PaintInfo;
class LineInfo;
class RenderRubyRun;
#if ENABLE(CSS_SHAPES)
class BasicShape;
#endif
class TextLayout;
class WordMeasurement;

template <class Iterator, class Run> class BidiResolver;
template <class Run> class BidiRunList;
template <class Iterator> struct MidpointState;
typedef BidiResolver<InlineIterator, BidiRun> InlineBidiResolver;
typedef MidpointState<InlineIterator> LineMidpointState;
typedef WTF::ListHashSet<RenderBox*, 16> TrackedRendererListHashSet;
typedef WTF::HashMap<const RenderBlock*, OwnPtr<TrackedRendererListHashSet>> TrackedDescendantsMap;
typedef WTF::HashMap<const RenderBox*, OwnPtr<HashSet<RenderBlock*>>> TrackedContainerMap;
typedef Vector<WordMeasurement, 64> WordMeasurements;

enum CaretType { CursorCaret, DragCaret };
enum ContainingBlockState { NewContainingBlock, SameContainingBlock };

#if ENABLE(IOS_TEXT_AUTOSIZING)
enum LineCount {
    NOT_SET = 0, NO_LINE = 1, ONE_LINE = 2, MULTI_LINE = 3
};
#endif

enum TextRunFlag {
    DefaultTextRunFlags = 0,
    RespectDirection = 1 << 0,
    RespectDirectionOverride = 1 << 1
};

typedef unsigned TextRunFlags;

class RenderBlock : public RenderBox {
public:
    friend class LineLayoutState;
#ifndef NDEBUG
    // Used by the PODIntervalTree for debugging the FloatingObject.
    template <class> friend struct ValueToString;
#endif

protected:
    RenderBlock(Element&, PassRef<RenderStyle>, unsigned baseTypeFlags);
    RenderBlock(Document&, PassRef<RenderStyle>, unsigned baseTypeFlags);
    virtual ~RenderBlock();

public:
    bool beingDestroyed() const { return m_beingDestroyed; }

    // These two functions are overridden for inline-block.
    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const OVERRIDE FINAL;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const OVERRIDE;

    LayoutUnit minLineHeightForReplacedRenderer(bool isFirstLine, LayoutUnit replacedHeight) const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void deleteLines();

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject&) OVERRIDE;

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0);

    void invalidateLineLayoutPath();

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

    void setHasMarginBeforeQuirk(bool b) { m_hasMarginBeforeQuirk = b; }
    void setHasMarginAfterQuirk(bool b) { m_hasMarginAfterQuirk = b; }

    bool hasMarginBeforeQuirk() const { return m_hasMarginBeforeQuirk; }
    bool hasMarginAfterQuirk() const { return m_hasMarginAfterQuirk; }

    bool hasMarginBeforeQuirk(const RenderBox& child) const;
    bool hasMarginAfterQuirk(const RenderBox& child) const;

    bool generatesLineBoxesForInlineChild(RenderObject*);

    void markPositionedObjectsForLayout();
    virtual void markForPaginationRelayoutIfNeeded() OVERRIDE FINAL;
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all of the line layout code has been moved out of RenderBlock
    virtual bool containsFloats() const { return false; }

    // Versions that can compute line offsets with the region and page offset passed in. Used for speed to avoid having to
    // compute the region all over again when you already know it.
    LayoutUnit availableLogicalWidthForLine(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return std::max<LayoutUnit>(0, logicalRightOffsetForLine(position, shouldIndentText, region, logicalHeight)
            - logicalLeftOffsetForLine(position, shouldIndentText, region, logicalHeight));
    }
    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const 
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(region), shouldIndentText, 0, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const 
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(region), shouldIndentText, 0, logicalHeight);
    }
    LayoutUnit startOffsetForLine(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return style().isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, region, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, region, logicalHeight);
    }
    LayoutUnit endOffsetForLine(LayoutUnit position, bool shouldIndentText, RenderRegion* region, LayoutUnit logicalHeight = 0) const
    {
        return !style().isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, region, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, region, logicalHeight);
    }

    LayoutUnit availableLogicalWidthForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return availableLogicalWidthForLine(position, shouldIndentText, regionAtBlockOffset(position), logicalHeight);
    }
    LayoutUnit logicalRightOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const 
    {
        return logicalRightOffsetForLine(position, logicalRightOffsetForContent(position), shouldIndentText, 0, logicalHeight);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const 
    {
        return logicalLeftOffsetForLine(position, logicalLeftOffsetForContent(position), shouldIndentText, 0, logicalHeight);
    }
    LayoutUnit pixelSnappedLogicalLeftOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const 
    {
        return roundToInt(logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight));
    }
    LayoutUnit pixelSnappedLogicalRightOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const 
    {
        // FIXME: Multicolumn layouts break carrying over subpixel values to the logical right offset because the lines may be shifted
        // by a subpixel value for all but the first column. This can lead to the actual pixel snapped width of the column being off
        // by one pixel when rendered versus layed out, which can result in the line being clipped. For now, we have to floor.
        // https://bugs.webkit.org/show_bug.cgi?id=105461
        return floorToInt(logicalRightOffsetForLine(position, shouldIndentText, logicalHeight));
    }
    LayoutUnit startOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return style().isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, logicalHeight);
    }
    LayoutUnit endOffsetForLine(LayoutUnit position, bool shouldIndentText, LayoutUnit logicalHeight = 0) const
    {
        return !style().isLeftToRightDirection() ? logicalLeftOffsetForLine(position, shouldIndentText, logicalHeight)
            : logicalWidth() - logicalRightOffsetForLine(position, shouldIndentText, logicalHeight);
    }

    LayoutUnit startAlignedOffsetForLine(LayoutUnit position, bool shouldIndentText);
    LayoutUnit textIndentOffset() const;

    virtual VisiblePosition positionForPoint(const LayoutPoint&) OVERRIDE;
    
    // Block flows subclass availableWidth to handle multi column layout (shrinking the width available to children when laying out.)
    virtual LayoutUnit availableLogicalWidth() const OVERRIDE FINAL;

    LayoutPoint flipForWritingModeIncludingColumns(const LayoutPoint&) const;
    void adjustStartEdgeForWritingModeIncludingColumns(LayoutRect&) const;

    GapRects selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer);
    LayoutRect logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        RenderObject* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    LayoutRect logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        RenderObject* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);
    void getSelectionGapInfo(SelectionState, bool& leftGap, bool& rightGap);
    RenderBlock* blockBeforeWithinSelectionRoot(LayoutSize& offset) const;

    LayoutRect logicalRectToPhysicalRect(const LayoutPoint& physicalPosition, const LayoutRect& logicalRect);

    void adjustRectForColumns(LayoutRect&) const;
    virtual void adjustForColumns(LayoutSize&, const LayoutPoint&) const OVERRIDE FINAL;
    void adjustForColumnRect(LayoutSize& offset, const LayoutPoint& locationInContainer) const;

    void addContinuationWithOutline(RenderInline*);
    bool paintsContinuationOutline(RenderInline*);

    virtual RenderBoxModelObject* virtualContinuation() const OVERRIDE FINAL { return continuation(); }
    bool isAnonymousBlockContinuation() const { return isAnonymousBlock() && continuation(); }
    RenderInline* inlineElementContinuation() const;
    RenderBlock* blockElementContinuation() const;

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    static RenderBlock* createAnonymousWithParentRendererAndDisplay(const RenderObject*, EDisplay = BLOCK);
    static RenderBlock* createAnonymousColumnsWithParentRenderer(const RenderObject*);
    static RenderBlock* createAnonymousColumnSpanWithParentRenderer(const RenderObject*);
    RenderBlock* createAnonymousBlock(EDisplay display = BLOCK) const { return createAnonymousWithParentRendererAndDisplay(this, display); }
    RenderBlock* createAnonymousColumnsBlock() const { return createAnonymousColumnsWithParentRenderer(this); }
    RenderBlock* createAnonymousColumnSpanBlock() const { return createAnonymousColumnSpanWithParentRenderer(this); }
    static void collapseAnonymousBoxChild(RenderBlock* parent, RenderBlock* child);

    virtual RenderBox* createAnonymousBoxWithSameTypeAs(const RenderObject* parent) const OVERRIDE;

    static bool shouldSkipCreatingRunsForObject(RenderObject* obj)
    {
        return obj->isFloating() || (obj->isOutOfFlowPositioned() && !obj->style().isOriginalDisplayInlineType() && !obj->container()->isRenderInline());
    }

    static TextRun constructTextRun(RenderObject* context, const Font&, const String&, const RenderStyle&,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion, TextRunFlags = DefaultTextRunFlags);

    static TextRun constructTextRun(RenderObject* context, const Font&, const RenderText*, const RenderStyle&,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    static TextRun constructTextRun(RenderObject* context, const Font&, const RenderText*, unsigned offset, unsigned length, const RenderStyle&,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    static TextRun constructTextRun(RenderObject* context, const Font&, const RenderText*, unsigned offset, const RenderStyle&,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

#if ENABLE(8BIT_TEXTRUN)
    static TextRun constructTextRun(RenderObject* context, const Font&, const LChar* characters, int length, const RenderStyle&,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);
#endif

    static TextRun constructTextRun(RenderObject* context, const Font&, const UChar* characters, int length, const RenderStyle&,
        TextRun::ExpansionBehavior = TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion);

    ColumnInfo* columnInfo() const;
    int columnGap() const;

    void updateColumnInfoFromStyle(RenderStyle*);
    
    LayoutUnit initialBlockOffsetForPainting() const;
    LayoutUnit blockDeltaForPaintingNextColumn() const;

    // These two functions take the ColumnInfo* to avoid repeated lookups of the info in the global HashMap.
    unsigned columnCount(ColumnInfo*) const;
    LayoutRect columnRectAt(ColumnInfo*, unsigned) const;

    LayoutUnit paginationStrut() const { return m_rareData ? m_rareData->m_paginationStrut : LayoutUnit(); }
    void setPaginationStrut(LayoutUnit);

    // The page logical offset is the object's offset from the top of the page in the page progression
    // direction (so an x-offset in vertical text and a y-offset for horizontal text).
    LayoutUnit pageLogicalOffset() const { return m_rareData ? m_rareData->m_pageLogicalOffset : LayoutUnit(); }
    void setPageLogicalOffset(LayoutUnit);

    // Accessors for logical width/height and margins in the containing block's block-flow direction.
    enum ApplyLayoutDeltaMode { ApplyLayoutDelta, DoNotApplyLayoutDelta };
    LayoutUnit logicalWidthForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.width() : child.height(); }
    LayoutUnit logicalHeightForChild(const RenderBox& child) const { return isHorizontalWritingMode() ? child.height() : child.width(); }
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

    void updateLogicalWidthForAlignment(const ETextAlign&, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount);

    virtual void updateFirstLetter();

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

    LayoutUnit computeStartPositionDeltaForChildAvoidingFloats(const RenderBox& child, LayoutUnit childMarginStart, RenderRegion* = 0);

    void placeRunInIfNeeded(RenderObject& newChild);
    bool runInIsPlacedIntoSiblingBlock(RenderObject& runIn);

#ifndef NDEBUG
    void checkPositionedObjectsNeedLayout();
    virtual void showLineTreeAndMark(const InlineBox* = nullptr, const char* = nullptr, const InlineBox* = nullptr, const char* = nullptr, const RenderObject* = nullptr) const;
#endif

#if ENABLE(IOS_TEXT_AUTOSIZING)
    int immediateLineCount();
    void adjustComputedFontSizes(float size, float visibleWidth);
    void resetComputedFontSize()
    {
        m_widthForTextAutosizing = -1;
        m_lineCountForTextAutosizing = NOT_SET;
    }
#endif

#if ENABLE(CSS_SHAPES)
    ShapeInsideInfo* ensureShapeInsideInfo()
    {
        if (!m_rareData || !m_rareData->m_shapeInsideInfo)
            setShapeInsideInfo(ShapeInsideInfo::createInfo(this));
        return m_rareData->m_shapeInsideInfo.get();
    }

    ShapeInsideInfo* shapeInsideInfo() const
    {
        if (!m_rareData || !m_rareData->m_shapeInsideInfo)
            return 0;
        return ShapeInsideInfo::isEnabledFor(this) ? m_rareData->m_shapeInsideInfo.get() : 0;
    }
    void setShapeInsideInfo(PassOwnPtr<ShapeInsideInfo> value)
    {
        if (!m_rareData)
            m_rareData = adoptPtr(new RenderBlockRareData());
        m_rareData->m_shapeInsideInfo = value;
    }
    void markShapeInsideDescendantsForLayout();
    ShapeInsideInfo* layoutShapeInsideInfo() const;
    bool allowsShapeInsideInfoSharing() const { return !isInline() && !isFloating(); }
    LayoutSize logicalOffsetFromShapeAncestorContainer(const RenderBlock* container) const;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) OVERRIDE;
#endif

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) OVERRIDE;

protected:
    virtual void willBeDestroyed() OVERRIDE;

    virtual void layout() OVERRIDE;

    void layoutPositionedObjects(bool relayoutChildren, bool fixedPositionObjectsOnly = false);
    void markFixedPositionObjectForLayoutIfNeeded(RenderObject& child);

    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual void paintObject(PaintInfo&, const LayoutPoint&) OVERRIDE;
    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect);
    bool paintChild(RenderBox&, PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect);
   
    LayoutUnit logicalRightOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining = 0, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalRightOffsetForLine(logicalRightFloatOffsetForLine(logicalTop, fixedOffset, heightRemaining, logicalHeight, ShapeOutsideFloatShapeOffset), applyTextIndent);
    }
    LayoutUnit logicalLeftOffsetForLine(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining = 0, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalLeftOffsetForLine(logicalLeftFloatOffsetForLine(logicalTop, fixedOffset, heightRemaining, logicalHeight, ShapeOutsideFloatShapeOffset), applyTextIndent);
    }
    LayoutUnit logicalRightOffsetForLineIgnoringShapeOutside(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining = 0, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalRightOffsetForLine(logicalRightFloatOffsetForLine(logicalTop, fixedOffset, heightRemaining, logicalHeight, ShapeOutsideFloatMarginBoxOffset), applyTextIndent);
    }
    LayoutUnit logicalLeftOffsetForLineIgnoringShapeOutside(LayoutUnit logicalTop, LayoutUnit fixedOffset, bool applyTextIndent, LayoutUnit* heightRemaining = 0, LayoutUnit logicalHeight = 0) const
    {
        return adjustLogicalLeftOffsetForLine(logicalLeftFloatOffsetForLine(logicalTop, fixedOffset, heightRemaining, logicalHeight, ShapeOutsideFloatMarginBoxOffset), applyTextIndent);
    }

    virtual ETextAlign textAlignmentForLine(bool endsWithSoftBreak) const;
    virtual void adjustInlineDirectionLineBounds(int /* expansionOpportunityCount */, float& /* logicalLeft */, float& /* logicalWidth */) const { }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;
    void adjustIntrinsicLogicalWidthsForColumns(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    virtual int firstLineBaseline() const OVERRIDE;
    virtual int inlineBlockBaseline(LineDirectionMode) const OVERRIDE;

    // Delay update scrollbar until finishDelayRepaint() will be
    // called. This function is used when a flexbox is laying out its
    // descendant. If multiple calls are made to startDelayRepaint(),
    // finishDelayRepaint() will do nothing until finishDelayRepaint()
    // is called the same number of times.
    static void startDelayUpdateScrollInfo();
    static void finishDelayUpdateScrollInfo();

    void updateScrollInfoAfterLayout();
    void removeFromDelayedUpdateScrollInfoSet();

    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle) OVERRIDE;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    virtual bool hasLineIfEmpty() const;
    
    bool simplifiedLayout();
    virtual void simplifiedNormalFlowLayout();

    void setDesiredColumnCountAndWidth(int, LayoutUnit);

public:
    virtual void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false);
    void clearLayoutOverflow();
protected:
    virtual void addOverflowFromChildren();
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void addOverflowFromInlineChildren() { }
    void addOverflowFromBlockChildren();
    void addOverflowFromPositionedObjects();
    void addVisualOverflowFromTheme();

    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) OVERRIDE;
    virtual void addFocusRingRectsForInlineChildren(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer);

    bool updateShapesBeforeBlockLayout();
    void updateShapesAfterBlockLayout(bool heightChanged = false);
    void computeRegionRangeForBoxChild(const RenderBox&) const;

    void estimateRegionRangeForBoxChild(const RenderBox&) const;
    bool updateRegionRangeForBoxChild(const RenderBox&) const;

    void updateBlockChildDirtyBitsBeforeLayout(bool relayoutChildren, RenderBox&);

    virtual void checkForPaginationLogicalHeightChange(LayoutUnit& pageLogicalHeight, bool& pageLogicalHeightChanged, bool& hasSpecifiedPageLogicalHeight);
    void prepareShapesAndPaginationBeforeBlockLayout(bool&);

private:
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalRightFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit*, LayoutUnit, ShapeOutsideFloatOffsetMode) const { return fixedOffset; };
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual LayoutUnit logicalLeftFloatOffsetForLine(LayoutUnit, LayoutUnit fixedOffset, LayoutUnit*, LayoutUnit, ShapeOutsideFloatOffsetMode) const { return fixedOffset; }
    LayoutUnit adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const;
    LayoutUnit adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats, bool applyTextIndent) const;

#if ENABLE(CSS_SHAPES)
    void computeShapeSize();
    void updateShapeInsideInfoAfterStyleChange(const ShapeValue*, const ShapeValue* oldShape);
    void relayoutShapeDescendantIfMoved(RenderBlock* child, LayoutSize offset);
#endif

    virtual const char* renderName() const OVERRIDE;

    virtual bool isInlineBlockOrInlineTable() const OVERRIDE FINAL { return isInline() && isReplaced(); }
    virtual bool canHaveChildren() const OVERRIDE { return true; }

    void makeChildrenNonInline(RenderObject* insertionPoint = nullptr);
    virtual void removeLeftoverAnonymousBlock(RenderBlock* child);

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void moveAllChildrenIncludingFloatsTo(RenderBlock* toBlock, bool fullRemoveInsert) { moveAllChildrenTo(toBlock, fullRemoveInsert); }

    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild) OVERRIDE;
    void addChildToAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild);

    void addChildIgnoringAnonymousColumnBlocks(RenderObject* newChild, RenderObject* beforeChild = 0);
    
    virtual bool isSelfCollapsingBlock() const OVERRIDE FINAL;
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual bool hasLines() const { return false; }

    void insertIntoTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);
    static void removeFromTrackedRendererMaps(RenderBox& descendant, TrackedDescendantsMap*&, TrackedContainerMap*&);

    // Called to lay out the legend for a fieldset or the ruby text of a ruby run.
    virtual RenderObject* layoutSpecialExcludedChild(bool /*relayoutChildren*/) { return 0; }

    void createFirstLetterRenderer(RenderObject* firstLetterBlock, RenderText* currentTextChild);
    void updateFirstLetterStyle(RenderObject* firstLetterBlock, RenderObject* firstLetterContainer);

    Node* nodeForHitTest() const;

    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void paintFloats(PaintInfo&, const LayoutPoint&, bool) { }
    virtual void paintInlineChildren(PaintInfo&, const LayoutPoint&) { }
    void paintContents(PaintInfo&, const LayoutPoint&);
    void paintColumnContents(PaintInfo&, const LayoutPoint&, bool paintFloats = false);
    void paintColumnRules(PaintInfo&, const LayoutPoint&);
    void paintSelection(PaintInfo&, const LayoutPoint&);
    void paintCaret(PaintInfo&, const LayoutPoint&, CaretType);

    virtual bool avoidsFloats() const OVERRIDE;

    bool hitTestColumns(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    virtual bool hitTestContents(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual bool hitTestFloats(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&) { return false; }
    virtual bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint&, HitTestAction) { return false; }

    virtual bool isPointInOverflowControl(HitTestResult&, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset);

    // FIXME: Make this method const so we can remove the const_cast in computeIntrinsicLogicalWidths.
    void computeInlinePreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth);
    void computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    // Obtains the nearest enclosing block (including this block) that contributes a first-line style to our inline
    // children.
    virtual RenderBlock* firstLineBlock() const OVERRIDE;

    virtual LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const OVERRIDE FINAL;
    virtual RenderStyle* outlineStyleForRepaint() const OVERRIDE FINAL;
    
    virtual RenderElement* hoverAncestor() const OVERRIDE FINAL;
    virtual void updateDragState(bool dragOn) OVERRIDE FINAL;
    virtual void childBecameNonInline(RenderObject* child) OVERRIDE FINAL;

    virtual LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool /*clipToVisibleContent*/) OVERRIDE FINAL
    {
        return selectionGapRectsForRepaint(repaintContainer);
    }
    virtual bool shouldPaintSelectionGaps() const OVERRIDE FINAL;
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
    LayoutUnit logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches&);
    LayoutUnit logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches&);
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual void clipOutFloatingObjects(RenderBlock&, const PaintInfo*, const LayoutPoint&, const LayoutSize&) { };
    friend class LogicalSelectionOffsetCaches;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const OVERRIDE;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const OVERRIDE;

    LayoutUnit desiredColumnWidth() const;
    unsigned desiredColumnCount() const;

    void paintContinuationOutlines(PaintInfo&, const LayoutPoint&);

    virtual LayoutRect localCaretRect(InlineBox*, int caretOffset, LayoutUnit* extraWidthToEndOfLine = 0) OVERRIDE FINAL;

    void adjustPointToColumnContents(LayoutPoint&) const;
    
    // FIXME-BLOCKFLOW: Remove virtualizaion when all callers have moved to RenderBlockFlow
    virtual VisiblePosition positionForPointWithInlineChildren(const LayoutPoint&);

    virtual void calcColumnWidth();
    void makeChildrenAnonymousColumnBlocks(RenderObject* beforeChild, RenderBlock* newBlockBox, RenderObject* newChild);

    bool expandsToEncloseOverhangingFloats() const;

    void splitBlocks(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                     RenderObject* beforeChild, RenderBoxModelObject* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderBoxModelObject* oldCont);
    RenderBlock* clone() const;
    RenderBlock* continuationBefore(RenderObject* beforeChild);
    RenderBlock* containingColumnsBlock(bool allowAnonymousColumnBlock = true);
    RenderBlock* columnsBlockForSpanningElement(RenderObject* newChild);

    RenderBoxModelObject& createReplacementRunIn(RenderBoxModelObject& runIn);
    void moveRunInUnderSiblingBlockIfNeeded(RenderObject& runIn);
    void moveRunInToOriginalPosition(RenderObject& runIn);

protected:
    void dirtyForLayoutFromPercentageHeightDescendants();
    
    void determineLogicalLeftPositionForChild(RenderBox& child, ApplyLayoutDeltaMode = DoNotApplyLayoutDelta);
    
    virtual ColumnInfo::PaginationUnit paginationUnit() const;

protected:
    // Adjust from painting offsets to the local coords of this renderer
    void offsetForContents(LayoutPoint&) const;

    virtual bool requiresColumns(int desiredColumnCount) const;

    virtual bool updateLogicalWidthAndColumnWidth();

    virtual bool canCollapseAnonymousBlockChild() const { return true; }

public:
    virtual LayoutUnit offsetFromLogicalTopOfFirstPage() const OVERRIDE;
    RenderRegion* regionAtBlockOffset(LayoutUnit) const;

    // FIXME: This is temporary to allow us to move code from RenderBlock into RenderBlockFlow that accesses member variables that we haven't moved out of
    // RenderBlock yet.
    friend class RenderBlockFlow;
    // FIXME-BLOCKFLOW: Remove this when the line layout stuff has all moved out of RenderBlock
    friend class LineBreaker;

public:
    // Allocated only when some of these fields have non-default values
    struct RenderBlockRareData {
        WTF_MAKE_NONCOPYABLE(RenderBlockRareData); WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderBlockRareData() 
            : m_paginationStrut(0)
            , m_pageLogicalOffset(0)
        { 
        }

        LayoutUnit m_paginationStrut;
        LayoutUnit m_pageLogicalOffset;

#if ENABLE(CSS_SHAPES)
        OwnPtr<ShapeInsideInfo> m_shapeInsideInfo;
#endif
     };

protected:
    OwnPtr<RenderBlockRareData> m_rareData;

    mutable signed m_lineHeight : 25;
    unsigned m_hasMarginBeforeQuirk : 1; // Note these quirk values can't be put in RenderBlockRareData since they are set too frequently.
    unsigned m_hasMarginAfterQuirk : 1;
    unsigned m_beingDestroyed : 1;
    unsigned m_hasMarkupTruncation : 1;
    unsigned m_hasBorderOrPaddingLogicalWidthChanged : 1;
    enum LineLayoutPath { UndeterminedPath, SimpleLinesPath, LineBoxesPath, ForceLineBoxesPath };
    unsigned m_lineLayoutPath : 2;

#if ENABLE(IOS_TEXT_AUTOSIZING)
    int m_widthForTextAutosizing;
    unsigned m_lineCountForTextAutosizing : 2;
#endif
    
    // RenderRubyBase objects need to be able to split and merge, moving their children around
    // (calling moveChildTo, moveAllChildrenTo, and makeChildrenNonInline).
    friend class RenderRubyBase;

private:
    // Used to store state between styleWillChange and styleDidChange
    static bool s_canPropagateFloatIntoSibling;
};

RENDER_OBJECT_TYPE_CASTS(RenderBlock, isRenderBlock())

LayoutUnit blockDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock);
LayoutUnit inlineDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock);
VisiblePosition positionForPointRespectingEditingBoundaries(RenderBlock&, RenderBox&, const LayoutPoint&);

} // namespace WebCore

#endif // RenderBlock_h
