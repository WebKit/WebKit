/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
#import "DumpRenderTreeBrowserView.h"

#if PLATFORM(IOS_FAMILY)

#import <WebCore/WebCoreThreadRun.h>
#import <WebKit/WebCoreThread.h>
#import <WebKit/WebUIDelegate.h>
#import <WebKit/WebUIKitDelegate.h>
#import <WebKit/WebView.h>

@interface UIWebBrowserView (WebUIKitDelegate)
- (BOOL)webView:(WebView *)webView shouldScrollToPoint:(CGPoint)point forFrame:(WebFrame *)frame;
@end

@interface UIWebBrowserView (UIKitInternals)
- (CGRect)_documentViewVisibleRect;
@end

@implementation DumpRenderTreeBrowserView

@synthesize scrollingUsesUIWebScrollView = _scrollingUsesUIWebScrollView;

// We override [UIWebBrowserView addInputString] to avoid UIKit keyboard blocking
// sending input strings to webkit because we don't have a interaction element in
// DRT. Interaction element is only set by user tapping a element on the screen.
//
// see: <rdar://problem/8040227> DumpRenderTree: make addInputString work in iPhone DRT
// see: <rdar://problem/10499625> DumpRenderTree: DRT should always use the UIScrollView for scrolling
- (void)addInputString:(NSString *)str
{
    [[self webView] insertText:str];
}

// This is temporary solution to make window.scroll work in DumpRenderTree. The reason is
// UIWebDocumentView's shouldScrollToPoint always tells WebKit not to scroll.  This makes
// sense for MobileSafari/UIWebView app that there is top scroller and the scroller is
// move to the right spot.  But DRT doesn't have scroller so the page never scrolls.
//
// see: <rdar://problem/8153438> DumpRenderTree: DRT needs to match MobileSafari/UIWebView app more closely.
- (BOOL)webView:(WebView *)webView shouldScrollToPoint:(CGPoint)point forFrame:(WebFrame *)frame
{
    if (_scrollingUsesUIWebScrollView)
        return [super webView:webView shouldScrollToPoint:point forFrame:frame];
    if ([webView mainFrame] == frame)
        [self sendScrollEventIfNecessaryWasUserScroll:NO];

    return YES;
}

- (void)webView:(WebView *)sender addMessageToConsole:(NSDictionary *)dictionary withSource:(NSString *)source
{
    // Forward this to DRT UIDelegate since iOS WebKit by default sends this message to UIKitDelegate.
    id uiDelegate = [[self webView] UIDelegate];
    [uiDelegate webView:sender addMessageToConsole:dictionary withSource:source];
}

- (void)webView:(WebView *)webView runOpenPanelForFileButtonWithResultListener:(id <WebOpenPanelResultListener>)resultListener configuration:(NSDictionary *)configuration
{
    WebThreadLock();
    BOOL allowMultipleFiles = [configuration[WebOpenPanelConfigurationAllowMultipleFilesKey] boolValue];
    [webView.UIDelegate webView:webView runOpenPanelForFileButtonWithResultListener:resultListener allowMultipleFiles:allowMultipleFiles];
}

@end

@implementation DumpRenderTreeBrowserView (DRTTesting)

- (CGRect)documentVisibleRect
{
    return [self _documentViewVisibleRect];
}

@end

@interface DumpRenderTreeWebScrollViewDelegate : NSObject <UIScrollViewDelegate> {
    DumpRenderTreeWebScrollView *_scrollView;
}
- (instancetype)initWithScrollView:(DumpRenderTreeWebScrollView *)scrollView;
@end


@implementation DumpRenderTreeWebScrollView

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    self.scrollViewDelegate = [[[DumpRenderTreeWebScrollViewDelegate alloc] initWithScrollView:self] autorelease];
    self.delegate = self.scrollViewDelegate;

    return self;
}

- (void)dealloc
{
    self.scrollViewDelegate = nil;
    [super dealloc];
}

- (void)zoomToScale:(double)scale animated:(BOOL)animated completionHandler:(void (^)(void))completionHandler
{
    ASSERT(!self.zoomToScaleCompletionHandler);
    self.zoomToScaleCompletionHandler = completionHandler;

    [self setZoomScale:scale animated:animated];
}

- (void)completedZoomToScale
{
    if (self.zoomToScaleCompletionHandler) {
        auto completionHandlerCopy = Block_copy(self.zoomToScaleCompletionHandler);

        WebThreadRun(^{
            completionHandlerCopy();
            Block_release(completionHandlerCopy);
        });
        self.zoomToScaleCompletionHandler = nullptr;
    }
}

@end

@implementation DumpRenderTreeWebScrollViewDelegate

- (instancetype)initWithScrollView:(DumpRenderTreeWebScrollView *)scrollView
{
    self = [super init];
    if (!self)
        return nil;
    
    _scrollView = scrollView;
    return self;
}

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{
    return [scrollView.subviews firstObject];
}

// UIScrollView delegate methods.
- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    [_scrollView completedZoomToScale];
}

@end

#endif // PLATFORM(IOS_FAMILY)
