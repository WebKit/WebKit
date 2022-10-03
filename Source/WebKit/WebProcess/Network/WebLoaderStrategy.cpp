/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "WebLoaderStrategy.h"

#include "HangDetectionDisabler.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "RemoteWorkerFrameLoaderClient.h"
#include "WebCompiledContentRuleList.h"
#include "WebCoreArgumentCoders.h"
#include "WebDocumentLoader.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoader.h"
#include "WebSWContextManagerConnection.h"
#include "WebServiceWorkerProvider.h"
#include "WebURLSchemeHandlerProxy.h"
#include "WebURLSchemeTaskProxy.h"
#include <WebCore/ApplicationCacheHost.h>
#include <WebCore/CachedResource.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/DataURLDecoder.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/InspectorInstrumentationWebKit.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/UserContentProvider.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/CString.h>

#if USE(QUICK_LOOK)
#include <WebCore/QuickLook.h>
#endif

#define WEBLOADERSTRATEGY_RELEASE_LOG_BASIC(fmt, ...) RELEASE_LOG(Network, "%p - WebLoaderStrategy::" fmt, this, ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_RELEASE_LOG_ERROR_BASIC(fmt, ...) RELEASE_LOG_ERROR(Network, "%p - WebLoaderStrategy::" fmt, this, ##__VA_ARGS__)

#define WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE "%p - [resourceLoader=%p, frameLoader=%p, frame=%p, webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 "] WebLoaderStrategy::"
#define WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_PARAMETERS this, &resourceLoader, resourceLoader.frameLoader(), resourceLoader.frame(), trackingParameters.pageID.toUInt64(), trackingParameters.frameID.toUInt64(), trackingParameters.resourceID.toUInt64()
#define WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_STANDARD_PARAMETERS this, nullptr, &frameLoader, &frameLoader.frame(), trackingParameters.pageID.toUInt64(), trackingParameters.frameID.toUInt64(), trackingParameters.resourceID.toUInt64()

#define WEBLOADERSTRATEGY_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)

#define WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WebLoaderStrategy::WebLoaderStrategy()
    : m_internallyFailedLoadTimer(RunLoop::main(), this, &WebLoaderStrategy::internallyFailedLoadTimerFired)
{
}

WebLoaderStrategy::~WebLoaderStrategy()
{
}

void WebLoaderStrategy::loadResource(Frame& frame, CachedResource& resource, ResourceRequest&& request, const ResourceLoaderOptions& options, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&& completionHandler)
{
    if (resource.type() != CachedResource::Type::MainResource || !frame.isMainFrame()) {
        if (auto* document = frame.mainFrame().document()) {
            if (document && document->loader())
                request.setIsAppInitiated(document->loader()->lastNavigationWasAppInitiated());
        }
    }

    SubresourceLoader::create(frame, resource, WTFMove(request), options, [this, referrerPolicy = options.referrerPolicy, completionHandler = WTFMove(completionHandler), resource = CachedResourceHandle<CachedResource>(&resource), frame = Ref { frame }] (RefPtr<SubresourceLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(*loader, resource.get(), referrerPolicy == ReferrerPolicy::NoReferrerWhenDowngrade);
        else
            RELEASE_LOG(Network, "%p - [webPageID=%" PRIu64 ", frameID=%" PRIu64 "] WebLoaderStrategy::loadResource: Unable to create SubresourceLoader", this, valueOrDefault(frame->pageID()).toUInt64(), valueOrDefault(frame->frameID()).toUInt64());
        completionHandler(WTFMove(loader));
    });
}

void WebLoaderStrategy::schedulePluginStreamLoad(Frame& frame, NetscapePlugInStreamLoaderClient& client, ResourceRequest&& request, CompletionHandler<void(RefPtr<NetscapePlugInStreamLoader>&&)>&& completionHandler)
{
    NetscapePlugInStreamLoader::create(frame, client, WTFMove(request), [this, completionHandler = WTFMove(completionHandler), frame = Ref { frame }] (RefPtr<NetscapePlugInStreamLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(*loader, 0, frame->document()->referrerPolicy() == ReferrerPolicy::NoReferrerWhenDowngrade);
        completionHandler(WTFMove(loader));
    });
}

static Seconds maximumBufferingTime(CachedResource* resource)
{
    if (!resource)
        return 0_s;

    switch (resource->type()) {
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::CSSStyleSheet:
    case CachedResource::Type::Script:
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::ModelResource:
#endif
        return Seconds::infinity();
    case CachedResource::Type::ImageResource:
        return 500_ms;
    case CachedResource::Type::MediaResource:
        return WebLoaderStrategy::mediaMaximumBufferingTime;
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::TextTrackResource:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
        return 0_s;
    }

    ASSERT_NOT_REACHED();
    return 0_s;
}

void WebLoaderStrategy::scheduleLoad(ResourceLoader& resourceLoader, CachedResource* resource, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    auto identifier = resourceLoader.identifier();
    ASSERT(identifier);

    auto& frameLoaderClient = resourceLoader.frameLoader()->client();

    WebResourceLoader::TrackingParameters trackingParameters;
    if (auto* webFrameLoaderClient = toWebFrameLoaderClient(frameLoaderClient))
        trackingParameters.webPageProxyID = valueOrDefault(webFrameLoaderClient->webPageProxyID());
    else if (is<RemoteWorkerFrameLoaderClient>(frameLoaderClient))
        trackingParameters.webPageProxyID = downcast<RemoteWorkerFrameLoaderClient>(frameLoaderClient).webPageProxyID();
    trackingParameters.pageID = valueOrDefault(frameLoaderClient.pageID());
    trackingParameters.frameID = valueOrDefault(frameLoaderClient.frameID());
    trackingParameters.resourceID = identifier;

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    // If the DocumentLoader schedules this as an archive resource load,
    // then we should remember the ResourceLoader in our records but not schedule it in the NetworkProcess.
    if (resourceLoader.documentLoader()->scheduleArchiveLoad(resourceLoader, resourceLoader.request())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as an archive resource.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be handled as an archive resource");
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader, trackingParameters));
        return;
    }
#endif

    if (resourceLoader.documentLoader()->applicationCacheHost().maybeLoadResource(resourceLoader, resourceLoader.request(), resourceLoader.request().url())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded from application cache.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be loaded from application cache");
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader, trackingParameters));
        return;
    }

    if (resourceLoader.request().url().protocolIsData()) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded as data.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be loaded as data");
        startLocalLoad(resourceLoader);
        return;
    }

#if USE(QUICK_LOOK)
    if (isQuickLookPreviewURL(resourceLoader.request().url())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a QuickLook resource.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be handled as a QuickLook resource");
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if USE(SOUP)
    // For apps that call g_resource_load in a web extension.
    // https://blogs.gnome.org/alexl/2012/01/26/resources-in-glib/
    if (resourceLoader.request().url().protocolIs("resource"_s)) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a GResource.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be handled as a GResource");
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if ENABLE(PDFJS)
    if (tryLoadingUsingPDFJSHandler(resourceLoader, trackingParameters))
        return;
#endif

    if (tryLoadingUsingURLSchemeHandler(resourceLoader, trackingParameters))
        return;

    if (InspectorInstrumentationWebKit::shouldInterceptRequest(resourceLoader)) {
        InspectorInstrumentationWebKit::interceptRequest(resourceLoader, [this, protectedResourceLoader = Ref { resourceLoader }, trackingParameters, shouldClearReferrerOnHTTPSToHTTPRedirect, resource](const ResourceRequest& request) {
            auto& resourceLoader = protectedResourceLoader.get();
            WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: intercepted URL will be scheduled with the NetworkProcess");
            scheduleLoadFromNetworkProcess(resourceLoader, request, trackingParameters, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime(resource));
        });
        return;
    }

    WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be scheduled with the NetworkProcess");
    scheduleLoadFromNetworkProcess(resourceLoader, resourceLoader.request(), trackingParameters, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime(resource));
}

bool WebLoaderStrategy::tryLoadingUsingURLSchemeHandler(ResourceLoader& resourceLoader, const WebResourceLoader::TrackingParameters& trackingParameters)
{
    RefPtr<WebPage> webPage;
    RefPtr<WebFrame> webFrame;

    if (auto* webFrameLoaderClient = toWebFrameLoaderClient(resourceLoader.frameLoader()->client())) {
        webFrame = &webFrameLoaderClient->webFrame();
        webPage = webFrame->page();
    } else if (auto* workerFrameLoaderClient = dynamicDowncast<RemoteWorkerFrameLoaderClient>(resourceLoader.frameLoader()->client())) {
        if (auto serviceWorkerPageIdentifier = workerFrameLoaderClient->serviceWorkerPageIdentifier()) {
            if (auto* page = Page::serviceWorkerPage(*serviceWorkerPageIdentifier)) {
                webPage = &WebPage::fromCorePage(*page);
                auto* frame = webPage->mainFrame();
                webFrame = frame ? WebFrame::fromCoreFrame(*frame) : nullptr;
            }
        }
    }

    if (!webPage || !webFrame)
        return false;

    auto* handler = webPage->urlSchemeHandlerForScheme(resourceLoader.request().url().protocol());
    if (!handler)
        return false;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, URL '%s' will be handled by a UIProcess URL scheme handler.", resourceLoader.url().string().utf8().data());
    WEBLOADERSTRATEGY_RELEASE_LOG("tryLoadingUsingURLSchemeHandler: URL will be handled by a UIProcess URL scheme handler");

    handler->startNewTask(resourceLoader, *webFrame);
    return true;
}

#if ENABLE(PDFJS)
bool WebLoaderStrategy::tryLoadingUsingPDFJSHandler(ResourceLoader& resourceLoader, const WebResourceLoader::TrackingParameters& trackingParameters)
{
    if (!resourceLoader.request().url().protocolIs("webkit-pdfjs-viewer"_s))
        return false;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a PDFJS resource.", resourceLoader.url().string().utf8().data());
    WEBLOADERSTRATEGY_RELEASE_LOG("tryLoadingUsingPDFJSHandler: URL will be scheduled with the PDFJS url scheme handler");

    startLocalLoad(resourceLoader);
    return true;
}
#endif

static void addParametersShared(const Frame* frame, NetworkResourceLoadParameters& parameters, bool isMainFrameNavigation = false)
{
    parameters.crossOriginAccessControlCheckEnabled = CrossOriginAccessControlCheckDisabler::singleton().crossOriginAccessControlCheckEnabled();
    parameters.hadMainFrameMainResourcePrivateRelayed = WebProcess::singleton().hadMainFrameMainResourcePrivateRelayed();

    if (!frame)
        return;

    // When loading the main frame, we need to get allowPrivacyProxy from the same DocumentLoader that
    // WebFrameLoaderClient::applyToDocumentLoader stored the value on. Otherwise, we need to get the
    // value from the main frame's current DocumentLoader.
    auto& mainFrame = frame->mainFrame();
    auto* mainFrameDocumentLoader = mainFrame.loader().policyDocumentLoader();
    if (!mainFrameDocumentLoader)
        mainFrameDocumentLoader = mainFrame.loader().provisionalDocumentLoader();
    if (!mainFrameDocumentLoader || !isMainFrameNavigation)
        mainFrameDocumentLoader = mainFrame.loader().documentLoader();

    parameters.allowPrivacyProxy = mainFrameDocumentLoader ? mainFrameDocumentLoader->allowPrivacyProxy() : true;

    if (auto* document = frame->document())
        parameters.crossOriginEmbedderPolicy = document->crossOriginEmbedderPolicy();

    if (auto* page = frame->page()) {
        parameters.pageHasResourceLoadClient = page->hasResourceLoadClient();
        parameters.shouldRelaxThirdPartyCookieBlocking = page->shouldRelaxThirdPartyCookieBlocking();
        page->logMediaDiagnosticMessage(parameters.request.httpBody());
    }

    if (auto* ownerElement = frame->ownerElement()) {
        if (auto* parentFrame = ownerElement->document().frame()) {
            parameters.parentFrameID = parentFrame->loader().frameID();
            parameters.parentCrossOriginEmbedderPolicy = ownerElement->document().crossOriginEmbedderPolicy();
            parameters.parentFrameURL = ownerElement->document().url();
        }
    }

    parameters.networkConnectionIntegrityEnabled = mainFrameDocumentLoader && mainFrameDocumentLoader->networkConnectionIntegrityEnabled();
}

void WebLoaderStrategy::scheduleLoadFromNetworkProcess(ResourceLoader& resourceLoader, const ResourceRequest& request, const WebResourceLoader::TrackingParameters& trackingParameters, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Seconds maximumBufferingTime)
{
    auto identifier = resourceLoader.identifier();
    ASSERT(identifier);

    auto* frame = resourceLoader.frame();

    if (auto* page = frame ? frame->page() : nullptr) {
        auto mainFrameMainResource = frame->isMainFrame()
            && resourceLoader.frameLoader()
            && resourceLoader.frameLoader()->notifier().isInitialRequestIdentifier(identifier)
            ? MainFrameMainResource::Yes : MainFrameMainResource::No;
        if (!page->allowsLoadFromURL(request.url(), mainFrameMainResource)) {
            RunLoop::main().dispatch([resourceLoader = Ref { resourceLoader }, error = blockedError(request)] {
                resourceLoader->didFail(error);
            });
            return;
        }
    }

    ContentSniffingPolicy contentSniffingPolicy = resourceLoader.shouldSniffContent() ? ContentSniffingPolicy::SniffContent : ContentSniffingPolicy::DoNotSniffContent;
    ContentEncodingSniffingPolicy contentEncodingSniffingPolicy = resourceLoader.shouldSniffContentEncoding() ? ContentEncodingSniffingPolicy::Sniff : ContentEncodingSniffingPolicy::DoNotSniff;
    StoredCredentialsPolicy storedCredentialsPolicy = resourceLoader.shouldUseCredentialStorage() ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be scheduled with the NetworkProcess with priority %d, storedCredentialsPolicy %i", resourceLoader.url().string().latin1().data(), static_cast<int>(resourceLoader.request().priority()), (int)storedCredentialsPolicy);

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = identifier;
    loadParameters.webPageProxyID = trackingParameters.webPageProxyID;
    loadParameters.webPageID = trackingParameters.pageID;
    loadParameters.webFrameID = trackingParameters.frameID;
    loadParameters.parentPID = presentingApplicationPID();
#if HAVE(AUDIT_TOKEN)
    loadParameters.networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken();
#endif
    loadParameters.request = request;
    loadParameters.contentSniffingPolicy = contentSniffingPolicy;
    loadParameters.contentEncodingSniffingPolicy = contentEncodingSniffingPolicy;
    loadParameters.storedCredentialsPolicy = storedCredentialsPolicy;
    // If there is no WebFrame then this resource cannot be authenticated with the client.
    loadParameters.clientCredentialPolicy = (loadParameters.webFrameID && loadParameters.webPageID && resourceLoader.isAllowedToAskUserForCredentials()) ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect;
    loadParameters.needsCertificateInfo = resourceLoader.shouldIncludeCertificateInfo();
    loadParameters.maximumBufferingTime = maximumBufferingTime;
    loadParameters.options = resourceLoader.options();
    loadParameters.preflightPolicy = resourceLoader.options().preflightPolicy;
    bool isMainFrameNavigation = resourceLoader.frame() && resourceLoader.frame()->isMainFrame() && resourceLoader.options().mode == FetchOptions::Mode::Navigate;
    addParametersShared(frame, loadParameters, isMainFrameNavigation);

#if ENABLE(SERVICE_WORKER)
    loadParameters.serviceWorkersMode = resourceLoader.options().loadedFromOpaqueSource == LoadedFromOpaqueSource::No ? resourceLoader.options().serviceWorkersMode : ServiceWorkersMode::None;
    loadParameters.serviceWorkerRegistrationIdentifier = resourceLoader.options().serviceWorkerRegistrationIdentifier;
    loadParameters.httpHeadersToKeep = resourceLoader.options().httpHeadersToKeep;
    if (resourceLoader.options().navigationPreloadIdentifier)
        loadParameters.navigationPreloadIdentifier = resourceLoader.options().navigationPreloadIdentifier;
#endif

    auto* document = frame ? frame->document() : nullptr;
    if (resourceLoader.options().cspResponseHeaders)
        loadParameters.cspResponseHeaders = resourceLoader.options().cspResponseHeaders;
    else if (document && !document->shouldBypassMainWorldContentSecurityPolicy() && resourceLoader.options().contentSecurityPolicyImposition == ContentSecurityPolicyImposition::DoPolicyCheck) {
        if (auto* contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }

    if (resourceLoader.options().crossOriginEmbedderPolicy)
        loadParameters.crossOriginEmbedderPolicy = *resourceLoader.options().crossOriginEmbedderPolicy;
    
#if ENABLE(APP_BOUND_DOMAINS) || ENABLE(CONTENT_EXTENSIONS)
    auto* webFrameLoaderClient = frame ? toWebFrameLoaderClient(frame->loader().client()) : nullptr;
    auto* webFrame = webFrameLoaderClient ? &webFrameLoaderClient->webFrame() : nullptr;
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    if (webFrame)
        loadParameters.isNavigatingToAppBoundDomain = webFrame->isTopFrameNavigatingToAppBoundDomain();
#endif

    if (document) {
        loadParameters.frameURL = document->url();
#if ENABLE(CONTENT_EXTENSIONS)
        loadParameters.mainDocumentURL = document->topDocument().url();
        // FIXME: Instead of passing userContentControllerIdentifier, the NetworkProcess should be able to get it using webPageId.
        if (auto* webPage = webFrame ? webFrame->page() : nullptr)
            loadParameters.userContentControllerIdentifier = webPage->userContentControllerIdentifier();
#endif
    }

    // FIXME: All loaders should provide their origin if navigation mode is cors/no-cors/same-origin.
    // As a temporary approach, we use the document origin if available or the HTTP Origin header otherwise.
    if (is<SubresourceLoader>(resourceLoader)) {
        auto& loader = downcast<SubresourceLoader>(resourceLoader);
        loadParameters.sourceOrigin = loader.origin();

        if (auto* headers = loader.originalHeaders())
            loadParameters.originalRequestHeaders = *headers;
    }

    if (!loadParameters.sourceOrigin && document)
        loadParameters.sourceOrigin = &document->securityOrigin();
    if (!loadParameters.sourceOrigin) {
        auto origin = request.httpOrigin();
        if (!origin.isNull())
            loadParameters.sourceOrigin = SecurityOrigin::createFromString(origin);
    }
    if (document) {
        loadParameters.topOrigin = &document->topOrigin();
        loadParameters.documentURL = document->url();
    }

    if (loadParameters.options.mode != FetchOptions::Mode::Navigate) {
        ASSERT(loadParameters.sourceOrigin);
        if (!loadParameters.sourceOrigin) {
            WEBLOADERSTRATEGY_RELEASE_LOG_ERROR("scheduleLoad: no sourceOrigin (priority=%d)", static_cast<int>(resourceLoader.request().priority()));
            scheduleInternallyFailedLoad(resourceLoader);
            return;
        }
    }

    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();

    loadParameters.isMainFrameNavigation = isMainFrameNavigation;
    if (loadParameters.isMainFrameNavigation && document)
        loadParameters.sourceCrossOriginOpenerPolicy = document->crossOriginOpenerPolicy();

    loadParameters.isMainResourceNavigationForAnyFrame = resourceLoader.frame() && resourceLoader.options().mode == FetchOptions::Mode::Navigate;
    if (loadParameters.isMainResourceNavigationForAnyFrame) {
        if (auto documentLoader = resourceLoader.documentLoader()) {
            loadParameters.navigationID = static_cast<WebDocumentLoader&>(*documentLoader).navigationID();
            loadParameters.navigationRequester = documentLoader->triggeringAction().requester();
        }
    }
    loadParameters.isCrossOriginOpenerPolicyEnabled = document && document->settings().crossOriginOpenerPolicyEnabled();
    loadParameters.isDisplayingInitialEmptyDocument = frame && frame->loader().stateMachine().isDisplayingInitialEmptyDocument();
    if (frame)
        loadParameters.effectiveSandboxFlags = frame->loader().effectiveSandboxFlags();
    if (auto openerFrame = frame ? frame->loader().opener() : nullptr) {
        if (auto openerDocument = openerFrame->document())
            loadParameters.openerURL = openerDocument->url();
    }

    loadParameters.shouldEnableCrossOriginResourcePolicy = !loadParameters.isMainFrameNavigation;

    if (resourceLoader.options().mode == FetchOptions::Mode::Navigate) {
        Vector<RefPtr<SecurityOrigin>> frameAncestorOrigins;
        for (auto* frame = resourceLoader.frame()->tree().parent(); frame; frame = frame->tree().parent())
            frameAncestorOrigins.append(&frame->document()->securityOrigin());
        loadParameters.frameAncestorOrigins = WTFMove(frameAncestorOrigins);
    }

    ASSERT((loadParameters.webPageID && loadParameters.webFrameID) || loadParameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials);

    std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume;
    if (loadParameters.isMainFrameNavigation)
        existingNetworkResourceLoadIdentifierToResume = std::exchange(m_existingNetworkResourceLoadIdentifierToResume, std::nullopt);
    WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: Resource is being scheduled with the NetworkProcess (priority=%d, existingNetworkResourceLoadIdentifierToResume=%" PRIu64 ")", static_cast<int>(resourceLoader.request().priority()), valueOrDefault(existingNetworkResourceLoadIdentifierToResume).toUInt64());
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad(loadParameters, existingNetworkResourceLoadIdentifierToResume), 0)) {
        WEBLOADERSTRATEGY_RELEASE_LOG_ERROR("scheduleLoad: Unable to schedule resource with the NetworkProcess (priority=%d)", static_cast<int>(resourceLoader.request().priority()));
        // We probably failed to schedule this load with the NetworkProcess because it had crashed.
        // This load will never succeed so we will schedule it to fail asynchronously.
        scheduleInternallyFailedLoad(resourceLoader);
        return;
    }

    auto loader = WebResourceLoader::create(resourceLoader, trackingParameters);
    m_webResourceLoaders.set(identifier, WTFMove(loader));
}

void WebLoaderStrategy::scheduleInternallyFailedLoad(WebCore::ResourceLoader& resourceLoader)
{
    m_internallyFailedResourceLoaders.add(&resourceLoader);
    m_internallyFailedLoadTimer.startOneShot(0_s);
}

void WebLoaderStrategy::internallyFailedLoadTimerFired()
{
    for (auto& resourceLoader : copyToVector(m_internallyFailedResourceLoaders))
        resourceLoader->didFail(internalError(resourceLoader->url()));
}

void WebLoaderStrategy::startLocalLoad(WebCore::ResourceLoader& resourceLoader)
{
    resourceLoader.start();
    m_webResourceLoaders.set(resourceLoader.identifier(), WebResourceLoader::create(resourceLoader, { }));
}

void WebLoaderStrategy::addURLSchemeTaskProxy(WebURLSchemeTaskProxy& task)
{
    auto result = m_urlSchemeTasks.add(task.identifier(), &task);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void WebLoaderStrategy::removeURLSchemeTaskProxy(WebURLSchemeTaskProxy& task)
{
    m_urlSchemeTasks.remove(task.identifier());
}

void WebLoaderStrategy::remove(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);
    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::remove, url '%s'", resourceLoader->url().string().utf8().data());

    auto identifier = resourceLoader->identifier();
    if (!identifier) {
        LOG_ERROR("WebLoaderStrategy removing a ResourceLoader that has no identifier.");
        return;
    }

    if (auto task = m_urlSchemeTasks.take(resourceLoader->identifier())) {
        ASSERT(!m_internallyFailedResourceLoaders.contains(resourceLoader));
        task->stopLoading();
        return;
    }

    if (m_internallyFailedResourceLoaders.contains(resourceLoader)) {
        m_internallyFailedResourceLoaders.remove(resourceLoader);
        return;
    }

    RefPtr<WebResourceLoader> loader = m_webResourceLoaders.take(identifier);
    // Loader may not be registered if we created it, but haven't scheduled yet (a bundle client can decide to cancel such request via willSendRequest).
    if (!loader)
        return;

    if (auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::RemoveLoadIdentifier(identifier), 0);

    // It's possible that this WebResourceLoader might be just about to message back to the NetworkProcess (e.g. ContinueWillSendRequest)
    // but there's no point in doing so anymore.
    loader->detachFromCoreLoader();
}

void WebLoaderStrategy::setDefersLoading(ResourceLoader&, bool)
{
}

void WebLoaderStrategy::crossOriginRedirectReceived(ResourceLoader*, const URL&)
{
    // We handle cross origin redirects entirely within the NetworkProcess.
    // We override this call in the WebProcess to make it a no-op.
}

void WebLoaderStrategy::servePendingRequests(ResourceLoadPriority)
{
    // This overrides the base class version.
    // We don't need to do anything as this is handled by the network process.
}

void WebLoaderStrategy::suspendPendingRequests()
{
    // Network process does keep requests in pending state.
}

void WebLoaderStrategy::resumePendingRequests()
{
    // Network process does keep requests in pending state.
}

void WebLoaderStrategy::networkProcessCrashed()
{
    WEBLOADERSTRATEGY_RELEASE_LOG_ERROR_BASIC("networkProcessCrashed: failing all pending resource loaders");

    for (auto& loader : m_webResourceLoaders.values()) {
        scheduleInternallyFailedLoad(*loader->resourceLoader());
        loader->detachFromCoreLoader();
    }

    m_webResourceLoaders.clear();

    auto pingLoadCompletionHandlers = WTFMove(m_pingLoadCompletionHandlers);
    for (auto& pingLoadCompletionHandler : pingLoadCompletionHandlers.values())
        pingLoadCompletionHandler(internalError(URL()), { });

    auto preconnectCompletionHandlers = WTFMove(m_preconnectCompletionHandlers);
    for (auto& preconnectCompletionHandler : preconnectCompletionHandlers.values())
        preconnectCompletionHandler(internalError(URL()));
}

static bool shouldClearReferrerOnHTTPSToHTTPRedirect(Frame* frame)
{
    if (frame) {
        if (auto* document = frame->document())
            return document->referrerPolicy() == ReferrerPolicy::NoReferrerWhenDowngrade;
    }
    return true;
}

WebLoaderStrategy::SyncLoadResult WebLoaderStrategy::loadDataURLSynchronously(const ResourceRequest& request)
{
    auto mode = DataURLDecoder::Mode::Legacy;
    if (request.requester() == ResourceRequest::Requester::Fetch)
        mode = DataURLDecoder::Mode::ForgivingBase64;

    SyncLoadResult result;
    auto decodeResult = DataURLDecoder::decode(request.url(), mode);
    if (!decodeResult) {
        WEBLOADERSTRATEGY_RELEASE_LOG_BASIC("loadDataURLSynchronously: decoding of data failed");
        result.error = internalError(request.url());
        return result;
    }

    result.response = ResourceResponse::dataURLResponse(request.url(), decodeResult.value());
    result.data = WTFMove(decodeResult->data);

    return result;
}

std::optional<WebLoaderStrategy::SyncLoadResult> WebLoaderStrategy::tryLoadingSynchronouslyUsingURLSchemeHandler(FrameLoader& frameLoader, WebCore::ResourceLoaderIdentifier identifier, const ResourceRequest& request)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(frameLoader.client());
    auto* webFrame = webFrameLoaderClient ? &webFrameLoaderClient->webFrame() : nullptr;
    auto* webPage = webFrame ? webFrame->page() : nullptr;
    if (!webPage)
        return std::nullopt;

    auto* handler = webPage->urlSchemeHandlerForScheme(request.url().protocol());
    if (!handler)
        return std::nullopt;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, sync load to URL '%s' will be handled by a UIProcess URL scheme handler.", request.url().string().utf8().data());

    SyncLoadResult result;
    handler->loadSynchronously(identifier, *webFrame, request, result.response, result.error, result.data);

    return result;
}

void WebLoaderStrategy::loadResourceSynchronously(FrameLoader& frameLoader, WebCore::ResourceLoaderIdentifier resourceLoadIdentifier, const ResourceRequest& request, ClientCredentialPolicy clientCredentialPolicy,  const FetchOptions& options, const HTTPHeaderMap& originalRequestHeaders, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(frameLoader.client());
    auto* webFrame = webFrameLoaderClient ? &webFrameLoaderClient->webFrame() : nullptr;
    auto* webPage = webFrame ? webFrame->page() : nullptr;
    auto* page = webPage ? webPage->corePage() : nullptr;

    auto webPageProxyID = webPage ? webPage->webPageProxyIdentifier() : WebPageProxyIdentifier { };
    auto pageID = webPage ? webPage->identifier() : PageIdentifier { };
    auto frameID = webFrame ? webFrame->frameID() : FrameIdentifier { };

    WebResourceLoader::TrackingParameters trackingParameters;
    trackingParameters.pageID = pageID;
    trackingParameters.frameID = frameID;
    trackingParameters.resourceID = resourceLoadIdentifier;

    auto* document = frameLoader.frame().document();
    if (!document) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: no document");
        error = internalError(request.url());
        return;
    }

    if (request.url().protocolIsData()) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: URL will be loaded as data");
        auto syncLoadResult = loadDataURLSynchronously(request);
        error = WTFMove(syncLoadResult.error);
        response = WTFMove(syncLoadResult.response);
        data = WTFMove(syncLoadResult.data);
        return;
    }

    if (auto syncLoadResult = tryLoadingSynchronouslyUsingURLSchemeHandler(frameLoader, resourceLoadIdentifier, request)) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: failed calling tryLoadingSynchronouslyUsingURLSchemeHandler (error=%d)", syncLoadResult->error.errorCode());
        error = WTFMove(syncLoadResult->error);
        response = WTFMove(syncLoadResult->response);
        data = WTFMove(syncLoadResult->data);
        return;
    }

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = resourceLoadIdentifier;
    loadParameters.webPageProxyID = webPageProxyID;
    loadParameters.webPageID = pageID;
    loadParameters.webFrameID = frameID;
    loadParameters.parentPID = presentingApplicationPID();
#if HAVE(AUDIT_TOKEN)
    loadParameters.networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken();
#endif
    loadParameters.request = request;
    loadParameters.contentSniffingPolicy = ContentSniffingPolicy::SniffContent;
    loadParameters.contentEncodingSniffingPolicy = ContentEncodingSniffingPolicy::Sniff;
    loadParameters.storedCredentialsPolicy = options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;
    loadParameters.clientCredentialPolicy = clientCredentialPolicy;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect(webFrame ? webFrame->coreFrame() : nullptr);
    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();

    loadParameters.options = options;
    loadParameters.sourceOrigin = &document->securityOrigin();
    loadParameters.topOrigin = &document->topOrigin();
    if (!document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (auto* contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }
    loadParameters.originalRequestHeaders = originalRequestHeaders;
#if ENABLE(APP_BOUND_DOMAINS)
    if (webFrame)
        loadParameters.isNavigatingToAppBoundDomain = webFrame->isTopFrameNavigatingToAppBoundDomain();
#endif
    addParametersShared(webFrame->coreFrame(), loadParameters);

    data.shrink(0);

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad(loadParameters), 0);
    if (!sendResult) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: failed sending synchronous network process message");
        if (page)
            page->diagnosticLoggingClient().logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::synchronousMessageFailedKey(), WebCore::ShouldSample::No);
        response = ResourceResponse();
        error = internalError(request.url());
    } else
        std::tie(error, response, data) = sendResult.takeReply();
}

void WebLoaderStrategy::pageLoadCompleted(Page& page)
{
    if (auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::PageLoadCompleted(WebPage::fromCorePage(page).identifier()), 0);
}

void WebLoaderStrategy::browsingContextRemoved(Frame& frame)
{
    ASSERT(frame.page());
    auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
    if (!networkProcessConnection)
        return;

    auto& page = WebPage::fromCorePage(*frame.page());
    networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::BrowsingContextRemoved(page.webPageProxyIdentifier(), page.identifier(), WebFrame::fromCoreFrame(frame)->frameID()), 0);
}

bool WebLoaderStrategy::usePingLoad() const
{
    return !DeprecatedGlobalSettings::fetchAPIKeepAliveEnabled();
}

void WebLoaderStrategy::startPingLoad(Frame& frame, ResourceRequest& request, const HTTPHeaderMap& originalRequestHeaders, const FetchOptions& options, ContentSecurityPolicyImposition policyCheck, PingLoadCompletionHandler&& completionHandler)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    auto* document = frame.document();
    if (!document || !webFrame) {
        if (completionHandler)
            completionHandler(internalError(request.url()), { });
        return;
    }

    auto* webPage = webFrame->page();
    if (!webPage) {
        if (completionHandler)
            completionHandler(internalError(request.url()), { });
        return;
    }

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = WebCore::ResourceLoaderIdentifier::generate();
    loadParameters.webPageProxyID = webPage->webPageProxyIdentifier();
    loadParameters.webPageID = webPage->identifier();
    loadParameters.webFrameID = webFrame->frameID();
    loadParameters.request = request;
    loadParameters.sourceOrigin = &document->securityOrigin();
    loadParameters.topOrigin = &document->topOrigin();
    loadParameters.parentPID = presentingApplicationPID();
#if HAVE(AUDIT_TOKEN)
    loadParameters.networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken();
#endif
    loadParameters.storedCredentialsPolicy = options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;
    loadParameters.options = options;
    loadParameters.originalRequestHeaders = originalRequestHeaders;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect(&frame);
    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();
    if (policyCheck == ContentSecurityPolicyImposition::DoPolicyCheck && !document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (auto* contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }
    addParametersShared(&frame, loadParameters);
#if ENABLE(APP_BOUND_DOMAINS)
    loadParameters.isNavigatingToAppBoundDomain = webFrame->isTopFrameNavigatingToAppBoundDomain();
#endif

    loadParameters.frameURL = document->url();
#if ENABLE(CONTENT_EXTENSIONS)
    loadParameters.mainDocumentURL = document->topDocument().url();
    // FIXME: Instead of passing userContentControllerIdentifier, we should just pass webPageId to NetworkProcess.
    loadParameters.userContentControllerIdentifier = webPage->userContentControllerIdentifier();
#endif

    if (completionHandler)
        m_pingLoadCompletionHandlers.add(loadParameters.identifier, WTFMove(completionHandler));

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::LoadPing { loadParameters }, 0);
}

void WebLoaderStrategy::didFinishPingLoad(WebCore::ResourceLoaderIdentifier pingLoadIdentifier, ResourceError&& error, ResourceResponse&& response)
{
    if (auto completionHandler = m_pingLoadCompletionHandlers.take(pingLoadIdentifier))
        completionHandler(WTFMove(error), WTFMove(response));
}

void WebLoaderStrategy::preconnectTo(FrameLoader& frameLoader, const URL& url, StoredCredentialsPolicy storedCredentialsPolicy, ShouldPreconnectAsFirstParty shouldPreconnectAsFirstParty, PreconnectCompletionHandler&& completionHandler)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(frameLoader.client());
    if (!webFrameLoaderClient) {
        if (completionHandler)
            completionHandler(internalError(url));
        return;
    }
    auto& webFrame = webFrameLoaderClient->webFrame();
    auto* webPage = webFrame.page();
    if (!webPage) {
        if (completionHandler)
            completionHandler(internalError(url));
        return;
    }

    preconnectTo(ResourceRequest { url }, *webPage, webFrame, storedCredentialsPolicy, shouldPreconnectAsFirstParty, WTFMove(completionHandler));
}

void WebLoaderStrategy::preconnectTo(WebCore::ResourceRequest&& request, WebPage& webPage, WebFrame& webFrame, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, ShouldPreconnectAsFirstParty shouldPreconnectAsFirstParty, PreconnectCompletionHandler&& completionHandler)
{
    if (webPage.corePage() && !webPage.corePage()->allowsLoadFromURL(request.url(), MainFrameMainResource::No)) {
        if (completionHandler)
            completionHandler({ });
        return;
    }

    if (auto* document = webPage.mainFrame()->document()) {
        if (shouldPreconnectAsFirstParty == ShouldPreconnectAsFirstParty::Yes)
            request.setFirstPartyForCookies(request.url());
        else
            request.setFirstPartyForCookies(document->firstPartyForCookies());
        if (auto* loader = document->loader())
            request.setIsAppInitiated(loader->lastNavigationWasAppInitiated());
    }

    NetworkResourceLoadParameters parameters;
    parameters.request = WTFMove(request);
    if (parameters.request.httpUserAgent().isEmpty()) {
        // FIXME: we add user-agent to the preconnect request because otherwise the preconnect
        // gets thrown away by CFNetwork when using an HTTPS proxy (<rdar://problem/59434166>).
        String webPageUserAgent = webPage.userAgent(parameters.request.url());
        if (!webPageUserAgent.isEmpty())
            parameters.request.setHTTPUserAgent(webPageUserAgent);
    }
    parameters.identifier = WebCore::ResourceLoaderIdentifier::generate();
    parameters.webPageProxyID = webPage.webPageProxyIdentifier();
    parameters.webPageID = webPage.identifier();
    parameters.webFrameID = webFrame.frameID();
    parameters.parentPID = presentingApplicationPID();
#if HAVE(AUDIT_TOKEN)
    parameters.networkProcessAuditToken = WebProcess::singleton().ensureNetworkProcessConnection().networkProcessAuditToken();
#endif
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;
    parameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();
    // FIXME: Use the proper destination once all fetch options are passed.
    parameters.options.destination = FetchOptions::Destination::EmptyString;
#if ENABLE(APP_BOUND_DOMAINS)
    parameters.isNavigatingToAppBoundDomain = webFrame.isTopFrameNavigatingToAppBoundDomain();
#endif

    std::optional<WebCore::ResourceLoaderIdentifier> preconnectionIdentifier;
    if (completionHandler) {
        preconnectionIdentifier = parameters.identifier;
        auto addResult = m_preconnectCompletionHandlers.add(*preconnectionIdentifier, WTFMove(completionHandler));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    // FIXME: Use sendWithAsyncReply instead of preconnectionIdentifier
    // FIXME: don't use WebCore::ResourceLoaderIdentifier for a preconnection identifier, too. It should have its own type.
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::PreconnectTo(preconnectionIdentifier, WTFMove(parameters)), 0);
}

void WebLoaderStrategy::didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, ResourceError&& error)
{
    if (auto completionHandler = m_preconnectCompletionHandlers.take(preconnectionIdentifier))
        completionHandler(WTFMove(error));
}

bool WebLoaderStrategy::isOnLine() const
{
    return m_isOnLine;
}

void WebLoaderStrategy::addOnlineStateChangeListener(Function<void(bool)>&& listener)
{
    WebProcess::singleton().ensureNetworkProcessConnection();
    m_onlineStateChangeListeners.append(WTFMove(listener));
}

void WebLoaderStrategy::isResourceLoadFinished(CachedResource& resource, CompletionHandler<void(bool)>&& callback)
{
    if (!resource.loader()) {
        callback(true);
        return;
    }

    auto* networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
    if (!networkProcessConnection) {
        callback(true);
        return;
    }

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::IsResourceLoadFinished(resource.loader()->identifier()), WTFMove(callback), 0);
}

void WebLoaderStrategy::setOnLineState(bool isOnLine)
{
    if (m_isOnLine == isOnLine)
        return;

    m_isOnLine = isOnLine;
    for (auto& listener : m_onlineStateChangeListeners)
        listener(isOnLine);
}

void WebLoaderStrategy::setCaptureExtraNetworkLoadMetricsEnabled(bool enabled)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCaptureExtraNetworkLoadMetricsEnabled(enabled), 0);
}

ResourceResponse WebLoaderStrategy::responseFromResourceLoadIdentifier(ResourceLoaderIdentifier resourceLoadIdentifier)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetNetworkLoadInformationResponse { resourceLoadIdentifier }, 0);
    auto [response] = sendResult.takeReplyOr(ResourceResponse { });
    return response;
}

Vector<NetworkTransactionInformation> WebLoaderStrategy::intermediateLoadInformationFromResourceLoadIdentifier(WebCore::ResourceLoaderIdentifier resourceLoadIdentifier)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetNetworkLoadIntermediateInformation { resourceLoadIdentifier }, 0);
    auto [information] = sendResult.takeReplyOr(Vector<NetworkTransactionInformation> { });
    return information;
}

NetworkLoadMetrics WebLoaderStrategy::networkMetricsFromResourceLoadIdentifier(WebCore::ResourceLoaderIdentifier resourceLoadIdentifier)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TakeNetworkLoadInformationMetrics { resourceLoadIdentifier }, 0);
    auto [networkMetrics] = sendResult.takeReplyOr(NetworkLoadMetrics { });
    return networkMetrics;
}

bool WebLoaderStrategy::shouldPerformSecurityChecks() const
{
    return DeprecatedGlobalSettings::restrictedHTTPResponseAccess();
}

bool WebLoaderStrategy::havePerformedSecurityChecks(const ResourceResponse& response) const
{
    if (!shouldPerformSecurityChecks())
        return false;
    switch (response.source()) {
    case ResourceResponse::Source::DOMCache:
    case ResourceResponse::Source::ApplicationCache:
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
    case ResourceResponse::Source::ServiceWorker:
    case ResourceResponse::Source::InspectorOverride:
        return false;
    case ResourceResponse::Source::DiskCache:
    case ResourceResponse::Source::DiskCacheAfterValidation:
    case ResourceResponse::Source::Network:
    case ResourceResponse::Source::Unknown:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void WebLoaderStrategy::setResourceLoadSchedulingMode(WebCore::Page& page, WebCore::LoadSchedulingMode mode)
{
    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.send(Messages::NetworkConnectionToWebProcess::SetResourceLoadSchedulingMode(WebPage::fromCorePage(page).identifier(), mode), 0);
}

void WebLoaderStrategy::prioritizeResourceLoads(const Vector<WebCore::SubresourceLoader*>& resources)
{
    auto identifiers = resources.map([](auto* loader) -> WebCore::ResourceLoaderIdentifier {
        return loader->identifier();
    });

    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.send(Messages::NetworkConnectionToWebProcess::PrioritizeResourceLoads(identifiers), 0);
}

} // namespace WebKit

#undef WEBLOADERSTRATEGY_RELEASE_LOG_BASIC
#undef WEBLOADERSTRATEGY_RELEASE_LOG_ERROR_BASIC
