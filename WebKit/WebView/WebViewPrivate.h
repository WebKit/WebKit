/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import <WebKit/WebView.h>
#import <WebKit/WebFramePrivate.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
#define WebNSInteger int
#else
#define WebNSInteger NSInteger
#endif

@class NSError;
@class WebFrame;
@class WebPreferences;

@protocol WebFormDelegate;

typedef void (*WebDidCancelAuthenticationChallengeFunc)(id, SEL, WebView *, id, NSURLAuthenticationChallenge *, WebDataSource *);
typedef void (*WebDidReceiveAuthenticationChallengeFunc)(id, SEL, WebView *, id, NSURLAuthenticationChallenge *, WebDataSource *);
typedef id (*WebIdentifierForRequestFunc)(id, SEL, WebView *, NSURLRequest *, WebDataSource *);
typedef NSURLRequest *(*WebWillSendRequestFunc)(id, SEL, WebView *, id, NSURLRequest *, NSURLResponse *, WebDataSource *);
typedef void (*WebDidReceiveResponseFunc)(id, SEL, WebView *, id, NSURLResponse *, WebDataSource *);
typedef void (*WebDidReceiveContentLengthFunc)(id, SEL, WebView *, id, WebNSInteger, WebDataSource *);
typedef void (*WebDidFinishLoadingFromDataSourceFunc)(id, SEL, WebView *, id, WebDataSource *);
typedef void (*WebDidFailLoadingWithErrorFromDataSourceFunc)(id, SEL, WebView *, id, NSError *, WebDataSource *);
typedef void (*WebDidLoadResourceFromMemoryCacheFunc)(id, SEL, WebView *, NSURLRequest *, NSURLResponse *, WebNSInteger, WebDataSource *);
typedef NSCachedURLResponse *(*WebWillCacheResponseFunc)(id, SEL, WebView *, id, NSCachedURLResponse *, WebDataSource *);

typedef struct _WebResourceDelegateImplementationCache {
    uint delegateImplementsDidCancelAuthenticationChallenge:1;
    uint delegateImplementsDidReceiveAuthenticationChallenge:1;
    uint delegateImplementsDidReceiveResponse:1;
    uint delegateImplementsDidReceiveContentLength:1;
    uint delegateImplementsDidFinishLoadingFromDataSource:1;
    uint delegateImplementsDidFailLoadingWithErrorFromDataSource:1;
    uint delegateImplementsWillSendRequest:1;
    uint delegateImplementsIdentifierForRequest:1;
    uint delegateImplementsDidLoadResourceFromMemoryCache:1;
    uint delegateImplementsWillCacheResponse:1;

    WebDidCancelAuthenticationChallengeFunc didCancelAuthenticationChallengeFunc;
    WebDidReceiveAuthenticationChallengeFunc didReceiveAuthenticationChallengeFunc;
    WebIdentifierForRequestFunc identifierForRequestFunc;
    WebWillSendRequestFunc willSendRequestFunc;
    WebDidReceiveResponseFunc didReceiveResponseFunc;
    WebDidReceiveContentLengthFunc didReceiveContentLengthFunc;
    WebDidFinishLoadingFromDataSourceFunc didFinishLoadingFromDataSourceFunc;
    WebDidFailLoadingWithErrorFromDataSourceFunc didFailLoadingWithErrorFromDataSourceFunc;
    WebDidLoadResourceFromMemoryCacheFunc didLoadResourceFromMemoryCacheFunc;
    WebWillCacheResponseFunc willCacheResponseFunc;
} WebResourceDelegateImplementationCache;

typedef void (*WebDidClearWindowObjectForFrameFunc)(id, SEL, WebView *, WebScriptObject *, WebFrame *);
typedef void (*WebWindowScriptObjectAvailableFunc)(id, SEL, WebView *, WebScriptObject *);
typedef void (*WebDidHandleOnloadEventsForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidReceiveServerRedirectForProvisionalLoadForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidCancelClientRedirectForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebWillPerformClientRedirectToURLDelayFireDateForFrameFunc)(id, SEL, WebView *, NSURL *, double, NSDate *, WebFrame *);
typedef void (*WebDidChangeLocationWithinPageForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebWillCloseFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidStartProvisionalLoadForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidReceiveTitleForFrameFunc)(id, SEL, WebView *, NSString *, WebFrame *);
typedef void (*WebDidCommitLoadForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidFailProvisionalLoadWithErrorForFrameFunc)(id, SEL, WebView *, NSError *, WebFrame *);
typedef void (*WebDidFailLoadWithErrorForFrameFunc)(id, SEL, WebView *, NSError *, WebFrame *);
typedef void (*WebDidFinishLoadForFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidFirstLayoutInFrameFunc)(id, SEL, WebView *, WebFrame *);
typedef void (*WebDidReceiveIconForFrameFunc)(id, SEL, WebView *, NSImage *, WebFrame *);
typedef void (*WebDidFinishDocumentLoadForFrameFunc)(id, SEL, WebView *, WebFrame *);

typedef struct _WebFrameLoadDelegateImplementationCache {
    uint delegateImplementsDidClearWindowObjectForFrame: 1;
    uint delegateImplementsWindowScriptObjectAvailable: 1;
    uint delegateImplementsDidHandleOnloadEventsForFrame: 1;
    uint delegateImplementsDidReceiveServerRedirectForProvisionalLoadForFrame: 1;
    uint delegateImplementsDidCancelClientRedirectForFrame: 1;
    uint delegateImplementsWillPerformClientRedirectToURLDelayFireDateForFrame: 1;
    uint delegateImplementsDidChangeLocationWithinPageForFrame: 1;
    uint delegateImplementsWillCloseFrame: 1;
    uint delegateImplementsDidStartProvisionalLoadForFrame: 1;
    uint delegateImplementsDidReceiveTitleForFrame: 1;
    uint delegateImplementsDidCommitLoadForFrame: 1;
    uint delegateImplementsDidFailProvisionalLoadWithErrorForFrame: 1;
    uint delegateImplementsDidFailLoadWithErrorForFrame: 1;
    uint delegateImplementsDidFinishLoadForFrame: 1;
    uint delegateImplementsDidFirstLayoutInFrame: 1;
    uint delegateImplementsDidReceiveIconForFrame: 1;
    uint delegateImplementsDidFinishDocumentLoadForFrame: 1;

    WebDidClearWindowObjectForFrameFunc didClearWindowObjectForFrameFunc;
    WebWindowScriptObjectAvailableFunc windowScriptObjectAvailableFunc;
    WebDidHandleOnloadEventsForFrameFunc didHandleOnloadEventsForFrameFunc;
    WebDidReceiveServerRedirectForProvisionalLoadForFrameFunc didReceiveServerRedirectForProvisionalLoadForFrameFunc;
    WebDidCancelClientRedirectForFrameFunc didCancelClientRedirectForFrameFunc;
    WebWillPerformClientRedirectToURLDelayFireDateForFrameFunc willPerformClientRedirectToURLDelayFireDateForFrameFunc;
    WebDidChangeLocationWithinPageForFrameFunc didChangeLocationWithinPageForFrameFunc;
    WebWillCloseFrameFunc willCloseFrameFunc;
    WebDidStartProvisionalLoadForFrameFunc didStartProvisionalLoadForFrameFunc;
    WebDidReceiveTitleForFrameFunc didReceiveTitleForFrameFunc;
    WebDidCommitLoadForFrameFunc didCommitLoadForFrameFunc;
    WebDidFailProvisionalLoadWithErrorForFrameFunc didFailProvisionalLoadWithErrorForFrameFunc;
    WebDidFailLoadWithErrorForFrameFunc didFailLoadWithErrorForFrameFunc;
    WebDidFinishLoadForFrameFunc didFinishLoadForFrameFunc;
    WebDidFirstLayoutInFrameFunc didFirstLayoutInFrameFunc;
    WebDidReceiveIconForFrameFunc didReceiveIconForFrameFunc;
    WebDidFinishDocumentLoadForFrameFunc didFinishDocumentLoadForFrameFunc;
} WebFrameLoadDelegateImplementationCache;

extern NSString *_WebCanGoBackKey;
extern NSString *_WebCanGoForwardKey;
extern NSString *_WebEstimatedProgressKey;
extern NSString *_WebIsLoadingKey;
extern NSString *_WebMainFrameIconKey;
extern NSString *_WebMainFrameTitleKey;
extern NSString *_WebMainFrameURLKey;
extern NSString *_WebMainFrameDocumentKey;

// pending public WebElementDictionary keys
extern NSString *WebElementTitleKey;             // NSString of the title of the element (used by Safari)
extern NSString *WebElementSpellingToolTipKey;   // NSString of a tooltip representing misspelling or bad grammar (used internally)
extern NSString *WebElementIsContentEditableKey; // NSNumber indicating whether the inner non-shared node is content editable (used internally)

// other WebElementDictionary keys
extern NSString *WebElementLinkIsLiveKey;        // NSNumber of BOOL indictating whether the link is live or not

typedef enum {
    WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows,
    WebDashboardBehaviorAlwaysSendActiveNullEventsToPlugIns,
    WebDashboardBehaviorAlwaysAcceptsFirstMouse,
    WebDashboardBehaviorAllowWheelScrolling,
    WebDashboardBehaviorUseBackwardCompatibilityMode
} WebDashboardBehavior;

@interface WebController : NSTreeController {
    IBOutlet WebView *webView;
}
- (WebView *)webView;
- (void)setWebView:(WebView *)newWebView;
@end

@interface WebView (WebViewEditingActionsPendingPublic)

- (void)outdent:(id)sender;

@end

@interface WebView (WebPendingPublic)

/*!
@method searchFor:direction:caseSensitive:wrap:startInSelection:
 @abstract Searches a document view for a string and highlights the string if it is found.
 Starts the search from the current selection.  Will search across all frames.
 @param string The string to search for.
 @param forward YES to search forward, NO to seach backwards.
 @param caseFlag YES to for case-sensitive search, NO for case-insensitive search.
 @param wrapFlag YES to wrap around, NO to avoid wrapping.
 @param startInSelection YES to begin search in the selected text (useful for incremental searching), NO to begin search after the selected text.
 @result YES if found, NO if not found.
 */
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection;

- (void)setMainFrameDocumentReady:(BOOL)mainFrameDocumentReady;

- (void)setTabKeyCyclesThroughElements:(BOOL)cyclesElements;
- (BOOL)tabKeyCyclesThroughElements;

- (void)scrollDOMRangeToVisible:(DOMRange *)range;

// setHoverFeedbackSuspended: can be called by clients that want to temporarily prevent the webView
// from displaying feedback about mouse position. Each WebDocumentView class that displays feedback
// about mouse position should honor this setting.
- (void)setHoverFeedbackSuspended:(BOOL)newValue;
- (BOOL)isHoverFeedbackSuspended;

/*!
@method setScriptDebugDelegate:
@abstract Set the WebView's WebScriptDebugDelegate delegate.
@param delegate The WebScriptDebugDelegate to set as the delegate.
*/    
- (void)setScriptDebugDelegate:(id)delegate;

/*!
@method scriptDebugDelegate
@abstract Return the WebView's WebScriptDebugDelegate.
@result The WebView's WebScriptDebugDelegate.
*/    
- (id)scriptDebugDelegate;

- (BOOL)shouldClose;

/*!
    @method aeDescByEvaluatingJavaScriptFromString:
    @param script The text of the JavaScript.
    @result The result of the script, converted to an NSAppleEventDescriptor, or nil for failure.
*/
- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)script;

// Support for displaying multiple text matches.
// These methods might end up moving into a protocol, so different document types can specify
// whether or not they implement the protocol. For now we'll just deal with HTML.
// These methods are still in flux; don't rely on them yet.
- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(unsigned)limit;
- (void)unmarkAllTextMatches;
- (NSArray *)rectsForTextMatches;

// Support for disabling registration with the undo manager. This is equivalent to the methods with the same names on NSTextView.
- (BOOL)allowsUndo;
- (void)setAllowsUndo:(BOOL)flag;

@end

@interface WebView (WebPrivate)

+ (BOOL)_developerExtrasEnabled;
+ (BOOL)_scriptDebuggerEnabled;

/*!
    @method setBackgroundColor:
    @param backgroundColor Color to use as the default background.
    @abstract Sets what color the receiver draws under transparent page background colors and images.
    This color is also used when no page is loaded. A color with alpha should only be used when the receiver is
    in a non-opaque window, since the color is drawn using NSCompositeCopy.
*/
- (void)setBackgroundColor:(NSColor *)backgroundColor;

/*!
    @method backgroundColor
    @result Returns the background color drawn under transparent page background colors and images.
    This color is also used when no page is loaded. A color with alpha should only be used when the receiver is
    in a non-opaque window, since the color is drawn using NSCompositeCopy.
*/
- (NSColor *)backgroundColor;

/*!
Could be worth adding to the API.
 @method loadItemsFromOtherView:
 @abstract Loads the view with the contents of the other view, including its backforward list.
 @param otherView   The WebView from which to copy contents.
 */
- (void)_loadBackForwardListFromOtherView:(WebView *)otherView;

+ (NSArray *)_supportedFileExtensions;

/*!
    @method canShowFile:
    @abstract Checks if the WebKit can show the content of the file at the specified path.
    @param path The path of the file to check
    @result YES if the WebKit can show the content of the file at the specified path.
*/
+ (BOOL)canShowFile:(NSString *)path;

/*!
    @method suggestedFileExtensionForMIMEType:
    @param MIMEType The MIME type to check.
    @result The extension based on the MIME type
*/
+ (NSString *)suggestedFileExtensionForMIMEType: (NSString *)MIMEType;

// May well become public
- (void)_setFormDelegate:(id<WebFormDelegate>)delegate;
- (id<WebFormDelegate>)_formDelegate;

- (void)_close;

/*!
    @method _registerViewClass:representationClass:forURLScheme:
    @discussion Register classes that implement WebDocumentView and WebDocumentRepresentation respectively.
    @param viewClass The WebDocumentView class to use to render data for a given MIME type.
    @param representationClass The WebDocumentRepresentation class to use to represent data of the given MIME type.
    @param scheme The URL scheme to represent with an object of the given class.
*/
+ (void)_registerViewClass:(Class)viewClass representationClass:(Class)representationClass forURLScheme:(NSString *)URLScheme;

+ (void)_unregisterViewClassAndRepresentationClassForMIMEType:(NSString *)MIMEType;

/*!
     @method _canHandleRequest:
     @abstract Performs a "preflight" operation that performs some
     speculative checks to see if a request can be used to create
     a WebDocumentView and WebDocumentRepresentation.
     @discussion The result of this method is valid only as long as no
     protocols or schemes are registered or unregistered, and as long as
     the request is not mutated (if the request is mutable). Hence, clients
     should be prepared to handle failures even if they have performed request
     preflighting by caling this method.
     @param request The request to preflight.
     @result YES if it is likely that a WebDocumentView and WebDocumentRepresentation
     can be created for the request, NO otherwise.
*/
+ (BOOL)_canHandleRequest:(NSURLRequest *)request;

+ (NSString *)_decodeData:(NSData *)data;

+ (void)_setAlwaysUseATSU:(BOOL)f;

- (NSCachedURLResponse *)_cachedResponseForURL:(NSURL *)URL;

- (void)_addScrollerDashboardRegions:(NSMutableDictionary *)regions;
- (NSDictionary *)_dashboardRegions;

- (void)_setDashboardBehavior:(WebDashboardBehavior)behavior to:(BOOL)flag;
- (BOOL)_dashboardBehavior:(WebDashboardBehavior)behavior;

+ (void)_setShouldUseFontSmoothing:(BOOL)f;
+ (BOOL)_shouldUseFontSmoothing;

+ (NSString *)_minimumRequiredSafariBuildNumber;

/*!
    @method setAlwaysShowVerticalScroller:
    @result Forces the vertical scroller to be visible if flag is YES, otherwise
    if flag is NO the scroller with automatically show and hide as needed.
 */
- (void)setAlwaysShowVerticalScroller:(BOOL)flag;

/*!
    @method alwaysShowVerticalScroller
    @result YES if the vertical scroller is always shown
 */
- (BOOL)alwaysShowVerticalScroller;

/*!
    @method setAlwaysShowHorizontalScroller:
    @result Forces the horizontal scroller to be visible if flag is YES, otherwise
    if flag is NO the scroller with automatically show and hide as needed.
 */
- (void)setAlwaysShowHorizontalScroller:(BOOL)flag;

/*!
    @method alwaysShowHorizontalScroller
    @result YES if the horizontal scroller is always shown
 */
- (BOOL)alwaysShowHorizontalScroller;

/*!
    @method setProhibitsMainFrameScrolling:
    @abstract Prohibits scrolling in the WebView's main frame.  Used to "lock" a WebView
    to a specific scroll position.
  */
- (void)setProhibitsMainFrameScrolling:(BOOL)prohibits;

/*!
    @method _setAdditionalWebPlugInPaths:
    @abstract Sets additional plugin search paths for a specific WebView.
 */
- (void)_setAdditionalWebPlugInPaths:(NSArray *)newPaths;

/*!
    @method _setInViewSourceMode:
    @abstract Used to place a WebView into a special source-viewing mode.
  */
- (void)_setInViewSourceMode:(BOOL)flag;

/*!
    @method _inViewSourceMode;
    @abstract Whether or not the WebView is in source-view mode for HTML.
  */
- (BOOL)_inViewSourceMode;

/*!
    @method _attachScriptDebuggerToAllFrames
    @abstract Attaches a script debugger to all frames belonging to the receiver.
 */
- (void)_attachScriptDebuggerToAllFrames;

/*!
    @method _detachScriptDebuggerFromAllFrames
    @abstract Detaches any script debuggers from all frames belonging to the receiver.
 */
- (void)_detachScriptDebuggerFromAllFrames;

- (BOOL)defersCallbacks; // called by QuickTime plug-in
- (void)setDefersCallbacks:(BOOL)defer; // called by QuickTime plug-in

// Re-adding this for the third time - please see <rdar://problem/5217124> for details 
- (void)handleAuthenticationForResource:(id)identifier challenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource;

@end

@interface WebView (WebViewPrintingPrivate)
/*!
    @method _adjustPrintingMarginsForHeaderAndFooter:
    @abstract Increase the top and bottom margins for the current print operation to
    account for the header and footer height. 
    @discussion Called by <WebDocument> implementors once when a print job begins. If the
    <WebDocument> implementor implements knowsPageRange:, this should be called from there.
    Otherwise this should be called from beginDocument. The <WebDocument> implementors need
    to also call _drawHeaderAndFooter.
*/
- (void)_adjustPrintingMarginsForHeaderAndFooter;

/*!
    @method _drawHeaderAndFooter
    @abstract Gives the WebView's UIDelegate a chance to draw a header and footer on the
    printed page. 
    @discussion This should be called by <WebDocument> implementors from an override of
    drawPageBorderWithSize:.
*/
- (void)_drawHeaderAndFooter;
@end

@interface WebView (WebViewGrammarChecking)

// FIXME: These two methods should be merged into WebViewEditing when we're not in API freeze
- (BOOL)isGrammarCheckingEnabled;
#ifndef BUILDING_ON_TIGER
- (void)setGrammarCheckingEnabled:(BOOL)flag;

// FIXME: This method should be merged into WebIBActions when we're not in API freeze
- (void)toggleGrammarChecking:(id)sender;
#endif
@end

@interface WebView (WebViewEditingInMail)
- (void)_insertNewlineInQuotedContent;
- (BOOL)_selectWordBeforeMenuEvent;
- (void)_setSelectWordBeforeMenuEvent:(BOOL)flag;
- (void)_replaceSelectionWithNode:(DOMNode *)node matchStyle:(BOOL)matchStyle;
@end

@interface _WebSafeForwarder : NSObject
{
    id target; // Non-retained. Don't retain delegates.
    id defaultTarget;
    Class templateClass;
}
- (id)initWithTarget:(id)t defaultTarget:(id)dt templateClass:(Class)aClass;
+ (id)safeForwarderWithTarget:(id)t defaultTarget:(id)dt templateClass:(Class)aClass;
@end

@interface NSObject (WebFrameLoadDelegatePrivate)
- (void)webView:(WebView *)sender didFirstLayoutInFrame:(WebFrame *)frame;

// didFinishDocumentLoadForFrame is sent when the document has finished loading, though not necessarily all
// of its subresources.
- (void)webView:(WebView *)sender didFinishDocumentLoadForFrame:(WebFrame *)frame;

// Addresses 4192534.  SPI for now.
- (void)webView:(WebView *)sender didHandleOnloadEventsForFrame:(WebFrame *)frame;

/*!
    @method webView:didClearWindowObject:forFrame:
    @abstract Notifies the delegate that the JavaScript window object in a frame has 
    been cleared in preparation for a new load. This is the preferred place to set custom 
    properties on the window object using the WebScriptObject and JavaScriptCore APIs.
    @param webView The webView sending the message.
    @param windowObject The WebScriptObject representing the frame's JavaScript window object.
    @param frame The WebFrame to which windowObject belongs.
    @discussion If a delegate implements both webView:didClearWindowObject:forFrame:
    and webView:windowScriptObjectAvailable:, only webView:didClearWindowObject:forFrame: 
    will be invoked. This enables a delegate to implement both methods for backwards 
    compatibility with older versions of WebKit.
*/
- (void)webView:(WebView *)webView didClearWindowObject:(WebScriptObject *)windowObject forFrame:(WebFrame *)frame;

@end

@interface NSObject (WebResourceLoadDelegatePrivate)
// Addresses <rdar://problem/5008925> - SPI for now
- (NSCachedURLResponse *)webView:(WebView *)sender resource:(id)identifier willCacheResponse:(NSCachedURLResponse *)response fromDataSource:(WebDataSource *)dataSource;
@end

#undef WebNSInteger
