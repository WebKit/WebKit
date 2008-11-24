/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebFrameLoaderClient.h"

#include "CFDictionaryPropertyBag.h"
#include "COMPropertyBag.h"
#include "EmbeddedWidget.h"
#include "MarshallingHelpers.h"
#include "WebCachedPagePlatformData.h"
#include "WebChromeClient.h"
#include "WebDocumentLoader.h"
#include "WebError.h"
#include "WebFrame.h"
#include "WebHistory.h"
#include "WebMutableURLRequest.h"
#include "WebNotificationCenter.h"
#include "WebScriptDebugServer.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLResponse.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/CachedPage.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/Page.h>
#include <WebCore/PluginPackage.h>
#include <WebCore/PluginView.h>
#include <WebCore/RenderPart.h>
#include <WebCore/ResourceHandle.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

static WebDataSource* getWebDataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoader*>(loader)->dataSource() : 0;
}

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame* webFrame)
    : m_webFrame(webFrame)
    , m_pluginView(0) 
    , m_hasSentResponseToPlugin(false) 
{
    ASSERT_ARG(webFrame, webFrame);
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_webFrame->webView();
}

void WebFrameLoaderClient::forceLayout()
{
    core(m_webFrame)->forceLayout(true);
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

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    ASSERT(challenge.sourceHandle());

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<WebURLAuthenticationChallenge> webChallenge(AdoptCOM, WebURLAuthenticationChallenge::createInstance(challenge));
        if (SUCCEEDED(resourceLoadDelegate->didReceiveAuthenticationChallenge(webView, identifier, webChallenge.get(), getWebDataSource(loader))))
            return;
    }

    // If the ResourceLoadDelegate doesn't exist or fails to handle the call, we tell the ResourceHandle
    // to continue without credential - this is the best approximation of Mac behavior
    challenge.sourceHandle()->receivedRequestToContinueWithoutCredential(challenge);
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

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& url, double delay, double fireDate)
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

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveTitle(webView, BString(title), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCommitLoadForFrame(webView, m_webFrame);
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

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFirstLayoutInFrame(webView, m_webFrame);
}

Frame* WebFrameLoaderClient::dispatchCreatePage()
{
    WebView* webView = m_webFrame->webView();

    COMPtr<IWebUIDelegate> ui;
    if (FAILED(webView->uiDelegate(&ui)))
        return 0;

    COMPtr<IWebView> newWebView;
    if (FAILED(ui->createWebViewWithRequest(webView, 0, &newWebView)))
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

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader*)
{
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_pluginView)
        return;

    if (m_pluginView->status() == PluginStatusLoadedSuccessfully)
        m_pluginView->didFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void WebFrameLoaderClient::postProgressStartedNotification()
{
    static BSTR progressStartedName = SysAllocString(WebViewProgressStartedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressStartedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification()
{
    static BSTR progressEstimateChangedName = SysAllocString(WebViewProgressEstimateChangedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressEstimateChangedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::postProgressFinishedNotification()
{
    static BSTR progressFinishedName = SysAllocString(WebViewProgressFinishedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressFinishedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    // FIXME: This should probably go through the data source.
    const String& textEncoding = loader->response().textEncodingName();

    if (!m_pluginView)
        receivedData(data, length, textEncoding);

    if (!m_pluginView || m_pluginView->status() != PluginStatusLoadedSuccessfully)
        return;

    if (!m_hasSentResponseToPlugin) {
        m_pluginView->didReceiveResponse(core(m_webFrame)->loader()->documentLoader()->response());
        // didReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
        // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
        // to null
        if (!m_pluginView)
            return;
        m_hasSentResponseToPlugin = true;
    }
    m_pluginView->didReceiveData(data, length);
}

void WebFrameLoaderClient::receivedData(const char* data, int length, const String& textEncoding)
{
    Frame* coreFrame = core(m_webFrame);
    if (!coreFrame)
        return;

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = coreFrame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    coreFrame->loader()->setEncoding(encoding, userChosen);

    coreFrame->loader()->addData(data, length);
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    // Telling the frame we received some data and passing 0 as the data is our
    // way to get work done that is normally done when the first bit of data is
    // received, even for the case of a document with no data (like about:blank)
    if (!m_pluginView) {
        committedLoad(loader, 0, 0);
        return;
    }

    if (m_pluginView->status() == PluginStatusLoadedSuccessfully)
        m_pluginView->didFinishLoading();
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void WebFrameLoaderClient::updateGlobalHistory(const KURL& url)
{
    WebHistory* history = WebHistory::sharedHistory();
    if (!history)
        return;
    DocumentLoader* loader = core(m_webFrame)->loader()->documentLoader();
    history->addItem(url, loader->title(), loader->urlForHistoryReflectsFailure());                 
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const
{
    return true;
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebDocumentLoader> loader = WebDocumentLoader::create(request, substituteData);

    COMPtr<WebDataSource> dataSource(AdoptCOM, WebDataSource::createInstance(loader.get()));

    loader->setDataSource(dataSource.get());
    return loader.release();
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& url)
{
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

    itemPrivate->setTitle(BString(title));
}

void WebFrameLoaderClient::savePlatformDataToCachedPage(CachedPage* cachedPage)
{
    Frame* coreFrame = core(m_webFrame);
    if (!coreFrame)
        return;

    ASSERT(coreFrame->loader()->documentLoader() == cachedPage->documentLoader());

    WebCachedPagePlatformData* webPlatformData = new WebCachedPagePlatformData(static_cast<IWebDataSource*>(getWebDataSource(coreFrame->loader()->documentLoader())));
    cachedPage->setCachedPagePlatformData(webPlatformData);
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    WebView* view = m_webFrame->webView();

    RECT rect;
    view->frameRect(&rect);
    bool transparent = view->transparent();
    Color backgroundColor = transparent ? Color::transparent : Color::white;
    WebCore::FrameLoaderClient::transitionToCommittedForNewPage(core(m_webFrame), IntRect(rect).size(), backgroundColor, transparent);
}

bool WebFrameLoaderClient::canCachePage() const
{
    return true;
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                            const String& referrer, bool /*allowsScrolling*/, int /*marginWidth*/, int /*marginHeight*/)
{
    RefPtr<Frame> result = createFrame(url, name, ownerElement, referrer);
    if (!result)
        return 0;
    return result.release();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<WebFrame> webFrame(AdoptCOM, WebFrame::createInstance());

    RefPtr<Frame> childFrame = webFrame->init(m_webFrame->webView(), coreFrame->page(), ownerElement);

    coreFrame->tree()->appendChild(childFrame);
    childFrame->tree()->setName(name);
    childFrame->init();

    loadURLIntoChild(URL, referrer, webFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void WebFrameLoaderClient::loadURLIntoChild(const KURL& originalURL, const String& referrer, WebFrame* childFrame)
{
    ASSERT(childFrame);
    ASSERT(core(childFrame));

    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    HistoryItem* parentItem = coreFrame->loader()->currentHistoryItem();
    FrameLoadType loadType = coreFrame->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirectWithLockedHistory;

    KURL url = originalURL;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (parentItem && parentItem->children().size() &&
        (isBackForwardLoadType(loadType)
         || loadType == FrameLoadTypeReload
         || loadType == FrameLoadTypeReloadAllowingStaleData))
    {
        if (HistoryItem* childItem = parentItem->childItemWithName(core(childFrame)->tree()->name())) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            url = childItem->originalURL();
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any child items as child frames load
                core(childFrame)->loader()->setProvisionalHistoryItem(childItem);
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                core(childFrame)->loader()->setCurrentHistoryItem(childItem);
        }
    }

    // FIXME: Handle loading WebArchives here
    String frameName = core(childFrame)->tree()->name();
    core(childFrame)->loader()->loadURL(url, referrer, frameName, childLoadType, 0, 0);
}

Widget* WebFrameLoaderClient::createPlugin(const IntSize& pluginSize, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    WebView* webView = m_webFrame->webView();

    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(webView->uiDelegate(&ui)) && ui) {
        COMPtr<IWebUIDelegatePrivate4> uiPrivate(Query, ui);

        if (uiPrivate) {
            // Assemble the view arguments in a property bag.
            HashMap<String, String> viewArguments;
            for (unsigned i = 0; i < paramNames.size(); i++) 
                viewArguments.set(paramNames[i], paramValues[i]);
            COMPtr<IPropertyBag> viewArgumentsBag(AdoptCOM, COMPropertyBag<String>::adopt(viewArguments));

            // Now create a new property bag where the view arguments is the only property.
            HashMap<String, COMPtr<IUnknown> > arguments;
            arguments.set(WebEmbeddedViewAttributesKey, COMPtr<IUnknown>(AdoptCOM, viewArgumentsBag.releaseRef()));
            COMPtr<IPropertyBag> argumentsBag(AdoptCOM, COMPropertyBag<COMPtr<IUnknown> >::adopt(arguments));

            COMPtr<IWebEmbeddedView> view;
            HRESULT result = uiPrivate->embeddedViewWithArguments(webView, m_webFrame, argumentsBag.get(), &view);
            if (SUCCEEDED(result)) {
                HWND parentWindow;
                HRESULT hr = webView->viewWindow((OLE_HANDLE*)&parentWindow);
                ASSERT(SUCCEEDED(hr));

                return EmbeddedWidget::create(view.get(), element, parentWindow, pluginSize);
            }
        }
    }

    Frame* frame = core(m_webFrame);
    PluginView* pluginView = PluginView::create(frame, pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;

    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;

    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return pluginView;

    RetainPtr<CFMutableDictionaryRef> userInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    unsigned count = (unsigned)paramNames.size();
    for (unsigned i = 0; i < count; i++) {
        if (paramNames[i] == "pluginspage") {
            static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInPageURLStringKey);
            RetainPtr<CFStringRef> str(AdoptCF, paramValues[i].createCFString());
            CFDictionarySetValue(userInfo.get(), key, str.get());
            break;
        }
    }

    if (!mimeType.isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorMIMETypeKey);

        RetainPtr<CFStringRef> str(AdoptCF, mimeType.createCFString());
        CFDictionarySetValue(userInfo.get(), key, str.get());
    }

    String pluginName;
    if (pluginView->plugin())
        pluginName = pluginView->plugin()->name();
    if (!pluginName.isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInNameKey);
        RetainPtr<CFStringRef> str(AdoptCF, pluginName.createCFString());
        CFDictionarySetValue(userInfo.get(), key, str.get());
    }

    COMPtr<CFDictionaryPropertyBag> userInfoBag(AdoptCOM, CFDictionaryPropertyBag::createInstance());
    userInfoBag->setDictionary(userInfo.get());
 
    int errorCode = 0;
    switch (pluginView->status()) {
        case PluginStatusCanNotFindPlugin:
            errorCode = WebKitErrorCannotFindPlugIn;
            break;
        case PluginStatusCanNotLoadPlugin:
            errorCode = WebKitErrorCannotLoadPlugIn;
            break;
        default:
            ASSERT_NOT_REACHED();
    }

    ResourceError resourceError(String(WebKitErrorDomain), errorCode, url.string(), String());
    COMPtr<IWebError> error(AdoptCOM, WebError::createInstance(resourceError, userInfoBag.get()));
     
    resourceLoadDelegate->plugInFailedWithError(webView, error.get(), getWebDataSource(frame->loader()->documentLoader()));

    return pluginView;
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    // Ideally, this function shouldn't be necessary, see <rdar://problem/4852889>

    m_pluginView = static_cast<PluginView*>(pluginWidget);
}

WebHistory* WebFrameLoaderClient::webHistory() const
{
    if (m_webFrame != m_webFrame->webView()->topLevelFrame())
        return 0;

    return WebHistory::sharedHistory();
}
