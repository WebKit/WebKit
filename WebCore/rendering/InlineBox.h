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

#ifndef InlineBox_h
#define InlineBox_h

#include "RenderObject.h" // needed for RenderObject::PaintInfo

namespace WebCore {

class HitTestResult;
class RootInlineBox;

struct HitTestRequest;

// InlineBox represents a rectangle that occurs on a line.  It corresponds to
// some RenderObject (i.e., it represents a portion of that RenderObject).
class InlineBox {
public:
    InlineBox(RenderObject* obj)
        : m_object(obj)
        , m_x(0)
        , m_y(0)
        , m_width(0)
        , m_height(0)
        , m_baseline(0)
        , m_next(0)
        , m_prev(0)
        , m_parent(0)
        , m_firstLine(false)
        , m_constructed(false)
        , m_dirty(false)
        , m_extracted(false)
        , m_includeLeftEdge(false)
        , m_includeRightEdge(false)
        , m_hasTextChildren(true)
        , m_endsWithBreak(false)
        , m_hasSelectedChildren(false)
        , m_hasEllipsisBox(false)
        , m_reversed(false)
        , m_treatAsText(true)
        , m_determinedIfNextOnLineExists(false)
        , m_determinedIfPrevOnLineExists(false)
        , m_nextOnLineExists(false)
        , m_prevOnLineExists(false)
        , m_toAdd(0)
#ifndef NDEBUG
        , m_hasBadParent(false)
#endif
    {
    }

    InlineBox(RenderObject* obj, int x, int y, int width, int height, int baseline, bool firstLine, bool constructed,
              bool dirty, bool extracted, InlineBox* next, InlineBox* prev, InlineFlowBox* parent)
        : m_object(obj)
        , m_x(x)
        , m_y(y)
        , m_width(width)
        , m_height(height)
        , m_baseline(baseline)
        , m_next(next)
        , m_prev(prev)
        , m_parent(parent)
        , m_firstLine(firstLine)
        , m_constructed(constructed)
        , m_dirty(dirty)
        , m_extracted(extracted)
        , m_includeLeftEdge(false)
        , m_includeRightEdge(false)
        , m_hasTextChildren(true)
        , m_endsWithBreak(false)
        , m_hasSelectedChildren(false)   
        , m_hasEllipsisBox(false)
        , m_reversed(false)
        , m_treatAsText(true)
        , m_determinedIfNextOnLineExists(false)
        , m_determinedIfPrevOnLineExists(false)
        , m_nextOnLineExists(false)
        , m_prevOnLineExists(false)
        , m_toAdd(0)
#ifndef NDEBUG
        , m_hasBadParent(false)
#endif
    {
    }

    virtual ~InlineBox();

    virtual void destroy(RenderArena*);

    virtual void deleteLine(RenderArena*);
    virtual void extractLine();
    virtual void attachLine();

    virtual bool isLineBreak() const { return false; }

    virtual void adjustPosition(int dx, int dy);

    virtual void paint(RenderObject::PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

    // Overloaded new operator.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

private:
    // The normal operator new is disallowed.
    void* operator new(size_t) throw();

public:
#ifndef NDEBUG
    void showTreeForThis() const;
#endif
    virtual bool isInlineBox() { return false; }
    virtual bool isInlineFlowBox() { return false; }
    virtual bool isContainer() { return false; }
    virtual bool isInlineTextBox() { return false; }
    virtual bool isRootInlineBox() { return false; }
#if ENABLE(SVG) 
    virtual bool isSVGRootInlineBox() { return false; }
#endif
    virtual bool isText() const { return false; }

    bool isConstructed() { return m_constructed; }
    virtual void setConstructed()
    {
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
    void setNextOnLine(InlineBox* next)
    {
        ASSERT(m_parent || !next);
        m_next = next;
    }
    void setPrevOnLine(InlineBox* prev)
    {
        ASSERT(m_parent || !prev);
        m_prev = prev;
    }
    bool nextOnLineExists() const;
    bool prevOnLineExists() const;

    virtual InlineBox* firstLeafChild();
    virtual InlineBox* lastLeafChild();
    InlineBox* nextLeafChild();
    InlineBox* prevLeafChild();
        
    RenderObject* object() const { return m_object; }

    InlineFlowBox* parent() const
    {
        ASSERT(!m_hasBadParent);
        return m_parent;
    }
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

    bool hasTextChildren() const { return m_hasTextChildren; }

    virtual int topOverflow() { return yPos(); }
    virtual int bottomOverflow() { return yPos() + height(); }
    virtual int leftOverflow() { return xPos(); }
    virtual int rightOverflow() { return xPos() + width(); }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    
    virtual void clearTruncation() { }

    bool isDirty() const { return m_dirty; }
    void markDirty(bool dirty = true) { m_dirty = dirty; }

    void dirtyLineBoxes();
    
    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&);

    void setHasBadParent();

    int toAdd() const { return m_toAdd; }
    
public:
    RenderObject* m_object;

    int m_x;
    int m_y;
    int m_width;
    int m_height;
    int m_baseline;

private:
    InlineBox* m_next; // The next element on the same line as us.
    InlineBox* m_prev; // The previous element on the same line as us.

    InlineFlowBox* m_parent; // The box that contains us.
    
    // Some of these bits are actually for subclasses and moved here to compact the structures.

    // for this class
protected:
    bool m_firstLine : 1;
private:
    bool m_constructed : 1;
protected:
    bool m_dirty : 1;
    bool m_extracted : 1;

    // for InlineFlowBox
    bool m_includeLeftEdge : 1;
    bool m_includeRightEdge : 1;
    bool m_hasTextChildren : 1;

    // for RootInlineBox
    bool m_endsWithBreak : 1;  // Whether the line ends with a <br>.
    bool m_hasSelectedChildren : 1; // Whether we have any children selected (this bit will also be set if the <br> that terminates our line is selected).
    bool m_hasEllipsisBox : 1; 

    // for InlineTextBox
public:
    bool m_reversed : 1;
    bool m_dirOverride : 1;
    bool m_treatAsText : 1; // Whether or not to treat a <br> as text for the purposes of line height.
protected:
    mutable bool m_determinedIfNextOnLineExists : 1;
    mutable bool m_determinedIfPrevOnLineExists : 1;
    mutable bool m_nextOnLineExists : 1;
    mutable bool m_prevOnLineExists : 1;
    int m_toAdd : 13; // for justified text

#ifndef NDEBUG
private:
    bool m_hasBadParent;
#endif
};

#ifdef NDEBUG
inline InlineBox::~InlineBox()
{
}
#endif

inline void InlineBox::setHasBadParent()
{
#ifndef NDEBUG
    m_hasBadParent = true;
#endif
}

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::InlineBox*);
#endif

#endif // InlineBox_h
