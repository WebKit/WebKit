/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#include "config.h"
#include "RenderObject.h"

#include "AccessibilityObjectCache.h" 
#include "CachedImage.h"
#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "IntPointArray.h"
#include "KWQTextStream.h"
#include "KWQWMatrix.h"
#include "Position.h"
#include "RenderArena.h"
#include "RenderView.h"
#include "RenderFlexibleBox.h"
#include "RenderInline.h"
#include "RenderListItem.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "cssstyleselector.h"
#include "dom2_eventsimpl.h"


using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

#ifndef NDEBUG
static void *baseOfRenderObjectBeingDeleted;
#endif

void* RenderObject::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderObject::operator delete(void* ptr, size_t sz)
{
    ASSERT(baseOfRenderObjectBeingDeleted == ptr);
    
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

RenderObject *RenderObject::createObject(Node* node,  RenderStyle* style)
{
    RenderObject *o = 0;
    RenderArena* arena = node->document()->renderArena();
    switch(style->display())
    {
    case NONE:
        break;
    case INLINE:
        o = new (arena) RenderInline(node);
        break;
    case BLOCK:
        o = new (arena) RenderBlock(node);
        break;
    case INLINE_BLOCK:
        o = new (arena) RenderBlock(node);
        break;
    case LIST_ITEM:
        o = new (arena) RenderListItem(node);
        break;
    case RUN_IN:
    case COMPACT:
        o = new (arena) RenderBlock(node);
        break;
    case TABLE:
    case INLINE_TABLE:
        o = new (arena) RenderTable(node);
        break;
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        o = new (arena) RenderTableSection(node);
        break;
    case TABLE_ROW:
        o = new (arena) RenderTableRow(node);
        break;
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        o = new (arena) RenderTableCol(node);
        break;
    case TABLE_CELL:
        o = new (arena) RenderTableCell(node);
        break;
    case TABLE_CAPTION:
        o = new (arena) RenderBlock(node);
        break;
    case BOX:
    case INLINE_BOX:
        o = new (arena) RenderFlexibleBox(node);
        break;
    }
    return o;
}

#ifndef NDEBUG
struct RenderObjectCounter { 
    static int count; 
    ~RenderObjectCounter() { if (count != 0) fprintf(stderr, "LEAK: %d RenderObject\n", count); } 
};
int RenderObjectCounter::count;
static RenderObjectCounter renderObjectCounter;
#endif

RenderObject::RenderObject(Node* node)
    : CachedObjectClient(),
m_style( 0 ),
m_node( node ),
m_parent( 0 ),
m_previous( 0 ),
m_next( 0 ),
m_verticalPosition( PositionUndefined ),
m_needsLayout( false ),
m_normalChildNeedsLayout( false ),
m_posChildNeedsLayout( false ),
m_minMaxKnown( false ),
m_floating( false ),

m_positioned( false ),
m_relPositioned( false ),
m_paintBackground( false ),

m_isAnonymous( node == node->document() ),
m_recalcMinMax( false ),
m_isText( false ),
m_inline( true ),

m_replaced( false ),
m_isDragging( false ),
m_hasOverflowClip(false)
{
#ifndef NDEBUG
    ++RenderObjectCounter::count;
#endif
}

RenderObject::~RenderObject()
{
#ifndef NDEBUG
    --RenderObjectCounter::count;
#endif
}

bool RenderObject::hasAncestor(const RenderObject *obj) const
{
    for (const RenderObject *r = this; r; r = r->m_parent)
        if (r == obj)
            return true;
    return false;
}

bool RenderObject::isRoot() const
{
    return element() && element()->renderer() == this &&
           element()->document()->documentElement() == element();
}

bool RenderObject::isBody() const
{
    return element() && element()->renderer() == this && element()->hasTagName(bodyTag);
}

bool RenderObject::isHR() const
{
    return element() && element()->hasTagName(hrTag);
}

bool RenderObject::isHTMLMarquee() const
{
    return element() && element()->renderer() == this && element()->hasTagName(marqueeTag);
}

bool RenderObject::canHaveChildren() const
{
    return false;
}

RenderFlow* RenderObject::continuation() const
{
    return 0;
}

bool RenderObject::isInlineContinuation() const
{
    return false;
}

void RenderObject::addChild(RenderObject* , RenderObject *)
{
    KHTMLAssert(0);
}

RenderObject* RenderObject::removeChildNode(RenderObject* )
{
    KHTMLAssert(0);
    return 0;
}

void RenderObject::removeChild(RenderObject* )
{
    KHTMLAssert(0);
}

void RenderObject::appendChildNode(RenderObject*)
{
    KHTMLAssert(0);
}

void RenderObject::insertChildNode(RenderObject*, RenderObject*)
{
    KHTMLAssert(0);
}

RenderObject *RenderObject::nextInPreOrder() const
{
    if (RenderObject* o = firstChild())
        return o;
        
    return nextInPreOrderAfterChildren();
}

RenderObject* RenderObject::nextInPreOrderAfterChildren() const
{
    RenderObject* o;
    if (!(o = nextSibling())) {
        o = parent();
        while (o && !o->nextSibling())
            o = o->parent();
        if (o)
            o = o->nextSibling();
    }
    return o;
}

RenderObject *RenderObject::previousInPreOrder() const
{
    if (RenderObject* o = previousSibling()) {
        while (o->lastChild())
            o = o->lastChild();
        return o;
    }

    return parent();
}

RenderObject* RenderObject::childAt(unsigned index) const
{
    RenderObject* child = firstChild();
    for (unsigned i = 0; child && i < index; i++)
        child = child->nextSibling();
    return child;
}

bool RenderObject::isEditable() const
{
    RenderText *textRenderer = 0;
    if (isText()) {
        textRenderer = static_cast<RenderText *>(const_cast<RenderObject *>(this));
    }

    return style()->visibility() == VISIBLE && 
        element() && element()->isContentEditable() &&
        ((isBlockFlow() && !firstChild()) || 
        isReplaced() || 
        isBR() || 
        (textRenderer && textRenderer->firstTextBox()));
}

RenderObject *RenderObject::nextEditable() const
{
    RenderObject *r = const_cast<RenderObject *>(this);
    RenderObject *n = firstChild();
    if (n) {
        while (n) { 
            r = n; 
            n = n->firstChild(); 
        }
        if (r->isEditable())
            return r;
        else 
            return r->nextEditable();
    }
    n = r->nextSibling();
    if (n) {
        r = n;
        while (n) { 
            r = n; 
            n = n->firstChild(); 
        }
        if (r->isEditable())
            return r;
        else 
            return r->nextEditable();
    }
    n = r->parent();
    while (n) {
        r = n;
        n = r->nextSibling();
        if (n) {
            r = n;
            n = r->firstChild();
            while (n) { 
                r = n; 
                n = n->firstChild(); 
            }
            if (r->isEditable())
                return r;
            else 
                return r->nextEditable();
        }
        n = r->parent();
    }
    return 0;
}

RenderObject *RenderObject::previousEditable() const
{
    RenderObject *r = const_cast<RenderObject *>(this);
    RenderObject *n = firstChild();
    if (n) {
        while (n) { 
            r = n; 
            n = n->lastChild(); 
        }
        if (r->isEditable())
            return r;
        else 
            return r->previousEditable();
    }
    n = r->previousSibling();
    if (n) {
        r = n;
        while (n) { 
            r = n; 
            n = n->lastChild(); 
        }
        if (r->isEditable())
            return r;
        else 
            return r->previousEditable();
    }    
    n = r->parent();
    while (n) {
        r = n;
        n = r->previousSibling();
        if (n) {
            r = n;
            n = r->lastChild();
            while (n) { 
                r = n; 
                n = n->lastChild(); 
            }
            if (r->isEditable())
                return r;
            else 
                return r->previousEditable();
        }
        n = r->parent();
    }
    return 0;
} 

RenderObject *RenderObject::firstLeafChild() const
{
    RenderObject *r = firstChild();
    while (r) {
        RenderObject *n = 0;
        n = r->firstChild();
        if (!n)
            break;
        r = n;
    }
    return r;
}

RenderObject *RenderObject::lastLeafChild() const
{
    RenderObject *r = lastChild();
    while (r) {
        RenderObject *n = 0;
        n = r->lastChild();
        if (!n)
            break;
        r = n;
    }
    return r;
}

static void addLayers(RenderObject* obj, RenderLayer* parentLayer, RenderObject*& newObject,
                      RenderLayer*& beforeChild)
{
    if (obj->layer()) {
        if (!beforeChild && newObject) {
            // We need to figure out the layer that follows newObject.  We only do
            // this the first time we find a child layer, and then we update the
            // pointer values for newObject and beforeChild used by everyone else.
            beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
            newObject = 0;
        }
        parentLayer->addChild(obj->layer(), beforeChild);
        return;
    }

    for (RenderObject* curr = obj->firstChild(); curr; curr = curr->nextSibling())
        addLayers(curr, parentLayer, newObject, beforeChild);
}

void RenderObject::addLayers(RenderLayer* parentLayer, RenderObject* newObject)
{
    if (!parentLayer)
        return;
    
    RenderObject* object = newObject;
    RenderLayer* beforeChild = 0;
    WebCore::addLayers(this, parentLayer, object, beforeChild);
}

void RenderObject::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;
    
    if (layer()) {
        parentLayer->removeChild(layer());
        return;
    }

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->removeLayers(parentLayer);
}

void RenderObject::moveLayers(RenderLayer* oldParent, RenderLayer* newParent)
{
    if (!newParent)
        return;
    
    if (layer()) {
        if (oldParent)
            oldParent->removeChild(layer());
        newParent->addChild(layer());
        return;
    }

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(oldParent, newParent);
}

RenderLayer* RenderObject::findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint,
                                         bool checkParent)
{
    // Error check the parent layer passed in.  If it's null, we can't find anything.
    if (!parentLayer)
        return 0;
        
    // Step 1: Descend into our siblings trying to find the next layer.  If we do find
    // a layer, and if its parent layer matches our desired parent layer, then we have
    // a match.
    for (RenderObject* curr = startPoint ? startPoint->nextSibling() : firstChild();
         curr; curr = curr->nextSibling()) {
        RenderLayer* nextLayer = curr->findNextLayer(parentLayer, 0, false);
        if (nextLayer) {
            if (nextLayer->parent() == parentLayer)
                return nextLayer;
            return 0;
        }
    }
    
    // Step 2: If our layer is the desired parent layer, then we're finished.  We didn't
    // find anything.
    RenderLayer* ourLayer = layer();
    if (parentLayer == ourLayer)
        return 0;
    
    // Step 3: If we have a layer, then return that layer.  It will be checked against
    // the desired parent layer in the for loop above.
    if (ourLayer)
        return ourLayer;
    
    // Step 4: If |checkParent| is set, climb up to our parent and check its siblings that
    // follow us to see if we can locate a layer.
    if (checkParent && parent())
        return parent()->findNextLayer(parentLayer, this, true);
    
    return 0;
}
    
RenderLayer* RenderObject::enclosingLayer()
{
    RenderObject* curr = this;
    while (curr) {
        RenderLayer *layer = curr->layer();
        if (layer)
            return layer;
        curr = curr->parent();
    }
    return 0;
}

bool RenderObject::requiresLayer()
{
    return isRoot() || isPositioned() || isRelPositioned() || isTransparent() || hasOverflowClip();
}

RenderBlock* RenderObject::firstLineBlock() const
{
    return 0;
}

void RenderObject::updateFirstLetter()
{}

int RenderObject::offsetLeft() const
{
    int x = xPos();
    if (!isPositioned()) {
        if (isRelPositioned()) {
            int y = 0;
            ((RenderBox*)this)->relativePositionOffset(x, y);
        }
        
        RenderObject* offsetPar = offsetParent();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            x += curr->xPos();
            curr = curr->parent();
        }
    }
    return x;
}

int RenderObject::offsetTop() const
{
    int y = yPos();
    if (!isPositioned()) {
        if (isRelPositioned()) {
            int x = 0;
            ((RenderBox*)this)->relativePositionOffset(x, y);
        }
        RenderObject* offsetPar = offsetParent();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            if (!curr->isTableRow())
                y += curr->yPos();
            curr = curr->parent();
        }
    }
    return y;
}
    
RenderObject* RenderObject::offsetParent() const
{
    // FIXME: It feels like this function could almost be written using containing blocks.
    bool skipTables = isPositioned() || isRelPositioned();
    RenderObject* curr = parent();
    while (curr && (!curr->element() || 
                    (!curr->isPositioned() && !curr->isRelPositioned() && 
                        !(!style()->htmlHacks() && skipTables ? curr->isRoot() : curr->isBody())))) {
        if (!skipTables && curr->element() && (curr->isTableCell() || curr->isTable()))
            break;
        curr = curr->parent();
    }
    return curr;
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
int
RenderObject::clientWidth() const
{
    return width() - borderLeft() - borderRight() -
        (includeScrollbarSize() ? layer()->verticalScrollbarWidth() : 0);
}

int
RenderObject::clientHeight() const
{
    return height() - borderTop() - borderBottom() -
      (includeScrollbarSize() ? layer()->horizontalScrollbarHeight() : 0);
}

// scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
// object has overflow:hidden/scroll/auto specified and also has overflow.
int
RenderObject::scrollWidth() const
{
    return hasOverflowClip() ? layer()->scrollWidth() : overflowWidth();
}

int
RenderObject::scrollHeight() const
{
    return hasOverflowClip() ? layer()->scrollHeight() : overflowHeight();
}

bool RenderObject::scroll(KWQScrollDirection direction, KWQScrollGranularity granularity, float multiplier)
{
    RenderLayer *l = layer();
    if (l != 0 && l->scroll(direction, granularity, multiplier)) {
        return true;
    }
    RenderBlock *b = containingBlock();
    if (b != 0 && !b->isRenderView()) {
        return b->scroll(direction, granularity, multiplier);
    }
    return false;
}

bool
RenderObject::hasStaticX() const
{
    return (style()->left().isAuto() && style()->right().isAuto()) ||
            style()->left().isStatic() ||
            style()->right().isStatic();
}

bool
RenderObject::hasStaticY() const
{
    return (style()->top().isAuto() && style()->bottom().isAuto()) || style()->top().isStatic();
}

void RenderObject::markAllDescendantsWithFloatsForLayout(RenderObject*)
{
}

void RenderObject::setNeedsLayout(bool b, bool markParents) 
{
    bool alreadyNeededLayout = m_needsLayout;
    m_needsLayout = b;
    if (b) {
        if (!alreadyNeededLayout && markParents)
            markContainingBlocksForLayout();
    }
    else {
        m_posChildNeedsLayout = false;
        m_normalChildNeedsLayout = false;
    }
}

void RenderObject::setChildNeedsLayout(bool b, bool markParents)
{
    bool alreadyNeededLayout = m_normalChildNeedsLayout;
    m_normalChildNeedsLayout = b;
    if (b) {
        if (!alreadyNeededLayout && markParents)
            markContainingBlocksForLayout();
    }
    else {
        m_posChildNeedsLayout = false;
        m_normalChildNeedsLayout = false;
    }
}

void RenderObject::markContainingBlocksForLayout(bool scheduleRelayout)
{
    RenderObject *o = container();
    RenderObject *last = this;

    while (o) {
        if (!last->isText() && (last->style()->position() == FixedPosition || last->style()->position() == AbsolutePosition)) {
            if (o->m_posChildNeedsLayout)
                return;
            o->m_posChildNeedsLayout = true;
        } else {
            if (o->m_normalChildNeedsLayout)
                return;
            o->m_normalChildNeedsLayout = true;
        }

        last = o;
        if (scheduleRelayout && last->isTextField())
            break;
        o = o->container();
    }

    if (scheduleRelayout)
        last->scheduleRelayout();
}

RenderBlock* RenderObject::containingBlock() const
{
    if(isTableCell())
        return static_cast<const RenderTableCell *>(this)->table();
    if (isRenderView())
        return (RenderBlock*)this;

    RenderObject *o = parent();
    if (!isText() && m_style->position() == FixedPosition) {
        while ( o && !o->isRenderView() )
            o = o->parent();
    }
    else if (!isText() && m_style->position() == AbsolutePosition) {
        while (o && (o->style()->position() == StaticPosition || (o->isInline() && !o->isReplaced()))
               && !o->isRoot() && !o->isRenderView()) {
            // For relpositioned inlines, we return the nearest enclosing block.  We don't try
            // to return the inline itself.  This allows us to avoid having a positioned objects
            // list in all RenderInlines and lets us return a strongly-typed RenderBlock* result
            // from this method.  The container() method can actually be used to obtain the
            // inline directly.
            if (o->style()->position() == RelativePosition && o->isInline() && !o->isReplaced())
                return o->containingBlock();
            o = o->parent();
        }
    } else {
        while (o && ((o->isInline() && !o->isReplaced()) || o->isTableRow() || o->isTableSection()
                     || o->isTableCol() || o->isFrameSet()
#if SVG_SUPPORT
                     || o->isKCanvasContainer()
#endif
                     ))
            o = o->parent();
    }

    if (!o || !o->isRenderBlock())
        return 0; // Probably doesn't happen any more, but leave just in case. -dwh
    
    return static_cast<RenderBlock*>(o);
}

int RenderObject::containingBlockWidth() const
{
    // ###
    return containingBlock()->contentWidth();
}

int RenderObject::containingBlockHeight() const
{
    // ###
    return containingBlock()->contentHeight();
}

bool RenderObject::mustRepaintBackgroundOrBorder() const
{
    // If we don't have a background/border, then nothing to do.
    if (!shouldPaintBackgroundOrBorder())
        return false;
    
    // Ok, let's check the background first.
    const BackgroundLayer* bgLayer = style()->backgroundLayers();
    if (bgLayer->next())
        return true; // Nobody will use multiple background layers without wanting fancy positioning.
    
    // Make sure we have a valid background image.
    CachedImage* bg = bgLayer->backgroundImage();
    bool shouldPaintBackgroundImage = bg && bg->canRender();
    
    // These are always percents or auto.
    if (shouldPaintBackgroundImage && 
        (bgLayer->backgroundXPosition().value() != 0 || bgLayer->backgroundYPosition().value() != 0
        || bgLayer->backgroundSize().width.isPercent() || bgLayer->backgroundSize().height.isPercent()))
        return true; // The background image will shift unpredictably if the size changes.
        
    // Background is ok.  Let's check border.
    if (style()->hasBorder()) {
        // Border images are not ok.
        CachedImage* borderImage = style()->borderImage().image();
        bool shouldPaintBorderImage = borderImage && borderImage->canRender();
        if (shouldPaintBorderImage && borderImage->isLoaded())
            return true; // If the image hasn't loaded, we're still using the normal border style.
    }

    return false;
}

void RenderObject::drawBorderArc(GraphicsContext* p, int x, int y, float thickness, IntSize radius, int angleStart, 
    int angleSpan, BorderSide s, Color c, EBorderStyle style, bool firstCorner)
{
    if ((style == DOUBLE && ((thickness / 2) < 3)) || 
        ((style == RIDGE || style == GROOVE) && ((thickness / 2) < 2)))
        style = SOLID;
    
    switch (style) {
        case BNONE:
        case BHIDDEN:
            return;
        case DOTTED:
            p->setPen(Pen(c, thickness == 1 ? 0 : (int)thickness, Pen::DotLine));
        case DASHED:
            if(style == DASHED)
                p->setPen(Pen(c, thickness == 1 ? 0 : (int)thickness, Pen::DashLine));
            
            if (thickness > 0) {
                if (s == BSBottom || s == BSTop)
                    p->drawArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), thickness, angleStart, angleSpan);
                else //We are drawing a left or right border
                    p->drawArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), thickness, angleStart, angleSpan);
            }
            
            break;
        case DOUBLE: {
            float third = thickness / 3;
            float innerThird = (thickness + 1) / 6;
            int shiftForInner = (int)(innerThird * 2.5);
            p->setPen(Pen::NoPen);
            
            int outerY = y;
            int outerHeight = radius.height() * 2;
            int innerX = x + shiftForInner;
            int innerY = y + shiftForInner;
            int innerWidth = (radius.width() - shiftForInner) * 2;
            int innerHeight = (radius.height() - shiftForInner) * 2;
            if (innerThird > 1 && (s == BSTop || (firstCorner && (s == BSLeft || s == BSRight)))) {
                outerHeight += 2;
                innerHeight += 2;
            }
            
            p->drawArc(IntRect(x, outerY, radius.width() * 2, outerHeight), third, angleStart, angleSpan);
            p->drawArc(IntRect(innerX, innerY, innerWidth, innerHeight), (innerThird > 2) ? innerThird - 1 : innerThird,
                angleStart, angleSpan);
            break;
        }
        case GROOVE:
        case RIDGE: {
            Color c2;
            if ((style == RIDGE && (s == BSTop || s == BSLeft)) || 
                (style == GROOVE && (s == BSBottom || s == BSRight)))
                c2 = c.dark();
            else {
                c2 = c;
                c = c.dark();
            }

            p->setPen(Pen::NoPen);
            p->setFillColor(c.rgb());
            p->drawArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), thickness, angleStart, angleSpan);
            
            float halfThickness = (thickness + 1) / 4;
            int shiftForInner = (int)(halfThickness * 1.5);
            p->setFillColor(c2.rgb());
            p->drawArc(IntRect(x + shiftForInner, y + shiftForInner, (radius.width() - shiftForInner) * 2,
                (radius.height() - shiftForInner) * 2), (halfThickness > 2) ? halfThickness - 1 : halfThickness,
                angleStart, angleSpan);
            break;
        }
        case INSET:
            if(s == BSTop || s == BSLeft)
                c = c.dark();
        case OUTSET:
            if(style == OUTSET && (s == BSBottom || s == BSRight))
                c = c.dark();
        case SOLID:
            p->setPen(Pen::NoPen);
            p->setFillColor(c.rgb());
            p->drawArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), thickness, angleStart, angleSpan);
            break;
    }
}

void RenderObject::drawBorder(GraphicsContext* p, int x1, int y1, int x2, int y2,
                              BorderSide s, Color c, const Color& textcolor, EBorderStyle style,
                              int adjbw1, int adjbw2, bool invalidisInvert)
{
    int width = (s==BSTop||s==BSBottom?y2-y1:x2-x1);

    if(style == DOUBLE && width < 3)
        style = SOLID;

    if(!c.isValid()) {
        if(invalidisInvert)
        {
            // FIXME: The original KHTML did XOR here -- what do we want to do instead?
            c = Color::white;
        }
        else {
            if (style == INSET || style == OUTSET || style == RIDGE || style == GROOVE)
                c.setRgb(238, 238, 238);
            else
                c = textcolor;
        }
    }

    switch(style)
    {
    case BNONE:
    case BHIDDEN:
        return;
    case DOTTED:
        p->setPen(Pen(c, width == 1 ? 0 : width, Pen::DotLine));
        /* nobreak; */
    case DASHED:
        if(style == DASHED)
            p->setPen(Pen(c, width == 1 ? 0 : width, Pen::DashLine));

        if (width > 0)
            switch(s)
            {
            case BSBottom:
            case BSTop:
                p->drawLine(IntPoint(x1, (y1+y2)/2), IntPoint(x2, (y1+y2)/2));
                break;
            case BSRight:
            case BSLeft:
                p->drawLine(IntPoint((x1+x2)/2, y1), IntPoint((x1+x2)/2, y2));
                break;
            }
                
        break;

    case DOUBLE:
    {
        int third = (width+1)/3;

        if (adjbw1 == 0 && adjbw2 == 0)
        {
            p->setPen(Pen::NoPen);
            p->setFillColor(c.rgb());
            switch(s)
            {
            case BSTop:
            case BSBottom:
                p->drawRect(IntRect(x1, y1      , x2-x1, third));
                p->drawRect(IntRect(x1, y2-third, x2-x1, third));
                break;
            case BSLeft:
                p->drawRect(IntRect(x1      , y1+1, third, y2-y1-1));
                p->drawRect(IntRect(x2-third, y1+1, third, y2-y1-1));
                break;
            case BSRight:
                p->drawRect(IntRect(x1      , y1+1, third, y2-y1-1));
                p->drawRect(IntRect(x2-third, y1+1, third, y2-y1-1));
                break;
            }
        }
        else
        {
            int adjbw1bigthird;
            if (adjbw1>0)
                adjbw1bigthird = adjbw1+1;
            else
                adjbw1bigthird = adjbw1 - 1;
            adjbw1bigthird /= 3;

            int adjbw2bigthird;
            if (adjbw2>0)
                adjbw2bigthird = adjbw2 + 1;
            else
                adjbw2bigthird = adjbw2 - 1;
            adjbw2bigthird /= 3;

          switch(s)
            {
            case BSTop:
              drawBorder(p, x1+max((-adjbw1*2+1)/3,0), y1        , x2-max((-adjbw2*2+1)/3,0), y1 + third, s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              drawBorder(p, x1+max(( adjbw1*2+1)/3,0), y2 - third, x2-max(( adjbw2*2+1)/3,0), y2        , s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              break;
            case BSLeft:
              drawBorder(p, x1        , y1+max((-adjbw1*2+1)/3,0), x1+third, y2-max((-adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              drawBorder(p, x2 - third, y1+max(( adjbw1*2+1)/3,0), x2      , y2-max(( adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              break;
            case BSBottom:
              drawBorder(p, x1+max(( adjbw1*2+1)/3,0), y1      , x2-max(( adjbw2*2+1)/3,0), y1+third, s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              drawBorder(p, x1+max((-adjbw1*2+1)/3,0), y2-third, x2-max((-adjbw2*2+1)/3,0), y2      , s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              break;
            case BSRight:
            drawBorder(p, x1      , y1+max(( adjbw1*2+1)/3,0), x1+third, y2-max(( adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
            drawBorder(p, x2-third, y1+max((-adjbw1*2+1)/3,0), x2      , y2-max((-adjbw2*2+1)/3,0), s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
              break;
            default:
              break;
            }
        }
        break;
    }
    case RIDGE:
    case GROOVE:
    {
        EBorderStyle s1;
        EBorderStyle s2;
        if (style==GROOVE)
        {
            s1 = INSET;
            s2 = OUTSET;
        }
        else
        {
            s1 = OUTSET;
            s2 = INSET;
        }

        int adjbw1bighalf;
        int adjbw2bighalf;
        if (adjbw1>0) adjbw1bighalf=adjbw1+1;
        else adjbw1bighalf=adjbw1-1;
        adjbw1bighalf/=2;

        if (adjbw2>0) adjbw2bighalf=adjbw2+1;
        else adjbw2bighalf=adjbw2-1;
        adjbw2bighalf/=2;

        switch (s)
        {
        case BSTop:
            drawBorder(p, x1+max(-adjbw1  ,0)/2,  y1        , x2-max(-adjbw2,0)/2, (y1+y2+1)/2, s, c, textcolor, s1, adjbw1bighalf, adjbw2bighalf);
            drawBorder(p, x1+max( adjbw1+1,0)/2, (y1+y2+1)/2, x2-max( adjbw2+1,0)/2,  y2        , s, c, textcolor, s2, adjbw1/2, adjbw2/2);
            break;
        case BSLeft:
            drawBorder(p,  x1        , y1+max(-adjbw1  ,0)/2, (x1+x2+1)/2, y2-max(-adjbw2,0)/2, s, c, textcolor, s1, adjbw1bighalf, adjbw2bighalf);
            drawBorder(p, (x1+x2+1)/2, y1+max( adjbw1+1,0)/2,  x2        , y2-max( adjbw2+1,0)/2, s, c, textcolor, s2, adjbw1/2, adjbw2/2);
            break;
        case BSBottom:
            drawBorder(p, x1+max( adjbw1  ,0)/2,  y1        , x2-max( adjbw2,0)/2, (y1+y2+1)/2, s, c, textcolor, s2,  adjbw1bighalf, adjbw2bighalf);
            drawBorder(p, x1+max(-adjbw1+1,0)/2, (y1+y2+1)/2, x2-max(-adjbw2+1,0)/2,  y2        , s, c, textcolor, s1, adjbw1/2, adjbw2/2);
            break;
        case BSRight:
            drawBorder(p,  x1        , y1+max( adjbw1  ,0)/2, (x1+x2+1)/2, y2-max( adjbw2,0)/2, s, c, textcolor, s2, adjbw1bighalf, adjbw2bighalf);
            drawBorder(p, (x1+x2+1)/2, y1+max(-adjbw1+1,0)/2,  x2        , y2-max(-adjbw2+1,0)/2, s, c, textcolor, s1, adjbw1/2, adjbw2/2);
            break;
        }
        break;
    }
    case INSET:
        if(s == BSTop || s == BSLeft)
            c = c.dark();

        /* nobreak; */
    case OUTSET:
        if(style == OUTSET && (s == BSBottom || s == BSRight))
            c = c.dark();
        /* nobreak; */
    case SOLID:
        IntPointArray quad(4);
        p->setPen(Pen::NoPen);
        p->setFillColor(c.rgb());
        ASSERT(x2 >= x1);
        ASSERT(y2 >= y1);
        if (adjbw1==0 && adjbw2 == 0) {
            p->drawRect(IntRect(x1,y1,x2-x1,y2-y1));
            return;
        }
        switch(s) {
        case BSTop:
            quad.setPoints(4,
                           x1+max(-adjbw1,0), y1,
                           x1+max( adjbw1,0), y2,
                           x2-max( adjbw2,0), y2,
                           x2-max(-adjbw2,0), y1);
            break;
        case BSBottom:
            quad.setPoints(4,
                           x1+max( adjbw1,0), y1,
                           x1+max(-adjbw1,0), y2,
                           x2-max(-adjbw2,0), y2,
                           x2-max( adjbw2,0), y1);
            break;
        case BSLeft:
          quad.setPoints(4,
                         x1, y1+max(-adjbw1,0),
                         x1, y2-max(-adjbw2,0),
                         x2, y2-max( adjbw2,0),
                         x2, y1+max( adjbw1,0));
            break;
        case BSRight:
          quad.setPoints(4,
                         x1, y1+max( adjbw1,0),
                         x1, y2-max( adjbw2,0),
                         x2, y2-max(-adjbw2,0),
                         x2, y1+max(-adjbw1,0));
            break;
        }
        p->drawConvexPolygon(quad);
        break;
    }
}

bool RenderObject::paintBorderImage(GraphicsContext* p, int _tx, int _ty, int w, int h, const RenderStyle* style)
{
    CachedImage* borderImage = style->borderImage().image();
    if (!borderImage->isLoaded())
        return true; // Never paint a border image incrementally, but don't paint the fallback borders either.
    
    // If we have a border radius, the border image gets clipped to the rounded rect.
    bool clipped = false;
    if (style->hasBorderRadius()) {
        IntRect clipRect(_tx, _ty, w, h);
        p->save();
        p->addRoundedRectClip(clipRect,
            style->borderTopLeftRadius(), style->borderTopRightRadius(),
            style->borderBottomLeftRadius(), style->borderBottomRightRadius());
        clipped = true;
    }

    int imageWidth = borderImage->image()->width();
    int imageHeight = borderImage->image()->height();

    int topSlice = min(imageHeight, style->borderImage().m_slices.top.calcValue(borderImage->image()->height()));
    int bottomSlice = min(imageHeight, style->borderImage().m_slices.bottom.calcValue(borderImage->image()->height()));
    int leftSlice = min(imageWidth, style->borderImage().m_slices.left.calcValue(borderImage->image()->width()));    
    int rightSlice = min(imageWidth, style->borderImage().m_slices.right.calcValue(borderImage->image()->width()));

    EBorderImageRule hRule = style->borderImage().horizontalRule();
    EBorderImageRule vRule = style->borderImage().verticalRule();
    
    bool drawLeft = leftSlice > 0 && style->borderLeftWidth() > 0;
    bool drawTop = topSlice > 0 && style->borderTopWidth() > 0;
    bool drawRight = rightSlice > 0 && style->borderRightWidth() > 0;
    bool drawBottom = bottomSlice > 0 && style->borderBottomWidth() > 0;
    bool drawMiddle = (imageWidth - leftSlice - rightSlice) > 0 && (w - style->borderLeftWidth() - style->borderRightWidth()) > 0 &&
                      (imageHeight - topSlice - bottomSlice) > 0 && (h - style->borderTopWidth() - style->borderBottomWidth()) > 0;

    if (drawLeft) {
        // Paint the top and bottom left corners.
        
        // The top left corner rect is (_tx, _ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            p->drawImage(borderImage->image(), IntRect(_tx, _ty, style->borderLeftWidth(), style->borderTopWidth()),
                         IntRect(0, 0, leftSlice, topSlice));
        
        // The bottom left corner rect is (_tx, _ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            p->drawImage(borderImage->image(), IntRect(_tx, _ty + h - style->borderBottomWidth(), style->borderLeftWidth(), style->borderBottomWidth()),
                         IntRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice));
                      
        // Paint the left edge.
        // Have to scale and tile into the border rect.
        p->drawTiledImage(borderImage->image(), IntRect(_tx, _ty + style->borderTopWidth(), style->borderLeftWidth(),
                                    h - style->borderTopWidth() - style->borderBottomWidth()),
                                    IntRect(0, topSlice, leftSlice, imageHeight - topSlice - bottomSlice),
                                    Image::StretchTile, (Image::TileRule)vRule);
    }
    
    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (_tx + w - rightWidth, _ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            p->drawImage(borderImage->image(), IntRect(_tx + w - style->borderRightWidth(), _ty, style->borderRightWidth(), style->borderTopWidth()),
                         IntRect(imageWidth - rightSlice, 0, rightSlice, topSlice));
        
        // The bottom right corner rect is (_tx + w - rightWidth, _ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, botomSlice)
        if (drawBottom)
            p->drawImage(borderImage->image(), IntRect(_tx + w - style->borderRightWidth(), _ty + h - style->borderBottomWidth(), style->borderRightWidth(), style->borderBottomWidth()),
                         IntRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice));
                      
        // Paint the right edge.
        p->drawTiledImage(borderImage->image(), IntRect(_tx + w - style->borderRightWidth(), _ty + style->borderTopWidth(), style->borderRightWidth(),
                          h - style->borderTopWidth() - style->borderBottomWidth()),
                          IntRect(imageWidth - rightSlice, topSlice, rightSlice, imageHeight - topSlice - bottomSlice),
                          Image::StretchTile, (Image::TileRule)vRule);
    }

    // Paint the top edge.
    if (drawTop)
        p->drawTiledImage(borderImage->image(), IntRect(_tx + style->borderLeftWidth(), _ty, w - style->borderLeftWidth() - style->borderRightWidth(), style->borderTopWidth()),
                          IntRect(leftSlice, 0, imageWidth - rightSlice - leftSlice, topSlice),
                          (Image::TileRule)hRule, Image::StretchTile);
    
    // Paint the bottom edge.
    if (drawBottom)
        p->drawTiledImage(borderImage->image(), IntRect(_tx + style->borderLeftWidth(), _ty + h - style->borderBottomWidth(), 
                          w - style->borderLeftWidth() - style->borderRightWidth(), style->borderBottomWidth()),
                          IntRect(leftSlice, imageHeight - bottomSlice, imageWidth - rightSlice - leftSlice, bottomSlice),
                          (Image::TileRule)hRule, Image::StretchTile);
    
    // Paint the middle.
    if (drawMiddle)
        p->drawTiledImage(borderImage->image(), IntRect(_tx + style->borderLeftWidth(), _ty + style->borderTopWidth(), w - style->borderLeftWidth() - style->borderRightWidth(),
                          h - style->borderTopWidth() - style->borderBottomWidth()),
                          IntRect(leftSlice, topSlice, imageWidth - rightSlice - leftSlice, imageHeight - topSlice - bottomSlice),
                          (Image::TileRule)hRule, (Image::TileRule)vRule);

    // Clear the clip for the border radius.
    if (clipped)
        p->restore();

    return true;
}

void RenderObject::paintBorder(GraphicsContext* p, int _tx, int _ty, int w, int h, const RenderStyle* style, bool begin, bool end)
{
    CachedImage* borderImage = style->borderImage().image();
    bool shouldPaintBackgroundImage = borderImage && borderImage->canRender();
    if (shouldPaintBackgroundImage)
        shouldPaintBackgroundImage = paintBorderImage(p, _tx, _ty, w, h, style);
    
    if (shouldPaintBackgroundImage)
        return;

    const Color& tc = style->borderTopColor();
    const Color& bc = style->borderBottomColor();
    const Color& lc = style->borderLeftColor();
    const Color& rc = style->borderRightColor();

    bool tt = style->borderTopIsTransparent();
    bool bt = style->borderBottomIsTransparent();
    bool rt = style->borderRightIsTransparent();
    bool lt = style->borderLeftIsTransparent();
    
    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool renderTop = ts > BHIDDEN && !tt;
    bool renderLeft = ls > BHIDDEN && begin && !lt;
    bool renderRight = rs > BHIDDEN && end && !rt;
    bool renderBottom = bs > BHIDDEN && !bt;

    // Need sufficient width and height to contain border radius curves.  Sanity check our top/bottom
    // values and our width/height values to make sure the curves can all fit. If not, then we won't paint
    // any border radii.
    bool renderRadii = false;
    IntSize topLeft = style->borderTopLeftRadius();
    IntSize topRight = style->borderTopRightRadius();
    IntSize bottomLeft = style->borderBottomLeftRadius();
    IntSize bottomRight = style->borderBottomRightRadius();

    if (style->hasBorderRadius()) {
        int requiredWidth = max(topLeft.width() + topRight.width(), bottomLeft.width() + bottomRight.width());
        int requiredHeight = max(topLeft.height() + bottomLeft.height(), topRight.height() + bottomRight.height());
        renderRadii = (requiredWidth <= w && requiredHeight <= h);
    }
    
    // Clip to the rounded rectangle.
    if (renderRadii) {
        p->save();
        p->addRoundedRectClip(IntRect(_tx, _ty, w, h), topLeft, topRight, bottomLeft, bottomRight);
    }

    int firstAngleStart, secondAngleStart, firstAngleSpan, secondAngleSpan;
    float thickness;
    bool upperLeftBorderStylesMatch = renderLeft && (ts == ls) && (tc == lc) && (ts != INSET) && (ts != GROOVE);
    bool upperRightBorderStylesMatch = renderRight && (ts == rs) && (tc == rc) && (ts != OUTSET) && (ts != RIDGE);
    bool lowerLeftBorderStylesMatch = renderLeft && (bs == ls) && (bc == lc) && (bs != OUTSET) && (bs != RIDGE);
    bool lowerRightBorderStylesMatch = renderRight && (bs == rs) && (bc == rc) && (bs != INSET) && (bs != GROOVE);

    if (renderTop) {
        bool ignore_left = (renderRadii && topLeft.width() > 0) ||
            ((tc == lc) && (tt == lt) &&
            (ts >= OUTSET) &&
            (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET));

        bool ignore_right = (renderRadii && topRight.width() > 0) ||
            ((tc == rc) && (tt == rt) &&
            (ts >= OUTSET) &&
            (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET));
        
        int x = _tx;
        int x2 = _tx + w;
        if (renderRadii) {
            x += topLeft.width();
            x2 -= topRight.width();
        }
        
        drawBorder(p, x, _ty, x2, _ty +  style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   ignore_left ? 0 : style->borderLeftWidth(),
                   ignore_right? 0 : style->borderRightWidth());
        
        if (renderRadii) {
            int leftX = _tx;
            int leftY = _ty;
            int rightX = _tx + w - topRight.width() * 2;
            firstAngleStart = 90;
            firstAngleSpan = upperLeftBorderStylesMatch ? 90 : 45;
            
            // We make the arc double thick and let the clip rect take care of clipping the extra off.
            // We're doing this because it doesn't seem possible to match the curve of the clip exactly
            // with the arc-drawing function.
            thickness = style->borderTopWidth() * 2;
            
            if (upperRightBorderStylesMatch) {
                secondAngleStart = 0;
                secondAngleSpan = 90;
            } else {
                secondAngleStart = 45;
                secondAngleSpan = 45;
            }
            
            // The inner clip clips inside the arc. This is especially important for 1px borders.
            bool applyLeftInnerClip = (style->borderLeftWidth() < topLeft.width())
                && (style->borderTopWidth() < topLeft.height())
                && (ts != DOUBLE || style->borderTopWidth() > 6);
            if (applyLeftInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(leftX, leftY, topLeft.width() * 2, topLeft.height() * 2), 
                    style->borderTopWidth());
            }
            
            // Draw upper left arc
            drawBorderArc(p, leftX, leftY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                BSTop, tc, ts, true);
            if (applyLeftInnerClip)
                p->restore();
            
            bool applyRightInnerClip = (style->borderRightWidth() < topRight.width())
                && (style->borderTopWidth() < topRight.height())
                && (ts != DOUBLE || style->borderTopWidth() > 6);
            if (applyRightInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(rightX, leftY, topRight.width() * 2, topRight.height() * 2),
                    style->borderTopWidth());
            }
            
            // Draw upper right arc
            drawBorderArc(p, rightX, leftY, thickness, topRight, secondAngleStart, secondAngleSpan,
                BSTop, tc, ts, false);
            if (applyRightInnerClip)
                p->restore();
        }
    }

    if (renderBottom) {
        bool ignore_left = (renderRadii && bottomLeft.width() > 0) ||
        ((bc == lc) && (bt == lt) &&
        (bs >= OUTSET) &&
        (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET));

        bool ignore_right = (renderRadii && bottomRight.width() > 0) ||
            ((bc == rc) && (bt == rt) &&
            (bs >= OUTSET) &&
            (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET));
        
        int x = _tx;
        int x2 = _tx + w;
        if (renderRadii) {
            x += bottomLeft.width();
            x2 -= bottomRight.width();
        }

        drawBorder(p, x, _ty + h - style->borderBottomWidth(), x2, _ty + h, BSBottom, bc, style->color(), bs,
                   ignore_left ? 0 :style->borderLeftWidth(),
                   ignore_right? 0 :style->borderRightWidth());
        
        if (renderRadii) {
            int leftX = _tx;
            int leftY = _ty + h  - bottomLeft.height() * 2;
            int rightX = _tx + w - bottomRight.width() * 2;
            secondAngleStart = 270;
            secondAngleSpan = upperRightBorderStylesMatch ? 90 : 45;
            thickness = style->borderBottomWidth() * 2;
            
            if (upperLeftBorderStylesMatch) {
                firstAngleStart = 180;
                firstAngleSpan = 90;
            } else {
                firstAngleStart = 225;
                firstAngleSpan =  45;
            }
            
            bool applyLeftInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                && (style->borderBottomWidth() < bottomLeft.height())
                && (bs != DOUBLE || style->borderBottomWidth() > 6);
            if (applyLeftInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(leftX, leftY, bottomLeft.width() * 2, bottomLeft.height() * 2), 
                    style->borderBottomWidth());
            }
            
            // Draw lower left arc
            drawBorderArc(p, leftX, leftY, thickness, bottomLeft, firstAngleStart, firstAngleSpan,
                BSBottom, bc, bs, true);
            if (applyLeftInnerClip)
                p->restore();
                
            bool applyRightInnerClip = (style->borderRightWidth() < bottomRight.width())
                && (style->borderBottomWidth() < bottomRight.height())
                && (bs != DOUBLE || style->borderBottomWidth() > 6);
            if (applyRightInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(rightX, leftY, bottomRight.width() * 2, bottomRight.height() * 2),
                    style->borderBottomWidth());
            }
            
            // Draw lower right arc
            drawBorderArc(p, rightX, leftY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                BSBottom, bc, bs, false);
            if (applyRightInnerClip)
                p->restore();
        }
    }
    
    if (renderLeft) {
        bool ignore_top = (renderRadii && topLeft.height() > 0) ||
          ((tc == lc) && (tt == lt) &&
          (ls >= OUTSET) &&
          (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET));

        bool ignore_bottom = (renderRadii && bottomLeft.height() > 0) ||
          ((bc == lc) && (bt == lt) &&
          (ls >= OUTSET) &&
          (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET));

        int y = _ty;
        int y2 = _ty + h;
        if (renderRadii) {
            y += topLeft.height();
            y2 -= bottomLeft.height();
        }
        
        drawBorder(p, _tx, y, _tx + style->borderLeftWidth(), y2, BSLeft, lc, style->color(), ls,
                   ignore_top?0:style->borderTopWidth(),
                   ignore_bottom?0:style->borderBottomWidth());

        if (renderRadii && (!upperLeftBorderStylesMatch || !lowerLeftBorderStylesMatch)) {
            int topX = _tx;
            int topY = _ty;
            int bottomY = _ty + h  - bottomLeft.height() * 2;
            firstAngleStart = 135;
            secondAngleStart = 180;
            firstAngleSpan = secondAngleSpan = 45;
            thickness = style->borderLeftWidth() * 2;
            
            bool applyTopInnerClip = (style->borderLeftWidth() < topLeft.width())
                && (style->borderTopWidth() < topLeft.height())
                && (ls != DOUBLE || style->borderLeftWidth() > 6);
            if (applyTopInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(topX, topY, topLeft.width() * 2, topLeft.height() * 2), 
                    style->borderLeftWidth());
            }
            
            // Draw top left arc
            drawBorderArc(p, topX, topY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                BSLeft, lc, ls, true);
            if (applyTopInnerClip)
                p->restore();
            
            bool applyBottomInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                && (style->borderBottomWidth() < bottomLeft.height())
                && (ls != DOUBLE || style->borderLeftWidth() > 6);
            if (applyBottomInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(topX, bottomY, bottomLeft.width() * 2, bottomLeft.height() * 2), 
                    style->borderLeftWidth());
            }
            
            // Draw bottom left arc
            drawBorderArc(p, topX, bottomY, thickness, bottomLeft, secondAngleStart, secondAngleSpan,
                BSLeft, lc, ls, false);
            if (applyBottomInnerClip)
                p->restore();
        }
    }

    if (renderRight) {
        bool ignore_top = (renderRadii && topRight.height() > 0) ||
          ((tc == rc) && (tt == rt) &&
          (rs >= DOTTED || rs == INSET) &&
          (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET));

        bool ignore_bottom = (renderRadii && bottomRight.height() > 0) ||
          ((bc == rc) && (bt == rt) &&
          (rs >= DOTTED || rs == INSET) &&
          (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET));

        int y = _ty;
        int y2 = _ty + h;
        if (renderRadii) {
            y += topRight.height();
            y2 -= bottomRight.height();
        }

        drawBorder(p, _tx + w - style->borderRightWidth(), y, _tx + w, y2, BSRight, rc, style->color(), rs,
                   ignore_top?0:style->borderTopWidth(),
                   ignore_bottom?0:style->borderBottomWidth());

        if (renderRadii && (!upperRightBorderStylesMatch || !lowerRightBorderStylesMatch)) {
            int topX = _tx + w - topRight.width() * 2;
            int topY = _ty;
            int bottomY = _ty + h  - bottomRight.height() * 2;
            firstAngleStart = 0; 
            secondAngleStart = 315;
            firstAngleSpan = secondAngleSpan = 45;
            thickness = style->borderRightWidth() * 2;
            
            bool applyTopInnerClip = (style->borderRightWidth() < topRight.width())
                && (style->borderTopWidth() < topRight.height())
                && (rs != DOUBLE || style->borderRightWidth() > 6);
            if (applyTopInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(topX, topY, topRight.width() * 2, topRight.height() * 2),
                    style->borderRightWidth());
            }
            
            // Draw top right arc
            drawBorderArc(p, topX, topY, thickness, topRight, firstAngleStart, firstAngleSpan,
                BSRight, rc, rs, true);
            if (applyTopInnerClip)
                p->restore();
            
            bool applyBottomInnerClip = (style->borderRightWidth() < bottomRight.width())
                && (style->borderBottomWidth() < bottomRight.height())
                && (rs != DOUBLE || style->borderRightWidth() > 6);
            if (applyBottomInnerClip) {
                p->save();
                p->addInnerRoundedRectClip(IntRect(topX, bottomY, bottomRight.width() * 2, bottomRight.height() * 2),
                    style->borderRightWidth());
            }
            
            // Draw bottom right arc
            drawBorderArc(p, topX, bottomY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                BSRight, rc, rs, false);
            if (applyBottomInnerClip)
                p->restore();
        }
    }
    
    if (renderRadii)
        p->restore(); // Undo the clip.
}

DeprecatedValueList<IntRect> RenderObject::lineBoxRects()
{
    return DeprecatedValueList<IntRect>();
}

void RenderObject::absoluteRects(DeprecatedValueList<IntRect>& rects, int _tx, int _ty)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (continuation()) {
        rects.append(IntRect(_tx, _ty - collapsedMarginTop(), 
                           width(), height()+collapsedMarginTop()+collapsedMarginBottom()));
        continuation()->absoluteRects(rects, 
                                      _tx - xPos() + continuation()->containingBlock()->xPos(),
                                      _ty - yPos() + continuation()->containingBlock()->yPos());
    }
    else
        rects.append(IntRect(_tx, _ty, width(), height() + borderTopExtra() + borderBottomExtra()));
}

IntRect RenderObject::absoluteBoundingBoxRect()
{
    int x = 0, y = 0;
    absolutePosition(x, y);
    DeprecatedValueList<IntRect> rects;
    absoluteRects(rects, x, y);

    if (rects.isEmpty())
        return IntRect();

    DeprecatedValueList<IntRect>::ConstIterator it = rects.begin();
    IntRect result = *it;
    while (++it != rects.end())
        result.unite(*it);
    return result;
}

void RenderObject::addAbsoluteRectForLayer(IntRect& result)
{
    if (layer())
        result.unite(absoluteBoundingBoxRect());
    for (RenderObject* current = firstChild(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
}

IntRect RenderObject::paintingRootRect(IntRect& topLevelRect)
{
    IntRect result = absoluteBoundingBoxRect();
    topLevelRect = result;
    for (RenderObject* current = firstChild(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
    return result;
}

void RenderObject::addFocusRingRects(GraphicsContext* p, int _tx, int _ty)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (continuation()) {
        p->addFocusRingRect(IntRect(_tx, _ty - collapsedMarginTop(), width(), height()+collapsedMarginTop()+collapsedMarginBottom()));
        continuation()->addFocusRingRects(p, 
                                          _tx - xPos() + continuation()->containingBlock()->xPos(),
                                          _ty - yPos() + continuation()->containingBlock()->yPos());
    }
    else
        p->addFocusRingRect(IntRect(_tx, _ty, width(), height()));
}

void RenderObject::paintOutline(GraphicsContext* p, int _tx, int _ty, int w, int h, const RenderStyle* style)
{
    int ow = style->outlineWidth();
    if(!ow) return;

    EBorderStyle os = style->outlineStyle();
    if (os <= BHIDDEN)
        return;
    
    Color oc = style->outlineColor();
    if (!oc.isValid())
        oc = style->color();
    
    int offset = style->outlineOffset();
    
    if (style->outlineStyleIsAuto()) {
        if (!theme()->supportsFocusRing(style)) {
            // Only paint the focus ring by hand if the theme isn't able to draw the focus ring.
            p->initFocusRing(ow, offset);
            addFocusRingRects(p, _tx, _ty);
            p->drawFocusRing(oc);
            p->clearFocusRing();
        }
        return;
    }

    _tx -= offset;
    _ty -= offset;
    w += 2*offset;
    h += 2*offset;
    
    drawBorder(p, _tx-ow, _ty-ow, _tx, _ty+h+ow, BSLeft,
               Color(oc), style->color(),
               os, ow, ow, true);

    drawBorder(p, _tx-ow, _ty-ow, _tx+w+ow, _ty, BSTop,
               Color(oc), style->color(),
               os, ow, ow, true);

    drawBorder(p, _tx+w, _ty-ow, _tx+w+ow, _ty+h+ow, BSRight,
               Color(oc), style->color(),
               os, ow, ow, true);

    drawBorder(p, _tx-ow, _ty+h, _tx+w+ow, _ty+h+ow, BSBottom,
               Color(oc), style->color(),
               os, ow, ow, true);

}

void RenderObject::paint(PaintInfo& i, int tx, int ty)
{
}

void RenderObject::repaint(bool immediate)
{
    // Can't use view(), since we might be unrooted.
    RenderObject* o = this;
    while ( o->parent() ) o = o->parent();
    if (!o->isRenderView())
        return;
    RenderView* c = static_cast<RenderView*>(o);
    if (c->printingMode())
        return; // Don't repaint if we're printing.
    c->repaintViewRectangle(getAbsoluteRepaintRect(), immediate);    
}

void RenderObject::repaintRectangle(const IntRect& r, bool immediate)
{
    // Can't use view(), since we might be unrooted.
    RenderObject* o = this;
    while ( o->parent() ) o = o->parent();
    if (!o->isRenderView())
        return;
    RenderView* c = static_cast<RenderView*>(o);
    if (c->printingMode())
        return; // Don't repaint if we're printing.
    IntRect absRect(r);
    computeAbsoluteRepaintRect(absRect);
    c->repaintViewRectangle(absRect, immediate);
}

bool RenderObject::repaintAfterLayoutIfNeeded(const IntRect& oldBounds, const IntRect& oldFullBounds)
{
    RenderView* c = view();
    if (c->printingMode())
        return false; // Don't repaint if we're printing.
            
    IntRect newBounds, newFullBounds;
    getAbsoluteRepaintRectIncludingFloats(newBounds, newFullBounds);
    if (newBounds == oldBounds && !selfNeedsLayout())
        return false;

    bool fullRepaint = selfNeedsLayout() || newBounds.location() != oldBounds.location() || mustRepaintBackgroundOrBorder();
    if (fullRepaint) {
        c->repaintViewRectangle(oldFullBounds);
        if (newBounds != oldBounds)
            c->repaintViewRectangle(newFullBounds);
        return true;
    }

    // We didn't move, but we did change size.  Invalidate the delta, which will consist of possibly 
    // two rectangles (but typically only one).
    int ow = style() ? style()->outlineSize() : 0;
    int width = abs(newBounds.width() - oldBounds.width());
    if (width)
        c->repaintViewRectangle(IntRect(min(newBounds.x() + newBounds.width(), oldBounds.x() + oldBounds.width()) - borderRight() - ow,
            newBounds.y(),
            width + borderRight() + ow,
            max(newBounds.height(), oldBounds.height())));
    int height = abs(newBounds.height() - oldBounds.height());
    if (height)
        c->repaintViewRectangle(IntRect(newBounds.x(),
            min(newBounds.bottom(), oldBounds.bottom()) - borderBottom() - ow,
            max(newBounds.width(), oldBounds.width()),
            height + borderBottom() + ow));
    return false;
}

void RenderObject::repaintDuringLayoutIfMoved(int x, int y)
{
}

void RenderObject::repaintFloatingDescendants()
{
}

bool RenderObject::checkForRepaintDuringLayout() const
{
    return !document()->view()->needsFullRepaint() && !layer();
}

void RenderObject::repaintObjectsBeforeLayout()
{
    if (!needsLayout() || isText())
        return;
    
    bool blockWithInlineChildren = (isRenderBlock() && !isTable() && normalChildNeedsLayout() && childrenInline());
    if (selfNeedsLayout()) {
        repaint();
        if (blockWithInlineChildren)
            return;
    }

    for (RenderObject* current = firstChild(); current; current = current->nextSibling()) {
        if (!current->isPositioned()) // RenderBlock subclass method handles walking the positioned objects.
            current->repaintObjectsBeforeLayout();
    }
}

IntRect RenderObject::getAbsoluteRepaintRectWithOutline(int ow)
{
    IntRect r(getAbsoluteRepaintRect());
    r.inflate(ow);

    if (continuation() && !isInline())
        r.inflateY(collapsedMarginTop());
    
    if (isInlineFlow())
        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
            if (!curr->isText())
                r.unite(curr->getAbsoluteRepaintRectWithOutline(ow));

    return r;
}

IntRect RenderObject::getAbsoluteRepaintRect()
{
    if (parent())
        return parent()->getAbsoluteRepaintRect();
    return IntRect();
}

void RenderObject::getAbsoluteRepaintRectIncludingFloats(IntRect& bounds, IntRect& fullBounds)
{
    bounds = fullBounds = getAbsoluteRepaintRect();
}

void RenderObject::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    if (parent())
        return parent()->computeAbsoluteRepaintRect(r, f);
}

void RenderObject::dirtyLinesFromChangedChild(RenderObject* child)
{
}

#ifndef NDEBUG

DeprecatedString RenderObject::information() const
{
    DeprecatedString str;
    QTextStream ts(&str);
    ts << renderName()
        << "(" << (style() ? style()->refCount() : 0) << ")"
       << ": " << (void*)this << "  ";
    if (isInline()) ts << "il ";
    if (childrenInline()) ts << "ci ";
    if (isFloating()) ts << "fl ";
    if (isAnonymous()) ts << "an ";
    if (isRelPositioned()) ts << "rp ";
    if (isPositioned()) ts << "ps ";
    if (needsLayout()) ts << "nl ";
    if (m_recalcMinMax) ts << "rmm ";
    if (style() && style()->zIndex()) ts << "zI: " << style()->zIndex();
    if (element()) {
        if (element()->active())
            ts << "act ";
        if (element()->isLink())
            ts << "anchor ";
        if (element()->focused())
            ts << "focus ";
        ts << " <" <<  element()->localName().deprecatedString() << ">";
        ts << " (" << xPos() << "," << yPos() << "," << width() << "," << height() << ")";
        if (isTableCell()) {
            const RenderTableCell* cell = static_cast<const RenderTableCell *>(this);
            ts << " [r=" << cell->row() << " c=" + cell->col() << " rs=" + cell->rowSpan() << " cs=" + cell->colSpan() << "]";
        }
    }
    return str;
}

void RenderObject::dump(QTextStream *stream, DeprecatedString ind) const
{
    if (isAnonymous()) { *stream << " anonymous"; }
    if (isFloating()) { *stream << " floating"; }
    if (isPositioned()) { *stream << " positioned"; }
    if (isRelPositioned()) { *stream << " relPositioned"; }
    if (isText()) { *stream << " text"; }
    if (isInline()) { *stream << " inline"; }
    if (isReplaced()) { *stream << " replaced"; }
    if (shouldPaintBackgroundOrBorder()) { *stream << " paintBackground"; }
    if (needsLayout()) { *stream << " needsLayout"; }
    if (minMaxKnown()) { *stream << " minMaxKnown"; }
    *stream << endl;

    RenderObject *child = firstChild();
    while( child != 0 )
    {
        *stream << ind << child->renderName() << ": ";
        child->dump(stream,ind+"  ");
        child = child->nextSibling();
    }
}

void RenderObject::showTreeForThis() const
{
    if (element())
        element()->showTreeForThis();
}

#endif

static Node *selectStartNode(const RenderObject *object)
{
    Node *node = 0;
    bool forcedOn = false;

    for (const RenderObject *curr = object; curr; curr = curr->parent()) {
        if (curr->style()->userSelect() == SELECT_TEXT)
            forcedOn = true;
        if (!forcedOn && curr->style()->userSelect() == SELECT_NONE)
            return 0;

        if (!node)
            node = curr->element();
    }

    // somewhere up the render tree there must be an element!
    ASSERT(node);

    return node;
}

bool RenderObject::canSelect() const
{
    return selectStartNode(this) != 0;
}

bool RenderObject::shouldSelect() const
{
    if (Node *node = selectStartNode(this))
        return EventTargetNodeCast(node)->dispatchHTMLEvent(selectstartEvent, true, true);

    return false;
}

Color RenderObject::selectionColor(GraphicsContext* p) const
{
    Color color;
    if (style()->userSelect() != SELECT_NONE) {
        RenderStyle* pseudoStyle = getPseudoStyle(RenderStyle::SELECTION);
        if (pseudoStyle && pseudoStyle->backgroundColor().isValid())
            color = pseudoStyle->backgroundColor();
        else
            color = p->selectedTextBackgroundColor();
    }

    return color;
}

Node* RenderObject::draggableNode(bool dhtmlOK, bool uaOK, int x, int y, bool& dhtmlWillDrag) const
{
    if (!dhtmlOK && !uaOK)
        return 0;

    const RenderObject* curr = this;
    while (curr) {
        Node *elt = curr->element();
        if (elt && elt->nodeType() == Node::TEXT_NODE) {
            // Since there's no way for the author to address the -webkit-user-drag style for a text node,
            // we use our own judgement.
            if (uaOK && view()->frameView()->frame()->shouldDragAutoNode(curr->node(), IntPoint(x, y))) {
                dhtmlWillDrag = false;
                return curr->node();
            } else if (curr->shouldSelect()) {
                // In this case we have a click in the unselected portion of text.  If this text is
                // selectable, we want to start the selection process instead of looking for a parent
                // to try to drag.
                return 0;
            }
        } else {
            EUserDrag dragMode = curr->style()->userDrag();
            if (dhtmlOK && dragMode == DRAG_ELEMENT) {
                dhtmlWillDrag = true;
                return curr->node();
            } else if (uaOK && dragMode == DRAG_AUTO
                       && view()->frameView()->frame()->shouldDragAutoNode(curr->node(), IntPoint(x, y)))
            {
                dhtmlWillDrag = false;
                return curr->node();
            }
        }
        curr = curr->parent();
    }
    return 0;
}

void RenderObject::selectionStartEnd(int& spos, int& epos)
{
    view()->selectionStartEnd(spos, epos);
}

RenderBlock* RenderObject::createAnonymousBlock()
{
    RenderStyle *newStyle = new (renderArena()) RenderStyle();
    newStyle->inheritFrom(m_style);
    newStyle->setDisplay(BLOCK);

    RenderBlock *newBox = new (renderArena()) RenderBlock(document() /* anonymous box */);
    newBox->setStyle(newStyle);
    return newBox;
}

void RenderObject::handleDynamicFloatPositionChange()
{
    // We have gone from not affecting the inline status of the parent flow to suddenly
    // having an impact.  See if there is a mismatch between the parent flow's
    // childrenInline() state and our state.
    setInline(style()->isDisplayInlineType());
    if (isInline() != parent()->childrenInline()) {
        if (!isInline()) {
            if (parent()->isRenderInline()) {
                // We have to split the parent flow.
                RenderInline* parentInline = static_cast<RenderInline*>(parent());
                RenderBlock* newBox = parentInline->createAnonymousBlock();
                
                RenderFlow* oldContinuation = parent()->continuation();
                parentInline->setContinuation(newBox);

                RenderObject* beforeChild = nextSibling();
                parent()->removeChildNode(this);
                parentInline->splitFlow(beforeChild, newBox, this, oldContinuation);
            }
            else if (parent()->isRenderBlock())
                static_cast<RenderBlock*>(parent())->makeChildrenNonInline();
        }
        else {
            // An anonymous block must be made to wrap this inline.
            RenderBlock* box = createAnonymousBlock();
            parent()->insertChildNode(box, this);
            box->appendChildNode(parent()->removeChildNode(this));
        }
    }
}

void RenderObject::setStyle(RenderStyle *style)
{
    if (m_style == style)
        return;

    bool affectsParentBlock = false;
    RenderStyle::Diff d = RenderStyle::Equal;
    if (m_style) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        if (style) {
#if __APPLE__
            if (m_style->visibility() != style->visibility() ||
                m_style->zIndex() != style->zIndex() ||
                m_style->hasAutoZIndex() != style->hasAutoZIndex())
                document()->setDashboardRegionsDirty(true);
#endif

            if ((m_style->hasAutoZIndex() != style->hasAutoZIndex() ||
                 m_style->zIndex() != style->zIndex() ||
                 m_style->visibility() != style->visibility()) && layer()) {
                layer()->stackingContext()->dirtyZOrderLists();
                if (m_style->hasAutoZIndex() != style->hasAutoZIndex() ||
                    m_style->visibility() != style->visibility())
                    layer()->dirtyZOrderLists();
            }
        }

        d = m_style->diff(style);

        // If we have no layer(), just treat a RepaintLayer hint as a normal Repaint.
        if (d == RenderStyle::RepaintLayer && !layer())
            d = RenderStyle::Repaint;
        
        // The background of the root element or the body element could propagate up to
        // the canvas.  Just dirty the entire canvas when our style changes substantially.
        if (d >= RenderStyle::Repaint && element() &&
            (element()->hasTagName(htmlTag) || element()->hasTagName(bodyTag)))
            view()->repaint();
        else if (m_parent && !isText()) {
            // Do a repaint with the old style first, e.g., for example if we go from
            // having an outline to not having an outline.
            if (d == RenderStyle::RepaintLayer) {
                layer()->repaintIncludingDescendants();
                if (!(m_style->clip() == style->clip()))
                    layer()->clearClipRects();
            }
            else if (d == RenderStyle::Repaint)
                repaint();
        }

        // When a layout hint happens, we go ahead and do a repaint of the layer, since the layer could
        // end up being destroyed.
        if (d == RenderStyle::Layout && layer() &&
            (m_style->position() != style->position() ||
             m_style->zIndex() != style->zIndex() ||
             m_style->hasAutoZIndex() != style->hasAutoZIndex() ||
             !(m_style->clip() == style->clip()) ||
             m_style->hasClip() != style->hasClip() ||
             m_style->opacity() != style->opacity()))
            layer()->repaintIncludingDescendants();

        // When a layout hint happens and an object's position style changes, we have to do a layout
        // to dirty the render tree using the old position value now.
        if (d == RenderStyle::Layout && m_parent && m_style->position() != style->position())
            markContainingBlocksForLayout();
        
        if (isFloating() && (m_style->floating() != style->floating()))
            // For changes in float styles, we need to conceivably remove ourselves
            // from the floating objects list.
            removeFromObjectLists();
        else if (isPositioned() && (style->position() != AbsolutePosition && style->position() != FixedPosition))
            // For changes in positioning styles, we need to conceivably remove ourselves
            // from the positioned objects list.
            removeFromObjectLists();
        
        affectsParentBlock = m_style && isFloatingOrPositioned() &&
            (!style->isFloating() && style->position() != AbsolutePosition && style->position() != FixedPosition)
            && parent() && (parent()->isBlockFlow() || parent()->isInlineFlow());
        
        // reset style flags
        m_floating = false;
        m_positioned = false;
        m_relPositioned = false;
        m_paintBackground = false;
        m_hasOverflowClip = false;
    }

    RenderStyle *oldStyle = m_style;
    m_style = style;

    updateBackgroundImages(oldStyle);
    
    if (m_style)
        m_style->ref();
    
    if (oldStyle)
        oldStyle->deref(renderArena());

    setShouldPaintBackgroundOrBorder(m_style->hasBorder() || m_style->hasBackground() || m_style->hasAppearance());

    if (affectsParentBlock)
        handleDynamicFloatPositionChange();
    
    // No need to ever schedule repaints from a style change of a text run, since
    // we already did this for the parent of the text run.
    if (d == RenderStyle::Layout && m_parent)
        setNeedsLayoutAndMinMaxRecalc();
    else if (m_parent && !isText() && (d == RenderStyle::RepaintLayer || d == RenderStyle::Repaint))
        // Do a repaint with the new style now, e.g., for example if we go from
        // not having an outline to having an outline.
        repaint();
}

void RenderObject::setStyleInternal(RenderStyle* st)
{
    if (m_style == st)
        return;
    if (m_style)
        m_style->deref(renderArena());
    m_style = st;
    if (m_style)
        m_style->ref();
}

void RenderObject::updateBackgroundImages(RenderStyle* oldStyle)
{
    // FIXME: This will be slow when a large number of images is used.  Fix by using a dict.
    const BackgroundLayer* oldLayers = oldStyle ? oldStyle->backgroundLayers() : 0;
    const BackgroundLayer* newLayers = m_style ? m_style->backgroundLayers() : 0;
    for (const BackgroundLayer* currOld = oldLayers; currOld; currOld = currOld->next()) {
        if (currOld->backgroundImage() && (!newLayers || !newLayers->containsImage(currOld->backgroundImage())))
            currOld->backgroundImage()->deref(this);
    }
    for (const BackgroundLayer* currNew = newLayers; currNew; currNew = currNew->next()) {
        if (currNew->backgroundImage() && (!oldLayers || !oldLayers->containsImage(currNew->backgroundImage())))
            currNew->backgroundImage()->ref(this);
    }
    
    CachedImage* oldBorderImage = oldStyle ? oldStyle->borderImage().image() : 0;
    CachedImage* newBorderImage = m_style ? m_style->borderImage().image() : 0;
    if (oldBorderImage != newBorderImage) {
        if (oldBorderImage)
            oldBorderImage->deref(this);
        if (newBorderImage)
            newBorderImage->ref(this);
    }
}

IntRect RenderObject::viewRect() const
{
    return view()->viewRect();
}

bool RenderObject::absolutePosition(int &xPos, int &yPos, bool f)
{
    RenderObject* o = parent();
    if (o) {
        o->absolutePosition(xPos, yPos, f);
        yPos += o->borderTopExtra();
        if (o->hasOverflowClip())
            o->layer()->subtractScrollOffset(xPos, yPos); 
        return true;
    }
    else
    {
        xPos = yPos = 0;
        return false;
    }
}

IntRect RenderObject::caretRect(int offset, EAffinity affinity, int *extraWidthToEndOfLine)
{
   if (extraWidthToEndOfLine)
       *extraWidthToEndOfLine = 0;

    return IntRect();
}

int RenderObject::paddingTop() const
{
    int w = 0;
    Length padding = m_style->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.calcMinValue(w);
    if ( isTableCell() && padding.isAuto() )
        w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::paddingBottom() const
{
    int w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.calcMinValue(w);
    if ( isTableCell() && padding.isAuto() )
        w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::paddingLeft() const
{
    int w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.calcMinValue(w);
    if ( isTableCell() && padding.isAuto() )
        w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::paddingRight() const
{
    int w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->contentWidth();
    w = padding.calcMinValue(w);
    if ( isTableCell() && padding.isAuto() )
        w = static_cast<const RenderTableCell *>(this)->table()->cellPadding();
    return w;
}

int RenderObject::tabWidth() const
{
    if (style()->collapseWhiteSpace())
        return 0;
        
    return containingBlock()->tabWidth(true);
}

RenderView* RenderObject::view() const
{
    return static_cast<RenderView*>(document()->renderer());
}

RenderObject *RenderObject::container() const
{
    // This method is extremely similar to containingBlock(), but with a few notable
    // exceptions.
    // (1) It can be used on orphaned subtrees, i.e., it can be called safely even when
    // the object is not part of the primary document subtree yet.
    // (2) For normal flow elements, it just returns the parent.
    // (3) For absolute positioned elements, it will return a relative positioned inline.
    // containingBlock() simply skips relpositioned inlines and lets an enclosing block handle
    // the layout of the positioned object.  This does mean that calcAbsoluteHorizontal and
    // calcAbsoluteVertical have to use container().
    EPosition pos = m_style->position();
    RenderObject *o = 0;
    if (!isText() && pos == FixedPosition) {
        // container() can be called on an object that is not in the
        // tree yet.  We don't call view() since it will assert if it
        // can't get back to the canvas.  Instead we just walk as high up
        // as we can.  If we're in the tree, we'll get the root.  If we
        // aren't we'll get the root of our little subtree (most likely
        // we'll just return 0).
        o = parent();
        while (o && o->parent()) o = o->parent();
    }
    else if (!isText() && pos == AbsolutePosition) {
        // Same goes here.  We technically just want our containing block, but
        // we may not have one if we're part of an uninstalled subtree.  We'll
        // climb as high as we can though.
        o = parent();
        while (o && o->style()->position() == StaticPosition && !o->isRoot() && !o->isRenderView())
            o = o->parent();
    }
    else
        o = parent();
    return o;
}

// This code has been written to anticipate the addition of CSS3-::outside and ::inside generated
// content (and perhaps XBL).  That's why it uses the render tree and not the DOM tree.
RenderObject* RenderObject::hoverAncestor() const
{
    return (!isInline() && continuation()) ? continuation() : parent();
}

bool RenderObject::isSelectionBorder() const
{
    SelectionState st = selectionState();
    return st == SelectionStart || st == SelectionEnd || st == SelectionBoth;
}


void RenderObject::removeFromObjectLists()
{
    if (isFloating()) {
        RenderBlock* outermostBlock = containingBlock();
        for (RenderBlock* p = outermostBlock; p && !p->isRenderView(); p = p->containingBlock()) {
            if (p->containsFloat(this))
                outermostBlock = p;
        }
        
        if (outermostBlock)
            outermostBlock->markAllDescendantsWithFloatsForLayout(this);
    }

    if (isPositioned()) {
        RenderObject *p;
        for (p = parent(); p; p = p->parent()) {
            if (p->isRenderBlock())
                static_cast<RenderBlock*>(p)->removePositionedObject(this);
        }
    }
}

RenderArena* RenderObject::renderArena() const
{
    Document* doc = document();
    return doc ? doc->renderArena() : 0;
}

void RenderObject::remove()
{
    document()->getAccObjectCache()->remove(this);

    removeFromObjectLists();

    if (parent())
        parent()->removeChild(this);
    
    deleteLineBoxWrapper();
}

bool RenderObject::documentBeingDestroyed() const
{
    return !document()->renderer();
}

void RenderObject::destroy()
{
    // By default no ref-counting. RenderWidget::destroy() doesn't call
    // this function because it needs to do ref-counting. If anything
    // in this function changes, be sure to fix RenderWidget::destroy() as well. 

    remove();
    
    arenaDelete(document()->renderArena(), this);
}

void RenderObject::arenaDelete(RenderArena *arena, void *base)
{
    if (m_style->backgroundImage())
        m_style->backgroundImage()->deref(this);
    if (m_style)
        m_style->deref(arena);
    
#ifndef NDEBUG
    void *savedBase = baseOfRenderObjectBeingDeleted;
    baseOfRenderObjectBeingDeleted = base;
#endif
    delete this;
#ifndef NDEBUG
    baseOfRenderObjectBeingDeleted = savedBase;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t *)base, base);
}

VisiblePosition RenderObject::positionForCoordinates(int x, int y)
{
    return VisiblePosition(element(), caretMinOffset(), DOWNSTREAM);
}

bool RenderObject::isDragging() const
{ 
    return m_isDragging; 
}

void RenderObject::updateDragState(bool dragOn)
{
    bool valueChanged = (dragOn != m_isDragging);
    m_isDragging = dragOn;
    if (valueChanged && style()->affectedByDragRules())
        element()->setChanged();
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->updateDragState(dragOn);
    if (continuation())
        continuation()->updateDragState(dragOn);
}

bool RenderObject::hitTest(NodeInfo& info, int x, int y, int tx, int ty, HitTestFilter hitTestFilter)
{
    bool inside = false;
    if (hitTestFilter != HitTestSelf) {
        // First test the foreground layer (lines and inlines).
        inside = nodeAtPoint(info, x, y, tx, ty, HitTestForeground);
        
        // Test floats next.
        if (!inside)
            inside = nodeAtPoint(info, x, y, tx, ty, HitTestFloat);

        // Finally test to see if the mouse is in the background (within a child block's background).
        if (!inside)
            inside = nodeAtPoint(info, x, y, tx, ty, HitTestChildBlockBackgrounds);
    }
    
    // See if the mouse is inside us but not any of our descendants
    if (hitTestFilter != HitTestDescendants && !inside)
        inside = nodeAtPoint(info, x, y, tx, ty, HitTestBlockBackground);
        
    return inside;
}

void RenderObject::setInnerNode(NodeInfo& info)
{
    if (!info.innerNode() && !isInline() && continuation()) {
        // We are in the margins of block elements that are part of a continuation.  In
        // this case we're actually still inside the enclosing inline element that was
        // split.  Go ahead and set our inner node accordingly.
        info.setInnerNode(continuation()->element());
        if (!info.innerNonSharedNode())
            info.setInnerNonSharedNode(continuation()->element());
    }

    if (!info.innerNode() && element())
        info.setInnerNode(element());
            
    if(!info.innerNonSharedNode() && element())
        info.setInnerNonSharedNode(element());
}

bool RenderObject::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                               HitTestAction hitTestAction)
{
    return false;
}

short RenderObject::verticalPositionHint( bool firstLine ) const
{
    short vpos = m_verticalPosition;
    if ( m_verticalPosition == PositionUndefined || firstLine ) {
        vpos = getVerticalPosition( firstLine );
        if ( !firstLine )
            m_verticalPosition = vpos;
    }
    return vpos;

}

short RenderObject::getVerticalPosition( bool firstLine ) const
{
    if (!isInline())
        return 0;

    // This method determines the vertical position for inline elements.
    int vpos = 0;
    EVerticalAlign va = style()->verticalAlign();
    if ( va == TOP ) {
        vpos = PositionTop;
    } else if ( va == BOTTOM ) {
        vpos = PositionBottom;
    } else if ( va == LENGTH ) {
        vpos = -style()->verticalAlignLength().calcValue( lineHeight( firstLine ) );
    } else  {
        bool checkParent = parent()->isInline() && !parent()->isInlineBlockOrInlineTable() && parent()->style()->verticalAlign() != TOP && parent()->style()->verticalAlign() != BOTTOM;
        vpos = checkParent ? parent()->verticalPositionHint( firstLine ) : 0;
        // don't allow elements nested inside text-top to have a different valignment.
        if ( va == BASELINE )
            return vpos;

        const Font &f = parent()->font(firstLine);
        int fontsize = f.pixelSize();
    
        if (va == SUB)
            vpos += fontsize/5 + 1;
        else if (va == SUPER)
            vpos -= fontsize/3 + 1;
        else if (va == TEXT_TOP)
            vpos += baselinePosition(firstLine) - f.ascent();
        else if (va == MIDDLE)
            vpos += - (int)(f.xHeight()/2) - lineHeight( firstLine )/2 + baselinePosition( firstLine );
        else if (va == TEXT_BOTTOM) {
            vpos += f.descent();
            if (!isReplaced())
                vpos -= font(firstLine).descent();
        } else if ( va == BASELINE_MIDDLE )
            vpos += - lineHeight( firstLine )/2 + baselinePosition( firstLine );
    }
    
    return vpos;
}

short RenderObject::lineHeight( bool firstLine, bool ) const
{
    RenderStyle* s = style(firstLine);
    
    Length lh = s->lineHeight();

    // its "unset", choose nice default
    if (lh.value() < 0)
        return s->font().lineSpacing();

    if (lh.isPercent())
        return lh.calcMinValue(s->fontSize());

    // its fixed
    return lh.value();
}

short RenderObject::baselinePosition(bool firstLine, bool isRootLineBox) const
{
    const Font& f = font(firstLine);
    return f.ascent() + (lineHeight(firstLine, isRootLineBox) - f.height()) / 2;
}

void RenderObject::invalidateVerticalPositions()
{
    m_verticalPosition = PositionUndefined;
    RenderObject *child = firstChild();
    while( child ) {
        child->invalidateVerticalPositions();
        child = child->nextSibling();
    }
}

void RenderObject::recalcMinMaxWidths()
{
    KHTMLAssert( m_recalcMinMax );

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << " recalcMinMaxWidths() this=" << this <<endl;
#endif

    if (m_recalcMinMax)
        updateFirstLetter();
    
    RenderObject *child = firstChild();
    while( child ) {
        int cmin = 0;
        int cmax = 0;
        bool test = false;
        if ( ( m_minMaxKnown && child->m_recalcMinMax ) || !child->m_minMaxKnown ) {
            cmin = child->minWidth();
            cmax = child->maxWidth();
            test = true;
        }
        if ( child->m_recalcMinMax )
            child->recalcMinMaxWidths();
        if ( !child->m_minMaxKnown )
            child->calcMinMaxWidth();
        if ( m_minMaxKnown && test && (cmin != child->minWidth() || cmax != child->maxWidth()) )
            m_minMaxKnown = false;
        child = child->nextSibling();
    }

    // we need to recalculate, if the contains inline children, as the change could have
    // happened somewhere deep inside the child tree. Also do this for blocks or tables that
    // are inline (i.e., inline-block and inline-table).
    if ((!isInline() || isInlineBlockOrInlineTable()) && childrenInline())
        m_minMaxKnown = false;

    if ( !m_minMaxKnown )
        calcMinMaxWidth();
    m_recalcMinMax = false;
}

void RenderObject::scheduleRelayout()
{
     if (isRenderView()) {
         FrameView* view = static_cast<RenderView*>(this)->frameView();
         if (view)
             view->scheduleRelayout();
     } else {
         FrameView* v = view() ? view()->frameView() : 0;
         if (v)
             v->scheduleRelayoutOfSubtree(node());
     }
}


void RenderObject::removeLeftoverAnonymousBoxes()
{
}

InlineBox* RenderObject::createInlineBox(bool, bool isRootLineBox, bool)
{
    KHTMLAssert(!isRootLineBox);
    return new (renderArena()) InlineBox(this);
}

void RenderObject::dirtyLineBoxes(bool, bool)
{
}

InlineBox* RenderObject::inlineBoxWrapper() const
{
    return 0;
}

void RenderObject::setInlineBoxWrapper(InlineBox* b)
{
}

void RenderObject::deleteLineBoxWrapper()
{
}

RenderStyle* RenderObject::firstLineStyle() const 
{
    RenderStyle *s = m_style; 
    const RenderObject* obj = isText() ? parent() : this;
    if (obj->isBlockFlow()) {
        RenderBlock* firstLineBlock = obj->firstLineBlock();
        if (firstLineBlock)
            s = firstLineBlock->getPseudoStyle(RenderStyle::FIRST_LINE, style());
    } else if (!obj->isAnonymous() && obj->isInlineFlow()) {
        RenderStyle* parentStyle = obj->parent()->firstLineStyle();
        if (parentStyle != obj->parent()->style()) {
            // A first-line style is in effect. We need to cache a first-line style
            // for ourselves.
            style()->setHasPseudoStyle(RenderStyle::FIRST_LINE_INHERITED);
            s = obj->getPseudoStyle(RenderStyle::FIRST_LINE_INHERITED, parentStyle);
        }
    }
    return s;
}

RenderStyle* RenderObject::getPseudoStyle(RenderStyle::PseudoId pseudo, RenderStyle* parentStyle) const
{
    if (!style()->hasPseudoStyle(pseudo))
        return 0;
    
    if (!parentStyle)
        parentStyle = style();

    RenderStyle* result = style()->getPseudoStyle(pseudo);
    if (result)
        return result;
    
    Node* node = element();
    if (isText())
        node = element()->parentNode();
    if (!node)
        return 0;
    
    if (pseudo == RenderStyle::FIRST_LINE_INHERITED) {
        result = document()->styleSelector()->styleForElement(static_cast<Element*>(node), parentStyle, false);
        result->setStyleType(RenderStyle::FIRST_LINE_INHERITED);
    } else
        result = document()->styleSelector()->pseudoStyleForElement(pseudo, static_cast<Element*>(node), parentStyle);
    if (result) {
        style()->addPseudoStyle(result);
        result->deref(document()->renderArena());
    }
    return result;
}

void RenderObject::getTextDecorationColors(int decorations, Color& underline, Color& overline,
                                           Color& linethrough, bool quirksMode)
{
    RenderObject* curr = this;
    do {
        int currDecs = curr->style()->textDecoration();
        if (currDecs) {
            if (currDecs & UNDERLINE) {
                decorations &= ~UNDERLINE;
                underline = curr->style()->color();
            }
            if (currDecs & OVERLINE) {
                decorations &= ~OVERLINE;
                overline = curr->style()->color();
            }
            if (currDecs & LINE_THROUGH) {
                decorations &= ~LINE_THROUGH;
                linethrough = curr->style()->color();
            }
        }
        curr = curr->parent();
        if (curr && curr->isRenderBlock() && curr->continuation())
            curr = curr->continuation();
    } while (curr && decorations && (!quirksMode || !curr->element() ||
                                     (!curr->element()->hasTagName(aTag) && !curr->element()->hasTagName(fontTag))));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (decorations && curr) {
        if (decorations & UNDERLINE)
            underline = curr->style()->color();
        if (decorations & OVERLINE)
            overline = curr->style()->color();
        if (decorations & LINE_THROUGH)
            linethrough = curr->style()->color();
    }        
}

void RenderObject::updateWidgetPosition()
{
}

DeprecatedValueList<DashboardRegionValue> RenderObject::computeDashboardRegions()
{
    DeprecatedValueList<DashboardRegionValue> regions;
    collectDashboardRegions(regions);
    return regions;
}

void RenderObject::addDashboardRegions (DeprecatedValueList<DashboardRegionValue>& regions)
{
    // Convert the style regions to absolute coordinates.
    if (style()->visibility() != VISIBLE) 
        return;

    DeprecatedValueList<StyleDashboardRegion> styleRegions = style()->dashboardRegions();
    if (styleRegions.count() > 0) {
        unsigned i, count = styleRegions.count();
        for (i = 0; i < count; i++){
            StyleDashboardRegion styleRegion = styleRegions[i];
            
            int w = width();
            int h = height();
            
            DashboardRegionValue region;
            region.label = styleRegion.label;
            region.bounds = IntRect (
                styleRegion.offset.left.value(),
                styleRegion.offset.top.value(),
                w - styleRegion.offset.left.value() - styleRegion.offset.right.value(),
                h - styleRegion.offset.top.value() - styleRegion.offset.bottom.value());
            region.type = styleRegion.type;

            region.clip = region.bounds;
            computeAbsoluteRepaintRect(region.clip);
            if (region.clip.height() < 0) {
                region.clip.setHeight(0);
                region.clip.setWidth(0);
            }

            int x, y;
            absolutePosition(x, y);
            region.bounds.setX(x + styleRegion.offset.left.value());
            region.bounds.setY(y + styleRegion.offset.top.value());
            
            regions.append(region);
        }
    }
}

void RenderObject::collectDashboardRegions (DeprecatedValueList<DashboardRegionValue>& regions)
{
    // RenderTexts don't have their own style, they just use their parent's style,
    // so we don't want to include them.
    if (isText())
        return;
        
    addDashboardRegions (regions);
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
        curr->collectDashboardRegions(regions);
    }
}


void RenderObject::collectBorders(DeprecatedValueList<CollapsedBorderValue>& borderStyles)
{
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->collectBorders(borderStyles);
}

bool RenderObject::avoidsFloats() const
{
    return isReplaced() || isTable() || hasOverflowClip() || isHR() || isFlexibleBox(); 
}

bool RenderObject::usesLineWidth() const
{
    // 1. All auto-width objects that avoid floats should always use lineWidth
    // 2. For objects with a specified width, we match WinIE's behavior:
    // (a) tables use contentWidth
    // (b) <hr>s use lineWidth
    // (c) all other objects use lineWidth in quirks mode and contentWidth in strict mode.
    return (avoidsFloats() && (style()->width().isAuto() || isHR() || (style()->htmlHacks() && !isTable())));
}

UChar RenderObject::backslashAsCurrencySymbol() const
{
    if (Node *node = element())
        if (Decoder *decoder = node->document()->decoder())
            return decoder->encoding().backslashAsCurrencySymbol();
    return '\\';
}

void RenderObject::imageChanged(CachedImage *image)
{
    // Repaint when the background image or border image finishes loading.
    // This is needed for RenderBox objects, and also for table objects that hold
    // backgrounds that are then respected by the table cells (which are RenderBox
    // subclasses). It would be even better to find a more elegant way of doing this that
    // would avoid putting this function and the CachedObjectClient base class into RenderObject.
    if (image && image->canRender() && parent()) {
        if (view() && element() && (element()->hasTagName(htmlTag) || element()->hasTagName(bodyTag)))
            view()->repaint();    // repaint the entire canvas since the background gets propagated up
        else
            repaint();              // repaint object, which is a box or a container with boxes inside it
    }
}

bool RenderObject::willRenderImage(CachedImage*)
{
    // Without visibility we won't render (and therefore don't care about animation).
    if (style()->visibility() != VISIBLE)
        return false;

    // If we're not in a window (i.e., we're dormant from being put in the b/f cache or in a background tab)
    // then we don't want to render either.
    return !document()->inPageCache() && document()->view()->inWindow();
}

int RenderObject::maximalOutlineSize(PaintPhase p) const
{
    if (p != PaintPhaseOutline && p != PaintPhaseSelfOutline)
        return 0;
    return static_cast<RenderView*>(document()->renderer())->maximalOutlineSize();
}

int RenderObject::caretMinOffset() const
{
    return 0;
}

int RenderObject::caretMaxOffset() const
{
    return 0;
}

unsigned RenderObject::caretMaxRenderedOffset() const
{
    return 0;
}

int RenderObject::previousOffset (int current) const
{
    int previousOffset = current - 1;
    return previousOffset;
}

int RenderObject::nextOffset (int current) const
{
    int nextOffset = current + 1;
    return nextOffset;
}

InlineBox *RenderObject::inlineBox(int offset, EAffinity affinity)
{
    return inlineBoxWrapper();
}

#if SVG_SUPPORT

FloatRect RenderObject::relativeBBox(bool) const
{
    return FloatRect();
}

QMatrix RenderObject::localTransform() const
{
    return QMatrix(1, 0, 0, 1, xPos(), yPos());
}
 
void RenderObject::setLocalTransform(const QMatrix&)
{
    ASSERT(false);
}

QMatrix RenderObject::absoluteTransform() const
{
    if (parent())
        return localTransform() * parent()->absoluteTransform();
    return localTransform();
}

#endif

}

#ifndef NDEBUG

void showTree(const WebCore::RenderObject* ro)
{
    if (ro)
        ro->showTreeForThis();
}

#endif
