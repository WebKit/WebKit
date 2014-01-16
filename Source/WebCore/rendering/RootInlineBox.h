/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
 *
 */

#ifndef RootInlineBox_h
#define RootInlineBox_h

#include "BidiContext.h"
#include "InlineFlowBox.h"

namespace WebCore {

class EllipsisBox;
class HitTestResult;
class LogicalSelectionOffsetCaches;
class RenderBlockFlow;
class RenderRegion;

struct BidiStatus;
struct GapRects;

class RootInlineBox : public InlineFlowBox {
public:
    explicit RootInlineBox(RenderBlockFlow&);
    virtual ~RootInlineBox();

    RenderBlockFlow& blockFlow() const;

    void detachEllipsisBox();

    RootInlineBox* nextRootBox() const;
    RootInlineBox* prevRootBox() const;

    virtual void adjustPosition(float dx, float dy) override final;

    LayoutUnit lineTop() const { return m_lineTop; }
    LayoutUnit lineBottom() const { return m_lineBottom; }

    LayoutUnit lineTopWithLeading() const { return m_lineTopWithLeading; }
    LayoutUnit lineBottomWithLeading() const { return m_lineBottomWithLeading; }
    
    LayoutUnit paginationStrut() const { return m_paginationStrut; }
    void setPaginationStrut(LayoutUnit strut) { m_paginationStrut = strut; }

    bool isFirstAfterPageBreak() const { return m_isFirstAfterPageBreak; }
    void setIsFirstAfterPageBreak(bool isFirstAfterPageBreak) { m_isFirstAfterPageBreak = isFirstAfterPageBreak; }

    LayoutUnit paginatedLineWidth() const { return m_paginatedLineWidth; }
    void setPaginatedLineWidth(LayoutUnit width) { m_paginatedLineWidth = width; }

    // It should not be assumed the containingRegion() is always valid.
    // It can also be nullptr if the flow has no region chain.
    RenderRegion* containingRegion() const;
    void setContainingRegion(RenderRegion&);
    void clearContainingRegion();

    LayoutUnit selectionTop() const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const { return std::max<LayoutUnit>(0, selectionBottom() - selectionTop()); }

    LayoutUnit selectionTopAdjustedForPrecedingBlock() const;
    LayoutUnit selectionHeightAdjustedForPrecedingBlock() const { return std::max<LayoutUnit>(0, selectionBottom() - selectionTopAdjustedForPrecedingBlock()); }

    int blockDirectionPointInLine() const;

    LayoutUnit alignBoxesInBlockDirection(LayoutUnit heightOfBlock, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    void setLineTopBottomPositions(LayoutUnit top, LayoutUnit bottom, LayoutUnit topWithLeading, LayoutUnit bottomWithLeading)
    { 
        m_lineTop = top; 
        m_lineBottom = bottom;
        m_lineTopWithLeading = topWithLeading;
        m_lineBottomWithLeading = bottomWithLeading;
    }

    RenderObject* lineBreakObj() const { return m_lineBreakObj; }
    BidiStatus lineBreakBidiStatus() const;
    void setLineBreakInfo(RenderObject*, unsigned breakPos, const BidiStatus&);

    unsigned lineBreakPos() const { return m_lineBreakPos; }
    void setLineBreakPos(unsigned p) { m_lineBreakPos = p; }

    using InlineBox::endsWithBreak;
    using InlineBox::setEndsWithBreak;

    void childRemoved(InlineBox* box);

    bool lineCanAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth);
    // Return the truncatedWidth, the width of the truncated text + ellipsis.
    float placeEllipsis(const AtomicString& ellipsisStr, bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, InlineBox* markupBox = 0);
    // Return the position of the EllipsisBox or -1.
    virtual float placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox) override final;

    using InlineBox::hasEllipsisBox;
    EllipsisBox* ellipsisBox() const;

    void paintEllipsisBox(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) const;

    virtual void clearTruncation() override final;

    bool isHyphenated() const;

    virtual int baselinePosition(FontBaseline baselineType) const override final;
    virtual LayoutUnit lineHeight() const override final;

#if PLATFORM(MAC)
    void addHighlightOverflow();
    void paintCustomHighlight(PaintInfo&, const LayoutPoint&, const AtomicString& highlightType);
#endif

    virtual void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom) override final;

    using InlineBox::hasSelectedChildren;
    using InlineBox::setHasSelectedChildren;

    virtual RenderObject::SelectionState selectionState() override final;
    InlineBox* firstSelectedBox();
    InlineBox* lastSelectedBox();

    GapRects lineSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit selTop, LayoutUnit selHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);

    IntRect computeCaretRect(float logicalLeftPosition, unsigned caretWidth, LayoutUnit* extraWidthToEndOfLine) const;

    InlineBox* closestLeafChildForPoint(const IntPoint&, bool onlyEditableLeaves);
    InlineBox* closestLeafChildForLogicalLeftPosition(int, bool onlyEditableLeaves = false);

    void appendFloat(RenderBox& floatingBox)
    {
        ASSERT(!isDirty());
        if (m_floats)
            m_floats->append(&floatingBox);
        else
            m_floats = adoptPtr(new Vector<RenderBox*>(1, &floatingBox));
    }

    Vector<RenderBox*>* floatsPtr() { ASSERT(!isDirty()); return m_floats.get(); }

    virtual void extractLineBoxFromRenderObject() override final;
    virtual void attachLineBoxToRenderObject() override final;
    virtual void removeLineBoxFromRenderObject() override final;
    
    FontBaseline baselineType() const { return static_cast<FontBaseline>(m_baselineType); }

    bool hasAnnotationsBefore() const { return m_hasAnnotationsBefore; }
    bool hasAnnotationsAfter() const { return m_hasAnnotationsAfter; }

    LayoutRect paddedLayoutOverflowRect(LayoutUnit endPadding) const;

    void ascentAndDescentForBox(InlineBox*, GlyphOverflowAndFallbackFontsMap&, int& ascent, int& descent, bool& affectsAscent, bool& affectsDescent) const;
    LayoutUnit verticalPositionForBox(InlineBox*, VerticalPositionCache&);
    bool includeLeadingForBox(InlineBox*) const;
    bool includeFontForBox(InlineBox*) const;
    bool includeGlyphsForBox(InlineBox*) const;
    bool includeMarginForBox(InlineBox*) const;
    bool fitsToGlyphs() const;
    bool includesRootLineBoxFontOrLeading() const;
    
    LayoutUnit logicalTopVisualOverflow() const
    {
        return InlineFlowBox::logicalTopVisualOverflow(lineTop());
    }
    LayoutUnit logicalBottomVisualOverflow() const
    {
        return InlineFlowBox::logicalBottomVisualOverflow(lineBottom());
    }
    LayoutUnit logicalTopLayoutOverflow() const
    {
        return InlineFlowBox::logicalTopLayoutOverflow(lineTop());
    }
    LayoutUnit logicalBottomLayoutOverflow() const
    {
        return InlineFlowBox::logicalBottomLayoutOverflow(lineBottom());
    }

#if ENABLE(CSS3_TEXT_DECORATION)
    // Used to calculate the underline offset for TextUnderlinePositionUnder.
    float maxLogicalTop() const;
#endif

    Node* getLogicalStartBoxWithNode(InlineBox*&) const;
    Node* getLogicalEndBoxWithNode(InlineBox*&) const;

#ifndef NDEBUG
    virtual const char* boxName() const override;
#endif
private:
    virtual bool isRootInlineBox() const override final { return true; }

    LayoutUnit lineSnapAdjustment(LayoutUnit delta = 0) const;

    LayoutUnit beforeAnnotationsAdjustment() const;

    // This folds into the padding at the end of InlineFlowBox on 64-bit.
    unsigned m_lineBreakPos;

    // Where this line ended.  The exact object and the position within that object are stored so that
    // we can create an InlineIterator beginning just after the end of this line.
    RenderObject* m_lineBreakObj;
    RefPtr<BidiContext> m_lineBreakContext;

    LayoutUnit m_lineTop;
    LayoutUnit m_lineBottom;

    LayoutUnit m_lineTopWithLeading;
    LayoutUnit m_lineBottomWithLeading;

    LayoutUnit m_paginationStrut;
    LayoutUnit m_paginatedLineWidth;

    // Floats hanging off the line are pushed into this vector during layout. It is only
    // good for as long as the line has not been marked dirty.
    OwnPtr<Vector<RenderBox*>> m_floats;
};

INLINE_BOX_OBJECT_TYPE_CASTS(RootInlineBox, isRootInlineBox())

inline RootInlineBox* RootInlineBox::nextRootBox() const
{
    return toRootInlineBox(m_nextLineBox);
}

inline RootInlineBox* RootInlineBox::prevRootBox() const
{
    return toRootInlineBox(m_prevLineBox);
}

} // namespace WebCore

#endif // RootInlineBox_h
