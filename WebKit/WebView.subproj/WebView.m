/*	
    WebView.m
    Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSUserDefaults_NSURLExtras.h>
#import <Foundation/NSURLConnection.h>

#import <Foundation/NSData_NSURLExtras.h>
#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURLDownloadPrivate.h>
#import <Foundation/NSURLFileTypeMappings.h>
#import <WebCore/WebCoreEncodings.h>
#import <WebCore/WebCoreSettings.h>
#import <WebKit/WebDefaultFrameLoadDelegate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebFormDelegatePrivate.h>

static const struct UserAgentSpoofTableEntry *_web_findSpoofTableEntry(const char *, unsigned);

// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#include "WebUserAgentSpoofTable.c"
#undef __inline

NSString *WebElementFrameKey = 			@"WebElementFrame";
NSString *WebElementImageKey = 			@"WebElementImage";
NSString *WebElementImageAltStringKey = 	@"WebElementImageAltString";
NSString *WebElementImageRectKey = 		@"WebElementImageRect";
NSString *WebElementImageURLKey = 		@"WebElementImageURL";
NSString *WebElementIsSelectedKey = 	        @"WebElementIsSelected";
NSString *WebElementLinkURLKey = 		@"WebElementLinkURL";
NSString *WebElementLinkTargetFrameKey =	@"WebElementTargetFrame";
NSString *WebElementLinkLabelKey = 		@"WebElementLinkLabel";
NSString *WebElementLinkTitleKey = 		@"WebElementLinkTitle";

NSString *WebViewProgressStartedNotification = @"WebProgressStartedNotification";
NSString *WebViewProgressEstimateChangedNotification = @"WebProgressEstimateChangedNotification";
NSString *WebViewProgressFinishedNotification = @"WebProgressFinishedNotification";

enum { WebViewVersion = 2 };


static NSMutableSet *schemesWithRepresentationsSet;

NSString *_WebCanGoBackKey = @"canGoBack";
NSString *_WebCanGoForwardKey = @"canGoForward";
NSString *_WebEstimatedProgressKey = @"estimatedProgress";
NSString *_WebIsLoadingKey = @"isLoading";
NSString *_WebMainFrameIconKey = @"mainFrameIcon";
NSString *_WebMainFrameTitleKey = @"mainFrameTitle";
NSString *_WebMainFrameURLKey = @"mainFrameURL";

@interface WebProgressItem : NSObject
{
@public
    long long bytesReceived;
    long long estimatedLength;
}
@end

@implementation WebProgressItem
@end

@implementation WebViewPrivate

- init 
{
    backForwardList = [[WebBackForwardList alloc] init];
    textSizeMultiplier = 1;
    progressNotificationInterval = 0.02;
    progressNotificationTimeInterval = 0.1;
    settings = [[WebCoreSettings alloc] init];

    return self;
}

- (void)dealloc
{
    ASSERT(!mainFrame);
    
    [backForwardList release];
    [applicationNameForUserAgent release];
    [userAgentOverride release];
    int i;
    for (i = 0; i != NumUserAgentStringTypes; ++i) {
        [userAgent[i] release];
    }
    
    [preferences release];
    [settings release];
    [hostWindow release];
    
    [policyDelegateForwarder release];
    [resourceProgressDelegateForwarder release];
    [UIDelegateForwarder release];
    [frameLoadDelegateForwarder release];
    
    [progressItems release];
    
    [super dealloc];
}

@end

@implementation WebView (WebPrivate)

+ (void)_setAlwaysUseATSU:(BOOL)f
{
    [WebTextRenderer _setAlwaysUseATSU:f];
}


+ (BOOL)canShowFile:(NSString *)path
{
    NSString *MIMEType;

    MIMEType = [WebView _MIMETypeForFile:path];
    return [[self class] canShowMIMEType:MIMEType];
}

+ (NSString *)suggestedFileExtensionForMIMEType: (NSString *)type
{
    return [[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:type];
}

- (void)_close
{
    if (_private->setName != nil) {
        [WebViewSets removeWebView:self fromSetNamed:_private->setName];
        [_private->setName release];
        _private->setName = nil;
    }

    [_private->mainFrame _detachFromParent];
    [_private->mainFrame release];
    _private->mainFrame = nil;
    
    // Clear the page cache so we call destroy on all the plug-ins in the page cache to break any retain cycles.
    // See comment in [WebHistoryItem _releaseAllPendingPageCaches] for more information.
    [_private->backForwardList _clearPageCache];
}

- (WebFrame *)_createFrameNamed:(NSString *)fname inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling
{
    WebFrameView *childView = [[WebFrameView alloc] initWithFrame:NSMakeRect(0,0,0,0)];

    [childView _setWebView:self];
    [childView setAllowsScrolling:allowsScrolling];
    
    WebFrame *newFrame = [[WebFrame alloc] initWithName:fname webFrameView:childView webView:self];

    [childView release];

    [parent _addChild:newFrame];
    
    [newFrame release];
        
    return newFrame;
}

- (void)_finishedLoadingResourceFromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];
    
    ASSERT(dataSource != nil);
    
    // This resource has completed, so check if the load is complete for all frames.
    if (frame != nil) {
        [frame _transitionToLayoutAcceptable];
        [frame _checkLoadComplete];
    }
}

- (void)_mainReceivedBytesSoFar: (unsigned)bytesSoFar fromDataSource: (WebDataSource *)dataSource complete: (BOOL)isComplete
{
    WebFrame *frame = [dataSource webFrame];
    
    ASSERT(dataSource != nil);

    // The frame may be nil if a previously cancelled load is still making progress callbacks.
    if (frame == nil)
        return;
        
    // This resource has completed, so check if the load is complete for all frames.
    if (isComplete){
        // If the load is complete, mark the primary load as done.  The primary load is the load
        // of the main document.  Other resources may still be arriving.
        [dataSource _setPrimaryLoadComplete: YES];
        [frame _checkLoadComplete];
    }
    else {
        // If the frame isn't complete it might be ready for a layout.  Perform that check here.
        // Note that transitioning a frame to this state doesn't guarantee a layout, rather it
        // just indicates that an early layout can be performed.
        int timedLayoutSize = [[WebPreferences standardPreferences] _initialTimedLayoutSize];
        if ((int)bytesSoFar > timedLayoutSize)
            [frame _transitionToLayoutAcceptable];
    }
}


- (void)_receivedError: (NSError *)error fromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];

    [frame _checkLoadComplete];
}


- (void)_mainReceivedError:(NSError *)error fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete
{
    ASSERT(dataSource);
#ifndef NDEBUG
    ASSERT([dataSource webFrame]);
#endif
    
    [dataSource _setMainDocumentError: error];

    if (isComplete) {
        [dataSource _setPrimaryLoadComplete:YES];
        [[dataSource webFrame] _checkLoadComplete];
    }
}

+ (NSString *)_MIMETypeForFile:(NSString *)path
{
    NSString *extension = [path pathExtension];
    NSString *MIMEType = nil;

    // Get the MIME type from the extension.
    if ([extension length] != 0) {
        MIMEType = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
    }

    // If we can't get a known MIME type from the extension, sniff.
    if ([MIMEType length] == 0 || [MIMEType isEqualToString:@"application/octet-stream"]) {
        NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:path];
        NSData *data = [handle readDataOfLength:GUESS_MIME_TYPE_PEEK_LENGTH];
        [handle closeFile];
        if ([data length] != 0) {
            MIMEType = [data _web_guessedMIMEType];
        }
        if ([MIMEType length] == 0){
            MIMEType = @"application/octet-stream";
        }
    }

    return MIMEType;
}

- (void)_downloadURL:(NSURL *)URL
{
    [self _downloadURL:URL toDirectory:nil];
}

- (void)_downloadURL:(NSURL *)URL toDirectory:(NSString *)directory
{
    ASSERT(URL);
    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
    [WebDownload _downloadWithRequest:request
                             delegate:_private->downloadDelegate
                            directory:[directory isAbsolutePath] ? directory : nil];
    [request release];
}

- (BOOL)defersCallbacks
{
    return _private->defersCallbacks;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    if (defers == _private->defersCallbacks) {
        return;
    }

    _private->defersCallbacks = defers;
    [_private->mainFrame _defersCallbacksChanged];
}

- (void)_setTopLevelFrameName:(NSString *)name
{
    [[self mainFrame] _setName:name];
}

- (WebFrame *)_findFrameInThisWindowNamed: (NSString *)name
{
    return [[self mainFrame] _descendantFrameNamed:name];
}

- (WebFrame *)_findFrameNamed:(NSString *)name
{
    // Try this WebView first.
    WebFrame *frame = [self _findFrameInThisWindowNamed:name];

    if (frame != nil) {
        return frame;
    }

    // Try other WebViews in the same set
    if (_private->setName != nil) {
        NSEnumerator *enumerator = [WebViewSets webViewsInSetNamed:_private->setName];
        WebView *webView;
        while ((webView = [enumerator nextObject]) != nil && frame == nil) {
	    frame = [webView _findFrameInThisWindowNamed:name];
        }
    }

    return frame;
}

- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request
{
    id wd = [self UIDelegate];
    WebView *newWindowWebView = nil;
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWindowWebView = [wd webView:self createWebViewWithRequest:request];
    else {
        newWindowWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:self createWebViewWithRequest: request];
    }

    [[newWindowWebView _UIDelegateForwarder] webViewShow: newWindowWebView];

    return newWindowWebView;
}

- (NSMenu *)_menuForElement:(NSDictionary *)element
{
    NSArray *defaultMenuItems = [[WebDefaultUIDelegate sharedUIDelegate]
          webView:self contextMenuItemsForElement:element defaultMenuItems:nil];
    NSArray *menuItems = defaultMenuItems;
    NSMenu *menu = nil;
    unsigned i;

    if (_private->UIDelegate) {
        id cd = _private->UIDelegate;
        
        if ([cd respondsToSelector:@selector(webView:contextMenuItemsForElement:defaultMenuItems:)])
            menuItems = [cd webView:self contextMenuItemsForElement:element defaultMenuItems:defaultMenuItems];
    } 

    if (menuItems && [menuItems count] > 0) {
        menu = [[[NSMenu alloc] init] autorelease];

        for (i=0; i<[menuItems count]; i++) {
            [menu addItem:[menuItems objectAtIndex:i]];
        }
    }

    return menu;
}

- (void)_mouseDidMoveOverElement:(NSDictionary *)dictionary modifierFlags:(unsigned)modifierFlags
{
    // When the mouse isn't over this view at all, we'll get called with a dictionary of nil over
    // and over again. So it's a good idea to catch that here and not send multiple calls to the delegate
    // for that case.
    
    if (dictionary && _private->lastElementWasNonNil) {
        [[self _UIDelegateForwarder] webView:self mouseDidMoveOverElement:dictionary modifierFlags:modifierFlags];
    }
    _private->lastElementWasNonNil = dictionary != nil;
}

- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)type
{
    // We never go back/forward on a per-frame basis, so the target must be the main frame
    ASSERT([item target] == nil || [self _findFrameNamed:[item target]] == [self mainFrame]);

    // abort any current load if we're going back/forward
    [[self mainFrame] stopLoading];
    [[self mainFrame] _goToItem:item withLoadType:type];
}

// Not used now, but could be if we ever store frames in bookmarks or history
- (void)_loadItem:(WebHistoryItem *)item
{
    WebHistoryItem *newItem = [item copy];	// Makes a deep copy, happily
    [[self backForwardList] addItem:newItem];
    [self _goToItem:newItem withLoadType:WebFrameLoadTypeIndexedBackForward];
}

- (void)_loadBackForwardListFromOtherView:(WebView *)otherView
{
    // It turns out the right combination of behavior is done with the back/forward load
    // type.  (See behavior matrix at the top of WebFramePrivate.)  So we copy all the items
    // in the back forward list, and go to the current one.

    WebBackForwardList *bfList = [self backForwardList];
    ASSERT(![bfList currentItem]);	// destination list should be empty

    WebBackForwardList *otherBFList = [otherView backForwardList];
    if (![otherBFList currentItem]) {
        return;		// empty back forward list, bail
    }

    WebHistoryItem *newItemToGoTo = nil;
    int lastItemIndex = [otherBFList forwardListCount];
    int i;
    for (i = -[otherBFList backListCount]; i <= lastItemIndex; i++) {
        if (i == 0) {
            // If this item is showing , save away its current scroll and form state,
            // since that might have changed since loading and it is normally not saved
            // until we leave that page.
            [[otherView mainFrame] _saveDocumentAndScrollState];
        }
        WebHistoryItem *newItem = [[otherBFList itemAtIndex:i] copy];
        [bfList addItem:newItem];
        if (i == 0) {
            newItemToGoTo = newItem;
        }
    }
    
    [self _goToItem:newItemToGoTo withLoadType:WebFrameLoadTypeIndexedBackForward];
}

- (void)_setFormDelegate: (id<WebFormDelegate>)delegate
{
    _private->formDelegate = delegate;
}

- (id<WebFormDelegate>)_formDelegate
{
    if (!_private->formDelegate) {
        // create lazily, to give the client a chance to set one before we bother to alloc the shared one
        _private->formDelegate = [WebFormDelegate _sharedWebFormDelegate];
    }
    return _private->formDelegate;
}

- (WebCoreSettings *)_settings
{
    return _private->settings;
}

- (void)_updateWebCoreSettingsFromPreferences: (WebPreferences *)preferences
{
    [_private->settings setCursiveFontFamily:[preferences cursiveFontFamily]];
    [_private->settings setDefaultFixedFontSize:[preferences defaultFixedFontSize]];
    [_private->settings setDefaultFontSize:[preferences defaultFontSize]];
    [_private->settings setDefaultTextEncoding:[preferences defaultTextEncodingName]];
    [_private->settings setFantasyFontFamily:[preferences fantasyFontFamily]];
    [_private->settings setFixedFontFamily:[preferences fixedFontFamily]];
    [_private->settings setJavaEnabled:[preferences isJavaEnabled]];
    [_private->settings setJavaScriptEnabled:[preferences isJavaScriptEnabled]];
    [_private->settings setJavaScriptCanOpenWindowsAutomatically:[preferences javaScriptCanOpenWindowsAutomatically]];
    [_private->settings setMinimumFontSize:[preferences minimumFontSize]];
    [_private->settings setPluginsEnabled:[preferences arePlugInsEnabled]];
    [_private->settings setSansSerifFontFamily:[preferences sansSerifFontFamily]];
    [_private->settings setSerifFontFamily:[preferences serifFontFamily]];
    [_private->settings setStandardFontFamily:[preferences standardFontFamily]];
    [_private->settings setWillLoadImagesAutomatically:[preferences loadsImagesAutomatically]];

    if ([preferences userStyleSheetEnabled]) {
        [_private->settings setUserStyleSheetLocation:[[preferences userStyleSheetLocation] _web_originalDataAsString]];
    } else {
        [_private->settings setUserStyleSheetLocation:@""];
    }
    [_private->settings setShouldPrintBackgrounds:[preferences shouldPrintBackgrounds]];
}

- (void)_releaseUserAgentStrings
{
    int i;
    for (i = 0; i != NumUserAgentStringTypes; ++i) {
        [_private->userAgent[i] release];
        _private->userAgent[i] = nil;
    }
}


- (void)_preferencesChangedNotification: (NSNotification *)notification
{
    WebPreferences *preferences = (WebPreferences *)[notification object];
    
    ASSERT(preferences == [self preferences]);
    [self _releaseUserAgentStrings];
    [self _updateWebCoreSettingsFromPreferences: preferences];
}

- _frameLoadDelegateForwarder
{
    if (!_private->frameLoadDelegateForwarder)
        _private->frameLoadDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self frameLoadDelegate]  defaultTarget: [WebDefaultFrameLoadDelegate sharedFrameLoadDelegate] templateClass: [WebDefaultFrameLoadDelegate class]];
    return _private->frameLoadDelegateForwarder;
}

- _resourceLoadDelegateForwarder
{
    if (!_private->resourceProgressDelegateForwarder)
        _private->resourceProgressDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self resourceLoadDelegate] defaultTarget: [WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] templateClass: [WebDefaultResourceLoadDelegate class]];
    return _private->resourceProgressDelegateForwarder;
}

- (void)_cacheResourceLoadDelegateImplementations
{
    if ([[self resourceLoadDelegate] respondsToSelector:@selector(webView:resource:didCancelAuthenticationChallenge:fromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsDidCancelAuthenticationChallenge = YES;

    if ([[self resourceLoadDelegate] respondsToSelector:@selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsDidReceiveAuthenticationChallenge = YES;

    if ([[self resourceLoadDelegate] respondsToSelector:@selector(webView:resource:didFinishLoadingFromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsDidFinishLoadingFromDataSource = YES;
    if ([[self resourceLoadDelegate] respondsToSelector:@selector(webView:resource:didReceiveContentLength:fromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsDidReceiveContentLength = YES;
    if ([[self resourceLoadDelegate] respondsToSelector:@selector(webView:resource:didReceiveResponse:fromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsDidReceiveResponse = YES;
    if ([[self resourceLoadDelegate] respondsToSelector:@selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsWillSendRequest = YES;
    if ([[self resourceLoadDelegate] respondsToSelector: @selector(webView:identifierForInitialRequest:fromDataSource:)])
        _private->resourceLoadDelegateImplementations.delegateImplementsIdentifierForRequest = YES;
}

- (WebResourceDelegateImplementationCache)_resourceLoadDelegateImplementations
{
    return _private->resourceLoadDelegateImplementations;
}

- _policyDelegateForwarder
{
    if (!_private->policyDelegateForwarder)
        _private->policyDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self policyDelegate] defaultTarget: [WebDefaultPolicyDelegate sharedPolicyDelegate] templateClass: [WebDefaultPolicyDelegate class]];
    return _private->policyDelegateForwarder;
}

- _UIDelegateForwarder
{
    if (!_private->UIDelegateForwarder)
        _private->UIDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self UIDelegate] defaultTarget: [WebDefaultUIDelegate sharedUIDelegate] templateClass: [WebDefaultUIDelegate class]];
    return _private->UIDelegateForwarder;
}


- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;

    if ([frame dataSource] == dataSource)
        return frame;

    if ([frame provisionalDataSource] == dataSource)
        return frame;

    frames = [frame childFrames];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForDataSource: dataSource fromFrame: aFrame];
        if (result)
            return result;
    }

    return nil;
}


- (WebFrame *)_frameForDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [self mainFrame];

    return [self _frameForDataSource: dataSource fromFrame: frame];
}


- (WebFrame *)_frameForView: (WebFrameView *)aView fromFrame: (WebFrame *)frame
{
    NSArray *frames;
    int i, count;
    WebFrame *result, *aFrame;

    if ([frame frameView] == aView)
        return frame;

    frames = [frame childFrames];
    count = [frames count];
    for (i = 0; i < count; i++){
        aFrame = [frames objectAtIndex: i];
        result = [self _frameForView: aView fromFrame: aFrame];
        if (result)
            return result;
    }

    return nil;
}

- (WebFrame *)_frameForView: (WebFrameView *)aView
{
    WebFrame *frame = [self mainFrame];

    return [self _frameForView: aView fromFrame: frame];
}

- (void)_registerDraggedTypes
{
    [self registerForDraggedTypes:[NSPasteboard _web_dragTypesForURL]];
}

- (void)_closeWindow
{
    [[self _UIDelegateForwarder] webViewClose:self];
}

+ (void)_unregisterViewClassAndRepresentationClassForMIMEType:(NSString *)MIMEType;
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:NO] removeObjectForKey:MIMEType];
}

+ (void)_registerViewClass:(Class)viewClass representationClass:(Class)representationClass forURLScheme:(NSString *)URLScheme;
{
    NSString *MIMEType = [self _generatedMIMETypeForURLScheme:URLScheme];
    [self registerViewClass:viewClass representationClass:representationClass forMIMEType:MIMEType];

    // This is used to make _representationExistsForURLScheme faster.
    // Without this set, we'd have to create the MIME type each time.
    if (schemesWithRepresentationsSet == nil) {
        schemesWithRepresentationsSet = [[NSMutableSet alloc] init];
    }
    [schemesWithRepresentationsSet addObject:[[[URLScheme lowercaseString] copy] autorelease]];
}

+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme
{
    return [@"x-apple-web-kit/" stringByAppendingString:[URLScheme lowercaseString]];
}

+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme
{
    return [schemesWithRepresentationsSet containsObject:[URLScheme lowercaseString]];
}

+ (BOOL)_canHandleRequest:(NSURLRequest *)request
{
    if ([NSURLConnection canHandleRequest:request]) {
        return YES;
    }

    return [self _representationExistsForURLScheme:[[request URL] scheme]];
}

+ (NSString *)_decodeData:(NSData *)data
{
    return [WebCoreEncodings decodeData:data];
}

- (void)_pushPerformingProgrammaticFocus
{
    _private->programmaticFocusCount++;
}

- (void)_popPerformingProgrammaticFocus
{
    _private->programmaticFocusCount--;
}

- (BOOL)_isPerformingProgrammaticFocus
{
    return _private->programmaticFocusCount != 0;
}

#define UnknownTotalBytes -1
#define WebProgressItemDefaultEstimatedLength 1024*16

- (void)_didChangeValueForKey: (NSString *)key
{
    LOG (Bindings, "calling didChangeValueForKey: %@", key);
    [self didChangeValueForKey: key];
}

- (void)_willChangeValueForKey: (NSString *)key
{
    LOG (Bindings, "calling willChangeValueForKey: %@", key);
    [self willChangeValueForKey: key];
}

// Always start progress at INITIAL_PROGRESS_VALUE so it appears progress indicators
// will immediately show some progress.  This helps provide feedback as soon as a load
// starts.
#define INITIAL_PROGRESS_VALUE 0.1

- (void)_resetProgress
{
    [_private->progressItems release];
    _private->progressItems = nil;
    _private->totalPageAndResourceBytesToLoad = 0;
    _private->totalBytesReceived = 0;
    _private->progressValue = 0;
    _private->lastNotifiedProgressValue = 0;
    _private->lastNotifiedProgressTime = 0;
    _private->finalProgressChangedSent = NO;
    _private->numProgressTrackedFrames = 0;
    [_private->orginatingProgressFrame release];
    _private->orginatingProgressFrame = nil;
}
- (void)_progressStarted:(WebFrame *)frame
{
    LOG (Progress, "frame %p(%@), _private->numProgressTrackedFrames %d, _private->orginatingProgressFrame %p", frame, [frame name], _private->numProgressTrackedFrames, _private->orginatingProgressFrame);
    [self _willChangeValueForKey: @"estimatedProgress"];
    if (_private->numProgressTrackedFrames == 0 || _private->orginatingProgressFrame == frame){
        [self _resetProgress];
        _private->progressValue = INITIAL_PROGRESS_VALUE;
        _private->orginatingProgressFrame = [frame retain];
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:self];
    }
    _private->numProgressTrackedFrames++;
    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_finalProgressComplete
{
    LOG (Progress, "");

    // Before resetting progress value be sure to send client a least one notification
    // with final progress value.
    if (!_private->finalProgressChangedSent) {
        _private->progressValue = 1;
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
    }
    
    [self _resetProgress];
    
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
}

- (void)_progressCompleted:(WebFrame *)frame
{    
    LOG (Progress, "frame %p(%@), _private->numProgressTrackedFrames %d, _private->orginatingProgressFrame %p", frame, [frame name], _private->numProgressTrackedFrames, _private->orginatingProgressFrame);

    if (_private->numProgressTrackedFrames <= 0)
        return;

    [self _willChangeValueForKey: @"estimatedProgress"];

    _private->numProgressTrackedFrames--;
    if (_private->numProgressTrackedFrames == 0 ||
        (frame == _private->orginatingProgressFrame && _private->numProgressTrackedFrames != 0)){
        [self _finalProgressComplete];
    }
    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_incrementProgressForConnection:(NSURLConnection *)con response:(NSURLResponse *)response;
{
    if (!con)
        return;

    LOG (Progress, "_private->numProgressTrackedFrames %d, _private->orginatingProgressFrame %p", _private->numProgressTrackedFrames, _private->orginatingProgressFrame);
    
    if (_private->numProgressTrackedFrames <= 0)
        return;
        
    WebProgressItem *item = [[WebProgressItem alloc] init];

    if (!item)
        return;

    long long length = [response expectedContentLength];
    if (length < 0){
        length = WebProgressItemDefaultEstimatedLength;
    }
    item->estimatedLength = length;
    _private->totalPageAndResourceBytesToLoad += length;
    
    if (!_private->progressItems)
        _private->progressItems = [[NSMutableDictionary alloc] init];
        
    [_private->progressItems _web_setObject:item forUncopiedKey:con];
    [item release];
}

- (void)_incrementProgressForConnection:(NSURLConnection *)con data:(NSData *)data
{
    if (!con)
        return;

    WebProgressItem *item = [_private->progressItems objectForKey:con];

    if (!item)
        return;

    [self _willChangeValueForKey: @"estimatedProgress"];

    unsigned bytesReceived = [data length];
    double increment = 0, percentOfRemainingBytes;
    long long remainingBytes, estimatedBytesForPendingRequests;

    item->bytesReceived += bytesReceived;
    if (item->bytesReceived > item->estimatedLength){
        _private->totalPageAndResourceBytesToLoad += ((item->bytesReceived*2) - item->estimatedLength);
        item->estimatedLength = item->bytesReceived*2;
    }
    
    int numPendingOrLoadingRequests = [[self mainFrame] _numPendingOrLoadingRequests:YES];
    estimatedBytesForPendingRequests = WebProgressItemDefaultEstimatedLength * numPendingOrLoadingRequests;
    remainingBytes = ((_private->totalPageAndResourceBytesToLoad + estimatedBytesForPendingRequests) - _private->totalBytesReceived);
    percentOfRemainingBytes = (double)bytesReceived / (double)remainingBytes;
    increment = (1.0 - _private->progressValue) * percentOfRemainingBytes;
    
    _private->totalBytesReceived += bytesReceived;
    
    _private->progressValue += increment;

    if (_private->progressValue < 0.0)
        _private->progressValue = 0.0;

    if (_private->progressValue > 1.0)
        _private->progressValue = 1.0;

    double now = CFAbsoluteTimeGetCurrent();
    double notifiedProgressTimeDelta = CFAbsoluteTimeGetCurrent() - _private->lastNotifiedProgressTime;
    _private->lastNotifiedProgressTime = now;
    
    LOG (Progress, "_private->progressValue %g, _private->numProgressTrackedFrames %d", _private->progressValue, _private->numProgressTrackedFrames);
    double notificationProgressDelta = _private->progressValue - _private->lastNotifiedProgressValue;
    if ((notificationProgressDelta >= _private->progressNotificationInterval ||
            notifiedProgressTimeDelta >= _private->progressNotificationTimeInterval) &&
            _private->numProgressTrackedFrames > 0) {
        if (!_private->finalProgressChangedSent) {
            if (_private->progressValue == 1)
                _private->finalProgressChangedSent = YES;
            [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
            _private->lastNotifiedProgressValue = _private->progressValue;
        }
    }

    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_completeProgressForConnection:(NSURLConnection *)con
{
    WebProgressItem *item = [_private->progressItems objectForKey:con];

    if (!item)
        return;
        
    // Adjust the total expected bytes to account for any overage/underage.
    long long delta = item->bytesReceived - item->estimatedLength;
    _private->totalPageAndResourceBytesToLoad += delta;
    item->estimatedLength = item->bytesReceived;
}

// Required to prevent automatic observer notifications.
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key {
    return NO;
}

- (NSArray *)_declaredKeys {
    static NSArray *declaredKeys = nil;
    
    if (!declaredKeys) {
        declaredKeys = [[NSArray alloc] initWithObjects:_WebMainFrameURLKey, _WebIsLoadingKey, _WebEstimatedProgressKey, _WebCanGoBackKey, _WebCanGoForwardKey, _WebMainFrameTitleKey, _WebMainFrameIconKey, nil];
    }

    return declaredKeys;
}

- (void)setObservationInfo:(void *)info
{
    _private->observationInfo = info;
}

- (void *)observationInfo
{
    return _private->observationInfo;
}

- (void)_willChangeBackForwardKeys
{
    [self _willChangeValueForKey: _WebCanGoBackKey];
    [self _willChangeValueForKey: _WebCanGoForwardKey];
}

- (void)_didChangeBackForwardKeys
{
    [self _didChangeValueForKey: _WebCanGoBackKey];
    [self _didChangeValueForKey: _WebCanGoForwardKey];
}

- (void)_didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    [self _willChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];

        [self _willChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didCommitLoadForFrame:(WebFrame *)frame
{
    if (frame == [self mainFrame]){
        [self _didChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFinishLoadForFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        // Force an observer update by sending a will/did.
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebIsLoadingKey];
        
        [self _didChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_reloadForPluginChanges
{
    [[self mainFrame] _reloadForPluginChanges];
}

@end


@implementation _WebSafeForwarder

- initWithTarget: t defaultTarget: dt templateClass: (Class)aClass
{
    [super init];
    target = t;		// Non retained.
    defaultTarget = dt;
    templateClass = aClass;
    return self;
}


// Used to send messages to delegates that implement informal protocols.
+ safeForwarderWithTarget: t defaultTarget: dt templateClass: (Class)aClass;
{
    return [[[_WebSafeForwarder alloc] initWithTarget: t defaultTarget: dt templateClass: aClass] autorelease];
}

#ifndef NDEBUG
NSMutableDictionary *countInvocations;
#endif

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
#ifndef NDEBUG
    if (!countInvocations){
        countInvocations = [[NSMutableDictionary alloc] init];
    }
    NSNumber *count = [countInvocations objectForKey: NSStringFromSelector([anInvocation selector])];
    if (!count)
        count = [NSNumber numberWithInt: 1];
    else
        count = [NSNumber numberWithInt: [count intValue] + 1];
    [countInvocations setObject: count forKey: NSStringFromSelector([anInvocation selector])];
#endif
    if ([target respondsToSelector: [anInvocation selector]])
        [anInvocation invokeWithTarget: target];
    else if ([defaultTarget respondsToSelector: [anInvocation selector]])
        [anInvocation invokeWithTarget: defaultTarget];
    // Do nothing quietly if method not implemented.
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [templateClass instanceMethodSignatureForSelector: aSelector];
}

@end

@interface WebView (WebInternal)
- (BOOL)_isLoading;
@end

@implementation WebView

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    Class viewClass = [WebFrameView _viewClassForMIMEType:MIMEType];
    Class repClass = [WebDataSource _representationClassForMIMEType:MIMEType];

    if (!viewClass || !repClass) {
	[[WebPluginDatabase installedPlugins] loadPluginIfNeededForMIMEType: MIMEType];
        viewClass = [WebFrameView _viewClassForMIMEType:MIMEType];
        repClass = [WebDataSource _representationClassForMIMEType:MIMEType];
    }
    
    // Special-case WebTextView for text types that shouldn't be shown.
    if (viewClass && repClass) {
        if (viewClass == [WebTextView class] &&
            repClass == [WebTextRepresentation class] &&
            [[WebTextView unsupportedTextMIMETypes] containsObject:MIMEType]) {
            return NO;
        }
        return YES;
    }
    
    return NO;
}

+ (BOOL)canShowMIMETypeAsHTML:(NSString *)MIMEType
{
    return [WebFrameView _canShowMIMETypeAsHTML:MIMEType];
}

- (void)_commonInitializationWithFrameName:(NSString *)frameName groupName:(NSString *)groupName
{
    NSRect f = [self frame];
    WebFrameView *wv = [[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)];
    [wv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: wv];
    [wv release];

    _private->mainFrame = [[WebFrame alloc] initWithName: frameName webFrameView: wv  webView: self];
    [self setGroupName:groupName];
    
    // If there's already a next key view (e.g., from a nib), wire it up to our
    // contained frame view. In any case, wire our next key view up to the our
    // contained frame view. This works together with our becomeFirstResponder 
    // and setNextKeyView overrides.
    NSView *nextKeyView = [self nextKeyView];
    if (nextKeyView != nil && nextKeyView != wv) {
        [wv setNextKeyView:nextKeyView];
    }
    [super setNextKeyView:wv];

    ++WebViewCount;

    [self _registerDraggedTypes];

    // Update WebCore with preferences.  These values will either come from an archived WebPreferences,
    // or from the standard preferences, depending on whether this method was called from initWithCoder:
    // or initWithFrame, respectively.
    [self _updateWebCoreSettingsFromPreferences: [self preferences]];
    
    // Register to receive notifications whenever preference values change.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedNotification object:[self preferences]];
}

- init
{
    return [self initWithFrame: NSZeroRect frameName: nil groupName: nil];
}

- initWithFrame: (NSRect)f
{
    [self initWithFrame: f frameName:nil groupName:nil];
    return self;
}

- initWithFrame: (NSRect)f frameName: (NSString *)frameName groupName: (NSString *)groupName;
{
    [super initWithFrame: f];
    _private = [[WebViewPrivate alloc] init];
    [self _commonInitializationWithFrameName:frameName groupName:groupName];
    [self setMaintainsBackForwardList: YES];
    return self;
}

- (id)initWithCoder:(NSCoder *)decoder
{
    WebView *result = nil;

NS_DURING

    NSString *frameName;
    NSString *groupName;
    
    result = [super initWithCoder:decoder];
    result->_private = [[WebViewPrivate alloc] init];
    
    // We don't want any of the archived subviews.  The subviews will always
    // be created in _commonInitializationFrameName:groupName:.
    [[result subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    
    if ([decoder allowsKeyedCoding]){
        frameName = [decoder decodeObjectForKey:@"FrameName"];
        groupName = [decoder decodeObjectForKey:@"GroupName"];
                
        [result setPreferences: [decoder decodeObjectForKey:@"Preferences"]];
	result->_private->useBackForwardList = [decoder decodeBoolForKey:@"UseBackForwardList"];

        LOG (Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", frameName, groupName, (int)_private->useBackForwardList);
    }
    else {
        int version;
    
        [decoder decodeValueOfObjCType:@encode(int) at:&version];
        frameName = [decoder decodeObject];
        groupName = [decoder decodeObject];
        [result setPreferences: [decoder decodeObject]];
        if (version > 1)
            [decoder decodeValuesOfObjCTypes:"c",&result->_private->useBackForwardList];
    }
    [result _commonInitializationWithFrameName:frameName groupName:groupName];
    
NS_HANDLER

    result = nil;
    [self release];

NS_ENDHANDLER

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    [super encodeWithCoder:encoder];

    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:[[self mainFrame] name] forKey:@"FrameName"];
        [encoder encodeObject:[self groupName] forKey:@"GroupName"];
        [encoder encodeObject:[self preferences] forKey:@"Preferences"];
	[encoder encodeBool:_private->useBackForwardList forKey:@"UseBackForwardList"];

        LOG (Encoding, "FrameName = %@, GroupName = %@, useBackForwardList = %d\n", [[self mainFrame] name], [self groupName], (int)_private->useBackForwardList);
    }
    else {
        int version = WebViewVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:[[self mainFrame] name]];
        [encoder encodeObject:[self groupName]];
        [encoder encodeObject:[self preferences]];
        [encoder encodeValuesOfObjCTypes:"c",&_private->useBackForwardList];
    }
}

- (void)dealloc
{
    [self _close];
    
    --WebViewCount;
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [WebPreferences _removeReferenceForIdentifier: [self preferencesIdentifier]];
    
    [_private release];
    [super dealloc];
}

- (void)setPreferences: (WebPreferences *)prefs
{
    if (_private->preferences != prefs){
        [[NSNotificationCenter defaultCenter] removeObserver: self name: WebPreferencesChangedNotification object: [self preferences]];
        [WebPreferences _removeReferenceForIdentifier: [_private->preferences identifier]];
        [_private->preferences release];
        _private->preferences = [prefs retain];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                    name:WebPreferencesChangedNotification object:[self preferences]];
    }
}

- (WebPreferences *)preferences
{
    return _private->preferences ? _private->preferences : [WebPreferences standardPreferences];
}

- (void)setPreferencesIdentifier:(NSString *)anIdentifier
{
    if (![anIdentifier isEqual: [[self preferences] identifier]]){
        [self setPreferences: [[WebPreferences alloc] initWithIdentifier:anIdentifier]];
    }
}

- (NSString *)preferencesIdentifier
{
    return [[self preferences] identifier];
}


- (void)setUIDelegate:delegate
{
    _private->UIDelegate = delegate;
    [_private->UIDelegateForwarder release];
    _private->UIDelegateForwarder = nil;
}

- UIDelegate
{
    return _private->UIDelegate;
}

- (void)setResourceLoadDelegate: delegate
{
    _private->resourceProgressDelegate = delegate;
    [_private->resourceProgressDelegateForwarder release];
    _private->resourceProgressDelegateForwarder = nil;
    [self _cacheResourceLoadDelegateImplementations];
}


- resourceLoadDelegate
{
    return _private->resourceProgressDelegate;
}


- (void)setDownloadDelegate: delegate
{
    _private->downloadDelegate = delegate;
}


- downloadDelegate
{
    return _private->downloadDelegate;
}

- (void)setPolicyDelegate:delegate
{
    _private->policyDelegate = delegate;
    [_private->policyDelegateForwarder release];
    _private->policyDelegateForwarder = nil;
}

- policyDelegate
{
    return _private->policyDelegate;
}

- (void)setFrameLoadDelegate:delegate
{
    _private->frameLoadDelegate = delegate;
    [_private->frameLoadDelegateForwarder release];
    _private->frameLoadDelegateForwarder = nil;
}

- frameLoadDelegate
{
    return _private->frameLoadDelegate;
}

- (WebFrame *)mainFrame
{
    return _private->mainFrame;
}

- (WebBackForwardList *)backForwardList
{
    if (_private->useBackForwardList)
        return _private->backForwardList;
    return nil;
}

- (void)setMaintainsBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (BOOL)goBack
{
    WebHistoryItem *item = [[self backForwardList] backItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeBack];
        return YES;
    }
    return NO;
}

- (BOOL)goForward
{
    WebHistoryItem *item = [[self backForwardList] forwardItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeForward];
        return YES;
    }
    return NO;
}

- (BOOL)goToBackForwardItem:(WebHistoryItem *)item
{
    [self _goToItem: item withLoadType: WebFrameLoadTypeIndexedBackForward];
    return YES;
}

- (void)setTextSizeMultiplier:(float)m
{
    if (_private->textSizeMultiplier == m) {
        return;
    }
    _private->textSizeMultiplier = m;
    [[self mainFrame] _textSizeMultiplierChanged];
}

- (float)textSizeMultiplier
{
    return _private->textSizeMultiplier;
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationName
{
    NSString *name = [applicationName copy];
    [_private->applicationNameForUserAgent release];
    _private->applicationNameForUserAgent = name;
    int i;
    for (i = 0; i != NumUserAgentStringTypes; ++i) {
        [_private->userAgent[i] release];
        _private->userAgent[i] = nil;
    }
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    NSString *override = [userAgentString copy];
    [_private->userAgentOverride release];
    _private->userAgentOverride = override;
}

- (NSString *)customUserAgent
{
    return [[_private->userAgentOverride retain] autorelease];
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] frameView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentText)]
        && [documentView supportsTextEncoding];
}

- (void)setCustomTextEncodingName:(NSString *)encoding
{
    NSString *oldEncoding = [self customTextEncodingName];
    if (encoding == oldEncoding || [encoding isEqualToString:oldEncoding]) {
        return;
    }
    [[self mainFrame] _reloadAllowingStaleDataWithOverrideEncoding:encoding];
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil) {
        dataSource = [[self mainFrame] dataSource];
    }
    if (dataSource == nil) {
        return nil;
    }
    return [dataSource _overrideEncoding];
}

- (NSString *)customTextEncodingName
{
    return [self _mainFrameOverrideEncoding];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    return [[[self mainFrame] _bridge] stringByEvaluatingJavaScriptFromString:script];
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    if (_private->userAgentOverride) {
        return [[_private->userAgentOverride retain] autorelease];
    }
    
    // Look to see if we need to spoof.
    // First step is to get the host as a C-format string.
    UserAgentStringType type = Safari;
    NSString *host = [URL host];
    char hostBuffer[256];
    if (host && CFStringGetCString((CFStringRef)host, hostBuffer, sizeof(hostBuffer), kCFStringEncodingASCII)) {
        // Next step is to find the last part of the name.
        // We get the part with only two dots, and the part with only one dot.
        const char *thirdToLastPeriod = NULL;
        const char *nextToLastPeriod = NULL;
        const char *lastPeriod = NULL;
        char c;
        char *p = hostBuffer;
        while ((c = *p)) {
            if (c == '.') {
                thirdToLastPeriod = nextToLastPeriod;
                nextToLastPeriod = lastPeriod;
                lastPeriod = p;
            } else {
                *p = tolower(c);
            }
            ++p;
        }
        // Now look up that last part in the gperf spoof table.
        if (lastPeriod) {
            const char *lastPart;
            const struct UserAgentSpoofTableEntry *entry = NULL;
            if (nextToLastPeriod) {
                lastPart = thirdToLastPeriod ? thirdToLastPeriod + 1 : hostBuffer;
                entry = _web_findSpoofTableEntry(lastPart, p - lastPart);
            }
            if (!entry) {
                lastPart = nextToLastPeriod ? nextToLastPeriod + 1 : hostBuffer;
                entry = _web_findSpoofTableEntry(lastPart, p - lastPart);
            }
            if (entry) {
                type = entry->type;
            }
        }
    }

    NSString **userAgentStorage = &_private->userAgent[type];

    NSString *userAgent = *userAgentStorage;
    if (userAgent) {
        return [[userAgent retain] autorelease];
    }
    
    // FIXME: Some day we will start reporting the actual CPU here instead of hardcoding PPC.

    NSString *language = [NSUserDefaults _web_preferredLanguageCode];
    id sourceVersion = [[NSBundle bundleForClass:[WebView class]]
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey];
    NSString *applicationName = _private->applicationNameForUserAgent;

    switch (type) {
        case Safari:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko) %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko)",
                    language, sourceVersion];
            }
            break;
        case MacIE:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@ %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@",
                    language, sourceVersion];
            }
            break;
        case WinIE:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT) AppleWebKit/%@ %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT) AppleWebKit/%@",
                    language, sourceVersion];
            }
            break;
    }

    *userAgentStorage = [userAgent retain];
    return userAgent;
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (hostWindow != _private->hostWindow) {
        [[self mainFrame] _viewWillMoveToHostWindow:hostWindow];
        [_private->hostWindow release];
        _private->hostWindow = [hostWindow retain];
        [[self mainFrame] _viewDidMoveToHostWindow];
    }
}

- (NSWindow *)hostWindow
{
    return _private->hostWindow;
}


- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return [self _web_dragOperationForDraggingInfo:sender];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    NSURL *URL = [[sender draggingPasteboard] _web_bestURL];

    if (URL) {
        NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
        [[self mainFrame] loadRequest:request];
        [request release];
    }
}

- (BOOL)acceptsFirstResponder
{
    return [[[self mainFrame] frameView] acceptsFirstResponder];
}

- (BOOL)becomeFirstResponder
{
    // This works together with setNextKeyView to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has very similar code.
    NSWindow *window = [self window];
    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    
    if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
        NSView *previousValidKeyView = [self previousValidKeyView];
        if ((previousValidKeyView != self) && (previousValidKeyView != mainFrameView)) {
            [window makeFirstResponder:previousValidKeyView];
            return YES;
        } else {
            return NO;
        }
    }
    
    if ([mainFrameView acceptsFirstResponder]) {
        [window makeFirstResponder:mainFrameView];
        return YES;
    } 
    
    return NO;
}

- (void)setNextKeyView:(NSView *)aView
{
    // This works together with becomeFirstResponder to splice the WebView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebFrameView has very similar code.
    WebFrameView *mainFrameView = [[self mainFrame] frameView];
    if (mainFrameView != nil) {
        [mainFrameView setNextKeyView:aView];
    } else {
        [super setNextKeyView:aView];
    }
}

// Return the frame holding first responder
- (WebFrame *)_currentFrame
{
    // Find the frame holding the first responder, or holding the first form in the doc
    NSResponder *resp = [[self window] firstResponder];
    if (!resp || ![resp isKindOfClass:[NSView class]] || ![(NSView *)resp isDescendantOf:self]) {
        return nil;	// first responder outside our view tree
    } else {
        WebFrameView *frameView = (WebFrameView *)[(NSView *)resp _web_superviewOfClass:[WebFrameView class]];
        return [frameView webFrame];
    }
}

static WebFrame *incrementFrame(WebFrame *curr, BOOL forward, BOOL wrapFlag)
{
    return forward ? [curr _nextFrameWithWrap:wrapFlag]
                   : [curr _previousFrameWithWrap:wrapFlag];
}

// Search from the end of the currently selected location, or from the beginning of the
// document if nothing is selected.  Deals with subframes.
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    // Get the frame holding the selection, or start with the main frame
    WebFrame *startFrame = [self _currentFrame];
    if (!startFrame) {
        startFrame = [self mainFrame];
    }

    // Search the first frame, then all the other frames, in order
    NSView <WebDocumentSearching> *startSearchView = nil;
    BOOL startHasSelection = NO;
    WebFrame *frame = startFrame;
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebDocumentSearching)]) {
            NSView <WebDocumentSearching> *searchView = (NSView <WebDocumentSearching> *)view;

            // first time through
            if (frame == startFrame) {
                // Remember if start even has a selection, to know if we need to search more later
                if ([searchView isKindOfClass:[WebHTMLView class]]) {
                    // optimization for the common case, to avoid making giant string for selection
                    startHasSelection = [[startFrame _bridge] selectionStart] != nil;
                } else if ([searchView conformsToProtocol:@protocol(WebDocumentText)]) {
                    startHasSelection = [(id <WebDocumentText>)searchView selectedString] != nil;
                }
                startSearchView = searchView;
            }
            
            // Note at this point we are assuming the search will be done top-to-bottom,
            // not starting at any selection that exists.  See 3228554.
            BOOL success = [searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:NO];
            if (success) {
                [[self window] makeFirstResponder:searchView];
                return YES;
            }
        }
        frame = incrementFrame(frame, forward, wrapFlag);
    } while (frame != nil && frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (wrapFlag && startHasSelection && startSearchView) {
        BOOL success = [startSearchView searchFor:string direction:forward caseSensitive:caseFlag wrap:YES];
        if (success) {
            [[self window] makeFirstResponder:startSearchView];
            return YES;
        }
    }
    return NO;
}

+ (void) registerViewClass:(Class)viewClass representationClass: (Class)representationClass forMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:YES] setObject:viewClass forKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:YES] setObject:representationClass forKey:MIMEType];
}

- (void)setGroupName:(NSString *)groupName
{
    if (groupName != _private->setName){
        [_private->setName release];
        _private->setName = [groupName copy];
        [WebViewSets addWebView:self toSetNamed:_private->setName];
    }
}

- (NSString *)groupName
{
    return _private->setName;
}

- (double)estimatedProgress
{
    return _private->progressValue;
}

@end


@implementation WebView (WebIBActions)

- (IBAction)takeStringURLFrom: sender
{
    NSString *URLString = [sender stringValue];
    
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (BOOL)canGoBack
{
    return [[self backForwardList] backItem] != nil;
}

- (BOOL)canGoForward
{
    return [[self backForwardList] forwardItem] != nil;
}

- (IBAction)goBack:(id)sender
{
    [self goBack];
}

- (IBAction)goForward:(id)sender
{
    [self goForward];
}

- (IBAction)stopLoading:(id)sender
{
    [[self mainFrame] stopLoading];
}

- (IBAction)reload:(id)sender
{
    [[self mainFrame] reload];
}

#define MinimumTextSizeMultiplier	0.5
#define MaximumTextSizeMultiplier	3.0
#define TextSizeMultiplierRatio		1.2

- (BOOL)canMakeTextSmaller
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    // FIXME: This will prevent text sizing in subframes if the main frame doesn't support it
    if (![[[[self mainFrame] frameView] documentView] conformsToProtocol:@protocol(_web_WebDocumentTextSizing)]) {
        return NO;
    }
    if ([self textSizeMultiplier]/TextSizeMultiplierRatio < MinimumTextSizeMultiplier) {
        return NO;
    }
    return YES;
}

- (BOOL)canMakeTextLarger
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    // FIXME: This will prevent text sizing in subframes if the main frame doesn't support it
    if (![[[[self mainFrame] frameView] documentView] conformsToProtocol:@protocol(_web_WebDocumentTextSizing)]) {
        return NO;
    }
    if ([self textSizeMultiplier]*TextSizeMultiplierRatio > MaximumTextSizeMultiplier) {
        return NO;
    }
    return YES;
}

- (IBAction)makeTextSmaller:(id)sender
{
    if (![self canMakeTextSmaller]) {
        return;
    }
    [self setTextSizeMultiplier:[self textSizeMultiplier]/TextSizeMultiplierRatio];
}

- (IBAction)makeTextLarger:(id)sender
{
    if (![self canMakeTextLarger]) {
        return;
    }
    [self setTextSizeMultiplier:[self textSizeMultiplier]*TextSizeMultiplierRatio];
}

- (BOOL)_isLoading
{
    WebFrame *mainFrame = [self mainFrame];
    return [[mainFrame dataSource] isLoading]
        || [[mainFrame provisionalDataSource] isLoading];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:)) {
	return [self canGoBack];
    } else if (action == @selector(goForward:)) {
	return [self canGoForward];
    } else if (action == @selector(makeTextLarger:)) {
	return [self canMakeTextLarger];
    } else if (action == @selector(makeTextSmaller:)) {
	return [self canMakeTextSmaller];
    } else if (action == @selector(reload:)) {
	return [[self mainFrame] dataSource] != nil;
    } else if (action == @selector(stopLoading:)) {
	return [self _isLoading];
    }

    return YES;
}


@end


@implementation WebView (WebPendingPublic)

- (void)setMainFrameURL:(NSString *)URLString
{
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (NSString *)mainFrameURL
{
    WebDataSource *ds;
    ds = [[self mainFrame] provisionalDataSource];
    if (!ds)
        ds = [[self mainFrame] dataSource];
    return [[[ds request] URL] _web_originalDataAsString];
}

- (BOOL)isLoading
{
    LOG (Bindings, "isLoading = %d", (int)[self _isLoading]);
    return [self _isLoading];
}

- (NSString *)mainFrameTitle
{
    NSString *mainFrameTitle = [[[self mainFrame] dataSource] pageTitle];
    return (mainFrameTitle != nil) ? mainFrameTitle : @"";
}

- (NSImage *)mainFrameIcon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[[[[self mainFrame] dataSource] _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
}

@end

@implementation WebView (WebDebugBinding)

- (void)addObserver:(NSObject *)anObserver forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(void *)context
{
    LOG (Bindings, "addObserver:%p forKeyPath:%@ options:%x context:%p", anObserver, keyPath, options, context);
    [super addObserver:anObserver forKeyPath:keyPath options:options context:context];
}

- (void)removeObserver:(NSObject *)anObserver forKeyPath:(NSString *)keyPath
{
    LOG (Bindings, "removeObserver:%p forKeyPath:%@", anObserver, keyPath);
    [super removeObserver:anObserver forKeyPath:keyPath];
}

@end

