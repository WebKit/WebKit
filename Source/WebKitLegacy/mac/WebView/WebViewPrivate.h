/*
 * Copyright (C) 2005-2014 Apple Inc. All rights reserved.
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

#import <WebKitLegacy/WebView.h>
#import <WebKitLegacy/WebFramePrivate.h>
#import <JavaScriptCore/JSBase.h>

#if TARGET_OS_IPHONE
#import <CoreGraphics/CGColor.h>
#import <WebKitLegacy/WAKView.h>
#endif

#if !defined(ENABLE_DASHBOARD_SUPPORT)
#if TARGET_OS_IPHONE
#define ENABLE_DASHBOARD_SUPPORT 0
#else
#define ENABLE_DASHBOARD_SUPPORT 1
#endif
#endif

#if !defined(ENABLE_REMOTE_INSPECTOR)
// FIXME: Should we just remove this ENABLE flag everywhere?
#define ENABLE_REMOTE_INSPECTOR 1
#endif

#if !defined(ENABLE_TOUCH_EVENTS)
#if TARGET_OS_IPHONE
#define ENABLE_TOUCH_EVENTS 1
#else
#define ENABLE_TOUCH_EVENTS 0
#endif
#endif

@class UIColor;
@class UIImage;
@class NSError;
@class WebFrame;
@class WebDeviceOrientation;
@class WebGeolocationPosition;
@class WebInspector;
@class WebNotification;
@class WebPreferences;
@class WebScriptWorld;
@class WebSecurityOrigin;
@class WebTextIterator;
#if TARGET_OS_IPHONE
@class CALayer;
@class WebFixedPositionContent;

@protocol WebCaretChangeListener;
#endif
@protocol WebDeviceOrientationProvider;
@protocol WebFormDelegate;

#if !TARGET_OS_IPHONE
extern NSString *_WebCanGoBackKey;
extern NSString *_WebCanGoForwardKey;
extern NSString *_WebEstimatedProgressKey;
extern NSString *_WebIsLoadingKey;
extern NSString *_WebMainFrameIconKey;
extern NSString *_WebMainFrameTitleKey;
extern NSString *_WebMainFrameURLKey;
extern NSString *_WebMainFrameDocumentKey;
#endif

#if TARGET_OS_IPHONE
extern NSString * const WebViewProgressEstimatedProgressKey;
extern NSString * const WebViewProgressBackgroundColorKey;
#endif

// pending public WebElementDictionary keys
extern NSString *WebElementTitleKey;             // NSString of the title of the element (used by Safari)
extern NSString *WebElementSpellingToolTipKey;   // NSString of a tooltip representing misspelling or bad grammar (used internally)
extern NSString *WebElementIsContentEditableKey; // NSNumber indicating whether the inner non-shared node is content editable (used internally)
extern NSString *WebElementMediaURLKey;          // NSURL of the media element

// other WebElementDictionary keys
extern NSString *WebElementLinkIsLiveKey;        // NSNumber of BOOL indicating whether the link is live or not
extern NSString *WebElementIsInScrollBarKey;

// One of the subviews of the WebView entered compositing mode.
extern NSString *_WebViewDidStartAcceleratedCompositingNotification;

#if TARGET_OS_IPHONE
extern NSString *WebQuickLookFileNameKey;
extern NSString *WebQuickLookUTIKey;
#endif

#if TARGET_OS_IOS
@protocol UIDropSession;
#endif

extern NSString * const WebViewWillCloseNotification;

#if ENABLE_DASHBOARD_SUPPORT
// FIXME: Remove this once it is verified no one is dependent on it.
typedef enum {
    WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows,
    WebDashboardBehaviorAlwaysSendActiveNullEventsToPlugIns,
    WebDashboardBehaviorAlwaysAcceptsFirstMouse,
    WebDashboardBehaviorAllowWheelScrolling,
    WebDashboardBehaviorUseBackwardCompatibilityMode
} WebDashboardBehavior;
#endif

typedef enum {
    WebInjectAtDocumentStart,
    WebInjectAtDocumentEnd,
} WebUserScriptInjectionTime;

typedef enum {
    WebInjectInAllFrames,
    WebInjectInTopFrameOnly
} WebUserContentInjectedFrames;

enum {
    WebFindOptionsCaseInsensitive = 1 << 0,
    WebFindOptionsAtWordStarts = 1 << 1,
    WebFindOptionsTreatMedialCapitalAsWordStart = 1 << 2,
    WebFindOptionsBackwards = 1 << 3,
    WebFindOptionsWrapAround = 1 << 4,
    WebFindOptionsStartInSelection = 1 << 5
};
typedef NSUInteger WebFindOptions;

typedef enum {
    WebPaginationModeUnpaginated,
    WebPaginationModeLeftToRight,
    WebPaginationModeRightToLeft,
    WebPaginationModeTopToBottom,
    WebPaginationModeBottomToTop,
} WebPaginationMode;

enum {
    WebDidFirstLayout = 1 << 0,
    WebDidFirstVisuallyNonEmptyLayout = 1 << 1,
    WebDidHitRelevantRepaintedObjectsAreaThreshold = 1 << 2
};
typedef NSUInteger WebLayoutMilestones;

typedef enum {
    WebPageVisibilityStateVisible,
    WebPageVisibilityStateHidden,
    WebPageVisibilityStatePrerender
} WebPageVisibilityState;

typedef enum {
    WebNotificationPermissionAllowed,
    WebNotificationPermissionNotAllowed,
    WebNotificationPermissionDenied
} WebNotificationPermission;

@interface WebUITextIndicatorData : NSObject
@property (nonatomic, retain) UIImage *dataInteractionImage;
@property (nonatomic, assign) CGRect selectionRectInRootViewCoordinates;
@property (nonatomic, assign) CGRect textBoundingRectInRootViewCoordinates;
@property (nonatomic, retain) NSArray<NSValue *> *textRectsInBoundingRectCoordinates; // CGRect values
@property (nonatomic, assign) CGFloat contentImageScaleFactor;
@property (nonatomic, retain) UIImage *contentImageWithHighlight;
@property (nonatomic, retain) UIImage *contentImage;
@property (nonatomic, retain) UIImage *contentImageWithoutSelection;
@property (nonatomic, assign) CGRect contentImageWithoutSelectionRectInRootViewCoordinates;
@property (nonatomic, retain) UIColor *estimatedBackgroundColor;
@end

#if !TARGET_OS_IPHONE
@interface WebController : NSTreeController {
    IBOutlet WebView *webView;
}
- (WebView *)webView;
- (void)setWebView:(WebView *)newWebView;
@end
#endif

@interface WebView (WebViewEditingActionsPendingPublic)

- (void)outdent:(id)sender;
- (NSDictionary *)typingAttributes;

@end

@interface WebView (WebPendingPublic)

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;
- (void)unscheduleFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode;

- (BOOL)findString:(NSString *)string options:(WebFindOptions)options;
- (DOMRange *)DOMRangeOfString:(NSString *)string relativeTo:(DOMRange *)previousRange options:(WebFindOptions)options;

- (void)setMainFrameDocumentReady:(BOOL)mainFrameDocumentReady;

- (void)setTabKeyCyclesThroughElements:(BOOL)cyclesElements;
- (BOOL)tabKeyCyclesThroughElements;

- (void)scrollDOMRangeToVisible:(DOMRange *)range;
#if TARGET_OS_IPHONE
- (void)scrollDOMRangeToVisible:(DOMRange *)range withInset:(CGFloat)inset;
#endif

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

/*!
    @method setHistoryDelegate:
    @abstract Set the WebView's WebHistoryDelegate delegate.
    @param delegate The WebHistoryDelegate to set as the delegate.
*/    
- (void)setHistoryDelegate:(id)delegate;

/*!
    @method historyDelegate
    @abstract Return the WebView's WebHistoryDelegate delegate.
    @result The WebView's WebHistoryDelegate delegate.
*/    
- (id)historyDelegate;

- (BOOL)shouldClose;

#if !TARGET_OS_IPHONE
/*!
    @method aeDescByEvaluatingJavaScriptFromString:
    @param script The text of the JavaScript.
    @result The result of the script, converted to an NSAppleEventDescriptor, or nil for failure.
*/
- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)script;
#endif

// Support for displaying multiple text matches.
// These methods might end up moving into a protocol, so different document types can specify
// whether or not they implement the protocol. For now we'll just deal with HTML.
// These methods are still in flux; don't rely on them yet.
- (BOOL)canMarkAllTextMatches;
- (NSUInteger)countMatchesForText:(NSString *)string options:(WebFindOptions)options highlight:(BOOL)highlight limit:(NSUInteger)limit markMatches:(BOOL)markMatches;
- (NSUInteger)countMatchesForText:(NSString *)string inDOMRange:(DOMRange *)range options:(WebFindOptions)options highlight:(BOOL)highlight limit:(NSUInteger)limit markMatches:(BOOL)markMatches;
- (void)unmarkAllTextMatches;
- (NSArray *)rectsForTextMatches;

// Support for disabling registration with the undo manager. This is equivalent to the methods with the same names on NSTextView.
- (BOOL)allowsUndo;
- (void)setAllowsUndo:(BOOL)flag;

/*!
    @method setPageSizeMultiplier:
    @abstract Change the zoom factor of the page in views managed by this webView.
    @param multiplier A fractional percentage value, 1.0 is 100%.
*/    
- (void)setPageSizeMultiplier:(float)multiplier;

/*!
    @method pageSizeMultiplier
    @result The page size multipler.
*/    
- (float)pageSizeMultiplier;

// Commands for doing page zoom.  Will end up in WebView (WebIBActions) <NSUserInterfaceValidations>
- (BOOL)canZoomPageIn;
- (IBAction)zoomPageIn:(id)sender;
- (BOOL)canZoomPageOut;
- (IBAction)zoomPageOut:(id)sender;
- (BOOL)canResetPageZoom;
- (IBAction)resetPageZoom:(id)sender;

// Sets a master volume control for all media elements in the WebView. Valid values are 0..1.
- (void)setMediaVolume:(float)volume;
- (float)mediaVolume;

- (void)suspendAllMediaPlayback;
- (void)resumeAllMediaPlayback;

#if !TARGET_OS_IPHONE
@property (nonatomic, setter=_setAllowsLinkPreview:) BOOL _allowsLinkPreview;
#endif

// Add visited links
- (void)addVisitedLinks:(NSArray *)visitedLinks;
#if TARGET_OS_IPHONE
- (void)removeVisitedLink:(NSURL *)url;
#endif
@end

@interface WebView (WebPrivate)

+ (void)_setIconLoadingEnabled:(BOOL)enabled;
+ (BOOL)_isIconLoadingEnabled;

@property (nonatomic, assign, setter=_setUseDarkAppearance:) BOOL _useDarkAppearance;
@property (nonatomic, assign, setter=_setUseElevatedUserInterfaceLevel:) BOOL _useElevatedUserInterfaceLevel;

- (void)_setUseDarkAppearance:(BOOL)useDarkAppearance useInactiveAppearance:(BOOL)useInactiveAppearance;
- (void)_setUseDarkAppearance:(BOOL)useDarkAppearance useElevatedUserInterfaceLevel:(BOOL)useElevatedUserInterfaceLevel;

- (WebInspector *)inspector;

#if ENABLE_REMOTE_INSPECTOR
+ (void)_enableRemoteInspector;
+ (void)_disableRemoteInspector;
+ (void)_disableAutoStartRemoteInspector;
+ (BOOL)_isRemoteInspectorEnabled;
+ (BOOL)_hasRemoteInspectorSession;

/*!
    @method allowsRemoteInspection
    @result Returns whether or not this WebView will allow a Remote Web Inspector
    to attach to it.
*/
- (BOOL)allowsRemoteInspection;

/*!
    @method setAllowsRemoteInspection:
    @param allow The new permission for this WebView.
    @abstract Sets the permission of this WebView to either allow or disallow
    a Remote Web Inspector to attach to it.
*/
- (void)setAllowsRemoteInspection:(BOOL)allow;

/*!
    @method setShowingInspectorIndication
    @param enabled Show the indication when true, hide when false.
    @abstract indicate this WebView on screen for a remote inspector.
*/
- (void)setShowingInspectorIndication:(BOOL)enabled;

#if TARGET_OS_IPHONE
- (void)_setHostApplicationProcessIdentifier:(pid_t)pid auditToken:(audit_token_t)auditToken;
#endif

#endif // ENABLE_REMOTE_INSPECTOR

#if !TARGET_OS_IPHONE
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
#else
- (void)setBackgroundColor:(CGColorRef)backgroundColor;
- (CGColorRef)backgroundColor;
#endif

/*!
Could be worth adding to the API.
 @method _loadBackForwardListFromOtherView:
 @abstract Loads the view with the contents of the other view, including its backforward list.
 @param otherView   The WebView from which to copy contents.
 */
- (void)_loadBackForwardListFromOtherView:(WebView *)otherView;

/*
 @method _reportException:inContext:
 @abstract Logs the exception to the Web Inspector. This only needs called for exceptions that
 occur while using the JavaScriptCore APIs with a context owned by a WebKit.
 @param exception The exception value to log.
 @param context   The context the exception occured in.
*/
+ (void)_reportException:(JSValueRef)exception inContext:(JSContextRef)context;

/*!
 @method _dispatchPendingLoadRequests:
 @abstract Dispatches any pending load requests that have been scheduled because of recent DOM additions or style changes.
 @discussion You only need to call this method if you require synchronous notification of loads through the resource load delegate.
 Otherwise the resource load delegate will be notified about loads during a future run loop iteration.
 */
- (void)_dispatchPendingLoadRequests;

#if !TARGET_OS_IPHONE
+ (NSArray *)_supportedFileExtensions;
#endif

/*!
    @method canShowFile:
    @abstract Checks if the WebKit can show the content of the file at the specified path.
    @param path The path of the file to check
    @result YES if the WebKit can show the content of the file at the specified path.
*/
+ (BOOL)canShowFile:(NSString *)path;

#if !TARGET_OS_IPHONE
/*!
    @method suggestedFileExtensionForMIMEType:
    @param MIMEType The MIME type to check.
    @result The extension based on the MIME type
*/
+ (NSString *)suggestedFileExtensionForMIMEType: (NSString *)MIMEType;
#endif

+ (NSString *)_standardUserAgentWithApplicationName:(NSString *)applicationName;
#if TARGET_OS_IPHONE
- (void)_setBrowserUserAgentProductVersion:(NSString *)productVersion buildVersion:(NSString *)buildVersion bundleVersion:(NSString *)bundleVersion;
- (void)_setUIWebViewUserAgentWithBuildVersion:(NSString *)buildVersion;
#endif

/*!
    @method canCloseAllWebViews
    @abstract Checks if all the open WebViews can be closed (by dispatching the beforeUnload event to the pages).
    @result YES if all the WebViews can be closed.
*/
+ (BOOL)canCloseAllWebViews;

#if TARGET_OS_IPHONE
- (id)initSimpleHTMLDocumentWithStyle:(NSString *)style frame:(CGRect)frame preferences:(WebPreferences *)preferences groupName:(NSString *)groupName;
- (id)_formDelegateForwarder;
- (id)_formDelegateForSelector:(SEL)selector;
- (id)_webMailDelegate;
- (void)setWebMailDelegate:(id)delegate;
- (id <WebCaretChangeListener>)caretChangeListener;
- (void)setCaretChangeListener:(id <WebCaretChangeListener>)listener;

- (NSSet *)caretChangeListeners;
- (void)addCaretChangeListener:(id <WebCaretChangeListener>)listener;
- (void)removeCaretChangeListener:(id <WebCaretChangeListener>)listener;
- (void)removeAllCaretChangeListeners;

- (void)caretChanged;

- (void)_dispatchUnloadEvent;

- (DOMCSSStyleDeclaration *)styleAtSelectionStart;

- (NSUInteger)_renderTreeSize;

- (void)_setResourceLoadSchedulerSuspended:(BOOL)suspend;
+ (void)_setTileCacheLayerPoolCapacity:(unsigned)capacity;

+ (void)_releaseMemoryNow;

- (void)_replaceCurrentHistoryItem:(WebHistoryItem *)item;

#if TARGET_OS_IOS
- (BOOL)_requestStartDataInteraction:(CGPoint)clientPosition globalPosition:(CGPoint)globalPosition;
- (WebUITextIndicatorData *)_getDataInteractionData;
@property (nonatomic, readonly, strong, getter=_dataOperationTextIndicator) WebUITextIndicatorData *dataOperationTextIndicator;
@property (nonatomic, readonly) NSUInteger _dragSourceAction;
@property (nonatomic, strong, readonly) NSString *_draggedLinkTitle;
@property (nonatomic, strong, readonly) NSURL *_draggedLinkURL;
@property (nonatomic, readonly) CGRect _draggedElementBounds;
- (uint64_t)_enteredDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation;
- (uint64_t)_updatedDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation;
- (void)_exitedDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation;
- (void)_performDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation;
- (BOOL)_tryToPerformDataInteraction:(id <UIDropSession>)session client:(CGPoint)clientPosition global:(CGPoint)globalPosition operation:(uint64_t)operation;
- (void)_endedDataInteraction:(CGPoint)clientPosition global:(CGPoint)globalPosition;

@property (nonatomic, readonly, getter=_dataInteractionCaretRect) CGRect dataInteractionCaretRect;
#endif

// Deprecated. Use -[WebDataSource _quickLookContent] instead.
- (NSDictionary *)quickLookContentForURL:(NSURL *)url;
#endif

// May well become public
- (void)_setFormDelegate:(id<WebFormDelegate>)delegate;
- (id<WebFormDelegate>)_formDelegate;

- (BOOL)_isClosed;

// _close is now replaced by public method -close. It remains here only for backward compatibility
// until callers can be weaned off of it.
- (void)_close;

// Indicates if the WebView is in the midst of a user gesture.
- (BOOL)_isProcessingUserGesture;

// Determining and updating page visibility state.
- (BOOL)_isViewVisible;
- (void)_updateVisibilityState;

// SPI for DumpRenderTree
- (void)_updateActiveState;

- (void)_didScrollDocumentInFrameView:(WebFrameView *)frameView;

/*!
    @method _registerViewClass:representationClass:forURLScheme:
    @discussion Register classes that implement WebDocumentView and WebDocumentRepresentation respectively.
    @param viewClass The WebDocumentView class to use to render data for a given MIME type.
    @param representationClass The WebDocumentRepresentation class to use to represent data of the given MIME type.
    @param URLScheme The URL scheme to represent with an object of the given class.
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

+ (void)_setAlwaysUsesComplexTextCodePath:(BOOL)f;

#if !TARGET_OS_IPHONE
- (NSCachedURLResponse *)_cachedResponseForURL:(NSURL *)URL;
#endif

#if ENABLE_DASHBOARD_SUPPORT
// FIXME: Remove these once we have verified no one is calling them
- (void)_addScrollerDashboardRegions:(NSMutableDictionary *)regions;
- (NSDictionary *)_dashboardRegions;

- (void)_setDashboardBehavior:(WebDashboardBehavior)behavior to:(BOOL)flag;
- (BOOL)_dashboardBehavior:(WebDashboardBehavior)behavior;
#endif

#if !TARGET_OS_IPHONE
// These two methods are useful for a test harness that needs a consistent appearance for the focus rings
// regardless of OS X version.
+ (void)_setUsesTestModeFocusRingColor:(BOOL)f;
+ (BOOL)_usesTestModeFocusRingColor;
#endif

#if TARGET_OS_IPHONE
- (void)_setUIKitDelegate:(id)delegate;
- (id)_UIKitDelegate;
- (void)_clearDelegates;

- (NSURL *)_displayURL;

+ (NSArray *)_productivityDocumentMIMETypes;

- (void)_setAllowsMessaging:(BOOL)aFlag;
- (BOOL)_allowsMessaging;

- (void)_setCustomFixedPositionLayoutRectInWebThread:(CGRect)rect synchronize:(BOOL)synchronize;
- (void)_setCustomFixedPositionLayoutRect:(CGRect)rect;

- (WebFixedPositionContent*)_fixedPositionContent;

- (void)_viewGeometryDidChange;
- (void)_overflowScrollPositionChangedTo:(CGPoint)offset forNode:(DOMNode *)node isUserScroll:(BOOL)userScroll;

#if ENABLE_TOUCH_EVENTS
- (NSArray *)_touchEventRegions;
#endif

/*!
    @method _doNotStartObservingNetworkReachability
    @abstract Does not start observation of network reachability in any WebView.
    @discussion To take effect, this method must be called before the first WebView is created.
 */
+ (void)_doNotStartObservingNetworkReachability;
#endif

#if !TARGET_OS_IPHONE
/*!
    @method setAlwaysShowVerticalScroller:
    @abstract Forces the vertical scroller to be visible if flag is YES, otherwise
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
    @abstract Forces the horizontal scroller to be visible if flag is YES, otherwise
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
#endif /* !TARGET_OS_IPHONE */

#if TARGET_OS_IPHONE
/*!
    @method _pluginsAreRunning
    @result Returns YES if any plug-ins in the WebView are running.
 */
- (BOOL)_pluginsAreRunning;
/*!
    @method _destroyAllPlugIns
    @abstract Destroys all plug-ins in all of the WebView's frames.
 */
- (void)_destroyAllPlugIns;
/*!
    @method _startAllPlugIns
    @abstract Starts all plug-ins in all of the WebView's frames.
 */
- (void)_startAllPlugIns;
/*!
    @method _stopAllPlugIns
    @abstract Stops all plug-ins in all of the WebView's frames.
 */
- (void)_stopAllPlugIns;
/*!
    @method _stopAllPlugInsForPageCache
    @abstract Stops all plug-ins in all of the WebView's frames.
    Called when the page is entering the PageCache and lets the
    plug-in know this by sending -webPlugInStopForPageCache.
*/
- (void)_stopAllPlugInsForPageCache;
/*!
    @method _restorePlugInsFromCache
    @abstract Reconnects plug-ins from all of the WebView's frames to the
    WebView and performs any other necessary reinitialization.
 */
- (void)_restorePlugInsFromCache;

/*!
    @method _setMediaLayer:forPluginView:
    @abstract Set the layer that renders plug-in content for the given pluginView.
    If layer is NULL, removes any existing layer. Returns YES if the set or
    remove was successful.
 */
#if TARGET_OS_IPHONE
- (BOOL)_setMediaLayer:(CALayer*)layer forPluginView:(WAKView*)pluginView;
#else
- (BOOL)_setMediaLayer:(CALayer*)layer forPluginView:(NSView*)pluginView;
#endif

/*!
 @method _wantsTelephoneNumberParsing
 @abstract Does this WebView want phone number parsing? (This could ultimately be disallowed by the document itself).
 */

- (BOOL)_wantsTelephoneNumberParsing;

/*!
 @method _setWantsTelephoneNumberParsing
 @abstract Explicitly disable WebKit phone number parsing on this WebView, or say that you want it enabled if possible.
 */

- (void)_setWantsTelephoneNumberParsing:(BOOL)flag;

/*!
    @method _setNeedsUnrestrictedGetMatchedCSSRules
    @abstract Explicitly enables/disables cross origin CSS rules matching.
 */
- (void)_setNeedsUnrestrictedGetMatchedCSSRules:(BOOL)flag;
#endif /* TARGET_OS_IPHONE */

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

- (BOOL)usesPageCache;
- (void)setUsesPageCache:(BOOL)usesPageCache;

/*!
    @method textIteratorForRect:
    @param rect The rectangle of the document that we're interested in text from.
    @result WebTextIterator object, initialized with a range that corresponds to
    the passed-in rectangle.
    @abstract This method gives the text for the approximate range of the document
    corresponding to the rectangle. The range is determined by using hit testing at
    the top left and bottom right of the rectangle. Because of that, there can be
    text visible in the rectangle that is not included in the iterator. If you need
    a guarantee of iterating all text that is visible, then you need to instead make
    a WebTextIterator with a DOMRange that covers the entire document.
 */
- (WebTextIterator *)textIteratorForRect:(NSRect)rect;

#if !TARGET_OS_IPHONE
- (void)_clearUndoRedoOperations;
#endif

/* Used to do fast (lower quality) scaling of images so that window resize can be quick. */
- (BOOL)_inFastImageScalingMode;
- (void)_setUseFastImageScalingMode:(BOOL)flag;

- (BOOL)_cookieEnabled;
- (void)_setCookieEnabled:(BOOL)enable;

// SPI for DumpRenderTree
- (void)_executeCoreCommandByName:(NSString *)name value:(NSString *)value;
- (void)_clearMainFrameName;

- (void)setSelectTrailingWhitespaceEnabled:(BOOL)flag;
- (BOOL)isSelectTrailingWhitespaceEnabled;

- (void)setMemoryCacheDelegateCallsEnabled:(BOOL)suspend;
- (BOOL)areMemoryCacheDelegateCallsEnabled;

// SPI for DumpRenderTree
- (BOOL)_postsAcceleratedCompositingNotifications;
- (void)_setPostsAcceleratedCompositingNotifications:(BOOL)flag;
- (BOOL)_isUsingAcceleratedCompositing;
- (void)_setBaseCTM:(CGAffineTransform)transform forContext:(CGContextRef)context;

// For DumpRenderTree
- (BOOL)interactiveFormValidationEnabled;
- (void)setInteractiveFormValidationEnabled:(BOOL)enabled;
- (int)validationMessageTimerMagnification;
- (void)setValidationMessageTimerMagnification:(int)newValue;
- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem;

// Returns YES if NSView -displayRectIgnoringOpacity:inContext: will produce a faithful representation of the content.
- (BOOL)_isSoftwareRenderable;

- (void)setTracksRepaints:(BOOL)flag;
- (BOOL)isTrackingRepaints;
- (void)resetTrackedRepaints;
- (NSArray*)trackedRepaintRects; // Returned array contains rectValue NSValues.

#if !TARGET_OS_IPHONE
// Which pasteboard text is coming from in editing delegate methods such as shouldInsertNode.
- (NSPasteboard *)_insertionPasteboard;
#endif

// Allow lists access from an origin (sourceOrigin) to a set of one or more origins described by the parameters:
// - destinationProtocol: The protocol to grant access to.
// - destinationHost: The host to grant access to.
// - allowDestinationSubdomains: If host is a domain, setting this to YES will allow host and all its subdomains, recursively.
+ (void)_addOriginAccessAllowListEntryWithSourceOrigin:(NSString *)sourceOrigin destinationProtocol:(NSString *)destinationProtocol destinationHost:(NSString *)destinationHost allowDestinationSubdomains:(BOOL)allowDestinationSubdomains;
+ (void)_removeOriginAccessAllowListEntryWithSourceOrigin:(NSString *)sourceOrigin destinationProtocol:(NSString *)destinationProtocol destinationHost:(NSString *)destinationHost allowDestinationSubdomains:(BOOL)allowDestinationSubdomains;

// Removes all allow list entries created with _addOriginAccessAllowListEntryWithSourceOrigin.
+ (void)_resetOriginAccessAllowLists;

+ (void)_addUserScriptToGroup:(NSString *)groupName world:(WebScriptWorld *)world source:(NSString *)source url:(NSURL *)url includeMatchPatternStrings:(NSArray *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray *)excludeMatchPatternStrings injectionTime:(WebUserScriptInjectionTime)injectionTime injectedFrames:(WebUserContentInjectedFrames)injectedFrames;
+ (void)_addUserStyleSheetToGroup:(NSString *)groupName world:(WebScriptWorld *)world source:(NSString *)source url:(NSURL *)url includeMatchPatternStrings:(NSArray *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray *)excludeMatchPatternStrings injectedFrames:(WebUserContentInjectedFrames)injectedFrames;

+ (void)_removeUserScriptFromGroup:(NSString *)groupName world:(WebScriptWorld *)world url:(NSURL *)url;
+ (void)_removeUserStyleSheetFromGroup:(NSString *)groupName world:(WebScriptWorld *)world url:(NSURL *)url;
+ (void)_removeUserScriptsFromGroup:(NSString *)groupName world:(WebScriptWorld *)world;
+ (void)_removeUserStyleSheetsFromGroup:(NSString *)groupName world:(WebScriptWorld *)world;
+ (void)_removeAllUserContentFromGroup:(NSString *)groupName;

// SPI for DumpRenderTree
+ (void)_setLoadResourcesSerially:(BOOL)serialize;
- (void)_forceRepaintForTesting;

+ (void)_setDomainRelaxationForbidden:(BOOL)forbidden forURLScheme:(NSString *)scheme;
+ (void)_registerURLSchemeAsSecure:(NSString *)scheme;
+ (void)_registerURLSchemeAsAllowingLocalStorageAccessInPrivateBrowsing:(NSString *)scheme;
+ (void)_registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsing:(NSString *)scheme;

- (void)_scaleWebView:(float)scale atOrigin:(NSPoint)origin;
- (float)_viewScaleFactor; // This is actually pageScaleFactor.

- (void)_setUseFixedLayout:(BOOL)fixed;
- (void)_setFixedLayoutSize:(NSSize)size;

- (BOOL)_useFixedLayout;
- (NSSize)_fixedLayoutSize;

- (void)_setPaginationMode:(WebPaginationMode)paginationMode;
- (WebPaginationMode)_paginationMode;

- (void)_listenForLayoutMilestones:(WebLayoutMilestones)layoutMilestones;
- (WebLayoutMilestones)_layoutMilestones;

- (WebPageVisibilityState)_visibilityState;
- (void)_setVisibilityState:(WebPageVisibilityState)visibilityState isInitialState:(BOOL)isInitialState;

#if !TARGET_OS_IPHONE
- (BOOL)windowOcclusionDetectionEnabled;
- (void)setWindowOcclusionDetectionEnabled:(BOOL)flag;
#endif

// Whether the column-break-{before,after} properties are respected instead of the
// page-break-{before,after} properties.
- (void)_setPaginationBehavesLikeColumns:(BOOL)behavesLikeColumns;
- (BOOL)_paginationBehavesLikeColumns;

// Set to 0 to have the page length equal the view length.
- (void)_setPageLength:(CGFloat)pageLength;
- (CGFloat)_pageLength;
- (void)_setGapBetweenPages:(CGFloat)pageGap;
- (CGFloat)_gapBetweenPages;
- (NSUInteger)_pageCount;

// Whether or not a line grid is enabled by default when paginated via the pagination API.
- (void)_setPaginationLineGridEnabled:(BOOL)lineGridEnabled;
- (BOOL)_paginationLineGridEnabled;

#if !TARGET_OS_IPHONE
- (void)_setCustomBackingScaleFactor:(CGFloat)overrideScaleFactor;
- (CGFloat)_backingScaleFactor;
#endif

// Deprecated. Use the methods in pending public above instead.
- (NSUInteger)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(NSUInteger)limit;
- (NSUInteger)countMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight limit:(NSUInteger)limit markMatches:(BOOL)markMatches;

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
// Deprecated. Use findString.
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection;

/*!
    @method _HTTPPipeliningEnabled
    @abstract Checks the HTTP pipelining status.
    @discussion Defaults to NO.
    @result YES if HTTP pipelining is enabled, NO if not enabled.
 */
+ (BOOL)_HTTPPipeliningEnabled;

/*!
    @method _setHTTPPipeliningEnabled:
    @abstract Set the HTTP pipelining status.
    @discussion Defaults to NO.
    @param enabled The new HTTP pipelining status.
 */
+ (void)_setHTTPPipeliningEnabled:(BOOL)enabled;

@property (nonatomic, copy, getter=_sourceApplicationAuditData, setter=_setSourceApplicationAuditData:) NSData *sourceApplicationAuditData;

- (void)_setFontFallbackPrefersPictographs:(BOOL)flag;

#if TARGET_OS_IPHONE
- (void)showCandidates:(NSArray *)candidates forString:(NSString *)string inRect:(NSRect)rectOfTypedString forSelectedRange:(NSRange)range view:(WAKView *)view completionHandler:(void (^)(NSTextCheckingResult *acceptedCandidate))completionBlock;
#else
- (void)showCandidates:(NSArray *)candidates forString:(NSString *)string inRect:(NSRect)rectOfTypedString forSelectedRange:(NSRange)range view:(NSView *)view completionHandler:(void (^)(NSTextCheckingResult *acceptedCandidate))completionBlock;
#endif
- (void)forceRequestCandidatesForTesting;
- (BOOL)shouldRequestCandidates;

typedef struct WebEdgeInsets {
    CGFloat top;
    CGFloat left;
    CGFloat bottom;
    CGFloat right;
} WebEdgeInsets;

@property (nonatomic, assign, setter=_setUnobscuredSafeAreaInsets:) WebEdgeInsets _unobscuredSafeAreaInsets;

@property (nonatomic, assign, setter=_setUseSystemAppearance:) BOOL _useSystemAppearance;

@end

#if !TARGET_OS_IPHONE
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

- (BOOL)isGrammarCheckingEnabled;
- (void)setGrammarCheckingEnabled:(BOOL)flag;

- (void)toggleGrammarChecking:(id)sender;

@end

@interface WebView (WebViewTextChecking)

- (BOOL)isAutomaticQuoteSubstitutionEnabled;
- (BOOL)isAutomaticLinkDetectionEnabled;
- (BOOL)isAutomaticDashSubstitutionEnabled;
- (BOOL)isAutomaticTextReplacementEnabled;
- (BOOL)isAutomaticSpellingCorrectionEnabled;
- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)flag;
- (void)toggleAutomaticQuoteSubstitution:(id)sender;
- (void)setAutomaticLinkDetectionEnabled:(BOOL)flag;
- (void)toggleAutomaticLinkDetection:(id)sender;
- (void)setAutomaticDashSubstitutionEnabled:(BOOL)flag;
- (void)toggleAutomaticDashSubstitution:(id)sender;
- (void)setAutomaticTextReplacementEnabled:(BOOL)flag;
- (void)toggleAutomaticTextReplacement:(id)sender;
- (void)setAutomaticSpellingCorrectionEnabled:(BOOL)flag;
- (void)toggleAutomaticSpellingCorrection:(id)sender;
@end
#endif /* !TARGET_OS_IPHONE */

@interface WebView (WebViewEditingInMail)
- (void)_insertNewlineInQuotedContent;
- (void)_replaceSelectionWithNode:(DOMNode *)node matchStyle:(BOOL)matchStyle;
- (BOOL)_selectionIsCaret;
- (BOOL)_selectionIsAll;
- (void)_simplifyMarkup:(DOMNode *)startNode endNode:(DOMNode *)endNode;

@end

@interface WebView (WebViewDeviceOrientation)
- (void)_setDeviceOrientationProvider:(id<WebDeviceOrientationProvider>)deviceOrientationProvider;
- (id<WebDeviceOrientationProvider>)_deviceOrientationProvider;
@end

#if TARGET_OS_IPHONE
@protocol WebGeolocationProvider;

@protocol WebGeolocationProviderInitializationListener <NSObject>
- (void)initializationAllowedWebView:(WebView *)webView;
- (void)initializationDeniedWebView:(WebView *)webView;
@end

#endif

@protocol WebGeolocationProvider <NSObject>
- (void)registerWebView:(WebView *)webView;
- (void)unregisterWebView:(WebView *)webView;
- (WebGeolocationPosition *)lastPosition;
#if TARGET_OS_IPHONE
- (void)setEnableHighAccuracy:(BOOL)enableHighAccuracy;
- (void)initializeGeolocationForWebView:(WebView *)webView listener:(id<WebGeolocationProviderInitializationListener>)listener;
- (void)stopTrackingWebView:(WebView *)webView;
#endif
@end

@protocol WebNotificationProvider
- (void)registerWebView:(WebView *)webView;
- (void)unregisterWebView:(WebView *)webView;

- (void)showNotification:(WebNotification *)notification fromWebView:(WebView *)webView;
- (void)cancelNotification:(WebNotification *)notification;
- (void)notificationDestroyed:(WebNotification *)notification;
- (void)clearNotifications:(NSArray *)notificationIDs;
- (WebNotificationPermission)policyForOrigin:(WebSecurityOrigin *)origin;

- (void)webView:(WebView *)webView didShowNotification:(NSString *)notificationID;
- (void)webView:(WebView *)webView didClickNotification:(NSString *)notificationID;
- (void)webView:(WebView *)webView didCloseNotifications:(NSArray *)notificationIDs;
@end

@interface WebView (WebViewGeolocation)
- (void)_setGeolocationProvider:(id<WebGeolocationProvider>)locationProvider;
- (id<WebGeolocationProvider>)_geolocationProvider;

- (void)_geolocationDidChangePosition:(WebGeolocationPosition *)position;
- (void)_geolocationDidFailWithMessage:(NSString *)errorMessage;
#if TARGET_OS_IPHONE
- (void)_resetAllGeolocationPermission;
#endif
@end

@interface WebView (WebViewNotification)
- (void)_setNotificationProvider:(id<WebNotificationProvider>)notificationProvider;
- (id<WebNotificationProvider>)_notificationProvider;

- (void)_notificationDidShow:(NSString *)notificationID;
- (void)_notificationDidClick:(NSString *)notificationID;
- (void)_notificationsDidClose:(NSArray *)notificationIDs;

- (NSString *)_notificationIDForTesting:(JSValueRef)jsNotification;
- (void)_clearNotificationPermissionState;
@end

@interface WebView (WebViewFontSelection)
+ (void)_setFontAllowList:(NSArray *)allowList;
@end

#if TARGET_OS_IPHONE

@interface WebView (WebViewIOSPDF)
+ (Class)_getPDFRepresentationClass;
+ (void)_setPDFRepresentationClass:(Class)pdfRepresentationClass;

+ (Class)_getPDFViewClass;
+ (void)_setPDFViewClass:(Class)pdfViewClass;
@end

@interface WebView (WebViewIOSAdditions)
- (NSArray<DOMElement *> *)_editableElementsInRect:(CGRect)rect;
- (void)revealCurrentSelection;

// View must be a UIView.
- (void)_installVisualIdentificationOverlayForViewIfNeeded:(id)view kind:(NSString *)kind;
@end

#endif

@interface NSObject (WebViewFrameLoadDelegatePrivate)
- (void)webView:(WebView *)sender didFirstLayoutInFrame:(WebFrame *)frame;

// didFinishDocumentLoadForFrame is sent when the document has finished loading, though not necessarily all
// of its subresources.
// FIXME 5259339: Currently this callback is not sent for (some?) pages loaded entirely from the cache.
- (void)webView:(WebView *)sender didFinishDocumentLoadForFrame:(WebFrame *)frame;

// Addresses 4192534.  SPI for now.
- (void)webView:(WebView *)sender didHandleOnloadEventsForFrame:(WebFrame *)frame;

- (void)webView:(WebView *)sender didFirstVisuallyNonEmptyLayoutInFrame:(WebFrame *)frame;

- (void)webView:(WebView *)sender didLayout:(WebLayoutMilestones)milestones;

#if TARGET_OS_IPHONE
- (void)webThreadWebView:(WebView *)sender didLayout:(WebLayoutMilestones)milestones;
#endif

// For implementing the WebInspector's test harness
- (void)webView:(WebView *)webView didClearInspectorWindowObject:(WebScriptObject *)windowObject forFrame:(WebFrame *)frame;

@end

@interface NSObject (WebViewResourceLoadDelegatePrivate)
// Addresses <rdar://problem/5008925> - SPI for now
- (NSCachedURLResponse *)webView:(WebView *)sender resource:(id)identifier willCacheResponse:(NSCachedURLResponse *)response fromDataSource:(WebDataSource *)dataSource;
@end

#ifdef __cplusplus
extern "C" {
#endif

// This is a C function to avoid calling +[WebView initialize].
void WebInstallMemoryPressureHandler(void);

#ifdef __cplusplus
}
#endif
