/*
 * Copyright (C) 2012, 2015, 2018 Apple Inc. All rights reserved.
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

#include "DataReference.h"
#include "HangDetectionDisabler.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "SessionTracker.h"
#include "SharedBufferDataReference.h"
#include "WebCompiledContentRuleList.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebResourceLoader.h"
#include "WebServiceWorkerProvider.h"
#include "WebURLSchemeHandlerProxy.h"
#include "WebURLSchemeTaskProxy.h"
#include <WebCore/ApplicationCacheHost.h>
#include <WebCore/CachedResource.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
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

#define RELEASE_LOG_IF_ALLOWED(permissionChecker, fmt, ...) RELEASE_LOG_IF(permissionChecker.isAlwaysOnLoggingAllowed(), Network, "%p - WebLoaderStrategy::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(permissionChecker, fmt, ...) RELEASE_LOG_ERROR_IF(permissionChecker.isAlwaysOnLoggingAllowed(), Network, "%p - WebLoaderStrategy::" fmt, this, ##__VA_ARGS__)

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
    SubresourceLoader::create(frame, resource, WTFMove(request), options, [this, completionHandler = WTFMove(completionHandler), resource = CachedResourceHandle<CachedResource>(&resource), frame = makeRef(frame)] (RefPtr<SubresourceLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(*loader, resource.get(), frame->document()->referrerPolicy() == ReferrerPolicy::NoReferrerWhenDowngrade);
        else
            RELEASE_LOG_IF_ALLOWED(frame.get(), "loadResource: Unable to create SubresourceLoader (frame = %p", &frame);
        completionHandler(WTFMove(loader));
    });
}

void WebLoaderStrategy::schedulePluginStreamLoad(Frame& frame, NetscapePlugInStreamLoaderClient& client, ResourceRequest&& request, CompletionHandler<void(RefPtr<NetscapePlugInStreamLoader>&&)>&& completionHandler)
{
    NetscapePlugInStreamLoader::create(frame, client, WTFMove(request), [this, completionHandler = WTFMove(completionHandler), frame = makeRef(frame)] (RefPtr<NetscapePlugInStreamLoader>&& loader) mutable {
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
    case CachedResource::Type::CSSStyleSheet:
    case CachedResource::Type::Script:
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
    case CachedResource::Type::FontResource:
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
        return Seconds::infinity();
    case CachedResource::Type::ImageResource:
        return 500_ms;
    case CachedResource::Type::MediaResource:
        return 50_ms;
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::LinkPrefetch:
#if ENABLE(VIDEO_TRACK)
    case CachedResource::Type::TextTrackResource:
#endif
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
    ResourceLoadIdentifier identifier = resourceLoader.identifier();
    ASSERT(identifier);

    auto& frameLoaderClient = resourceLoader.frameLoader()->client();

    WebResourceLoader::TrackingParameters trackingParameters;
    trackingParameters.pageID = frameLoaderClient.pageID().valueOr(0);
    trackingParameters.frameID = frameLoaderClient.frameID().valueOr(0);
    trackingParameters.resourceID = identifier;
    auto sessionID = frameLoaderClient.sessionID();

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    // If the DocumentLoader schedules this as an archive resource load,
    // then we should remember the ResourceLoader in our records but not schedule it in the NetworkProcess.
    if (resourceLoader.documentLoader()->scheduleArchiveLoad(resourceLoader, resourceLoader.request())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as an archive resource.", resourceLoader.url().string().utf8().data());
        RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL will be handled as an archive resource (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader, trackingParameters));
        return;
    }
#endif

    if (resourceLoader.documentLoader()->applicationCacheHost().maybeLoadResource(resourceLoader, resourceLoader.request(), resourceLoader.request().url())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded from application cache.", resourceLoader.url().string().utf8().data());
        RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL will be loaded from application cache (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader, trackingParameters));
        return;
    }

    if (resourceLoader.request().url().protocolIsData()) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded as data.", resourceLoader.url().string().utf8().data());
        RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL will be loaded as data (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
        startLocalLoad(resourceLoader);
        return;
    }

#if USE(QUICK_LOOK)
    if (isQuickLookPreviewURL(resourceLoader.request().url())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a QuickLook resource.", resourceLoader.url().string().utf8().data());
        RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL will be handled as a QuickLook resource (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if USE(SOUP)
    // For apps that call g_resource_load in a web extension.
    // https://blogs.gnome.org/alexl/2012/01/26/resources-in-glib/
    if (resourceLoader.request().url().protocolIs("resource")) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a GResource.", resourceLoader.url().string().utf8().data());
        RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL will be handled as a GResource (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if ENABLE(SERVICE_WORKER)
    WebServiceWorkerProvider::singleton().handleFetch(resourceLoader, resource, sessionID, shouldClearReferrerOnHTTPSToHTTPRedirect, [this, trackingParameters, identifier, sessionID, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime = maximumBufferingTime(resource), resourceLoader = makeRef(resourceLoader)] (ServiceWorkerClientFetch::Result result) mutable {
        if (result != ServiceWorkerClientFetch::Result::Unhandled) {
            LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be scheduled through ServiceWorker handle fetch algorithm", resourceLoader->url().string().latin1().data());
            RELEASE_LOG_IF_ALLOWED(resourceLoader.get(), "scheduleLoad: URL will be scheduled through ServiceWorker handle fetch algorithm (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader->frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
            return;
        }
        if (resourceLoader->options().serviceWorkersMode == ServiceWorkersMode::Only) {
            RELEASE_LOG_ERROR_IF_ALLOWED(resourceLoader.get(), "scheduleLoad: unable to schedule URL through ServiceWorker handle fetch algorithm (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader->frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
            callOnMainThread([resourceLoader = WTFMove(resourceLoader)] {
                auto error = internalError(resourceLoader->request().url());
                error.setType(ResourceError::Type::Cancellation);
                resourceLoader->didFail(error);
            });
            return;
        }

        if (!WebProcess::singleton().webLoaderStrategy().tryLoadingUsingURLSchemeHandler(resourceLoader))
            WebProcess::singleton().webLoaderStrategy().scheduleLoadFromNetworkProcess(resourceLoader.get(), resourceLoader->request(), trackingParameters, sessionID, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime);
        else
            RELEASE_LOG_IF_ALLOWED(resourceLoader.get(), "scheduleLoad: URL not handled by any handlers (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader->frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
    });
#else
    if (!tryLoadingUsingURLSchemeHandler(resourceLoader))
        scheduleLoadFromNetworkProcess(resourceLoader, resourceLoader.request(), trackingParameters, sessionID, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime(resource));
    else
        RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL not handled by any handlers (frame = %p, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), trackingParameters.pageID, trackingParameters.frameID, identifier);
#endif
}

bool WebLoaderStrategy::tryLoadingUsingURLSchemeHandler(ResourceLoader& resourceLoader)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(resourceLoader.frameLoader()->client());
    auto* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : nullptr;
    auto* webPage = webFrame ? webFrame->page() : nullptr;
    if (webPage) {
        if (auto* handler = webPage->urlSchemeHandlerForScheme(resourceLoader.request().url().protocol().toStringWithoutCopying())) {
            LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, URL '%s' will be handled by a UIProcess URL scheme handler.", resourceLoader.url().string().utf8().data());
            RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: URL will be handled by a UIProcess URL scheme handler (frame = %p, resourceID = %lu)", resourceLoader.frame(), resourceLoader.identifier());

            handler->startNewTask(resourceLoader);
            return true;
        }
    }
    return false;
}

void WebLoaderStrategy::scheduleLoadFromNetworkProcess(ResourceLoader& resourceLoader, const ResourceRequest& request, const WebResourceLoader::TrackingParameters& trackingParameters, PAL::SessionID sessionID, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Seconds maximumBufferingTime)
{
    ResourceLoadIdentifier identifier = resourceLoader.identifier();
    ASSERT(identifier);

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be scheduled with the NetworkProcess with priority %d", resourceLoader.url().string().latin1().data(), static_cast<int>(resourceLoader.request().priority()));

    ContentSniffingPolicy contentSniffingPolicy = resourceLoader.shouldSniffContent() ? ContentSniffingPolicy::SniffContent : ContentSniffingPolicy::DoNotSniffContent;
    ContentEncodingSniffingPolicy contentEncodingSniffingPolicy = resourceLoader.shouldSniffContentEncoding() ? ContentEncodingSniffingPolicy::Sniff : ContentEncodingSniffingPolicy::DoNotSniff;
    StoredCredentialsPolicy storedCredentialsPolicy = resourceLoader.shouldUseCredentialStorage() ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = identifier;
    loadParameters.webPageID = trackingParameters.pageID;
    loadParameters.webFrameID = trackingParameters.frameID;
    loadParameters.parentPID = presentingApplicationPID();
    loadParameters.sessionID = sessionID;
    loadParameters.request = request;
    loadParameters.contentSniffingPolicy = contentSniffingPolicy;
    loadParameters.contentEncodingSniffingPolicy = contentEncodingSniffingPolicy;
    loadParameters.storedCredentialsPolicy = storedCredentialsPolicy;
    // If there is no WebFrame then this resource cannot be authenticated with the client.
    loadParameters.clientCredentialPolicy = (loadParameters.webFrameID && loadParameters.webPageID && resourceLoader.isAllowedToAskUserForCredentials()) ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect;
    loadParameters.defersLoading = resourceLoader.defersLoading();
    loadParameters.needsCertificateInfo = resourceLoader.shouldIncludeCertificateInfo();
    loadParameters.maximumBufferingTime = maximumBufferingTime;
    loadParameters.options = resourceLoader.options();
    loadParameters.preflightPolicy = resourceLoader.options().preflightPolicy;
    loadParameters.isHTTPSUpgradeEnabled = resourceLoader.frame() ? resourceLoader.frame()->settings().HTTPSUpgradeEnabled() : false;

    auto* document = resourceLoader.frame() ? resourceLoader.frame()->document() : nullptr;
    if (resourceLoader.options().cspResponseHeaders)
        loadParameters.cspResponseHeaders = resourceLoader.options().cspResponseHeaders;
    else if (document && !document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (auto* contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }

    if (resourceLoader.isSubresourceLoader()) {
        if (auto* headers = static_cast<SubresourceLoader&>(resourceLoader).originalHeaders())
            loadParameters.originalRequestHeaders = *headers;
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (document) {
        loadParameters.mainDocumentURL = document->topDocument().url();
        // FIXME: Instead of passing userContentControllerIdentifier, the NetworkProcess should be able to get it using webPageId.
        auto* webFrameLoaderClient = toWebFrameLoaderClient(resourceLoader.frame()->loader().client());
        auto* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : nullptr;
        auto* webPage = webFrame ? webFrame->page() : nullptr;
        if (webPage)
            loadParameters.userContentControllerIdentifier = webPage->userContentControllerIdentifier();
    }
#endif

    // FIXME: All loaders should provide their origin if navigation mode is cors/no-cors/same-origin.
    // As a temporary approach, we use the document origin if available or the HTTP Origin header otherwise.
    if (resourceLoader.isSubresourceLoader())
        loadParameters.sourceOrigin = static_cast<SubresourceLoader&>(resourceLoader).origin();

    if (!loadParameters.sourceOrigin && document)
        loadParameters.sourceOrigin = &document->securityOrigin();
    if (!loadParameters.sourceOrigin) {
        auto origin = request.httpOrigin();
        if (!origin.isNull())
            loadParameters.sourceOrigin = SecurityOrigin::createFromString(origin);
    }

    if (loadParameters.options.mode != FetchOptions::Mode::Navigate) {
        ASSERT(loadParameters.sourceOrigin);
        if (!loadParameters.sourceOrigin) {
            RELEASE_LOG_ERROR_IF_ALLOWED(resourceLoader, "scheduleLoad: no sourceOrigin (frame = %p, priority = %d, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), static_cast<int>(resourceLoader.request().priority()), loadParameters.webPageID, loadParameters.webFrameID, loadParameters.identifier);
            scheduleInternallyFailedLoad(resourceLoader);
            return;
        }
    }

    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();

    loadParameters.isMainFrameNavigation = resourceLoader.frame() && resourceLoader.frame()->isMainFrame() && resourceLoader.options().mode == FetchOptions::Mode::Navigate;

    loadParameters.shouldEnableCrossOriginResourcePolicy = RuntimeEnabledFeatures::sharedFeatures().crossOriginResourcePolicyEnabled() && !loadParameters.isMainFrameNavigation;

    if (resourceLoader.options().mode == FetchOptions::Mode::Navigate) {
        Vector<RefPtr<SecurityOrigin>> frameAncestorOrigins;
        for (auto* frame = resourceLoader.frame()->tree().parent(); frame; frame = frame->tree().parent())
            frameAncestorOrigins.append(makeRefPtr(frame->document()->securityOrigin()));
        loadParameters.frameAncestorOrigins = WTFMove(frameAncestorOrigins);
    }

    ASSERT((loadParameters.webPageID && loadParameters.webFrameID) || loadParameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials);

    RELEASE_LOG_IF_ALLOWED(resourceLoader, "scheduleLoad: Resource is being scheduled with the NetworkProcess (frame = %p, priority = %d, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), static_cast<int>(resourceLoader.request().priority()), loadParameters.webPageID, loadParameters.webFrameID, loadParameters.identifier);
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad(loadParameters), 0)) {
        RELEASE_LOG_ERROR_IF_ALLOWED(resourceLoader, "scheduleLoad: Unable to schedule resource with the NetworkProcess (frame = %p, priority = %d, pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", resourceLoader.frame(), static_cast<int>(resourceLoader.request().priority()), loadParameters.webPageID, loadParameters.webFrameID, loadParameters.identifier);
        // We probably failed to schedule this load with the NetworkProcess because it had crashed.
        // This load will never succeed so we will schedule it to fail asynchronously.
        scheduleInternallyFailedLoad(resourceLoader);
        return;
    }

    m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader, trackingParameters));
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

    if (auto task = m_urlSchemeTasks.take(resourceLoader->identifier())) {
        ASSERT(!m_internallyFailedResourceLoaders.contains(resourceLoader));
        task->stopLoading();
        return;
    }

    if (m_internallyFailedResourceLoaders.contains(resourceLoader)) {
        m_internallyFailedResourceLoaders.remove(resourceLoader);
        return;
    }
    
    ResourceLoadIdentifier identifier = resourceLoader->identifier();
    if (!identifier) {
        LOG_ERROR("WebLoaderStrategy removing a ResourceLoader that has no identifier.");
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (WebServiceWorkerProvider::singleton().cancelFetch(makeObjectIdentifier<FetchIdentifierType>(identifier)))
        return;
#endif

    RefPtr<WebResourceLoader> loader = m_webResourceLoaders.take(identifier);
    // Loader may not be registered if we created it, but haven't scheduled yet (a bundle client can decide to cancel such request via willSendRequest).
    if (!loader)
        return;

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveLoadIdentifier(identifier), 0);

    // It's possible that this WebResourceLoader might be just about to message back to the NetworkProcess (e.g. ContinueWillSendRequest)
    // but there's no point in doing so anymore.
    loader->detachFromCoreLoader();
}

void WebLoaderStrategy::setDefersLoading(ResourceLoader& resourceLoader, bool defers)
{
    ResourceLoadIdentifier identifier = resourceLoader.identifier();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetDefersLoading(identifier, defers), 0);
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
    RELEASE_LOG_ERROR(Network, "WebLoaderStrategy::networkProcessCrashed: failing all pending resource loaders");

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

Optional<WebLoaderStrategy::SyncLoadResult> WebLoaderStrategy::tryLoadingSynchronouslyUsingURLSchemeHandler(FrameLoader& frameLoader, ResourceLoadIdentifier identifier, const ResourceRequest& request)
{
    auto* webFrameLoaderClient = toWebFrameLoaderClient(frameLoader.client());
    auto* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : nullptr;
    auto* webPage = webFrame ? webFrame->page() : nullptr;
    if (!webPage)
        return WTF::nullopt;

    auto* handler = webPage->urlSchemeHandlerForScheme(request.url().protocol().toStringWithoutCopying());
    if (!handler)
        return WTF::nullopt;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, sync load to URL '%s' will be handled by a UIProcess URL scheme handler.", request.url().string().utf8().data());

    SyncLoadResult result;
    handler->loadSynchronously(identifier, request, result.response, result.error, result.data);

    return WTFMove(result);
}

void WebLoaderStrategy::loadResourceSynchronously(FrameLoader& frameLoader, unsigned long resourceLoadIdentifier, const ResourceRequest& request, ClientCredentialPolicy clientCredentialPolicy,  const FetchOptions& options, const HTTPHeaderMap& originalRequestHeaders, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    WebFrameLoaderClient* webFrameLoaderClient = toWebFrameLoaderClient(frameLoader.client());
    WebFrame* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : nullptr;
    WebPage* webPage = webFrame ? webFrame->page() : nullptr;

    auto pageID = webPage ? webPage->pageID() : 0;
    auto frameID = webFrame ? webFrame->frameID() : 0;
    auto sessionID = webPage ? webPage->sessionID() : PAL::SessionID::defaultSessionID();

    auto* document = frameLoader.frame().document();
    if (!document) {
        RELEASE_LOG_ERROR_IF_ALLOWED(sessionID, "loadResourceSynchronously: no document (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %lu)", pageID, frameID, resourceLoadIdentifier);
        error = internalError(request.url());
        return;
    }

    if (auto syncLoadResult = tryLoadingSynchronouslyUsingURLSchemeHandler(frameLoader, resourceLoadIdentifier, request)) {
        RELEASE_LOG_ERROR_IF_ALLOWED(sessionID, "loadResourceSynchronously: failed calling tryLoadingSynchronouslyUsingURLSchemeHandler (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %lu, error = %d)", pageID, frameID, resourceLoadIdentifier, syncLoadResult->error.errorCode());
        error = WTFMove(syncLoadResult->error);
        response = WTFMove(syncLoadResult->response);
        data = WTFMove(syncLoadResult->data);
        return;
    }

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = resourceLoadIdentifier;
    loadParameters.webPageID = pageID;
    loadParameters.webFrameID = frameID;
    loadParameters.parentPID = presentingApplicationPID();
    loadParameters.sessionID = sessionID;
    loadParameters.request = request;
    loadParameters.contentSniffingPolicy = ContentSniffingPolicy::SniffContent;
    loadParameters.contentEncodingSniffingPolicy = ContentEncodingSniffingPolicy::Sniff;
    loadParameters.storedCredentialsPolicy = options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;
    loadParameters.clientCredentialPolicy = clientCredentialPolicy;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect(webFrame ? webFrame->coreFrame() : nullptr);
    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();

    loadParameters.options = options;
    loadParameters.sourceOrigin = &document->securityOrigin();
    if (!document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (auto* contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }
    loadParameters.originalRequestHeaders = originalRequestHeaders;

    data.shrink(0);

    HangDetectionDisabler hangDetectionDisabler;

    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad(loadParameters), Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::Reply(error, response, data), 0)) {
        RELEASE_LOG_ERROR_IF_ALLOWED(sessionID, "loadResourceSynchronously: failed sending synchronous network process message (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %lu)", pageID, frameID, resourceLoadIdentifier);
        if (auto* page = webPage ? webPage->corePage() : nullptr)
            page->diagnosticLoggingClient().logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::synchronousMessageFailedKey(), WebCore::ShouldSample::No);
        response = ResourceResponse();
        error = internalError(request.url());
    }
}

void WebLoaderStrategy::pageLoadCompleted(uint64_t webPageID)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::PageLoadCompleted(webPageID), 0);
}

static uint64_t generateLoadIdentifier()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

void WebLoaderStrategy::startPingLoad(Frame& frame, ResourceRequest& request, const HTTPHeaderMap& originalRequestHeaders, const FetchOptions& options, PingLoadCompletionHandler&& completionHandler)
{
    auto* document = frame.document();
    if (!document) {
        if (completionHandler)
            completionHandler(internalError(request.url()), { });
        return;
    }

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = generateLoadIdentifier();
    loadParameters.request = request;
    loadParameters.sourceOrigin = &document->securityOrigin();
    loadParameters.parentPID = presentingApplicationPID();
    loadParameters.sessionID = frame.page() ? frame.page()->sessionID() : PAL::SessionID::defaultSessionID();
    loadParameters.storedCredentialsPolicy = options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;
    loadParameters.options = options;
    loadParameters.originalRequestHeaders = originalRequestHeaders;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect(&frame);
    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();
    if (!document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (auto * contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }

#if ENABLE(CONTENT_EXTENSIONS)
    loadParameters.mainDocumentURL = document->topDocument().url();
    // FIXME: Instead of passing userContentControllerIdentifier, we should just pass webPageId to NetworkProcess.
    WebFrameLoaderClient* webFrameLoaderClient = toWebFrameLoaderClient(frame.loader().client());
    WebFrame* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : nullptr;
    WebPage* webPage = webFrame ? webFrame->page() : nullptr;
    if (webPage)
        loadParameters.userContentControllerIdentifier = webPage->userContentControllerIdentifier();
#endif

    if (completionHandler)
        m_pingLoadCompletionHandlers.add(loadParameters.identifier, WTFMove(completionHandler));

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::LoadPing { loadParameters }, 0);
}

void WebLoaderStrategy::didFinishPingLoad(uint64_t pingLoadIdentifier, ResourceError&& error, ResourceResponse&& response)
{
    if (auto completionHandler = m_pingLoadCompletionHandlers.take(pingLoadIdentifier))
        completionHandler(WTFMove(error), WTFMove(response));
}

void WebLoaderStrategy::preconnectTo(FrameLoader& frameLoader, const URL& url, StoredCredentialsPolicy storedCredentialsPolicy, PreconnectCompletionHandler&& completionHandler)
{
    uint64_t preconnectionIdentifier = generateLoadIdentifier();
    auto addResult = m_preconnectCompletionHandlers.add(preconnectionIdentifier, WTFMove(completionHandler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    auto* webFrameLoaderClient = toWebFrameLoaderClient(frameLoader.client());
    if (!webFrameLoaderClient) {
        completionHandler(internalError(url));
        return;
    }
    auto* webFrame = webFrameLoaderClient->webFrame();
    if (!webFrame) {
        completionHandler(internalError(url));
        return;
    }
    auto* webPage = webFrame->page();
    if (!webPage) {
        completionHandler(internalError(url));
        return;
    }

    NetworkResourceLoadParameters parameters;
    parameters.request = ResourceRequest { url };
    parameters.webPageID = webPage ? webPage->pageID() : 0;
    parameters.webFrameID = webFrame ? webFrame->frameID() : 0;
    parameters.parentPID = presentingApplicationPID();
    parameters.sessionID = webPage ? webPage->sessionID() : PAL::SessionID::defaultSessionID();
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;
    parameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();
    // FIXME: Use the proper destination once all fetch options are passed.
    parameters.options.destination = FetchOptions::Destination::EmptyString;

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::PreconnectTo(preconnectionIdentifier, WTFMove(parameters)), 0);
}

void WebLoaderStrategy::didFinishPreconnection(uint64_t preconnectionIdentifier, ResourceError&& error)
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

ResourceResponse WebLoaderStrategy::responseFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier)
{
    ResourceResponse response;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetNetworkLoadInformationResponse { resourceLoadIdentifier }, Messages::NetworkConnectionToWebProcess::GetNetworkLoadInformationResponse::Reply { response }, 0);
    return response;
}

Vector<NetworkTransactionInformation> WebLoaderStrategy::intermediateLoadInformationFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier)
{
    Vector<NetworkTransactionInformation> information;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetNetworkLoadIntermediateInformation { resourceLoadIdentifier }, Messages::NetworkConnectionToWebProcess::GetNetworkLoadIntermediateInformation::Reply { information }, 0);
    return information;
}

NetworkLoadMetrics WebLoaderStrategy::networkMetricsFromResourceLoadIdentifier(uint64_t resourceLoadIdentifier)
{
    NetworkLoadMetrics networkMetrics;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TakeNetworkLoadInformationMetrics { resourceLoadIdentifier }, Messages::NetworkConnectionToWebProcess::TakeNetworkLoadInformationMetrics::Reply { networkMetrics }, 0);
    return networkMetrics;
}

bool WebLoaderStrategy::shouldPerformSecurityChecks() const
{
    return RuntimeEnabledFeatures::sharedFeatures().restrictedHTTPResponseAccess();
}

bool WebLoaderStrategy::havePerformedSecurityChecks(const ResourceResponse& response) const
{
    if (!shouldPerformSecurityChecks())
        return false;
    switch (response.source()) {
    case ResourceResponse::Source::ApplicationCache:
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
    case ResourceResponse::Source::ServiceWorker:
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

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
