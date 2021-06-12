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

#pragma once

#include "BidiContext.h"
#include "LegacyInlineFlowBox.h"
#include "RenderBox.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class HitTestResult;
class LegacyEllipsisBox;
class LogicalSelectionOffsetCaches;
class RenderBlockFlow;
class RenderFragmentContainer;

struct BidiStatus;
struct GapRects;

class LegacyRootInlineBox : public LegacyInlineFlowBox, public CanMakeWeakPtr<LegacyRootInlineBox> {
    WTF_MAKE_ISO_ALLOCATED(LegacyRootInlineBox);
public:
    explicit LegacyRootInlineBox(RenderBlockFlow&);
    virtual ~LegacyRootInlineBox();

    RenderBlockFlow& blockFlow() const;

    void detachEllipsisBox();

    LegacyRootInlineBox* nextRootBox() const;
    LegacyRootInlineBox* prevRootBox() const;

    void adjustPosition(float dx, float dy) final;

    LayoutUnit lineTop() const { return m_lineTop; }
    LayoutUnit lineBottom() const { return m_lineBottom; }

    LayoutUnit lineBoxTop() const { return m_lineBoxTop; }
    LayoutUnit lineBoxBottom() const { return m_lineBoxBottom; }
    LayoutUnit lineBoxHeight() const { return lineBoxBottom() - lineBoxTop(); }
    
    LayoutUnit paginationStrut() const { return m_paginationStrut; }
    void setPaginationStrut(LayoutUnit strut) { m_paginationStrut = strut; }

    bool isFirstAfterPageBreak() const { return m_isFirstAfterPageBreak; }
    void setIsFirstAfterPageBreak(bool isFirstAfterPageBreak) { m_isFirstAfterPageBreak = isFirstAfterPageBreak; }

    LayoutUnit paginatedLineWidth() const { return m_paginatedLineWidth; }
    void setPaginatedLineWidth(LayoutUnit width) { m_paginatedLineWidth = width; }

    // It should not be assumed the containingFragment() is always valid.
    // It can also be nullptr if the flow has no fragment chain.
    RenderFragmentContainer* containingFragment() const;
    void setContainingFragment(RenderFragmentContainer&);
    void clearContainingFragment();

    enum class ForHitTesting : bool { No, Yes };
    LayoutUnit selectionTop(ForHitTesting = ForHitTesting::No) const;
    LayoutUnit selectionBottom() const;
    LayoutUnit selectionHeight() const { return std::max<LayoutUnit>(0, selectionBottom() - selectionTop()); }

    LayoutUnit selectionTopAdjustedForPrecedingBlock() const;
    LayoutUnit selectionHeightAdjustedForPrecedingBlock() const { return std::max<LayoutUnit>(0, selectionBottom() - selectionTopAdjustedForPrecedingBlock()); }

    LayoutUnit alignBoxesInBlockDirection(LayoutUnit heightOfBlock, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&);
    void setLineTopBottomPositions(LayoutUnit top, LayoutUnit bottom, LayoutUnit lineBoxTop, LayoutUnit lineBoxBottom)
    { 
        m_lineTop = top; 
        m_lineBottom = bottom;
        m_lineBoxTop = lineBoxTop;
        m_lineBoxBottom = lineBoxBottom;
    }

    RenderObject* lineBreakObj() const { return m_lineBreakObj.get(); }
    BidiStatus lineBreakBidiStatus() const;
    void setLineBreakInfo(RenderObject*, unsigned breakPos, const BidiStatus&);

    unsigned lineBreakPos() const { return m_lineBreakPos; }
    void setLineBreakPos(unsigned p) { m_lineBreakPos = p; }

    bool isForTrailingFloats() const { return m_isForTrailingFloats; }
    void setIsForTrailingFloats() { m_isForTrailingFloats = true; }

    using LegacyInlineBox::endsWithBreak;
    using LegacyInlineBox::setEndsWithBreak;

    void childRemoved(LegacyInlineBox*);

    bool lineCanAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth);
    // Return the truncatedWidth, the width of the truncated text + ellipsis.
    float placeEllipsis(const AtomString& ellipsisStr, bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, LegacyInlineBox* markupBox = nullptr);
    // Return the position of the LegacyEllipsisBox or -1.
    float placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox) final;

    using LegacyInlineBox::hasEllipsisBox;
    LegacyEllipsisBox* ellipsisBox() const;

    void paintEllipsisBox(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) const;

    void clearTruncation() final;

    bool isHyphenated() const;

    LayoutUnit baselinePosition(FontBaseline baselineType) const final;
    LayoutUnit lineHeight() const final;

    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

    RenderObject::HighlightState selectionState() final;
    LegacyInlineBox* firstSelectedBox();
    LegacyInlineBox* lastSelectedBox();

    GapRects lineSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
        LayoutUnit selTop, LayoutUnit selHeight, const LogicalSelectionOffsetCaches&, const PaintInfo*);

    using CleanLineFloatList = Vector<WeakPtr<RenderBox>>;
    void appendFloat(RenderBox& floatingBox)
    {
        ASSERT(!isDirty());
        if (m_floats)
            m_floats->append(makeWeakPtr(floatingBox));
        else
            m_floats = makeUnique<CleanLineFloatList>(1, makeWeakPtr(floatingBox));
    }

    void removeFloat(RenderBox& floatingBox)
    {
        ASSERT(m_floats);
        ASSERT(m_floats->contains(&floatingBox));
        m_floats->remove(m_floats->find(&floatingBox));
    }

    CleanLineFloatList* floatsPtr() { ASSERT(!isDirty()); return m_floats.get(); }

    void extractLineBoxFromRenderObject() final;
    void attachLineBoxToRenderObject() final;
    void removeLineBoxFromRenderObject() final;
    
    FontBaseline baselineType() const { return static_cast<FontBaseline>(m_baselineType); }

    bool hasAnnotationsBefore() const { return m_hasAnnotationsBefore; }
    bool hasAnnotationsAfter() const { return m_hasAnnotationsAfter; }

    LayoutRect paddedLayoutOverflowRect(LayoutUnit endPadding) const;

    void ascentAndDescentForBox(LegacyInlineBox&, GlyphOverflowAndFallbackFontsMap&, LayoutUnit& ascent, LayoutUnit& descent, bool& affectsAscent, bool& affectsDescent) const;
    LayoutUnit verticalPositionForBox(LegacyInlineBox*, VerticalPositionCache&);
    bool fitsToGlyphs() const;
    bool includesRootLineBoxFontOrLeading() const;
    
    LayoutUnit logicalTopVisualOverflow() const
    {
        return LegacyInlineFlowBox::logicalTopVisualOverflow(lineTop());
    }
    LayoutUnit logicalBottomVisualOverflow() const
    {
        return LegacyInlineFlowBox::logicalBottomVisualOverflow(lineBottom());
    }
    LayoutUnit logicalTopLayoutOverflow() const
    {
        return LegacyInlineFlowBox::logicalTopLayoutOverflow(lineTop());
    }
    LayoutUnit logicalBottomLayoutOverflow() const
    {
        return LegacyInlineFlowBox::logicalBottomLayoutOverflow(lineBottom());
    }

#if ENABLE(TREE_DEBUGGING)
    void outputLineBox(WTF::TextStream&, bool mark, int depth) const final;
    const char* boxName() const final;
#endif
private:
    bool isRootInlineBox() const final { return true; }

    bool includeLeadingForBox(LegacyInlineBox&) const;
    bool includeFontForBox(LegacyInlineBox&) const;
    bool includeGlyphsForBox(LegacyInlineBox&) const;
    bool includeInitialLetterForBox(LegacyInlineBox&) const;
    bool includeMarginForBox(LegacyInlineBox&) const;

    LayoutUnit lineSnapAdjustment(LayoutUnit delta = 0_lu) const;

    LayoutUnit beforeAnnotationsAdjustment() const;

    // Where this line ended. The exact object and the position within that object are stored so that
    // we can create an InlineIterator beginning just after the end of this line.
    WeakPtr<RenderObject> m_lineBreakObj;
    RefPtr<BidiContext> m_lineBreakContext;

    LayoutUnit m_lineTop;
    LayoutUnit m_lineBottom;

    LayoutUnit m_lineBoxTop;
    LayoutUnit m_lineBoxBottom;

    LayoutUnit m_paginationStrut;
    LayoutUnit m_paginatedLineWidth;

    // Floats hanging off the line are pushed into this vector during layout. It is only
    // good for as long as the line has not been marked dirty.
    std::unique_ptr<CleanLineFloatList> m_floats;

    unsigned m_lineBreakPos { 0 };
};

inline LegacyRootInlineBox* LegacyRootInlineBox::nextRootBox() const
{
    return downcast<LegacyRootInlineBox>(m_nextLineBox);
}

inline LegacyRootInlineBox* LegacyRootInlineBox::prevRootBox() const
{
    return downcast<LegacyRootInlineBox>(m_prevLineBox);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(LegacyRootInlineBox, isRootInlineBox())
