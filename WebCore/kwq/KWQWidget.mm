/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <qwidget.h>

#import <KWQView.h>
#import <WebCoreFrameView.h>
#import <KWQLogging.h>
#import <KWQWindowWidget.h>

#import <khtmlview.h>
#import <render_replaced.h>
#import <KWQKHTMLPartImpl.h>

using khtml::RenderWidget;

/*
    A QWidget roughly corresponds to an NSView.  In Qt a QFrame and QMainWindow inherit
    from a QWidget.  In Cocoa a NSWindow does not inherit from NSView.  We
    emulate most QWidgets using NSViews.
*/


class QWidgetPrivate
{
public:
    QStyle *style;
    QFont font;
    QCursor cursor;
    QPalette pal;
    NSView *view;
};

QWidget::QWidget() 
    : data(new QWidgetPrivate)
{
    data->view = [[KWQView alloc] initWithWidget:this];

    static QStyle defaultStyle;
    data->style = &defaultStyle;
}

QWidget::QWidget(NSView *view)
    : data(new QWidgetPrivate)
{
    data->view = [view retain];

    static QStyle defaultStyle;
    data->style = &defaultStyle;
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
    setFrameGeometry(QRect(pos().x(), pos().y(), w, h));
}

void QWidget::setActiveWindow() 
{
    [[data->view window] makeKeyAndOrderFront:nil];
}

void QWidget::setEnabled(bool enabled)
{
    id view = data->view;
    if ([view respondsToSelector:@selector(setEnabled:)]) {
        [view setEnabled:enabled];
    }
}

long QWidget::winId() const
{
    return (long)this;
}

int QWidget::x() const
{
    return frameGeometry().topLeft().x();
}

int QWidget::y() const 
{
    return frameGeometry().topLeft().y();
}

int QWidget::width() const 
{ 
    return frameGeometry().size().width();
}

int QWidget::height() const 
{
    return frameGeometry().size().height();
}

QSize QWidget::size() const 
{
    return frameGeometry().size();
}

void QWidget::resize(const QSize &s) 
{
    resize(s.width(), s.height());
}

QPoint QWidget::pos() const 
{
    return frameGeometry().topLeft();
}

void QWidget::move(int x, int y) 
{
    setFrameGeometry(QRect(x, y, width(), height()));
}

void QWidget::move(const QPoint &p) 
{
    move(p.x(), p.y());
}

QRect QWidget::frameGeometry() const
{
    NSView *view = getView();
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
    }
    return QRect([view frame]);
}

int QWidget::baselinePosition() const
{
    return height();
}

QWidget *QWidget::topLevelWidget() const 
{
    NSWindow *window = nil;
    NSView *view = getView();

    window = [view window];
    while (window == nil && view != nil) { 
	view = [view superview]; 
	window = [view window];
    }

    return window ? KWQWindowWidget::fromNSWindow(window) : NULL;
}

QPoint QWidget::mapToGlobal(const QPoint &p) const
{
    // This is only used by JavaScript to implement the getting
    // the screenX and screen Y coordinates.

    if (topLevelWidget() != nil) {
	return topLevelWidget()->mapToGlobal(p);
    } else {
	return p;
    }
}

void QWidget::setFocus()
{
    // KHTML will call setFocus on us without first putting us in our
    // superview and positioning us. This works around that issue.
    RenderWidget *renderWidget = dynamic_cast<RenderWidget *>(const_cast<QObject *>(eventFilterObject()));
    int x, y;
    if (renderWidget && renderWidget->absolutePosition(x, y)) {
        renderWidget->view()->addChild(this, x, y);
    }
    
    [[getView() window] makeFirstResponder:getView()];
}

void QWidget::clearFocus()
{
    KWQKHTMLPartImpl::clearDocumentFocus(this);
}

QWidget::FocusPolicy QWidget::focusPolicy() const
{
    // This is the AppKit rule for what can be tabbed to.
    // An NSControl that accepts first responder, and has an editable, enabled cell.
    
    NSView *view = getView();
    if (![view acceptsFirstResponder] || ![view isKindOfClass:[NSControl class]]) {
        return NoFocus;
    }
    NSControl *control = (NSControl *)view;
    NSCell *cell = [control cell];
    if (![cell isEditable] || ![cell isEnabled]) {
        return NoFocus;
    }
    return TabFocus;
}

void QWidget::setFocusPolicy(FocusPolicy fp)
{
}

void QWidget::setFocusProxy(QWidget *w)
{
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
}

bool QWidget::isVisible() const
{
    // FIXME - rewrite interms of top level widget?
    return [[data->view window] isVisible];
}

void QWidget::setCursor(const QCursor &cur)
{
    data->cursor = cur;
    
    id view = data->view;
    while (view) {
        if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) { 
            [view setCursor:data->cursor.handle()];
        }
        view = [view superview];
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
    return false;
}

bool QWidget::focusNextPrevChild(bool)
{
    ERROR("not yet implemented");
    return true;
}

bool QWidget::hasMouseTracking() const
{
    return true;
}

void QWidget::setFrameGeometry(const QRect &rect)
{
    NSView *view = getView();
    
    ASSERT(view);
    
    // A QScrollView is a widget only used to represent a frame.  If
    // this widget's view is a WebCoreFrameView the we resize it's containing
    // view,  an WebView.  The scrollview contained by the WebView
    // will be autosized.
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
        ASSERT(view);
    }
    
    [view setFrame:rect];
}

QPoint QWidget::mapFromGlobal(const QPoint &p) const
{
    NSPoint bp;
    bp = [[data->view window] convertScreenToBase:[data->view convertPoint:p toView:nil]];
    return QPoint(bp);
}

NSView *QWidget::getView() const
{
    return data->view;
}

void QWidget::setView(NSView *view)
{
    [view retain];
    [data->view release];
    data->view = view;
}

void QWidget::lockDrawingFocus()
{
    [getView() lockFocus];
}

void QWidget::unlockDrawingFocus()
{
    [getView() unlockFocus];
}

void QWidget::disableFlushDrawing()
{
    [[getView() window] disableFlushWindow];
}

void QWidget::enableFlushDrawing()
{
    NSWindow *window = [getView() window];
    [window enableFlushWindow];
    [window flushWindowIfNeeded];
}

void QWidget::setDrawingAlpha(float alpha)
{
    CGContextSetAlpha((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], alpha);
}

void QWidget::paint(QPainter *p, const QRect &r)
{
    if (p->paintingDisabled()) {
        return;
    }
    NSView *view = getView();
#if 0
    NSRect rect = NSIntersectionRect([view convertRect:r fromView:[view superview]], [view bounds]);
    NSLog(@"%@, rect is %@, bounds rect is %@", view, NSStringFromRect(rect), NSStringFromRect([view bounds]));
#endif
    [view displayRectIgnoringOpacity:[view bounds]];
}
