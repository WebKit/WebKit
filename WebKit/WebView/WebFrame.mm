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

#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMDocumentInternal.h"
#import "DOMElementInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebBackForwardList.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebFrameBridge.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameLoaderClient.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryItem.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebKitLogging.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebPolicyDelegatePrivate.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebNullPluginView.h"
#import "WebPlugin.h"
#import "WebPluginController.h"
#import "WebPolicyDelegatePrivate.h"
#import "WebPreferencesPrivate.h"
#import "WebScriptDebugDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/Chrome.h>
#import <WebCore/Document.h>
#import <WebCore/Event.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameMac.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/Page.h>
#import <WebCore/SelectionController.h>
#import <WebCore/WebDataProtocol.h>
#import <WebCore/FormState.h>
#import <WebKit/DOMDocument.h>
#import <WebKit/DOMElement.h>
#import <WebKit/DOMHTMLElement.h>
#import <WebKit/DOMNode.h>
#import <WebKit/DOMRange.h>

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

NSString *WebPageCacheEntryDateKey = @"WebPageCacheEntryDateKey";
NSString *WebPageCacheDataSourceKey = @"WebPageCacheDataSourceKey";
NSString *WebPageCacheDocumentViewKey = @"WebPageCacheDocumentViewKey";

@interface WebFrame (ForwardDecls)
- (void)_loadHTMLString:(NSString *)string baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;
- (WebHistoryItem *)_createItem:(BOOL)useOriginal;
- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;
- (WebHistoryItem *)_currentBackForwardListItemToResetTo;
@end

@interface NSView (WebFramePluginHosting)
- (void)setWebFrame:(WebFrame *)webFrame;
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

    [super dealloc];
}

- (void)setWebFrameView:(WebFrameView *)v 
{ 
    [v retain];
    [webFrameView release];
    webFrameView = v;
}

@end

CSSStyleDeclaration* core(DOMCSSStyleDeclaration *declaration)
{
    return [declaration _CSSStyleDeclaration];
}

DOMCSSStyleDeclaration *kit(WebCore::CSSStyleDeclaration* declaration)
{
    return [DOMCSSStyleDeclaration _CSSStyleDeclarationWith:declaration];
}

Element* core(DOMElement *element)
{
    return [element _element];
}

DOMElement *kit(Element* element)
{
    return [DOMElement _elementWith:element];
}

Node* core(DOMNode *node)
{
    return [node _node];
}

DOMNode *kit(Node* node)
{
    return [DOMNode _nodeWith:node];
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


static inline WebFrame *frame(WebCoreFrameBridge *bridge)
{
    return ((WebFrameBridge *)bridge)->_frame;
}

FrameMac* core(WebFrame *frame)
{
    if (!frame)
        return 0;
    
    if (!frame->_private->bridge)
        return 0;

    return frame->_private->bridge->m_frame;
}

WebFrame *kit(Frame* frame)
{
    return frame ? ((WebFrameBridge *)Mac(frame)->bridge())->_frame : nil;
}

Page* core(WebView *webView)
{
    return [webView page];
}

WebView *kit(Page* page)
{
    WebChromeClient* chromeClient = static_cast<WebChromeClient*>(page->chrome()->client());
    return chromeClient->webView();
}

WebView *getWebView(WebFrame *webFrame)
{
    Frame* coreFrame = core(webFrame);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->page());
}

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
        WebHistoryItem *bfItem = [[getWebView(self) mainFrame] _createItemTreeWithTargetFrame:self clippedAtTarget:doClip];
        LOG (BackForward, "for frame %@, adding item  %@\n", [self name], bfItem);
        [[getWebView(self) backForwardList] addItem:bfItem];
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
    WebCoreDocumentLoader *documentLoader = [dataSrc _documentLoader];

    if (useOriginal)
        request = documentLoader ? documentLoader->originalRequestCopy() : nil;
    else
        request = [dataSrc request];

    if (unreachableURL != nil) {
        URL = unreachableURL;
        originalURL = unreachableURL;
    } else {
        URL = [request URL];
        originalURL = documentLoader ? [documentLoader->originalRequestCopy() URL] : nil;
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
    [self _setPreviousItem:_private->currentItem];
    [self _setCurrentItem:bfItem];

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
    return [[getWebView(self) backForwardList] _usesPageCache] && core(self)->loader()->canCachePage();
}

- (void)_purgePageCache
{
    // This method implements the rule for purging the page cache.
    unsigned sizeLimit = [[getWebView(self) backForwardList] pageCacheSize];
    unsigned pagesCached = 0;
    WebBackForwardList *backForwardList = [getWebView(self) backForwardList];
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
    return FrameLoader::timeOfLastCompletedLoad() - kCFAbsoluteTimeIntervalSince1970;
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
    NSData *formData = [item formData];

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
        core(self)->loader()->scrollToAnchor([item URL]);
    
        // must do this maintenance here, since we don't go through a real page reload
        [self _setCurrentItem:item];
        core(self)->loader()->client()->restoreScrollPositionAndViewState();

        // Fake the URL change by updating the data source's request.  This will no longer
        // be necessary if we do the better fix described above.
        [self _frameLoader]->documentLoader()->replaceRequestURLForAnchorScroll(itemURL);
        
        [[getWebView(self) _frameLoadDelegateForwarder] webView:getWebView(self)
                               didChangeLocationWithinPageForFrame:self];
        [_private->internalLoadDelegate webFrame:self didFinishLoadWithError:nil];
    } else {
        // Remember this item so we can traverse any child items as child frames load
        [self _setProvisionalItem:item];

        WebDataSource *newDataSource;
        BOOL inPageCache = NO;
        
        // Check if we'll be using the page cache.  We only use the page cache
        // if one exists and it is less than _backForwardCacheExpirationInterval
        // seconds old.  If the cache is expired it gets flushed here.
        if ([item hasPageCache]){
            NSDictionary *pageCache = [item pageCache];
            NSDate *cacheDate = [pageCache objectForKey: WebPageCacheEntryDateKey];
            NSTimeInterval delta = [[NSDate date] timeIntervalSinceDate: cacheDate];
            if (delta <= [[getWebView(self) preferences] _backForwardCacheExpirationInterval]) {
                newDataSource = [pageCache objectForKey: WebPageCacheDataSourceKey];
                [self _frameLoader]->load([newDataSource _documentLoader], loadType, 0);   
                inPageCache = YES;
            } else {
                LOG (PageCache, "Not restoring page from back/forward cache because cache entry has expired, %@ (%3.5f > %3.5f seconds)\n", [_private->provisionalItem URL], delta, [[getWebView(self) preferences] _backForwardCacheExpirationInterval]);
                [item setHasPageCache: NO];
            }
        }
        
        if (!inPageCache) {
            NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:itemURL];
            [self _frameLoader]->addExtraFieldsToRequest(request, true, (formData != nil));

            // If this was a repost that failed the page cache, we might try to repost the form.
            NavigationAction action;
            if (formData) {
                [request setHTTPMethod:@"POST"];
                [request _web_setHTTPReferrer:[item formReferrer]];
                [request setHTTPBody:formData];
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
                    action = NavigationAction(itemURL, NavigationTypeFormResubmitted);
                } else
                    // We can use the cache, don't use navType=resubmit
                    action = NavigationAction(itemURL, loadType, false);
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

                action = NavigationAction(itemOriginalURL, loadType, false);
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
        
        [self _setCurrentItem:item];

        // Restore form state (works from currentItem)
        [_private->bridge restoreDocumentState];
        // Restore the scroll position (taken in favor of going back to the anchor)
        core(self)->loader()->client()->restoreScrollPositionAndViewState();
        
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
    if ([[getWebView(self) _policyDelegateForwarder] webView:getWebView(self) shouldGoToHistoryItem:item]) {    
        WebBackForwardList *backForwardList = [getWebView(self) backForwardList];
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
                [childFrame _setProvisionalItem:childItem];
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                [childFrame _setCurrentItem:childItem];
        }
    }

    WebArchive *archive = [[self dataSource] _popSubframeArchiveWithName:[childFrame name]];
    if (archive)
        [childFrame loadArchive:archive];
    else
        [childFrame _frameLoader]->load(URL, referrer, childLoadType,
            String(), nil, 0, HashMap<String, String>());
}

- (void)_saveScrollPositionAndViewStateToItem:(WebHistoryItem *)item
{
    if (item) {
        NSView <WebDocumentView> *docView = [_private->webFrameView documentView];
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
    return core(self)->loader()->numPendingOrLoadingRequests(recurse);
}

- (void)_reloadForPluginChanges
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        NSView <WebDocumentView> *documentView = [[kit(frame) frameView] documentView];
        if (([documentView isKindOfClass:[WebHTMLView class]] && coreFrame->loader()->containsPlugins()))
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
    BOOL drawsBackground = [getWebView(self) drawsBackground];
    NSColor *backgroundColor = [getWebView(self) backgroundColor];

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
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->removeMarkers(DocumentMarker::Grammar);
    }
}
#endif

- (void)_unmarkAllMisspellings
{
    Frame* coreFrame = core(self);
    for (Frame* frame = coreFrame; frame; frame = frame->tree()->traverseNext(coreFrame)) {
        Document *doc = frame->document();
        if (!doc)
            return;

        doc->removeMarkers(DocumentMarker::Spelling);
    }
}

- (BOOL)_hasSelection
{
    id documentView = [_private->webFrameView documentView];    

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
    id documentView = [_private->webFrameView documentView];    
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
    WebFrame *frameWithSelection = [[getWebView(self) mainFrame] _findFrameWithSelection];
    if (frameWithSelection != self)
        [frameWithSelection _clearSelection];

    // While we're in the general area of selection and frames, check that there is only one now.
    ASSERT([[getWebView(self) mainFrame] _atMostOneFrameHasSelection]);
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

- (BOOL)_isMainFrame
{
   Frame* coreFrame = core(self);
   if (!coreFrame)
       return NO;
   return coreFrame == coreFrame->page()->mainFrame() ;
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

- (void)_setProvisionalItem:(WebHistoryItem *)item
{
    [item retain];
    [_private->provisionalItem release];
    _private->provisionalItem = item;
}

- (void)_setPreviousItem:(WebHistoryItem *)item
{
    [item retain];
    [_private->previousItem release];
    _private->previousItem = item;
}

- (void)_setCurrentItem:(WebHistoryItem *)item
{
    [item retain];
    [_private->currentItem release];
    _private->currentItem = item;
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
    Document* document = core(self)->document();
    if (!document)
        return nil;
    HTMLElement* body = document->body();
    if (!body)
        return nil;
    RenderObject* bodyRenderer = body->renderer();
    if (!bodyRenderer)
        return nil;
    Color color = bodyRenderer->style()->backgroundColor();
    if (!color.isValid())
        return nil;
    return nsColor(color);
}

- (BOOL)_isFrameSet
{
    return core(self)->isFrameSet();
}

- (BOOL)_firstLayoutDone
{
    return [self _frameLoader]->firstLayoutDone();
}

- (WebFrameLoadType)_loadType
{
    return (WebFrameLoadType)[self _frameLoader]->loadType();
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
    return getWebView(self);
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
    if (!element || !element->isHTMLElement())
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
        RefPtr<DocumentLoader> documentLoader = core(self)->loader()->client()->createDocumentLoader(request);
        [dataSource(documentLoader.get()) _addToUnarchiveState:archive];
        [self _frameLoader]->load(documentLoader.get());
    }
}

- (void)stopLoading
{
    [self _frameLoader]->stopAllLoaders();
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
