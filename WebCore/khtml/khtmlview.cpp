/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
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

#include "khtml_part.h"
#include "khtml_events.h"

#include "html/html_documentimpl.h"
#include "html/html_inlineimpl.h"
#include "rendering/render_object.h"
#include "rendering/render_root.h"
#include "rendering/render_style.h"
#include "xml/dom2_eventsimpl.h"
#include "misc/htmlhashes.h"
#include "misc/helper.h"
#include "khtml_settings.h"

#include <kcursor.h>
#include <ksimpleconfig.h>
#include <kstddirs.h>
#include <kprinter.h>

#include <qpixmap.h>
#include <qtooltip.h>
#include <qstring.h>
#include <qpainter.h>
#include <qpalette.h>
#include <qevent.h>
#include <qdatetime.h>
#include <qpaintdevicemetrics.h>
#include <qtimer.h>
#include <kapp.h>

#include <kimageio.h>
#include <kdebug.h>

#define PAINT_BUFFER_HEIGHT 128

template class QList<KHTMLView>;

QList<KHTMLView> *KHTMLView::lstViews = 0L;

using namespace DOM;
using namespace khtml;
class KHTMLToolTip;

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
    }
    ~KHTMLViewPrivate()
    {
        delete formCompletions;
        delete tp; tp = 0;
        delete paintBuffer; paintBuffer =0;
        if (underMouse)
	    underMouse->deref();
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
	borderX = 30;
	borderY = 30;
	clickX = -1;
	clickY = -1;
	prevMouseX = -1;
	prevMouseY = -1;
	clickCount = 0;
	isDoubleClick = false;
    }

    QPainter *tp;
    QPixmap  *paintBuffer;
    NodeImpl *underMouse;

    // the node that was selected when enter was pressed
    ElementImpl *originalNode;

    bool borderTouched;
    bool borderStart;

    QScrollView::ScrollBarMode vmode;
    QScrollView::ScrollBarMode hmode;
    bool prevScrollbarVisible;
    bool linkPressed;
    bool useSlowRepaints;

    int borderX, borderY;
    KSimpleConfig *formCompletions;

    int clickX, clickY, clickCount;
    bool isDoubleClick;

    int prevMouseX, prevMouseY;
};

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
    : QScrollView( parent, name, WResizeNoErase | WRepaintNoErase )
{
    m_part = part;
    d = new KHTMLViewPrivate;
    QScrollView::setVScrollBarMode(d->vmode);
    QScrollView::setHScrollBarMode(d->hmode);
    connect(kapp, SIGNAL(kdisplayPaletteChanged()), this, SLOT(slotPaletteChanged()));

    // initialize QScrollview
    enableClipper(true);
    viewport()->setMouseTracking(true);
    viewport()->setBackgroundMode(NoBackground);

    KImageIO::registerFormats();

    viewport()->setCursor(arrowCursor);
#ifndef QT_NO_TOOLTIP
    ( void ) new KHTMLToolTip( this, d );
#endif

    init();

    viewport()->show();
}

KHTMLView::~KHTMLView()
{
    if (m_part)
    {
        //WABA: Is this Ok? Do I need to deref it as well?
        //Does this need to be done somewhere else?
        DOM::DocumentImpl *doc = m_part->xmlDocImpl();
        if (doc)
            doc->detach();
    }
    lstViews->removeRef( this );
    if(lstViews->isEmpty())
    {
        delete lstViews;
        lstViews = 0;
    }

    delete d; d = 0;
}

void KHTMLView::init()
{
    if ( lstViews == 0L )
        lstViews = new QList<KHTMLView>;
    lstViews->setAutoDelete( FALSE );
    lstViews->append( this );

    if(!d->paintBuffer) d->paintBuffer = new QPixmap(PAINT_BUFFER_HEIGHT, PAINT_BUFFER_HEIGHT);
   if(!d->tp) d->tp = new QPainter();

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
    viewport()->erase();

    if(d->useSlowRepaints)
        setStaticBackground(false);

    d->reset();
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

    int w = visibleWidth();
    int h = visibleHeight();

    layout();

    //  this is to make sure we get the right width even if the scrolbar has dissappeared
    // due to the size change.
    if(visibleHeight() != h || visibleWidth() != w)
        layout();

    KApplication::sendPostedEvents(viewport(), QEvent::Paint);
}

void KHTMLView::drawContents( QPainter *p, int ex, int ey, int ew, int eh )
{
    if(!m_part->xmlDocImpl()) {
        p->fillRect(ex, ey, ew, eh, palette().normal().brush(QColorGroup::Base));
        return;
    }

    //kdDebug( 6000 ) << "drawContents x=" << ex << ",y=" << ey << ",w=" << ew << ",h=" << eh << endl;

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

void KHTMLView::layout(bool)
{
    //### take care of frmaes (hide scrollbars,...)
    if( m_part->xmlDocImpl() ) {
        DOM::DocumentImpl *document = m_part->xmlDocImpl();

        khtml::RenderRoot* root = static_cast<khtml::RenderRoot *>(document->renderer());

        if (document->isHTMLDocument()) {
            NodeImpl *body = static_cast<HTMLDocumentImpl*>(document)->body();
            if(body && body->renderer() && body->id() == ID_FRAMESET) {
                QScrollView::setVScrollBarMode(AlwaysOff);
                QScrollView::setHScrollBarMode(AlwaysOff);
                _width = visibleWidth();
                body->renderer()->setLayouted(false);
                body->renderer()->layout();
                root->layout();
                return;
            }
        }

        _height = visibleHeight();
        _width = visibleWidth();

        //QTime qt;
        //qt.start();
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


    // Make this frame the active one
    // ### need some visual indication for the active frame.
    /* ### use PartManager (Simon)
       if ( _isFrame && !_isSelected )
       {
        kdDebug( 6000 ) << "activating frame!" << endl;
        topView()->setFrameSelected(this);
    }*/

    d->isDoubleClick = false;

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MousePress );
    m_part->xmlDocImpl()->prepareMouseEvent( xm, ym, 0, 0, &mev );

    if (d->clickCount > 0 && d->clickX == xm && d->clickY == ym) // ### support mouse threshold
	d->clickCount++;
    else {
	d->clickCount = 1;
	d->clickX = xm;
	d->clickY = ym;
    }

    dispatchMouseEvent(EventImpl::MOUSEDOWN_EVENT,mev.innerNode.handle(),true,
		       d->clickCount,_mouse,true,DOM::NodeImpl::MousePress);
    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed();

    khtml::MousePressEvent event( _mouse, xm, ym, mev.url, mev.innerNode );
    event.setNodePos( mev.nodeAbsX, mev.nodeAbsY );
    QApplication::sendEvent( m_part, &event );

    emit m_part->nodeActivated(mev.innerNode);
}

void KHTMLView::viewportMouseDoubleClickEvent( QMouseEvent *_mouse )
{
    if(!m_part->xmlDocImpl()) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    //kdDebug( 6000 ) << "mouseDblClickEvent: x=" << xm << ", y=" << ym << endl;

    d->isDoubleClick = true;

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MouseDblClick );
    m_part->xmlDocImpl()->prepareMouseEvent( xm, ym, 0, 0, &mev );

    // We do the same thing as viewportMousePressEvent() here, since the DOM does not treat
    // single and double-click events as separate (only the detail, i.e. number of clicks differs)
    // In other words an even detail value for a mouse click event means a double click, and an
    // odd detail value means a single click
    if (d->clickCount > 0 && d->clickX == xm && d->clickY == ym) // ### support mouse threshold
	d->clickCount++;
    else {
	d->clickCount = 1;
	d->clickX = xm;
	d->clickY = ym;
    }
    dispatchMouseEvent(EventImpl::MOUSEDOWN_EVENT,mev.innerNode.handle(),true,
		       d->clickCount,_mouse,true,DOM::NodeImpl::MouseDblClick);

    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed();

    khtml::MouseDoubleClickEvent event( _mouse, xm, ym, mev.url, mev.innerNode );
    event.setNodePos( mev.nodeAbsX, mev.nodeAbsY );
    QApplication::sendEvent( m_part, &event );

    // ###
    //if ( url.length() )
    //emit doubleClick( url.string(), _mouse->button() );
}

void KHTMLView::viewportMouseMoveEvent( QMouseEvent * _mouse )
{
    if(!m_part->xmlDocImpl()) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MouseMove );
    m_part->xmlDocImpl()->prepareMouseEvent( xm, ym, 0, 0, &mev );
    dispatchMouseEvent(EventImpl::MOUSEMOVE_EVENT,mev.innerNode.handle(),false,
		       0,_mouse,true,DOM::NodeImpl::MouseMove);

    if (d->clickCount > 0 &&
        QPoint(d->clickX-xm,d->clickY-ym).manhattanLength() > QApplication::startDragDistance()) {
	d->clickCount = 0;  // moving the mouse outside the threshold invalidates the click
    }

    // execute the scheduled script. This is to make sure the mouseover events come after the mouseout events
    m_part->executeScheduledScript();

    QCursor c = KCursor::arrowCursor();
    if ( !mev.innerNode.isNull() && mev.innerNode.handle()->style() ) {
      khtml::RenderStyle* style = mev.innerNode.handle()->style();
      if ((style->cursor() == CURSOR_AUTO) && (style->cursorImage())
	    && !(style->cursorImage()->pixmap().isNull())) {
        /* First of all it works: Check out http://www.iam.unibe.ch/~schlpbch/cursor.html
	*
	* But, I don't know what exactly we have to do here: rescale to 32*32, change to monochrome..
	*/
    	//kdDebug( 6000 ) << "using custom cursor" << endl;
	const QPixmap p = style->cursorImage()->pixmap();
	// ### fix
	c = QCursor(p);
      } else {
        switch ( style->cursor() ) {
        case CURSOR_AUTO:
            if ( mev.url.length() && const_cast<KHTMLSettings *>(m_part->settings())->changeCursor() )
                c = m_part->urlCursor();
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
        case CURSOR_DEFAULT:
            break;
        }
      }
    }
    viewport()->setCursor( c );

    d->prevMouseX = xm;
    d->prevMouseY = ym;

    khtml::MouseMoveEvent event( _mouse, xm, ym, mev.url, mev.innerNode );
    event.setNodePos( mev.nodeAbsX, mev.nodeAbsY );
    QApplication::sendEvent( m_part, &event );
}

void KHTMLView::viewportMouseReleaseEvent( QMouseEvent * _mouse )
{
    if ( !m_part->xmlDocImpl() ) return;

    int xm, ym;
    viewportToContents(_mouse->x(), _mouse->y(), xm, ym);

    //kdDebug( 6000 ) << "\nmouseReleaseEvent: x=" << xm << ", y=" << ym << endl;

    DOM::NodeImpl::MouseEvent mev( _mouse->stateAfter(), DOM::NodeImpl::MouseRelease );
    m_part->xmlDocImpl()->prepareMouseEvent( xm, ym, 0, 0, &mev );

    dispatchMouseEvent(EventImpl::MOUSEUP_EVENT,mev.innerNode.handle(),true,
		       d->clickCount,_mouse,false,DOM::NodeImpl::MouseRelease);

    if (d->clickCount > 0 &&
        QPoint(d->clickX-xm,d->clickY-ym).manhattanLength() <= QApplication::startDragDistance())
	dispatchMouseEvent(EventImpl::CLICK_EVENT,mev.innerNode.handle(),true,
			   d->clickCount,_mouse,true,DOM::NodeImpl::MouseRelease);

    if (mev.innerNode.handle())
	mev.innerNode.handle()->setPressed(false);

    khtml::MouseReleaseEvent event( _mouse, xm, ym, mev.url, mev.innerNode );
    event.setNodePos( mev.nodeAbsX, mev.nodeAbsY );
    QApplication::sendEvent( m_part, &event );
}

void KHTMLView::keyPressEvent( QKeyEvent *_ke )
{
//    if(m_part->keyPressHook(_ke)) return;

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
		ElementImpl *e = m_part->xmlDocImpl()->focusNode();
		if (e)
		    e->setActive();
		d->originalNode = e;
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

void KHTMLView::keyReleaseEvent( QKeyEvent *_ke )
{
    switch(_ke->key())
    {
    case Key_Enter:
    case Key_Return:
	// ### FIXME:
	// move this code to HTMLAnchorElementImpl::setPressed(false),
	// or even better to HTMLAnchorElementImpl::event()
	if (m_part->xmlDocImpl())
	{
	    ElementImpl *e = m_part->xmlDocImpl()->focusNode();
	    if (e && e==d->originalNode && (e->id()==ID_A || e->id()==ID_AREA))
	    {
		HTMLAnchorElementImpl *a = static_cast<HTMLAnchorElementImpl *>(e);
		emit m_part->urlSelected( a->areaHref().string(),
					  LeftButton, 0,
					  a->targetRef().string() );
	    }
            if (e)
	        e->setActive(false);
	}
        return;
      break;
    }
    //    if(m_part->keyReleaseHook(_ke)) return;
    QScrollView::keyReleaseEvent( _ke);
}

bool KHTMLView::focusNextPrevChild( bool next )
{
    if (focusWidget()!=this)
	setFocus();
    if (m_part->xmlDocImpl() && gotoLink(next))
	return true;
    if (m_part->parentPart() && m_part->parentPart()->view())
	return m_part->parentPart()->view()->focusNextPrevChild(next);
    m_part->overURL(QString(), 0);

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
    int x, y, xe, ye;
    x = bounds.left();
    y = bounds.top();
    xe = bounds.right();
    ye = bounds.bottom();

    kdDebug(6000)<<"scrolling coords: x="<<x<<" y="<<y<<" width="<<xe-x<<" height="<<ye-y<<endl;

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

    if ( (scrollX!=maxx) && (scrollY!=maxy) )
	return true;
    else return false;

}

bool KHTMLView::gotoLink(bool forward)
{
    if (!m_part->xmlDocImpl())
        return false;

    ElementImpl *currentNode = m_part->xmlDocImpl()->focusNode();
    ElementImpl *nextTarget = m_part->xmlDocImpl()->findNextLink(currentNode, forward);

    if (!currentNode && !d->borderTouched)
    {
	d->borderStart = forward;
	d->borderTouched = true;

	if (contentsY() != (forward?0:(contentsHeight()-visibleHeight())))
        {
	    setContentsPos(contentsX(), (forward?0:contentsHeight()));
	    if (nextTarget)
	    {
		QRect nextRect = nextTarget->getRect();
		if (nextRect.top()  < contentsY() ||
		    nextRect.bottom() > contentsY()+visibleHeight())
		    return true;
	    }
	    else return true;
	}
    }

    if (!nextTarget || (!currentNode && d->borderStart != forward))
	nextTarget = 0;

    QRect nextRect;
    if (nextTarget)
	nextRect = nextTarget->getRect();
    else
	nextRect = QRect(contentsX()+visibleWidth()/2, (forward?contentsHeight():0), 0, 0);

    if (scrollTo(nextRect))
    {
	if (!nextTarget)
	{
	    if (m_part->xmlDocImpl()->focusNode()) m_part->xmlDocImpl()->setFocusNode(0);
	    d->borderTouched = false;
	    return false;
	}
	else
	{
	    HTMLAnchorElementImpl *anchor = 0;
	    if ( ( nextTarget->id() == ID_A || nextTarget->id() == ID_AREA ) )
		anchor = static_cast<HTMLAnchorElementImpl *>( nextTarget );

	    if (anchor && !anchor->areaHref().isNull()) m_part->overURL(anchor->areaHref().string(), 0);
	    else m_part->overURL(QString(), 0);

	    kdDebug(6000)<<"reached link:"<<nextTarget->nodeName().string()<<endl;

	    m_part->xmlDocImpl()->setFocusNode(nextTarget);
	    emit m_part->nodeActivated(Node(nextTarget));
	}
    }
    return true;
}

bool KHTMLView::gotoNextLink()
{ return gotoLink(true); }

bool KHTMLView::gotoPrevLink()
{ return gotoLink(false); }

void KHTMLView::print()
{
    if(!m_part->xmlDocImpl()) return;
    khtml::RenderRoot *root = static_cast<khtml::RenderRoot *>(m_part->xmlDocImpl()->renderer());
    if(!root) return;

    KPrinter *printer = new KPrinter;
    if(printer->setup(this)) {
        QApplication::setOverrideCursor( waitCursor );
        // set up KPrinter
        printer->setFullPage(false);
        printer->setCreator("KDE 2.1 HTML Library");
        QString docname = m_part->xmlDocImpl()->URL().string();
        if ( !docname.isEmpty() )
	    printer->setDocName(docname);

        QPainter *p = new QPainter;
        p->begin( printer );
	khtml::setPrintPainter( p );

        m_part->xmlDocImpl()->setPaintDevice( printer );

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

        QValueList<int> oldSizes = m_part->fontSizes();

        const int printFontSizes[] = { 6, 7, 8, 10, 12, 14, 18, 24,
                                       28, 34, 40, 48, 56, 68, 82, 100, 0 };
        QValueList<int> fontSizes;
        for ( int i = 0; printFontSizes[i] != 0; i++ )
            fontSizes << printFontSizes[ i ];
        m_part->setFontSizes(fontSizes);
        m_part->xmlDocImpl()->applyChanges();

        root->updateSize();

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
        // We print the bottom 'overlap' units again at the top of the next page.
        int overlap = 30;
        p->setClipRect(0,0, pageWidth, pageHeight);
        if(root->docWidth() > metrics.width()) {
            double scale = ((double) metrics.width())/((double) root->docWidth());
#ifndef QT_NO_TRANSFORMATIONS
            p->scale(scale, scale);
#endif
            pageHeight = (int) (pageHeight/scale);
            pageWidth = (int) (pageWidth/scale);
            overlap = (int) (overlap/scale);
        }
        kdDebug(6000) << "printing: scaled html width = " << pageWidth
                      << " height = " << pageHeight << endl;
        int top = 0;
        while(top < root->docHeight()) {
            if(top > 0) printer->newPage();

            root->print(p, 0, top, pageWidth, pageHeight, 0, 0);
            p->translate(0,-(pageHeight-overlap));
            if (top + pageHeight >= root->docHeight())
                break; // Stop if we have printed everything
            top += (pageHeight-overlap);
        }

        p->end();
        delete p;

        // and now reset the layout to the usual one...
        root->setPrintingMode(false);
	khtml::setPrintPainter( 0 );
        m_part->xmlDocImpl()->setPaintDevice( this );
        m_part->setFontSizes(oldSizes);
        m_part->xmlDocImpl()->applyChanges();
        QApplication::restoreOverrideCursor();
    }
    delete printer;
}

void KHTMLView::slotPaletteChanged()
{
    if(!m_part->xmlDocImpl()) return;
    DOM::DocumentImpl *document = m_part->xmlDocImpl();
    if (!document->isHTMLDocument()) return;
    khtml::RenderRoot *root = static_cast<khtml::RenderRoot *>(document->renderer());
    if(!root) return;
    root->style()->resetPalette();
    NodeImpl *body = static_cast<HTMLDocumentImpl*>(document)->body();
    if(!body) return;
    body->setChanged(true);
    body->applyChanges();
}

void KHTMLView::paint(QPainter *p, const QRect &rc, int yOff, bool *more)
{
    if(!m_part->xmlDocImpl()) return;
    khtml::RenderRoot *root = static_cast<khtml::RenderRoot *>(m_part->xmlDocImpl()->renderer());
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
        updateContents(contentsX(),contentsY(),visibleWidth(),visibleHeight());
    }
    d->prevScrollbarVisible = verticalScrollBar()->isVisible();
}

void KHTMLView::toggleActLink(bool pressed)
{
    ElementImpl *e = m_part->xmlDocImpl()->focusNode();
    // ### FIXME:
    // move this code to HTMLAnchorElementImpl::setPressed(false),
    // or even better to HTMLAnchorElementImpl::event()
    if (!pressed && e && e==d->originalNode && (e->id()==ID_A || e->id()==ID_AREA))
    {
	HTMLAnchorElementImpl *a = static_cast<HTMLAnchorElementImpl *>(e);
	emit m_part->urlSelected( a->areaHref().string(),
				  LeftButton, 0,
				  a->targetRef().string() );
    }
    if (e)
	e->setActive(pressed);
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
    for (int i = 0; i < value.length(); ++i)
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

void KHTMLView::dispatchMouseEvent(int eventId, DOM::NodeImpl *targetNode, bool cancelable,
				   int detail,QMouseEvent *_mouse, bool setUnder,
				   int mouseEventType)
{
    if (d->underMouse)
	d->underMouse->deref();
    d->underMouse = targetNode;
    if (d->underMouse)
	d->underMouse->ref();

    int exceptioncode;
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
    bool metaKey = false; // ### qt support?

    // Also send the DOMFOCUSIN_EVENT when clicking on a node (Niko)
    // usually done in setFocusNode()
    // Allow LMB, MMB and RMB (correct?)
//     if(button != -1 && targetNode)
//     {
// 	UIEventImpl *ue = new UIEventImpl(EventImpl::DOMFOCUSIN_EVENT,
// 				          true,false,m_part->xmlDocImpl()->defaultView(),
// 					  0);

// 	ue->ref();
// 	targetNode->dispatchEvent(ue,exceptioncode);
// 	ue->deref();
//     }



    // mouseout/mouseover
    if (setUnder && (d->prevMouseX != clientX || d->prevMouseY != clientY)) {
    	NodeImpl *oldUnder = 0;

	if (d->prevMouseX >= 0 && d->prevMouseY >= 0) {
	    NodeImpl::MouseEvent mev( _mouse->stateAfter(), static_cast<NodeImpl::MouseEventType>(mouseEventType));
	    m_part->xmlDocImpl()->prepareMouseEvent( d->prevMouseX, d->prevMouseY, 0, 0, &mev );
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
		oldUnder->dispatchEvent(me,exceptioncode);
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
		targetNode->dispatchEvent(me,exceptioncode);
		me->deref();
	    }

	    if (oldUnder)
		oldUnder->deref();
	}
    }

    if (targetNode) {
	// send the actual event
	MouseEventImpl *me = new MouseEventImpl(static_cast<EventImpl::EventId>(eventId),
						true,cancelable,m_part->xmlDocImpl()->defaultView(),
						detail,screenX,screenY,clientX,clientY,
						ctrlKey,altKey,shiftKey,metaKey,
						button,0);
	me->ref();
	targetNode->dispatchEvent(me,exceptioncode);
	bool defaultHandled = me->defaultHandled();
	me->deref();

	// special case for HTML click & ondblclick handler
	if (eventId == EventImpl::CLICK_EVENT) {
	    me = new MouseEventImpl(d->isDoubleClick ? EventImpl::KHTML_DBLCLICK_EVENT : EventImpl::KHTML_CLICK_EVENT,
				    true,cancelable,m_part->xmlDocImpl()->defaultView(),
				    detail,screenX,screenY,clientX,clientY,
				    ctrlKey,altKey,shiftKey,metaKey,
				    button,0);

	    me->ref();
	    if (defaultHandled)
		me->setDefaultHandled();
	    targetNode->dispatchEvent(me,exceptioncode);
	    me->deref();
	}
    }

}

void KHTMLView::viewportWheelEvent(QWheelEvent* e)
{
    if ( d->vmode == QScrollView::AlwaysOff )
        e->accept();
    else if ( !verticalScrollBar()->isVisible() && m_part->parentPart() ) {
        if ( m_part->parentPart()->view() )
            m_part->parentPart()->view()->wheelEvent( e );

        e->ignore();
    }
    else
        QScrollView::viewportWheelEvent( e );
}

void KHTMLView::focusOutEvent( QFocusEvent *e )
{
    m_part->stopAutoScroll();
    QScrollView::focusOutEvent( e );
}

// vim:ts=4:sw=4
