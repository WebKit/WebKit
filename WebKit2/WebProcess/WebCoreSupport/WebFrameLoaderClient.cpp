/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebFrameLoaderClient.h"

#include "NotImplemented.h"
#include "WebCoreTypeArgumentMarshalling.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebNavigationDataStore.h"
#include "WebPage.h"
#include "WebPageProxyMessageKinds.h"
#include "WebProcess.h"
#include <WebCore/Chrome.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/Page.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/Widget.h>
#include <WebCore/WindowFeatures.h>

using namespace WebCore;

namespace WebKit {

void WebFrameLoaderClient::frameLoaderDestroyed()
{
    m_frame->invalidate();

    // Balances explicit ref() in WebFrame::createMainFrame and WebFrame::createSubframe.
    m_frame->deref();
}

bool WebFrameLoaderClient::hasHTMLView() const
{
    return true;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_frame->page();
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayout()
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

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();
}


void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse)
{
    notImplemented();
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&)    
{
    notImplemented();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long identifier, const ProtectionSpace&)
{
    notImplemented();
    return false;
}
#endif

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&)
{
    notImplemented();
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidLoadResourceByXMLHttpRequest(unsigned long identifier, const ScriptString&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveServerRedirectForProvisionalLoadForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL&, double interval, double fireDate)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* provisionalLoader = m_frame->coreFrame()->loader()->provisionalDocumentLoader();
    const String& url = provisionalLoader->url().string();

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidStartProvisionalLoadForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID(), url));
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveTitleForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID(), title));
}

void WebFrameLoaderClient::dispatchDidChangeIcons()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidCommitLoadForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError&)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidFailProvisionalLoadForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError&)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidFailLoadForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidFinishLoadForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidFirstLayoutForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidFirstVisuallyNonEmptyLayoutForFrame, webPage->pageID(), CoreIPC::In(m_frame->frameID()));
}

Frame* WebFrameLoaderClient::dispatchCreatePage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return 0;

    // Just call through to the chrome client.
    Page* newPage = webPage->corePage()->chrome()->createWindow(m_frame->coreFrame(), FrameLoadRequest(), WindowFeatures());
    if (!newPage)
        return 0;
    
    return newPage->mainFrame();
}

void WebFrameLoaderClient::dispatchShow()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->show();
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const String& MIMEType, const ResourceRequest& request)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    uint64_t listenerID = m_frame->setUpPolicyListener(function);
    const String& url = request.url().string(); // FIXME: Pass entire request.

    WebProcess::shared().connection()->send(WebPageProxyMessage::DecidePolicyForMIMEType, webPage->pageID(),
                                            CoreIPC::In(m_frame->frameID(), MIMEType, url, listenerID));
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction& navigationAction, const ResourceRequest& request, PassRefPtr<FormState>, const String& frameName)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    uint64_t listenerID = m_frame->setUpPolicyListener(function);

    // FIXME: Pass more than just the navigation action type.
    // FIXME: Pass the frame name.
    const String& url = request.url().string(); // FIXME: Pass entire request.

    WebProcess::shared().connection()->send(WebPageProxyMessage::DecidePolicyForNewWindowAction, webPage->pageID(),
                                            CoreIPC::In(m_frame->frameID(), (uint32_t)navigationAction.type(), url, listenerID));
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& navigationAction, const ResourceRequest& request, PassRefPtr<FormState>)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    uint64_t listenerID = m_frame->setUpPolicyListener(function);

    // FIXME: Pass more than just the navigation action type.
    const String& url = request.url().string(); // FIXME: Pass entire request.

    WebProcess::shared().connection()->send(WebPageProxyMessage::DecidePolicyForNavigationAction, webPage->pageID(),
                                            CoreIPC::In(m_frame->frameID(), (uint32_t)navigationAction.type(), url, listenerID));
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    m_frame->invalidatePolicyListener();
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState>)
{
    notImplemented();

    Frame* coreFrame = m_frame->coreFrame();
    (coreFrame->loader()->policyChecker()->*function)(PolicyUse);
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::willChangeEstimatedProgress()
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeEstimatedProgress()
{
    notImplemented();
}

void WebFrameLoaderClient::postProgressStartedNotification()
{
    if (WebPage* webPage = m_frame->page())
        WebProcess::shared().connection()->send(WebPageProxyMessage::DidStartProgress, webPage->pageID(), CoreIPC::In());
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification()
{
    if (WebPage* webPage = m_frame->page()) {
        double progress = webPage->corePage()->progress()->estimatedProgress();
        WebProcess::shared().connection()->send(WebPageProxyMessage::DidChangeProgress, webPage->pageID(), CoreIPC::In(progress));
    }
}

void WebFrameLoaderClient::postProgressFinishedNotification()
{
    if (WebPage* webPage = m_frame->page())
        WebProcess::shared().connection()->send(WebPageProxyMessage::DidFinishProgress, webPage->pageID(), CoreIPC::In());
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrameLoaderClient::startDownload(const ResourceRequest&)
{
    notImplemented();
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
    const String& textEncoding = loader->response().textEncodingName();
    
    receivedData(data, length, textEncoding);
}

void WebFrameLoaderClient::receivedData(const char* data, int length, const String& textEncoding)
{
    Frame* coreFrame = m_frame->coreFrame();
    if (!coreFrame)
        return;
    
    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = coreFrame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    coreFrame->loader()->writer()->setEncoding(encoding, userChosen);
    
    coreFrame->loader()->addData(data, length);
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    committedLoad(loader, 0, 0);
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader()->documentLoader();

    WebNavigationDataStore data;
    data.url = loader->urlForHistory().string();
    data.title = loader->title();

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidNavigateWithNavigationData,
                                            webPage->pageID(),
                                            CoreIPC::In(data, m_frame->frameID()));
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader()->documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    // Client redirect
    if (!loader->clientRedirectSourceForHistory().isNull()) {
        WebProcess::shared().connection()->send(WebPageProxyMessage::DidPerformClientRedirect,
                                                webPage->pageID(),
                                                CoreIPC::In(loader->clientRedirectSourceForHistory(), 
                                                            loader->clientRedirectDestinationForHistory(),
                                                            m_frame->frameID()));
    }

    // Server redirect
    if (!loader->serverRedirectSourceForHistory().isNull()) {
        WebProcess::shared().connection()->send(WebPageProxyMessage::DidPerformServerRedirect,
                                                webPage->pageID(),
                                                CoreIPC::In(loader->serverRedirectSourceForHistory(),
                                                            loader->serverRedirectDestinationForHistory(),
                                                            m_frame->frameID()));
    }
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const
{
    notImplemented();
    return true;
}

void WebFrameLoaderClient::dispatchDidAddBackForwardItem(HistoryItem*) const
{
    if (WebPage* webPage = m_frame->page())
        webPage->backForwardListDidChange();
}

void WebFrameLoaderClient::dispatchDidRemoveBackForwardItem(HistoryItem*) const
{
    if (WebPage* webPage = m_frame->page())
        webPage->backForwardListDidChange();
}

void WebFrameLoaderClient::dispatchDidChangeBackForwardIndex() const
{
    if (WebPage* webPage = m_frame->page())
        webPage->backForwardListDidChange();
}

void WebFrameLoaderClient::didDisplayInsecureContent()
{
    notImplemented();
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin*)
{
    notImplemented();
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    return WebKit::cancelledError(request);
}

ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return WebKit::blockedError(request);
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return WebKit::cannotShowURLError(request);
}

ResourceError WebFrameLoaderClient::interruptForPolicyChangeError(const ResourceRequest& request)
{
    return WebKit::interruptForPolicyChangeError(request);
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return WebKit::cannotShowMIMETypeError(response);
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return WebKit::fileDoesNotExistError(response);
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError&)
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMEType(const String& MIMEType) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    notImplemented();
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void WebFrameLoaderClient::restoreViewState()
{
    notImplemented();
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

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& data)
{
    return DocumentLoader::create(request, data);
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidUpdateHistoryTitle, webPage->pageID(), CoreIPC::In(title, url.string(), m_frame->frameID()));
}

String WebFrameLoaderClient::userAgent(const KURL&)
{
    notImplemented();
    return "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6; en-us) AppleWebKit/531.4 (KHTML, like Gecko) Version/4.0.3 Safari/531.4";
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    notImplemented();
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    m_frame->coreFrame()->createView(m_frame->page()->size(), Color::white, false, IntSize(), false);
}

bool WebFrameLoaderClient::canCachePage() const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                                    const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    WebPage* webPage = m_frame->page();

    RefPtr<WebFrame> subframe = WebFrame::createSubframe(webPage, name, ownerElement);

    // Notify the UI process that subframe has been added.
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidCreateSubFrame, webPage->pageID(), CoreIPC::In(subframe->frameID()));

    Frame* coreSubframe = subframe->coreFrame();

     // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    m_frame->coreFrame()->loader()->loadURLIntoChildFrame(url, referrer, coreSubframe);

    // The frame's onload handler may have removed it from the document.
    if (!coreSubframe->tree()->parent())
        return 0;

    return coreSubframe;
}

void WebFrameLoaderClient::didTransferChildFrameToNewDocument()
{
    notImplemented();
}    

PassRefPtr<Widget> WebFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool loadManually)
{
    notImplemented();
    return 0;
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
}

PassRefPtr<Widget> WebFrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType WebFrameLoaderClient::objectContentType(const KURL& url, const String& mimeType)
{
    notImplemented();
    return ObjectContentNone;
}

String WebFrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*)
{
    notImplemented();
}

void WebFrameLoaderClient::documentElementAvailable()
{
    notImplemented();
}

void WebFrameLoaderClient::didPerformFirstNavigation() const
{
    notImplemented();
}

void WebFrameLoaderClient::registerForIconNotification(bool listen)
{
    notImplemented();
}

#if PLATFORM(MAC)
#if ENABLE(MAC_JAVA_BRIDGE)
jobject WebFrameLoaderClient::javaApplet(NSView*) { return 0; }
#endif
NSCachedURLResponse* WebFrameLoaderClient::willCacheResponse(DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const
{
    notImplemented();
    return 0;
}

#endif
#if USE(CFNETWORK)
bool WebFrameLoaderClient::shouldCacheResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&, const unsigned char* data, unsigned long long length)
{
    notImplemented();
    return false;
}

#endif

bool WebFrameLoaderClient::shouldUsePluginDocument(const String& /*mimeType*/) const
{
    notImplemented();
    return false;
}

} // namespace WebKit
