/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "BackForwardList.h"
#import "DOMElementInternal.h"
#import "DOMHTMLFormElementInternal.h"
#import "WebBackForwardList.h"
#import "WebBasePluginPackage.h"
#import "WebCachedFramePlatformData.h"
#import "WebChromeClient.h"
#import "WebDataSourceInternal.h"
#import "WebDelegateImplementationCaching.h"
#import "WebDocumentInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebDownload.h"
#import "WebDynamicScrollBarsViewInternal.h"
#import "WebElementDictionary.h"
#import "WebFormDelegate.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegatePrivate.h"
#import "WebFrameNetworkingContext.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryDelegate.h"
#import "WebHistoryInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitVersionChecks.h"
#import "WebNSURLExtras.h"
#import "WebNavigationData.h"
#import "WebNetscapePluginPackage.h"
#import "WebNetscapePluginView.h"
#import "WebPanelAuthenticationHandler.h"
#import "WebPluginController.h"
#import "WebPluginPackage.h"
#import "WebPluginViewFactoryPrivate.h"
#import "WebPolicyDelegate.h"
#import "WebPolicyDelegatePrivate.h"
#import "WebPreferences.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourceLoadDelegatePrivate.h"
#import "WebScriptWorldInternal.h"
#import "WebSecurityOriginInternal.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <JavaScriptCore/JSContextInternal.h>
#import <WebCore/AuthenticationMac.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/CachedFrame.h>
#import <WebCore/Chrome.h>
#import <WebCore/DNS.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/FormState.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderStateMachine.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/FrameTree.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLAppletElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLFrameElement.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/HistoryController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/Page.h>
#import <WebCore/PluginBlacklist.h>
#import <WebCore/PluginViewBase.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubresourceLoader.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebGLBlacklist.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/Widget.h>
#import <WebKitLegacy/DOMElement.h>
#import <WebKitLegacy/DOMHTMLFormElement.h>
#import <pal/spi/cocoa/NSURLDownloadSPI.h>
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/MainThread.h>
#import <wtf/Ref.h>
#import <wtf/RunLoop.h>
#import <wtf/text/WTFString.h>

#if USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)
#import "NetscapePluginHostManager.h"
#import "WebHostedNetscapePluginView.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <WebCore/HTMLPlugInImageElement.h>
#import <WebCore/WAKClipView.h>
#import <WebCore/WAKScrollView.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#import "WebMailDelegate.h"
#import "WebUIKitDelegate.h"
#endif

#if USE(QUICK_LOOK)
#import <WebCore/PreviewLoaderClient.h>
#import <WebCore/QuickLook.h>
#import <pal/spi/cocoa/NSFileManagerSPI.h>
#endif

#if HAVE(APP_LINKS)
#import <WebCore/WebCoreThreadRun.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#endif

#if ENABLE(CONTENT_FILTERING)
#import <WebCore/PolicyChecker.h>
#endif

using namespace WebCore;
using namespace HTMLNames;

#if PLATFORM(IOS_FAMILY)
@interface WebHTMLView (Init)
- (id)initWithFrame:(CGRect)frame;
@end
#endif

// For backwards compatibility with older WebKit plug-ins.
NSString *WebPluginBaseURLKey = @"WebPluginBaseURL";
NSString *WebPluginAttributesKey = @"WebPluginAttributes";
NSString *WebPluginContainerKey = @"WebPluginContainer";

@interface WebFramePolicyListener : NSObject <WebPolicyDecisionListener, WebFormSubmissionListener> {
    RefPtr<Frame> _frame;
    FramePolicyFunction _policyFunction;
#if HAVE(APP_LINKS)
    RetainPtr<NSURL> _appLinkURL;
#endif
    PolicyAction _defaultPolicy;
}

- (id)initWithFrame:(Frame*)frame policyFunction:(FramePolicyFunction&&)policyFunction defaultPolicy:(PolicyAction)defaultPolicy;
#if HAVE(APP_LINKS)
- (id)initWithFrame:(Frame*)frame policyFunction:(FramePolicyFunction&&)policyFunction defaultPolicy:(PolicyAction)defaultPolicy appLinkURL:(NSURL *)url;
#endif

- (void)invalidate;

@end

static inline WebDataSource *dataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoaderMac*>(loader)->dataSource() : nil;
}

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame *webFrame)
    : m_webFrame(webFrame)
{
}

std::optional<uint64_t> WebFrameLoaderClient::pageID() const
{
    return std::nullopt;
}

std::optional<uint64_t> WebFrameLoaderClient::frameID() const
{
    return std::nullopt;
}

PAL::SessionID WebFrameLoaderClient::sessionID() const
{
    RELEASE_ASSERT_NOT_REACHED();
    return PAL::SessionID::defaultSessionID();
}

void WebFrameLoaderClient::frameLoaderDestroyed()
{
    [m_webFrame.get() _clearCoreFrame];
    delete this;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return [m_webFrame.get() webView] != nil;
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader* loader)
{
    [dataSource(loader) _makeRepresentation];
}

bool WebFrameLoaderClient::hasHTMLView() const
{
    NSView <WebDocumentView> *view = [m_webFrame->_private->webFrameView documentView];
    return [view isKindOfClass:[WebHTMLView class]];
}

#if PLATFORM(IOS_FAMILY)
bool WebFrameLoaderClient::forceLayoutOnRestoreFromPageCache()
{
    NSView <WebDocumentView> *view = [m_webFrame->_private->webFrameView documentView];
    // This gets called to lay out a page restored from the page cache.
    // To work around timing problems with UIKit, restore fixed 
    // layout settings here.
    WebView* webView = getWebView(m_webFrame.get());
    bool isMainFrame = [webView mainFrame] == m_webFrame.get();
    Frame* coreFrame = core(m_webFrame.get());
    if (isMainFrame && coreFrame->view()) {
        IntSize newSize([webView _fixedLayoutSize]);
        coreFrame->view()->setFixedLayoutSize(newSize);
        coreFrame->view()->setUseFixedLayout(!newSize.isEmpty());
    }
    [view setNeedsLayout:YES];
    [view layout];
    return true;
}
#endif

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    WebFrameView *thisView = m_webFrame->_private->webFrameView;
    NSView <WebDocumentView> *thisDocumentView = [thisView documentView];
    ASSERT(thisDocumentView != nil);
    
    // Tell the just loaded document to layout.  This may be necessary
    // for non-html content that needs a layout message.
    if (!([[m_webFrame.get() _dataSource] _isDocumentHTML])) {
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
    //remove any NetScape plugins that are children of this frame because they are about to be detached
    WebView *webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView removePluginInstanceViewsFor:(m_webFrame.get())];
#endif
    [m_webFrame->_private->webFrameView _setWebFrame:nil]; // needed for now to be compatible w/ old behavior

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didRemoveFrameFromHierarchyFunc)
        CallFrameLoadDelegate(implementations->didRemoveFrameFromHierarchyFunc, webView, @selector(webView:didRemoveFrameFromHierarchy:), m_webFrame.get());
}

void WebFrameLoaderClient::detachedFromParent3()
{
    [m_webFrame->_private->webFrameView release];
    m_webFrame->_private->webFrameView = nil;
}

void WebFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, PAL::SessionID, const ResourceRequest& request, const ResourceResponse& response)
{
    WebView *webView = getWebView(m_webFrame.get());
    SubresourceLoader* mainResourceLoader = documentLoader->mainResourceLoader();

    if (!mainResourceLoader) {
        // The resource has already been cached, or the conversion is being attmpted when not calling SubresourceLoader::didReceiveResponse().
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        WebDownload *webDownload = [[WebDownload alloc] initWithRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody) delegate:[webView downloadDelegate]];
        ALLOW_DEPRECATED_DECLARATIONS_END
        [webDownload autorelease];
        return;
    }

    ResourceHandle* handle = mainResourceLoader->handle();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [WebDownload _downloadWithLoadingConnection:handle->connection() request:request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody) response:response.nsURLResponse() delegate:[webView downloadDelegate] proxy:nil];
ALLOW_DEPRECATED_DECLARATIONS_END
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& response, int length)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);
#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidLoadResourceFromMemoryCacheFunc) {
        CallResourceLoadDelegateInWebThread(implementations->webThreadDidLoadResourceFromMemoryCacheFunc, webView, @selector(webThreadWebView:didLoadResourceFromMemoryCache:response:length:fromDataSource:), request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody), response.nsURLResponse(), length, dataSource(loader));
        return true;
    } 
#endif
    if (!implementations->didLoadResourceFromMemoryCacheFunc)
        return false;

    CallResourceLoadDelegate(implementations->didLoadResourceFromMemoryCacheFunc, webView, @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:), request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody), response.nsURLResponse(), length, dataSource(loader));
    return true;
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    RetainPtr<id> object;

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadIdentifierForRequestFunc) {
        object = CallResourceLoadDelegateInWebThread(implementations->webThreadIdentifierForRequestFunc, webView, @selector(webThreadWebView:identifierForInitialRequest:fromDataSource:), request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody), dataSource(loader));
    } else
#endif
    if (implementations->identifierForRequestFunc)
        object = CallResourceLoadDelegate(implementations->identifierForRequestFunc, webView, @selector(webView:identifierForInitialRequest:fromDataSource:), request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody), dataSource(loader));
    else
        object = adoptNS([[NSObject alloc] init]);

    [webView _addObject:object.get() forIdentifier:identifier];
}

void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (redirectResponse.isNull())
        static_cast<WebDocumentLoaderMac*>(loader)->increaseLoadCount(identifier);

    NSURLRequest *currentURLRequest = request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody);

#if PLATFORM(MAC)
    if (MacApplication::isAppleMail() && loader->substituteData().isValid()) {
        // Mail.app checks for this property to detect data / archive loads.
        [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)currentURLRequest];
    }
#endif

    NSURLRequest *newURLRequest = currentURLRequest;
#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadWillSendRequestFunc) {
        newURLRequest = (NSURLRequest *)CallResourceLoadDelegateInWebThread(implementations->webThreadWillSendRequestFunc, webView, @selector(webThreadWebView:resource:willSendRequest:redirectResponse:fromDataSource:), [webView _objectForIdentifier:identifier], currentURLRequest, redirectResponse.nsURLResponse(), dataSource(loader));
    } else
#endif
    if (implementations->willSendRequestFunc)
        newURLRequest = (NSURLRequest *)CallResourceLoadDelegate(implementations->willSendRequestFunc, webView, @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:), [webView _objectForIdentifier:identifier], currentURLRequest, redirectResponse.nsURLResponse(), dataSource(loader));

    if (newURLRequest != currentURLRequest)
        request.updateFromDelegatePreservingOldProperties(ResourceRequest(newURLRequest));
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader* loader, unsigned long identifier)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations->shouldUseCredentialStorageFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            return CallResourceLoadDelegateReturningBoolean(NO, implementations->shouldUseCredentialStorageFunc, webView, @selector(webView:resource:shouldUseCredentialStorageForDataSource:), resource, dataSource(loader));
    }

    return true;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    NSURLAuthenticationChallenge *webChallenge = mac(challenge);

    if (implementations->didReceiveAuthenticationChallengeFunc) {
        if (id resource = [webView _objectForIdentifier:identifier]) {
            CallResourceLoadDelegate(implementations->didReceiveAuthenticationChallengeFunc, webView, @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:), resource, webChallenge, dataSource(loader));
            return;
        }
    }

#if !PLATFORM(IOS_FAMILY)
    NSWindow *window = [webView hostWindow] ? [webView hostWindow] : [webView window];
    [[WebPanelAuthenticationHandler sharedHandler] startAuthentication:webChallenge window:window];
#endif
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader* loader, unsigned long identifier, const ProtectionSpace& protectionSpace)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);
    
    NSURLProtectionSpace *webProtectionSpace = protectionSpace.nsSpace();
    
    if (implementations->canAuthenticateAgainstProtectionSpaceFunc) {
        if (id resource = [webView _objectForIdentifier:identifier]) {
            return CallResourceLoadDelegateReturningBoolean(NO, implementations->canAuthenticateAgainstProtectionSpaceFunc, webView, @selector(webView:resource:canAuthenticateAgainstProtectionSpace:forDataSource:), resource, webProtectionSpace, dataSource(loader));
        }
    }

    // If our resource load delegate doesn't handle the question, then only send authentication
    // challenges for pre-iOS-3.0, pre-10.6 protection spaces.  This is the same as the default implementation
    // in CFNetwork.
    return (protectionSpace.authenticationScheme() < ProtectionSpaceAuthenticationSchemeClientCertificateRequested);
}
#endif

#if PLATFORM(IOS_FAMILY)
RetainPtr<CFDictionaryRef> WebFrameLoaderClient::connectionProperties(DocumentLoader* loader, unsigned long identifier)
{
    WebView *webView = getWebView(m_webFrame.get());
    id resource = [webView _objectForIdentifier:identifier];
    if (!resource)
        return nullptr;

    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);
    if (implementations->connectionPropertiesFunc)
        return (CFDictionaryRef)CallResourceLoadDelegate(implementations->connectionPropertiesFunc, webView, @selector(webView:connectionPropertiesForResource:dataSource:), resource, dataSource(loader));

    return nullptr;
}
#endif

bool WebFrameLoaderClient::shouldPaintBrokenImage(const URL& imageURL) const
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

    if (implementations->shouldPaintBrokenImageForURLFunc) {
        NSURL* url = imageURL;
        return CallResourceLoadDelegateReturningBoolean(YES, implementations->shouldPaintBrokenImageForURLFunc, webView, @selector(webView:shouldPaintBrokenImageForURL:), url);
    }
    return true;
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidReceiveResponseFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidReceiveResponseFunc, webView, @selector(webThreadWebView:resource:didReceiveResponse:fromDataSource:), resource, response.nsURLResponse(), dataSource(loader));
        
    } else
#endif
    if (implementations->didReceiveResponseFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didReceiveResponseFunc, webView, @selector(webView:resource:didReceiveResponse:fromDataSource:), resource, response.nsURLResponse(), dataSource(loader));
    }
}

void WebFrameLoaderClient::willCacheResponse(DocumentLoader* loader, unsigned long identifier, NSCachedURLResponse* response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadWillCacheResponseFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            return completionHandler(CallResourceLoadDelegateInWebThread(implementations->webThreadWillCacheResponseFunc, webView, @selector(webThreadWebView:resource:willCacheResponse:fromDataSource:), resource, response, dataSource(loader)));
        
    } else
#endif
    if (implementations->willCacheResponseFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            return completionHandler(CallResourceLoadDelegate(implementations->willCacheResponseFunc, webView, @selector(webView:resource:willCacheResponse:fromDataSource:), resource, response, dataSource(loader)));
    }

    completionHandler(response);
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int dataLength)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);
#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidReceiveContentLengthFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidReceiveContentLengthFunc, webView, @selector(webThreadWebView:resource:didReceiveContentLength:fromDataSource:), resource, (NSInteger)dataLength, dataSource(loader));
    } else
#endif
    if (implementations->didReceiveContentLengthFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didReceiveContentLengthFunc, webView, @selector(webView:resource:didReceiveContentLength:fromDataSource:), resource, (NSInteger)dataLength, dataSource(loader));
    }
}

#if ENABLE(DATA_DETECTION)
void WebFrameLoaderClient::dispatchDidFinishDataDetection(NSArray *)
{
}
#endif

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidFinishLoadingFromDataSourceFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidFinishLoadingFromDataSourceFunc, webView, @selector(webThreadWebView:resource:didFinishLoadingFromDataSource:), resource, dataSource(loader));
    } else
#endif

    if (implementations->didFinishLoadingFromDataSourceFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didFinishLoadingFromDataSourceFunc, webView, @selector(webView:resource:didFinishLoadingFromDataSource:), resource, dataSource(loader));
    }

    [webView _removeObjectForIdentifier:identifier];

    static_cast<WebDocumentLoaderMac*>(loader)->decreaseLoadCount(identifier);
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidFailLoadingWithErrorFromDataSourceFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidFailLoadingWithErrorFromDataSourceFunc, webView, @selector(webThreadWebView:resource:didFailLoadingWithError:fromDataSource:), resource, (NSError *)error, dataSource(loader));
    } else
#endif
    if (implementations->didFailLoadingWithErrorFromDataSourceFunc) {
        if (id resource = [webView _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didFailLoadingWithErrorFromDataSourceFunc, webView, @selector(webView:resource:didFailLoadingWithError:fromDataSource:), resource, (NSError *)error, dataSource(loader));
    }

    [webView _removeObjectForIdentifier:identifier];

    static_cast<WebDocumentLoaderMac*>(loader)->decreaseLoadCount(identifier);
}

void WebFrameLoaderClient::dispatchDidDispatchOnloadEvents()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didHandleOnloadEventsForFrameFunc)
        CallFrameLoadDelegate(implementations->didHandleOnloadEventsForFrameFunc, webView, @selector(webView:didHandleOnloadEventsForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    m_webFrame->_private->provisionalURL = core(m_webFrame.get())->loader().provisionalDocumentLoader()->url().string();

    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didReceiveServerRedirectForProvisionalLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didReceiveServerRedirectForProvisionalLoadForFrameFunc, webView, @selector(webView:didReceiveServerRedirectForProvisionalLoadForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didCancelClientRedirectForFrameFunc)
        CallFrameLoadDelegate(implementations->didCancelClientRedirectForFrameFunc, webView, @selector(webView:didCancelClientRedirectForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const URL& url, double delay, WallTime fireDate, LockBackForwardList)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->willPerformClientRedirectToURLDelayFireDateForFrameFunc) {
        NSURL *cocoaURL = url;
        CallFrameLoadDelegate(implementations->willPerformClientRedirectToURLDelayFireDateForFrameFunc, webView, @selector(webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:), cocoaURL, delay, [NSDate dateWithTimeIntervalSince1970:fireDate.secondsSinceEpoch().seconds()], m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string();

    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didChangeLocationWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didChangeLocationWithinPageForFrameFunc, webView, @selector(webView:didChangeLocationWithinPageForFrame:), m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
    [[webView _UIKitDelegateForwarder] webView:webView didChangeLocationWithinPageForFrame:m_webFrame.get()];
#endif
}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string();

    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didPushStateWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didPushStateWithinPageForFrameFunc, webView, @selector(webView:didPushStateWithinPageForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string();

    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didReplaceStateWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didReplaceStateWithinPageForFrameFunc, webView, @selector(webView:didReplaceStateWithinPageForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string();

    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didPopStateWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didPopStateWithinPageForFrameFunc, webView, @selector(webView:didPopStateWithinPageForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchWillClose()
{
    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->willCloseFrameFunc)
        CallFrameLoadDelegate(implementations->willCloseFrameFunc, webView, @selector(webView:willCloseFrame:), m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
    [[webView _UIKitDelegateForwarder] webView:webView willCloseFrame:m_webFrame.get()];
#endif
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    ASSERT(!m_webFrame->_private->provisionalURL);
    m_webFrame->_private->provisionalURL = core(m_webFrame.get())->loader().provisionalDocumentLoader()->url().string();

    WebView *webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView _didStartProvisionalLoadForFrame:m_webFrame.get()];
#endif

#if PLATFORM(IOS_FAMILY)
    [[webView _UIKitDelegateForwarder] webView:webView didStartProvisionalLoadForFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didStartProvisionalLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didStartProvisionalLoadForFrameFunc, webView, @selector(webView:didStartProvisionalLoadForFrame:), m_webFrame.get());
}

static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

void WebFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);

    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didReceiveTitleForFrameFunc) {
        // FIXME: Use direction of title.
        CallFrameLoadDelegate(implementations->didReceiveTitleForFrameFunc, webView, @selector(webView:didReceiveTitle:forFrame:), (NSString *)truncatedTitle.string, m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent>)
{
    // Tell the client we've committed this URL.
    ASSERT([m_webFrame->_private->webFrameView documentView] != nil);
    
    WebView *webView = getWebView(m_webFrame.get());   
    [webView _didCommitLoadForFrame:m_webFrame.get()];

    m_webFrame->_private->url = m_webFrame->_private->provisionalURL;
    m_webFrame->_private->provisionalURL = nullptr;

#if PLATFORM(IOS_FAMILY)
    [[webView _UIKitDelegateForwarder] webView:webView didCommitLoadForFrame:m_webFrame.get()];
#endif
    
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didCommitLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didCommitLoadForFrameFunc, webView, @selector(webView:didCommitLoadForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    m_webFrame->_private->provisionalURL = nullptr;

    WebView *webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView _didFailProvisionalLoadWithError:error forFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didFailProvisionalLoadWithErrorForFrameFunc)
        CallFrameLoadDelegate(implementations->didFailProvisionalLoadWithErrorForFrameFunc, webView, @selector(webView:didFailProvisionalLoadWithError:forFrame:), (NSError *)error, m_webFrame.get());

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    ASSERT(!m_webFrame->_private->provisionalURL);

    WebView *webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView _didFailLoadWithError:error forFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didFailLoadWithErrorForFrameFunc)
        CallFrameLoadDelegate(implementations->didFailLoadWithErrorForFrameFunc, webView, @selector(webView:didFailLoadWithError:forFrame:), (NSError *)error, m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
    [[webView _UIKitDelegateForwarder] webView:webView didFailLoadWithError:((NSError *)error) forFrame:m_webFrame.get()];
#endif

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebView *webView = getWebView(m_webFrame.get());

#if PLATFORM(IOS_FAMILY)
    id webThreadDel = [webView _webMailDelegate];
    if ([webThreadDel respondsToSelector:@selector(_webthread_webView:didFinishDocumentLoadForFrame:)]) {
        [webThreadDel _webthread_webView:webView didFinishDocumentLoadForFrame:m_webFrame.get()];
    }
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didFinishDocumentLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didFinishDocumentLoadForFrameFunc, webView, @selector(webView:didFinishDocumentLoadForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    ASSERT(!m_webFrame->_private->provisionalURL);

    WebView *webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView _didFinishLoadForFrame:m_webFrame.get()];
#else
    [[webView _UIKitDelegateForwarder] webView:webView didFinishLoadForFrame:m_webFrame.get()];

    id webThreadDel = [webView _webMailDelegate];
    if ([webThreadDel respondsToSelector:@selector(_webthread_webView:didFinishLoadForFrame:)]) {
        [webThreadDel _webthread_webView:webView didFinishLoadForFrame:m_webFrame.get()];
    }
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didFinishLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didFinishLoadForFrameFunc, webView, @selector(webView:didFinishLoadForFrame:), m_webFrame.get());

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:nil];
}

void WebFrameLoaderClient::dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> milestones)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidLayoutFunc)
        CallFrameLoadDelegateInWebThread(implementations->webThreadDidLayoutFunc, webView, @selector(webThreadWebView:didLayout:), kitLayoutMilestones(milestones));
#else
    if (implementations->didLayoutFunc)
        CallFrameLoadDelegate(implementations->didLayoutFunc, webView, @selector(webView:didLayout:), kitLayoutMilestones(milestones));
#endif

    if (milestones & DidFirstLayout) {
        // FIXME: We should consider removing the old didFirstLayout API since this is doing double duty with the
        // new didLayout API.
        if (implementations->didFirstLayoutInFrameFunc)
            CallFrameLoadDelegate(implementations->didFirstLayoutInFrameFunc, webView, @selector(webView:didFirstLayoutInFrame:), m_webFrame.get());

#if PLATFORM(IOS_FAMILY)
        [[webView _UIKitDelegateForwarder] webView:webView didFirstLayoutInFrame:m_webFrame.get()];
#endif
 
        // See WebFrameLoaderClient::provisionalLoadStarted.
        WebDynamicScrollBarsView *scrollView = [m_webFrame->_private->webFrameView _scrollView];
        if ([getWebView(m_webFrame.get()) drawsBackground])
            [scrollView setDrawsBackground:YES];
#if !PLATFORM(IOS_FAMILY)
        [scrollView setVerticalScrollElasticity:NSScrollElasticityAutomatic];
        [scrollView setHorizontalScrollElasticity:NSScrollElasticityAutomatic];
#endif
    }

    if (milestones & DidFirstVisuallyNonEmptyLayout) {
        // FIXME: We should consider removing the old didFirstVisuallyNonEmptyLayoutForFrame API since this is doing
        // double duty with the new didLayout API.
        if (implementations->didFirstVisuallyNonEmptyLayoutInFrameFunc)
            CallFrameLoadDelegate(implementations->didFirstVisuallyNonEmptyLayoutInFrameFunc, webView, @selector(webView:didFirstVisuallyNonEmptyLayoutInFrame:), m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
        if ([webView mainFrame] == m_webFrame.get())
            [[webView _UIKitDelegateForwarder] webView:webView didFirstVisuallyNonEmptyLayoutInFrame:m_webFrame.get()];
#endif
    }
}

Frame* WebFrameLoaderClient::dispatchCreatePage(const NavigationAction&)
{
    WebView *currentWebView = getWebView(m_webFrame.get());
    NSDictionary *features = [[NSDictionary alloc] init];
    WebView *newWebView = [[currentWebView _UIDelegateForwarder] webView:currentWebView 
                                                createWebViewWithRequest:nil
                                                          windowFeatures:features];
    [features release];
    
#if USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)
    if (newWebView)
        WebKit::NetscapePluginHostManager::singleton().didCreateWindow();
#endif
        
    return core([newWebView mainFrame]);
}

void WebFrameLoaderClient::dispatchShow()
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _UIDelegateForwarder] webViewShow:webView];
}

void WebFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest& request, FramePolicyFunction&& function)
{
    WebView *webView = getWebView(m_webFrame.get());

    [[webView _policyDelegateForwarder] webView:webView
                        decidePolicyForMIMEType:response.mimeType()
                                        request:request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody)
                                          frame:m_webFrame.get()
                               decisionListener:setUpPolicyListener(WTFMove(function), PolicyAction::Use).get()];
}


static BOOL shouldTryAppLink(WebView *webView, const NavigationAction& action, Frame* targetFrame)
{
#if HAVE(APP_LINKS)
    BOOL mainFrameNavigation = !targetFrame || targetFrame->isMainFrame();
    if (!mainFrameNavigation)
        return NO;

    if (!action.processingUserGesture())
        return NO;

    if (targetFrame && targetFrame->document() && hostsAreEqual(targetFrame->document()->url(), action.url()))
        return NO;

    return YES;
#else
    return NO;
#endif
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction& action, const ResourceRequest& request, FormState* formState, const String& frameName, FramePolicyFunction&& function)
{
    WebView *webView = getWebView(m_webFrame.get());
    BOOL tryAppLink = shouldTryAppLink(webView, action, nullptr);

    [[webView _policyDelegateForwarder] webView:webView
            decidePolicyForNewWindowAction:actionDictionary(action, formState)
                                   request:request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody)
                              newFrameName:frameName
                          decisionListener:setUpPolicyListener(WTFMove(function), PolicyAction::Ignore, tryAppLink ? (NSURL *)request.url() : nil).get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& action, const ResourceRequest& request, const ResourceResponse&, FormState* formState, PolicyDecisionMode, FramePolicyFunction&& function)
{
    WebView *webView = getWebView(m_webFrame.get());
    BOOL tryAppLink = shouldTryAppLink(webView, action, core(m_webFrame.get()));

    [[webView _policyDelegateForwarder] webView:webView
                decidePolicyForNavigationAction:actionDictionary(action, formState)
                                        request:request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody)
                                          frame:m_webFrame.get()
                               decisionListener:setUpPolicyListener(WTFMove(function), PolicyAction::Ignore, tryAppLink ? (NSURL *)request.url() : nil).get()];
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    if (!m_policyListener)
        return;

    [m_policyListener invalidate];
    m_policyListener = nil;
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError& error)
{
    WebView *webView = getWebView(m_webFrame.get());
    [[webView _policyDelegateForwarder] webView:webView unableToImplementPolicyWithError:error frame:m_webFrame.get()];    
}

static NSDictionary *makeFormFieldValuesDictionary(FormState& formState)
{
    auto& textFieldValues = formState.textFieldValues();
    size_t size = textFieldValues.size();
    NSMutableDictionary *dictionary = [[NSMutableDictionary alloc] initWithCapacity:size];
    for (auto& value : textFieldValues)
        [dictionary setObject:value.second forKey:value.first];
    return [dictionary autorelease];
}

void WebFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&& formState)
{
    id <WebFormDelegate> formDelegate = [getWebView(m_webFrame.get()) _formDelegate];
    if (!formDelegate)
        return;

    DOMHTMLFormElement *formElement = kit(&formState->form());
    NSDictionary *values = makeFormFieldValuesDictionary(formState.get());
    CallFormDelegate(getWebView(m_webFrame.get()), @selector(willSendSubmitEventToForm:inFrame:withValues:), formElement, m_webFrame.get(), values);
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FormState& formState, CompletionHandler<void()>&& completionHandler)
{
    id <WebFormDelegate> formDelegate = [getWebView(m_webFrame.get()) _formDelegate];
    if (!formDelegate) {
        completionHandler();
        return;
    }

    NSDictionary *values = makeFormFieldValuesDictionary(formState);
    CallFormDelegate(getWebView(m_webFrame.get()), @selector(frame:sourceFrame:willSubmitForm:withValues:submissionListener:), m_webFrame.get(), kit(formState.sourceDocument().frame()), kit(&formState.form()), values, setUpPolicyListener([completionHandler = WTFMove(completionHandler)](PolicyAction) mutable { completionHandler(); }, PolicyAction::Ignore).get());
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader* loader)
{
    [dataSource(loader) _revertToProvisionalState];
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader* loader, const ResourceError& error)
{
    [dataSource(loader) _setMainDocumentError:error];
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready)
{
    [getWebView(m_webFrame.get()) setMainFrameDocumentReady:ready];
}

void WebFrameLoaderClient::startDownload(const ResourceRequest& request, const String& /* suggestedName */)
{
    // FIXME: Should download full request.
    [getWebView(m_webFrame.get()) _downloadURL:request.url()];
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader* loader)
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Should do this only in main frame case, right?
    [getWebView(m_webFrame.get()) _willChangeValueForKey:_WebMainFrameTitleKey];
#endif
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader* loader)
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Should do this only in main frame case, right?
    [getWebView(m_webFrame.get()) _didChangeValueForKey:_WebMainFrameTitleKey];
#endif
}

void WebFrameLoaderClient::didReplaceMultipartContent()
{
#if PLATFORM(IOS_FAMILY)
    if (FrameView *view = core(m_webFrame.get())->view())
        view->didReplaceMultipartContent();
#endif
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

static inline NSString *nilOrNSString(const String& string)
{
    if (string.isNull())
        return nil;
    return string;
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    WebView* view = getWebView(m_webFrame.get());
    DocumentLoader* loader = core(m_webFrame.get())->loader().documentLoader();
#if PLATFORM(IOS_FAMILY)
    if (loader->urlForHistory() == blankURL())
        return;
#endif

    if ([view historyDelegate]) {
        WebHistoryDelegateImplementationCache* implementations = WebViewGetHistoryDelegateImplementations(view);
        if (implementations->navigatedFunc) {
            WebNavigationData *data = [[WebNavigationData alloc] initWithURLString:loader->url()
                                                                             title:nilOrNSString(loader->title().string)
                                                                   originalRequest:loader->originalRequestCopy().nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody)
                                                                          response:loader->response().nsURLResponse()
                                                                 hasSubstituteData:loader->substituteData().isValid()
                                                              clientRedirectSource:loader->clientRedirectSourceForHistory()];

            CallHistoryDelegate(implementations->navigatedFunc, view, @selector(webView:didNavigateWithNavigationData:inFrame:), data, m_webFrame.get());
            [data release];
        }
    
        return;
    }

    [[WebHistory optionalSharedHistory] _visitedURL:loader->urlForHistory() withTitle:loader->title().string method:loader->originalRequestCopy().httpMethod() wasFailure:loader->urlForHistoryReflectsFailure()];
}

static void addRedirectURL(WebHistoryItem *item, const String& url)
{
    if (!item->_private->_redirectURLs)
        item->_private->_redirectURLs = std::make_unique<Vector<String>>();

    // Our API allows us to store all the URLs in the redirect chain, but for
    // now we only have a use for the final URL.
    item->_private->_redirectURLs->resize(1);
    item->_private->_redirectURLs->at(0) = url;
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    WebView* view = getWebView(m_webFrame.get());
    WebHistoryDelegateImplementationCache* implementations = [view historyDelegate] ? WebViewGetHistoryDelegateImplementations(view) : 0;
    
    DocumentLoader* loader = core(m_webFrame.get())->loader().documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    if (!loader->clientRedirectSourceForHistory().isNull()) {
        if (implementations) {
            if (implementations->clientRedirectFunc) {
                CallHistoryDelegate(implementations->clientRedirectFunc, view, @selector(webView:didPerformClientRedirectFromURL:toURL:inFrame:), 
                    m_webFrame->_private->url.get(), loader->clientRedirectDestinationForHistory(), m_webFrame.get());
            }
        } else if (WebHistoryItem *item = [[WebHistory optionalSharedHistory] _itemForURLString:loader->clientRedirectSourceForHistory()])
            addRedirectURL(item, loader->clientRedirectDestinationForHistory());
    }

    if (!loader->serverRedirectSourceForHistory().isNull()) {
        if (implementations) {
            if (implementations->serverRedirectFunc) {
                CallHistoryDelegate(implementations->serverRedirectFunc, view, @selector(webView:didPerformServerRedirectFromURL:toURL:inFrame:), 
                    loader->serverRedirectSourceForHistory(), loader->serverRedirectDestinationForHistory(), m_webFrame.get());
            }
        } else if (WebHistoryItem *item = [[WebHistory optionalSharedHistory] _itemForURLString:loader->serverRedirectSourceForHistory()])
            addRedirectURL(item, loader->serverRedirectDestinationForHistory());
    }
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem& item) const
{
    WebView* view = getWebView(m_webFrame.get());
    return [[view _policyDelegateForwarder] webView:view shouldGoToHistoryItem:kit(&item)];
}

void WebFrameLoaderClient::didDisplayInsecureContent()
{
    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didDisplayInsecureContentFunc)
        CallFrameLoadDelegate(implementations->didDisplayInsecureContentFunc, webView, @selector(webViewDidDisplayInsecureContent:));
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin& origin, const URL& insecureURL)
{
    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didRunInsecureContentFunc) {
        RetainPtr<WebSecurityOrigin> webSecurityOrigin = adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:&origin]);
        CallFrameLoadDelegate(implementations->didRunInsecureContentFunc, webView, @selector(webView:didRunInsecureContent:), webSecurityOrigin.get());
    }
}

void WebFrameLoaderClient::didDetectXSS(const URL& insecureURL, bool didBlockEntirePage)
{
    WebView *webView = getWebView(m_webFrame.get());   
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);
    if (implementations->didDetectXSSFunc) {
        // FIXME: must pass didBlockEntirePage if we want to do more on mac than just pass tests.
        NSURL* insecureNSURL = insecureURL;
        CallFrameLoadDelegate(implementations->didDetectXSSFunc, webView, @selector(webView:didDetectXSS:), insecureNSURL);
    }
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:request.url()];
}
    
ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotUseRestrictedPort URL:request.url()];
}

ResourceError WebFrameLoaderClient::blockedByContentBlockerError(const ResourceRequest& request)
{
    RELEASE_ASSERT_NOT_REACHED(); // Content blockers are not enabled in WebKit1.
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotShowURL URL:request.url()];
}

ResourceError WebFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:request.url()];
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebFrameLoaderClient::blockedByContentFilterError(const ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadBlockedByContentFilter URL:request.url()];
}
#endif

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:WebKitErrorCannotShowMIMEType URL:response.url()];
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist URL:response.url()];    
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response)
{
    NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInWillHandleLoad
                                                    contentURL:response.url()
                                                 pluginPageURL:nil
                                                    pluginName:nil
                                                      MIMEType:response.mimeType()];
    return [error autorelease];
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error)
{
    // FIXME: Needs to check domain.
    // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent
    // loading plugin content twice.  See <rdar://problem/4258008>
    return error.errorCode() != NSURLErrorCancelled && error.errorCode() != WebKitErrorPlugInWillHandleLoad;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest& request) const
{
    return [WebView _canHandleRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody) forMainFrame:core(m_webFrame.get())->isMainFrame()];
}

bool WebFrameLoaderClient::canShowMIMEType(const String& MIMEType) const
{
    return [getWebView(m_webFrame.get()) _canShowMIMEType:MIMEType];
}

bool WebFrameLoaderClient::canShowMIMETypeAsHTML(const String& MIMEType) const
{
    return [WebView canShowMIMETypeAsHTML:MIMEType];
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

    // See WebFrameLoaderClient::provisionalLoadStarted.
    if ([getWebView(m_webFrame.get()) drawsBackground])
        [[m_webFrame->_private->webFrameView _scrollView] setDrawsBackground:YES];
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem& item)
{
#if PLATFORM(IOS_FAMILY)
    // Let UIKit handle the scroll point for the main frame.
    WebFrame *webFrame = m_webFrame.get();
    WebView *webView = getWebView(webFrame);   
    if (webFrame == [webView mainFrame]) {
        [[webView _UIKitDelegateForwarder] webView:webView saveStateToHistoryItem:kit(&item) forFrame:webFrame];
        return;
    }
#endif                    
    
    NSView <WebDocumentView> *docView = [m_webFrame->_private->webFrameView documentView];

    // we might already be detached when this is called from detachFromParent, in which
    // case we don't want to override real data earlier gathered with (0,0)
    if ([docView superview] && [docView conformsToProtocol:@protocol(_WebDocumentViewState)])
        item.setViewState([(id <_WebDocumentViewState>)docView viewState]);
}

void WebFrameLoaderClient::restoreViewState()
{
    HistoryItem* currentItem = core(m_webFrame.get())->loader().history().currentItem();
    ASSERT(currentItem);

    // FIXME: As the ASSERT attests, it seems we should always have a currentItem here.
    // One counterexample is <rdar://problem/4917290>
    // For now, to cover this issue in release builds, there is no technical harm to returning
    // early and from a user standpoint - as in the above radar - the previous page load failed 
    // so there *is* no scroll state to restore!
    if (!currentItem)
        return;

#if PLATFORM(IOS_FAMILY)
    // Let UIKit handle the scroll point for the main frame.
    WebFrame *webFrame = m_webFrame.get();
    WebView *webView = getWebView(webFrame);   
    if (webFrame == [webView mainFrame]) {
        [[webView _UIKitDelegateForwarder] webView:webView restoreStateFromHistoryItem:kit(currentItem) forFrame:webFrame force:NO];
        return;
    }
#endif                    
    
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
    // Tell the scroll view not to draw a background so we can leave the contents of
    // the old page showing during the beginning of the loading process.

    // This will stay set to NO until:
    //    1) The load gets far enough along: WebFrameLoader::frameLoadCompleted.
    //    2) The window is resized: -[WebFrameView setFrameSize:].
    // or 3) The view is moved out of the window: -[WebFrameView viewDidMoveToWindow].
    // Please keep the comments in these four functions in agreement with each other.

    WebDynamicScrollBarsView *scrollView = [m_webFrame->_private->webFrameView _scrollView];
    [scrollView setDrawsBackground:NO];
#if !PLATFORM(IOS_FAMILY)
    [scrollView setVerticalScrollElasticity:NSScrollElasticityNone];
    [scrollView setHorizontalScrollElasticity:NSScrollElasticityNone];
#endif
}

void WebFrameLoaderClient::didFinishLoad()
{
    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:nil];    
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    m_activeIconLoadCallbackID = 0;

    if (![m_webFrame.get() _dataSource]) {
        ASSERT(!core(m_webFrame.get())->tree().childCount());
        return;
    }
    
#if !PLATFORM(IOS_FAMILY)
    // Make sure that any work that is triggered by resigning first reponder can get done.
    // The main example where this came up is the textDidEndEditing that is sent to the
    // FormsDelegate (3223413). We need to do this before _detachChildren, since that will
    // remove the views as a side-effect of freeing the frame, at which point we can't
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
#endif
}

Ref<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    auto loader = WebDocumentLoaderMac::create(request, substituteData);

    WebDataSource *dataSource = [[WebDataSource alloc] _initWithDocumentLoader:loader.copyRef()];
    loader->setDataSource(dataSource, getWebView(m_webFrame.get()));
    [dataSource release];

    return WTFMove(loader);
}

void WebFrameLoaderClient::setTitle(const StringWithDirection& title, const URL& url)
{
    WebView* view = getWebView(m_webFrame.get());
    
    if ([view historyDelegate]) {
        WebHistoryDelegateImplementationCache* implementations = WebViewGetHistoryDelegateImplementations(view);
        // FIXME: Use direction of title.
        if (implementations->setTitleFunc)
            CallHistoryDelegate(implementations->setTitleFunc, view, @selector(webView:updateHistoryTitle:forURL:inFrame:), (NSString *)title.string, (NSString *)url, m_webFrame.get());
        else if (implementations->deprecatedSetTitleFunc) {
IGNORE_WARNINGS_BEGIN("undeclared-selector")
            CallHistoryDelegate(implementations->deprecatedSetTitleFunc, view, @selector(webView:updateHistoryTitle:forURL:), (NSString *)title.string, (NSString *)url);
IGNORE_WARNINGS_END
        }
        return;
    }

    NSURL* nsURL = url;
    nsURL = [nsURL _webkit_canonicalize];
    if(!nsURL)
        return;
#if PLATFORM(IOS_FAMILY)
    if ([[nsURL absoluteString] isEqualToString:@"about:blank"])
        return;
#endif
    [[[WebHistory optionalSharedHistory] itemForURL:nsURL] setTitle:title.string];
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame* cachedFrame)
{
    cachedFrame->setCachedFramePlatformData(std::make_unique<WebCachedFramePlatformData>(m_webFrame->_private->webFrameView.documentView));

#if PLATFORM(IOS_FAMILY)
    // At this point we know this frame is going to be cached. Stop all plugins.
    WebView *webView = getWebView(m_webFrame.get());
    [webView _stopAllPlugInsForPageCache];
#endif
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame* cachedFrame)
{
    WebCachedFramePlatformData* platformData = static_cast<WebCachedFramePlatformData*>(cachedFrame->cachedFramePlatformData());
    NSView <WebDocumentView> *cachedView = platformData->webDocumentView();
    ASSERT(cachedView != nil);
    ASSERT(cachedFrame->documentLoader());
    [cachedView setDataSource:dataSource(cachedFrame->documentLoader())];
    
#if !PLATFORM(IOS_FAMILY)
    // clean up webkit plugin instances before WebHTMLView gets freed.
    WebView *webView = getWebView(m_webFrame.get());
    [webView removePluginInstanceViewsFor:(m_webFrame.get())];
#endif
    
    [m_webFrame->_private->webFrameView _setDocumentView:cachedView];
}

#if PLATFORM(IOS_FAMILY)
void WebFrameLoaderClient::didRestoreFrameHierarchyForCachedFrame()
{
    // When entering the PageCache the Document is detached and the plugin view may
    // have cleaned itself up (removing its webview and layer references). Now, when
    // restoring the page we want to recreate whatever is necessary.
    WebView *webView = getWebView(m_webFrame.get());
    [webView _restorePlugInsFromCache];
}
#endif

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    WebDataSource *dataSource = [m_webFrame.get() _dataSource];

#if PLATFORM(IOS_FAMILY)
    bool willProduceHTMLView;
    // Fast path that skips initialization of objc class objects.
    if ([dataSource _documentLoader]->responseMIMEType() == "text/html")
        willProduceHTMLView = true;
    else
        willProduceHTMLView = [m_webFrame->_private->webFrameView _viewClassForMIMEType:[dataSource _responseMIMEType]] == [WebHTMLView class];
#else
    // FIXME (Viewless): I assume we want the equivalent of this optimization for viewless mode too.
    bool willProduceHTMLView = [m_webFrame->_private->webFrameView _viewClassForMIMEType:[dataSource _responseMIMEType]] == [WebHTMLView class];
#endif
    bool canSkipCreation = core(m_webFrame.get())->loader().stateMachine().committingFirstRealLoad() && willProduceHTMLView;
    if (canSkipCreation) {
        [[m_webFrame->_private->webFrameView documentView] setDataSource:dataSource];
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    // Don't suppress scrollbars before the view creation if we're making the view for a non-HTML view.
    if (!willProduceHTMLView)
        [[m_webFrame->_private->webFrameView _scrollView] setScrollBarsSuppressed:NO repaintOnUnsuppress:NO];
    
    // clean up webkit plugin instances before WebHTMLView gets freed.
    WebView *webView = getWebView(m_webFrame.get());
    [webView removePluginInstanceViewsFor:(m_webFrame.get())];
    
    NSView <WebDocumentView> *documentView = [m_webFrame->_private->webFrameView _makeDocumentViewForDataSource:dataSource];
#else
    NSView <WebDocumentView> *documentView = nil;

    // Fast path that skips initialization of objc class objects.
    if (willProduceHTMLView) {
        documentView = [[WebHTMLView alloc] initWithFrame:[m_webFrame->_private->webFrameView bounds]];
        [m_webFrame->_private->webFrameView _setDocumentView:documentView];
        [documentView release];
    } else
        documentView = [m_webFrame->_private->webFrameView _makeDocumentViewForDataSource:dataSource];
#endif
    if (!documentView)
        return;

    // FIXME: Could we skip some of this work for a top-level view that is not a WebHTMLView?

    // If we own the view, delete the old one - otherwise the render m_frame will take care of deleting the view.
    Frame* coreFrame = core(m_webFrame.get());
    Page* page = coreFrame->page();
    bool isMainFrame = coreFrame->isMainFrame();
    if (isMainFrame && coreFrame->view())
        coreFrame->view()->setParentVisible(false);
    coreFrame->setView(nullptr);
    RefPtr<FrameView> coreView = FrameView::create(*coreFrame);
    coreFrame->setView(coreView.copyRef());

    [m_webFrame.get() _updateBackgroundAndUpdatesWhileOffscreen];
    [m_webFrame->_private->webFrameView _install];

    if (isMainFrame) {
#if PLATFORM(IOS_FAMILY)
        coreView->setDelegatesScrolling(true);
#endif
        coreView->setParentVisible(true);
    }

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:dataSource];

    // The following is a no-op for WebHTMLRepresentation, but for custom document types
    // like the ones that Safari uses for bookmarks it is the only way the DocumentLoader
    // will get the proper title.
    if (auto* documentLoader = [dataSource _documentLoader])
        documentLoader->setTitle({ [dataSource pageTitle], TextDirection::LTR });

    if (auto* ownerElement = coreFrame->ownerElement())
        coreFrame->view()->setCanHaveScrollbars(ownerElement->scrollingMode() != ScrollbarAlwaysOff);

    // If the document view implicitly became first responder, make sure to set the focused frame properly.
    if ([[documentView window] firstResponder] == documentView) {
        page->focusController().setFocusedFrame(coreFrame);
        page->focusController().setFocused(true);
    }
}

void WebFrameLoaderClient::didSaveToPageCache()
{
}

void WebFrameLoaderClient::didRestoreFromPageCache()
{
#if PLATFORM(IOS_FAMILY)
    WebView *webView = getWebView(m_webFrame.get());
    if ([webView mainFrame] == m_webFrame.get())
        [[webView _UIKitDelegateForwarder] webViewDidRestoreFromPageCache:webView];
#endif
}

void WebFrameLoaderClient::dispatchDidBecomeFrameset(bool)
{
}

RetainPtr<WebFramePolicyListener> WebFrameLoaderClient::setUpPolicyListener(FramePolicyFunction&& function, PolicyAction defaultPolicy, NSURL *appLinkURL)
{
    // FIXME: <rdar://5634381> We need to support multiple active policy listeners.
    [m_policyListener invalidate];

    RetainPtr<WebFramePolicyListener> policyListener;
#if HAVE(APP_LINKS)
    if (appLinkURL)
        policyListener = adoptNS([[WebFramePolicyListener alloc] initWithFrame:core(m_webFrame.get()) policyFunction:WTFMove(function) defaultPolicy:defaultPolicy appLinkURL:appLinkURL]);
    else
#endif
        policyListener = adoptNS([[WebFramePolicyListener alloc] initWithFrame:core(m_webFrame.get()) policyFunction:WTFMove(function) defaultPolicy:defaultPolicy]);

    m_policyListener = policyListener.get();

    return policyListener;
}

String WebFrameLoaderClient::userAgent(const URL& url)
{
    WebView *webView = getWebView(m_webFrame.get());
    ASSERT(webView);

    // We should never get here with nil for the WebView unless there is a bug somewhere else.
    // But if we do, it's better to return the empty string than just crashing on the spot.
    // Most other call sites are tolerant of nil because of Objective-C behavior, but this one
    // is not because the return value of _userAgentForURL is a const URL&.
    if (!webView)
        return emptyString();

    return [webView _userAgentString];
}

NSDictionary *WebFrameLoaderClient::actionDictionary(const NavigationAction& action, FormState* formState) const
{
    unsigned modifierFlags = 0;
#if !PLATFORM(IOS_FAMILY)
    auto keyStateEventData = action.keyStateEventData();
    if (keyStateEventData && keyStateEventData->isTrusted) {
        if (keyStateEventData->ctrlKey)
            modifierFlags |= NSEventModifierFlagControl;
        if (keyStateEventData->altKey)
            modifierFlags |= NSEventModifierFlagOption;
        if (keyStateEventData->shiftKey)
            modifierFlags |= NSEventModifierFlagShift;
        if (keyStateEventData->metaKey)
            modifierFlags |= NSEventModifierFlagCommand;
    }
#else
    // No modifier flags on iOS right now
    modifierFlags = 0;
#endif

    NSURL *originalURL = action.url();

    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:static_cast<int>(action.type())], WebActionNavigationTypeKey,
        [NSNumber numberWithInt:modifierFlags], WebActionModifierFlagsKey,
        originalURL, WebActionOriginalURLKey,
        nil];

    if (auto mouseEventData = action.mouseEventData()) {
        WebElementDictionary *element = [[WebElementDictionary alloc]
            initWithHitTestResult:core(m_webFrame.get())->eventHandler().hitTestResultAtPoint(mouseEventData->absoluteLocation)];
        [result setObject:element forKey:WebActionElementKey];
        [element release];

        if (mouseEventData->isTrusted)
            [result setObject:[NSNumber numberWithInt:mouseEventData->button] forKey:WebActionButtonKey];
        else
            [result setObject:[NSNumber numberWithInt:WebCore::NoButton] forKey:WebActionButtonKey];
    }

    if (formState)
        [result setObject:kit(&formState->form()) forKey:WebActionFormKey];

    return result;
}

bool WebFrameLoaderClient::canCachePage() const
{
    // We can only cache HTML pages right now
    if (![[[m_webFrame _dataSource] representation] isKindOfClass:[WebHTMLRepresentation class]])
        return false;
    
    // We only cache pages if the back forward list is enabled and has a non-zero capacity.
    Page* page = core(m_webFrame.get())->page();
    if (!page)
        return false;
    
    BackForwardList& backForwardList = static_cast<BackForwardList&>(page->backForward().client());
    if (!backForwardList.enabled())
        return false;
    
    if (!backForwardList.capacity())
        return false;
    
    return true;
}

RefPtr<Frame> WebFrameLoaderClient::createFrame(const URL& url, const String& name, HTMLFrameOwnerElement& ownerElement,
    const String& referrer)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    ASSERT(m_webFrame);
    
    WebFrameView *childView = [[WebFrameView alloc] init];
    
    RefPtr<Frame> result = [WebFrame _createSubframeWithOwnerElement:&ownerElement frameName:name frameView:childView];
    [childView release];

    WebFrame *newFrame = kit(result.get());

    if ([newFrame _dataSource])
        [[newFrame _dataSource] _documentLoader]->setOverrideEncoding([[m_webFrame.get() _dataSource] _documentLoader]->overrideEncoding());  

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!result->page())
        return nullptr;
 
    core(m_webFrame.get())->loader().loadURLIntoChildFrame(url, referrer, result.get());

    // The frame's onload handler may have removed it from the document.
    if (!result->tree().parent())
        return nullptr;

    return result;

    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

ObjectContentType WebFrameLoaderClient::objectContentType(const URL& url, const String& mimeType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    String type = mimeType;

    if (type.isEmpty()) {
        // Try to guess the MIME type based off the extension.
        NSURL *URL = url;
        NSString *extension = [[URL path] pathExtension];
        if ([extension length] > 0) {
            type = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
            if (type.isEmpty()) {
                // If no MIME type is specified, use a plug-in if we have one that can handle the extension.
                if ([getWebView(m_webFrame.get()) _pluginForExtension:extension])
                    return ObjectContentType::PlugIn;
            }
        }
    }

    if (type.isEmpty())
        return ObjectContentType::Frame; // Go ahead and hope that we can display the content.

    ObjectContentType plugInType = ObjectContentType::None;
    if ([getWebView(m_webFrame.get()) _pluginForMIMEType:type])
        plugInType = ObjectContentType::PlugIn;

    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return ObjectContentType::Image;

    if (plugInType != ObjectContentType::None)
        return plugInType;

    if ([m_webFrame->_private->webFrameView _viewClassForMIMEType:type])
        return ObjectContentType::Frame;
    
    return ObjectContentType::None;

    END_BLOCK_OBJC_EXCEPTIONS;

    return ObjectContentType::None;
}

static NSMutableArray* kit(const Vector<String>& vector)
{
    unsigned len = vector.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity:len];
    for (unsigned x = 0; x < len; x++)
        [array addObject:vector[x]];
    return array;
}

static String parameterValue(const Vector<String>& paramNames, const Vector<String>& paramValues, const char* name)
{
    size_t size = paramNames.size();
    ASSERT(size == paramValues.size());
    for (size_t i = 0; i < size; ++i) {
        if (equalIgnoringASCIICase(paramNames[i], name))
            return paramValues[i];
    }
    return String();
}

static NSView *pluginView(WebFrame *frame, WebPluginPackage *pluginPackage,
    NSArray *attributeNames, NSArray *attributeValues, NSURL *baseURL,
    DOMElement *element, BOOL loadManually)
{
    WebHTMLView *docView = (WebHTMLView *)[[frame frameView] documentView];
    ASSERT([docView isKindOfClass:[WebHTMLView class]]);
        
    WebPluginController *pluginController = [docView _pluginController];
    
    // Store attributes in a dictionary so they can be passed to WebPlugins.
    NSMutableDictionary *attributes = [[NSMutableDictionary alloc] initWithObjects:attributeValues forKeys:attributeNames];
    
    [pluginPackage load];
    Class viewFactory = [pluginPackage viewFactory];
    
    NSView *view = nil;
    NSDictionary *arguments = nil;
    
    if ([viewFactory respondsToSelector:@selector(plugInViewWithArguments:)]) {
        arguments = [NSDictionary dictionaryWithObjectsAndKeys:
            baseURL, WebPlugInBaseURLKey,
            attributes, WebPlugInAttributesKey,
            pluginController, WebPlugInContainerKey,
            [NSNumber numberWithInt:loadManually ? WebPlugInModeFull : WebPlugInModeEmbed], WebPlugInModeKey,
            [NSNumber numberWithBool:!loadManually], WebPlugInShouldLoadMainResourceKey,
            element, WebPlugInContainingElementKey,
            nil];
        LOG(Plugins, "arguments:\n%@", arguments);
IGNORE_WARNINGS_BEGIN("undeclared-selector")
    } else if ([viewFactory respondsToSelector:@selector(pluginViewWithArguments:)]) {
IGNORE_WARNINGS_END
        arguments = [NSDictionary dictionaryWithObjectsAndKeys:
            baseURL, WebPluginBaseURLKey,
            attributes, WebPluginAttributesKey,
            pluginController, WebPluginContainerKey,
            element, WebPlugInContainingElementKey,
            nil];
        LOG(Plugins, "arguments:\n%@", arguments);
    }

    view = [pluginController plugInViewWithArguments:arguments fromPluginPackage:pluginPackage];
    [attributes release];

    return view;
}

class PluginWidget : public PluginViewBase {
public:
    PluginWidget(NSView *view = 0)
        : PluginViewBase(view)
    {
    }
    
private:
    virtual void invalidateRect(const IntRect& rect)
    {
        [platformWidget() setNeedsDisplayInRect:rect];
    }
};

#if PLATFORM(IOS_FAMILY)
@interface WAKView (UIKitSecretsWebKitKnowsAboutSeeUIWebPlugInView)
- (PlatformLayer *)pluginLayer;
- (BOOL)willProvidePluginLayer;
- (void)attachPluginLayer;
- (void)detachPluginLayer;
@end

class PluginWidgetIOS : public PluginWidget {
public:
    PluginWidgetIOS(WAKView *view)
        : PluginWidget(view)
    {
    }

    virtual PlatformLayer* platformLayer() const
    {
        if (![platformWidget() respondsToSelector:@selector(pluginLayer)])
            return nullptr;

        return [platformWidget() pluginLayer];   
    }

    virtual bool willProvidePluginLayer() const
    {
        return [platformWidget() respondsToSelector:@selector(willProvidePluginLayer)]
            && [platformWidget() willProvidePluginLayer];
    }

    virtual void attachPluginLayer()
    {
        if ([platformWidget() respondsToSelector:@selector(attachPluginLayer)])
            [platformWidget() attachPluginLayer];
    }

    virtual void detachPluginLayer()
    {
        if ([platformWidget() respondsToSelector:@selector(detachPluginLayer)])
            [platformWidget() detachPluginLayer];
    }
};
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(NETSCAPE_PLUGIN_API)

class NetscapePluginWidget : public PluginWidget {
public:
    NetscapePluginWidget(WebBaseNetscapePluginView *view)
        : PluginWidget(view)
    {
    }

    virtual PlatformLayer* platformLayer() const
    {
        return [(WebBaseNetscapePluginView *)platformWidget() pluginLayer];
    }

    virtual bool getFormValue(String& value)
    {
        NSString* nsValue = 0;
        if ([(WebBaseNetscapePluginView *)platformWidget() getFormValue:&nsValue]) {
            if (!nsValue)
                return false;
            value = String(nsValue);
            [nsValue release];
            return true;
        }
        return false;
    }

    virtual void handleEvent(Event& event)
    {
        Frame* frame = Frame::frameForWidget(*this);
        if (!frame)
            return;
        
        NSEvent* currentNSEvent = frame->eventHandler().currentNSEvent();
        if (event.type() == eventNames().mousemoveEvent)
            [(WebBaseNetscapePluginView *)platformWidget() handleMouseMoved:currentNSEvent];
        else if (event.type() == eventNames().mouseoverEvent)
            [(WebBaseNetscapePluginView *)platformWidget() handleMouseEntered:currentNSEvent];
        else if (event.type() == eventNames().mouseoutEvent)
            [(WebBaseNetscapePluginView *)platformWidget() handleMouseExited:currentNSEvent];
        else if (event.type() == eventNames().contextmenuEvent)
            event.setDefaultHandled(); // We don't know if the plug-in has handled mousedown event by displaying a context menu, so we never want WebKit to show a default one.
    }

    virtual void clipRectChanged()
    {
        // Changing the clip rect doesn't affect the view hierarchy, so the plugin must be told about the change directly.
        [(WebBaseNetscapePluginView *)platformWidget() updateAndSetWindow];
    }

private:
    virtual void notifyWidget(WidgetNotification notification)
    {
        switch (notification) {
        case WillPaintFlattened: {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            [(WebBaseNetscapePluginView *)platformWidget() cacheSnapshot];
            END_BLOCK_OBJC_EXCEPTIONS;
            break;
        }
        case DidPaintFlattened: {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            [(WebBaseNetscapePluginView *)platformWidget() clearCachedSnapshot];
            END_BLOCK_OBJC_EXCEPTIONS;
            break;
        }
        }
    }
};

#if USE(PLUGIN_HOST_PROCESS)
#define NETSCAPE_PLUGIN_VIEW WebHostedNetscapePluginView
#else
#define NETSCAPE_PLUGIN_VIEW WebNetscapePluginView
#endif

#endif // ENABLE(NETSCAPE_PLUGIN_API)

static bool shouldBlockPlugin(WebBasePluginPackage *pluginPackage)
{
#if PLATFORM(MAC)
    auto loadPolicy = PluginBlacklist::loadPolicyForPluginVersion(pluginPackage.bundleIdentifier, pluginPackage.bundleVersion);
    return loadPolicy == PluginBlacklist::LoadPolicy::BlockedForSecurity || loadPolicy == PluginBlacklist::LoadPolicy::BlockedForCompatibility;
#else
    UNUSED_PARAM(pluginPackage);
    return false;
#endif
}

RefPtr<Widget> WebFrameLoaderClient::createPlugin(const IntSize& size, HTMLPlugInElement& element, const URL& url,
    const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    ASSERT(paramNames.size() == paramValues.size());

    int errorCode = 0;

    WebView *webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    SEL selector = @selector(webView:plugInViewWithArguments:);
#endif

    Document* document = core(m_webFrame.get())->document();
    NSURL *baseURL = document->baseURL();
    NSURL *pluginURL = url;
    
    // <rdar://problem/8366089>: AppleConnect has a bug where it does not
    // understand the parameter names specified in the <object> element that
    // embeds its plug-in. This site-specific hack works around the issue by
    // converting the parameter names to lowercase before passing them to the
    // plug-in.
#if !PLATFORM(IOS_FAMILY)
    Frame* frame = core(m_webFrame.get());
#endif
    NSMutableArray *attributeKeys = kit(paramNames);
#if !PLATFORM(IOS_FAMILY)
    if (frame && frame->settings().needsSiteSpecificQuirks() && equalLettersIgnoringASCIICase(mimeType, "application/x-snkp")) {
        for (NSUInteger i = 0; i < [attributeKeys count]; ++i)
            [attributeKeys replaceObjectAtIndex:i withObject:[[attributeKeys objectAtIndex:i] lowercaseString]];
    }
    
    if ([[webView UIDelegate] respondsToSelector:selector]) {
        NSMutableDictionary *attributes = [[NSMutableDictionary alloc] initWithObjects:kit(paramValues) forKeys:attributeKeys];
        NSDictionary *arguments = [[NSDictionary alloc] initWithObjectsAndKeys:
            attributes, WebPlugInAttributesKey,
            [NSNumber numberWithInt:loadManually ? WebPlugInModeFull : WebPlugInModeEmbed], WebPlugInModeKey,
            [NSNumber numberWithBool:!loadManually], WebPlugInShouldLoadMainResourceKey,
            kit(&element), WebPlugInContainingElementKey,
            // FIXME: We should be passing base URL, see <https://bugs.webkit.org/show_bug.cgi?id=35215>.
            pluginURL, WebPlugInBaseURLKey, // pluginURL might be nil, so add it last
            nil];

        NSView *view = CallUIDelegate(webView, selector, arguments);

        [attributes release];
        [arguments release];

        if (view)
            return adoptRef(new PluginWidget(view));
    }
#endif

    NSString *MIMEType;
    WebBasePluginPackage *pluginPackage;
    if (mimeType.isEmpty()) {
        MIMEType = nil;
        pluginPackage = nil;
    } else {
        MIMEType = mimeType;
        pluginPackage = [webView _pluginForMIMEType:mimeType];
    }
    
    NSString *extension = [[pluginURL path] pathExtension];

#if PLATFORM(IOS_FAMILY)
    if (!pluginPackage && [extension length])
#else
    if (!pluginPackage && [extension length] && ![MIMEType length])
#endif
    {
        pluginPackage = [webView _pluginForExtension:extension];
        if (pluginPackage) {
            NSString *newMIMEType = [pluginPackage MIMETypeForExtension:extension];
            if ([newMIMEType length] != 0)
                MIMEType = newMIMEType;
        }
    }

    NSView *view = nil;

    if (pluginPackage) {
        if (shouldBlockPlugin(pluginPackage)) {
            errorCode = WebKitErrorBlockedPlugInVersion;
            if (is<RenderEmbeddedObject>(element.renderer()))
                downcast<RenderEmbeddedObject>(*element.renderer()).setPluginUnavailabilityReason(RenderEmbeddedObject::InsecurePluginVersion);
        } else {
            if ([pluginPackage isKindOfClass:[WebPluginPackage class]])
                view = pluginView(m_webFrame.get(), (WebPluginPackage *)pluginPackage, attributeKeys, kit(paramValues), baseURL, kit(&element), loadManually);
#if ENABLE(NETSCAPE_PLUGIN_API)
            else if ([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]) {
                WebBaseNetscapePluginView *pluginView = [[[NETSCAPE_PLUGIN_VIEW alloc]
                    initWithFrame:NSMakeRect(0, 0, size.width(), size.height())
                    pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                    URL:pluginURL
                    baseURL:baseURL
                    MIMEType:MIMEType
                    attributeKeys:attributeKeys
                    attributeValues:kit(paramValues)
                    loadManually:loadManually
                    element:&element] autorelease];

                return adoptRef(new NetscapePluginWidget(pluginView));
            }
#endif
        }
    } else
        errorCode = WebKitErrorCannotFindPlugIn;

    if (!errorCode && !view)
        errorCode = WebKitErrorCannotLoadPlugIn;
    
    if (errorCode && m_webFrame) {
        WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView);
        if (implementations->plugInFailedWithErrorFunc) {
            URL pluginPageURL = document->completeURL(stripLeadingAndTrailingHTMLSpaces(parameterValue(paramNames, paramValues, "pluginspage")));
            if (!pluginPageURL.protocolIsInHTTPFamily())
                pluginPageURL = URL();
            NSString *pluginName = pluginPackage ? (NSString *)[pluginPackage pluginInfo].name : nil;

            NSError *error = [[NSError alloc] _initWithPluginErrorCode:errorCode
                                                            contentURL:pluginURL pluginPageURL:pluginPageURL pluginName:pluginName MIMEType:MIMEType];
            CallResourceLoadDelegate(implementations->plugInFailedWithErrorFunc, [m_webFrame.get() webView],
                                     @selector(webView:plugInFailedWithError:dataSource:), error, [m_webFrame.get() _dataSource]);
            [error release];
        }

        return nullptr;
    }
    
    ASSERT(view);
#if PLATFORM(IOS_FAMILY)
    return adoptRef(new PluginWidgetIOS(view));
#else
    return adoptRef(new PluginWidget(view));
#endif

    END_BLOCK_OBJC_EXCEPTIONS;

    return nullptr;
}

void WebFrameLoaderClient::recreatePlugin(Widget*)
{
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget& pluginWidget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebHTMLRepresentation *representation = (WebHTMLRepresentation *)[[m_webFrame.get() _dataSource] representation];

    NSView *pluginView = pluginWidget.platformWidget();

#if ENABLE(NETSCAPE_PLUGIN_API)
    if ([pluginView isKindOfClass:[NETSCAPE_PLUGIN_VIEW class]])
        [representation _redirectDataToManualLoader:(NETSCAPE_PLUGIN_VIEW *)pluginView forPluginView:pluginView];
    else
#endif
    {
        WebHTMLView *documentView = (WebHTMLView *)[[m_webFrame.get() frameView] documentView];
        ASSERT([documentView isKindOfClass:[WebHTMLView class]]);
        [representation _redirectDataToManualLoader:[documentView _pluginController] forPluginView:pluginView];
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}
    
RefPtr<Widget> WebFrameLoaderClient::createJavaAppletWidget(const IntSize& size, HTMLAppletElement& element, const URL& baseURL,
    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSView *view = nil;

    NSString *MIMEType = @"application/x-java-applet";
    
    WebView *webView = getWebView(m_webFrame.get());

    WebBasePluginPackage *pluginPackage = [webView _pluginForMIMEType:MIMEType];

    int errorCode = WebKitErrorJavaUnavailable;

    if (pluginPackage) {
        if (shouldBlockPlugin(pluginPackage)) {
            errorCode = WebKitErrorBlockedPlugInVersion;
            if (is<RenderEmbeddedObject>(element.renderer()))
                downcast<RenderEmbeddedObject>(*element.renderer()).setPluginUnavailabilityReason(RenderEmbeddedObject::InsecurePluginVersion);
        } else {
#if ENABLE(NETSCAPE_PLUGIN_API)
            if ([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]) {
                view = [[[NETSCAPE_PLUGIN_VIEW alloc] initWithFrame:NSMakeRect(0, 0, size.width(), size.height())
                    pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                    URL:nil
                    baseURL:baseURL
                    MIMEType:MIMEType
                    attributeKeys:kit(paramNames)
                    attributeValues:kit(paramValues)
                    loadManually:NO
                    element:&element] autorelease];
                if (view)
                    return adoptRef(new NetscapePluginWidget(static_cast<WebBaseNetscapePluginView *>(view)));
            }
#endif
        }
    }

    if (!view) {
        WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(getWebView(m_webFrame.get()));
        if (implementations->plugInFailedWithErrorFunc) {
            NSString *pluginName = pluginPackage ? (NSString *)[pluginPackage pluginInfo].name : nil;
            NSError *error = [[NSError alloc] _initWithPluginErrorCode:errorCode contentURL:nil pluginPageURL:nil pluginName:pluginName MIMEType:MIMEType];
            CallResourceLoadDelegate(implementations->plugInFailedWithErrorFunc, [m_webFrame.get() webView],
                                     @selector(webView:plugInFailedWithError:dataSource:), error, [m_webFrame.get() _dataSource]);
            [error release];
        }
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

String WebFrameLoaderClient::overrideMediaType() const
{
    NSString* overrideType = [getWebView(m_webFrame.get()) mediaStyle];
    if (overrideType)
        return overrideType;
    return String();
}

#if ENABLE(WEBGL)
static bool shouldBlockWebGL()
{
#if PLATFORM(MAC)
    return WebGLBlacklist::shouldBlockWebGL();
#else
    return false;
#endif
}

WebCore::WebGLLoadPolicy WebFrameLoaderClient::webGLPolicyForURL(const URL&) const
{
    return shouldBlockWebGL() ? WebGLBlockCreation : WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebFrameLoaderClient::resolveWebGLPolicyForURL(const URL&) const
{
    return shouldBlockWebGL() ? WebGLBlockCreation : WebGLAllowCreation;
}
#endif // ENABLE(WEBGL)

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    WebView *webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView);

    if (implementations->didClearWindowObjectForFrameInScriptWorldFunc) {
        CallFrameLoadDelegate(implementations->didClearWindowObjectForFrameInScriptWorldFunc,
            webView, @selector(webView:didClearWindowObjectForFrame:inScriptWorld:), m_webFrame.get(), [WebScriptWorld findOrCreateWorld:world]);
        return;
    }

    if (&world != &mainThreadNormalWorld())
        return;

    Frame *frame = core(m_webFrame.get());
    ScriptController& script = frame->script();

#if JSC_OBJC_API_ENABLED
    if (implementations->didCreateJavaScriptContextForFrameFunc) {
        CallFrameLoadDelegate(implementations->didCreateJavaScriptContextForFrameFunc, webView, @selector(webView:didCreateJavaScriptContext:forFrame:),
            script.javaScriptContext(), m_webFrame.get());
    } else
#endif
    if (implementations->didClearWindowObjectForFrameFunc) {
        CallFrameLoadDelegate(implementations->didClearWindowObjectForFrameFunc, webView, @selector(webView:didClearWindowObject:forFrame:),
            script.windowScriptObject(), m_webFrame.get());
    } else if (implementations->windowScriptObjectAvailableFunc) {
        CallFrameLoadDelegate(implementations->windowScriptObjectAvailableFunc, webView, @selector(webView:windowScriptObjectAvailable:),
            script.windowScriptObject());
    }

    if ([webView scriptDebugDelegate]) {
        [m_webFrame.get() _detachScriptDebugger];
        [m_webFrame.get() _attachScriptDebugger];
    }
}

Ref<FrameNetworkingContext> WebFrameLoaderClient::createNetworkingContext()
{
    return WebFrameNetworkingContext::create(core(m_webFrame.get()));
}

#if PLATFORM(IOS_FAMILY)
bool WebFrameLoaderClient::shouldLoadMediaElementURL(const URL& url) const 
{
    WebView *webView = getWebView(m_webFrame.get());
    
    if (id policyDelegate = [webView policyDelegate]) {
        if ([policyDelegate respondsToSelector:@selector(webView:shouldLoadMediaURL:inFrame:)])
            return [policyDelegate webView:webView shouldLoadMediaURL:url inFrame:m_webFrame.get()];
    }
    return true;
}
#endif

#if USE(QUICK_LOOK)
RefPtr<PreviewLoaderClient> WebFrameLoaderClient::createPreviewLoaderClient(const String& fileName, const String& uti)
{
    class QuickLookDocumentWriter : public WebCore::PreviewLoaderClient {
    public:
        explicit QuickLookDocumentWriter(NSString *filePath)
            : m_filePath { filePath }
            , m_fileHandle { [NSFileHandle fileHandleForWritingAtPath:filePath] }
        {
            ASSERT(filePath.length);
        }

        ~QuickLookDocumentWriter()
        {
            [[NSFileManager defaultManager] _web_removeFileOnlyAtPath:m_filePath.get()];
        }

    private:
        RetainPtr<NSString> m_filePath;
        RetainPtr<NSFileHandle> m_fileHandle;

        void didReceiveDataArray(CFArrayRef dataArray) override
        {
            for (NSData *data in (NSArray *)dataArray)
                [m_fileHandle writeData:data];
        }

        void didFinishLoading() override
        {
            [m_fileHandle closeFile];
        }

        void didFail() override
        {
            [m_fileHandle closeFile];
        }
    };

    if (![m_webFrame webView].preferences.quickLookDocumentSavingEnabled)
        return nullptr;

    NSString *filePath = createTemporaryFileForQuickLook(fileName);
    if (!filePath)
        return nullptr;

    [m_webFrame provisionalDataSource]._quickLookContent = @{ WebQuickLookFileNameKey : filePath, WebQuickLookUTIKey : uti };
    return adoptRef(*new QuickLookDocumentWriter(filePath));
}
#endif

#if ENABLE(CONTENT_FILTERING)
void WebFrameLoaderClient::contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler unblockHandler)
{
    core(m_webFrame.get())->loader().policyChecker().setContentFilterUnblockHandler(WTFMove(unblockHandler));
}
#endif

void WebFrameLoaderClient::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

void WebFrameLoaderClient::getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>& icons)
{
    auto* frame = core(m_webFrame.get());
    DocumentLoader* documentLoader = frame->loader().documentLoader();
    ASSERT(documentLoader);

#if PLATFORM(MAC)
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_DEFAULT_ICON_LOADING)) {
        for (auto& icon : icons)
            documentLoader->didGetLoadDecisionForIcon(false, icon.second, 0);

        return;
    }
#endif

    bool disallowedDueToImageLoadSettings = false;
    if (!frame->settings().loadsImagesAutomatically() && !frame->settings().loadsSiteIconsIgnoringImageLoadingSetting())
        disallowedDueToImageLoadSettings = true;

    if (disallowedDueToImageLoadSettings || !frame->isMainFrame() || !documentLoader->url().protocolIsInHTTPFamily() || ![WebView _isIconLoadingEnabled]) {
        for (auto& icon : icons)
            documentLoader->didGetLoadDecisionForIcon(false, icon.second, 0);

        return;
    }

    ASSERT(!m_activeIconLoadCallbackID);

#if !PLATFORM(IOS_FAMILY)
    // WebKit 1, which only supports one icon per page URL, traditionally has preferred the last icon in case of multiple icons listed.
    // To preserve that behavior we walk the list backwards.
    for (auto icon = icons.rbegin(); icon != icons.rend(); ++icon) {
        if (icon->first.type != LinkIconType::Favicon || m_activeIconLoadCallbackID) {
            documentLoader->didGetLoadDecisionForIcon(false, icon->second, 0);
            continue;
        }

        m_activeIconLoadCallbackID = 1;
        documentLoader->didGetLoadDecisionForIcon(true, icon->second, m_activeIconLoadCallbackID);
    }
#else
    // No WebCore icon loading on iOS
    for (auto& icon : icons)
        documentLoader->didGetLoadDecisionForIcon(false, icon.second, 0);
#endif
}

#if !PLATFORM(IOS_FAMILY)
static NSImage *webGetNSImage(Image* image, NSSize size)
{
    ASSERT(size.width);
    ASSERT(size.height);

    // FIXME: We're doing the resize here for now because WebCore::Image doesn't yet support resizing/multiple representations
    // This makes it so there's effectively only one size of a particular icon in the system at a time. We should move this
    // to WebCore::Image at some point.
    if (!image)
        return nil;
    NSImage* nsImage = image->nsImage();
    if (!nsImage)
        return nil;
    if (!NSEqualSizes([nsImage size], size)) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [nsImage setScalesWhenResized:YES];
        ALLOW_DEPRECATED_DECLARATIONS_END
        [nsImage setSize:size];
    }
    return nsImage;
}
#endif // !PLATFORM(IOS_FAMILY)

void WebFrameLoaderClient::finishedLoadingIcon(uint64_t callbackID, SharedBuffer* iconData)
{
#if !PLATFORM(IOS_FAMILY)
    ASSERT(m_activeIconLoadCallbackID);
    ASSERT(callbackID = m_activeIconLoadCallbackID);
    m_activeIconLoadCallbackID = 0;

    WebView *webView = getWebView(m_webFrame.get());
    if (!webView)
        return;

    auto image = BitmapImage::create();
    if (image->setData(iconData, true) < EncodedDataStatus::SizeAvailable)
        return;

    NSImage *icon = webGetNSImage(image.ptr(), NSMakeSize(16, 16));
    if (icon)
        [webView _setMainFrameIcon:icon];
#else
    UNUSED_PARAM(callbackID);
    UNUSED_PARAM(iconData);
#endif
}

@implementation WebFramePolicyListener

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
#endif
}

- (id)initWithFrame:(Frame*)frame policyFunction:(FramePolicyFunction&&)policyFunction defaultPolicy:(PolicyAction)defaultPolicy
{
    self = [self init];
    if (!self)
        return nil;

    _frame = frame;
    _policyFunction = WTFMove(policyFunction);
    _defaultPolicy = defaultPolicy;

    return self;
}

#if HAVE(APP_LINKS)
- (id)initWithFrame:(Frame*)frame policyFunction:(FramePolicyFunction&&)policyFunction defaultPolicy:(PolicyAction)defaultPolicy appLinkURL:(NSURL *)appLinkURL
{
    self = [self initWithFrame:frame policyFunction:WTFMove(policyFunction) defaultPolicy:defaultPolicy];
    if (!self)
        return nil;

    _appLinkURL = appLinkURL;

    return self;
}
#endif

- (void)invalidate
{
    _frame = nullptr;
    if (auto policyFunction = std::exchange(_policyFunction, nullptr))
        policyFunction(PolicyAction::Ignore);
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebFramePolicyListener class], self))
        return;

    // If the app did not respond before the listener is destroyed, then we use the default policy ("Use" for navigation
    // response policy decision, "Ignore" for other policy decisions).
    _frame = nullptr;
    if (auto policyFunction = std::exchange(_policyFunction, nullptr)) {
        RELEASE_LOG_ERROR(Loading, "Client application failed to make a policy decision via WebPolicyDecisionListener, using defaultPolicy %hhu", _defaultPolicy);
        policyFunction(_defaultPolicy);
    }

    [super dealloc];
}

- (void)receivedPolicyDecision:(PolicyAction)action
{
    auto frame = WTFMove(_frame);
    if (!frame)
        return;

    if (auto policyFunction = std::exchange(_policyFunction, nullptr))
        policyFunction(action);
}

- (void)ignore
{
    [self receivedPolicyDecision:PolicyAction::Ignore];
}

- (void)download
{
    [self receivedPolicyDecision:PolicyAction::Download];
}

- (void)use
{
#if HAVE(APP_LINKS)
    if (_appLinkURL && _frame) {
        [LSAppLink openWithURL:_appLinkURL.get() completionHandler:^(BOOL success, NSError *) {
            WebThreadRun(^{
                if (success)
                    [self receivedPolicyDecision:PolicyAction::Ignore];
                else
                    [self receivedPolicyDecision:PolicyAction::Use];
            });
        }];
        return;
    }
#endif

    [self receivedPolicyDecision:PolicyAction::Use];
}

- (void)continue
{
    [self receivedPolicyDecision:PolicyAction::Use];
}

@end
