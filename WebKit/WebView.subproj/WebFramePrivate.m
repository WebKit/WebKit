/*	
    WebFramePrivate.m
	    
    Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebFramePrivate.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPolicyDelegate.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebResourceHandle.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebHTTPResourceRequest.h>

static const char * const stateNames[] = {
    "WebFrameStateProvisional",
    "WebFrameStateCommittedPage",
    "WebFrameStateLayoutAcceptable",
    "WebFrameStateComplete"
};

@implementation WebFramePrivate

- init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    state = WebFrameStateComplete;
    loadType = WebFrameLoadTypeStandard;
    
    return self;
}

- (void)dealloc
{
    [webView _setController:nil];
    [dataSource _setController:nil];
    [provisionalDataSource _setController:nil];

    [name release];
    [webView release];
    [dataSource release];
    [provisionalDataSource release];
    [bridge release];
    [scheduledLayoutTimer release];
    [children release];
    [pluginController release];

    [currentItem release];
    [provisionalItem release];
    [previousItem release];
    
    [super dealloc];
}

- (NSString *)name { return name; }
- (void)setName:(NSString *)n 
{
    NSString *newName = [n copy];
    [name release];
    name = newName;
}

- (WebView *)webView { return webView; }
- (void)setWebView: (WebView *)v 
{ 
    [v retain];
    [webView release];
    webView = v;
}

- (WebDataSource *)dataSource { return dataSource; }
- (void)setDataSource: (WebDataSource *)d
{
    [d retain];
    [dataSource release];
    dataSource = d;
}

- (WebController *)controller { return controller; }
- (void)setController: (WebController *)c
{ 
    controller = c; // not retained (yet)
}

- (WebDataSource *)provisionalDataSource { return provisionalDataSource; }
- (void)setProvisionalDataSource: (WebDataSource *)d
{ 
    [d retain];
    [provisionalDataSource release];
    provisionalDataSource = d;
}

- (WebFrameLoadType)loadType { return loadType; }
- (void)setLoadType: (WebFrameLoadType)t
{
    loadType = t;
}

- (WebHistoryItem *)provisionalItem { return provisionalItem; }
- (void)setProvisionalItem: (WebHistoryItem *)item
{
    [item retain];
    [provisionalItem release];
    provisionalItem = item;
}

- (WebHistoryItem *)previousItem { return previousItem; }
- (void)setPreviousItem:(WebHistoryItem *)item
{
    [item retain];
    [previousItem release];
    previousItem = item;
}

- (WebHistoryItem *)currentItem { return currentItem; }
- (void)setCurrentItem:(WebHistoryItem *)item
{
    [item retain];
    [currentItem release];
    currentItem = item;
}

@end

@implementation WebFrame (WebPrivate)

// helper method used in various nav cases below
- (WebHistoryItem *)_addBackForwardItemClippedAtTarget:(BOOL)doClip
{
    WebHistoryItem *bfItem = [[[self controller] mainFrame] _createItemTreeWithTargetFrame:self clippedAtTarget:doClip];
    [[[self controller] backForwardList] addEntry:bfItem];
    [bfItem release];
    return bfItem;
}

// NB: this returns an object with retain count of 1
- (WebHistoryItem *)_createItem
{
    WebDataSource *dataSrc = [self dataSource];
    NSURL *url = [[dataSrc request] URL];
    WebHistoryItem *bfItem = [[WebHistoryItem alloc] initWithURL:url target:[self name] parent:[[self parent] name] title:[dataSrc pageTitle]];
    [bfItem setAnchor:[url fragment]];
    [dataSrc _addBackForwardItem:bfItem];

    // Set the item for which we will save document state
    [_private setPreviousItem:[_private currentItem]];
    [_private setCurrentItem:bfItem];

    return bfItem;
}

/*
    In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  The item that was the target of the user's navigation is designated as the "targetItem".  When this method is called with doClip=YES we're able to create the whole tree except for the target's children, which will be loaded in the future.  That part of the tree will be filled out as the child loads are committed.
*/
// NB: this returns an object with retain count of 1
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip
{
    WebHistoryItem *bfItem = [self _createItem];

    [self _saveScrollPositionToItem:[_private previousItem]];
    if (!(doClip && self == targetFrame)) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        [_private->bridge saveDocumentState];

        if (_private->children) {
            unsigned i;
            for (i = 0; i < [_private->children count]; i++) {
                WebFrame *child = [_private->children objectAtIndex:i];
                WebHistoryItem *childItem = [child _createItemTreeWithTargetFrame:targetFrame clippedAtTarget:doClip];
                [bfItem addChildItem:childItem];
                [childItem release];
            }
        }
    }
    if (self == targetFrame) {
        [bfItem setIsTargetItem:YES];
    }
    return bfItem;
}

- (WebFrame *)_immediateChildFrameNamed:(NSString *)name
{
    int i;
    for (i = [_private->children count]-1; i >= 0; i--) {
        WebFrame *frame = [_private->children objectAtIndex:i];
        if ([[frame name] isEqualToString:name]) {
            return frame;
        }
    }
    return nil;
}

- (WebFrame *)_descendantFrameNamed:(NSString *)name
{
    if ([[self name] isEqualToString: name]){
        return self;
    }

    NSArray *children = [self children];
    WebFrame *frame;
    unsigned i;

    for (i = 0; i < [children count]; i++){
        frame = [children objectAtIndex: i];
        frame = [frame _descendantFrameNamed:name];
        if (frame){
            return frame;
        }
    }

    return nil;
}

- (void)_controllerWillBeDeallocated
{
    [self _detachFromParent];
}

- (void)_detachFromParent
{
    WebBridge *bridge = _private->bridge;
    _private->bridge = nil;
    
    [self stopLoading];
    [self _saveScrollPositionToItem:[_private currentItem]];
    [bridge closeURL];

    [[self children] makeObjectsPerformSelector:@selector(_detachFromParent)];
    
    [_private setController:nil];
    [_private->webView _setController:nil];
    [_private->dataSource _setController:nil];
    [_private->provisionalDataSource _setController:nil];

    [_private setDataSource:nil];
    [_private setWebView:nil];

    [_private->scheduledLayoutTimer invalidate];
    [_private->scheduledLayoutTimer release];
    _private->scheduledLayoutTimer = nil;
    
    [bridge release];
}

- (void)_setController: (WebController *)controller
{
    [_private setController:controller];
}

- (void)_setDataSource:(WebDataSource *)ds
{
    ASSERT(ds != _private->dataSource);
    
    if ([_private->dataSource isDocumentHTML] && ![ds isDocumentHTML]) {
        [_private->bridge removeFromFrame];
    }

    [[self children] makeObjectsPerformSelector:@selector(_detachFromParent)];
    [_private->children release];
    _private->children = nil;
    
    [_private setDataSource:ds];
    [ds _setController:[self controller]];
}

- (void)_setLoadType: (WebFrameLoadType)t
{
    [_private setLoadType: t];
}

- (WebFrameLoadType)_loadType
{
    return [_private loadType];
}

- (void)_scheduleLayout:(NSTimeInterval)inSeconds
{
    // FIXME: Maybe this should have the code to move up the deadline if the new interval brings the time even closer.
    if (_private->scheduledLayoutTimer == nil) {
        _private->scheduledLayoutTimer = [[NSTimer scheduledTimerWithTimeInterval:inSeconds target:self selector:@selector(_timedLayout:) userInfo:nil repeats:FALSE] retain];
    }
}

- (void)_timedLayout: (id)userInfo
{
    LOG(Timing, "%@:  state = %s", [self name], stateNames[_private->state]);
    
    [_private->scheduledLayoutTimer release];
    _private->scheduledLayoutTimer = nil;
    
    if (_private->state == WebFrameStateLayoutAcceptable) {
        NSView <WebDocumentView> *documentView = [[self webView] documentView];
        
        if ([self controller])
            LOG(Timing, "%@:  performing timed layout, %f seconds since start of document load", [self name], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
            
        [documentView setNeedsLayout: YES];

        if ([documentView isKindOfClass: [NSView class]]) {
            NSView *dview = (NSView *)documentView;
            
            
            NSRect frame = [dview frame];
            
            if (frame.size.width == 0 || frame.size.height == 0){
                // We must do the layout now, rather than depend on
                // display to do a lazy layout because the view
                // may be recently initialized with a zero size
                // and the AppKit will optimize out any drawing.
                
                // Force a layout now.  At this point we could
                // check to see if any CSS is pending and delay
                // the layout further to avoid the flash of unstyled
                // content.                    
                [documentView layout];
            }
        }
            
        [documentView setNeedsDisplay: YES];
    }
    else {
        if ([self controller])
            LOG(Timing, "%@:  NOT performing timed layout (not needed), %f seconds since start of document load", [self name], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
    }
}


- (void)_transitionToLayoutAcceptable
{
    switch ([self _state]) {
    	case WebFrameStateCommittedPage:
        {
            [self _setState: WebFrameStateLayoutAcceptable];
                    
            // Start a timer to guarantee that we get an initial layout after
            // X interval, even if the document and resources are not completely
            // loaded.
            BOOL timedDelayEnabled = [[WebPreferences standardPreferences] _initialTimedLayoutEnabled];
            if (timedDelayEnabled) {
                NSTimeInterval defaultTimedDelay = [[WebPreferences standardPreferences] _initialTimedLayoutDelay];
                double timeSinceStart;

                // If the delay getting to the commited state exceeds the initial layout delay, go
                // ahead and schedule a layout.
                timeSinceStart = (CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
                if (timeSinceStart > (double)defaultTimedDelay) {
                    LOG(Timing, "performing early layout because commit time, %f, exceeded initial layout interval %f", timeSinceStart, defaultTimedDelay);
                    [self _timedLayout: nil];
                }
                else {
                    NSTimeInterval timedDelay = defaultTimedDelay - timeSinceStart;
                    
                    LOG(Timing, "registering delayed layout after %f seconds, time since start %f", timedDelay, timeSinceStart);
                    [self _scheduleLayout: timedDelay];
                }
            }
            break;
        }

        case WebFrameStateProvisional:
        case WebFrameStateComplete:
        case WebFrameStateLayoutAcceptable:
        {
            break;
        }
        
        default:
        {
	    ASSERT_NOT_REACHED();
        }
    }
}


- (void)_transitionToCommitted
{
    ASSERT([self controller] != nil);

    // Destroy plug-ins before blowing away the view.
    [_private->pluginController destroyAllPlugins];
        
    switch ([self _state]) {
    	case WebFrameStateProvisional:
        {
            WebFrameLoadType loadType = [self _loadType];
            if (loadType == WebFrameLoadTypeForward ||
                loadType == WebFrameLoadTypeBack ||
                loadType == WebFrameLoadTypeIndexedBackForward)
            {
                // Once committed, we want to use current item for saving DocState, and
                // the provisional item for restoring state.
                // Note previousItem must be set before we close the URL, which will
                // happen when the data source is made non-provisional below
                [_private setPreviousItem:[_private currentItem]];
                ASSERT([_private provisionalItem]);
                [_private setCurrentItem:[_private provisionalItem]];
                [_private setProvisionalItem:nil];
            }

            // Set the committed data source on the frame.
            [self _setDataSource:_private->provisionalDataSource];
            [_private setProvisionalDataSource: nil];

            [self _setState: WebFrameStateCommittedPage];
        
            // Handle adding the URL to the back/forward list.
            WebDataSource *ds = [self dataSource];
            WebHistoryItem *entry = nil;
            NSString *ptitle = [ds pageTitle];

            if ([[self controller] usesBackForwardList]) {
                switch (loadType) {
                case WebFrameLoadTypeForward:
                case WebFrameLoadTypeBack:
                case WebFrameLoadTypeIndexedBackForward:
                    // Must grab the current scroll position before disturbing it
                    [self _saveScrollPositionToItem:[_private previousItem]];
                    [self _restoreScrollPosition];
                    break;
                    
                case WebFrameLoadTypeReload:
                    [self _saveScrollPositionToItem:[_private currentItem]];
                    break;
    
                case WebFrameLoadTypeStandard:
                    // Add item to history.
                    entry = [[WebHistory sharedHistory] addEntryForURL: [[[ds _originalRequest] URL] _web_canonicalize]];
                    if (ptitle)
                        [entry setTitle: ptitle];

                    if (![ds _isClientRedirect]) {
                        [self _addBackForwardItemClippedAtTarget:YES];
                    } else {
                        // update the URL in the BF list that we made before the redirect
                        [[[[self controller] backForwardList] currentEntry] setURL:[[ds request] URL]];
                    }
                    break;
                    
                case WebFrameLoadTypeInternal:
                    {  // braces because the silly compiler lets you declare vars everywhere but here?!
                    // Add an item to the item tree for this frame
                    WebHistoryItem *item = [self _createItem];
                    ASSERT([[self parent]->_private currentItem]);
                    [[[self parent]->_private currentItem] addChildItem:item];
                    [item release];
                    }
                    break;

                case WebFrameLoadTypeReloadAllowingStaleData:
                    break;
                    
                // FIXME Remove this check when dummy ds is removed.  An exception should be thrown
                // if we're in the WebFrameLoadTypeUninitialized state.
                default:
		    ASSERT_NOT_REACHED();
                }
            }

            // Tell the client we've committed this URL.
	    [[[self controller] locationChangeDelegate] locationChangeCommittedForDataSource:ds];
            
            // If we have a title let the controller know about it.
            if (ptitle) {
                [entry setTitle:ptitle];
		[[[self controller] locationChangeDelegate] receivedPageTitle:ptitle forDataSource:ds];
            }
            break;
        }
        
        case WebFrameStateCommittedPage:
        case WebFrameStateLayoutAcceptable:
        case WebFrameStateComplete:
        default:
        {
	    ASSERT_NOT_REACHED();
        }
    }
}

- (WebFrameState)_state
{
    return _private->state;
}

- (void)_setState: (WebFrameState)newState
{
    LOG(Loading, "%@:  transition from %s to %s", [self name], stateNames[_private->state], stateNames[newState]);
    if ([self controller])
        LOG(Timing, "%@:  transition from %s to %s, %f seconds since start of document load", [self name], stateNames[_private->state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && self == [[self controller] mainFrame]){
        LOG(DocumentLoad, "completed %@ (%f seconds)", [[[self dataSource] request] URL], CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
    }
    
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithInt:_private->state], WebPreviousFrameState,
                    [NSNumber numberWithInt:newState], WebCurrentFrameState, nil];
                    
    [[NSNotificationCenter defaultCenter] postNotificationName:WebFrameStateChangedNotification object:self userInfo:userInfo];
    
    _private->state = newState;
    
    if (_private->state == WebFrameStateProvisional) {
        // FIXME: This is OK as long as no one resizes the window,
        // but in the case where someone does, it means garbage outside
        // the occupied part of the scroll view.
        [[[self webView] frameScrollView] setDrawsBackground:NO];
    }
    
    if (_private->state == WebFrameStateComplete) {
        NSScrollView *sv = [[self webView] frameScrollView];
        [sv setDrawsBackground:YES];
        // FIXME: This overrides the setCopiesOnScroll setting done by
        // WebCore based on whether the page's contents are dynamic or not.
        [[sv contentView] setCopiesOnScroll:YES];
        [_private->scheduledLayoutTimer fire];
   	ASSERT(_private->scheduledLayoutTimer == nil);
        [_private setPreviousItem:nil];
    }
}

- (void)_isLoadComplete
{
    ASSERT([self controller] != nil);

    switch ([self _state]) {
        case WebFrameStateProvisional:
        {
            WebDataSource *pd = [self provisionalDataSource];
            
            LOG(Loading, "%@:  checking complete in WebFrameStateProvisional", [self name]);
            // If we've received any errors we may be stuck in the provisional state and actually
            // complete.
            if ([pd mainDocumentError]) {
                // Check all children first.
                LOG(Loading, "%@:  checking complete, current state WebFrameStateProvisional", [self name]);
                if (![pd isLoading]) {
                    LOG(Loading, "%@:  checking complete in WebFrameStateProvisional, load done", [self name]);

                    [[[self controller] locationChangeDelegate] locationChangeDone: [pd mainDocumentError] forDataSource:pd];

                    // We know the provisional data source didn't cut the mustard, release it.
                    [_private setProvisionalDataSource: nil];
                    
                    [self _setState: WebFrameStateComplete];
                    return;
                }
            }
            return;
        }
        
        case WebFrameStateCommittedPage:
        case WebFrameStateLayoutAcceptable:
        {
            WebDataSource *ds = [self dataSource];
            
            //LOG(Loading, "%@:  checking complete, current state WEBFRAMESTATE_COMMITTED", [self name]);
            if (![ds isLoading]) {
                id thisView = [self webView];
                NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
                ASSERT(thisDocumentView != nil);

                [self _setState: WebFrameStateComplete];

		// FIXME: need to avoid doing this in the non-HTML
		// case or the bridge may assert. Should make sure
		// there is a bridge/part in the proper state even for
		// non-HTML content.

                if ([ds isDocumentHTML]) {
		    [_private->bridge end];
		}

                // Unfortunately we have to get our parent to adjust the frames in this
                // frameset so this frame's geometry is set correctly.  This should
                // be a reasonably inexpensive operation.
                id parentDS = [[self parent] dataSource];
                if ([[parentDS _bridge] isFrameSet]){
                    id parentWebView = [[self parent] webView];
                    if ([parentWebView isDocumentHTML])
                        [[parentWebView documentView] _adjustFrames];
                }

                // Tell the just loaded document to layout.  This may be necessary
                // for non-html content that needs a layout message.
                [thisDocumentView setNeedsLayout: YES];
                [thisDocumentView layout];

                // Unfortunately if this frame has children we have to lay them
                // out too.  This could be an expensive operation.
                // FIXME:  If we can figure out how to avoid the layout of children,
                // (just need for iframe placement/sizing) we could get a few percent
                // speed improvement.
                [ds _layoutChildren];

                [thisDocumentView setNeedsDisplay: YES];
                //[thisDocumentView display];

                // If the user had a scroll point scroll to it.  This will override
                // the anchor point.  After much discussion it was decided by folks
                // that the user scroll point should override the anchor point.
                if ([[self controller] usesBackForwardList]){
                    switch ([self _loadType]) {
                    case WebFrameLoadTypeForward:
                    case WebFrameLoadTypeBack:
                    case WebFrameLoadTypeIndexedBackForward:
                    case WebFrameLoadTypeReload:
                        [self _restoreScrollPosition];
                        break;

                    case WebFrameLoadTypeStandard:
                    case WebFrameLoadTypeInternal:
                    case WebFrameLoadTypeReloadAllowingStaleData:
                        // Do nothing.
                        break;
                        
                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }

                [[[self controller] locationChangeDelegate] locationChangeDone: [ds mainDocumentError] forDataSource:ds];
 
                //if ([ds isDocumentHTML])
                //    [[ds representation] part]->closeURL();        
               
                return;
            }
            // A resource was loaded, but the entire frame isn't complete.  Schedule a
            // layout.
            else {
                if ([self _state] == WebFrameStateLayoutAcceptable) {
                    BOOL resourceTimedDelayEnabled = [[WebPreferences standardPreferences] _resourceTimedLayoutEnabled];
                    if (resourceTimedDelayEnabled) {
                        NSTimeInterval timedDelay = [[WebPreferences standardPreferences] _resourceTimedLayoutDelay];
                        [self _scheduleLayout: timedDelay];
                    }
                }
            }
            return;
        }
        
        case WebFrameStateComplete:
        {
            LOG(Loading, "%@:  checking complete, current state WebFrameStateComplete", [self name]);
            // Even if already complete, we might have set a previous item on a frame that
            // didn't do any data loading on the past transaction.  Make sure to clear these out.
            [_private setPreviousItem:nil];
            return;
        }
        
        // Yikes!  Serious horkage.
        default:
        {
	    ASSERT_NOT_REACHED();
        }
    }
}

+ (void)_recursiveCheckCompleteFromFrame: (WebFrame *)fromFrame
{
    int i, count;
    NSArray *childFrames;
    
    childFrames = [fromFrame children];
    count = [childFrames count];
    for (i = 0; i < count; i++) {
        WebFrame *childFrame;
        
        childFrame = [childFrames objectAtIndex: i];
        [WebFrame _recursiveCheckCompleteFromFrame: childFrame];
        [childFrame _isLoadComplete];
    }
    [fromFrame _isLoadComplete];
}

// Called every time a resource is completely loaded, or an error is received.
- (void)_checkLoadComplete
{
    ASSERT([self controller] != nil);

    // Now walk the frame tree to see if any frame that may have initiated a load is done.
    [WebFrame _recursiveCheckCompleteFromFrame: [[self controller] mainFrame]];
}

- (WebBridge *)_bridge
{
    return _private->bridge;
}

- (void)handleUnimplementablePolicy:(WebPolicy *)policy errorCode:(int)code forURL:(NSURL *)URL
{
    WebError *error = [WebError errorWithCode:code
                                     inDomain:WebErrorDomainWebKit
                                   failingURL:[URL absoluteString]];
    [[[self controller] policyDelegate] unableToImplementPolicy:policy error:error forURL:URL inFrame:self];    
}

- (BOOL)_shouldShowRequest:(WebResourceRequest *)request
{
    id <WebControllerPolicyDelegate> policyDelegate = [[self controller] policyDelegate];
    WebURLPolicy *URLPolicy = [policyDelegate URLPolicyForRequest:request inFrame:self];

    switch ([URLPolicy policyAction]) {
        case WebURLPolicyIgnore:
            return NO;

        case WebURLPolicyOpenExternally:
            if(![[NSWorkspace sharedWorkspace] openURL:[request URL]]){
                [self handleUnimplementablePolicy:URLPolicy errorCode:WebErrorCannotNotFindApplicationForURL forURL:[request URL]];
            }
            return NO;

        case WebURLPolicyUseContentPolicy:
            // handle non-file case first because it's short and sweet
            if (![[request URL] isFileURL]) {
                if (![WebResourceHandle canInitWithRequest:request]) {
                    [self handleUnimplementablePolicy:URLPolicy errorCode:WebErrorCannotShowURL forURL:[request URL]];
                    return NO;
                }
                return YES;
            } else {
                // file URL
                NSFileManager *fileManager = [NSFileManager defaultManager];
                NSString *path = [[request URL] path];
                NSString *type = [WebController _MIMETypeForFile: path];
                WebFileURLPolicy *fileURLPolicy = [policyDelegate fileURLPolicyForMIMEType:type andRequest:request inFrame:self];

                if([fileURLPolicy policyAction] == WebFileURLPolicyIgnore)
                    return NO;

		BOOL isDirectory;
		BOOL fileExists = [fileManager fileExistsAtPath:[[request URL] path] isDirectory:&isDirectory];

                if(!fileExists){
                    [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotFindFile forURL:[request URL]];
                    return NO;
                }

                if(![fileManager isReadableFileAtPath:path]){
                    [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotReadFile forURL:[request URL]];
                    return NO;
                }

                switch ([fileURLPolicy policyAction]) {
                    case WebFileURLPolicyUseContentPolicy:
                        if (isDirectory) {
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotShowDirectory forURL:[request URL]];
                            return NO;
                        } else if (![WebController canShowMIMEType: type]) {
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotShowMIMEType forURL:[request URL]];
                            return NO;
                        }
                        return YES;
                        
                    case WebFileURLPolicyOpenExternally:
                        if(![[NSWorkspace sharedWorkspace] openFile:path]){
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotFindApplicationForFile forURL:[request URL]];
                        }
                        return NO;

                    case WebFileURLPolicyRevealInFinder:
                        if(![[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:@""]){
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorFinderCannotOpenDirectory forURL:[request URL]];
                        }
                        return NO;

                    default:
                        [NSException raise:NSInvalidArgumentException format:
                @"fileURLPolicyForMIMEType:inFrame:isDirectory: returned WebFileURLPolicy with invalid action %d", [fileURLPolicy policyAction]];
                        return NO;
                }
            }

        default:
            [NSException raise:NSInvalidArgumentException format:@"URLPolicyForRequest: returned an invalid WebURLPolicy"];
            return NO;
    }
}

- (void)_setProvisionalDataSource:(WebDataSource *)d
{
    [_private setProvisionalDataSource:d];
}

// helper method that determines whether the subframes described by the item's subitems
// match our own current frameset
- (BOOL)_childFramesMatchItem:(WebHistoryItem *)item
{
    NSArray *childItems = [item children];
    int numChildItems = childItems ? [childItems count] : 0;
    int numChildFrames = _private->children ? [_private->children count] : 0;
    if (numChildFrames != numChildItems) {
        return NO;
    } else {
        int i;
        for (i = 0; i < numChildItems; i++) {
            NSString *itemTargetName = [[childItems objectAtIndex:i] target];
            //Search recursive here?
            if (![self _immediateChildFrameNamed:itemTargetName]) {
                return NO;	// couldn't match the i'th itemTarget
            }
        }
        return YES;		// found matches for all itemTargets
    }
}

// loads content into this frame, as specified by item
- (void)_loadItem:(WebHistoryItem *)item fromItem:(WebHistoryItem *)fromItem withLoadType: (WebFrameLoadType)type
{
    NSURL *itemURL = [item URL];
    WebResourceRequest *request;
    NSURL *currentURL = [[[self dataSource] request] URL];

    // Are we navigating to an anchor within the page?
    // Note if we have child frames we do a real reload, since the child frames might not
    // match our current frame structure, or they might not have the right content.  We could
    // check for all that as an additional optimization.
    if ([item anchor] &&
        [[itemURL _web_URLByRemovingFragment] isEqual: [currentURL _web_URLByRemovingFragment]] &&
        (!_private->children || ![_private->children count]))
    {
        [[_private->dataSource _bridge] scrollToAnchor: [item anchor]];
        [[[self controller] locationChangeDelegate] locationChangedWithinPageForDataSource:_private->dataSource];
    } else {
        request = [[WebResourceRequest alloc] initWithURL:itemURL];
    
        // set the request cache policy based on the type of request we have
        // however, allow any previously set value to take precendence
        if ([request requestCachePolicy] == WebRequestCachePolicyUseProtocolDefault) {
            switch (type) {
                case WebFrameLoadTypeStandard:
                    // if it's not a GET, reload from origin
                    // unsure whether this is the best policy
                    // other methods might be OK to get from the cache
                    if (![[request method] _web_isCaseInsensitiveEqualToString:@"GET"]) {
                        [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
                    }
                    break;
                case WebFrameLoadTypeReload:
                    [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
                    break;
                case WebFrameLoadTypeBack:
                case WebFrameLoadTypeForward:
                case WebFrameLoadTypeIndexedBackForward:
                    [request setRequestCachePolicy:WebRequestCachePolicyReturnCacheObjectLoadFromOriginIfNoCacheObject];
                    break;
                case WebFrameLoadTypeInternal:
                case WebFrameLoadTypeReloadAllowingStaleData:
                    // no-op: leave as protocol default
                    break;
                default:
                    ASSERT_NOT_REACHED();
            }
        }

        WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
        [request release];
        [self setProvisionalDataSource:newDataSource];
        // Remember this item so we can traverse any child items as child frames load
        [_private setProvisionalItem:item];
        [self _setLoadType:type];
        [self startLoading];
        [newDataSource release];
    }
}

// The general idea here is to traverse the frame tree and the item tree in parallel,
// tracking whether each frame already has the content the item requests.  If there is
// a match (by URL), we just restore scroll position and recurse.  Otherwise we must
// reload that frame, and all its kids.
- (void)_recursiveGoToItem:(WebHistoryItem *)item fromItem:(WebHistoryItem *)fromItem withLoadType:(WebFrameLoadType)type
{
    NSURL *itemURL = [item URL];
    NSURL *currentURL = [[[self dataSource] request] URL];

    // Always reload the target frame of the item we're going to.  This ensures that we will
    // do -some- load for the transition, which means a proper notification will be posted
    // to the app.
    // The exact URL has to match, including fragment.  We want to go through the _load
    // method, even if to do a within-page navigation.
    // The current frame tree and the frame tree snapshot in the item have to match.
    if (![item isTargetItem] &&
        [itemURL isEqual:currentURL] &&
        [_private->name isEqualToString:[item target]] &&
        [self _childFramesMatchItem:item])
    {
        // This content is good, so leave it alone and look for children that need reloading

        // Save form state (works from currentItem, since prevItem is nil)
        ASSERT(![_private previousItem]);
        [_private->bridge saveDocumentState];
        [self _saveScrollPositionToItem:[_private currentItem]];
        
        [_private setCurrentItem:item];

        // Restore form state (works from currentItem)
        [_private->bridge restoreDocumentState];
        // Restore the scroll position (taken in favor of going back to the anchor)
        [self _restoreScrollPosition];
        
        NSArray *childItems = [item children];
        int numChildItems = childItems ? [childItems count] : 0;
        int i;
        for (i = numChildItems - 1; i >= 0; i--) {
            WebHistoryItem *childItem = [childItems objectAtIndex:i];
            NSString *childName = [childItem target];
            WebHistoryItem *fromChildItem = [fromItem childItemWithName:childName];
            ASSERT(fromChildItem || [fromItem isTargetItem]);
            WebFrame *childFrame = [self _immediateChildFrameNamed:childName];
            ASSERT(childFrame);
            [childFrame _recursiveGoToItem:childItem fromItem:fromChildItem withLoadType:type];
        }
    } else {
        // We need to reload the content
        [self _loadItem:item fromItem:fromItem withLoadType:type];
    }
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
- (void)_goToItem: (WebHistoryItem *)item withLoadType: (WebFrameLoadType)type
{
    ASSERT(!_private->parent);
    WebBackForwardList *backForwardList = [[self controller] backForwardList];
    WebHistoryItem *currItem = [backForwardList currentEntry];
    // Set the BF cursor before commit, which lets the user quickly click back/forward again.
    // - plus, it only makes sense for the top level of the operation through the frametree,
    // as opposed to happening for some/one of the page commits that might happen soon
    [backForwardList goToEntry:item];
    [self _recursiveGoToItem:item fromItem:currItem withLoadType:type];
}

- (void)_loadRequest:(WebResourceRequest *)request
{
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    if ([self setProvisionalDataSource:newDataSource]) {
        [self startLoading];
    }
    [newDataSource release];
}

-(NSDictionary *)_actionInformationForNavigationType:(WebNavigationType)navigationType event:(NSEvent *)event
{

    switch (navigationType) {
    case WebNavigationTypeLinkClicked:
    case WebNavigationTypeFormSubmitted:
	;

	NSView *topViewInEventWindow = [[event window] contentView];
	NSView *viewContainingPoint = [topViewInEventWindow hitTest:[topViewInEventWindow convertPoint:[event locationInWindow] fromView:nil]];

	ASSERT(viewContainingPoint != nil);
	ASSERT([viewContainingPoint isKindOfClass:[WebHTMLView class]]);
	    
	NSPoint point = [viewContainingPoint convertPoint:[event locationInWindow] fromView:nil];
	NSDictionary *elementInfo = [(WebHTMLView *)viewContainingPoint _elementAtPoint:point];
	
	return [NSDictionary dictionaryWithObjectsAndKeys:
			     [NSNumber numberWithInt:navigationType], WebActionNavigationTypeKey,
			     elementInfo, WebActionElementKey,
			     [NSNumber numberWithInt:[event type]], WebActionButtonKey,
			     [NSNumber numberWithInt:[event modifierFlags]], WebActionModifierFlagsKey,
			     nil];


	break;
    case WebNavigationTypeOther:
	return [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:navigationType]
			     forKey:WebActionNavigationTypeKey];

	break;
    default:
	ASSERT_NOT_REACHED();
	return nil;
    }
}

-(BOOL)_continueAfterClickPolicyForEvent:(NSEvent *)event request:(WebResourceRequest *)request
{
    WebController *controller = [self controller];
    WebClickPolicy *clickPolicy;

    clickPolicy = [[controller policyDelegate] clickPolicyForAction:[self _actionInformationForNavigationType:WebNavigationTypeLinkClicked event:event]
					       andRequest:request
					       inFrame:self];

    WebPolicyAction clickAction = [clickPolicy policyAction];

    switch (clickAction) {
    case WebClickPolicyShow:
	return YES;
    case WebClickPolicyOpenExternally:
	if(![[NSWorkspace sharedWorkspace] openURL:[request URL]]){
	    [self handleUnimplementablePolicy:clickPolicy errorCode:WebErrorCannotNotFindApplicationForURL forURL:[request URL]];
	}
	break;
    case WebClickPolicyOpenNewWindow:
	[controller _openNewWindowWithRequest:request behind:NO];
	break;
    case WebClickPolicyOpenNewWindowBehind:
	[controller _openNewWindowWithRequest:request behind:YES];
	break;
    case WebClickPolicySave:
	[controller _downloadURL:[request URL]];
	break;
    case WebClickPolicyIgnore:
	break;
    default:
	[NSException raise:NSInvalidArgumentException
		     format:@"clickPolicyForElement:button:modifierFlags: returned an invalid WebClickPolicy"];
    }
    return NO;
}

// main funnel for navigating via callback from WebCore (e.g., clicking a link, redirect)
- (void)_loadURL:(NSURL *)URL loadType:(WebFrameLoadType)loadType clientRedirect:(BOOL)clientRedirect triggeringEvent:(NSEvent *)event
{
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setReferrer:[_private->bridge referrer]];
    if (loadType == WebFrameLoadTypeReload) {
	[request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
    }

    if (event != nil && ![self _continueAfterClickPolicyForEvent:event request:request]) {
	return;
    }

    // FIXME: This logic doesn't exactly match what KHTML does in openURL, so it's possible
    // this will screw up in some cases involving framesets.
    if (loadType != WebFrameLoadTypeReload && [URL fragment] && [[URL _web_URLByRemovingFragment] isEqual:[[_private->bridge URL] _web_URLByRemovingFragment]]) {
        // Just do anchor navigation within the existing content.  Note we only do this if there is
        // an anchor in the URL - otherwise this check might prevent us from reloading a document
        // that has subframes that are different than what we're displaying (in other words, a link
        // from within a frame is trying to reload the frameset into _top).
        WebDataSource *dataSrc = [self dataSource];

        // save scroll position before we open URL, which will jump to anchor
        [self _saveScrollPositionToItem:[_private currentItem]];

        // ???Might need to save scroll-state, form-state for all other frames

        ASSERT(![_private previousItem]);
        // will save form state to current item, since prevItem not set
        [_private->bridge openURL:URL];
        [dataSrc _setURL:URL];
        // NB: must happen after _setURL, since we add based on the current request
        [self _addBackForwardItemClippedAtTarget:NO];
        // This will clear previousItem from the rest of the frame tree tree that didn't
        // doing any loading.  We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state
        [self _checkLoadComplete];
        [[[self controller] locationChangeDelegate] locationChangedWithinPageForDataSource:dataSrc];
    } else {
        WebFrameLoadType previousLoadType = [self _loadType];
        WebDataSource *oldDataSource = [[self dataSource] retain];

        [self _loadRequest:request];
        // NB: must be done after loadRequest:, which sets the provDataSource, which
        //     inits the load type to Standard
        [self _setLoadType:loadType];
        if (clientRedirect) {
            // Inherit the loadType from the operation that spawned the redirect
            [self _setLoadType:previousLoadType];

            // need to transfer BF items from the dataSource that we're replacing
            WebDataSource *newDataSource = [self provisionalDataSource];
            [newDataSource _setIsClientRedirect:YES];
            [newDataSource _addBackForwardItems:[oldDataSource _backForwardItems]];
        }
        [request release];
        [oldDataSource release];
    }
}

- (void)_loadURL:(NSURL *)URL intoChild:(WebFrame *)childFrame
{
    //WebDataSource *dataSrc = [self dataSource];
    WebHistoryItem *parentItem = [_private currentItem];
    NSArray *childItems = [parentItem children];
    WebFrameLoadType loadType = [self _loadType];
    WebFrameLoadType childLoadType = WebFrameLoadTypeInternal;
    WebHistoryItem *childItem = nil;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    if ((loadType == WebFrameLoadTypeForward || 
        loadType == WebFrameLoadTypeBack ||
        loadType == WebFrameLoadTypeIndexedBackForward ||
        loadType == WebFrameLoadTypeReload) && childItems)
    {
        childItem = [parentItem childItemWithName:[childFrame name]];
        if (childItem) {
            URL = [childItem URL];
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;
        }
    }
            
    [childFrame _loadURL:URL loadType:childLoadType clientRedirect:NO triggeringEvent:nil];
    // want this here???
    if (childItem) {
        if (loadType != WebFrameLoadTypeReload) {
            // For back/forward, remember this item so we can traverse any child items as child frames load
            [childFrame->_private setProvisionalItem:childItem];
        } else {
            // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
            [childFrame->_private setCurrentItem:childItem];
        }
    }
}

- (void)_postWithURL:(NSURL *)URL data:(NSData *)data contentType:(NSString *)contentType
{
    // When posting, use the WebResourceHandleFlagLoadFromOrigin load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.
    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    [request setRequestCachePolicy:WebRequestCachePolicyLoadFromOrigin];
    [request setMethod:@"POST"];
    [request setData:data];
    [request setContentType:contentType];
    [request setReferrer:[_private->bridge referrer]];
    [self _loadRequest:request];
    [request release];
}

- (void)_saveScrollPositionToItem:(WebHistoryItem *)item
{
    if (item) {
        NSView *clipView = [[[self webView] documentView] superview];
        // we might already be detached when this is called from detachFromParent, in which
        // case we don't want to override real data earlier gathered with (0,0)
        if (clipView) {
            [item setScrollPoint:[clipView bounds].origin];
        }
    }
}

- (void)_restoreScrollPosition
{
    WebHistoryItem *entry = [_private currentItem];
    ASSERT(entry);
    [[[self webView] documentView] scrollPoint:[entry scrollPoint]];
}

- (void)_scrollToTop
{
    [[[self webView] documentView] scrollPoint: NSZeroPoint];
}

- (void)_textSizeMultiplierChanged
{
    [_private->bridge setTextSizeMultiplier:[[self controller] textSizeMultiplier]];
    [[self children] makeObjectsPerformSelector:@selector(_textSizeMultiplierChanged)];
}

- (void)_defersCallbacksChanged
{
    [[self provisionalDataSource] _defersCallbacksChanged];
    [[self dataSource] _defersCallbacksChanged];
}

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding
{
    WebDataSource *dataSource = [self dataSource];
    if (dataSource == nil) {
	return;
    }

    WebResourceRequest *request = [[dataSource request] copy];
    [request setRequestCachePolicy:WebRequestCachePolicyReturnCacheObjectLoadFromOriginIfNoCacheObject];
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    [request release];
    
    [newDataSource _setOverrideEncoding:encoding];
    
    if ([self setProvisionalDataSource:newDataSource]) {
	[self _setLoadType:WebFrameLoadTypeReloadAllowingStaleData];
        [self startLoading];
    }
    
    [newDataSource release];
}

- (void)_addChild:(WebFrame *)child
{
    if (_private->children == nil)
        _private->children = [[NSMutableArray alloc] init];
    [_private->children addObject:child];

    child->_private->parent = self;
    [[child _bridge] setParent:_private->bridge];
    [[child dataSource] _setOverrideEncoding:[[self dataSource] _overrideEncoding]];   
}

- (WebPluginController *)_pluginController
{
    if(!_private->pluginController){
        _private->pluginController = [[WebPluginController alloc] initWithWebFrame:self];
    }

    return _private->pluginController;
}

- (void)_addFramePathToString:(NSMutableString *)path
{
    if ([_private->name hasPrefix:@"<!--framePath "]) {
        // we have a generated name - take the path from our name
        NSRange ourPathRange = {14, [_private->name length] - 14 - 3};
        [path appendString:[_private->name substringWithRange:ourPathRange]];
    } else {
        // we have a generated name - just add our simple name to the end
        if (_private->parent) {
            [_private->parent _addFramePathToString:path];
        }
        [path appendString:@"/"];
        [path appendString:_private->name];
    }
}

// Generate a repeatable name for a child about to be added to us.  The name must be
// unique within the frame tree.  The string we generate includes a "path" of names
// from the root frame down to us.  For this path to be unique, each set of siblings must
// contribute a unique name to the path, which can't collide with any HTML-assigned names.
// We generate this path component by index in the child list along with an unlikely frame name.
- (NSString *)_generateFrameName
{
    NSMutableString *path = [NSMutableString stringWithCapacity:256];
    [path insertString:@"<!--framePath " atIndex:0];
    [self _addFramePathToString:path];
    // The new child's path component is all but the 1st char and the last 3 chars
    [path appendFormat:@"/<!--frame%d-->-->", _private->children ? [_private->children count] : 0];
    return path;
}

- (WebHistoryItem *)_itemForSavingDocState
{
    // For a standard page load, we will have a previous item set, which will be used to
    // store the form state.  However, in some cases we will have no previous item, and
    // the current item is the right place to save the state.  One example is when we
    // detach a bunch of frames because we are navigating from a site with frames to
    // another site.  Another is when saving the frame state of a frame that is not the
    // target of the current navigation (if we even decide to save with that granularity).

    // Because of previousItem's "masking" of currentItem for this purpose, it's important
    // that previousItem be cleared at the end of a page transition.  We leverage the
    // checkLoadComplete recursion to achieve this goal.

    WebHistoryItem *result = [_private previousItem] ? [_private previousItem] : [_private currentItem];
    return result;
}

- (WebHistoryItem *)_itemForRestoringDocState
{
    return [_private currentItem];
}

@end
