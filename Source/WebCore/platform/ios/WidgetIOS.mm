/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#if PLATFORM(IOS_FAMILY)

#import "Cursor.h"
#import "Document.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "HostWindow.h"
#import "PlatformMouseEvent.h"
#import "ScrollView.h"
#import "WAKScrollView.h"
#import "WAKView.h"
#import "WAKWindow.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import <wtf/BlockObjCExceptions.h>

@interface NSView (WebSetSelectedMethods)
- (void)setIsSelected:(BOOL)isSelected;
- (void)webPlugInSetIsSelected:(BOOL)isSelected;
@end

namespace WebCore {

static void safeRemoveFromSuperview(NSView *view)
{
    // FIXME: we should probably change the first responder of the window if
    // it is a descendant of the view we remove.
    // See: <rdar://problem/10360186>
    [view removeFromSuperview];
}

Widget::Widget(NSView* view)
{
    init(view);
}

Widget::~Widget()
{
}

// FIXME: Should move this to Chrome; bad layering that this knows about Frame.
void Widget::setFocus(bool focused)
{
    UNUSED_PARAM(focused);
}

void Widget::setCursor(const Cursor& cursor)
{
    FrameView* view = root();
    if (!view)
        return;
    view->hostWindow()->setCursor(cursor);
}

void Widget::show()
{
    if (isSelfVisible())
        return;

    setSelfVisible(true);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [getOuterView() setHidden:NO];
    END_BLOCK_OBJC_EXCEPTIONS
}

void Widget::hide()
{
    if (!isSelfVisible())
        return;

    setSelfVisible(false);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [getOuterView() setHidden:YES];
    END_BLOCK_OBJC_EXCEPTIONS
}

IntRect Widget::frameRect() const
{
    if (!platformWidget())
        return m_frame;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return enclosingIntRect([getOuterView() frame]);
    END_BLOCK_OBJC_EXCEPTIONS
    
    return m_frame;
}

void Widget::setFrameRect(const IntRect &rect)
{
    m_frame = rect;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    NSView *v = getOuterView();
    NSRect f = rect;
    if (!NSEqualRects(f, [v frame])) {
        [v setFrame:f];
        [v setNeedsDisplay: NO];
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

NSView* Widget::getOuterView() const
{
    NSView* view = platformWidget();

    // If this widget's view is a WebCoreFrameScrollView then we
    // resize its containing view, a WebFrameView.
    if ([view conformsToProtocol:@protocol(WebCoreFrameScrollView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view;
}

void Widget::paint(GraphicsContext& p, const IntRect& r, SecurityOriginPaintPolicy, EventRegionContext*)
{
    if (p.paintingDisabled())
        return;
    
    NSView *view = getOuterView();

    CGContextRef cgContext = p.platformContext();
    CGContextSaveGState(cgContext);

    NSRect viewFrame = [view frame];
    NSRect viewBounds = [view bounds];
    CGContextTranslateCTM(cgContext, viewFrame.origin.x - viewBounds.origin.x, viewFrame.origin.y - viewBounds.origin.y);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]] inContext:cgContext];
    END_BLOCK_OBJC_EXCEPTIONS

    CGContextRestoreGState(cgContext);
}

void Widget::setIsSelected(bool /*isSelected*/)
{
}

void Widget::addToSuperview(NSView *view)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(view);
    NSView *subview = getOuterView();

    if (!subview)
        return;

    ASSERT(![view isDescendantOf:subview]);
    
    if ([subview superview] != view)
        [view addSubview:subview];

    END_BLOCK_OBJC_EXCEPTIONS
}

void Widget::removeFromSuperview()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    safeRemoveFromSuperview(getOuterView());
    END_BLOCK_OBJC_EXCEPTIONS
}

IntRect Widget::convertFromRootToContainingWindow(const Widget* rootWidget, const IntRect& rect)
{
    if (!rootWidget->platformWidget())
        return rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT([rootWidget->platformWidget() isKindOfClass:[NSScrollView class]]);
    WAKScrollView *view = static_cast<WAKScrollView *>(rootWidget->platformWidget());
    if (WAKView *documentView = [view documentView])
        return enclosingIntRect([documentView convertRect:rect toView:nil]);
    return enclosingIntRect([view convertRect:rect toView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS

    return rect;
}

IntRect Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, const IntRect& rect)
{
    if (!rootWidget->platformWidget())
        return rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT([rootWidget->platformWidget() isKindOfClass:[NSScrollView class]]);
    WAKScrollView *view = static_cast<WAKScrollView *>(rootWidget->platformWidget());
    if (WAKView *documentView = [view documentView])
        return enclosingIntRect([documentView convertRect:rect fromView:nil]);
    return enclosingIntRect([view convertRect:rect fromView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS

    return rect;
}

IntPoint Widget::convertFromRootToContainingWindow(const Widget* rootWidget, const IntPoint& point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT([rootWidget->platformWidget() isKindOfClass:[NSScrollView class]]);
    WAKScrollView *view = static_cast<WAKScrollView *>(rootWidget->platformWidget());
    NSPoint convertedPoint;
    if (WAKView *documentView = [view documentView])
        convertedPoint = [documentView convertPoint:point toView:nil];
    else
        convertedPoint = [view convertPoint:point toView:nil];
    return IntPoint(roundf(convertedPoint.x), roundf(convertedPoint.y));
    END_BLOCK_OBJC_EXCEPTIONS

    return point;
}

IntPoint Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, const IntPoint& point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT([rootWidget->platformWidget() isKindOfClass:[NSScrollView class]]);
    WAKScrollView *view = static_cast<WAKScrollView *>(rootWidget->platformWidget());
    NSPoint convertedPoint;
    if (WAKView *documentView = [view documentView])
        convertedPoint = IntPoint([documentView convertPoint:point fromView:nil]);
    else
        convertedPoint = IntPoint([view convertPoint:point fromView:nil]);
    return IntPoint(roundf(convertedPoint.x), roundf(convertedPoint.y));
    END_BLOCK_OBJC_EXCEPTIONS

    return point;
}

NSView *Widget::platformWidget() const
{
    return m_widget.get();
}

void Widget::setPlatformWidget(NSView *widget)
{
    if (widget == m_widget)
        return;

    m_widget = widget;
}

}

#endif // PLATFORM(IOS_FAMILY)
