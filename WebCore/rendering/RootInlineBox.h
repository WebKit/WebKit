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

#ifndef RootInlineBox_H
#define RootInlineBox_H

#include "bidi.h"
#include "InlineFlowBox.h"

namespace WebCore {

class EllipsisBox;
struct GapRects;

class RootInlineBox : public InlineFlowBox
{
public:
    RootInlineBox(RenderObject* obj)
        : InlineFlowBox(obj)
        , m_topOverflow(0)
        , m_bottomOverflow(0)
        , m_leftOverflow(0)
        , m_rightOverflow(0)
        , m_lineBreakObj(0)
        , m_lineBreakPos(0)
        , m_lineBreakContext(0)
        , m_blockHeight(0)
        , m_endsWithBreak(false)
        , m_hasSelectedChildren(false)
        , m_ellipsisBox(0)
    {
    }
        
    virtual void destroy(RenderArena* renderArena);
    void detachEllipsisBox(RenderArena* renderArena);

    RootInlineBox* nextRootBox() { return static_cast<RootInlineBox*>(m_nextLine); }
    RootInlineBox* prevRootBox() { return static_cast<RootInlineBox*>(m_prevLine); }

    virtual void adjustPosition(int dx, int dy);
    
    virtual bool isRootInlineBox() { return true; }
    virtual int topOverflow() { return m_topOverflow; }
    virtual int bottomOverflow() { return m_bottomOverflow; }
    virtual int leftOverflow() { return m_leftOverflow; }
    virtual int rightOverflow() { return m_rightOverflow; }
    virtual void setVerticalOverflowPositions(int top, int bottom) { m_topOverflow = top; m_bottomOverflow = bottom; }
    virtual void setVerticalSelectionPositions(int top, int bottom) { m_selectionTop = top; m_selectionBottom = bottom; }
    void setHorizontalOverflowPositions(int left, int right) { m_leftOverflow = left; m_rightOverflow = right; }
    void setLineBreakInfo(RenderObject* obj, unsigned breakPos, BidiStatus* status, BidiContext* context);
    void setLineBreakPos(int p) { m_lineBreakPos = p; }

    void setBlockHeight(int h) { m_blockHeight = h; }
    void setEndsWithBreak(bool b) { m_endsWithBreak = b; }
    
    int blockHeight() const { return m_blockHeight; }
    bool endsWithBreak() const { return m_endsWithBreak; }
    RenderObject* lineBreakObj() const { return m_lineBreakObj; }
    unsigned lineBreakPos() const { return m_lineBreakPos; }
    BidiStatus lineBreakBidiStatus() const { return m_lineBreakBidiStatus; }
    BidiContext* lineBreakBidiContext() const { return m_lineBreakContext.get(); }

    void childRemoved(InlineBox* box);

    bool canAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth);
    void placeEllipsis(const AtomicString& ellipsisStr, bool ltr, int blockEdge, int ellipsisWidth, InlineBox* markupBox = 0);
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&);

    EllipsisBox* ellipsisBox() const { return m_ellipsisBox; }
    void paintEllipsisBox(RenderObject::PaintInfo& i, int _tx, int _ty) const;
    bool hitTestEllipsisBox(RenderObject::NodeInfo& info, int _x, int _y, int _tx, int _ty,
                            HitTestAction hitTestAction, bool inBox);
    
    virtual void clearTruncation();

    virtual void paint(RenderObject::PaintInfo& i, int _tx, int _ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty);

    bool hasSelectedChildren() const { return m_hasSelectedChildren; }
    void setHasSelectedChildren(bool b);
    
    virtual RenderObject::SelectionState selectionState();
    InlineBox* firstSelectedBox();
    InlineBox* lastSelectedBox();
    
    GapRects fillLineSelectionGap(int selTop, int selHeight, RenderBlock* rootBlock, int blockX, int blockY, 
                                  int tx, int ty, const RenderObject::PaintInfo* i);
    
    RenderBlock* block() const;

    int selectionTop();
    int selectionBottom() { return m_selectionBottom; }
    int selectionHeight() { return max(0, selectionBottom() - selectionTop()); }
 
    InlineBox* closestLeafChildForXPos(int _x, int _tx);

protected:
    // Normally we are only as tall as the style on our block dictates, but we might have content
    // that spills out above the height of our font (e.g, a tall image), or something that extends further
    // below our line (e.g., a child whose font has a huge descent).
    int m_topOverflow;
    int m_bottomOverflow;
    int m_leftOverflow;
    int m_rightOverflow;

    int m_selectionTop;
    int m_selectionBottom;

    // Where this line ended.  The exact object and the position within that object are stored so that
    // we can create a BidiIterator beginning just after the end of this line.
    RenderObject* m_lineBreakObj;
    unsigned m_lineBreakPos;
    
    BidiStatus m_lineBreakBidiStatus;
    RefPtr<BidiContext> m_lineBreakContext;
    
    // The height of the block at the end of this line.  This is where the next line starts.
    int m_blockHeight;
    
    // Whether the line ends with a <br>.
    bool m_endsWithBreak : 1;
    
    // Whether we have any children selected (this bit will also be set if the <br> that terminates our
    // line is selected).
    bool m_hasSelectedChildren : 1;

    // An inline text box that represents our text truncation string.
    EllipsisBox* m_ellipsisBox;
};

} //namespace

#endif
