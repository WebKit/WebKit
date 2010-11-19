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

#ifndef InlineFlowBox_h
#define InlineFlowBox_h

#include "InlineBox.h"
#include "RenderOverflow.h"

namespace WebCore {

class HitTestRequest;
class HitTestResult;
class InlineTextBox;
class RenderLineBoxList;
class VerticalPositionCache;

typedef HashMap<const InlineTextBox*, pair<Vector<const SimpleFontData*>, GlyphOverflow> > GlyphOverflowAndFallbackFontsMap;

class InlineFlowBox : public InlineBox {
public:
    InlineFlowBox(RenderObject* obj)
        : InlineBox(obj)
        , m_firstChild(0)
        , m_lastChild(0)
        , m_prevLineBox(0)
        , m_nextLineBox(0)
        , m_includeLogicalLeftEdge(false)
        , m_includeLogicalRightEdge(false)
#ifndef NDEBUG
        , m_hasBadChildList(false)
#endif
    {
        // Internet Explorer and Firefox always create a marker for list items, even when the list-style-type is none.  We do not make a marker
        // in the list-style-type: none case, since it is wasteful to do so.  However, in order to match other browsers we have to pretend like
        // an invisible marker exists.  The side effect of having an invisible marker is that the quirks mode behavior of shrinking lines with no
        // text children must not apply.  This change also means that gaps will exist between image bullet list items.  Even when the list bullet
        // is an image, the line is still considered to be immune from the quirk.
        m_hasTextChildren = obj->style()->display() == LIST_ITEM;
    }

#ifndef NDEBUG
    virtual ~InlineFlowBox();
#endif

    InlineFlowBox* prevLineBox() const { return m_prevLineBox; }
    InlineFlowBox* nextLineBox() const { return m_nextLineBox; }
    void setNextLineBox(InlineFlowBox* n) { m_nextLineBox = n; }
    void setPreviousLineBox(InlineFlowBox* p) { m_prevLineBox = p; }

    InlineBox* firstChild() const { checkConsistency(); return m_firstChild; }
    InlineBox* lastChild() const { checkConsistency(); return m_lastChild; }

    virtual bool isLeaf() const { return false; }
    
    InlineBox* firstLeafChild() const;
    InlineBox* lastLeafChild() const;

    virtual void setConstructed()
    {
        InlineBox::setConstructed();
        if (firstChild())
            firstChild()->setConstructed();
    }

    void addToLine(InlineBox* child);
    virtual void deleteLine(RenderArena*);
    virtual void extractLine();
    virtual void attachLine();
    virtual void adjustPosition(int dx, int dy);

    virtual void extractLineBoxFromRenderObject();
    virtual void attachLineBoxToRenderObject();
    virtual void removeLineBoxFromRenderObject();

    virtual void clearTruncation();

    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void paintMask(PaintInfo&, int tx, int ty);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int w, int h, CompositeOperator = CompositeSourceOver);
    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int w, int h, CompositeOperator = CompositeSourceOver);
    void paintBoxShadow(GraphicsContext*, RenderStyle*, ShadowStyle, int tx, int ty, int w, int h);
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

    virtual RenderLineBoxList* rendererLineBoxes() const;

    // logicalLeft = left in a horizontal line and top in a vertical line.
    int marginBorderPaddingLogicalLeft() const { return marginLogicalLeft() + borderLogicalLeft() + paddingLogicalLeft(); }
    int marginBorderPaddingLogicalRight() const { return marginLogicalRight() + borderLogicalRight() + paddingLogicalRight(); }
    int marginLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->marginLeft() : boxModelObject()->marginTop();
    }
    int marginLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->marginRight() : boxModelObject()->marginBottom();
    }
    int borderLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? renderer()->style()->borderLeftWidth() : renderer()->style()->borderTopWidth();
    }
    int borderLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? renderer()->style()->borderRightWidth() : renderer()->style()->borderBottomWidth();
    }
    int paddingLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->paddingLeft() : boxModelObject()->paddingTop();
    }
    int paddingLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->paddingRight() : boxModelObject()->paddingBottom();
    }

    bool includeLogicalLeftEdge() const { return m_includeLogicalLeftEdge; }
    bool includeLogicalRightEdge() const { return m_includeLogicalRightEdge; }
    void setEdges(bool includeLeft, bool includeRight)
    {
        m_includeLogicalLeftEdge = includeLeft;
        m_includeLogicalRightEdge = includeRight;
    }

    // Helper functions used during line construction and placement.
    void determineSpacingForFlowBoxes(bool lastLine, RenderObject* endObject);
    int getFlowSpacingLogicalWidth();
    bool onEndChain(RenderObject* endObject);
    int placeBoxesInInlineDirection(int logicalLeft, bool& needsWordSpacing, GlyphOverflowAndFallbackFontsMap&);
    void computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                  int& maxAscent, int& maxDescent, bool& setMaxAscent, bool& setMaxDescent,
                                  bool strictMode, GlyphOverflowAndFallbackFontsMap&, FontBaseline, VerticalPositionCache&);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                   int maxPositionTop, int maxPositionBottom);
    void placeBoxesInBlockDirection(int logicalTop, int maxHeight, int maxAscent, bool strictMode, int& lineTop, int& lineBottom, bool& setLineTop,
                                    int& lineTopIncludingMargins, int& lineBottomIncludingMargins, bool& containsRuby, FontBaseline);
    void flipLinesInBlockDirection(int lineTop, int lineBottom);
    void computeBlockDirectionOverflow(int lineTop, int lineBottom, bool strictMode, GlyphOverflowAndFallbackFontsMap&);
    bool requiresIdeographicBaseline(const GlyphOverflowAndFallbackFontsMap&) const;
    int computeBlockDirectionRubyAdjustment(int allowedPosition) const;
    
    void removeChild(InlineBox* child);

    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockLeftEdge, int blockRightEdge, int ellipsisWidth, bool&);

    bool hasTextChildren() const { return m_hasTextChildren; }

    void checkConsistency() const;
    void setHasBadChildList();

    // Line visual and layout overflow are in the coordinate space of the block.  This means that - unlike other unprefixed uses of the words
    // top/right/bottom/left in the code - these aren't purely physical directions.  For horizontal-tb and vertical-lr they will match physical
    // directions, but for horizontal-bt and vertical-rl, the top/bottom and left/right respectively are inverted when compared to
    // their physical counterparts.
    int topVisibleOverflow() const { return std::min(topLayoutOverflow(), topVisualOverflow()); }
    int bottomVisibleOverflow() const { return std::max(bottomLayoutOverflow(), bottomVisualOverflow()); }
    int leftVisibleOverflow() const { return std::min(leftLayoutOverflow(), leftVisualOverflow()); }
    int rightVisibleOverflow() const { return std::max(rightLayoutOverflow(), rightVisualOverflow()); }
    int logicalLeftVisibleOverflow() const { return std::min(logicalLeftLayoutOverflow(), logicalLeftVisualOverflow()); }
    int logicalRightVisibleOverflow() const { return std::max(logicalRightLayoutOverflow(), logicalRightVisualOverflow()); }
    int logicalTopVisibleOverflow() const { return std::min(logicalTopLayoutOverflow(), logicalTopVisualOverflow()); }
    int logicalBottomVisibleOverflow() const { return std::max(logicalBottomLayoutOverflow(), logicalBottomVisualOverflow()); }

    IntRect visibleOverflowRect() const { return m_overflow ? m_overflow->visibleOverflowRect() : IntRect(m_x, m_y, width(), height());  }

    int topLayoutOverflow() const { return m_overflow ? m_overflow->topLayoutOverflow() : m_y; }
    int bottomLayoutOverflow() const { return m_overflow ? m_overflow->bottomLayoutOverflow() : m_y + height(); }
    int leftLayoutOverflow() const { return m_overflow ? m_overflow->leftLayoutOverflow() : m_x; }
    int rightLayoutOverflow() const { return m_overflow ? m_overflow->rightLayoutOverflow() : m_x + width(); }
    IntRect layoutOverflowRect() const { return m_overflow ? m_overflow->layoutOverflowRect() : IntRect(m_x, m_y, width(), height()); }
    int logicalLeftLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? leftLayoutOverflow() : topLayoutOverflow(); }
    int logicalRightLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? rightLayoutOverflow() : bottomLayoutOverflow(); }
    int logicalTopLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? topVisualOverflow() : leftVisualOverflow(); }
    int logicalBottomLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? bottomLayoutOverflow() : rightLayoutOverflow(); }

    int topVisualOverflow() const { return m_overflow ? m_overflow->topVisualOverflow() : m_y; }
    int bottomVisualOverflow() const { return m_overflow ? m_overflow->bottomVisualOverflow() : m_y + height(); }
    int leftVisualOverflow() const { return m_overflow ? m_overflow->leftVisualOverflow() : m_x; }
    int rightVisualOverflow() const { return m_overflow ? m_overflow->rightVisualOverflow() : m_x + width(); }
    IntRect visualOverflowRect() const { return m_overflow ? m_overflow->visualOverflowRect() : IntRect(m_x, m_y, width(), height()); }
    int logicalLeftVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? leftVisualOverflow() : topVisualOverflow(); }
    int logicalRightVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? rightVisualOverflow() : bottomVisualOverflow(); }
    int logicalTopVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? topVisualOverflow() : leftVisualOverflow(); }
    int logicalBottomVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? bottomVisualOverflow() : rightVisualOverflow(); }

    void setInlineDirectionOverflowPositions(int logicalLeftLayoutOverflow, int logicalRightLayoutOverflow,
                                             int logicalLeftVisualOverflow, int logicalRightVisualOverflow);
    void setBlockDirectionOverflowPositions(int logicalTopLayoutOverflow, int logicalBottomLayoutOverflow,
                                            int logicalTopVisualOverflow, int logicalBottomVisualOverflow);

protected:
    OwnPtr<RenderOverflow> m_overflow;

    virtual bool isInlineFlowBox() const { return true; }

    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    
    InlineFlowBox* m_prevLineBox; // The previous box that also uses our RenderObject
    InlineFlowBox* m_nextLineBox; // The next box that also uses our RenderObject

    bool m_includeLogicalLeftEdge : 1;
    bool m_includeLogicalRightEdge : 1;
    bool m_hasTextChildren : 1;

#ifndef NDEBUG
    bool m_hasBadChildList;
#endif
};

inline void InlineFlowBox::setInlineDirectionOverflowPositions(int logicalLeftLayoutOverflow, int logicalRightLayoutOverflow, 
                                                               int logicalLeftVisualOverflow, int logicalRightVisualOverflow) 
{ 
    if (!m_overflow) {
        if (logicalLeftLayoutOverflow == logicalLeft() && logicalRightLayoutOverflow == logicalRight() 
            && logicalLeftVisualOverflow == logicalLeft() && logicalRightVisualOverflow == logicalRight())
            return;
        m_overflow = adoptPtr(new RenderOverflow(IntRect(m_x, m_y, width(), height())));   
    }

    if (isHorizontal()) {
        m_overflow->setLeftLayoutOverflow(logicalLeftLayoutOverflow);
        m_overflow->setRightLayoutOverflow(logicalRightLayoutOverflow);
        m_overflow->setLeftVisualOverflow(logicalLeftVisualOverflow); 
        m_overflow->setRightVisualOverflow(logicalRightVisualOverflow);
    } else {
        m_overflow->setTopLayoutOverflow(logicalLeftLayoutOverflow);
        m_overflow->setBottomLayoutOverflow(logicalRightLayoutOverflow);
        m_overflow->setTopVisualOverflow(logicalLeftVisualOverflow); 
        m_overflow->setBottomVisualOverflow(logicalRightVisualOverflow);  
    }
}

inline void InlineFlowBox::setBlockDirectionOverflowPositions(int logicalTopLayoutOverflow, int logicalBottomLayoutOverflow,
                                                              int logicalTopVisualOverflow, int logicalBottomVisualOverflow)
{
    if (!m_overflow) {
        if (logicalTopLayoutOverflow == logicalTop() && logicalBottomLayoutOverflow == logicalBottom()
            && logicalTopVisualOverflow == logicalTop() && logicalBottomVisualOverflow == logicalBottom())
            return;
        m_overflow = adoptPtr(new RenderOverflow(IntRect(m_x, m_y, width(), height())));
    }

    if (isHorizontal()) {
        m_overflow->setTopLayoutOverflow(logicalTopLayoutOverflow);
        m_overflow->setBottomLayoutOverflow(logicalBottomLayoutOverflow);
        m_overflow->setTopVisualOverflow(logicalTopVisualOverflow); 
        m_overflow->setBottomVisualOverflow(logicalBottomVisualOverflow);
    } else {
        m_overflow->setLeftLayoutOverflow(logicalTopLayoutOverflow);
        m_overflow->setRightLayoutOverflow(logicalBottomLayoutOverflow);
        m_overflow->setLeftVisualOverflow(logicalTopVisualOverflow); 
        m_overflow->setRightVisualOverflow(logicalBottomVisualOverflow);
    }
}

#ifdef NDEBUG
inline void InlineFlowBox::checkConsistency() const
{
}
#endif

inline void InlineFlowBox::setHasBadChildList()
{
#ifndef NDEBUG
    m_hasBadChildList = true;
#endif
}

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::InlineFlowBox*);
#endif

#endif // InlineFlowBox_h
