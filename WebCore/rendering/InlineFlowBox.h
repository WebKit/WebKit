/*
 * This file is part of the line box implementation for KDE.
 *
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef InlineFlowBox_H
#define InlineFlowBox_H

#include "InlineRunBox.h"

namespace WebCore {

class InlineFlowBox : public InlineRunBox
{
public:
    InlineFlowBox(RenderObject* obj)
    :InlineRunBox(obj)
    {
        m_firstChild = 0;
        m_lastChild = 0;
        m_includeLeftEdge = m_includeRightEdge = false;
        m_hasTextChildren = false;
        m_maxHorizontalShadow = 0;
    }

    RenderFlow* flowObject();

    virtual bool isInlineFlowBox() { return true; }

    InlineFlowBox* prevFlowBox() const { return static_cast<InlineFlowBox*>(m_prevLine); }
    InlineFlowBox* nextFlowBox() const { return static_cast<InlineFlowBox*>(m_nextLine); }
    
    InlineBox* firstChild() { return m_firstChild; }
    InlineBox* lastChild() { return m_lastChild; }

    virtual InlineBox* firstLeafChild();
    virtual InlineBox* lastLeafChild();
    InlineBox* firstLeafChildAfterBox(InlineBox* start=0);
    InlineBox* lastLeafChildBeforeBox(InlineBox* start=0);
        
    virtual void setConstructed() {
        InlineBox::setConstructed();
        if (m_firstChild)
            m_firstChild->setConstructed();
    }
    void addToLine(InlineBox* child);

    virtual void deleteLine(RenderArena* arena);
    virtual void extractLine();
    virtual void attachLine();
    virtual void adjustPosition(int dx, int dy);

    virtual void clearTruncation();
    
    virtual void paintBackgroundAndBorder(RenderObject::PaintInfo& i, int _tx, int _ty);
    void paintBackgrounds(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer,
                          int my, int mh, int _tx, int _ty, int w, int h);
    void paintBackground(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer,
                         int my, int mh, int _tx, int _ty, int w, int h);
    virtual void paintDecorations(RenderObject::PaintInfo& i, int _tx, int _ty, bool paintedChildren = false);
    virtual void paint(RenderObject::PaintInfo& i, int _tx, int _ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty);

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
    void setEdges(bool includeLeft, bool includeRight) {
        m_includeLeftEdge = includeLeft;
        m_includeRightEdge = includeRight;
    }
    virtual bool hasTextChildren() { return m_hasTextChildren; }

    // Helper functions used during line construction and placement.
    void determineSpacingForFlowBoxes(bool lastLine, RenderObject* endObject);
    int getFlowSpacingWidth();
    bool onEndChain(RenderObject* endObject);
    int placeBoxesHorizontally(int x, int& leftPosition, int& rightPosition, bool& needsWordSpacing);
    void verticallyAlignBoxes(int& heightOfBlock);
    void computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                  int& maxAscent, int& maxDescent, bool strictMode);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                   int maxPositionTop, int maxPositionBottom);
    void placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode,
                              int& topPosition, int& bottomPosition, int& selectionTop, int& selectionBottom);
    void shrinkBoxesWithNoTextChildren(int topPosition, int bottomPosition);
    
    virtual void setVerticalOverflowPositions(int top, int bottom) {}
    virtual void setVerticalSelectionPositions(int top, int bottom) {}
    int maxHorizontalShadow() const { return m_maxHorizontalShadow; }

    void removeChild(InlineBox* child);
    
    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&);

protected:
    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    int m_maxHorizontalShadow;
    bool m_includeLeftEdge : 1;
    bool m_includeRightEdge : 1;
    bool m_hasTextChildren : 1;
};

} //namespace

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::InlineBox*);
#endif

#endif
