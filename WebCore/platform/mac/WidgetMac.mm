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
#import "Font.h"
#import "FoundationExtras.h"
#import "GraphicsContext.h"
#import "BlockExceptions.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import "WebCoreWidgetHolder.h"
#import "WidgetClient.h"

namespace WebCore {

static bool deferFirstResponderChanges;
static Widget *deferredFirstResponder;

class WidgetPrivate
{
public:
    Font font;
    NSView* view;
    WidgetClient* client;
    bool visible;
    bool mustStayInWindow;
    bool removeFromSuperviewSoon;
};

Widget::Widget() : data(new WidgetPrivate)
{
    data->view = nil;
    data->client = 0;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::Widget(NSView* view) : data(new WidgetPrivate)
{
    data->view = KWQRetain(view);
    data->client = 0;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::~Widget() 
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    KWQRelease(data->view);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (deferredFirstResponder == this)
        deferredFirstResponder = 0;

    delete data;
}

void Widget::setActiveWindow() 
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [FrameMac::bridgeForWidget(this) focusWindow];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::setEnabled(bool enabled)
{
    id view = data->view;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view respondsToSelector:@selector(setEnabled:)]) {
        [view setEnabled:enabled];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool Widget::isEnabled() const
{
    id view = data->view;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view respondsToSelector:@selector(isEnabled)]) {
        return [view isEnabled];
    }
    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

IntRect Widget::frameGeometry() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return enclosingIntRect([getOuterView() frame]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntRect();
}

bool Widget::hasFocus() const
{
    if (deferFirstResponderChanges && deferredFirstResponder) {
        return this == deferredFirstResponder;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *view = [getView() _webcore_effectiveFirstResponder];

    id firstResponder = [FrameMac::bridgeForWidget(this) firstResponder];

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

    END_BLOCK_OBJC_EXCEPTIONS;

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

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *view = [getView() _webcore_effectiveFirstResponder];
    if ([view superview] && [view acceptsFirstResponder]) {
        WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(this);
        NSResponder *oldFirstResponder = [bridge firstResponder];

        [bridge makeFirstResponder:view];

        // Setting focus can actually cause a style change which might
        // remove the view from its superview while it's being made
        // first responder. This confuses AppKit so we must restore
        // the old first responder.

        if (![view superview])
            [bridge makeFirstResponder:oldFirstResponder];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::clearFocus()
{
    if (!hasFocus())
        return;
    FrameMac::clearDocumentFocus(this);
}

Widget::FocusPolicy Widget::focusPolicy() const
{
    // This provides support for controlling the widgets that take 
    // part in tab navigation. Widgets must:
    // 1. not be hidden by css
    // 2. be enabled
    // 3. accept first responder

    if (!client())
        return NoFocus;
    if (!client()->isVisible(const_cast<Widget*>(this)))
        return NoFocus;
    if (!isEnabled())
        return NoFocus;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (![getView() acceptsFirstResponder])
        return NoFocus;
    END_BLOCK_OBJC_EXCEPTIONS;

    return TabFocus;
}

const Font& Widget::font() const
{
    return data->font;
}

void Widget::setFont(const Font& font)
{
    data->font = font;
}

void Widget::setCursor(const Cursor& cursor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    for (id view = data->view; view; view = [view superview]) {
        if ([view respondsToSelector:@selector(setDocumentCursor:)]) {
            if ([view respondsToSelector:@selector(documentCursor)] && cursor.impl() == [view documentCursor])
                break;
            [view setDocumentCursor:cursor.impl()];
            break;
        }
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::show()
{
    if (!data || data->visible)
        return;

    data->visible = true;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden: NO];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::hide()
{
    if (!data || !data->visible)
        return;

    data->visible = false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden: YES];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::setFrameGeometry(const IntRect &rect)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSView *v = getOuterView();
    NSRect f = rect;
    if (!NSEqualRects(f, [v frame])) {
        [v setFrame:f];
        [v setNeedsDisplay: NO];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

IntPoint Widget::mapFromGlobal(const IntPoint &p) const
{
    NSPoint bp = {0,0};

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    bp = [[FrameMac::bridgeForWidget(this) window] convertScreenToBase:[data->view convertPoint:p toView:nil]];
    return IntPoint(bp);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntPoint();
}

NSView* Widget::getView() const
{
    return data->view;
}

void Widget::setView(NSView* view)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    KWQRetain(view);
    KWQRelease(data->view);
    data->view = view;
    END_BLOCK_OBJC_EXCEPTIONS;
}

NSView* Widget::getOuterView() const
{
    // If this widget's view is a WebCoreFrameView the we resize its containing view, a WebFrameView.

    NSView* view = data->view;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view;
}

// FIXME: Get rid of the single use of these next two functions (frame resizing), and remove them.

GraphicsContext* Widget::lockDrawingFocus()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getView() lockFocus];
    END_BLOCK_OBJC_EXCEPTIONS;
    PlatformGraphicsContext* platformContext = static_cast<PlatformGraphicsContext*>([[NSGraphicsContext currentContext] graphicsPort]);
    ASSERT([[NSGraphicsContext currentContext] isFlipped]);
    return new GraphicsContext(platformContext);
}

void Widget::unlockDrawingFocus(GraphicsContext* context)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getView() unlockFocus];
    END_BLOCK_OBJC_EXCEPTIONS;
    delete context;
}

void Widget::disableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [[getView() window] disableFlushWindow];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::enableFlushDrawing()
{
    // It's OK to use the real window here, because if the view's not
    // in the view hierarchy, then we don't actually want to affect
    // flushing.
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSWindow* window = [getView() window];
    [window enableFlushWindow];
    [window flushWindow];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
    NSView *view = getOuterView();
    // WebCoreTextArea and WebCoreTextField both rely on the fact that we use this particular
    // NSView display method. If you change this, be sure to update them as well.
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::sendConsumedMouseUp()
{
    if (client())
        client()->sendConsumedMouseUp(this);
}

void Widget::setIsSelected(bool isSelected)
{
    [FrameMac::bridgeForWidget(this) setIsSelected:isSelected forView:getView()];
}

void Widget::addToSuperview(NSView *superview)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(superview);
    NSView *subview = getOuterView();
    ASSERT(![superview isDescendantOf:subview]);
    if ([subview superview] != superview)
        [superview addSubview:subview];
    data->removeFromSuperviewSoon = false;

    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::removeFromSuperview()
{
    if (data->mustStayInWindow)
        data->removeFromSuperviewSoon = true;
    else {
        data->removeFromSuperviewSoon = false;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [getOuterView() removeFromSuperview];
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

void Widget::beforeMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(WebCoreWidgetHolder)]);
    Widget* widget = [(NSView <WebCoreWidgetHolder> *)view widget];
    if (widget) {
        ASSERT(view == widget->getOuterView());
        ASSERT(!widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = true;
    }
}

void Widget::afterMouseDown(NSView *view)
{
    ASSERT([view conformsToProtocol:@protocol(WebCoreWidgetHolder)]);
    Widget* widget = [(NSView <WebCoreWidgetHolder>*)view widget];
    if (!widget) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [view removeFromSuperview];
        END_BLOCK_OBJC_EXCEPTIONS;
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

void Widget::setClient(WidgetClient* c)
{
    data->client = c;
}

WidgetClient* Widget::client() const
{
    return data->client;
}

}
