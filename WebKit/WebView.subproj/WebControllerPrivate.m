/*	
    WebControllerPrivate.m
	Copyright (c) 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebContextMenuDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebControllerPolicyDelegatePrivate.h>
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
#import <WebFoundation/WebResourceHandlePrivate.h>
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

- (void)_receivedProgressForResourceHandle:(WebResourceHandle *)resourceHandle fromDataSource:(WebDataSource *)dataSource complete:(BOOL)isComplete
{
    WebFrame *frame = [dataSource webFrame];
    
    ASSERT(dataSource != nil);
    
    // This resource has completed, so check if the load is complete for all frames.
    if (isComplete) {
        if (frame != nil) {
            [frame _transitionToLayoutAcceptable];
            [frame _checkLoadComplete];
        }
    }
}

- (void)_mainReceivedProgressForResourceHandle: (WebResourceHandle *)resourceHandle bytesSoFar: (unsigned)bytesSoFar fromDataSource: (WebDataSource *)dataSource complete: (BOOL)isComplete
{
    WebFrame *frame = [dataSource webFrame];
    
    ASSERT(dataSource != nil);

    // The frame may be nil if a previously cancelled load is still making progress callbacks.
    if (frame == nil)
        return;
        
    // This resouce has completed, so check if the load is complete for all frames.
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



- (void)_receivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];

    NSString *resourceIdentifier = [[[resourceHandle _request] URL] absoluteString];
    if (resourceIdentifier == nil) {
        resourceIdentifier = [error failingURL];
    }
    if (resourceIdentifier) {
        [dataSource _addError:error forResource:resourceIdentifier];
    }
    
    [frame _checkLoadComplete];
}


- (void)_mainReceivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource
{
    WebFrame *frame = [dataSource webFrame];
    
    [dataSource _setMainDocumentError: error];
    [dataSource _setPrimaryLoadComplete: YES];

    [frame _checkLoadComplete];
}

- (void)_didStartLoading: (NSURL *)URL
{
    [[WebStandardPanels sharedStandardPanels] _didStartLoadingURL:URL inController:self];
}

- (void)_didStopLoading: (NSURL *)URL
{
    [[WebStandardPanels sharedStandardPanels] _didStopLoadingURL:URL inController:self];
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

- (void)_downloadURL:(NSURL *)URL withContentPolicy:(WebContentPolicy *)contentPolicy
{
    WebDataSource *dataSource = [[WebDataSource alloc] initWithURL:URL];
    WebFrame *webFrame = [self mainFrame];
        
    [dataSource _setContentPolicy:contentPolicy];
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
    [_private->topLevelFrameName release];
    _private->topLevelFrameName = [name retain];
}

- (WebFrame *)_frameInThisWindowNamed:(NSString *)name
{
    if ([_private->topLevelFrameName isEqualToString:name]) {
	return [self mainFrame];
    } else {
	return [[self mainFrame] frameNamed:name];
    }
}

- (WebController *)_openNewWindowWithURL:(NSURL *)URL referrer:(NSString *)referrer behind:(BOOL)behind
{
    WebController *newWindowController = [[self windowOperationsDelegate] createWindowWithURL:URL referrer:referrer];
    if (behind) {
        [[newWindowController windowOperationsDelegate] showWindowBehindFrontmost];
    } else {
        [[newWindowController windowOperationsDelegate] showWindow];
    }
    return newWindowController;
}

@end
