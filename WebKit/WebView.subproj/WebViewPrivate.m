/*	
    WebControllerPrivate.m
	Copyright (c) 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebContextMenuDelegate.h>
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
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>

@implementation WebControllerPrivate

- init 
{
    backForwardList = [[WebBackForwardList alloc] init];
    defaultContextMenuDelegate = [[WebDefaultContextMenuDelegate alloc] init];
    textSizeMultiplier = 1;
    userAgentLock = [[NSLock alloc] init];
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
    [userAgentLock release];
    
    [controllerSetName release];
    [topLevelFrameName release];

    [super dealloc];
}

@end


@implementation WebController (WebPrivate)

- (WebFrame *)_createFrameNamed:(NSString *)fname inParent:(WebFrame *)parent allowsScrolling:(BOOL)allowsScrolling
{
    WebView *childView = [[WebView alloc] initWithFrame:NSMakeRect(0,0,0,0)];

    [childView _setController:self];
    [childView setAllowsScrolling:allowsScrolling];
    
    WebFrame *newFrame = [[WebFrame alloc] initWithName:fname webView:childView provisionalDataSource:nil controller:self];

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


- (void)_mainReceivedError: (WebError *)error fromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];
    
    [dataSource _setMainDocumentError: error];
    [dataSource _setPrimaryLoadComplete: YES];

    [frame _checkLoadComplete];
}

+ (NSString *)_MIMETypeForFile: (NSString *)path
{
    NSString *result;
    NSString *extension = [path pathExtension];
    
    if ([extension isEqualToString:@""]) {
        result = @"text/html";
    }
    else {
        result = [[WebFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
        if (!result || [result isEqualToString:@"application/octet-stream"]) {
            NSString *contents = [[NSString alloc] initWithData:[NSData dataWithContentsOfFile:path]
                                                       encoding:NSASCIIStringEncoding];
            if([contents _web_looksLikeHTMLDocument]){
                result = @"text/html";
            }else{
                result = @"application/octet-stream";
            }
            [contents release];
        }
    }
    
    return result;
}

- (void)_downloadURL:(NSURL *)URL
{
    [self _downloadURL:URL toPath:nil];
}

- (void)_downloadURL:(NSURL *)URL toPath:(NSString *)path
{
    WebDataSource *dataSource = [[WebDataSource alloc] initWithURL:URL];
    WebFrame *webFrame = [self mainFrame];
        
    [dataSource _setIsDownloading:YES];
    [dataSource _setDownloadPath:path];
    if([webFrame setProvisionalDataSource:dataSource]){
        [webFrame startLoading];
    }
    [dataSource release];
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

    if(_private->contextMenuDelegate){
        menuItems = [_private->contextMenuDelegate contextMenuItemsForElement:element
                                                             defaultMenuItems:defaultMenuItems];
    } else {
        menuItems = defaultMenuItems;
    }

    if(menuItems && [menuItems count] > 0){
        menu = [[[NSMenu alloc] init] autorelease];

        for(i=0; i<[menuItems count]; i++){
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

@end
