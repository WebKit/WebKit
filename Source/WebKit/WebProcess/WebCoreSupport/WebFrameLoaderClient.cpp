/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebFrameLoaderClient.h"

#include "AuthenticationManager.h"
#include "DataReference.h"
#include "DrawingArea.h"
#include "FindController.h"
#include "FormDataReference.h"
#include "FrameInfoData.h"
#include "IPCTestingAPI.h"
#include "InjectedBundle.h"
#include "InjectedBundleDOMWindowExtension.h"
#include "Logging.h"
#include "NavigationActionData.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "PluginView.h"
#include "UserData.h"
#include "WKBundleAPICast.h"
#include "WebAutomationSessionProxy.h"
#include "WebBackForwardListProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebDocumentLoader.h"
#include "WebErrors.h"
#include "WebEvent.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebFullScreenManager.h"
#include "WebHitTestResultData.h"
#include "WebLoaderStrategy.h"
#include "WebNavigationDataStore.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebsitePoliciesData.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObject.h>
#include <WebCore/CachedFrame.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaDocument.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/UIEventWithKeyState.h>
#include <WebCore/Widget.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ProcessID.h>
#include <wtf/ProcessPrivilege.h>

#if ENABLE(FULLSCREEN_API)
#include <WebCore/FullscreenManager.h>
#endif

#define PREFIX_PARAMETERS "%p - [webFrame=%p, webFrameID=%" PRIu64 ", webPage=%p, webPageID=%" PRIu64 "] WebFrameLoaderClient::"
#define WEBFRAME (&webFrame())
#define WEBFRAMEID (webFrame().frameID().object().toUInt64())
#define WEBPAGE (webFrame().page())
#define WEBPAGEID (WEBPAGE ? WEBPAGE->identifier().toUInt64() : 0)

#define WEBFRAMELOADERCLIENT_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, PREFIX_PARAMETERS fmt, this, WEBFRAME, WEBFRAMEID, WEBPAGE, WEBPAGEID, ##__VA_ARGS__)
#define WEBFRAMELOADERCLIENT_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, PREFIX_PARAMETERS fmt, this, WEBFRAME, WEBFRAMEID, WEBPAGE, WEBPAGEID, ##__VA_ARGS__)
#define WEBFRAMELOADERCLIENT_RELEASE_LOG_FAULT(channel, fmt, ...) RELEASE_LOG_FAULT(channel, PREFIX_PARAMETERS fmt, this, WEBFRAME, WEBFRAMEID, WEBPAGE, WEBPAGEID, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WebFrameLoaderClient::WebFrameLoaderClient(Ref<WebFrame>&& frame)
    : m_frame(WTFMove(frame))
{
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
    m_frame->invalidate();
}

std::optional<WebPageProxyIdentifier> WebFrameLoaderClient::webPageProxyID() const
{
    if (m_frame->page())
        return m_frame->page()->webPageProxyIdentifier();

    return std::nullopt;
}

std::optional<PageIdentifier> WebFrameLoaderClient::pageID() const
{
    if (m_frame->page())
        return m_frame->page()->identifier();

    return std::nullopt;
}

#if ENABLE(TRACKING_PREVENTION)
void WebFrameLoaderClient::setHasFrameSpecificStorageAccess(FrameSpecificStorageAccessIdentifier&& frameSpecificStorageAccessIdentifier )
{
    ASSERT(!m_frameSpecificStorageAccessIdentifier);

    m_frameSpecificStorageAccessIdentifier = WTFMove(frameSpecificStorageAccessIdentifier);
}

void WebFrameLoaderClient::didLoadFromRegistrableDomain(RegistrableDomain&& domain)
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;
    
    webPage->didLoadFromRegistrableDomain(WTFMove(domain));
}

Vector<RegistrableDomain> WebFrameLoaderClient::loadedSubresourceDomains() const
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return { };

    return copyToVector(webPage->loadedSubresourceDomains());
}

#endif

bool WebFrameLoaderClient::hasHTMLView() const
{
    return !m_frameHasCustomContentProvider;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_frame->page();
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
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(TRACKING_PREVENTION)
    if (m_frameSpecificStorageAccessIdentifier) {
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveStorageAccessForFrame(
            m_frameSpecificStorageAccessIdentifier->frameID, m_frameSpecificStorageAccessIdentifier->pageID), 0);
        m_frameSpecificStorageAccessIdentifier = std::nullopt;
    }
#endif

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didRemoveFrameFromHierarchy(*webPage, m_frame, userData);
}

void WebFrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    bool pageIsProvisionallyLoading = false;
    if (FrameLoader* frameLoader = loader ? loader->frameLoader() : nullptr)
        pageIsProvisionallyLoading = frameLoader->provisionalDocumentLoader() == loader;

    webPage->injectedBundleResourceLoadClient().didInitiateLoadForResource(*webPage, m_frame, identifier, request, pageIsProvisionallyLoading);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidInitiateLoadForResource(identifier, m_frame->frameID(), request));
#endif

    webPage->addResourceRequest(identifier, request);
}

void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // The API can return a completely new request. We should ensure that at least the requester
    // is kept, so that if this is a main resource load it's still considered as such.
    auto requester = request.requester();
    auto appInitiatedValue = request.isAppInitiated();
    webPage->injectedBundleResourceLoadClient().willSendRequestForFrame(*webPage, m_frame, identifier, request, redirectResponse);
    if (!request.isNull()) {
        request.setRequester(requester);
        request.setIsAppInitiated(appInitiatedValue);

#if PLATFORM(GTK) || PLATFORM(WPE)
        webPage->send(Messages::WebPageProxy::DidSendRequestForResource(identifier, m_frame->frameID(), request, redirectResponse));
#endif
    }
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier identifier)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return true;

    return webPage->injectedBundleResourceLoadClient().shouldUseCredentialStorage(*webPage, m_frame, identifier);
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, WebCore::ResourceLoaderIdentifier, const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, WebCore::ResourceLoaderIdentifier, const ProtectionSpace& protectionSpace)
{
    // The WebKit 2 Networking process asks the UIProcess directly, so the WebContent process should never receive this callback.
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didReceiveResponseForResource(*webPage, m_frame, identifier, response);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidReceiveResponseForResource(identifier, m_frame->frameID(), response));
#endif
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier identifier, int dataLength)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didReceiveContentLengthForResource(*webPage, m_frame, identifier, dataLength);
}

#if ENABLE(DATA_DETECTION)
void WebFrameLoaderClient::dispatchDidFinishDataDetection(NSArray *detectionResults)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->setDataDetectionResults(detectionResults);
}
#endif

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier identifier)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didFinishLoadForResource(*webPage, m_frame, identifier);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidFinishLoadForResource(identifier, m_frame->frameID(), { }));
#endif

    webPage->removeResourceRequest(identifier);
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier identifier, const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didFailLoadForResource(*webPage, m_frame, identifier, error);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidFinishLoadForResource(identifier, m_frame->frameID(), error));
#endif

    webPage->removeResourceRequest(identifier);
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int /*length*/)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidDispatchOnloadEvents()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didHandleOnloadEventsForFrame(*webPage, m_frame);
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebDocumentLoader* documentLoader = static_cast<WebDocumentLoader*>(m_frame->coreFrame()->loader().provisionalDocumentLoader());
    if (!documentLoader) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG_FAULT(Loading, "dispatchDidReceiveServerRedirectForProvisionalLoad: Called with no provisional DocumentLoader (frameState=%hhu, stateForDebugging=%i)", m_frame->coreFrame()->loader().state(), m_frame->coreFrame()->loader().stateMachine().stateForDebugging());
        return;
    }

    RefPtr<API::Object> userData;

    LOG(Loading, "WebProcess %i - dispatchDidReceiveServerRedirectForProvisionalLoad to request url %s", getCurrentProcessID(), documentLoader->request().url().string().utf8().data());

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didReceiveServerRedirectForProvisionalLoadForFrame(*webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame(m_frame->frameID(), documentLoader->navigationID(), documentLoader->request(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidChangeProvisionalURL()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebDocumentLoader& documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().provisionalDocumentLoader());
    webPage->send(Messages::WebPageProxy::DidChangeProvisionalURLForFrame(m_frame->frameID(), documentLoader.navigationID(), documentLoader.url()));
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCancelClientRedirectForFrame(*webPage, m_frame);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidCancelClientRedirectForFrame(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const URL& url, double interval, WallTime fireDate, LockBackForwardList lockBackForwardList)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().willPerformClientRedirectForFrame(*webPage, m_frame, url.string(), interval, fireDate);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::WillPerformClientRedirectForFrame(m_frame->frameID(), url.string(), interval, lockBackForwardList));
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(*webPage, m_frame, SameDocumentNavigationAnchorNavigation, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrame(m_frame->frameID(), navigationID, SameDocumentNavigationAnchorNavigation, m_frame->coreFrame()->document()->url(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidChangeMainDocument()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->send(Messages::WebPageProxy::DidChangeMainDocument(m_frame->frameID()));
}

void WebFrameLoaderClient::dispatchWillChangeDocument(const URL& currentUrl, const URL& newUrl)
{
#if ENABLE(TRACKING_PREVENTION)
    if (m_frame->isMainFrame())
        return;

    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frameSpecificStorageAccessIdentifier && !WebCore::areRegistrableDomainsEqual(currentUrl, newUrl)) {
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveStorageAccessForFrame(
            m_frameSpecificStorageAccessIdentifier->frameID, m_frameSpecificStorageAccessIdentifier->pageID), 0);
        m_frameSpecificStorageAccessIdentifier = std::nullopt;
    }
#endif
}

void WebFrameLoaderClient::didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType navigationType)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(*webPage, m_frame, SameDocumentNavigationSessionStatePush, userData);

    NavigationActionData navigationActionData;
    navigationActionData.userGestureTokenIdentifier = WebProcess::singleton().userGestureTokenIdentifier(UserGestureIndicator::currentUserGesture());
    navigationActionData.canHandleRequest = true;
    navigationActionData.treatAsSameOriginNavigation = true;

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrameViaJSHistoryAPI(m_frame->frameID(), navigationType, m_frame->coreFrame()->document()->url(), navigationActionData, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationSessionStatePush);
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationSessionStateReplace);
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationSessionStatePop);
}

void WebFrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidExplicitOpen(const URL& url, const String& mimeType)
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidExplicitOpenForFrame(m_frame->frameID(), url, mimeType));
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(FULLSCREEN_API)
    auto* document = m_frame->coreFrame()->document();
    if (document && document->fullscreenManager().fullscreenElement())
        webPage->fullScreenManager()->exitFullScreenForElement(webPage->fullScreenManager()->element());
#endif

    webPage->findController().hideFindUI();
    webPage->sandboxExtensionTracker().didStartProvisionalLoad(m_frame.ptr());


    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didStartProvisionalLoadForFrame(*webPage, m_frame, userData);
    RefPtr provisionalLoader = static_cast<WebDocumentLoader*>(m_frame->coreFrame()->loader().provisionalDocumentLoader());

    if (!provisionalLoader || provisionalLoader->isContinuingLoadAfterProvisionalLoadStarted())
        return;
    
    auto& url = provisionalLoader->url();
    auto& unreachableURL = provisionalLoader->unreachableURL();
    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidStartProvisionalLoadForFrame(m_frame->frameID(), m_frame->info(), provisionalLoader->request(), provisionalLoader->navigationID(), url, unreachableURL, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

void WebFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);
    
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    // FIXME: Use direction of title.
    webPage->injectedBundleLoaderClient().didReceiveTitleForFrame(*webPage, truncatedTitle.string, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidReceiveTitleForFrame(m_frame->frameID(), truncatedTitle.string, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent> hasInsecureContent, std::optional<UsedLegacyTLS> usedLegacyTLSFromPageCache, std::optional<WasPrivateRelayed> wasPrivateRelayedFromPageCache)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebDocumentLoader& documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader());
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCommitLoadForFrame(*webPage, m_frame, userData);

    webPage->sandboxExtensionTracker().didCommitProvisionalLoad(m_frame.ptr());

    bool usedLegacyTLS = documentLoader.response().usedLegacyTLS();
    if (!usedLegacyTLS && usedLegacyTLSFromPageCache)
        usedLegacyTLS = usedLegacyTLSFromPageCache == UsedLegacyTLS::Yes;

    bool wasPrivateRelayed = documentLoader.response().wasPrivateRelayed();
    if (!wasPrivateRelayed && wasPrivateRelayedFromPageCache)
        wasPrivateRelayed = wasPrivateRelayedFromPageCache == WasPrivateRelayed::Yes;
    
    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidCommitLoadForFrame(m_frame->frameID(), m_frame->info(), documentLoader.request(), documentLoader.navigationID(), documentLoader.response().mimeType(), m_frameHasCustomContentProvider, m_frame->coreFrame()->loader().loadType(), valueOrCompute(documentLoader.response().certificateInfo(), [] { return CertificateInfo(); }), usedLegacyTLS, wasPrivateRelayed, m_frame->coreFrame()->document()->isPluginDocument(), hasInsecureContent, documentLoader.mouseEventPolicy(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    webPage->didCommitLoad(m_frame.ptr());
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading willContinueLoading)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // When stopping the provisional load due to a process swap on resource response (due to COOP), the provisional load
    // will simply keep going in a new process. As a result, we don't want to tell the injected bundle or the UIProcess client
    // that the provisional load failed.
    if (webPage->isStoppingLoadingDueToProcessSwap())
        return;

    WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDidFailProvisionalLoad:");

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailProvisionalLoadWithErrorForFrame(*webPage, m_frame, error, userData);

    webPage->sandboxExtensionTracker().didFailProvisionalLoad(m_frame.ptr());

    // FIXME: This is gross. This is necessary because if the client calls WKBundlePageStopLoading() from within the didFailProvisionalLoadWithErrorForFrame
    // injected bundle client call, that will cause the provisional DocumentLoader to be disconnected from the Frame, and didDistroyNavigation message
    // to be sent to the UIProcess (and the destruction of the DocumentLoader). If that happens, and we had captured the navigationID before injected bundle 
    // client call, the DidFailProvisionalLoadForFrame would send a navigationID of a destroyed Navigation, and the UIProcess would not be able to find it
    // in its table.
    //
    // A better solution to this problem would be find a clean way to postpone the disconnection of the DocumentLoader from the Frame until
    // the entire FrameLoaderClient function was complete.
    uint64_t navigationID = 0;
    ResourceRequest request;
    if (auto documentLoader = m_frame->coreFrame()->loader().provisionalDocumentLoader()) {
        navigationID = static_cast<WebDocumentLoader*>(documentLoader)->navigationID();
        request = documentLoader->request();
    }

    // Notify the UIProcess.
    WebCore::Frame* coreFrame = m_frame->coreFrame();
    webPage->send(Messages::WebPageProxy::DidFailProvisionalLoadForFrame(m_frame->frameID(), m_frame->info(), request, navigationID, coreFrame->loader().provisionalLoadErrorBeingHandledURL().string(), error, willContinueLoading, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame.ptr(), error.isCancellation());
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDidFailLoad:");

    RefPtr<API::Object> userData;

    auto& documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader());
    auto navigationID = documentLoader.navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailLoadWithErrorForFrame(*webPage, m_frame, error, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFailLoadForFrame(m_frame->frameID(), m_frame->info(), documentLoader.request(), navigationID, error, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame.ptr(), error.isCancellation());
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishDocumentLoadForFrame(*webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFinishDocumentLoadForFrame(m_frame->frameID(), navigationID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    webPage->didFinishDocumentLoad(m_frame);
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    Ref documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreFrame()->loader().documentLoader());
    auto navigationID = documentLoader->navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishLoadForFrame(*webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFinishLoadForFrame(m_frame->frameID(), m_frame->info(), documentLoader->request(), navigationID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame.ptr());

    webPage->didFinishLoad(m_frame);
}

void WebFrameLoaderClient::completePageTransitionIfNeeded()
{
    if (m_didCompletePageTransition)
        return;

    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didCompletePageTransition();
    m_didCompletePageTransition = true;
    WEBFRAMELOADERCLIENT_RELEASE_LOG(Layout, "completePageTransitionIfNeeded: dispatching didCompletePageTransition");
}

void WebFrameLoaderClient::dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> milestones)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    if (milestones & DidFirstLayout) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Layout, "dispatchDidReachLayoutMilestone: dispatching DidFirstLayoutForFrame");

        // FIXME: We should consider removing the old didFirstLayout API since this is doing double duty with the
        // new didLayout API.
        webPage->injectedBundleLoaderClient().didFirstLayoutForFrame(*webPage, m_frame, userData);
        webPage->send(Messages::WebPageProxy::DidFirstLayoutForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

#if USE(COORDINATED_GRAPHICS)
        // Make sure viewport properties are dispatched on the main frame by the time the first layout happens.
        ASSERT(!webPage->useFixedLayout() || m_frame.ptr() != &m_frame->page()->mainWebFrame() || m_frame->coreFrame()->document()->didDispatchViewportPropertiesChanged());
#endif
    }

#if !RELEASE_LOG_DISABLED
    StringBuilder builder;
    auto addIfSet = [&milestones, &builder] (WebCore::LayoutMilestone milestone, const String& toAdd) {
        if (milestones.contains(milestone)) {
            if (!builder.isEmpty())
                builder.append(", ");
            builder.append(toAdd);
        }
    };

    addIfSet(DidFirstLayout, "DidFirstLayout"_s);
    addIfSet(DidFirstVisuallyNonEmptyLayout, "DidFirstVisuallyNonEmptyLayout"_s);
    addIfSet(DidHitRelevantRepaintedObjectsAreaThreshold, "DidHitRelevantRepaintedObjectsAreaThreshold"_s);
    addIfSet(DidFirstFlushForHeaderLayer, "DidFirstFlushForHeaderLayer"_s);
    addIfSet(DidFirstLayoutAfterSuppressedIncrementalRendering, "DidFirstLayoutAfterSuppressedIncrementalRendering"_s);
    addIfSet(DidFirstPaintAfterSuppressedIncrementalRendering, "DidFirstPaintAfterSuppressedIncrementalRendering"_s);
    addIfSet(ReachedSessionRestorationRenderTreeSizeThreshold, "ReachedSessionRestorationRenderTreeSizeThreshold"_s);
    addIfSet(DidRenderSignificantAmountOfText, "DidRenderSignificantAmountOfText"_s);
    addIfSet(DidFirstMeaningfulPaint, "DidFirstMeaningfulPaint"_s);

    WEBFRAMELOADERCLIENT_RELEASE_LOG(Layout, "dispatchDidReachLayoutMilestone: dispatching DidReachLayoutMilestone (milestones=%" PUBLIC_LOG_STRING ")", builder.toString().utf8().data());
#endif

    // Send this after DidFirstLayout-specific calls since some clients expect to get those messages first.
    webPage->dispatchDidReachLayoutMilestone(milestones);

    if (milestones & DidFirstVisuallyNonEmptyLayout) {
        ASSERT(!m_frame->isMainFrame() || webPage->corePage()->settings().suppressesIncrementalRendering() || m_didCompletePageTransition);
        // FIXME: We should consider removing the old didFirstVisuallyNonEmptyLayoutForFrame API since this is doing double duty with the new didLayout API.
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Layout, "dispatchDidReachLayoutMilestone: dispatching DidFirstVisuallyNonEmptyLayoutForFrame");
        webPage->injectedBundleLoaderClient().didFirstVisuallyNonEmptyLayoutForFrame(*webPage, m_frame, userData);
        webPage->send(Messages::WebPageProxy::DidFirstVisuallyNonEmptyLayoutForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    }
}

void WebFrameLoaderClient::dispatchDidReachVisuallyNonEmptyState()
{
    if (!m_frame->page() || m_frame->page()->corePage()->settings().suppressesIncrementalRendering())
        return;
    ASSERT(m_frame->isMainFrame());
    completePageTransitionIfNeeded();
}

void WebFrameLoaderClient::dispatchDidLayout()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didLayoutForFrame(*webPage, m_frame);

    webPage->recomputeShortCircuitHorizontalWheelEventsState();

#if PLATFORM(IOS_FAMILY)
    webPage->updateSelectionAppearance();
#endif

    // NOTE: Unlike the other layout notifications, this does not notify the
    // the UIProcess for every call.

    if (m_frame.ptr() == &m_frame->page()->mainWebFrame()) {
        // FIXME: Remove at the soonest possible time.
        webPage->send(Messages::WebPageProxy::SetRenderTreeSize(webPage->renderTreeSize()));
        webPage->mainFrameDidLayout();
    }
}

Frame* WebFrameLoaderClient::dispatchCreatePage(const NavigationAction& navigationAction, NewFrameOpenerPolicy newFrameOpenerPolicy)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return nullptr;

    // Just call through to the chrome client.
    WindowFeatures windowFeatures;
    windowFeatures.noopener = newFrameOpenerPolicy == NewFrameOpenerPolicy::Suppress;
    Page* newPage = webPage->corePage()->chrome().createWindow(*m_frame->coreFrame(), windowFeatures, navigationAction);
    if (!newPage)
        return nullptr;
    
    return &newPage->mainFrame();
}

void WebFrameLoaderClient::dispatchShow()
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->show();
}

void WebFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest& request, WebCore::PolicyCheckIdentifier identifier, const String& downloadAttribute, FramePolicyFunction&& function)
{
    auto* webPage = m_frame->page();
    if (!webPage) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: ignoring because there's no web page");
        function(PolicyAction::Ignore, identifier);
        return;
    }

    if (!request.url().string()) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: continuing because the url string is null");
        function(PolicyAction::Use, identifier);
        return;
    }

    if (webPage->shouldSkipDecidePolicyForResponse(response, request)) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: continuing because injected bundle says so");
        function(PolicyAction::Use, identifier);
        return;
    }

    bool canShowResponse = webPage->canShowResponse(response);

    auto* coreFrame = m_frame->coreFrame();
    auto* policyDocumentLoader = coreFrame ? coreFrame->loader().provisionalDocumentLoader() : nullptr;
    auto navigationID = policyDocumentLoader ? static_cast<WebDocumentLoader&>(*policyDocumentLoader).navigationID() : 0;

    auto protector = m_frame.copyRef();
    uint64_t listenerID = m_frame->setUpPolicyListener(identifier, WTFMove(function), WebFrame::ForNavigationAction::No);
    if (!webPage->send(Messages::WebPageProxy::DecidePolicyForResponse(m_frame->frameID(), m_frame->info(), identifier, navigationID, response, request, canShowResponse, downloadAttribute, listenerID))) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: ignoring because WebPageProxy::DecidePolicyForResponse failed");
        m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { identifier, std::nullopt, PolicyAction::Ignore, 0, { }, { } });
    }
}

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
static void setWebHitTestResultDataInNavigationActionDataIfNecessary(const NavigationAction& navigationAction, NavigationActionData& navigationActionData, WebCore::Frame& coreFrame)
{
    auto mouseEventData = navigationAction.mouseEventData();
    if (!mouseEventData)
        return;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitTestResult = coreFrame.eventHandler().hitTestResultAtPoint(mouseEventData->absoluteLocation, hitType);

    WebKit::WebHitTestResultData webHitTestResultData(hitTestResult, false);
    navigationActionData.webHitTestResultData = WTFMove(webHitTestResultData);
}
#endif

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction& navigationAction, const ResourceRequest& request,
    FormState* formState, const String& frameName, WebCore::PolicyCheckIdentifier identifier, FramePolicyFunction&& function)
{
    auto* webPage = m_frame->page();
    if (!webPage) {
        function(PolicyAction::Ignore, identifier);
        return;
    }

    uint64_t listenerID = m_frame->setUpPolicyListener(identifier, WTFMove(function), WebFrame::ForNavigationAction::No);

    NavigationActionData navigationActionData;
    navigationActionData.navigationType = navigationAction.type();
    navigationActionData.modifiers = modifiersForNavigationAction(navigationAction);
    navigationActionData.mouseButton = mouseButton(navigationAction);
    navigationActionData.syntheticClickType = syntheticClickType(navigationAction);
    if (auto& data = navigationAction.mouseEventData())
        navigationActionData.clickLocationInRootViewCoordinates = data->locationInRootViewCoordinates;
    navigationActionData.userGestureTokenIdentifier = WebProcess::singleton().userGestureTokenIdentifier(navigationAction.userGestureToken());
    navigationActionData.canHandleRequest = webPage->canHandleRequest(request);
    navigationActionData.shouldOpenExternalURLsPolicy = navigationAction.shouldOpenExternalURLsPolicy();
    navigationActionData.downloadAttribute = navigationAction.downloadAttribute();
    navigationActionData.privateClickMeasurement = navigationAction.privateClickMeasurement();

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    if (auto* coreFrame = m_frame->coreFrame())
        setWebHitTestResultDataInNavigationActionDataIfNecessary(navigationAction, navigationActionData, *coreFrame);
#endif

    webPage->send(Messages::WebPageProxy::DecidePolicyForNewWindowAction(m_frame->frameID(), m_frame->info(), identifier, navigationActionData, request,
        frameName, listenerID));
}

void WebFrameLoaderClient::applyToDocumentLoader(WebsitePoliciesData&& websitePolicies)
{
    auto* coreFrame = m_frame->coreFrame();
    if (!coreFrame)
        return;
    WebDocumentLoader* documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().policyDocumentLoader());
    if (!documentLoader)
        documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().provisionalDocumentLoader());
    if (!documentLoader)
        documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().documentLoader());
    if (!documentLoader)
        return;

    WebsitePoliciesData::applyToDocumentLoader(WTFMove(websitePolicies), *documentLoader);
}

WebCore::AllowsContentJavaScript WebFrameLoaderClient::allowsContentJavaScriptFromMostRecentNavigation() const
{
    auto* webPage = m_frame->page();
    return webPage ? webPage->allowsContentJavaScriptFromMostRecentNavigation() : WebCore::AllowsContentJavaScript::No;
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& navigationAction, const ResourceRequest& request, const ResourceResponse& redirectResponse,
    FormState* formState, PolicyDecisionMode policyDecisionMode, WebCore::PolicyCheckIdentifier requestIdentifier, FramePolicyFunction&& function)
{
    auto* webPage = m_frame->page();
    if (!webPage) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because there's no web page");
        function(PolicyAction::Ignore, requestIdentifier);
        return;
    }

    LOG(Loading, "WebProcess %i - dispatchDecidePolicyForNavigationAction to request url %s", getCurrentProcessID(), request.url().string().utf8().data());

    // Always ignore requests with empty URLs. 
    if (request.isEmpty()) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because request is empty");
        function(PolicyAction::Ignore, requestIdentifier);
        return;
    }

    uint64_t listenerID = m_frame->setUpPolicyListener(requestIdentifier, WTFMove(function), WebFrame::ForNavigationAction::Yes);

    ASSERT(navigationAction.requester());
    auto& requester = navigationAction.requester().value();

    auto* requestingFrame = requester.globalFrameIdentifier && requester.globalFrameIdentifier->frameID ? WebProcess::singleton().webFrame(requester.globalFrameIdentifier->frameID) : nullptr;
    std::optional<WebCore::FrameIdentifier> originatingFrameID;
    std::optional<WebCore::FrameIdentifier> parentFrameID;
    if (requestingFrame) {
        originatingFrameID = requestingFrame->frameID();
        if (auto* parentFrame = requestingFrame->parentFrame())
            parentFrameID = parentFrame->frameID();
    }

    FrameInfoData originatingFrameInfoData {
        navigationAction.initiatedByMainFrame() == InitiatedByMainFrame::Yes,
        ResourceRequest { requester.url },
        requester.securityOrigin->data(),
        { },
        WTFMove(originatingFrameID),
        WTFMove(parentFrameID),
    };

    std::optional<WebPageProxyIdentifier> originatingPageID;
    if (requester.globalFrameIdentifier && requester.globalFrameIdentifier->pageID) {
        if (auto* webPage = WebProcess::singleton().webPage(requester.globalFrameIdentifier->pageID))
            originatingPageID = webPage->webPageProxyIdentifier();
    }

    NavigationActionData navigationActionData;
    navigationActionData.navigationType = navigationAction.type();
    navigationActionData.modifiers = modifiersForNavigationAction(navigationAction);
    navigationActionData.mouseButton = mouseButton(navigationAction);
    navigationActionData.syntheticClickType = syntheticClickType(navigationAction);
    if (auto& data = navigationAction.mouseEventData())
        navigationActionData.clickLocationInRootViewCoordinates = data->locationInRootViewCoordinates;
    navigationActionData.userGestureTokenIdentifier = WebProcess::singleton().userGestureTokenIdentifier(navigationAction.userGestureToken());
    navigationActionData.canHandleRequest = webPage->canHandleRequest(request);
    navigationActionData.shouldOpenExternalURLsPolicy = navigationAction.shouldOpenExternalURLsPolicy();
    navigationActionData.downloadAttribute = navigationAction.downloadAttribute();
    navigationActionData.isRedirect = !redirectResponse.isNull();
    navigationActionData.treatAsSameOriginNavigation = navigationAction.treatAsSameOriginNavigation();
    navigationActionData.hasOpenedFrames = navigationAction.hasOpenedFrames();
    navigationActionData.openedByDOMWithOpener = navigationAction.openedByDOMWithOpener();
    navigationActionData.requesterOrigin = requester.securityOrigin->data();
    navigationActionData.targetBackForwardItemIdentifier = navigationAction.targetBackForwardItemIdentifier();
    navigationActionData.sourceBackForwardItemIdentifier = navigationAction.sourceBackForwardItemIdentifier();
    navigationActionData.lockHistory = navigationAction.lockHistory();
    navigationActionData.lockBackForwardList = navigationAction.lockBackForwardList();
    navigationActionData.privateClickMeasurement = navigationAction.privateClickMeasurement();

    auto* coreFrame = m_frame->coreFrame();
    if (!coreFrame)
        return function(PolicyAction::Ignore, requestIdentifier);

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    setWebHitTestResultDataInNavigationActionDataIfNecessary(navigationAction, navigationActionData, *coreFrame);
#endif

    WebDocumentLoader* documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().policyDocumentLoader());
    if (!documentLoader) {
        // FIXME: When we receive a redirect after the navigation policy has been decided for the initial request,
        // the provisional load's DocumentLoader needs to receive navigation policy decisions. We need a better model for this state.
        documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().provisionalDocumentLoader());
    }
    if (!documentLoader)
        documentLoader = static_cast<WebDocumentLoader*>(coreFrame->loader().documentLoader());

    navigationActionData.clientRedirectSourceForHistory = documentLoader->clientRedirectSourceForHistory();
    navigationActionData.effectiveSandboxFlags = coreFrame->loader().effectiveSandboxFlags();

    // Notify the UIProcess.
    Ref protector { *coreFrame };

    if (policyDecisionMode == PolicyDecisionMode::Synchronous) {
        auto sendResult = webPage->sendSync(Messages::WebPageProxy::DecidePolicyForNavigationActionSync(m_frame->frameID(), m_frame->isMainFrame(), m_frame->info(), requestIdentifier, documentLoader->navigationID(), navigationActionData, originatingFrameInfoData, originatingPageID, navigationAction.resourceRequest(), request, IPC::FormDataReference { request.httpBody() }, redirectResponse));
        if (!sendResult) {
            WEBFRAMELOADERCLIENT_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because of failing to send sync IPC");
            m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { requestIdentifier, std::nullopt, PolicyAction::Ignore, 0, { }, { } });
            return;
        }

        auto [policyDecision] = sendResult.takeReply();
        WEBFRAMELOADERCLIENT_RELEASE_LOG(Network, "dispatchDecidePolicyForNavigationAction: Got policyAction %u from sync IPC", (unsigned)policyDecision.policyAction);
        m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { policyDecision.identifier, policyDecision.isNavigatingToAppBoundDomain, policyDecision.policyAction, 0, policyDecision.downloadID, { }});
        return;
    }

    ASSERT(policyDecisionMode == PolicyDecisionMode::Asynchronous);
    if (!webPage->send(Messages::WebPageProxy::DecidePolicyForNavigationActionAsync(m_frame->frameID(), m_frame->info(), requestIdentifier, documentLoader->navigationID(), navigationActionData, originatingFrameInfoData, originatingPageID, navigationAction.resourceRequest(), request, IPC::FormDataReference { request.httpBody() }, redirectResponse, listenerID))) {
        WEBFRAMELOADERCLIENT_RELEASE_LOG_ERROR(Network, "dispatchDecidePolicyForNavigationAction: ignoring because of failing to send async IPC");
        m_frame->didReceivePolicyDecision(listenerID, PolicyDecision { requestIdentifier, std::nullopt, PolicyAction::Ignore, 0, { }, { } });
    }
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    m_frame->invalidatePolicyListeners();
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
}

void WebFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<FormState>&& formState)
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    auto& form = formState->form();

    ASSERT(formState->sourceDocument().frame());
    auto* sourceFrame = WebFrame::fromCoreFrame(*formState->sourceDocument().frame());
    ASSERT(sourceFrame);

    webPage->injectedBundleFormClient().willSendSubmitEvent(webPage, &form, m_frame.ptr(), sourceFrame, formState->textFieldValues());
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FormState& formState, CompletionHandler<void()>&& completionHandler)
{
    WebPage* webPage = m_frame->page();
    if (!webPage) {
        completionHandler();
        return;
    }

    auto& form = formState.form();

    auto* sourceCoreFrame = formState.sourceDocument().frame();
    if (!sourceCoreFrame)
        return completionHandler();
    auto* sourceFrame = WebFrame::fromCoreFrame(*sourceCoreFrame);
    if (!sourceFrame)
        return completionHandler();

    auto& values = formState.textFieldValues();

    RefPtr<API::Object> userData;
    webPage->injectedBundleFormClient().willSubmitForm(webPage, &form, m_frame.ptr(), sourceFrame, values, userData);

    auto listenerID = m_frame->setUpWillSubmitFormListener(WTFMove(completionHandler));

    webPage->send(Messages::WebPageProxy::WillSubmitForm(m_frame->frameID(), sourceFrame->frameID(), values, listenerID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

inline bool WebFrameLoaderClient::hasPlugInView() const
{
#if !ENABLE(PDFKIT_PLUGIN)
    return false;
#else
    return m_pluginView;
#endif
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
#if ENABLE(PDFKIT_PLUGIN)
    if (!m_pluginView)
        return;
    
    m_pluginView->manualLoadDidFail();
    m_pluginView = nullptr;
    m_hasSentResponseToPluginView = false;
#endif
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrameLoaderClient::startDownload(const ResourceRequest& request, const String& suggestedName)
{
    m_frame->startDownload(request, suggestedName);
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::willReplaceMultipartContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->willReplaceMultipartContent(m_frame);
}

void WebFrameLoaderClient::didReplaceMultipartContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->didReplaceMultipartContent(m_frame);
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const SharedBuffer& data)
{
    if (!hasPlugInView())
        loader->commitData(data);

    // If the document is a stand-alone media document, now is the right time to cancel the WebKit load.
    // FIXME: This code should be shared across all ports. <http://webkit.org/b/48762>.
#if ENABLE(VIDEO)
    if (is<MediaDocument>(m_frame->coreFrame()->document()))
        loader->cancelMainResourceLoad(pluginWillHandleLoadError(loader->response()));
#endif

#if ENABLE(PDFKIT_PLUGIN)
    // Calling commitData did not create the plug-in view.
    if (!m_pluginView)
        return;

    if (!m_hasSentResponseToPluginView) {
        m_pluginView->manualLoadDidReceiveResponse(loader->response());
        // manualLoadDidReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
        // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
        // to null
        if (!m_pluginView)
            return;
        m_hasSentResponseToPluginView = true;
    }
    m_pluginView->manualLoadDidReceiveData(data);
#endif
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    if (m_frameHasCustomContentProvider) {
        WebPage* webPage = m_frame->page();
        if (!webPage)
            return;

        RefPtr<const SharedBuffer> contiguousData;
        RefPtr<const FragmentedSharedBuffer> mainResourceData = loader->mainResourceData();
        if (mainResourceData)
            contiguousData = mainResourceData->makeContiguous();
        IPC::DataReference dataReference(contiguousData ? contiguousData->data() : nullptr, contiguousData ? contiguousData->size() : 0);
        webPage->send(Messages::WebPageProxy::DidFinishLoadingDataForCustomContentProvider(loader->response().suggestedFilename(), dataReference));
    }

#if ENABLE(PDFKIT_PLUGIN)
    if (!m_pluginView)
        return;

    // If we just received an empty response without any data, we won't have sent a response to the plug-in view.
    // Make sure to do this before calling manualLoadDidFinishLoading.
    if (!m_hasSentResponseToPluginView) {
        m_pluginView->manualLoadDidReceiveResponse(loader->response());

        // Protect against the above call nulling out the plug-in (by trying to cancel the load for example).
        if (!m_pluginView)
            return;
    }

    m_pluginView->manualLoadDidFinishLoading();
    m_pluginView = nullptr;
    m_hasSentResponseToPluginView = false;
#endif
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader().documentLoader();

    WebNavigationDataStore data;
    data.url = loader->url().string();
    // FIXME: Use direction of title.
    data.title = loader->title().string;
    data.originalRequest = loader->originalRequestCopy();
    data.response = loader->response();

    webPage->send(Messages::WebPageProxy::DidNavigateWithNavigationData(data, m_frame->frameID()));
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader().documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    // Client redirect
    if (!loader->clientRedirectSourceForHistory().isNull()) {
        webPage->send(Messages::WebPageProxy::DidPerformClientRedirect(
            loader->clientRedirectSourceForHistory(), loader->clientRedirectDestinationForHistory(), m_frame->frameID()));
    }

    // Server redirect
    if (!loader->serverRedirectSourceForHistory().isNull()) {
        webPage->send(Messages::WebPageProxy::DidPerformServerRedirect(
            loader->serverRedirectSourceForHistory(), loader->serverRedirectDestinationForHistory(), m_frame->frameID()));
    }
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem& item) const
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return false;
    webPage->send(Messages::WebPageProxy::WillGoToBackForwardListItem(item.identifier(), item.isInBackForwardCache()));
    return true;
}

void WebFrameLoaderClient::didDisplayInsecureContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didDisplayInsecureContentForFrame(*webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidDisplayInsecureContentForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin&, const URL&)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didRunInsecureContentForFrame(*webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidRunInsecureContentForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request) const
{
    return WebKit::cancelledError(request);
}

ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request) const
{
    return WebKit::blockedError(request);
}

ResourceError WebFrameLoaderClient::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return WebKit::blockedByContentBlockerError(request);
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request) const
{
    return WebKit::cannotShowURLError(request);
}

ResourceError WebFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    return WebKit::interruptedForPolicyChangeError(request);
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebFrameLoaderClient::blockedByContentFilterError(const ResourceRequest& request) const
{
    return WebKit::blockedByContentFilterError(request);
}
#endif

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    return WebKit::cannotShowMIMETypeError(response);
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response) const
{
    return WebKit::fileDoesNotExistError(response);
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    return WebKit::pluginWillHandleLoadError(response);
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error) const
{
    static NeverDestroyed<const ResourceError> cancelledError(this->cancelledError(ResourceRequest()));
    static NeverDestroyed<const ResourceError> pluginWillHandleLoadError(this->pluginWillHandleLoadError(ResourceResponse()));

    if (error.errorCode() == cancelledError.get().errorCode() && error.domain() == cancelledError.get().domain())
        return false;

    if (error.errorCode() == pluginWillHandleLoadError.get().errorCode() && error.domain() == pluginWillHandleLoadError.get().domain())
        return false;

    return true;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMEType(const String& /*MIMEType*/) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMETypeAsHTML(const String& /*MIMEType*/) const
{
    return true;
}

bool WebFrameLoaderClient::representationExistsForURLScheme(StringView /*URLScheme*/) const
{
    notImplemented();
    return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(StringView /*URLScheme*/) const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    // Note: Can be called multiple times.
    if (!m_frame->isMainFrame())
        return;
    completePageTransitionIfNeeded();
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem& historyItem)
{
#if PLATFORM(IOS_FAMILY)
    if (m_frame->isMainFrame())
        m_frame->page()->savePageState(historyItem);
#else
    UNUSED_PARAM(historyItem);
#endif
}

void WebFrameLoaderClient::restoreViewState()
{
#if PLATFORM(IOS_FAMILY)
    Frame& frame = *m_frame->coreFrame();
    HistoryItem* currentItem = frame.loader().history().currentItem();
    if (FrameView* view = frame.view()) {
        if (m_frame->isMainFrame())
            m_frame->page()->restorePageState(*currentItem);
        else if (!view->wasScrolledByUser())
            view->setScrollPosition(currentItem->scrollPosition());
    }
#else
    // Inform the UI process of the scale factor.
    double scaleFactor = m_frame->coreFrame()->loader().history().currentItem()->pageScaleFactor();

    // A scale factor of 0 means the history item has the default scale factor, thus we do not need to update it.
    if (scaleFactor)
        m_frame->page()->send(Messages::WebPageProxy::PageScaleFactorDidChange(scaleFactor));

    // FIXME: This should not be necessary. WebCore should be correctly invalidating
    // the view on restores from the back/forward cache.
    if (m_frame->page() && m_frame.ptr() == &m_frame->page()->mainWebFrame())
        m_frame->page()->drawingArea()->setNeedsDisplay();
#endif
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frame->isMainFrame()) {
        webPage->didStartPageTransition();
        m_didCompletePageTransition = false;
    }
}

void WebFrameLoaderClient::didFinishLoad()
{
    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame.ptr());
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    notImplemented();
}

Ref<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return m_frame->page()->createDocumentLoader(*m_frame->coreFrame(), request, substituteData);
}

void WebFrameLoaderClient::updateCachedDocumentLoader(WebCore::DocumentLoader& loader)
{
    m_frame->page()->updateCachedDocumentLoader(static_cast<WebDocumentLoader&>(loader), *m_frame->coreFrame());
}

void WebFrameLoaderClient::setTitle(const StringWithDirection& title, const URL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // FIXME: Use direction of title.
    webPage->send(Messages::WebPageProxy::DidUpdateHistoryTitle(title.string, url.string(), m_frame->frameID()));
}

String WebFrameLoaderClient::userAgent(const URL& url) const
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return String();

    return webPage->userAgent(url);
}

String WebFrameLoaderClient::overrideContentSecurityPolicy() const
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return String();

    return webPage->overrideContentSecurityPolicy();
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    const ResourceResponse& response = m_frame->coreFrame()->loader().documentLoader()->response();
    m_frameHasCustomContentProvider = m_frame->isMainFrame() && m_frame->page()->shouldUseCustomContentProviderForResponse(response);
    m_frameCameFromBackForwardCache = true;
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    WebPage* webPage = m_frame->page();

    bool isMainFrame = m_frame->isMainFrame();
    bool shouldUseFixedLayout = isMainFrame && webPage->useFixedLayout();
    bool shouldDisableScrolling = isMainFrame && !webPage->mainFrameIsScrollable();
    bool shouldHideScrollbars = shouldDisableScrolling;
    IntRect fixedVisibleContentRect;

    auto oldView = m_frame->coreFrame()->view();

    auto overrideSizeForCSSDefaultViewportUnits = oldView ? oldView->overrideSizeForCSSDefaultViewportUnits() : std::nullopt;
    auto overrideSizeForCSSSmallViewportUnits = oldView ? oldView->overrideSizeForCSSSmallViewportUnits() : std::nullopt;
    auto overrideSizeForCSSLargeViewportUnits = oldView ? oldView->overrideSizeForCSSLargeViewportUnits() : std::nullopt;

#if USE(COORDINATED_GRAPHICS)
    if (oldView)
        fixedVisibleContentRect = oldView->fixedVisibleContentRect();
    if (shouldUseFixedLayout)
        shouldHideScrollbars = true;
#endif

    m_frameHasCustomContentProvider = isMainFrame
        && m_frame->coreFrame()->loader().documentLoader()
        && webPage->shouldUseCustomContentProviderForResponse(m_frame->coreFrame()->loader().documentLoader()->response());
    m_frameCameFromBackForwardCache = false;

    ScrollbarMode defaultScrollbarMode = shouldHideScrollbars ? ScrollbarMode::AlwaysOff : ScrollbarMode::Auto;

    ScrollbarMode horizontalScrollbarMode = webPage->alwaysShowsHorizontalScroller() ? ScrollbarMode::AlwaysOn : defaultScrollbarMode;
    ScrollbarMode verticalScrollbarMode = webPage->alwaysShowsVerticalScroller() ? ScrollbarMode::AlwaysOn : defaultScrollbarMode;

    bool horizontalLock = shouldHideScrollbars || webPage->alwaysShowsHorizontalScroller();
    bool verticalLock = shouldHideScrollbars || webPage->alwaysShowsVerticalScroller();

    m_frame->coreFrame()->createView(webPage->size(), webPage->backgroundColor(),
        webPage->fixedLayoutSize(), fixedVisibleContentRect, shouldUseFixedLayout,
        horizontalScrollbarMode, horizontalLock, verticalScrollbarMode, verticalLock);

    RefPtr<FrameView> view = m_frame->coreFrame()->view();

    if (overrideSizeForCSSDefaultViewportUnits)
        view->setOverrideSizeForCSSDefaultViewportUnits(*overrideSizeForCSSDefaultViewportUnits);

    if (overrideSizeForCSSSmallViewportUnits)
        view->setOverrideSizeForCSSSmallViewportUnits(*overrideSizeForCSSSmallViewportUnits);

    if (overrideSizeForCSSLargeViewportUnits)
        view->setOverrideSizeForCSSLargeViewportUnits(*overrideSizeForCSSLargeViewportUnits);

    if (int width = webPage->minimumSizeForAutoLayout().width()) {
        int height = std::max(webPage->minimumSizeForAutoLayout().height(), 1);
        view->enableFixedWidthAutoSizeMode(true, { width, height });

        if (webPage->autoSizingShouldExpandToViewHeight())
            view->setAutoSizeFixedMinimumHeight(webPage->size().height());
    }

    IntSize sizeToContentAutoSizeMaximumSize = webPage->sizeToContentAutoSizeMaximumSize();
    if (sizeToContentAutoSizeMaximumSize.width() && sizeToContentAutoSizeMaximumSize.height()) {
        if (isMainFrame)
            view->enableSizeToContentAutoSizeMode(true, sizeToContentAutoSizeMaximumSize);

        if (webPage->autoSizingShouldExpandToViewHeight())
            view->setAutoSizeFixedMinimumHeight(webPage->size().height());
    }

    if (auto viewportSizeForViewportUnits = webPage->viewportSizeForCSSViewportUnits())
        view->setSizeForCSSDefaultViewportUnits(*viewportSizeForViewportUnits);
    view->setProhibitsScrolling(shouldDisableScrolling);
    view->setVisualUpdatesAllowedByClient(!webPage->shouldExtendIncrementalRenderingSuppression());
#if PLATFORM(COCOA)
    auto* drawingArea = webPage->drawingArea();
    view->setViewExposedRect(drawingArea->viewExposedRect());
    if (isMainFrame)
        view->setDelegatedScrollingMode(drawingArea->delegatedScrollingMode());

    webPage->corePage()->setDelegatesScaling(drawingArea->usesDelegatedPageScaling());
#endif

    if (webPage->scrollPinningBehavior() != DoNotPin)
        view->setScrollPinningBehavior(webPage->scrollPinningBehavior());

#if USE(COORDINATED_GRAPHICS)
    if (shouldUseFixedLayout) {
        view->setDelegatedScrollingMode(shouldUseFixedLayout ? DelegatedScrollingMode::DelegatedToNativeScrollView : DelegatedScrollingMode::NotDelegated);
        view->setPaintsEntireContents(shouldUseFixedLayout);
        return;
    }
#endif
}

void WebFrameLoaderClient::didRestoreFromBackForwardCache()
{
    m_frameCameFromBackForwardCache = true;
}

bool WebFrameLoaderClient::canCachePage() const
{
    // We cannot cache frames that have custom representations because they are
    // rendered in the UIProcess.
    return !m_frameHasCustomContentProvider;
}

void WebFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader *documentLoader, const ResourceRequest& request, const ResourceResponse& response)
{
    m_frame->convertMainResourceLoadToDownload(documentLoader, request, response);
}

RefPtr<Frame> WebFrameLoaderClient::createFrame(const AtomString& name, HTMLFrameOwnerElement& ownerElement)
{
    auto* webPage = m_frame->page();
    ASSERT(webPage);
    auto subframe = WebFrame::createSubframe(*webPage, m_frame, name, ownerElement);
    auto* coreSubframe = subframe->coreFrame();
    if (!coreSubframe)
        return nullptr;

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!coreSubframe->page())
        return nullptr;

    return coreSubframe;
}

RefPtr<Widget> WebFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement& pluginElement, const URL& url, const Vector<AtomString>&, const Vector<AtomString>&, const String& mimeType, bool loadManually)
{
#if !ENABLE(PDFKIT_PLUGIN)
    UNUSED_PARAM(pluginElement);
    UNUSED_PARAM(url);
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(loadManually);
    return nullptr;
#else
    return PluginView::create(pluginElement, url, mimeType, loadManually && !m_frameCameFromBackForwardCache);
#endif
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget& pluginWidget)
{
#if !ENABLE(PDFKIT_PLUGIN)
    UNUSED_PARAM(pluginWidget);
#else
    m_pluginView = static_cast<PluginView*>(&pluginWidget);
#endif
}

static bool pluginSupportsExtension(const PluginData& pluginData, const String& extension)
{
    ASSERT(extension.convertToASCIILowercase() == extension);
    for (auto& type : pluginData.webVisibleMimeTypes()) {
        if (type.extensions.contains(extension))
            return true;
    }
    return false;
}

ObjectContentType WebFrameLoaderClient::objectContentType(const URL& url, const String& mimeTypeIn)
{
    // FIXME: This should eventually be merged with WebCore::FrameLoader::defaultObjectContentType.

    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty()) {
        auto path = url.path();
        auto dotPosition = path.reverseFind('.');
        if (dotPosition == notFound)
            return ObjectContentType::Frame;
        auto extension = path.substring(dotPosition + 1).convertToASCIILowercase();

        // Try to guess the MIME type from the extension.
        mimeType = MIMETypeRegistry::mimeTypeForExtension(extension);
        if (mimeType.isEmpty()) {
            // Check if there's a plug-in around that can handle the extension.
            if (auto* webPage = m_frame->page()) {
                if (pluginSupportsExtension(webPage->corePage()->pluginData(), extension))
                    return ObjectContentType::PlugIn;
            }
            return ObjectContentType::Frame;
        }
    }
#if ENABLE(PDFJS)
    if (auto* webPage = m_frame->page()) {
        if (webPage->corePage()->settings().pdfJSViewerEnabled() && MIMETypeRegistry::isPDFMIMEType(mimeType))
            return ObjectContentType::Frame;
    }
#endif
    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentType::Image;

    if (WebPage* webPage = m_frame->page()) {
        auto allowedPluginTypes = webFrame().coreFrame()->arePluginsEnabled()
            ? PluginData::AllPlugins : PluginData::OnlyApplicationPlugins;
        if (webPage->corePage()->pluginData().supportsMimeType(mimeType, allowedPluginTypes))
            return ObjectContentType::PlugIn;
    }

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentType::Frame;

#if PLATFORM(IOS_FAMILY)
    // iOS can render PDF in <object>/<embed> via PDFDocumentImage.
    if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(mimeType))
        return ObjectContentType::Image;
#endif

    return ObjectContentType::None;
}

AtomString WebFrameLoaderClient::overrideMediaType() const
{
    if (auto* page = m_frame->page())
        return page->overriddenMediaType();

    return nullAtom();
}

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(IPC_TESTING_API)
    if (world.isNormal() && webPage->ipcTestingAPIEnabled())
        IPCTestingAPI::inject(*webPage, m_frame.get(), world);
#endif

    webPage->injectedBundleLoaderClient().didClearWindowObjectForFrame(*webPage, m_frame, world);

    WebAutomationSessionProxy* automationSessionProxy = WebProcess::singleton().automationSessionProxy();
    if (automationSessionProxy && world.isNormal())
        automationSessionProxy->didClearWindowObjectForFrame(m_frame);
}

void WebFrameLoaderClient::dispatchGlobalObjectAvailable(DOMWrapperWorld& world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleLoaderClient().globalObjectIsAvailableForFrame(*webPage, m_frame, world);

#if ENABLE(WK_WEB_EXTENSIONS)
    if (auto* extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->globalObjectIsAvailableForFrame(*webPage, m_frame, world);
#endif
}

void WebFrameLoaderClient::dispatchServiceWorkerGlobalObjectAvailable(DOMWrapperWorld& world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleLoaderClient().serviceWorkerGlobalObjectIsAvailableForFrame(*webPage, m_frame, world);

#if ENABLE(WK_WEB_EXTENSIONS)
    if (auto* extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->serviceWorkerGlobalObjectIsAvailableForFrame(*webPage, m_frame, world);
#endif
}

void WebFrameLoaderClient::willInjectUserScript(DOMWrapperWorld& world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleLoaderClient().willInjectUserScriptForFrame(*webPage, m_frame, world);
}

void WebFrameLoaderClient::dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension* extension)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().willDisconnectDOMWindowExtensionFromGlobalObject(*webPage, extension);
}

void WebFrameLoaderClient::dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension* extension)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().didReconnectDOMWindowExtensionToGlobalObject(*webPage, extension);
}

void WebFrameLoaderClient::dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension* extension)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().willDestroyGlobalObjectForDOMWindowExtension(*webPage, extension);
}

#if PLATFORM(COCOA)
    
RemoteAXObjectRef WebFrameLoaderClient::accessibilityRemoteObject() 
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return 0;
    
    return webPage->accessibilityRemoteObject();
}
    
void WebFrameLoaderClient::willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier identifier, NSCachedURLResponse* response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return completionHandler(response);

    return completionHandler(webPage->injectedBundleResourceLoadClient().shouldCacheResponse(*webPage, m_frame, identifier) ? response : nil);
}

NSDictionary *WebFrameLoaderClient::dataDetectionContext()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return nil;

    return webPage->dataDetectionContext();
}

#endif // PLATFORM(COCOA)

void WebFrameLoaderClient::didChangeScrollOffset()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didChangeScrollOffsetForFrame(m_frame->coreFrame());
}

bool WebFrameLoaderClient::allowScript(bool enabledPerSettings)
{
    return enabledPerSettings && !hasPlugInView();
}

bool WebFrameLoaderClient::shouldForceUniversalAccessFromLocalURL(const URL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return false;

    return webPage->injectedBundleLoaderClient().shouldForceUniversalAccessFromLocalURL(*webPage, url.string());
}

Ref<FrameNetworkingContext> WebFrameLoaderClient::createNetworkingContext()
{
    ASSERT(!hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return WebFrameNetworkingContext::create(m_frame.ptr());
}

#if ENABLE(CONTENT_FILTERING)

void WebFrameLoaderClient::contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler unblockHandler)
{
    if (!unblockHandler.needsUIProcess()) {
        m_frame->coreFrame()->loader().policyChecker().setContentFilterUnblockHandler(WTFMove(unblockHandler));
        return;
    }

    if (WebPage* webPage { m_frame->page() })
        webPage->send(Messages::WebPageProxy::ContentFilterDidBlockLoadForFrame(unblockHandler, m_frame->frameID()));
}

#endif

void WebFrameLoaderClient::prefetchDNS(const String& hostname)
{
    WebProcess::singleton().prefetchDNS(hostname);
}

void WebFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return completionHandler(makeUnexpected(internalError(url)));

    NetworkResourceLoadParameters parameters;
    parameters.request = ResourceRequest(url);
    parameters.identifier = WebCore::ResourceLoaderIdentifier::generate();
    parameters.webPageProxyID = webPage->webPageProxyIdentifier();
    parameters.webPageID = webPage->identifier();
    parameters.webFrameID = m_frame->frameID();
    parameters.parentPID = presentingApplicationPID();
#if HAVE(AUDIT_TOKEN)
    parameters.networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken();
#endif
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;
    parameters.options.destination = FetchOptions::Destination::EmptyString;
#if ENABLE(APP_BOUND_DOMAINS)
    parameters.isNavigatingToAppBoundDomain = m_frame->isTopFrameNavigatingToAppBoundDomain();
#endif
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::SendH2Ping(parameters), WTFMove(completionHandler));
}

void WebFrameLoaderClient::didRestoreScrollPosition()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didRestoreScrollPosition();
}

void WebFrameLoaderClient::getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>& icons)
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    for (auto& icon : icons)
        webPage->send(Messages::WebPageProxy::GetLoadDecisionForIcon(icon.first, CallbackID::fromInteger(icon.second)));
}

#if ENABLE(SERVICE_WORKER)
void WebFrameLoaderClient::didFinishServiceWorkerPageRegistration(bool success)
{
    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->send(Messages::WebPageProxy::DidFinishServiceWorkerPageRegistration(success));
}
#endif

#if ENABLE(APP_BOUND_DOMAINS)
bool WebFrameLoaderClient::shouldEnableInAppBrowserPrivacyProtections() const
{
    return m_frame->shouldEnableInAppBrowserPrivacyProtections();
}

void WebFrameLoaderClient::notifyPageOfAppBoundBehavior()
{
    if (!m_frame->isMainFrame())
        return;

    auto* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->notifyPageOfAppBoundBehavior();
}
#endif

#if ENABLE(PDFKIT_PLUGIN)

bool WebFrameLoaderClient::shouldUsePDFPlugin(const String& contentType, StringView path) const
{
    auto* page = m_frame->page();
    return page && page->shouldUsePDFPlugin(contentType, path);
}

#endif

bool WebFrameLoaderClient::isParentProcessAFullWebBrowser() const
{
    auto* page = m_frame->page();
    return page && page->isParentProcessAWebBrowser();
}

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
void WebFrameLoaderClient::modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>)>&& completionHandler) const
{
    auto* webPage = m_frame->page();
    if (!webPage) {
        completionHandler({ });
        return;
    }

    webPage->sendWithAsyncReply(Messages::WebPageProxy::ModelInlinePreviewUUIDs(), WTFMove(completionHandler));
}
#endif

} // namespace WebKit

#undef PREFIX_PARAMETERS
#undef WEBFRAME
#undef WEBFRAMEID
#undef WEBPAGE
#undef WEBPAGEID
