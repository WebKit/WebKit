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

#import "DOMElementInternal.h"
#import "WebBackForwardList.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebPolicyDelegatePrivate.h"
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
#import "WebKitLogging.h"
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
#import <WebCore/AuthenticationMac.h>
#import <WebCore/BlockExceptions.h>
#import <WebCore/CachedPage.h>
#import <WebCore/Chrome.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FormState.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/IconDatabase.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformString.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/ResourceLoader.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/WebCoreFrameBridge.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/Widget.h>
#import <WebKit/DOMElement.h>
#import <WebKit/DOMHTMLFormElement.h>
#import <WebKitSystemInterface.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

// SPI for NSURLDownload
// Needed for <rdar://problem/5121850> 
@interface NSURLDownload (NSURLDownloadPrivate)

- (void)_setOriginatingURL:(NSURL *)url;
- (NSURL *)_originatingURL;

@end

// FIXME: This is unnecessary after <rdar://problem/5208329>
@interface WebHistoryItem (WebHistoryItemPrivate)

- (BOOL)_wasUserGesture;

@end

@interface WebFramePolicyListener : NSObject <WebPolicyDecisionListener, WebFormSubmissionListener>
{
    Frame* m_frame;
}
- (id)initWithWebCoreFrame:(Frame*)frame;
- (void)invalidate;
@end

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

void WebFrameLoaderClient::setDocumentViewFromCachedPage(CachedPage* cachedPage)
{
    DocumentLoader* cachedDocumentLoader = cachedPage->documentLoader();
    ASSERT(cachedDocumentLoader);
    cachedDocumentLoader->setFrame(core(m_webFrame.get()));
    NSView <WebDocumentView> *cachedView = cachedPage->documentView();
    ASSERT(cachedView != nil);
    [cachedView setDataSource:dataSource(cachedDocumentLoader)];
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

void WebFrameLoaderClient::setCopiesOnScroll()
{
    [[[m_webFrame->_private->webFrameView _scrollView] contentView] setCopiesOnScroll:YES];
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

void WebFrameLoaderClient::loadedFromCachedPage()
{
    // Release the resources kept in the page cache.
    // They will be reset when we leave this page.
    // The WebCore side of the page cache will have already been invalidated by
    // the bridge to prevent premature release.
    core(m_webFrame.get())->loader()->currentHistoryItem()->setCachedPage(0);
}

void WebFrameLoaderClient::download(ResourceHandle* handle, const ResourceRequest& request, const ResourceResponse& response)
{
    id proxy = handle->releaseProxy();
    ASSERT(proxy);
    
    WebView *webView = getWebView(m_webFrame.get());
    WebDownload *download = [WebDownload _downloadWithLoadingConnection:handle->connection()
                                                                request:request.nsURLRequest()
                                                               response:response.nsURLResponse()
                                                               delegate:[webView downloadDelegate]
                                                                  proxy:proxy]; 
    NSURL *originalURL = nil;
    WebBackForwardList *history = [webView backForwardList];
    WebHistoryItem *currentItem = nil;
    int backListCount = [history backListCount];
    int backCount = 0;
    
    // find the first item in the history that was originated
    // by the user
    while (backListCount > 0 && !originalURL) {
        currentItem = [history itemAtIndex:backCount--];
        
        if (![currentItem respondsToSelector:@selector(_wasUserGesture)] || [currentItem _wasUserGesture])
            originalURL = [currentItem URL];
    }

    ASSERT(originalURL);
    if ([download respondsToSelector:@selector(_setOriginatingURL:)])
        [download _setOriginatingURL:originalURL];
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& response, int length)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (!implementations.delegateImplementsDidLoadResourceFromMemoryCache)
        return false;

    implementations.didLoadResourceFromMemoryCacheFunc(resourceLoadDelegate, @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:), webView, request.nsURLRequest(), response.nsURLResponse(), length, dataSource(loader));
    return true;
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    id object;
    BOOL shouldRelease = NO;
    if (implementations.delegateImplementsIdentifierForRequest)
        object = implementations.identifierForRequestFunc(resourceLoadDelegate, @selector(webView:identifierForInitialRequest:fromDataSource:), webView, request.nsURLRequest(), dataSource(loader));
    else {
        object = [[NSObject alloc] init];
        shouldRelease = YES;
    }
    [webView _addObject:object forIdentifier:identifier];
    if (shouldRelease)
        [object release];
}

void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (redirectResponse.isNull())
        static_cast<WebDocumentLoaderMac*>(loader)->increaseLoadCount(identifier);

    if (implementations.delegateImplementsWillSendRequest)
        request = implementations.willSendRequestFunc(resourceLoadDelegate, @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:), webView, [webView _objectForIdentifier:identifier], request.nsURLRequest(), redirectResponse.nsURLResponse(), dataSource(loader));
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = [webView resourceLoadDelegate];
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    NSURLAuthenticationChallenge *webChallenge = mac(challenge);

    if (implementations.delegateImplementsDidReceiveAuthenticationChallenge) {
        [resourceLoadDelegate webView:webView resource:[webView _objectForIdentifier:identifier] didReceiveAuthenticationChallenge:webChallenge fromDataSource:dataSource(loader)];
        return;
    }

    NSWindow *window = [webView hostWindow] ? [webView hostWindow] : [webView window];
    [[WebPanelAuthenticationHandler sharedHandler] startAuthentication:webChallenge window:window];
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge&challenge)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = [webView resourceLoadDelegate];
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    NSURLAuthenticationChallenge *webChallenge = mac(challenge);

    if (implementations.delegateImplementsDidCancelAuthenticationChallenge) {
        [resourceLoadDelegate webView:webView resource:[webView _objectForIdentifier:identifier] didCancelAuthenticationChallenge:webChallenge fromDataSource:dataSource(loader)];
        return;
    }

    [(WebPanelAuthenticationHandler *)[WebPanelAuthenticationHandler sharedHandler] cancelAuthentication:webChallenge];
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidReceiveResponse)
        implementations.didReceiveResponseFunc(resourceLoadDelegate, @selector(webView:resource:didReceiveResponse:fromDataSource:), webView, [webView _objectForIdentifier:identifier], response.nsURLResponse(), dataSource(loader));
}

NSCachedURLResponse* WebFrameLoaderClient::willCacheResponse(DocumentLoader* loader, unsigned long identifier, NSCachedURLResponse* response) const
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsWillCacheResponse)
        return implementations.willCacheResponseFunc(resourceLoadDelegate, @selector(webView:resource:willCacheResponse:fromDataSource:), webView, [webView _objectForIdentifier:identifier], response, dataSource(loader));

    return response;
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int lengthReceived)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidReceiveContentLength)
        implementations.didReceiveContentLengthFunc(resourceLoadDelegate, @selector(webView:resource:didReceiveContentLength:fromDataSource:), webView, [webView _objectForIdentifier:identifier], (WebNSUInteger)lengthReceived, dataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);
    
    if (implementations.delegateImplementsDidFinishLoadingFromDataSource)
        implementations.didFinishLoadingFromDataSourceFunc(resourceLoadDelegate, @selector(webView:resource:didFinishLoadingFromDataSource:), webView, [webView _objectForIdentifier:identifier], dataSource(loader));
    [webView _removeObjectForIdentifier:identifier];

    static_cast<WebDocumentLoaderMac*>(loader)->decreaseLoadCount(identifier);
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const WebCore::ResourceError& error)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resourceLoadDelegate = WebViewGetResourceLoadDelegate(webView);
    WebResourceDelegateImplementationCache implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations.delegateImplementsDidFailLoadingWithErrorFromDataSource)
        implementations.didFailLoadingWithErrorFromDataSourceFunc(resourceLoadDelegate, @selector(webView:resource:didFailLoadingWithError:fromDataSource:), webView, [webView _objectForIdentifier:identifier], error, dataSource(loader));
    [webView _removeObjectForIdentifier:identifier];

    static_cast<WebDocumentLoaderMac*>(loader)->decreaseLoadCount(identifier);
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidHandleOnloadEventsForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didHandleOnloadEventsForFrameFunc(frameLoadDelegate, @selector(webView:didHandleOnloadEventsForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidReceiveServerRedirectForProvisionalLoadForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didReceiveServerRedirectForProvisionalLoadForFrameFunc(frameLoadDelegate, @selector(webView:didReceiveServerRedirectForProvisionalLoadForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidCancelClientRedirectForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didCancelClientRedirectForFrameFunc(frameLoadDelegate, @selector(webView:didCancelClientRedirectForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& URL, double delay, double fireDate)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsWillPerformClientRedirectToURLDelayFireDateForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.willPerformClientRedirectToURLDelayFireDateForFrameFunc(frameLoadDelegate, @selector(webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:), webView, URL.getNSURL(), delay, [NSDate dateWithTimeIntervalSince1970:fireDate], m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidChangeLocationWithinPageForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didChangeLocationWithinPageForFrameFunc(frameLoadDelegate, @selector(webView:didChangeLocationWithinPageForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchWillClose()
{
    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsWillCloseFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.willCloseFrameFunc(frameLoadDelegate, @selector(webView:willCloseFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    ASSERT([m_webFrame.get() _isMainFrame]);
    WebView *webView = getWebView(m_webFrame.get());   

    // FIXME: This willChangeValueForKey call is too late, because the icon has already changed by now.
    [webView _willChangeValueForKey:_WebMainFrameIconKey];

    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidReceiveIconForFrame) {
        Image* image = iconDatabase()->iconForPageURL(core(m_webFrame.get())->loader()->url().url(), IntSize(16, 16));
        if (NSImage *icon = webGetNSImage(image, NSMakeSize(16, 16))) {
            id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
            implementations.didReceiveIconForFrameFunc(frameLoadDelegate, @selector(webView:didReceiveIcon:forFrame:), webView, icon, m_webFrame.get());
        }
    }

    [webView _didChangeValueForKey:_WebMainFrameIconKey];
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didStartProvisionalLoadForFrame:m_webFrame.get()];

    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidStartProvisionalLoadForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didStartProvisionalLoadForFrameFunc(frameLoadDelegate, @selector(webView:didStartProvisionalLoadForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidReceiveTitleForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didReceiveTitleForFrameFunc(frameLoadDelegate, @selector(webView:didReceiveTitle:forFrame:), webView, title, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    // Tell the client we've committed this URL.
    ASSERT([m_webFrame->_private->webFrameView documentView] != nil);
    
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didCommitLoadForFrame:m_webFrame.get()];

    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidCommitLoadForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didCommitLoadForFrameFunc(frameLoadDelegate, @selector(webView:didCommitLoadForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didFailProvisionalLoadWithError:error forFrame:m_webFrame.get()];
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidFailProvisionalLoadWithErrorForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didFailProvisionalLoadWithErrorForFrameFunc(frameLoadDelegate, @selector(webView:didFailProvisionalLoadWithError:forFrame:), webView, error, m_webFrame.get());
    }
    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didFailLoadWithError:error forFrame:m_webFrame.get()];

    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidFailLoadWithErrorForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didFailLoadWithErrorForFrameFunc(frameLoadDelegate, @selector(webView:didFailLoadWithError:forFrame:), webView, error, m_webFrame.get());
    }

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidFinishDocumentLoadForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didFinishDocumentLoadForFrameFunc(frameLoadDelegate, @selector(webView:didFinishDocumentLoadForFrame:), webView, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didFinishLoadForFrame:m_webFrame.get()];

    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidFinishLoadForFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didFinishLoadForFrameFunc(frameLoadDelegate, @selector(webView:didFinishLoadForFrame:), webView, m_webFrame.get());
    }

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:nil];
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations.delegateImplementsDidFirstLayoutInFrame) {
        id frameLoadDelegate = WebViewGetFrameLoadDelegate(webView);
        implementations.didFirstLayoutInFrameFunc(frameLoadDelegate, @selector(webView:didFirstLayoutInFrame:), webView, m_webFrame.get());
    }
}

Frame* WebFrameLoaderClient::dispatchCreatePage()
{
    WebView *currentWebView = getWebView(m_webFrame.get());
    id wd = [currentWebView UIDelegate];
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        return core([[wd webView:currentWebView createWebViewWithRequest:nil] mainFrame]);
    return 0;
}

void WebFrameLoaderClient::dispatchShow()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _UIDelegateForwarder] webViewShow:webView];
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction function,
    const String& MIMEType, const ResourceRequest& request)
{
    WebView *webView = getWebView(m_webFrame.get());

    [[webView _policyDelegateForwarder] webView:webView
                        decidePolicyForMIMEType:MIMEType
                                        request:request.nsURLRequest()
                                          frame:m_webFrame.get()
                               decisionListener:setUpPolicyListener(function).get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function,
    const NavigationAction& action, const ResourceRequest& request, const String& frameName)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _policyDelegateForwarder] webView:webView
            decidePolicyForNewWindowAction:actionDictionary(action)
                                   request:request.nsURLRequest()
                              newFrameName:frameName
                          decisionListener:setUpPolicyListener(function).get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function,
    const NavigationAction& action, const ResourceRequest& request)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _policyDelegateForwarder] webView:webView
                decidePolicyForNavigationAction:actionDictionary(action)
                                        request:request.nsURLRequest()
                                          frame:m_webFrame.get()
                               decisionListener:setUpPolicyListener(function).get()];
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    [m_policyListener.get() invalidate];
    m_policyListener = nil;
    m_policyFunction = 0;
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError& error)
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

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader* loader)
{
    [dataSource(loader) _revertToProvisionalState];
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader* loader, const ResourceError& error)
{
    [dataSource(loader) _setMainDocumentError:error];
}

void WebFrameLoaderClient::clearUnarchivingState(DocumentLoader* loader)
{
    [dataSource(loader) _clearUnarchivingState];
}

void WebFrameLoaderClient::willChangeEstimatedProgress()
{
    [getWebView(m_webFrame.get()) _willChangeValueForKey:_WebEstimatedProgressKey];
}

void WebFrameLoaderClient::didChangeEstimatedProgress()
{
    [getWebView(m_webFrame.get()) _didChangeValueForKey:_WebEstimatedProgressKey];
}

void WebFrameLoaderClient::postProgressStartedNotification()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:getWebView(m_webFrame.get())];
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:getWebView(m_webFrame.get())];
}

void WebFrameLoaderClient::postProgressFinishedNotification()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:getWebView(m_webFrame.get())];
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready)
{
    [getWebView(m_webFrame.get()) setMainFrameDocumentReady:ready];
}

void WebFrameLoaderClient::startDownload(const ResourceRequest& request)
{
    // FIXME: Should download full request.
    [getWebView(m_webFrame.get()) _downloadURL:request.url().getNSURL()];
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

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    NSData *nsData = [[NSData alloc] initWithBytesNoCopy:(void*)data length:length freeWhenDone:NO];
    [dataSource(loader) _receivedData:nsData];
    [nsData release];
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    [dataSource(loader) _finishedLoading];
}

void WebFrameLoaderClient::finalSetupForReplace(DocumentLoader* loader)
{
    [dataSource(loader) _clearUnarchivingState];
}

// FIXME: <rdar://problem/4880065> - Push Global History into WebCore
// Once that task is complete, this will go away
void WebFrameLoaderClient::updateGlobalHistoryForStandardLoad(const KURL& url)
{
    NSURL *nsurl = url.getNSURL();
    WebHistoryItem *entry = [[WebHistory optionalSharedHistory] addItemForURL:nsurl];
    String pageTitle = core(m_webFrame.get())->loader()->documentLoader()->title();
    if (pageTitle.length())
        [entry setTitle:pageTitle];
}
 
// FIXME: <rdar://problem/4880065> - Push Global History into WebCore
// Once that task is complete, this will go away
void WebFrameLoaderClient::updateGlobalHistoryForReload(const KURL& url)
{
    WebHistory *sharedHistory = [WebHistory optionalSharedHistory];
    WebHistoryItem *item = [sharedHistory itemForURL:url.getNSURL()];
    if (item)
        [sharedHistory setLastVisitedTimeInterval:[NSDate timeIntervalSinceReferenceDate] forItem:item];
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem* item) const
{
    WebView* view = getWebView(m_webFrame.get());
    WebHistoryItem *webItem = kit(item);
    
    return [[view _policyDelegateForwarder] webView:view shouldGoToHistoryItem:webItem];
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:request.url().getNSURL()];
}
    
ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotUseRestrictedPort URL:request.url().getNSURL()];
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotShowURL URL:request.url().getNSURL()];
}

ResourceError WebFrameLoaderClient::interruptForPolicyChangeError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:request.url().getNSURL()];
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:WebKitErrorCannotShowMIMEType URL:response.url().getNSURL()];
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist URL:response.url().getNSURL()];    
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error)
{
    // FIXME: Needs to check domain.
    // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent
    // loading plugin content twice.  See <rdar://problem/4258008>
    return error.errorCode() != NSURLErrorCancelled && error.errorCode() != WebKitErrorPlugInWillHandleLoad;
}

void WebFrameLoaderClient::setDefersLoading(bool defers)
{
    if (!defers)
        deliverArchivedResourcesAfterDelay();
}

bool WebFrameLoaderClient::willUseArchive(ResourceLoader* loader, const ResourceRequest& request, const KURL& originalURL) const
{
    if (request.url() != originalURL)
        return false;
    if (!canUseArchivedResource(request.nsURLRequest()))
        return false;
    WebResource *resource = [dataSource(core(m_webFrame.get())->loader()->activeDocumentLoader()) _archivedSubresourceForURL:originalURL.getNSURL()];
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
    core(m_webFrame.get())->loader()->setPreviousHistoryItem(0);
}


void WebFrameLoaderClient::saveViewStateToItem(HistoryItem* item)
{
    if (!item)
        return;
    
    NSView <WebDocumentView> *docView = [m_webFrame->_private->webFrameView documentView];

    // we might already be detached when this is called from detachFromParent, in which
    // case we don't want to override real data earlier gathered with (0,0)
    if ([docView superview] && [docView conformsToProtocol:@protocol(_WebDocumentViewState)])
        item->setViewState([(id <_WebDocumentViewState>)docView viewState]);
}

void WebFrameLoaderClient::restoreViewState()
{
    HistoryItem* currentItem = core(m_webFrame.get())->loader()->currentHistoryItem();
    ASSERT(currentItem);

    // FIXME: As the ASSERT attests, it seems we should always have a currentItem here.
    // One counterexample is <rdar://problem/4917290>
    // For now, to cover this issue in release builds, there is no technical harm to returning
    // early and from a user standpoint - as in the above radar - the previous page load failed 
    // so there *is* no scroll state to restore!
    if (!currentItem)
        return;
    
    NSView <WebDocumentView> *docView = [m_webFrame->_private->webFrameView documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentViewState)]) {        
        id state = currentItem->viewState();
        if (state) {
            [(id <_WebDocumentViewState>)docView setViewState:state];
        }
    }
}

void WebFrameLoaderClient::provisionalLoadStarted()
{    
    // FIXME: This is OK as long as no one resizes the window,
    // but in the case where someone does, it means garbage outside
    // the occupied part of the scroll view.
    [[m_webFrame->_private->webFrameView _scrollView] setDrawsBackground:NO];
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
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebDocumentLoaderMac> loader = new WebDocumentLoaderMac(request, substituteData);

    WebDataSource *dataSource = [[WebDataSource alloc] _initWithDocumentLoader:loader.get()];
    loader->setDataSource(dataSource, getWebView(m_webFrame.get()));
    [dataSource release];

    return loader.release();
}

// FIXME: <rdar://problem/4880065> - Push Global History into WebCore
// Once that task is complete, this will go away
void WebFrameLoaderClient::setTitle(const String& title, const KURL& URL)
{
    NSURL* nsURL = canonicalURL(URL.getNSURL());
    if(!nsURL)
        return;
    NSString *titleNSString = title;
    [[[WebHistory optionalSharedHistory] itemForURL:nsURL] setTitle:titleNSString];
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
        loader->didReceiveData((const char*)[data bytes], [data length], [data length], true);
        [data release];
        loader->didFinishLoading();
    }
}

void WebFrameLoaderClient::saveDocumentViewToCachedPage(CachedPage* cachedPage)
{
    cachedPage->setDocumentView([m_webFrame->_private->webFrameView documentView]);
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

String WebFrameLoaderClient::userAgent(const KURL& url)
{
    return [getWebView(m_webFrame.get()) _userAgentForURL:url];
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

bool WebFrameLoaderClient::canCachePage() const
{
    // We can only cache HTML pages right now
    return [[[m_webFrame.get() dataSource] representation] isKindOfClass:[WebHTMLRepresentation class]];
}

Frame* WebFrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                         const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    WebFrameBridge* bridge = m_webFrame->_private->bridge;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    return [bridge createChildFrameNamed:name
                   withURL:url.getNSURL()
                   referrer:referrer 
                   ownerElement:ownerElement
                   allowsScrolling:allowsScrolling
                   marginWidth:marginWidth
                   marginHeight:marginHeight];

    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

ObjectContentType WebFrameLoaderClient::objectContentType(const KURL& url, const String& mimeType)
{
    WebFrameBridge* bridge = m_webFrame->_private->bridge;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return (ObjectContentType)[bridge determineObjectFromMIMEType:mimeType URL:url.getNSURL()];
    END_BLOCK_OBJC_EXCEPTIONS;
    return ObjectContentNone;
}

static NSArray* nsArray(const Vector<String>& vector)
{
    unsigned len = vector.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity:len];
    for (unsigned x = 0; x < len; x++)
        [array addObject:vector[x]];
    return array;
}

Widget* WebFrameLoaderClient::createPlugin(Element* element, const KURL& url, const Vector<String>& paramNames,
                                           const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    WebFrameBridge* bridge = m_webFrame->_private->bridge;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return new Widget([bridge viewForPluginWithURL:url.getNSURL()
                              attributeNames:nsArray(paramNames)
                              attributeValues:nsArray(paramValues)
                              MIMEType:mimeType
                              DOMElement:[DOMElement _wrapElement:element]
                              loadManually:loadManually]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_webFrame->_private->bridge redirectDataToPlugin:pluginWidget->getView()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

WebCore::Widget* WebFrameLoaderClient::createJavaAppletWidget(const IntSize& size, Element* element, const KURL& baseURL, 
                                                              const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    Widget* result = new Widget;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebFrameBridge* bridge = m_webFrame->_private->bridge;
    result->setView([bridge viewForJavaAppletWithFrame:NSMakeRect(0, 0, size.width(), size.height())
                            attributeNames:nsArray(paramNames)
                            attributeValues:nsArray(paramValues)
                            baseURL:baseURL.getNSURL()
                            DOMElement:[DOMElement _wrapElement:element]]);    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return result;
}

String WebFrameLoaderClient::overrideMediaType() const
{
    NSString* overrideType = [m_webFrame->_private->bridge overrideMediaType];
    if (overrideType)
        return overrideType;
    return String();
}

void WebFrameLoaderClient::windowObjectCleared() const
{
    [m_webFrame->_private->bridge windowObjectCleared];
}

@implementation WebFramePolicyListener

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

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
    ASSERT_MAIN_THREAD();
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
