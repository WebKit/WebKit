/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebFrameLoaderClient.h"

// Terrible hack; lets us get at the WebFrame private structure.
#define private public
#import "WebFrame.h"
#undef private

#import "WebBackForwardList.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDocumentInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebDownloadInternal.h"
#import "WebElementDictionary.h"
#import "WebFormDelegate.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentation.h"
#import "WebHTMLView.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebHistoryPrivate.h"
#import "WebIconDatabaseInternal.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitNSStringExtras.h"
#import "WebNSURLExtras.h"
#import "WebPanelAuthenticationHandler.h"
#import "WebPolicyDelegate.h"
#import "WebPreferences.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourcePrivate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebUIDelegate.h"
#import "WebViewInternal.h"
#import <WebCore/Chrome.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FormState.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/FrameMac.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/IconDatabase.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/Page.h>
#import <WebCore/PageState.h>
#import <WebCore/PlatformString.h>
#import <WebCore/ResourceLoader.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/WebCoreFrameBridge.h>
#import <WebCore/WebCorePageState.h>
#import <WebCore/WebDataProtocol.h>
#import <WebKit/DOMElement.h>
#import <WebKitSystemInterface.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

@interface WebFramePolicyListener : NSObject <WebPolicyDecisionListener, WebFormSubmissionListener>
{
    Frame* m_frame;
}
- (id)initWithWebCoreFrame:(Frame*)frame;
- (void)invalidate;
@end

static inline WebView *getWebView(DocumentLoader* loader)
{
    return kit(loader->frameLoader()->frame()->page());
}

static inline WebDataSource *dataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoaderMac*>(loader)->dataSource() : nil;
}

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame *webFrame)
    : m_webFrame(webFrame)
    , m_policyFunction(0)
    , m_archivedResourcesDeliveryTimer(this, &WebFrameLoaderClient::deliverArchivedResources)
{
}

void WebFrameLoaderClient::frameLoaderDestroyed()
{
    delete this;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return [m_webFrame.get() webView] != nil;
}

bool WebFrameLoaderClient::hasFrameView() const
{
    return m_webFrame->_private->webFrameView != nil;
}

bool WebFrameLoaderClient::hasBackForwardList() const
{
    return [getWebView(m_webFrame.get()) backForwardList] != nil;
}

void WebFrameLoaderClient::resetBackForwardList()
{
    // Note this doesn't verify the current load type as a b/f operation because it is called from
    // a subframe in the case of a delegate bailing out of the nav before it even gets to provisional state.
    WebFrame *mainFrame = [getWebView(m_webFrame.get()) mainFrame];
    WebHistoryItem *resetItem = mainFrame->_private->currentItem;
    if (resetItem)
        [[getWebView(m_webFrame.get()) backForwardList] goToItem:resetItem];
}

bool WebFrameLoaderClient::provisionalItemIsTarget() const
{
    return [m_webFrame->_private->provisionalItem isTargetItem];
}

bool WebFrameLoaderClient::loadProvisionalItemFromPageCache()
{
    WebHistoryItem *item = m_webFrame->_private->provisionalItem;
    if (![item hasPageCache])
        return false;
    NSDictionary *pageCache = [item pageCache];
    if (![pageCache objectForKey:WebCorePageCacheStateKey])
        return false;
    [[m_webFrame.get() provisionalDataSource] _loadFromPageCache:pageCache];
    return true;
}

void WebFrameLoaderClient::invalidateCurrentItemPageCache()
{
    // When we are pre-commit, the currentItem is where the pageCache data resides
    WebHistoryItem *currentItem = m_webFrame->_private->currentItem;
    NSDictionary *pageCache = [currentItem pageCache];
    WebCorePageState *state = [pageCache objectForKey:WebCorePageCacheStateKey];
    WebCore::PageState* pageState = [state impl];

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the WebCorePageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    ASSERT(!pageState || pageState->document() == core(m_webFrame.get())->document());
    if (pageState && pageState->document() == core(m_webFrame.get())->document())
        pageState->clear();

    // We're assuming that WebCore invalidates its pageCache state in didNotOpen:pageCache:
    [currentItem setHasPageCache:NO];
}

bool WebFrameLoaderClient::privateBrowsingEnabled() const
{
    return [[getWebView(m_webFrame.get()) preferences] privateBrowsingEnabled];
}

void WebFrameLoaderClient::makeDocumentView()
{
    WebFrameView *v = m_webFrame->_private->webFrameView;
    WebDataSource *ds = [m_webFrame.get() dataSource];

    NSView <WebDocumentView> *documentView = [v _makeDocumentViewForDataSource:ds];
    if (!documentView)
        return;

    WebFrameBridge *bridge = m_webFrame->_private->bridge;

    // FIXME: We could save work and not do this for a top-level view that is not a WebHTMLView.
    [bridge createFrameViewWithNSView:documentView marginWidth:[v _marginWidth] marginHeight:[v _marginHeight]];
    [m_webFrame.get() _updateBackground];
    [bridge installInFrame:[v _scrollView]];

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:ds];
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader* loader)
{
    [dataSource(loader) _makeRepresentation];
}

void WebFrameLoaderClient::setDocumentViewFromPageCache(NSDictionary *pageCache)
{
    NSView <WebDocumentView> *cachedView = [pageCache objectForKey:WebPageCacheDocumentViewKey];
    ASSERT(cachedView != nil);
    [m_webFrame->_private->webFrameView _setDocumentView:cachedView];
}

void WebFrameLoaderClient::forceLayout()
{
    NSView <WebDocumentView> *view = [m_webFrame->_private->webFrameView documentView];
    if ([view isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)view setNeedsToApplyStyles:YES];
    [view setNeedsLayout:YES];
    [view layout];
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    WebFrameView *thisView = m_webFrame->_private->webFrameView;
    NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
    ASSERT(thisDocumentView != nil);
    
    // Tell the just loaded document to layout.  This may be necessary
    // for non-html content that needs a layout message.
    if (!([[m_webFrame.get() dataSource] _isDocumentHTML])) {
        [thisDocumentView setNeedsLayout:YES];
        [thisDocumentView layout];
        [thisDocumentView setNeedsDisplay:YES];
    }
}

void WebFrameLoaderClient::updateHistoryForCommit()
{
    FrameLoadType type = core(m_webFrame.get())->loader()->loadType();
    if (isBackForwardLoadType(type) ||
        (type == FrameLoadTypeReload && [[m_webFrame.get() provisionalDataSource] unreachableURL] != nil)) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        [m_webFrame.get() _setPreviousItem:m_webFrame->_private->currentItem];
        ASSERT(m_webFrame->_private->provisionalItem);
        [m_webFrame.get() _setCurrentItem:m_webFrame->_private->provisionalItem];
        [m_webFrame.get() _setProvisionalItem:nil];
    }
}

void WebFrameLoaderClient::updateHistoryForBackForwardNavigation()
{
    // Must grab the current scroll position before disturbing it
    [m_webFrame.get() _saveScrollPositionAndViewStateToItem:m_webFrame->_private->previousItem];
}

void WebFrameLoaderClient::updateHistoryForReload()
{
    WebHistoryItem *currItem = m_webFrame->_private->previousItem;
    [currItem setHasPageCache:NO];
    if (core(m_webFrame.get())->loader()->loadType() == FrameLoadTypeReload)
        [m_webFrame.get() _saveScrollPositionAndViewStateToItem:currItem];
    WebDataSource *dataSource = [m_webFrame.get() dataSource];
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

void WebFrameLoaderClient::updateHistoryForStandardLoad()
{
    WebDataSource *dataSource = [m_webFrame.get() dataSource];
    if (![dataSource _documentLoader]->isClientRedirect()) {
        NSURL *URL = [dataSource _URLForHistory];
        if (URL && ![URL _web_isEmpty]) {
            ASSERT(getWebView(m_webFrame.get()));
            if (![[getWebView(m_webFrame.get()) preferences] privateBrowsingEnabled]) {
                WebHistoryItem *entry = [[WebHistory optionalSharedHistory] addItemForURL:URL];
                if ([dataSource pageTitle])
                    [entry setTitle:[dataSource pageTitle]];                            
            }
            [m_webFrame.get() _addBackForwardItemClippedAtTarget:YES];
        }
    } else {
        NSURLRequest *request = [dataSource request];
        
        // Update the URL in the BF list that we made before the redirect, unless
        // this is alternate content for an unreachable URL (we want the BF list
        // item to remember the unreachable URL in case it becomes reachable later).
        if ([request _webDataRequestUnreachableURL] == nil) {
            [m_webFrame->_private->currentItem setURL:[request URL]];

            // clear out the form data so we don't repost it to the wrong place if we
            // ever go back/forward to this item
            [m_webFrame->_private->currentItem _setFormInfoFromRequest:request];

            // We must also clear out form data so we don't try to restore it into the incoming page,
            // see -_opened
        }
    }
}

void WebFrameLoaderClient::updateHistoryForInternalLoad()
{
    // Add an item to the item tree for this frame
    ASSERT(!core(m_webFrame.get())->loader()->documentLoader()->isClientRedirect());
    WebFrame *parentFrame = [m_webFrame.get() parentFrame];
    if (parentFrame) {
        WebHistoryItem *parentItem = parentFrame->_private->currentItem;
        // The only case where parentItem==nil should be when a parent frame loaded an
        // empty URL, which doesn't set up a current item in that parent.
        if (parentItem)
            [parentItem addChildItem:[m_webFrame.get() _createItem:YES]];
    } else {
        // See 3556159. It's not clear if it's valid to be in WebFrameLoadTypeOnLoadEvent
        // for a top-level frame, but that was a likely explanation for those crashes,
        // so let's guard against it.
        // ...and all WebFrameLoadTypeOnLoadEvent uses were folded to WebFrameLoadTypeInternal
        LOG_ERROR("no parent frame in transitionToCommitted:, WebFrameLoadTypeInternal");
    }
}

void WebFrameLoaderClient::updateHistoryAfterClientRedirect()
{
    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // khtml has closed the URL and saved away the form state.
    WebHistoryItem *item = m_webFrame->_private->currentItem;
    [item setDocumentState:nil];
    [item setScrollPoint:NSZeroPoint];
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    [[[m_webFrame->_private->webFrameView _scrollView] contentView] setCopiesOnScroll:YES];
}

LoadErrorResetToken* WebFrameLoaderClient::tokenForLoadErrorReset()
{
    if (isBackForwardLoadType(core(m_webFrame.get())->loader()->loadType()) && [m_webFrame.get() _isMainFrame])
        return (LoadErrorResetToken*)[m_webFrame->_private->currentItem retain];
    return 0;
}

void WebFrameLoaderClient::resetAfterLoadError(LoadErrorResetToken* token)
{
    WebHistoryItem *item = (WebHistoryItem *)token;
    if (!item)
        return;
    [[getWebView(m_webFrame.get()) backForwardList] goToItem:item];
    [item release];
}

void WebFrameLoaderClient::doNotResetAfterLoadError(LoadErrorResetToken* token)
{
    WebHistoryItem *item = (WebHistoryItem *)token;
    [item release];
}

void WebFrameLoaderClient::willCloseDocument()
{
    [m_webFrame->_private->plugInViews makeObjectsPerformSelector:@selector(setWebFrame:) withObject:nil];
    [m_webFrame->_private->plugInViews release];
    m_webFrame->_private->plugInViews = nil;
}

void WebFrameLoaderClient::detachedFromParent1()
{
    [m_webFrame.get() _saveScrollPositionAndViewStateToItem:m_webFrame->_private->currentItem];
}

void WebFrameLoaderClient::detachedFromParent2()
{
    [m_webFrame->_private->inspectors makeObjectsPerformSelector:@selector(_webFrameDetached:) withObject:m_webFrame.get()];
    [m_webFrame->_private->webFrameView _setWebFrame:nil]; // needed for now to be compatible w/ old behavior
}

void WebFrameLoaderClient::detachedFromParent3()
{
    [m_webFrame->_private->webFrameView release];
    m_webFrame->_private->webFrameView = nil;
}

void WebFrameLoaderClient::detachedFromParent4()
{
    m_webFrame->_private->bridge = nil;
}

void WebFrameLoaderClient::loadedFromPageCache()
{
    // Release the resources kept in the page cache.
    // They will be reset when we leave this page.
    // The WebCore side of the page cache will have already been invalidated by
    // the bridge to prevent premature release.
    [m_webFrame->_private->currentItem setHasPageCache:NO];
}

void WebFrameLoaderClient::download(NSURLConnection *connection, NSURLRequest *request,
    NSURLResponse *response, id proxy)
{
    [WebDownload _downloadWithLoadingConnection:connection
                                        request:request
                                       response:response
                                       delegate:[getWebView(m_webFrame.get()) downloadDelegate]
                                          proxy:proxy];
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader* loader, NSURLRequest *request, NSURLResponse *response, int length)
{
    WebView *webView = getWebView(loader);
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (!implementations.delegateImplementsDidLoadResourceFromMemoryCache)
        return false;

    implementations.didLoadResourceFromMemoryCacheFunc(resourceLoadDelegate, @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:), webView, request, response, length, dataSource(loader));
    return true;
}

id WebFrameLoaderClient::dispatchIdentifierForInitialRequest(DocumentLoader* loader, NSURLRequest *request)
{
    WebView *webView = getWebView(loader);
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsIdentifierForRequest)
        return implementations.identifierForRequestFunc(resourceLoadDelegate, @selector(webView:identifierForInitialRequest:fromDataSource:), webView, request, dataSource(loader));

    return [[[NSObject alloc] init] autorelease];
}

NSURLRequest *WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, id identifier,
    NSURLRequest *request, NSURLResponse *redirectResponse)
{
    WebView *webView = getWebView(loader);
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsWillSendRequest)
        return implementations.willSendRequestFunc(resourceLoadDelegate, @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:), webView, identifier, request, redirectResponse, dataSource(loader));

    return request;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, id identifier,
    NSURLAuthenticationChallenge *challenge)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = [webView resourceLoadDelegate];
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidReceiveAuthenticationChallenge) {
        [resourceLoadDelegate webView:webView resource:identifier didReceiveAuthenticationChallenge:challenge fromDataSource:dataSource(loader)];
        return;
    }

    NSWindow *window = [webView hostWindow] ? [webView hostWindow] : [webView window];
    [[WebPanelAuthenticationHandler sharedHandler] startAuthentication:challenge window:window];
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader* loader, id identifier,
    NSURLAuthenticationChallenge *challenge)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = [webView resourceLoadDelegate];
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidCancelAuthenticationChallenge) {
        [resourceLoadDelegate webView:webView resource:identifier didCancelAuthenticationChallenge:challenge fromDataSource:dataSource(loader)];
        return;
    }

    [(WebPanelAuthenticationHandler *)[WebPanelAuthenticationHandler sharedHandler] cancelAuthentication:challenge];
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader, id identifier, NSURLResponse *response)
{
    WebView *webView = getWebView(loader);
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidReceiveResponse)
        implementations.didReceiveResponseFunc(resourceLoadDelegate, @selector(webView:resource:didReceiveResponse:fromDataSource:), webView, identifier, response, dataSource(loader));
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader, id identifier, int lengthReceived)
{
    WebView *webView = getWebView(loader);
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidReceiveContentLength)
        implementations.didReceiveContentLengthFunc(resourceLoadDelegate, @selector(webView:resource:didReceiveContentLength:fromDataSource:), webView, identifier, (WebNSUInteger)lengthReceived, dataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, id identifier)
{
    WebView *webView = getWebView(loader);
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);
    
    if (implementations.delegateImplementsDidFinishLoadingFromDataSource)
        implementations.didFinishLoadingFromDataSourceFunc(resourceLoadDelegate, @selector(webView:resource:didFinishLoadingFromDataSource:), webView, identifier, dataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader, id identifier, NSError *error)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _resourceLoadDelegateForwarder] webView:webView resource:identifier didFailLoadingWithError:error fromDataSource:dataSource(loader)];
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _frameLoadDelegateForwarder] webView:webView didHandleOnloadEventsForFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _frameLoadDelegateForwarder] webView:webView
       didReceiveServerRedirectForProvisionalLoadForFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _frameLoadDelegateForwarder] webView:webView didCancelClientRedirectForFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& URL, double delay, double fireDate)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _frameLoadDelegateForwarder] webView:webView
                         willPerformClientRedirectToURL:URL.getNSURL()
                                                  delay:delay
                                               fireDate:[NSDate dateWithTimeIntervalSince1970:fireDate]
                                               forFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebView *webView = getWebView(m_webFrame.get());   
    [[webView _frameLoadDelegateForwarder] webView:webView didChangeLocationWithinPageForFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchWillClose()
{
    WebView *webView = getWebView(m_webFrame.get());   
    [[webView _frameLoadDelegateForwarder] webView:webView willCloseFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    WebView *webView = getWebView(m_webFrame.get());   
    ASSERT([m_webFrame.get() _isMainFrame]);
    // FIXME: This willChangeValueForKey call is too late, because the icon has already changed by now.
    [webView _willChangeValueForKey:_WebMainFrameIconKey];
    id delegate = [webView frameLoadDelegate];
    if ([delegate respondsToSelector:@selector(webView:didReceiveIcon:forFrame:)]) {
        Image* image = IconDatabase::sharedIconDatabase()->
            iconForPageURL(core(m_webFrame.get())->loader()->url().url(), IntSize(16, 16));
        NSImage *icon = webGetNSImage(image, NSMakeSize(16, 16));
        if (icon)
            [delegate webView:webView didReceiveIcon:icon forFrame:m_webFrame.get()];
    }
    [webView _didChangeValueForKey:_WebMainFrameIconKey];
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didStartProvisionalLoadForFrame:m_webFrame.get()];
    [[webView _frameLoadDelegateForwarder] webView:webView didStartProvisionalLoadForFrame:m_webFrame.get()];    
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebView *webView = getWebView(m_webFrame.get());   
    [[webView _frameLoadDelegateForwarder] webView:webView didReceiveTitle:title forFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    // Tell the client we've committed this URL.
    ASSERT([m_webFrame->_private->webFrameView documentView] != nil);

    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didCommitLoadForFrame:m_webFrame.get()];
    [[webView _frameLoadDelegateForwarder] webView:webView didCommitLoadForFrame:m_webFrame.get()];
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(NSError *error)
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didFailProvisionalLoadWithError:error forFrame:m_webFrame.get()];
    [[webView _frameLoadDelegateForwarder] webView:webView didFailProvisionalLoadWithError:error forFrame:m_webFrame.get()];
    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFailLoad(NSError *error)
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didFailLoadWithError:error forFrame:m_webFrame.get()];
    [[webView _frameLoadDelegateForwarder] webView:webView didFailLoadWithError:error forFrame:m_webFrame.get()];
    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didFinishLoadForFrame:m_webFrame.get()];
    [[webView _frameLoadDelegateForwarder] webView:webView didFinishLoadForFrame:m_webFrame.get()];
    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:nil];
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _frameLoadDelegateForwarder] webView:webView didFirstLayoutInFrame:m_webFrame.get()];
}

Frame* WebFrameLoaderClient::dispatchCreatePage(NSURLRequest *request)
{
    WebView *currentWebView = getWebView(m_webFrame.get());
    id wd = [currentWebView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        return core([[wd webView:currentWebView createWebViewWithRequest:request] mainFrame]);
    return 0;
}

void WebFrameLoaderClient::dispatchShow()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _UIDelegateForwarder] webViewShow:webView];
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction function,
    const String& MIMEType, NSURLRequest *request)
{
    WebView *webView = getWebView(m_webFrame.get());

    [[webView _policyDelegateForwarder] webView:webView
                        decidePolicyForMIMEType:MIMEType
                                        request:request
                                          frame:m_webFrame.get()
                               decisionListener:setUpPolicyListener(function).get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function,
    const NavigationAction& action, NSURLRequest *request, const String& frameName)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _policyDelegateForwarder] webView:webView
            decidePolicyForNewWindowAction:actionDictionary(action)
                                   request:request
                              newFrameName:frameName
                          decisionListener:setUpPolicyListener(function).get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function,
    const NavigationAction& action, NSURLRequest *request)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _policyDelegateForwarder] webView:webView
                decidePolicyForNavigationAction:actionDictionary(action)
                                        request:request
                                          frame:m_webFrame.get()
                               decisionListener:setUpPolicyListener(function).get()];
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    [m_policyListener.get() invalidate];
    m_policyListener = nil;
    m_policyFunction = 0;
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(NSError *error)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _policyDelegateForwarder] webView:webView
        unableToImplementPolicyWithError:error frame:m_webFrame.get()];    
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState> formState)
{
    id <WebFormDelegate> formDelegate = [getWebView(m_webFrame.get()) _formDelegate];
    if (!formDelegate) {
        (core(m_webFrame.get())->loader()->*function)(PolicyUse);
        return;
    }

    NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] initWithCapacity:formState->values().size()];
    HashMap<String, String>::const_iterator end = formState->values().end();
    for (HashMap<String, String>::const_iterator it = formState->values().begin(); it != end; ++it)
        if (!it->first.isNull() && !it->second.isNull())
            [dictionary setObject:it->second forKey:it->first];

    [formDelegate frame:m_webFrame.get()
            sourceFrame:kit(formState->sourceFrame())
         willSubmitForm:kit(formState->form())
             withValues:dictionary
     submissionListener:setUpPolicyListener(function).get()];

    [dictionary release];
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader* loader)
{
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:getWebView(m_webFrame.get())
            didLoadMainResourceForDataSource:dataSource(loader)];
}

void WebFrameLoaderClient::clearLoadingFromPageCache(DocumentLoader* loader)
{
    [dataSource(loader) _setLoadingFromPageCache:NO];
}

bool WebFrameLoaderClient::isLoadingFromPageCache(DocumentLoader* loader)
{
    return [dataSource(loader) _loadingFromPageCache];
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader* loader)
{
    [dataSource(loader) _revertToProvisionalState];
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader* loader, NSError *error)
{
    [dataSource(loader) _setMainDocumentError:error];
}

void WebFrameLoaderClient::clearUnarchivingState(DocumentLoader* loader)
{
    [dataSource(loader) _clearUnarchivingState];
}

void WebFrameLoaderClient::progressStarted()
{
    [getWebView(m_webFrame.get()) _progressStarted:m_webFrame.get()];
}

void WebFrameLoaderClient::progressCompleted()
{
    [getWebView(m_webFrame.get()) _progressCompleted:m_webFrame.get()];
}

void WebFrameLoaderClient::incrementProgress(id identifier, NSURLResponse *response)
{
    [getWebView(m_webFrame.get()) _incrementProgressForIdentifier:identifier response:response];
}

void WebFrameLoaderClient::incrementProgress(id identifier, NSData *data)
{
    [getWebView(m_webFrame.get()) _incrementProgressForIdentifier:identifier data:data];
}

void WebFrameLoaderClient::completeProgress(id identifier)
{
    [getWebView(m_webFrame.get()) _completeProgressForIdentifier:identifier];
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready)
{
    [getWebView(m_webFrame.get()) setMainFrameDocumentReady:ready];
}

void WebFrameLoaderClient::startDownload(NSURLRequest *request)
{
    // FIXME: Should download full request.
    [getWebView(m_webFrame.get()) _downloadURL:[request URL]];
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader* loader)
{
    // FIXME: Should do this only in main frame case, right?
    [getWebView(m_webFrame.get()) _willChangeValueForKey:_WebMainFrameTitleKey];
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader* loader)
{
    // FIXME: Should do this only in main frame case, right?
    [getWebView(m_webFrame.get()) _didChangeValueForKey:_WebMainFrameTitleKey];
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, NSData *data)
{
    [dataSource(loader) _receivedData:data];
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    [dataSource(loader) _finishedLoading];
}

void WebFrameLoaderClient::finalSetupForReplace(DocumentLoader* loader)
{
    [dataSource(loader) _clearUnarchivingState];
}

NSError *WebFrameLoaderClient::cancelledError(NSURLRequest *request)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:[request URL]];
}

NSError *WebFrameLoaderClient::cannotShowURLError(NSURLRequest *request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotShowURL URL:[request URL]];
}

NSError *WebFrameLoaderClient::interruptForPolicyChangeError(NSURLRequest *request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:[request URL]];
}

NSError *WebFrameLoaderClient::cannotShowMIMETypeError(NSURLResponse *response)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:WebKitErrorCannotShowMIMEType URL:[response URL]];    
}

NSError *WebFrameLoaderClient::fileDoesNotExistError(NSURLResponse *response)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist URL:[response URL]];    
}

bool WebFrameLoaderClient::shouldFallBack(NSError *error)
{
    // FIXME: Needs to check domain.
    // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent
    // loading plugin content twice.  See <rdar://problem/4258008>
    return [error code] != NSURLErrorCancelled && [error code] != WebKitErrorPlugInWillHandleLoad;
}

void WebFrameLoaderClient::setDefersLoading(bool defers)
{
    if (!defers)
        deliverArchivedResourcesAfterDelay();
}

bool WebFrameLoaderClient::willUseArchive(ResourceLoader* loader, NSURLRequest *request, NSURL *originalURL) const
{
    if (![[request URL] isEqual:originalURL])
        return false;
    if (!canUseArchivedResource(request))
        return false;
    WebResource *resource = [dataSource(core(m_webFrame.get())->loader()->activeDocumentLoader()) _archivedSubresourceForURL:originalURL];
    if (!resource)
        return false;
    if (!canUseArchivedResource([resource _response]))
        return false;
    m_pendingArchivedResources.set(loader, resource);
    // Deliver the resource after a delay because callers don't expect to receive callbacks while calling this method.
    deliverArchivedResourcesAfterDelay();
    return true;
}

bool WebFrameLoaderClient::isArchiveLoadPending(ResourceLoader* loader) const
{
    return m_pendingArchivedResources.contains(loader);
}

void WebFrameLoaderClient::cancelPendingArchiveLoad(ResourceLoader* loader)
{
    if (m_pendingArchivedResources.isEmpty())
        return;
    m_pendingArchivedResources.remove(loader);
    if (m_pendingArchivedResources.isEmpty())
        m_archivedResourcesDeliveryTimer.stop();
}

void WebFrameLoaderClient::clearArchivedResources()
{
    m_pendingArchivedResources.clear();
    m_archivedResourcesDeliveryTimer.stop();
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest& request) const
{
    return [WebView _canHandleRequest:request.nsURLRequest()];
}

bool WebFrameLoaderClient::canShowMIMEType(const String& MIMEType) const
{
    return [WebView canShowMIMEType:MIMEType];
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& URLScheme) const
{
    return [WebView _representationExistsForURLScheme:URLScheme];
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    return [WebView _generatedMIMETypeForURLScheme:URLScheme];
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    // Note: Can be called multiple times.
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to clear these out.
    NSScrollView *sv = [m_webFrame->_private->webFrameView _scrollView];
    if ([getWebView(m_webFrame.get()) drawsBackground])
        [sv setDrawsBackground:YES];
    [m_webFrame.get() _setPreviousItem:nil];
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
void WebFrameLoaderClient::restoreScrollPositionAndViewState()
{
    ASSERT(m_webFrame->_private->currentItem);
    NSView <WebDocumentView> *docView = [m_webFrame->_private->webFrameView documentView];
    NSPoint point = [m_webFrame->_private->currentItem scrollPoint];
    if ([docView conformsToProtocol:@protocol(_WebDocumentViewState)]) {        
        id state = [m_webFrame->_private->currentItem viewState];
        if (state) {
            [(id <_WebDocumentViewState>)docView setViewState:state];
        }
        
        [(id <_WebDocumentViewState>)docView setScrollPoint:point];
    } else {
        [docView scrollPoint:point];
    }
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    FrameLoadType loadType = core(m_webFrame.get())->loader()->loadType();
    
    // FIXME: This is OK as long as no one resizes the window,
    // but in the case where someone does, it means garbage outside
    // the occupied part of the scroll view.
    [[m_webFrame->_private->webFrameView _scrollView] setDrawsBackground:NO];
    
    // Cache the page, if possible.
    // Don't write to the cache if in the middle of a redirect, since we will want to
    // store the final page we end up on.
    // No point writing to the cache on a reload or loadSame, since we will just write
    // over it again when we leave that page.
    WebHistoryItem *item = m_webFrame->_private->currentItem;
    if ([m_webFrame.get() _canCachePage]
        && item
        && !core(m_webFrame.get())->loader()->isQuickRedirectComing()
        && loadType != FrameLoadTypeReload 
        && loadType != FrameLoadTypeReloadAllowingStaleData
        && loadType != FrameLoadTypeSame
        && ![[m_webFrame.get() dataSource] isLoading]
        && !core(m_webFrame.get())->loader()->documentLoader()->isStopping()) {
        if ([[[m_webFrame.get() dataSource] representation] isKindOfClass:[WebHTMLRepresentation class]]) {
            if (![item pageCache]) {
                // Add the items to this page's cache.
                if (createPageCache(item))
                    // See if any page caches need to be purged after the addition of this new page cache.
                    [m_webFrame.get() _purgePageCache];
            }
        } else
            // Put the document into a null state, so it can be restored correctly.
            core(m_webFrame.get())->loader()->clear();
    }
}

bool WebFrameLoaderClient::shouldTreatURLAsSameAsCurrent(const KURL& URL) const
{
    WebHistoryItem *item = m_webFrame->_private->currentItem;
    NSString* URLString = [URL.getNSURL() _web_originalDataAsString];
    return [URLString isEqual:[item URLString]] || [URLString isEqual:[item originalURLString]];
}

void WebFrameLoaderClient::addHistoryItemForFragmentScroll()
{
    [m_webFrame.get() _addBackForwardItemClippedAtTarget:NO];
}

void WebFrameLoaderClient::didFinishLoad()
{
    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:nil];    
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    if (![m_webFrame.get() dataSource]) {
        ASSERT(!core(m_webFrame.get())->tree()->childCount());
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
    NSView *frameView = m_webFrame->_private->webFrameView;
    NSWindow *window = [frameView window];
    NSResponder *firstResp = [window firstResponder];
    if ([firstResp isKindOfClass:[NSView class]] && [(NSView *)firstResp isDescendantOf:frameView])
        [window endEditingFor:firstResp];
    
    core(m_webFrame.get())->loader()->detachChildren();
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(NSURLRequest *request)
{
    RefPtr<WebDocumentLoaderMac> loader = new WebDocumentLoaderMac(request);

    WebDataSource *dataSource = [[WebDataSource alloc] _initWithDocumentLoader:loader.get()];
    loader->setDataSource(dataSource);
    [dataSource release];

    return loader.release();
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& URL)
{
    NSString *titleNSString = title;
    [[[WebHistory optionalSharedHistory] itemForURL:URL.getNSURL()] setTitle:titleNSString];
    [m_webFrame->_private->currentItem setTitle:titleNSString];
}

// The following 2 functions are copied from [NSHTTPURLProtocol _cachedResponsePassesValidityChecks] and modified for our needs.
// FIXME: It would be nice to eventually to share this logic somehow.
bool WebFrameLoaderClient::canUseArchivedResource(NSURLRequest *request) const
{
    NSURLRequestCachePolicy policy = [request cachePolicy];
    if (policy == NSURLRequestReturnCacheDataElseLoad)
        return true;
    if (policy == NSURLRequestReturnCacheDataDontLoad)
        return true;
    if (policy == NSURLRequestReloadIgnoringCacheData)
        return false;
    if ([request valueForHTTPHeaderField:@"must-revalidate"] != nil)
        return false;
    if ([request valueForHTTPHeaderField:@"proxy-revalidate"] != nil)
        return false;
    if ([request valueForHTTPHeaderField:@"If-Modified-Since"] != nil)
        return false;
    if ([request valueForHTTPHeaderField:@"Cache-Control"] != nil)
        return false;
    if ([@"POST" _webkit_isCaseInsensitiveEqualToString:[request HTTPMethod]])
        return false;
    return true;
}

bool WebFrameLoaderClient::canUseArchivedResource(NSURLResponse *response) const
{
    if (WKGetNSURLResponseMustRevalidate(response))
        return false;
    if (WKGetNSURLResponseCalculatedExpiration(response) - CFAbsoluteTimeGetCurrent() < 1)
        return false;
    return true;
}

void WebFrameLoaderClient::deliverArchivedResourcesAfterDelay() const
{
    if (m_pendingArchivedResources.isEmpty())
        return;
    if (core(m_webFrame.get())->page()->defersLoading())
        return;
    if (!m_archivedResourcesDeliveryTimer.isActive())
        m_archivedResourcesDeliveryTimer.startOneShot(0);
}

void WebFrameLoaderClient::deliverArchivedResources(Timer<WebFrameLoaderClient>*)
{
    if (m_pendingArchivedResources.isEmpty())
        return;
    if (core(m_webFrame.get())->page()->defersLoading())
        return;

    const ResourceMap copy = m_pendingArchivedResources;
    m_pendingArchivedResources.clear();

    ResourceMap::const_iterator end = copy.end();
    for (ResourceMap::const_iterator it = copy.begin(); it != end; ++it) {
        RefPtr<ResourceLoader> loader = it->first;
        WebResource *resource = it->second.get();
        NSData *data = [[resource data] retain];
        loader->didReceiveResponse([resource _response]);
        loader->didReceiveData(data, [data length], true);
        [data release];
        loader->didFinishLoading();
    }
}

bool WebFrameLoaderClient::createPageCache(WebHistoryItem *item)
{
    WebCorePageState *pageState = [[WebCorePageState alloc] initWithPage:core(m_webFrame.get())->page()];
    if (!pageState) {
        [item setHasPageCache:NO];
        return false;
    }

    [item setHasPageCache:YES];
    NSMutableDictionary *pageCache = [item pageCache];

    [pageCache setObject:pageState forKey:WebCorePageCacheStateKey];
    [pageCache setObject:[NSDate date] forKey:WebPageCacheEntryDateKey];
    [pageCache setObject:[m_webFrame.get() dataSource] forKey:WebPageCacheDataSourceKey];
    [pageCache setObject:[m_webFrame->_private->webFrameView documentView] forKey:WebPageCacheDocumentViewKey];

    [pageState release];

    return true;
}

RetainPtr<WebFramePolicyListener> WebFrameLoaderClient::setUpPolicyListener(FramePolicyFunction function)
{
    ASSERT(!m_policyListener);
    ASSERT(!m_policyFunction);

    [m_policyListener.get() invalidate];

    WebFramePolicyListener *listener = [[WebFramePolicyListener alloc] initWithWebCoreFrame:core(m_webFrame.get())];
    m_policyListener = listener;
    [listener release];
    m_policyFunction = function;

    return listener;
}

void WebFrameLoaderClient::receivedPolicyDecison(PolicyAction action)
{
    ASSERT(m_policyListener);
    ASSERT(m_policyFunction);

    FramePolicyFunction function = m_policyFunction;

    m_policyListener = nil;
    m_policyFunction = 0;

    (core(m_webFrame.get())->loader()->*function)(action);
}

String WebFrameLoaderClient::userAgent()
{
    return [getWebView(m_webFrame.get()) _userAgent];
}

static const MouseEvent* findMouseEvent(const Event* event)
{
    for (const Event* e = event; e; e = e->underlyingEvent())
        if (e->isMouseEvent())
            return static_cast<const MouseEvent*>(e);
    return 0;
}

NSDictionary *WebFrameLoaderClient::actionDictionary(const NavigationAction& action) const
{
    unsigned modifierFlags = 0;
    const Event* event = action.event();
    if (const UIEventWithKeyState* keyStateEvent = findEventWithKeyState(const_cast<Event*>(event))) {
        if (keyStateEvent->ctrlKey())
            modifierFlags |= NSControlKeyMask;
        if (keyStateEvent->altKey())
            modifierFlags |= NSAlternateKeyMask;
        if (keyStateEvent->shiftKey())
            modifierFlags |= NSShiftKeyMask;
        if (keyStateEvent->metaKey())
            modifierFlags |= NSCommandKeyMask;
    }
    if (const MouseEvent* mouseEvent = findMouseEvent(event)) {
        IntPoint point(mouseEvent->clientX(), mouseEvent->clientY());
        WebElementDictionary *element = [[WebElementDictionary alloc]
            initWithHitTestResult:core(m_webFrame.get())->eventHandler()->hitTestResultAtPoint(point, false)];
        NSDictionary *result = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:action.type()], WebActionNavigationTypeKey,
            element, WebActionElementKey,
            [NSNumber numberWithInt:mouseEvent->button()], WebActionButtonKey,
            [NSNumber numberWithInt:modifierFlags], WebActionModifierFlagsKey,
            action.URL().getNSURL(), WebActionOriginalURLKey,
            nil];
        [element release];
        return result;
    }
    return [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:action.type()], WebActionNavigationTypeKey,
        [NSNumber numberWithInt:modifierFlags], WebActionModifierFlagsKey,
        action.URL().getNSURL(), WebActionOriginalURLKey,
        nil];
}

@implementation WebFramePolicyListener

- (id)initWithWebCoreFrame:(Frame*)frame
{
    self = [self init];
    if (!self)
        return nil;
    frame->ref();
    m_frame = frame;
    return self;
}

- (void)invalidate
{
    if (m_frame) {
        m_frame->deref();
        m_frame = 0;
    }
}

- (void)dealloc
{
    if (m_frame)
        m_frame->deref();
    [super dealloc];
}

- (void)finalize
{
    if (m_frame)
        m_frame->deref();
    [super finalize];
}

- (void)receivedPolicyDecision:(PolicyAction)action
{
    RefPtr<Frame> frame = adoptRef(m_frame);
    m_frame = 0;
    if (frame)
        static_cast<WebFrameLoaderClient*>(frame->loader()->client())->receivedPolicyDecison(action);
}

- (void)ignore
{
    [self receivedPolicyDecision:PolicyIgnore];
}

- (void)download
{
    [self receivedPolicyDecision:PolicyDownload];
}

- (void)use
{
    [self receivedPolicyDecision:PolicyUse];
}

- (void)continue
{
    [self receivedPolicyDecision:PolicyUse];
}

@end
