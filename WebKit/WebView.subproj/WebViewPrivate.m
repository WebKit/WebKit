/*	
    WebControllerPrivate.m
    Copyright (c) 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultContextMenuDelegate.h>
#import <WebKit/WebDefaultLocationChangeDelegate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDefaultWindowOperationsDelegate.h>
#import <WebKit/WebDownloadPrivate.h>
#import <WebKit/WebFormDelegatePrivate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebStandardPanelsPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebNSDataExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLRequest.h>

#import <WebCore/WebCoreSettings.h>

@implementation WebViewPrivate

- init 
{
    backForwardList = [[WebBackForwardList alloc] init];
    defaultContextMenuDelegate = [[WebDefaultContextMenuDelegate alloc] init];
    textSizeMultiplier = 1;

    settings = [[WebCoreSettings alloc] init];

    return self;
}

- (void)_clearControllerReferences: (WebFrame *)aFrame
{
    NSArray *frames;
    WebFrame *nextFrame;
    int i, count;
        
    [[aFrame dataSource] _setController: nil];
    [[aFrame frameView] _setController: nil];
    [aFrame _setController: nil];

    // Walk the frame tree, niling the controller.
    frames = [aFrame children];
    count = [frames count];
    for (i = 0; i < count; i++){
        nextFrame = [frames objectAtIndex: i];
        [self _clearControllerReferences: nextFrame];
    }
}

- (void)dealloc
{
    [self _clearControllerReferences: mainFrame];
    [mainFrame _controllerWillBeDeallocated];
    
    [mainFrame release];
    [defaultContextMenuDelegate release];
    [backForwardList release];
    [applicationNameForUserAgent release];
    [userAgentOverride release];
    int i;
    for (i = 0; i != NumUserAgentStringTypes; ++i) {
        [userAgent[i] release];
    }
    
    [controllerSetName release];

    [preferences release];
    [settings release];
    [hostWindow release];
    
    [policyDelegateForwarder release];
    [contextMenuDelegateForwarder release];
    [resourceProgressDelegateForwarder release];
    [windowOperationsDelegateForwarder release];
    [locationChangeDelegateForwarder release];
    
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
    return [[WebFileTypeMappings sharedMappings] preferredExtensionForMIMEType:type];
}


- (WebFrame *)_createFrameNamed:(NSString *)fname inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling
{
    WebFrameView *childView = [[WebFrameView alloc] initWithFrame:NSMakeRect(0,0,0,0)];

    [childView _setController:self];
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


- (void)_receivedError: (WebError *)error fromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];

    [frame _checkLoadComplete];
}


- (void)_mainReceivedError:(WebError *)error fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete
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
        MIMEType = [[WebFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
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
    WebDownload *download = [[WebDownload alloc] initWithRequest:request];
    [request release];
    
    if (directory != nil && [directory isAbsolutePath]) {
        [download _setDirectoryPath:directory];
    }

    // The download retains itself in loadWithDelegate.
    [download loadWithDelegate:_private->downloadDelegate];
    [download release];
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

- (WebFrame *)_findFrameNamed: (NSString *)name
{
    // Try this controller first
    WebFrame *frame = [self _findFrameInThisWindowNamed:name];

    if (frame != nil) {
        return frame;
    }

    // Try other controllers in the same set
    if (_private->controllerSetName != nil) {
        NSEnumerator *enumerator = [WebControllerSets controllersInSetNamed:_private->controllerSetName];
        WebView *controller;
        while ((controller = [enumerator nextObject]) != nil && frame == nil) {
	    frame = [controller _findFrameInThisWindowNamed:name];
        }
    }

    return frame;
}

- (WebView *)_openNewWindowWithRequest:(NSURLRequest *)request
{
    id wd = [self windowOperationsDelegate];
    WebView *newWindowController = nil;
    if ([wd respondsToSelector:@selector(webView:createWindowWithRequest:)])
        newWindowController = [wd webView:self createWindowWithRequest:request];
    else {
        newWindowController = [[WebDefaultWindowOperationsDelegate sharedWindowOperationsDelegate] webView:self createWindowWithRequest: request];
    }

    [[newWindowController _windowOperationsDelegateForwarder] webViewShowWindow: self];

    return newWindowController;
}

- (NSMenu *)_menuForElement:(NSDictionary *)element
{
    NSArray *defaultMenuItems = [_private->defaultContextMenuDelegate
          webView:self contextMenuItemsForElement:element defaultMenuItems:nil];
    NSArray *menuItems = defaultMenuItems;
    NSMenu *menu = nil;
    unsigned i;

    if (_private->contextMenuDelegate) {
        id cd = _private->contextMenuDelegate;
        
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
        [[self _windowOperationsDelegateForwarder] webView:self mouseDidMoveOverElement:dictionary modifierFlags:modifierFlags];
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
        [_private->settings setUserStyleSheetLocation:[[preferences userStyleSheetLocation] absoluteString]];
    } else {
        [_private->settings setUserStyleSheetLocation:@""];
    }
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
    
    ASSERT (preferences == [self preferences]);
    [self _releaseUserAgentStrings];
    [self _updateWebCoreSettingsFromPreferences: preferences];
}

- _locationChangeDelegateForwarder
{
    if (!_private->locationChangeDelegateForwarder)
        _private->locationChangeDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self locationChangeDelegate]  defaultTarget: [WebDefaultLocationChangeDelegate sharedLocationChangeDelegate] templateClass: [WebDefaultLocationChangeDelegate class]];
    return _private->locationChangeDelegateForwarder;
}

- _resourceLoadDelegateForwarder
{
    if (!_private->resourceProgressDelegateForwarder)
        _private->resourceProgressDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self resourceLoadDelegate] defaultTarget: [WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] templateClass: [WebDefaultResourceLoadDelegate class]];
    return _private->resourceProgressDelegateForwarder;
}

- (void)_cacheResourceLoadDelegateImplementations
{
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

- _contextMenuDelegateForwarder
{
    if (!_private->contextMenuDelegateForwarder)
        _private->contextMenuDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self contextMenuDelegate] defaultTarget: [WebDefaultContextMenuDelegate sharedContextMenuDelegate] templateClass: [WebDefaultContextMenuDelegate class]];
    return _private->contextMenuDelegateForwarder;
}

- _windowOperationsDelegateForwarder
{
    if (!_private->windowOperationsDelegateForwarder)
        _private->windowOperationsDelegateForwarder = [[_WebSafeForwarder alloc] initWithTarget: [self windowOperationsDelegate] defaultTarget: [WebDefaultWindowOperationsDelegate sharedWindowOperationsDelegate] templateClass: [WebDefaultWindowOperationsDelegate class]];
    return _private->windowOperationsDelegateForwarder;
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

    frames = [frame children];
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

    frames = [frame children];
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

