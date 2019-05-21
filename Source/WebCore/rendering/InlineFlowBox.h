/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "InlineBox.h"
#include "RenderOverflow.h"
#include "ShadowData.h"

namespace WebCore {

class HitTestRequest;
class HitTestResult;
class InlineTextBox;
class RenderLineBoxList;
class Font;
class VerticalPositionCache;

struct GlyphOverflow;

typedef HashMap<const InlineTextBox*, std::pair<Vector<const Font*>, GlyphOverflow>> GlyphOverflowAndFallbackFontsMap;

class InlineFlowBox : public InlineBox {
    WTF_MAKE_ISO_ALLOCATED(InlineFlowBox);
public:
    explicit InlineFlowBox(RenderBoxModelObject& renderer)
        : InlineBox(renderer)
        , m_includeLogicalLeftEdge(false)
        , m_includeLogicalRightEdge(false)
        , m_descendantsHaveSameLineHeightAndBaseline(true)
        , m_baselineType(AlphabeticBaseline)
        , m_hasAnnotationsBefore(false)
        , m_hasAnnotationsAfter(false)
        , m_isFirstAfterPageBreak(false)
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
        , m_hasBadChildList(false)
#endif
        , m_firstChild(nullptr)
        , m_lastChild(nullptr)
        , m_prevLineBox(nullptr)
        , m_nextLineBox(nullptr)
    {
        // Internet Explorer and Firefox always create a marker for list items, even when the list-style-type is none.  We do not make a marker
        // in the list-style-type: none case, since it is wasteful to do so.  However, in order to match other browsers we have to pretend like
        // an invisible marker exists.  The side effect of having an invisible marker is that the quirks mode behavior of shrinking lines with no
        // text children must not apply.  This change also means that gaps will exist between image bullet list items.  Even when the list bullet
        // is an image, the line is still considered to be immune from the quirk.
        m_hasTextChildren = renderer.style().display() == DisplayType::ListItem;
        m_hasTextDescendants = m_hasTextChildren;
    }

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    virtual ~InlineFlowBox();
#endif

#if ENABLE(TREE_DEBUGGING)
    void outputLineTreeAndMark(WTF::TextStream&, const InlineBox* markedBox, int depth) const override;
    const char* boxName() const override;
#endif

    RenderBoxModelObject& renderer() const { return downcast<RenderBoxModelObject>(InlineBox::renderer()); }
    const RenderStyle& lineStyle() const { return isFirstLine() ? renderer().firstLineStyle() : renderer().style(); }

    InlineFlowBox* prevLineBox() const { return m_prevLineBox; }
    InlineFlowBox* nextLineBox() const { return m_nextLineBox; }
    void setNextLineBox(InlineFlowBox* n) { m_nextLineBox = n; }
    void setPreviousLineBox(InlineFlowBox* p) { m_prevLineBox = p; }

    InlineBox* firstChild() const { checkConsistency(); return m_firstChild; }
    InlineBox* lastChild() const { checkConsistency(); return m_lastChild; }

    bool isLeaf() const final { return false; }
    
    InlineBox* firstLeafChild() const;
    InlineBox* lastLeafChild() const;

    typedef void (*CustomInlineBoxRangeReverse)(void* userData, Vector<InlineBox*>::iterator first, Vector<InlineBox*>::iterator last);
    void collectLeafBoxesInLogicalOrder(Vector<InlineBox*>&, CustomInlineBoxRangeReverse customReverseImplementation = nullptr, void* userData = nullptr) const;

    void setConstructed() final
    {
        InlineBox::setConstructed();
        for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
            child->setConstructed();
    }

    void addToLine(InlineBox* child);
    void deleteLine() final;
    void extractLine() final;
    void attachLine() final;
    void adjustPosition(float dx, float dy) override;

    virtual void extractLineBoxFromRenderObject();
    virtual void attachLineBoxToRenderObject();
    virtual void removeLineBoxFromRenderObject();

    void clearTruncation() override;

    void paintBoxDecorations(PaintInfo&, const LayoutPoint&);
    void paintMask(PaintInfo&, const LayoutPoint&);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, CompositeOperator = CompositeSourceOver);
    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, CompositeOperator = CompositeSourceOver);
    void paintBoxShadow(const PaintInfo&, const RenderStyle&, ShadowStyle, const LayoutRect&);
    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom) override;
    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom, HitTestAction) override;

    bool boxShadowCanBeAppliedToBackground(const FillLayer&) const;

    // logicalLeft = left in a horizontal line and top in a vertical line.
    LayoutUnit marginBorderPaddingLogicalLeft() const { return LayoutUnit(marginLogicalLeft() + borderLogicalLeft() + paddingLogicalLeft()); }
    LayoutUnit marginBorderPaddingLogicalRight() const { return LayoutUnit(marginLogicalRight() + borderLogicalRight() + paddingLogicalRight()); }
    LayoutUnit marginLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? renderer().marginLeft() : renderer().marginTop();
    }
    LayoutUnit marginLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? renderer().marginRight() : renderer().marginBottom();
    }
    float borderLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? lineStyle().borderLeftWidth() : lineStyle().borderTopWidth();
    }
    float borderLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? lineStyle().borderRightWidth() : lineStyle().borderBottomWidth();
    }
    float paddingLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? renderer().paddingLeft() : renderer().paddingTop();
    }
    float paddingLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? renderer().paddingRight() : renderer().paddingBottom();
    }

    bool includeLogicalLeftEdge() const { return m_includeLogicalLeftEdge; }
    bool includeLogicalRightEdge() const { return m_includeLogicalRightEdge; }
    void setEdges(bool includeLeft, bool includeRight)
    {
        m_includeLogicalLeftEdge = includeLeft;
        m_includeLogicalRightEdge = includeRight;
    }

    // Helper functions used during line construction and placement.
    void determineSpacingForFlowBoxes(bool lastLine, bool isLogicallyLastRunWrapped, RenderObject* logicallyLastRunRenderer);
    LayoutUnit getFlowSpacingLogicalWidth();
    float placeBoxesInInlineDirection(float logicalLeft, bool& needsWordSpacing);
    float placeBoxRangeInInlineDirection(InlineBox* firstChild, InlineBox* lastChild, float& logicalLeft, float& minLogicalLeft, float& maxLogicalRight, bool& needsWordSpacing);
    void beginPlacingBoxRangesInInlineDirection(float logicalLeft) { setLogicalLeft(logicalLeft); }
    void endPlacingBoxRangesInInlineDirection(float logicalLeft, float logicalRight, float minLogicalLeft, float maxLogicalRight)
    {
        setLogicalWidth(logicalRight - logicalLeft);
        if (knownToHaveNoOverflow() && (minLogicalLeft < logicalLeft || maxLogicalRight > logicalRight))
            clearKnownToHaveNoOverflow();
    }

    void computeLogicalBoxHeights(RootInlineBox&, LayoutUnit& maxPositionTop, LayoutUnit& maxPositionBottom,
        int& maxAscent, int& maxDescent, bool& setMaxAscent, bool& setMaxDescent,
        bool strictMode, GlyphOverflowAndFallbackFontsMap&, FontBaseline, VerticalPositionCache&);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
        int maxPositionTop, int maxPositionBottom);
    void placeBoxesInBlockDirection(LayoutUnit logicalTop, LayoutUnit maxHeight, int maxAscent, bool strictMode, LayoutUnit& lineTop, LayoutUnit& lineBottom, bool& setLineTop,
        LayoutUnit& lineTopIncludingMargins, LayoutUnit& lineBottomIncludingMargins, bool& hasAnnotationsBefore, bool& hasAnnotationsAfter, FontBaseline);
    void flipLinesInBlockDirection(LayoutUnit lineTop, LayoutUnit lineBottom);
    bool requiresIdeographicBaseline(const GlyphOverflowAndFallbackFontsMap&) const;

    LayoutUnit computeOverAnnotationAdjustment(LayoutUnit allowedPosition) const;
    LayoutUnit computeUnderAnnotationAdjustment(LayoutUnit allowedPosition) const;

    void computeOverflow(LayoutUnit lineTop, LayoutUnit lineBottom, GlyphOverflowAndFallbackFontsMap&);
    
    void removeChild(InlineBox* child);

    RenderObject::SelectionState selectionState() override;

    bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth) const final;
    float placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, float &truncatedWidth, bool&) override;

    bool hasTextChildren() const { return m_hasTextChildren; }
    bool hasTextDescendants() const { return m_hasTextDescendants; }
    void setHasTextChildren() { m_hasTextChildren = true; setHasTextDescendants(); }
    void setHasTextDescendants() { m_hasTextDescendants = true; }
    
    void checkConsistency() const;
    void setHasBadChildList();

    // Line visual and layout overflow are in the coordinate space of the block.  This means that they aren't purely physical directions.
    // For horizontal-tb and vertical-lr they will match physical directions, but for horizontal-bt and vertical-rl, the top/bottom and left/right
    // respectively are flipped when compared to their physical counterparts.  For example minX is on the left in vertical-lr, but it is on the right in vertical-rl.
    LayoutRect layoutOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        return m_overflow ? m_overflow->layoutOverflowRect() : enclosingLayoutRect(frameRectIncludingLineHeight(lineTop, lineBottom));
    }
    LayoutUnit logicalTopLayoutOverflow(LayoutUnit lineTop) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->layoutOverflowRect().y() : m_overflow->layoutOverflowRect().x();
        return lineTop;
    }
    LayoutUnit logicalBottomLayoutOverflow(LayoutUnit lineBottom) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->layoutOverflowRect().maxY() : m_overflow->layoutOverflowRect().maxX();
        return lineBottom;
    }
    LayoutRect logicalLayoutOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        LayoutRect result = layoutOverflowRect(lineTop, lineBottom);
        if (!renderer().isHorizontalWritingMode())
            result = result.transposedRect();
        return result;
    }

    LayoutRect visualOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    { 
        return m_overflow ? m_overflow->visualOverflowRect() : enclosingLayoutRect(frameRectIncludingLineHeight(lineTop, lineBottom));
    }
    LayoutUnit logicalLeftVisualOverflow() const { return m_overflow ? (isHorizontal() ? m_overflow->visualOverflowRect().x() : m_overflow->visualOverflowRect().y()) : LayoutUnit(logicalLeft()); }
    LayoutUnit logicalRightVisualOverflow() const { return m_overflow ? (isHorizontal() ? m_overflow->visualOverflowRect().maxX() : m_overflow->visualOverflowRect().maxY()) : LayoutUnit(ceilf(logicalRight())); }
    LayoutUnit logicalTopVisualOverflow(LayoutUnit lineTop) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->visualOverflowRect().y() : m_overflow->visualOverflowRect().x();
        return lineTop;
    }
    LayoutUnit logicalBottomVisualOverflow(LayoutUnit lineBottom) const
    {
        if (m_overflow)
            return isHorizontal() ? m_overflow->visualOverflowRect().maxY() : m_overflow->visualOverflowRect().maxX();
        return lineBottom;
    }
    LayoutRect logicalVisualOverflowRect(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        LayoutRect result = visualOverflowRect(lineTop, lineBottom);
        if (!renderer().isHorizontalWritingMode())
            result = result.transposedRect();
        return result;
    }

    void setOverflowFromLogicalRects(const LayoutRect& logicalLayoutOverflow, const LayoutRect& logicalVisualOverflow, LayoutUnit lineTop, LayoutUnit lineBottom);
    void setLayoutOverflow(const LayoutRect&, LayoutUnit lineTop, LayoutUnit lineBottom);
    void setVisualOverflow(const LayoutRect&, LayoutUnit lineTop, LayoutUnit lineBottom);

    FloatRect frameRectIncludingLineHeight(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        if (isHorizontal())
            return FloatRect(x(), lineTop, width(), lineBottom - lineTop);
        return FloatRect(lineTop, y(), lineBottom - lineTop, height());
    }
    
    FloatRect logicalFrameRectIncludingLineHeight(LayoutUnit lineTop, LayoutUnit lineBottom) const
    {
        return FloatRect(logicalLeft(), lineTop, logicalWidth(), lineBottom - lineTop);
    }
    
    bool descendantsHaveSameLineHeightAndBaseline() const { return m_descendantsHaveSameLineHeightAndBaseline; }
    void clearDescendantsHaveSameLineHeightAndBaseline()
    { 
        m_descendantsHaveSameLineHeightAndBaseline = false;
        if (parent() && parent()->descendantsHaveSameLineHeightAndBaseline())
            parent()->clearDescendantsHaveSameLineHeightAndBaseline();
    }

    void computeReplacedAndTextLineTopAndBottom(LayoutUnit& lineTop, LayoutUnit& lineBottom) const;

    void maxLogicalBottomForTextDecorationLine(float& maxLogicalBottom, const RenderElement* decorationRenderer, OptionSet<TextDecoration>) const;
    void minLogicalTopForTextDecorationLine(float& minLogicalTop, const RenderElement* decorationRenderer, OptionSet<TextDecoration>) const;

private:
    bool isInlineFlowBox() const final { return true; }
    void boxModelObject() const = delete;

    void addBoxShadowVisualOverflow(LayoutRect& logicalVisualOverflow);
    void addBorderOutsetVisualOverflow(LayoutRect& logicalVisualOverflow);
    void addTextBoxVisualOverflow(InlineTextBox&, GlyphOverflowAndFallbackFontsMap&, LayoutRect& logicalVisualOverflow);
    void addOutlineVisualOverflow(LayoutRect& logicalVisualOverflow);
    void addReplacedChildOverflow(const InlineBox*, LayoutRect& logicalLayoutOverflow, LayoutRect& logicalVisualOverflow);
    void constrainToLineTopAndBottomIfNeeded(LayoutRect&) const;

private:
    unsigned m_includeLogicalLeftEdge : 1;
    unsigned m_includeLogicalRightEdge : 1;
    unsigned m_hasTextChildren : 1;
    unsigned m_hasTextDescendants : 1;
    unsigned m_descendantsHaveSameLineHeightAndBaseline : 1;

protected:
    // The following members are only used by RootInlineBox but moved here to keep the bits packed.

    // Whether or not this line uses alphabetic or ideographic baselines by default.
    unsigned m_baselineType : 1; // FontBaseline

    // If the line contains any ruby runs, then this will be true.
    unsigned m_hasAnnotationsBefore : 1;
    unsigned m_hasAnnotationsAfter : 1;

    unsigned m_lineBreakBidiStatusEor : 5; // UCharDirection
    unsigned m_lineBreakBidiStatusLastStrong : 5; // UCharDirection
    unsigned m_lineBreakBidiStatusLast : 5; // UCharDirection

    unsigned m_isFirstAfterPageBreak : 1;

    // End of RootInlineBox-specific members.

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
private:
    unsigned m_hasBadChildList : 1;
#endif

protected:
    RefPtr<RenderOverflow> m_overflow;

    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    
    InlineFlowBox* m_prevLineBox; // The previous box that also uses our RenderObject
    InlineFlowBox* m_nextLineBox; // The next box that also uses our RenderObject
};

#ifdef NDEBUG

inline void InlineFlowBox::checkConsistency() const
{
}

#endif

#if ASSERT_WITH_SECURITY_IMPLICATION_DISABLED

inline void InlineFlowBox::setHasBadChildList()
{
}

#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_INLINE_BOX(InlineFlowBox, isInlineFlowBox())

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from the debugger.
void showTree(const WebCore::InlineFlowBox*);
#endif
