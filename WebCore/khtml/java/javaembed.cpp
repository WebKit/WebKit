/****************************************************************************
    Implementation of QXEmbed class

   Copyright (C) 1999-2000 Troll Tech AS

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*****************************************************************************/

#include "javaembed.h"

#include <kdebug.h>
#include <klocale.h>

#include <qapplication.h>
#include <qevent.h>

class KJavaEmbedPrivate
{
friend class KJavaEmbed;
};

QString getQtEventName( QEvent* e )
{
    QString s;

    switch( e->type() )
    {
        case QEvent::None:
            s = "None";
            break;
        case QEvent::Timer:
            s = "Timer";
            break;
        case QEvent::MouseButtonPress:
            s = "MouseButtonPress";
            break;
        case QEvent::MouseButtonRelease:
            s = "MouseButtonRelease";
            break;
        case QEvent::MouseButtonDblClick:
            s = "MouseButtonClick";
            break;
        case QEvent::MouseMove:
            s = "MouseMove";
            break;
        case QEvent::KeyPress:
            s = "KeyPress";
            break;
        case QEvent::KeyRelease:
            s = "KeyRelease";
            break;
        case QEvent::FocusIn:
            s = "FocusIn";
            break;
        case QEvent::FocusOut:
            s = "FocusOut";
            break;
        case QEvent::Enter:
            s = "Enter";
            break;
        case QEvent::Leave:
            s = "Leave";
            break;
        case QEvent::Paint:
            s = "Paint";
            break;
        case QEvent::Move:
            s = "Move";
            break;
        case QEvent::Resize:
            s = "Resize";
            break;
        case QEvent::Create:
            s = "Create";
            break;
        case QEvent::Destroy:
            s = "Destroy";
            break;
        case QEvent::Show:
            s = "Show";
            break;
        case QEvent::Hide:
            s = "Hide";
            break;
        case QEvent::Close:
            s = "Close";
            break;
        case QEvent::Quit:
            s = "Quit";
            break;
        case QEvent::Reparent:
            s = "Reparent";
            break;
        case QEvent::ShowMinimized:
            s = "ShowMinimized";
            break;
        case QEvent::ShowNormal:
            s = "ShowNormal";
            break;
        case QEvent::WindowActivate:
            s = "WindowActivate";
            break;
        case QEvent::WindowDeactivate:
            s = "WindowDeactivate";
            break;
        case QEvent::ShowToParent:
            s = "ShowToParent";
            break;
        case QEvent::HideToParent:
            s = "HideToParent";
            break;
        case QEvent::ShowMaximized:
            s = "ShowMaximized";
            break;
        case QEvent::Accel:
            s = "Accel";
            break;
        case QEvent::Wheel:
            s = "Wheel";
            break;
        case QEvent::AccelAvailable:
            s = "AccelAvailable";
            break;
        case QEvent::CaptionChange:
            s = "CaptionChange";
            break;
        case QEvent::IconChange:
            s = "IconChange";
            break;
        case QEvent::ParentFontChange:
            s = "ParentFontChange";
            break;
        case QEvent::ApplicationFontChange:
            s = "ApplicationFontChange";
            break;
        case QEvent::ParentPaletteChange:
            s = "ParentPaletteChange";
            break;
        case QEvent::ApplicationPaletteChange:
            s = "ApplicationPaletteChange";
            break;
        case QEvent::Clipboard:
            s = "Clipboard";
            break;
        case QEvent::Speech:
            s = "Speech";
            break;
        case QEvent::SockAct:
            s = "SockAct";
            break;
        case QEvent::AccelOverride:
            s = "AccelOverride";
            break;
        case QEvent::DragEnter:
            s = "DragEnter";
            break;
        case QEvent::DragMove:
            s = "DragMove";
            break;
        case QEvent::DragLeave:
            s = "DragLeave";
            break;
        case QEvent::Drop:
            s = "Drop";
            break;
        case QEvent::DragResponse:
            s = "DragResponse";
            break;
        case QEvent::ChildInserted:
            s = "ChildInserted";
            break;
        case QEvent::ChildRemoved:
            s = "ChildRemoved";
            break;
        case QEvent::LayoutHint:
            s = "LayoutHint";
            break;
        case QEvent::ShowWindowRequest:
            s = "ShowWindowRequest";
            break;
        case QEvent::ActivateControl:
            s = "ActivateControl";
            break;
        case QEvent::DeactivateControl:
            s = "DeactivateControl";
            break;
        case QEvent::User:
            s = "User Event";
            break;

        default:
            s = "Undefined Event, value = " + QString::number( e->type() );
            break;
    }

    return s;
}

/*!\reimp
 */
bool KJavaEmbed::eventFilter( QObject* o, QEvent* e)
{
    QEvent::Type t = e->type();

    if( t != QEvent::MouseMove && t != QEvent::Timer && t <= QEvent::User )
    {
        kdDebug(6100) << "KJavaEmbed::eventFilter, event = " << getQtEventName( e ) << endl;

        if( o == this )
            kdDebug(6100) << "event is for me:)" << endl;

        switch ( e->type() )
        {
            case QEvent::FocusIn:
                break;

            case QEvent::FocusOut:
                break;

            case QEvent::Leave:
                /* check to see if we are entering the applet somehow... */
                break;

            case QEvent::Enter:
                break;

            case QEvent::WindowActivate:
    	        break;

            case QEvent::WindowDeactivate:
    	        break;

            default:
                break;
        }

    }

    return FALSE;
}


#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
QString getX11EventName( XEvent* e )
{
    QString s;

    switch( e->type )
    {
        case KeyPress:
            s = "KeyPress";
            break;
        case KeyRelease:
            s = "KeyRelease";
            break;
        case ButtonPress:
            s = "ButtonPress";
            break;
        case ButtonRelease:
            s = "ButtonRelease";
            break;
        case MotionNotify:
            s = "MotionNotify";
            break;
        case EnterNotify:
            s = "EnterNotify";
            break;
        case LeaveNotify:
            s = "LeaveNotify";
            break;
        case FocusIn:
            s = "FocusIn";
            break;
        case FocusOut:
            s = "FocusOut";
            break;
        case KeymapNotify:
            s = "KeymapNotify";
            break;
        case Expose:
            s = "Expose";
            break;
        case GraphicsExpose:
            s = "GraphicsExpose";
            break;
        case NoExpose:
            s = "NoExpose";
            break;
        case VisibilityNotify:
            s = "VisibilityNotify";
            break;
        case CreateNotify:
            s = "CreateNotify";
            break;
        case DestroyNotify:
            s = "DestroyNotify";
            break;
        case UnmapNotify:
            s = "UnmapNotify";
            break;
        case MapNotify:
            s = "MapNotify";
            break;
        case MapRequest:
            s = "MapRequest";
            break;
        case ReparentNotify:
            s = "ReparentNotify";
            break;
        case ConfigureNotify:
            s = "ConfigureNotify";
            break;
        case ConfigureRequest:
            s = "ConfigureRequest";
            break;
        case GravityNotify:
            s = "GravityNotify";
            break;
        case ResizeRequest:
            s = "ResizeRequest";
            break;
        case CirculateNotify:
            s = "CirculateNofify";
            break;
        case CirculateRequest:
            s = "CirculateRequest";
            break;
        case PropertyNotify:
            s = "PropertyNotify";
            break;
        case SelectionClear:
            s = "SelectionClear";
            break;
        case SelectionRequest:
            s = "SelectionRequest";
            break;
        case SelectionNotify:
            s = "SelectionNotify";
            break;
        case ColormapNotify:
            s = "ColormapNotify";
            break;
        case ClientMessage:
            s = "ClientMessage";
            break;
        case MappingNotify:
            s = "MappingNotify";
            break;
        case LASTEvent:
            s = "LASTEvent";
            break;

        default:
            s = "Undefined";
            break;
    }

    return s;
}

/*!
  Constructs a xembed widget.

  The \e parent, \e name and \e f arguments are passed to the QFrame
  constructor.
 */
KJavaEmbed::KJavaEmbed( QWidget *parent, const char *name, WFlags f )
  : QWidget( parent, name, f )
{
    d = new KJavaEmbedPrivate;

    setFocusPolicy( StrongFocus );
    setKeyCompression( FALSE );
    setAcceptDrops( TRUE );

    window = 0;
}

/*!
  Destructor. Cleans up the focus if necessary.
 */
KJavaEmbed::~KJavaEmbed()
{
    if ( window != 0 )
    {
        XUnmapWindow( qt_xdisplay(), window );
        QApplication::flushX();
    }

    delete d;
}

/*!\reimp
 */
void KJavaEmbed::resizeEvent( QResizeEvent* e )
{
//    kdDebug(6100) << "KJavaEmbed::resizeEvent" << endl;
    QWidget::resizeEvent( e );

    if ( window != 0 )
        XResizeWindow( qt_xdisplay(), window, e->size().width(), e->size().height() );
}

bool  KJavaEmbed::event( QEvent* e)
{
//    kdDebug(6100) << "KJavaEmbed::event, event type = " << getQtEventName( e ) << endl;
    switch( e->type() )
    {
        case QEvent::ShowWindowRequest:
            XMapRaised( qt_xdisplay(), window );
            break;

        default:
            break;
    }

    return QWidget::event( e );
}

/*!\reimp
 */
void KJavaEmbed::focusInEvent( QFocusEvent* )
{
//    kdDebug(6100) << "KJavaEmbed::focusInEvent" << endl;

    if ( !window )
        return;

    XEvent ev;
    memset( &ev, 0, sizeof( ev ) );
    ev.xfocus.type = FocusIn;
    ev.xfocus.window = window;

    XSendEvent( qt_xdisplay(), window, true, NoEventMask, &ev );
}

/*!\reimp
 */
void KJavaEmbed::focusOutEvent( QFocusEvent* )
{
//    kdDebug(6100) << "KJavaEmbed::focusOutEvent" << endl;

    if ( !window )
        return;

    XEvent ev;
    memset( &ev, 0, sizeof( ev ) );
    ev.xfocus.type = FocusOut;
    ev.xfocus.window = window;
    XSendEvent( qt_xdisplay(), window, true, NoEventMask, &ev );
}


/*!
  Embeds the window with the identifier \a w into this xembed widget.

  This function is useful if the server knows about the client window
  that should be embedded.  Often it is vice versa: the client knows
  about its target embedder. In that case, it is not necessary to call
  embed(). Instead, the client will call the static function
  embedClientIntoWindow().

  \sa embeddedWinId()
 */
void KJavaEmbed::embed( WId w )
{
//    kdDebug(6100) << "KJavaEmbed::embed" << endl;

    if ( w == 0 )
        return;

    window = w;

    //first withdraw the window
    XWithdrawWindow( qt_xdisplay(), window, qt_xscreen() );
    QApplication::flushX();

    //now reparent the window to be swallowed by the KJavaEmbed widget
    XReparentWindow( qt_xdisplay(), window, winId(), 0, 0 );
    QApplication::syncX();

    //now resize it
    XResizeWindow( qt_xdisplay(), window, width(), height() );
    XMapRaised( qt_xdisplay(), window );

    setFocus();
}

/*!\reimp
 */
bool KJavaEmbed::focusNextPrevChild( bool next )
{
    if ( window )
        return FALSE;
    else
        return QWidget::focusNextPrevChild( next );
}

/*!\reimp
 */
bool KJavaEmbed::x11Event( XEvent* e)
{
//    kdDebug(6100) << "KJavaEmbed::x11Event, event = " << getX11EventName( e )
//        << ", window = " << e->xany.window << endl;

    switch ( e->type )
    {
        case DestroyNotify:
            if ( e->xdestroywindow.window == window )
            {
                window = 0;
            }
            break;

        default:
	        break;
    }

    return false;
}

/*!
  Specifies that this widget can use additional space, and that it can
  survive on less than sizeHint().
*/

QSizePolicy KJavaEmbed::sizePolicy() const
{
    return QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
}

/*!
  Returns a size sufficient for the embedded window
*/
QSize KJavaEmbed::sizeHint() const
{
    return minimumSizeHint();
}

/*!
  Returns the minimum size specified by the embedded window.
*/
QSize KJavaEmbed::minimumSizeHint() const
{
    if ( window )
    {
        kdDebug(6100) << "KJavaEmbed::minimumSizeHint, getting hints from window" << endl;

        XSizeHints size;
        long msize;
        if( XGetWMNormalHints( qt_xdisplay(), window, &size, &msize ) &&
            ( size.flags & PMinSize) )
        {
            kdDebug(6100) << "XGetWMNormalHints succeeded, width = " << size.min_width
                          << ", height = " << size.min_height << endl;

            return QSize( size.min_width, size.min_height );
        }
    }

    return QSize( 0, 0 );
}

// for KDE
#include "javaembed.moc"
