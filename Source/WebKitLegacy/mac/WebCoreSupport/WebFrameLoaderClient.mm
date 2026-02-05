/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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
#import "WebPanelAuthenticationHandler.h"
#import "WebPluginController.h"
#import "WebPluginPackage.h"
#import "WebPluginViewFactoryPrivate.h"
#import "WebPolicyDelegate.h"
#import "WebPolicyDelegatePrivate.h"
#import "WebPreferences.h"
#import "WebPreferencesInternal.h"
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
#import <WebCore/BackForwardItemIdentifier.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/CachedFrame.h>
#import <WebCore/Chrome.h>
#import <WebCore/ContainerNodeInlines.h>
#import <WebCore/DNS.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/DocumentPage.h>
#import <WebCore/DocumentView.h>
#import <WebCore/EventHandler.h>
#import <WebCore/EventNames.h>
#import <WebCore/FocusController.h>
#import <WebCore/FormState.h>
#import <WebCore/FrameDestructionObserverInlines.h>
#import <WebCore/FrameInlines.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderStateMachine.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/FrameTree.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLFrameElement.h>
#import <WebCore/HTMLFrameOwnerElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/HistoryController.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageAdapter.h>
#import <WebCore/LoaderNSURLExtras.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MouseEvent.h>
#import <WebCore/PluginViewBase.h>
#import <WebCore/ProtectionSpace.h>
#import <WebCore/ResourceError.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SubresourceLoader.h>
#import <WebCore/WebCoreJITOperations.h>
#import <WebCore/WebCoreMainThread.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/Widget.h>
#import <WebKitLegacy/DOMElement.h>
#import <WebKitLegacy/DOMHTMLFormElement.h>
#import <pal/spi/cocoa/NSURLDownloadSPI.h>
#import <pal/spi/cocoa/NSURLFileTypeMappingsSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/EnumTraits.h>
#import <wtf/MainThread.h>
#import <wtf/NakedPtr.h>
#import <wtf/Ref.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKClipView.h>
#import <WebCore/WAKScrollView.h>
#import <WebCore/WAKWindow.h>
#import <WebCore/WebCoreThreadMessage.h>
#import "WebMailDelegate.h"
#import "WebUIKitDelegate.h"
#endif

#if USE(QUICK_LOOK)
#import <WebCore/LegacyPreviewLoaderClient.h>
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
    RefPtr<WebCore::LocalFrame> _frame;
    WebCore::FramePolicyFunction _policyFunction;
#if HAVE(APP_LINKS)
    RetainPtr<NSURL> _appLinkURL;
    RetainPtr<NSURL> _referrerURL;
#endif
    WebCore::PolicyAction _defaultPolicy;
}

- (id)initWithFrame:(NakedPtr<WebCore::LocalFrame>)frame policyFunction:(WebCore::FramePolicyFunction&&)policyFunction defaultPolicy:(WebCore::PolicyAction)defaultPolicy;
#if HAVE(APP_LINKS)
- (id)initWithFrame:(NakedPtr<WebCore::LocalFrame>)frame policyFunction:(WebCore::FramePolicyFunction&&)policyFunction defaultPolicy:(WebCore::PolicyAction)defaultPolicy appLinkURL:(NSURL *)url referrerURL:(NSURL *)referrerURL;
#endif

- (void)invalidate;

@end

WebDataSource *dataSource(WebCore::DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoaderMac*>(loader)->dataSource() : nil;
}

WebFrameLoaderClient::WebFrameLoaderClient(WebCore::FrameLoader& loader, WebFrame *webFrame)
    : WebCore::LocalFrameLoaderClient(loader)
    , m_webFrame(webFrame)
{
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
    [m_webFrame.get() _clearCoreFrame];
}

bool WebFrameLoaderClient::hasWebView() const
{
    return [m_webFrame.get() webView] != nil;
}

void WebFrameLoaderClient::makeRepresentation(WebCore::DocumentLoader* loader)
{
    [dataSource(loader) _makeRepresentation];
}

bool WebFrameLoaderClient::hasHTMLView() const
{
    NSView <WebDocumentView> *view = [m_webFrame->_private->webFrameView documentView];
    return [view isKindOfClass:[WebHTMLView class]];
}

#if PLATFORM(IOS_FAMILY)
bool WebFrameLoaderClient::forceLayoutOnRestoreFromBackForwardCache()
{
    NSView <WebDocumentView> *view = [m_webFrame->_private->webFrameView documentView];
    // This gets called to lay out a page restored from the back/forward cache.
    // To work around timing problems with UIKit, restore fixed 
    // layout settings here.
    RetainPtr webView = getWebView(m_webFrame.get());
    bool isMainFrame = [webView.get() mainFrame] == m_webFrame.get();
    auto* coreFrame = core(m_webFrame.get());
    if (isMainFrame && coreFrame->view()) {
        WebCore::IntSize newSize([webView.get() _fixedLayoutSize]);
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
    auto thisView = m_webFrame->_private->webFrameView;
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
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[[m_webFrame->_private->webFrameView _scrollView] contentView] setCopiesOnScroll:YES];
ALLOW_DEPRECATED_DECLARATIONS_END
}

void WebFrameLoaderClient::detachedFromParent2()
{
    //remove any NetScape plugins that are children of this frame because they are about to be detached
    RetainPtr webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView.get() removePluginInstanceViewsFor:(m_webFrame.get())];
#endif
    [m_webFrame->_private->webFrameView _setWebFrame:nil]; // needed for now to be compatible w/ old behavior

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didRemoveFrameFromHierarchyFunc)
        CallFrameLoadDelegate(implementations->didRemoveFrameFromHierarchyFunc, webView.get(), @selector(webView:didRemoveFrameFromHierarchy:), m_webFrame.get());
}

void WebFrameLoaderClient::detachedFromParent3()
{
    m_webFrame->_private->webFrameView = nil;
}

void WebFrameLoaderClient::convertMainResourceLoadToDownload(WebCore::DocumentLoader* documentLoader, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    auto* mainResourceLoader = documentLoader->mainResourceLoader();

    if (!mainResourceLoader) {
        // The resource has already been cached, or the conversion is being attmpted when not calling SubresourceLoader::didReceiveResponse().
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        RetainPtr webDownload = adoptNS([[WebDownload alloc] initWithRequest:request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get() delegate:[webView.get() downloadDelegate]]);
ALLOW_DEPRECATED_DECLARATIONS_END
        webDownload.autorelease();
        return;
    }

    auto* handle = mainResourceLoader->handle();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [WebDownload _downloadWithLoadingConnection:handle->connection() request:request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get() response:response.protectedNSURLResponse().get() delegate:[webView.get() downloadDelegate] proxy:nil];
ALLOW_DEPRECATED_DECLARATIONS_END
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader* loader, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, int length)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());
#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidLoadResourceFromMemoryCacheFunc) {
        CallResourceLoadDelegateInWebThread(implementations->webThreadDidLoadResourceFromMemoryCacheFunc, webView.get(), @selector(webThreadWebView:didLoadResourceFromMemoryCache:response:length:fromDataSource:), request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get(), response.protectedNSURLResponse().get(), length, dataSource(loader));
        return true;
    }
#endif
    if (!implementations->didLoadResourceFromMemoryCacheFunc)
        return false;

    CallResourceLoadDelegate(implementations->didLoadResourceFromMemoryCacheFunc, webView.get(), @selector(webView:didLoadResourceFromMemoryCache:response:length:fromDataSource:), request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get(), response.protectedNSURLResponse().get(), length, dataSource(loader));
    return true;
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(WebCore::ResourceLoaderIdentifier identifier, WebCore::DocumentLoader* loader, const WebCore::ResourceRequest& request)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

    RetainPtr<id> object;

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadIdentifierForRequestFunc) {
        object = CallResourceLoadDelegateInWebThread(implementations->webThreadIdentifierForRequestFunc, webView.get(), @selector(webThreadWebView:identifierForInitialRequest:fromDataSource:), request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get(), dataSource(loader));
    } else
#endif
    if (implementations->identifierForRequestFunc)
        object = CallResourceLoadDelegate(implementations->identifierForRequestFunc, webView.get(), @selector(webView:identifierForInitialRequest:fromDataSource:), request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get(), dataSource(loader));
    else
        object = adoptNS([[NSObject alloc] init]);

    [webView.get() _addObject:object.get() forIdentifier:identifier];
}

void WebFrameLoaderClient::dispatchWillSendRequest(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, WebCore::ResourceRequest& request, const WebCore::ResourceResponse& redirectResponse)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

    if (redirectResponse.isNull())
        static_cast<WebDocumentLoaderMac*>(loader)->increaseLoadCount(identifier);

    RetainPtr currentURLRequest = request.nsURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody);

#if PLATFORM(MAC)
    if (WTF::MacApplication::isAppleMail() && loader->substituteData().isValid()) {
        // Mail.app checks for this property to detect data / archive loads.
        [NSURLProtocol setProperty:@"" forKey:@"WebDataRequest" inRequest:(NSMutableURLRequest *)currentURLRequest.get()];
    }
#endif

    RetainPtr newURLRequest = currentURLRequest;
#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadWillSendRequestFunc) {
        newURLRequest = (NSURLRequest *)CallResourceLoadDelegateInWebThread(implementations->webThreadWillSendRequestFunc, webView.get(), @selector(webThreadWebView:resource:willSendRequest:redirectResponse:fromDataSource:), [webView.get() _objectForIdentifier:identifier], currentURLRequest.get(), redirectResponse.protectedNSURLResponse().get(), dataSource(loader));
    } else
#endif
    if (implementations->willSendRequestFunc)
        newURLRequest = (NSURLRequest *)CallResourceLoadDelegate(implementations->willSendRequestFunc, webView.get(), @selector(webView:resource:willSendRequest:redirectResponse:fromDataSource:), [webView.get() _objectForIdentifier:identifier], currentURLRequest.get(), redirectResponse.protectedNSURLResponse().get(), dataSource(loader));

    if (newURLRequest != currentURLRequest)
        request.updateFromDelegatePreservingOldProperties(WebCore::ResourceRequest(newURLRequest.get()));
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

    if (implementations->shouldUseCredentialStorageFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            return CallResourceLoadDelegateReturningBoolean(NO, implementations->shouldUseCredentialStorageFunc, webView.get(), @selector(webView:resource:shouldUseCredentialStorageForDataSource:), resource, dataSource(loader));
    }

    return true;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, const WebCore::AuthenticationChallenge& challenge)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

    RetainPtr webChallenge = mac(challenge);

    if (implementations->didReceiveAuthenticationChallengeFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier]) {
            CallResourceLoadDelegate(implementations->didReceiveAuthenticationChallengeFunc, webView.get(), @selector(webView:resource:didReceiveAuthenticationChallenge:fromDataSource:), resource, webChallenge.get(), dataSource(loader));
            return;
        }
    }

#if !PLATFORM(IOS_FAMILY)
    NSWindow *window = [webView.get() hostWindow] ? [webView.get() hostWindow] : [webView.get() window];
    [[WebPanelAuthenticationHandler sharedHandler] startAuthentication:webChallenge.get() window:window];
#endif
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebFrameLoaderClient::canAuthenticateAgainstProtectionSpace(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, const WebCore::ProtectionSpace& protectionSpace)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

    RetainPtr webProtectionSpace = protectionSpace.nsSpace();

    if (implementations->canAuthenticateAgainstProtectionSpaceFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier]) {
            return CallResourceLoadDelegateReturningBoolean(NO, implementations->canAuthenticateAgainstProtectionSpaceFunc, webView.get(), @selector(webView:resource:canAuthenticateAgainstProtectionSpace:forDataSource:), resource, webProtectionSpace.get(), dataSource(loader));
        }
    }

    // If our resource load delegate doesn't handle the question, then only send authentication
    // challenges for pre-iOS-3.0, pre-10.6 protection spaces.  This is the same as the default implementation
    // in CFNetwork.
    return (protectionSpace.authenticationScheme() < WebCore::ProtectionSpace::AuthenticationScheme::ClientCertificateRequested);
}
#endif

#if PLATFORM(IOS_FAMILY)
RetainPtr<CFDictionaryRef> WebFrameLoaderClient::connectionProperties(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    id resource = [webView.get() _objectForIdentifier:identifier];
    if (!resource)
        return nullptr;

    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());
    if (implementations->connectionPropertiesFunc)
        return (CFDictionaryRef)CallResourceLoadDelegate(implementations->connectionPropertiesFunc, webView.get(), @selector(webView:connectionPropertiesForResource:dataSource:), resource, dataSource(loader));

    return nullptr;
}
#endif

bool WebFrameLoaderClient::shouldPaintBrokenImage(const URL& imageURL) const
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

    if (implementations->shouldPaintBrokenImageForURLFunc) {
        RetainPtr url = imageURL.createNSURL();
        return CallResourceLoadDelegateReturningBoolean(YES, implementations->shouldPaintBrokenImageForURLFunc, webView.get(), @selector(webView:shouldPaintBrokenImageForURL:), url.get());
    }
    return true;
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, const WebCore::ResourceResponse& response)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidReceiveResponseFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidReceiveResponseFunc, webView.get(), @selector(webThreadWebView:resource:didReceiveResponse:fromDataSource:), resource, response.protectedNSURLResponse().get(), dataSource(loader));

    } else
#endif
    if (implementations->didReceiveResponseFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didReceiveResponseFunc, webView.get(), @selector(webView:resource:didReceiveResponse:fromDataSource:), resource, response.protectedNSURLResponse().get(), dataSource(loader));
    }
}

void WebFrameLoaderClient::willCacheResponse(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, NSCachedURLResponse* response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadWillCacheResponseFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            return completionHandler(CallResourceLoadDelegateInWebThread(implementations->webThreadWillCacheResponseFunc, webView.get(), @selector(webThreadWebView:resource:willCacheResponse:fromDataSource:), resource, response, dataSource(loader)));
    } else
#endif
    if (implementations->willCacheResponseFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            return completionHandler(CallResourceLoadDelegate(implementations->willCacheResponseFunc, webView.get(), @selector(webView:resource:willCacheResponse:fromDataSource:), resource, response, dataSource(loader)));
    }

    completionHandler(response);
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, int dataLength)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());
#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidReceiveContentLengthFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidReceiveContentLengthFunc, webView.get(), @selector(webThreadWebView:resource:didReceiveContentLength:fromDataSource:), resource, (NSInteger)dataLength, dataSource(loader));
    } else
#endif
    if (implementations->didReceiveContentLengthFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didReceiveContentLengthFunc, webView.get(), @selector(webView:resource:didReceiveContentLength:fromDataSource:), resource, (NSInteger)dataLength, dataSource(loader));
    }
}

#if ENABLE(DATA_DETECTION)
void WebFrameLoaderClient::dispatchDidFinishDataDetection(NSArray *)
{
}
#endif

void WebFrameLoaderClient::dispatchDidFinishLoading(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidFinishLoadingFromDataSourceFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidFinishLoadingFromDataSourceFunc, webView.get(), @selector(webThreadWebView:resource:didFinishLoadingFromDataSource:), resource, dataSource(loader));
    } else
#endif

    if (implementations->didFinishLoadingFromDataSourceFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didFinishLoadingFromDataSourceFunc, webView.get(), @selector(webView:resource:didFinishLoadingFromDataSource:), resource, dataSource(loader));
    }

    [webView.get() _removeObjectForIdentifier:identifier];

    static_cast<WebDocumentLoaderMac*>(loader)->decreaseLoadCount(identifier);
}

void WebFrameLoaderClient::dispatchDidFailLoading(WebCore::DocumentLoader* loader, WebCore::ResourceLoaderIdentifier identifier, const WebCore::ResourceError& error)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidFailLoadingWithErrorFromDataSourceFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegateInWebThread(implementations->webThreadDidFailLoadingWithErrorFromDataSourceFunc, webView.get(), @selector(webThreadWebView:resource:didFailLoadingWithError:fromDataSource:), resource, (NSError *)error, dataSource(loader));
    } else
#endif
    if (implementations->didFailLoadingWithErrorFromDataSourceFunc) {
        if (id resource = [webView.get() _objectForIdentifier:identifier])
            CallResourceLoadDelegate(implementations->didFailLoadingWithErrorFromDataSourceFunc, webView.get(), @selector(webView:resource:didFailLoadingWithError:fromDataSource:), resource, (NSError *)error, dataSource(loader));
    }

    [webView.get() _removeObjectForIdentifier:identifier];

    static_cast<WebDocumentLoaderMac*>(loader)->decreaseLoadCount(identifier);
}

void WebFrameLoaderClient::dispatchDidDispatchOnloadEvents()
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didHandleOnloadEventsForFrameFunc)
        CallFrameLoadDelegate(implementations->didHandleOnloadEventsForFrameFunc, webView.get(), @selector(webView:didHandleOnloadEventsForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    RefPtr frame = core(m_webFrame.get());
    if (!frame)
        return;

    RefPtr provisionalDocumentLoader = frame->loader().provisionalDocumentLoader();
    if (!provisionalDocumentLoader)
        return;

    m_webFrame->_private->provisionalURL = provisionalDocumentLoader->url().string().createNSString().autorelease();

    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didReceiveServerRedirectForProvisionalLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didReceiveServerRedirectForProvisionalLoadForFrameFunc, webView.get(), @selector(webView:didReceiveServerRedirectForProvisionalLoadForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didCancelClientRedirectForFrameFunc)
        CallFrameLoadDelegate(implementations->didCancelClientRedirectForFrameFunc, webView.get(), @selector(webView:didCancelClientRedirectForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const URL& url, double delay, WallTime fireDate, WebCore::LockBackForwardList)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->willPerformClientRedirectToURLDelayFireDateForFrameFunc) {
        RetainPtr nsURL = url.createNSURL();
        CallFrameLoadDelegate(implementations->willPerformClientRedirectToURLDelayFireDateForFrameFunc, webView.get(), @selector(webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:), nsURL.get(), delay, [NSDate dateWithTimeIntervalSince1970:fireDate.secondsSinceEpoch().seconds()], m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string().createNSString();

    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didChangeLocationWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didChangeLocationWithinPageForFrameFunc, webView.get(), @selector(webView:didChangeLocationWithinPageForFrame:), m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
    [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didChangeLocationWithinPageForFrame:m_webFrame.get()];
#endif
}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string().createNSString();

    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didPushStateWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didPushStateWithinPageForFrameFunc, webView.get(), @selector(webView:didPushStateWithinPageForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string().createNSString();

    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didReplaceStateWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didReplaceStateWithinPageForFrameFunc, webView.get(), @selector(webView:didReplaceStateWithinPageForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    m_webFrame->_private->url = core(m_webFrame.get())->document()->url().string().createNSString();

    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didPopStateWithinPageForFrameFunc)
        CallFrameLoadDelegate(implementations->didPopStateWithinPageForFrameFunc, webView.get(), @selector(webView:didPopStateWithinPageForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchWillClose()
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->willCloseFrameFunc)
        CallFrameLoadDelegate(implementations->willCloseFrameFunc, webView.get(), @selector(webView:willCloseFrame:), m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
    [[webView.get() _UIKitDelegateForwarder] webView:webView.get() willCloseFrame:m_webFrame.get()];
#endif
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    ASSERT(!m_webFrame->_private->provisionalURL);
    m_webFrame->_private->provisionalURL = core(m_webFrame.get())->loader().provisionalDocumentLoader()->url().string().createNSString();

    RetainPtr webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView.get() _didStartProvisionalLoadForFrame:m_webFrame.get()];
#endif

#if PLATFORM(IOS_FAMILY)
    [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didStartProvisionalLoadForFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didStartProvisionalLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didStartProvisionalLoadForFrameFunc, webView.get(), @selector(webView:didStartProvisionalLoadForFrame:), m_webFrame.get());
}

static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

void WebFrameLoaderClient::dispatchDidReceiveTitle(const WebCore::StringWithDirection& title)
{
    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);

    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didReceiveTitleForFrameFunc) {
        // FIXME: Use direction of title.
        CallFrameLoadDelegate(implementations->didReceiveTitleForFrameFunc, webView.get(), @selector(webView:didReceiveTitle:forFrame:), truncatedTitle.string.createNSString().get(), m_webFrame.get());
    }
}

void WebFrameLoaderClient::dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>, std::optional<WebCore::UsedLegacyTLS>, std::optional<WebCore::WasPrivateRelayed>)
{
    // Tell the client we've committed this URL.
    ASSERT([m_webFrame->_private->webFrameView documentView] != nil);
    RetainPtr webView = getWebView(m_webFrame.get());
    [webView.get() _didCommitLoadForFrame:m_webFrame.get()];

    m_webFrame->_private->url = m_webFrame->_private->provisionalURL;
    m_webFrame->_private->provisionalURL = nullptr;

#if PLATFORM(IOS_FAMILY)
    [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didCommitLoadForFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didCommitLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didCommitLoadForFrameFunc, webView.get(), @selector(webView:didCommitLoadForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const WebCore::ResourceError& error, WebCore::WillContinueLoading, WebCore::WillInternallyHandleFailure)
{
    m_webFrame->_private->provisionalURL = nullptr;

    RetainPtr webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView.get() _didFailProvisionalLoadWithError:error forFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didFailProvisionalLoadWithErrorForFrameFunc)
        CallFrameLoadDelegate(implementations->didFailProvisionalLoadWithErrorForFrameFunc, webView.get(), @selector(webView:didFailProvisionalLoadWithError:forFrame:), (NSError *)error, m_webFrame.get());

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFailLoad(const WebCore::ResourceError& error)
{
    ASSERT(!m_webFrame->_private->provisionalURL);

    RetainPtr webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView.get() _didFailLoadWithError:error forFrame:m_webFrame.get()];
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didFailLoadWithErrorForFrameFunc)
        CallFrameLoadDelegate(implementations->didFailLoadWithErrorForFrameFunc, webView.get(), @selector(webView:didFailLoadWithError:forFrame:), (NSError *)error, m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
    [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didFailLoadWithError:((NSError *)error) forFrame:m_webFrame.get()];
#endif

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:error];
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    RetainPtr webView = getWebView(m_webFrame.get());

#if PLATFORM(IOS_FAMILY)
    id webThreadDel = [webView.get() _webMailDelegate];
    if ([webThreadDel respondsToSelector:@selector(_webthread_webView:didFinishDocumentLoadForFrame:)]) {
        [webThreadDel _webthread_webView:webView.get() didFinishDocumentLoadForFrame:m_webFrame.get()];
    }
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didFinishDocumentLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didFinishDocumentLoadForFrameFunc, webView.get(), @selector(webView:didFinishDocumentLoadForFrame:), m_webFrame.get());
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    ASSERT(!m_webFrame->_private->provisionalURL);

    RetainPtr webView = getWebView(m_webFrame.get());
#if !PLATFORM(IOS_FAMILY)
    [webView.get() _didFinishLoadForFrame:m_webFrame.get()];
#else
    [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didFinishLoadForFrame:m_webFrame.get()];

    id webThreadDel = [webView.get() _webMailDelegate];
    if ([webThreadDel respondsToSelector:@selector(_webthread_webView:didFinishLoadForFrame:)]) {
        [webThreadDel _webthread_webView:webView.get() didFinishLoadForFrame:m_webFrame.get()];
    }
#endif

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());
    if (implementations->didFinishLoadForFrameFunc)
        CallFrameLoadDelegate(implementations->didFinishLoadForFrameFunc, webView.get(), @selector(webView:didFinishLoadForFrame:), m_webFrame.get());

    [m_webFrame->_private->internalLoadDelegate webFrame:m_webFrame.get() didFinishLoadWithError:nil];
}

void WebFrameLoaderClient::dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> milestones)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());

#if PLATFORM(IOS_FAMILY)
    if (implementations->webThreadDidLayoutFunc)
        CallFrameLoadDelegateInWebThread(implementations->webThreadDidLayoutFunc, webView.get(), @selector(webThreadWebView:didLayout:), kitLayoutMilestones(milestones));
#else
    if (implementations->didLayoutFunc)
        CallFrameLoadDelegate(implementations->didLayoutFunc, webView.get(), @selector(webView:didLayout:), kitLayoutMilestones(milestones));
#endif

    if (milestones & WebCore::LayoutMilestone::DidFirstLayout) {
        // FIXME: We should consider removing the old didFirstLayout API since this is doing double duty with the
        // new didLayout API.
        if (implementations->didFirstLayoutInFrameFunc)
            CallFrameLoadDelegate(implementations->didFirstLayoutInFrameFunc, webView.get(), @selector(webView:didFirstLayoutInFrame:), m_webFrame.get());

#if PLATFORM(IOS_FAMILY)
        [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didFirstLayoutInFrame:m_webFrame.get()];
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

    if (milestones & WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout) {
        // FIXME: We should consider removing the old didFirstVisuallyNonEmptyLayoutForFrame API since this is doing
        // double duty with the new didLayout API.
        if (implementations->didFirstVisuallyNonEmptyLayoutInFrameFunc)
            CallFrameLoadDelegate(implementations->didFirstVisuallyNonEmptyLayoutInFrameFunc, webView.get(), @selector(webView:didFirstVisuallyNonEmptyLayoutInFrame:), m_webFrame.get());
#if PLATFORM(IOS_FAMILY)
        if ([webView.get() mainFrame] == m_webFrame.get())
            [[webView.get() _UIKitDelegateForwarder] webView:webView.get() didFirstVisuallyNonEmptyLayoutInFrame:m_webFrame.get()];
#endif
    }
}

WebCore::LocalFrame* WebFrameLoaderClient::dispatchCreatePage(const WebCore::NavigationAction&, WebCore::NewFrameOpenerPolicy policy)
{
    RetainPtr currentWebView = getWebView(m_webFrame.get());
    auto features = adoptNS([[NSDictionary alloc] init]);
    WebView *newWebView = [[currentWebView.get() _UIDelegateForwarder] webView:currentWebView.get()
                                                createWebViewWithRequest:nil
                                                          windowFeatures:features.get()];

    if (newWebView) {
        if (policy == WebCore::NewFrameOpenerPolicy::Allow)
            core([newWebView mainFrame])->setOpenerForWebKitLegacy(core(m_webFrame.get()));
        // Note: applyWindowFeatures is intentionally omitted here corresponding to WebLocalFrameLoaderClient::dispatchCreatePage just using the default window features.

        if (RefPtr opener = core(m_webFrame.get())) {
            auto effectiveSandboxFlags = opener->effectiveSandboxFlags();
            if (!effectiveSandboxFlags.contains(WebCore::SandboxFlag::PropagatesToAuxiliaryBrowsingContexts))
                effectiveSandboxFlags = { };
            core(newWebView)->mainFrame().updateSandboxFlags(effectiveSandboxFlags, WebCore::Frame::NotifyUIProcess::No);
        }
    }

    return core([newWebView mainFrame]);
}

void WebFrameLoaderClient::dispatchShow()
{
    RetainPtr webView = getWebView(m_webFrame.get());
    [[webView.get() _UIDelegateForwarder] webViewShow:webView.get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForResponse(const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, const String&, WebCore::FramePolicyFunction&& function)
{
    RetainPtr webView = getWebView(m_webFrame.get());

    [[webView.get() _policyDelegateForwarder] webView:webView.get()
        decidePolicyForMIMEType:response.mimeType().createNSString().get()
        request:request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get()
        frame:m_webFrame.get()
        decisionListener:setUpPolicyListener(WTF::move(function), WebCore::PolicyAction::Use, nil, nil).get()];
}


static BOOL shouldTryAppLink(WebView *webView, const WebCore::NavigationAction& action, WebCore::LocalFrame* targetFrame)
{
#if HAVE(APP_LINKS)
    BOOL mainFrameNavigation = !targetFrame || targetFrame->isMainFrame();
    if (!mainFrameNavigation)
        return NO;

    if (!action.processingUserGesture())
        return NO;

    if (targetFrame && targetFrame->document() && targetFrame->document()->url().host() == action.url().host())
        return NO;

    return YES;
#else
    return NO;
#endif
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, WebCore::FormState* formState, const String& frameName, std::optional<WebCore::HitTestResult>&&, WebCore::FramePolicyFunction&& function)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    BOOL tryAppLink = shouldTryAppLink(webView.get(), action, nullptr);

    RetainPtr<NSURL> appLinkURL;
    RetainPtr<NSURL> referrerURL;
    if (tryAppLink) {
        appLinkURL = request.url().createNSURL();
        if (!request.httpReferrer().isEmpty())
            referrerURL = URL(request.httpReferrer()).createNSURL();
    }

    [[webView.get() _policyDelegateForwarder] webView:webView.get()
        decidePolicyForNewWindowAction:actionDictionary(action, formState)
        request:request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get()
        newFrameName:frameName.createNSString().get()
        decisionListener:setUpPolicyListener(WTF::move(function), WebCore::PolicyAction::Ignore, appLinkURL.get(), referrerURL.get()).get()];
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const WebCore::NavigationAction& action, const WebCore::ResourceRequest& request, const WebCore::ResourceResponse&, WebCore::FormState* formState, const String&, std::optional<WebCore::NavigationIdentifier>, std::optional<WebCore::HitTestResult>&&, bool, WebCore::NavigationUpgradeToHTTPSBehavior, WebCore::SandboxFlags, WebCore::PolicyDecisionMode, WebCore::FramePolicyFunction&& function)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    BOOL tryAppLink = shouldTryAppLink(webView.get(), action, core(m_webFrame.get()));

    RetainPtr<NSURL> appLinkURL;
    RetainPtr<NSURL> referrerURL;
    if (tryAppLink) {
        appLinkURL = request.url().createNSURL();
        if (!request.httpReferrer().isEmpty())
            referrerURL = URL(request.httpReferrer()).createNSURL();
    }

    [[webView.get() _policyDelegateForwarder] webView:webView.get()
        decidePolicyForNavigationAction:actionDictionary(action, formState)
        request:request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get()
        frame:m_webFrame.get()
        decisionListener:setUpPolicyListener(WTF::move(function), WebCore::PolicyAction::Ignore, appLinkURL.get(), referrerURL.get()).get()];
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    if (!m_policyListener)
        return;

    [m_policyListener invalidate];
    m_policyListener = nil;
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const WebCore::ResourceError& error)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    [[webView.get() _policyDelegateForwarder] webView:webView.get() unableToImplementPolicyWithError:error frame:m_webFrame.get()];    
}

static NSDictionary *makeFormFieldValuesDictionary(WebCore::FormState& formState)
{
    auto& textFieldValues = formState.textFieldValues();
    size_t size = textFieldValues.size();
    auto dictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:size]);
    for (auto& value : textFieldValues)
        [dictionary setObject:value.second.createNSString().get() forKey:value.first.createNSString().get()];
    return dictionary.autorelease();
}

void WebFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<WebCore::FormState>&& formState)
{
    id <WebFormDelegate> formDelegate = [getWebView(m_webFrame.get()) _formDelegate];
    if (!formDelegate)
        return;

    RetainPtr formElement = kit(&formState->form());
    RetainPtr values = makeFormFieldValuesDictionary(formState.get());
    CallFormDelegate(getWebView(m_webFrame.get()), @selector(willSendSubmitEventToForm:inFrame:withValues:), formElement.get(), m_webFrame.get(), values.get());
}

void WebFrameLoaderClient::dispatchWillSubmitForm(WebCore::FormState& formState, URL&&, String&&, CompletionHandler<void()>&& completionHandler)
{
    id <WebFormDelegate> formDelegate = [getWebView(m_webFrame.get()) _formDelegate];
    if (!formDelegate) {
        completionHandler();
        return;
    }

    RetainPtr values = makeFormFieldValuesDictionary(formState);
    CallFormDelegate(getWebView(m_webFrame.get()), @selector(frame:sourceFrame:willSubmitForm:withValues:submissionListener:), m_webFrame.get(), kit(formState.sourceDocument().frame()), kit(&formState.form()), values.get(), setUpPolicyListener([completionHandler = WTF::move(completionHandler)] (WebCore::PolicyAction) mutable { completionHandler(); },
        WebCore::PolicyAction::Ignore, nil, nil).get());
}

void WebFrameLoaderClient::revertToProvisionalState(WebCore::DocumentLoader* loader)
{
    [dataSource(loader) _revertToProvisionalState];
}

void WebFrameLoaderClient::setMainDocumentError(WebCore::DocumentLoader* loader, const WebCore::ResourceError& error)
{
    [dataSource(loader) _setMainDocumentError:error];
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready)
{
    [getWebView(m_webFrame.get()) setMainFrameDocumentReady:ready];
}

void WebFrameLoaderClient::startDownload(const WebCore::ResourceRequest& request, const String& /* suggestedName */, WebCore::FromDownloadAttribute)
{
    // FIXME: Should download full request.
    [getWebView(m_webFrame.get()) _downloadURL:request.url().createNSURL().get()];
}

void WebFrameLoaderClient::willChangeTitle(WebCore::DocumentLoader* loader)
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Should do this only in main frame case, right?
    [getWebView(m_webFrame.get()) _willChangeValueForKey:_WebMainFrameTitleKey];
#endif
}

void WebFrameLoaderClient::didChangeTitle(WebCore::DocumentLoader* loader)
{
#if !PLATFORM(IOS_FAMILY)
    // FIXME: Should do this only in main frame case, right?
    [getWebView(m_webFrame.get()) _didChangeValueForKey:_WebMainFrameTitleKey];
#endif
}

void WebFrameLoaderClient::didReplaceMultipartContent()
{
#if PLATFORM(IOS_FAMILY)
    if (auto* view = core(m_webFrame.get())->view())
        view->didReplaceMultipartContent();
#endif
}

void WebFrameLoaderClient::committedLoad(WebCore::DocumentLoader* loader, const WebCore::SharedBuffer& data)
{
    [dataSource(loader) _receivedData:data.createNSData().get()];
}

void WebFrameLoaderClient::finishedLoading(WebCore::DocumentLoader* loader)
{
    [dataSource(loader) _finishedLoading];
}

static inline NSString *nilOrNSString(const String& string)
{
    if (string.isNull())
        return nil;
    return string.createNSString().autorelease();
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    RetainPtr view = getWebView(m_webFrame.get());
    auto* loader = core(m_webFrame.get())->loader().documentLoader();
#if PLATFORM(IOS_FAMILY)
    if (loader->urlForHistory() == aboutBlankURL())
        return;
#endif

    if ([view.get() historyDelegate]) {
        WebHistoryDelegateImplementationCache* implementations = WebViewGetHistoryDelegateImplementations(view.get());
        if (implementations->navigatedFunc) {
            auto data = adoptNS([[WebNavigationData alloc] initWithURLString:loader->url().string().createNSString().get()
                title:nilOrNSString(loader->title().string)
                originalRequest:loader->originalRequestCopy().protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get()
                response:loader->response().protectedNSURLResponse().get()
                hasSubstituteData:loader->substituteData().isValid()
                clientRedirectSource:loader->clientRedirectSourceForHistory().createNSString().get()]);

            CallHistoryDelegate(implementations->navigatedFunc, view.get(), @selector(webView:didNavigateWithNavigationData:inFrame:), data.get(), m_webFrame.get());
        }
    
        return;
    }

    [[WebHistory optionalSharedHistory] _visitedURL:loader->urlForHistory().createNSURL().get() withTitle:loader->title().string.createNSString().get() method:loader->originalRequestCopy().httpMethod().createNSString().get() wasFailure:loader->urlForHistoryReflectsFailure()];
}

static void addRedirectURL(WebHistoryItem *item, const String& url)
{
    if (!item->_private->_redirectURLs)
        item->_private->_redirectURLs = makeUnique<Vector<String>>();

    // Our API allows us to store all the URLs in the redirect chain, but for
    // now we only have a use for the final URL.
    item->_private->_redirectURLs->resize(1);
    item->_private->_redirectURLs->at(0) = url;
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    RetainPtr view = getWebView(m_webFrame.get());
    WebHistoryDelegateImplementationCache* implementations = [view.get() historyDelegate] ? WebViewGetHistoryDelegateImplementations(view.get()) : 0;

    auto* loader = core(m_webFrame.get())->loader().documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    if (!loader->clientRedirectSourceForHistory().isNull()) {
        if (implementations) {
            if (implementations->clientRedirectFunc) {
                CallHistoryDelegate(implementations->clientRedirectFunc, view.get(), @selector(webView:didPerformClientRedirectFromURL:toURL:inFrame:),
                    m_webFrame->_private->url.get(), loader->clientRedirectDestinationForHistory().createNSString().get(), m_webFrame.get());
            }
        } else if (RetainPtr item = [[WebHistory optionalSharedHistory] _itemForURLString:loader->clientRedirectSourceForHistory().createNSString().get()])
            addRedirectURL(item.get(), loader->clientRedirectDestinationForHistory());
    }

    if (!loader->serverRedirectSourceForHistory().isNull()) {
        if (implementations) {
            if (implementations->serverRedirectFunc) {
                CallHistoryDelegate(implementations->serverRedirectFunc, view.get(), @selector(webView:didPerformServerRedirectFromURL:toURL:inFrame:),
                    loader->serverRedirectSourceForHistory().createNSString().get(), loader->serverRedirectDestinationForHistory().createNSString().get(), m_webFrame.get());
            }
        } else if (RetainPtr item = [[WebHistory optionalSharedHistory] _itemForURLString:loader->serverRedirectSourceForHistory().createNSString().get()])
            addRedirectURL(item.get(), loader->serverRedirectDestinationForHistory());
    }
}

WebCore::ShouldGoToHistoryItem WebFrameLoaderClient::shouldGoToHistoryItem(WebCore::HistoryItem& item, WebCore::IsSameDocumentNavigation, WebCore::ProcessSwapDisposition) const
{
    RetainPtr view = getWebView(m_webFrame.get());
    return [[view.get() _policyDelegateForwarder] webView:view.get() shouldGoToHistoryItem:kit(&item)] ? WebCore::ShouldGoToHistoryItem::Yes : WebCore::ShouldGoToHistoryItem::No;
}

bool WebFrameLoaderClient::supportsAsyncShouldGoToHistoryItem() const
{
    return false;
}

void WebFrameLoaderClient::shouldGoToHistoryItemAsync(WebCore::HistoryItem&, CompletionHandler<void(WebCore::ShouldGoToHistoryItem)>&&) const
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool WebFrameLoaderClient::shouldFallBack(const WebCore::ResourceError& error) const
{
    // FIXME: Needs to check domain.
    // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent
    // loading plugin content twice.  See <rdar://problem/4258008>
    return error.errorCode() != NSURLErrorCancelled && error.errorCode() != WebKitErrorPlugInWillHandleLoad;
}

bool WebFrameLoaderClient::canHandleRequest(const WebCore::ResourceRequest& request) const
{
    return [WebView _canHandleRequest:request.protectedNSURLRequest(WebCore::HTTPBodyUpdatePolicy::UpdateHTTPBody).get() forMainFrame:core(m_webFrame.get())->isMainFrame()];
}

bool WebFrameLoaderClient::canShowMIMEType(const String& MIMEType) const
{
    return [getWebView(m_webFrame.get()) _canShowMIMEType:MIMEType.createNSString().get()];
}

bool WebFrameLoaderClient::canShowMIMETypeAsHTML(const String& MIMEType) const
{
    return [WebView canShowMIMETypeAsHTML:MIMEType.createNSString().get()];
}

bool WebFrameLoaderClient::representationExistsForURLScheme(StringView URLScheme) const
{
    return [WebView _representationExistsForURLScheme:URLScheme.createNSString().get()];
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(StringView URLScheme) const
{
    return [WebView _generatedMIMETypeForURLScheme:URLScheme.createNSString().get()];
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    // Note: Can be called multiple times.

    // See WebFrameLoaderClient::provisionalLoadStarted.
    if ([getWebView(m_webFrame.get()) drawsBackground])
        [[m_webFrame->_private->webFrameView _scrollView] setDrawsBackground:YES];
}

void WebFrameLoaderClient::saveViewStateToItem(WebCore::HistoryItem& item)
{
#if PLATFORM(IOS_FAMILY)
    // Let UIKit handle the scroll point for the main frame.
    WebFrame *webFrame = m_webFrame.get();
    RetainPtr webView = getWebView(webFrame);   
    if (webFrame == [webView.get() mainFrame]) {
        [[webView.get() _UIKitDelegateForwarder] webView:webView.get() saveStateToHistoryItem:kit(&item) forFrame:webFrame];
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
    WebCore::HistoryItem* currentItem = core(m_webFrame.get())->loader().history().currentItem();
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
    RetainPtr webView = getWebView(webFrame);   
    if (webFrame == [webView.get() mainFrame]) {
        [[webView.get() _UIKitDelegateForwarder] webView:webView.get() restoreStateFromHistoryItem:kit(currentItem) forFrame:webFrame force:NO];
        return;
    }
#endif                    

    NSView <WebDocumentView> *docView = [m_webFrame->_private->webFrameView documentView];
    if ([docView conformsToProtocol:@protocol(_WebDocumentViewState)]) {
        RetainPtr<id> state = currentItem->viewState();
        if (state) {
            [(id <_WebDocumentViewState>)docView setViewState:state.get()];
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
    auto frameView = m_webFrame->_private->webFrameView;
    NSWindow *window = [frameView window];
    NSResponder *firstResp = [window firstResponder];
    if ([firstResp isKindOfClass:[NSView class]] && [(NSView *)firstResp isDescendantOf:frameView.get()])
        [window endEditingFor:firstResp];
#endif
}

Ref<WebCore::DocumentLoader> WebFrameLoaderClient::createDocumentLoader(WebCore::ResourceRequest&& request, WebCore::SubstituteData&& substituteData, WebCore::ResourceRequest&& originalRequest)
{
    auto loader = WebDocumentLoaderMac::create(WTF::move(request), WTF::move(substituteData), WTF::move(originalRequest));

    auto dataSource = adoptNS([[WebDataSource alloc] _initWithDocumentLoader:loader.copyRef()]);
    loader->setDataSource(dataSource.get(), getWebView(m_webFrame.get()));

    return WTF::move(loader);
}

Ref<WebCore::DocumentLoader> WebFrameLoaderClient::createDocumentLoader(WebCore::ResourceRequest&& request, WebCore::SubstituteData&& substituteData)
{
    return createDocumentLoader(WTF::move(request), WTF::move(substituteData), { });
}

void WebFrameLoaderClient::setTitle(const WebCore::StringWithDirection& title, const URL& url)
{
    RetainPtr view = getWebView(m_webFrame.get());

    if ([view.get() historyDelegate]) {
        WebHistoryDelegateImplementationCache* implementations = WebViewGetHistoryDelegateImplementations(view.get());
        // FIXME: Use direction of title.
        if (implementations->setTitleFunc)
            CallHistoryDelegate(implementations->setTitleFunc, view.get(), @selector(webView:updateHistoryTitle:forURL:inFrame:), title.string.createNSString().get(), url.string().createNSString().get(), m_webFrame.get());
        else if (implementations->deprecatedSetTitleFunc) {
            IGNORE_WARNINGS_BEGIN("undeclared-selector")
            CallHistoryDelegate(implementations->deprecatedSetTitleFunc, view.get(), @selector(webView:updateHistoryTitle:forURL:), title.string.createNSString().get(), url.string().createNSString().get());
            IGNORE_WARNINGS_END
        }
        return;
    }

    RetainPtr nsURL = url.createNSURL();
    nsURL = [nsURL _webkit_canonicalize];
    if(!nsURL)
        return;
#if PLATFORM(IOS_FAMILY)
    if ([[nsURL absoluteString] isEqualToString:@"about:blank"])
        return;
#endif
    [[[WebHistory optionalSharedHistory] itemForURL:nsURL.get()] setTitle:title.string.createNSString().get()];
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(WebCore::CachedFrame* cachedFrame)
{
    cachedFrame->setCachedFramePlatformData(makeUnique<WebCachedFramePlatformData>([m_webFrame->_private->webFrameView documentView]));

#if PLATFORM(IOS_FAMILY)
    // At this point we know this frame is going to be cached. Stop all plugins.
    RetainPtr webView = getWebView(m_webFrame.get());
    [webView.get() _stopAllPlugInsForPageCache];
#endif
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(WebCore::CachedFrame* cachedFrame)
{
    WebCachedFramePlatformData* platformData = static_cast<WebCachedFramePlatformData*>(cachedFrame->cachedFramePlatformData());
    RetainPtr<NSView<WebDocumentView>> cachedView = platformData->webDocumentView();
    ASSERT(cachedView);
    ASSERT(cachedFrame->documentLoader());
    [cachedView.get() setDataSource:dataSource(cachedFrame->documentLoader())];

#if !PLATFORM(IOS_FAMILY)
    // clean up webkit plugin instances before WebHTMLView gets freed.
    RetainPtr webView = getWebView(m_webFrame.get());
    [webView.get() removePluginInstanceViewsFor:(m_webFrame.get())];
#endif
    
    [m_webFrame->_private->webFrameView _setDocumentView:cachedView.get()];
}

#if PLATFORM(IOS_FAMILY)
void WebFrameLoaderClient::didRestoreFrameHierarchyForCachedFrame()
{
    // When entering the PageCache the Document is detached and the plugin view may
    // have cleaned itself up (removing its webview and layer references). Now, when
    // restoring the page we want to recreate whatever is necessary.
    RetainPtr webView = getWebView(m_webFrame.get());
    [webView.get() _restorePlugInsFromCache];
}
#endif

void WebFrameLoaderClient::transitionToCommittedForNewPage(InitializingIframe)
{
    WebDataSource *dataSource = [m_webFrame.get() _dataSource];

#if PLATFORM(IOS_FAMILY)
    bool willProduceHTMLView;
    // Fast path that skips initialization of objc class objects.
    if ([dataSource _documentLoader]->responseMIMEType() == "text/html"_s)
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
    RetainPtr webView = getWebView(m_webFrame.get());
    [webView.get() removePluginInstanceViewsFor:(m_webFrame.get())];
    
    RetainPtr<NSView <WebDocumentView>> documentView = [m_webFrame->_private->webFrameView _makeDocumentViewForDataSource:dataSource];
#else
    RetainPtr<NSView <WebDocumentView>> documentView;

    // Fast path that skips initialization of objc class objects.
    if (willProduceHTMLView) {
        documentView = adoptNS([[WebHTMLView alloc] initWithFrame:[m_webFrame->_private->webFrameView bounds]]);
        [m_webFrame->_private->webFrameView _setDocumentView:documentView.get()];
    } else
        documentView = [m_webFrame->_private->webFrameView _makeDocumentViewForDataSource:dataSource];
#endif
    if (!documentView)
        return;

    // FIXME: Could we skip some of this work for a top-level view that is not a WebHTMLView?

    // If we own the view, delete the old one - otherwise the render m_frame will take care of deleting the view.
    auto* coreFrame = core(m_webFrame.get());
    auto* page = coreFrame->page();
    bool isMainFrame = coreFrame->isMainFrame();
    if (isMainFrame && coreFrame->view())
        coreFrame->view()->setParentVisible(false);
    coreFrame->setView(nullptr);
    auto coreView = WebCore::LocalFrameView::create(*coreFrame);
    coreFrame->setView(coreView.copyRef());

#if PLATFORM(IOS_FAMILY)
    page->setDelegatesScaling(true);
#endif

    [m_webFrame.get() _updateBackgroundAndUpdatesWhileOffscreen];
    [m_webFrame->_private->webFrameView _install];

    if (isMainFrame) {
#if PLATFORM(IOS_FAMILY)
        coreView->setDelegatedScrollingMode(WebCore::DelegatedScrollingMode::DelegatedToNativeScrollView);
#endif
        coreView->setParentVisible(true);
    }

    // Call setDataSource on the document view after it has been placed in the view hierarchy.
    // This what we for the top-level view, so should do this for views in subframes as well.
    [documentView setDataSource:dataSource];

    // The following is a no-op for WebHTMLRepresentation, but for custom document types
    // like the ones that Safari uses for bookmarks it is the only way the DocumentLoader
    // will get the proper title.
    if (auto documentLoader = [dataSource _documentLoader])
        documentLoader->setTitle({ [dataSource pageTitle], WebCore::TextDirection::LTR });

    if (auto* ownerElement = coreFrame->ownerElement())
        coreFrame->view()->setCanHaveScrollbars(ownerElement->scrollingMode() != WebCore::ScrollbarMode::AlwaysOff);

    // If the document view implicitly became first responder, make sure to set the focused frame properly.
    if ([[documentView window] firstResponder] == documentView) {
        page->focusController().setFocusedFrame(coreFrame);
        page->focusController().setFocused(true);
    }
}

void WebFrameLoaderClient::didRestoreFromBackForwardCache()
{
#if PLATFORM(IOS_FAMILY)
    RetainPtr webView = getWebView(m_webFrame.get());
    if ([webView.get() mainFrame] == m_webFrame.get())
        [[webView.get() _UIKitDelegateForwarder] webViewDidRestoreFromPageCache:webView.get()];
#endif
}

RetainPtr<WebFramePolicyListener> WebFrameLoaderClient::setUpPolicyListener(WebCore::FramePolicyFunction&& function, WebCore::PolicyAction defaultPolicy, NSURL *appLinkURL, NSURL* referrerURL)
{
    // FIXME: <rdar://5634381> We need to support multiple active policy listeners.
    [m_policyListener invalidate];

    RetainPtr<WebFramePolicyListener> policyListener;
#if HAVE(APP_LINKS)
    if (appLinkURL)
        policyListener = adoptNS([[WebFramePolicyListener alloc] initWithFrame:core(m_webFrame.get()) policyFunction:WTF::move(function) defaultPolicy:defaultPolicy appLinkURL:appLinkURL referrerURL:referrerURL]);
    else
#endif
        policyListener = adoptNS([[WebFramePolicyListener alloc] initWithFrame:core(m_webFrame.get()) policyFunction:WTF::move(function) defaultPolicy:defaultPolicy]);

    m_policyListener = policyListener.get();

    return policyListener;
}

String WebFrameLoaderClient::userAgent(const URL& url) const
{
    RetainPtr webView = getWebView(m_webFrame.get());
    ASSERT(webView);

    // We should never get here with nil for the WebView unless there is a bug somewhere else.
    // But if we do, it's better to return the empty string than just crashing on the spot.
    // Most other call sites are tolerant of nil because of Objective-C behavior, but this one
    // is not because the return value of _userAgentForURL is a const URL&.
    if (!webView)
        return emptyString();

    return [webView.get() _userAgentString];
}

NSDictionary *WebFrameLoaderClient::actionDictionary(const WebCore::NavigationAction& action, WebCore::FormState* formState) const
{
    using namespace WebCore;

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

    RetainPtr originalURL = action.url().createNSURL();

    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        @(static_cast<int>(action.type())), WebActionNavigationTypeKey,
        @(modifierFlags), WebActionModifierFlagsKey,
        originalURL.get(), WebActionOriginalURLKey,
        nil];

    if (auto mouseEventData = action.mouseEventData()) {
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
        auto element = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:core(m_webFrame.get())->eventHandler().hitTestResultAtPoint(mouseEventData->absoluteLocation, hitType)]);
        [result setObject:element.get() forKey:WebActionElementKey];

        auto button = enumToUnderlyingType(mouseEventData->isTrusted ? mouseEventData->button : MouseButton::None);
        [result setObject:@(button) forKey:WebActionButtonKey];
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
    auto* page = core(m_webFrame.get())->page();
    if (!page)
        return false;
    
    auto& backForwardList = static_cast<BackForwardList&>(page->backForward().client());
    if (!backForwardList.enabled())
        return false;
    
    if (!backForwardList.capacity())
        return false;
    
    return true;
}

RefPtr<WebCore::LocalFrame> WebFrameLoaderClient::createFrame(const AtomString& name, WebCore::HTMLFrameOwnerElement& ownerElement)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    ASSERT(m_webFrame);

    auto* ownerFrame = ownerElement.document().frame();
    if (!ownerFrame) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    auto* page = ownerFrame->page();
    if (!page) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto childView = adoptNS([[WebFrameView alloc] init]);
    auto result = [WebFrame _createSubframeWithOwnerElement:ownerElement page:*page frameName:name frameView:childView.get()];
    RetainPtr newFrame = kit(result.ptr());

    if ([newFrame.get() _dataSource])
        [[newFrame.get() _dataSource] _documentLoader]->setOverrideEncoding([[m_webFrame.get() _dataSource] _documentLoader]->overrideEncoding());

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!result->page())
        return nullptr;

    return result;

    END_BLOCK_OBJC_EXCEPTIONS

    return nullptr;
}

WebCore::ObjectContentType WebFrameLoaderClient::objectContentType(const URL& url, const String& mimeType)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    String type = mimeType;

    if (type.isEmpty()) {
        // Try to guess the MIME type based off the extension.
        RetainPtr nsURL = url.createNSURL();
        NSString *extension = [[nsURL path] pathExtension];
        if ([extension length] > 0) {
            type = [[NSURLFileTypeMappings sharedMappings] MIMETypeForExtension:extension];
            if (type.isEmpty()) {
                // If no MIME type is specified, use a plug-in if we have one that can handle the extension.
                if ([getWebView(m_webFrame.get()) _pluginForExtension:extension])
                    return WebCore::ObjectContentType::PlugIn;
            }
        }
    }

    if (type.isEmpty())
        return WebCore::ObjectContentType::Frame; // Go ahead and hope that we can display the content.

    WebCore::ObjectContentType plugInType = WebCore::ObjectContentType::None;
    if ([getWebView(m_webFrame.get()) _pluginForMIMEType:type.createNSString().get()])
        plugInType = WebCore::ObjectContentType::PlugIn;

    if (WebCore::MIMETypeRegistry::isSupportedImageMIMEType(type))
        return WebCore::ObjectContentType::Image;

    if (plugInType != WebCore::ObjectContentType::None)
        return plugInType;

    if ([m_webFrame->_private->webFrameView _viewClassForMIMEType:type.createNSString().get()])
        return WebCore::ObjectContentType::Frame;
    
    return WebCore::ObjectContentType::None;

    END_BLOCK_OBJC_EXCEPTIONS

    return WebCore::ObjectContentType::None;
}

static AtomString parameterValue(const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues, ASCIILiteral name)
{
    size_t size = paramNames.size();
    ASSERT(size == paramValues.size());
    for (size_t i = 0; i < size; ++i) {
        if (equalIgnoringASCIICase(paramNames[i], name))
            return paramValues[i];
    }
    return nullAtom();
}

static NSView *pluginView(WebFrame *frame, WebPluginPackage *pluginPackage,
    NSArray *attributeNames, NSArray *attributeValues, NSURL *baseURL,
    DOMElement *element, BOOL loadManually)
{
    WebHTMLView *docView = (WebHTMLView *)[[frame frameView] documentView];
    ASSERT([docView isKindOfClass:[WebHTMLView class]]);

    WebPluginController *pluginController = [docView _pluginController];

    // Store attributes in a dictionary so they can be passed to WebPlugins.
    auto attributes = adoptNS([[NSMutableDictionary alloc] initWithObjects:attributeValues forKeys:attributeNames]);

    [pluginPackage load];
    Class viewFactory = [pluginPackage viewFactory];
    
    NSDictionary *arguments = nil;

IGNORE_WARNINGS_BEGIN("undeclared-selector")
    auto oldSelector = @selector(pluginViewWithArguments:);
IGNORE_WARNINGS_END

    if ([viewFactory respondsToSelector:@selector(plugInViewWithArguments:)]) {
        arguments = @{
            WebPlugInBaseURLKey: baseURL,
            WebPlugInAttributesKey: attributes.get(),
            WebPlugInContainerKey: pluginController,
            WebPlugInModeKey: @(loadManually ? WebPlugInModeFull : WebPlugInModeEmbed),
            WebPlugInShouldLoadMainResourceKey: [NSNumber numberWithBool:!loadManually],
            WebPlugInContainingElementKey: element,
        };
        LOG(Plugins, "arguments:\n%@", arguments);
    } else if ([viewFactory respondsToSelector:oldSelector]) {
        arguments = @{
            WebPluginBaseURLKey: baseURL,
            WebPluginAttributesKey: attributes.get(),
            WebPluginContainerKey: pluginController,
            WebPlugInContainingElementKey: element,
        };
        LOG(Plugins, "arguments:\n%@", arguments);
    }
    (void)arguments;

    return nil;
}

class PluginWidget : public WebCore::PluginViewBase {
public:
    PluginWidget(NSView *view = nil)
        : WebCore::PluginViewBase(view)
    {
    }
    
private:
    virtual void invalidateRect(const WebCore::IntRect& rect)
    {
        [platformWidget() setNeedsDisplayInRect:rect];
    }
};

static bool shouldBlockPlugin(WebBasePluginPackage *)
{
    return true;
}

RefPtr<WebCore::Widget> WebFrameLoaderClient::createPlugin(WebCore::HTMLPlugInElement& element, const URL& url,
    const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues, const String& mimeType, bool loadManually)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(paramNames.size() == paramValues.size());

    int errorCode = 0;

    RetainPtr webView = getWebView(m_webFrame.get());
    auto* document = core(m_webFrame.get())->document();
    RetainPtr baseURL = document->baseURL().createNSURL();
    RetainPtr pluginURL = url.createNSURL();
    auto attributeKeys = createNSArray(paramNames);

#if PLATFORM(MAC)
    SEL selector = @selector(webView:plugInViewWithArguments:);
    if ([[webView.get() UIDelegate] respondsToSelector:selector]) {
        RetainPtr attributes = [NSDictionary dictionaryWithObjects:createNSArray(paramValues).get() forKeys:attributeKeys.get()];
        auto arguments = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:
            attributes.get(), WebPlugInAttributesKey,
            @(loadManually ? WebPlugInModeFull : WebPlugInModeEmbed), WebPlugInModeKey,
            [NSNumber numberWithBool:!loadManually], WebPlugInShouldLoadMainResourceKey,
            kit(&element), WebPlugInContainingElementKey,
            // FIXME: We should be passing base URL, see <https://bugs.webkit.org/show_bug.cgi?id=35215>.
            pluginURL.get(), WebPlugInBaseURLKey, // pluginURL might be nil, so add it last
            nil]);

        RetainPtr view = CallUIDelegate(webView.get(), selector, arguments.get());

        if (view)
            return adoptRef(*new PluginWidget(view.get()));
    }
#endif

    RetainPtr<NSString> MIMEType;
    WebBasePluginPackage *pluginPackage;
    if (mimeType.isEmpty())
        pluginPackage = nil;
    else {
        MIMEType = mimeType.createNSString();
        pluginPackage = [webView.get() _pluginForMIMEType:MIMEType.get()];
    }

    NSString *extension = [[pluginURL path] pathExtension];

#if PLATFORM(IOS_FAMILY)
    if (!pluginPackage && [extension length])
#else
    if (!pluginPackage && [extension length] && ![MIMEType length])
#endif
    {
        pluginPackage = [webView.get() _pluginForExtension:extension];
        if (pluginPackage) {
            RetainPtr newMIMEType = [pluginPackage MIMETypeForExtension:extension];
            if ([newMIMEType length] != 0)
                MIMEType = WTF::move(newMIMEType);
        }
    }

    RetainPtr<NSView> view;

    if (pluginPackage) {
        if (shouldBlockPlugin(pluginPackage)) {
            errorCode = WebKitErrorBlockedPlugInVersion;
            if (is<WebCore::RenderEmbeddedObject>(element.renderer()))
                downcast<WebCore::RenderEmbeddedObject>(*element.renderer()).setPluginUnavailabilityReason(WebCore::PluginUnavailabilityReason::InsecurePluginVersion);
        } else {
            if ([pluginPackage isKindOfClass:[WebPluginPackage class]])
                view = pluginView(m_webFrame.get(), (WebPluginPackage *)pluginPackage, attributeKeys.get(), createNSArray(paramValues).get(), baseURL.get(), kit(&element), loadManually);
        }
    } else
        errorCode = WebKitErrorCannotFindPlugIn;

    if (!errorCode && !view)
        errorCode = WebKitErrorCannotLoadPlugIn;

    if (errorCode && m_webFrame) {
        WebResourceDelegateImplementationCache* implementations = WebViewGetResourceLoadDelegateImplementations(webView.get());
        if (implementations->plugInFailedWithErrorFunc) {
            URL pluginPageURL = document->completeURL(parameterValue(paramNames, paramValues, "pluginspage"_s));
            if (!pluginPageURL.protocolIsInHTTPFamily())
                pluginPageURL = URL();
            RetainPtr pluginName = pluginPackage ? [pluginPackage pluginInfo].name.createNSString() : nil;

            RetainPtr error = adoptNS([[NSError alloc] _initWithPluginErrorCode:errorCode contentURL:pluginURL.get() pluginPageURL:pluginPageURL.createNSURL().get() pluginName:pluginName.get() MIMEType:MIMEType.get()]);
            CallResourceLoadDelegate(implementations->plugInFailedWithErrorFunc, [m_webFrame.get() webView],
                                     @selector(webView:plugInFailedWithError:dataSource:), error.get(), [m_webFrame.get() _dataSource]);
        }

        return nullptr;
    }

    ASSERT(view);
    return adoptRef(new PluginWidget(view.get()));

    END_BLOCK_OBJC_EXCEPTIONS

    return nullptr;
}

void WebFrameLoaderClient::redirectDataToPlugin(WebCore::Widget& pluginWidget)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    WebHTMLRepresentation *representation = (WebHTMLRepresentation *)[[m_webFrame.get() _dataSource] representation];

    RetainPtr pluginView = pluginWidget.platformWidget();

    {
        WebHTMLView *documentView = (WebHTMLView *)[[m_webFrame.get() frameView] documentView];
        ASSERT([documentView isKindOfClass:[WebHTMLView class]]);
        [representation _redirectDataToManualLoader:[documentView _pluginController] forPluginView:pluginView.get()];
    }

    END_BLOCK_OBJC_EXCEPTIONS
}

void WebFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(WebCore::internalError(url)));
}

AtomString WebFrameLoaderClient::overrideMediaType() const
{
    NSString* overrideType = [getWebView(m_webFrame.get()) mediaStyle];
    if (overrideType)
        return AtomString(overrideType);
    return nullAtom();
}

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld& world)
{
    RetainPtr webView = getWebView(m_webFrame.get());
    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(webView.get());

    if (implementations->didClearWindowObjectForFrameInScriptWorldFunc) {
        CallFrameLoadDelegate(implementations->didClearWindowObjectForFrameInScriptWorldFunc,
            webView.get(), @selector(webView:didClearWindowObjectForFrame:inScriptWorld:), m_webFrame.get(), [WebScriptWorld findOrCreateWorld:world]);
        return;
    }

    if (&world != &WebCore::mainThreadNormalWorldSingleton())
        return;

    auto* frame = core(m_webFrame.get());
    auto& script = frame->script();

#if JSC_OBJC_API_ENABLED
    if (implementations->didCreateJavaScriptContextForFrameFunc) {
        CallFrameLoadDelegate(implementations->didCreateJavaScriptContextForFrameFunc, webView.get(), @selector(webView:didCreateJavaScriptContext:forFrame:),
            script.javaScriptContext(), m_webFrame.get());
    } else
#endif
    if (implementations->didClearWindowObjectForFrameFunc) {
        CallFrameLoadDelegate(implementations->didClearWindowObjectForFrameFunc, webView.get(), @selector(webView:didClearWindowObject:forFrame:),
            script.windowScriptObject(), m_webFrame.get());
    } else if (implementations->windowScriptObjectAvailableFunc) {
        CallFrameLoadDelegate(implementations->windowScriptObjectAvailableFunc, webView.get(), @selector(webView:windowScriptObjectAvailable:),
            script.windowScriptObject());
    }

    if ([webView.get() scriptDebugDelegate]) {
        [m_webFrame.get() _detachScriptDebugger];
        [m_webFrame.get() _attachScriptDebugger];
    }
}

Ref< WebCore::FrameNetworkingContext> WebFrameLoaderClient::createNetworkingContext()
{
    return WebFrameNetworkingContext::create(core(m_webFrame.get()));
}

RefPtr<WebCore::HistoryItem> WebFrameLoaderClient::createHistoryItemTree(bool clipAtTarget, WebCore::BackForwardItemIdentifier itemID) const
{
    Ref coreMainFrame = core(m_webFrame.get())->rootFrame();
    return coreMainFrame->loader().history().createItemTree(*core(m_webFrame.get()), clipAtTarget, itemID);
}

#if PLATFORM(IOS_FAMILY)
bool WebFrameLoaderClient::shouldLoadMediaElementURL(const URL& url) const
{
    RetainPtr webView = getWebView(m_webFrame.get());

    if (id policyDelegate = [webView.get() policyDelegate]) {
        if ([policyDelegate respondsToSelector:@selector(webView:shouldLoadMediaURL:inFrame:)])
            return [policyDelegate webView:webView.get() shouldLoadMediaURL:url.createNSURL().get() inFrame:m_webFrame.get()];
    }
    return true;
}
#endif

#if USE(QUICK_LOOK)
RefPtr<WebCore::LegacyPreviewLoaderClient> WebFrameLoaderClient::createPreviewLoaderClient(const String& fileName, const String& uti)
{
    class QuickLookDocumentWriter : public WebCore::LegacyPreviewLoaderClient {
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

        void didReceiveData(const WebCore::SharedBuffer& buffer) override
        {
            auto dataArray = buffer.createNSDataArray();
            for (NSData *data in dataArray.get())
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

    RetainPtr filePath = WebCore::createTemporaryFileForQuickLook(fileName.createNSString().get());
    if (!filePath)
        return nullptr;

    auto documentWriter = adoptRef(*new QuickLookDocumentWriter(filePath.get()));

    [m_webFrame provisionalDataSource]._quickLookContent = @{ WebQuickLookFileNameKey : filePath.get(), WebQuickLookUTIKey : uti.createNSString().get() };
    [m_webFrame provisionalDataSource]._quickLookPreviewLoaderClient = documentWriter.ptr();
    return documentWriter;
}
#endif

#if ENABLE(CONTENT_FILTERING)
void WebFrameLoaderClient::contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler&& unblockHandler)
{
    core(m_webFrame.get())->loader().policyChecker().setContentFilterUnblockHandler(WTF::move(unblockHandler));
}
#endif

void WebFrameLoaderClient::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

void WebFrameLoaderClient::getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>& icons)
{
    auto* frame = core(m_webFrame.get());
    auto* documentLoader = frame->loader().documentLoader();
    ASSERT(documentLoader);

#if PLATFORM(MAC)
    if (!WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITH_DEFAULT_ICON_LOADING)) {
        for (auto& icon : icons)
            documentLoader->didGetLoadDecisionForIcon(false, icon.second, [](auto) { });

        return;
    }
#endif

    bool disallowedDueToImageLoadSettings = false;
    if (!frame->settings().loadsImagesAutomatically())
        disallowedDueToImageLoadSettings = true;

    if (disallowedDueToImageLoadSettings || !frame->isMainFrame() || !documentLoader->url().protocolIsInHTTPFamily() || ![WebView _isIconLoadingEnabled]) {
        for (auto& icon : icons)
            documentLoader->didGetLoadDecisionForIcon(false, icon.second, [](auto) { });

        return;
    }

#if !PLATFORM(IOS_FAMILY)
    ASSERT(!m_loadingIcon);
    // WebKit 1, which only supports one icon per page URL, traditionally has preferred the last icon in case of multiple icons listed.
    // To preserve that behavior we walk the list backwards.
    for (auto icon = icons.rbegin(); icon != icons.rend(); ++icon) {
        if (icon->first.type != WebCore::LinkIconType::Favicon || m_loadingIcon) {
            documentLoader->didGetLoadDecisionForIcon(false, icon->second, [](auto) { });
            continue;
        }

        m_loadingIcon = true;
        documentLoader->didGetLoadDecisionForIcon(true, icon->second, [weakThis = WeakPtr { *this }] (WebCore::FragmentedSharedBuffer* buffer) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->finishedLoadingIcon(buffer);
        });
    }
#else
    // No WebCore icon loading on iOS
    for (auto& icon : icons)
        documentLoader->didGetLoadDecisionForIcon(false, icon.second, [](auto) { });
#endif
}

#if !PLATFORM(IOS_FAMILY)
static NSImage *webGetNSImage(WebCore::Image* image, NSSize size)
{
    ASSERT(size.width);
    ASSERT(size.height);

    // FIXME: We're doing the resize here for now because WebCore::Image doesn't yet support resizing/multiple representations
    // This makes it so there's effectively only one size of a particular icon in the system at a time. We should move this
    // to WebCore::Image at some point.
    if (!image)
        return nil;
    RetainPtr nsImage = image->adapter().nsImage();
    if (!nsImage)
        return nil;
    if (!NSEqualSizes([nsImage.get() size], size))
        [nsImage.get() setSize:size];
    return nsImage.autorelease();
}
#endif // !PLATFORM(IOS_FAMILY)

void WebFrameLoaderClient::finishedLoadingIcon(WebCore::FragmentedSharedBuffer* iconData)
{
#if !PLATFORM(IOS_FAMILY)
    ASSERT(m_loadingIcon);
    m_loadingIcon = false;

    RetainPtr webView = getWebView(m_webFrame.get());
    if (!webView)
        return;

    auto image = WebCore::BitmapImage::create();
    if (image->setData(iconData, true) < WebCore::EncodedDataStatus::SizeAvailable)
        return;

    RetainPtr icon = webGetNSImage(image.ptr(), NSMakeSize(16, 16));
    if (icon)
        [webView.get() _setMainFrameIcon:icon.get()];
#else
    UNUSED_PARAM(iconData);
#endif
}

@implementation WebFramePolicyListener

+ (void)initialize
{
    WebCore::initializeMainThreadIfNeeded();
}

- (id)initWithFrame:(NakedPtr<WebCore::LocalFrame>)frame policyFunction:(WebCore::FramePolicyFunction&&)policyFunction defaultPolicy:(WebCore::PolicyAction)defaultPolicy
{
    self = [self init];
    if (!self)
        return nil;

    _frame = frame;
    _policyFunction = WTF::move(policyFunction);
    _defaultPolicy = defaultPolicy;

    return self;
}

#if HAVE(APP_LINKS)
- (id)initWithFrame:(NakedPtr<WebCore::LocalFrame>)frame policyFunction:(WebCore::FramePolicyFunction&&)policyFunction defaultPolicy:(WebCore::PolicyAction)defaultPolicy appLinkURL:(NSURL *)appLinkURL referrerURL:(NSURL *)referrerURL
{
    self = [self initWithFrame:frame policyFunction:WTF::move(policyFunction) defaultPolicy:defaultPolicy];
    if (!self)
        return nil;

    _appLinkURL = appLinkURL;
    _referrerURL = WebCore::SecurityOrigin::create(URL { referrerURL })->toURL().createNSURL();

    return self;
}
#endif

- (void)invalidate
{
    _frame = nullptr;
    if (auto policyFunction = std::exchange(_policyFunction, nullptr))
        policyFunction(WebCore::PolicyAction::Ignore);
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebFramePolicyListener class], self))
        return;

    // If the app did not respond before the listener is destroyed, then we use the default policy ("Use" for navigation
    // response policy decision, "Ignore" for other policy decisions).
    _frame = nullptr;
    if (auto policyFunction = std::exchange(_policyFunction, nullptr)) {
        RELEASE_LOG_ERROR(Loading, "Client application failed to make a policy decision via WebPolicyDecisionListener, using defaultPolicy %hhu", enumToUnderlyingType(_defaultPolicy));
        policyFunction(_defaultPolicy);
    }

    [super dealloc];
}

- (void)receivedPolicyDecision:(WebCore::PolicyAction)action
{
    auto frame = WTF::move(_frame);
    if (!frame)
        return;

    if (auto policyFunction = std::exchange(_policyFunction, nullptr))
        policyFunction(action);
}

- (void)ignore
{
    [self receivedPolicyDecision:WebCore::PolicyAction::Ignore];
}

- (void)download
{
    [self receivedPolicyDecision:WebCore::PolicyAction::Download];
}

- (void)use
{
#if HAVE(APP_LINKS)
    if (_appLinkURL && _frame) {
        RetainPtr<_LSOpenConfiguration> configuration = adoptNS([[_LSOpenConfiguration alloc] init]);
        configuration.get().referrerURL = _referrerURL.get();
        [LSAppLink openWithURL:_appLinkURL.get() configuration:configuration.get() completionHandler:^(BOOL success, NSError *) {
#if USE(WEB_THREAD)
            WebThreadRun(^{
#else
            RunLoop::mainSingleton().dispatch([self, strongSelf = retainPtr(self), success] {
#endif
                if (success)
                    [self receivedPolicyDecision:WebCore::PolicyAction::Ignore];
                else
                    [self receivedPolicyDecision:WebCore::PolicyAction::Use];
            });
        }];
        return;
    }
#endif

    [self receivedPolicyDecision:WebCore::PolicyAction::Use];
}

- (void)continue
{
    [self receivedPolicyDecision:WebCore::PolicyAction::Use];
}

@end
