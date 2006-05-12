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
#include "RenderWidget.h"

#include "Document.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "RenderCanvas.h"

using namespace std;

namespace WebCore {

using namespace EventNames;

// Returns 1 since a replaced element can have the caret positioned 
// at its beginning (0), or at its end (1).
// NOTE: Yet, "select" elements can have any number of "option" elements
// as children, so this "0 or 1" idea does not really hold up.

RenderWidget::RenderWidget(Node* node)
      : RenderReplaced(node)
      , m_widget(0)
      , m_refCount(0)
{
    // a replaced element doesn't support being anonymous
    ASSERT(node);
    m_view = node->document()->view();

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

void RenderWidget::paint(PaintInfo& i, int tx, int ty)
{
    if (!shouldPaint(i, tx, ty))
        return;

    tx += m_x;
    ty += m_y;

    if (shouldPaintBackgroundOrBorder() && i.phase != PaintPhaseOutline && i.phase != PaintPhaseSelfOutline) 
        paintBoxDecorations(i, tx, ty);

    if (!m_view || i.phase != PaintPhaseForeground || style()->visibility() != VISIBLE)
        return;

    if (m_widget) {
        // Move the widget if necessary.  We normally move and resize widgets during layout, but sometimes
        // widgets can move without layout occurring (most notably when you scroll a document that
        // contains fixed positioned elements).
        m_widget->move(tx + borderLeft() + paddingLeft(), ty + borderTop() + paddingTop());

        // Tell the widget to paint now.  This is the only time the widget is allowed
        // to paint itself.  That way it will composite properly with z-indexed layers.
        m_widget->paint(i.p, i.r);
    }

    // Paint a partially transparent wash over selected widgets.
    if (isSelected() && !document()->printing())
        i.p->fillRect(selectionRect(), selectionColor(i.p));
}

void RenderWidget::focusIn(Widget*)
{
    RenderArena* arena = ref();
    RefPtr<Node> elem = element();
    if (elem)
        elem->document()->setFocusNode(elem);
    deref(arena);
}

void RenderWidget::focusOut(Widget*)
{
    RenderArena* arena = ref();
    RefPtr<Node> elem = element();
    if (elem && elem == elem->document()->focusNode())
        elem->document()->setFocusNode(0);
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
            m_widget->setIsSelected(isSelected());
    }
}

void RenderWidget::deleteWidget()
{
    delete m_widget;
}
}
