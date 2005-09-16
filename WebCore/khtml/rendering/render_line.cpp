/**
* This file is part of the html renderer for KDE.
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
 */
// -------------------------------------------------------------------------

#include <kdebug.h>
#include <assert.h>
#include <qpainter.h>
#include <qpen.h>
#include <kglobal.h>

#include "rendering/render_flow.h"
#include "rendering/render_text.h"
#include "rendering/render_table.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_formimpl.h"
#include "render_inline.h"
#include "render_block.h"
#include "render_arena.h"
#include "render_line.h"

#include "khtmlview.h"

using namespace DOM;

#ifndef NDEBUG
static bool inInlineBoxDetach;
#endif

namespace khtml {
    
class EllipsisBox : public InlineBox
{
public:
    EllipsisBox(RenderObject* obj, const DOM::AtomicString& ellipsisStr, InlineFlowBox* p,
                int w, int y, int h, int b, bool firstLine, InlineBox* markupBox)
    :InlineBox(obj), m_str(ellipsisStr) {
        m_parent = p;
        m_width = w;
        m_y = y;
        m_height = h;
        m_baseline = b;
        m_firstLine = firstLine;
        m_constructed = true;
        m_markupBox = markupBox;
    }
    
    virtual void paint(RenderObject::PaintInfo& i, int _tx, int _ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& info, int _x, int _y, int _tx, int _ty);

private:
    DOM::AtomicString m_str;
    InlineBox* m_markupBox;
};

void InlineBox::remove()
{ 
    if (parent())
        parent()->removeChild(this);
}

void InlineBox::detach(RenderArena* renderArena)
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
    assert(inInlineBoxDetach);

    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

#ifndef NDEBUG
void InlineBox::showTree() const
{
    if (m_object)
        m_object->showTree();
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
    m_object->setInlineBoxWrapper(0);
    detach(arena);
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

void InlineBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    if (!object()->shouldPaintWithinRoot(i) || i.phase == PaintActionOutline)
        return;

    // Paint all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    bool paintSelectionOnly = i.phase == PaintActionSelection;
    RenderObject::PaintInfo info(i.p, i.r, paintSelectionOnly ? i.phase : PaintActionBlockBackground, i.paintingRoot);
    object()->paint(info, tx, ty);
    if (!paintSelectionOnly) {
        info.phase = PaintActionChildBlockBackgrounds;
        object()->paint(info, tx, ty);
        info.phase = PaintActionFloat;
        object()->paint(info, tx, ty);
        info.phase = PaintActionForeground;
        object()->paint(info, tx, ty);
        info.phase = PaintActionOutline;
        object()->paint(info, tx, ty);
    }
}

bool InlineBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    // Hit test all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    return object()->hitTest(i, x, y, tx, ty);
}

RootInlineBox* InlineBox::root()
{ 
    if (m_parent)
        return m_parent->root(); 
    return static_cast<RootInlineBox*>(this);
}

bool InlineBox::nextOnLineExists() const
{
    if (!parent())
        return false;
    
    if (nextOnLine())
        return true;
    
    return parent()->nextOnLineExists();
}

bool InlineBox::prevOnLineExists() const
{
    if (!parent())
        return false;
    
    if (prevOnLine())
        return true;
    
    return parent()->prevOnLineExists();
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
    
    QRect boxRect(m_x, 0, m_width, 10);
    QRect ellipsisRect(ltr ? blockEdge - ellipsisWidth : blockEdge, 0, ellipsisWidth, 10);
    return !(boxRect.intersects(ellipsisRect));
}

int InlineBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool&)
{
    // Use -1 to mean "we didn't set the position."
    return -1;
}

RenderFlow* InlineFlowBox::flowObject()
{
    return static_cast<RenderFlow*>(m_object);
}

int InlineFlowBox::marginLeft()
{
    if (!includeLeftEdge())
        return 0;
    
    RenderStyle* cstyle = object()->style();
    Length margin = cstyle->marginLeft();
    if (margin.type != Variable)
        return (margin.type == Fixed ? margin.value : object()->marginLeft());
    return 0;
}

int InlineFlowBox::marginRight()
{
    if (!includeRightEdge())
        return 0;
    
    RenderStyle* cstyle = object()->style();
    Length margin = cstyle->marginRight();
    if (margin.type != Variable)
        return (margin.type == Fixed ? margin.value : object()->marginRight());
    return 0;
}

int InlineFlowBox::marginBorderPaddingLeft()
{
    return marginLeft() + borderLeft() + paddingLeft();
}

int InlineFlowBox::marginBorderPaddingRight()
{
    return marginRight() + borderRight() + paddingRight();
}

int InlineFlowBox::getFlowSpacingWidth()
{
    int totWidth = marginBorderPaddingLeft() + marginBorderPaddingRight();
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->isInlineFlowBox())
            totWidth += static_cast<InlineFlowBox*>(curr)->getFlowSpacingWidth();
    }
    return totWidth;
}

void InlineFlowBox::addToLine(InlineBox* child) {
    if (!m_firstChild)
        m_firstChild = m_lastChild = child;
    else {
        m_lastChild->m_next = child;
        child->m_prev = m_lastChild;
        m_lastChild = child;
    }
    child->setFirstLineStyleBit(m_firstLine);
    child->setParent(this);
    if (child->isText())
        m_hasTextChildren = true;
    if (child->object()->selectionState() != RenderObject::SelectionNone)
        root()->setHasSelectedChildren(true);
}

void InlineFlowBox::removeChild(InlineBox* child)
{
    if (!m_dirty)
        dirtyLineBoxes();

    root()->childRemoved(child);

    if (child == m_firstChild)
        m_firstChild = child->nextOnLine();
    if (child == m_lastChild)
        m_lastChild = child->prevOnLine();
    if (child->nextOnLine())
        child->nextOnLine()->setPrevOnLine(child->prevOnLine());
    if (child->prevOnLine())
        child->prevOnLine()->setNextOnLine(child->nextOnLine());
    
    child->setParent(0);
}

void InlineFlowBox::deleteLine(RenderArena* arena)
{
    InlineBox* child = m_firstChild;
    InlineBox* next = 0;
    while (child) {
        next = child->nextOnLine();
        child->deleteLine(arena);
        child = next;
    }
    
    static_cast<RenderFlow*>(m_object)->removeLineBox(this);
    detach(arena);
}

void InlineFlowBox::extractLine()
{
    if (!m_extracted)
        static_cast<RenderFlow*>(m_object)->extractLineBox(this);
    for (InlineBox* child = m_firstChild; child; child = child->nextOnLine())
        child->extractLine();
}

void InlineFlowBox::attachLine()
{
    if (m_extracted)
        static_cast<RenderFlow*>(m_object)->attachLineBox(this);
    for (InlineBox* child = m_firstChild; child; child = child->nextOnLine())
        child->attachLine();
}

void InlineFlowBox::adjustPosition(int dx, int dy)
{
    InlineRunBox::adjustPosition(dx, dy);
    for (InlineBox* child = m_firstChild; child; child = child->nextOnLine())
        child->adjustPosition(dx, dy);
}

bool InlineFlowBox::onEndChain(RenderObject* endObject)
{
    if (!endObject)
        return false;
    
    if (endObject == object())
        return true;

    RenderObject* curr = endObject;
    RenderObject* parent = curr->parent();
    while (parent && !parent->isRenderBlock()) {
        if (parent->lastChild() != curr || parent == object())
            return false;
            
        curr = parent;
        parent = curr->parent();
    }

    return true;
}

void InlineFlowBox::determineSpacingForFlowBoxes(bool lastLine, RenderObject* endObject)
{
    // All boxes start off open.  They will not apply any margins/border/padding on
    // any side.
    bool includeLeftEdge = false;
    bool includeRightEdge = false;

    RenderFlow* flow = static_cast<RenderFlow*>(object());
    
    if (!flow->firstChild())
        includeLeftEdge = includeRightEdge = true; // Empty inlines never split across lines.
    else if (parent()) { // The root inline box never has borders/margins/padding.
        bool ltr = flow->style()->direction() == LTR;
        
        // Check to see if all initial lines are unconstructed.  If so, then
        // we know the inline began on this line.
        if (!flow->firstLineBox()->isConstructed()) {
            if (ltr && flow->firstLineBox() == this)
                includeLeftEdge = true;
            else if (!ltr && flow->lastLineBox() == this)
                includeRightEdge = true;
        }
    
        // In order to determine if the inline ends on this line, we check three things:
        // (1) If we are the last line and we don't have a continuation(), then we can
        // close up.
        // (2) If the last line box for the flow has an object following it on the line (ltr,
        // reverse for rtl), then the inline has closed.
        // (3) The line may end on the inline.  If we are the last child (climbing up
        // the end object's chain), then we just closed as well.
        if (!flow->lastLineBox()->isConstructed()) {
            if (ltr) {
                if (!nextLineBox() &&
                    ((lastLine && !object()->continuation()) || nextOnLineExists()
                     || onEndChain(endObject)))
                    includeRightEdge = true;
            }
            else {
                if ((!prevLineBox() || !prevLineBox()->isConstructed()) &&
                    ((lastLine && !object()->continuation()) ||
                     prevOnLineExists() || onEndChain(endObject)))
                    includeLeftEdge = true;
            }
        }
    }

    setEdges(includeLeftEdge, includeRightEdge);

    // Recur into our children.
    for (InlineBox* currChild = firstChild(); currChild; currChild = currChild->nextOnLine()) {
        if (currChild->isInlineFlowBox()) {
            InlineFlowBox* currFlow = static_cast<InlineFlowBox*>(currChild);
            currFlow->determineSpacingForFlowBoxes(lastLine, endObject);
        }
    }
}

int InlineFlowBox::placeBoxesHorizontally(int x, int& leftPosition, int& rightPosition, bool & needsWordSpacing)
{
    // Set our x position.
    setXPos(x);
    leftPosition = kMin(x, leftPosition);

    int startX = x;
    x += borderLeft() + paddingLeft();
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText()) {
            InlineTextBox *text = static_cast<InlineTextBox*>(curr);
            RenderText *rt = static_cast<RenderText*>(text->object());
            if (rt->length()) {
                if (needsWordSpacing && rt->text()[text->start()].isSpace())
                    x += rt->htmlFont(m_firstLine)->getWordSpacing();
                needsWordSpacing = !rt->text()[text->end()].isSpace();
            }
            text->setXPos(x);
            leftPosition = kMin(x, leftPosition);
            rightPosition = kMax(x + text->width(), rightPosition);
            x += text->width();
        }
        else {
            if (curr->object()->isPositioned()) {
                if (curr->object()->parent()->style()->direction() == LTR)
                    curr->setXPos(x);
                else
                    // Our offset that we cache needs to be from the edge of the right border box and
                    // not the left border box.  We have to subtract |x| from the width of the block
                    // (which can be obtained from the root line box).
                    curr->setXPos(root()->object()->width()-x);
                continue; // The positioned object has no effect on the width.
            }
            if (curr->object()->isInlineFlow()) {
                InlineFlowBox* flow = static_cast<InlineFlowBox*>(curr);
                if (curr->object()->isCompact()) {
                    int ignoredX = x;
                    flow->placeBoxesHorizontally(ignoredX, leftPosition, rightPosition, needsWordSpacing);
                }
                else {
                    x += flow->marginLeft();
                    x = flow->placeBoxesHorizontally(x, leftPosition, rightPosition, needsWordSpacing);
                    x += flow->marginRight();
                }
            }
            else if (!curr->object()->isCompact()) {
                x += curr->object()->marginLeft();
                curr->setXPos(x);
                leftPosition = kMin(x, leftPosition);
                rightPosition = kMax(x + curr->width(), rightPosition);
                x += curr->width() + curr->object()->marginRight();
            }
        }
    }

    x += borderRight() + paddingRight();
    setWidth(x-startX);
    rightPosition = kMax(xPos() + width(), rightPosition);

    return x;
}

void InlineFlowBox::verticallyAlignBoxes(int& heightOfBlock)
{
    int maxPositionTop = 0;
    int maxPositionBottom = 0;
    int maxAscent = 0;
    int maxDescent = 0;

    // Figure out if we're in strict mode.  Note that we can't simply use !style()->htmlHacks(),
    // because that would match almost strict mode as well.
    RenderObject* curr = object();
    while (curr && !curr->element())
        curr = curr->container();
    bool strictMode = (curr && curr->document()->inStrictMode());
    
    computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, strictMode);

    if (maxAscent + maxDescent < kMax(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    int maxHeight = maxAscent + maxDescent;
    int topPosition = heightOfBlock;
    int bottomPosition = heightOfBlock;
    placeBoxesVertically(heightOfBlock, maxHeight, maxAscent, strictMode, topPosition, bottomPosition);

    setVerticalOverflowPositions(topPosition, bottomPosition);

    // Shrink boxes with no text children in quirks and almost strict mode.
    if (!strictMode)
        shrinkBoxesWithNoTextChildren(topPosition, bottomPosition);
    
    heightOfBlock += maxHeight;
}

void InlineFlowBox::adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                              int maxPositionTop, int maxPositionBottom)
{
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        // The computed lineheight needs to be extended for the
        // positioned elements
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        if (curr->yPos() == PositionTop || curr->yPos() == PositionBottom) {
            if (curr->yPos() == PositionTop) {
                if (maxAscent + maxDescent < curr->height())
                    maxDescent = curr->height() - maxAscent;
            }
            else {
                if (maxAscent + maxDescent < curr->height())
                    maxAscent = curr->height() - maxDescent;
            }

            if (maxAscent + maxDescent >= kMax(maxPositionTop, maxPositionBottom))
                break;
        }

        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);
    }
}

void InlineFlowBox::computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                             int& maxAscent, int& maxDescent, bool strictMode)
{
    if (isRootInlineBox()) {
        // Examine our root box.
        setHeight(object()->lineHeight(m_firstLine, true));
        bool isTableCell = object()->isTableCell();
        if (isTableCell) {
            RenderTableCell* tableCell = static_cast<RenderTableCell*>(object());
            setBaseline(tableCell->RenderBlock::baselinePosition(m_firstLine, true));
        }
        else
            setBaseline(object()->baselinePosition(m_firstLine, true));
        if (hasTextChildren() || strictMode) {
            int ascent = baseline();
            int descent = height() - ascent;
            if (maxAscent < ascent)
                maxAscent = ascent;
            if (maxDescent < descent)
                maxDescent = descent;
        }
    }
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        curr->setHeight(curr->object()->lineHeight(m_firstLine));
        curr->setBaseline(curr->object()->baselinePosition(m_firstLine));
        curr->setYPos(curr->object()->verticalPositionHint(m_firstLine));
        if (curr->yPos() == PositionTop) {
            if (maxPositionTop < curr->height())
                maxPositionTop = curr->height();
        }
        else if (curr->yPos() == PositionBottom) {
            if (maxPositionBottom < curr->height())
                maxPositionBottom = curr->height();
        }
        else if (curr->hasTextChildren() || strictMode) {
            int ascent = curr->baseline() - curr->yPos();
            int descent = curr->height() - ascent;
            if (maxAscent < ascent)
                maxAscent = ascent;
            if (maxDescent < descent)
                maxDescent = descent;
        }

        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, strictMode);
    }
}

void InlineFlowBox::placeBoxesVertically(int y, int maxHeight, int maxAscent, bool strictMode,
                                         int& topPosition, int& bottomPosition)
{
    if (isRootInlineBox())
        setYPos(y + maxAscent - baseline());// Place our root box.
    
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        // Adjust boxes to use their real box y/height and not the logical height (as dictated by
        // line-height).
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->placeBoxesVertically(y, maxHeight, maxAscent, strictMode,
                                                                    topPosition, bottomPosition);

        bool childAffectsTopBottomPos = true;
        if (curr->yPos() == PositionTop)
            curr->setYPos(y);
        else if (curr->yPos() == PositionBottom)
            curr->setYPos(y + maxHeight - curr->height());
        else {
            if (!curr->hasTextChildren() && !strictMode)
                childAffectsTopBottomPos = false;
            curr->setYPos(curr->yPos() + y + maxAscent - curr->baseline());
        }
        
        int newY = curr->yPos();
        int newHeight = curr->height();
        int newBaseline = curr->baseline();
        if (curr->isText() || curr->isInlineFlowBox()) {
            const QFontMetrics &fm = curr->object()->fontMetrics( m_firstLine );
            newBaseline = fm.ascent();
            newY += curr->baseline() - newBaseline;
            newHeight = newBaseline+fm.descent();
            if (curr->isInlineFlowBox()) {
                newHeight += curr->object()->borderTop() + curr->object()->paddingTop() +
                            curr->object()->borderBottom() + curr->object()->paddingBottom();
                newY -= curr->object()->borderTop() + curr->object()->paddingTop();
                newBaseline += curr->object()->borderTop() + curr->object()->paddingTop();
            }	
        }
        else if (!curr->object()->isBR()) {
            newY += curr->object()->marginTop();
            newHeight = curr->height() - (curr->object()->marginTop() + curr->object()->marginBottom());
        }

        curr->setYPos(newY);
        curr->setHeight(newHeight);
        curr->setBaseline(newBaseline);

        if (childAffectsTopBottomPos) {
            if (newY < topPosition)
                topPosition = newY;
            if (newY + newHeight > bottomPosition)
                bottomPosition = newY + newHeight;
        }
    }

    if (isRootInlineBox()) {
        const QFontMetrics &fm = object()->fontMetrics( m_firstLine );
        setHeight(fm.ascent()+fm.descent());
        setYPos(yPos() + baseline() - fm.ascent());
        setBaseline(fm.ascent());
        if (hasTextChildren() || strictMode) {
            if (yPos() < topPosition)
                topPosition = yPos();
            if (yPos() + height() > bottomPosition)
                bottomPosition = yPos() + height();
        }
    }
}

void InlineFlowBox::shrinkBoxesWithNoTextChildren(int topPos, int bottomPos)
{
    // First shrink our kids.
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isPositioned())
            continue; // Positioned placeholders don't affect calculations.
        
        if (curr->isInlineFlowBox())
            static_cast<InlineFlowBox*>(curr)->shrinkBoxesWithNoTextChildren(topPos, bottomPos);
    }

    // See if we have text children. If not, then we need to shrink ourselves to fit on the line.
    if (!hasTextChildren()) {
        if (yPos() < topPos)
            setYPos(topPos);
        if (yPos() + height() > bottomPos)
            setHeight(bottomPos - yPos());
        if (baseline() > height())
            setBaseline(height());
    }
}

bool InlineFlowBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    // Check children first.
    for (InlineBox* curr = lastChild(); curr; curr = curr->prevOnLine()) {
        if (!curr->object()->layer() && curr->nodeAtPoint(i, x, y, tx, ty)) {
            object()->setInnerNode(i);
            return true;
        }
    }

    // Now check ourselves.
    QRect rect(tx + m_x, ty + m_y, m_width, m_height);
    if (object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->setInnerNode(i);
        return true;
    }
    
    return false;
}

void InlineFlowBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    bool intersectsDamageRect = true;
    int xPos = tx + m_x - object()->maximalOutlineSize(i.phase);
    int w = width() + 2 * object()->maximalOutlineSize(i.phase);
    if ((xPos >= i.r.x() + i.r.width()) || (xPos + w <= i.r.x()))
        intersectsDamageRect = false;
    
    if (intersectsDamageRect) {
        if (i.phase == PaintActionOutline) {
            // Add ourselves to the paint info struct's list of inlines that need to paint their
            // outlines.
            if (object()->style()->visibility() == VISIBLE && object()->style()->outlineWidth() > 0 &&
                !object()->isInlineContinuation() && !isRootInlineBox()) {
                if (!i.outlineObjects)
                    i.outlineObjects = new QPtrDict<RenderFlow>;
                if (!i.outlineObjects->find(flowObject()))
                    i.outlineObjects->insert(flowObject(), flowObject());
            }
        }
        else {
            // 1. Paint our background and border.
            paintBackgroundAndBorder(i, tx, ty);
            
            // 2. Paint our underline and overline.
            paintDecorations(i, tx, ty, false);
        }
    }

    // 3. Paint our children.
    for (InlineBox* curr = firstChild(); curr; curr = curr->nextOnLine()) {
        if (!curr->object()->layer())
            curr->paint(i, tx, ty);
    }

    // 4. Paint our strike-through
    if (intersectsDamageRect && i.phase != PaintActionOutline)
        paintDecorations(i, tx, ty, true);
}

void InlineFlowBox::paintBackgrounds(QPainter* p, const QColor& c, const BackgroundLayer* bgLayer,
                                     int my, int mh, int _tx, int _ty, int w, int h)
{
    if (!bgLayer)
        return;
    paintBackgrounds(p, c, bgLayer->next(), my, mh, _tx, _ty, w, h);
    paintBackground(p, c, bgLayer, my, mh, _tx, _ty, w, h);
}

void InlineFlowBox::paintBackground(QPainter* p, const QColor& c, const BackgroundLayer* bgLayer,
                                    int my, int mh, int _tx, int _ty, int w, int h)
{
    CachedImage* bg = bgLayer->backgroundImage();
    bool hasBackgroundImage = bg && (bg->pixmap_size() == bg->valid_rect().size()) &&
                              !bg->isTransparent() && !bg->isErrorImage();
    if (!hasBackgroundImage || (!prevLineBox() && !nextLineBox()) || !parent())
        object()->paintBackgroundExtended(p, c, bgLayer, my, mh, _tx, _ty, w, h, 
                                          borderLeft(), borderRight(), paddingLeft(), paddingRight());
    else {
        // We have a background image that spans multiple lines.
        // We need to adjust _tx and _ty by the width of all previous lines.
        // Think of background painting on inlines as though you had one long line, a single continuous
        // strip.  Even though that strip has been broken up across multiple lines, you still paint it
        // as though you had one single line.  This means each line has to pick up the background where
        // the previous line left off.
        // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
        // but it isn't even clear how this should work at all.
        int xOffsetOnLine = 0;
        for (InlineRunBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
            xOffsetOnLine += curr->width();
        int startX = _tx - xOffsetOnLine;
        int totalWidth = xOffsetOnLine;
        for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
            totalWidth += curr->width();
        QRect clipRect(_tx, _ty, width(), height());
        clipRect = p->xForm(clipRect);
        p->save();
        p->addClip(clipRect);
        object()->paintBackgroundExtended(p, c, bgLayer, my, mh, startX, _ty,
                                          totalWidth, h, borderLeft(), borderRight(), paddingLeft(), paddingRight());
        p->restore();
    }
}

void InlineFlowBox::paintBackgroundAndBorder(RenderObject::PaintInfo& i, int _tx, int _ty)
{
    if (!object()->shouldPaintWithinRoot(i) || object()->style()->visibility() != VISIBLE ||
        i.phase != PaintActionForeground)
        return;

    // Move x/y to our coordinates.
    _tx += m_x;
    _ty += m_y;
    
    int w = width();
    int h = height();

    int my = kMax(_ty, i.r.y());
    int mh;
    if (_ty < i.r.y())
        mh= kMax(0, h - (i.r.y() - _ty));
    else
        mh = kMin(i.r.height(), h);

    QPainter* p = i.p;
    
    // You can use p::first-line to specify a background. If so, the root line boxes for
    // a line may actually have to paint a background.
    RenderStyle* styleToUse = object()->style(m_firstLine);
    if ((!parent() && m_firstLine && styleToUse != object()->style()) || 
        (parent() && object()->shouldPaintBackgroundOrBorder())) {
        QColor c = styleToUse->backgroundColor();
        paintBackgrounds(p, c, styleToUse->backgroundLayers(), my, mh, _tx, _ty, w, h);

        // :first-line cannot be used to put borders on a line. Always paint borders with our
        // non-first-line style.
        if (parent() && object()->style()->hasBorder()) {
            CachedImage* borderImage = object()->style()->borderImage().image();
            bool hasBorderImage = borderImage && (borderImage->pixmap_size() == borderImage->valid_rect().size()) &&
                                  !borderImage->isTransparent() && !borderImage->isErrorImage();
            if (hasBorderImage && !borderImage->isLoaded())
                return; // Don't paint anything while we wait for the image to load.
            
            // The simple case is where we either have no border image or we are the only box for this object.  In those
            // cases only a single call to draw is required.
            if (!hasBorderImage || (!prevLineBox() && !nextLineBox()))
                object()->paintBorder(p, _tx, _ty, w, h, object()->style(), includeLeftEdge(), includeRightEdge());
            else {
                // We have a border image that spans multiple lines.
                // We need to adjust _tx and _ty by the width of all previous lines.
                // Think of border image painting on inlines as though you had one long line, a single continuous
                // strip.  Even though that strip has been broken up across multiple lines, you still paint it
                // as though you had one single line.  This means each line has to pick up the image where
                // the previous line left off.
                // FIXME: What the heck do we do with RTL here? The math we're using is obviously not right,
                // but it isn't even clear how this should work at all.
                int xOffsetOnLine = 0;
                for (InlineRunBox* curr = prevLineBox(); curr; curr = curr->prevLineBox())
                    xOffsetOnLine += curr->width();
                int startX = _tx - xOffsetOnLine;
                int totalWidth = xOffsetOnLine;
                for (InlineRunBox* curr = this; curr; curr = curr->nextLineBox())
                    totalWidth += curr->width();
                QRect clipRect(_tx, _ty, width(), height());
                clipRect = p->xForm(clipRect);
                p->save();
                p->addClip(clipRect);
                object()->paintBorder(p, startX, _ty, totalWidth, h, object()->style());
                p->restore();
            }
        }
    }
}

static bool shouldDrawDecoration(RenderObject* obj)
{
    bool shouldDraw = false;
    for (RenderObject* curr = obj->firstChild();
         curr; curr = curr->nextSibling()) {
        if (curr->isInlineFlow()) {
            shouldDraw = true;
            break;
        }
        else if (curr->isText() && !curr->isBR() && (curr->style()->whiteSpace() == PRE ||
                 !curr->element() || !curr->element()->containsOnlyWhitespace())) {
            shouldDraw = true;
            break;
        }	
    }
    return shouldDraw;
}

void InlineFlowBox::paintDecorations(RenderObject::PaintInfo& i, int _tx, int _ty, bool paintedChildren)
{
    // Paint text decorations like underlines/overlines. We only do this if we aren't in quirks mode (i.e., in
    // almost-strict mode or strict mode).
    if (object()->style()->htmlHacks() || !object()->shouldPaintWithinRoot(i) ||
        object()->style()->visibility() != VISIBLE)
        return;

    QPainter* p = i.p;
    _tx += m_x;
    _ty += m_y;
    RenderStyle* styleToUse = object()->style(m_firstLine);
    int deco = parent() ? styleToUse->textDecoration() : styleToUse->textDecorationsInEffect();
    if (deco != TDNONE && 
        ((!paintedChildren && ((deco & UNDERLINE) || (deco & OVERLINE))) || (paintedChildren && (deco & LINE_THROUGH))) &&
        shouldDrawDecoration(object())) {
        int x = m_x + borderLeft() + paddingLeft();
        int w = m_width - (borderLeft() + paddingLeft() + borderRight() + paddingRight());
        RootInlineBox* rootLine = root();
        if (rootLine->ellipsisBox()) {
            int ellipsisX = rootLine->ellipsisBox()->xPos();
            int ellipsisWidth = rootLine->ellipsisBox()->width();
            
            // FIXME: Will need to work with RTL
            if (rootLine == this) {
                if (x + w >= ellipsisX + ellipsisWidth)
                    w -= (x + w - ellipsisX - ellipsisWidth);
            }
            else {
                if (x >= ellipsisX)
                    return;
                if (x + w >= ellipsisX)
                    w -= (x + w - ellipsisX);
            }
        }
            
#if APPLE_CHANGES
        // Set up the appropriate text-shadow effect for the decoration.
        // FIXME: Support multiple shadow effects.  Need more from the CG API before we can do this.
        bool setShadow = false;
        if (styleToUse->textShadow()) {
            p->setShadow(styleToUse->textShadow()->x, styleToUse->textShadow()->y,
                         styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
            setShadow = true;
        }
#endif
        
        // We must have child boxes and have decorations defined.
        _tx += borderLeft() + paddingLeft();
        
        QColor underline, overline, linethrough;
        underline = overline = linethrough = styleToUse->color();
        if (!parent())
            object()->getTextDecorationColors(deco, underline, overline, linethrough);

        if (styleToUse->font() != p->font())
            p->setFont(styleToUse->font());

        if (deco & UNDERLINE && !paintedChildren) {
            p->setPen(underline);
            p->drawLineForText(_tx, _ty, m_baseline, w);
        }
        if (deco & OVERLINE && !paintedChildren) {
            p->setPen(overline);
            p->drawLineForText(_tx, _ty, 0, w);
        }
        if (deco & LINE_THROUGH && paintedChildren) {
            p->setPen(linethrough);
            p->drawLineForText(_tx, _ty, 2*m_baseline/3, w);
        }

#if APPLE_CHANGES
        if (setShadow)
            p->clearShadow();
#endif
    }
}

InlineBox* InlineFlowBox::firstLeafChild()
{
    return firstLeafChildAfterBox();
}

InlineBox* InlineFlowBox::lastLeafChild()
{
    return lastLeafChildBeforeBox();
}

InlineBox* InlineFlowBox::firstLeafChildAfterBox(InlineBox* start)
{
    InlineBox* leaf = 0;
    for (InlineBox* box = start ? start->nextOnLine() : firstChild(); box && !leaf; box = box->nextOnLine())
        leaf = box->firstLeafChild();
    if (start && !leaf && parent())
        return parent()->firstLeafChildAfterBox(this);
    return leaf;
}

InlineBox* InlineFlowBox::lastLeafChildBeforeBox(InlineBox* start)
{
    InlineBox* leaf = 0;
    for (InlineBox* box = start ? start->prevOnLine() : lastChild(); box && !leaf; box = box->prevOnLine())
        leaf = box->lastLeafChild();
    if (start && !leaf && parent())
        return parent()->lastLeafChildBeforeBox(this);
    return leaf;
}

RenderObject::SelectionState InlineFlowBox::selectionState()
{
    return RenderObject::SelectionNone;
}

bool InlineFlowBox::canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth)
{
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine()) {
        if (!box->canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth))
            return false;
    }
    return true;
}

int InlineFlowBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    int result = -1;
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine()) {
        int currResult = box->placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
        if (currResult != -1 && result == -1)
            result = currResult;
    }
    return result;
}

void InlineFlowBox::clearTruncation()
{
    for (InlineBox *box = firstChild(); box; box = box->nextOnLine())
        box->clearTruncation();
}

void EllipsisBox::paint(RenderObject::PaintInfo& i, int _tx, int _ty)
{
    QPainter* p = i.p;
    RenderStyle* _style = m_firstLine ? m_object->style(true) : m_object->style();
    if (_style->font() != p->font())
        p->setFont(_style->font());

    const Font* font = &_style->htmlFont();
    QColor textColor = _style->color();
    if (textColor != p->pen().color())
        p->setPen(textColor);
    bool setShadow = false;
    if (_style->textShadow()) {
        p->setShadow(_style->textShadow()->x, _style->textShadow()->y,
                     _style->textShadow()->blur, _style->textShadow()->color);
        setShadow = true;
    }
    
    const DOMString& str = m_str.qstring();
    font->drawText(p, m_x + _tx, 
                      m_y + _ty + m_baseline,
                      0, 0,
                      (str.impl())->s,
                      str.length(), 0, str.length(),
                      0, 
                      QPainter::LTR, _style->visuallyOrdered());
                      
    if (setShadow)
        p->clearShadow();
    
    if (m_markupBox) {
        // Paint the markup box
        _tx += m_x + m_width - m_markupBox->xPos();
        _ty += m_y + m_baseline - (m_markupBox->yPos() + m_markupBox->baseline());
        m_markupBox->paint(i, _tx, _ty);
    }
}

bool EllipsisBox::nodeAtPoint(RenderObject::NodeInfo& info, int _x, int _y, int _tx, int _ty)
{
    // Hit test the markup box.
    if (m_markupBox) {
        _tx += m_x + m_width - m_markupBox->xPos();
        _ty += m_y + m_baseline - (m_markupBox->yPos() + m_markupBox->baseline());
        if (m_markupBox->nodeAtPoint(info, _x, _y, _tx, _ty)) {
            object()->setInnerNode(info);
            return true;
        }
    }

    QRect rect(_tx + m_x, _ty + m_y, m_width, m_height);
    if (object()->style()->visibility() == VISIBLE && rect.contains(_x, _y)) {
        object()->setInnerNode(info);
        return true;
    }
    return false;
}

void RootInlineBox::detach(RenderArena* arena)
{
    detachEllipsisBox(arena);
    InlineFlowBox::detach(arena);
}

void RootInlineBox::detachEllipsisBox(RenderArena* arena)
{
    if (m_ellipsisBox) {
        m_ellipsisBox->detach(arena);
        m_ellipsisBox = 0;
    }
}

void RootInlineBox::clearTruncation()
{
    if (m_ellipsisBox) {
        detachEllipsisBox(m_object->renderArena());
        InlineFlowBox::clearTruncation();
    }
}

bool RootInlineBox::canAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth)
{
    // First sanity-check the unoverflowed width of the whole line to see if there is sufficient room.
    int delta = ltr ? lineBoxEdge - blockEdge : blockEdge - lineBoxEdge;
    if (width() - delta < ellipsisWidth)
        return false;

    // Next iterate over all the line boxes on the line.  If we find a replaced element that intersects
    // then we refuse to accommodate the ellipsis.  Otherwise we're ok.
    return InlineFlowBox::canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth);
}

void RootInlineBox::placeEllipsis(const AtomicString& ellipsisStr,  bool ltr, int blockEdge, int ellipsisWidth,
                                  InlineBox* markupBox)
{
    // Create an ellipsis box.
    m_ellipsisBox = new (m_object->renderArena()) EllipsisBox(m_object, ellipsisStr, this, 
                                                              ellipsisWidth - (markupBox ? markupBox->width() : 0),
                                                              yPos(), height(), baseline(), !prevRootBox(),
                                                              markupBox);

    if (ltr && (xPos() + width() + ellipsisWidth) <= blockEdge) {
        m_ellipsisBox->m_x = xPos() + width();
        return;
    }

    // Now attempt to find the nearest glyph horizontally and place just to the right (or left in RTL)
    // of that glyph.  Mark all of the objects that intersect the ellipsis box as not painting (as being
    // truncated).
    bool foundBox = false;
    m_ellipsisBox->m_x = placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
}

int RootInlineBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    int result = InlineFlowBox::placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
    if (result == -1)
        result = ltr ? blockEdge - ellipsisWidth : blockEdge;
    return result;
}

void RootInlineBox::paintEllipsisBox(RenderObject::PaintInfo& i, int _tx, int _ty) const
{
    if (m_ellipsisBox && object()->shouldPaintWithinRoot(i) && object()->style()->visibility() == VISIBLE &&
        i.phase == PaintActionForeground)
        m_ellipsisBox->paint(i, _tx, _ty);
}

void RootInlineBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    InlineFlowBox::paint(i, tx, ty);
    paintEllipsisBox(i, tx, ty);
}

bool RootInlineBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    if (m_ellipsisBox && object()->style()->visibility() == VISIBLE) {
        if (m_ellipsisBox->nodeAtPoint(i, x, y, tx, ty)) {
            object()->setInnerNode(i);
            return true;
        }
    }
    return InlineFlowBox::nodeAtPoint(i, x, y, tx, ty);
}

void RootInlineBox::adjustPosition(int dx, int dy)
{
    InlineFlowBox::adjustPosition(dx, dy);
    m_topOverflow += dy;
    m_bottomOverflow += dy;
    m_blockHeight += dy;
}

void RootInlineBox::childRemoved(InlineBox* box)
{
    if (box->object() == m_lineBreakObj)
        setLineBreakInfo(0,0);

    RootInlineBox* prev = prevRootBox();
    if (prev && prev->lineBreakObj() == box->object()) {
        prev->setLineBreakInfo(0,0);
        prev->markDirty();
    }
}

GapRects RootInlineBox::fillLineSelectionGap(int selTop, int selHeight, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, 
                                             const RenderObject::PaintInfo* i)
{
    GapRects result;
    RenderObject::SelectionState lineState = selectionState();

    bool leftGap, rightGap;
    block()->getHorizontalSelectionGapInfo(lineState, leftGap, rightGap);

    InlineBox* firstBox = firstSelectedBox();
    InlineBox* lastBox = lastSelectedBox();
    if (leftGap)
        result.uniteLeft(block()->fillLeftSelectionGap(firstBox->parent()->object(), 
                                                       firstBox->xPos(), selTop, selHeight, 
                                                       rootBlock, blockX, blockY, tx, ty, i));
    if (rightGap)
        result.uniteRight(block()->fillRightSelectionGap(lastBox->parent()->object(), 
                                                         lastBox->xPos() + lastBox->width(), selTop, selHeight, 
                                                         rootBlock, blockX, blockY, tx, ty, i));

    if (firstBox && firstBox != lastBox) {
        // Now fill in any gaps on the line that occurred between two selected elements.
        int lastX = firstBox->xPos() + firstBox->width();
        for (InlineBox* box = firstBox->nextLeafChild(); box; box = box->nextLeafChild()) {
            if (box->selectionState() != RenderObject::SelectionNone) {
                result.uniteCenter(block()->fillHorizontalSelectionGap(box->parent()->object(),
                                                                       lastX + tx, selTop + ty,
                                                                       box->xPos() - lastX, selHeight, i));
                lastX = box->xPos() + box->width();
            }
            if (box == lastBox)
                break;
        }
    }
      
    return result;
}

void RootInlineBox::setHasSelectedChildren(bool b)
{
    if (m_hasSelectedChildren == b)
        return;
    m_hasSelectedChildren = b;
}

RenderObject::SelectionState RootInlineBox::selectionState()
{
    // Walk over all of the selected boxes.
    RenderObject::SelectionState state = RenderObject::SelectionNone;
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        RenderObject::SelectionState boxState = box->selectionState();
        if ((boxState == RenderObject::SelectionStart && state == RenderObject::SelectionEnd) ||
            (boxState == RenderObject::SelectionEnd && state == RenderObject::SelectionStart))
            state = RenderObject::SelectionBoth;
        else if (state == RenderObject::SelectionNone ||
                 ((boxState == RenderObject::SelectionStart || boxState == RenderObject::SelectionEnd) &&
                  (state == RenderObject::SelectionNone || state == RenderObject::SelectionInside)))
            state = boxState;
        if (state == RenderObject::SelectionBoth)
            break;
    }
    
    return state;
}

InlineBox* RootInlineBox::firstSelectedBox()
{
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild())
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    return 0;
}

InlineBox* RootInlineBox::lastSelectedBox()
{
    for (InlineBox* box = lastLeafChild(); box; box = box->prevLeafChild())
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    return 0;
}

int RootInlineBox::selectionTop()
{
    if (!prevRootBox())
        return topOverflow();
    
    int prevBottom = prevRootBox()->bottomOverflow();
    if (prevBottom < m_topOverflow && block()->containsFloats()) {
        // This line has actually been moved further down, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the previous
        // line's bottom overflow if the offsets are greater on both sides.
        int prevLeft = block()->leftOffset(prevBottom);
        int prevRight = block()->rightOffset(prevBottom);
        int newLeft = block()->leftOffset(m_topOverflow);
        int newRight = block()->rightOffset(m_topOverflow);
        if (prevLeft > newLeft || prevRight < newRight)
            return m_topOverflow;
    }
    
    return prevBottom;
}
 
RenderBlock* RootInlineBox::block() const
{
    return static_cast<RenderBlock*>(m_object);
}

InlineBox* RootInlineBox::closestLeafChildForXPos(int _x, int _tx)
{
    InlineBox *firstLeaf = firstLeafChildAfterBox();
    InlineBox *lastLeaf = lastLeafChildBeforeBox();
    if (firstLeaf == lastLeaf)
        return firstLeaf;
    
    // Avoid returning a list marker when possible.
    if (_x <= _tx + firstLeaf->m_x && !firstLeaf->object()->isListMarker())
        // The x coordinate is less or equal to left edge of the firstLeaf.
        // Return it.
        return firstLeaf;
    
    if (_x >= _tx + lastLeaf->m_x + lastLeaf->m_width && !lastLeaf->object()->isListMarker())
        // The x coordinate is greater or equal to right edge of the lastLeaf.
        // Return it.
        return lastLeaf;

    for (InlineBox *leaf = firstLeaf; leaf && leaf != lastLeaf; leaf = leaf->nextLeafChild()) {
        if (!leaf->object()->isListMarker()) {
            int leafX = _tx + leaf->m_x;
            if (_x < leafX + leaf->m_width)
                // The x coordinate is less than the right edge of the box.
                // Return it.
                return leaf;
        }
    }

    return lastLeaf;
}

}

