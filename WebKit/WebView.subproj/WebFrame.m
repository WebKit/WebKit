/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebFrameInternal.h>

#import <WebKit/DOM.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebFormDataStream.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebHistoryPrivate.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNetscapePluginDocumentView.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPluginDocumentView.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebUIDelegate.h>

#import <objc/objc-runtime.h>

#ifndef NDEBUG
static const char * const stateNames[] = {
    "WebFrameStateProvisional",
    "WebFrameStateCommittedPage",
    "WebFrameStateLayoutAcceptable",
    "WebFrameStateComplete"
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

NSString *WebPageCacheEntryDateKey = @"WebPageCacheEntryDateKey";
NSString *WebPageCacheDataSourceKey = @"WebPageCacheDataSourceKey";
NSString *WebPageCacheDocumentViewKey = @"WebPageCacheDocumentViewKey";

@interface NSObject (WebExtraPerformMethod)

- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2 withObject:(id)object3;

@end

@implementation NSObject (WebExtraPerformMethod)

- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2 withObject:(id)object3
{
    return objc_msgSend(self, aSelector, object1, object2, object3);
}

@end

@interface NSArray (WebSafeMakePerform)

- (void)_web_safeMakeObjectsPerformSelector:(SEL)aSelector;

@end


@implementation NSArray (WebSafeMakePerform)

- (void)_web_safeMakeObjectsPerformSelector:(SEL)aSelector
{
    unsigned count = [self count];
    if (0 == count)
        return;

    if (count > 128) {
        [[self copy] makeObjectsPerformSelector:aSelector];
        return;
    }
 
    id batch[128];
    [self getObjects:batch range:NSMakeRange(0, count)];
    unsigned i;
    for (i = 0; i < count; i++) {
        objc_msgSend(batch[i], aSelector);
    }
}

@end

// One day we might want to expand the use of this kind of class such that we'd receive one
// over the bridge, and possibly hand it on through to the FormsDelegate.
// Today it is just used internally to keep some state as we make our way through a bunch
// layers while doing a load.
@interface WebFormState : NSObject
{
    DOMElement *_form;
    NSDictionary *_values;
    WebFrame *_sourceFrame;
}
- (id)initWithForm:(DOMElement *)form values:(NSDictionary *)values sourceFrame:(WebFrame *)sourceFrame;
- (DOMElement *)form;
- (NSDictionary *)values;
- (WebFrame *)sourceFrame;
@end

@interface WebFrame (ForwardDecls)
- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(WebFrameLoadType)loadType formState:(WebFormState *)formState;
- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;
- (NSDictionary *)_actionInformationForLoadType:(WebFrameLoadType)loadType isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL;

- (void)_saveScrollPositionAndViewStateToItem:(WebHistoryItem *)item;
- (void)_restoreScrollPositionAndViewState;

- (WebHistoryItem *)_createItem: (BOOL)useOriginal;
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;
- (WebHistoryItem *)_currentBackForwardListItemToResetTo;
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
    [name release];
    [webFrameView release];
    [dataSource release];
    [provisionalDataSource release];
    [bridge release];
    [children release];

    [currentItem release];
    [provisionalItem release];
    [previousItem release];
    
    [scriptDebugger release];
    
    ASSERT(listener == nil);
    ASSERT(policyRequest == nil);
    ASSERT(policyFrameName == nil);
    ASSERT(policyTarget == nil);
    ASSERT(policyFormState == nil);
    ASSERT(policyDataSource == nil);

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

- (WebView *)webView { return webView; }
- (void)setWebView: (WebView *)wv
{
    webView = wv; // not retained (yet)
}

- (WebDataSource *)provisionalDataSource { return provisionalDataSource; }
- (void)setProvisionalDataSource: (WebDataSource *)d
{
    ASSERT(!d || !provisionalDataSource);
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

- (NSURLRequest *)_webDataRequestForData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName: (NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL
{
    NSURL *fakeURL = [NSURL _web_uniqueWebDataURL];
    NSMutableURLRequest *request = [[[NSMutableURLRequest alloc] initWithURL:fakeURL] autorelease];
    [request _webDataRequestSetData:data];
    [request _webDataRequestSetEncoding:encodingName];
    [request _webDataRequestSetBaseURL:URL];
    [request _webDataRequestSetUnreachableURL:unreachableURL];
    [request _webDataRequestSetMIMEType: MIMEType ? MIMEType : (NSString *)@"text/html"];
    return request;
}

- (BOOL)_shouldReloadToHandleUnreachableURLFromRequest:(NSURLRequest *)request
{
    NSURL *unreachableURL = [request _webDataRequestUnreachableURL];
    if (unreachableURL == nil) {
        return NO;
    }
    
    if (_private->policyLoadType != WebFrameLoadTypeForward
        && _private->policyLoadType != WebFrameLoadTypeBack
        && _private->policyLoadType != WebFrameLoadTypeIndexedBackForward) {
        return NO;
    }
    
    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    WebDataSource *compareDataSource = nil;
    if (_private->delegateIsDecidingNavigationPolicy || _private->delegateIsHandlingUnimplementablePolicy) {
        compareDataSource = _private->policyDataSource;
    } else if (_private->delegateIsHandlingProvisionalLoadError) {
        compareDataSource = [self provisionalDataSource];
    }
    
    return compareDataSource != nil && [unreachableURL isEqual:[[compareDataSource request] URL]];
}

- (void)_loadRequest:(NSURLRequest *)request subresources:(NSArray *)subresources subframeArchives:(NSArray *)subframeArchives
{
    WebFrameLoadType loadType;
    
    // note this copies request
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:request];
    NSMutableURLRequest *r = [newDataSource request];
    [self _addExtraFieldsToRequest:r alwaysFromRequest: NO];
    if ([self _shouldTreatURLAsSameAsCurrent:[request URL]]) {
        [r setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        loadType = WebFrameLoadTypeSame;
    } else {
        loadType = WebFrameLoadTypeStandard;
    }
    
    [newDataSource _setOverrideEncoding:[[self dataSource] _overrideEncoding]];
    [newDataSource _addSubresources:subresources];
    [newDataSource _addSubframeArchives:subframeArchives];
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the b/f list, we treat it as a reload so the b/f list 
    // is appropriately maintained.
    if ([self _shouldReloadToHandleUnreachableURLFromRequest:request]) {
        ASSERT(loadType == WebFrameLoadTypeStandard);
        loadType = WebFrameLoadTypeReload;
    }
    
    [self _loadDataSource:newDataSource withLoadType:loadType formState:nil];
    [newDataSource release];
}

- (void)_setWebView:(WebView *)v
{
    // To set to nil, we have to use _detachFromParent, not this.
    ASSERT(v);
    [_private setWebView:v];
}

// helper method used in various nav cases below
- (void)_addBackForwardItemClippedAtTarget:(BOOL)doClip
{
    if ([[self dataSource] _URLForHistory] != nil) {
        WebHistoryItem *bfItem = [[[self webView] mainFrame] _createItemTreeWithTargetFrame:self clippedAtTarget:doClip];
        LOG (BackForward, "for frame %@, adding item  %@\n", [self name], bfItem);
        [[[self webView] backForwardList] addItem:bfItem];
    }
}

- (WebHistoryItem *)_createItem: (BOOL)useOriginal
{
    WebDataSource *dataSrc = [self dataSource];
    NSURLRequest *request;
    NSURL *unreachableURL = [dataSrc unreachableURL];
    NSURL *URL;
    NSURL *originalURL;
    WebHistoryItem *bfItem;

    if (useOriginal) {
        request = [dataSrc _originalRequest];
    } else {
        request = [dataSrc request];
    }

    if (unreachableURL != nil) {
        URL = unreachableURL;
        originalURL = unreachableURL;
    } else {
        URL = [request URL];
        originalURL = [[dataSrc _originalRequest] URL];
    }

    LOG (History, "creating item for %@", request);
    
    // Frames that have never successfully loaded any content
    // may have no URL at all. Currently our history code can't
    // deal with such things, so we nip that in the bud here.
    // Later we may want to learn to live with nil for URL.
    // See bug 3368236 and related bugs for more information.
    if (URL == nil) {
        URL = [NSURL URLWithString:@"about:blank"];
    }
    if (originalURL == nil) {
        originalURL = [NSURL URLWithString:@"about:blank"];
    }
    
    bfItem = [[[WebHistoryItem alloc] initWithURL:URL target:[self name] parent:[[self parentFrame] name] title:[dataSrc pageTitle]] autorelease];
    [bfItem setOriginalURLString:[originalURL _web_originalDataAsString]];

    // save form state if this is a POST
    [bfItem _setFormInfoFromRequest:request];

    // Set the item for which we will save document state
    [_private setPreviousItem:[_private currentItem]];
    [_private setCurrentItem:bfItem];

    return bfItem;
}

/*
    In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  
    The item that was the target of the user's navigation is designated as the "targetItem".  
    When this method is called with doClip=YES we're able to create the whole tree except for the target's children, 
    which will be loaded in the future.  That part of the tree will be filled out as the child loads are committed.
*/
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip
{
    WebHistoryItem *bfItem = [self _createItem: [self parentFrame]?YES:NO];

    [self _saveScrollPositionAndViewStateToItem:[_private previousItem]];
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

- (BOOL)_isDescendantOfFrame:(WebFrame *)frame
{
    if (self == frame)
        return YES;
    
    NSArray *children = [frame _internalChildFrames];
    unsigned i;
    for (i = 0; i < [children count]; i++) {
        WebFrame *child = [children objectAtIndex:i];
        if (self == child || [self _isDescendantOfFrame:child]) {
            return YES;
        }
    }
    return NO;
}

- (BOOL)_isFrameSet
{
    return [_private->bridge isFrameSet];
}

- (BOOL)_shouldAllowAccessFrom:(WebFrame *)source
{
    // if no source frame, allow access
    if (source == nil) {
        return YES;
    }

    //   - allow access if the two frames are in the same window
    if ([self webView] == [source webView]) {
        return YES;
    }

    //   - allow if the request is made from a local file.
    NSString *sourceDomain = [[source _bridge] domain];
    if ([sourceDomain length] == 0) {
        return YES;
    }

    //   - allow access if this frame or one of its ancestors
    //     has the same origin as source
    WebFrame *ancestor = self;
    while (ancestor != nil) {
        NSString *ancestorDomain = [[ancestor _bridge] domain];
        if (ancestorDomain != nil && [sourceDomain _webkit_isCaseInsensitiveEqualToString:ancestorDomain]) {
            return YES;
        }
        ancestor = [ancestor parentFrame];
    }

    //   - allow access if this frame is a toplevel window and the source
    //     can access its opener. Note that we only allow one level of
    //     recursion here.
    if ([self parentFrame] == nil) {
        NSString *openerDomain = [[[self _bridge] opener] domain];
        if (openerDomain != nil && [sourceDomain _webkit_isCaseInsensitiveEqualToString:openerDomain]) {
            return YES;
        }
    }

    // otherwise deny access
    return NO;
}


- (WebFrame *)_descendantFrameNamed:(NSString *)name sourceFrame:(WebFrame *)source
{
    // for security reasons, we do not want to even make frames visible to frames that
    // can't access them 
    if ([[self name] isEqualToString: name] && [self _shouldAllowAccessFrom:source]) {
        return self;
    }

    // It's OK to use the internal version of getting the child
    // frames, since we know this method won't change the set of
    // frames
    NSArray *children = [self _internalChildFrames];
    WebFrame *frame;
    unsigned i;

    for (i = 0; i < [children count]; i++){
        frame = [children objectAtIndex: i];
        frame = [frame _descendantFrameNamed:name sourceFrame:source];
        if (frame){
            return frame;
        }
    }

    return nil;
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
        [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView willCloseFrame:self];
    }
}

- (void)_detachFromParent
{
    WebBridge *bridge = _private->bridge;
    _private->bridge = nil;

    [self stopLoading];
    [self _saveScrollPositionAndViewStateToItem:[_private currentItem]];

    [bridge closeURL];

    [self _detachChildren];

    [_private setWebView:nil];
    [_private->webFrameView _setWebView:nil];
    [_private->dataSource _setWebView:nil];
    [_private->provisionalDataSource _setWebView:nil];

    [self _setDataSource:nil];
    [_private setWebFrameView:nil];

    [bridge close];
    
    [bridge release];
}

- (void)_setDataSource:(WebDataSource *)ds
{
    if (ds == nil && _private->dataSource == nil) {
        return;
    }

    ASSERT(ds != _private->dataSource);

    if (_private->dataSource) {
        // Make sure that any work that is triggered by resigning first reponder can get done.
        // The main example where this came up is the textDidEndEditing that is sent to the
        // FormsDelegate (3223413).  We need to do this before _detachChildren, since that will
        // remove the views as a side-effect of freeing the bridge, at which point we can't
        // post the FormDelegate messages.
        //
        // Note that this can also take FirstResponder away from a child of our frameView that
        // is not in a child frame's view.  This is OK because we are in the process
        // of loading new content, which will blow away all editors in this top frame, and if
        // a non-editor is firstReponder it will not be affected by endEditingFor:.
        // Potentially one day someone could write a DocView whose editors were not all
        // replaced by loading new content, but that does not apply currently.
        NSView *frameView = [self frameView];
        NSWindow *window = [frameView window];
        NSResponder *firstResp = [window firstResponder];
        if ([firstResp isKindOfClass:[NSView class]]
            && [(NSView *)firstResp isDescendantOf:frameView])
        {
            [window endEditingFor:firstResp];
        }

        [self _detachChildren];

        [_private->dataSource _setWebFrame:nil];
    } else {
        ASSERT(!_private->children);
    }

    [_private setDataSource:ds];
    [ds _setWebView:[self webView]];
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

- (void)_transitionToLayoutAcceptable
{
    switch ([self _state]) {
        case WebFrameStateCommittedPage:
        {
            [self _setState: WebFrameStateLayoutAcceptable];
            if (!([[self dataSource] _isDocumentHTML])) {
                // Go ahead and lay out/display non-HTML the minute we have some data.  This makes
                // more sense for text files (which can always be immediately displayed).
                WebFrameView *thisView = [self frameView];
                NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
                ASSERT(thisDocumentView != nil);
                [thisDocumentView setNeedsLayout:YES];
                [thisDocumentView layout];
                [thisDocumentView setNeedsDisplay:YES];
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
    [self _updateDrawsBackground];
    [_private->bridge installInFrame:[v _scrollView]];

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:_private->dataSource];
}

- (void)_receivedMainResourceError:(NSError *)error
{
    if ([self _state] == WebFrameStateProvisional) {
        NSURL *failedURL = [[_private->provisionalDataSource _originalRequest] URL];
        // When we are pre-commit, the currentItem is where the pageCache data resides
        NSDictionary *pageCache = [[_private currentItem] pageCache];
        [[self _bridge] didNotOpenURL:failedURL pageCache:pageCache];
        // We're assuming that WebCore invalidates its pageCache state in didNotOpen:pageCache:
        [[_private currentItem] setHasPageCache:NO];
    }
}

- (void)_transitionToCommitted: (NSDictionary *)pageCache
{
    ASSERT([self webView] != nil);

    switch ([self _state]) {
        case WebFrameStateProvisional:
        {
            [[[[self frameView] _scrollView] contentView] setCopiesOnScroll:YES];

            WebFrameLoadType loadType = [self _loadType];
            if (loadType == WebFrameLoadTypeForward ||
                loadType == WebFrameLoadTypeBack ||
                loadType == WebFrameLoadTypeIndexedBackForward ||
                (loadType == WebFrameLoadTypeReload && [_private->provisionalDataSource unreachableURL] != nil))
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

            [[self _bridge] closeURL];

            // Set the committed data source on the frame.
            [self _setDataSource:_private->provisionalDataSource];
                
            [self _setProvisionalDataSource: nil];

            [self _setState: WebFrameStateCommittedPage];
        
            // Handle adding the URL to the back/forward list.
            WebDataSource *ds = [self dataSource];
            NSString *ptitle = [ds pageTitle];

            switch (loadType) {
            case WebFrameLoadTypeForward:
            case WebFrameLoadTypeBack:
            case WebFrameLoadTypeIndexedBackForward:
                if ([[self webView] backForwardList]) {
                    // Must grab the current scroll position before disturbing it
                    [self _saveScrollPositionAndViewStateToItem:[_private previousItem]];
                    
                    // Create a document view for this document, or used the cached view.
                    if (pageCache){
                        NSView <WebDocumentView> *cachedView = [pageCache objectForKey: WebPageCacheDocumentViewKey];
                        ASSERT(cachedView != nil);
                        [[self frameView] _setDocumentView: cachedView];
                    }
                    else
                        [self _makeDocumentView];
                }
                break;

            case WebFrameLoadTypeReload:
            case WebFrameLoadTypeSame:
            {
                WebHistoryItem *currItem = [_private currentItem];
                LOG(PageCache, "Clearing back/forward cache, %@\n", [currItem URL]);
                // FIXME: rjw sez this cache clearing is no longer needed
                [currItem setHasPageCache:NO];
                if (loadType == WebFrameLoadTypeReload) {
                    [self _saveScrollPositionAndViewStateToItem:currItem];
                }
                NSURLRequest *request = [ds request];
                if ([request _webDataRequestUnreachableURL] == nil) {
                    // Sometimes loading a page again leads to a different result because of cookies.  Bugzilla 4072
                    [currItem setURL:[request URL]];
                }
                // Update the last visited time.  Mostly interesting for URL autocompletion
                // statistics.
                NSURL *URL = [[[ds _originalRequest] URL] _webkit_canonicalize];
                WebHistoryItem *oldItem = [[WebHistory optionalSharedHistory] itemForURL:URL];
                if (oldItem) {
                    [oldItem _setLastVisitedTimeInterval:[NSDate timeIntervalSinceReferenceDate]];
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
                    // Add item to history and BF list
                    NSURL *URL = [ds _URLForHistory];
                    if (URL && ![URL _web_isEmpty]){
                        ASSERT([self webView]);
                        if (![[[self webView] preferences] privateBrowsingEnabled]) {
                            WebHistoryItem *entry = [[WebHistory optionalSharedHistory] addItemForURL:URL];
                            if (ptitle)
                                [entry setTitle: ptitle];                            
                        }
                        [self _addBackForwardItemClippedAtTarget:YES];
                    }

                } else {
                    NSURLRequest *request = [ds request];
                    
                    // update the URL in the BF list that we made before the redirect, unless
                    // this is alternate content for an unreachable URL (we want the BF list
                    // item to remember the unreachable URL in case it becomes reachable later)
                    if ([request _webDataRequestUnreachableURL] == nil) {
                        [[_private currentItem] setURL:[request URL]];

                        // clear out the form data so we don't repost it to the wrong place if we
                        // ever go back/forward to this item
                        [[_private currentItem] _setFormInfoFromRequest:request];

                        // We must also clear out form data so we don't try to restore it into the incoming page,
                        // see -_opened
                    }
                }
                [self _makeDocumentView];
                break;
                
            case WebFrameLoadTypeInternal:
                // Add an item to the item tree for this frame
                ASSERT(![ds _isClientRedirect]);
                WebFrame *parentFrame = [self parentFrame];
                if (parentFrame) {
                    WebHistoryItem *parentItem = [parentFrame->_private currentItem];
                    // The only case where parentItem==nil should be when a parent frame loaded an
                    // empty URL, which doesn't set up a current item in that parent.
                    if (parentItem) {
                        [parentItem addChildItem:[self _createItem: YES]];
                    }
                } else {
                    // See 3556159.  It's not clear if it's valid to be in WebFrameLoadTypeOnLoadEvent
                    // for a top-level frame, but that was a likely explanation for those crashes,
                    // so let's guard against it.
                    // ...and all WebFrameLoadTypeOnLoadEvent uses were folded to WebFrameLoadTypeInternal
                    ERROR("no parent frame in _transitionToCommitted:, loadType=%d", loadType);
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
            [[self webView] _didCommitLoadForFrame: self];
            [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView didCommitLoadForFrame:self];
            
            // If we have a title let the WebView know about it.
            if (ptitle) {
                [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                                                           didReceiveTitle:ptitle
                                                                  forFrame:self];
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

- (BOOL)_canCachePage
{
    return [[[self webView] backForwardList] _usesPageCache];
}

- (void)_purgePageCache
{
    // This method implements the rule for purging the page cache.
    unsigned sizeLimit = [[[self webView] backForwardList] pageCacheSize];
    unsigned pagesCached = 0;
    WebBackForwardList *backForwardList = [[self webView] backForwardList];
    NSArray *backList = [backForwardList backListWithLimit: 999999];
    WebHistoryItem *oldestNonSnapbackItem = nil;
    
    unsigned i;
    for (i = 0; i < [backList count]; i++){
        WebHistoryItem *item = [backList objectAtIndex: i];
        if ([item hasPageCache]){
            if (oldestNonSnapbackItem == nil && ![item alwaysAttemptToUsePageCache])
                oldestNonSnapbackItem = item;
            pagesCached++;
        }
    }

    // Snapback items are never directly purged here.
    if (pagesCached >= sizeLimit) {
        LOG(PageCache, "Purging back/forward cache, %@\n", [oldestNonSnapbackItem URL]);
        [oldestNonSnapbackItem setHasPageCache:NO];
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

- (BOOL)_createPageCacheForItem:(WebHistoryItem *)item
{
    NSMutableDictionary *pageCache;

    [item setHasPageCache: YES];

    if (![_private->bridge saveDocumentToPageCache]){
        [item setHasPageCache: NO];
        return NO;
    }
    else {
        pageCache = [item pageCache];
        [[self dataSource] _setStoredInPageCache: YES];
        [pageCache setObject: [NSDate date]  forKey: WebPageCacheEntryDateKey];
        [pageCache setObject: [self dataSource] forKey: WebPageCacheDataSourceKey];
        [pageCache setObject: [[self frameView] documentView] forKey: WebPageCacheDocumentViewKey];
    }
    return YES;
}

- (void)_setState: (WebFrameState)newState
{
    LOG(Loading, "%@:  transition from %s to %s", [self name], stateNames[_private->state], stateNames[newState]);
    if ([self webView])
        LOG(Timing, "%@:  transition from %s to %s, %f seconds since start of document load", [self name], stateNames[_private->state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[self webView] mainFrame] dataSource] _loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && self == [[self webView] mainFrame]){
        LOG(DocumentLoad, "completed %@ (%f seconds)", [[[self dataSource] request] URL], CFAbsoluteTimeGetCurrent() - [[self dataSource] _loadingStartedTime]);
    }
    
    _private->state = newState;
    
    if (_private->state == WebFrameStateProvisional) {
        [_private->bridge provisionalLoadStarted];
    
        // FIXME: This is OK as long as no one resizes the window,
        // but in the case where someone does, it means garbage outside
        // the occupied part of the scroll view.
        [[[self frameView] _scrollView] setDrawsBackground:NO];

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
            && ![[self dataSource] _isStopping])
        {
            if ([[[self dataSource] representation] isKindOfClass: [WebHTMLRepresentation class]]) {
                if (![item pageCache]){

                    // Add the items to this page's cache.
                    if ([self _createPageCacheForItem:item]) {
                        LOG(PageCache, "Saving page to back/forward cache, %@\n", [[self dataSource] _URL]);

                        // See if any page caches need to be purged after the addition of this
                        // new page cache.
                        [self _purgePageCache];
                    }
                    else {
                        LOG(PageCache, "NOT saving page to back/forward cache, unable to create items, %@\n", [[self dataSource] _URL]);
                    }
                }
            }
            else {
                // Put the document into a null state, so it can be restored correctly.
                [_private->bridge clear];
            }
        }
        else {
            LOG(PageCache, "NOT saving page to back/forward cache, %@\n", [[self dataSource] _URL]);
        }
    }
    
    if (_private->state == WebFrameStateComplete) {
        NSScrollView *sv = [[self frameView] _scrollView];
        if ([[self webView] drawsBackground])
            [sv setDrawsBackground:YES];
        [_private setPreviousItem:nil];
        _timeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();

        [[self dataSource] _stopRecordingResponses];
    }
}

// Called after we send an openURL:... down to WebCore.
- (void)_opened
{
    if ([self _loadType] == WebFrameLoadTypeStandard && [[self dataSource] _isClientRedirect]) {
        // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
        // khtml has closed the URL and saved away the form state.
        WebHistoryItem *item = [_private currentItem];
        [item setDocumentState:nil];
        [item setScrollPoint:NSZeroPoint];
    }

    if ([[self dataSource] _loadingFromPageCache]){
        // Force a layout to update view size and thereby update scrollbars.
        NSView <WebDocumentView> *view = [[self frameView] documentView];
        if ([view isKindOfClass:[WebHTMLView class]]) {
            [(WebHTMLView *)view setNeedsToApplyStyles:YES];
        }
        [view setNeedsLayout: YES];
        [view layout];

        NSArray *responses = [[self dataSource] _responses];
        NSURLResponse *response;
        int i, count = [responses count];
        for (i = 0; i < count; i++){
            response = [responses objectAtIndex: i];
            // FIXME: If the WebKit client changes or cancels the request, this is not respected.
            NSError *error;
            id identifier;
            NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[response URL]];
            [self _requestFromDelegateForRequest:request identifier:&identifier error:&error];
            [self _sendRemainingDelegateMessagesWithIdentifier:identifier response:response length:[response expectedContentLength] error:error];
            [request release];
        }
        
        // Release the resources kept in the page cache.  They will be
        // reset when we leave this page.  The core side of the page cache
        // will have already been invalidated by the bridge to prevent
        // premature release.
        [[_private currentItem] setHasPageCache: NO];

        [[self dataSource] _setPrimaryLoadComplete: YES];
        [self _checkLoadCompleteForThisFrame];
    }
}

- (void)_checkLoadCompleteForThisFrame
{
    ASSERT([self webView] != nil);

    switch ([self _state]) {
        case WebFrameStateProvisional:
        {
            WebDataSource *pd = [self provisionalDataSource];
            
            LOG(Loading, "%@:  checking complete in WebFrameStateProvisional", [self name]);
            // If we've received any errors we may be stuck in the provisional state and actually
            // complete.
            NSError *error = [pd _mainDocumentError];
            if (error != nil) {
                // Check all children first.
                LOG(Loading, "%@:  checking complete, current state WebFrameStateProvisional", [self name]);
                WebHistoryItem *resetItem = [self _currentBackForwardListItemToResetTo];
                BOOL shouldReset = YES;
                if (![pd isLoading]) {
                    LOG(Loading, "%@:  checking complete in WebFrameStateProvisional, load done", [self name]);
                    [[self webView] _didFailProvisionalLoadWithError:error forFrame:self];
                    _private->delegateIsHandlingProvisionalLoadError = YES;
                    [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                                          didFailProvisionalLoadWithError:error
                                                                 forFrame:self];
                    _private->delegateIsHandlingProvisionalLoadError = NO;
                    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:error];
                    
                    [pd _stopLoading];
                    // Finish resetting the load state, but only if another load hasn't been started by the
                    // delegate callback.
                    if (pd == _private->provisionalDataSource) {
                        [self _setProvisionalDataSource:nil];
                        
                        [[self webView] _progressCompleted: self];
                        
                        [self _setState:WebFrameStateComplete];
                    } else {
                        NSURL *unreachableURL = [_private->provisionalDataSource unreachableURL];
                        if (unreachableURL != nil && [unreachableURL isEqual:[[pd request] URL]]) {
                            shouldReset = NO;
                        }
                    }
                }
                if (shouldReset && resetItem != nil) {
                    [[[self webView] backForwardList] goToItem:resetItem];
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

                // FIXME: Is this subsequent work important if we already navigated away?
                // Maybe there are bugs because of that, or extra work we can skip because
                // the new page is ready.

                // Tell the just loaded document to layout.  This may be necessary
                // for non-html content that needs a layout message.
                if (!([[self dataSource] _isDocumentHTML])) {
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
                        [self _restoreScrollPositionAndViewState];
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

                NSError *error = [ds _mainDocumentError];
                if (error != nil) {
                    [[self webView] _didFailLoadWithError:error forFrame:self];
                    [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                                                     didFailLoadWithError:error
                                                                 forFrame:self];
                    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:error];
                } else {
                    [[self webView] _didFinishLoadForFrame:self];
                    [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                                                    didFinishLoadForFrame:self];
                    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];
                }
                
                [[self webView] _progressCompleted: self];
 
                return;
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

- (void)_recursiveCheckLoadComplete
{
    // Checking for load complete may indeed alter the set of child
    // frames. However, _web_safeMakeObjectsPerformSelector: makes
    // sure to copy the array so it is safe against changes.
    [[self _internalChildFrames] _web_safeMakeObjectsPerformSelector:@selector(_recursiveCheckLoadComplete)];
    [self _checkLoadCompleteForThisFrame];
}

// Called every time a resource is completely loaded, or an error is received.
- (void)_checkLoadComplete
{
    ASSERT([self webView] != nil);

    // Now walk the frame tree to see if any frame that may have initiated a load is done.
    [[[self webView] mainFrame] _recursiveCheckLoadComplete];
}

- (WebBridge *)_bridge
{
    return _private->bridge;
}

- (void)_handleUnimplementablePolicyWithErrorCode:(int)code forURL:(NSURL *)URL
{
    NSError *error = [NSError _webKitErrorWithDomain:WebKitErrorDomain code:code URL:URL];
    WebView *wv = [self webView];
    _private->delegateIsHandlingUnimplementablePolicy = YES;
    [[wv _policyDelegateForwarder] webView:wv unableToImplementPolicyWithError:error frame:self];    
    _private->delegateIsHandlingUnimplementablePolicy = NO;
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

- (BOOL)_shouldReloadForCurrent:(NSURL *)currentURL andDestination:(NSURL *)destinationURL
{
    return !(([currentURL fragment] || [destinationURL fragment]) &&
    [[currentURL _webkit_URLByRemovingFragment] isEqual: [destinationURL _webkit_URLByRemovingFragment]]);
}

// Walk the frame tree and ensure that the URLs match the URLs in the item.
- (BOOL)_URLsMatchItem:(WebHistoryItem *)item
{
    NSURL *currentURL = [[[self dataSource] request] URL];

    if (![[[item URL] _webkit_URLByRemovingFragment] isEqual:[currentURL _webkit_URLByRemovingFragment]])
        return NO;
    
    NSArray *childItems = [item children];
    WebHistoryItem *childItem;
    WebFrame *childFrame;
    int i, count = [childItems count];
    for (i = 0; i < count; i++){
        childItem = [childItems objectAtIndex:i];
        childFrame = [self _immediateChildFrameNamed:[childItem target]];
        if (![childFrame _URLsMatchItem: childItem])
            return NO;
    }
    
    return YES;
}

// loads content into this frame, as specified by item
- (void)_loadItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)loadType
{
    NSURL *itemURL = [item URL];
    NSURL *itemOriginalURL = [NSURL _web_URLWithDataAsString:[item originalURLString]];
    NSURL *currentURL = [[[self dataSource] request] URL];
    NSArray *formData = [item formData];

    // Are we navigating to an anchor within the page?
    // Note if we have child frames we do a real reload, since the child frames might not
    // match our current frame structure, or they might not have the right content.  We could
    // check for all that as an additional optimization.
    // We also do not do anchor-style navigation if we're posting a form.
    
    // FIXME: These checks don't match the ones in _loadURL:referrer:loadType:target:triggeringEvent:isFormSubmission:
    // Perhaps they should.
    if (!formData && ![self _shouldReloadForCurrent:itemURL andDestination:currentURL] && [self _URLsMatchItem:item] )
    {
#if 0
        // FIXME:  We need to normalize the code paths for anchor navigation.  Something
        // like the following line of code should be done, but also accounting for correct
        // updates to the back/forward list and scroll position.
        // rjw 4/9/03 See 3223929.
        [self _loadURL:itemURL referrer:[[[self dataSource] request] HTTPReferrer] loadType:loadType target:nil triggeringEvent:nil form:nil formValues:nil];
#endif
        // must do this maintenance here, since we don't go through a real page reload
        [self _saveScrollPositionAndViewStateToItem:[_private currentItem]];
        // FIXME: form state might want to be saved here too

        // FIXME: Perhaps we can use scrollToAnchorWithURL here instead and remove the older scrollToAnchor:?
        NSString *anchor = [[item URLString] _webkit_URLFragment];
        if (anchor)
            [[_private->dataSource _bridge] scrollToAnchor: anchor];
    
        // must do this maintenance here, since we don't go through a real page reload
        [_private setCurrentItem:item];
        [self _restoreScrollPositionAndViewState];

        // Fake the URL change by updating the datasource's request.  This will no longer
        // be necessary if we do the better fix described above.
        NSMutableURLRequest *hackedRequest = [[[self dataSource] request] mutableCopy];
        [hackedRequest setURL: itemURL];
        [[self dataSource] __adoptRequest:hackedRequest];
        [hackedRequest release];
        
        [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                               didChangeLocationWithinPageForFrame:self];
        [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];
    } else {
        // Remember this item so we can traverse any child items as child frames load
        [_private setProvisionalItem:item];

        WebDataSource *newDataSource;
        BOOL inPageCache = NO;
        
        // Check if we'll be using the page cache.  We only use the page cache
        // if one exists and it is less than _backForwardCacheExpirationInterval
        // seconds old.  If the cache is expired it gets flushed here.
        if ([item hasPageCache]){
            NSDictionary *pageCache = [item pageCache];
            NSDate *cacheDate = [pageCache objectForKey: WebPageCacheEntryDateKey];
            NSTimeInterval delta = [[NSDate date] timeIntervalSinceDate: cacheDate];

            if (delta <= [[[self webView] preferences] _backForwardCacheExpirationInterval]){
                newDataSource = [pageCache objectForKey: WebPageCacheDataSourceKey];
                [self _loadDataSource:newDataSource withLoadType:loadType formState:nil];   
                inPageCache = YES;
            }         
            else {
                LOG (PageCache, "Not restoring page from back/forward cache because cache entry has expired, %@ (%3.5f > %3.5f seconds)\n", [[_private provisionalItem] URL], delta, [[[self webView] preferences] _backForwardCacheExpirationInterval]);
                [item setHasPageCache: NO];
            }
        }
        
        if (!inPageCache) {
            NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:itemURL];
            [self _addExtraFieldsToRequest:request alwaysFromRequest: (formData != nil)?YES:NO];

            // If this was a repost that failed the page cache, we might try to repost the form.
            NSDictionary *action;
            if (formData) {
                [request setHTTPMethod:@"POST"];
                [request _web_setHTTPReferrer:[item formReferrer]];
                webSetHTTPBody(request, formData);
                [request _web_setHTTPContentType:[item formContentType]];

                // Slight hack to test if the WF cache contains the page we're going to.  We want
                // to know this before talking to the policy delegate, since it affects whether we
                // show the DoYouReallyWantToRepost nag.
                //
                // This trick has a small bug (3123893) where we might find a cache hit, but then
                // have the item vanish when we try to use it in the ensuing nav.  This should be
                // extremely rare, but in that case the user will get an error on the navigation.
                [request setCachePolicy:NSURLRequestReturnCacheDataDontLoad];
                NSURLResponse *synchResponse = nil;
                [NSURLConnection sendSynchronousRequest:request returningResponse:&synchResponse error:nil];
                if (synchResponse == nil) { 
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
                        if (![[itemURL scheme] isEqual:@"https"]) {
                            [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];
                        }
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

                action = [self _actionInformationForLoadType:loadType isFormSubmission:NO event:nil originalURL:itemOriginalURL];
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
        [self _saveScrollPositionAndViewStateToItem:[_private currentItem]];
        
        [_private setCurrentItem:item];

        // Restore form state (works from currentItem)
        [_private->bridge restoreDocumentState];
        // Restore the scroll position (taken in favor of going back to the anchor)
        [self _restoreScrollPositionAndViewState];
        
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
        [self _loadItem:item withLoadType:type];
    }
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
- (void)_goToItem: (WebHistoryItem *)item withLoadType: (WebFrameLoadType)type
{
    ASSERT(!_private->parent);
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    if ([[_private->webView _policyDelegateForwarder] webView:_private->webView shouldGoToHistoryItem:item]) {    
        WebBackForwardList *backForwardList = [[self webView] backForwardList];
        WebHistoryItem *currItem = [backForwardList currentItem];
        // Set the BF cursor before commit, which lets the user quickly click back/forward again.
        // - plus, it only makes sense for the top level of the operation through the frametree,
        // as opposed to happening for some/one of the page commits that might happen soon
        [backForwardList goToItem:item];
        [self _recursiveGoToItem:item fromItem:currItem withLoadType:type];
    }
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
                NSDictionary *elementInfo = [(WebHTMLView *)viewContainingPoint elementAtPoint:point];
        
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
                [NSNumber numberWithInt:[event modifierFlags]], WebActionModifierFlagsKey,
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

- (void)_setPolicyDataSource:(WebDataSource *)dataSource
{
    [dataSource retain];
    [_private->policyDataSource release];
    _private->policyDataSource = dataSource;
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

    WebView *wv = [self webView];
    [[wv _policyDelegateForwarder] webView:wv
            decidePolicyForNewWindowAction:action
                                   request:request
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

- (void)_checkNavigationPolicyForRequest:(NSURLRequest *)request
                              dataSource:(WebDataSource *)dataSource
                               formState:(WebFormState *)formState
                                 andCall:(id)target
                            withSelector:(SEL)selector
{
    NSDictionary *action = [dataSource _triggeringAction];
    if (action == nil) {
        action = [self _actionInformationForNavigationType:WebNavigationTypeOther event:nil originalURL:[request URL]];
        [dataSource _setTriggeringAction:action];
    }

    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if ([request isEqual:[dataSource _lastCheckedRequest]] || [[request URL] _web_isEmpty]) {
        [target performSelector:selector withObject:request withObject:nil];
        return;
    }
    
    // We are always willing to show alternate content for unreachable URLs;
    // treat it like a reload so it maintains the right state for b/f list.
    if ([request _webDataRequestUnreachableURL] != nil) {
        if (_private->policyLoadType == WebFrameLoadTypeForward
            || _private->policyLoadType == WebFrameLoadTypeBack
            || _private->policyLoadType == WebFrameLoadTypeIndexedBackForward) {
            _private->policyLoadType = WebFrameLoadTypeReload;
        }
        [target performSelector:selector withObject:request withObject:nil];
        return;
    }
    
    [dataSource _setLastCheckedRequest:request];

    WebPolicyDecisionListener *listener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(_continueAfterNavigationPolicy:)];
    
    ASSERT(_private->policyRequest == nil);
    _private->policyRequest = [request retain];
    ASSERT(_private->policyTarget == nil);
    _private->policyTarget = [target retain];
    _private->policySelector = selector;
    ASSERT(_private->listener == nil);
    _private->listener = [listener retain];
    ASSERT(_private->policyFormState == nil);
    _private->policyFormState = [formState retain];

    WebView *wv = [self webView];
    _private->delegateIsDecidingNavigationPolicy = YES;
    [[wv _policyDelegateForwarder] webView:wv
           decidePolicyForNavigationAction:action
                                   request:request
                                     frame:self
                          decisionListener:listener];
    _private->delegateIsDecidingNavigationPolicy = NO;
    
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
        if (![WebView _canHandleRequest:request]) {
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
    LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [self name], self, (int)_private->quickRedirectComing);
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

    [_private->bridge scrollToAnchorWithURL:URL];
    
    if (!isRedirect) {
        // This will clear previousItem from the rest of the frame tree tree that didn't
        // doing any loading.  We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state
        [self _checkLoadComplete];
    }

    [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                      didChangeLocationWithinPageForFrame:self];
    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];
}

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request alwaysFromRequest: (BOOL)f
{
    [request _web_setHTTPUserAgent:[[self webView] userAgentForURL:[request URL]]];
    
    // Don't set the cookie policy URL if it's already been set.
    if ([request mainDocumentURL] == nil){
        if (self == [[self webView] mainFrame] || f) {
            [request setMainDocumentURL:[request URL]];
        } else {
            [request setMainDocumentURL:[[[[self webView] mainFrame] dataSource] _URL]];
        }
    }
}

- (void)_continueLoadRequestAfterNewWindowPolicy:(NSURLRequest *)request frameName:(NSString *)frameName formState:(WebFormState *)formState
{
    if (!request) {
        return;
    }
    
    WebView *webView = nil;
    WebView *currentWebView = [self webView];
    id wd = [currentWebView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        webView = [wd webView:currentWebView createWebViewWithRequest:nil];
    else
        webView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:nil];
        
    [webView _setTopLevelFrameName:frameName];
    [[webView _UIDelegateForwarder] webViewShow:webView];
    WebFrame *frame = [webView mainFrame];

    [[self _bridge] setOpener:[frame _bridge]];
    [frame _loadRequest:request triggeringAction:nil loadType:WebFrameLoadTypeStandard formState:formState];
}


// main funnel for navigating via callback from WebCore (e.g., clicking a link, redirect)
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer loadType:(WebFrameLoadType)loadType target:(NSString *)target triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values
{
    BOOL isFormSubmission = (values != nil);

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [request _web_setHTTPReferrer:referrer];
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
        formState = [[WebFormState alloc] initWithForm:form values:values sourceFrame:self];
    }

    if (target != nil) {
        WebFrame *targetFrame = [self findFrameNamed:target];
        if (targetFrame != nil) {
            [targetFrame _loadURL:URL referrer:referrer loadType:loadType target:nil triggeringEvent:event form:form formValues:values];
        } else {
            [self _checkNewWindowPolicyForRequest:request
                                    action:action
                                 frameName:target
                                 formState:formState
                                   andCall:self
                              withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
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
        && ![self _shouldReloadForCurrent:URL andDestination:[_private->bridge URL]]

        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && ![_private->bridge isFrameSet]) {
        
        // Just do anchor navigation within the existing content.
        
        // We don't do this if we are submitting a form, explicitly reloading,
        // currently displaying a frameset, or if the new URL does not have a fragment.
        // These rules are based on what KHTML was doing in KHTMLPart::openURL.
        
        
        // FIXME: What about load types other than Standard and Reload?

        [oldDataSource _setTriggeringAction:action];
        [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];
        [self _checkNavigationPolicyForRequest:request
                                    dataSource:oldDataSource
                                     formState:formState
                                       andCall:self
                                  withSelector:@selector(_continueFragmentScrollAfterNavigationPolicy:formState:)];
    } else {
        [self _loadRequest:request triggeringAction:action loadType:loadType formState:formState];
        if (_private->quickRedirectComing) {
            LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [self name], self, (int)_private->quickRedirectComing);
            _private->quickRedirectComing = NO;
            [[self provisionalDataSource] _setIsClientRedirect:YES];
        } else if (sameURL) {
            // Example of this case are sites that reload the same URL with a different cookie
            // driving the generated content, or a master frame with links that drive a target
            // frame, where the user has clicked on the same link repeatedly.
            [self _setLoadType:WebFrameLoadTypeSame];
        }            
    }

    [request release];
    [oldDataSource release];
    [formState release];
}

- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame
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
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            URL = [NSURL _web_URLWithDataAsString:[childItem originalURLString]];
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

    WebArchive *archive = [[self dataSource] _popSubframeArchiveWithName:[childFrame name]];
    if (archive) {
        [childFrame loadArchive:archive];
    } else {
        [childFrame _loadURL:URL referrer:referrer loadType:childLoadType target:nil triggeringEvent:nil form:nil formValues:nil];
    }
}

- (void)_postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSArray *)postData contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values
{
    // When posting, use the NSURLRequestReloadIgnoringCacheData load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.

    // FIXME: Where's the code that implements what the comment above says?

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [self _addExtraFieldsToRequest:request alwaysFromRequest:YES];
    [request _web_setHTTPReferrer:referrer];
    [request setHTTPMethod:@"POST"];
    webSetHTTPBody(request, postData);
    [request _web_setHTTPContentType:contentType];

    NSDictionary *action = [self _actionInformationForLoadType:WebFrameLoadTypeStandard isFormSubmission:YES event:event originalURL:URL];
    WebFormState *formState = nil;
    if (form && values) {
        formState = [[WebFormState alloc] initWithForm:form values:values sourceFrame:self];
    }

    if (target != nil) {
        WebFrame *targetFrame = [self findFrameNamed:target];

        if (targetFrame != nil) {
            [targetFrame _loadRequest:request triggeringAction:action loadType:WebFrameLoadTypeStandard formState:formState];
        } else {
            [self _checkNewWindowPolicyForRequest:request action:action frameName:target formState:formState andCall:self withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
        }
        [request release];
        [formState release];
        return;
    }

    [self _loadRequest:request triggeringAction:action loadType:WebFrameLoadTypeStandard formState:formState];

    [request release];
    [formState release];
}

- (void)_clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction
{
    LOG(Redirect, "%@(%p) Client redirect to: %@, [self dataSource] = %p, lockHistory = %d, isJavaScriptFormAction = %d", [self name], self, URL, [self dataSource], (int)lockHistory, (int)isJavaScriptFormAction);

    [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                                willPerformClientRedirectToURL:URL
                                                         delay:seconds
                                                      fireDate:date
                                                      forFrame:self];
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation.

    if (![self dataSource] || isJavaScriptFormAction) {
        // If we don't have a dataSource, we have no "original" load on which to base a redirect,
        // so we better just treat the redirect as a normal load.
        _private->quickRedirectComing = NO;
        LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [self name], self, (int)_private->quickRedirectComing);
    } else {
        _private->quickRedirectComing = lockHistory;
        LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [self name], self, (int)_private->quickRedirectComing);
    }
}

- (void)_clientRedirectCancelled:(BOOL)cancelWithLoadInProgress
{
    [[[self webView] _frameLoadDelegateForwarder] webView:_private->webView
                               didCancelClientRedirectForFrame:self];
    if (!cancelWithLoadInProgress)
        _private->quickRedirectComing = NO;
    LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [self name], self, (int)_private->quickRedirectComing);
}

- (void)_setTitle:(NSString *)title
{
    [[_private currentItem] setTitle:title];
}

- (void)_saveScrollPositionAndViewStateToItem:(WebHistoryItem *)item
{
    if (item) {
        NSView <WebDocumentView> *docView = [[self frameView] documentView];
        NSView *parent = [docView superview];
        // we might already be detached when this is called from detachFromParent, in which
        // case we don't want to override real data earlier gathered with (0,0)
        if (parent) {
            NSPoint point;
            if ([docView conformsToProtocol:@protocol(_WebDocumentViewState)]) {
                // The view has it's own idea of where it is scrolled to, perhaps because it contains its own
                // ScrollView instead of using the one provided by the WebFrame
                point = [(id <_WebDocumentViewState>)docView scrollPoint];
                [item setViewState:[(id <_WebDocumentViewState>)docView viewState]];
            } else {
                // Parent is the clipview of the DynamicScrollView the WebFrame installs
                ASSERT([parent isKindOfClass:[NSClipView class]]);
                point = [parent bounds].origin;
            }
            [item setScrollPoint:point];
        }
    }
}

/*
    There is a race condition between the layout and load completion that affects restoring the scroll position.
    We try to restore the scroll position at both the first layout and upon load completion.

    1) If first layout happens before the load completes, we want to restore the scroll position then so that the
       first time we draw the page is already scrolled to the right place, instead of starting at the top and later
       jumping down.  It is possible that the old scroll position is past the part of the doc laid out so far, in
       which case the restore silent fails and we will fix it in when we try to restore on doc completion.
    2) If the layout happens after the load completes, the attempt to restore at load completion time silently
       fails.  We then successfully restore it when the layout happens.
 */

- (void)_restoreScrollPositionAndViewState
{
    ASSERT([_private currentItem]);
    NSView <WebDocumentView> *docView = [[self frameView] documentView];
    NSPoint point = [[_private currentItem] scrollPoint];
    if ([docView conformsToProtocol:@protocol(_WebDocumentViewState)]) {        
        id state = [[_private currentItem] viewState];
        if (state) {
            [(id <_WebDocumentViewState>)docView setViewState:state];
        }
        
        [(id <_WebDocumentViewState>)docView setScrollPoint:point];
    } else {
        [docView scrollPoint:point];
    }
}

- (void)_defersCallbacksChanged
{
    [[self provisionalDataSource] _defersCallbacksChanged];
    [[self dataSource] _defersCallbacksChanged];
}

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[[self frameView] documentView] viewWillMoveToHostWindow:hostWindow];
    // It's OK to use the internal version because this method is
    // guaranteed not to change the set of frames.
    [[self _internalChildFrames] makeObjectsPerformSelector:@selector(_viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)_viewDidMoveToHostWindow
{
    [[[self frameView] documentView] viewDidMoveToHostWindow];
    // It's OK to use the internal version because this method is
    // guaranteed not to change the set of frames.
    [[self _internalChildFrames] makeObjectsPerformSelector:@selector(_viewDidMoveToHostWindow)];
}

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding
{
    WebDataSource *dataSource = [self dataSource];
    if (dataSource == nil) {
        return;
    }

    NSMutableURLRequest *request = [[dataSource request] mutableCopy];
    NSURL *unreachableURL = [dataSource unreachableURL];
    if (unreachableURL != nil) {
        [request setURL:unreachableURL];
    }
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
 
    unsigned currentIndex = [_private->children count] - 1;
    // we keep track of sibling pointers to avoid the overhead of a lookup in the children array
    
    if (currentIndex > 0) {
        WebFrame *previousFrame = [[self childFrames] objectAtIndex: currentIndex - 1];
        previousFrame->_private->nextSibling = child;
        child->_private->previousSibling = previousFrame;
        ASSERT(child->_private->nextSibling == nil);
    }
    
}

- (void)_removeChild:(WebFrame *)child
{
    // move corresponding previous and next WebFrame sibling pointers to their new positions
    // when we remove a child we may have to reattach the previous frame's next frame and visa versa
    if (child->_private->previousSibling) {
        child->_private->previousSibling->_private->nextSibling = child->_private->nextSibling;
    }
    
    if (child->_private->nextSibling) { 
        child->_private->nextSibling->_private->previousSibling = child->_private->previousSibling; 
    }

    [_private->children removeObject:child];
    child->_private->parent = nil;
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
    // FIXME: Shouldn't this number be the index of this frame in its parent rather than the child count?
    [path appendFormat:@"/<!--frame%d-->-->", _private->children ? [_private->children count] : 0];
    return path;
}

- (void)_resetBackForwardList
{
    // Note this doesn't verify the current load type as a b/f operation because it is called from
    // a subframe in the case of a delegate bailing out of the nav before it even gets to provisional state.
    ASSERT(self == [[self webView] mainFrame]);
    WebHistoryItem *resetItem = [_private currentItem];
    if (resetItem)
        [[[self webView] backForwardList] goToItem:resetItem];
}

// If we bailed out of a b/f navigation, we might need to set the b/f cursor back to the current
// item, because we optimistically move it right away at the start of the operation. But when
// alternate content is loaded for an unreachableURL, we don't want to reset the b/f cursor.
// Return the item that we would reset to, so we can decide later whether to actually reset.
- (WebHistoryItem *)_currentBackForwardListItemToResetTo
{
    WebFrameLoadType loadType = [self _loadType];
    if ((loadType == WebFrameLoadTypeForward
         || loadType == WebFrameLoadTypeBack
         || loadType == WebFrameLoadTypeIndexedBackForward)
        && self == [[self webView] mainFrame]) {
        return [_private currentItem];
    }
    return nil;
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

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
- (void)_saveDocumentAndScrollState
{
    [_private->bridge saveDocumentState];
    [self _saveScrollPositionAndViewStateToItem:[_private currentItem]];

    // It's OK to use the internal version because this method is
    // guaranteed not to change the set of frames.
    [[self _internalChildFrames] makeObjectsPerformSelector:@selector(_saveDocumentAndScrollState)];
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
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil _private->policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, _private->policyDataSource should still be set.
    ASSERT(_private->policyDataSource || [[self provisionalDataSource] unreachableURL] != nil);

    WebHistoryItem *item = [_private provisionalItem];
    if (!request) {
        [self _setPolicyDataSource:nil];
        // If the delegate punts on the navigation, we have the problem that we have optimistically moved
        // the b/f cursor already, so move it back.  For sanity we only do this if the navigation of the
        // target frame or top-level frame is canceled.  A primary case of this is the user responding 
        // Cancel to the form repost nag sheet.
        if (([item isTargetItem] || [[self webView] mainFrame] == self)
            && (_private->policyLoadType == WebFrameLoadTypeForward
                || _private->policyLoadType == WebFrameLoadTypeBack
                || _private->policyLoadType == WebFrameLoadTypeIndexedBackForward))
            [[[self webView] mainFrame] _resetBackForwardList];
        return;
    }
    
    WebFrameLoadType loadType = _private->policyLoadType;
    WebDataSource *dataSource = [_private->policyDataSource retain];
    
    [self stopLoading];
    [self _setLoadType:loadType];
    [self _setProvisionalDataSource:dataSource];
    [dataSource release];

    [self _setPolicyDataSource:nil];
    
    // We tell the documentView provisionalDataSourceChanged:
    // once it has been created by the WebView.
    
    [self _setState: WebFrameStateProvisional];
    
    if (self == [[self webView] mainFrame])
        LOG(DocumentLoad, "loading %@", [[[self provisionalDataSource] request] URL]);

    if ((loadType == WebFrameLoadTypeForward ||
        loadType == WebFrameLoadTypeBack ||
        loadType == WebFrameLoadTypeIndexedBackForward) &&
        [item hasPageCache]){
        NSDictionary *pageCache = [[_private provisionalItem] pageCache];
        if ([pageCache objectForKey:WebCorePageCacheStateKey]){
            LOG (PageCache, "Restoring page from back/forward cache, %@\n", [[_private provisionalItem] URL]);
            [_private->provisionalDataSource _startLoading: pageCache];
            return;
        }
    }

    if (formState) {
        // It's a bit of a hack to reuse the WebPolicyDecisionListener for the continuation
        // mechanism across the willSubmitForm callout.
        _private->listener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(_continueAfterWillSubmitForm:)];
        [[[self webView] _formDelegate] frame:self sourceFrame:[formState sourceFrame] willSubmitForm:[formState form] withValues:[formState values] submissionListener:_private->listener];
    } 
    else {
        [self _continueAfterWillSubmitForm:WebPolicyUse];
    }
}

- (void)_loadDataSource:(WebDataSource *)newDataSource withLoadType:(WebFrameLoadType)loadType formState:(WebFormState *)formState
{
    ASSERT([self webView] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.

    ASSERT([self frameView] != nil);

    _private->policyLoadType = loadType;

    WebFrame *parentFrame = [self parentFrame];
    if (parentFrame) {
        [newDataSource _setOverrideEncoding:[[parentFrame dataSource] _overrideEncoding]];
    }
    [newDataSource _setWebView:[self webView]];
    [newDataSource _setJustOpenedForTargetedLink:_private->justOpenedForTargetedLink];
    _private->justOpenedForTargetedLink = NO;

    [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    [self _setPolicyDataSource:newDataSource];

    [self _checkNavigationPolicyForRequest:[newDataSource request]
                                dataSource:newDataSource
                                 formState:formState
                                   andCall:self
                              withSelector:@selector(_continueLoadRequestAfterNavigationPolicy:formState:)];
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
    NSString* URLString = [URL _web_originalDataAsString];
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

// Returns the last child of us and any children, or nil
- (WebFrame *)_lastChild
{
    if (_private->children && [_private->children count]) {
        WebFrame *ourLastKid = [_private->children lastObject];
        WebFrame *kidsLastKid = [ourLastKid _lastChild];
        return kidsLastKid ? kidsLastKid : ourLastKid;
    }
    return nil;                // no kids
}

// Return next frame to be traversed, visiting children after parent
- (WebFrame *)_nextFrameWithWrap:(BOOL)wrapFlag
{
    if (_private->children && [_private->children count]) {
        return [_private->children objectAtIndex:0];
    } else if (_private->parent) {
        WebFrame *frame;
        for (frame = self; frame->_private->parent; frame = frame->_private->parent) {
            WebFrame *nextSibling = frame->_private->nextSibling;
            if (nextSibling) {
                return nextSibling;
            }
        }
        return wrapFlag ? frame : nil;                // made it all the way to the top
    } else {
        return wrapFlag ? self : nil;                // self is the top and we have no kids
    }
}

// Return previous frame to be traversed, exact reverse order of _nextFrame
- (WebFrame *)_previousFrameWithWrap:(BOOL)wrapFlag
{
    WebFrame *prevSibling = _private->previousSibling;
    if (prevSibling) {
        WebFrame *prevSiblingLastChild = [prevSibling _lastChild];
        return prevSiblingLastChild ? prevSiblingLastChild : prevSibling;
    } else if (_private->parent) {
        return _private->parent;
    } else {
        // no siblings, no parent, self==top
        if (wrapFlag) {
            WebFrame *selfLastChild = [self _lastChild];
            return selfLastChild ? selfLastChild : self;
        } else {
            // top view is always the last one in this ordering, so prev is nil without wrap
            return nil;
        }
    }
}

- (void)_setShouldCreateRenderers:(BOOL)f
{
    [_private->bridge setShouldCreateRenderers:f];
}

- (BOOL)_shouldCreateRenderers
{
    return [_private->bridge shouldCreateRenderers];
}

- (int)_numPendingOrLoadingRequests:(BOOL)recurse
{
    int num;

    if (!recurse)
        return [[self _bridge] numPendingOrLoadingRequests];

    num = [[self _bridge] numPendingOrLoadingRequests];
    // It's OK to use the internal version because this method is
    // guaranteed not to change the set of frames.
    NSArray *children = [self _internalChildFrames];
    int i, count = [children count];
    WebFrame *child;
    for (i = 0; i < count; i++){
        child = [children objectAtIndex: 0];
        num += [child _numPendingOrLoadingRequests:recurse];
    }
    return num;
}

- (NSColor *)_bodyBackgroundColor
{
    return [_private->bridge bodyBackgroundColor];
}

- (void)_reloadForPluginChanges
{
    NSView <WebDocumentView> *documentView = [[self frameView] documentView];
    if ([documentView isKindOfClass:[WebNetscapePluginDocumentView class]] ||
        [documentView isKindOfClass:[WebPluginDocumentView class]]) {
        [self reload];
    } else if ([documentView isKindOfClass:[WebHTMLView class]]) {
        NSEnumerator *viewEnumerator = [[documentView subviews] objectEnumerator];
        NSView *view;
        // FIXME:  We should ask the frame if it contains plugins, rather
        // than doing this check.
        while ((view = [viewEnumerator nextObject]) != nil) {
            if ([view isKindOfClass:[WebNetscapePluginEmbeddedView class]] ||
                [view isKindOfClass:[WebNullPluginView class]] ||
                [WebPluginController isPlugInView:view]) {
                [self reload];
                break;
            }
        }
    } else {
        [[self childFrames] makeObjectsPerformSelector:@selector(_reloadForPluginChanges)];
    }
}

- (NSArray *)_internalChildFrames
{
    return _private->children;
}

- (void)_attachScriptDebugger
{
    if (!_private->scriptDebugger) {
        _private->scriptDebugger = [[WebScriptDebugger alloc] initWithWebFrame:self];
    }
}

@end

@implementation WebFrame (WebInternal)

- (void)_accumulateDocumentViews:(NSMutableArray *)result
{
    id docView = [[self frameView] documentView];
    if (docView) {
        [result addObject:docView];
    }
    [_private->children makeObjectsPerformSelector:@selector(_accumulateDocumentViews:) withObject:result];
}

- (NSArray *)_documentViews
{
    NSMutableArray *result = [NSMutableArray array];
    [self _accumulateDocumentViews:result];
    return result;
}

- (void)_updateDrawsBackground
{
    BOOL drawsBackground = [[self webView] drawsBackground];
    if (!drawsBackground)
        [[[self frameView] _scrollView] setDrawsBackground:NO];
    id documentView = [[self frameView] documentView];
    if ([documentView respondsToSelector:@selector(setDrawsBackground:)])
        [documentView setDrawsBackground:drawsBackground];
    [[self _bridge] setDrawsBackground:drawsBackground];
    [_private->children makeObjectsPerformSelector:@selector(_updateDrawsBackground)];
}

- (void)_setInternalLoadDelegate:(id)internalLoadDelegate
{
    _private->internalLoadDelegate = internalLoadDelegate;
}

- (id)_internalLoadDelegate
{
    return _private->internalLoadDelegate;
}

- (NSURLRequest *)_requestFromDelegateForRequest:(NSURLRequest *)request identifier:(id *)identifier error:(NSError **)error
{
    ASSERT(request != nil);
    
    WebView *wv = [self webView];
    id delegate = [wv resourceLoadDelegate];
    id sharedDelegate = [WebDefaultResourceLoadDelegate sharedResourceLoadDelegate];
    WebResourceDelegateImplementationCache implementations = [wv _resourceLoadDelegateImplementations];
    WebDataSource *dataSource = [self dataSource];
    
    if (implementations.delegateImplementsIdentifierForRequest) {
        *identifier = [delegate webView:wv identifierForInitialRequest:request fromDataSource:dataSource];
    } else {
        *identifier = [sharedDelegate webView:wv identifierForInitialRequest:request fromDataSource:dataSource];
    }
        
    NSURLRequest *newRequest;
    if (implementations.delegateImplementsWillSendRequest) {
        newRequest = [delegate webView:wv resource:*identifier willSendRequest:request redirectResponse:nil fromDataSource:dataSource];
    } else {
        newRequest = [sharedDelegate webView:wv resource:*identifier willSendRequest:request redirectResponse:nil fromDataSource:dataSource];
    }
    
    if (newRequest == nil) {
        *error = [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:[request URL]];
    } else {
        *error = nil;
    }
    
    return newRequest;
}

- (void)_sendRemainingDelegateMessagesWithIdentifier:(id)identifier response:(NSURLResponse *)response length:(unsigned)length error:(NSError *)error 
{    
    WebView *wv = [self webView];
    id delegate = [wv resourceLoadDelegate];
    id sharedDelegate = [WebDefaultResourceLoadDelegate sharedResourceLoadDelegate];
    WebResourceDelegateImplementationCache implementations = [wv _resourceLoadDelegateImplementations];
    WebDataSource *dataSource = [self dataSource];
        
    if (response != nil) {
        if (implementations.delegateImplementsDidReceiveResponse) {
            [delegate webView:wv resource:identifier didReceiveResponse:response fromDataSource:dataSource];
        } else {
            [sharedDelegate webView:wv resource:identifier didReceiveResponse:response fromDataSource:dataSource];
        }
    }
    
    if (length > 0) {
        if (implementations.delegateImplementsDidReceiveContentLength) {
            [delegate webView:wv resource:identifier didReceiveContentLength:length fromDataSource:dataSource];
        } else {
            [sharedDelegate webView:wv resource:identifier didReceiveContentLength:length fromDataSource:dataSource];
        }
    }
    
    if (error == nil) {
        if (implementations.delegateImplementsDidFinishLoadingFromDataSource) {
            [delegate webView:wv resource:identifier didFinishLoadingFromDataSource:dataSource];
        } else {
            [sharedDelegate webView:wv resource:identifier didFinishLoadingFromDataSource:dataSource];
        }
        [wv _finishedLoadingResourceFromDataSource:dataSource];
    } else {
        [[wv _resourceLoadDelegateForwarder] webView:wv resource:identifier didFailLoadingWithError:error fromDataSource:dataSource];
    }
}

- (void)_safeLoadURL:(NSURL *)URL
{
   // Call the bridge because this is where our security checks are made.
    [[self _bridge] loadURL:URL 
                    referrer:[[[[self dataSource] request] URL] _web_originalDataAsString]
                      reload:NO
                 userGesture:YES       
                      target:nil
             triggeringEvent:[NSApp currentEvent]
                        form:nil 
                  formValues:nil];
}

- (void)_saveResourceAndSendRemainingDelegateMessagesWithRequest:(NSURLRequest *)request
                                                      identifier:(id)identifier 
                                                        response:(NSURLResponse *)response 
                                                            data:(NSData *)data
                                                           error:(NSError *)error
{
    unsigned length = [data length];
    if (length > 0 && error == nil) {
        ASSERT(request != nil);
        WebResource *resource = [[WebResource alloc] _initWithData:data URL:[request URL] response:response];
        ASSERT(resource != nil);    
        [[self dataSource] addSubresource:resource];
        [resource release];
    }
    [self _sendRemainingDelegateMessagesWithIdentifier:identifier response:response length:length error:error];
}

- (void)_unmarkAllMisspellings
{
    [[self _bridge] unmarkAllMisspellings];
    [_private->children makeObjectsPerformSelector:@selector(_unmarkAllMisspellings)];
}

- (void)_didFirstLayout
{
    if ([[self webView] backForwardList]) {
        WebFrameLoadType loadType = [self _loadType];
        if (loadType == WebFrameLoadTypeForward ||
            loadType == WebFrameLoadTypeBack ||
            loadType == WebFrameLoadTypeIndexedBackForward)
        {
            [self _restoreScrollPositionAndViewState];
        }
    }
}

@end

@implementation WebFormState : NSObject

- (id)initWithForm:(DOMElement *)form values:(NSDictionary *)values sourceFrame:(WebFrame *)sourceFrame
{
    self = [super init];
    if (!self)
        return nil;
    
    _form = [form retain];
    _values = [values copy];
    _sourceFrame = [sourceFrame retain];
    return self;
}

- (void)dealloc
{
    [_form release];
    [_values release];
    [_sourceFrame release];
    [super dealloc];
}

- (DOMElement *)form
{
    return _form;
}

- (NSDictionary *)values
{
    return _values;
}

- (WebFrame *)sourceFrame
{
    return _sourceFrame;
}

@end

@implementation WebFrame

- init
{
    return [self initWithName:nil webFrameView:nil webView:nil];
}

- initWithName:(NSString *)n webFrameView:(WebFrameView *)fv webView:(WebView *)v
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebFramePrivate alloc] init];

    [self _setWebView:v];
    [self _setName:n];

    _private->bridge = [[WebBridge alloc] initWithWebFrame:self];
    
    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebView:v];
    }
    
    ++WebFrameCount;
    
    return self;
}

- (void)dealloc
{
    [self _detachFromParent];
    [_private->webFrameView _setWebView:nil];
    [_private->dataSource _setWebView:nil];
    [_private->provisionalDataSource _setWebView:nil];

    [_private release];

    --WebFrameCount;

    [super dealloc];
}

- (void)finalize
{
    // FIXME: Should not do this work at finalize time. Need to do it at a predictable time instead.
    [self _detachFromParent];
    [_private->webFrameView _setWebView:nil];
    [_private->dataSource _setWebView:nil];
    [_private->provisionalDataSource _setWebView:nil];

    --WebFrameCount;

    [super finalize];
}

- (NSString *)name
{
    return [_private name];
}

- (WebFrameView *)frameView
{
    return [_private webFrameView];
}

- (WebView *)webView
{
    return [_private webView];
}

- (DOMDocument *)DOMDocument
{
    return [[self dataSource] _isDocumentHTML] ? [_private->bridge DOMDocument] : nil;
}

- (DOMHTMLElement *)frameElement
{
    return [[self webView] mainFrame] != self ? [_private->bridge frameElement] : nil;
}

- (WebDataSource *)provisionalDataSource
{
    return [_private provisionalDataSource];
}

- (WebDataSource *)dataSource
{
    return [_private dataSource];
}

- (void)loadRequest:(NSURLRequest *)request
{
    [self _loadRequest:request subresources:nil subframeArchives:nil];
}

- (void)_loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL
{
    NSURLRequest *request = [self _webDataRequestForData:data 
                                                MIMEType:MIMEType 
                                        textEncodingName:encodingName 
                                                 baseURL:URL
                                          unreachableURL:unreachableURL];
    [self loadRequest:request];
}


- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL
{
    [self _loadData:data MIMEType:MIMEType textEncodingName:encodingName baseURL:URL unreachableURL:nil];
}

- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL
{
    CFStringEncoding cfencoding = CFStringGetFastestEncoding((CFStringRef)string);
    NSStringEncoding nsencoding = CFStringConvertEncodingToNSStringEncoding(cfencoding);
    CFStringRef cfencodingName = CFStringConvertEncodingToIANACharSetName(cfencoding);
    
    if (!cfencodingName || nsencoding == kCFStringEncodingInvalidId){
        NSData *data = [string dataUsingEncoding: NSUnicodeStringEncoding];
        [self _loadData:data MIMEType:nil textEncodingName:@"utf-16" baseURL:URL unreachableURL:unreachableURL];
    }
    else {
        NSData *data = [string dataUsingEncoding: nsencoding];
        [self _loadData:data MIMEType:nil textEncodingName:(NSString *)cfencodingName baseURL:URL unreachableURL:unreachableURL];
    }
}

- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)URL
{
    [self _loadHTMLString:string baseURL:URL unreachableURL:nil];
}

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)URL forUnreachableURL:(NSURL *)unreachableURL
{
    [self _loadHTMLString:string baseURL:URL unreachableURL:unreachableURL];
}

- (void)loadArchive:(WebArchive *)archive
{
    WebResource *mainResource = [archive mainResource];
    if (mainResource) {
        NSURLRequest *request = [self _webDataRequestForData:[mainResource data] 
                                                    MIMEType:[mainResource MIMEType]
                                            textEncodingName:[mainResource textEncodingName]
                                                     baseURL:[mainResource URL]
                                              unreachableURL:nil];
        [self _loadRequest:request subresources:[archive subresources] subframeArchives:[archive subframeArchives]];
    }
}

- (void)stopLoading
{
    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (_private->isStoppingLoad) {
        return;
    }
    _private->isStoppingLoad = YES;
    
    [self _invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    [_private->provisionalDataSource _stopLoading];
    [_private->dataSource _stopLoading];

    // Release the provisional data source because there's no point in keeping it around since it is unused in this case.
    [self _setProvisionalDataSource:nil];
    
    _private->isStoppingLoad = NO;
}


- (void)reload
{
    WebDataSource *dataSource = [self dataSource];
    if (dataSource == nil) {
        return;
    }

    NSMutableURLRequest *initialRequest = [dataSource request];
    
    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if ([[[[dataSource request] URL] absoluteString] length] == 0) {
        return;
    }

    // Replace error-page URL with the URL we were trying to reach.
    NSURL *unreachableURL = [initialRequest _webDataRequestUnreachableURL];
    if (unreachableURL != nil) {
        initialRequest = [NSURLRequest requestWithURL:unreachableURL];
    }
    
    // initWithRequest copies the request
    WebDataSource *newDataSource = [[WebDataSource alloc] initWithRequest:initialRequest];
    NSMutableURLRequest *request = [newDataSource request];

    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    // If we're about to rePOST, set up action so the app can warn the user
    if ([[request HTTPMethod] _webkit_isCaseInsensitiveEqualToString:@"POST"]) {
        NSDictionary *action = [self _actionInformationForNavigationType:WebNavigationTypeFormResubmitted event:nil originalURL:[request URL]];
        [newDataSource _setTriggeringAction:action];
    }

    [newDataSource _setOverrideEncoding:[dataSource _overrideEncoding]];
    
    [self _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReload formState:nil];

    [newDataSource release];
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    // First, deal with 'special' names.
    if ([name isEqualToString:@"_self"] || [name isEqualToString:@"_current"]){
        return self;
    }
    
    if ([name isEqualToString:@"_top"]) {
        return [[self webView] mainFrame];
    }
    
    if ([name isEqualToString:@"_parent"]) {
        WebFrame *parent = [self parentFrame];
        return parent ? parent : self;
    }
    
    if ([name isEqualToString:@"_blank"]) {
        return nil;
    }

    // Search from this frame down.
    WebFrame *frame = [self _descendantFrameNamed:name sourceFrame:self];

    if (!frame) {
        // Search in this WebView then in others.
        frame = [[self webView] _findFrameNamed:name sourceFrame:self];
    }

    return frame;
}

- (WebFrame *)parentFrame
{
    return [[_private->parent retain] autorelease];
}

- (NSArray *)childFrames
{
    return [[_private->children copy] autorelease];
}

@end
