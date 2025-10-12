/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "Chrome.h"
#import "Cursor.h"
#import "DocumentPage.h"
#import "FontCascade.h"
#import "GraphicsContext.h"
#import "LocalFrameInlines.h"
#import "LocalFrameView.h"
#import "PlatformMouseEvent.h"
#import "WebCoreFrameView.h"
#import "WebCoreView.h"
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@interface NSWindow (WebWindowDetails)
- (BOOL)_needsToResetDragMargins;
- (void)_setNeedsToResetDragMargins:(BOOL)needs;
@end

@interface NSView (WebSetSelectedMethods)
- (void)setIsSelected:(BOOL)isSelected;
- (void)webPlugInSetIsSelected:(BOOL)isSelected;
@end

@interface NSView (Widget)
- (void)visibleRectDidChange;
@end

namespace WebCore {

static void safeRemoveFromSuperview(NSView *view)
{
    // If the view is the first responder, then set the window's first responder to nil so
    // we don't leave the window pointing to a view that's no longer in it.
    NSWindow *window = [view window];
    auto *firstResponderView = dynamic_objc_cast<NSView>([window firstResponder]);
    if ([firstResponderView isDescendantOf:view])
        [window makeFirstResponder:nil];

    // Suppress the resetting of drag margins since we know we can't affect them.
    BOOL resetDragMargins = [window _needsToResetDragMargins];
    [window _setNeedsToResetDragMargins:NO];
    [view removeFromSuperview];
    [window _setNeedsToResetDragMargins:resetDragMargins];
}

Widget::Widget(NSView *view)
{
    init(view);
}

Widget::~Widget()
{
}

// FIXME: Should move this to Chrome; bad layering that this knows about Frame.
void Widget::setFocus(bool focused)
{
    if (!focused)
        return;

    RefPtr frame = LocalFrame::frameForWidget(*this);
    if (!frame)
        return;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
 
    // Call this even when there is no platformWidget(). WK2 will focus on the widget in the UIProcess.
    RetainPtr view = [protectedPlatformWidget() _webcore_effectiveFirstResponder];
    if (RefPtr page = frame->page())
        page->chrome().focusNSView(view.get());

    END_BLOCK_OBJC_EXCEPTIONS
}

void Widget::show()
{
    if (isSelfVisible())
        return;

    setSelfVisible(true);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [protectedOuterView() setHidden:NO];
    END_BLOCK_OBJC_EXCEPTIONS
}

void Widget::hide()
{
    if (!isSelfVisible())
        return;

    setSelfVisible(false);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [protectedOuterView() setHidden:YES];
    END_BLOCK_OBJC_EXCEPTIONS
}

IntRect Widget::frameRect() const
{
    if (!platformWidget())
        return m_frame;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return enclosingIntRect([protectedOuterView() frame]);
    END_BLOCK_OBJC_EXCEPTIONS
    
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr outerView = this->outerView();
    if (!outerView)
        return;

    // Take a reference to this Widget, because sending messages to outerView can invoke arbitrary
    // code including recalc style/layout, which can deref it.
    Ref<Widget> protectedThis(*this);

    NSRect frame = rect;
    if (!NSEqualRects(frame, [outerView frame])) {
        outerView.get().frame = frame;
        outerView.get().needsDisplay = NO;
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

NSView *Widget::outerView() const
{
    RetainPtr view = platformWidget();

    // If this widget's view is a WebCoreFrameScrollView then we
    // resize its containing view, a WebFrameView.
    // No need to protect @protocol().
    SUPPRESS_UNCOUNTED_ARG if ([view conformsToProtocol:@protocol(WebCoreFrameScrollView)]) {
        view = [view superview];
        ASSERT(view);
    }

    return view.get();
}

RetainPtr<NSView> Widget::protectedOuterView() const
{
    return outerView();
}

void Widget::paint(GraphicsContext& p, const IntRect& r, SecurityOriginPaintPolicy, RegionContext*)
{
    if (p.paintingDisabled())
        return;
    RetainPtr view = outerView();

    // We don't want to paint the view at all if it's layer backed, because then we'll end up
    // with multiple copies of the view contents, one in the view's layer itself and one in the
    // WebHTMLView's backing store (either a layer or the window backing store).
    if (view.get().layer) {
#if PLATFORM(MAC)
        // However, Quicken Essentials has a plug-in that depends on drawing to update the layer (see <rdar://problem/15221231>).
        if (!WTF::MacApplication::isQuickenEssentials())
#endif
        return;
    }

    // Take a reference to this Widget, because sending messages to the views can invoke arbitrary
    // code, which can deref it.
    Ref<Widget> protectedThis(*this);

    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (currentContext == [[view window] graphicsContext] || ![currentContext isDrawingToScreen]) {
ALLOW_DEPRECATED_DECLARATIONS_END
        // This is the common case of drawing into a window or an inclusive layer, or printing.
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]]];
        END_BLOCK_OBJC_EXCEPTIONS
        return;
    }

    // This is the case of drawing into a bitmap context other than a window backing store. It gets hit beneath
    // -cacheDisplayInRect:toBitmapImageRep:, and when painting into compositing layers.

    // Transparent subframes are in fact implemented with scroll views that return YES from -drawsBackground (whenever the WebView
    // itself is in drawsBackground mode). In the normal drawing code path, the scroll views are never asked to draw the background,
    // so this is not an issue, but in this code path they are, so the following code temporarily turns background drwaing off.
    RetainPtr innerView = platformWidget();
    // No need to retain @protocol.
    SUPPRESS_UNCOUNTED_ARG if ([innerView conformsToProtocol:@protocol(WebCoreFrameScrollView)]) {
        RetainPtr scrollView = checked_objc_cast<NSScrollView>(innerView.get());
        // -copiesOnScroll will return NO whenever the content view is not fully opaque.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([scrollView drawsBackground] && ![[scrollView contentView] copiesOnScroll])
            [scrollView setDrawsBackground:NO];
ALLOW_DEPRECATED_DECLARATIONS_END
    }

    RetainPtr cgContext = p.platformContext();
    ASSERT(cgContext == [currentContext CGContext]);
    CGContextSaveGState(cgContext.get());

    NSRect viewFrame = [view frame];
    NSRect viewBounds = [view bounds];

    // Set up the translation and (flipped) orientation of the graphics context. In normal drawing, AppKit does it as it descends down
    // the view hierarchy. Since Widget::paint is always called with a context that has a flipped coordinate system, and
    // -[NSView displayRectIgnoringOpacity:inContext:] expects an unflipped context we always flip here.
    CGContextTranslateCTM(cgContext.get(), viewFrame.origin.x - viewBounds.origin.x, viewFrame.origin.y + viewFrame.size.height + viewBounds.origin.y);
    CGContextScaleCTM(cgContext.get(), 1, -1);

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    {
        NSGraphicsContext *nsContext = [NSGraphicsContext graphicsContextWithCGContext:cgContext.get() flipped:NO];
        [view displayRectIgnoringOpacity:[view convertRect:r fromView:[view superview]] inContext:nsContext];
    }
    END_BLOCK_OBJC_EXCEPTIONS

    CGContextRestoreGState(cgContext.get());
}

void Widget::setIsSelected(bool isSelected)
{
    RetainPtr view = platformWidget();

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if ([view respondsToSelector:@selector(webPlugInSetIsSelected:)])
        [view webPlugInSetIsSelected:isSelected];
    else if ([view respondsToSelector:@selector(setIsSelected:)])
        [view setIsSelected:isSelected];
    END_BLOCK_OBJC_EXCEPTIONS
}

void Widget::removeFromSuperview()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    safeRemoveFromSuperview(protectedOuterView().get());
    END_BLOCK_OBJC_EXCEPTIONS
}

// MARK: -

// These are here to deal with flipped coords on Mac.

IntPoint Widget::convertFromRootToContainingWindow(const Widget* rootWidget, IntPoint point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return IntPoint([rootWidget->protectedPlatformWidget() convertPoint:point toView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS
    return point;
}

FloatPoint Widget::convertFromRootToContainingWindow(const Widget* rootWidget, FloatPoint point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [rootWidget->protectedPlatformWidget() convertPoint:point toView:nil];
    END_BLOCK_OBJC_EXCEPTIONS
    return point;
}

IntRect Widget::convertFromRootToContainingWindow(const Widget* rootWidget, const IntRect& rect)
{
    if (!rootWidget->platformWidget())
        return rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return enclosingIntRect([rootWidget->protectedPlatformWidget() convertRect:rect toView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS

    return rect;
}

FloatRect Widget::convertFromRootToContainingWindow(const Widget* rootWidget, const FloatRect& rect)
{
    if (!rootWidget->platformWidget())
        return rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [rootWidget->protectedPlatformWidget() convertRect:rect toView:nil];
    END_BLOCK_OBJC_EXCEPTIONS

    return rect;
}

// MARK: -

IntPoint Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, IntPoint point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return IntPoint([rootWidget->protectedPlatformWidget() convertPoint:point fromView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS

    return point;
}

FloatPoint Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, FloatPoint point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [rootWidget->protectedPlatformWidget() convertPoint:point fromView:nil];
    END_BLOCK_OBJC_EXCEPTIONS

    return point;
}

DoublePoint Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, DoublePoint point)
{
    if (!rootWidget->platformWidget())
        return point;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [rootWidget->protectedPlatformWidget() convertPoint:point fromView:nil];
    END_BLOCK_OBJC_EXCEPTIONS

    return point;
}

IntRect Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, const IntRect& rect)
{
    if (!rootWidget->platformWidget())
        return rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return enclosingIntRect([rootWidget->protectedPlatformWidget() convertRect:rect fromView:nil]);
    END_BLOCK_OBJC_EXCEPTIONS

    return rect;
}

FloatRect Widget::convertFromContainingWindowToRoot(const Widget* rootWidget, const FloatRect& rect)
{
    if (!rootWidget->platformWidget())
        return rect;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    return [rootWidget->protectedPlatformWidget() convertRect:rect fromView:nil];
    END_BLOCK_OBJC_EXCEPTIONS

    return rect;
}

// MARK: -

NSView *Widget::platformWidget() const
{
    return m_widget.get();
}

RetainPtr<NSView> Widget::protectedPlatformWidget() const
{
    return platformWidget();
}

void Widget::setPlatformWidget(NSView *widget)
{
    if (widget == m_widget)
        return;

    m_widget = widget;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
