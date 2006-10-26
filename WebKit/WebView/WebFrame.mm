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
#import "WebDataSourceInternal.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDefaultUIDelegate.h"
#import "WebDocumentInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebDownloadInternal.h"
#import "WebFrameBridge.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoaderClient.h"
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
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebNullPluginView.h"
#import "WebPlugin.h"
#import "WebPluginController.h"
#import "WebPolicyDeciderMac.h"
#import "WebPreferencesPrivate.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourcePrivate.h"
#import "WebScriptDebugDelegatePrivate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebUIDelegate.h"
#import "WebViewInternal.h"
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameMac.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/Page.h>
#import <WebCore/SelectionController.h>
#import <WebCore/WebDataProtocol.h>
#import <WebCore/WebFormDataStream.h>
#import <WebCore/WebFormState.h>
#import <WebCore/WebLoader.h>
#import <WebKit/DOM.h>
#import <WebKitSystemInterface.h>
#import <objc/objc-runtime.h>
#import <wtf/HashMap.h>

// FIXME: We should have a way to get the following DOM interface from the WebCore internal headers, but we
// can't make the internal DOM headers private since they are not generated at the time installhdrs is called.

@interface DOMDocument (WebCoreInternal)
- (WebCore::Document *)_document;
+ (DOMDocument *)_documentWith:(WebCore::Document *)impl;
@end

@interface DOMElement (WebCoreInternal)
- (WebCore::Element *)_element;
+ (DOMElement *)_elementWith:(WebCore::Element *)impl;
@end

@interface DOMHTMLElement (WebCoreInternal)
- (WebCore::HTMLElement *)_HTMLElement;
+ (DOMHTMLElement *)_HTMLElementWith:(WebCore::HTMLElement *)impl;
@end

@interface DOMRange (WebCoreInternal)
- (WebCore::Range *)_range;
+ (DOMRange *)_rangeWith:(WebCore::Range *)impl;
@end

/*
Here is the current behavior matrix for four types of navigations:

Standard Nav:

 Restore form state:   YES
 Restore scroll and focus state:  YES
 Cache policy: NSURLRequestUseProtocolCachePolicy
 Add to back/forward list: YES
 
Back/Forward:

 Restore form state:   YES
 Restore scroll and focus state:  YES
 Cache policy: NSURLRequestReturnCacheDataElseLoad
 Add to back/forward list: NO

Reload (meaning only the reload button):

 Restore form state:   NO
 Restore scroll and focus state:  YES
 Cache policy: NSURLRequestReloadIgnoringCacheData
 Add to back/forward list: NO

Repeat load of the same URL (by any other means of navigation other than the reload button, including hitting return in the location field):

 Restore form state:   NO
 Restore scroll and focus state:  NO, reset to initial conditions
 Cache policy: NSURLRequestReloadIgnoringCacheData
 Add to back/forward list: NO
*/

using namespace WebCore;

typedef HashMap<RefPtr<WebResourceLoader>, RetainPtr<WebResource> > ResourceMap;

NSString *WebPageCacheEntryDateKey = @"WebPageCacheEntryDateKey";
NSString *WebPageCacheDataSourceKey = @"WebPageCacheDataSourceKey";
NSString *WebPageCacheDocumentViewKey = @"WebPageCacheDocumentViewKey";

@interface WebFrame (ForwardDecls)
- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;
- (void)_saveScrollPositionAndViewStateToItem:(WebHistoryItem *)item;
- (WebHistoryItem *)_createItem:(BOOL)useOriginal;
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;
- (WebHistoryItem *)_currentBackForwardListItemToResetTo;
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

    ResourceMap* pendingArchivedResources;
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
    
    ASSERT(plugInViews == nil);
    [inspectors release];

    delete pendingArchivedResources;

    [super dealloc];
}

- (void)finalize
{
    delete pendingArchivedResources;

    [super finalize];
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

static inline WebFrame *frame(WebCoreFrameBridge *bridge)
{
    return [(WebFrameBridge *)bridge webFrame];
}

FrameMac* core(WebFrame *frame)
{
    return [[frame _bridge] _frame];
}

WebFrame *kit(Frame* frame)
{
    return frame ? [(WebFrameBridge *)Mac(frame)->bridge() webFrame] : nil;
}

Element* core(DOMElement *element)
{
    return [element _element];
}

DOMElement *kit(Element* element)
{
    return [DOMElement _elementWith:element];
}

Document* core(DOMDocument *document)
{
    return [document _document];
}

DOMDocument *kit(Document* document)
{
    return [DOMDocument _documentWith:document];
}

HTMLElement* core(DOMHTMLElement *element)
{
    return [element _HTMLElement];
}

DOMHTMLElement *kit(HTMLElement *element)
{
    return [DOMHTMLElement _HTMLElementWith:element];
}

Range* core(DOMRange *range)
{
    return [range _range];
}

DOMRange *kit(Range* range)
{
    return [DOMRange _rangeWith:range];
}

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

- (WebHistoryItem *)_createItem:(BOOL)useOriginal
{
    WebDataSource *dataSrc = [self dataSource];
    NSURLRequest *request;
    NSURL *unreachableURL = [dataSrc unreachableURL];
    NSURL *URL;
    NSURL *originalURL;
    WebHistoryItem *bfItem;

    if (useOriginal)
        request = [dataSrc _documentLoader]->originalRequestCopy();
    else
        request = [dataSrc request];

    if (unreachableURL != nil) {
        URL = unreachableURL;
        originalURL = unreachableURL;
    } else {
        URL = [request URL];
        originalURL = [[dataSrc _documentLoader]->originalRequestCopy() URL];
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

        for (Frame* child = core(self)->tree()->firstChild(); child; child = child->tree()->nextSibling())
            [bfItem addChildItem:[kit(child) _createItemTreeWithTargetFrame:targetFrame clippedAtTarget:doClip]];
    }
    if (self == targetFrame)
        [bfItem setIsTargetItem:YES];

    return bfItem;
}

- (BOOL)_canCachePage
{
    return [[[self webView] backForwardList] _usesPageCache] && core(self)->canCachePage();
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
    return FrameLoader::timeOfLastCompletedLoad();
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
    int numChildFrames = core(self)->tree()->childCount();
    if (numChildFrames != numChildItems)
        return NO;

    int i;
    for (i = 0; i < numChildItems; i++) {
        NSString *itemTargetName = [[childItems objectAtIndex:i] target];
        //Search recursive here?
        if (!core(self)->tree()->child(itemTargetName))
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
        childFrame = kit(core(self)->tree()->child([childItem target]));
        if (![childFrame _URLsMatchItem:childItem])
            return NO;
    }
    
    return YES;
}

// loads content into this frame, as specified by item
- (void)_loadItem:(WebHistoryItem *)item withLoadType:(FrameLoadType)loadType
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
    if (!formData && ![self _frameLoader]->shouldReload(itemURL, currentURL) && [self _URLsMatchItem:item]) {
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

        // We always call scrollToAnchor here, even if the URL doesn't have an
        // anchor fragment. This is so we'll keep the WebCore Frame's URL up-to-date.
        core(self)->scrollToAnchor([item URL]);
    
        // must do this maintenance here, since we don't go through a real page reload
        [_private setCurrentItem:item];
        [self _restoreScrollPositionAndViewState];

        // Fake the URL change by updating the data source's request.  This will no longer
        // be necessary if we do the better fix described above.
        [self _frameLoader]->documentLoader()->replaceRequestURLForAnchorScroll(itemURL);
        
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
            if (delta <= [[[self webView] preferences] _backForwardCacheExpirationInterval]) {
                newDataSource = [pageCache objectForKey: WebPageCacheDataSourceKey];
                [self _frameLoader]->load([newDataSource _documentLoader], loadType, 0);   
                inPageCache = YES;
            } else {
                LOG (PageCache, "Not restoring page from back/forward cache because cache entry has expired, %@ (%3.5f > %3.5f seconds)\n", [_private->provisionalItem URL], delta, [[[self webView] preferences] _backForwardCacheExpirationInterval]);
                [item setHasPageCache: NO];
            }
        }
        
        if (!inPageCache) {
            NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:itemURL];
            [self _frameLoader]->addExtraFieldsToRequest(request, true, (formData != nil));

            // If this was a repost that failed the page cache, we might try to repost the form.
            NSDictionary *action;
            if (formData) {
                [request setHTTPMethod:@"POST"];
                [request _web_setHTTPReferrer:[item formReferrer]];
                webSetHTTPBody(request, formData);
                [request _web_setHTTPContentType:[item formContentType]];

                // Slight hack to test if the NSURL cache contains the page we're going to.  We want
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
                    // Not in the NSURL cache
                    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
                    action = [self _frameLoader]->actionInformation(NavigationTypeFormResubmitted, nil, itemURL);
                } else
                    // We can use the cache, don't use navType=resubmit
                    action = [self _frameLoader]->actionInformation(loadType, false, nil, itemURL);
            } else {
                switch (loadType) {
                    case FrameLoadTypeReload:
                        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
                        break;
                    case FrameLoadTypeBack:
                    case FrameLoadTypeForward:
                    case FrameLoadTypeIndexedBackForward:
                        if (![[itemURL scheme] isEqual:@"https"])
                            [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];
                        break;
                    case FrameLoadTypeStandard:
                    case FrameLoadTypeInternal:
                        // no-op: leave as protocol default
                        // FIXME:  I wonder if we ever hit this case
                        break;
                    case FrameLoadTypeSame:
                    case FrameLoadTypeReloadAllowingStaleData:
                    default:
                        ASSERT_NOT_REACHED();
                }

                action = [self _frameLoader]->actionInformation(loadType, false, nil, itemOriginalURL);
            }

            [self _frameLoader]->load(request, action, loadType, 0);
            [request release];
        }
    }
}

// The general idea here is to traverse the frame tree and the item tree in parallel,
// tracking whether each frame already has the content the item requests.  If there is
// a match (by URL), we just restore scroll position and recurse.  Otherwise we must
// reload that frame, and all its kids.
- (void)_recursiveGoToItem:(WebHistoryItem *)item fromItem:(WebHistoryItem *)fromItem withLoadType:(FrameLoadType)type
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
            WebFrame *childFrame = kit(core(self)->tree()->child(childName));
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
- (void)_goToItem:(WebHistoryItem *)item withLoadType:(FrameLoadType)type
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

- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame
{
    WebHistoryItem *parentItem = _private->currentItem;
    NSArray *childItems = [parentItem children];
    FrameLoadType loadType = [self _frameLoader]->loadType();
    FrameLoadType childLoadType = FrameLoadTypeInternal;
    WebHistoryItem *childItem = nil;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (childItems &&
        (isBackForwardLoadType(loadType)
         || loadType == FrameLoadTypeReload
         || loadType == FrameLoadTypeReloadAllowingStaleData))
    {
        childItem = [parentItem childItemWithName:[childFrame name]];
        if (childItem) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            URL = [NSURL _web_URLWithDataAsString:[childItem originalURLString]];
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any child items as child frames load
                [childFrame->_private setProvisionalItem:childItem];
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                [childFrame->_private setCurrentItem:childItem];
        }
    }

    WebArchive *archive = [[self dataSource] _popSubframeArchiveWithName:[childFrame name]];
    if (archive)
        [childFrame loadArchive:archive];
    else
        [childFrame _frameLoader]->load(URL, referrer, childLoadType, nil, nil, nil, nil);
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

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewWillMoveToHostWindow:hostWindow];
}

- (void)_viewDidMoveToHostWindow
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [[[kit(frame) frameView] documentView] viewDidMoveToHostWindow];
}

- (void)_addChild:(WebFrame *)child
{
    core(self)->tree()->appendChild(adoptRef(core(child)));
    if ([child dataSource])
        [[child dataSource] _documentLoader]->setOverrideEncoding([[self dataSource] _documentLoader]->overrideEncoding());  
}

// If we bailed out of a b/f navigation, we might need to set the b/f cursor back to the current
// item, because we optimistically move it right away at the start of the operation. But when
// alternate content is loaded for an unreachableURL, we don't want to reset the b/f cursor.
// Return the item that we would reset to, so we can decide later whether to actually reset.
- (WebHistoryItem *)_currentBackForwardListItemToResetTo
{
    if (isBackForwardLoadType([self _frameLoader]->loadType()) && [self _isMainFrame])
        return _private->currentItem;
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
    switch ([self _frameLoader]->loadType()) {
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadAllowingStaleData:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            // Don't restore any form state on reload or loadSame
            return nil;
        case FrameLoadTypeBack:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
        case FrameLoadTypeInternal:
        case FrameLoadTypeStandard:
            return _private->currentItem;
    }
    ASSERT_NOT_REACHED();
    return nil;
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
- (void)_saveDocumentAndScrollState
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        [Mac(frame)->bridge() saveDocumentState];
        [kit(frame) _saveScrollPositionAndViewStateToItem:kit(frame)->_private->currentItem];
    }
}

- (int)_numPendingOrLoadingRequests:(BOOL)recurse
{
    if (!recurse)
        return [_private->bridge numPendingOrLoadingRequests];

    int num = 0;
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        num += [Mac(frame)->bridge() numPendingOrLoadingRequests];

    return num;
}

- (void)_reloadForPluginChanges
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if (([documentView isKindOfClass:[WebHTMLView class]] && coreFrame->containsPlugins()))
            [kit(frame) reload];
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
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _pauseNullEventsForAllNetscapePlugins];
    }
}

- (void)_recursive_resumeNullEventsForAllNetscapePlugins
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if ([documentView isKindOfClass:[WebHTMLView class]])
            [(WebHTMLView *)documentView _resumeNullEventsForAllNetscapePlugins];
    }
}

- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v coreFrame:(Frame*)coreFrame
{
    self = [super init];
    if (!self)
        return nil;

    _private = [[WebFramePrivate alloc] init];

    _private->bridge = (WebFrameBridge *)Mac(coreFrame)->bridge();

    if (fv) {
        [_private setWebFrameView:fv];
        [fv _setWebFrame:self];
    }

    ++WebFrameCount;

    [self _frameLoader]->setClient(new WebFrameLoaderClient(self));

    return self;
}

- (NSArray *)_documentViews
{
    NSMutableArray *result = [NSMutableArray array];
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        id docView = [[kit(frame) frameView] documentView];
        if (docView)
            [result addObject:docView];
    }
    return result;
}

- (void)_updateBackground
{
    BOOL drawsBackground = [[self webView] drawsBackground];
    NSColor *backgroundColor = [[self webView] backgroundColor];

    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        WebFrameBridge *bridge = (WebFrameBridge *)Mac(frame)->bridge();
        WebFrame *webFrame = [bridge webFrame];
        // Never call setDrawsBackground:YES here on the scroll view or the background color will
        // flash between pages loads. setDrawsBackground:YES will be called in _frameLoadCompleted.
        if (!drawsBackground)
            [[[webFrame frameView] _scrollView] setDrawsBackground:NO];
        [[[webFrame frameView] _scrollView] setBackgroundColor:backgroundColor];
        id documentView = [[webFrame frameView] documentView];
        if ([documentView respondsToSelector:@selector(setDrawsBackground:)])
            [documentView setDrawsBackground:drawsBackground];
        if ([documentView respondsToSelector:@selector(setBackgroundColor:)])
            [documentView setBackgroundColor:backgroundColor];
        [bridge setDrawsBackground:drawsBackground];
        [bridge setBaseBackgroundColor:backgroundColor];
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

#if !BUILDING_ON_TIGER
- (void)_unmarkAllBadGrammar
{
    // FIXME: Implement this across the bridge a la _unmarkAllMisspellings
}
#endif

- (void)_unmarkAllMisspellings
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        [Mac(frame)->bridge() unmarkAllMisspellings];
}

- (BOOL)_hasSelection
{
    id documentView = [[self frameView] documentView];    

    // optimization for common case to avoid creating potentially large selection string
    if ([documentView isKindOfClass:[WebHTMLView class]])
        if (Frame* coreFrame = core(self))
            return coreFrame->selectionController()->isRange();

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

#if !ASSERT_DISABLED

- (BOOL)_atMostOneFrameHasSelection
{
    // FIXME: 4186050 is one known case that makes this debug check fail.
    BOOL found = NO;
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        if ([kit(frame) _hasSelection]) {
            if (found)
                return NO;
            found = YES;
        }
    return YES;
}
#endif

- (WebFrame *)_findFrameWithSelection
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame))
        if ([kit(frame) _hasSelection])
            return kit(frame);
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

- (FrameLoader*)_frameLoader
{
    Frame* frame = core(self);
    return frame ? frame->loader() : 0;
}

static inline WebDataSource *dataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoaderMac*>(loader)->dataSource() : nil;
}

- (WebDataSource *)_dataSourceForDocumentLoader:(DocumentLoader*)loader
{
    return dataSource(loader);
}

- (void)_addDocumentLoader:(DocumentLoader*)loader toUnarchiveState:(WebArchive *)archive
{
    [dataSource(loader) _addToUnarchiveState:archive];
}

- (id <WebFormDelegate>)_formDelegate
{
    return [[self webView] _formDelegate];
}

@end

@implementation WebFrame (WebPrivate)

// FIXME: Yhis exists only as a convenience for Safari, consider moving there.
- (BOOL)_isDescendantOfFrame:(WebFrame *)ancestor
{
    Frame* coreFrame = core(self);
    return coreFrame && coreFrame->tree()->isDescendantOf(core(ancestor));
}

- (void)_setShouldCreateRenderers:(BOOL)frame
{
    [_private->bridge setShouldCreateRenderers:frame];
}

- (NSColor *)_bodyBackgroundColor
{
    return core(self)->bodyBackgroundColor();
}

- (BOOL)_isFrameSet
{
    return [_private->bridge isFrameSet];
}

- (BOOL)_firstLayoutDone
{
    return [self _frameLoader]->firstLayoutDone();
}

- (WebFrameLoadType)_loadType
{
    return (WebFrameLoadType)[self _frameLoader]->loadType();
}

@end

@implementation WebFrame

- (id)init
{
    return nil;
}

// Should be deprecated.
- (id)initWithName:(NSString *)name webFrameView:(WebFrameView *)view webView:(WebView *)webView
{
    return nil;
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
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    return coreFrame->tree()->name();
}

- (WebFrameView *)frameView
{
    return _private->webFrameView;
}

- (WebView *)webView
{
    Frame* coreFrame = core(self);
    return coreFrame ? [coreFrame->page()->bridge() webView] : nil;
}

- (DOMDocument *)DOMDocument
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    // FIXME: Why do we need this check?
    if (![[self dataSource] _isDocumentHTML])
        return nil;
    return kit(coreFrame->document());
}

- (DOMHTMLElement *)frameElement
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    Element* element = coreFrame->ownerElement();
    if (!element->isHTMLElement())
        return nil;
    return kit(static_cast<HTMLElement*>(element));
}

- (WebDataSource *)provisionalDataSource
{
    FrameLoader* frameLoader = [self _frameLoader];
    return frameLoader ? dataSource(frameLoader->provisionalDocumentLoader()) : nil;
}

- (WebDataSource *)dataSource
{
    FrameLoader* frameLoader = [self _frameLoader];
    return frameLoader ? dataSource(frameLoader->documentLoader()) : nil;
}

- (void)loadRequest:(NSURLRequest *)request
{
    [self _frameLoader]->load(request);
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
        RefPtr<DocumentLoader> documentLoader = [self _createDocumentLoaderWithRequest:request];
        [dataSource(documentLoader.get()) _addToUnarchiveState:archive];
        [self _frameLoader]->load(documentLoader.get());
    }
}

- (void)stopLoading
{
    [self _frameLoader]->stopLoading();
}

- (void)reload
{
    [self _frameLoader]->reload();
}

- (WebFrame *)findFrameNamed:(NSString *)name
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->tree()->find(name));
}

- (WebFrame *)parentFrame
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return nil;
    return [[kit(coreFrame->tree()->parent()) retain] autorelease];
}

- (NSArray *)childFrames
{
    Frame* coreFrame = core(self);
    if (!coreFrame)
        return [NSArray array];
    NSMutableArray *children = [NSMutableArray arrayWithCapacity:coreFrame->tree()->childCount()];
    for (Frame* child = coreFrame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        [children addObject:kit(child)];
    return children;
}

@end

@implementation WebFrame (WebFrameLoaderClient)

- (BOOL)_hasBackForwardList
{
    return [[self webView] backForwardList] != nil;
}

- (void)_resetBackForwardList
{
    // Note this doesn't verify the current load type as a b/f operation because it is called from
    // a subframe in the case of a delegate bailing out of the nav before it even gets to provisional state.
    WebFrame *mainFrame = [[self webView] mainFrame];
    WebHistoryItem *resetItem = mainFrame->_private->currentItem;
    if (resetItem)
        [[[self webView] backForwardList] goToItem:resetItem];
}

- (void)_invalidateCurrentItemPageCache
{
    // When we are pre-commit, the currentItem is where the pageCache data resides
    NSDictionary *pageCache = [_private->currentItem pageCache];

    [_private->bridge invalidatePageCache:pageCache];
    
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

- (BOOL)_privateBrowsingEnabled
{
    return [[[self webView] preferences] privateBrowsingEnabled];
}

- (void)_makeDocumentView
{
    NSView <WebDocumentView> *documentView = [_private->webFrameView _makeDocumentViewForDataSource:[self dataSource]];
    if (!documentView)
        return;

    // FIXME: We could save work and not do this for a top-level view that is not a WebHTMLView.
    WebFrameView *v = _private->webFrameView;
    [_private->bridge createFrameViewWithNSView:documentView marginWidth:[v _marginWidth] marginHeight:[v _marginHeight]];
    [self _updateBackground];
    [_private->bridge installInFrame:[v _scrollView]];

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:[self dataSource]];
}

- (void)_forceLayout
{
    NSView <WebDocumentView> *view = [[self frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)view setNeedsToApplyStyles:YES];
    [view setNeedsLayout:YES];
    [view layout];
}

- (void)_updateHistoryForCommit
{
    FrameLoadType type = [self _frameLoader]->loadType();
    if (isBackForwardLoadType(type) ||
        (type == FrameLoadTypeReload && [[self provisionalDataSource] unreachableURL] != nil)) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        [_private setPreviousItem:_private->currentItem];
        ASSERT(_private->provisionalItem);
        [_private setCurrentItem:_private->provisionalItem];
        [_private setProvisionalItem:nil];
    }
}

- (void)_updateHistoryForReload
{
    WebHistoryItem *currItem = _private->currentItem;
    LOG(PageCache, "Clearing back/forward cache, %@\n", [currItem URL]);
    [currItem setHasPageCache:NO];
    if ([self _frameLoader]->loadType() == FrameLoadTypeReload)
        [self _saveScrollPositionAndViewStateToItem:currItem];
    WebDataSource *dataSource = [self dataSource];
    NSURLRequest *request = [dataSource request];
    // Sometimes loading a page again leads to a different result because of cookies. Bugzilla 4072
    if ([request _webDataRequestUnreachableURL] == nil)
        [currItem setURL:[request URL]];
    // Update the last visited time. Mostly interesting for URL autocompletion statistics.
    NSURL *URL = [[[dataSource _documentLoader]->originalRequestCopy() URL] _webkit_canonicalize];
    WebHistory *sharedHistory = [WebHistory optionalSharedHistory];
    WebHistoryItem *oldItem = [sharedHistory itemForURL:URL];
    if (oldItem)
        [sharedHistory setLastVisitedTimeInterval:[NSDate timeIntervalSinceReferenceDate] forItem:oldItem];
}

- (void)_updateHistoryForStandardLoad
{
    WebDataSource *dataSource = [self dataSource];
    if (![dataSource _documentLoader]->isClientRedirect()) {
        NSURL *URL = [dataSource _URLForHistory];
        if (URL && ![URL _web_isEmpty]) {
            ASSERT([self webView]);
            if (![[[self webView] preferences] privateBrowsingEnabled]) {
                WebHistoryItem *entry = [[WebHistory optionalSharedHistory] addItemForURL:URL];
                if ([dataSource pageTitle])
                    [entry setTitle:[dataSource pageTitle]];                            
            }
            [self _addBackForwardItemClippedAtTarget:YES];
        }
    } else {
        NSURLRequest *request = [dataSource request];
        
        // Update the URL in the BF list that we made before the redirect, unless
        // this is alternate content for an unreachable URL (we want the BF list
        // item to remember the unreachable URL in case it becomes reachable later).
        if ([request _webDataRequestUnreachableURL] == nil) {
            [_private->currentItem setURL:[request URL]];

            // clear out the form data so we don't repost it to the wrong place if we
            // ever go back/forward to this item
            [_private->currentItem _setFormInfoFromRequest:request];

            // We must also clear out form data so we don't try to restore it into the incoming page,
            // see -_opened
        }
    }
}

- (void)_updateHistoryForBackForwardNavigation
{
    // Must grab the current scroll position before disturbing it
    [self _saveScrollPositionAndViewStateToItem:_private->previousItem];
}

- (void)_updateHistoryForInternalLoad
{
    // Add an item to the item tree for this frame
    ASSERT(![self _frameLoader]->documentLoader()->isClientRedirect());
    WebFrame *parentFrame = [self parentFrame];
    if (parentFrame) {
        WebHistoryItem *parentItem = parentFrame->_private->currentItem;
        // The only case where parentItem==nil should be when a parent frame loaded an
        // empty URL, which doesn't set up a current item in that parent.
        if (parentItem)
            [parentItem addChildItem:[self _createItem:YES]];
    } else {
        // See 3556159. It's not clear if it's valid to be in WebFrameLoadTypeOnLoadEvent
        // for a top-level frame, but that was a likely explanation for those crashes,
        // so let's guard against it.
        // ...and all WebFrameLoadTypeOnLoadEvent uses were folded to WebFrameLoadTypeInternal
        LOG_ERROR("no parent frame in transitionToCommitted:, WebFrameLoadTypeInternal");
    }
}

- (LoadErrorResetToken *)_tokenForLoadErrorReset
{
    return (LoadErrorResetToken*)[[self _currentBackForwardListItemToResetTo] retain];
}

- (void)_resetAfterLoadError:(LoadErrorResetToken *)token
{
    WebHistoryItem *item = (WebHistoryItem *)token;
    if (item)
        [[[self webView] backForwardList] goToItem:item];
    [item release];
}

- (void)_doNotResetAfterLoadError:(LoadErrorResetToken *)token
{
    WebHistoryItem *item = (WebHistoryItem *)token;
    [item release];
}

- (void)_dispatchDidHandleOnloadEventsForFrame
{
    WebView *webView = [self webView];
    [[webView _frameLoadDelegateForwarder] webView:webView didHandleOnloadEventsForFrame:self];
}

- (void)_dispatchDidReceiveServerRedirectForProvisionalLoadForFrame
{
    WebView *webView = [self webView];
    [[webView _frameLoadDelegateForwarder] webView:webView
       didReceiveServerRedirectForProvisionalLoadForFrame:self];
}

- (id)_dispatchIdentifierForInitialRequest:(NSURLRequest *)clientRequest fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];
    id resourceLoadDelegate = [webView resourceLoadDelegate];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsIdentifierForRequest)
        return [resourceLoadDelegate webView:webView identifierForInitialRequest:clientRequest fromDataSource:dataSource(loader)];

    return [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView identifierForInitialRequest:clientRequest fromDataSource:dataSource(loader)];
}

- (NSURLRequest *)_dispatchResource:(id)identifier willSendRequest:(NSURLRequest *)clientRequest redirectResponse:(NSURLResponse *)redirectResponse fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];
    id resourceLoadDelegate = [webView resourceLoadDelegate];

    if ([webView _resourceLoadDelegateImplementations].delegateImplementsWillSendRequest)
        return [resourceLoadDelegate webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:dataSource(loader)];
    else
        return [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:dataSource(loader)];
}

- (void)_dispatchDidReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];
    id resourceLoadDelegate = [webView resourceLoadDelegate];

    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveAuthenticationChallenge)
        [resourceLoadDelegate webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource(loader)];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource(loader)];
}

- (void)_dispatchDidCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];
    id resourceLoadDelegate = [webView resourceLoadDelegate];

    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidCancelAuthenticationChallenge)
        [resourceLoadDelegate webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource(loader)];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource(loader)];
}

- (void)_dispatchResource:(id)identifier didReceiveResponse:(NSURLResponse *)r fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];

    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveResponse)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:dataSource(loader)];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:dataSource(loader)];
}

- (void)_dispatchResource:(id)identifier didReceiveContentLength:(int)lengthReceived fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];

    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveContentLength)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:(WebNSUInteger)lengthReceived fromDataSource:dataSource(loader)];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:(WebNSUInteger)lengthReceived fromDataSource:dataSource(loader)];
}

- (void)_dispatchResource:(id)identifier didFinishLoadingFromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidFinishLoadingFromDataSource)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:dataSource(loader)];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:dataSource(loader)];
}


- (void)_dispatchResource:(id)identifier didFailLoadingWithError:error fromDocumentLoader:(DocumentLoader*)loader
{
    WebView *webView = [self webView];
    [[webView _resourceLoadDelegateForwarder] webView:webView resource:identifier didFailLoadingWithError:error fromDataSource:dataSource(loader)];
}

- (void)_dispatchDidCancelClientRedirectForFrame
{
    WebView *webView = [self webView];
    [[webView _frameLoadDelegateForwarder] webView:webView didCancelClientRedirectForFrame:self];
}

- (void)_dispatchWillPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date
{
    WebView *webView = [self webView];   
    [[webView _frameLoadDelegateForwarder] webView:webView
                         willPerformClientRedirectToURL:URL
                                                  delay:seconds
                                               fireDate:date
                                               forFrame:self];
}

- (void)_dispatchDidChangeLocationWithinPageForFrame
{
    WebView *webView = [self webView];   
    [[webView _frameLoadDelegateForwarder] webView:webView didChangeLocationWithinPageForFrame:self];
}

- (void)_dispatchWillCloseFrame
{
    WebView *webView = [self webView];   
    [[webView _frameLoadDelegateForwarder] webView:webView willCloseFrame:self];
}

- (void)_dispatchDidReceiveIcon:(NSImage *)icon
{
    WebView *webView = [self webView];   
    ASSERT([self _isMainFrame]);
    [webView _willChangeValueForKey:_WebMainFrameIconKey];
    [[webView _frameLoadDelegateForwarder] webView:webView didReceiveIcon:icon forFrame:self];
    [webView _didChangeValueForKey:_WebMainFrameIconKey];
}

- (void)_dispatchDidStartProvisionalLoadForFrame
{
    WebView *webView = [self webView];   
    [webView _didStartProvisionalLoadForFrame:self];
    [[webView _frameLoadDelegateForwarder] webView:webView didStartProvisionalLoadForFrame:self];    
}

- (void)_dispatchDidReceiveTitle:(NSString *)title
{
    WebView *webView = [self webView];   
    [[webView _frameLoadDelegateForwarder] webView:webView didReceiveTitle:title forFrame:self];
}

- (void)_dispatchDidCommitLoadForFrame
{
    // Tell the client we've committed this URL.
    ASSERT([[self frameView] documentView] != nil);

    WebView *webView = [self webView];   
    [webView _didCommitLoadForFrame:self];
    [[webView _frameLoadDelegateForwarder] webView:webView didCommitLoadForFrame:self];
}

- (void)_dispatchDidFailProvisionalLoadWithError:(NSError *)error
{
    WebView *webView = [self webView];   
    [webView _didFailProvisionalLoadWithError:error forFrame:self];
    [[webView _frameLoadDelegateForwarder] webView:webView didFailProvisionalLoadWithError:error forFrame:self];
    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:error];
}

- (void)_dispatchDidFailLoadWithError:(NSError *)error
{
    WebView *webView = [self webView];   
    [webView _didFailLoadWithError:error forFrame:self];
    [[webView _frameLoadDelegateForwarder] webView:webView didFailLoadWithError:error forFrame:self];
    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:error];
}

- (void)_dispatchDidFinishLoadForFrame
{
    WebView *webView = [self webView];   
    [webView _didFinishLoadForFrame:self];
    [[webView _frameLoadDelegateForwarder] webView:webView didFinishLoadForFrame:self];
    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];
}

- (void)_dispatchDidFirstLayoutInFrame
{
    WebView *webView = [self webView];
    [[webView _frameLoadDelegateForwarder] webView:webView didFirstLayoutInFrame:self];
}

- (Frame*)_dispatchCreateWebViewWithRequest:(NSURLRequest *)request
{
    WebView *currentWebView = [self webView];
    id wd = [currentWebView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        return core([[wd webView:currentWebView createWebViewWithRequest:request] mainFrame]);
    return core([[[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:request] mainFrame]);
}

- (void)_dispatchShow
{
    WebView *webView = [self webView];
    [[webView _UIDelegateForwarder] webViewShow:webView];
}

- (WebPolicyDecider *)_createPolicyDeciderWithTarget:(id)target action:(SEL)action
{
    return [[WebPolicyDeciderMac alloc] initWithTarget:target action:action];
}

static inline WebPolicyDecisionListener *decisionListener(WebPolicyDecider *decider)
{
    return [(WebPolicyDeciderMac *)decider decisionListener];
}

- (void)_dispatchDecidePolicyForMIMEType:(NSString *)MIMEType request:(NSURLRequest *)request decider:(WebPolicyDecider *)decider
{
    WebView *webView = [self webView];

    [[webView _policyDelegateForwarder] webView:webView decidePolicyForMIMEType:MIMEType request:request frame:self decisionListener:decisionListener(decider)];
}

- (void)_dispatchDecidePolicyForNewWindowAction:(NSDictionary *)action request:(NSURLRequest *)request newFrameName:(NSString *)frameName decider:(WebPolicyDecider *)decider
{
    WebView *webView = [self webView];
    [[webView _policyDelegateForwarder] webView:webView
            decidePolicyForNewWindowAction:action
                                   request:request
                              newFrameName:frameName
                          decisionListener:decisionListener(decider)];
}

- (void)_dispatchDecidePolicyForNavigationAction:(NSDictionary *)action request:(NSURLRequest *)request decider:(WebPolicyDecider *)decider
{
    WebView *webView = [self webView];
    [[webView _policyDelegateForwarder] webView:webView
                decidePolicyForNavigationAction:action
                                        request:request
                                          frame:self
                               decisionListener:decisionListener(decider)];
}

- (void)_dispatchUnableToImplementPolicyWithError:(NSError *)error
{
    WebView *webView = [self webView];
    [[webView _policyDelegateForwarder] webView:webView unableToImplementPolicyWithError:error frame:self];    
}

- (void)_dispatchSourceFrame:(Frame*)sourceFrame willSubmitForm:(Element*)form withValues:(NSDictionary *)values submissionDecider:(WebPolicyDecider *)decider
{
    [[self _formDelegate] frame:self sourceFrame:kit(sourceFrame) willSubmitForm:kit(form) withValues:values submissionListener:decisionListener(decider)];
}

- (void)_detachedFromParent1
{
    [self _saveScrollPositionAndViewStateToItem:_private->currentItem];
}

- (void)_detachedFromParent2
{
    [_private->inspectors makeObjectsPerformSelector:@selector(_webFrameDetached:) withObject:self];
    [_private->webFrameView _setWebFrame:nil]; // needed for now to be compatible w/ old behavior
}

- (void)_detachedFromParent3
{
    [_private setWebFrameView:nil];
}

- (void)_detachedFromParent4
{
    _private->bridge = nil;
}

- (void)_updateHistoryAfterClientRedirect
{
    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // khtml has closed the URL and saved away the form state.
    WebHistoryItem *item = _private->currentItem;
    [item setDocumentState:nil];
    [item setScrollPoint:NSZeroPoint];
}

- (void)_loadedFromPageCache
{
    // Release the resources kept in the page cache.
    // They will be reset when we leave this page.
    // The WebCore side of the page cache will have already been invalidated by
    // the bridge to prevent premature release.
    [_private->currentItem setHasPageCache:NO];
}

- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)response proxy:(id)proxy
{
    [WebDownload _downloadWithLoadingConnection:connection
                                        request:request
                                       response:response
                                       delegate:[[self webView] downloadDelegate]
                                          proxy:proxy];
}

- (void)_setDocumentViewFromPageCache:(NSDictionary *)pageCache
{
    NSView <WebDocumentView> *cachedView = [pageCache objectForKey:WebPageCacheDocumentViewKey];
    ASSERT(cachedView != nil);
    [[self frameView] _setDocumentView:cachedView];
}

- (void)_setCopiesOnScroll
{
    [[[[self frameView] _scrollView] contentView] setCopiesOnScroll:YES];
}

- (void)_dispatchDidLoadMainResourceForDocumentLoader:(DocumentLoader*)loader
{
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:[self webView]
            didLoadMainResourceForDataSource:dataSource(loader)];
}

- (void)_forceLayoutForNonHTML
{
    WebFrameView *thisView = [self frameView];
    NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
    ASSERT(thisDocumentView != nil);
    
    // Tell the just loaded document to layout.  This may be necessary
    // for non-html content that needs a layout message.
    if (!([[self dataSource] _isDocumentHTML])) {
        [thisDocumentView setNeedsLayout:YES];
        [thisDocumentView layout];
        [thisDocumentView setNeedsDisplay:YES];
    }
}

- (void)_clearLoadingFromPageCacheForDocumentLoader:(DocumentLoader*)loader
{
    [dataSource(loader) _setLoadingFromPageCache:NO];
}

- (BOOL)_isDocumentLoaderLoadingFromPageCache:(DocumentLoader*)loader
{
    return [dataSource(loader) _loadingFromPageCache];
}

- (void)_makeRepresentationForDocumentLoader:(DocumentLoader*)loader
{
    [dataSource(loader) _makeRepresentation];
}

- (void)_revertToProvisionalStateForDocumentLoader:(DocumentLoader*)loader
{
    [dataSource(loader) _revertToProvisionalState];
}

- (void)_setMainDocumentError:(NSError *)error forDocumentLoader:(DocumentLoader*)loader
{
    [dataSource(loader) _setMainDocumentError:error];
}

- (void)_clearUnarchivingStateForLoader:(DocumentLoader*)loader
{
    [dataSource(loader) _clearUnarchivingState];
}

- (void)_progressStarted
{
    [[self webView] _progressStarted:self];
}

- (void)_progressCompleted
{
    [[self webView] _progressCompleted:self];
}

- (void)_incrementProgressForIdentifier:(id)identifier response:(NSURLResponse *)response
{
    [[self webView] _incrementProgressForIdentifier:identifier response:response];
}

- (void)_incrementProgressForIdentifier:(id)identifier data:(NSData *)data
{
    [[self webView] _incrementProgressForIdentifier:identifier data:data];
}

- (void)_completeProgressForIdentifier:(id)identifier
{
    [[self webView] _completeProgressForIdentifier:identifier];
}

- (void)_setMainFrameDocumentReady:(BOOL)ready
{
    [[self webView] setMainFrameDocumentReady:ready];
}

- (void)_willChangeTitleForDocument:(DocumentLoader*)loader
{
    // FIXME: Should do this only in main frame case, right?
    [[self webView] _willChangeValueForKey:_WebMainFrameTitleKey];
}

- (void)_didChangeTitleForDocument:(DocumentLoader*)loader
{
    // FIXME: Should do this only in main frame case, right?
    [[self webView] _didChangeValueForKey:_WebMainFrameTitleKey];
}

- (void)_startDownloadWithRequest:(NSURLRequest *)request
{
    // FIXME: Should download full request.
    [[self webView] _downloadURL:[request URL]];
}

- (void)_finishedLoadingDocument:(DocumentLoader*)loader
{
    [dataSource(loader) _finishedLoading];
}

- (void)_committedLoadWithDocumentLoader:(DocumentLoader*)loader data:(NSData *)data
{
    [dataSource(loader) _receivedData:data];
}

- (void)_documentLoader:(DocumentLoader*)loader setMainDocumentError:(NSError *)error
{
    [dataSource(loader) _setMainDocumentError:error];
}

- (void)_finalSetupForReplaceWithDocumentLoader:(DocumentLoader*)loader
{
    [dataSource(loader) _clearUnarchivingState];
}

- (NSError *)_cancelledErrorWithRequest:(NSURLRequest *)request
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:[request URL]];
}

- (NSError *)_cannotShowURLErrorWithRequest:(NSURLRequest *)request
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotShowURL URL:[request URL]];
}

- (NSError *)_interruptForPolicyChangeErrorWithRequest:(NSURLRequest *)request
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:[request URL]];
}

- (NSError *)_cannotShowMIMETypeErrorWithResponse:(NSURLResponse *)response
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:WebKitErrorCannotShowMIMEType URL:[response URL]];    
}

- (NSError *)_fileDoesNotExistErrorWithResponse:(NSURLResponse *)response
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist URL:[response URL]];    
}

- (BOOL)_shouldFallBackForError:(NSError *)error
{
    // FIXME: Needs to check domain.
    // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent
    // loading plugin content twice.  See <rdar://problem/4258008>
    return [error code] != NSURLErrorCancelled && [error code] != WebKitErrorPlugInWillHandleLoad;
}

- (BOOL)_hasWebView
{
    return [self webView] != nil;
}

- (BOOL)_hasFrameView
{
    return [self frameView] != nil;
}

- (NSURL *)_mainFrameURL
{
    return [[[[self webView] mainFrame] dataSource] _URL];
}

// The following 2 methods are copied from [NSHTTPURLProtocol _cachedResponsePassesValidityChecks] and modified for our needs.
// FIXME: It would be nice to eventually to share this code somehow.
- (BOOL)_canUseResourceForRequest:(NSURLRequest *)request
{
    NSURLRequestCachePolicy policy = [request cachePolicy];
    if (policy == NSURLRequestReturnCacheDataElseLoad)
        return YES;
    if (policy == NSURLRequestReturnCacheDataDontLoad)
        return YES;
    if (policy == NSURLRequestReloadIgnoringCacheData)
        return NO;
    if ([request valueForHTTPHeaderField:@"must-revalidate"] != nil)
        return NO;
    if ([request valueForHTTPHeaderField:@"proxy-revalidate"] != nil)
        return NO;
    if ([request valueForHTTPHeaderField:@"If-Modified-Since"] != nil)
        return NO;
    if ([request valueForHTTPHeaderField:@"Cache-Control"] != nil)
        return NO;
    if ([@"POST" _webkit_isCaseInsensitiveEqualToString:[request HTTPMethod]])
        return NO;
    return YES;
}

- (BOOL)_canUseResourceWithResponse:(NSURLResponse *)response
{
    if (WKGetNSURLResponseMustRevalidate(response))
        return NO;
    if (WKGetNSURLResponseCalculatedExpiration(response) - CFAbsoluteTimeGetCurrent() < 1)
        return NO;
    return YES;
}

- (void)_deliverArchivedResourcesAfterDelay
{
    if (!_private->pendingArchivedResources)
        return;
    if (_private->pendingArchivedResources->isEmpty())
        return;
    if ([self _frameLoader]->defersCallbacks())
        return;
    
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_deliverArchivedResources) object:nil];
    [self performSelector:@selector(_deliverArchivedResources) withObject:nil afterDelay:0];
}

- (BOOL)_willUseArchiveForRequest:(NSURLRequest *)r originalURL:(NSURL *)originalURL loader:(WebResourceLoader*)loader
{
    if (![[r URL] isEqual:originalURL])
        return NO;
    if (![self _canUseResourceForRequest:r])
        return NO;
    WebResource *resource = [dataSource([self _frameLoader]->activeDocumentLoader()) _archivedSubresourceForURL:originalURL];
    if (!resource)
        return NO;
    if (![self _canUseResourceWithResponse:[resource _response]])
        return NO;
    if (!_private->pendingArchivedResources)
        _private->pendingArchivedResources = new ResourceMap;
    _private->pendingArchivedResources->set(loader, resource);
    // Deliver the resource after a delay because callers don't expect to receive callbacks while calling this method.
    [self _deliverArchivedResourcesAfterDelay];
    return YES;
}

- (BOOL)_archiveLoadPendingForLoader:(WebResourceLoader*)loader
{
    if (!_private->pendingArchivedResources)
        return false;
    return _private->pendingArchivedResources->contains(loader);
}

- (void)_cancelPendingArchiveLoadForLoader:(WebResourceLoader*)loader
{
    if (!_private->pendingArchivedResources)
        return;
    if (_private->pendingArchivedResources->isEmpty())
        return;
    _private->pendingArchivedResources->remove(loader);
    if (_private->pendingArchivedResources->isEmpty())
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_deliverArchivedResources) object:nil];
}

- (void)_clearArchivedResources
{
    if (_private->pendingArchivedResources)
        _private->pendingArchivedResources->clear();
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_deliverArchivedResources) object:nil];
}

- (void)_deliverArchivedResources
{
    if (!_private->pendingArchivedResources)
        return;
    if (_private->pendingArchivedResources->isEmpty())
        return;
    if ([self _frameLoader]->defersCallbacks())
        return;

    const ResourceMap copy = *_private->pendingArchivedResources;
    _private->pendingArchivedResources->clear();

    ResourceMap::const_iterator end = copy.end();
    for (ResourceMap::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<WebResourceLoader> loader = it->first;
        WebResource *resource = it->second.get();
        NSData *data = [[resource data] retain];
        loader->didReceiveResponse([resource _response]);
        loader->didReceiveData(data, [data length], true);
        [data release];
        loader->didFinishLoading();
    }
}

- (void)_setDefersCallbacks:(BOOL)defers
{
    if (!defers)
        [self _deliverArchivedResourcesAfterDelay];
}

- (BOOL)_canHandleRequest:(NSURLRequest *)request
{
    return [WebView _canHandleRequest:request];
}

- (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    return [WebView canShowMIMEType:MIMEType];
}

- (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme
{
    return [WebView _representationExistsForURLScheme:URLScheme];
}

- (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme
{
    return [WebView _generatedMIMETypeForURLScheme:URLScheme];
}

- (NSDictionary *)_elementForEvent:(NSEvent *)event
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp:
            break;
        default:
            return nil;
    }

    NSView *topViewInEventWindow = [[event window] contentView];
    NSView *viewContainingPoint = [topViewInEventWindow hitTest:
        [topViewInEventWindow convertPoint:[event locationInWindow] fromView:nil]];
    while (viewContainingPoint) {
        if ([viewContainingPoint isKindOfClass:[WebView class]])
            return [(WebView *)viewContainingPoint elementAtPoint:
                [viewContainingPoint convertPoint:[event locationInWindow] fromView:nil]];
        viewContainingPoint = [viewContainingPoint superview];
    }
    return nil;
}

- (void)_frameLoadCompleted
{
    // Note: Can be called multiple times.
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to clear these out.
    NSScrollView *sv = [[self frameView] _scrollView];
    if ([[self webView] drawsBackground])
        [sv setDrawsBackground:YES];
    [_private setPreviousItem:nil];
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

- (void)_setTitle:(NSString *)title forURL:(NSURL *)URL
{
    [[[WebHistory optionalSharedHistory] itemForURL:URL] setTitle:title];
    [_private->currentItem setTitle:title];
}

- (PassRefPtr<DocumentLoader>)_createDocumentLoaderWithRequest:(NSURLRequest *)request
{
    RefPtr<WebDocumentLoaderMac> loader = new WebDocumentLoaderMac(request);

    WebDataSource *dataSource = [[WebDataSource alloc] _initWithDocumentLoader:loader.get()];
    loader->setDataSource(dataSource);
    [dataSource release];

    return loader.release();
}

- (void)_prepareForDataSourceReplacement
{
    if (![self dataSource]) {
        ASSERT(!core(self)->tree()->childCount());
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
    if ([firstResp isKindOfClass:[NSView class]] && [(NSView *)firstResp isDescendantOf:frameView])
        [window endEditingFor:firstResp];
    
    [self _frameLoader]->detachChildren();
}

- (void)_didFinishLoad
{
    [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];    
}

- (void)_addHistoryItemForFragmentScroll
{
    [self _addBackForwardItemClippedAtTarget:NO];
}

- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL
{
    WebHistoryItem *item = _private->currentItem;
    NSString* URLString = [URL _web_originalDataAsString];
    return [URLString isEqual:[item URLString]] || [URLString isEqual:[item originalURLString]];
}    

- (void)_provisionalLoadStarted
{
    FrameLoadType loadType = [self _frameLoader]->loadType();
    
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
        && item
        && ![self _frameLoader]->isQuickRedirectComing()
        && loadType != FrameLoadTypeReload 
        && loadType != FrameLoadTypeReloadAllowingStaleData
        && loadType != FrameLoadTypeSame
        && ![[self dataSource] isLoading]
        && ![self _frameLoader]->documentLoader()->isStopping()) {
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
            core(self)->clear();
    } else
        LOG(PageCache, "NOT saving page to back/forward cache, %@\n", [[self dataSource] _URL]);
}

@end
