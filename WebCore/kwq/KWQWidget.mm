/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQWidget.h"

#import "KWQExceptions.h"
#import "KWQFoundationExtras.h"
#import "MacFrame.h"
#import "KWQLogging.h"
#import "KWQStyle.h"
#import "KWQView.h"
#import "KWQWindowWidget.h"
#import "WebCoreBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import "khtmlview.h"
#import "render_canvas.h"
#import "render_replaced.h"
#import "render_style.h"

using khtml::RenderWidget;

static bool deferFirstResponderChanges;
static QWidget *deferredFirstResponder;

/*
    A QWidget roughly corresponds to an NSView.  In Qt a QFrame and QMainWindow inherit
    from a QWidget.  In Cocoa a NSWindow does not inherit from NSView.  We
    emulate most QWidgets using NSViews.
*/

class KWQWidgetPrivate
{
public:
    QStyle *style;
    QFont font;
    QPalette pal;
    NSView *view;
    bool visible;
    bool mustStayInWindow;
    bool removeFromSuperviewSoon;
};

QWidget::QWidget() : data(new KWQWidgetPrivate)
{
    static QStyle defaultStyle;
    data->style = &defaultStyle;
    data->view = nil;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

QWidget::QWidget(NSView *view) : data(new KWQWidgetPrivate)
{
    static QStyle defaultStyle;
    data->style = &defaultStyle;
    data->view = KWQRetain(view);
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

QWidget::~QWidget() 
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQRelease(data->view);
    KWQ_UNBLOCK_EXCEPTIONS;

    if (deferredFirstResponder == this)
        deferredFirstResponder = 0;

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
    KWQ_BLOCK_EXCEPTIONS;
    [MacFrame::bridgeForWidget(this) focusWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::setEnabled(bool enabled)
{
    id view = data->view;
    KWQ_BLOCK_EXCEPTIONS;
    if ([view respondsToSelector:@selector(setEnabled:)]) {
        [view setEnabled:enabled];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QWidget::isEnabled() const
{
    id view = data->view;

    KWQ_BLOCK_EXCEPTIONS;
    if ([view respondsToSelector:@selector(isEnabled)]) {
        return [view isEnabled];
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
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
    QRect rect;
    KWQ_BLOCK_EXCEPTIONS;
    rect = QRect([getOuterView() frame]);
    return rect;
    KWQ_UNBLOCK_EXCEPTIONS;
    return QRect();
}

int QWidget::baselinePosition(int height) const
{
    return height;
}

bool QWidget::hasFocus() const
{
    if (deferFirstResponderChanges && deferredFirstResponder) {
        return this == deferredFirstResponder;
    }

    KWQ_BLOCK_EXCEPTIONS;

    NSView *view = [getView() _webcore_effectiveFirstResponder];

    id firstResponder = [MacFrame::bridgeForWidget(this) firstResponder];

    if (!firstResponder) {
        return false;
    }
    if (firstResponder == view) {
        return true;
    }

    // Some widgets, like text fields, secure text fields, text areas, and selects
    // (when displayed using a list box) may have a descendent widget that is
    // first responder. This checksDescendantsForFocus() check, turned on for the 
    // four widget types listed, enables the additional check which makes this 
    // function work correctly for the above-mentioned widget types.
    if (checksDescendantsForFocus() && 
        [firstResponder isKindOfClass:[NSView class]] && 
        [(NSView *)firstResponder isDescendantOf:view]) {
        // Return true when the first responder is a subview of this widget's view
        return true;
    }

    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QWidget::setFocus()
{
    if (hasFocus()) {
        return;
    }

    if (deferFirstResponderChanges) {
        deferredFirstResponder = this;
        return;
    }

    KWQ_BLOCK_EXCEPTIONS;
    NSView *view = [getView() _webcore_effectiveFirstResponder];
    if ([view superview] && [view acceptsFirstResponder]) {
        WebCoreBridge *bridge = MacFrame::bridgeForWidget(this);
        NSResponder *oldFirstResponder = [bridge firstResponder];

        [bridge makeFirstResponder:view];

        // Setting focus can actually cause a style change which might
        // remove the view from its superview while it's being made
        // first responder. This confuses AppKit so we must restore
        // the old first responder.

        if (![view superview]) {
            [bridge makeFirstResponder:oldFirstResponder];
        }
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::clearFocus()
{
    if (!hasFocus()) {
        return;
    }
    
    MacFrame::clearDocumentFocus(this);
}

bool QWidget::checksDescendantsForFocus() const
{
    return false;
}

QWidget::FocusPolicy QWidget::focusPolicy() const
{
    // This provides support for controlling the widgets that take 
    // part in tab navigation. Widgets must:
    // 1. not be hidden by css
    // 2. be enabled
    // 3. accept first responder

    RenderWidget *widget = const_cast<RenderWidget *>
	(static_cast<const RenderWidget *>(eventFilterObject()));
    if (widget->style()->visibility() != khtml::VISIBLE)
        return NoFocus;

    if (!isEnabled())
        return NoFocus;
    
    KWQ_BLOCK_EXCEPTIONS;
    if (![getView() acceptsFirstResponder])
        return NoFocus;
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return TabFocus;
}

const QPalette& QWidget::palette() const
{
    return data->pal;
}

void QWidget::setPalette(const QPalette &palette)
{
    data->pal = palette;
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
    
    KWQ_BLOCK_EXCEPTIONS;
    return [[MacFrame::bridgeForWidget(this) window] isVisible];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QWidget::setCursor(const QCursor &cur)
{
    KWQ_BLOCK_EXCEPTIONS;
    for (id view = data->view; view; view = [view superview]) {
        if ([view respondsToSelector:@selector(setDocumentCursor:)]) {
            if ([view respondsToSelector:@selector(documentCursor)] && cur.handle() == [view documentCursor])
                break;
            [view setDocumentCursor:cur.handle()];
            break;
        }
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QWidget::event(QEvent *)
{
    return false;
}

void QWidget::show()
{
    if (!data || data->visible)
        return;

    data->visible = true;

    KWQ_BLOCK_EXCEPTIONS;
    [getOuterView() setHidden: NO];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::hide()
{
    if (!data || !data->visible)
        return;

    data->visible = false;

    KWQ_BLOCK_EXCEPTIONS;
    [getOuterView() setHidden: YES];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::setFrameGeometry(const QRect &rect)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSView *v = getOuterView();
    NSRect f = rect;
    if (!NSEqualRects(f, [v frame])) {
        [v setFrame:f];
        [v setNeedsDisplay: NO];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

QPoint QWidget::mapFromGlobal(const QPoint &p) const
{
    NSPoint bp = {0,0};

    KWQ_BLOCK_EXCEPTIONS;
    bp = [[MacFrame::bridgeForWidget(this) window] convertScreenToBase:[data->view convertPoint:p toView:nil]];
    return QPoint(bp);
    KWQ_UNBLOCK_EXCEPTIONS;
    return QPoint();
}

NSView *QWidget::getView() const
{
    return data->view;
}

void QWidget::setView(NSView *view)
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQRetain(view);
    KWQRelease(data->view);
    data->view = view;
    KWQ_UNBLOCK_EXCEPTIONS;
}

NSView *QWidget::getOuterView() const
{
    // A QScrollView is a widget normally used to represent a frame.
    // If this widget's view is a WebCoreFrameView the we resize its containing view, a WebFrameView.
    // The scroll view contained by the WebFrameView will be autosized.

    NSView *view = data->view;

    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view;
}

void QWidget::lockDrawingFocus()
{
    KWQ_BLOCK_EXCEPTIONS;
    [getView() lockFocus];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::unlockDrawingFocus()
{
    KWQ_BLOCK_EXCEPTIONS;
    [getView() unlockFocus];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::disableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    KWQ_BLOCK_EXCEPTIONS;
    [[getView() window] disableFlushWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::enableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    KWQ_BLOCK_EXCEPTIONS;
    NSWindow *window = [getView() window];
    [window enableFlushWindow];
    [window flushWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::setDrawingAlpha(float alpha)
{
    KWQ_BLOCK_EXCEPTIONS;
    CGContextSetAlpha((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], alpha);
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::paint(QPainter *p, const QRect &r)
{
    if (p->paintingDisabled()) {
        return;
    }
    NSView *view = getOuterView();
    // KWQTextArea and KWQTextField both rely on the fact that we use this particular
    // NSView display method. If you change this, be sure to update them as well.
    KWQ_BLOCK_EXCEPTIONS;
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::sendConsumedMouseUp()
{
    khtml::RenderWidget *widget = const_cast<khtml::RenderWidget *>
	(static_cast<const khtml::RenderWidget *>(eventFilterObject()));

    KWQ_BLOCK_EXCEPTIONS;
    widget->sendConsumedMouseUp();
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::setIsSelected(bool isSelected)
{
    [MacFrame::bridgeForWidget(this) setIsSelected:isSelected forView:getView()];
}

void QWidget::addToSuperview(NSView *superview)
{
    KWQ_BLOCK_EXCEPTIONS;

    ASSERT(superview);
    NSView *subview = getOuterView();
    ASSERT(![superview isDescendantOf:subview]);
    if ([subview superview] != superview)
        [superview addSubview:subview];
    data->removeFromSuperviewSoon = false;

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QWidget::removeFromSuperview()
{
    if (data->mustStayInWindow)
        data->removeFromSuperviewSoon = true;
    else {
        data->removeFromSuperviewSoon = false;
        KWQ_BLOCK_EXCEPTIONS;
        [getOuterView() removeFromSuperview];
        KWQ_UNBLOCK_EXCEPTIONS;
    }
}

void QWidget::beforeMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(KWQWidgetHolder)]);
    QWidget *widget = [(NSView <KWQWidgetHolder> *)view widget];
    if (widget) {
        ASSERT(view == widget->getOuterView());
        ASSERT(!widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = true;
    }
}

void QWidget::afterMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(KWQWidgetHolder)]);
    QWidget *widget = [(NSView <KWQWidgetHolder> *)view widget];
    if (!widget) {
        KWQ_BLOCK_EXCEPTIONS;
        [view removeFromSuperview];
        KWQ_UNBLOCK_EXCEPTIONS;
    } else {
        ASSERT(widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = false;
        if (widget->data->removeFromSuperviewSoon)
            widget->removeFromSuperview();
    }
}

void QWidget::setDeferFirstResponderChanges(bool defer)
{
    deferFirstResponderChanges = defer;
    if (!defer) {
        QWidget *r = deferredFirstResponder;
        deferredFirstResponder = 0;
        if (r) {
            r->setFocus();
        }
    }
}
