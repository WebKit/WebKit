/*	
    IFWebFramePrivate.mm
	    
    Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/IFWebFramePrivate.h>

#import <WebKit/IFDocument.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFHTMLView.h>
#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFPreferencesPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebControllerPrivate.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebCoreFrame.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebKitErrors.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/WebKitDebug.h>

#import <WebFoundation/WebFoundation.h>

static const char * const stateNames[6] = {
    "zero state",
    "IFWEBFRAMESTATE_UNINITIALIZED",
    "IFWEBFRAMESTATE_PROVISIONAL",
    "IFWEBFRAMESTATE_COMMITTED_PAGE",
    "IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE",
    "IFWEBFRAMESTATE_COMPLETE"
};

@implementation IFWebFramePrivate

- (void)dealloc
{
    [webView _setController: nil];
    [dataSource _setController: nil];
    [dataSource _setLocationChangeHandler: nil];
    [provisionalDataSource _setController: nil];
    [provisionalDataSource _setLocationChangeHandler: nil];

    [name autorelease];
    [webView autorelease];
    [dataSource autorelease];
    [provisionalDataSource autorelease];
    [bridgeFrame release];
    
    [super dealloc];
}

- (NSString *)name { return name; }
- (void)setName: (NSString *)n 
{
    [name autorelease];
    name = [n retain];
}

- (IFWebView *)webView { return webView; }
- (void)setWebView: (IFWebView *)v 
{ 
    [webView autorelease];
    webView = [v retain];
}

- (IFWebDataSource *)dataSource { return dataSource; }
- (void)setDataSource: (IFWebDataSource *)d
{
    if (dataSource != d) {
        [dataSource _setController: nil];
        [dataSource _setLocationChangeHandler: nil];
        [dataSource autorelease];
        dataSource = [d retain];
    }
}

- (IFWebController *)controller { return controller; }
- (void)setController: (IFWebController *)c
{ 
    controller = c; // not retained (yet)
}

- (IFWebDataSource *)provisionalDataSource { return provisionalDataSource; }
- (void)setProvisionalDataSource: (IFWebDataSource *)d
{ 
    if (provisionalDataSource != d) {
        [provisionalDataSource autorelease];
        provisionalDataSource = [d retain];
    }
}

@end

@implementation IFWebFrame (IFPrivate)

- (void)_parentDataSourceWillBeDeallocated
{
    [_private setController:nil];
    [_private->dataSource _setParent:nil];
    [_private->provisionalDataSource _setParent:nil];
}

- (void)_setController: (IFWebController *)controller
{
    [_private setController: controller];
}

- (void)_setDataSource: (IFWebDataSource *)ds
{
    [_private setDataSource: ds];
    [ds _setController: [self controller]];
}

- (void)_scheduleLayout: (NSTimeInterval)inSeconds
{
    if (_private->scheduledLayoutPending == NO) {
        [NSTimer scheduledTimerWithTimeInterval:inSeconds target:self selector: @selector(_timedLayout:) userInfo: nil repeats:FALSE];
        _private->scheduledLayoutPending = YES;
    }
}

- (void)_timedLayout: (id)userInfo
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  state = %s\n", [[self name] cString], stateNames[_private->state]);
    
    _private->scheduledLayoutPending = NO;
    if (_private->state == IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE) {
        NSView <IFDocumentView> *documentView = [[self webView] documentView];
        
        if ([self controller])
            WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  performing timed layout, %f seconds since start of document load\n", [[self name] cString], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
            
        if ([[self webView] isDocumentHTML]) {
            IFHTMLView *htmlView = (IFHTMLView *)documentView;
            
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


- (void)_transitionProvisionalToLayoutAcceptable
{
    switch ([self _state]) {
    	case IFWEBFRAMESTATE_COMMITTED_PAGE:
        {
            [self _setState: IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE];
                    
            // Start a timer to guarantee that we get an initial layout after
            // X interval, even if the document and resources are not completely
            // loaded.
            BOOL timedDelayEnabled = [[IFPreferences standardPreferences] _initialTimedLayoutEnabled];
            if (timedDelayEnabled) {
                NSTimeInterval defaultTimedDelay = [[IFPreferences standardPreferences] _initialTimedLayoutDelay];
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

        case IFWEBFRAMESTATE_PROVISIONAL:
        case IFWEBFRAMESTATE_COMPLETE:
        case IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE:
        {
            break;
        }
        
        case IFWEBFRAMESTATE_UNINITIALIZED:
        default:
        {
            [[NSException exceptionWithName:NSGenericException reason: [NSString stringWithFormat: @"invalid state attempting to transition to IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE from %s", stateNames[_private->state]] userInfo: nil] raise];
            return;
        }
    }
}


- (void)_transitionProvisionalToCommitted
{
    WEBKIT_ASSERT ([self controller] != nil);
    NSView <IFDocumentView> *documentView = [[self webView] documentView];
    
    switch ([self _state]) {
    	case IFWEBFRAMESTATE_PROVISIONAL:
        {
            WEBKIT_ASSERT (documentView != nil);

            // Set the committed data source on the frame.
            [self _setDataSource: _private->provisionalDataSource];
            
            // provisionalDataSourceCommitted: will reset the view and begin trying to
            // display the new new datasource.
            [documentView provisionalDataSourceCommitted: _private->provisionalDataSource];
 
            // Now that the provisional data source is committed, release it.
            [_private setProvisionalDataSource: nil];
        
            [self _setState: IFWEBFRAMESTATE_COMMITTED_PAGE];
        
            [[[self dataSource] _locationChangeHandler] locationChangeCommittedForDataSource:[self dataSource]];
            
            // If we have a title let the controller know about it.
            if ([[self dataSource] pageTitle])
                [[[self dataSource] _locationChangeHandler] receivedPageTitle:[[self dataSource] pageTitle] forDataSource:[self dataSource]];

            break;
        }
        
        case IFWEBFRAMESTATE_UNINITIALIZED:
        case IFWEBFRAMESTATE_COMMITTED_PAGE:
        case IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE:
        case IFWEBFRAMESTATE_COMPLETE:
        default:
        {
            [[NSException exceptionWithName:NSGenericException reason:[NSString stringWithFormat: @"invalid state attempting to transition to IFWEBFRAMESTATE_COMMITTED from %s", stateNames[_private->state]] userInfo: nil] raise];
            return;
        }
    }
}

- (IFWebFrameState)_state
{
    return _private->state;
}

- (void)_setState: (IFWebFrameState)newState
{
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  transition from %s to %s\n", [[self name] cString], stateNames[_private->state], stateNames[newState]);
    if ([self controller])
        WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "%s:  transition from %s to %s, %f seconds since start of document load\n", [[self name] cString], stateNames[_private->state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[self controller] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == IFWEBFRAMESTATE_COMPLETE && self == [[self controller] mainFrame]){
        WEBKITDEBUGLEVEL (WEBKIT_LOG_DOCUMENTLOAD, "completed %s (%f seconds)", [[[[self dataSource] inputURL] absoluteString] cString], CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
    }
    
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithInt:_private->state], IFPreviousFrameState,
                    [NSNumber numberWithInt:newState], IFCurrentFrameState, nil];
                    
    [[NSNotificationCenter defaultCenter] postNotificationName:IFFrameStateChangedNotification object:self userInfo:userInfo];
    
    _private->state = newState;
    
    if (_private->state == IFWEBFRAMESTATE_PROVISIONAL){
        [[[self webView] frameScrollView] setDrawsBackground: NO];
    }
    
    if (_private->state == IFWEBFRAMESTATE_COMPLETE){
        NSScrollView *sv = [[self webView] frameScrollView];
        [sv setDrawsBackground: YES];
        [[sv contentView] setCopiesOnScroll: YES];
    }
}

- (void)_isLoadComplete
{
    WEBKIT_ASSERT ([self controller] != nil);

    switch ([self _state]) {
        // Shouldn't ever be in this state.
        case IFWEBFRAMESTATE_UNINITIALIZED:
        {
            [[NSException exceptionWithName:NSGenericException reason:@"invalid state IFWEBFRAMESTATE_UNINITIALIZED during completion check with error" userInfo: nil] raise];
            return;
        }
        
        case IFWEBFRAMESTATE_PROVISIONAL:
        {
            IFWebDataSource *pd = [self provisionalDataSource];
            
            WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete in IFWEBFRAMESTATE_PROVISIONAL\n", [[self name] cString]);
            // If we've received any errors we may be stuck in the provisional state and actually
            // complete.
            if ([[pd errors] count] != 0 || [pd mainDocumentError]) {
                // Check all children first.
                WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete, current state IFWEBFRAMESTATE_PROVISIONAL, %d errors\n", [[self name] cString], [[pd errors] count]);
                if (![pd isLoading]) {
                    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete in IFWEBFRAMESTATE_PROVISIONAL, load done\n", [[self name] cString]);

                    [[pd _locationChangeHandler] locationChangeDone: [pd mainDocumentError] forDataSource:pd];

                    // We now the provisional data source didn't cut the mustard, release it.
                    [_private setProvisionalDataSource: nil];
                    
                    [self _setState: IFWEBFRAMESTATE_COMPLETE];
                    return;
                }
            }
            return;
        }
        
        case IFWEBFRAMESTATE_COMMITTED_PAGE:
        case IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE:
        {
            IFWebDataSource *ds = [self dataSource];
            
            //WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete, current state IFWEBFRAMESTATE_COMMITTED\n", [[self name] cString]);
            if (![ds isLoading]) {
                id mainView = [[[self controller] mainFrame] webView];
                NSView <IFDocumentView> *mainDocumentView = [mainView documentView];
#if 0
                id thisView = [self webView];
                NSView <IFDocumentView> *thisDocumentView = [thisView documentView];
#endif
                [self _setState: IFWEBFRAMESTATE_COMPLETE];
                
                [[ds _bridge] end];

                // We have to layout the main document as
                // it may change the size of frames.
                // FIXME:  Why is this necessary? and recurse.
                {
                    if ([mainView isDocumentHTML]) {
                        [(IFHTMLView *)mainDocumentView setNeedsLayout: YES];
                    }
                    [mainDocumentView layout];
                    
                    NSArray *subFrames = [[[[self controller] mainFrame] dataSource] children];
                    unsigned int i;
                    id dview;
                    for (i = 0; i < [subFrames count]; i++){
                        dview = [[[subFrames objectAtIndex: i] webView] documentView];
                        if ([[[subFrames objectAtIndex: i] webView] isDocumentHTML])
                            [dview setNeedsLayout: YES];
                        [dview layout];
                    }
                }

#if 0
                // Tell the just loaded document to layout.  This may be necessary
                // for non-html content that needs a layout message.
                if ([thisView isDocumentHTML]){
                    [thisDocumentView setNeedsLayout: YES];
                }
                [thisDocumentView layout];

                [thisDocumentView setNeedsDisplay: YES];
                [thisDocumentView display];
#endif

                // Jump to anchor point, if necessary.
                [[ds _bridge] scrollToBaseAnchor];

                // FIXME:  We have to draw the whole document hierarchy.  We should be 
                // able to just draw the document associated with this
                // frame, but that doesn't work.  Not sure why.
                [mainDocumentView setNeedsDisplay: YES];
                [mainDocumentView display];

                [[ds _locationChangeHandler] locationChangeDone: [ds mainDocumentError] forDataSource:ds];
 
                //if ([ds isDocumentHTML])
                //    [[ds representation] part]->closeURL();        
               
                return;
            }
            // A resource was loaded, but the entire frame isn't complete.  Schedule a
            // layout.
            else {
                if ([self _state] == IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE) {
                    BOOL resourceTimedDelayEnabled = [[IFPreferences standardPreferences] _resourceTimedLayoutEnabled];
                    if (resourceTimedDelayEnabled) {
                        NSTimeInterval timedDelay = [[IFPreferences standardPreferences] _resourceTimedLayoutDelay];
                        [self _scheduleLayout: timedDelay];
                    }
                }
            }
            return;
        }
        
        case IFWEBFRAMESTATE_COMPLETE:
        {
            WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "%s:  checking complete, current state IFWEBFRAMESTATE_COMPLETE\n", [[self name] cString]);
            return;
        }
        
        // Yikes!  Serious horkage.
        default:
        {
            [[NSException exceptionWithName:NSGenericException reason:@"invalid state during completion check with error" userInfo: nil] raise];
            break;
        }
    }
}

+ (void)_recursiveCheckCompleteFromFrame: (IFWebFrame *)fromFrame
{
    int i, count;
    NSArray *childFrames;
    
    childFrames = [[fromFrame dataSource] children];
    count = [childFrames count];
    for (i = 0; i < count; i++) {
        IFWebFrame *childFrame;
        
        childFrame = [childFrames objectAtIndex: i];
        [IFWebFrame _recursiveCheckCompleteFromFrame: childFrame];
        [childFrame _isLoadComplete];
    }
    [fromFrame _isLoadComplete];
}

// Called every time a resource is completely loaded, or an error is received.
- (void)_checkLoadComplete
{

    WEBKIT_ASSERT ([self controller] != nil);

    // Now walk the frame tree to see if any frame that may have initiated a load is done.
    [IFWebFrame _recursiveCheckCompleteFromFrame: [[self controller] mainFrame]];
}

- (IFWebCoreFrame *)_bridgeFrame
{
    return _private->bridgeFrame;
}

- (BOOL)_shouldShowDataSource:(IFWebDataSource *)dataSource
{
    id <IFWebControllerPolicyHandler> policyHandler = [[self controller] policyHandler];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *url = [dataSource inputURL];
    IFFileURLPolicy fileURLPolicy;
    NSString *path = [url path];
    BOOL isDirectory, fileExists;
    IFError *error;
    
    IFURLPolicy urlPolicy = [policyHandler URLPolicyForURL:url];
    
    if(urlPolicy == IFURLPolicyUseContentPolicy){
                    
        if([url isFileURL]){
        
            fileExists = [fileManager fileExistsAtPath:path isDirectory:&isDirectory];
            
            NSString *type = [IFWebController _MIMETypeForFile: path];
                
            if(isDirectory){
                fileURLPolicy = [policyHandler fileURLPolicyForMIMEType: nil dataSource: dataSource isDirectory:YES];
            }else{
                fileURLPolicy = [policyHandler fileURLPolicyForMIMEType: type dataSource: dataSource isDirectory:NO];
            }
            
            if(fileURLPolicy == IFFileURLPolicyIgnore)
                return NO;
            
            if(!fileExists){
                error = [[IFError alloc] initWithErrorCode:IFErrorCodeFileDoesntExist 
                            inDomain:IFErrorCodeDomainWebKit failingURL:url];
                [policyHandler unableToImplementFileURLPolicy: error forDataSource: dataSource];
                return NO;
            }
            
            if(![fileManager isReadableFileAtPath:path]){
                error = [[IFError alloc] initWithErrorCode:IFErrorCodeFileNotReadable 
                            inDomain:IFErrorCodeDomainWebKit failingURL:url];
                [policyHandler unableToImplementFileURLPolicy: error forDataSource: dataSource];
                return NO;
            }
            
            if(fileURLPolicy == IFFileURLPolicyUseContentPolicy){
                if(isDirectory){
                    error = [[IFError alloc] initWithErrorCode:IFErrorCodeCantShowDirectory 
                                inDomain:IFErrorCodeDomainWebKit failingURL: url];
                    [policyHandler unableToImplementFileURLPolicy: error forDataSource: dataSource];
                    return NO;
                }
                else if(![IFWebController canShowMIMEType: type]){
                    error = [[IFError alloc] initWithErrorCode:IFErrorCodeCantShowMIMEType 
                                inDomain:IFErrorCodeDomainWebKit failingURL: url];
                    [policyHandler unableToImplementFileURLPolicy: error forDataSource: dataSource];
                    return NO;
                }else{
                    // File exists, its readable, we can show it
                    return YES;
                }
            }else if(fileURLPolicy == IFFileURLPolicyOpenExternally){
                if(![workspace openFile:path]){
                    error = [[IFError alloc] initWithErrorCode:IFErrorCodeCouldntFindApplicationForFile 
                                inDomain:IFErrorCodeDomainWebKit failingURL: url];
                    [policyHandler unableToImplementFileURLPolicy: error forDataSource: dataSource];
                }
                return NO;
            }else if(fileURLPolicy == IFFileURLPolicyReveal){
                if(![workspace selectFile:path inFileViewerRootedAtPath:@""]){
                        error = [[IFError alloc] initWithErrorCode:IFErrorCodeFinderCouldntOpenDirectory 
                                    inDomain:IFErrorCodeDomainWebKit failingURL: url];
                        [policyHandler unableToImplementFileURLPolicy: error forDataSource: dataSource];
                    }
                return NO;
            }else{
                [NSException raise:NSInvalidArgumentException format:
                    @"fileURLPolicyForMIMEType:dataSource:isDirectory: returned an invalid IFFileURLPolicy"];
                return NO;
            }
        }else{
            if(![IFURLHandle canInitWithURL:url]){
            	error = [[IFError alloc] initWithErrorCode:IFErrorCodeCantShowURL 
                        inDomain:IFErrorCodeDomainWebKit failingURL: url];
                [policyHandler unableToImplementURLPolicyForURL: url error: error];
                return NO;
            }
            // we can handle this URL
            return YES;
        }
    }
    else if(urlPolicy == IFURLPolicyOpenExternally){
        if(![workspace openURL:url]){
            error = [[IFError alloc] initWithErrorCode:IFErrorCodeCouldntFindApplicationForURL 
                        inDomain:IFErrorCodeDomainWebKit failingURL: url];
            [policyHandler unableToImplementURLPolicyForURL: url error: error];
        }
        return NO;
    }
    else if(urlPolicy == IFURLPolicyIgnore){
        return NO;
    }
    else{
        [NSException raise:NSInvalidArgumentException format:@"URLPolicyForURL: returned an invalid IFURLPolicy"];
        return NO;
    }
}

@end
