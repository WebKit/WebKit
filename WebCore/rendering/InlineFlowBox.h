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

typedef HashMap<const InlineTextBox*, pair<Vector<const SimpleFontData*>, GlyphOverflow> > GlyphOverflowAndFallbackFontsMap;

class InlineFlowBox : public InlineBox {
public:
    InlineFlowBox(RenderObject* obj)
        : InlineBox(obj)
        , m_firstChild(0)
        , m_lastChild(0)
        , m_prevLineBox(0)
        , m_nextLineBox(0)
        , m_includeLeftEdge(false)
        , m_includeRightEdge(false)
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
    virtual void paintTextDecorations(PaintInfo&, int tx, int ty, bool paintedChildren = false);
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

    virtual RenderLineBoxList* rendererLineBoxes() const;

    int marginBorderPaddingLeft() const { return marginLeft() + borderLeft() + paddingLeft(); }
    int marginBorderPaddingRight() const { return marginRight() + borderRight() + paddingRight(); }
    int marginLeft() const { if (includeLeftEdge()) return boxModelObject()->marginLeft(); return 0; }
    int marginRight() const { if (includeRightEdge()) return boxModelObject()->marginRight(); return 0; }
    int borderLeft() const { if (includeLeftEdge()) return renderer()->style()->borderLeftWidth(); return 0; }
    int borderRight() const { if (includeRightEdge()) return renderer()->style()->borderRightWidth(); return 0; }
    int borderTop() const { return renderer()->style()->borderTopWidth(); }
    int borderBottom() const { return renderer()->style()->borderBottomWidth(); }
    int paddingLeft() const { if (includeLeftEdge()) return boxModelObject()->paddingLeft(); return 0; }
    int paddingRight() const { if (includeRightEdge()) return boxModelObject()->paddingRight(); return 0; }
    int paddingTop() const { return boxModelObject()->paddingTop(); }
    int paddingBottom() const { return boxModelObject()->paddingBottom(); }

    bool includeLeftEdge() const { return m_includeLeftEdge; }
    bool includeRightEdge() const { return m_includeRightEdge; }
    void setEdges(bool includeLeft, bool includeRight)
    {
        m_includeLeftEdge = includeLeft;
        m_includeRightEdge = includeRight;
    }

    // Helper functions used during line construction and placement.
    void determineSpacingForFlowBoxes(bool lastLine, RenderObject* endObject);
    int getFlowSpacingWidth();
    bool onEndChain(RenderObject* endObject);
    int placeBoxesHorizontally(int x, bool& needsWordSpacing, GlyphOverflowAndFallbackFontsMap&);
    void computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                  int& maxAscent, int& maxDescent, bool strictMode, GlyphOverflowAndFallbackFontsMap&);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                   int maxPositionTop, int maxPositionBottom);
    void placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode, int& lineTop, int& lineBottom);
    void computeVerticalOverflow(int lineTop, int lineBottom, bool strictMode, GlyphOverflowAndFallbackFontsMap&);

    void removeChild(InlineBox* child);

    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockLeftEdge, int blockRightEdge, int ellipsisWidth, bool&);

    bool hasTextChildren() const { return m_hasTextChildren; }

    void checkConsistency() const;
    void setHasBadChildList();

    int topVisibleOverflow() const { return std::min(topLayoutOverflow(), topVisualOverflow()); }
    int bottomVisibleOverflow() const { return std::max(bottomLayoutOverflow(), bottomVisualOverflow()); }
    int leftVisibleOverflow() const { return std::min(leftLayoutOverflow(), leftVisualOverflow()); }
    int rightVisibleOverflow() const { return std::max(rightLayoutOverflow(), rightVisualOverflow()); }
    IntRect visibleOverflowRect() const { return m_overflow ? m_overflow->visibleOverflowRect() : IntRect(m_x, m_y, m_logicalWidth, logicalHeight());  }

    int topLayoutOverflow() const { return m_overflow ? m_overflow->topLayoutOverflow() : m_y; }
    int bottomLayoutOverflow() const { return m_overflow ? m_overflow->bottomLayoutOverflow() : m_y + logicalHeight(); }
    int leftLayoutOverflow() const { return m_overflow ? m_overflow->leftLayoutOverflow() : m_x; }
    int rightLayoutOverflow() const { return m_overflow ? m_overflow->rightLayoutOverflow() : m_x + m_logicalWidth; }
    IntRect layoutOverflowRect() const { return m_overflow ? m_overflow->layoutOverflowRect() : IntRect(m_x, m_y, m_logicalWidth, logicalHeight()); }

    int topVisualOverflow() const { return m_overflow ? m_overflow->topVisualOverflow() : m_y; }
    int bottomVisualOverflow() const { return m_overflow ? m_overflow->bottomVisualOverflow() : m_y + logicalHeight(); }
    int leftVisualOverflow() const { return m_overflow ? m_overflow->leftVisualOverflow() : m_x; }
    int rightVisualOverflow() const { return m_overflow ? m_overflow->rightVisualOverflow() : m_x + m_logicalWidth; }
    IntRect visualOverflowRect() const { return m_overflow ? m_overflow->visualOverflowRect() : IntRect(m_x, m_y, m_logicalWidth, logicalHeight()); }

    void setHorizontalOverflowPositions(int leftLayoutOverflow, int rightLayoutOverflow, int leftVisualOverflow, int rightVisualOverflow);
    void setVerticalOverflowPositions(int topLayoutOverflow, int bottomLayoutOverflow, int topVisualOverflow, int bottomVisualOverflow, int boxHeight);

protected:
    OwnPtr<RenderOverflow> m_overflow;

    virtual bool isInlineFlowBox() const { return true; }

    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    
    InlineFlowBox* m_prevLineBox; // The previous box that also uses our RenderObject
    InlineFlowBox* m_nextLineBox; // The next box that also uses our RenderObject

    bool m_includeLeftEdge : 1;
    bool m_includeRightEdge : 1;
    bool m_hasTextChildren : 1;

#ifndef NDEBUG
    bool m_hasBadChildList;
#endif
};

inline void InlineFlowBox::setHorizontalOverflowPositions(int leftLayoutOverflow, int rightLayoutOverflow, int leftVisualOverflow, int rightVisualOverflow) 
{ 
    if (!m_overflow) {
        if (leftLayoutOverflow == m_x && rightLayoutOverflow == m_x + m_logicalWidth && leftVisualOverflow == m_x && rightVisualOverflow == m_x + m_logicalWidth)
            return;
        m_overflow = adoptPtr(new RenderOverflow(IntRect(m_x, m_y, m_logicalWidth, m_renderer->style(m_firstLine)->font().height())));    
    }

    m_overflow->setLeftLayoutOverflow(leftLayoutOverflow);
    m_overflow->setRightLayoutOverflow(rightLayoutOverflow);
    m_overflow->setLeftVisualOverflow(leftVisualOverflow); 
    m_overflow->setRightVisualOverflow(rightVisualOverflow);  
}

inline void InlineFlowBox::setVerticalOverflowPositions(int topLayoutOverflow, int bottomLayoutOverflow, int topVisualOverflow, int bottomVisualOverflow, int boxHeight)
{
    if (!m_overflow) {
        if (topLayoutOverflow == m_y && bottomLayoutOverflow == m_y + boxHeight && topVisualOverflow == m_y && bottomVisualOverflow == m_y + boxHeight)
            return;
        m_overflow = adoptPtr(new RenderOverflow(IntRect(m_x, m_y, m_logicalWidth, boxHeight)));
    }

    m_overflow->setTopLayoutOverflow(topLayoutOverflow);
    m_overflow->setBottomLayoutOverflow(bottomLayoutOverflow);
    m_overflow->setTopVisualOverflow(topVisualOverflow); 
    m_overflow->setBottomVisualOverflow(bottomVisualOverflow);  
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
