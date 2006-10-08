/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebFrameInternal.h"

#import "WebArchive.h"
#import "WebBackForwardList.h"
#import "WebDataProtocol.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDefaultUIDelegate.h"
#import "WebDocumentInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebFormDataStream.h"
#import "WebFrameBridge.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoader.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebHTMLViewPrivate.h"
#import "WebHistoryItemPrivate.h"
#import "WebHistoryPrivate.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebNullPluginView.h"
#import "WebPlugin.h"
#import "WebPluginController.h"
#import "WebPreferencesPrivate.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourcePrivate.h"
#import "WebScriptDebugDelegatePrivate.h"
#import "WebUIDelegate.h"
#import "WebViewInternal.h"
#import <WebKit/DOM.h>
#import <WebKitSystemInterface.h>
#import <objc/objc-runtime.h>

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

@interface WebFrame (ForwardDecls)
- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;
- (NSDictionary *)_actionInformationForLoadType:(WebFrameLoadType)loadType isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL;

- (void)_saveScrollPositionAndViewStateToItem:(WebHistoryItem *)item;

- (WebHistoryItem *)_createItem:(BOOL)useOriginal;
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;
- (WebHistoryItem *)_currentBackForwardListItemToResetTo;
@end

@interface WebFrame (FrameTraversal)
- (WebFrame *)_firstChildFrame;
- (WebFrame *)_lastChildFrame;
- (unsigned)_childFrameCount;
- (WebFrame *)_previousSiblingFrame;
- (WebFrame *)_nextSiblingFrame;
- (WebFrame *)_traverseNextFrameStayWithin:(WebFrame *)stayWithin;
@end

@interface NSView (WebFramePluginHosting)
- (void)setWebFrame:(WebFrame *)webFrame;
@end

@interface WebFramePrivate : NSObject
{
@public
    WebFrameView *webFrameView;

    WebFrameBridge *bridge;
    WebHistoryItem *currentItem;        // BF item for our current content
    WebHistoryItem *provisionalItem;    // BF item for where we're trying to go
                                        // (only known when navigating to a pre-existing BF item)
    WebHistoryItem *previousItem;       // BF item for previous content, see _itemForSavingDocState

    WebScriptDebugger *scriptDebugger;
    id internalLoadDelegate;
    
    NSMutableSet *plugInViews;
    NSMutableSet *inspectors;
}

- (void)setWebFrameView:(WebFrameView *)v;

- (void)setProvisionalItem:(WebHistoryItem *)item;
- (void)setPreviousItem:(WebHistoryItem *)item;
- (void)setCurrentItem:(WebHistoryItem *)item;

@end

@implementation WebFramePrivate

- (void)dealloc
{
    [webFrameView release];

    [currentItem release];
    [provisionalItem release];
    [previousItem release];
    
    [scriptDebugger release];
    
    [inspectors release];

    ASSERT(plugInViews == nil);
    
    [super dealloc];
}

- (void)setWebFrameView:(WebFrameView *)v 
{ 
    [v retain];
    [webFrameView release];
    webFrameView = v;
}

- (void)setProvisionalItem:(WebHistoryItem *)item
{
    [item retain];
    [provisionalItem release];
    provisionalItem = item;
}

- (void)setPreviousItem:(WebHistoryItem *)item
{
    [item retain];
    [previousItem release];
    previousItem = item;
}

- (void)setCurrentItem:(WebHistoryItem *)item
{
    [item retain];
    [currentItem release];
    currentItem = item;
}

@end

static inline WebFrame *Frame(WebCoreFrameBridge *bridge)
{
    return [(WebFrameBridge *)bridge webFrame];
}

@implementation WebFrame (FrameTraversal)

- (WebFrame *)_firstChildFrame
{
    return Frame([[self _bridge] firstChild]);
}

- (WebFrame *)_lastChildFrame
{
    return Frame([[self _bridge] lastChild]);
}

- (unsigned)_childFrameCount
{
    return [[self _bridge] childCount];
}

- (WebFrame *)_previousSiblingFrame;
{
    return Frame([[self _bridge] previousSibling]);
}

- (WebFrame *)_nextSiblingFrame;
{
    return Frame([[self _bridge] nextSibling]);
}

- (WebFrame *)_traverseNextFrameStayWithin:(WebFrame *)stayWithin
{
    return Frame([[self _bridge] traverseNextFrameStayWithin:[stayWithin _bridge]]);
}

@end

@implementation WebFrame (WebInternal)

- (NSURLRequest *)_webDataRequestForData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL
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

// helper method used in various nav cases below
- (void)_addBackForwardItemClippedAtTarget:(BOOL)doClip
{
    if ([[self dataSource] _URLForHistory] != nil) {
        WebHistoryItem *bfItem = [[[self webView] mainFrame] _createItemTreeWithTargetFrame:self clippedAtTarget:doClip];
        LOG (BackForward, "for frame %@, adding item  %@\n", [self name], bfItem);
        [[[self webView] backForwardList] addItem:bfItem];
    }
}

- (void)_addHistoryItemForFragmentScroll
{
    [self _addBackForwardItemClippedAtTarget:NO];
}

- (void)_didFinishLoad
{
    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];    
}

- (WebHistoryItem *)_createItem:(BOOL)useOriginal
{
    WebDataSource *dataSrc = [self dataSource];
    NSURLRequest *request;
    NSURL *unreachableURL = [dataSrc unreachableURL];
    NSURL *URL;
    NSURL *originalURL;
    WebHistoryItem *bfItem;

    if (useOriginal)
        request = [[dataSrc _documentLoader] originalRequestCopy];
    else
        request = [dataSrc request];

    if (unreachableURL != nil) {
        URL = unreachableURL;
        originalURL = unreachableURL;
    } else {
        URL = [request URL];
        originalURL = [[[dataSrc _documentLoader] originalRequestCopy] URL];
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
    [_private setPreviousItem:_private->currentItem];
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
    WebHistoryItem *bfItem = [self _createItem:[self parentFrame] ? YES : NO];

    [self _saveScrollPositionAndViewStateToItem:_private->previousItem];
    if (!(doClip && self == targetFrame)) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        [_private->bridge saveDocumentState];

        for (WebFrame *child = [self _firstChildFrame]; child; child = [child _nextSiblingFrame])
            [bfItem addChildItem:[child _createItemTreeWithTargetFrame:targetFrame clippedAtTarget:doClip]];
    }
    if (self == targetFrame)
        [bfItem setIsTargetItem:YES];

    return bfItem;
}

- (WebFrame *)_immediateChildFrameNamed:(NSString *)name
{
    return Frame([[self _bridge] childFrameNamed:name]);
}

- (void)_detachChildren
{
    // FIXME: is it really necessary to do this in reverse order any more?
    WebFrame *child = [self _lastChildFrame];
    WebFrame *prev = [child _previousSiblingFrame];
    for (; child; child = prev, prev = [child _previousSiblingFrame])
        [child _detachFromParent];
}

- (void)_detachFromParent
{
    WebFrameBridge *bridge = [_private->bridge retain];

    [bridge closeURL];
    [self stopLoading];

    [self _saveScrollPositionAndViewStateToItem:_private->currentItem];
    [self _detachChildren];
    [_private->inspectors makeObjectsPerformSelector:@selector(_webFrameDetached:) withObject:self];

    [_private->webFrameView _setWebFrame:nil]; // needed for now to be compatible w/ old behavior

    [[self _frameLoader] clearDataSource];
    [_private setWebFrameView:nil];

    [self retain]; // retain self temporarily because dealloc can re-enter this method

    [[[self parentFrame] _bridge] removeChild:bridge];

    [bridge close];
    [bridge release];

    bridge = nil;
    _private->bridge = nil;

    [self release];
}

- (void)_makeDocumentView
{
    NSView <WebDocumentView> *documentView = [_private->webFrameView _makeDocumentViewForDataSource:[[self _frameLoader] dataSource]];
    if (!documentView)
        return;

    // FIXME: We could save work and not do this for a top-level view that is not a WebHTMLView.
    WebFrameView *v = _private->webFrameView;
    [_private->bridge createFrameViewWithNSView:documentView marginWidth:[v _marginWidth] marginHeight:[v _marginHeight]];
    [self _updateBackground];
    [_private->bridge installInFrame:[v _scrollView]];

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:[[self _frameLoader] dataSource]];
}

- (void)_transitionToCommitted:(NSDictionary *)pageCache
{
    ASSERT([self webView] != nil);
    
    switch ([[self _frameLoader] state]) {
        case WebFrameStateProvisional:
        {
            [[[[self frameView] _scrollView] contentView] setCopiesOnScroll:YES];

            WebFrameLoadType loadType = [[self _frameLoader] loadType];
            if (loadType == WebFrameLoadTypeForward ||
                loadType == WebFrameLoadTypeBack ||
                loadType == WebFrameLoadTypeIndexedBackForward ||
                (loadType == WebFrameLoadTypeReload && [[[self _frameLoader] provisionalDataSource] unreachableURL] != nil))
            {
                // Once committed, we want to use current item for saving DocState, and
                // the provisional item for restoring state.
                // Note previousItem must be set before we close the URL, which will
                // happen when the data source is made non-provisional below
                [_private setPreviousItem:_private->currentItem];
                ASSERT(_private->provisionalItem);
                [_private setCurrentItem:_private->provisionalItem];
                [_private setProvisionalItem:nil];
            }

            // The call to closeURL invokes the unload event handler, which can execute arbitrary
            // JavaScript. If the script initiates a new load, we need to abandon the current load,
            // or the two will stomp each other.
            WebDataSource *pd = [[self _frameLoader] provisionalDataSource];
            [[self _bridge] closeURL];
            if (pd != [[self _frameLoader] provisionalDataSource])
                return;

            [[self _frameLoader] commitProvisionalLoad];

            // Handle adding the URL to the back/forward list.
            WebDataSource *ds = [self dataSource];
            NSString *ptitle = [ds pageTitle];

            switch (loadType) {
            case WebFrameLoadTypeForward:
            case WebFrameLoadTypeBack:
            case WebFrameLoadTypeIndexedBackForward:
                if ([[self webView] backForwardList]) {
                    // Must grab the current scroll position before disturbing it
                    [self _saveScrollPositionAndViewStateToItem:_private->previousItem];
                    
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
            case WebFrameLoadTypeReplace:
            {
                WebHistoryItem *currItem = _private->currentItem;
                LOG(PageCache, "Clearing back/forward cache, %@\n", [currItem URL]);
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
                NSURL *URL = [[[[ds _documentLoader] originalRequestCopy] URL] _webkit_canonicalize];
                WebHistory *sharedHistory = [WebHistory optionalSharedHistory];
                WebHistoryItem *oldItem = [sharedHistory itemForURL:URL];
                if (oldItem)
                    [sharedHistory setLastVisitedTimeInterval:[NSDate timeIntervalSinceReferenceDate] forItem:oldItem];
                
                [self _makeDocumentView];
                break;
            }

            // FIXME - just get rid of this case, and merge WebFrameLoadTypeReloadAllowingStaleData with the above case
            case WebFrameLoadTypeReloadAllowingStaleData:
                [self _makeDocumentView];
                break;
                
            case WebFrameLoadTypeStandard:
                if (![[ds _documentLoader] isClientRedirect]) {
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
                        [_private->currentItem setURL:[request URL]];

                        // clear out the form data so we don't repost it to the wrong place if we
                        // ever go back/forward to this item
                        [_private->currentItem _setFormInfoFromRequest:request];

                        // We must also clear out form data so we don't try to restore it into the incoming page,
                        // see -_opened
                    }
                }
                [self _makeDocumentView];
                break;
                
            case WebFrameLoadTypeInternal:
                // Add an item to the item tree for this frame
                ASSERT(![[ds _documentLoader] isClientRedirect]);
                WebFrame *parentFrame = [self parentFrame];
                if (parentFrame) {
                    WebHistoryItem *parentItem = parentFrame->_private->currentItem;
                    // The only case where parentItem==nil should be when a parent frame loaded an
                    // empty URL, which doesn't set up a current item in that parent.
                    if (parentItem)
                        [parentItem addChildItem:[self _createItem: YES]];
                } else {
                    // See 3556159.  It's not clear if it's valid to be in WebFrameLoadTypeOnLoadEvent
                    // for a top-level frame, but that was a likely explanation for those crashes,
                    // so let's guard against it.
                    // ...and all WebFrameLoadTypeOnLoadEvent uses were folded to WebFrameLoadTypeInternal
                    LOG_ERROR("no parent frame in _transitionToCommitted:, loadType=%d", loadType);
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
            [[[self webView] _frameLoadDelegateForwarder] webView:[self webView] didCommitLoadForFrame:self];
            
            // If we have a title let the WebView know about it.
            if (ptitle) {
                [[[self webView] _frameLoadDelegateForwarder] webView:[self webView]
                                                           didReceiveTitle:ptitle
                                                                  forFrame:self];
            }
            break;
        }
        
        case WebFrameStateCommittedPage:
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

+ (CFAbsoluteTime)_timeOfLastCompletedLoad
{
    return [WebFrameLoader timeOfLastCompletedLoad];
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
        [pageCache setObject:[NSDate date]  forKey: WebPageCacheEntryDateKey];
        [pageCache setObject:[self dataSource] forKey: WebPageCacheDataSourceKey];
        [pageCache setObject:[[self frameView] documentView] forKey: WebPageCacheDocumentViewKey];
    }
    return YES;
}

- (void)_provisionalLoadStarted
{
    WebFrameLoadType loadType = [[self _frameLoader] loadType];
    
    // FIXME: This is OK as long as no one resizes the window,
    // but in the case where someone does, it means garbage outside
    // the occupied part of the scroll view.
    [[[self frameView] _scrollView] setDrawsBackground:NO];
    
    // Cache the page, if possible.
    // Don't write to the cache if in the middle of a redirect, since we will want to
    // store the final page we end up on.
    // No point writing to the cache on a reload or loadSame, since we will just write
    // over it again when we leave that page.
    WebHistoryItem *item = _private->currentItem;
    if ([self _canCachePage]
        && [_private->bridge canCachePage]
        && item
        && ![[self _frameLoader] isQuickRedirectComing]
        && loadType != WebFrameLoadTypeReload 
        && loadType != WebFrameLoadTypeReloadAllowingStaleData
        && loadType != WebFrameLoadTypeSame
        && ![[self dataSource] isLoading]
        && ![[[self _frameLoader] documentLoader] isStopping]) {
        if ([[[self dataSource] representation] isKindOfClass:[WebHTMLRepresentation class]]) {
            if (![item pageCache]){
                // Add the items to this page's cache.
                if ([self _createPageCacheForItem:item]) {
                    LOG(PageCache, "Saving page to back/forward cache, %@\n", [[self dataSource] _URL]);
                    
                    // See if any page caches need to be purged after the addition of this
                    // new page cache.
                    [self _purgePageCache];
                }
                else
                    LOG(PageCache, "NOT saving page to back/forward cache, unable to create items, %@\n", [[self dataSource] _URL]);
            }
        } else
            // Put the document into a null state, so it can be restored correctly.
            [_private->bridge clear];
    } else
        LOG(PageCache, "NOT saving page to back/forward cache, %@\n", [[self dataSource] _URL]);
}

// Called after we send an openURL:... down to WebCore.
- (void)_opened
{
    if ([[self _frameLoader] loadType] == WebFrameLoadTypeStandard && [[[self dataSource] _documentLoader] isClientRedirect]) {
        // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
        // khtml has closed the URL and saved away the form state.
        WebHistoryItem *item = _private->currentItem;
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

        NSArray *responses = [[[self _frameLoader] documentLoader] responses];
        NSURLResponse *response;
        int i, count = [responses count];
        for (i = 0; i < count; i++){
            response = [responses objectAtIndex: i];
            // FIXME: If the WebKit client changes or cancels the request, this is not respected.
            NSError *error;
            id identifier;
            NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[response URL]];
            [self _requestFromDelegateForRequest:request identifier:&identifier error:&error];
            [self _sendRemainingDelegateMessagesWithIdentifier:identifier response:response length:(unsigned)[response expectedContentLength] error:error];
            [request release];
        }
        
        // Release the resources kept in the page cache.  They will be
        // reset when we leave this page.  The core side of the page cache
        // will have already been invalidated by the bridge to prevent
        // premature release.
        [_private->currentItem setHasPageCache:NO];

        [[[self _frameLoader] documentLoader] setPrimaryLoadComplete:YES];
        // why only this frame and not parent frames?
        [self _checkLoadCompleteForThisFrame];
    }
}

- (void)_checkLoadCompleteForThisFrame
{
    ASSERT([self webView] != nil);

    switch ([[self _frameLoader] state]) {
        case WebFrameStateProvisional:
        {
            if ([[self _frameLoader] delegateIsHandlingProvisionalLoadError])
                return;

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
                    [[self _frameLoader] setDelegateIsHandlingProvisionalLoadError:YES];
                    [[[self webView] _frameLoadDelegateForwarder] webView:[self webView]
                                          didFailProvisionalLoadWithError:error
                                                                 forFrame:self];
                    [[self _frameLoader] setDelegateIsHandlingProvisionalLoadError:NO];
                    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:error];

                    // FIXME: can stopping loading here possibly have
                    // any effect, if isLoading is false, which it
                    // must be, to be in this branch of the if? And is it ok to just do 
                    // a full-on stopLoading?
                    [[self _frameLoader] stopLoadingSubframes];
                    [[pd _documentLoader] stopLoading];

                    // Finish resetting the load state, but only if another load hasn't been started by the
                    // delegate callback.
                    if (pd == [[self _frameLoader] provisionalDataSource])
                        [[self _frameLoader] clearProvisionalLoad];
                    else {
                        NSURL *unreachableURL = [[[self _frameLoader] provisionalDataSource] unreachableURL];
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
        {
            WebDataSource *ds = [self dataSource];
            
            //LOG(Loading, "%@:  checking complete, current state WEBFRAMESTATE_COMMITTED", [self name]);
            if (![ds isLoading]) {
                WebFrameView *thisView = [self frameView];
                NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
                ASSERT(thisDocumentView != nil);

                [[self _frameLoader] markLoadComplete];

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
                    switch ([[self _frameLoader] loadType]) {
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
                    case WebFrameLoadTypeReplace:
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
                    [[[self webView] _frameLoadDelegateForwarder] webView:[self webView]
                                                     didFailLoadWithError:error
                                                                 forFrame:self];
                    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:error];
                } else {
                    [[self webView] _didFinishLoadForFrame:self];
                    [[[self webView] _frameLoadDelegateForwarder] webView:[self webView]
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

- (void)_handledOnloadEvents
{
    [[[self webView] _frameLoadDelegateForwarder] webView:[self webView] didHandleOnloadEventsForFrame:self];
}

// Called every time a resource is completely loaded, or an error is received.
- (void)_checkLoadComplete
{
    ASSERT([self webView] != nil);

    WebFrame *parent;
    for (WebFrame *frame = self; frame; frame = parent) {
        [frame retain];
        [frame _checkLoadCompleteForThisFrame];
        parent = [frame parentFrame];
        [frame release];
    }
}

- (WebFrameBridge *)_bridge
{
    return _private->bridge;
}

// helper method that determines whether the subframes described by the item's subitems
// match our own current frameset
- (BOOL)_childFramesMatchItem:(WebHistoryItem *)item
{
    NSArray *childItems = [item children];
    int numChildItems = [childItems count];
    int numChildFrames = [self _childFrameCount];
    if (numChildFrames != numChildItems)
        return NO;

    int i;
    for (i = 0; i < numChildItems; i++) {
        NSString *itemTargetName = [[childItems objectAtIndex:i] target];
        //Search recursive here?
        if (![self _immediateChildFrameNamed:itemTargetName])
            return NO; // couldn't match the i'th itemTarget
    }

    return YES; // found matches for all itemTargets
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
    if (!formData && ![[self _frameLoader] shouldReloadForCurrent:itemURL andDestination:currentURL] && [self _URLsMatchItem:item] )
    {
#if 0
        // FIXME:  We need to normalize the code paths for anchor navigation.  Something
        // like the following line of code should be done, but also accounting for correct
        // updates to the back/forward list and scroll position.
        // rjw 4/9/03 See 3223929.
        [self _loadURL:itemURL referrer:[[[self dataSource] request] HTTPReferrer] loadType:loadType target:nil triggeringEvent:nil form:nil formValues:nil];
#endif
        // must do this maintenance here, since we don't go through a real page reload
        [self _saveScrollPositionAndViewStateToItem:_private->currentItem];
        // FIXME: form state might want to be saved here too

        // We always call scrollToAnchorWithURL here, even if the URL doesn't have an
        // anchor fragment. This is so we'll keep the WebCore Frame's URL up-to-date.
        [_private->bridge scrollToAnchorWithURL:[item URL]];
    
        // must do this maintenance here, since we don't go through a real page reload
        [_private setCurrentItem:item];
        [self _restoreScrollPositionAndViewState];

        // Fake the URL change by updating the data source's request.  This will no longer
        // be necessary if we do the better fix described above.
        [[[self _frameLoader] documentLoader] replaceRequestURLForAnchorScrollWithURL:itemURL];
        
        [[[self webView] _frameLoadDelegateForwarder] webView:[self webView]
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
                [[self _frameLoader] loadDataSource:newDataSource withLoadType:loadType formState:nil];   
                inPageCache = YES;
            }
            else {
                LOG (PageCache, "Not restoring page from back/forward cache because cache entry has expired, %@ (%3.5f > %3.5f seconds)\n", [_private->provisionalItem URL], delta, [[[self webView] preferences] _backForwardCacheExpirationInterval]);
                [item setHasPageCache: NO];
            }
        }
        
        if (!inPageCache) {
            NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:itemURL];
            [self _addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:(formData != nil) ? YES : NO];

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

            [[self _frameLoader] _loadRequest:request triggeringAction:action loadType:loadType formState:nil];
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
        ASSERT(!_private->previousItem);
        [_private->bridge saveDocumentState];
        [self _saveScrollPositionAndViewStateToItem:_private->currentItem];
        
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
- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)type
{
    ASSERT(![self parentFrame]);
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    if ([[[self webView] _policyDelegateForwarder] webView:[self webView] shouldGoToHistoryItem:item]) {    
        WebBackForwardList *backForwardList = [[self webView] backForwardList];
        WebHistoryItem *currItem = [backForwardList currentItem];
        // Set the BF cursor before commit, which lets the user quickly click back/forward again.
        // - plus, it only makes sense for the top level of the operation through the frametree,
        // as opposed to happening for some/one of the page commits that might happen soon
        [backForwardList goToItem:item];
        [self _recursiveGoToItem:item fromItem:currItem withLoadType:type];
    }
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

- (void)_continueLoadRequestAfterNewWindowPolicy:(NSURLRequest *)request frameName:(NSString *)frameName formState:(WebFormState *)formState
{
    if (!request)
        return;

    [self retain];

    WebView *webView = nil;
    WebView *currentWebView = [self webView];
    id wd = [currentWebView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        webView = [wd webView:currentWebView createWebViewWithRequest:nil];
    else
        webView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:nil];
    if (!webView)
        goto exit;

    WebFrame *frame = [webView mainFrame];
    if (!frame)
        goto exit;

    [frame retain];

    [[frame _bridge] setName:frameName];

    [[webView _UIDelegateForwarder] webViewShow:webView];

    [[frame _bridge] setOpener:[self _bridge]];
    [[frame _frameLoader] _loadRequest:request triggeringAction:nil loadType:WebFrameLoadTypeStandard formState:formState];

    [frame release];

exit:
    [self release];
}

- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame
{
    WebHistoryItem *parentItem = _private->currentItem;
    NSArray *childItems = [parentItem children];
    WebFrameLoadType loadType = [[self _frameLoader] loadType];
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
        [[childFrame _frameLoader] loadURL:URL referrer:referrer loadType:childLoadType target:nil triggeringEvent:nil form:nil formValues:nil];
    }
}

- (void)_postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSArray *)postData contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values
{
    // When posting, use the NSURLRequestReloadIgnoringCacheData load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.

    // FIXME: Where's the code that implements what the comment above says?

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [self _addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:YES];
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
            [[targetFrame _frameLoader] _loadRequest:request triggeringAction:action loadType:WebFrameLoadTypeStandard formState:formState];
        } else {
            [[self _frameLoader] checkNewWindowPolicyForRequest:request action:action frameName:target formState:formState andCall:self
                withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
        }
        [request release];
        [formState release];
        return;
    }

    [[self _frameLoader] _loadRequest:request triggeringAction:action loadType:WebFrameLoadTypeStandard formState:formState];

    [request release];
    [formState release];
}

- (void)_setTitle:(NSString *)title
{
    [_private->currentItem setTitle:title];
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

- (void)_defersCallbacksChanged
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self])
        [[frame _frameLoader] defersCallbacksChanged];
}

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self])
        [[[frame frameView] documentView] viewWillMoveToHostWindow:hostWindow];
}

- (void)_viewDidMoveToHostWindow
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self])
        [[[frame frameView] documentView] viewDidMoveToHostWindow];
}

- (void)_addChild:(WebFrame *)child
{
    [[self _bridge] appendChild:[child _bridge]];
    [[[child dataSource] _documentLoader] setOverrideEncoding:[[[self dataSource] _documentLoader] overrideEncoding]];  
}

// If we bailed out of a b/f navigation, we might need to set the b/f cursor back to the current
// item, because we optimistically move it right away at the start of the operation. But when
// alternate content is loaded for an unreachableURL, we don't want to reset the b/f cursor.
// Return the item that we would reset to, so we can decide later whether to actually reset.
- (WebHistoryItem *)_currentBackForwardListItemToResetTo
{
    WebFrameLoadType loadType = [[self _frameLoader] loadType];
    if ((loadType == WebFrameLoadTypeForward
         || loadType == WebFrameLoadTypeBack
         || loadType == WebFrameLoadTypeIndexedBackForward)
        && self == [[self webView] mainFrame]) {
        return _private->currentItem;
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

    return _private->previousItem ? _private->previousItem : _private->currentItem;
}

- (WebHistoryItem *)_itemForRestoringDocState
{
    switch ([[self _frameLoader] loadType]) {
        case WebFrameLoadTypeReload:
        case WebFrameLoadTypeReloadAllowingStaleData:
        case WebFrameLoadTypeSame:
        case WebFrameLoadTypeReplace:
            // Don't restore any form state on reload or loadSame
            return nil;
        case WebFrameLoadTypeBack:
        case WebFrameLoadTypeForward:
        case WebFrameLoadTypeIndexedBackForward:
        case WebFrameLoadTypeInternal:
        case WebFrameLoadTypeStandard:
            return _private->currentItem;
    }
    ASSERT_NOT_REACHED();
    return nil;
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
- (void)_saveDocumentAndScrollState
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        [[frame _bridge] saveDocumentState];
        [frame _saveScrollPositionAndViewStateToItem:frame->_private->currentItem];
    }
}

// used to decide to use loadType=Same
- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL
{
    WebHistoryItem *item = _private->currentItem;
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
    [[self _frameLoader] checkNewWindowPolicyForRequest:request action:action frameName:frameName formState:nil andCall:self withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
}

// Return next frame to be traversed, visiting children after parent
- (WebFrame *)_nextFrameWithWrap:(BOOL)wrapFlag
{
    return Frame([[self _bridge] nextFrameWithWrap:wrapFlag]);
}

// Return previous frame to be traversed, exact reverse order of _nextFrame
- (WebFrame *)_previousFrameWithWrap:(BOOL)wrapFlag
{
    return Frame([[self _bridge] previousFrameWithWrap:wrapFlag]);
}

- (BOOL)_shouldCreateRenderers
{
    return [_private->bridge shouldCreateRenderers];
}

- (int)_numPendingOrLoadingRequests:(BOOL)recurse
{
    if (!recurse)
        return [[self _bridge] numPendingOrLoadingRequests];

    int num = 0;
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self])
        num += [[frame _bridge] numPendingOrLoadingRequests];

    return num;
}

- (void)_reloadForPluginChanges
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        NSView <WebDocumentView> *documentView = [[frame frameView] documentView];
        if (([documentView isKindOfClass:[WebHTMLView class]] && [_private->bridge containsPlugins]))
            [frame reload];
    }
}

- (void)_attachScriptDebugger
{
    if (!_private->scriptDebugger)
        _private->scriptDebugger = [[WebScriptDebugger alloc] initWithWebFrame:self];
}

- (void)_detachScriptDebugger
{
    if (_private->scriptDebugger) {
        id old = _private->scriptDebugger;
        _private->scriptDebugger = nil;
        [old release];
    }
}

- (void)_recursive_pauseNullEventsForAllNetscapePlugins
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        NSView <WebDocumentView> *documentView = [[frame frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _pauseNullEventsForAllNetscapePlugins];
    }
}

- (void)_recursive_resumeNullEventsForAllNetscapePlugins
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        NSView <WebDocumentView> *documentView = [[frame frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _resumeNullEventsForAllNetscapePlugins];
    }
}

- (void)_didReceiveServerRedirectForProvisionalLoadForFrame
{
    [[[self webView] _frameLoadDelegateForwarder] webView:[self webView]
        didReceiveServerRedirectForProvisionalLoadForFrame:self];
}

- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v bridge:(WebFrameBridge *)bridge
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebFramePrivate alloc] init];

    _private->bridge = bridge;

    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebFrame:self];
    }
    
    ++WebFrameCount;
    
    return self;
}

- (NSArray *)_documentViews
{
    NSMutableArray *result = [NSMutableArray array];
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        id docView = [[frame frameView] documentView];
        if (docView)
            [result addObject:docView];
    }
        
    return result;
}

- (void)_updateBackground
{
    BOOL drawsBackground = [[self webView] drawsBackground];
    NSColor *backgroundColor = [[self webView] backgroundColor];

    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        // Never call setDrawsBackground:YES here on the scroll view or the background color will
        // flash between pages loads. setDrawsBackground:YES will be called in WebFrame's _frameLoadCompleted.
        if (!drawsBackground)
            [[[frame frameView] _scrollView] setDrawsBackground:NO];
        [[[frame frameView] _scrollView] setBackgroundColor:backgroundColor];
        id documentView = [[frame frameView] documentView];
        if ([documentView respondsToSelector:@selector(setDrawsBackground:)])
            [documentView setDrawsBackground:drawsBackground];
        if ([documentView respondsToSelector:@selector(setBackgroundColor:)])
            [documentView setBackgroundColor:backgroundColor];
        [[frame _bridge] setDrawsBackground:drawsBackground];
        [[frame _bridge] setBaseBackgroundColor:backgroundColor];
    }
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
            [delegate webView:wv resource:identifier didReceiveContentLength:(WebNSUInteger)length fromDataSource:dataSource];
        } else {
            [sharedDelegate webView:wv resource:identifier didReceiveContentLength:(WebNSUInteger)length fromDataSource:dataSource];
        }
    }
    
    if (error == nil) {
        if (implementations.delegateImplementsDidFinishLoadingFromDataSource) {
            [delegate webView:wv resource:identifier didFinishLoadingFromDataSource:dataSource];
        } else {
            [sharedDelegate webView:wv resource:identifier didFinishLoadingFromDataSource:dataSource];
        }
        [self _checkLoadComplete];
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

- (void)_unmarkAllMisspellings
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self])
        [[frame _bridge] unmarkAllMisspellings];
}

- (BOOL)_hasSelection
{
    id documentView = [[self frameView] documentView];    
    
    // optimization for common case to avoid creating potentially large selection string
    if ([documentView isKindOfClass:[WebHTMLView class]]) {
        DOMRange *selectedDOMRange = [[self _bridge] selectedDOMRange];
        return selectedDOMRange && ![selectedDOMRange collapsed];
    }
    
    if ([documentView conformsToProtocol:@protocol(WebDocumentText)])
        return [[documentView selectedString] length] > 0;
    
    return NO;
}

- (void)_clearSelection
{
    id documentView = [[self frameView] documentView];    
    if ([documentView conformsToProtocol:@protocol(WebDocumentText)])
        [documentView deselectAll];
}

#ifndef NDEBUG

- (BOOL)_atMostOneFrameHasSelection;
{
    // FIXME: 4186050 is one known case that makes this debug check fail
    BOOL found = NO;
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self]) {
        if ([frame _hasSelection]) {
            if (found)
                return NO;
            found = YES;
        }
    }

    return YES;
}
#endif

- (WebFrame *)_findFrameWithSelection
{
    for (WebFrame *frame = self; frame; frame = [frame _traverseNextFrameStayWithin:self])
        if ([frame _hasSelection])
            return frame;

    return nil;
}

- (void)_clearSelectionInOtherFrames
{
    // We rely on WebDocumentSelection protocol implementors to call this method when they become first 
    // responder. It would be nicer to just notice first responder changes here instead, but there's no 
    // notification sent when the first responder changes in general (Radar 2573089).
    WebFrame *frameWithSelection = [[[self webView] mainFrame] _findFrameWithSelection];
    if (frameWithSelection != self)
        [frameWithSelection _clearSelection];

    // While we're in the general area of selection and frames, check that there is only one now.
    ASSERT([[[self webView] mainFrame] _atMostOneFrameHasSelection]);
}

- (BOOL)_subframeIsLoading
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (WebFrame *frame = [self _lastChildFrame]; frame; frame = [frame _previousSiblingFrame])
        if ([[frame dataSource] isLoading] || [[frame provisionalDataSource] isLoading])
            return YES;
    return NO;
}

- (void)_addPlugInView:(NSView *)plugInView
{
    ASSERT([plugInView respondsToSelector:@selector(setWebFrame:)]);
    ASSERT(![_private->plugInViews containsObject:plugInView]);
    
    if (!_private->plugInViews)
        _private->plugInViews = [[NSMutableSet alloc] init];
        
    [plugInView setWebFrame:self];
    [_private->plugInViews addObject:plugInView];
}

- (void)_removeAllPlugInViews
{
    if (!_private->plugInViews)
        return;
    
    [_private->plugInViews makeObjectsPerformSelector:@selector(setWebFrame:) withObject:nil];
    [_private->plugInViews release];
    _private->plugInViews = nil;
}

// This is called when leaving a page or closing the WebView
- (void)_willCloseURL
{
    [self _removeAllPlugInViews];
}

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request mainResource:(BOOL)mainResource alwaysFromRequest:(BOOL)f
{
    [request _web_setHTTPUserAgent:[[self webView] userAgentForURL:[request URL]]];
    
    if ([[self _frameLoader] loadType] == WebFrameLoadTypeReload)
        [request setValue:@"max-age=0" forHTTPHeaderField:@"Cache-Control"];
    
    // Don't set the cookie policy URL if it's already been set.
    if ([request mainDocumentURL] == nil) {
        if (mainResource && (self == [[self webView] mainFrame] || f))
            [request setMainDocumentURL:[request URL]];
        else
            [request setMainDocumentURL:[[[[self webView] mainFrame] dataSource] _URL]];
    }
    
    if (mainResource)
        [request setValue:@"text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5" forHTTPHeaderField:@"Accept"];
}

- (BOOL)_isMainFrame
{
    return self == [[self webView] mainFrame];
}

- (void)_addInspector:(WebInspector *)inspector
{
    if (!_private->inspectors)
        _private->inspectors = [[NSMutableSet alloc] init];
    ASSERT(![_private->inspectors containsObject:inspector]);
    [_private->inspectors addObject:inspector];
}

- (void)_removeInspector:(WebInspector *)inspector
{
    ASSERT([_private->inspectors containsObject:inspector]);
    [_private->inspectors removeObject:inspector];
}

- (WebFrameLoader *)_frameLoader
{
    return [_private->bridge loader];
}

- (void)_prepareForDataSourceReplacement
{
    if (![[self _frameLoader] dataSource]) {
        ASSERT(![self _childFrameCount]);
        return;
    }
    
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
}

- (void)_frameLoadCompleted
{
    NSScrollView *sv = [[self frameView] _scrollView];
    if ([[self webView] drawsBackground])
        [sv setDrawsBackground:YES];
    [_private setPreviousItem:nil];
}

- (WebDataSource *)_dataSourceForDocumentLoader:(WebDocumentLoader *)loader
{
    return [(WebDocumentLoaderMac *)loader dataSource];
}

- (WebDocumentLoader *)_createDocumentLoaderWithRequest:(NSURLRequest *)request
{
    WebDocumentLoaderMac *loader = [[WebDocumentLoaderMac alloc] initWithRequest:request];
    
    WebDataSource *dataSource = [[WebDataSource alloc] _initWithDocumentLoader:loader];
    [loader setDataSource:dataSource];
    [dataSource release];

    return loader;
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
    ASSERT(_private->currentItem);
    NSView <WebDocumentView> *docView = [[self frameView] documentView];
    NSPoint point = [_private->currentItem scrollPoint];
    if ([docView conformsToProtocol:@protocol(_WebDocumentViewState)]) {        
        id state = [_private->currentItem viewState];
        if (state) {
            [(id <_WebDocumentViewState>)docView setViewState:state];
        }
        
        [(id <_WebDocumentViewState>)docView setScrollPoint:point];
    } else {
        [docView scrollPoint:point];
    }
}

@end

@implementation WebFrame (WebPrivate)

// FIXME: this exists only as a convenience for Safari, consider moving there
- (BOOL)_isDescendantOfFrame:(WebFrame *)ancestor
{
    return [[self _bridge] isDescendantOfFrame:[ancestor _bridge]];
}

- (void)_setShouldCreateRenderers:(BOOL)f
{
    [_private->bridge setShouldCreateRenderers:f];
}

- (NSColor *)_bodyBackgroundColor
{
    return [_private->bridge bodyBackgroundColor];
}

- (BOOL)_isFrameSet
{
    return [_private->bridge isFrameSet];
}

- (BOOL)_firstLayoutDone
{
    return [[self _frameLoader] firstLayoutDone];
}

- (WebFrameLoadType)_loadType
{
    return [[self _frameLoader] loadType];
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

- (id)init
{
    return [self initWithName:nil webFrameView:nil webView:nil];
}

// FIXME: this method can't work any more and should be marked deprecated
- (id)initWithName:(NSString *)n webFrameView:(WebFrameView *)fv webView:(WebView *)v
{
    return [self _initWithWebFrameView:fv webView:v bridge:nil];
}

- (void)dealloc
{
    ASSERT(_private->bridge == nil);
    [_private release];

    --WebFrameCount;

    [super dealloc];
}

- (void)finalize
{
    ASSERT(_private->bridge == nil);

    --WebFrameCount;

    [super finalize];
}

- (NSString *)name
{
    return [[self _bridge] name];
}

- (WebFrameView *)frameView
{
    return _private->webFrameView;
}

- (WebView *)webView
{
    return [[[self _bridge] page] webView];
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
    return [[self _frameLoader] provisionalDataSource];
}

- (WebDataSource *)dataSource
{
    return [[self _frameLoader] dataSource];
}

- (void)loadRequest:(NSURLRequest *)request
{
    // FIXME: is this the right place to reset loadType? Perhaps this should be done
    // after loading is finished or aborted.
    [[self _frameLoader] setLoadType:WebFrameLoadTypeStandard];
    [[self _frameLoader] _loadRequest:request archive:nil];
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
    NSData *data = [string dataUsingEncoding:NSUTF8StringEncoding];
    [self _loadData:data MIMEType:nil textEncodingName:@"UTF-8" baseURL:URL unreachableURL:unreachableURL];
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
        [[self _frameLoader] _loadRequest:request archive:archive];
    }
}

- (void)stopLoading
{
    [[self _frameLoader] stopLoading];
}

- (void)reload
{
    [[self _frameLoader] reload];
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    return Frame([[self _bridge] findFrameNamed:name]);
}

- (WebFrame *)parentFrame
{
    return [[Frame([[self _bridge] parent]) retain] autorelease];
}

- (NSArray *)childFrames
{
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:[self _childFrameCount]];
    for (WebFrame *child = [self _firstChildFrame]; child; child = [child _nextSiblingFrame])
        [children addObject:child];

    return children;
}

@end

@implementation WebFrame (WebFrameLoaderClient)

- (void)_resetBackForwardList
{
    // Note this doesn't verify the current load type as a b/f operation because it is called from
    // a subframe in the case of a delegate bailing out of the nav before it even gets to provisional state.
    ASSERT(self == [[self webView] mainFrame]);
    WebHistoryItem *resetItem = _private->currentItem;
    if (resetItem)
        [[[self webView] backForwardList] goToItem:resetItem];
}

- (void)_invalidateCurrentItemPageCache
{
    // When we are pre-commit, the currentItem is where the pageCache data resides
    NSDictionary *pageCache = [_private->currentItem pageCache];

    [[self _bridge] invalidatePageCache:pageCache];
    
    // We're assuming that WebCore invalidates its pageCache state in didNotOpen:pageCache:
    [_private->currentItem setHasPageCache:NO];
}

- (BOOL)_provisionalItemIsTarget
{
    return [_private->provisionalItem isTargetItem];
}

- (BOOL)_loadProvisionalItemFromPageCache
{
    WebHistoryItem *item = _private->provisionalItem;
    if (![item hasPageCache])
        return NO;
    NSDictionary *pageCache = [item pageCache];
    if (![pageCache objectForKey:WebCorePageCacheStateKey])
        return NO;
    LOG(PageCache, "Restoring page from back/forward cache, %@", [item URL]);
    [[self provisionalDataSource] _loadFromPageCache:pageCache];
    return YES;
}

@end
