/*	
    WebControllerPrivate.m
	Copyright (c) 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebFormDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultContextMenuDelegate.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebStandardPanelsPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebAssertions.h>

#import <WebFoundation/WebCacheLoaderConstants.h>
#import <WebFoundation/WebError.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebNSDataExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>

#import <WebCore/WebCoreSettings.h>

@implementation WebControllerPrivate

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
    [[aFrame webView] _setController: nil];
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
    [topLevelFrameName release];

    [preferences release];
    [settings release];
    
    [super dealloc];
}

@end


@implementation WebController (WebPrivate)

- (WebFrame *)_createFrameNamed:(NSString *)fname inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling
{
    WebView *childView = [[WebView alloc] initWithFrame:NSMakeRect(0,0,0,0)];

    [childView _setController:self];
    [childView setAllowsScrolling:allowsScrolling];
    
    WebFrame *newFrame = [[WebFrame alloc] initWithName:fname webView:childView controller:self];

    [childView release];

    [parent _addChild:newFrame];
    
    [newFrame release];
        
    return newFrame;
}

- (id<WebContextMenuDelegate>)_defaultContextMenuDelegate
{
    return _private->defaultContextMenuDelegate;
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
    if (![dataSource isDownloading])
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

+ (NSArray *)_supportedImageMIMETypes
{
    static NSArray *imageMIMETypes = nil;

    if(!imageMIMETypes){
        NSEnumerator *enumerator = [[NSImage imageFileTypes] objectEnumerator];
        WebFileTypeMappings *mappings = [WebFileTypeMappings sharedMappings];
        NSMutableSet *mimes = [NSMutableSet set];
        NSString *type;
        
        while ((type = [enumerator nextObject]) != nil) {
            NSString *mime = [mappings MIMETypeForExtension:type];
            if(mime && ![mime isEqualToString:@"application/octet-stream"] && ![mime isEqualToString:@"application/pdf"]){
                [mimes addObject:mime];
            }
        }
    
        imageMIMETypes = [[mimes allObjects] retain];
    }

    return imageMIMETypes;
}

- (void)_downloadURL:(NSURL *)URL
{
    [self _downloadURL:URL toDirectory:nil];
}

- (void)_downloadURL:(NSURL *)URL toDirectory:(NSString *)directory
{
    ASSERT(URL);
    
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    WebFrame *webFrame = [self mainFrame];

    [webFrame _downloadRequest:request toDirectory:directory];
    [request release];
}

- (BOOL)_defersCallbacks
{
    return _private->defersCallbacks;
}

- (void)_setDefersCallbacks:(BOOL)defers
{
    if (defers == _private->defersCallbacks) {
        return;
    }

    _private->defersCallbacks = defers;
    [_private->mainFrame _defersCallbacksChanged];
}

- (void)_setTopLevelFrameName:(NSString *)name
{
    // It's wrong to name a frame "_blank".
    if(![name isEqualToString:@"_blank"]){
        [_private->topLevelFrameName release];
        _private->topLevelFrameName = [name retain];
    }
}

- (WebFrame *)_findFrameInThisWindowNamed: (NSString *)name
{
    if ([_private->topLevelFrameName isEqualToString:name]) {
	return [self mainFrame];
    } else {
	return [[self mainFrame] _descendantFrameNamed:name];
    }
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
        WebController *controller;
        while ((controller = [enumerator nextObject]) != nil && frame == nil) {
	    frame = [controller _findFrameInThisWindowNamed:name];
        }
    }

    return frame;
}

- (WebController *)_openNewWindowWithRequest:(WebResourceRequest *)request behind:(BOOL)behind
{
    WebController *newWindowController = [[self windowOperationsDelegate] createWindowWithRequest:request];
    if (behind) {
        [[newWindowController windowOperationsDelegate] showWindowBehindFrontmost];
    } else {
        [[newWindowController windowOperationsDelegate] showWindow];
    }
    return newWindowController;
}

- (NSMenu *)_menuForElement:(NSDictionary *)element
{
    NSArray *defaultMenuItems = [_private->defaultContextMenuDelegate contextMenuItemsForElement:element
                                                                                defaultMenuItems:nil];
    NSArray *menuItems = nil;
    NSMenu *menu = nil;
    unsigned i;

    if (_private->contextMenuDelegate) {
        menuItems = [_private->contextMenuDelegate contextMenuItemsForElement:element
                                                             defaultMenuItems:defaultMenuItems];
    } else {
        menuItems = defaultMenuItems;
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
        [[self windowOperationsDelegate] mouseDidMoveOverElement:dictionary modifierFlags:modifierFlags];
    }
    _private->lastElementWasNonNil = dictionary != nil;
}

- (void)_setFormDelegate: (id<WebFormDelegate>)delegate
{
    _private->formDelegate = delegate;
}

- (id<WebFormDelegate>)_formDelegate
{
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
    [_private->settings setFantasyFontFamily:[preferences fantasyFontFamily]];
    [_private->settings setFixedFontFamily:[preferences fixedFontFamily]];
    [_private->settings setJavaEnabled:[preferences JavaEnabled]];
    [_private->settings setJavaScriptEnabled:[preferences JavaScriptEnabled]];
    [_private->settings setJavaScriptCanOpenWindowsAutomatically:[preferences JavaScriptCanOpenWindowsAutomatically]];
    [_private->settings setMinimumFontSize:[preferences minimumFontSize]];
    [_private->settings setPluginsEnabled:[preferences pluginsEnabled]];
    [_private->settings setSansSerifFontFamily:[preferences sansSerifFontFamily]];
    [_private->settings setSerifFontFamily:[preferences serifFontFamily]];
    [_private->settings setStandardFontFamily:[preferences standardFontFamily]];
    [_private->settings setWillLoadImagesAutomatically:[preferences willLoadImagesAutomatically]];

    if ([preferences userStyleSheetEnabled]) {
        [_private->settings setUserStyleSheetLocation:[preferences userStyleSheetLocation]];
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

@end
