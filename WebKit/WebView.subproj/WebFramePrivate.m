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
    
    return self;
}

- (void)dealloc
{
    [scheduledLayoutTimer invalidate];
    [scheduledLayoutTimer release];
    
    [webView _setController:nil];
    [dataSource _setController:nil];
    [provisionalDataSource _setController:nil];

    [bridge release];
    [name release];
    [webView release];
    [dataSource release];
    [provisionalDataSource release];
    [children release];
    
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


@end

@implementation WebFrame (WebPrivate)

- (void)_controllerWillBeDeallocated
{
    [self _detachFromParent];
}

- (void)_detachFromParent
{
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
}

- (void)_setController: (WebController *)controller
{
    [_private setController:controller];
}

- (void)_setDataSource:(WebDataSource *)ds
{
    ASSERT(ds != _private->dataSource);
    
    if ([_private->dataSource isDocumentHTML] && ![ds isDocumentHTML]) {
        [[self _bridge] removeFromFrame];
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
    LOG(Timing, "%s:  state = %s", [[self name] cString], stateNames[_private->state]);
    
    [_private->scheduledLayoutTimer release];
    _private->scheduledLayoutTimer = nil;
    
    if (_private->state == WebFrameStateLayoutAcceptable) {
        NSView <WebDocumentView> *documentView = [[self webView] documentView];
        
        if ([self controller])
            LOG(Timing, "%s:  performing timed layout, %f seconds since start of document load", [[self name] cString], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
            
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
            LOG(Timing, "%s:  NOT performing timed layout (not needed), %f seconds since start of document load", [[self name] cString], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
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
    NSView <WebDocumentView> *documentView;
    WebHistoryItem *backForwardItem;
    WebBackForwardList *backForwardList = [[self controller] backForwardList];
    WebFrame *parentFrame;
    
    documentView = [[self webView] documentView];

    switch ([self _state]) {
    	case WebFrameStateProvisional:
        {
	    ASSERT(documentView != nil);

            // Set the committed data source on the frame.
            [self _setDataSource:_private->provisionalDataSource];
            [_private setProvisionalDataSource: nil];

            [self _setState: WebFrameStateCommittedPage];
        
            // Handle adding the URL to the back/forward list.
            WebDataSource *ds = [self dataSource];
            WebHistoryItem *entry = nil;
            NSString *ptitle = [ds pageTitle];

            if ([[self controller] useBackForwardList]){
                switch ([self _loadType]) {
                case WebFrameLoadTypeForward:
                    [backForwardList goForward];
                    [self _restoreScrollPosition];
                    break;
    
                case WebFrameLoadTypeIntermediateBack:
                    break;
                    
                case WebFrameLoadTypeBack:
                    [backForwardList goBack];
                    [self _restoreScrollPosition];
                    break;
                    
                case WebFrameLoadTypeReload:
                    [self _scrollToTop];
                    break;
    
                case WebFrameLoadTypeStandard:
                    // Add item to history.
                    entry = [[WebHistory sharedHistory] addEntryForURL: [[[ds request] URL] _web_canonicalize]];
                    if (ptitle)
                        [entry setTitle: ptitle];
                
                    // Add item to back/forward list.
                    parentFrame = [self parent]; 
                    backForwardItem = [[WebHistoryItem alloc] initWithURL:[[[self dataSource] request] URL]
                                                                   target:[self name]
                                                                   parent:[parentFrame name]
                                                                    title:[[self dataSource] pageTitle]];
                    [[[self controller] backForwardList] addEntry: backForwardItem];
                    [backForwardItem release];
                    // Scroll to top.
                    break;
    
                case WebFrameLoadTypeInternal:
                    // Do nothing, this was a frame/iframe non user load.
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
            if (ptitle){
                [entry setTitle: ptitle];
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
    LOG(Loading, "%s:  transition from %s to %s", [[self name] cString], stateNames[_private->state], stateNames[newState]);
    if ([self controller])
        LOG(Timing, "%s:  transition from %s to %s, %f seconds since start of document load", [[self name] cString], stateNames[_private->state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && self == [[self controller] mainFrame]){
        LOG(DocumentLoad, "completed %s (%f seconds)", [[[[[self dataSource] request] URL] absoluteString] cString], CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
    }
    
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithInt:_private->state], WebPreviousFrameState,
                    [NSNumber numberWithInt:newState], WebCurrentFrameState, nil];
                    
    [[NSNotificationCenter defaultCenter] postNotificationName:WebFrameStateChangedNotification object:self userInfo:userInfo];
    
    _private->state = newState;
    
    if (_private->state == WebFrameStateProvisional){
        [[[self webView] frameScrollView] setDrawsBackground: NO];
    }
    
    if (_private->state == WebFrameStateComplete){
        NSScrollView *sv = [[self webView] frameScrollView];
        [sv setDrawsBackground: YES];
        [[sv contentView] setCopiesOnScroll: YES];
        [_private->scheduledLayoutTimer fire];
   	ASSERT(_private->scheduledLayoutTimer == nil);
    }
}

- (void)_isLoadComplete
{
    ASSERT([self controller] != nil);

    switch ([self _state]) {
        case WebFrameStateProvisional:
        {
            WebDataSource *pd = [self provisionalDataSource];
            
            LOG(Loading, "%s:  checking complete in WebFrameStateProvisional", [[self name] cString]);
            // If we've received any errors we may be stuck in the provisional state and actually
            // complete.
            if ([[pd errors] count] != 0 || [pd mainDocumentError]) {
                // Check all children first.
                LOG(Loading, "%s:  checking complete, current state WebFrameStateProvisional, %d errors", [[self name] cString], [[pd errors] count]);
                if (![pd isLoading]) {
                    LOG(Loading, "%s:  checking complete in WebFrameStateProvisional, load done", [[self name] cString]);

                    [[[self controller] locationChangeDelegate] locationChangeDone: [pd mainDocumentError] forDataSource:pd];

                    // We now the provisional data source didn't cut the mustard, release it.
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
            
            //LOG(Loading, "%s:  checking complete, current state WEBFRAMESTATE_COMMITTED", [[self name] cString]);
            if (![ds isLoading]) {
                id thisView = [self webView];
                NSView <WebDocumentView> *thisDocumentView = [thisView documentView];

                [self _setState: WebFrameStateComplete];

		// FIXME: need to avoid doing this in the non-HTML
		// case or the bridge may assert. Should make sure
		// there is a bridge/part in the proper state even for
		// non-HTML content.

                if ([ds isDocumentHTML]) {
		    [[self _bridge] end];
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
                if ([[self controller] useBackForwardList]){
                    switch ([self _loadType]) {
                    case WebFrameLoadTypeForward:
                        [self _restoreScrollPosition];
                        break;
        
                    case WebFrameLoadTypeIntermediateBack:
                        [[self controller] goBack];
                        break;
                        
                    case WebFrameLoadTypeBack:
                        [self _restoreScrollPosition];
                        break;
                        
                    case WebFrameLoadTypeReload:
                        [self _scrollToTop];
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
            LOG(Loading, "%s:  checking complete, current state WebFrameStateComplete", [[self name] cString]);
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

- (BOOL)_shouldShowURL:(NSURL *)URL
{
    id <WebControllerPolicyDelegate> policyDelegate = [[self controller] policyDelegate];
    WebURLPolicy *URLPolicy = [policyDelegate URLPolicyForURL:URL inFrame:self];

    switch ([URLPolicy policyAction]) {
        case WebURLPolicyIgnore:
            return NO;

        case WebURLPolicyOpenExternally:
            if(![[NSWorkspace sharedWorkspace] openURL:URL]){
                [self handleUnimplementablePolicy:URLPolicy errorCode:WebErrorCannotNotFindApplicationForURL forURL:URL];
            }
            return NO;

        case WebURLPolicyUseContentPolicy:
            // handle non-file case first because it's short and sweet
            if (![URL isFileURL]) {
                if (![WebResourceHandle canInitWithRequest:[WebResourceRequest requestWithURL:URL]]) {
                    [self handleUnimplementablePolicy:URLPolicy errorCode:WebErrorCannotShowURL forURL:URL];
                    return NO;
                }
                return YES;
            } else {
                // file URL
                NSFileManager *fileManager = [NSFileManager defaultManager];
                NSString *path = [URL path];
                BOOL isDirectory;
                BOOL fileExists = [fileManager fileExistsAtPath:path isDirectory:&isDirectory];
                NSString *type = [WebController _MIMETypeForFile: path];
                WebFileURLPolicy *fileURLPolicy = [policyDelegate fileURLPolicyForMIMEType:type inFrame:self isDirectory:isDirectory];

                if([fileURLPolicy policyAction] == WebFileURLPolicyIgnore)
                    return NO;

                if(!fileExists){
                    [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotFindFile forURL:URL];
                    return NO;
                }

                if(![fileManager isReadableFileAtPath:path]){
                    [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotReadFile forURL:URL];
                    return NO;
                }

                switch ([fileURLPolicy policyAction]) {
                    case WebFileURLPolicyUseContentPolicy:
                        if (isDirectory) {
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotShowDirectory forURL:URL];
                            return NO;
                        } else if (![WebController canShowMIMEType: type]) {
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotShowMIMEType forURL:URL];
                            return NO;
                        }
                        return YES;
                        
                    case WebFileURLPolicyOpenExternally:
                        if(![[NSWorkspace sharedWorkspace] openFile:path]){
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorCannotFindApplicationForFile forURL:URL];
                        }
                        return NO;

                    case WebFileURLPolicyRevealInFinder:
                        if(![[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:@""]){
                            [self handleUnimplementablePolicy:fileURLPolicy errorCode:WebErrorFinderCannotOpenDirectory forURL:URL];
                        }
                        return NO;

                    default:
                        [NSException raise:NSInvalidArgumentException format:
                @"fileURLPolicyForMIMEType:inFrame:isDirectory: returned WebFileURLPolicy with invalid action %d", [fileURLPolicy policyAction]];
                        return NO;
                }
            }

        default:
            [NSException raise:NSInvalidArgumentException format:@"URLPolicyForURL: returned an invalid WebURLPolicy"];
            return NO;
    }
}

- (void)_setProvisionalDataSource:(WebDataSource *)d
{
    [_private setProvisionalDataSource:d];
}


- (void)_goToItem: (WebHistoryItem *)item withFrameLoadType: (WebFrameLoadType)type
{
    NSURL *itemURL = [item URL];
    WebResourceRequest *request;
    WebDataSource *dataSource;
    NSURL *originalURL = [[[self dataSource] request] URL];
    
    if ([item anchor] && [[itemURL _web_URLByRemovingFragment] isEqual: [originalURL _web_URLByRemovingFragment]]) {
        WebBackForwardList *backForwardList = [[self controller] backForwardList];

        if (type == WebFrameLoadTypeForward)
            [backForwardList goForward];
        else if (type == WebFrameLoadTypeBack)
            [backForwardList goBack];
        else 
            [NSException raise:NSInvalidArgumentException format:@"WebFrameLoadType incorrect"];
        [[_private->dataSource _bridge] scrollToAnchor: [item anchor]];
    }
    else {
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
                case WebFrameLoadTypeIndexedBack:
                case WebFrameLoadTypeIndexedForward:
                case WebFrameLoadTypeIntermediateBack:
                    [request setRequestCachePolicy:WebRequestCachePolicyReturnCacheObjectLoadFromOriginIfNoCacheObject];
                    break;
                case WebFrameLoadTypeInternal:
                case WebFrameLoadTypeReloadAllowingStaleData:
                    // no-op: leave as protocol default
                    break;
            }
        }
        
        dataSource = [[WebDataSource alloc] initWithRequest:request];
        [request release];
        [self setProvisionalDataSource: dataSource];
        [self _setLoadType: type];
        [self startLoading];
        [dataSource release];
    }
}

- (void)_restoreScrollPosition
{
    WebHistoryItem *entry;

    entry = (WebHistoryItem *)[[[self controller] backForwardList] currentEntry];
    [[[self webView] documentView] scrollPoint: [entry scrollPoint]];
}

- (void)_scrollToTop
{
    NSPoint origin;

    origin.x = origin.y = 0.0;
    [[[self webView] documentView] scrollPoint: origin];
}

- (void)_textSizeMultiplierChanged
{
    [[self _bridge] setTextSizeMultiplier:[[self controller] textSizeMultiplier]];
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
    [[child dataSource] _setOverrideEncoding:[[self dataSource] _overrideEncoding]];   
}

@end
