/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
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
#include "khtmlview.moc"

#include "khtmlview.h"

#include "khtml_part.h"
#include "khtml_events.h"

#include "html/html_documentimpl.h"
#include "html/html_inlineimpl.h"
#include "html/html_formimpl.h"
#include "rendering/render_object.h"
#include "rendering/render_canvas.h"
#include "rendering/render_style.h"
#include "rendering/render_replaced.h"
#include "xml/dom2_eventsimpl.h"
#include "css/cssstyleselector.h"
#include "misc/htmlhashes.h"
#include "misc/helper.h"
#include "khtml_settings.h"
#include "khtml_printsettings.h"

#include <kcursor.h>
#include <ksimpleconfig.h>
#include <kstandarddirs.h>
#include <kprinter.h>

#include <qtooltip.h>
#include <qpainter.h>
#include <qpaintdevicemetrics.h>
#include <kapplication.h>

#include <kimageio.h>
#include <assert.h>
#include <kdebug.h>
#include <kurldrag.h>
#include <qobjectlist.h>

#define PAINT_BUFFER_HEIGHT 128

using namespace DOM;
using namespace khtml;
class KHTMLToolTip;

#ifndef QT_NO_TOOLTIP

class KHTMLToolTip : public QToolTip
{
public:
    KHTMLToolTip(KHTMLView *view,  KHTMLViewPrivate* vp) : QToolTip(view->viewport())
    {
        m_view = view;
        m_viewprivate = vp;
    };

protected:
    virtual void maybeTip(const QPoint &);

private:
    KHTMLView *m_view;
    KHTMLViewPrivate* m_viewprivate;
};

#endif

class KHTMLViewPrivate {
    friend class KHTMLToolTip;
public:
    KHTMLViewPrivate()
    {
        underMouse = 0;
        reset();
        tp=0;
        paintBuffer=0;
        formCompletions=0;
        prevScrollbarVisible = true;
	timerId = 0;
        repaintTimerId = 0;
        complete = false;
        mousePressed = false;
	tooltip = 0;
    }
    ~KHTMLViewPrivate()
    {
        delete formCompletions;
        delete tp; tp = 0;
        delete paintBuffer; paintBuffer =0;
        
        if (underMouse)
	    underMouse->deref();
	delete tooltip;
    }
    void reset()
    {
        if (underMouse)
	    underMouse->deref();
	underMouse = 0;
        linkPressed = false;
        useSlowRepaints = false;
        originalNode = 0;
	borderTouched = false;
#ifndef KHTML_NO_SCROLLBARS
        vmode = QScrollView::Auto;
        hmode = QScrollView::Auto;
#else
        vmode = QScrollView::AlwaysOff;
        hmode = QScrollView::AlwaysOff;
#endif
        scrollBarMoved = false;
        ignoreWheelEvents = false;
	borderX = 30;
	borderY = 30;
	clickX = -1;
	clickY = -1;
        prevMouseX = -1;
        prevMouseY = -1;
	clickCount = 0;
	isDoubleClick = false;
	scrollingSelf = false;
	timerId = 0;
        repaintTimerId = 0;
        complete = false;
        mousePressed = false;
        firstRelayout = true;
        layoutSchedulingEnabled = true;
        updateRect = QRect();
    }

    QPainter *tp;
    QPixmap  *paintBuffer;
    NodeImpl *underMouse;

    // the node that was selected when enter was pressed
    NodeImpl *originalNode;

    bool borderTouched:1;
    bool borderStart:1;
    bool scrollBarMoved:1;

    QScrollView::ScrollBarMode vmode;
    QScrollView::ScrollBarMode hmode;
    bool prevScrollbarVisible;
    bool linkPressed;
    bool useSlowRepaints;
    bool ignoreWheelEvents;

    int borderX, borderY;
    KSimpleConfig *formCompletions;

    int clickX, clickY, clickCount;
    bool isDoubleClick;

    int prevMouseX, prevMouseY;
    bool scrollingSelf;
    int timerId;

    int repaintTimerId;
    bool complete;
    bool firstRelayout;
    bool layoutSchedulingEnabled;
    bool mousePressed;
    QRect updateRect;
    KHTMLToolTip *tooltip;
};

#ifndef QT_NO_TOOLTIP

void KHTMLToolTip::maybeTip(const QPoint& /*p*/)
{
    DOM::NodeImpl *node = m_viewprivate->underMouse;
    while ( node ) {
        if ( node->isElementNode() ) {
            QString s = static_cast<DOM::ElementImpl*>( node )->getAttribute( ATTR_TITLE ).string();
            if ( !s.isEmpty() ) {
                QRect r( m_view->contentsToViewport( node->getRect().topLeft() ), node->getRect().size() );
                tip( r,  s );
            }
            break;
        }
        node = node->parentNode();
    }
}
#endif

KHTMLView::KHTMLView( KHTMLPart *part, QWidget *parent, const char *name)
    : QScrollView( parent, name, WResizeNoErase | WRepaintNoErase | WPaintUnclipped ),
      _refCount(1)
{
    m_medium = "screen";

    m_layoutObject = 0;
    
    m_part = part;
#if APPLE_CHANGES
    m_part->ref();
#endif
    d = new KHTMLViewPrivate;
    QScrollView::setVScrollBarMode(d->vmode);
    QScrollView::setHScrollBarMode(d->hmode);
    connect(kapp, SIGNAL(kdisplayPaletteChanged()), this, SLOT(slotPaletteChanged()));
    connect(this, SIGNAL(contentsMoving(int, int)), this, SLOT(slotScrollBarMoved()));

    // initialize QScrollview
    enableClipper(true);
    viewport()->setMouseTracking(true);
    viewport()->setBackgroundMode(NoBackground);

    KImageIO::registerFormats();

#ifndef QT_NO_TOOLTIP
    d->tooltip = new KHTMLToolTip( this, d );
#endif

    init();

    viewport()->show();
}

KHTMLView::~KHTMLView()
{
    assert(_refCount == 0);

    if (m_part)
    {
        //WABA: Is this Ok? Do I need to deref it as well?
        //Does this need to be done somewhere else?
        DOM::DocumentImpl *doc = m_part->xmlDocImpl();
        if (doc)
            doc->detach();

#if APPLE_CHANGES
        m_part->deref();
#endif
    }

    delete d; d = 0;
}

void KHTMLView::clearPart()
{
    if (m_part){
        m_part->deref();
        m_part = 0;
    }
}

void KHTMLView::init()
{
#if !APPLE_CHANGES
    if(!d->paintBuffer) d->paintBuffer = new QPixmap(PAINT_BUFFER_HEIGHT, PAINT_BUFFER_HEIGHT);
    if(!d->tp) d->tp = new QPainter();
#endif

    setFocusPolicy(QWidget::StrongFocus);
    viewport()->setFocusPolicy( QWidget::WheelFocus );
    viewport()->setFocusProxy(this);

    _marginWidth = -1; // undefined
    _marginHeight = -1;
    _width = 0;
    _height = 0;

    setAcceptDrops(true);
    
    resizeContents(visibleWidth(), visibleHeight());
}

void KHTMLView::clear()
{


//    viewport()->erase();

    setStaticBackground(false);

    d->reset();
    killTimers();
    emit cleared();

    QScrollView::setHScrollBarMode(d->hmode);
    if (d->vmode==Auto)
        QScrollView::setVScrollBarMode(d->prevScrollbarVisible?AlwaysOn:Auto);
    else
        QScrollView::setVScrollBarMode(d->vmode);

    resizeContents(visibleWidth(), visibleHeight());
}

void KHTMLView::hideEvent(QHideEvent* e)
{
//    viewport()->setBackgroundMode(PaletteBase);
    QScrollView::hideEvent(e);
}

void KHTMLView::showEvent(QShowEvent* e)
{
//    viewport()->setBackgroundMode(NoBackground);
    QScrollView::showEvent(e);
}

void KHTMLView::resizeEvent (QResizeEvent* e)
{
    QScrollView::resizeEvent(e);

#if !APPLE_CHANGES
    int w = visibleWidth();
    int h = visibleHeight();

    layout();

    //  this is to make sure we get the right width even if the scrolbar has dissappeared
    // due to the size change.
    if(visibleHeight() != h || visibleWidth() != w)
        layout();
#endif
    if ( m_part && m_part->xmlDocImpl() )
        m_part->xmlDocImpl()->dispatchWindowEvent( EventImpl::RESIZE_EVENT, false, false );
    KApplication::sendPostedEvents(viewport(), QEvent::Paint);
}

#if !APPLE_CHANGES

// this is to get rid of a compiler virtual overload mismatch warning. do not remove
void KHTMLView::drawContents( QPainter*)
{
}

void KHTMLView::drawContents( QPainter *p, int ex, int ey, int ew, int eh )
{
    //kdDebug( 6000 ) << "drawContents x=" << ex << ",y=" << ey << ",w=" << ew << ",h=" << eh << endl;
    if(!m_part->xmlDocImpl() || !m_part->xmlDocImpl()->renderer()) {
        p->fillRect(ex, ey, ew, eh, palette().normal().brush(QColorGroup::Base));
        return;
    }
    if ( d->paintBuffer->width() < visibleWidth() )
        d->paintBuffer->resize(visibleWidth(),PAINT_BUFFER_HEIGHT);

    int py=0;
    while (py < eh) {
        int ph = eh-py < PAINT_BUFFER_HEIGHT ? eh-py : PAINT_BUFFER_HEIGHT;
        d->tp->begin(d->paintBuffer);
        d->tp->translate(-ex, -ey-py);
        d->tp->fillRect(ex, ey+py, ew, ph, palette().normal().brush(QColorGroup::Base));
        m_part->xmlDocImpl()->renderer()->print(d->tp, ex, ey+py, ew, ph, 0, 0);
#ifdef BOX_DEBUG
	if (m_part->xmlDocImpl()->focusNode())
	{
	    d->tp->setBrush(Qt::NoBrush);
	    d->tp->drawRect(m_part->xmlDocImpl()->focusNode()->getRect());
	}
#endif
        d->tp->end();

        p->drawPixmap(ex, ey+py, *d->paintBuffer, 0, 0, ew, ph);
        py += PAINT_BUFFER_HEIGHT;
    }

    khtml::DrawContentsEvent event( p, ex, ey, ew, eh );
    QApplication::sendEvent( m_part, &event );

}

#endif // !APPLE_CHANGES

void KHTMLView::setMarginWidth(int w)
{
    // make it update the rendering area when set
    _marginWidth = w;
}

void KHTMLView::setMarginHeight(int h)
{
    // make it update the rendering area when set
    _marginHeight = h;
}

void KHTMLView::layout()
{
    if( m_part->xmlDocImpl() ) {
        DOM::DocumentImpl *document = m_part->xmlDocImpl();

        khtml::RenderCanvas* root = static_cast<khtml::RenderCanvas *>(document->renderer());
        if ( !root ) return;

         if (document->isHTMLDocument()) {
             NodeImpl *body = static_cast<HTMLDocumentImpl*>(document)->body();
             if(body && body->renderer() && body->id() == ID_FRAMESET) {
                 QScrollView::setVScrollBarMode(AlwaysOff);
                 QScrollView::setHScrollBarMode(AlwaysOff);
                 body->renderer()->setNeedsLayout(true);
             }
         }

        _height = visibleHeight();
        _width = visibleWidth();

        //QTime qt;
        //qt.start();
        root->setNeedsLayoutAndMinMaxRecalc();
        // avoid recursing into relayouts because of scrollbar-flicker

        root->layout();

        //kdDebug( 6000 ) << "TIME: layout() dt=" << qt.elapsed() << endl;
    } else {
        _width = visibleWidth();
    }
}

//
// Event Handling
//
/////////////////

void KHTMLView::viewportMousePressEvent( QMouseEvent *_mouse )
{
    if(!m_part->xmlDocImpl()) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    //kdDebug( 6000 ) << "\nmousePressEvent: x=" << xm << ", y=" << ym << endl;

    d->isDoubleClick = false;
    d->mousePressed = true;

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MousePress );
    m_part->xmlDocImpl()->prepareMouseEvent( false, xm, ym, &mev );

#if !APPLE_CHANGES
    if (d->clickCount > 0 &&
        QPoint(d->clickX-xm,d->clickY-ym).manhattanLength() <= QApplication::startDragDistance())
	d->clickCount++;
    else {
	d->clickCount = 1;
	d->clickX = xm;
	d->clickY = ym;
    }
#else
    if (KWQ(m_part)->passSubframeEventToSubframe(mev))
        return;

    d->clickX = xm;
    d->clickY = ym;
    d->clickCount = _mouse->clickCount();
#endif    

    bool swallowEvent = dispatchMouseEvent(EventImpl::MOUSEDOWN_EVENT,mev.innerNode.handle(),true,
                                           d->clickCount,_mouse,true,DOM::NodeImpl::MousePress);
    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed();

    if (!swallowEvent) {
	khtml::MousePressEvent event( _mouse, xm, ym, mev.url, mev.target, mev.innerNode );
	QApplication::sendEvent( m_part, &event );
#if APPLE_CHANGES
        // Many AK widgets run their own event loops and consume events while the mouse is down.
        // When they finish, currentEvent is the mouseUp that they exited on.  We need to update
        // the khtml state with this mouseUp, which khtml never saw.
        // If this event isn't a mouseUp, we assume that the mouseUp will be coming later.  There
        // is a hole here if the widget consumes the mouseUp and subsequent events.
        if (KWQ(m_part)->lastEventIsMouseUp()) {
            d->mousePressed = false;
        }
#endif        
	emit m_part->nodeActivated(mev.innerNode);
    }
}

void KHTMLView::viewportMouseDoubleClickEvent( QMouseEvent *_mouse )
{
    if(!m_part->xmlDocImpl()) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    //kdDebug( 6000 ) << "mouseDblClickEvent: x=" << xm << ", y=" << ym << endl;

    d->isDoubleClick = true;
#if APPLE_CHANGES
    // We get this instead of a second mouse-up 
    d->mousePressed = false;
#endif

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MouseDblClick );
    m_part->xmlDocImpl()->prepareMouseEvent( false, xm, ym, &mev );

#if APPLE_CHANGES
    if (KWQ(m_part)->passSubframeEventToSubframe(mev))
        return;

    d->clickCount = _mouse->clickCount();
    bool swallowEvent = dispatchMouseEvent(EventImpl::MOUSEUP_EVENT,mev.innerNode.handle(),true,
                                           d->clickCount,_mouse,false,DOM::NodeImpl::MouseRelease);
    
    dispatchMouseEvent(EventImpl::CLICK_EVENT,mev.innerNode.handle(),true,
			   d->clickCount,_mouse,true,DOM::NodeImpl::MouseRelease);

    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed(false);

    // Qt delivers a release event AND a double click event.
    if (!swallowEvent) {
	khtml::MouseReleaseEvent event1( _mouse, xm, ym, mev.url, mev.target, mev.innerNode );
	QApplication::sendEvent( m_part, &event1 );

	khtml::MouseDoubleClickEvent event2( _mouse, xm, ym, mev.url, mev.target, mev.innerNode );
	QApplication::sendEvent( m_part, &event2 );
    }

#else
    // We do the same thing as viewportMousePressEvent() here, since the DOM does not treat
    // single and double-click events as separate (only the detail, i.e. number of clicks differs)
    // In other words an even detail value for a mouse click event means a double click, and an
    // odd detail value means a single click
    bool swallowEvent = dispatchMouseEvent(EventImpl::MOUSEDOWN_EVENT,mev.innerNode.handle(),true,
                                           d->clickCount,_mouse,true,DOM::NodeImpl::MouseDblClick);

    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed();

    if (!swallowEvent) {
	khtml::MouseDoubleClickEvent event( _mouse, xm, ym, mev.url, mev.target, mev.innerNode );
	QApplication::sendEvent( m_part, &event );

	// ###
	//if ( url.length() )
	//emit doubleClick( url.string(), _mouse->button() );
    }
#endif    
}

static bool isSubmitImage(DOM::NodeImpl *node)
{
    return node
        && node->isHTMLElement()
        && node->id() == ID_INPUT
        && static_cast<HTMLInputElementImpl*>(node)->inputType() == HTMLInputElementImpl::IMAGE;
}

void KHTMLView::viewportMouseMoveEvent( QMouseEvent * _mouse )
{
    if(!m_part->xmlDocImpl()) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MouseMove );
    m_part->xmlDocImpl()->prepareMouseEvent( false, xm, ym, &mev );
#if APPLE_CHANGES
    if (KWQ(m_part)->passSubframeEventToSubframe(mev))
        return;
#endif

    bool swallowEvent = dispatchMouseEvent(EventImpl::MOUSEMOVE_EVENT,mev.innerNode.handle(),false,
                                           0,_mouse,true,DOM::NodeImpl::MouseMove);

    if (d->clickCount > 0 &&
        QPoint(d->clickX-xm,d->clickY-ym).manhattanLength() > QApplication::startDragDistance()) {
	d->clickCount = 0;  // moving the mouse outside the threshold invalidates the click
    }

    // execute the scheduled script. This is to make sure the mouseover events come after the mouseout events
    m_part->executeScheduledScript();

    khtml::RenderStyle* style = (mev.innerNode.handle() && mev.innerNode.handle()->renderer() &&
                                 mev.innerNode.handle()->renderer()->style()) ? mev.innerNode.handle()->renderer()->style() : 0;
    QCursor c;
    if (style && style->cursor() == CURSOR_AUTO && style->cursorImage()
        && !(style->cursorImage()->pixmap().isNull())) {
        /* First of all it works: Check out http://www.iam.unibe.ch/~schlpbch/cursor.html
         *
         * But, I don't know what exactly we have to do here: rescale to 32*32, change to monochrome..
         */
        //kdDebug( 6000 ) << "using custom cursor" << endl;
        const QPixmap p = style->cursorImage()->pixmap();
        // ### fix
        c = QCursor(p);
    }

    switch ( style ? style->cursor() : CURSOR_AUTO) {
    case CURSOR_AUTO:
        if ( d->mousePressed )
            // during selection, use an IBeam no matter what we're over
            c = KCursor::ibeamCursor();
        else if ( (mev.url.length() || isSubmitImage(mev.innerNode.handle()))
                  && m_part->settings()->changeCursor() )
            c = m_part->urlCursor();
        else if ( !mev.innerNode.isNull()
                  && (mev.innerNode.nodeType() == Node::TEXT_NODE
                      || mev.innerNode.nodeType() == Node::CDATA_SECTION_NODE) )
            c = KCursor::ibeamCursor();
        break;
    case CURSOR_CROSS:
        c = KCursor::crossCursor();
        break;
    case CURSOR_POINTER:
        c = m_part->urlCursor();
        break;
    case CURSOR_MOVE:
        c = KCursor::sizeAllCursor();
        break;
    case CURSOR_E_RESIZE:
    case CURSOR_W_RESIZE:
        c = KCursor::sizeHorCursor();
        break;
    case CURSOR_N_RESIZE:
    case CURSOR_S_RESIZE:
        c = KCursor::sizeVerCursor();
        break;
    case CURSOR_NE_RESIZE:
    case CURSOR_SW_RESIZE:
        c = KCursor::sizeBDiagCursor();
        break;
    case CURSOR_NW_RESIZE:
    case CURSOR_SE_RESIZE:
        c = KCursor::sizeFDiagCursor();
        break;
    case CURSOR_TEXT:
        c = KCursor::ibeamCursor();
        break;
    case CURSOR_WAIT:
        c = KCursor::waitCursor();
        break;
    case CURSOR_HELP:
        c = KCursor::whatsThisCursor();
        break;
    case CURSOR_DEFAULT:
        break;
    }

    QWidget *vp = viewport();
    if ( vp->cursor().handle() != c.handle() ) {
        if( c.handle() == KCursor::arrowCursor().handle())
            vp->unsetCursor();
        else
            vp->setCursor( c );
    }
    d->prevMouseX = xm;
    d->prevMouseY = ym;

    if (!swallowEvent) {
        khtml::MouseMoveEvent event( _mouse, xm, ym, mev.url, mev.target, mev.innerNode );
        QApplication::sendEvent( m_part, &event );
    }
}

void KHTMLView::resetCursor()
{
    viewport()->unsetCursor();
}

void KHTMLView::viewportMouseReleaseEvent( QMouseEvent * _mouse )
{
    if ( !m_part->xmlDocImpl() ) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    d->mousePressed = false;

    //kdDebug( 6000 ) << "\nmouseReleaseEvent: x=" << xm << ", y=" << ym << endl;

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MouseRelease );
    m_part->xmlDocImpl()->prepareMouseEvent( false, xm, ym, &mev );
#if APPLE_CHANGES
    if (KWQ(m_part)->passSubframeEventToSubframe(mev))
        return;
#endif

    bool swallowEvent = dispatchMouseEvent(EventImpl::MOUSEUP_EVENT,mev.innerNode.handle(),true,
                                           d->clickCount,_mouse,false,DOM::NodeImpl::MouseRelease);

    if (d->clickCount > 0 &&
        QPoint(d->clickX-xm,d->clickY-ym).manhattanLength() <= QApplication::startDragDistance())
	dispatchMouseEvent(EventImpl::CLICK_EVENT,mev.innerNode.handle(),true,
			   d->clickCount,_mouse,true,DOM::NodeImpl::MouseRelease);

    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed(false);

    if (!swallowEvent) {
	khtml::MouseReleaseEvent event( _mouse, xm, ym, mev.url, mev.target, mev.innerNode );
	QApplication::sendEvent( m_part, &event );
    }
}

void KHTMLView::keyPressEvent( QKeyEvent *_ke )
{

    if (m_part->xmlDocImpl() && m_part->xmlDocImpl()->focusNode()) {
        if (m_part->xmlDocImpl()->focusNode()->dispatchKeyEvent(_ke))
        {
            _ke->accept();
            return;
        }
    }

    int offs = (clipper()->height() < 30) ? clipper()->height() : 30;
    if (_ke->state()&ShiftButton)
      switch(_ke->key())
        {
        case Key_Space:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( 0, -clipper()->height() - offs );
            break;
        }
    else
        switch ( _ke->key() )
        {
        case Key_Down:
        case Key_J:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( 0, 10 );
            break;

        case Key_Space:
        case Key_Next:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( 0, clipper()->height() - offs );
            break;

        case Key_Up:
        case Key_K:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( 0, -10 );
            break;

        case Key_Prior:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( 0, -clipper()->height() + offs );
            break;
        case Key_Right:
        case Key_L:
            if ( d->hmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( 10, 0 );
            break;
        case Key_Left:
        case Key_H:
            if ( d->hmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                scrollBy( -10, 0 );
            break;
        case Key_Enter:
        case Key_Return:
	    // ### FIXME:
	    // move this code to HTMLAnchorElementImpl::setPressed(false),
	    // or even better to HTMLAnchorElementImpl::event()
            if (m_part->xmlDocImpl()) {
		NodeImpl *n = m_part->xmlDocImpl()->focusNode();
		if (n)
		    n->setActive();
		d->originalNode = n;
	    }
            break;
        case Key_Home:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                setContentsPos( 0, 0 );
            break;
        case Key_End:
            if ( d->vmode == QScrollView::AlwaysOff )
                _ke->accept();
            else
                setContentsPos( 0, contentsHeight() - visibleHeight() );
            break;
        default:
	    _ke->ignore();
            return;
        }
    _ke->accept();
}

void KHTMLView::keyReleaseEvent(QKeyEvent *_ke)
{
    if(m_part->xmlDocImpl() && m_part->xmlDocImpl()->focusNode()) {
        // Qt is damn buggy. we receive release events from our child
        // widgets. therefore, do not support keyrelease event on generic
        // nodes for now until we found  a workaround for the Qt bugs. (Dirk)
//         if (m_part->xmlDocImpl()->focusNode()->dispatchKeyEvent(_ke)) {
//             _ke->accept();
//             return;
//         }
//        QScrollView::keyReleaseEvent(_ke);
        Q_UNUSED(_ke);
    }
}

void KHTMLView::contentsContextMenuEvent ( QContextMenuEvent *_ce )
{
// ### what kind of c*** is that ?
#if 0
    if (!m_part->xmlDocImpl()) return;
    int xm = _ce->x();
    int ym = _ce->y();

    DOM::NodeImpl::MouseEvent mev( _ce->state(), DOM::NodeImpl::MouseMove ); // ### not a mouse event!
    m_part->xmlDocImpl()->prepareMouseEvent( xm, ym, &mev );

    NodeImpl *targetNode = mev.innerNode.handle();
    if (targetNode && targetNode->renderer() && targetNode->renderer()->isWidget()) {
        int absx = 0;
        int absy = 0;
        targetNode->renderer()->absolutePosition(absx,absy);
        QPoint pos(xm-absx,ym-absy);

        QWidget *w = static_cast<RenderWidget*>(targetNode->renderer())->widget();
        QContextMenuEvent cme(_ce->reason(),pos,_ce->globalPos(),_ce->state());
        setIgnoreEvents(true);
        QApplication::sendEvent(w,&cme);
        setIgnoreEvents(false);
    }
#endif
}

bool KHTMLView::focusNextPrevChild( bool next )
{
    // Now try to find the next child
    if (m_part->xmlDocImpl()) {
        focusNextPrevNode(next);
        if (m_part->xmlDocImpl()->focusNode() != 0)
            return true; // focus node found
    }

    // If we get here, there is no next/previous child to go to, so pass up to the next/previous child in our parent
    if (m_part->parentPart() && m_part->parentPart()->view()) {
        return m_part->parentPart()->view()->focusNextPrevChild(next);
    }

    return QWidget::focusNextPrevChild(next);
}

void KHTMLView::doAutoScroll()
{
    QPoint pos = QCursor::pos();
    pos = viewport()->mapFromGlobal( pos );

    int xm, ym;
    viewportToContents(pos.x(), pos.y(), xm, ym);

    pos = QPoint(pos.x() - viewport()->x(), pos.y() - viewport()->y());
    if ( (pos.y() < 0) || (pos.y() > visibleHeight()) ||
         (pos.x() < 0) || (pos.x() > visibleWidth()) )
    {
        ensureVisible( xm, ym, 0, 5 );
    }
}

DOM::NodeImpl *KHTMLView::nodeUnderMouse() const
{
    return d->underMouse;
}

bool KHTMLView::scrollTo(const QRect &bounds)
{
    d->scrollingSelf = true; // so scroll events get ignored

    int x, y, xe, ye;
    x = bounds.left();
    y = bounds.top();
    xe = bounds.right();
    ye = bounds.bottom();

    //kdDebug(6000)<<"scrolling coords: x="<<x<<" y="<<y<<" width="<<xe-x<<" height="<<ye-y<<endl;

    int deltax;
    int deltay;

    int curHeight = visibleHeight();
    int curWidth = visibleWidth();

    if (ye-y>curHeight-d->borderY)
	ye  = y + curHeight - d->borderY;

    if (xe-x>curWidth-d->borderX)
	xe = x + curWidth - d->borderX;

    // is xpos of target left of the view's border?
    if (x < contentsX() + d->borderX )
            deltax = x - contentsX() - d->borderX;
    // is xpos of target right of the view's right border?
    else if (xe + d->borderX > contentsX() + curWidth)
            deltax = xe + d->borderX - ( contentsX() + curWidth );
    else
        deltax = 0;

    // is ypos of target above upper border?
    if (y < contentsY() + d->borderY)
            deltay = y - contentsY() - d->borderY;
    // is ypos of target below lower border?
    else if (ye + d->borderY > contentsY() + curHeight)
            deltay = ye + d->borderY - ( contentsY() + curHeight );
    else
        deltay = 0;

    int maxx = curWidth-d->borderX;
    int maxy = curHeight-d->borderY;

    int scrollX,scrollY;

    scrollX = deltax > 0 ? (deltax > maxx ? maxx : deltax) : deltax == 0 ? 0 : (deltax>-maxx ? deltax : -maxx);
    scrollY = deltay > 0 ? (deltay > maxy ? maxy : deltay) : deltay == 0 ? 0 : (deltay>-maxy ? deltay : -maxy);

    if (contentsX() + scrollX < 0)
	scrollX = -contentsX();
    else if (contentsWidth() - visibleWidth() - contentsX() < scrollX)
	scrollX = contentsWidth() - visibleWidth() - contentsX();

    if (contentsY() + scrollY < 0)
	scrollY = -contentsY();
    else if (contentsHeight() - visibleHeight() - contentsY() < scrollY)
	scrollY = contentsHeight() - visibleHeight() - contentsY();

    scrollBy(scrollX, scrollY);



    // generate abs(scroll.)
    if (scrollX<0)
        scrollX=-scrollX;
    if (scrollY<0)
        scrollY=-scrollY;

    d->scrollingSelf = false;

    if ( (scrollX!=maxx) && (scrollY!=maxy) )
	return true;
    else return false;

}

void KHTMLView::focusNextPrevNode(bool next)
{
    // Sets the focus node of the document to be the node after (or if next is false, before) the current focus node.
    // Only nodes that are selectable (i.e. for which isSelectable() returns true) are taken into account, and the order
    // used is that specified in the HTML spec (see DocumentImpl::nextFocusNode() and DocumentImpl::previousFocusNode()
    // for details).

    DocumentImpl *doc = m_part->xmlDocImpl();
    NodeImpl *oldFocusNode = doc->focusNode();
    NodeImpl *newFocusNode;

    // Find the next/previous node from the current one
    if (next)
        newFocusNode = doc->nextFocusNode(oldFocusNode);
    else
        newFocusNode = doc->previousFocusNode(oldFocusNode);

    // If there was previously no focus node and the user has scrolled the document, then instead of picking the first
    // focusable node in the document, use the first one that lies within the visible area (if possible).
    if (!oldFocusNode && newFocusNode && d->scrollBarMoved) {

      kdDebug(6000) << " searching for visible link" << endl;

        bool visible = false;
        NodeImpl *toFocus = newFocusNode;
        while (!visible && toFocus) {
            QRect focusNodeRect = toFocus->getRect();
            if ((focusNodeRect.left() > contentsX()) && (focusNodeRect.right() < contentsX() + visibleWidth()) &&
                (focusNodeRect.top() > contentsY()) && (focusNodeRect.bottom() < contentsY() + visibleHeight())) {
                // toFocus is visible in the contents area
                visible = true;
            }
            else {
                // toFocus is _not_ visible in the contents area, pick the next node
                if (next)
                    toFocus = doc->nextFocusNode(toFocus);
                else
                    toFocus = doc->previousFocusNode(toFocus);
            }
        }

        if (toFocus)
            newFocusNode = toFocus;
    }

    d->scrollBarMoved = false;

    if (!newFocusNode)
      {
        // No new focus node, scroll to bottom or top depending on next
        if (next)
            scrollTo(QRect(contentsX()+visibleWidth()/2,contentsHeight(),0,0));
        else
            scrollTo(QRect(contentsX()+visibleWidth()/2,0,0,0));
    }
    else
    // Scroll the view as necessary to ensure that the new focus node is visible
    {
      if (oldFocusNode)
	{
	  if (!scrollTo(newFocusNode->getRect()))
	    return;
	}
      else
	{
	  ensureVisible(contentsX(), next?0:contentsHeight());
	  //return;
	}
    }
    // Set focus node on the document
    m_part->xmlDocImpl()->setFocusNode(newFocusNode);
    emit m_part->nodeActivated(Node(newFocusNode));

#if 0
    if (newFocusNode) {

        // this does not belong here. it should run a query on the tree (Dirk)
        // I'll fix this very particular part of the code soon when I cleaned
        // up the positioning code
        // If the newly focussed node is a link, notify the part

        HTMLAnchorElementImpl *anchor = 0;
        if ((newFocusNode->id() == ID_A || newFocusNode->id() == ID_AREA))
            anchor = static_cast<HTMLAnchorElementImpl *>(newFocusNode);

        if (anchor && !anchor->areaHref().isNull())
            m_part->overURL(anchor->areaHref().string(), 0);
        else
            m_part->overURL(QString(), 0);
    }
#endif
}

void KHTMLView::setMediaType( const QString &medium )
{
    m_medium = medium;
}

QString KHTMLView::mediaType() const
{
    return m_medium;
}

#if !APPLE_CHANGES

void KHTMLView::print()
{
    if(!m_part->xmlDocImpl()) return;
    khtml::RenderCanvas *root = static_cast<khtml::RenderCanvas *>(m_part->xmlDocImpl()->renderer());
    if(!root) return;

    // this only works on Unix - we assume 72dpi
    KPrinter *printer = new KPrinter(QPrinter::PrinterResolution);
    printer->addDialogPage(new KHTMLPrintSettings());
    if(printer->setup(this)) {
        viewport()->setCursor( waitCursor ); // only viewport(), no QApplication::, otherwise we get the busy cursor in kdeprint's dialogs
        // set up KPrinter
        printer->setFullPage(false);
        printer->setCreator("KDE 3.0 HTML Library");
        QString docname = m_part->xmlDocImpl()->URL();
        if ( !docname.isEmpty() )
	    printer->setDocName(docname);

        QPainter *p = new QPainter;
        p->begin( printer );
        khtml::setPrintPainter( p );

        m_part->xmlDocImpl()->setPaintDevice( printer );
        QString oldMediaType = mediaType();
        setMediaType( "print" );
        // We ignore margin settings for html and body when printing
        // and use the default margins from the print-system
        // (In Qt 3.0.x the default margins are hardcoded in Qt)
        m_part->xmlDocImpl()->setPrintStyleSheet( printer->option("kde-khtml-printfriendly") == "true" ?
                                                  "* { background-image: none !important;"
                                                  "    background-color: white !important;"
                                                  "    color: black !important; }"
                                                  "body { margin: 0px !important; }"
                                                  "html { margin: 0px !important; }" :
                                                  "body { margin: 0px !important; }"
                                                  "html { margin: 0px !important; }"
                                                  );


        QPaintDeviceMetrics metrics( printer );

        // this is a simple approximation... we layout the document
        // according to the width of the page, then just cut
        // pages without caring about the content. We should do better
        // in the future, but for the moment this is better than no
        // printing support
        kdDebug(6000) << "printing: physical page width = " << metrics.width()
                      << " height = " << metrics.height() << endl;
        root->setPrintingMode(true);
        root->setWidth(metrics.width());

        m_part->xmlDocImpl()->styleSelector()->computeFontSizes(&metrics, 100);
        m_part->xmlDocImpl()->updateStyleSelector();
        root->setPrintImages( printer->option("kde-khtml-printimages") == "true");
        root->setNeedsLayoutAndMinMaxRecalc();
        root->layout();

        // ok. now print the pages.
        kdDebug(6000) << "printing: html page width = " << root->docWidth()
                      << " height = " << root->docHeight() << endl;
        kdDebug(6000) << "printing: margins left = " << printer->margins().width()
                      << " top = " << printer->margins().height() << endl;
        kdDebug(6000) << "printing: paper width = " << metrics.width()
                      << " height = " << metrics.height() << endl;
        // if the width is too large to fit on the paper we just scale
        // the whole thing.
        int pageHeight = metrics.height();
        int pageWidth = metrics.width();
        p->setClipRect(0,0, pageWidth, pageHeight);
        if(root->docWidth() > metrics.width()) {
            double scale = ((double) metrics.width())/((double) root->docWidth());
#ifndef QT_NO_TRANSFORMATIONS
            p->scale(scale, scale);
#endif
            pageHeight = (int) (pageHeight/scale);
            pageWidth = (int) (pageWidth/scale);
        }
        kdDebug(6000) << "printing: scaled html width = " << pageWidth
                      << " height = " << pageHeight << endl;
        int top = 0;
        while(top < root->docHeight()) {
            if(top > 0) printer->newPage();
            root->setTruncatedAt(top+pageHeight);

            root->print(p, 0, top, pageWidth, pageHeight, 0, 0);
            if (top + pageHeight >= root->docHeight())
                break; // Stop if we have printed everything

            p->translate(0, top - root->truncatedAt());
            top = root->truncatedAt();
        }

        p->end();
        delete p;

        // and now reset the layout to the usual one...
        root->setPrintingMode(false);
        khtml::setPrintPainter( 0 );
        setMediaType( oldMediaType );
        m_part->xmlDocImpl()->setPaintDevice( this );
        m_part->xmlDocImpl()->styleSelector()->computeFontSizes(m_part->xmlDocImpl()->paintDeviceMetrics(), m_part->zoomFactor());
        m_part->xmlDocImpl()->updateStyleSelector();
        viewport()->unsetCursor();
    }
    delete printer;
}

void KHTMLView::slotPaletteChanged()
{
    if(!m_part->xmlDocImpl()) return;
    DOM::DocumentImpl *document = m_part->xmlDocImpl();
    if (!document->isHTMLDocument()) return;
    khtml::RenderCanvas *root = static_cast<khtml::RenderCanvas *>(document->renderer());
    if(!root) return;
    root->style()->resetPalette();
    NodeImpl *body = static_cast<HTMLDocumentImpl*>(document)->body();
    if(!body) return;
    body->setChanged(true);
    body->recalcStyle( NodeImpl::Force );
}

void KHTMLView::paint(QPainter *p, const QRect &rc, int yOff, bool *more)
{
    if(!m_part->xmlDocImpl()) return;
    khtml::RenderCanvas *root = static_cast<khtml::RenderCanvas *>(m_part->xmlDocImpl()->renderer());
    if(!root) return;

    m_part->xmlDocImpl()->setPaintDevice(p->device());
    root->setPrintingMode(true);
    root->setWidth(rc.width());

    p->save();
    p->setClipRect(rc);
    p->translate(rc.left(), rc.top());
    double scale = ((double) rc.width()/(double) root->docWidth());
    int height = (int) ((double) rc.height() / scale);
#ifndef QT_NO_TRANSFORMATIONS
    p->scale(scale, scale);
#endif

    root->print(p, 0, yOff, root->docWidth(), height, 0, 0);
    if (more)
        *more = yOff + height < root->docHeight();
    p->restore();

    root->setPrintingMode(false);
    m_part->xmlDocImpl()->setPaintDevice( this );
}

#endif // !APPLE_CHANGES

void KHTMLView::useSlowRepaints()
{
    kdDebug(0) << "slow repaints requested" << endl;
    d->useSlowRepaints = true;
    setStaticBackground(true);
}


void KHTMLView::setVScrollBarMode ( ScrollBarMode mode )
{
#ifndef KHTML_NO_SCROLLBARS
    d->vmode = mode;
    QScrollView::setVScrollBarMode(mode);
#else
    Q_UNUSED( mode );
#endif
}

void KHTMLView::setHScrollBarMode ( ScrollBarMode mode )
{
#ifndef KHTML_NO_SCROLLBARS
    d->hmode = mode;
    QScrollView::setHScrollBarMode(mode);
#else
    Q_UNUSED( mode );
#endif
}

void KHTMLView::restoreScrollBar ( )
{
    int ow = visibleWidth();
    QScrollView::setVScrollBarMode(d->vmode);
    if (visibleWidth() != ow)
    {
        layout();
//        scheduleRepaint(contentsX(),contentsY(),visibleWidth(),visibleHeight());
    }
#if !APPLE_CHANGES
    d->prevScrollbarVisible = verticalScrollBar()->isVisible();
#endif
}

QStringList KHTMLView::formCompletionItems(const QString &name) const
{
    if (!m_part->settings()->isFormCompletionEnabled())
        return QStringList();
    if (!d->formCompletions)
        d->formCompletions = new KSimpleConfig(locateLocal("data", "khtml/formcompletions"));
    return d->formCompletions->readListEntry(name);
}

void KHTMLView::addFormCompletionItem(const QString &name, const QString &value)
{
    if (!m_part->settings()->isFormCompletionEnabled())
        return;
    // don't store values that are all numbers or just numbers with
    // dashes or spaces as those are likely credit card numbers or
    // something similar
    bool cc_number(true);
    for (unsigned int i = 0; i < value.length(); ++i)
    {
      QChar c(value[i]);
      if (!c.isNumber() && c != '-' && !c.isSpace())
      {
        cc_number = false;
        break;
      }
    }
    if (cc_number)
      return;
    QStringList items = formCompletionItems(name);
    if (!items.contains(value))
        items.prepend(value);
    while ((int)items.count() > m_part->settings()->maxFormCompletionItems())
        items.remove(items.fromLast());
    d->formCompletions->writeEntry(name, items);
}

bool KHTMLView::dispatchMouseEvent(int eventId, DOM::NodeImpl *targetNode, bool cancelable,
				   int detail,QMouseEvent *_mouse, bool setUnder,
				   int mouseEventType)
{
    if (d->underMouse)
	d->underMouse->deref();
    d->underMouse = targetNode;
    if (d->underMouse)
	d->underMouse->ref();

    int exceptioncode = 0;
    int clientX, clientY;
    viewportToContents(_mouse->x(), _mouse->y(), clientX, clientY);
    int screenX = _mouse->globalX();
    int screenY = _mouse->globalY();
    int button = -1;
    switch (_mouse->button()) {
	case LeftButton:
	    button = 0;
	    break;
	case MidButton:
	    button = 1;
	    break;
	case RightButton:
	    button = 2;
	    break;
	default:
	    break;
    }
    bool ctrlKey = (_mouse->state() & ControlButton);
    bool altKey = (_mouse->state() & AltButton);
    bool shiftKey = (_mouse->state() & ShiftButton);
    bool metaKey = (_mouse->state() & MetaButton);

    // mouseout/mouseover
    if (setUnder && (d->prevMouseX != clientX || d->prevMouseY != clientY)) {

        // ### this code sucks. we should save the oldUnder instead of calculating
        // it again. calculating is expensive! (Dirk)
        NodeImpl *oldUnder = 0;
	if (d->prevMouseX >= 0 && d->prevMouseY >= 0) {
	    NodeImpl::MouseEvent mev( _mouse->stateAfter(), static_cast<NodeImpl::MouseEventType>(mouseEventType));
	    m_part->xmlDocImpl()->prepareMouseEvent( true, d->prevMouseX, d->prevMouseY, &mev );
	    oldUnder = mev.innerNode.handle();
	}
	if (oldUnder != targetNode) {
	    // send mouseout event to the old node
	    if (oldUnder){
		oldUnder->ref();
		MouseEventImpl *me = new MouseEventImpl(EventImpl::MOUSEOUT_EVENT,
							true,true,m_part->xmlDocImpl()->defaultView(),
							0,screenX,screenY,clientX,clientY,
							ctrlKey,altKey,shiftKey,metaKey,
							button,targetNode);
		me->ref();
		oldUnder->dispatchEvent(me,exceptioncode,true);
		me->deref();
	    }

	    // send mouseover event to the new node
	    if (targetNode) {
		MouseEventImpl *me = new MouseEventImpl(EventImpl::MOUSEOVER_EVENT,
							true,true,m_part->xmlDocImpl()->defaultView(),
							0,screenX,screenY,clientX,clientY,
							ctrlKey,altKey,shiftKey,metaKey,
							button,oldUnder);

		me->ref();
		targetNode->dispatchEvent(me,exceptioncode,true);
		me->deref();
	    }

            if (oldUnder)
                oldUnder->deref();
        }
    }

    bool swallowEvent = false;

    if (targetNode) {
	// send the actual event
	MouseEventImpl *me = new MouseEventImpl(static_cast<EventImpl::EventId>(eventId),
						true,cancelable,m_part->xmlDocImpl()->defaultView(),
						detail,screenX,screenY,clientX,clientY,
						ctrlKey,altKey,shiftKey,metaKey,
						button,0);
	me->ref();
	targetNode->dispatchEvent(me,exceptioncode,true);
	bool defaultHandled = me->defaultHandled();
        if (me->defaultHandled() || me->defaultPrevented())
            swallowEvent = true;
	me->deref();

	// Special case: If it's a click event, we also send the KHTML_CLICK or KHTML_DBLCLICK event. This is not part
	// of the DOM specs, but is used for compatibility with the traditional onclick="" and ondblclick="" attributes,
	// as there is no way to tell the difference between single & double clicks using DOM (only the click count is
	// stored, which is not necessarily the same)
	if (eventId == EventImpl::CLICK_EVENT) {
	    me = new MouseEventImpl(d->isDoubleClick ? EventImpl::KHTML_DBLCLICK_EVENT : EventImpl::KHTML_CLICK_EVENT,
				    true,cancelable,m_part->xmlDocImpl()->defaultView(),
				    detail,screenX,screenY,clientX,clientY,
				    ctrlKey,altKey,shiftKey,metaKey,
				    button,0);

	    me->ref();
	    if (defaultHandled)
		me->setDefaultHandled();
	    targetNode->dispatchEvent(me,exceptioncode,true);
            if (me->defaultHandled() || me->defaultPrevented())
                swallowEvent = true;
	    me->deref();
        }
        else if (eventId == EventImpl::MOUSEDOWN_EVENT) {
            // Focus should be shifted on mouse down, not on a click.  -dwh
            if (targetNode->isSelectable())
                m_part->xmlDocImpl()->setFocusNode(targetNode);
#if !APPLE_CHANGES
            // In Safari we don't want to take the focus away from a text field
            // when we click on, say, a form button. But I suspect this could be
            // something specific to Macintosh, so I am leaving this code here inside
            // !APPLE_CHANGES. This will probably change when we do more of the keyboard
            // navigation work.
            else
                m_part->xmlDocImpl()->setFocusNode(0);
#endif
        }
    }

    return swallowEvent;
}

void KHTMLView::setIgnoreWheelEvents( bool e )
{
    d->ignoreWheelEvents = e;
}

#ifndef QT_NO_WHEELEVENT

void KHTMLView::viewportWheelEvent(QWheelEvent* e)
{
#if !APPLE_CHANGES
    if ( d->ignoreWheelEvents && !verticalScrollBar()->isVisible() && m_part->parentPart() ) {
        if ( m_part->parentPart()->view() )
            m_part->parentPart()->view()->wheelEvent( e );
        e->ignore();
    }
    else if ( d->vmode == QScrollView::AlwaysOff ) {
        e->accept();
    }
    else {
        d->scrollBarMoved = true;
        QScrollView::viewportWheelEvent( e );
    }
#endif
}
#endif

#if !APPLE_CHANGES
void KHTMLView::dragEnterEvent( QDragEnterEvent* ev )
{
    // Handle drops onto frames (#16820)
    // Drops on the main html part is handled by Konqueror (and shouldn't do anything
    // in e.g. kmail, so not handled here).
    if ( m_part->parentPart() )
    {
        // Duplicated from KonqView::eventFilter
        if ( QUriDrag::canDecode( ev ) )
        {
            KURL::List lstDragURLs;
            bool ok = KURLDrag::decode( ev, lstDragURLs );
            QObjectList *children = this->queryList( "QWidget" );

            if ( ok &&
                 !lstDragURLs.first().url().contains( "javascript:", false ) && // ### this looks like a hack to me
                 ev->source() != this &&
                 children &&
                 children->findRef( ev->source() ) == -1 )
                ev->acceptAction();

            delete children;
        }
    }
    QScrollView::dragEnterEvent( ev );
}

void KHTMLView::dropEvent( QDropEvent *ev )
{
    // Handle drops onto frames (#16820)
    // Drops on the main html part is handled by Konqueror (and shouldn't do anything
    // in e.g. kmail, so not handled here).
    if ( m_part->parentPart() )
    {
        KURL::List lstDragURLs;
        bool ok = KURLDrag::decode( ev, lstDragURLs );

        KHTMLPart* part = m_part->parentPart();
        while ( part && part->parentPart() )
            part = part->parentPart();
        KParts::BrowserExtension *ext = part->browserExtension();
        if ( ok && ext && lstDragURLs.first().isValid() )
            emit ext->openURLRequest( lstDragURLs.first() );
    }
    QScrollView::dropEvent( ev );
}
#endif // !APPLE_CHANGES

void KHTMLView::focusOutEvent( QFocusEvent *e )
{
    m_part->stopAutoScroll();
    QScrollView::focusOutEvent( e );
}

void KHTMLView::slotScrollBarMoved()
{
    if (!d->scrollingSelf)
        d->scrollBarMoved = true;
}

void KHTMLView::timerEvent ( QTimerEvent *e )
{
//    kdDebug() << "timer event " << e->timerId() << endl;
    if (e->timerId()==d->timerId)
    {
        d->firstRelayout = false;
        killTimer(d->timerId);

        d->layoutSchedulingEnabled=false;
        layout();
        d->layoutSchedulingEnabled=true;

        d->timerId = 0;

        //scheduleRepaint(contentsX(),contentsY(),visibleWidth(),visibleHeight());
        d->updateRect = QRect(contentsX(),contentsY(),visibleWidth(),visibleHeight());
    }

    if( m_part->xmlDocImpl() ) {
        DOM::DocumentImpl *document = m_part->xmlDocImpl();
        khtml::RenderCanvas* root = static_cast<khtml::RenderCanvas *>(document->renderer());
        if (root){
            // Do not allow a full layout if we had a clip object set.
            if ( root->needsLayout() && !m_layoutObject) {
                killTimer(d->repaintTimerId);
                d->repaintTimerId = 0;
                //qDebug("needs layout, delaying repaint");
                scheduleRelayout();
                return;
            }
            resizeContents(root->docWidth(), root->docHeight());
        }
    }
    setStaticBackground(d->useSlowRepaints);

//        kdDebug() << "scheduled repaint "<< d->repaintTimerId  << endl;
    // If we have an overflow:hidden object, we cache the current update rect and then
    // clear the update rect, so that when we call repaint(), the union of the layout
    // object's rect and the current update rect is just the layout object's' rect.
    QRect oldUpdateRect = d->updateRect;
    bool hasRepaintTimer = d->repaintTimerId;
    if (m_layoutObject) {
        d->updateRect = QRect();
        m_layoutObject->repaint();
    }
    
    // Now we paint.  This will be either a full paint or just the paint of a clipped
    // object's area.
    updateContents( d->updateRect );
    killTimer(d->repaintTimerId);
    d->repaintTimerId = 0;

    if (m_layoutObject) {
        // Restore the update rect, and if we had a repaint pending before the clipped
        // object did its paint, go ahead and reschedule that paint to occur as soon
        // as possible.
        d->updateRect = oldUpdateRect;
        if (hasRepaintTimer)
            scheduleRepaint(d->updateRect.x(), d->updateRect.y(), d->updateRect.width(),
                            d->updateRect.height());
        m_layoutObject = 0;
    }
}

void KHTMLView::scheduleRelayout(khtml::RenderObject* clippedObj)
{
    if (!d->layoutSchedulingEnabled)
        return;
        
    if (m_layoutObject != clippedObj)
        m_layoutObject = 0;
    
    if (d->timerId)
        return;

    m_layoutObject = clippedObj;
    
    bool parsing = false;
    if( m_part->xmlDocImpl() ) {
        parsing = m_part->xmlDocImpl()->parsing();
    }

    d->timerId = startTimer( parsing ? 1000 : 0 );
}

void KHTMLView::unscheduleRelayout()
{
    if (!d->timerId)
        return;

    killTimer(d->timerId);
    d->timerId = 0;
}

void KHTMLView::unscheduleRepaint()
{
    if (!d->repaintTimerId)
        return;

    killTimer(d->repaintTimerId);
    d->repaintTimerId = 0;
}

void KHTMLView::scheduleRepaint(int x, int y, int w, int h)
{

    //kdDebug() << "scheduleRepaint(" << x << "," << y << "," << w << "," << h << ")" << endl;


    bool parsing = false;
    if( m_part->xmlDocImpl() ) {
        parsing = m_part->xmlDocImpl()->parsing();
    }

//     kdDebug() << "parsing " << parsing << endl;
//     kdDebug() << "complete " << d->complete << endl;

    int time;

    // if complete...
    if (d->complete)
        // ...repaint immediatly
        time = 0;
    else
    {
        if (parsing)
            // not complete and still parsing
            time = 300;
        else
            // not complete, not parsing, extend the timer if it exists
            // otherwise, repaint immediatly
            time = d->repaintTimerId ? 400 : 0;
    }

    if (d->repaintTimerId) {
        killTimer(d->repaintTimerId);
        d->updateRect = d->updateRect.unite(QRect(x,y,w,h));
    } else
        d->updateRect = QRect(x,y,w,h);

    d->repaintTimerId = startTimer( time );

//     kdDebug() << "starting timer " << time << endl;
}


void KHTMLView::complete()
{
//     kdDebug() << "KHTMLView::complete()" << endl;

    d->complete = true;

    // is there a relayout pending?
    if (d->timerId)
    {
//         kdDebug() << "requesting relayout now" << endl;
        // do it now
        killTimer(d->timerId);
        d->timerId = startTimer( 0 );
    }

    // is there a repaint pending?
    if (d->repaintTimerId)
    {
//         kdDebug() << "requesting repaint now" << endl;
        // do it now
        killTimer(d->repaintTimerId);
        d->repaintTimerId = startTimer( 1 );
    }
}
