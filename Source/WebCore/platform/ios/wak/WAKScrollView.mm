/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WAKScrollView.h"

#if PLATFORM(IOS_FAMILY)

#import "WAKAppKitStubs.h"
#import "WAKClipView.h"
#import "WAKViewInternal.h"
#import "WAKWindow.h"
#import "WebEvent.h"

@interface WAKClipView(PrivateAPI)
- (void)_setDocumentView:(WAKView *)aView;
@end

// FIXME: get rid of this and use polymorphic response to notifications.
@interface WAKScrollView()
- (void)_adjustScrollers;
@end

static void _notificationCallback(WKViewRef v, WKViewNotificationType type, void *userInfo)
{
    UNUSED_PARAM(v);
    switch (type){
        case WKViewNotificationViewFrameSizeChanged: {
            WAKScrollView *scrollView = (WAKScrollView *)userInfo;
            ASSERT(scrollView);
            ASSERT([scrollView isKindOfClass:[WAKScrollView class]]);
            [scrollView _adjustScrollers];
            break;
        }
        default: {
            break;
        }
    }
}

@implementation WAKScrollView

@synthesize delegate;

- (id)initWithFrame:(CGRect)rect
{
    WKViewRef view = WKViewCreateWithFrame(rect, &viewContext);
    viewContext.notificationCallback = _notificationCallback;
    viewContext.notificationUserInfo = self;
    self = [super _initWithViewRef:(WKViewRef)view];
    WKRelease(view);

    _contentView = [[WAKClipView alloc] initWithFrame:rect];
    [self addSubview:_contentView];

    return self;
}

- (void)dealloc
{
    [_documentView autorelease];
    [_contentView release];
    [super dealloc];
}

- (BOOL)_selfHandleEvent:(WebEvent *)event
{
    switch (event.type) {
        case WebEventScrollWheel:
            [self scrollWheel:event];
            return YES;
        default:
            return NO;
    }
}

- (void)setHasVerticalScroller:(BOOL)flag
{
    UNUSED_PARAM(flag);
}

- (BOOL)hasVerticalScroller
{
    return NO;
}

- (void)setHasHorizontalScroller:(BOOL)flag
{
    UNUSED_PARAM(flag);
}

- (BOOL)hasHorizontalScroller
{
    return NO;
}

- (CGRect)documentVisibleRect 
{
    return [_contentView documentVisibleRect];
}

- (void)setDocumentView:(WAKView *)view
{
    if (view != _documentView) {
        [_documentView release];
        _documentView = [view retain];
        [_contentView _setDocumentView:view];
    }
}

- (id)documentView 
{
    return _documentView;
}

- (WAKClipView *)contentView 
{ 
    return _contentView;
}

- (void)setDrawsBackground:(BOOL)flag
{
    UNUSED_PARAM(flag);
}

- (BOOL)drawsBackground 
{
    return NO; 
}

- (void)setLineScroll:(float)value
{
    UNUSED_PARAM(value);
}

- (float)verticalLineScroll
{
    return 0;
}

- (float)horizontalLineScroll 
{ 
    return 0;
}

- (void)reflectScrolledClipView:(WAKClipView *)aClipView
{
    UNUSED_PARAM(aClipView);
}

- (void)drawRect:(CGRect)rect
{
    UNUSED_PARAM(rect);
}

// WebCoreFrameView methods

- (void)setHorizontalScrollingMode:(WebCore::ScrollbarMode)mode
{
    UNUSED_PARAM(mode);
}

- (void)setVerticalScrollingMode:(WebCore::ScrollbarMode)mode
{
    UNUSED_PARAM(mode);
}

- (void)setScrollingMode:(WebCore::ScrollbarMode)mode
{
    UNUSED_PARAM(mode);
}

- (WebCore::ScrollbarMode)horizontalScrollingMode
{
    return WebCore::ScrollbarAuto;
}

- (WebCore::ScrollbarMode)verticalScrollingMode
{
    return WebCore::ScrollbarAuto;
}

#pragma mark -
#pragma mark WebCoreFrameScrollView protocol

- (void)setScrollingModes:(WebCore::ScrollbarMode)hMode vertical:(WebCore::ScrollbarMode)vMode andLock:(BOOL)lock
{
    UNUSED_PARAM(hMode);
    UNUSED_PARAM(vMode);
    UNUSED_PARAM(lock);
}

- (void)scrollingModes:(WebCore::ScrollbarMode*)hMode vertical:(WebCore::ScrollbarMode*)vMode
{
    UNUSED_PARAM(hMode);
    UNUSED_PARAM(vMode);
}

- (void)setScrollBarsSuppressed:(BOOL)suppressed repaintOnUnsuppress:(BOOL)repaint
{
    UNUSED_PARAM(suppressed);
    UNUSED_PARAM(repaint);
}

- (void)setScrollOrigin:(NSPoint)scrollOrigin updatePositionAtAll:(BOOL)updatePositionAtAll immediately:(BOOL)updatePositionImmediately
{
    UNUSED_PARAM(updatePositionAtAll);
    UNUSED_PARAM(updatePositionImmediately);

    // The cross-platform ScrollView call already checked to see if the old/new scroll origins were the same or not
    // so we don't have to check for equivalence here.
    _scrollOrigin = scrollOrigin;

    [_documentView setBoundsOrigin:NSMakePoint(-scrollOrigin.x, -scrollOrigin.y)];
}

- (NSPoint)scrollOrigin
{
    return _scrollOrigin;
}

#pragma mark -

static bool shouldScroll(WAKScrollView *scrollView, CGPoint scrollPoint)
{
    // We can scroll as long as we are not the last scroll view.
    WAKView *view = scrollView;
    while ((view = [view superview]))
        if ([view isKindOfClass:[WAKScrollView class]])
            return YES;

    id delegate = [scrollView delegate];
    SEL selector = @selector(scrollView:shouldScrollToPoint:);
    return delegate == nil || ![delegate respondsToSelector:selector] || [delegate scrollView:scrollView shouldScrollToPoint:scrollPoint];
}

static float viewDocumentScrollableLength(WAKScrollView *scrollView, bool horizontalOrientation)
{
    float scrollableAmount = 0, documentLength = 0, clipLength = 0;
    WAKView *documentView = [scrollView documentView];
    ASSERT(documentView);

    CGRect frame = [documentView frame];
    if (horizontalOrientation)
        documentLength = frame.size.width;
    else
        documentLength = frame.size.height;

    WAKClipView *clipView = [scrollView contentView];
    ASSERT_WITH_MESSAGE(clipView, "The WAKClipView is supposed to be created by the WAKScrollView at initialization.");
    if (clipView) {
        CGRect frame = [clipView frame];
        if (horizontalOrientation)
            clipLength = frame.size.width;
        else
            clipLength = frame.size.height;
    }

    scrollableAmount = documentLength - clipLength;
    if (scrollableAmount <= 0)
        scrollableAmount = 0;

    return scrollableAmount;
}

static float updateScrollerWithDocumentPosition(WAKScrollView *scrollView, bool horizontalOrientation, float documentPosition)
{
    float documentOriginPosition = 0.;
    if (documentPosition > 0) {
        float scrollableLength = viewDocumentScrollableLength(scrollView, horizontalOrientation);
        if (scrollableLength > 0) {
            float scrolledLength = MIN(documentPosition, scrollableLength);
            documentOriginPosition = -scrolledLength;
        }
    }

    return documentOriginPosition;
}

static bool setDocumentViewOrigin(WAKScrollView *scrollView, WAKView *documentView, CGPoint newDocumentOrigin)
{
    ASSERT(documentView);
    ASSERT(documentView == [scrollView documentView]);

    CGPoint oldDocumentOrigin = [documentView frame].origin;
    if (!CGPointEqualToPoint(oldDocumentOrigin, newDocumentOrigin)) {
        [documentView setFrameOrigin:newDocumentOrigin];
        [scrollView setNeedsDisplay:YES];
        WKViewRef documentViewRef = [documentView _viewRef];
        if (documentViewRef->context && documentViewRef->context->notificationCallback)
            documentViewRef->context->notificationCallback(documentViewRef, WKViewNotificationViewDidScroll, documentViewRef->context->notificationUserInfo);
        return true;
    }
    return false;
}


static BOOL scrollViewToPoint(WAKScrollView *scrollView, CGPoint point)
{
    WAKView *documentView = [scrollView documentView];
    if (!documentView)
        return NO;

    if (!shouldScroll(scrollView, point))
        return NO;

    CGPoint newDocumentOrigin;
    newDocumentOrigin.x = updateScrollerWithDocumentPosition(scrollView, /* horizontal? = */ true, point.x);
    newDocumentOrigin.y = updateScrollerWithDocumentPosition(scrollView, /* horizontal? = */ false, point.y);
    return setDocumentViewOrigin(scrollView, documentView, newDocumentOrigin);
}

- (void)scrollPoint:(NSPoint)point
{
    scrollViewToPoint(self, point);
}

- (void)scrollWheel:(WebEvent *)anEvent
{
    if (!_documentView)
        return [[self nextResponder] scrollWheel:anEvent];
    
    CGPoint origin = [_documentView frame].origin;
    origin.x = roundf(-origin.x - anEvent.deltaX);
    origin.y = roundf(-origin.y - anEvent.deltaY);

    if (!scrollViewToPoint(self, origin))
        return [[self nextResponder] scrollWheel:anEvent];
}

- (CGRect)unobscuredContentRect
{
    // Only called by WebCore::ScrollView::unobscuredContentRect
    WAKView* view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WAKScrollView class]])
            return [self documentVisibleRect];
    }

    WAKWindow* window = [self window];
    // If we don't have a WAKWindow, we must be in a offscreen WebView.
    if (!window)
        return [self documentVisibleRect];

    CGRect windowVisibleRect = CGRectIntegral([window exposedScrollViewRect]);
    return [_documentView convertRect:windowVisibleRect fromView:nil];
}

- (CGRect)exposedContentRect
{
    // Only called by WebCore::ScrollView::exposedContentRect
    WAKView* view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WAKScrollView class]])
            return [self documentVisibleRect];
    }

    WAKWindow* window = [self window];
    // If we don't have a WAKWindow, we must be in a offscreen WebView.
    if (!window)
        return [self documentVisibleRect];

    CGRect windowVisibleRect = CGRectIntegral([window extendedVisibleRect]);
    return [_documentView convertRect:windowVisibleRect fromView:nil];
}

- (void)setActualScrollPosition:(CGPoint)point
{
    WAKView* view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WAKScrollView class]]) {
            // No need for coordinate transformation if what is being scrolled is a subframe
            [self scrollPoint:point];
            return;
        }
    }

    if (!_documentView)
        return;
    CGPoint windowPoint = [_documentView convertPoint:point toView:nil];
    [self scrollPoint:windowPoint];
}

- (NSString *)description
{
    NSMutableString *description = [NSMutableString stringWithFormat:@"<%@: ; ", [super description]];

    [description appendFormat:@"documentView: WAK: %p; ", _documentView];

    CGRect frame = [self documentVisibleRect];
    [description appendFormat:@"documentVisible = (%g %g; %g %g); ", frame.origin.x, frame.origin.y, frame.size.width, frame.size.height];

    frame = [self unobscuredContentRect];
    [description appendFormat:@"actualDocumentVisible = (%g %g; %g %g)>", frame.origin.x, frame.origin.y, frame.size.width, frame.size.height];

    return description;
}

- (BOOL)inProgrammaticScroll
{
    return NO;
}

- (void)_adjustScrollers
{
    // Set the clip view's size to the scroll view's size so the document view can use the correct clip view size when laying out.
    [_contentView setFrameSize:[self bounds].size];

    if (_documentView) {
        CGPoint newDocumentOrigin = [_documentView frame].origin;
        setDocumentViewOrigin(self, _documentView, newDocumentOrigin);
    }
}

@end

#endif // PLATFORM(IOS_FAMILY)
