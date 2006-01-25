/*
 * This file is part of the line box implementation for KDE.
 *
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef RENDER_LINE_H
#define RENDER_LINE_H

#include "rendering/render_object.h"
#include "bidi.h"
#include "Shared.h"

namespace DOM {
class AtomicString;
};

namespace khtml {

class EllipsisBox;
class InlineFlowBox;
class RootInlineBox;
class RenderBlock;
class RenderFlow;

// InlineBox represents a rectangle that occurs on a line.  It corresponds to
// some RenderObject (i.e., it represents a portion of that RenderObject).
class InlineBox
{
public:
    InlineBox(RenderObject* obj)
    :m_object(obj), m_x(0), m_y(0), m_width(0), m_height(0), m_baseline(0),
     m_firstLine(false), m_constructed(false), m_dirty(false), m_extracted(false)
    {
        m_next = 0;
        m_prev = 0;
        m_parent = 0;
    }

    virtual ~InlineBox() {};

    virtual void destroy(RenderArena* renderArena);

    virtual void deleteLine(RenderArena* arena);
    virtual void extractLine();
    virtual void attachLine();

    virtual void adjustPosition(int dx, int dy);

    virtual void paint(RenderObject::PaintInfo& i, int _tx, int _ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty);
    
    // Overloaded new operator.
    void* operator new(size_t sz, RenderArena* renderArena) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);

private:
    // The normal operator new is disallowed.
    void* operator new(size_t sz) throw();
    
public:
#ifndef NDEBUG
    void showTree() const;
#endif
    virtual bool isInlineBox() { return false; }
    virtual bool isInlineFlowBox() { return false; }
    virtual bool isContainer() { return false; }
    virtual bool isInlineTextBox() { return false; }
    virtual bool isRootInlineBox() { return false; }
    
    virtual bool isText() const { return false; }

    bool isConstructed() { return m_constructed; }
    virtual void setConstructed() {
        m_constructed = true;
        if (m_next)
            m_next->setConstructed();
    }
    void setExtracted(bool b = true) { m_extracted = b; }
    
    void setFirstLineStyleBit(bool f) { m_firstLine = f; }
    bool isFirstLineStyle() const { return m_firstLine; }

    void remove();

    InlineBox* nextOnLine() const { return m_next; }
    InlineBox* prevOnLine() const { return m_prev; }
    void setNextOnLine(InlineBox* next) { m_next = next; }
    void setPrevOnLine(InlineBox* prev) { m_prev = prev; }
    bool nextOnLineExists() const;
    bool prevOnLineExists() const;

    virtual InlineBox* firstLeafChild();
    virtual InlineBox* lastLeafChild();
    InlineBox* nextLeafChild();
    InlineBox* prevLeafChild();
        
    RenderObject* object() const { return m_object; }

    InlineFlowBox* parent() const { return m_parent; }
    void setParent(InlineFlowBox* par) { m_parent = par; }

    RootInlineBox* root();
    
    void setWidth(int w) { m_width = w; }
    int width() const { return m_width; }

    void setXPos(int x) { m_x = x; }
    int xPos() const { return m_x; }

    void setYPos(int y) { m_y = y; }
    int yPos() const { return m_y; }

    void setHeight(int h) { m_height = h; }
    int height() const { return m_height; }
    
    void setBaseline(int b) { m_baseline = b; }
    int baseline() const { return m_baseline; }

    virtual bool hasTextChildren() { return true; }

    virtual int topOverflow() { return yPos(); }
    virtual int bottomOverflow() { return yPos()+height(); }
    virtual int leftOverflow() { return xPos(); }
    virtual int rightOverflow() { return xPos()+width(); }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    
    virtual void clearTruncation() {};

    bool isDirty() const { return m_dirty; }
    void markDirty(bool dirty=true) { m_dirty = dirty; }

    void dirtyLineBoxes();
    
    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&);

public: // FIXME: Would like to make this protected, but methods are accessing these
        // members over in the part.
    RenderObject* m_object;

    int m_x;
    int m_y;
    int m_width;
    int m_height;
    int m_baseline;
    
    bool m_firstLine : 1;
    bool m_constructed : 1;
    bool m_dirty : 1;
    bool m_extracted : 1;

    InlineBox* m_next; // The next element on the same line as us.
    InlineBox* m_prev; // The previous element on the same line as us.

    InlineFlowBox* m_parent; // The box that contains us.
};

class InlineRunBox : public InlineBox
{
public:
    InlineRunBox(RenderObject* obj)
    :InlineBox(obj)
    {
        m_prevLine = 0;
        m_nextLine = 0;
    }

    InlineRunBox* prevLineBox() const { return m_prevLine; }
    InlineRunBox* nextLineBox() const { return m_nextLine; }
    void setNextLineBox(InlineRunBox* n) { m_nextLine = n; }
    void setPreviousLineBox(InlineRunBox* p) { m_prevLine = p; }

    virtual void paintBackgroundAndBorder(RenderObject::PaintInfo& i, int _tx, int _ty) {};
    virtual void paintDecorations(RenderObject::PaintInfo& i, int _tx, int _ty, bool paintedChildren = false) {};
    
protected:
    InlineRunBox* m_prevLine;  // The previous box that also uses our RenderObject
    InlineRunBox* m_nextLine;  // The next box that also uses our RenderObject
};

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
    void paintBackgrounds(QPainter* p, const Color& c, const BackgroundLayer* bgLayer,
                          int my, int mh, int _tx, int _ty, int w, int h);
    void paintBackground(QPainter* p, const Color& c, const BackgroundLayer* bgLayer,
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
                              int& topPosition, int& bottomPosition);
    void shrinkBoxesWithNoTextChildren(int topPosition, int bottomPosition);
    
    virtual void setVerticalOverflowPositions(int top, int bottom) {}

    void removeChild(InlineBox* child);
    
    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&);

protected:
    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    bool m_includeLeftEdge : 1;
    bool m_includeRightEdge : 1;
    bool m_hasTextChildren : 1;
};

class RootInlineBox : public InlineFlowBox
{
public:
    RootInlineBox(RenderObject* obj)
    : InlineFlowBox(obj), m_topOverflow(0), m_bottomOverflow(0), m_leftOverflow(0), m_rightOverflow(0),
      m_lineBreakObj(0), m_lineBreakPos(0), m_lineBreakContext(0),
      m_blockHeight(0), m_endsWithBreak(false), m_hasSelectedChildren(false), m_ellipsisBox(0)
    {}
        
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
    void setHorizontalOverflowPositions(int left, int right) { m_leftOverflow = left; m_rightOverflow = right; }
    void setLineBreakInfo(RenderObject* obj, uint breakPos, BidiStatus* status, BidiContext* context);
    void setLineBreakPos(int p) { m_lineBreakPos = p; }

    void setBlockHeight(int h) { m_blockHeight = h; }
    void setEndsWithBreak(bool b) { m_endsWithBreak = b; }
    
    int blockHeight() const { return m_blockHeight; }
    bool endsWithBreak() const { return m_endsWithBreak; }
    RenderObject* lineBreakObj() const { return m_lineBreakObj; }
    uint lineBreakPos() const { return m_lineBreakPos; }
    BidiStatus lineBreakBidiStatus() const { return m_lineBreakBidiStatus; }
    BidiContext* lineBreakBidiContext() const { return m_lineBreakContext.get(); }

    void childRemoved(InlineBox* box);

    bool canAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth);
    void placeEllipsis(const DOM::AtomicString& ellipsisStr, bool ltr, int blockEdge, int ellipsisWidth, InlineBox* markupBox = 0);
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
    int selectionHeight() { return kMax(0, m_bottomOverflow - selectionTop()); }
 
    InlineBox* closestLeafChildForXPos(int _x, int _tx);

protected:
    // Normally we are only as tall as the style on our block dictates, but we might have content
    // that spills out above the height of our font (e.g, a tall image), or something that extends further
    // below our line (e.g., a child whose font has a huge descent).
    int m_topOverflow;
    int m_bottomOverflow;
    int m_leftOverflow;
    int m_rightOverflow;

    // Where this line ended.  The exact object and the position within that object are stored so that
    // we can create a BidiIterator beginning just after the end of this line.
    RenderObject* m_lineBreakObj;
    uint m_lineBreakPos;
    
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

}; //namespace

#endif
