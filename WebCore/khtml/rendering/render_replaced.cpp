/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
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
 * $Id$
 */
#include "render_replaced.h"
#include "render_root.h"

#include <assert.h>
#include <qwidget.h>
#include <qpainter.h>
#include <qevent.h>
#include <qapplication.h>

#include "khtmlview.h"
#include "xml/dom2_eventsimpl.h"
#include "khtml_part.h"
#include "xml/dom_docimpl.h" // ### remove dependency

using namespace khtml;
using namespace DOM;


RenderReplaced::RenderReplaced(DOM::NodeImpl* node)
    : RenderBox(node)
{
    // init RenderObject attributes
    setReplaced(true);

    m_intrinsicWidth = 200;
    m_intrinsicHeight = 150;
}

void RenderReplaced::print( QPainter *p, int _x, int _y, int _w, int _h,
                            int _tx, int _ty)
{
    // not visible or nont even once layouted?
    if (style()->visibility() != VISIBLE || m_y <=  -500000)  return;

    _tx += m_x;
    _ty += m_y;

    if((_ty > _y + _h) || (_ty + m_height < _y)) return;

    if(hasSpecialObjects()) printBoxDecorations(p, _x, _y, _w, _h, _tx, _ty);

    // overflow: hidden
    bool clipped = false;
    if (style()->overflow()==OHIDDEN || style()->jsClipMode() ) {
        calcClip(p, _tx, _ty);
	clipped = true;
    }

    printObject(p, _x, _y, _w, _h, _tx, _ty);

    // overflow: hidden
    // restore clip region
    if ( clipped ) {
	p->restore();
    }
}

void RenderReplaced::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown());

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderReplaced::calcMinMaxWidth() known=" << minMaxKnown() << endl;
#endif

    bool isPercent = false;
    int width = calcReplacedWidth(&isPercent);

    if ( isPercent ) {
        m_minWidth = 0;
        m_maxWidth = width;
    }
    else
        m_minWidth = m_maxWidth = width;

    setMinMaxKnown();
}

short RenderReplaced::lineHeight( bool ) const
{
    return height()+marginTop()+marginBottom();
}

short RenderReplaced::baselinePosition( bool ) const
{
    return height()+marginTop()+marginBottom();
}

void RenderReplaced::position(int x, int y, int, int, int, bool, bool)
{
    m_x = x + marginLeft();
    m_y = y + marginTop();
}

// -----------------------------------------------------------------------------

RenderWidget::RenderWidget(DOM::NodeImpl* node)
        : RenderReplaced(node)
{
    m_widget = 0;
    // a replaced element doesn't support being anonymous
    assert(node);
    m_view = node->getDocument()->view();
    m_paintingSelf = false;
    m_ignorePaintEvents = false;
    m_widgetShown = false;

    // this is no real reference counting, its just there
    // to make sure that we're not deleted while we're recursed
    // in an eventFilter of the widget
    ref();
}

void RenderWidget::detach()
{
    remove();

    if ( m_widget ) {
        if ( m_view )
            m_view->removeChild( m_widget );

        m_widget->removeEventFilter( this );
        m_widget->setMouseTracking( false );
    }
    deref();
}

RenderWidget::~RenderWidget()
{
    KHTMLAssert( refCount() <= 0 );

    delete m_widget;
}

void RenderWidget::setQWidget(QWidget *widget)
{
    if (widget != m_widget)
    {
        if (m_widget) {
            m_widget->removeEventFilter(this);
            disconnect( m_widget, SIGNAL( destroyed()), this, SLOT( slotWidgetDestructed()));
            delete m_widget;
            m_widget = 0;
        }
        m_widget = widget;
        if (m_widget) {
            connect( m_widget, SIGNAL( destroyed()), this, SLOT( slotWidgetDestructed()));
            m_widget->installEventFilter(this);
            // if we're already layouted, apply the calculated space to the
            // widget immediately
            if (layouted()) {
                m_widget->resize( m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                                  m_height-borderLeft()-borderRight()-paddingLeft()-paddingRight());
            }
            else
                setPos(xPos(), -500000);
        }
    }
}

void RenderWidget::layout( )
{
    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );
    if ( m_widget )
        m_widget->resize( m_width-borderLeft()-borderRight()-paddingLeft()-paddingRight(),
                          m_height-borderLeft()-borderRight()-paddingLeft()-paddingRight());
    setLayouted();
}

void RenderWidget::slotWidgetDestructed()
{
    m_widget = 0;
}

void RenderWidget::setStyle(RenderStyle *_style)
{
    RenderReplaced::setStyle(_style);
    if(m_widget)
    {
        m_widget->setFont(style()->font());
        if (style()->visibility() != VISIBLE) {
            m_widget->hide();
            m_widgetShown = false;
        }
    }

    // do not paint background or borders for widgets
    setSpecialObjects(false);
}

void RenderWidget::printObject(QPainter *p, int, int, int, int, int _tx, int _ty)
{
    if (!m_widget || !m_view)
	return;

    if (style()->visibility() != VISIBLE) {
	m_widget->hide();
	m_widgetShown = false;
	return;
    }

    // add offset for relative positioning
    if(isRelPositioned())
	relativePositionOffset(_tx, _ty);

    // Although the widget is a child of our KHTMLView, it is not displayed directly in the view area. Instead, it is
    // first drawn onto a pixmap which is then drawn onto the rendering area. This is to that form controls and other
    // widgets can be drawn as part of the normal rendering process, and the z-index of other objects can be properly
    // taken into account.

//    if (!m_widgetShown) {
//	m_widgetShown = true;
	m_view->addChild(m_widget, _tx+borderLeft()+paddingLeft(), _ty+borderTop()+paddingTop());
	m_widget->show();
//    }

/*
    m_view->setIgnoreEvents(true);
    QWidget *prevFocusWidget = qApp->focusWidget();
    DocumentImpl *doc = m_view->part()->xmlDocImpl();
    if (doc->focusNode() && doc->focusNode()->renderer() == this)// ### use RenderObject flag
	m_widget->setFocus();

    m_paintingSelf = true;
    QPixmap widgetPixmap = QPixmap::grabWidget(m_widget);
    m_paintingSelf = false;

    if (prevFocusWidget)
	prevFocusWidget->setFocus();
    m_view->setIgnoreEvents(false);

    p->drawPixmap(_tx+borderLeft()+paddingLeft(), _ty+borderTop()+paddingTop(), widgetPixmap);
*/
}

bool RenderWidget::eventFilter(QObject *o, QEvent *e)
{
/*
//    if (e->type() == QEvent::ShowWindowRequest)
//	return true;

    if ((e->type() == QEvent::Paint) && m_paintingSelf)
	return false;

    if (e->type() == QEvent::Paint) {
	if (!m_paintingSelf && !m_ignorePaintEvents) {
	    int xpos = 0;
	    int ypos = 0;
	    absolutePosition(xpos,ypos);
	    m_view->updateContents(xpos,ypos,width(),height());
	}
	return true;
    }
*/
    return QObject::eventFilter(o,e);
}

void RenderWidget::handleDOMEvent(EventImpl *evt)
{
    // Since the widget is stored outside of the normal viewing area and doesn't receive the events directly (the
    // events are recevied by our KHTMLView), we have to pass the events on to the widget. Doing this also ensures that
    // widgets only receive events if the event occurs over part of the widget that is visible, e.g. if there is an
    // object with a higher z-index partially obscuring the widget and the user clicks on it, the widget won't get the
    // event

    if (!m_widget)
	return;

    bool doRepaint = false;

    if (evt->isMouseEvent()) {
	MouseEventImpl *mev = static_cast<MouseEventImpl*>(evt);

	// Work out event type
	QEvent::Type qtype = QEvent::None;
	if (mev->id() == EventImpl::MOUSEDOWN_EVENT)
	    qtype = QEvent::MouseButtonPress;
	else if (mev->id() == EventImpl::MOUSEUP_EVENT)
	    qtype = QEvent::MouseButtonRelease;
	else if (mev->id() == EventImpl::MOUSEMOVE_EVENT)
	    qtype = QEvent::MouseMove;
	else if (mev->id() == EventImpl::MOUSEOVER_EVENT)
	    qtype = QEvent::Enter;
	else if (mev->id() == EventImpl::MOUSEOUT_EVENT)
	    qtype = QEvent::Leave;
	else if (mev->id() == EventImpl::KHTML_ORIGCLICK_MOUSEUP_EVENT)
	    qtype = QEvent::MouseButtonRelease;

	// Work out button
	int button;
	if (mev->button() == 2)
	    button = Qt::RightButton;
	else if (mev->button() == 1)
	    button = Qt::MidButton;
	else
	    button = Qt::LeftButton;

	// Work out key modifier state
	int state = 0;
	if (mev->ctrlKey())
	    state |= Qt::ControlButton;
	if (mev->altKey())
	    state |= Qt::AltButton;
	if (mev->shiftKey())
	    state |= Qt::ShiftButton;
	// ### meta key ?

	// Get local & global positions for the mouse event
	int xpos = 0;
	int ypos = 0;
	absolutePosition(xpos,ypos);
	QPoint pos(mev->clientX()-xpos,mev->clientY()-ypos);
	QPoint globalPos(mev->screenX(),mev->screenY());

	// Create the event
	QEvent *event = 0;
	if (qtype == QEvent::MouseButtonPress || qtype == QEvent::MouseButtonRelease || qtype == QEvent::MouseMove)
	    event = new QMouseEvent(qtype,pos,globalPos,button,state);
	else if (qtype == QEvent::Enter || qtype == QEvent::Leave)
	    event = new QEvent(qtype);

	// Send the actual event to the widget
	if (event) {
	    if (sendWidgetEvent(event))
		doRepaint = true;
	    delete event;
	}
    }
    else if (evt->id() == EventImpl::DOMFOCUSIN_EVENT) {
	QFocusEvent focusEvent(QEvent::FocusIn);

	m_widget->setFocusProxy(m_view);
	m_view->setFocus();

	sendWidgetEvent(&focusEvent);
	doRepaint = true;
    }
    else if (evt->id() == EventImpl::DOMFOCUSOUT_EVENT) {
	QFocusEvent focusEvent(QEvent::FocusOut);

	m_widget->setFocusProxy(0);

	sendWidgetEvent(&focusEvent);
	doRepaint = true;
    }
    else if (evt->id() == EventImpl::KHTML_KEYDOWN_EVENT ||
	     evt->id() == EventImpl::KHTML_KEYUP_EVENT) {
	KeyEventImpl *keyEvent = static_cast<KeyEventImpl*>(evt);

	if (sendWidgetEvent(keyEvent->qKeyEvent)) {
	    keyEvent->qKeyEvent->accept();
	    doRepaint = true;
	}
    }

    if (doRepaint) {
	int xpos = 0;
	int ypos = 0;
	absolutePosition(xpos,ypos);
	m_view->updateContents(xpos,ypos,width(),height());
    }

    evt->setDefaultHandled();
}

bool RenderWidget::sendWidgetEvent(QEvent *event)
{
    m_view->setIgnoreEvents(true);
    m_ignorePaintEvents = true;
    QWidget *prevFocusWidget = qApp->focusWidget();
    DocumentImpl *doc = m_view->part()->xmlDocImpl();
    if (doc->focusNode() && doc->focusNode()->renderer() == this)// ### use RenderObject flag
	m_widget->setFocus();

    bool eventHandled = QApplication::sendEvent(m_widget,event);

    if (prevFocusWidget)
	prevFocusWidget->setFocus();
    m_ignorePaintEvents = false;
    m_view->setIgnoreEvents(false);

    return eventHandled;
}

#include "render_replaced.moc"

