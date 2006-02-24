/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "Widget.h"

#import "Cursor.h"
#import "FoundationExtras.h"
#import "KWQExceptions.h"
#import "KWQView.h"
#import "MacFrame.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import "render_replaced.h"
#import <qpalette.h>

namespace WebCore {

static bool deferFirstResponderChanges;
static Widget *deferredFirstResponder;

class WidgetPrivate
{
public:
    QFont font;
    QPalette pal;
    NSView* view;
    bool visible;
    bool mustStayInWindow;
    bool removeFromSuperviewSoon;
};

Widget::Widget() : data(new WidgetPrivate)
{
    data->view = nil;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::Widget(NSView* view) : data(new WidgetPrivate)
{
    data->view = KWQRetain(view);
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::~Widget() 
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQRelease(data->view);
    KWQ_UNBLOCK_EXCEPTIONS;

    if (deferredFirstResponder == this)
        deferredFirstResponder = 0;

    delete data;
}

void Widget::setActiveWindow() 
{
    KWQ_BLOCK_EXCEPTIONS;
    [MacFrame::bridgeForWidget(this) focusWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::setEnabled(bool enabled)
{
    id view = data->view;
    KWQ_BLOCK_EXCEPTIONS;
    if ([view respondsToSelector:@selector(setEnabled:)]) {
        [view setEnabled:enabled];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool Widget::isEnabled() const
{
    id view = data->view;

    KWQ_BLOCK_EXCEPTIONS;
    if ([view respondsToSelector:@selector(isEnabled)]) {
        return [view isEnabled];
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

IntRect Widget::frameGeometry() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return enclosingIntRect([getOuterView() frame]);
    KWQ_UNBLOCK_EXCEPTIONS;
    return IntRect();
}

bool Widget::hasFocus() const
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

void Widget::setFocus()
{
    if (hasFocus())
        return;

    if (deferFirstResponderChanges) {
        deferredFirstResponder = this;
        return;
    }

    KWQ_BLOCK_EXCEPTIONS;
    NSView *view = [getView() _webcore_effectiveFirstResponder];
    if ([view superview] && [view acceptsFirstResponder]) {
        WebCoreFrameBridge *bridge = MacFrame::bridgeForWidget(this);
        NSResponder *oldFirstResponder = [bridge firstResponder];

        [bridge makeFirstResponder:view];

        // Setting focus can actually cause a style change which might
        // remove the view from its superview while it's being made
        // first responder. This confuses AppKit so we must restore
        // the old first responder.

        if (![view superview])
            [bridge makeFirstResponder:oldFirstResponder];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::clearFocus()
{
    if (!hasFocus())
        return;
    MacFrame::clearDocumentFocus(this);
}

Widget::FocusPolicy Widget::focusPolicy() const
{
    // This provides support for controlling the widgets that take 
    // part in tab navigation. Widgets must:
    // 1. not be hidden by css
    // 2. be enabled
    // 3. accept first responder

    RenderWidget *widget = const_cast<RenderWidget *>
        (static_cast<const RenderWidget *>(eventFilterObject()));
    if (widget->style()->visibility() != VISIBLE)
        return NoFocus;

    if (!isEnabled())
        return NoFocus;
    
    KWQ_BLOCK_EXCEPTIONS;
    if (![getView() acceptsFirstResponder])
        return NoFocus;
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return TabFocus;
}

const QPalette& Widget::palette() const
{
    return data->pal;
}

void Widget::setPalette(const QPalette &palette)
{
    data->pal = palette;
}

QFont Widget::font() const
{
    return data->font;
}

void Widget::setFont(const QFont &font)
{
    data->font = font;
}

void Widget::setCursor(const Cursor& cursor)
{
    KWQ_BLOCK_EXCEPTIONS;
    for (id view = data->view; view; view = [view superview]) {
        if ([view respondsToSelector:@selector(setDocumentCursor:)]) {
            if ([view respondsToSelector:@selector(documentCursor)] && cursor.impl() == [view documentCursor])
                break;
            [view setDocumentCursor:cursor.impl()];
            break;
        }
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::show()
{
    if (!data || data->visible)
        return;

    data->visible = true;

    KWQ_BLOCK_EXCEPTIONS;
    [getOuterView() setHidden: NO];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::hide()
{
    if (!data || !data->visible)
        return;

    data->visible = false;

    KWQ_BLOCK_EXCEPTIONS;
    [getOuterView() setHidden: YES];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::setFrameGeometry(const IntRect &rect)
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

IntPoint Widget::mapFromGlobal(const IntPoint &p) const
{
    NSPoint bp = {0,0};

    KWQ_BLOCK_EXCEPTIONS;
    bp = [[MacFrame::bridgeForWidget(this) window] convertScreenToBase:[data->view convertPoint:p toView:nil]];
    return IntPoint(bp);
    KWQ_UNBLOCK_EXCEPTIONS;
    return IntPoint();
}

NSView* Widget::getView() const
{
    return data->view;
}

void Widget::setView(NSView* view)
{
    KWQ_BLOCK_EXCEPTIONS;
    KWQRetain(view);
    KWQRelease(data->view);
    data->view = view;
    KWQ_UNBLOCK_EXCEPTIONS;
}

NSView* Widget::getOuterView() const
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

void Widget::lockDrawingFocus()
{
    KWQ_BLOCK_EXCEPTIONS;
    [getView() lockFocus];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::unlockDrawingFocus()
{
    KWQ_BLOCK_EXCEPTIONS;
    [getView() unlockFocus];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::disableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    KWQ_BLOCK_EXCEPTIONS;
    [[getView() window] disableFlushWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::enableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    KWQ_BLOCK_EXCEPTIONS;
    NSWindow* window = [getView() window];
    [window enableFlushWindow];
    [window flushWindow];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::setDrawingAlpha(float alpha)
{
    KWQ_BLOCK_EXCEPTIONS;
    CGContextSetAlpha((CGContextRef)[[NSGraphicsContext currentContext] graphicsPort], alpha);
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::paint(QPainter *p, const IntRect &r)
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

void Widget::sendConsumedMouseUp()
{
    RenderWidget* widget = const_cast<RenderWidget *>
        (static_cast<const RenderWidget *>(eventFilterObject()));

    KWQ_BLOCK_EXCEPTIONS;
    widget->sendConsumedMouseUp();
    KWQ_UNBLOCK_EXCEPTIONS;
}

void Widget::setIsSelected(bool isSelected)
{
    [MacFrame::bridgeForWidget(this) setIsSelected:isSelected forView:getView()];
}

void Widget::addToSuperview(NSView *superview)
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

void Widget::removeFromSuperview()
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

void Widget::beforeMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(KWQWidgetHolder)]);
    Widget* widget = [(NSView <KWQWidgetHolder> *)view widget];
    if (widget) {
        ASSERT(view == widget->getOuterView());
        ASSERT(!widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = true;
    }
}

void Widget::afterMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(KWQWidgetHolder)]);
    Widget* widget = [(NSView <KWQWidgetHolder>*)view widget];
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

void Widget::setDeferFirstResponderChanges(bool defer)
{
    deferFirstResponderChanges = defer;
    if (!defer) {
        Widget* r = deferredFirstResponder;
        deferredFirstResponder = 0;
        if (r) {
            r->setFocus();
        }
    }
}

}
