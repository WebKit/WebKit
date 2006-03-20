/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
#include "render_replaced.h"

#include "Document.h" // ### remove dependency
#include "EventNames.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "dom2_eventsimpl.h"
#include "Position.h"
#include "BrowserExtension.h"
#include "RenderArena.h"
#include "RenderCanvas.h"
#include "render_line.h"
#include "VisiblePosition.h"
#include "Widget.h"

namespace WebCore {

using namespace EventNames;

RenderReplaced::RenderReplaced(WebCore::Node* node)
    : RenderBox(node)
{
    // init RenderObject attributes
    setReplaced(true);

    m_intrinsicWidth = 300;
    m_intrinsicHeight = 150;
    m_selectionState = SelectionNone;
}

bool RenderReplaced::shouldPaint(PaintInfo& i, int& _tx, int& _ty)
{
    if (i.phase != PaintActionForeground && i.phase != PaintActionOutline && i.phase != PaintActionSelection)
        return false;

    if (!shouldPaintWithinRoot(i))
        return false;
        
    // if we're invisible or haven't received a layout yet, then just bail.
    if (style()->visibility() != VISIBLE || m_y <=  -500000)  return false;

    int tx = _tx + m_x;
    int ty = _ty + m_y;

    // Early exit if the element touches the edges.
    int top = ty;
    int bottom = ty + m_height;
    if (m_selectionState != SelectionNone && m_inlineBoxWrapper) {
        int selTop = _ty + m_inlineBoxWrapper->root()->selectionTop();
        int selBottom = _ty + selTop + m_inlineBoxWrapper->root()->selectionHeight();
        top = kMin(selTop, top);
        bottom = kMax(selBottom, bottom);
    }
    
    int os = 2*maximalOutlineSize(i.phase);
    if (tx >= i.r.right() + os || tx + m_width <= i.r.x() - os)
        return false;
    if (top >= i.r.bottom() + os || bottom <= i.r.y() - os)
        return false;

    return true;
}

void RenderReplaced::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown());

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderReplaced::calcMinMaxWidth() known=" << minMaxKnown() << endl;
#endif

    int width = calcReplacedWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight();
    if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent())) {
        m_minWidth = 0;
        m_maxWidth = width;
    } else
        m_minWidth = m_maxWidth = width;

    setMinMaxKnown();
}

short RenderReplaced::lineHeight( bool, bool ) const
{
    return height()+marginTop()+marginBottom();
}

short RenderReplaced::baselinePosition( bool, bool ) const
{
    return height()+marginTop()+marginBottom();
}

int RenderReplaced::caretMinOffset() const 
{ 
    return 0; 
}

// Returns 1 since a replaced element can have the caret positioned 
// at its beginning (0), or at its end (1).
// NOTE: Yet, "select" elements can have any number of "option" elements
// as children, so this "0 or 1" idea does not really hold up.
int RenderReplaced::caretMaxOffset() const 
{ 
    return 1; 
}

unsigned RenderReplaced::caretMaxRenderedOffset() const
{
    return 1; 
}

VisiblePosition RenderReplaced::positionForCoordinates(int _x, int _y)
{
    InlineBox *box = inlineBoxWrapper();
    if (!box)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    RootInlineBox *root = box->root();

    int absx, absy;
    containingBlock()->absolutePosition(absx, absy);

    int top = absy + root->topOverflow();
    int bottom = root->nextRootBox() ? absy + root->nextRootBox()->topOverflow() : absy + root->bottomOverflow();

    if (_y < top)
        return VisiblePosition(element(), caretMinOffset(), DOWNSTREAM); // coordinates are above
    
    if (_y >= bottom)
        return VisiblePosition(element(), caretMaxOffset(), DOWNSTREAM); // coordinates are below
    
    if (element()) {
        if (_x <= absx + xPos() + (width() / 2))
            return VisiblePosition(element(), 0, DOWNSTREAM);

        return VisiblePosition(element(), 1, DOWNSTREAM);
    }

    return RenderBox::positionForCoordinates(_x, _y);
}

IntRect RenderReplaced::selectionRect()
{
    if (selectionState() == SelectionNone)
        return IntRect();
    if (!m_inlineBoxWrapper)
        // We're a block-level replaced element.  Just return our own dimensions.
        return absoluteBoundingBoxRect();

    RenderBlock* cb =  containingBlock();
    if (!cb)
        return IntRect();
    
    RootInlineBox* root = m_inlineBoxWrapper->root();
    int selectionTop = root->selectionTop();
    int selectionHeight = root->selectionHeight();
    int selectionLeft = xPos();
    int selectionRight = xPos() + width();
    
    int absx, absy;
    cb->absolutePosition(absx, absy);
    if (cb->hasOverflowClip())
        cb->layer()->subtractScrollOffset(absx, absy);

    return IntRect(selectionLeft + absx, selectionTop + absy, selectionRight - selectionLeft, selectionHeight);
}

void RenderReplaced::setSelectionState(SelectionState s)
{
    m_selectionState = s;
    if (m_inlineBoxWrapper) {
        RootInlineBox* line = m_inlineBoxWrapper->root();
        if (line)
            line->setHasSelectedChildren(s != SelectionNone);
    }
    
    containingBlock()->setSelectionState(s);
}

bool RenderReplaced::isSelected()
{
    SelectionState s = selectionState();
    if (s == SelectionNone)
        return false;
    if (s == SelectionInside)
        return true;

    int selectionStart, selectionEnd;
    RenderObject::selectionStartEnd(selectionStart, selectionEnd);
    if (s == SelectionStart)
        return selectionStart == 0;
        
    int end = element()->hasChildNodes() ? element()->childNodeCount() : 1;
    if (s == SelectionEnd)
        return selectionEnd == end;
    if (s == SelectionBoth)
        return selectionStart == 0 && selectionEnd == end;
        
    ASSERT(0);
    return false;
}

Color RenderReplaced::selectionColor(GraphicsContext* p) const
{
    Color color = RenderBox::selectionColor(p);
         
    // Force a 60% alpha so that no user-specified selection color can obscure selected images.
    if (color.alpha() > 153)
        color = Color(color.red(), color.green(), color.blue(), 153);

    return color;
}

// -----------------------------------------------------------------------------

RenderWidget::RenderWidget(WebCore::Node* node)
      : RenderReplaced(node)
      , m_refCount(0)
{
    m_widget = 0;
    // a replaced element doesn't support being anonymous
    ASSERT(node);
    m_view = node->getDocument()->view();

    canvas()->addWidget(this);

    // this is no real reference counting, its just there
    // to make sure that we're not deleted while we're recursed
    // in an eventFilter of the widget
    ref();
}

void RenderWidget::destroy()
{
    // We can't call the base class's destroy because we don't
    // want to unconditionally delete ourselves (we're ref-counted).
    // So the code below includes copied and pasted contents of
    // both RenderBox::destroy() and RenderObject::destroy().
    // Fix originally made for <rdar://problem/4228818>.

    if (RenderCanvas *c = canvas())
        c->removeWidget(this);

    remove();

    if (m_widget) {
        if (m_view)
            m_view->removeChild(m_widget);
        m_widget->setClient(0);
    }

    RenderLayer* layer = m_layer;
    RenderArena* arena = renderArena();
    
    if (layer)
        layer->clearClipRect();
    
    setNode(0);
    deref(arena);
    
    if (layer)
        layer->destroy(arena);
}

RenderWidget::~RenderWidget()
{
    KHTMLAssert(m_refCount <= 0);
    deleteWidget();
}

void RenderWidget::resizeWidget(Widget* widget, int w, int h)
{
    if (element() && (widget->width() != w || widget->height() != h)) {
        RenderArena *arena = ref();
        element()->ref();
        widget->resize(w, h);
        element()->deref();
        deref(arena);
    }
}

void RenderWidget::setWidget(Widget* widget)
{
    if (widget != m_widget) {
        if (m_widget) {
            m_widget->setClient(0);
            deleteWidget();
        }
        m_widget = widget;
        if (m_widget) {
            m_widget->setClient(this);
            // if we've already received a layout, apply the calculated space to the
            // widget immediately, but we have to have really been full constructed (with a non-null
            // style pointer).
            if (!needsLayout() && style())
                resizeWidget(m_widget,
                    m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight(),
                    m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom());
            else
                setPos(xPos(), -500000);
            if (style()) {
                if (style()->visibility() != VISIBLE)
                    m_widget->hide();
                else
                    m_widget->show();
            }
            m_view->addChild(m_widget, -500000, 0);
        }
    }
}

void RenderWidget::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    setNeedsLayout(false);
}

void RenderWidget::sendConsumedMouseUp(Widget*)
{
    RenderArena* arena = ref();
    EventTargetNodeCast(node())->dispatchSimulatedMouseEvent(mouseupEvent);
    deref(arena);
}

void RenderWidget::setStyle(RenderStyle *_style)
{
    RenderReplaced::setStyle(_style);
    if (m_widget) {
        m_widget->setFont(style()->font());
        if (style()->visibility() != VISIBLE)
            m_widget->hide();
        else
            m_widget->show();
    }
}

void RenderWidget::paint(PaintInfo& i, int _tx, int _ty)
{
    if (!shouldPaint(i, _tx, _ty)) return;

    _tx += m_x;
    _ty += m_y;
    
    if (shouldPaintBackgroundOrBorder() && i.phase != PaintActionOutline) 
        paintBoxDecorations(i, _tx, _ty);

    if (!m_widget || !m_view || i.phase != PaintActionForeground ||
        style()->visibility() != VISIBLE)
        return;

    // Move the widget if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    m_widget->move(_tx + borderLeft() + paddingLeft(), _ty + borderTop() + paddingTop());
    
    // Tell the widget to paint now.  This is the only time the widget is allowed
    // to paint itself.  That way it will composite properly with z-indexed layers.
    m_widget->paint(i.p, i.r);
    
    // Paint a partially transparent wash over selected widgets.
    if (isSelected() && !i.p->printing())
        i.p->fillRect(selectionRect(), selectionColor(i.p));
}

void RenderWidget::focusIn(Widget*)
{
    RenderArena* arena = ref();
    RefPtr<Node> elem = element();
    if (elem)
        elem->getDocument()->setFocusNode(elem);
    deref(arena);
}

void RenderWidget::focusOut(Widget*)
{
    RenderArena* arena = ref();
    RefPtr<Node> elem = element();
    if (elem && elem == elem->getDocument()->focusNode())
        elem->getDocument()->setFocusNode(0);
    deref(arena);
}

void RenderWidget::scrollToVisible(Widget* widget)
{
    if (RenderLayer* layer = enclosingLayer())
        layer->scrollRectToVisible(absoluteBoundingBoxRect());
}

bool RenderWidget::isVisible(Widget* widget)
{
    return style()->visibility() == VISIBLE;
}

Element* RenderWidget::element(Widget* widget)
{
    Node* n = node();
    return n->isElementNode() ? static_cast<Element*>(n) : 0;
}

void RenderWidget::deref(RenderArena *arena)
{
    if (--m_refCount <= 0)
        arenaDelete(arena, this);
}

void RenderWidget::updateWidgetPosition()
{
    if (!m_widget)
        return;
    
    int x, y, width, height;
    absolutePosition(x, y);
    x += borderLeft() + paddingLeft();
    y += borderTop() + paddingTop();
    width = m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    height = m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom();
    IntRect newBounds(x,y,width,height);
    IntRect oldBounds(m_widget->frameGeometry());
    if (newBounds != oldBounds) {
        // The widget changed positions.  Update the frame geometry.
        if (checkForRepaintDuringLayout()) {
            RenderCanvas* c = canvas();
            if (!c->printingMode()) {
                c->repaintViewRectangle(oldBounds);
                c->repaintViewRectangle(newBounds);
            }
        }

        RenderArena *arena = ref();
        element()->ref();
        m_widget->setFrameGeometry(newBounds);
        element()->deref();
        deref(arena);
    }
}

void RenderWidget::setSelectionState(SelectionState s) 
{
    if (selectionState() != s) {
        RenderReplaced::setSelectionState(s);
        m_selectionState = s;
        if (m_widget)
            m_widget->setIsSelected(m_selectionState != SelectionNone);
    }
}

void RenderWidget::deleteWidget()
{
    delete m_widget;
}

}
