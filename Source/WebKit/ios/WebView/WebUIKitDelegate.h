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

#ifndef WebUIKitDelegate_h
#define WebUIKitDelegate_h

#import <WebKitLegacy/WAKAppKitStubs.h>
#import <WebKitLegacy/WKContentObservation.h>

@class DOMNode;
@class DOMDocumentFragment;
@class WAKView;
@class WebDataSource;
@class WebFrame;
@class WebHistoryItem;
@class WebView;
@class WebPluginPackage;
@class WebSecurityOrigin;
@class UIWebPlugInView;
@protocol WebOpenPanelResultListener;

@interface NSObject (WebUIKitDelegate)
- (void)webView:(WebView *)webView didCommitLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;
- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame;
- (void)webViewDidReceiveMobileDocType:(WebView *)webView;
- (void)webView:(WebView *)aWebView didReceiveViewportArguments:(NSDictionary *)arguments;
- (void)webView:(WebView *)aWebView needsScrollNotifications:(NSNumber *)aNumber forFrame:(WebFrame *)aFrame;
- (void)webView:(WebView *)webView saveStateToHistoryItem:(WebHistoryItem *)item forFrame:(WebFrame *)frame;
- (void)webView:(WebView *)webView restoreStateFromHistoryItem:(WebHistoryItem *)item forFrame:(WebFrame *)frame force:(BOOL)force;
- (BOOL)webView:(WebView *)webView shouldScrollToPoint:(CGPoint)point forFrame:(WebFrame *)frame;
- (void)webView:(WebView *)webView didObserveDeferredContentChange:(WKContentChange)aChange forFrame:(WebFrame *)frame;
- (void)webViewDidPreventDefaultForEvent:(WebView *)webView;
- (void)webThreadWebViewDidLayout:(WebView *)webView byScrolling:(BOOL)byScrolling;
- (void)webViewDidStartOverflowScroll:(WebView *)webView;
- (void)webViewDidEndOverflowScroll:(WebView *)webView;

// File Upload support
- (void)webView:(WebView *)webView runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener allowMultipleFiles:(BOOL)allowMultipleFiles acceptMIMETypes:(NSArray *)mimeTypes;

// AutoFill support
- (void)webView:(WebView *)webView willCloseFrame:(WebFrame *)frame;
- (void)webView:(WebView *)webView didFirstLayoutInFrame:(WebFrame *)frame;
- (void)webView:(WebView *)webView didFirstVisuallyNonEmptyLayoutInFrame:(WebFrame *)frame;

// Focus support
- (void)webView:(WebView *)webView elementDidFocusNode:(DOMNode *)node;
- (void)webView:(WebView *)webView elementDidBlurNode:(DOMNode *)node;

// PageCache support
- (void)webViewDidRestoreFromPageCache:(WebView *)webView;

- (NSView *)webView:(WebView *)webView plugInViewWithArguments:(NSDictionary *)arguments fromPlugInPackage:(WebPluginPackage *)package;
- (void)webView:(WebView *)webView willShowFullScreenForPlugInView:(id)plugInView;
- (void)webView:(WebView *)webView didHideFullScreenForPlugInView:(id)plugInView;
- (void)webView:(WebView *)aWebView didReceiveMessage:(NSDictionary *)aMessage;
// FIXME: to be removed when UIKit implements the new one.
- (void)addInputString:(NSString *)str fromVariantKey:(BOOL)isPopupVariant;
- (void)addInputString:(NSString *)str withFlags:(NSUInteger)flags;
- (void)deleteFromInput;

// Accelerated compositing
- (void)_webthread_webView:(WebView*)webView attachRootLayer:(id)rootLayer;
- (void)webViewDidCommitCompositingLayerChanges:(WebView*)webView;

- (void)webView:(WebView*)webView didCreateOrUpdateScrollingLayer:(id)layer withContentsLayer:(id)contentsLayer scrollSize:(NSValue*)sizeValue forNode:(DOMNode *)node
    allowHorizontalScrollbar:(BOOL)allowHorizontalScrollbar allowVerticalScrollbar:(BOOL)allowVerticalScrollbar;
- (void)webView:(WebView*)webView willRemoveScrollingLayer:(id)layer withContentsLayer:(id)contentsLayer forNode:(DOMNode *)node;

- (void)revealedSelectionByScrollingWebFrame:(WebFrame *)webFrame;

// Spellcheck support.
// Returns an array of NSValues containing NSRanges which indicate the misspellings.
- (NSArray *)checkSpellingOfString:(NSString *)stringToCheck;

- (void)webView:(WebView *)webView willAddPlugInView:(id)plugInView;

- (void)webViewDidDrawTiles:(WebView *)sender;

// Pasteboard support delegates
- (void)writeDataToPasteboard:(NSDictionary*)representations;
- (NSArray*)readDataFromPasteboard:(NSString*)type withIndex:(NSInteger)index;
- (NSInteger)getPasteboardItemsCount;
- (NSArray*)supportedPasteboardTypesForCurrentSelection;
- (BOOL)hasRichlyEditableSelection;
- (BOOL)performsTwoStepPaste:(DOMDocumentFragment*)fragment;
- (NSInteger)getPasteboardChangeCount;
- (CGPoint)interactionLocation;
- (void)showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(CGRect)elementRect;

#if ENABLE(ORIENTATION_EVENTS)
- (int)deviceOrientation;
#endif

- (BOOL)isUnperturbedDictationResultMarker:(id)metadataForMarker;
- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message withSource:(NSString *)source;
@end

#endif // WebUIKitDelegate_h
