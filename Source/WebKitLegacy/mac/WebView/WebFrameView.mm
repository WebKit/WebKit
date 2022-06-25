/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebFrameView.h"

#import "BackForwardList.h"
#import "WebClipView.h"
#import "WebDataSourcePrivate.h"
#import "WebDocument.h"
#import "WebDynamicScrollBarsViewInternal.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebFrameViewPrivate.h"
#import "WebHistoryItemInternal.h"
#import "WebHTMLViewPrivate.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitStatisticsPrivate.h"
#import "WebKitVersionChecks.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSViewExtras.h"
#import "WebPDFView.h"
#import "WebPreferenceKeysPrivate.h"
#import "WebResourceInternal.h"
#import "WebViewInternal.h"
#import "WebViewPrivate.h"
#import <Foundation/NSURLRequest.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/DragController.h>
#import <WebCore/EventHandler.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/Page.h>
#import <WebCore/RenderView.h>
#import <WebCore/RenderWidget.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreFrameView.h>
#import <WebCore/WebCoreView.h>
#import <wtf/Assertions.h>

#if PLATFORM(IOS_FAMILY)
#import "WebFrameInternal.h"
#import "WebPDFViewIOS.h"
#import "WebUIKitDelegate.h"
#import <Foundation/NSURLRequest.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/WAKClipView.h>
#import <WebCore/WAKScrollView.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WKGraphics.h>
#import <WebCore/WebEvent.h>
#else
#import "WebNSPasteboardExtras.h"
#import "WebNSWindowExtras.h"
#endif

#if !PLATFORM(IOS_FAMILY)
@interface NSWindow (WindowPrivate)
- (BOOL)_needsToResetDragMargins;
- (void)_setNeedsToResetDragMargins:(BOOL)s;
@end
#endif

@interface NSClipView (AppKitSecretsIKnow)
- (BOOL)_scrollTo:(const NSPoint *)newOrigin animate:(BOOL)animate; // need the boolean result from this method
@end

#if PLATFORM(MAC)
@interface NSView ()
- (void)setBackgroundColor:(NSColor *)color;
@end
#endif

enum {
    SpaceKey = 0x0020
};

@interface WebFrameView (WebFrameViewFileInternal) <WebCoreFrameView>
- (float)_verticalKeyboardScrollDistance;
@end

@interface WebFrameViewPrivate : NSObject {
@public
    WebFrame *webFrame;
    RetainPtr<WebDynamicScrollBarsView> frameScrollView;
    BOOL includedInWebKitStatistics;
}
@end

@implementation WebFrameViewPrivate

@end

@implementation WebFrameView (WebFrameViewFileInternal)

- (float)_verticalKeyboardScrollDistance
{
    // Arrow keys scroll the same distance that clicking the scroll arrow does.
    return [[self _scrollView] verticalLineScroll];
}

- (NakedPtr<WebCore::Frame>)_web_frame
{
    return core(_private->webFrame);
}

@end

@implementation WebFrameView (WebInternal)

// Note that the WebVew is not retained.
- (WebView *)_webView
{
    return [_private->webFrame webView];
}

- (void)_setDocumentView:(NSView <WebDocumentView> *)view
{
    WebDynamicScrollBarsView *sv = [self _scrollView];
    
#if ENABLE(DRAG_SUPPORT)
    core([self _webView])->dragController().setDidInitiateDrag(false);
#endif
    
#if !PLATFORM(IOS_FAMILY)
    [sv setSuppressLayout:YES];
    
    // If the old view is the first responder, transfer first responder status to the new view as 
    // a convenience and so that we don't leave the window pointing to a view that's no longer in it.
    NSWindow *window = [sv window];
    NSResponder *firstResponder = [window firstResponder];
    bool makeNewViewFirstResponder = [firstResponder isKindOfClass:[NSView class]] && [(NSView *)firstResponder isDescendantOf:[sv documentView]];

    // Suppress the resetting of drag margins since we know we can't affect them.
    BOOL resetDragMargins = [window _needsToResetDragMargins];
    [window _setNeedsToResetDragMargins:NO];
#endif
    [sv setDocumentView:view];
#if !PLATFORM(IOS_FAMILY)
    [window _setNeedsToResetDragMargins:resetDragMargins];

    if (makeNewViewFirstResponder)
        [window makeFirstResponder:view];
    [sv setSuppressLayout:NO];
#else
    ASSERT(_private->webFrame);

    auto* frame = core(_private->webFrame);

    ASSERT(frame);
    ASSERT(frame->page());

    if (frame == &frame->page()->mainFrame())
        [[self window] makeFirstResponder:[self documentView]];
#endif
}

-(NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource
{
    NSString* MIMEType = [dataSource _responseMIMEType];
    if (!MIMEType)
        MIMEType = @"text/html";
    Class viewClass = [self _viewClassForMIMEType:MIMEType];
    RetainPtr<NSView <WebDocumentView>> documentView;
    if (viewClass) {
        // If the dataSource's representation has already been created, and it is also the
        // same class as the desired documentView, then use it as the documentView instead
        // of creating another one (Radar 4340787).
        id <WebDocumentRepresentation> dataSourceRepresentation = [dataSource representation];
        if (dataSourceRepresentation && [dataSourceRepresentation class] == viewClass)
            documentView = (NSView <WebDocumentView> *)dataSourceRepresentation;
        else
            documentView = adoptNS([(NSView <WebDocumentView> *)[viewClass alloc] init]);
    }
    
    [self _setDocumentView:documentView.get()];
    return documentView.autorelease();
}

- (void)_setWebFrame:(WebFrame *)webFrame
{
    if (!webFrame) {
        NSView *docV = [self documentView];
        if ([docV respondsToSelector:@selector(close)])
            [docV performSelector:@selector(close)];
    }

    // Not retained because the WebView owns the WebFrame, which owns the WebFrameView.
    _private->webFrame = webFrame;    

    if (!_private->includedInWebKitStatistics && [webFrame _isIncludedInWebKitStatistics]) {
        _private->includedInWebKitStatistics = YES;
        ++WebFrameViewCount;
    }
}

- (WebDynamicScrollBarsView *)_scrollView
{
    // This can be called by [super dealloc] when cleaning up the key view loop,
    // after _private has been nilled out.
    if (_private == nil)
        return nil;
    return _private->frameScrollView.get();
}

- (float)_verticalPageScrollDistance
{
    float height = [[self _contentView] bounds].size.height;
    return std::max<float>(height * WebCore::Scrollbar::minFractionToStepWhenPaging(), height - WebCore::Scrollbar::maxOverlapBetweenPages());
}

+ (NSMutableDictionary *)_viewTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static NeverDestroyed viewTypes = [] {
        auto types = adoptNS([[NSMutableDictionary alloc] init]);
        addTypesFromClass(types.get(), [WebHTMLView class], [WebHTMLView supportedNonImageMIMETypes]);
        addTypesFromClass(types.get(), [WebHTMLView class], [WebHTMLView supportedMediaMIMETypes]);

        // Since this is a "secret default" we don't bother registering it.
        BOOL omitPDFSupport = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
        if (!omitPDFSupport) {
#if PLATFORM(IOS_FAMILY)
#define WebPDFView ([WebView _getPDFViewClass])
#endif
            addTypesFromClass(types.get(), [WebPDFView class], [WebPDFView supportedMIMETypes]);
#if PLATFORM(IOS_FAMILY)
#undef WebPDFView
#endif
        }
        return types;
    }();
    static BOOL addedImageTypes = NO;
    if (!addedImageTypes && !allowImageTypeOmission) {
        addTypesFromClass(viewTypes.get().get(), [WebHTMLView class], [WebHTMLView supportedImageMIMETypes]);
        addedImageTypes = YES;
    }
    
    return viewTypes.get().get();
}

+ (BOOL)_canShowMIMETypeAsHTML:(NSString *)MIMEType
{
    return [[[self _viewTypesAllowImageTypeOmission:YES] _webkit_objectForMIMEType:MIMEType] isSubclassOfClass:[WebHTMLView class]];
}

+ (Class)_viewClassForMIMEType:(NSString *)MIMEType allowingPlugins:(BOOL)allowPlugins
{
    Class viewClass;
    return [WebView _viewClass:&viewClass andRepresentationClass:nil forMIMEType:MIMEType allowingPlugins:allowPlugins] ? viewClass : nil;
}

- (Class)_viewClassForMIMEType:(NSString *)MIMEType
{
    Class retVal = [[self class] _viewClassForMIMEType:MIMEType allowingPlugins:[[[self _webView] preferences] arePlugInsEnabled]];

#if PLATFORM(IOS_FAMILY)   
    if ([retVal respondsToSelector:@selector(_representationClassForWebFrame:)])
        retVal = [retVal performSelector:@selector(_representationClassForWebFrame:) withObject:[self webFrame]];
#endif
        
    return retVal;
}

- (void)_install
{
    ASSERT(_private->webFrame);
    ASSERT(_private->frameScrollView);

    auto* frame = core(_private->webFrame);

    ASSERT(frame);
    ASSERT(frame->page());

    // If this isn't the main frame, it must have an owner element set, or it
    // won't ever get installed in the view hierarchy.
    ASSERT(frame->isMainFrame() || frame->ownerElement());

    auto* view = frame->view();

    view->setPlatformWidget(_private->frameScrollView.get());

    // FIXME: Frame tries to do this too. Is this code needed?
    if (WebCore::RenderWidget* owner = frame->ownerRenderer()) {
        owner->setWidget(view);
        // Now the RenderWidget owns the view, so we don't any more.
    }

    view->updateCanHaveScrollbars();
}

- (void)_frameSizeChanged
{
    // See WebFrameLoaderClient::provisionalLoadStarted.
    if ([[[self webFrame] webView] drawsBackground])
        [[self _scrollView] setDrawsBackground:YES];
    if (auto coreFrame = [self _web_frame]) {
        if (auto* coreFrameView = coreFrame->view())
            coreFrameView->availableContentSizeChanged(WebCore::ScrollableArea::AvailableSizeChangeReason::AreaSizeChanged);
    }
}

@end

@implementation WebFrameView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
 
    static bool didFirstTimeInitialization;
    if (!didFirstTimeInitialization) {
        didFirstTimeInitialization = true;
        
        // Need to tell WebCore what function to call for the "History Item has Changed" notification.
        // Note: We also do this in WebHistoryItem's init method.
        // FIXME: This means that if we mix legacy WebKit and modern WebKit in the same process, we won't get both notifications.
        WebCore::notifyHistoryItemChanged = WKNotifyHistoryItemChanged;

#if !PLATFORM(IOS_FAMILY)
        if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_MAIN_THREAD_EXCEPTIONS))
            setDefaultThreadViolationBehavior(WebCore::LogOnFirstThreadViolation, WebCore::ThreadViolationRoundOne);

        bool throwExceptionsForRoundTwo = WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_ROUND_TWO_MAIN_THREAD_EXCEPTIONS);
        if (!throwExceptionsForRoundTwo)
            setDefaultThreadViolationBehavior(WebCore::LogOnFirstThreadViolation, WebCore::ThreadViolationRoundTwo);

        bool throwExceptionsForRoundThree = WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_ROUND_THREE_MAIN_THREAD_EXCEPTIONS);
        if (!throwExceptionsForRoundThree)
            setDefaultThreadViolationBehavior(WebCore::LogOnFirstThreadViolation, WebCore::ThreadViolationRoundThree);
#endif
    }

    _private = [[WebFrameViewPrivate alloc] init];

    auto scrollView = adoptNS([[WebDynamicScrollBarsView alloc] initWithFrame:NSMakeRect(0.0f, 0.0f, frame.size.width, frame.size.height)]);
    _private->frameScrollView = scrollView;
#if PLATFORM(IOS_FAMILY)
    [scrollView setDelegate:self];
#else
    [scrollView setContentView:adoptNS([[WebClipView alloc] initWithFrame:[scrollView bounds]]).get()];
#endif
    [scrollView setDrawsBackground:NO];
    [scrollView setHasVerticalScroller:NO];
    [scrollView setHasHorizontalScroller:NO];
    [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scrollView setLineScroll:WebCore::Scrollbar::pixelsPerLineStep()];
    [self addSubview:scrollView.get()];

    // Don't call our overridden version of setNextKeyView here; we need to make the standard NSView
    // link between us and our subview so that previousKeyView and previousValidKeyView work as expected.
    // This works together with our becomeFirstResponder and setNextKeyView overrides.
    [super setNextKeyView:scrollView.get()];
    
    return self;
}

- (void)dealloc 
{
    if (_private && _private->includedInWebKitStatistics)
        --WebFrameViewCount;
    
    [_private release];
    _private = nil;
    
    [super dealloc];
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)scrollView:(WAKScrollView *)scrollView shouldScrollToPoint:(CGPoint)point
{
    WebView *webView = [self _webView];
    return [[webView _UIKitDelegateForwarder] webView:webView shouldScrollToPoint:point forFrame:_private->webFrame];
}
#endif

- (WebFrame *)webFrame
{
    // This method can be called beneath -[NSView dealloc] after _private has been cleared.
    return _private ? _private->webFrame : nil;
}

- (void)setAllowsScrolling:(BOOL)flag
{
    WebCore::Frame *frame = core([self webFrame]);
    if (WebCore::FrameView *view = frame? frame->view() : 0)
        view->setCanHaveScrollbars(flag);
}

- (BOOL)allowsScrolling
{
    WebCore::Frame *frame = core([self webFrame]);
    if (WebCore::FrameView *view = frame? frame->view() : 0)
        return view->canHaveScrollbars();
    return YES;
}

- (NSView <WebDocumentView> *)documentView
{
    return [[self _scrollView] documentView];
}

- (BOOL)acceptsFirstResponder
{
    // We always accept first responder; this matches OS X 10.2 WebKit
    // behavior (see 3469791).
    return YES;
}

- (BOOL)becomeFirstResponder
{
    // This works together with setNextKeyView to splice the WebFrameView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebView has similar code.
    
    NSWindow *window = [self window];
    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        // If we couldn't find a previous valid key view, ask the WebView. This handles frameset
        // cases (one is mentioned in Radar bug 3748628). Note that previousValidKeyView should
        // never be self but can be due to AppKit oddness (mentioned in Radar bug 3748628).
        if (previousValidKeyView == nil || previousValidKeyView == self)
            previousValidKeyView = [[[self webFrame] webView] previousValidKeyView];
        [window makeFirstResponder:previousValidKeyView];
    } else {
        // If the scroll view won't accept first-responderness now, then just become
        // the first responder ourself like a normal view. This lets us be the first 
        // responder in cases where no page has yet been loaded.
        if ([[self _scrollView] acceptsFirstResponder])
            [window makeFirstResponder:[self _scrollView]];
    }
    
    return YES;
}

- (void)setNextKeyView:(NSView *)aView
{
    // This works together with becomeFirstResponder to splice the WebFrameView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebView has very similar code.
    if ([self _scrollView] != nil) {
        [[self _scrollView] setNextKeyView:aView];
    } else {
        [super setNextKeyView:aView];
    }
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
}

- (void)drawRect:(NSRect)rect
{
#if !PLATFORM(IOS_FAMILY)
    if (![self documentView])
#else
    if (![self documentView] || [[self documentView] frame].size.height == 0 || [[self webFrame] _isCommitting])
#endif
    {
        // Need to paint ourselves if there's no documentView to do it instead.
        if ([[self _webView] drawsBackground]) {
#if !PLATFORM(IOS_FAMILY)
            [[[self _webView] backgroundColor] set];
            NSRectFill(rect);
#else
            CGContextRef cgContext = WKGetCurrentGraphicsContext();
            CGContextSetFillColorWithColor(cgContext, WebCore::cachedCGColor(WebCore::Color::white).get());
            WKRectFill(cgContext, rect);
#endif
        }
    } else {
#ifndef NDEBUG
        if ([[self _scrollView] drawsBackground]) {
#if !PLATFORM(IOS_FAMILY)
            [[NSColor cyanColor] set];
            NSRectFill(rect);
#else
            CGContextRef cgContext = WKGetCurrentGraphicsContext();
            CGContextSetFillColorWithColor(cgContext, WebCore::cachedCGColor(WebCore::Color::cyan).get());
            WKRectFill(cgContext, rect);
#endif
        }
#endif
    }
}

#if PLATFORM(MAC)
- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (void)updateLayer
{
    // Do what -drawRect: does but by setting a backgroundColor on the view. This avoids
    // backing store for this view when the WebView is layer-backed.
    if (![self documentView]) {
        if ([[self _webView] drawsBackground]) {
            [self setBackgroundColor:[[self _webView] backgroundColor]];
            return;
        }
    } else {
#ifndef NDEBUG
        if ([[self _scrollView] drawsBackground]) {
            [self setBackgroundColor:[NSColor cyanColor]];
            return;
        }
#endif
    }

    [self setBackgroundColor:[NSColor clearColor]];
}
#endif

- (NSRect)visibleRect
{
    // This method can be called beneath -[NSView dealloc] after we have cleared _private.
    if (!_private)
        return [super visibleRect];

    // FIXME: <rdar://problem/6213380> This method does not work correctly with transforms, for two reasons:
    // 1) [super visibleRect] does not account for the transform, since it is not represented
    //    in the NSView hierarchy.
    // 2) -_getVisibleRect: does not correct for transforms.

    NSRect rendererVisibleRect;
    if (![[self webFrame] _getVisibleRect:&rendererVisibleRect])
        return [super visibleRect];

    if (NSIsEmptyRect(rendererVisibleRect))
        return NSZeroRect;

    NSRect viewVisibleRect = [super visibleRect];
    if (NSIsEmptyRect(viewVisibleRect))
        return NSZeroRect;

    NSRect frame = [self frame];
    // rendererVisibleRect is in the parent's coordinate space, and frame is in the superview's coordinate space.
    // The return value from this method needs to be in this view's coordinate space. We get that right by subtracting
    // the origins (and correcting for flipping), but when we support transforms, we will need to do better than this.
    rendererVisibleRect.origin.x -= frame.origin.x;
    rendererVisibleRect.origin.y = NSMaxY(frame) - NSMaxY(rendererVisibleRect);
    return NSIntersectionRect(rendererVisibleRect, viewVisibleRect);
}

- (void)setFrameSize:(NSSize)size
{
    if (!NSEqualSizes(size, [self frame].size))
        [self _frameSizeChanged];

    [super setFrameSize:size];
}

- (void)viewDidMoveToWindow
{
    // See WebFrameLoaderClient::provisionalLoadStarted.
    // Need to check _private for nil because this can be called inside -[WebView initWithCoder:].
    if (_private && [[[self webFrame] webView] drawsBackground])
        [[self _scrollView] setDrawsBackground:YES];
    [super viewDidMoveToWindow];
}

- (BOOL)_scrollOverflowInDirection:(WebCore::ScrollDirection)direction granularity:(WebCore::ScrollGranularity)granularity
{
    // scrolling overflows is only applicable if we're dealing with an WebHTMLView
    if (![[self documentView] isKindOfClass:[WebHTMLView class]])
        return NO;
    auto* frame = core([self webFrame]);
    if (!frame)
        return NO;
    return frame->eventHandler().scrollOverflow(direction, granularity);
}


- (BOOL)_isVerticalDocument
{
    auto coreFrame = [self _web_frame];
    if (!coreFrame)
        return YES;
    auto* document = coreFrame->document();
    if (!document)
        return YES;
    auto* renderView = document->renderView();
    if (!renderView)
        return YES;
    return renderView->style().isHorizontalWritingMode();
}

- (BOOL)_isFlippedDocument
{
    auto coreFrame = [self _web_frame];
    if (!coreFrame)
        return NO;
    auto* document = coreFrame->document();
    if (!document)
        return NO;
    auto* renderView = document->renderView();
    if (!renderView)
        return NO;
    return renderView->style().isFlippedBlocksWritingMode();
}

- (BOOL)_scrollToBeginningOfDocument
{
    if ([self _scrollOverflowInDirection:WebCore::ScrollUp granularity:WebCore::ScrollGranularity::Document])
        return YES;
    if (![self _isScrollable])
        return NO;
    NSPoint point = [(NSView *)[[self _scrollView] documentView] frame].origin;
    point.x += [[self _scrollView] scrollOrigin].x;
    point.y += [[self _scrollView] scrollOrigin].y;
    return [[self _contentView] _scrollTo:&point animate:YES];
}

- (BOOL)_scrollToEndOfDocument
{
    if ([self _scrollOverflowInDirection:WebCore::ScrollDown granularity:WebCore::ScrollGranularity::Document])
        return YES;
    if (![self _isScrollable])
        return NO;
    NSRect frame = [(NSView *)[[self _scrollView] documentView] frame];
    
    bool isVertical = [self _isVerticalDocument];
    bool isFlipped = [self _isFlippedDocument];

    NSPoint point;
    if (isVertical) {
        if (!isFlipped)
            point = NSMakePoint(frame.origin.x, NSMaxY(frame));
        else
            point = NSMakePoint(frame.origin.x, NSMinY(frame));
    } else {
        if (!isFlipped)
            point = NSMakePoint(NSMaxX(frame), frame.origin.y);
        else
            point = NSMakePoint(NSMinX(frame), frame.origin.y);
    }
    
    // Reset the position opposite to the block progression direction.
    if (isVertical)
        point.x += [[self _scrollView] scrollOrigin].x;
    else
        point.y += [[self _scrollView] scrollOrigin].y;
    return [[self _contentView] _scrollTo:&point animate:YES];
}

- (void)scrollToBeginningOfDocument:(id)sender
{
    if ([self _scrollToBeginningOfDocument])
        return;
    
    if (WebFrameView *child = [self _largestScrollableChild]) {
        if ([child _scrollToBeginningOfDocument])
            return;
    }
    [[self nextResponder] tryToPerform:@selector(scrollToBeginningOfDocument:) with:sender];
}

- (void)scrollToEndOfDocument:(id)sender
{
    if ([self _scrollToEndOfDocument])
        return;

    if (WebFrameView *child = [self _largestScrollableChild]) {
        if ([child _scrollToEndOfDocument])
            return;
    }
    [[self nextResponder] tryToPerform:@selector(scrollToEndOfDocument:) with:sender];
}

- (void)_goBack
{
    [[self _webView] goBack];
}

- (void)_goForward
{
    [[self _webView] goForward];
}

- (BOOL)_scrollVerticallyBy:(float)delta
{
    // This method uses the secret method _scrollTo on NSClipView.
    // It does that because it needs to know definitively whether scrolling was
    // done or not to help implement the "scroll parent if we are at the limit" feature.
    // In the presence of smooth scrolling, there's no easy way to tell if the method
    // did any scrolling or not with the public API.
    NSPoint point = [[self _contentView] bounds].origin;
    point.y += delta;
    return [[self _contentView] _scrollTo:&point animate:YES];
}

- (BOOL)_scrollHorizontallyBy:(float)delta
{
    NSPoint point = [[self _contentView] bounds].origin;
    point.x += delta;
    return [[self _contentView] _scrollTo:&point animate:YES];
}

- (float)_horizontalKeyboardScrollDistance
{
    // Arrow keys scroll the same distance that clicking the scroll arrow does.
    return [[self _scrollView] horizontalLineScroll];
}

- (float)_horizontalPageScrollDistance
{
    float width = [[self _contentView] bounds].size.width;
    return std::max<float>(width * WebCore::Scrollbar::minFractionToStepWhenPaging(), width - WebCore::Scrollbar::maxOverlapBetweenPages());
}

- (BOOL)_pageVertically:(BOOL)up
{
    if ([self _scrollOverflowInDirection:up ? WebCore::ScrollUp : WebCore::ScrollDown granularity:WebCore::ScrollGranularity::Page])
        return YES;
    
    if (![self _isScrollable])
        return [[self _largestScrollableChild] _pageVertically:up];

    float delta = [self _verticalPageScrollDistance];
    return [self _scrollVerticallyBy:up ? -delta : delta];
}

- (BOOL)_pageHorizontally:(BOOL)left
{
    if ([self _scrollOverflowInDirection:left ? WebCore::ScrollLeft : WebCore::ScrollRight granularity:WebCore::ScrollGranularity::Page])
        return YES;

    if (![self _isScrollable])
        return [[self _largestScrollableChild] _pageHorizontally:left];
    
    float delta = [self _horizontalPageScrollDistance];
    return [self _scrollHorizontallyBy:left ? -delta : delta];
}

- (BOOL)_pageInBlockProgressionDirection:(BOOL)forward
{
    // Determine whether we're calling _pageVertically or _pageHorizontally.
    BOOL isVerticalDocument = [self _isVerticalDocument];
    BOOL isFlippedBlock = [self _isFlippedDocument];
    if (isVerticalDocument)
        return [self _pageVertically:isFlippedBlock ? !forward : forward];
    return [self _pageHorizontally:isFlippedBlock ? !forward : forward];
}

- (BOOL)_scrollLineVertically:(BOOL)up
{
    if ([self _scrollOverflowInDirection:up ? WebCore::ScrollUp : WebCore::ScrollDown granularity:WebCore::ScrollGranularity::Line])
        return YES;

    if (![self _isScrollable])
        return [[self _largestScrollableChild] _scrollLineVertically:up];
    
    float delta = [self _verticalKeyboardScrollDistance];
    return [self _scrollVerticallyBy:up ? -delta : delta];
}

- (BOOL)_scrollLineHorizontally:(BOOL)left
{
    if ([self _scrollOverflowInDirection:left ? WebCore::ScrollLeft : WebCore::ScrollRight granularity:WebCore::ScrollGranularity::Line])
        return YES;

    if (![self _isScrollable])
        return [[self _largestScrollableChild] _scrollLineHorizontally:left];

    float delta = [self _horizontalKeyboardScrollDistance];
    return [self _scrollHorizontallyBy:left ? -delta : delta];
}

- (void)scrollPageUp:(id)sender
{
    if (![self _pageInBlockProgressionDirection:YES]) {
        // If we were already at the top, tell the next responder to scroll if it can.
        [[self nextResponder] tryToPerform:@selector(scrollPageUp:) with:sender];
    }
}

- (void)scrollPageDown:(id)sender
{
    if (![self _pageInBlockProgressionDirection:NO]) {
        // If we were already at the bottom, tell the next responder to scroll if it can.
        [[self nextResponder] tryToPerform:@selector(scrollPageDown:) with:sender];
    }
}

- (void)scrollLineUp:(id)sender
{
    if (![self _scrollLineVertically:YES])
        [[self nextResponder] tryToPerform:@selector(scrollLineUp:) with:sender];
}

- (void)scrollLineDown:(id)sender
{
    if (![self _scrollLineVertically:NO])
        [[self nextResponder] tryToPerform:@selector(scrollLineDown:) with:sender];
}

- (BOOL)_firstResponderIsFormControl
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    NSResponder *firstResponder = [[self window] firstResponder];
    
    // WebHTMLView is an NSControl subclass these days, but it's not a form control
    if ([firstResponder isKindOfClass:[WebHTMLView class]]) {
        return NO;
    }
    return [firstResponder isKindOfClass:[NSControl class]];
#endif
}

#if PLATFORM(IOS_FAMILY)
// Unlike OS X WebKit, on iOS, unhandled mouse events are forwarded to allow for scrolling.
// Since mouse events were forwarded to this WebFrameView, this means that the subviews didn't
// handle the event. Pass the events to the next scroll view.
- (void)_forwardMouseEvent:(WebEvent *)event
{
    WAKView *superview = [self superview];
    if ([superview conformsToProtocol:@protocol(WebDocumentView)])
        [[[superview _web_parentWebFrameView] _scrollView] handleEvent:event];
    else
        [[self nextResponder] handleEvent:event];
}

- (void)mouseDown:(WebEvent *)event
{
    [self _forwardMouseEvent:event];
}

- (void)mouseUp:(WebEvent *)event
{
    [self _forwardMouseEvent:event];
}
#endif

#if !PLATFORM(IOS_FAMILY)
- (void)keyDown:(NSEvent *)event
#else
- (void)keyDown:(WebEvent *)event
#endif
{
    // Implement common browser behaviors for all kinds of content.

    // FIXME: This is not a good time to execute commands for WebHTMLView. We should run these at the time commands sent by key bindings
    // are executed for consistency.
    // This doesn't work automatically because most of the keys handled here are translated into moveXXX commands, which are not handled
    // by Editor when focus is not in editable content.

#if !PLATFORM(IOS_FAMILY)
    NSString *characters = [event characters];
    int modifierFlags = [event modifierFlags];
#else
    NSString *characters = event.characters;
    int modifierFlags = event.modifierFlags;
#endif
    int index, count;
    BOOL callSuper = YES;
    auto coreFrame = [self _web_frame];
    BOOL maintainsBackForwardList = coreFrame && static_cast<BackForwardList&>(coreFrame->page()->backForward().client()).enabled() ? YES : NO;

    count = [characters length];
    for (index = 0; index < count; ++index) {
        switch ([characters characterAtIndex:index]) {
            case NSDeleteCharacter:
                if (!maintainsBackForwardList || ![[[self _webView] preferences] backspaceKeyNavigationEnabled]) {
                    callSuper = YES;
                    break;
                }
                // This odd behavior matches some existing browsers,
                // including Windows IE
                if (modifierFlags & NSEventModifierFlagShift) {
                    [self _goForward];
                } else {
                    [self _goBack];
                }
                callSuper = NO;
                break;
            case SpaceKey:
                // Checking for a control will allow events to percolate 
                // correctly when the focus is on a form control and we
                // are in full keyboard access mode.
                if ((![self allowsScrolling] && ![self _largestScrollableChild]) || [self _firstResponderIsFormControl]) {
                    callSuper = YES;
                    break;
                }
                if (modifierFlags & NSEventModifierFlagShift) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollPageDown:nil];
                }
                callSuper = NO;
                break;
            case NSPageUpFunctionKey:
                if (![self allowsScrolling] && ![self _largestScrollableChild]) {
                    callSuper = YES;
                    break;
                }
                [self scrollPageUp:nil];
                callSuper = NO;
                break;
            case NSPageDownFunctionKey:
                if (![self allowsScrolling] && ![self _largestScrollableChild]) {
                    callSuper = YES;
                    break;
                }
                [self scrollPageDown:nil];
                callSuper = NO;
                break;
            case NSHomeFunctionKey:
                if (![self allowsScrolling] && ![self _largestScrollableChild]) {
                    callSuper = YES;
                    break;
                }
                [self scrollToBeginningOfDocument:nil];
                callSuper = NO;
                break;
            case NSEndFunctionKey:
                if (![self allowsScrolling] && ![self _largestScrollableChild]) {
                    callSuper = YES;
                    break;
                }
                [self scrollToEndOfDocument:nil];
                callSuper = NO;
                break;
            case NSUpArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if (modifierFlags & (NSEventModifierFlagShift | NSEventModifierFlagControl)) {
                    callSuper = YES;
                    break;
                }
#if !PLATFORM(IOS_FAMILY)
                if ((![self allowsScrolling] && ![self _largestScrollableChild]) ||
                    [[[self window] firstResponder] isKindOfClass:[NSPopUpButton class]]) {
                    // Let arrow keys go through to pop up buttons
                    // <rdar://problem/3455910>: hitting up or down arrows when focus is on a 
                    // pop-up menu should pop the menu
                    callSuper = YES;
                    break;
                }
#endif
                if (modifierFlags & NSEventModifierFlagCommand) {
                    [self scrollToBeginningOfDocument:nil];
                } else if (modifierFlags & NSEventModifierFlagOption) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollLineUp:nil];
                }
                callSuper = NO;
                break;
            case NSDownArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if (modifierFlags & (NSEventModifierFlagShift | NSEventModifierFlagControl)) {
                    callSuper = YES;
                    break;
                }
#if !PLATFORM(IOS_FAMILY)
                if ((![self allowsScrolling] && ![self _largestScrollableChild]) ||
                    [[[self window] firstResponder] isKindOfClass:[NSPopUpButton class]]) {
                    // Let arrow keys go through to pop up buttons
                    // <rdar://problem/3455910>: hitting up or down arrows when focus is on a 
                    // pop-up menu should pop the menu
                    callSuper = YES;
                    break;
                }
#endif
                if (modifierFlags & NSEventModifierFlagCommand) {
                    [self scrollToEndOfDocument:nil];
                } else if (modifierFlags & NSEventModifierFlagOption) {
                    [self scrollPageDown:nil];
                } else {
                    [self scrollLineDown:nil];
                }
                callSuper = NO;
                break;
            case NSLeftArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if (modifierFlags & (NSEventModifierFlagShift | NSEventModifierFlagControl)) {
                    callSuper = YES;
                    break;
                }
                // Check back/forward related keys.
                if (modifierFlags & NSEventModifierFlagCommand) {
                    if (!maintainsBackForwardList) {
                        callSuper = YES;
                        break;
                    }
                    [self _goBack];
                } else {
                    // Now check scrolling related keys.
                    if ((![self allowsScrolling] && ![self _largestScrollableChild])) {
                        callSuper = YES;
                        break;
                    }

                    if (modifierFlags & NSEventModifierFlagOption) {
                        [self _pageHorizontally:YES];
                    } else {
                        [self _scrollLineHorizontally:YES];
                    }
                }
                callSuper = NO;
                break;
            case NSRightArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if (modifierFlags & (NSEventModifierFlagShift | NSEventModifierFlagControl)) {
                    callSuper = YES;
                    break;
                }
                // Check back/forward related keys.
                if (modifierFlags & NSEventModifierFlagCommand) {
                    if (!maintainsBackForwardList) {
                        callSuper = YES;
                        break;
                    }
                    [self _goForward];
                } else {
                    // Now check scrolling related keys.
                    if ((![self allowsScrolling] && ![self _largestScrollableChild])) {
                        callSuper = YES;
                        break;
                    }

                    if (modifierFlags & NSEventModifierFlagOption) {
                        [self _pageHorizontally:NO];
                    } else {
                        [self _scrollLineHorizontally:NO];
                    }
                }
                callSuper = NO;
                break;
        }
    }
    if (callSuper) {
        [super keyDown:event];
    } else {
#if PLATFORM(MAC)
        // If we did something useful, get the cursor out of the way.
        [NSCursor setHiddenUntilMouseMoves:YES];
#endif
    }
}

- (NSView *)_webcore_effectiveFirstResponder
{
    NSView *view = [self documentView];
    return view ? [view _webcore_effectiveFirstResponder] : [super _webcore_effectiveFirstResponder];
}

#if !PLATFORM(IOS_FAMILY)
- (BOOL)canPrintHeadersAndFooters
{
    NSView *documentView = [[self _scrollView] documentView];
    if ([documentView respondsToSelector:@selector(canPrintHeadersAndFooters)]) {
        return [(id)documentView canPrintHeadersAndFooters];
    }
    return NO;
}

- (NSPrintOperation *)printOperationWithPrintInfo:(NSPrintInfo *)printInfo
{
    NSView *documentView = [[self _scrollView] documentView];
    if (!documentView) {
        return nil;
    }
    if ([documentView respondsToSelector:@selector(printOperationWithPrintInfo:)]) {
        return [(id)documentView printOperationWithPrintInfo:printInfo];
    }
    return [NSPrintOperation printOperationWithView:documentView printInfo:printInfo];
}
#endif

- (BOOL)documentViewShouldHandlePrint
{
    NSView *documentView = [[self _scrollView] documentView];
    if (documentView && [documentView respondsToSelector:@selector(documentViewShouldHandlePrint)])
        return [(id)documentView documentViewShouldHandlePrint];
    
    return NO;
}

- (void)printDocumentView
{
    NSView *documentView = [[self _scrollView] documentView];
    if (documentView && [documentView respondsToSelector:@selector(printDocumentView)])
        [(id)documentView printDocumentView];
}

@end

@implementation WebFrameView (WebPrivate)

- (float)_area
{
    NSRect frame = [self frame];
    return frame.size.height * frame.size.width;
}

- (BOOL)_isScrollable
{
#if !PLATFORM(IOS_FAMILY)
    WebDynamicScrollBarsView *scrollView = [self _scrollView];
    return [scrollView horizontalScrollingAllowed] || [scrollView verticalScrollingAllowed];
#else
    return [self _hasScrollBars];
#endif
}

- (WebFrameView *)_largestScrollableChild
{
    WebFrameView *largest = nil;
    NSArray *frameChildren = [[self webFrame] childFrames];
    
    unsigned i;
    for (i=0; i < [frameChildren count]; i++) {
        WebFrameView *childFrameView = [[frameChildren objectAtIndex:i] frameView];
        WebFrameView *scrollableFrameView = [childFrameView _isScrollable] ? childFrameView : [childFrameView _largestScrollableChild];
        if (!scrollableFrameView)
            continue;
        
        // Some ads lurk in child frames of zero width and height, see radar 4406994. These don't count as scrollable.
        // Maybe someday we'll discover that this minimum area check should be larger, but this covers the known cases.
        float area = [scrollableFrameView _area];
        if (area < 1.0)
            continue;
        
        if (!largest || (area > [largest _area])) {
            largest = scrollableFrameView;
        }
    }
    
    return largest;
}

- (BOOL)_hasScrollBars
{
    // FIXME: This method was used by Safari 4.0.x and older versions, but has not been used by any other WebKit
    // clients to my knowledge, and will not be used by future versions of Safari. It can probably be removed 
    // once we no longer need to keep nightly WebKit builds working with Safari 4.0.x and earlier.
    NSScrollView *scrollView = [self _scrollView];
    return [scrollView hasHorizontalScroller] || [scrollView hasVerticalScroller];
}

- (WebFrameView *)_largestChildWithScrollBars
{
    // FIXME: This method was used by Safari 4.0.x and older versions, but has not been used by any other WebKit
    // clients to my knowledge, and will not be used by future versions of Safari. It can probably be removed 
    // once we no longer need to keep nightly WebKit builds working with Safari 4.0.x and earlier.
    WebFrameView *largest = nil;
    NSArray *frameChildren = [[self webFrame] childFrames];
    
    unsigned i;
    for (i=0; i < [frameChildren count]; i++) {
        WebFrameView *childFrameView = [[frameChildren objectAtIndex:i] frameView];
        WebFrameView *scrollableFrameView = [childFrameView _hasScrollBars] ? childFrameView : [childFrameView _largestChildWithScrollBars];
        if (!scrollableFrameView)
            continue;
        
        // Some ads lurk in child frames of zero width and height, see radar 4406994. These don't count as scrollable.
        // Maybe someday we'll discover that this minimum area check should be larger, but this covers the known cases.
        float area = [scrollableFrameView _area];
        if (area < 1.0)
            continue;
        
        if (!largest || (area > [largest _area])) {
            largest = scrollableFrameView;
        }
    }
    
    return largest;
}

- (NSClipView *)_contentView
{
    return [[self _scrollView] contentView];
}

- (Class)_customScrollViewClass
{
    if ([_private->frameScrollView class] == [WebDynamicScrollBarsView class])
        return nil;
    return [_private->frameScrollView class];
}

#if !PLATFORM(IOS_FAMILY)
- (void)_setCustomScrollViewClass:(Class)customClass
{
    if (!customClass)
        customClass = [WebDynamicScrollBarsView class];
    ASSERT([customClass isSubclassOfClass:[WebDynamicScrollBarsView class]]);
    if (customClass == [_private->frameScrollView class])
        return;
    if (![customClass isSubclassOfClass:[WebDynamicScrollBarsView class]])
        return;

    auto oldScrollView = _private->frameScrollView; // already retained
    auto documentView = retainPtr([self documentView]);

    RetainPtr<WebDynamicScrollBarsView> scrollView  = adoptNS([[customClass alloc] initWithFrame:[oldScrollView frame]]);
    [scrollView setContentView:adoptNS([[WebClipView alloc] initWithFrame:[scrollView bounds]]).get()];
    [scrollView setDrawsBackground:[oldScrollView drawsBackground]];
    [scrollView setHasVerticalScroller:[oldScrollView hasVerticalScroller]];
    [scrollView setHasHorizontalScroller:[oldScrollView hasHorizontalScroller]];
    [scrollView setAutoresizingMask:[oldScrollView autoresizingMask]];
    [scrollView setLineScroll:[oldScrollView lineScroll]];
    [self addSubview:scrollView.get()];

    // don't call our overridden version here; we need to make the standard NSView link between us
    // and our subview so that previousKeyView and previousValidKeyView work as expected. This works
    // together with our becomeFirstResponder and setNextKeyView overrides.
    [super setNextKeyView:scrollView.get()];

    _private->frameScrollView = WTFMove(scrollView);

    [self _setDocumentView:documentView.get()];
    [self _install];

    [oldScrollView removeFromSuperview];
}
#endif // !PLATFORM(IOS_FAMILY)

@end
