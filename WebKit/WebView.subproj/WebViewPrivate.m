/*	
    WebViewPrivate.m
    Copyright (c) 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebViewPrivate.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultFrameLoadDelegate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebFormDelegatePrivate.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebKit/WebAssertions.h>

#import <Foundation/NSURLFileTypeMappings.h>
#import <Foundation/NSData_NSURLExtras.h>
#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLDownloadPrivate.h>
#import <Foundation/NSURLRequest.h>

#import <WebCore/WebCoreEncodings.h>
#import <WebCore/WebCoreSettings.h>

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


- (void)_progressStarted
{
    [self _willChangeValueForKey: @"estimatedProgress"];
    if (_private->numProgressTrackedFrames == 0){
        _private->totalPageAndResourceBytesToLoad = 0;
        _private->totalBytesReceived = 0;
        _private->progressValue = 0;
        _private->lastNotifiedProgressValue = 0;
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:self];
    }
    _private->numProgressTrackedFrames++;
    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_progressCompleted
{
    [self _willChangeValueForKey: @"estimatedProgress"];
    if (![[[self mainFrame] dataSource] isLoading])
        _private->numProgressTrackedFrames = 0;
        
    if (_private->numProgressTrackedFrames == 0){
        [_private->progressItems release];
        _private->progressItems = nil;
        _private->progressValue = 1;
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:self];
    }
    [self _didChangeValueForKey: @"estimatedProgress"];
}

- (void)_incrementProgressForConnection:(NSURLConnection *)con response:(NSURLResponse *)response;
{
    if (!con)
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

    double notificationProgressDelta = _private->progressValue - _private->lastNotifiedProgressValue;
    if (notificationProgressDelta >= _private->progressNotificationInterval){
        [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:self];
        _private->lastNotifiedProgressValue = _private->progressValue;
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
        [self _willChangeValueForKey: _WebIsLoadingKey];
        [self _willChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didCommitLoadForFrame:(WebFrame *)frame
{
    if (frame == [self mainFrame])
        [self _didChangeValueForKey: _WebMainFrameURLKey];
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFinishLoadForFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame])
        [self _didChangeValueForKey: _WebIsLoadingKey];
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame])
        [self _didChangeValueForKey: _WebIsLoadingKey];
    [NSApp setWindowsNeedUpdate:YES];
}

- (void)_didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    [self _didChangeBackForwardKeys];
    if (frame == [self mainFrame]){
        [self _didChangeValueForKey: _WebIsLoadingKey];
        [self _didChangeValueForKey: _WebMainFrameURLKey];
    }
    [NSApp setWindowsNeedUpdate:YES];
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

