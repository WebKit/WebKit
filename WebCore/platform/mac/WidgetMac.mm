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

#import "BlockExceptions.h"
#import "Cursor.h"
#import "Document.h"
#import "Font.h"
#import "Frame.h"
#import "GraphicsContext.h"
#import "Page.h"
#import "PlatformMouseEvent.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import "WidgetClient.h"

#import <wtf/RetainPtr.h>

@interface NSWindow (WindowPrivate)
- (BOOL) _needsToResetDragMargins;
- (void) _setNeedsToResetDragMargins:(BOOL)s;
@end

namespace WebCore {

class WidgetPrivate {
public:
    RetainPtr<NSView> view;
    WidgetClient* client;
    bool visible;
    bool mustStayInWindow;
    bool removeFromSuperviewSoon;
};

static void safeRemoveFromSuperview(NSView *view)
{
    // If the the view is the first responder, then set the window's first responder to nil so
    // we don't leave the window pointing to a view that's no longer in it.
    NSWindow *window = [view window];
    NSResponder *firstResponder = [window firstResponder];
    if ([firstResponder isKindOfClass:[NSView class]] && [(NSView *)firstResponder isDescendantOf:view])
        [window makeFirstResponder:nil];

    // Suppress the resetting of drag margins since we know we can't affect them.
    BOOL resetDragMargins = [window _needsToResetDragMargins];
    [window _setNeedsToResetDragMargins:NO];
    [view removeFromSuperview];
    [window _setNeedsToResetDragMargins:resetDragMargins];
}

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
    data->view = view;
    data->client = 0;
    data->visible = true;
    data->mustStayInWindow = false;
    data->removeFromSuperviewSoon = false;
}

Widget::~Widget() 
{
    delete data;
}

void Widget::setEnabled(bool enabled)
{
    id view = data->view.get();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([view respondsToSelector:@selector(setEnabled:)]) {
        [view setEnabled:enabled];
    }
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool Widget::isEnabled() const
{
    id view = data->view.get();

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

// FIXME: Should move this to Chrome; bad layering that this knows about Frame.
void Widget::setFocus()
{
    Frame* frame = Frame::frameForWidget(this);
    if (!frame)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
 
    NSView *view = [getView() _webcore_effectiveFirstResponder];
    if (Page* page = frame->page())
        page->chrome()->focusNSView(view);
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

 void Widget::setCursor(const Cursor& cursor)
 {
    if ([NSCursor currentCursor] == cursor.impl())
        return;
    [cursor.impl() set];
}

void Widget::show()
{
    if (!data || data->visible)
        return;

    data->visible = true;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden:NO];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::hide()
{
    if (!data || !data->visible)
        return;

    data->visible = false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getOuterView() setHidden:YES];
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

NSView* Widget::getView() const
{
    return data->view.get();
}

void Widget::setView(NSView* view)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    data->view = view;
    END_BLOCK_OBJC_EXCEPTIONS;
}

NSView* Widget::getOuterView() const
{
    // If this widget's view is a WebCoreFrameView the we resize its containing view, a WebFrameView.

    NSView* view = data->view.get();
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view;
}

void Widget::paint(GraphicsContext* p, const IntRect& r)
{
    if (p->paintingDisabled())
        return;
    NSView *view = getOuterView();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::invalidate()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getView() setNeedsDisplay: YES];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::invalidateRect(const IntRect& r)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [getView() setNeedsDisplayInRect: r];
    END_BLOCK_OBJC_EXCEPTIONS;
}

// FIXME: Should move this to Chrome; bad layering that this knows about Frame.
void Widget::setIsSelected(bool isSelected)
{
    if (Frame* frame = Frame::frameForWidget(this))
        [frame->bridge() setIsSelected:isSelected forView:getView()];
}

void Widget::addToSuperview(NSView *view)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(view);
    NSView *subview = getOuterView();
    ASSERT(![view isDescendantOf:subview]);
    
    // Suppress the resetting of drag margins since we know we can't affect them.
    NSWindow* window = [view window];
    BOOL resetDragMargins = [window _needsToResetDragMargins];
    [window _setNeedsToResetDragMargins:NO];
    if ([subview superview] != view)
        [view addSubview:subview];
    data->removeFromSuperviewSoon = false;
    [window _setNeedsToResetDragMargins:resetDragMargins];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void Widget::removeFromSuperview()
{
    if (data->mustStayInWindow)
        data->removeFromSuperviewSoon = true;
    else {
        data->removeFromSuperviewSoon = false;
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        safeRemoveFromSuperview(getOuterView());
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

void Widget::beforeMouseDown(NSView *view, Widget* widget)
{
    if (widget) {
        ASSERT(view == widget->getOuterView());
        ASSERT(!widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = true;
    }
}

void Widget::afterMouseDown(NSView *view, Widget* widget)
{
    if (!widget) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        safeRemoveFromSuperview(view);
        END_BLOCK_OBJC_EXCEPTIONS;
    } else {
        ASSERT(widget->data->mustStayInWindow);
        widget->data->mustStayInWindow = false;
        if (widget->data->removeFromSuperviewSoon)
            widget->removeFromSuperview();
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

void Widget::removeFromParent()
{
}

IntPoint Widget::convertToScreenCoordinate(NSView *view, const IntPoint& point)
{
    NSPoint conversionPoint = { point.x(), point.y() };
    conversionPoint = [view convertPoint:conversionPoint toView:nil];
    return globalPoint(conversionPoint, [view window]);
}

}
