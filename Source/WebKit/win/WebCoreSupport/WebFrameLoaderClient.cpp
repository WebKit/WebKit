/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebFrameLoaderClient.h"

#include "CFDictionaryPropertyBag.h"
#include "COMPropertyBag.h"
#include "DOMHTMLClasses.h"
#include "DefaultPolicyDelegate.h"
#include "EmbeddedWidget.h"
#include "MarshallingHelpers.h"
#include "NotImplemented.h"
#include "WebActionPropertyBag.h"
#include "WebCachedFramePlatformData.h"
#include "WebChromeClient.h"
#include "WebDocumentLoader.h"
#include "WebDownload.h"
#include "WebError.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebFramePolicyListener.h"
#include "WebHistory.h"
#include "WebHistoryItem.h"
#include "WebMutableURLRequest.h"
#include "WebNavigationData.h"
#include "WebNotificationCenter.h"
#include "WebScriptWorld.h"
#include "WebSecurityOrigin.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLResponse.h"
#include "WebView.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/BackForwardController.h>
#include <WebCore/CachedFrame.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLAppletElement.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLParserIdioms.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/Page.h>
#include <WebCore/PluginPackage.h>
#include <WebCore/PluginView.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/RenderWidget.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/ScriptController.h>
#include <WebCore/Settings.h>

using namespace WebCore;
using namespace HTMLNames;

static WebDataSource* getWebDataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoader*>(loader)->dataSource() : 0;
}

class WebFrameLoaderClient::WebFramePolicyListenerPrivate {
public:
    WebFramePolicyListenerPrivate() 
        : m_policyFunction(nullptr)
    { 
    }

    ~WebFramePolicyListenerPrivate() { }

    FramePolicyFunction m_policyFunction;
    COMPtr<WebFramePolicyListener> m_policyListener;
};

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame* webFrame)
    : m_webFrame(webFrame)
    , m_manualLoader(0)
    , m_policyListenerPrivate(adoptPtr(new WebFramePolicyListenerPrivate))
    , m_hasSentResponseToPlugin(false) 
{
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}

void WebFrameLoaderClient::frameLoaderDestroyed()
{
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_webFrame->webView();
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent2()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void WebFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader* documentLoader, const ResourceRequest& request, const ResourceResponse& response)
{
    COMPtr<IWebDownloadDelegate> downloadDelegate;
    COMPtr<IWebView> webView;
    if (SUCCEEDED(m_webFrame->webView(&webView))) {
        if (FAILED(webView->downloadDelegate(&downloadDelegate))) {
            // If the WebView doesn't successfully provide a download delegate we'll pass a null one
            // into the WebDownload - which may or may not decide to use a DefaultDownloadDelegate
            LOG_ERROR("Failed to get downloadDelegate from WebView");
            downloadDelegate = 0;
        }
    }

    // Its the delegate's job to ref the WebDownload to keep it alive - otherwise it will be destroyed
    // when this method returns
    COMPtr<WebDownload> download;
    download.adoptRef(WebDownload::createInstance(documentLoader->mainResourceLoader()->handle(), request, response, downloadDelegate.get()));
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int /*length*/)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebMutableURLRequest> webURLRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));
    resourceLoadDelegate->identifierForInitialRequest(webView, webURLRequest.get(), getWebDataSource(loader), identifier);
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader* loader, unsigned long identifier)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return true;

    COMPtr<IWebResourceLoadDelegatePrivate> resourceLoadDelegatePrivate;
    if (FAILED(resourceLoadDelegate->QueryInterface(IID_IWebResourceLoadDelegatePrivate, reinterpret_cast<void**>(&resourceLoadDelegatePrivate))))
        return true;

    BOOL shouldUse;
    if (SUCCEEDED(resourceLoadDelegatePrivate->shouldUseCredentialStorage(webView, identifier, getWebDataSource(loader), &shouldUse)))
        return shouldUse;

    return true;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    ASSERT(challenge.authenticationClient());

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<WebURLAuthenticationChallenge> webChallenge(AdoptCOM, WebURLAuthenticationChallenge::createInstance(challenge));
        if (SUCCEEDED(resourceLoadDelegate->didReceiveAuthenticationChallenge(webView, identifier, webChallenge.get(), getWebDataSource(loader))))
            return;
    }

    // If the ResourceLoadDelegate doesn't exist or fails to handle the call, we tell the ResourceHandle
    // to continue without credential - this is the best approximation of Mac behavior
    challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebURLAuthenticationChallenge> webChallenge(AdoptCOM, WebURLAuthenticationChallenge::createInstance(challenge));
    resourceLoadDelegate->didCancelAuthenticationChallenge(webView, identifier, webChallenge.get(), getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebMutableURLRequest> webURLRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));
    COMPtr<WebURLResponse> webURLRedirectResponse(AdoptCOM, WebURLResponse::createInstance(redirectResponse));

    COMPtr<IWebURLRequest> newWebURLRequest;
    if (FAILED(resourceLoadDelegate->willSendRequest(webView, identifier, webURLRequest.get(), webURLRedirectResponse.get(), getWebDataSource(loader), &newWebURLRequest)))
        return;

    if (webURLRequest == newWebURLRequest)
        return;

    if (!newWebURLRequest) {
        request = ResourceRequest();
        return;
    }

    COMPtr<WebMutableURLRequest> newWebURLRequestImpl(Query, newWebURLRequest);
    if (!newWebURLRequestImpl)
        return;

    request = newWebURLRequestImpl->resourceRequest();
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebURLResponse> webURLResponse(AdoptCOM, WebURLResponse::createInstance(response));
    resourceLoadDelegate->didReceiveResponse(webView, identifier, webURLResponse.get(), getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int length)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    resourceLoadDelegate->didReceiveContentLength(webView, identifier, length, getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    resourceLoadDelegate->didFinishLoadingFromDataSource(webView, identifier, getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebError> webError(AdoptCOM, WebError::createInstance(error));
    resourceLoadDelegate->didFailLoadingWithError(webView, identifier, webError.get(), getWebDataSource(loader));
}

bool WebFrameLoaderClient::shouldCacheResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response, const unsigned char* data, const unsigned long long length)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return true;

    COMPtr<IWebResourceLoadDelegatePrivate> resourceLoadDelegatePrivate(Query, resourceLoadDelegate);
    if (!resourceLoadDelegatePrivate)
        return true;

    COMPtr<IWebURLResponse> urlResponse(AdoptCOM, WebURLResponse::createInstance(response));
    BOOL shouldCache;
    if (SUCCEEDED(resourceLoadDelegatePrivate->shouldCacheResponse(webView, identifier, urlResponse.get(), data, length, getWebDataSource(loader), &shouldCache)))
        return shouldCache;

    return true;
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didHandleOnloadEventsForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveServerRedirectForProvisionalLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCancelClientRedirectForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const URL& url, double delay, double fireDate)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willPerformClientRedirectToURL(webView, BString(url.string()), delay, MarshallingHelpers::CFAbsoluteTimeToDATE(fireDate), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didChangeLocationWithinPageForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (FAILED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) || !frameLoadDelegatePriv)
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> frameLoadDelegatePriv2(Query, frameLoadDelegatePriv);
    if (!frameLoadDelegatePriv2)
        return;

    frameLoadDelegatePriv2->didPushStateWithinPageForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (FAILED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) || !frameLoadDelegatePriv)
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> frameLoadDelegatePriv2(Query, frameLoadDelegatePriv);
    if (!frameLoadDelegatePriv2)
        return;

    frameLoadDelegatePriv2->didReplaceStateWithinPageForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (FAILED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) || !frameLoadDelegatePriv)
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> frameLoadDelegatePriv2(Query, frameLoadDelegatePriv);
    if (!frameLoadDelegatePriv2)
        return;

    frameLoadDelegatePriv2->didPopStateWithinPageForFrame(webView, m_webFrame);
}
void WebFrameLoaderClient::dispatchWillClose()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willCloseFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    m_webFrame->webView()->dispatchDidReceiveIconFromWebFrame(m_webFrame);
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didStartProvisionalLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        // FIXME: use direction of title.
        frameLoadDelegate->didReceiveTitle(webView, BString(title.string()), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidChangeIcons(WebCore::IconType type)
{
    if (type != WebCore::Favicon)
        return;

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (FAILED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) || !frameLoadDelegatePriv)
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> frameLoadDelegatePriv2(Query, frameLoadDelegatePriv);
    if (!frameLoadDelegatePriv2)
        return;

    frameLoadDelegatePriv2->didChangeIcons(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCommitLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        frameLoadDelegate->didFailProvisionalLoadWithError(webView, webError.get(), m_webFrame);
    }
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate))) {
        COMPtr<IWebError> webError;
        webError.adoptRef(WebError::createInstance(error));
        frameLoadDelegate->didFailLoadWithError(webView, webError.get(), m_webFrame);
    }
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFinishDocumentLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didFinishLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidLayout(LayoutMilestones milestones)
{
    WebView* webView = m_webFrame->webView();

    if (milestones & DidFirstLayout) {
        COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePrivate;
        if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePrivate)) && frameLoadDelegatePrivate)
            frameLoadDelegatePrivate->didFirstLayoutInFrame(webView, m_webFrame);
    }

    if (milestones & DidFirstVisuallyNonEmptyLayout) {
        COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePrivate;
        if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePrivate)) && frameLoadDelegatePrivate)
            frameLoadDelegatePrivate->didFirstVisuallyNonEmptyLayoutInFrame(webView, m_webFrame);
    }
}

Frame* WebFrameLoaderClient::dispatchCreatePage(const NavigationAction& navigationAction)
{
    WebView* webView = m_webFrame->webView();

    COMPtr<IWebUIDelegate> ui;
    if (FAILED(webView->uiDelegate(&ui)))
        return 0;

    COMPtr<IWebView> newWebView;
    COMPtr<WebMutableURLRequest> request = adoptCOM(WebMutableURLRequest::createInstance(ResourceRequest(navigationAction.url())));
    if (FAILED(ui->createWebViewWithRequest(webView, request.get(), &newWebView)) || !newWebView)
        return 0;

    COMPtr<IWebFrame> mainFrame;
    if (FAILED(newWebView->mainFrame(&mainFrame)))
        return 0;

    COMPtr<WebFrame> mainFrameImpl(Query, mainFrame);
    return core(mainFrameImpl.get());
}

void WebFrameLoaderClient::dispatchShow()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(webView->uiDelegate(&ui)))
        ui->webViewShow(webView);
}

void WebFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest& request, FramePolicyFunction function)
{
    WebView* webView = m_webFrame->webView();
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (FAILED(webView->policyDelegate(&policyDelegate)))
        policyDelegate = DefaultPolicyDelegate::sharedInstance();

    COMPtr<IWebURLRequest> urlRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));

    if (SUCCEEDED(policyDelegate->decidePolicyForMIMEType(webView, BString(response.mimeType()), urlRequest.get(), m_webFrame, setUpPolicyListener(function).get())))
        return;

    function(PolicyUse);
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction& action, const ResourceRequest& request, PassRefPtr<FormState> formState, const String& frameName, FramePolicyFunction function)
{
    WebView* webView = m_webFrame->webView();
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (FAILED(webView->policyDelegate(&policyDelegate)))
        policyDelegate = DefaultPolicyDelegate::sharedInstance();

    COMPtr<IWebURLRequest> urlRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));
    COMPtr<WebActionPropertyBag> actionInformation(AdoptCOM, WebActionPropertyBag::createInstance(action, formState ? formState->form() : 0, coreFrame));

    if (SUCCEEDED(policyDelegate->decidePolicyForNewWindowAction(webView, actionInformation.get(), urlRequest.get(), BString(frameName), setUpPolicyListener(function).get())))
        return;

    function(PolicyUse);
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& action, const ResourceRequest& request, PassRefPtr<FormState> formState, FramePolicyFunction function)
{
    WebView* webView = m_webFrame->webView();
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (FAILED(webView->policyDelegate(&policyDelegate)))
        policyDelegate = DefaultPolicyDelegate::sharedInstance();

    COMPtr<IWebURLRequest> urlRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));
    COMPtr<WebActionPropertyBag> actionInformation(AdoptCOM, WebActionPropertyBag::createInstance(action, formState ? formState->form() : 0, coreFrame));

    if (SUCCEEDED(policyDelegate->decidePolicyForNavigationAction(webView, actionInformation.get(), urlRequest.get(), m_webFrame, setUpPolicyListener(function).get())))
        return;

    function(PolicyUse);
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError& error)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebPolicyDelegate> policyDelegate;
    if (FAILED(webView->policyDelegate(&policyDelegate)))
        policyDelegate = DefaultPolicyDelegate::sharedInstance();

    COMPtr<IWebError> webError(AdoptCOM, WebError::createInstance(error));
    policyDelegate->unableToImplementPolicyWithError(webView, webError.get(), m_webFrame);
}

void WebFrameLoaderClient::dispatchWillSendSubmitEvent(PassRefPtr<WebCore::FormState>)
{
}

void WebFrameLoaderClient::dispatchWillSubmitForm(PassRefPtr<FormState> formState, FramePolicyFunction function)
{
    WebView* webView = m_webFrame->webView();
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<IWebFormDelegate> formDelegate;

    if (FAILED(webView->formDelegate(&formDelegate))) {
        function(PolicyUse);
        return;
    }

    COMPtr<IDOMElement> formElement(AdoptCOM, DOMElement::createInstance(formState->form()));

    HashMap<String, String> formValuesMap;
    const StringPairVector& textFieldValues = formState->textFieldValues();
    size_t size = textFieldValues.size();
    for (size_t i = 0; i < size; ++i)
        formValuesMap.add(textFieldValues[i].first, textFieldValues[i].second);

    COMPtr<IPropertyBag> formValuesPropertyBag(AdoptCOM, COMPropertyBag<String>::createInstance(formValuesMap));

    COMPtr<WebFrame> sourceFrame(kit(formState->sourceDocument()->frame()));
    if (SUCCEEDED(formDelegate->willSubmitForm(m_webFrame, sourceFrame.get(), formElement.get(), formValuesPropertyBag.get(), setUpPolicyListener(function).get())))
        return;

    // FIXME: Add a sane default implementation
    function(PolicyUse);
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_manualLoader)
        return;

    m_manualLoader->didFail(error);
    m_manualLoader = 0;
    m_hasSentResponseToPlugin = false;
}

void WebFrameLoaderClient::progressStarted(WebCore::Frame&)
{
    static BSTR progressStartedName = SysAllocString(WebViewProgressStartedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressStartedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::progressEstimateChanged(WebCore::Frame&)
{
    static BSTR progressEstimateChangedName = SysAllocString(WebViewProgressEstimateChangedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressEstimateChangedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::progressFinished(WebCore::Frame&)
{
    static BSTR progressFinishedName = SysAllocString(WebViewProgressFinishedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressFinishedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::startDownload(const ResourceRequest& request, const String& /* suggestedName */)
{
    m_webFrame->webView()->downloadURL(request.url());
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (!m_manualLoader)
        loader->commitData(data, length);

    // If the document is a stand-alone media document, now is the right time to cancel the WebKit load.
    // FIXME: This code should be shared across all ports. <http://webkit.org/b/48762>.
    Frame* coreFrame = core(m_webFrame);
    if (coreFrame->document()->isMediaDocument())
        loader->cancelMainResourceLoad(pluginWillHandleLoadError(loader->response()));

    if (!m_manualLoader)
        return;

    if (!m_hasSentResponseToPlugin) {
        m_manualLoader->didReceiveResponse(loader->response());
        // didReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
        // setting up this stream can cause the main document load to be cancelled, setting m_manualLoader
        // to null
        if (!m_manualLoader)
            return;
        m_hasSentResponseToPlugin = true;
    }
    m_manualLoader->didReceiveData(data, length);
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader*)
{
    if (!m_manualLoader)
        return;

    m_manualLoader->didFinishLoading();
    m_manualLoader = 0;
    m_hasSentResponseToPlugin = false;
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    DocumentLoader* loader = core(m_webFrame)->loader().documentLoader();
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebHistoryDelegate> historyDelegate;
    webView->historyDelegate(&historyDelegate);

    if (historyDelegate) {
        COMPtr<IWebURLResponse> urlResponse(AdoptCOM, WebURLResponse::createInstance(loader->response()));
        COMPtr<IWebURLRequest> urlRequest(AdoptCOM, WebMutableURLRequest::createInstance(loader->originalRequestCopy()));
        
        COMPtr<IWebNavigationData> navigationData(AdoptCOM, WebNavigationData::createInstance(
            loader->urlForHistory(), loader->title().string(), urlRequest.get(), urlResponse.get(), loader->substituteData().isValid(), loader->clientRedirectSourceForHistory()));

        historyDelegate->didNavigateWithNavigationData(webView, navigationData.get(), m_webFrame);
        return;
    }

    WebHistory* history = WebHistory::sharedHistory();
    if (!history)
        return;

    history->visitedURL(loader->urlForHistory(), loader->title().string(), loader->originalRequestCopy().httpMethod(), loader->urlForHistoryReflectsFailure(), !loader->clientRedirectSourceForHistory());
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebHistoryDelegate> historyDelegate;
    webView->historyDelegate(&historyDelegate);

    WebHistory* history = WebHistory::sharedHistory();

    DocumentLoader* loader = core(m_webFrame)->loader().documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    if (!loader->clientRedirectSourceForHistory().isNull()) {
        if (historyDelegate) {
            BString sourceURL(loader->clientRedirectSourceForHistory());
            BString destURL(loader->clientRedirectDestinationForHistory());
            historyDelegate->didPerformClientRedirectFromURL(webView, sourceURL, destURL, m_webFrame);
        } else {
            if (history) {
                if (COMPtr<IWebHistoryItem> iWebHistoryItem = history->itemForURLString(loader->clientRedirectSourceForHistory())) {
                    COMPtr<WebHistoryItem> webHistoryItem(Query, iWebHistoryItem);
                    webHistoryItem->historyItem()->addRedirectURL(loader->clientRedirectDestinationForHistory());
                }
            }
        }
    }

    if (!loader->serverRedirectSourceForHistory().isNull()) {
        if (historyDelegate) {
            BString sourceURL(loader->serverRedirectSourceForHistory());
            BString destURL(loader->serverRedirectDestinationForHistory());
            historyDelegate->didPerformServerRedirectFromURL(webView, sourceURL, destURL, m_webFrame);
        } else {
            if (history) {
                if (COMPtr<IWebHistoryItem> iWebHistoryItem = history->itemForURLString(loader->serverRedirectSourceForHistory())) {
                    COMPtr<WebHistoryItem> webHistoryItem(Query, iWebHistoryItem);
                    webHistoryItem->historyItem()->addRedirectURL(loader->serverRedirectDestinationForHistory());
                }
            }
        }
    }
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const
{
    return true;
}

void WebFrameLoaderClient::updateGlobalHistoryItemForPage()
{
    HistoryItem* historyItem = 0;
    WebView* webView = m_webFrame->webView();

    if (Page* page = webView->page()) {
        if (!page->settings().privateBrowsingEnabled())
            historyItem = page->backForward().currentItem();
    }

    webView->setGlobalHistoryItem(historyItem);
}

void WebFrameLoaderClient::didDisplayInsecureContent()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (FAILED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) || !frameLoadDelegatePriv)
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> frameLoadDelegatePriv2(Query, frameLoadDelegatePriv);
    if (!frameLoadDelegatePriv2)
        return;

    frameLoadDelegatePriv2->didDisplayInsecureContent(webView);
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin* origin, const URL& insecureURL)
{
    COMPtr<IWebSecurityOrigin> webSecurityOrigin = WebSecurityOrigin::createInstance(origin);

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (FAILED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) || !frameLoadDelegatePriv)
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> frameLoadDelegatePriv2(Query, frameLoadDelegatePriv);
    if (!frameLoadDelegatePriv2)
        return;

    frameLoadDelegatePriv2->didRunInsecureContent(webView, webSecurityOrigin.get());
}

void WebFrameLoaderClient::didDetectXSS(const URL&, bool)
{
    // FIXME: propogate call into the private delegate.
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    // FIXME: Need ChickenCat to include CFNetwork/CFURLError.h to get these values
    // Alternatively, we could create our own error domain/codes.
    return ResourceError(String(WebURLErrorDomain), -999, request.url().string(), String("Cancelled"));
}

ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorCannotUseRestrictedPort, request.url().string(), WEB_UI_STRING("Not allowed to use restricted network port", "WebKitErrorCannotUseRestrictedPort description"));
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorCannotShowURL, request.url().string(), WEB_UI_STRING("The URL can\xE2\x80\x99t be shown", "WebKitErrorCannotShowURL description"));
}

ResourceError WebFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().string(), WEB_UI_STRING("Frame load interrupted", "WebKitErrorFrameLoadInterruptedByPolicyChange description"));
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError(String(), WebKitErrorCannotShowMIMEType, response.url().string(), WEB_UI_STRING("Content with specified MIME type can\xE2\x80\x99t be shown", "WebKitErrorCannotShowMIMEType description"));
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError(String(WebURLErrorDomain), -1100, response.url().string(), String("File does not exist."));
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorPlugInWillHandleLoad, response.url().string(), WEB_UI_STRING("Plug-in handled load", "WebKitErrorPlugInWillHandleLoad description"));
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error)
{
    if (error.errorCode() == WebURLErrorCancelled && error.domain() == String(WebURLErrorDomain))
        return false;

    if (error.errorCode() == WebKitErrorPlugInWillHandleLoad && error.domain() == String(WebKitErrorDomain))
        return false;

    return true;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest& request) const
{
    return WebView::canHandleRequest(request);
}

bool WebFrameLoaderClient::canShowMIMEType(const String& mimeType) const
{
    return m_webFrame->webView()->canShowMIMEType(mimeType);
}

bool WebFrameLoaderClient::canShowMIMETypeAsHTML(const String& mimeType) const
{
    return m_webFrame->webView()->canShowMIMETypeAsHTML(mimeType);
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& /*URLScheme*/) const
{
    notImplemented();
    return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& /*URLScheme*/) const
{
    notImplemented();
    ASSERT_NOT_REACHED();
    return String();
}

void WebFrameLoaderClient::frameLoadCompleted()
{
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem*)
{
}

void WebFrameLoaderClient::restoreViewState()
{
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    notImplemented();
}

void WebFrameLoaderClient::didFinishLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    notImplemented();
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebDocumentLoader> loader = WebDocumentLoader::create(request, substituteData);

    COMPtr<WebDataSource> dataSource(AdoptCOM, WebDataSource::createInstance(loader.get()));

    loader->setDataSource(dataSource.get());
    return loader.release();
}

void WebFrameLoaderClient::setTitle(const StringWithDirection& title, const URL& url)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebHistoryDelegate> historyDelegate;
    webView->historyDelegate(&historyDelegate);
    if (historyDelegate) {
        BString titleBSTR(title.string());
        BString urlBSTR(url.string());
        historyDelegate->updateHistoryTitle(webView, titleBSTR, urlBSTR);
        return;
    }

    BOOL privateBrowsingEnabled = FALSE;
    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(m_webFrame->webView()->preferences(&preferences)))
        preferences->privateBrowsingEnabled(&privateBrowsingEnabled);
    if (privateBrowsingEnabled)
        return;

    // update title in global history
    COMPtr<WebHistory> history = webHistory();
    if (!history)
        return;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(history->itemForURL(BString(url.string()), &item)))
        return;

    COMPtr<IWebHistoryItemPrivate> itemPrivate(Query, item);
    if (!itemPrivate)
        return;

    itemPrivate->setTitle(BString(title.string()));
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame* cachedFrame)
{
#if USE(CFNETWORK)
    Frame* coreFrame = core(m_webFrame);
    if (!coreFrame)
        return;

    ASSERT(coreFrame->loader().documentLoader() == cachedFrame->documentLoader());

    cachedFrame->setCachedFramePlatformData(std::make_unique<WebCachedFramePlatformData>(static_cast<IWebDataSource*>(getWebDataSource(coreFrame->loader().documentLoader()))));
#else
    notImplemented();
#endif
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    WebView* view = m_webFrame->webView();

    RECT rect;
    view->frameRect(&rect);
    bool transparent = view->transparent();
    Color backgroundColor = transparent ? Color::transparent : Color::white;
    core(m_webFrame)->createView(IntRect(rect).size(), backgroundColor, transparent);
}

void WebFrameLoaderClient::didSaveToPageCache()
{
}

void WebFrameLoaderClient::didRestoreFromPageCache()
{
}

void WebFrameLoaderClient::dispatchDidBecomeFrameset(bool)
{
}

String WebFrameLoaderClient::userAgent(const URL& url)
{
    return m_webFrame->webView()->userAgentForKURL(url);
}

bool WebFrameLoaderClient::canCachePage() const
{
    return true;
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const URL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                            const String& referrer, bool /*allowsScrolling*/, int /*marginWidth*/, int /*marginHeight*/)
{
    RefPtr<Frame> result = createFrame(url, name, ownerElement, referrer);
    if (!result)
        return 0;
    return result.release();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const URL& URL, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<WebFrame> webFrame(AdoptCOM, WebFrame::createInstance());

    RefPtr<Frame> childFrame = webFrame->createSubframeWithOwnerElement(m_webFrame->webView(), coreFrame->page(), ownerElement);

    childFrame->tree().setName(name);
    coreFrame->tree().appendChild(childFrame);
    childFrame->init();

    coreFrame->loader().loadURLIntoChildFrame(URL, referrer, childFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree().parent())
        return 0;

    return childFrame.release();
}

ObjectContentType WebFrameLoaderClient::objectContentType(const URL& url, const String& mimeType, bool shouldPreferPlugInsForImages)
{
    return WebCore::FrameLoader::defaultObjectContentType(url, mimeType, shouldPreferPlugInsForImages);
}

void WebFrameLoaderClient::dispatchDidFailToStartPlugin(const PluginView* pluginView) const
{
    WebView* webView = m_webFrame->webView();

    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    RetainPtr<CFMutableDictionaryRef> userInfo = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    Frame* frame = core(m_webFrame);
    ASSERT(frame == pluginView->parentFrame());

    if (!pluginView->pluginsPage().isNull()) {
        URL pluginPageURL = frame->document()->completeURL(stripLeadingAndTrailingHTMLSpaces(pluginView->pluginsPage()));
        if (pluginPageURL.protocolIsInHTTPFamily()) {
            static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInPageURLStringKey);
            CFDictionarySetValue(userInfo.get(), key, pluginPageURL.string().createCFString().get());
        }
    }

    if (!pluginView->mimeType().isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorMIMETypeKey);
        CFDictionarySetValue(userInfo.get(), key, pluginView->mimeType().createCFString().get());
    }

    if (pluginView->plugin()) {
        String pluginName = pluginView->plugin()->name();
        if (!pluginName.isNull()) {
            static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInNameKey);
            CFDictionarySetValue(userInfo.get(), key, pluginName.createCFString().get());
        }
    }

    COMPtr<CFDictionaryPropertyBag> userInfoBag = CFDictionaryPropertyBag::createInstance();
    userInfoBag->setDictionary(userInfo.get());
 
    int errorCode = 0;
    String description;
    switch (pluginView->status()) {
        case PluginStatusCanNotFindPlugin:
            errorCode = WebKitErrorCannotFindPlugIn;
            description = WEB_UI_STRING("The plug-in can\xE2\x80\x99t be found", "WebKitErrorCannotFindPlugin description");
            break;
        case PluginStatusCanNotLoadPlugin:
            errorCode = WebKitErrorCannotLoadPlugIn;
            description = WEB_UI_STRING("The plug-in can\xE2\x80\x99t be loaded", "WebKitErrorCannotLoadPlugin description");
            break;
        default:
            ASSERT_NOT_REACHED();
    }

    ResourceError resourceError(String(WebKitErrorDomain), errorCode, pluginView->url().string(), String());
    COMPtr<IWebError> error(AdoptCOM, WebError::createInstance(resourceError, userInfoBag.get()));
     
    resourceLoadDelegate->plugInFailedWithError(webView, error.get(), getWebDataSource(frame->loader().documentLoader()));
}

PassRefPtr<Widget> WebFrameLoaderClient::createPlugin(const IntSize& pluginSize, HTMLPlugInElement* element, const URL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    WebView* webView = m_webFrame->webView();

    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(webView->uiDelegate(&ui)) && ui) {
        COMPtr<IWebUIDelegatePrivate> uiPrivate(Query, ui);

        if (uiPrivate) {
            // Assemble the view arguments in a property bag.
            HashMap<String, String> viewArguments;
            for (unsigned i = 0; i < paramNames.size(); i++) 
                viewArguments.set(paramNames[i], paramValues[i]);
            COMPtr<IPropertyBag> viewArgumentsBag(AdoptCOM, COMPropertyBag<String>::adopt(viewArguments));
            COMPtr<IDOMElement> containingElement(AdoptCOM, DOMElement::createInstance(element));

            HashMap<String, COMVariant> arguments;

            arguments.set(WebEmbeddedViewAttributesKey, viewArgumentsBag);
            arguments.set(WebEmbeddedViewBaseURLKey, url.string());
            arguments.set(WebEmbeddedViewContainingElementKey, containingElement);
            arguments.set(WebEmbeddedViewMIMETypeKey, mimeType);

            COMPtr<IPropertyBag> argumentsBag(AdoptCOM, COMPropertyBag<COMVariant>::adopt(arguments));

            COMPtr<IWebEmbeddedView> view;
            HRESULT result = uiPrivate->embeddedViewWithArguments(webView, m_webFrame, argumentsBag.get(), &view);
            if (SUCCEEDED(result)) {
                OLE_HANDLE parentWindow;
                HRESULT hr = webView->viewWindow(&parentWindow);
                ASSERT(SUCCEEDED(hr));

                return EmbeddedWidget::create(view.get(), element, reinterpret_cast<HWND>(parentWindow), pluginSize);
            }
        }
    }

    Frame* frame = core(m_webFrame);
    RefPtr<PluginView> pluginView = PluginView::create(frame, pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;

    dispatchDidFailToStartPlugin(pluginView.get());

    return 0;
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    // Ideally, this function shouldn't be necessary, see <rdar://problem/4852889>
    if (!pluginWidget || pluginWidget->isPluginView())
        m_manualLoader = toPluginView(pluginWidget);
    else 
        m_manualLoader = static_cast<EmbeddedWidget*>(pluginWidget);
}

PassRefPtr<Widget> WebFrameLoaderClient::createJavaAppletWidget(const IntSize& pluginSize, HTMLAppletElement* element, const URL& /*baseURL*/, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    RefPtr<PluginView> pluginView = PluginView::create(core(m_webFrame), pluginSize, element, URL(), paramNames, paramValues, "application/x-java-applet", false);

    // Check if the plugin can be loaded successfully
    if (pluginView->plugin() && pluginView->plugin()->load())
        return pluginView;

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return pluginView;

    COMPtr<CFDictionaryPropertyBag> userInfoBag = CFDictionaryPropertyBag::createInstance();

    ResourceError resourceError(String(WebKitErrorDomain), WebKitErrorJavaUnavailable, String(), WEB_UI_STRING("Java is unavailable", "WebKitErrorJavaUnavailable description"));
    COMPtr<IWebError> error(AdoptCOM, WebError::createInstance(resourceError, userInfoBag.get()));

    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    resourceLoadDelegate->plugInFailedWithError(webView, error.get(), getWebDataSource(coreFrame->loader().documentLoader()));

    return pluginView;
}

WebHistory* WebFrameLoaderClient::webHistory() const
{
    if (m_webFrame != m_webFrame->webView()->topLevelFrame())
        return 0;

    return WebHistory::sharedHistory();
}

String WebFrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    if (!coreFrame->settings().isScriptEnabled())
        return;

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (FAILED(webView->frameLoadDelegate(&frameLoadDelegate)))
        return;

    COMPtr<IWebFrameLoadDelegatePrivate2> delegatePrivate(Query, frameLoadDelegate);
    if (delegatePrivate && delegatePrivate->didClearWindowObjectForFrameInScriptWorld(webView, m_webFrame, WebScriptWorld::findOrCreateWorld(world).get()) != E_NOTIMPL)
        return;

    if (&world != &mainThreadNormalWorld())
        return;

    JSContextRef context = toRef(coreFrame->script().globalObject(world)->globalExec());
    JSObjectRef windowObject = toRef(coreFrame->script().globalObject(world));
    ASSERT(windowObject);

    if (FAILED(frameLoadDelegate->didClearWindowObject(webView, context, windowObject, m_webFrame)))
        frameLoadDelegate->windowScriptObjectAvailable(webView, context, windowObject);
}

void WebFrameLoaderClient::registerForIconNotification(bool listen)
{
    m_webFrame->webView()->registerForIconNotification(listen);
}

PassRefPtr<FrameNetworkingContext> WebFrameLoaderClient::createNetworkingContext()
{
    return WebFrameNetworkingContext::create(core(m_webFrame), userAgent(m_webFrame->url()));
}

bool WebFrameLoaderClient::shouldAlwaysUsePluginDocument(const String& mimeType) const
{
    WebView* webView = m_webFrame->webView();
    if (!webView)
        return false;

    return webView->shouldUseEmbeddedView(mimeType);
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    if (m_policyListenerPrivate->m_policyListener) {
        m_policyListenerPrivate->m_policyListener->invalidate();
        m_policyListenerPrivate->m_policyListener = 0;
    }

    m_policyListenerPrivate->m_policyFunction = nullptr;
}

COMPtr<WebFramePolicyListener> WebFrameLoaderClient::setUpPolicyListener(WebCore::FramePolicyFunction function)
{
    // FIXME: <rdar://5634381> We need to support multiple active policy listeners.

    if (m_policyListenerPrivate->m_policyListener)
        m_policyListenerPrivate->m_policyListener->invalidate();

    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    m_policyListenerPrivate->m_policyListener.adoptRef(WebFramePolicyListener::createInstance(coreFrame));
    m_policyListenerPrivate->m_policyFunction = function;

    return m_policyListenerPrivate->m_policyListener;
}

void WebFrameLoaderClient::receivedPolicyDecision(PolicyAction action)
{
    ASSERT(m_policyListenerPrivate->m_policyListener);
    ASSERT(m_policyListenerPrivate->m_policyFunction);

    FramePolicyFunction function = m_policyListenerPrivate->m_policyFunction;

    m_policyListenerPrivate->m_policyListener = 0;
    m_policyListenerPrivate->m_policyFunction = nullptr;

    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    function(action);
}
