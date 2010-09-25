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

#include "RenderBoxModelObject.h"
#include "TextDirection.h"

namespace WebCore {

class HitTestRequest;
class HitTestResult;
class RootInlineBox;

// InlineBox represents a rectangle that occurs on a line.  It corresponds to
// some RenderObject (i.e., it represents a portion of that RenderObject).
class InlineBox {
public:
    InlineBox(RenderObject* obj)
        : m_next(0)
        , m_prev(0)
        , m_parent(0)
        , m_renderer(obj)
        , m_x(0)
        , m_y(0)
        , m_logicalWidth(0)
        , m_firstLine(false)
        , m_constructed(false)
        , m_bidiEmbeddingLevel(0)
        , m_dirty(false)
        , m_extracted(false)
#if ENABLE(SVG)
        , m_hasVirtualLogicalHeight(false)
#endif
        , m_isVertical(false)
        , m_endsWithBreak(false)
        , m_hasSelectedChildren(false)
        , m_hasEllipsisBoxOrHyphen(false)
        , m_dirOverride(false)
        , m_isText(false)
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

    InlineBox(RenderObject* obj, int x, int y, int logicalWidth, bool firstLine, bool constructed,
              bool dirty, bool extracted, bool isVertical, InlineBox* next, InlineBox* prev, InlineFlowBox* parent)
        : m_next(next)
        , m_prev(prev)
        , m_parent(parent)
        , m_renderer(obj)
        , m_x(x)
        , m_y(y)
        , m_logicalWidth(logicalWidth)
        , m_firstLine(firstLine)
        , m_constructed(constructed)
        , m_bidiEmbeddingLevel(0)
        , m_dirty(dirty)
        , m_extracted(extracted)
#if ENABLE(SVG)
        , m_hasVirtualLogicalHeight(false)
#endif
        , m_isVertical(isVertical)
        , m_endsWithBreak(false)
        , m_hasSelectedChildren(false)   
        , m_hasEllipsisBoxOrHyphen(false)
        , m_dirOverride(false)
        , m_isText(false)
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

    virtual void paint(PaintInfo&, int tx, int ty);
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

    bool isText() const { return m_isText; }
    void setIsText(bool b) { m_isText = b; }
 
    virtual bool isInlineFlowBox() const { return false; }
    virtual bool isInlineTextBox() const { return false; }
    virtual bool isRootInlineBox() const { return false; }
#if ENABLE(SVG)
    virtual bool isSVGInlineTextBox() const { return false; }
    virtual bool isSVGRootInlineBox() const { return false; }
#endif

    bool hasVirtualLogicalHeight() const { return m_hasVirtualLogicalHeight; }
    void setHasVirtualLogicalHeight() { m_hasVirtualLogicalHeight = true; }
    virtual int virtualLogicalHeight() const
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    bool isVertical() const { return m_isVertical; }
    void setIsVertical(bool v) { m_isVertical = v; }

    virtual IntRect calculateBoundaries() const
    {
        ASSERT_NOT_REACHED();
        return IntRect();
    }

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

    virtual bool isLeaf() const { return true; }
    
    InlineBox* nextLeafChild() const;
    InlineBox* prevLeafChild() const;
        
    RenderObject* renderer() const { return m_renderer; }

    InlineFlowBox* parent() const
    {
        ASSERT(!m_hasBadParent);
        return m_parent;
    }
    void setParent(InlineFlowBox* par) { m_parent = par; }

    const RootInlineBox* root() const;
    RootInlineBox* root();

    // x() is the left side of the box in the containing block's coordinate system.
    void setX(int x) { m_x = x; }
    int x() const { return m_x; }

    // y() is the top side of the box in the containing block's coordinate system.
    void setY(int y) { m_y = y; }
    int y() const { return m_y; }

    // The logicalLeft position is the left edge of the line box in a horizontal line and the top edge in a vertical line.
    int logicalLeft() const { return !m_isVertical ? m_x : m_y; }
    void setLogicalLeft(int left)
    {
        if (!m_isVertical)
            m_x = left;
        else
            m_y = left;
    }

    // The logicalTop[ position is the top edge of the line box in a horizontal line and the left edge in a vertical line.
    int logicalTop() const { return !m_isVertical ? m_y : m_x; }
    void setLogicalTop(int top)
    {
        if (!m_isVertical)
            m_y = top;
        else
            m_x = top;
    }

    // The logical width is our extent in the line's overall inline direction, i.e., width for horizontal text and height for vertical text.
    void setLogicalWidth(int w) { m_logicalWidth = w; }
    int logicalWidth() const { return m_logicalWidth; }

    // The logical height is our extent in the block flow direction, i.e., height for horizontal text and width for vertical text.
    int logicalHeight() const;

    inline int baselinePosition(bool isRootLineBox) const { return renderer()->baselinePosition(m_firstLine, isRootLineBox); }
    inline int lineHeight(bool isRootLineBox) const { return renderer()->lineHeight(m_firstLine, isRootLineBox); }

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    unsigned char bidiLevel() const { return m_bidiEmbeddingLevel; }
    void setBidiLevel(unsigned char level) { m_bidiEmbeddingLevel = level; }
    TextDirection direction() const { return m_bidiEmbeddingLevel % 2 ? RTL : LTR; }
    int caretLeftmostOffset() const { return direction() == LTR ? caretMinOffset() : caretMaxOffset(); }
    int caretRightmostOffset() const { return direction() == LTR ? caretMaxOffset() : caretMinOffset(); }

    virtual void clearTruncation() { }

    bool isDirty() const { return m_dirty; }
    void markDirty(bool dirty = true) { m_dirty = dirty; }

    void dirtyLineBoxes();
    
    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    // visibleLeftEdge, visibleRightEdge are in the parent's coordinate system.
    virtual int placeEllipsisBox(bool ltr, int visibleLeftEdge, int visibleRightEdge, int ellipsisWidth, bool&);

    void setHasBadParent();

    int toAdd() const { return m_toAdd; }
    
    bool visibleToHitTesting() const { return renderer()->style()->visibility() == VISIBLE && renderer()->style()->pointerEvents() != PE_NONE; }
    
    // Use with caution! The type is not checked!
    RenderBoxModelObject* boxModelObject() const
    { 
        if (!m_renderer->isText())
            return toRenderBoxModelObject(m_renderer);
        return 0;
    }

private:
    InlineBox* m_next; // The next element on the same line as us.
    InlineBox* m_prev; // The previous element on the same line as us.

    InlineFlowBox* m_parent; // The box that contains us.

public:
    RenderObject* m_renderer;

    int m_x;
    int m_y;
    int m_logicalWidth;
    
    // Some of these bits are actually for subclasses and moved here to compact the structures.

    // for this class
protected:
    bool m_firstLine : 1;
private:
    bool m_constructed : 1;
    unsigned char m_bidiEmbeddingLevel : 6;
protected:
    bool m_dirty : 1;
    bool m_extracted : 1;
    bool m_hasVirtualLogicalHeight : 1;

    bool m_isVertical : 1;

    // for RootInlineBox
    bool m_endsWithBreak : 1;  // Whether the line ends with a <br>.
    bool m_hasSelectedChildren : 1; // Whether we have any children selected (this bit will also be set if the <br> that terminates our line is selected).
    bool m_hasEllipsisBoxOrHyphen : 1; 

    // for InlineTextBox
public:
    bool m_dirOverride : 1;
    bool m_isText : 1; // Whether or not this object represents text with a non-zero height. Includes non-image list markers, text boxes.
protected:
    mutable bool m_determinedIfNextOnLineExists : 1;
    mutable bool m_determinedIfPrevOnLineExists : 1;
    mutable bool m_nextOnLineExists : 1;
    mutable bool m_prevOnLineExists : 1;
    int m_toAdd : 11; // for justified text

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
