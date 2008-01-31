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

class HitTestResult;

struct HitTestRequest;

class InlineFlowBox : public InlineRunBox {
public:
    InlineFlowBox(RenderObject* obj)
        : InlineRunBox(obj)
        , m_firstChild(0)
        , m_lastChild(0)
        , m_maxHorizontalVisualOverflow(0)
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

    RenderFlow* flowObject();

    virtual bool isInlineFlowBox() { return true; }

    InlineFlowBox* prevFlowBox() const { return static_cast<InlineFlowBox*>(m_prevLine); }
    InlineFlowBox* nextFlowBox() const { return static_cast<InlineFlowBox*>(m_nextLine); }

    InlineBox* firstChild() { checkConsistency(); return m_firstChild; }
    InlineBox* lastChild() { checkConsistency(); return m_lastChild; }

    virtual InlineBox* firstLeafChild();
    virtual InlineBox* lastLeafChild();
    InlineBox* firstLeafChildAfterBox(InlineBox* start = 0);
    InlineBox* lastLeafChildBeforeBox(InlineBox* start = 0);

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

    virtual void clearTruncation();

    virtual void paintBoxDecorations(RenderObject::PaintInfo&, int tx, int ty);
    void paintBackgrounds(GraphicsContext*, const Color&, const BackgroundLayer*,
                          int my, int mh, int tx, int ty, int w, int h);
    void paintBackground(GraphicsContext*, const Color&, const BackgroundLayer*,
                         int my, int mh, int tx, int ty, int w, int h);
    void paintBoxShadow(GraphicsContext*, RenderStyle*, int tx, int ty, int w, int h);
    virtual void paintTextDecorations(RenderObject::PaintInfo&, int tx, int ty, bool paintedChildren = false);
    virtual void paint(RenderObject::PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

    int marginBorderPaddingLeft();
    int marginBorderPaddingRight();
    int marginLeft();
    int marginRight();
    int borderLeft() { if (includeLeftEdge()) return object()->borderLeft(); return 0; }
    int borderRight() { if (includeRightEdge()) return object()->borderRight(); return 0; }
    int paddingLeft() { if (includeLeftEdge()) return object()->paddingLeft(); return 0; }
    int paddingRight() { if (includeRightEdge()) return object()->paddingRight(); return 0; }

    bool includeLeftEdge() { return m_includeLeftEdge; }
    bool includeRightEdge() { return m_includeRightEdge; }
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
    virtual void verticallyAlignBoxes(int& heightOfBlock);
    void computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                  int& maxAscent, int& maxDescent, bool strictMode);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                   int maxPositionTop, int maxPositionBottom);
    void placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode,
                              int& topPosition, int& bottomPosition, int& selectionTop, int& selectionBottom);
    void shrinkBoxesWithNoTextChildren(int topPosition, int bottomPosition);
    
    virtual void setVerticalOverflowPositions(int top, int bottom) { }
    virtual void setVerticalSelectionPositions(int top, int bottom) { }
    int maxHorizontalVisualOverflow() const { return m_maxHorizontalVisualOverflow; }

    void removeChild(InlineBox* child);

    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&);

    void checkConsistency() const;
    void setHasBadChildList();

private:
    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    int m_maxHorizontalVisualOverflow;

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
void showTree(const WebCore::InlineBox*);
#endif

#endif // InlineFlowBox_h
