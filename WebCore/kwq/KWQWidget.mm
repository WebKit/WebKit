/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQWidget.h"

#import "KWQView.h"
#import "WebCoreFrameView.h"
#import "KWQLogging.h"
#import "KWQWindowWidget.h"

#import "khtmlview.h"
#import "render_canvas.h"
#import "render_replaced.h"
#import "KWQKHTMLPart.h"
#import "WebCoreBridge.h"

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
    [KWQKHTMLPart::bridgeForWidget(this) focusWindow];
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
    return QRect([getOuterView() frame]);
}

int QWidget::baselinePosition() const
{
    return height();
}

bool QWidget::hasFocus() const
{
    NSView *view = getView();
    NSView *firstResponder = [KWQKHTMLPart::bridgeForWidget(this) firstResponder];
    if (!firstResponder) {
        return false;
    }
    if (firstResponder == view) {
        return true;
    }
    // The following check handles both text field editors and the secure text field
    // that goes inside the KWQTextField (and its editor). We have to check the class
    // of the view because we don't want to be fooled by subviews of NSScrollView, for example.
    if ([view isKindOfClass:[NSTextField class]]
            && [firstResponder isKindOfClass:[NSView class]]
            && [firstResponder isDescendantOf:view]) {
        return true;
    }
    return false;
}

void QWidget::setFocus()
{
    if (hasFocus()) {
        return;
    }
    
    // KHTML will call setFocus on us without first putting us in our
    // superview and positioning us. Normally layout computes the position
    // and the drawing process positions the widget. Do both things explicitly.
    RenderWidget *renderWidget = dynamic_cast<RenderWidget *>(const_cast<QObject *>(eventFilterObject()));
    int x, y;
    if (renderWidget) {
        if (renderWidget->canvas()->needsLayout()) {
            renderWidget->view()->layout();
        }
        if (renderWidget->absolutePosition(x, y)) {
            renderWidget->view()->addChild(this, x, y);
        }
    }
    
    NSView *view = getView();
    if ([view acceptsFirstResponder]) {
        [KWQKHTMLPart::bridgeForWidget(this) makeFirstResponder:view];
    }
}

void QWidget::clearFocus()
{
    if (!hasFocus()) {
        return;
    }
    
    KWQKHTMLPart::clearDocumentFocus(this);
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
    return [[KWQKHTMLPart::bridgeForWidget(this) window] isVisible];
}

void QWidget::setCursor(const QCursor &cur)
{
    id view = data->view;
    while (view) {
        if ([view respondsToSelector:@selector(setDocumentCursor:)]) {
            [view setDocumentCursor:cur.handle()];
            break;
        }
        view = [view superview];
    }
}

QCursor QWidget::cursor()
{
    id view = data->view;
    while (view) {
        if ([view respondsToSelector:@selector(documentCursor)]) { 
            return [view documentCursor];
        }
        view = [view superview];
    }
    return QCursor();
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
    [getOuterView() setFrame:rect];
}

QPoint QWidget::mapFromGlobal(const QPoint &p) const
{
    NSPoint bp;
    bp = [[KWQKHTMLPart::bridgeForWidget(this) window] convertScreenToBase:[data->view convertPoint:p toView:nil]];
    return QPoint(bp);
}

NSView *QWidget::getView() const
{
    return data->view;
}

void QWidget::setView(NSView *view)
{
    if (view == data->view) {
        return;
    }
    
    [data->view release];
    data->view = [view retain];
}

NSView *QWidget::getOuterView() const
{
    // A QScrollView is a widget normally used to represent a frame.
    // If this widget's view is a WebCoreFrameView the we resize its containing view, a WebFrameView.
    // The scroll view contained by the WebFrameView will be autosized.
    NSView *view = data->view;
    ASSERT(view);
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
        ASSERT(view);
    }
    return view;
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
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    [[getView() window] disableFlushWindow];
}

void QWidget::enableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    NSWindow *window = [getView() window];
    [window enableFlushWindow];
    [window flushWindow];
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
    NSView *view = getOuterView();
    // KWQTextArea and KWQTextField both rely on the fact that we use this particular
    // NSView display method. If you change this, be sure to update them as well.
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
}

void QWidget::sendConsumedMouseUp()
{
    khtml::RenderWidget *widget = const_cast<khtml::RenderWidget *>
	(static_cast<const khtml::RenderWidget *>(eventFilterObject()));

    widget->sendConsumedMouseUp(QPoint([[NSApp currentEvent] locationInWindow]),
			      // FIXME: should send real state and button
			      0, 0);
}
