/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <qwidget.h>

#include <kwqdebug.h>

#import <KWQView.h>

/*
    A QWidget rougly corresponds to an NSView.  In Qt a QFrame and QMainWindow inherit
    from a QWidget.  In Cocoa a NSWindow does not inherit from NSView.  We will
    emulate QWidgets using NSViews.
    
    The only QWidget emulated should be KHTMLView.  It's inheritance graph is
        KHTMLView
            QScrollView
                QFrame
                    QWidget
    
    We may not need to emulate any of the QScrollView behaviour as we MAY
    get scrolling behaviour for free simply be implementing all rendering 
    in an NSView and using NSScrollView.
    
    It appears that the only dependence of the window in khtml and kjs is
    the javascript functions that manipulate a window.  We may want to
    rework work that code rather than add NSWindow glue code.
*/


class QWidgetPrivate
{
friend class QWidget;
public:
    
    QWidgetPrivate() { }
    
    ~QWidgetPrivate() {}
    
private:
    QWidget::FocusPolicy focusPolicy;
    QStyle	*style;
    QFont	font;
    QCursor	cursor;
    QPalette    pal;
    NSView	*view;
    int lockCount;
};

QWidget::QWidget(QWidget *parent=0, const char *name=0, WFlags f=0) 
{
    static QStyle *defaultStyle = new QStyle;
    
    data = new QWidgetPrivate;
    data->lockCount = 0;
    data->view = [[KWQView alloc] initWithFrame: NSMakeRect (0,0,0,0) widget: this];
    data->style = defaultStyle;
}

QWidget::~QWidget() 
{
    [data->view release];
    delete data;
}


QSize QWidget::sizeHint() const 
{
    // May be overriden by subclasses.
    return QSize(0,0);
}

void QWidget::resize(int w, int h) 
{
    KWQDEBUG ("%p %s to w %d h %d\n", getView(), [[[getView() class] className] cString], w, h);
    internalSetGeometry( pos().x(), pos().y(), w, h );
}

void QWidget::setActiveWindow() 
{
    _logNeverImplemented();
}


void QWidget::setEnabled(bool) 
{
    _logNeverImplemented();
}


void QWidget::setAutoMask(bool) 
{
    _logNeverImplemented();
}


void QWidget::setMouseTracking(bool) 
{
//    _logNeverImplemented();
}


int QWidget::winId() const
{
/*
    Does this winId need to be valid across sessions?  It appears to
    be used by HTMLDocument::setCookie( const DOMString & value ).  If
    so we have to rethink the current approach.
    
    Just return the hash of the NSView.
*/
    return (int)[data->view hash];
}


int QWidget::x() const 
{
    NSRect vFrame = [getView() frame];
    return (int)vFrame.origin.x;
}


int QWidget::y() const 
{
    NSRect vFrame = [getView() frame];
    return (int)vFrame.origin.y;
}


int QWidget::width() const 
{ 
    NSRect vFrame = [getView() frame];
    return (int)vFrame.size.width;
}


int QWidget::height() const 
{
    NSRect vFrame = [getView() frame];
    return (int)vFrame.size.height;
}


QSize QWidget::size() const 
{
    NSRect vFrame = [getView() frame];
    return QSize ((int)vFrame.size.width, (int)vFrame.size.height);
}


void QWidget::resize(const QSize &s) 
{
    resize( s.width(), s.height());
}


QPoint QWidget::pos() const 
{
    NSRect vFrame = [getView() frame];
    return QPoint ((int)vFrame.origin.x, (int)vFrame.origin.y);
}


void QWidget::move(int x, int y) 
{
    KWQDEBUG ("%p %s to x %d y %d\n", getView(), [[[getView() class] className] cString], x, y);
    internalSetGeometry( x,
			 y,
			 width(), height() );
}


void QWidget::move(const QPoint &p) 
{
    move( p.x(), p.y() );
}


QRect QWidget::frameGeometry() const
{
    NSRect vFrame = [getView() frame];
    return QRect((int)vFrame.origin.x, (int)vFrame.origin.y, (int)vFrame.size.width, (int)vFrame.size.height);
}


QWidget *QWidget::topLevelWidget() const 
{
    // This is only used by JavaScript to implement the various
    // window geometry manipulations and accessors, i.e.:
    // window.moveTo(), window.moveBy(), window.resizeBy(), window.resizeTo(),
    // outerWidth, outerHeight.
    
    // This should return a subclass of QWidget that fronts for an
    // NSWindow.
    return (QWidget *)this;
}


QPoint QWidget::mapToGlobal(const QPoint &p) const
{
    // This is only used by JavaScript to implement the getting
    // the screenX and screen Y coordinates.
    NSPoint sp;
    sp = [[data->view window] convertBaseToScreen: [data->view convertPoint: NSMakePoint((float)p.x(), (float)p.y()) toView: nil]];
    return QPoint((int)sp.x, (int)sp.y);
}


void QWidget::setFocus()
{
    _logNeverImplemented();
}


void QWidget::clearFocus()
{
    _logNeverImplemented();
}


QWidget::FocusPolicy QWidget::focusPolicy() const
{
    return data->focusPolicy;
}


void QWidget::setFocusPolicy(FocusPolicy fp)
{
    data->focusPolicy = fp;
}


void QWidget::setFocusProxy( QWidget *w)
{
    if (w != 0)
        data->focusPolicy = w->focusPolicy();
    // else?  FIXME: [rjw] we need to understand kde's focus policy.  I don't
    // think this is even relevant for us.
}


const QPalette& QWidget::palette() const
{
    return data->pal;
}


void QWidget::setPalette(const QPalette &palette)
{
    data->pal = palette;
}

void QWidget::unsetPalette()
{
    // Only called by RenderFormElement::layout, which I suspect will
    // have to be rewritten.  Do nothing.
    _logNeverImplemented();
}


QStyle &QWidget::style() const
{
    return *data->style;
}


void QWidget::setStyle(QStyle *style)
{
    // According to the Qt implementation 
    /*
    Sets the widget's GUI style to \a style. Ownership of the style
    object is not transferred.
    */
    data->style = style;
}


QFont QWidget::font() const
{
    return data->font;
}


void QWidget::setFont(const QFont &font)
{
    data->font = font;
}


void QWidget::constPolish() const
{
    _logNeverImplemented();
}


QSize QWidget::minimumSizeHint() const
{
    NSView *view = getView();
    
    if ([view isKindOfClass: [NSControl class]]) {
        NSControl *control = (NSControl *)view;
        [control sizeToFit];
        NSRect frame = [view frame];
        return QSize((int)frame.size.width, (int)frame.size.height);
    }

    return QSize(0,0);
}


bool QWidget::isVisible() const
{
    return [[data->view window] isVisible];
}


void QWidget::setCursor(const QCursor &cur)
{
    data->cursor = cur;
    
    id view = data->view;
    if ([view respondsToSelector:@selector(setCursor:)]) { 
	[view setCursor:(NSCursor *)data->cursor.handle()];
    }
}

QCursor QWidget::cursor()
{
    return data->cursor;
}

void QWidget::unsetCursor()
{
    setCursor(QCursor());
}

bool QWidget::event(QEvent *)
{
    // This will eventually not be called, or called from our implementation
    // of run loop, or something???
    _logNeverImplemented();
    return TRUE;
}


bool QWidget::focusNextPrevChild(bool)
{
    _logNeverImplemented();
    return TRUE;
}


bool QWidget::hasMouseTracking() const
{
//    _logNeverImplemented();
    return true;
}


void QWidget::show()
{
    _logNeverImplemented();
}


void QWidget::hide()
{
    _logNeverImplemented();
}


// no copying or assignment
// note that these are "standard" (no pendantic stuff needed)



void QWidget::internalSetGeometry( int x, int y, int w, int h )
{
    NSView *docview;
    
    //if ([nsview isKindOfClass: NSClassFromString(@"IFDynamicScrollBarsView")])
    //if ([data->view isKindOfClass: NSClassFromString(@"NSScrollView")])
    //    docview = [((NSScrollView *)data->view) documentView];
    //else
        docview = data->view;
    [docview setFrame: NSMakeRect (x, y, w, h)];
}


void QWidget::showEvent(QShowEvent *)
{
    _logNeverImplemented();
}


void QWidget::hideEvent(QHideEvent *)
{
    _logNeverImplemented();
}


void QWidget::wheelEvent(QWheelEvent *)
{
    _logNeverImplemented();
}


void QWidget::keyPressEvent(QKeyEvent *)
{
    _logNeverImplemented();
}


void QWidget::keyReleaseEvent(QKeyEvent *)
{
    _logNeverImplemented();
}


void QWidget::focusOutEvent(QFocusEvent *)
{
    _logNeverImplemented();
}


void QWidget::setBackgroundMode(BackgroundMode)
{
    _logNeverImplemented();
}


void QWidget::setAcceptDrops(bool)
{
    _logNeverImplemented();
}


void QWidget::erase()
{
    _logNeverImplemented();
}


QWidget *QWidget::focusWidget() const
{
    _logNeverImplemented();
    return 0;
}


QPoint QWidget::mapFromGlobal(const QPoint &point) const
{
    _logNeverImplemented();
    return QPoint(0,0);
}

void QWidget::paint (void *)
{
    _logNotYetImplemented();
}

NSView *QWidget::getView() const
{
    return data->view;
}


void QWidget::setView(NSView *view)
{
    if (data->view)
        [data->view release];
    data->view = [view retain];
}

void QWidget::endEditing()
{
    id window, firstResponder;
    
    // Catch the field editor case.
    window = [getView() window];
    [window endEditingFor: nil];
    
    // The previous case is probably not necessary, given that we whack
    // any NSText first repsonders.
    firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass: NSClassFromString(@"NSText")])
        [window makeFirstResponder: nil];
}


bool QWidget::_lockFocus()
{
    if ([getView() canDraw]){
        [getView() lockFocus];
        data->lockCount++;
        return 1;
    }
    return 0;
}

void QWidget::_unlockFocus()
{
    if (data->lockCount){
        [getView() unlockFocus];
        data->lockCount--;
    }
}


void QWidget::_flushWindow()
{
    [[getView() window] flushWindow];
}


void QWidget::_displayRect (QRect rect)
{
    [getView() displayRect: NSMakeRect (rect.x(), rect.y(), rect.width(), rect.height())];
}

