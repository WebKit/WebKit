/*
    WebFramePrivate.m
    
    Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebFramePrivate.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultWindowOperationsDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebFormDelegate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryPrivate.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebLocationChangeDelegate.h>
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebNSStringExtras.h>
#import <WebFoundation/WebResource.h>
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLRequestPrivate.h>
#import <WebFoundation/NSURLResponse.h>

#import <WebFoundation/WebSynchronousResult.h>

#import <objc/objc-runtime.h>

#ifndef NDEBUG
static const char * const stateNames[] = {
    "WebFrameStateProvisional",
    "WebFrameStateCommittedPage",
    "WebFrameStateLayoutAcceptable",
    "WebFrameStateComplete"
};

static const char * const loadTypeNames[] = {
    "WebFrameLoadTypeStandard",
    "WebFrameLoadTypeBack",
    "WebFrameLoadTypeForward",
    "WebFrameLoadTypeIndexedBackForward",
    "WebFrameLoadTypeReload",
    "WebFrameLoadTypeReloadAllowingStaleData",
    "WebFrameLoadTypeSame",
    "WebFrameLoadTypeInternal"
};
#endif

/*
Here is the current behavior matrix for four types of navigations:

Standard Nav:

 Restore form state:   YES
 Restore scroll and focus state:  YES
 WF Cache policy: NSURLRequestUseProtocolCachePolicy
 Add to back/forward list: YES
 
Back/Forward:

 Restore form state:   YES
 Restore scroll and focus state:  YES
 WF Cache policy: NSURLRequestReturnCacheDataElseLoad
 Add to back/forward list: NO

Reload (meaning only the reload button):

 Restore form state:   NO
 Restore scroll and focus state:  YES
 WF Cache policy: NSURLRequestReloadIgnoringCacheData
 Add to back/forward list: NO

Repeat load of the same URL (by any other means of navigation other than the reload button, including hitting return in the location field):

 Restore form state:   NO
 Restore scroll and focus state:  NO, reset to initial conditions
 WF Cache policy: NSURLRequestReloadIgnoringCacheData
 Add to back/forward list: NO
*/

@interface NSObject (WebExtraPerformMethod)

- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2 withObject:(id)object3;

@end

@implementation NSObject (WebExtraPerformMethod)

- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2 withObject:(id)object3
{
    return objc_msgSend(self, aSelector, object1, object2, object3);
}

@end


// One day we might want to expand the use of this kind of class such that we'd receive one
// over the bridge, and possibly hand it on through to the FormsDelegate.
// Today it is just used internally to keep some state as we make our way through a bunch
// layers while doing a load.
@interface WebFormState : NSObject
{
    NSObject <WebDOMElement> *_form;
    NSDictionary *_values;
}
- (id)initWithForm:(NSObject <WebDOMElement> *)form values:(NSDictionary *)values;
- (id <WebDOMElement>)form;
- (NSDictionary *)values;
@end

@interface WebFrame (ForwardDecls)
- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(WebFrameLoadType)loadType formState:(WebFormState *)formState;

- (NSDictionary *)_actionInformationForLoadType:(WebFrameLoadType)loadType isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL;

- (void)_saveScrollPositionToItem:(WebHistoryItem *)item;
- (void)_restoreScrollPosition;
- (void)_scrollToTop;

- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;

- (void)_resetBackForwardListToCurrent;
@end

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
    [webFrameView _setController:nil];
    [dataSource _setController:nil];
    [provisionalDataSource _setController:nil];

    [name release];
    [webFrameView release];
    [dataSource release];
    [provisionalDataSource release];
    [bridge release];
    [scheduledLayoutTimer release];
    [children release];

    [currentItem release];
    [provisionalItem release];
    [previousItem release];
    
    ASSERT(listener == nil);
    ASSERT(policyRequest == nil);
    ASSERT(policyFrameName == nil);
    ASSERT(policyTarget == nil);
    ASSERT(policyFormState == nil);

    [super dealloc];
}

- (NSString *)name { return name; }
- (void)setName:(NSString *)n 
{
    NSString *newName = [n copy];
    [name release];
    name = newName;
}

- (WebFrameView *)webFrameView { return webFrameView; }
- (void)setWebFrameView: (WebFrameView *)v 
{ 
    [v retain];
    [webFrameView release];
    webFrameView = v;
}

- (WebDataSource *)dataSource { return dataSource; }
- (void)setDataSource: (WebDataSource *)d
{
    [d retain];
    [dataSource release];
    dataSource = d;
}

- (WebView *)controller { return controller; }
- (void)setController: (WebView *)c
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

- (void)setController: (WebView *)controller
{
    // To set controller to nil, we have to use _controllerWillBeDeallocated, not this.
    ASSERT(controller);
    [_private setController: controller];
}


// helper method used in various nav cases below
- (WebHistoryItem *)_addBackForwardItemClippedAtTarget:(BOOL)doClip
{
    WebHistoryItem *bfItem = [[[self webView] mainFrame] _createItemTreeWithTargetFrame:self clippedAtTarget:doClip];
    [[[self webView] backForwardList] addItem:bfItem];
    return bfItem;
}

- (WebHistoryItem *)_createItem
{
    WebDataSource *dataSrc = [self dataSource];
    NSURLRequest *request = [dataSrc request];
    NSURL *URL = [request URL];
    WebHistoryItem *bfItem;

    bfItem = [[[WebHistoryItem alloc] initWithURL:URL target:[self name] parent:[[self parent] name] title:[dataSrc pageTitle]] autorelease];
    [bfItem setAnchor:[URL fragment]];
    [dataSrc _addBackForwardItem:bfItem];
    [bfItem setOriginalURLString:[[[dataSrc _originalRequest] URL] absoluteString]];

    // save form state if this is a POST
    if ([[request HTTPMethod] _web_isCaseInsensitiveEqualToString:@"POST"]) {
        [bfItem setFormData:[request HTTPBody]];
        [bfItem setFormContentType:[request HTTPContentType]];
        [bfItem setFormReferrer:[request HTTPReferrer]];
    }

    // Set the item for which we will save document state
    [_private setPreviousItem:[_private currentItem]];
    [_private setCurrentItem:bfItem];

    return bfItem;
}

/*
    In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  The item that was the target of the user's navigation is designated as the "targetItem".  When this method is called with doClip=YES we're able to create the whole tree except for the target's children, which will be loaded in the future.  That part of the tree will be filled out as the child loads are committed.
*/
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

- (void)_setName:(NSString *)name
{
    // It's wrong to name a frame "_blank".
    if (![name isEqualToString:@"_blank"]) {
	[_private setName:name];
    }
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

- (void)_detachChildren
{
    // Note we have to be careful to remove the kids as we detach each one,
    // since detaching stops loading, which checks loadComplete, which runs the whole
    // frame tree, at which point we don't want to trip on already detached kids.
    if (_private->children) {
        int i;
        for (i = [_private->children count]-1; i >=0; i--) {
            [[_private->children objectAtIndex:i] _detachFromParent];
            [_private->children removeObjectAtIndex:i];
        }
        [_private->children release];
        _private->children = nil;
    }
}

- (void)_closeOldDataSources
{
    if (_private->children) {
        int i;
        for (i = [_private->children count]-1; i >=0; i--) {
            [[_private->children objectAtIndex:i] _closeOldDataSources];
        }
    }
    if (_private->dataSource) {
        [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller willCloseLocationForDataSource:_private->dataSource];
    }
}

- (void)_detachFromParent
{
    WebBridge *bridge = _private->bridge;
    _private->bridge = nil;

    [self stopLoading];
    [self _saveScrollPositionToItem:[_private currentItem]];

    [bridge closeURL];

    [self _detachChildren];

    [_private setController:nil];
    [_private->webFrameView _setController:nil];
    [_private->dataSource _setController:nil];
    [_private->provisionalDataSource _setController:nil];

    [self _setDataSource:nil];
    [_private setWebFrameView:nil];

    [_private->scheduledLayoutTimer invalidate];
    [_private->scheduledLayoutTimer release];
    _private->scheduledLayoutTimer = nil;
    
    [bridge release];
}

- (void)_setController: (WebView *)controller
{
    [_private setController:controller];
}

- (void)_setDataSource:(WebDataSource *)ds
{
    if (ds == nil && _private->dataSource == nil) {
	return;
    }

    ASSERT(ds != _private->dataSource);
    
    if ([_private->dataSource isDocumentHTML] && ![ds isDocumentHTML]) {
        [_private->bridge removeFromFrame];
    }

    [self _detachChildren];
    
    [_private->dataSource _setWebFrame:nil];

    [_private setDataSource:ds];
    [ds _setController:[self webView]];
    [ds _setWebFrame:self];
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
        NSView <WebDocumentView> *documentView = [[self frameView] documentView];
        
        if ([self webView])
            LOG(Timing, "%@:  performing timed layout, %f seconds since start of document load", [self name], CFAbsoluteTimeGetCurrent() - [[[[self webView] mainFrame] dataSource] _loadingStartedTime]);
            
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
        if ([self webView])
            LOG(Timing, "%@:  NOT performing timed layout (not needed), %f seconds since start of document load", [self name], CFAbsoluteTimeGetCurrent() - [[[[self webView] mainFrame] dataSource] _loadingStartedTime]);
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
            return;
        }

        case WebFrameStateProvisional:
        case WebFrameStateComplete:
        case WebFrameStateLayoutAcceptable:
            return;
    }
    ASSERT_NOT_REACHED();
}

- (void)_makeDocumentView
{
    NSView <WebDocumentView> *documentView = [_private->webFrameView _makeDocumentViewForDataSource:_private->dataSource];
    if (!documentView) {
        return;
    }

    // FIXME: We could save work and not do this for a top-level view that is not a WebHTMLView.
    WebFrameView *v = _private->webFrameView;
    [_private->bridge createKHTMLViewWithNSView:documentView marginWidth:[v _marginWidth] marginHeight:[v _marginHeight]];
    [_private->bridge installInFrame:[v scrollView]];

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:_private->dataSource];
}

- (void)_transitionToCommitted: (NSDictionary *)pageCache
{
    ASSERT([self webView] != nil);

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
                
            [self _setProvisionalDataSource: nil];

            [self _setState: WebFrameStateCommittedPage];
        
            // Handle adding the URL to the back/forward list.
            WebDataSource *ds = [self dataSource];
            WebHistoryItem *entry = nil;
            NSString *ptitle = [ds pageTitle];

            switch (loadType) {
            case WebFrameLoadTypeForward:
            case WebFrameLoadTypeBack:
            case WebFrameLoadTypeIndexedBackForward:
                if ([[self webView] backForwardList]) {
                    // Must grab the current scroll position before disturbing it
                    [self _saveScrollPositionToItem:[_private previousItem]];
                    
                    // Create a document view for this document, or used the cached view.
                    if (pageCache){
                        NSView <WebDocumentView> *cachedView = [pageCache objectForKey: @"WebKitDocumentView"];
                        ASSERT (cachedView != nil);
                        [[self frameView] _setDocumentView: cachedView];
                    }
                    else
                        [self _makeDocumentView];
                        
                    // FIXME - I'm not sure this call does anything.  Should be dealt with as
                    // part of 3024377
                    [self _restoreScrollPosition];
                }
                break;

            case WebFrameLoadTypeReload:
            case WebFrameLoadTypeSame:
            {
                WebHistoryItem *currItem = [_private currentItem];
                LOG(PageCache, "Clearing back/forward cache, %s\n", [[[currItem URL] absoluteString] cString]);
                // FIXME: rjw sez this cache clearing is no longer needed
                [currItem setHasPageCache:NO];
                if (loadType == WebFrameLoadTypeReload) {
                    [self _saveScrollPositionToItem:currItem];
                }
                [self _makeDocumentView];
                break;
            }

            // FIXME - just get rid of this case, and merge WebFrameLoadTypeReloadAllowingStaleData with the above case
            case WebFrameLoadTypeReloadAllowingStaleData:
                [self _makeDocumentView];
                break;
                
            case WebFrameLoadTypeStandard:
                if (![ds _isClientRedirect]) {
                    // Add item to history.
		    NSURL *URL = [[[ds _originalRequest] URL] _web_canonicalize];
		    if ([[URL absoluteString] length] > 0) {
			entry = [[WebHistory sharedHistory] addItemForURL:URL];
			if (ptitle)
			    [entry setTitle: ptitle];
                        [self _addBackForwardItemClippedAtTarget:YES];
		    }

                } else {
                    // update the URL in the BF list that we made before the redirect
                    [[_private currentItem] setURL:[[ds request] URL]];
                }
                [self _makeDocumentView];
                break;
                
            case WebFrameLoadTypeInternal:
                // Add an item to the item tree for this frame
                ASSERT(![ds _isClientRedirect]);
                WebHistoryItem *parentItem = [[self parent]->_private currentItem];
                // The only case where parentItem==nil should be when a parent frame loaded an
                // empty URL, which doesn't set up a current item in that parent.
                if (parentItem) {
                    [parentItem addChildItem:[self _createItem]];
                }
                [self _makeDocumentView];
                break;

            // FIXME Remove this check when dummy ds is removed.  An exception should be thrown
            // if we're in the WebFrameLoadTypeUninitialized state.
            default:
                ASSERT_NOT_REACHED();
            }

            
            // Tell the client we've committed this URL.
            ASSERT([[self frameView] documentView] != nil);
            [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller locationChangeCommittedForDataSource:ds];
            
            // If we have a title let the controller know about it.
            if (ptitle) {
                [entry setTitle:ptitle];
                [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller receivedPageTitle:ptitle forDataSource:ds];
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


    if (pageCache){
        [[self dataSource] _setPrimaryLoadComplete: YES];
        [self _isLoadComplete];
    }
}

- (BOOL)_canCachePage
{
    return [[[self webView] backForwardList] usesPageCache];
}

- (void)_purgePageCache
{
    // This method implements the rule for purging the page cache.
    unsigned sizeLimit = [[[self webView] backForwardList] pageCacheSize];
    unsigned pagesCached = 0;
    WebBackForwardList *backForwardList = [[self webView] backForwardList];
    NSArray *backList = [backForwardList backListWithSizeLimit: 999999];
    WebHistoryItem *oldestItem = nil;
    
    unsigned i;
    for (i = 0; i < [backList count]; i++){
        WebHistoryItem *item = [backList objectAtIndex: i];
        if ([item hasPageCache]){
            if (oldestItem == nil)
                oldestItem = item;
            pagesCached++;
        }
    }
    
    // Snapback items are never directly purged here.
    if (pagesCached >= sizeLimit && ![oldestItem alwaysAttemptToUsePageCache]){
        LOG(PageCache, "Purging back/forward cache, %s\n", [[[oldestItem URL] absoluteString] cString]);
        [oldestItem setHasPageCache: NO];
    }
}

- (WebFrameState)_state
{
    return _private->state;
}

static CFAbsoluteTime _timeOfLastCompletedLoad;
+ (CFAbsoluteTime)_timeOfLastCompletedLoad
{
    return _timeOfLastCompletedLoad;
}

- (void)_setState: (WebFrameState)newState
{
    LOG(Loading, "%@:  transition from %s to %s", [self name], stateNames[_private->state], stateNames[newState]);
    if ([self webView])
        LOG(Timing, "%@:  transition from %s to %s, %f seconds since start of document load", [self name], stateNames[_private->state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[self webView] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && self == [[self webView] mainFrame]){
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
        [[[self frameView] scrollView] setDrawsBackground:NO];

        // Cache the page, if possible.
        // Don't write to the cache if in the middle of a redirect, since we will want to
        // store the final page we end up on.
        // No point writing to the cache on a reload or loadSame, since we will just write
        // over it again when we leave that page.
        WebHistoryItem *item = [_private currentItem];
        WebFrameLoadType loadType = [self _loadType];
        if ([self _canCachePage]
            && [_private->bridge canCachePage]
            && item
            && !_private->quickRedirectComing
            && loadType != WebFrameLoadTypeReload 
            && loadType != WebFrameLoadTypeReloadAllowingStaleData
            && loadType != WebFrameLoadTypeSame
            && ![[self dataSource] isLoading]
            && ![[self dataSource] _isStopping]
            && [[[self dataSource] representation] isKindOfClass: [WebHTMLRepresentation class]])
        {
            if (![item pageCache]){
                LOG(PageCache, "Saving page to back/forward cache, %s\n", [[[[self dataSource] _URL] absoluteString] cString]);
                [item setHasPageCache: YES];
                [[self dataSource] _setStoredInPageCache: YES];
                [[item pageCache] setObject: [self dataSource] forKey: @"WebKitDataSource"];
                [[item pageCache] setObject: [[self frameView] documentView] forKey: @"WebKitDocumentView"];
                [_private->bridge saveDocumentToPageCache];
                [self _purgePageCache];
            }
        }
    }
    
    if (_private->state == WebFrameStateComplete) {
        NSScrollView *sv = [[self frameView] scrollView];
        [sv setDrawsBackground:YES];
        // FIXME: This overrides the setCopiesOnScroll setting done by
        // WebCore based on whether the page's contents are dynamic or not.
        [[sv contentView] setCopiesOnScroll:YES];
        [_private->scheduledLayoutTimer fire];
        ASSERT(_private->scheduledLayoutTimer == nil);
        [_private setPreviousItem:nil];
        _timeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();
    }
}

// Called after we send an openURL:... down to WebCore.
- (void)_opened
{
    if ([[self dataSource] _loadingFromPageCache]){
        // Force a layout to update view size and thereby
        // update scrollbars.
        [_private->bridge reapplyStyles];
        [[[self frameView] documentView] setNeedsLayout: YES];
        [[[self frameView] documentView] layout];
        [self _restoreScrollPosition];
        
        NSArray *responses = [[self dataSource] _responses];
        NSURLResponse *response;
        int i, count = [responses count];
        for (i = 0; i < count; i++){
            response = [responses objectAtIndex: i];
            [_private->bridge objectLoadedFromCacheWithURL: [[response URL] absoluteString]
                    response: response
                    size: [response expectedContentLength]];
        }
        
        // Release the resources kept in the page cache.  They will be
        // reset when we leave this page.  The core side of the page cache
        // will have already been invalidated by the bridge to prevent
        // premature release.
        [[_private currentItem] setHasPageCache: NO];
    }
}

- (void)_isLoadComplete
{
    ASSERT([self webView] != nil);

    switch ([self _state]) {
        case WebFrameStateProvisional:
        {
            WebDataSource *pd = [self provisionalDataSource];
            
            LOG(Loading, "%@:  checking complete in WebFrameStateProvisional", [self name]);
            // If we've received any errors we may be stuck in the provisional state and actually
            // complete.
            if ([pd _mainDocumentError]) {
                // Check all children first.
                LOG(Loading, "%@:  checking complete, current state WebFrameStateProvisional", [self name]);
                [self _resetBackForwardListToCurrent];
                if (![pd isLoading]) {
                    LOG(Loading, "%@:  checking complete in WebFrameStateProvisional, load done", [self name]);

                    [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller locationChangeDone:[pd _mainDocumentError] forDataSource:pd];

                    // We know the provisional data source didn't cut the muster, release it.
                    [self _setProvisionalDataSource:nil];
                    
                    [self _setState:WebFrameStateComplete];
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
                WebFrameView *thisView = [self frameView];
                NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
                ASSERT(thisDocumentView != nil);

                // FIXME: need to avoid doing this in the non-HTML case or the bridge may assert.
                // Should instead make sure the bridge/part is in the proper state even for
                // non-HTML content, or make a call to the document and let it deal with the bridge.

                [self _setState:WebFrameStateComplete];
                if ([ds isDocumentHTML]) {
                    [_private->bridge end];
                }

                // FIXME: Is this subsequent work important if we already navigated away?
                // Maybe there are bugs because of that, or extra work we can skip because
                // the new page is ready.

                // Unfortunately we have to get our parent to adjust the frames in this
                // frameset so this frame's geometry is set correctly.  This should
                // be a reasonably inexpensive operation.
                WebDataSource *parentDS = [[self parent] dataSource];
                if ([[parentDS _bridge] isFrameSet]){
                    WebFrameView *parentWebFrameView = [[self parent] frameView];
                    if ([parentWebFrameView isDocumentHTML])
                        [(WebHTMLView *)[parentWebFrameView documentView] _adjustFrames];
                }

                // Tell the just loaded document to layout.  This may be necessary
                // for non-html content that needs a layout message.
                if (!([[self frameView] isDocumentHTML])) {
                    [thisDocumentView setNeedsLayout:YES];
                    [thisDocumentView layout];
                    [thisDocumentView setNeedsDisplay:YES];
                }
                 
                // If the user had a scroll point scroll to it.  This will override
                // the anchor point.  After much discussion it was decided by folks
                // that the user scroll point should override the anchor point.
                if ([[self webView] backForwardList]) {
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
                    case WebFrameLoadTypeSame:
                        // Do nothing.
                        break;

                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }

                [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller locationChangeDone:[ds _mainDocumentError] forDataSource:ds];
 
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
                        [self _scheduleLayout:timedDelay];
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
    }
    
    // Yikes!  Serious horkage.
    ASSERT_NOT_REACHED();
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
    ASSERT([self webView] != nil);

    // Now walk the frame tree to see if any frame that may have initiated a load is done.
    [WebFrame _recursiveCheckCompleteFromFrame: [[self webView] mainFrame]];
}

- (WebBridge *)_bridge
{
    return _private->bridge;
}

- (void)_handleUnimplementablePolicyWithErrorCode:(int)code forURL:(NSURL *)URL
{
    WebError *error = [WebError errorWithCode:code
                                     inDomain:WebErrorDomainWebKit
                                   failingURL:[URL absoluteString]];
    WebView *c = [self webView];
    [[c _policyDelegateForwarder] webView:c unableToImplementPolicyWithError:error inFrame:self];    
}

- (void)_clearProvisionalDataSource
{
    [self _setProvisionalDataSource:nil];
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
                return NO; // couldn't match the i'th itemTarget
            }
        }
        return YES; // found matches for all itemTargets
    }
}

// loads content into this frame, as specified by item
- (void)_loadItem:(WebHistoryItem *)item fromItem:(WebHistoryItem *)fromItem withLoadType:(WebFrameLoadType)loadType
{
    NSURL *itemURL = [item URL];
    NSURL *currentURL = [[[self dataSource] request] URL];
    NSData *formData = [item formData];

    // Are we navigating to an anchor within the page?
    // Note if we have child frames we do a real reload, since the child frames might not
    // match our current frame structure, or they might not have the right content.  We could
    // check for all that as an additional optimization.
    // We also do not do anchor-style navigation if we're posting a form.
    
    // FIXME: These checks don't match the ones in _loadURL:referrer:loadType:target:triggeringEvent:isFormSubmission:
    // Perhaps they should.
    
    if (!formData
        && [item anchor]
        && [[itemURL _web_URLByRemovingFragment] isEqual: [currentURL _web_URLByRemovingFragment]]
        && (!_private->children || ![_private->children count]))
    {
        // must do this maintenance here, since we don't go through a real page reload
        [self _saveScrollPositionToItem:[_private currentItem]];
        // FIXME: form state might want to be saved here too

        // FIXME: Perhaps we can use scrollToAnchorWithURL here instead and remove the older scrollToAnchor:?
        [[_private->dataSource _bridge] scrollToAnchor: [item anchor]];
    
        // must do this maintenance here, since we don't go through a real page reload
        [_private setCurrentItem:item];
        [self _restoreScrollPosition];

        [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller locationChangedWithinPageForDataSource:_private->dataSource];
    } else {
        // Remember this item so we can traverse any child items as child frames load
        [_private setProvisionalItem:item];

        WebDataSource *newDataSource;
        if ([item hasPageCache]){
            newDataSource = [[item pageCache] objectForKey: @"WebKitDataSource"];
            [self _loadDataSource:newDataSource withLoadType:loadType formState:nil];            
        }
        else {
            NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:itemURL];
            [self _addExtraFieldsToRequest:request alwaysFromRequest: (formData != nil)?YES:NO];

            // If this was a repost that failed the page cache, we might try to repost the form.
            NSDictionary *action;
            if (formData) {
                [request HTTPSetMethod:@"POST"];
                [request HTTPSetBody:formData];
                [request HTTPSetContentType:[item formContentType]];
                [request HTTPSetReferrer:[item formReferrer]];

                // Slight hack to test if the WF cache contains the page we're going to.  We want
                // to know this before talking to the policy delegate, since it affects whether we
                // show the DoYouReallyWantToRepost nag.
                //
                // This trick has a small bug (3123893) where we might find a cache hit, but then
                // have the item vanish when we try to use it in the ensuing nav.  This should be
                // extremely rare, but in that case the user will get an error on the navigation.
                [request setCachePolicy:NSURLRequestReturnCacheDataDontLoad];
                /* WebFoundation API FIXME: fix when new synchronous code is implemented */
                //WebSynchronousResult *result = [WebResource sendSynchronousRequest:request];
                if (NO) { 
                    // Not in WF cache
                    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
                    action = [self _actionInformationForNavigationType:WebNavigationTypeFormResubmitted event:nil originalURL:itemURL];
                } else {
                    // We can use the cache, don't use navType=resubmit
                    action = [self _actionInformationForLoadType:loadType isFormSubmission:NO event:nil originalURL:itemURL];
                }
            } else {
                switch (loadType) {
                    case WebFrameLoadTypeReload:
                        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
                        break;
                    case WebFrameLoadTypeBack:
                    case WebFrameLoadTypeForward:
                    case WebFrameLoadTypeIndexedBackForward:
                        [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];
                        break;
                    case WebFrameLoadTypeStandard:
                    case WebFrameLoadTypeInternal:
                        // no-op: leave as protocol default
                        // FIXME:  I wonder if we ever hit this case
                        break;
                    case WebFrameLoadTypeSame:
                    case WebFrameLoadTypeReloadAllowingStaleData:
                    default:
                        ASSERT_NOT_REACHED();
                }

                action = [self _actionInformationForLoadType:loadType isFormSubmission:NO event:nil originalURL:itemURL];
            }

            [self _loadRequest:request triggeringAction:action loadType:loadType formState:nil];
            [request release];
        }
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
	(([self name] == nil && [item target] == nil) ||[[self name] isEqualToString:[item target]]) &&
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
    WebBackForwardList *backForwardList = [[self webView] backForwardList];
    WebHistoryItem *currItem = [backForwardList currentItem];
    // Set the BF cursor before commit, which lets the user quickly click back/forward again.
    // - plus, it only makes sense for the top level of the operation through the frametree,
    // as opposed to happening for some/one of the page commits that might happen soon
    [backForwardList goToItem:item];
    [self _recursiveGoToItem:item fromItem:currItem withLoadType:type];
}

- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(WebFrameLoadType)loadType formState:(WebFormState *)formState
{
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    [newDataSource _setTriggeringAction:action];

    [newDataSource _setOverrideEncoding:[[self dataSource] _overrideEncoding]];

    [self _loadDataSource:newDataSource withLoadType:loadType formState:formState];

    [newDataSource release];
}

-(NSDictionary *)_actionInformationForNavigationType:(WebNavigationType)navigationType event:(NSEvent *)event originalURL:(NSURL *)URL
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp:
        {
            NSView *topViewInEventWindow = [[event window] contentView];
            NSView *viewContainingPoint = [topViewInEventWindow hitTest:[topViewInEventWindow convertPoint:[event locationInWindow] fromView:nil]];
            while (viewContainingPoint != nil) {
                if ([viewContainingPoint isKindOfClass:[WebHTMLView class]]) {
                    break;
                }
                viewContainingPoint = [viewContainingPoint superview];
            }
            if (viewContainingPoint != nil) {
                NSPoint point = [viewContainingPoint convertPoint:[event locationInWindow] fromView:nil];
                NSDictionary *elementInfo = [(WebHTMLView *)viewContainingPoint _elementAtPoint:point];
        
                return [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithInt:navigationType], WebActionNavigationTypeKey,
                    elementInfo, WebActionElementKey,
                    [NSNumber numberWithInt:[event buttonNumber]], WebActionButtonKey,
                    [NSNumber numberWithInt:[event modifierFlags]], WebActionModifierFlagsKey,
                    URL, WebActionOriginalURLKey,
                    nil];
            }
        }
        
        // fall through
        
        default:
            return [NSDictionary dictionaryWithObjectsAndKeys:
                [NSNumber numberWithInt:navigationType], WebActionNavigationTypeKey,
                URL, WebActionOriginalURLKey,
                nil];
    }
}

-(NSDictionary *)_actionInformationForLoadType:(WebFrameLoadType)loadType isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL
{
    WebNavigationType navType;
    if (isFormSubmission) {
        navType = WebNavigationTypeFormSubmitted;
    } else if (event == nil) {
        if (loadType == WebFrameLoadTypeReload) {
            navType = WebNavigationTypeReload;
        } else if (loadType == WebFrameLoadTypeForward
                   || loadType == WebFrameLoadTypeBack
                   || loadType == WebFrameLoadTypeIndexedBackForward) {
            navType = WebNavigationTypeBackForward;
        } else {
            navType = WebNavigationTypeOther;
        }
    } else {
        navType = WebNavigationTypeLinkClicked;
    }
    return [self _actionInformationForNavigationType:navType event:event originalURL:URL];
}

- (void)_invalidatePendingPolicyDecisionCallingDefaultAction:(BOOL)call
{
    [_private->listener _invalidate];
    [_private->listener release];
    _private->listener = nil;

    NSURLRequest *request = _private->policyRequest;
    NSString *frameName = _private->policyFrameName;
    id target = _private->policyTarget;
    SEL selector = _private->policySelector;
    WebFormState *formState = _private->policyFormState;

    _private->policyRequest = nil;
    _private->policyFrameName = nil;
    _private->policyTarget = nil;
    _private->policySelector = nil;
    _private->policyFormState = nil;

    if (call) {
	if (frameName) {
	    [target performSelector:selector withObject:nil withObject:nil withObject:nil];
	} else {
	    [target performSelector:selector withObject:nil withObject:nil];
	}
    }

    [request release];
    [frameName release];
    [target release];
    [formState release];
}

- (void)_checkNewWindowPolicyForRequest:(NSURLRequest *)request action:(NSDictionary *)action frameName:(NSString *)frameName formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector
{
    WebPolicyDecisionListener *listener = [[WebPolicyDecisionListener alloc]
        _initWithTarget:self action:@selector(_continueAfterNewWindowPolicy:)];

    _private->policyRequest = [request retain];
    _private->policyTarget = [target retain];
    _private->policyFrameName = [frameName retain];
    _private->policySelector = selector;
    _private->listener = [listener retain];
    _private->policyFormState = [formState retain];

    WebView *c = [self webView];
    [[c _policyDelegateForwarder] webView:c decideNewWindowPolicyForAction:action
						                      andRequest:request
						                    newFrameName:frameName
						                decisionListener:listener];
    
    [listener release];
}

-(void)_continueAfterNewWindowPolicy:(WebPolicyAction)policy
{
    NSURLRequest *request = [[_private->policyRequest retain] autorelease];
    NSString *frameName = [[_private->policyFrameName retain] autorelease];
    id target = [[_private->policyTarget retain] autorelease];
    SEL selector = _private->policySelector;
    WebFormState *formState = [[_private->policyFormState retain] autorelease];

    // will release _private->policy* objects, hence the above retains
    [self _invalidatePendingPolicyDecisionCallingDefaultAction:NO];

    BOOL shouldContinue = NO;

    switch (policy) {
    case WebPolicyIgnore:
        break;
    case WebPolicyDownload:
	// FIXME: should download full request
        [[self webView] _downloadURL:[request URL]];
        break;
    case WebPolicyUse:
	shouldContinue = YES;
        break;
    default:
	ASSERT_NOT_REACHED();
    }

    [target performSelector:selector withObject:(shouldContinue ? request : nil) withObject:frameName withObject:formState];
}


- (void)_checkNavigationPolicyForRequest:(NSURLRequest *)request dataSource:(WebDataSource *)dataSource formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector
{
    NSDictionary *action = [dataSource _triggeringAction];
    if (action == nil) {
        action = [self _actionInformationForNavigationType:WebNavigationTypeOther event:nil originalURL:[request URL]];
        [dataSource _setTriggeringAction:action];
    }

    // Don't ask more than once for the same request
    if ([request isEqual:[dataSource _lastCheckedRequest]]) {
        [target performSelector:selector withObject:request withObject:nil];
        return;
    }

    // If we are loading the empty URL, don't bother to ask - clients
    // are likely to get confused.
    if ([[[request URL] absoluteString] length] == 0) {
        [target performSelector:selector withObject:request withObject:nil];
        return;
    }

    [dataSource _setLastCheckedRequest:request];

    WebPolicyDecisionListener *listener = [[WebPolicyDecisionListener alloc]
        _initWithTarget:self action:@selector(_continueAfterNavigationPolicy:)];

    _private->policyRequest = [request retain];
    _private->policyTarget = [target retain];
    _private->policySelector = selector;
    _private->listener = [listener retain];
    _private->policyFormState = [formState retain];

    WebView *c = [self webView];
    [[c _policyDelegateForwarder] webView:c decideNavigationPolicyForAction:action
                                                             andRequest:request
                                                                inFrame:self
                                                       decisionListener:listener];
    
    [listener release];
}

-(void)_continueAfterNavigationPolicy:(WebPolicyAction)policy
{
    NSURLRequest *request = [[_private->policyRequest retain] autorelease];
    id target = [[_private->policyTarget retain] autorelease];
    SEL selector = _private->policySelector;
    WebFormState *formState = [[_private->policyFormState retain] autorelease];

    // will release _private->policy* objects, hence the above retains
    [self _invalidatePendingPolicyDecisionCallingDefaultAction:NO];

    BOOL shouldContinue = NO;

    switch (policy) {
    case WebPolicyIgnore:
        break;
    case WebPolicyDownload:
	// FIXME: should download full request
        [[self webView] _downloadURL:[request URL]];
        break;
    case WebPolicyUse:
        if (![WebResource canInitWithRequest:request]) {
            [self _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowURL forURL:[request URL]];
        } else {
            shouldContinue = YES;
        }
        break;
    default:
	ASSERT_NOT_REACHED();
    }

    [target performSelector:selector withObject:(shouldContinue ? request : nil) withObject:formState];
}

-(void)_continueFragmentScrollAfterNavigationPolicy:(NSURLRequest *)request formState:(WebFormState *)formState
{
    if (!request) {
        return;
    }

    NSURL *URL = [request URL];
    WebDataSource *dataSrc = [self dataSource];

    BOOL isRedirect = _private->quickRedirectComing;
    _private->quickRedirectComing = NO;

    [dataSrc _setURL:URL];
    if (!isRedirect && ![self _shouldTreatURLAsSameAsCurrent:URL]) {
        // NB: must happen after _setURL, since we add based on the current request.
        // Must also happen before we openURL and displace the scroll position, since
        // adding the BF item will save away scroll state.

        // NB2:  If we were loading a long, slow doc, and the user anchor nav'ed before
        // it was done, currItem is now set the that slow doc, and prevItem is whatever was
        // before it.  Adding the b/f item will bump the slow doc down to prevItem, even
        // though its load is not yet done.  I think this all works out OK, for one because
        // we have already saved away the scroll and doc state for the long slow load,
        // but it's not an obvious case.
        [self _addBackForwardItemClippedAtTarget:NO];
    }
    
    [_private->bridge scrollToAnchorWithURL:[URL absoluteString]];
    
    if (!isRedirect) {
        // This will clear previousItem from the rest of the frame tree tree that didn't
        // doing any loading.  We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state
        [self _checkLoadComplete];
    }

    [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller locationChangedWithinPageForDataSource:dataSrc];
}

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request alwaysFromRequest: (BOOL)f
{
    [request HTTPSetUserAgent:[[self webView] userAgentForURL:[request URL]]];
    
    // Don't set the cookie policy URL if it's already been set.
    if ([request HTTPCookiePolicyBaseURL] == nil){
        if (self == [[self webView] mainFrame] || f) {
            [request HTTPSetCookiePolicyBaseURL:[request URL]];
        } else {
            [request HTTPSetCookiePolicyBaseURL:[[[[self webView] mainFrame] dataSource] _URL]];
        }
    }
}



-(void)_continueLoadRequestAfterNewWindowPolicy:(NSURLRequest *)request frameName:(NSString *)frameName formState:(WebFormState *)formState
{
    if (!request) {
        return;
    }
    
    WebView *controller = nil;
    WebView *currentController = [self webView];
    id wd = [currentController windowOperationsDelegate];
    if ([wd respondsToSelector:@selector(webView:createWindowWithRequest:)])
	controller = [wd webView:currentController createWindowWithRequest:nil];
    else
        controller = [[WebDefaultWindowOperationsDelegate sharedWindowOperationsDelegate] webView:currentController createWindowWithRequest:nil];
        
    [controller _setTopLevelFrameName:frameName];
    [[controller _windowOperationsDelegateForwarder] webViewShowWindow:controller];
    WebFrame *frame = [controller mainFrame];

    [frame _loadRequest:request triggeringAction:nil loadType:WebFrameLoadTypeStandard formState:formState];
}


// main funnel for navigating via callback from WebCore (e.g., clicking a link, redirect)
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer loadType:(WebFrameLoadType)loadType target:(NSString *)target triggeringEvent:(NSEvent *)event form:(id <WebDOMElement>)form formValues:(NSDictionary *)values
{
    BOOL isFormSubmission = (values != nil);

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [request HTTPSetReferrer:referrer];
    [self _addExtraFieldsToRequest:request alwaysFromRequest: (event != nil || isFormSubmission)];
    if (loadType == WebFrameLoadTypeReload) {
        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    }

    // I believe this is never called with LoadSame.  If it is, we probably want to set the cache
    // policy of LoadFromOrigin, but I didn't test that.
    ASSERT(loadType != WebFrameLoadTypeSame);

    NSDictionary *action = [self _actionInformationForLoadType:loadType isFormSubmission:isFormSubmission event:event originalURL:URL];
    WebFormState *formState = nil;
    if (form && values) {
        formState = [[WebFormState alloc] initWithForm:form values:values];
    }

    if (target != nil) {
	WebFrame *targetFrame = [self findFrameNamed:target];
	if (targetFrame != nil) {
	    [targetFrame _loadURL:URL referrer:referrer loadType:loadType target:nil triggeringEvent:event form:form formValues:values];
	} else {
	    [self _checkNewWindowPolicyForRequest:request action:action frameName:target formState:formState andCall:self withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
	}
	[request release];
	[formState release];
	return;
    }

    WebDataSource *oldDataSource = [[self dataSource] retain];

    BOOL sameURL = [self _shouldTreatURLAsSameAsCurrent:URL];

    // Make sure to do scroll to anchor processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (!isFormSubmission
        && loadType != WebFrameLoadTypeReload
        && loadType != WebFrameLoadTypeSame
        && ![_private->bridge isFrameSet]
        && [URL fragment]
        && [[URL _web_URLByRemovingFragment] isEqual:[[NSURL _web_URLWithString:[_private->bridge URL]] _web_URLByRemovingFragment]]) {
        
        // Just do anchor navigation within the existing content.
        
        // We don't do this if we are submitting a form, explicitly reloading,
        // currently displaying a frameset, or if the new URL does not have a fragment.
        // These rules are based on what KHTML was doing in KHTMLPart::openURL.
        
        // One reason we only do this if there is an anchor in the URL is that
        // this might prevent us from reloading a document that has subframes that are
        // different than what we're displaying (in other words, a link from within a
        // frame is trying to reload the frameset into _top).
        
        // FIXME: What about load types other than Standard and Reload?

        [oldDataSource _setTriggeringAction:action];
        [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];
        [self _checkNavigationPolicyForRequest:request dataSource:oldDataSource formState:formState andCall:self withSelector:@selector(_continueFragmentScrollAfterNavigationPolicy:formState:)];
    } else {
        [self _loadRequest:request triggeringAction:action loadType:loadType formState:formState];
        if (_private->quickRedirectComing) {
            _private->quickRedirectComing = NO;
            
            // need to transfer BF items from the dataSource that we're replacing
            WebDataSource *newDataSource = [self provisionalDataSource];
            [newDataSource _setIsClientRedirect:YES];
            [newDataSource _addBackForwardItems:[oldDataSource _backForwardItems]];
        } else if (sameURL) {
            // Example of this case are sites that reload the same URL with a different cookie
            // driving the generated content, or a master frame with links that drive a target
            // frame, where the user has clicked on the same link repeatedly.
            [self _setLoadType:WebFrameLoadTypeSame];
        }            
        [request release];
    }

    [oldDataSource release];
    [formState release];
}

- (void)_loadURL:(NSURL *)URL intoChild:(WebFrame *)childFrame
{
    WebHistoryItem *parentItem = [_private currentItem];
    NSArray *childItems = [parentItem children];
    WebFrameLoadType loadType = [self _loadType];
    WebFrameLoadType childLoadType = WebFrameLoadTypeInternal;
    WebHistoryItem *childItem = nil;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (childItems &&
        (loadType == WebFrameLoadTypeForward
         || loadType == WebFrameLoadTypeBack
         || loadType == WebFrameLoadTypeIndexedBackForward
         || loadType == WebFrameLoadTypeReload
         || loadType == WebFrameLoadTypeReloadAllowingStaleData))
    {
        childItem = [parentItem childItemWithName:[childFrame name]];
        if (childItem) {
            URL = [childItem URL];
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (loadType == WebFrameLoadTypeForward
                || loadType == WebFrameLoadTypeBack
                || loadType == WebFrameLoadTypeIndexedBackForward)
            {
                // For back/forward, remember this item so we can traverse any child items as child frames load
                [childFrame->_private setProvisionalItem:childItem];
            } else {
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                [childFrame->_private setCurrentItem:childItem];
            }
        }
    }

    // FIXME: is this the right referrer?
    [childFrame _loadURL:URL referrer:[[self _bridge] referrer] loadType:childLoadType target:nil triggeringEvent:nil form:nil formValues:nil];
}

- (void)_postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSData *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(id <WebDOMElement>)form formValues:(NSDictionary *)values
{
    // When posting, use the WebResourceHandleFlagLoadFromOrigin load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [self _addExtraFieldsToRequest:request alwaysFromRequest: YES];
    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    [request HTTPSetMethod:@"POST"];
    [request HTTPSetBody:data];
    [request HTTPSetContentType:contentType];
    [request HTTPSetReferrer:referrer];

    NSDictionary *action = [self _actionInformationForLoadType:WebFrameLoadTypeStandard isFormSubmission:YES event:event originalURL:URL];
    WebFormState *formState = nil;
    if (form && values) {
        formState = [[WebFormState alloc] initWithForm:form values:values];
    }

    if (target != nil) {
	WebFrame *targetFrame = [self findFrameNamed:target];

	if (targetFrame != nil) {
	    [targetFrame _postWithURL:URL referrer:referrer target:nil data:data contentType:contentType triggeringEvent:event form:form formValues:values];
	    return;
	}

	[self _checkNewWindowPolicyForRequest:request action:action frameName:target formState:formState andCall:self withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
	[request release];
	[formState release];
	return;
    }

    [self _loadRequest:request triggeringAction:action loadType:WebFrameLoadTypeStandard formState:formState];

    [request release];
    [formState release];
}

- (void)_clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory
{
    LOG(Redirect, "Client redirect to: %@", URL);

    [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller clientWillRedirectTo:URL delay:seconds fireDate:date forFrame:self];
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation.

    if (![self dataSource]) {
        // If we don't have a dataSource, we have no "original" load on which to base a redirect,
        // so we better just treat the redirect as a normal load.
        _private->quickRedirectComing = NO;
    } else {
        _private->quickRedirectComing = lockHistory;
    }
}

- (void)_clientRedirectCancelled
{
    [[[self webView] _locationChangeDelegateForwarder] webView: _private->controller clientRedirectCancelledForFrame:self];
    _private->quickRedirectComing = NO;
}

- (void)_saveScrollPositionToItem:(WebHistoryItem *)item
{
    if (item) {
        NSView *clipView = [[[self frameView] documentView] superview];
        // we might already be detached when this is called from detachFromParent, in which
        // case we don't want to override real data earlier gathered with (0,0)
        if (clipView) {
            [item setScrollPoint:[clipView bounds].origin];
        }
    }
}

- (void)_restoreScrollPosition
{
    ASSERT([_private currentItem]);
    [[[self frameView] documentView] scrollPoint:[[_private currentItem] scrollPoint]];
}

- (void)_scrollToTop
{
    [[[self frameView] documentView] scrollPoint: NSZeroPoint];
}

- (void)_textSizeMultiplierChanged
{
    [_private->bridge setTextSizeMultiplier:[[self webView] textSizeMultiplier]];
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

    NSMutableURLRequest *request = [[dataSource request] mutableCopy];
    [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    [request release];
    
    [newDataSource _setOverrideEncoding:encoding];

    [self _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReloadAllowingStaleData formState:nil];
    
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

- (void)_addFramePathToString:(NSMutableString *)path
{
    if ([_private->name hasPrefix:@"<!--framePath "]) {
        // we have a generated name - take the path from our name
        NSRange ourPathRange = {14, [_private->name length] - 14 - 3};
        [path appendString:[_private->name substringWithRange:ourPathRange]];
    } else {
        // we don't have a generated name - just add our simple name to the end
        if (_private->parent) {
            [_private->parent _addFramePathToString:path];
        }
        [path appendString:@"/"];
        if (_private->name) {
            [path appendString:_private->name];
        }
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

// If we bailed out of a b/f navigation, we need to set the b/f cursor back to the current
// item, because we optimistically move it right away at the start of the operation
- (void)_resetBackForwardListToCurrent {
    WebFrameLoadType loadType = [self _loadType];
    if ((loadType == WebFrameLoadTypeForward
        || loadType == WebFrameLoadTypeBack
        || loadType == WebFrameLoadTypeIndexedBackForward)
        && [_private currentItem]
        && self == [[self webView] mainFrame])
    {
        [[[self webView] backForwardList] goToItem:[_private currentItem]];
    }
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
    switch ([self _loadType]) {
        case WebFrameLoadTypeReload:
        case WebFrameLoadTypeReloadAllowingStaleData:
        case WebFrameLoadTypeSame:
            // Don't restore any form state on reload or loadSame
            return nil;
        case WebFrameLoadTypeBack:
        case WebFrameLoadTypeForward:
        case WebFrameLoadTypeIndexedBackForward:
        case WebFrameLoadTypeInternal:
        case WebFrameLoadTypeStandard:
            return [_private currentItem];
    }
    ASSERT_NOT_REACHED();
    return nil;
}

// Called after the FormsDelegate is done processing willSubmitForm:
-(void)_continueAfterWillSubmitForm:(WebPolicyAction)policy
{
    if (_private->listener) {
        [_private->listener _invalidate];
        [_private->listener release];
        _private->listener = nil;
    }
    [_private->provisionalDataSource _startLoading];
}

-(void)_continueLoadRequestAfterNavigationPolicy:(NSURLRequest *)request formState:(WebFormState *)formState
{
    if (!request) {
        [self _resetBackForwardListToCurrent];
        [self _setLoadType: WebFrameLoadTypeStandard];
        [self _setProvisionalDataSource:nil];
        return;
    }
    
    // We tell the documentView provisionalDataSourceChanged:
    // once it has been created by the controller.
    
    [self _setState: WebFrameStateProvisional];
    
    if (self == [[self webView] mainFrame])
        LOG(DocumentLoad, "loading %@", [[[self provisionalDataSource] request] URL]);

    WebFrameLoadType loadType = [self _loadType];
    if ((loadType == WebFrameLoadTypeForward ||
        loadType == WebFrameLoadTypeBack ||
        loadType == WebFrameLoadTypeIndexedBackForward) &&
        [[_private provisionalItem] hasPageCache]){
        NSDictionary *pageCache = [[_private provisionalItem] pageCache];
        if ([pageCache objectForKey:@"WebCorePageState"]){
            LOG (PageCache, "Restoring page from back/forward cache, %s\n", [[[[_private provisionalItem] URL] absoluteString] cString]);
            [_private->provisionalDataSource _startLoading: pageCache];
        }
    } else {
        if (formState) {
            // It's a bit of a hack to reuse the WebPolicyDecisionListener for the continuation
            // mechanism across the willSubmitForm callout.
            _private->listener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(_continueAfterWillSubmitForm:)];
            [[[self webView] _formDelegate] frame:self willSubmitForm:[formState form] withValues:[formState values] submissionListener:_private->listener];
        } else {
            [self _continueAfterWillSubmitForm:WebPolicyUse];
        }
    }
}

- (void)_loadDataSource:(WebDataSource *)newDataSource withLoadType:(WebFrameLoadType)loadType formState:(WebFormState *)formState
{
    ASSERT([self webView] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.

    ASSERT([self frameView] != nil);

    [self stopLoading];

    [self _setLoadType:loadType];

    if ([self parent]) {
        [newDataSource _setOverrideEncoding:[[[self parent] dataSource] _overrideEncoding]];
    }
    [newDataSource _setController:[self webView]];
    [newDataSource _setJustOpenedForTargetedLink:_private->justOpenedForTargetedLink];
    _private->justOpenedForTargetedLink = NO;

    [self _setProvisionalDataSource:newDataSource];
    
    ASSERT([newDataSource webFrame] == self);

    [self _checkNavigationPolicyForRequest:[newDataSource request] dataSource:newDataSource formState:formState andCall:self withSelector:@selector(_continueLoadRequestAfterNavigationPolicy:formState:)];
}

- (void)_setJustOpenedForTargetedLink:(BOOL)justOpened
{
    _private->justOpenedForTargetedLink = justOpened;
}

- (void)_setProvisionalDataSource: (WebDataSource *)d
{
    if (_private->provisionalDataSource != _private->dataSource) {
	[_private->provisionalDataSource _setWebFrame:nil];
    }
    [_private setProvisionalDataSource: d];
    [d _setWebFrame:self];
}

// used to decide to use loadType=Same
- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL
{
    WebHistoryItem *item = [_private currentItem];
    NSString* URLString = [URL absoluteString];
    return [URLString isEqual:[item URLString]] || [URLString isEqual:[item originalURLString]];
}    

- (void)_loadRequest:(NSURLRequest *)request inFrameNamed:(NSString *)frameName
{
    if (frameName == nil) {
	[self loadRequest:request];
	return;
    }

    WebFrame *frame = [self findFrameNamed:frameName];
    
    if (frame != nil) {
	[frame loadRequest:request];
	return;
    }

    NSDictionary *action = [self _actionInformationForNavigationType:WebNavigationTypeOther event:nil originalURL:[request URL]];
    [self _checkNewWindowPolicyForRequest:request action:(NSDictionary *)action frameName:frameName formState:nil andCall:self withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
}

@end

@implementation WebFormState : NSObject

- (id)initWithForm:(NSObject <WebDOMElement> *)form values:(NSDictionary *)values
{
    [super init];
    _form = [form retain];
    _values = [values copy];
    return self;
}

- (void)dealloc
{
    [_form release];
    [_values release];
    [super dealloc];
}

- (id <WebDOMElement>)form
{
    return _form;
}

- (NSDictionary *)values
{
    return _values;
}

@end
