/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "render_arena.h"
#include "render_canvas.h"
#include "render_line.h"

#include <assert.h>
#include <qwidget.h>
#include <qpainter.h>
#include <qevent.h>
#include <qapplication.h>

#include "khtml_ext.h"
#include "khtmlview.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom_docimpl.h" // ### remove dependency
#include "xml/dom_position.h"
#include "xml/EventNames.h"
#include <kdebug.h>

using namespace khtml;
using namespace DOM;
using namespace DOM::EventNames;

RenderReplaced::RenderReplaced(DOM::NodeImpl* node)
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
    if ((tx >= i.r.x() + i.r.width() + os) || (tx + m_width <= i.r.x() - os))
        return false;
    if ((top >= i.r.y() + i.r.height() + os) || (bottom <= i.r.y() - os))
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

QRect RenderReplaced::selectionRect()
{
    if (selectionState() == SelectionNone)
        return QRect();
    if (!m_inlineBoxWrapper)
        // We're a block-level replaced element.  Just return our own dimensions.
        return absoluteBoundingBoxRect();

    RenderBlock* cb =  containingBlock();
    if (!cb)
        return QRect();
    
    RootInlineBox* root = m_inlineBoxWrapper->root();
    int selectionTop = root->selectionTop();
    int selectionHeight = root->selectionHeight();
    int selectionLeft = xPos();
    int selectionRight = xPos() + width();
    
    int absx, absy;
    cb->absolutePosition(absx, absy);

    return QRect(selectionLeft + absx, selectionTop + absy, selectionRight - selectionLeft, selectionHeight);
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


QColor RenderReplaced::selectionColor(QPainter *p) const
{
    QColor color = RenderBox::selectionColor(p);
         
    // Force a 60% alpha so that no user-specified selection color can obscure selected images.
    if (qAlpha(color.rgb()) > 153)
        color = QColor(qRgba(color.red(), color.green(), color.blue(), 153));

    return color;
}

// -----------------------------------------------------------------------------

RenderWidget::RenderWidget(DOM::NodeImpl* node)
      : RenderReplaced(node),
        m_deleteWidget(false),
        m_refCount(0)
{
    m_widget = 0;
    // a replaced element doesn't support being anonymous
    assert(node);
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
            m_view->removeChild( m_widget );

        m_widget->removeEventFilter( this );
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

    if (m_deleteWidget)
        delete m_widget;
}

void  RenderWidget::resizeWidget( QWidget *widget, int w, int h )
{

    if (element() && (widget->width() != w || widget->height() != h)) {
        RenderArena *arena = ref();
        element()->ref();
        widget->resize( w, h );
        element()->deref();
        deref(arena);
    }
}

void RenderWidget::setQWidget(QWidget *widget, bool deleteWidget)
{
    if (widget != m_widget)
    {
        if (m_widget) {
            m_widget->removeEventFilter(this);
            disconnect( m_widget, SIGNAL( destroyed()), this, SLOT( slotWidgetDestructed()));
            if (m_deleteWidget)
                delete m_widget;
            m_widget = 0;
        }
        m_widget = widget;
        if (m_widget) {
            connect( m_widget, SIGNAL( destroyed()), this, SLOT( slotWidgetDestructed()));
            m_widget->installEventFilter(this);
            // if we've already received a layout, apply the calculated space to the
            // widget immediately, but we have to have really been full constructed (with a non-null
            // style pointer).
            if (!needsLayout() && style()) {
                resizeWidget( m_widget,
                              m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                              m_height-borderLeft()-borderRight()-paddingLeft()-paddingRight() );
            }
            else
                setPos(xPos(), -500000);

            if (style()) {
                if (style()->visibility() != VISIBLE)
                    m_widget->hide();
                else
                    m_widget->show();
            }
        }
        m_view->addChild( m_widget, -500000, 0 );
    }
    m_deleteWidget = deleteWidget;
}

void RenderWidget::layout( )
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    setNeedsLayout(false);
}

void RenderWidget::sendConsumedMouseUp()
{
    RenderArena *arena = ref();
    element()->dispatchSimulatedMouseEvent(mouseupEvent);
    deref(arena);
}

void RenderWidget::slotWidgetDestructed()
{
    m_widget = 0;
}

void RenderWidget::setStyle(RenderStyle *_style)
{
    RenderReplaced::setStyle(_style);
    if (m_widget)
    {
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
    int newX = _tx + borderLeft() + paddingLeft();
    int newY = _ty + borderTop() + paddingTop();
    if (m_view->childX(m_widget) != newX || m_view->childY(m_widget) != newY)
        m_widget->move(newX, newY);
    
    // Tell the widget to paint now.  This is the only time the widget is allowed
    // to paint itself.  That way it will composite properly with z-indexed layers.
    m_widget->paint(i.p, i.r);
    
    // Paint a partially transparent wash over selected widgets.
    bool isPrinting = (i.p->device()->devType() == QInternal::Printer);
    if (m_selectionState != SelectionNone && !isPrinting) {
        QBrush brush(selectionColor(i.p));
        QRect selRect(selectionRect());
        i.p->fillRect(selRect.x(), selRect.y(), selRect.width(), selRect.height(), brush);
    }
}

void RenderWidget::handleFocusOut()
{
}

bool RenderWidget::eventFilter(QObject* /*o*/, QEvent* e)
{
    if ( !element() ) return true;

    RenderArena *arena = ref();
    DOM::NodeImpl *elem = element();
    elem->ref();

    bool filtered = false;

    //kdDebug() << "RenderWidget::eventFilter type=" << e->type() << endl;
    switch(e->type()) {
    case QEvent::FocusOut:
       //static const char* const r[] = {"Mouse", "Tab", "Backtab", "ActiveWindow", "Popup", "Shortcut", "Other" };
        //kdDebug() << "RenderFormElement::eventFilter FocusOut widget=" << m_widget << " reason:" << r[QFocusEvent::reason()] << endl;
        // Don't count popup as a valid reason for losing the focus
        // (example: opening the options of a select combobox shouldn't emit onblur)
        if ( QFocusEvent::reason() != QFocusEvent::Popup )
       {
           //kdDebug(6000) << "RenderWidget::eventFilter captures FocusOut" << endl;
            if (elem->getDocument()->focusNode() == elem)
                elem->getDocument()->setFocusNode(0);
            handleFocusOut();
        }
        break;
    case QEvent::FocusIn:
        //kdDebug(6000) << "RenderWidget::eventFilter captures FocusIn" << endl;
        elem->getDocument()->setFocusNode(elem);
        break;
    case QEvent::MouseButtonPress:
//       handleMousePressed(static_cast<QMouseEvent*>(e));
        break;
    case QEvent::MouseButtonRelease:
        break;
    case QEvent::MouseButtonDblClick:
        break;
    case QEvent::MouseMove:
        break;
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
        if (!elem->dispatchKeyEvent(static_cast<QKeyEvent*>(e)))
            filtered = true;
        break;
    }
    default: break;
    };

    elem->deref();

    // stop processing if the widget gets deleted, but continue in all other cases
    if (m_refCount == 1)
        filtered = true;
    deref(arena);

    return filtered;
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
    absolutePosition(x,y);
    x += borderLeft() + paddingLeft();
    y += borderTop() + paddingTop();
    width = m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    height = m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom();
    QRect newBounds(x,y,width,height);
    QRect oldBounds(m_widget->frameGeometry());
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
    if (m_selectionState != s) {
        RenderReplaced::setSelectionState(s);
        m_selectionState = s;
        if (m_widget)
            m_widget->setIsSelected(m_selectionState != SelectionNone);
    }
}
