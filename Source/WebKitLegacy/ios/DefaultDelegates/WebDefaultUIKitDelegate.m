/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WebDefaultUIKitDelegate.h"

#import "DOMRange.h"
#import "WebUIDelegate.h"

static WebDefaultUIKitDelegate *sharedDelegate = nil;

@implementation WebDefaultUIKitDelegate

+ (WebDefaultUIKitDelegate *)sharedUIKitDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultUIKitDelegate alloc] init];
    }
    return sharedDelegate;
}

- (CGPoint)contentsPointForWebView:(WebView *)aWebView
{
    return CGPointZero;
}

- (CGRect)documentVisibleRectForWebView:(WebView *)aWebView
{
    return CGRectZero;
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{

}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{

}

- (void)webView:(WebView *)webView saveStateToHistoryItem:(WebHistoryItem *)item forFrame:(WebFrame *)frame
{
    
}

- (void)webView:(WebView *)webView restoreStateFromHistoryItem:(WebHistoryItem *)item forFrame:(WebFrame *)frame force:(BOOL)force
{
    
}

- (void)webView:(WebView *)aWebView didReceiveViewportArguments:(NSDictionary *)arguments
{

}

- (void)webView:(WebView *)aWebView needsScrollNotifications:(NSNumber *)aNumber forFrame:(WebFrame *)aFrame
{
    
}

- (void)webView:(WebView *)aWebView didObserveDeferredContentChange:(WKContentChange)aChange forFrame:(WebFrame *)frame
{
    
}

- (void)webViewDidPreventDefaultForEvent:(WebView *)webView
{
}

- (BOOL)webView:(WebView *)webView shouldScrollToPoint:(CGPoint)point forFrame:(WebFrame *)frame
{
    return YES;
}

- (void)webView:(WebView *)webView willCloseFrame:(WebFrame *)frame
{
    
}

- (void)webView:(WebView *)webView didFinishDocumentLoadForFrame:(WebFrame *)frame
{
    
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView didFirstLayoutInFrame:(WebFrame *)frame
{
    
}

- (void)webView:(WebView *)webView didFirstVisuallyNonEmptyLayoutInFrame:(WebFrame *)frame
{
}

- (void)webView:(WebView *)webView elementDidFocusNode:(DOMNode *)node
{
}

- (void)webView:(WebView *)webView elementDidBlurNode:(DOMNode *)node
{
}

- (void)webViewDidRestoreFromPageCache:(WebView *)webView
{
}

- (void)webViewDidReceiveMobileDocType:(WebView *)webView
{
    
}

- (NSView *)webView:(WebView *)webView plugInViewWithArguments:(NSDictionary *)arguments fromPlugInPackage:(WebPluginPackage *)package
{
    return nil;
}

- (void)webView:(WebView *)webView willShowFullScreenForPlugInView:(id)plugInView
{
}

- (void)webView:(WebView *)webView didHideFullScreenForPlugInView:(id)plugInView
{
}

- (void)webView:(WebView *)aWebView didReceiveMessage:(NSDictionary *)aMessage
{
}

- (BOOL)handleKeyCommandForCurrentEvent
{
    return NO;
}

- (void)addInputString:(NSString *)str withFlags:(NSUInteger)flags
{
}

// FIXME: to be removed when UIKit implements the new one below.
- (void)deleteFromInput
{
}

- (void)deleteFromInputWithFlags:(NSUInteger)flags
{
}

- (void)_webthread_webView:(WebView *)sender attachRootLayer:(id)layer
{
}

- (void)webViewDidCommitCompositingLayerChanges:(WebView*)webView
{
}

- (void)webView:(WebView*)webView didCreateOrUpdateScrollingLayer:(id)layer withContentsLayer:(id)contentsLayer scrollSize:(NSValue*)sizeValue forNode:(DOMNode *)node
    allowHorizontalScrollbar:(BOOL)allowHorizontalScrollbar allowVerticalScrollbar:(BOOL)allowVerticalScrollbar
{
}

- (void)webView:(WebView*)webView willRemoveScrollingLayer:(id)layer withContentsLayer:(id)contentsLayer forNode:(DOMNode *)node
{
}

- (void)revealedSelectionByScrollingWebFrame:(WebFrame *)webFrame
{
}

- (void)webViewDidLayout:(WebView *)webView
{
}

- (void)webViewDidStartOverflowScroll:(WebView *)webView
{
}

- (void)webViewDidEndOverflowScroll:(WebView *)webView
{
}

- (void)webView:(WebView *)webView runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener configuration:(NSDictionary *)configuration
{
    [resultListener cancel];
}

- (NSArray *)checkSpellingOfString:(NSString *)stringToCheck
{
    return nil;
}

- (void)writeDataToPasteboard:(NSDictionary *)representations
{
}

- (NSArray*)readDataFromPasteboard:(NSString*)type withIndex:(NSInteger)index
{
    return nil;
}

- (NSInteger)getPasteboardItemsCount
{
    return 0;
}

- (NSArray*)supportedPasteboardTypesForCurrentSelection 
{ 
    return nil; 
} 

- (CGPoint)interactionLocation
{
    return CGPointZero;
}

- (void)showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(CGRect)elementRect
{
}

#if ENABLE(ORIENTATION_EVENTS)
- (int)deviceOrientation
{
    return 0;
}
#endif

- (BOOL)hasRichlyEditableSelection
{
    return NO;
}

- (BOOL)performsTwoStepPaste:(DOMDocumentFragment*)fragment
{
    return NO;
}

- (BOOL)performTwoStepDrop:(DOMDocumentFragment *)fragment atDestination:(DOMRange *)destination isMove:(BOOL)isMove
{
    return NO;
}

- (NSInteger)getPasteboardChangeCount
{
    return 0;
}

- (BOOL)isUnperturbedDictationResultMarker:(id)metadataForMarker
{
    return NO;
}

- (void)webView:(WebView *)webView willAddPlugInView:(id)plugInView
{
}

- (void)webViewDidDrawTiles:(WebView *)webView
{
}

- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message withSource:(NSString *)source
{
}
@end

#endif // PLATFORM(IOS_FAMILY)
