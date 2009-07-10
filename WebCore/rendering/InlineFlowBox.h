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

#include "InlineRunBox.h"

namespace WebCore {

class HitTestRequest;
class HitTestResult;
class RenderLineBoxList;

class InlineFlowBox : public InlineRunBox {
public:
    InlineFlowBox(RenderObject* obj)
        : InlineRunBox(obj)
        , m_firstChild(0)
        , m_lastChild(0)
        , m_maxHorizontalVisualOverflow(0)
        , m_includeLeftEdge(false)
        , m_includeRightEdge(false)
        , m_hasTextChildren(true)
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

    InlineFlowBox* prevFlowBox() const { return static_cast<InlineFlowBox*>(m_prevLine); }
    InlineFlowBox* nextFlowBox() const { return static_cast<InlineFlowBox*>(m_nextLine); }

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

    virtual void paintBoxDecorations(RenderObject::PaintInfo&, int tx, int ty);
    virtual void paintMask(RenderObject::PaintInfo&, int tx, int ty);
    void paintFillLayers(const RenderObject::PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int w, int h, CompositeOperator = CompositeSourceOver);
    void paintFillLayer(const RenderObject::PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int w, int h, CompositeOperator = CompositeSourceOver);
    void paintBoxShadow(GraphicsContext*, RenderStyle*, int tx, int ty, int w, int h);
    virtual void paintTextDecorations(RenderObject::PaintInfo&, int tx, int ty, bool paintedChildren = false);
    virtual void paint(RenderObject::PaintInfo&, int tx, int ty);
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
    virtual int placeBoxesHorizontally(int x, int& leftPosition, int& rightPosition, bool& needsWordSpacing);
    virtual int verticallyAlignBoxes(int heightOfBlock);
    void computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                  int& maxAscent, int& maxDescent, bool strictMode);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                   int maxPositionTop, int maxPositionBottom);
    void placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode,
                              int& topPosition, int& bottomPosition, int& selectionTop, int& selectionBottom);
    
    virtual void setVerticalOverflowPositions(int /*top*/, int /*bottom*/) { }
    virtual void setVerticalSelectionPositions(int /*top*/, int /*bottom*/) { }
    short maxHorizontalVisualOverflow() const { return m_maxHorizontalVisualOverflow; }

    void removeChild(InlineBox* child);

    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockLeftEdge, int blockRightEdge, int ellipsisWidth, bool&);

    bool hasTextChildren() const { return m_hasTextChildren; }

    void checkConsistency() const;
    void setHasBadChildList();

private:
    virtual bool isInlineFlowBox() const { return true; }

    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    short m_maxHorizontalVisualOverflow;
    
    bool m_includeLeftEdge : 1;
    bool m_includeRightEdge : 1;
    bool m_hasTextChildren : 1;

#ifndef NDEBUG
    bool m_hasBadChildList;
#endif
};

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
