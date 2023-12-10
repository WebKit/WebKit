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
#include "WebLocalFrameLoaderClient.h"

#include "APIInjectedBundleFormClient.h"
#include "APIInjectedBundlePageLoaderClient.h"
#include "APIInjectedBundlePageResourceLoadClient.h"
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
#include "MessageSenderInlines.h"
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
#include <WebCore/FrameLoader.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HistoryController.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MediaDocument.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/Quirks.h>
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

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionControllerProxy.h"
#endif

#define PREFIX_PARAMETERS "%p - [webFrame=%p, webFrameID=%" PRIu64 ", webPage=%p, webPageID=%" PRIu64 "] WebLocalFrameLoaderClient::"
#define WEBFRAME (&webFrame())
#define WEBFRAMEID (webFrame().frameID().object().toUInt64())
#define WEBPAGE (webFrame().page())
#define WEBPAGEID (WEBPAGE ? WEBPAGE->identifier().toUInt64() : 0)

#define WebLocalFrameLoaderClient_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, PREFIX_PARAMETERS fmt, this, WEBFRAME, WEBFRAMEID, WEBPAGE, WEBPAGEID, ##__VA_ARGS__)
#define WebLocalFrameLoaderClient_RELEASE_LOG_FAULT(channel, fmt, ...) RELEASE_LOG_FAULT(channel, PREFIX_PARAMETERS fmt, this, WEBFRAME, WEBFRAMEID, WEBPAGE, WEBPAGEID, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WebLocalFrameLoaderClient::WebLocalFrameLoaderClient(Ref<WebFrame>&& frame, ScopeExit<Function<void()>>&& invalidator)
    : WebFrameLoaderClient(WTFMove(frame))
    , m_frameInvalidator(WTFMove(invalidator))
{
}

WebLocalFrameLoaderClient::~WebLocalFrameLoaderClient() = default;

std::optional<WebPageProxyIdentifier> WebLocalFrameLoaderClient::webPageProxyID() const
{
    if (m_frame->page())
        return m_frame->page()->webPageProxyIdentifier();

    return std::nullopt;
}

void WebLocalFrameLoaderClient::setHasFrameSpecificStorageAccess(FrameSpecificStorageAccessIdentifier&& frameSpecificStorageAccessIdentifier )
{
    ASSERT(!m_frameSpecificStorageAccessIdentifier);

    m_frameSpecificStorageAccessIdentifier = WTFMove(frameSpecificStorageAccessIdentifier);
}

void WebLocalFrameLoaderClient::didLoadFromRegistrableDomain(RegistrableDomain&& domain)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
    
    webPage->didLoadFromRegistrableDomain(WTFMove(domain));
}

Vector<RegistrableDomain> WebLocalFrameLoaderClient::loadedSubresourceDomains() const
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return { };

    return copyToVector(webPage->loadedSubresourceDomains());
}

bool WebLocalFrameLoaderClient::hasHTMLView() const
{
    return !m_frameHasCustomContentProvider;
}

bool WebLocalFrameLoaderClient::hasWebView() const
{
    return m_frame->page();
}

void WebLocalFrameLoaderClient::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void WebLocalFrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebLocalFrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void WebLocalFrameLoaderClient::detachedFromParent2()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frameSpecificStorageAccessIdentifier) {
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveStorageAccessForFrame(
            m_frameSpecificStorageAccessIdentifier->frameID, m_frameSpecificStorageAccessIdentifier->pageID), 0);
        m_frameSpecificStorageAccessIdentifier = std::nullopt;
    }

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didRemoveFrameFromHierarchy(*webPage, m_frame, userData);
}

void WebLocalFrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void WebLocalFrameLoaderClient::assignIdentifierToInitialRequest(ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    RefPtr webPage = m_frame->page();
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

void WebLocalFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    RefPtr webPage = m_frame->page();
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

bool WebLocalFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier identifier)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return true;

    return webPage->injectedBundleResourceLoadClient().shouldUseCredentialStorage(*webPage, m_frame, identifier);
}

void WebLocalFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, WebCore::ResourceLoaderIdentifier, const AuthenticationChallenge&)
{
    ASSERT_NOT_REACHED();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebLocalFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, WebCore::ResourceLoaderIdentifier, const ProtectionSpace& protectionSpace)
{
    // The WebKit 2 Networking process asks the UIProcess directly, so the WebContent process should never receive this callback.
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void WebLocalFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didReceiveResponseForResource(*webPage, m_frame, identifier, response);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidReceiveResponseForResource(identifier, m_frame->frameID(), response));
#endif
}

void WebLocalFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier identifier, int dataLength)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didReceiveContentLengthForResource(*webPage, m_frame, identifier, dataLength);
}

#if ENABLE(DATA_DETECTION)
void WebLocalFrameLoaderClient::dispatchDidFinishDataDetection(NSArray *detectionResults)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->setDataDetectionResults(detectionResults);
}
#endif

void WebLocalFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, ResourceLoaderIdentifier identifier)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didFinishLoadForResource(*webPage, m_frame, identifier);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidFinishLoadForResource(identifier, m_frame->frameID(), { }));
#endif

    webPage->removeResourceRequest(identifier);
}

void WebLocalFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, ResourceLoaderIdentifier identifier, const ResourceError& error)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleResourceLoadClient().didFailLoadForResource(*webPage, m_frame, identifier, error);

#if PLATFORM(GTK) || PLATFORM(WPE)
    webPage->send(Messages::WebPageProxy::DidFinishLoadForResource(identifier, m_frame->frameID(), error));
#endif

    webPage->removeResourceRequest(identifier);
}

bool WebLocalFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int /*length*/)
{
    notImplemented();
    return false;
}

void WebLocalFrameLoaderClient::dispatchDidDispatchOnloadEvents()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didHandleOnloadEventsForFrame(*webPage, m_frame);
}

void WebLocalFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr documentLoader = static_cast<WebDocumentLoader*>(m_frame->coreLocalFrame()->loader().provisionalDocumentLoader());
    if (!documentLoader) {
        WebLocalFrameLoaderClient_RELEASE_LOG_FAULT(Loading, "dispatchDidReceiveServerRedirectForProvisionalLoad: Called with no provisional DocumentLoader (frameState=%hhu, stateForDebugging=%i)", static_cast<uint8_t>(m_frame->coreLocalFrame()->loader().state()), m_frame->coreLocalFrame()->loader().stateMachine().stateForDebugging());
        return;
    }

    RefPtr<API::Object> userData;

    LOG(Loading, "WebProcess %i - dispatchDidReceiveServerRedirectForProvisionalLoad to request url %s", getCurrentProcessID(), documentLoader->request().url().string().utf8().data());

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didReceiveServerRedirectForProvisionalLoadForFrame(*webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame(m_frame->frameID(), documentLoader->navigationID(), documentLoader->request(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebLocalFrameLoaderClient::dispatchDidChangeProvisionalURL()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    Ref documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreLocalFrame()->loader().provisionalDocumentLoader());
    webPage->send(Messages::WebPageProxy::DidChangeProvisionalURLForFrame(m_frame->frameID(), documentLoader->navigationID(), documentLoader->url()));
}

void WebLocalFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCancelClientRedirectForFrame(*webPage, m_frame);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidCancelClientRedirectForFrame(m_frame->frameID()));
}

void WebLocalFrameLoaderClient::dispatchWillPerformClientRedirect(const URL& url, double interval, WallTime fireDate, LockBackForwardList lockBackForwardList)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().willPerformClientRedirectForFrame(*webPage, m_frame, url.string(), interval, fireDate);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::WillPerformClientRedirectForFrame(m_frame->frameID(), url.string(), interval, lockBackForwardList));
}

void WebLocalFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didSameDocumentNavigationForFrame(m_frame);
}

void WebLocalFrameLoaderClient::dispatchDidChangeMainDocument()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->send(Messages::WebPageProxy::DidChangeMainDocument(m_frame->frameID()));
}

void WebLocalFrameLoaderClient::dispatchWillChangeDocument(const URL& currentURL, const URL& newURL)
{
    if (m_frame->isMainFrame())
        return;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frameSpecificStorageAccessIdentifier && !WebCore::areRegistrableDomainsEqual(currentURL, newURL)) {
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveStorageAccessForFrame(
            m_frameSpecificStorageAccessIdentifier->frameID, m_frameSpecificStorageAccessIdentifier->pageID), 0);
        m_frameSpecificStorageAccessIdentifier = std::nullopt;
    }
}

void WebLocalFrameLoaderClient::didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType navigationType)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didSameDocumentNavigationForFrame(*webPage, m_frame, SameDocumentNavigationType::SessionStatePush, userData);

    NavigationActionData navigationActionData {
        WebCore::NavigationType::Other,
        { }, /* modifiers */
        WebMouseEventButton::None,
        WebMouseEventSyntheticClickType::NoTap,
        WebProcess::singleton().userGestureTokenIdentifier(UserGestureIndicator::currentUserGesture()),
        UserGestureIndicator::currentUserGesture() ? UserGestureIndicator::currentUserGesture()->authorizationToken() : std::nullopt,
        true, /* canHandleRequest */
        WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow,
        { }, /* downloadAttribute */
        { }, /* clickLocationInRootViewCoordinates */
        { }, /* redirectResponse */
        true, /* treatAsSameOriginNavigation */
        false, /* hasOpenedFrames */
        false, /* openedByDOMWithOpener */
        !!m_frame->coreLocalFrame()->loader().opener(), /* hasOpener */
        { }, /* requesterOrigin */
        { }, /* requesterTopOrigin */
        std::nullopt, /* targetBackForwardItemIdentifier */
        std::nullopt, /* sourceBackForwardItemIdentifier */
        WebCore::LockHistory::No,
        WebCore::LockBackForwardList::No,
        { }, /* clientRedirectSourceForHistory */
        0, /* effectiveSandboxFlags */
        std::nullopt, /* privateClickMeasurement */
        { }, /* advancedPrivacyProtections */
        { }, /* originatorAdvancedPrivacyProtections */
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        std::nullopt, /* webHitTestResultData */
#endif
    };

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidSameDocumentNavigationForFrameViaJSHistoryAPI(m_frame->frameID(), navigationType, m_frame->coreLocalFrame()->document()->url(), navigationActionData, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

}

void WebLocalFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType::SessionStatePush);
}

void WebLocalFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType::SessionStateReplace);
}

void WebLocalFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    didSameDocumentNavigationForFrameViaJSHistoryAPI(SameDocumentNavigationType::SessionStatePop);
}

void WebLocalFrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void WebLocalFrameLoaderClient::dispatchDidExplicitOpen(const URL& url, const String& mimeType)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidExplicitOpenForFrame(m_frame->frameID(), url, mimeType));
}

void WebLocalFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(FULLSCREEN_API)
    auto* document = m_frame->coreLocalFrame()->document();
    if (document && document->fullscreenManager().fullscreenElement())
        webPage->fullScreenManager()->exitFullScreenForElement(webPage->fullScreenManager()->element());
#endif

    webPage->findController().hideFindUI();
    webPage->sandboxExtensionTracker().didStartProvisionalLoad(m_frame.ptr());

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didStartProvisionalLoadForFrame(*webPage, m_frame, userData);
    RefPtr provisionalLoader = static_cast<WebDocumentLoader*>(m_frame->coreLocalFrame()->loader().provisionalDocumentLoader());

    if (!provisionalLoader || provisionalLoader->isContinuingLoadAfterProvisionalLoadStarted())
        return;
    
    auto& url = provisionalLoader->url();
    auto& unreachableURL = provisionalLoader->unreachableURL();

#if ENABLE(WK_WEB_EXTENSIONS)
    // Notify the extensions controller.
    if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->didStartProvisionalLoadForFrame(*webPage, m_frame, url);
#endif

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidStartProvisionalLoadForFrame(m_frame->frameID(), m_frame->info(), provisionalLoader->request(), provisionalLoader->navigationID(), url, unreachableURL, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

void WebLocalFrameLoaderClient::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    RefPtr webPage = m_frame->page();
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

void WebLocalFrameLoaderClient::dispatchDidCommitLoad(std::optional<HasInsecureContent> hasInsecureContent, std::optional<UsedLegacyTLS> usedLegacyTLSFromPageCache, std::optional<WasPrivateRelayed> wasPrivateRelayedFromPageCache)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    Ref documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreLocalFrame()->loader().documentLoader());
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCommitLoadForFrame(*webPage, m_frame, userData);

    webPage->sandboxExtensionTracker().didCommitProvisionalLoad(m_frame.ptr());

    bool usedLegacyTLS = documentLoader->response().usedLegacyTLS();
    if (!usedLegacyTLS && usedLegacyTLSFromPageCache)
        usedLegacyTLS = usedLegacyTLSFromPageCache == UsedLegacyTLS::Yes;

    bool wasPrivateRelayed = documentLoader->response().wasPrivateRelayed();
    if (!wasPrivateRelayed && wasPrivateRelayedFromPageCache)
        wasPrivateRelayed = wasPrivateRelayedFromPageCache == WasPrivateRelayed::Yes;

    auto certificateInfo = valueOrCompute(documentLoader->response().certificateInfo(), [] {
        return CertificateInfo();
    });
    hasInsecureContent = hasInsecureContent ? *hasInsecureContent : (certificateInfo.containsNonRootSHA1SignedCertificate() ? HasInsecureContent::Yes : HasInsecureContent::No);

#if ENABLE(WK_WEB_EXTENSIONS)
    // Notify the extensions controller.
    if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->didCommitLoadForFrame(*webPage, m_frame, m_frame->url());
#endif

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidCommitLoadForFrame(m_frame->frameID(), m_frame->info(), documentLoader->request(), documentLoader->navigationID(), documentLoader->response().mimeType(), m_frameHasCustomContentProvider, m_frame->coreLocalFrame()->loader().loadType(), certificateInfo, usedLegacyTLS, wasPrivateRelayed, m_frame->coreLocalFrame()->document()->isPluginDocument(), *hasInsecureContent, documentLoader->mouseEventPolicy(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    webPage->didCommitLoad(m_frame.ptr());
}

void WebLocalFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading willContinueLoading, WillInternallyHandleFailure willInternallyHandleFailure)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    // When stopping the provisional load due to a process swap on resource response (due to COOP), the provisional load
    // will simply keep going in a new process. As a result, we don't want to tell the injected bundle or the UIProcess client
    // that the provisional load failed.
    if (webPage->isStoppingLoadingDueToProcessSwap())
        return;

    WebLocalFrameLoaderClient_RELEASE_LOG(Network, "dispatchDidFailProvisionalLoad:");

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
    // the entire LocalFrameLoaderClient function was complete.
    uint64_t navigationID = 0;
    ResourceRequest request;
    if (auto documentLoader = m_frame->coreLocalFrame()->loader().provisionalDocumentLoader()) {
        navigationID = static_cast<WebDocumentLoader*>(documentLoader)->navigationID();
        request = documentLoader->request();
    }

    // Notify the UIProcess.
    auto* coreFrame = m_frame->coreLocalFrame();
    webPage->send(Messages::WebPageProxy::DidFailProvisionalLoadForFrame(m_frame->info(), request, navigationID, coreFrame->loader().provisionalLoadErrorBeingHandledURL().string(), error, willContinueLoading, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get()), willInternallyHandleFailure));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame.ptr(), error.isCancellation());
}

void WebLocalFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    WebLocalFrameLoaderClient_RELEASE_LOG(Network, "dispatchDidFailLoad:");

    RefPtr<API::Object> userData;

    Ref documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreLocalFrame()->loader().documentLoader());
    auto navigationID = documentLoader->navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailLoadWithErrorForFrame(*webPage, m_frame, error, userData);

#if ENABLE(WK_WEB_EXTENSIONS)
    // Notify the extensions controller.
    if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->didFailLoadForFrame(*webPage, m_frame, m_frame->url());
#endif

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFailLoadForFrame(m_frame->frameID(), m_frame->info(), documentLoader->request(), navigationID, error, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame.ptr(), error.isCancellation());
}

void WebLocalFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    auto navigationID = static_cast<WebDocumentLoader&>(*m_frame->coreLocalFrame()->loader().documentLoader()).navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishDocumentLoadForFrame(*webPage, m_frame, userData);

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFinishDocumentLoadForFrame(m_frame->frameID(), navigationID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    webPage->didFinishDocumentLoad(m_frame);
}

void WebLocalFrameLoaderClient::dispatchDidFinishLoad()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    Ref documentLoader = static_cast<WebDocumentLoader&>(*m_frame->coreLocalFrame()->loader().documentLoader());
    auto navigationID = documentLoader->navigationID();

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishLoadForFrame(*webPage, m_frame, userData);

#if ENABLE(WK_WEB_EXTENSIONS)
    // Notify the extensions controller.
    if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->didFinishLoadForFrame(*webPage, m_frame, m_frame->url());
#endif

    // Notify the UIProcess.
    webPage->send(Messages::WebPageProxy::DidFinishLoadForFrame(m_frame->frameID(), m_frame->info(), documentLoader->request(), navigationID, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame.ptr());

    webPage->didFinishLoad(m_frame);
}

void WebLocalFrameLoaderClient::completePageTransitionIfNeeded()
{
    if (m_didCompletePageTransition)
        return;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didCompletePageTransition();
    m_didCompletePageTransition = true;
    WebLocalFrameLoaderClient_RELEASE_LOG(Layout, "completePageTransitionIfNeeded: dispatching didCompletePageTransition");
}

void WebLocalFrameLoaderClient::dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone> milestones)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    if (milestones & WebCore::LayoutMilestone::DidFirstLayout) {
        WebLocalFrameLoaderClient_RELEASE_LOG(Layout, "dispatchDidReachLayoutMilestone: dispatching DidFirstLayoutForFrame");

        // FIXME: We should consider removing the old didFirstLayout API since this is doing double duty with the
        // new didLayout API.
        webPage->injectedBundleLoaderClient().didFirstLayoutForFrame(*webPage, m_frame, userData);
        webPage->send(Messages::WebPageProxy::DidFirstLayoutForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));

#if USE(COORDINATED_GRAPHICS)
        // Make sure viewport properties are dispatched on the main frame by the time the first layout happens.
        ASSERT(!webPage->useFixedLayout() || m_frame.ptr() != &m_frame->page()->mainWebFrame() || m_frame->coreLocalFrame()->document()->didDispatchViewportPropertiesChanged());
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

    addIfSet(WebCore::LayoutMilestone::DidFirstLayout, "DidFirstLayout"_s);
    addIfSet(WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout, "DidFirstVisuallyNonEmptyLayout"_s);
    addIfSet(WebCore::LayoutMilestone::DidHitRelevantRepaintedObjectsAreaThreshold, "DidHitRelevantRepaintedObjectsAreaThreshold"_s);
    addIfSet(WebCore::LayoutMilestone::DidFirstLayoutAfterSuppressedIncrementalRendering, "DidFirstLayoutAfterSuppressedIncrementalRendering"_s);
    addIfSet(WebCore::LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering, "DidFirstPaintAfterSuppressedIncrementalRendering"_s);
    addIfSet(WebCore::LayoutMilestone::ReachedSessionRestorationRenderTreeSizeThreshold, "ReachedSessionRestorationRenderTreeSizeThreshold"_s);
    addIfSet(WebCore::LayoutMilestone::DidRenderSignificantAmountOfText, "DidRenderSignificantAmountOfText"_s);
    addIfSet(WebCore::LayoutMilestone::DidFirstMeaningfulPaint, "DidFirstMeaningfulPaint"_s);

    WebLocalFrameLoaderClient_RELEASE_LOG(Layout, "dispatchDidReachLayoutMilestone: dispatching DidReachLayoutMilestone (milestones=%" PUBLIC_LOG_STRING ")", builder.toString().utf8().data());
#endif

    // Send this after DidFirstLayout-specific calls since some clients expect to get those messages first.
    webPage->dispatchDidReachLayoutMilestone(milestones);

    if (milestones & WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout) {
        ASSERT(!m_frame->isMainFrame() || webPage->corePage()->settings().suppressesIncrementalRendering() || m_didCompletePageTransition);
        // FIXME: We should consider removing the old didFirstVisuallyNonEmptyLayoutForFrame API since this is doing double duty with the new didLayout API.
        WebLocalFrameLoaderClient_RELEASE_LOG(Layout, "dispatchDidReachLayoutMilestone: dispatching DidFirstVisuallyNonEmptyLayoutForFrame");
        webPage->injectedBundleLoaderClient().didFirstVisuallyNonEmptyLayoutForFrame(*webPage, m_frame, userData);
        webPage->send(Messages::WebPageProxy::DidFirstVisuallyNonEmptyLayoutForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
    }
}

void WebLocalFrameLoaderClient::dispatchDidReachVisuallyNonEmptyState()
{
    if (!m_frame->page() || m_frame->page()->corePage()->settings().suppressesIncrementalRendering())
        return;
    ASSERT(m_frame->isRootFrame());
    completePageTransitionIfNeeded();
}

void WebLocalFrameLoaderClient::dispatchDidLayout()
{
    RefPtr webPage = m_frame->page();
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

LocalFrame* WebLocalFrameLoaderClient::dispatchCreatePage(const NavigationAction& navigationAction, NewFrameOpenerPolicy newFrameOpenerPolicy)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return nullptr;

    // Just call through to the chrome client.
    WindowFeatures windowFeatures;
    windowFeatures.noopener = newFrameOpenerPolicy == NewFrameOpenerPolicy::Suppress;
    Page* newPage = webPage->corePage()->chrome().createWindow(*m_frame->coreLocalFrame(), windowFeatures, navigationAction);
    if (!newPage)
        return nullptr;
    
    auto* localMainFrame = dynamicDowncast<LocalFrame>(newPage->mainFrame());
    if (!localMainFrame)
        return nullptr;

    return localMainFrame;
}

void WebLocalFrameLoaderClient::dispatchShow()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->show();
}

void WebLocalFrameLoaderClient::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest& request, WebCore::PolicyCheckIdentifier identifier, const String& downloadAttribute, FramePolicyFunction&& function)
{
    RefPtr webPage = m_frame->page();
    if (!webPage) {
        WebLocalFrameLoaderClient_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: ignoring because there's no web page");
        function(PolicyAction::Ignore, identifier);
        return;
    }

    if (!request.url().string()) {
        WebLocalFrameLoaderClient_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: continuing because the url string is null");
        function(PolicyAction::Use, identifier);
        return;
    }

    if (webPage->shouldSkipDecidePolicyForResponse(response, request)) {
        WebLocalFrameLoaderClient_RELEASE_LOG(Network, "dispatchDecidePolicyForResponse: continuing because injected bundle says so");
        function(PolicyAction::Use, identifier);
        return;
    }

    bool canShowResponse = webPage->canShowResponse(response);

    RefPtr coreFrame = m_frame->coreLocalFrame();
    RefPtr policyDocumentLoader = coreFrame ? coreFrame->loader().provisionalDocumentLoader() : nullptr;
    auto navigationID = policyDocumentLoader ? static_cast<WebDocumentLoader&>(*policyDocumentLoader).navigationID() : 0;

    auto protectedFrame = m_frame.copyRef();
    uint64_t listenerID = protectedFrame->setUpPolicyListener(identifier, WTFMove(function), WebFrame::ForNavigationAction::No);

    webPage->sendWithAsyncReply(Messages::WebPageProxy::DecidePolicyForResponse(protectedFrame->info(), navigationID, response, request, canShowResponse, downloadAttribute), [frame = protectedFrame, listenerID, identifier] (PolicyDecision&& policyDecision) {
        frame->didReceivePolicyDecision(listenerID, identifier, WTFMove(policyDecision));
    });
}

void WebLocalFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(const NavigationAction& navigationAction, const ResourceRequest& request, FormState* formState, const String& frameName, WebCore::PolicyCheckIdentifier identifier, FramePolicyFunction&& function)
{
    RefPtr webPage = m_frame->page();
    if (!webPage) {
        function(PolicyAction::Ignore, identifier);
        return;
    }

    uint64_t listenerID = m_frame->setUpPolicyListener(identifier, WTFMove(function), WebFrame::ForNavigationAction::No);

    auto& mouseEventData = navigationAction.mouseEventData();
    NavigationActionData navigationActionData {
        navigationAction.type(),
        modifiersForNavigationAction(navigationAction),
        mouseButton(navigationAction),
        syntheticClickType(navigationAction),
        WebProcess::singleton().userGestureTokenIdentifier(navigationAction.userGestureToken()),
        navigationAction.userGestureToken() ? navigationAction.userGestureToken()->authorizationToken() : std::nullopt,
        webPage->canHandleRequest(request),
        navigationAction.shouldOpenExternalURLsPolicy(),
        navigationAction.downloadAttribute(),
        mouseEventData ? mouseEventData->locationInRootViewCoordinates : FloatPoint(),
        { }, /* redirectResponse */
        false, /* treatAsSameOriginNavigation */
        false, /* hasOpenedFrames */
        false, /* openedByDOMWithOpener */
        navigationAction.newFrameOpenerPolicy() == NewFrameOpenerPolicy::Allow, /* hasOpener */
        { }, /* requesterOrigin */
        { }, /* requesterTopOrigin */
        std::nullopt, /* targetBackForwardItemIdentifier */
        std::nullopt, /* sourceBackForwardItemIdentifier */
        WebCore::LockHistory::No,
        WebCore::LockBackForwardList::No,
        { }, /* clientRedirectSourceForHistory */
        0, /* effectiveSandboxFlags */
        navigationAction.privateClickMeasurement(),
        { }, /* advancedPrivacyProtections */
        { }, /* originatorAdvancedPrivacyProtections */
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        WebHitTestResultData::fromNavigationActionAndLocalFrame(navigationAction, m_frame->coreLocalFrame()),
#endif
    };

    webPage->sendWithAsyncReply(Messages::WebPageProxy::DecidePolicyForNewWindowAction(m_frame->info(), navigationActionData, request, frameName), [frame = m_frame, listenerID, identifier] (PolicyDecision&& policyDecision) {
        frame->didReceivePolicyDecision(listenerID, identifier, WTFMove(policyDecision));
    });
}

void WebLocalFrameLoaderClient::applyToDocumentLoader(WebsitePoliciesData&& websitePolicies)
{
    auto* coreFrame = m_frame->coreLocalFrame();
    if (!coreFrame)
        return;

    RefPtr documentLoader = WebDocumentLoader::loaderForWebsitePolicies(*coreFrame);
    if (!documentLoader)
        return;

    WebsitePoliciesData::applyToDocumentLoader(WTFMove(websitePolicies), *documentLoader);
}

WebCore::AllowsContentJavaScript WebLocalFrameLoaderClient::allowsContentJavaScriptFromMostRecentNavigation() const
{
    RefPtr webPage = m_frame->page();
    return webPage ? webPage->allowsContentJavaScriptFromMostRecentNavigation() : WebCore::AllowsContentJavaScript::No;
}

void WebLocalFrameLoaderClient::dispatchDecidePolicyForNavigationAction(const NavigationAction& navigationAction, const ResourceRequest& request, const ResourceResponse& redirectResponse,
    FormState* formState, PolicyDecisionMode policyDecisionMode, WebCore::PolicyCheckIdentifier requestIdentifier, FramePolicyFunction&& function)
{
    WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(navigationAction, request, redirectResponse, formState, policyDecisionMode, requestIdentifier, WTFMove(function));
}

void WebLocalFrameLoaderClient::cancelPolicyCheck()
{
    m_frame->invalidatePolicyListeners();
}

void WebLocalFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
}

void WebLocalFrameLoaderClient::dispatchWillSendSubmitEvent(Ref<FormState>&& formState)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    Ref form = formState->form();

    ASSERT(formState->sourceDocument().frame());
    auto sourceFrame = WebFrame::fromCoreFrame(*formState->sourceDocument().frame());
    ASSERT(sourceFrame);

    webPage->injectedBundleFormClient().willSendSubmitEvent(webPage.get(), form.ptr(), m_frame.ptr(), sourceFrame.get(), formState->textFieldValues());
}

void WebLocalFrameLoaderClient::dispatchWillSubmitForm(FormState& formState, CompletionHandler<void()>&& completionHandler)
{
    RefPtr webPage = m_frame->page();
    if (!webPage) {
        completionHandler();
        return;
    }

    Ref form = formState.form();

    RefPtr sourceCoreFrame = formState.sourceDocument().frame();
    if (!sourceCoreFrame)
        return completionHandler();
    auto sourceFrame = WebFrame::fromCoreFrame(*sourceCoreFrame);
    if (!sourceFrame)
        return completionHandler();

    auto& values = formState.textFieldValues();

    RefPtr<API::Object> userData;
    webPage->injectedBundleFormClient().willSubmitForm(webPage.get(), form.ptr(), m_frame.ptr(), sourceFrame.get(), values, userData);

    webPage->sendWithAsyncReply(Messages::WebPageProxy::WillSubmitForm(m_frame->frameID(), sourceFrame->frameID(), values, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())), WTFMove(completionHandler));
}

void WebLocalFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

inline bool WebLocalFrameLoaderClient::hasPlugInView() const
{
#if ENABLE(PDF_PLUGIN)
    return m_pluginView;
#else
    return false;
#endif
}

void WebLocalFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError&)
{
#if ENABLE(PDF_PLUGIN)
    if (!m_pluginView)
        return;
    
    m_pluginView->manualLoadDidFail();
    m_pluginView = nullptr;
    m_hasSentResponseToPluginView = false;
#endif
}

void WebLocalFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebLocalFrameLoaderClient::startDownload(const ResourceRequest& request, const String& suggestedName)
{
    m_frame->startDownload(request, suggestedName);
}

void WebLocalFrameLoaderClient::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebLocalFrameLoaderClient::didChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebLocalFrameLoaderClient::willReplaceMultipartContent()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->willReplaceMultipartContent(m_frame);
}

void WebLocalFrameLoaderClient::didReplaceMultipartContent()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->didReplaceMultipartContent(m_frame);
}

void WebLocalFrameLoaderClient::committedLoad(DocumentLoader* loader, const SharedBuffer& data)
{
    if (!hasPlugInView())
        loader->commitData(data);

    // If the document is a stand-alone media document, now is the right time to cancel the WebKit load.
    // FIXME: This code should be shared across all ports. <http://webkit.org/b/48762>.
#if ENABLE(VIDEO)
    if (is<MediaDocument>(m_frame->coreLocalFrame()->document()))
        loader->cancelMainResourceLoad(pluginWillHandleLoadError(loader->response()));
#endif

#if ENABLE(PDF_PLUGIN)
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

void WebLocalFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    if (m_frameHasCustomContentProvider) {
        RefPtr webPage = m_frame->page();
        if (!webPage)
            return;

        RefPtr<const SharedBuffer> contiguousData;
        RefPtr<const FragmentedSharedBuffer> mainResourceData = loader->mainResourceData();
        if (mainResourceData)
            contiguousData = mainResourceData->makeContiguous();
        IPC::DataReference dataReference(contiguousData ? contiguousData->data() : nullptr, contiguousData ? contiguousData->size() : 0);
        webPage->send(Messages::WebPageProxy::DidFinishLoadingDataForCustomContentProvider(loader->response().suggestedFilename(), dataReference));
    }

#if ENABLE(PDF_PLUGIN)
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

void WebLocalFrameLoaderClient::updateGlobalHistory()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr loader = m_frame->coreLocalFrame()->loader().documentLoader();

    WebNavigationDataStore data;
    data.url = loader->url().string();
    // FIXME: Use direction of title.
    data.title = loader->title().string;
    data.originalRequest = loader->originalRequestCopy();
    data.response = loader->response();

    webPage->send(Messages::WebPageProxy::DidNavigateWithNavigationData(data, m_frame->frameID()));
}

void WebLocalFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreLocalFrame()->loader().documentLoader();
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

bool WebLocalFrameLoaderClient::shouldGoToHistoryItem(HistoryItem& item) const
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return false;
    webPage->send(Messages::WebPageProxy::WillGoToBackForwardListItem(item.identifier(), item.isInBackForwardCache()));
    return true;
}

void WebLocalFrameLoaderClient::didDisplayInsecureContent()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didDisplayInsecureContentForFrame(*webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidDisplayInsecureContentForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebLocalFrameLoaderClient::didRunInsecureContent(SecurityOrigin&, const URL&)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<API::Object> userData;

    webPage->injectedBundleLoaderClient().didRunInsecureContentForFrame(*webPage, m_frame, userData);

    webPage->send(Messages::WebPageProxy::DidRunInsecureContentForFrame(m_frame->frameID(), UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

ResourceError WebLocalFrameLoaderClient::cancelledError(const ResourceRequest& request) const
{
    return WebKit::cancelledError(request);
}

ResourceError WebLocalFrameLoaderClient::blockedError(const ResourceRequest& request) const
{
    return WebKit::blockedError(request);
}

ResourceError WebLocalFrameLoaderClient::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return WebKit::blockedByContentBlockerError(request);
}

ResourceError WebLocalFrameLoaderClient::cannotShowURLError(const ResourceRequest& request) const
{
    return WebKit::cannotShowURLError(request);
}

ResourceError WebLocalFrameLoaderClient::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    return WebKit::interruptedForPolicyChangeError(request);
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebLocalFrameLoaderClient::blockedByContentFilterError(const ResourceRequest& request) const
{
    return WebKit::blockedByContentFilterError(request);
}
#endif

ResourceError WebLocalFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    return WebKit::cannotShowMIMETypeError(response);
}

ResourceError WebLocalFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response) const
{
    return WebKit::fileDoesNotExistError(response);
}

ResourceError WebLocalFrameLoaderClient::httpsUpgradeRedirectLoopError(const ResourceRequest& request) const
{
    return WebKit::httpsUpgradeRedirectLoopError(request);
}

ResourceError WebLocalFrameLoaderClient::httpNavigationWithHTTPSOnlyError(const ResourceRequest& request) const
{
    return WebKit::httpNavigationWithHTTPSOnlyError(request);
}

ResourceError WebLocalFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    return WebKit::pluginWillHandleLoadError(response);
}

bool WebLocalFrameLoaderClient::shouldFallBack(const ResourceError& error) const
{
    static NeverDestroyed<const ResourceError> cancelledError(this->cancelledError(ResourceRequest()));
    static NeverDestroyed<const ResourceError> pluginWillHandleLoadError(this->pluginWillHandleLoadError(ResourceResponse()));

    if (error.errorCode() == cancelledError.get().errorCode() && error.domain() == cancelledError.get().domain())
        return false;

    if (error.errorCode() == pluginWillHandleLoadError.get().errorCode() && error.domain() == pluginWillHandleLoadError.get().domain())
        return false;

    return true;
}

bool WebLocalFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool WebLocalFrameLoaderClient::canShowMIMEType(const String& /*MIMEType*/) const
{
    notImplemented();
    return true;
}

bool WebLocalFrameLoaderClient::canShowMIMETypeAsHTML(const String& /*MIMEType*/) const
{
    return true;
}

bool WebLocalFrameLoaderClient::representationExistsForURLScheme(StringView /*URLScheme*/) const
{
    notImplemented();
    return false;
}

void WebLocalFrameLoaderClient::loadStorageAccessQuirksIfNeeded()
{
    RefPtr webPage = m_frame->page();

    if (!webPage || !m_frame->coreLocalFrame() || !m_frame->coreLocalFrame()->isMainFrame() || !m_frame->coreLocalFrame()->document())
        return;

    auto* document = m_frame->coreLocalFrame()->document();
    RegistrableDomain domain { document->url() };
    if (!WebProcess::singleton().haveStorageAccessQuirksForDomain(domain))
        return;

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::StorageAccessQuirkForTopFrameDomain(WTFMove(domain)), [weakDocument = WeakPtr { *document }](Vector<RegistrableDomain>&& domains) {
        if (!domains.size())
            return;
        if (!weakDocument)
            return;
        weakDocument->quirks().setSubFrameDomainsForStorageAccessQuirk(WTFMove(domains));
    });

}

String WebLocalFrameLoaderClient::generatedMIMETypeForURLScheme(StringView /*URLScheme*/) const
{
    notImplemented();
    return String();
}

void WebLocalFrameLoaderClient::frameLoadCompleted()
{
    // Note: Can be called multiple times.
    if (!m_frame->isMainFrame())
        return;
    completePageTransitionIfNeeded();
}

void WebLocalFrameLoaderClient::saveViewStateToItem(HistoryItem& historyItem)
{
#if PLATFORM(IOS_FAMILY)
    if (m_frame->isMainFrame())
        m_frame->page()->savePageState(historyItem);
#else
    UNUSED_PARAM(historyItem);
#endif
}

void WebLocalFrameLoaderClient::restoreViewState()
{
#if PLATFORM(IOS_FAMILY)
    auto& frame = *m_frame->coreLocalFrame();
    auto* currentItem = frame.loader().history().currentItem();
    if (auto* view = frame.view()) {
        if (m_frame->isMainFrame())
            m_frame->page()->restorePageState(*currentItem);
        else if (!view->wasScrolledByUser())
            view->setScrollPosition(currentItem->scrollPosition());
    }
#else
    // Inform the UI process of the scale factor.
    double scaleFactor = m_frame->coreLocalFrame()->loader().history().currentItem()->pageScaleFactor();

    // A scale factor of 0 means the history item has the default scale factor, thus we do not need to update it.
    if (scaleFactor)
        m_frame->page()->send(Messages::WebPageProxy::PageScaleFactorDidChange(scaleFactor));

    // FIXME: This should not be necessary. WebCore should be correctly invalidating
    // the view on restores from the back/forward cache.
    if (m_frame->page() && m_frame.ptr() == &m_frame->page()->mainWebFrame())
        m_frame->page()->drawingArea()->setNeedsDisplay();
#endif
}

void WebLocalFrameLoaderClient::provisionalLoadStarted()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    if (m_frame->isMainFrame()) {
        webPage->didStartPageTransition();
        m_didCompletePageTransition = false;
    }
}

void WebLocalFrameLoaderClient::didFinishLoad()
{
    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame.ptr());
}

void WebLocalFrameLoaderClient::prepareForDataSourceReplacement()
{
    notImplemented();
}

Ref<DocumentLoader> WebLocalFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return m_frame->page()->createDocumentLoader(*m_frame->coreLocalFrame(), request, substituteData);
}

void WebLocalFrameLoaderClient::updateCachedDocumentLoader(WebCore::DocumentLoader& loader)
{
    m_frame->page()->updateCachedDocumentLoader(static_cast<WebDocumentLoader&>(loader), *m_frame->coreLocalFrame());
}

void WebLocalFrameLoaderClient::setTitle(const StringWithDirection& title, const URL& url)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    // FIXME: Use direction of title.
    webPage->send(Messages::WebPageProxy::DidUpdateHistoryTitle(title.string, url.string(), m_frame->frameID()));
}

String WebLocalFrameLoaderClient::userAgent(const URL& url) const
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return String();

    return webPage->userAgent(url);
}

String WebLocalFrameLoaderClient::overrideContentSecurityPolicy() const
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return String();

    return webPage->overrideContentSecurityPolicy();
}

void WebLocalFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void WebLocalFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    const ResourceResponse& response = m_frame->coreLocalFrame()->loader().documentLoader()->response();
    m_frameHasCustomContentProvider = m_frame->isMainFrame() && m_frame->page()->shouldUseCustomContentProviderForResponse(response);
    m_frameCameFromBackForwardCache = true;
}

void WebLocalFrameLoaderClient::transitionToCommittedForNewPage()
{
    RefPtr webPage = m_frame->page();

    bool isMainFrame = m_frame->isMainFrame();
    bool shouldUseFixedLayout = isMainFrame && webPage->useFixedLayout();
    bool shouldDisableScrolling = isMainFrame && !webPage->mainFrameIsScrollable();
    bool shouldHideScrollbars = shouldDisableScrolling;
    IntRect fixedVisibleContentRect;

    auto oldView = m_frame->coreLocalFrame()->view();

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
        && m_frame->coreLocalFrame()->loader().documentLoader()
        && webPage->shouldUseCustomContentProviderForResponse(m_frame->coreLocalFrame()->loader().documentLoader()->response());
    m_frameCameFromBackForwardCache = false;

    ScrollbarMode defaultScrollbarMode = shouldHideScrollbars ? ScrollbarMode::AlwaysOff : ScrollbarMode::Auto;

    ScrollbarMode horizontalScrollbarMode = webPage->alwaysShowsHorizontalScroller() ? ScrollbarMode::AlwaysOn : defaultScrollbarMode;
    ScrollbarMode verticalScrollbarMode = webPage->alwaysShowsVerticalScroller() ? ScrollbarMode::AlwaysOn : defaultScrollbarMode;

    bool horizontalLock = shouldHideScrollbars || webPage->alwaysShowsHorizontalScroller();
    bool verticalLock = shouldHideScrollbars || webPage->alwaysShowsVerticalScroller();

    m_frame->coreLocalFrame()->createView(webPage->size(), webPage->backgroundColor(),
        webPage->fixedLayoutSize(), fixedVisibleContentRect, shouldUseFixedLayout,
        horizontalScrollbarMode, horizontalLock, verticalScrollbarMode, verticalLock);

    RefPtr view = m_frame->coreLocalFrame()->view();

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

    if (webPage->scrollPinningBehavior() != ScrollPinningBehavior::DoNotPin)
        view->setScrollPinningBehavior(webPage->scrollPinningBehavior());

#if USE(COORDINATED_GRAPHICS)
    if (shouldUseFixedLayout) {
        view->setDelegatedScrollingMode(shouldUseFixedLayout ? DelegatedScrollingMode::DelegatedToNativeScrollView : DelegatedScrollingMode::NotDelegated);
        view->setPaintsEntireContents(shouldUseFixedLayout);
        return;
    }
#endif
}

void WebLocalFrameLoaderClient::didRestoreFromBackForwardCache()
{
    m_frameCameFromBackForwardCache = true;
}

bool WebLocalFrameLoaderClient::canCachePage() const
{
    // We cannot cache frames that have custom representations because they are
    // rendered in the UIProcess.
    return !m_frameHasCustomContentProvider;
}

void WebLocalFrameLoaderClient::convertMainResourceLoadToDownload(DocumentLoader *documentLoader, const ResourceRequest& request, const ResourceResponse& response)
{
    m_frame->convertMainResourceLoadToDownload(documentLoader, request, response);
}

RefPtr<LocalFrame> WebLocalFrameLoaderClient::createFrame(const AtomString& name, HTMLFrameOwnerElement& ownerElement)
{
    RefPtr webPage = m_frame->page();
    ASSERT(webPage);
    auto subframe = WebFrame::createSubframe(*webPage, m_frame, name, ownerElement);
    auto* coreSubframe = subframe->coreLocalFrame();
    if (!coreSubframe)
        return nullptr;

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!coreSubframe->page())
        return nullptr;

    return coreSubframe;
}

RefPtr<Widget> WebLocalFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement& pluginElement, const URL& url, const Vector<AtomString>&, const Vector<AtomString>&, const String& mimeType, bool loadManually)
{
#if ENABLE(PDF_PLUGIN)
    return PluginView::create(pluginElement, url, mimeType, loadManually && !m_frameCameFromBackForwardCache);
#else
    UNUSED_PARAM(pluginElement);
    UNUSED_PARAM(url);
    UNUSED_PARAM(mimeType);
    UNUSED_PARAM(loadManually);
    return nullptr;
#endif
}

void WebLocalFrameLoaderClient::redirectDataToPlugin(Widget& pluginWidget)
{
#if ENABLE(PDF_PLUGIN)
    m_pluginView = static_cast<PluginView*>(&pluginWidget);
#else
    UNUSED_PARAM(pluginWidget);
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

ObjectContentType WebLocalFrameLoaderClient::objectContentType(const URL& url, const String& mimeTypeIn)
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
            if (RefPtr webPage = m_frame->page()) {
                if (pluginSupportsExtension(webPage->corePage()->pluginData(), extension))
                    return ObjectContentType::PlugIn;
            }
            return ObjectContentType::Frame;
        }
    }
#if ENABLE(PDFJS)
    if (RefPtr webPage = m_frame->page()) {
        if (webPage->corePage()->settings().pdfJSViewerEnabled() && MIMETypeRegistry::isPDFMIMEType(mimeType))
            return ObjectContentType::Frame;
    }
#endif
    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentType::Image;

    if (RefPtr webPage = m_frame->page()) {
        auto allowedPluginTypes = webFrame().coreLocalFrame()->arePluginsEnabled()
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

AtomString WebLocalFrameLoaderClient::overrideMediaType() const
{
    if (auto* page = m_frame->page())
        return page->overriddenMediaType();

    return nullAtom();
}

void WebLocalFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    RefPtr webPage = m_frame->page();
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

void WebLocalFrameLoaderClient::dispatchGlobalObjectAvailable(DOMWrapperWorld& world)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->globalObjectIsAvailableForFrame(*webPage, m_frame, world);
#endif

    webPage->injectedBundleLoaderClient().globalObjectIsAvailableForFrame(*webPage, m_frame, world);
}

void WebLocalFrameLoaderClient::dispatchServiceWorkerGlobalObjectAvailable(DOMWrapperWorld& world)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

#if ENABLE(WK_WEB_EXTENSIONS)
    if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
        extensionControllerProxy->serviceWorkerGlobalObjectIsAvailableForFrame(*webPage, m_frame, world);
#endif

    webPage->injectedBundleLoaderClient().serviceWorkerGlobalObjectIsAvailableForFrame(*webPage, m_frame, world);
}

void WebLocalFrameLoaderClient::willInjectUserScript(DOMWrapperWorld& world)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleLoaderClient().willInjectUserScriptForFrame(*webPage, m_frame, world);
}

void WebLocalFrameLoaderClient::dispatchWillDisconnectDOMWindowExtensionFromGlobalObject(WebCore::DOMWindowExtension* extension)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().willDisconnectDOMWindowExtensionFromGlobalObject(*webPage, extension);
}

void WebLocalFrameLoaderClient::dispatchDidReconnectDOMWindowExtensionToGlobalObject(WebCore::DOMWindowExtension* extension)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().didReconnectDOMWindowExtensionToGlobalObject(*webPage, extension);
}

void WebLocalFrameLoaderClient::dispatchWillDestroyGlobalObjectForDOMWindowExtension(WebCore::DOMWindowExtension* extension)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
        
    webPage->injectedBundleLoaderClient().willDestroyGlobalObjectForDOMWindowExtension(*webPage, extension);
}

#if PLATFORM(COCOA)
    
RemoteAXObjectRef WebLocalFrameLoaderClient::accessibilityRemoteObject()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return 0;
    
    return webPage->accessibilityRemoteObject();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void WebLocalFrameLoaderClient::setAXIsolatedTreeRoot(WebCore::AXCoreObject* axObject)
{
    ASSERT(isMainRunLoop());
    if (RefPtr webPage = m_frame->page())
        webPage->setAXIsolatedTreeRoot(axObject);
}
#endif

void WebLocalFrameLoaderClient::willCacheResponse(DocumentLoader*, ResourceLoaderIdentifier identifier, NSCachedURLResponse* response, CompletionHandler<void(NSCachedURLResponse *)>&& completionHandler) const
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return completionHandler(response);

    return completionHandler(webPage->injectedBundleResourceLoadClient().shouldCacheResponse(*webPage, m_frame, identifier) ? response : nil);
}

std::optional<double> WebLocalFrameLoaderClient::dataDetectionReferenceDate()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return std::nullopt;

    return webPage->dataDetectionReferenceDate();
}

#endif // PLATFORM(COCOA)

void WebLocalFrameLoaderClient::didChangeScrollOffset()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didChangeScrollOffsetForFrame(m_frame->coreLocalFrame());
}

bool WebLocalFrameLoaderClient::allowScript(bool enabledPerSettings)
{
    return enabledPerSettings && !hasPlugInView();
}

bool WebLocalFrameLoaderClient::shouldForceUniversalAccessFromLocalURL(const URL& url)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return false;

    return webPage->injectedBundleLoaderClient().shouldForceUniversalAccessFromLocalURL(*webPage, url.string());
}

Ref<FrameNetworkingContext> WebLocalFrameLoaderClient::createNetworkingContext()
{
    ASSERT(!hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));
    return WebFrameNetworkingContext::create(m_frame.ptr());
}

#if ENABLE(CONTENT_FILTERING)

void WebLocalFrameLoaderClient::contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler unblockHandler)
{
    if (!unblockHandler.needsUIProcess()) {
        m_frame->coreLocalFrame()->loader().policyChecker().setContentFilterUnblockHandler(WTFMove(unblockHandler));
        return;
    }

    if (WebPage* webPage { m_frame->page() })
        webPage->send(Messages::WebPageProxy::ContentFilterDidBlockLoadForFrame(unblockHandler, m_frame->frameID()));
}

#endif

void WebLocalFrameLoaderClient::prefetchDNS(const String& hostname)
{
    WebProcess::singleton().prefetchDNS(hostname);
}

void WebLocalFrameLoaderClient::sendH2Ping(const URL& url, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler)
{
    RefPtr webPage = m_frame->page();
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

void WebLocalFrameLoaderClient::didRestoreScrollPosition()
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->didRestoreScrollPosition();
}

void WebLocalFrameLoaderClient::getLoadDecisionForIcons(const Vector<std::pair<WebCore::LinkIcon&, uint64_t>>& icons)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    for (auto& icon : icons)
        webPage->send(Messages::WebPageProxy::GetLoadDecisionForIcon(icon.first, CallbackID::fromInteger(icon.second)));
}

void WebLocalFrameLoaderClient::broadcastFrameRemovalToOtherProcesses()
{
    WebFrameLoaderClient::broadcastFrameRemovalToOtherProcesses();
}

void WebLocalFrameLoaderClient::broadcastMainFrameURLChangeToOtherProcesses(const URL& url)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;
    webPage->send(Messages::WebPageProxy::BroadcastMainFrameURLChangeToOtherProcesses(url));
}

void WebLocalFrameLoaderClient::didFinishServiceWorkerPageRegistration(bool success)
{
    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->send(Messages::WebPageProxy::DidFinishServiceWorkerPageRegistration(success));
}

#if ENABLE(APP_BOUND_DOMAINS)
bool WebLocalFrameLoaderClient::shouldEnableInAppBrowserPrivacyProtections() const
{
    return m_frame->shouldEnableInAppBrowserPrivacyProtections();
}

void WebLocalFrameLoaderClient::notifyPageOfAppBoundBehavior()
{
    if (!m_frame->isMainFrame())
        return;

    RefPtr webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->notifyPageOfAppBoundBehavior();
}
#endif

#if ENABLE(PDF_PLUGIN)

bool WebLocalFrameLoaderClient::shouldUsePDFPlugin(const String& contentType, StringView path) const
{
    auto* page = m_frame->page();
    return page && page->shouldUsePDFPlugin(contentType, path);
}

#endif

bool WebLocalFrameLoaderClient::isParentProcessAFullWebBrowser() const
{
    auto* page = m_frame->page();
    return page && page->isParentProcessAWebBrowser();
}

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
void WebLocalFrameLoaderClient::modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>)>&& completionHandler) const
{
    RefPtr webPage = m_frame->page();
    if (!webPage) {
        completionHandler({ });
        return;
    }

    webPage->sendWithAsyncReply(Messages::WebPageProxy::ModelInlinePreviewUUIDs(), WTFMove(completionHandler));
}
#endif

void WebLocalFrameLoaderClient::dispatchLoadEventToOwnerElementInAnotherProcess()
{
    auto* page = m_frame->page();
    if (!page)
        return;
    page->send(Messages::WebPageProxy::DispatchLoadEventToFrameOwnerElement(m_frame->frameID()));
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

void WebLocalFrameLoaderClient::didAccessWindowProxyPropertyViaOpener(WebCore::SecurityOriginData&& parentOrigin, WebCore::WindowProxyProperty property)
{
    RefPtr page = m_frame->page();
    if (!page)
        return;

    page->send(Messages::WebPageProxy::DidAccessWindowProxyPropertyViaOpenerForFrame(m_frame->frameID(), WTFMove(parentOrigin), property));
}

#endif

} // namespace WebKit

#undef PREFIX_PARAMETERS
#undef WEBFRAME
#undef WEBFRAMEID
#undef WEBPAGE
#undef WEBPAGEID
