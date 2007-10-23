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
 */

#include "config.h"
#include "InlineBox.h"

#include "HitTestResult.h"
#include "InlineFlowBox.h"
#include "RenderArena.h"
#include "RootInlineBox.h"

using namespace std;

namespace WebCore {
    
#ifndef NDEBUG
static bool inInlineBoxDetach;
#endif

#ifndef NDEBUG

InlineBox::~InlineBox()
{
    if (!m_hasBadParent && m_parent)
        m_parent->setHasBadChildList();
}

#endif

void InlineBox::remove()
{ 
    if (parent())
        parent()->removeChild(this);
}

void InlineBox::destroy(RenderArena* renderArena)
{
#ifndef NDEBUG
    inInlineBoxDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inInlineBoxDetach = false;
#endif

    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void* InlineBox::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void InlineBox::operator delete(void* ptr, size_t sz)
{
    ASSERT(inInlineBoxDetach);

    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

#ifndef NDEBUG
void InlineBox::showTreeForThis() const
{
    if (m_object)
        m_object->showTreeForThis();
}
#endif

int InlineBox::caretMinOffset() const 
{ 
    return 0; 
}

int InlineBox::caretMaxOffset() const 
{ 
    return 1; 
}

unsigned InlineBox::caretMaxRenderedOffset() const 
{ 
    return 1; 
}

void InlineBox::dirtyLineBoxes()
{
    markDirty();
    for (InlineFlowBox* curr = parent(); curr && !curr->isDirty(); curr = curr->parent())
        curr->markDirty();
}

void InlineBox::deleteLine(RenderArena* arena)
{
    if (!m_extracted)
        m_object->setInlineBoxWrapper(0);
    destroy(arena);
}

void InlineBox::extractLine()
{
    m_extracted = true;
    m_object->setInlineBoxWrapper(0);
}

void InlineBox::attachLine()
{
    m_extracted = false;
    m_object->setInlineBoxWrapper(this);
}

void InlineBox::adjustPosition(int dx, int dy)
{
    m_x += dx;
    m_y += dy;
    if (m_object->isReplaced() || m_object->isBR())
        m_object->setPos(m_object->xPos() + dx, m_object->yPos() + dy);
}

void InlineBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (!object()->shouldPaintWithinRoot(paintInfo) || (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection))
        return;

    // Paint all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    bool paintSelectionOnly = paintInfo.phase == PaintPhaseSelection;
    RenderObject::PaintInfo info(paintInfo);
    info.phase = paintSelectionOnly ? paintInfo.phase : PaintPhaseBlockBackground;
    object()->paint(info, tx, ty);
    if (!paintSelectionOnly) {
        info.phase = PaintPhaseChildBlockBackgrounds;
        object()->paint(info, tx, ty);
        info.phase = PaintPhaseFloat;
        object()->paint(info, tx, ty);
        info.phase = PaintPhaseForeground;
        object()->paint(info, tx, ty);
        info.phase = PaintPhaseOutline;
        object()->paint(info, tx, ty);
    }
}

bool InlineBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    // Hit test all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    return object()->hitTest(request, result, IntPoint(x, y), tx, ty);
}

RootInlineBox* InlineBox::root()
{ 
    if (m_parent)
        return m_parent->root(); 
    ASSERT(isRootInlineBox());
    return static_cast<RootInlineBox*>(this);
}

bool InlineBox::nextOnLineExists() const
{
    if (!m_determinedIfNextOnLineExists) {
        m_determinedIfNextOnLineExists = true;

        if (!parent())
            m_nextOnLineExists = false;
        else if (nextOnLine())
            m_nextOnLineExists = true;
        else
            m_nextOnLineExists = parent()->nextOnLineExists();
    }
    return m_nextOnLineExists;
}

bool InlineBox::prevOnLineExists() const
{
    if (!m_determinedIfPrevOnLineExists) {
        m_determinedIfPrevOnLineExists = true;
        
        if (!parent())
            m_prevOnLineExists = false;
        else if (prevOnLine())
            m_prevOnLineExists = true;
        else
            m_prevOnLineExists = parent()->prevOnLineExists();
    }
    return m_prevOnLineExists;
}

InlineBox* InlineBox::firstLeafChild()
{
    return this;
}

InlineBox* InlineBox::lastLeafChild()
{
    return this;
}

InlineBox* InlineBox::nextLeafChild()
{
    return parent() ? parent()->firstLeafChildAfterBox(this) : 0;
}

InlineBox* InlineBox::prevLeafChild()
{
    return parent() ? parent()->lastLeafChildBeforeBox(this) : 0;
}

RenderObject::SelectionState InlineBox::selectionState()
{
    return object()->selectionState();
}

bool InlineBox::canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth)
{
    // Non-replaced elements can always accommodate an ellipsis.
    if (!m_object || !m_object->isReplaced())
        return true;
    
    IntRect boxRect(m_x, 0, m_width, 10);
    IntRect ellipsisRect(ltr ? blockEdge - ellipsisWidth : blockEdge, 0, ellipsisWidth, 10);
    return !(boxRect.intersects(ellipsisRect));
}

int InlineBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&)
{
    // Use -1 to mean "we didn't set the position."
    return -1;
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::InlineBox* b)
{
    if (b)
        b->showTreeForThis();
}

#endif
