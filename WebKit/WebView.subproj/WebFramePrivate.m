/*	
    WebFramePrivate.mm
	    
    Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebFramePrivate.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebFoundation.h>

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
    
    [webView _setController: nil];
    [dataSource _setController: nil];
    [provisionalDataSource _setController: nil];

    [bridge release];
    [name release];
    [webView release];
    [dataSource release];
    [provisionalDataSource release];
    
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

- (void)_parentDataSourceWillBeDeallocated
{
    [_private setController:nil];
    [_private->dataSource _setParent:nil];
    [_private->provisionalDataSource _setParent:nil];
}

- (void)_setController: (WebController *)controller
{
    [_private setController: controller];
}

- (void)_setDataSource: (WebDataSource *)ds
{
    [_private setDataSource: ds];
    [ds _setController: [self controller]];
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
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  state = %s\n", [[self name] cString], stateNames[_private->state]);
    
    [_private->scheduledLayoutTimer release];
    _private->scheduledLayoutTimer = nil;
    
    if (_private->state == WebFrameStateLayoutAcceptable) {
        NSView <WebDocumentView> *documentView = [[self webView] documentView];
        
        if ([self controller])
            WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  performing timed layout, %f seconds since start of document load\n", [[self name] cString], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
            
        if ([[self webView] isDocumentHTML]) {
            WebHTMLView *htmlView = (WebHTMLView *)documentView;
            
            [htmlView setNeedsLayout: YES];
            
            NSRect frame = [htmlView frame];
            
            if (frame.size.width == 0 || frame.size.height == 0){
                // We must do the layout now, rather than depend on
                // display to do a lazy layout because the view
                // may be recently initialized with a zero size
                // and the AppKit will optimize out any drawing.
                
                // Force a layout now.  At this point we could
                // check to see if any CSS is pending and delay
                // the layout further to avoid the flash of unstyled
                // content.                    
                [htmlView layout];
            }
        }
            
        [documentView setNeedsDisplay: YES];
    }
    else {
        if ([self controller])
            WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  NOT performing timed layout (not needed), %f seconds since start of document load\n", [[self name] cString], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
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
                    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "performing early layout because commit time, %f, exceeded initial layout interval %f\n", timeSinceStart, defaultTimedDelay);
                    [self _timedLayout: nil];
                }
                else {
                    NSTimeInterval timedDelay = defaultTimedDelay - timeSinceStart;
                    
                    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "registering delayed layout after %f seconds, time since start %f\n", timedDelay, timeSinceStart);
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
	    WEBKIT_ASSERT_NOT_REACHED();
        }
    }
}


- (void)_transitionToCommitted
{
    WEBKIT_ASSERT ([self controller] != nil);
    NSView <WebDocumentView> *documentView;
    WebHistoryItem *backForwardItem;
    WebBackForwardList *backForwardList = [[self controller] backForwardList];
    WebFrame *parentFrame;
    
    documentView = [[self webView] documentView];

    switch ([self _state]) {
    	case WebFrameStateProvisional:
        {
	    WEBKIT_ASSERT (documentView != nil);

            // Set the committed data source on the frame.
            [self _setDataSource: _private->provisionalDataSource];

            // Now that the provisional data source is committed, release it.
            [_private setProvisionalDataSource: nil];
        
            [self _setState: WebFrameStateCommittedPage];
        
            // Handle adding the URL to the back/forward list.
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
                    
                case WebFrameLoadTypeRefresh:
                    [self _scrollToTop];
                    break;
    
                case WebFrameLoadTypeStandard:
                    parentFrame = [[self controller] frameForDataSource: [[self dataSource] parent]]; 
                    backForwardItem = [[WebHistoryItem alloc] initWithURL:[[self dataSource] originalURL] target: [self name] parent: [parentFrame name] title:[[self dataSource] pageTitle] image: nil];
                    [[[self controller] backForwardList] addEntry: backForwardItem];
                    [backForwardItem release];
                    // Scroll to top.
                    break;
    
                case WebFrameLoadTypeInternal:
                    // Do nothing, this was a frame/iframe non user load.
                    break;
                    
                // FIXME Remove this check when dummy ds is removed.  An exception should be thrown
                // if we're in the WebFrameLoadTypeUninitialized state.
                case WebFrameLoadTypeUninitialized:
                default:
		    WEBKIT_ASSERT_NOT_REACHED();
                }
            }
            
            // Tell the client we've committed this URL.
	    [[[self controller] locationChangeHandler] locationChangeCommittedForDataSource:[self dataSource]];
            
            // If we have a title let the controller know about it.
            if ([[self dataSource] pageTitle])
		[[[self controller] locationChangeHandler] receivedPageTitle:[[self dataSource] pageTitle] forDataSource:[self dataSource]];
            break;
        }
        
        case WebFrameStateCommittedPage:
        case WebFrameStateLayoutAcceptable:
        case WebFrameStateComplete:
        default:
        {
	    WEBKIT_ASSERT_NOT_REACHED();
        }
    }
}

- (WebFrameState)_state
{
    return _private->state;
}

- (void)_setState: (WebFrameState)newState
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  transition from %s to %s\n", [[self name] cString], stateNames[_private->state], stateNames[newState]);
    if ([self controller])
        WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  transition from %s to %s, %f seconds since start of document load\n", [[self name] cString], stateNames[_private->state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && self == [[self controller] mainFrame]){
        WEBKITDEBUGLEVEL (WEBKIT_LOG_DOCUMENTLOAD, "completed %s (%f seconds)", [[[[self dataSource] originalURL] absoluteString] cString], CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
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
    }
}

- (void)_isLoadComplete
{
    WEBKIT_ASSERT ([self controller] != nil);

    switch ([self _state]) {
        case WebFrameStateProvisional:
        {
            WebDataSource *pd = [self provisionalDataSource];
            
            WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete in WebFrameStateProvisional\n", [[self name] cString]);
            // If we've received any errors we may be stuck in the provisional state and actually
            // complete.
            if ([[pd errors] count] != 0 || [pd mainDocumentError]) {
                // Check all children first.
                WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete, current state WebFrameStateProvisional, %d errors\n", [[self name] cString], [[pd errors] count]);
                if (![pd isLoading]) {
                    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete in WebFrameStateProvisional, load done\n", [[self name] cString]);

                    [[[self controller] locationChangeHandler] locationChangeDone: [pd mainDocumentError] forDataSource:pd];

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
            
            //WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete, current state WEBFRAMESTATE_COMMITTED\n", [[self name] cString]);
            if (![ds isLoading]) {
                id thisView = [self webView];
                NSView <WebDocumentView> *thisDocumentView = [thisView documentView];

                [self _setState: WebFrameStateComplete];

		// FIXME: need to avoid doing this in the non-HTML
		// case or the bridge may assert. Should make sure
		// there is a bridge/part in the proper state even for
		// non-HTML content.

                if ([ds isDocumentHTML]) {
		    [[ds _bridge] end];
		}

                // Unfortunately we have to get our parent to adjust the frames in this
                // frameset so this frame's geometry is set correctly.  This should
                // be a reasonably inexpensive operation.
                id parentDS = [[[ds parent] webFrame] dataSource];
                if ([[parentDS _bridge] isFrameSet]){
                    id parentWebView = [[[ds parent] webFrame] webView];
                    if ([parentWebView isDocumentHTML])
                        [[parentWebView documentView] _adjustFrames];
                }

                // Tell the just loaded document to layout.  This may be necessary
                // for non-html content that needs a layout message.
                if ([thisView isDocumentHTML]){
                    WebHTMLView *hview = (WebHTMLView *)thisDocumentView;
                    [hview setNeedsLayout: YES];
                }
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
                        
                    case WebFrameLoadTypeRefresh:
                        [self _scrollToTop];
                        break;
        
                    case WebFrameLoadTypeStandard:
                    case WebFrameLoadTypeInternal:
                        // Do nothing.
                        break;
                        
                    // FIXME Remove this check when dummy ds is removed.  An exception should be thrown
                    // if we're in the WebFrameLoadTypeUninitialized state.
                    case WebFrameLoadTypeUninitialized:
                        break;
                        
                    default:
                        [[NSException exceptionWithName:NSGenericException reason:@"invalid load type during commit transition" userInfo: nil] raise];
                        break;
                    }
                }

                [[[self controller] locationChangeHandler] locationChangeDone: [ds mainDocumentError] forDataSource:ds];
 
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
            WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete, current state WebFrameStateComplete\n", [[self name] cString]);
            return;
        }
        
        // Yikes!  Serious horkage.
        default:
        {
	    WEBKIT_ASSERT_NOT_REACHED();
        }
    }
}

+ (void)_recursiveCheckCompleteFromFrame: (WebFrame *)fromFrame
{
    int i, count;
    NSArray *childFrames;
    
    childFrames = [[fromFrame dataSource] children];
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

    WEBKIT_ASSERT ([self controller] != nil);

    // Now walk the frame tree to see if any frame that may have initiated a load is done.
    [WebFrame _recursiveCheckCompleteFromFrame: [[self controller] mainFrame]];
}

- (WebBridge *)_bridge
{
    return _private->bridge;
}

- (BOOL)_shouldShowDataSource:(WebDataSource *)dataSource
{
    id <WebControllerPolicyHandler> policyHandler = [[self controller] policyHandler];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *URL = [dataSource originalURL];
    WebFileURLPolicy *fileURLPolicy;
    NSString *path = [URL path], *URLString = [URL absoluteString];
    BOOL isDirectory, fileExists;
    WebError *error;
    
    WebURLPolicy *URLPolicy = [policyHandler URLPolicyForURL:URL];
    
    if([URLPolicy policyAction] == WebURLPolicyUseContentPolicy){
                    
        if([URL isFileURL]){
        
            fileExists = [fileManager fileExistsAtPath:path isDirectory:&isDirectory];
            
            NSString *type = [WebController _MIMETypeForFile: path];
                
            if(isDirectory){
                fileURLPolicy = [policyHandler fileURLPolicyForMIMEType: nil dataSource: dataSource isDirectory:YES];
            }else{
                fileURLPolicy = [policyHandler fileURLPolicyForMIMEType: type dataSource: dataSource isDirectory:NO];
            }
            
            if([fileURLPolicy policyAction] == WebFileURLPolicyIgnore)
                return NO;
            
            if(!fileExists){
                error = [WebError errorWithCode:WebErrorFileDoesNotExist inDomain:WebErrorDomainWebKit failingURL:URLString];
                [policyHandler unableToImplementFileURLPolicy: fileURLPolicy error: error forDataSource: dataSource];
                return NO;
            }
            
            if(![fileManager isReadableFileAtPath:path]){
                error = [WebError errorWithCode:WebErrorFileNotReadable  inDomain:WebErrorDomainWebKit failingURL:URLString];
                [policyHandler unableToImplementFileURLPolicy: fileURLPolicy error: error forDataSource: dataSource];
                return NO;
            }
            
            if([fileURLPolicy policyAction] == WebFileURLPolicyUseContentPolicy){
                if(isDirectory){
                    error = [WebError errorWithCode:WebErrorCannotShowDirectory  inDomain:WebErrorDomainWebKit failingURL:URLString];
                    [policyHandler unableToImplementFileURLPolicy: fileURLPolicy error: error forDataSource: dataSource];

                    return NO;
                }
                else if(![WebController canShowMIMEType: type]){
                    error = [WebError errorWithCode:WebErrorCannotShowMIMEType  inDomain:WebErrorDomainWebKit failingURL:URLString];
                    [policyHandler unableToImplementFileURLPolicy: fileURLPolicy error: error forDataSource: dataSource];
                    return NO;
                }else{
                    // File exists, its readable, we can show it
                    return YES;
                }
            }else if([fileURLPolicy policyAction] == WebFileURLPolicyOpenExternally){
                if(![workspace openFile:path]){
                    error = [WebError errorWithCode:WebErrorCouldNotFindApplicationForFile  inDomain:WebErrorDomainWebKit failingURL:URLString];
                    [policyHandler unableToImplementFileURLPolicy: fileURLPolicy error: error forDataSource: dataSource];
                }
                return NO;
            }else if([fileURLPolicy policyAction] == WebFileURLPolicyRevealInFinder){
                if(![workspace selectFile:path inFileViewerRootedAtPath:@""]){
                        error = [WebError errorWithCode:WebErrorFinderCouldNotOpenDirectory  inDomain:WebErrorDomainWebKit failingURL:URLString];
                        [policyHandler unableToImplementFileURLPolicy: fileURLPolicy error: error forDataSource: dataSource];
                    }
                return NO;
            }else{
                [NSException raise:NSInvalidArgumentException format:
                    @"fileURLPolicyForMIMEType:dataSource:isDirectory: returned an invalid WebFileURLPolicy"];
                return NO;
            }
        }else{
            if(![WebResourceHandle canInitWithURL:URL]){
            	error = [WebError errorWithCode:WebErrorCannotShowURL inDomain:WebErrorDomainWebKit failingURL:URLString];
                [policyHandler unableToImplementURLPolicy: URLPolicy error: error forURL: URL];
                return NO;
            }
            // we can handle this URL
            return YES;
        }
    }
    else if([URLPolicy policyAction] == WebURLPolicyOpenExternally){
        if(![workspace openURL:URL]){
            error = [WebError errorWithCode:WebErrorCouldNotFindApplicationForURL inDomain:WebErrorDomainWebKit failingURL:URLString];
            [policyHandler unableToImplementURLPolicy: URLPolicy error: error forURL: URL];
        }
        return NO;
    }
    else if([URLPolicy policyAction] == WebURLPolicyIgnore){
        return NO;
    }
    else{
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
    WebDataSource *dataSource;
    NSURL *originalURL = [[self dataSource] originalURL];
    
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
        unsigned flags = 0;
        if (type == WebFrameLoadTypeBack || type == WebFrameLoadTypeForward) {
            flags = WebResourceHandleUseCachedObjectIfPresent;
        }
        dataSource = [[WebDataSource alloc] initWithURL:itemURL attributes:nil flags:flags];
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
    [[[self dataSource] children] makeObjectsPerformSelector:@selector(_textSizeMultiplierChanged)];
}

@end
