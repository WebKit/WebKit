/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#include "NetworkResourceLoader.h"

#include "ArgumentCoders.h"
#include "EarlyHintsResourceLoader.h"
#include "FormDataReference.h"
#include "LoadedWebArchive.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NetworkCache.h"
#include "NetworkCacheSpeculativeLoadManager.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoad.h"
#include "NetworkLoadChecker.h"
#include "NetworkOriginAccessPatterns.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSchemeRegistry.h"
#include "NetworkSession.h"
#include "NetworkStorageManager.h"
#include "PrivateRelayed.h"
#include "ResourceLoadInfo.h"
#include "ServiceWorkerFetchTask.h"
#include "SharedBufferReference.h"
#include "WebErrors.h"
#include "WebLoaderStrategy.h"
#include "WebPageMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWServerConnection.h"
#include "WebSharedWorkerServer.h"
#include "WebsiteDataStore.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/COEPInheritenceViolationReportBody.h>
#include <WebCore/CORPViolationReportBody.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/HTTPStatusCodes.h>
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LinkHeader.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ReportingScope.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityPolicy.h>
#include <WebCore/ShareableResource.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/ViolationReportType.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Expected.h>
#include <wtf/RunLoop.h>
#include <wtf/text/MakeString.h>

#if USE(QUICK_LOOK)
#include <WebCore/PreviewConverter.h>
#endif

#if ENABLE(CONTENT_FILTERING)
#include <WebCore/ContentFilter.h>
#include <WebCore/ContentFilterUnblockHandler.h>
#endif

#if HAVE(BROWSERENGINEKIT_WEBCONTENTFILTER)
#include "WebParentalControlsURLFilter.h"
#endif

#define LOADER_RELEASE_LOG_WITH_THIS(thisPtr, fmt, ...) RELEASE_LOG(Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, WTF::getPtr(thisPtr), thisPtr->webPageProxyID().toUInt64(), thisPtr->pageID().toUInt64(), thisPtr->frameID().toUInt64(), thisPtr->coreIdentifier().toUInt64(), thisPtr->isMainResource(), static_cast<unsigned>(thisPtr->m_parameters.options.destination), thisPtr->isSynchronous(), ##__VA_ARGS__)
#define LOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, this, webPageProxyID().toUInt64(), pageID().toUInt64(), frameID().toUInt64(), coreIdentifier().toUInt64(), isMainResource(), static_cast<unsigned>(m_parameters.options.destination), isSynchronous(), ##__VA_ARGS__)
#define LOADER_RELEASE_LOG_DEBUG(fmt, ...) RELEASE_LOG_DEBUG(Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, this, webPageProxyID().toUInt64(), pageID().toUInt64(), frameID().toUInt64(), coreIdentifier().toUInt64(), isMainResource(), static_cast<unsigned>(m_parameters.options.destination), isSynchronous(), ##__VA_ARGS__)
#define LOADER_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, this, webPageProxyID().toUInt64(), pageID().toUInt64(), frameID().toUInt64(), coreIdentifier().toUInt64(), isMainResource(), static_cast<unsigned>(m_parameters.options.destination), isSynchronous(), ##__VA_ARGS__)
#define LOADER_RELEASE_LOG_FAULT(fmt, ...) RELEASE_LOG_FAULT(Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, this, webPageProxyID().toUInt64(), pageID().toUInt64(), frameID().toUInt64(), coreIdentifier().toUInt64(), isMainResource(), static_cast<unsigned>(m_parameters.options.destination), isSynchronous(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

struct NetworkResourceLoader::SynchronousLoadData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(NetworkResourceLoader);

    SynchronousLoadData(CompletionHandler<void(const ResourceError&, const ResourceResponse, Vector<uint8_t>&&)>&& reply)
        : delayedReply(WTFMove(reply))
    {
        ASSERT(delayedReply);
    }
    ResourceRequest currentRequest;
    CompletionHandler<void(const ResourceError&, const ResourceResponse, Vector<uint8_t>&&)> delayedReply;
    ResourceResponse response;
    ResourceError error;
};

static void sendReplyToSynchronousRequest(NetworkResourceLoader::SynchronousLoadData& data, const FragmentedSharedBuffer* buffer, const NetworkLoadMetrics& metrics)
{
    ASSERT(data.delayedReply);
    ASSERT(!data.response.isNull() || !data.error.isNull());

    if (!data.delayedReply)
        return;

    Vector<uint8_t> responseBuffer;
    if (buffer && buffer->size())
        responseBuffer.append(buffer->makeContiguous()->span());

    data.response.setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics>::create(metrics));

    data.delayedReply(data.error, data.response, WTFMove(responseBuffer));
    data.delayedReply = nullptr;
}

NetworkResourceLoader::NetworkResourceLoader(NetworkResourceLoadParameters&& parameters, NetworkConnectionToWebProcess& connection, CompletionHandler<void(const ResourceError&, const ResourceResponse, Vector<uint8_t>&&)>&& synchronousReply)
    : m_parameters { WTFMove(parameters) }
    , m_connection { connection }
    , m_fileReferences(connection.resolveBlobReferences(m_parameters))
    , m_isAllowedToAskUserForCredentials { m_parameters.clientCredentialPolicy == ClientCredentialPolicy::MayAskClientForCredentials }
    , m_bufferingTimer { *this, &NetworkResourceLoader::bufferingTimerFired }
    , m_shouldCaptureExtraNetworkLoadMetrics(m_connection->captureExtraNetworkLoadMetricsEnabled())
    , m_resourceLoadID { NetworkResourceLoadIdentifier::generate() }
{
    ASSERT(RunLoop::isMain());

    if (CheckedPtr session = connection.networkProcess().networkSession(sessionID()))
        m_cache = session->cache();

    if (synchronousReply || m_parameters.shouldRestrictHTTPResponseAccess || m_parameters.options.keepAlive) {
        NetworkLoadChecker::LoadType requestLoadType = isMainFrameLoad() ? NetworkLoadChecker::LoadType::MainFrame : NetworkLoadChecker::LoadType::Other;
        m_networkLoadChecker = NetworkLoadChecker::create(Ref { connection.networkProcess() }.get(), this,  &connection.schemeRegistry(), FetchOptions { m_parameters.options },
            sessionID(), webPageProxyID(), HTTPHeaderMap { m_parameters.originalRequestHeaders }, URL { m_parameters.request.url() },
            URL { m_parameters.documentURL }, m_parameters.sourceOrigin.copyRef(), m_parameters.topOrigin.copyRef(), m_parameters.parentOrigin(),
            m_parameters.preflightPolicy, originalRequest().httpReferrer(), m_parameters.allowPrivacyProxy, m_parameters.advancedPrivacyProtections,
            shouldCaptureExtraNetworkLoadMetrics(), requestLoadType);

        RefPtr networkLoadChecker = m_networkLoadChecker;
        if (m_parameters.cspResponseHeaders)
            networkLoadChecker->setCSPResponseHeaders(ContentSecurityPolicyResponseHeaders { m_parameters.cspResponseHeaders.value() });
        networkLoadChecker->setParentCrossOriginEmbedderPolicy(m_parameters.parentCrossOriginEmbedderPolicy);
        networkLoadChecker->setCrossOriginEmbedderPolicy(m_parameters.crossOriginEmbedderPolicy);
#if ENABLE(CONTENT_EXTENSIONS)
        networkLoadChecker->setContentExtensionController(URL { m_parameters.mainDocumentURL }, URL { m_parameters.frameURL }, m_parameters.userContentControllerIdentifier);
#endif
    }
    if (synchronousReply)
        m_synchronousLoadData = makeUnique<SynchronousLoadData>(WTFMove(synchronousReply));
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_networkLoad);
    ASSERT(!isSynchronous() || !m_synchronousLoadData->delayedReply);
    ASSERT(m_fileReferences.isEmpty());
    if (m_responseCompletionHandler)
        m_responseCompletionHandler(PolicyAction::Ignore);
}

Ref<NetworkConnectionToWebProcess> NetworkResourceLoader::protectedConnectionToWebProcess() const
{
    return connectionToWebProcess();
}

RefPtr<NetworkCache::Cache> NetworkResourceLoader::protectedCache() const
{
    return m_cache;
}

RefPtr<ServiceWorkerFetchTask> NetworkResourceLoader::protectedServiceWorkerFetchTask() const
{
    return m_serviceWorkerFetchTask;
}

bool NetworkResourceLoader::canUseCache(const ResourceRequest& request) const
{
    if (!m_cache)
        return false;
    ASSERT(!sessionID().isEphemeral());

    if (!request.url().protocolIsInHTTPFamily())
        return false;
    if (originalRequest().cachePolicy() == WebCore::ResourceRequestCachePolicy::DoNotUseAnyCache)
        return false;

    return true;
}

bool NetworkResourceLoader::canUseCachedRedirect(const ResourceRequest& request) const
{
    if (!canUseCache(request) || m_cacheEntryForMaxAgeCapValidation)
        return false;
    // Limit cached redirects to avoid cycles and other trouble.
    // Networking layer follows over 30 redirects but caching that many seems unnecessary.
    static const unsigned maximumCachedRedirectCount { 5 };
    if (m_redirectCount > maximumCachedRedirectCount)
        return false;

    return true;
}

bool NetworkResourceLoader::isSynchronous() const
{
    return !!m_synchronousLoadData;
}

void NetworkResourceLoader::start()
{
    startRequest(originalRequest());
}

void NetworkResourceLoader::startRequest(const ResourceRequest& newRequest)
{
    ASSERT(RunLoop::isMain());
    RefPtr networkLoadChecker = m_networkLoadChecker;
    LOADER_RELEASE_LOG("startRequest: hasNetworkLoadChecker=%d", !!networkLoadChecker);

    m_networkActivityTracker = protectedConnectionToWebProcess()->startTrackingResourceLoad(pageID(), coreIdentifier(), isMainFrameLoad());

    ASSERT(!m_wasStarted);
    m_wasStarted = true;

    if (networkLoadChecker) {
        networkLoadChecker->check(ResourceRequest { newRequest }, this, [weakThis = WeakPtr { *this }] (auto&& result) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            WTF::switchOn(result,
                [protectedThis] (ResourceError& error) {
                    LOADER_RELEASE_LOG_WITH_THIS(protectedThis, "start: NetworkLoadChecker::check returned an error (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d, isCancellation=%d)", error.domain().utf8().data(), error.errorCode(), error.isCancellation());
                    if (!error.isCancellation())
                        protectedThis->didFailLoading(error);
                },
                [protectedThis] (NetworkLoadChecker::RedirectionTriplet& triplet) {
                    LOADER_RELEASE_LOG_WITH_THIS(protectedThis, "start: NetworkLoadChecker::check returned a synthetic redirect");
                    protectedThis->m_isWaitingContinueWillSendRequestForCachedRedirect = true;
                    protectedThis->willSendRedirectedRequest(WTFMove(triplet.request), WTFMove(triplet.redirectRequest), WTFMove(triplet.redirectResponse), [](auto) { });
                },
                [protectedThis] (ResourceRequest& request) {
                    LOADER_RELEASE_LOG_WITH_THIS(protectedThis, "start: NetworkLoadChecker::check is done");
                    if (protectedThis->canUseCache(request)) {
                        protectedThis->retrieveCacheEntry(request);
                        return;
                    }

                    protectedThis->startNetworkLoad(WTFMove(request), FirstLoad::Yes);
                }
            );
        });
        return;
    }
    // FIXME: Remove that code path once m_networkLoadChecker is used for all network loads.
    if (canUseCache(newRequest)) {
        retrieveCacheEntry(originalRequest());
        return;
    }

    startNetworkLoad(ResourceRequest { newRequest }, FirstLoad::Yes);
}

#if ENABLE(CONTENT_FILTERING)
static void setSharedParentalControlsURLFilterIfNecessary()
{
#if HAVE(BROWSERENGINEKIT_WEBCONTENTFILTER) && !HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    ASSERT(isMainRunLoop());
    static bool initialized = false;
    if (!initialized) {
        WebCore::ParentalControlsURLFilter::setGlobalFilter(WebParentalControlsURLFilter::create());
        initialized = true;
    }
#endif
}

void NetworkResourceLoader::startContentFiltering(ResourceRequest&& request, CompletionHandler<void(ResourceRequest)>&& completionHandler)
{
    if (!isMainResource()) {
        completionHandler(WTFMove(request));
        return;
    }
    setSharedParentalControlsURLFilterIfNecessary();
    m_contentFilter = ContentFilter::create(*this);
    CheckedPtr contentFilter = m_contentFilter.get();
#if HAVE(AUDIT_TOKEN)
    contentFilter->setHostProcessAuditToken(protectedConnectionToWebProcess()->networkProcess().sourceApplicationAuditToken());
#endif
    contentFilter->startFilteringMainResource(request.url());

    CompletionHandler<void(ResourceRequest)> completion = [contentFilter, completionHandler = WTFMove(completionHandler)](ResourceRequest&& request) mutable {
        ASSERT(isMainRunLoop());
        if (CheckedPtr filter = std::exchange(contentFilter, nullptr); request.isNull())
            filter->stopFilteringMainResource();
        completionHandler(WTFMove(request));
    };
    contentFilter->continueAfterWillSendRequest(WTFMove(request), ResourceResponse(), WTFMove(completion));
}

#endif

void NetworkResourceLoader::retrieveCacheEntry(const ResourceRequest& request)
{
    LOADER_RELEASE_LOG("retrieveCacheEntry: isMainFrameLoad=%d", isMainFrameLoad());
    ASSERT(canUseCache(request));

    Ref protectedThis { *this };
    RefPtr cache = m_cache;
    if (isMainFrameLoad()) {
        ASSERT(m_parameters.options.mode == FetchOptions::Mode::Navigate);
        if (CheckedPtr session = protectedConnectionToWebProcess()->networkProcess().networkSession(sessionID())) {
            if (auto entry = session->checkedPrefetchCache()->take(request.url())) {
                LOADER_RELEASE_LOG("retrieveCacheEntry: retrieved an entry from the prefetch cache (isRedirect=%d)", !entry->redirectRequest.isNull());
                if (!entry->redirectRequest.isNull()) {
                    auto cacheEntry = cache->makeRedirectEntry(request, entry->response, entry->redirectRequest);
                    retrieveCacheEntryInternal(WTFMove(cacheEntry), ResourceRequest { request });
                    auto maxAgeCap = validateCacheEntryForMaxAgeCapValidation(request, entry->redirectRequest, entry->response);
                    cache->storeRedirect(request, entry->response, entry->redirectRequest, maxAgeCap);
                    return;
                }
                auto buffer = entry->releaseBuffer();
                auto cacheEntry = cache->makeEntry(request, entry->response, entry->privateRelayed, buffer.copyRef());
                retrieveCacheEntryInternal(WTFMove(cacheEntry), ResourceRequest { request });
                cache->store(request, entry->response, entry->privateRelayed, WTFMove(buffer));
                return;
            }
        }
    }

    LOADER_RELEASE_LOG("retrieveCacheEntry: Checking the HTTP disk cache");
    cache->retrieve(request, globalFrameID(), m_parameters.isNavigatingToAppBoundDomain, m_parameters.allowPrivacyProxy, m_parameters.advancedPrivacyProtections, [weakThis = WeakPtr { *this }, request = ResourceRequest { request }](auto entry, auto info) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        LOADER_RELEASE_LOG_WITH_THIS(protectedThis, "retrieveCacheEntry: Done checking the HTTP disk cache (foundCachedEntry=%d)", !!entry);
        protectedThis->logSlowCacheRetrieveIfNeeded(info);

        if (!entry) {
            protectedThis->startNetworkLoad(WTFMove(request), FirstLoad::Yes);
            return;
        }
        protectedThis->retrieveCacheEntryInternal(WTFMove(entry), WTFMove(request));
    });
}

void NetworkResourceLoader::retrieveCacheEntryInternal(std::unique_ptr<NetworkCache::Entry>&& entry, WebCore::ResourceRequest&& request)
{
    LOADER_RELEASE_LOG("retrieveCacheEntryInternal:");
    if (entry->hasReachedPrevalentResourceAgeCap()) {
        LOADER_RELEASE_LOG("retrieveCacheEntryInternal: Revalidating cached entry because it reached the prevalent resource age cap");
        m_cacheEntryForMaxAgeCapValidation = WTFMove(entry);
        ResourceRequest revalidationRequest = originalRequest();
        startNetworkLoad(WTFMove(revalidationRequest), FirstLoad::Yes);
        return;
    }
    if (entry->redirectRequest()) {
        LOADER_RELEASE_LOG("retrieveCacheEntryInternal: Cached entry is a redirect");
        dispatchWillSendRequestForCacheEntry(WTFMove(request), WTFMove(entry));
        return;
    }
    if (m_parameters.needsCertificateInfo && !entry->response().certificateInfo()) {
        LOADER_RELEASE_LOG("retrieveCacheEntryInternal: Cached entry is missing certificate information so we are not using it");
        startNetworkLoad(WTFMove(request), FirstLoad::Yes);
        return;
    }
    if (entry->needsValidation() || request.cachePolicy() == WebCore::ResourceRequestCachePolicy::RefreshAnyCacheData) {
        LOADER_RELEASE_LOG("retrieveCacheEntryInternal: Cached entry needs revalidation");
        validateCacheEntry(WTFMove(entry));
        return;
    }
    LOADER_RELEASE_LOG("retrieveCacheEntryInternal: Cached entry is directly usable");
    didRetrieveCacheEntry(WTFMove(entry));
}

bool NetworkResourceLoader::shouldSendResourceLoadMessages() const
{
    if (m_parameters.pageHasResourceLoadClient)
        return true;

#if ENABLE(WK_WEB_EXTENSIONS)
    if (m_parameters.pageHasLoadedWebExtensions)
        return true;
#endif

    return false;
}

void NetworkResourceLoader::startNetworkLoad(ResourceRequest&& request, FirstLoad load)
{
    if (load == FirstLoad::Yes) {
        consumeSandboxExtensions();

        if (isSynchronous() || m_parameters.maximumBufferingTime > 0_s)
            m_bufferedData.empty();

        if (canUseCache(request))
            m_bufferedDataForCache.empty();
    }

    NetworkLoadParameters parameters = m_parameters.networkLoadParameters();
    parameters.networkActivityTracker = m_networkActivityTracker;
    if (parameters.storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use && m_networkLoadChecker)
        parameters.storedCredentialsPolicy = m_networkLoadChecker->storedCredentialsPolicy();

    CheckedPtr networkSession = protectedConnectionToWebProcess()->networkSession();
    if (!networkSession) {
        WTFLogAlways("Attempted to create a NetworkLoad with a session (id=%" PRIu64 ") that does not exist.", sessionID().toUInt64());
        LOADER_RELEASE_LOG_ERROR("startNetworkLoad: Attempted to create a NetworkLoad for a session that does not exist (sessionID=%" PRIu64 ")", sessionID().toUInt64());
        protectedConnectionToWebProcess()->networkProcess().logDiagnosticMessage(webPageProxyID(), WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::invalidSessionIDKey(), WebCore::ShouldSample::No);
        didFailLoading(internalError(request.url()));
        return;
    }

    if (request.wasSchemeOptimisticallyUpgraded()) {
        double optimisticUpgradeTimeout { 3 };
        if (double average = networkSession->currentHTTPSConnectionAverageTiming())
            optimisticUpgradeTimeout = average;
        request.setTimeoutInterval(optimisticUpgradeTimeout);
    }

    LOADER_RELEASE_LOG("startNetworkLoad: (isFirstLoad=%d, timeout=%f)", load == FirstLoad::Yes, request.timeoutInterval());

    if (request.url().protocolIsBlob()) {
        ASSERT(parameters.topOrigin);
        parameters.blobFileReferences = networkSession->blobRegistry().filesInBlob(originalRequest().url(), parameters.topOrigin ? std::optional { parameters.topOrigin->data() } : std::nullopt);
    }

    if (shouldSendResourceLoadMessages()) {
        std::optional<IPC::FormDataReference> httpBody;
        if (auto formData = request.httpBody()) {
            static constexpr auto maxSerializedRequestSize = 1024 * 1024;
            if (formData->lengthInBytes() <= maxSerializedRequestSize)
                httpBody = IPC::FormDataReference { WTFMove(formData) };
        }
        protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidSendRequest(webPageProxyID(), resourceLoadInfo(), request, httpBody), 0);
    }

    if (networkSession->shouldSendPrivateTokenIPCForTesting())
        protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::DidAllowPrivateTokenUsageByThirdPartyForTesting(sessionID(), request.isPrivateTokenUsageByThirdPartyAllowed(), request.url()), 0);

    parameters.request = WTFMove(request);
    parameters.isNavigatingToAppBoundDomain = m_parameters.isNavigatingToAppBoundDomain;
    m_networkLoad = NetworkLoad::create(*this, WTFMove(parameters), *networkSession);
    
    WeakPtr weakThis { *this };
    RefPtr networkLoad = m_networkLoad;
    if (isSynchronous())
        networkLoad->start(); // May delete this object
    else
        networkLoad->startWithScheduling();

    if (weakThis && networkLoad)
        LOADER_RELEASE_LOG("startNetworkLoad: Going to the network (description=%" PUBLIC_LOG_STRING ")", networkLoad->description().utf8().data());
}

ResourceLoadInfo NetworkResourceLoader::resourceLoadInfo()
{
    auto loadedFromCache = [] (const ResourceResponse& response) {
        switch (response.source()) {
        case ResourceResponse::Source::DiskCache:
        case ResourceResponse::Source::DiskCacheAfterValidation:
        case ResourceResponse::Source::MemoryCache:
        case ResourceResponse::Source::MemoryCacheAfterValidation:
        case ResourceResponse::Source::LegacyApplicationCachePlaceholder:
        case ResourceResponse::Source::DOMCache:
            return true;
        case ResourceResponse::Source::Unknown:
        case ResourceResponse::Source::Network:
        case ResourceResponse::Source::ServiceWorker:
        case ResourceResponse::Source::InspectorOverride:
            break;
        }
        return false;
    };

    auto resourceType = [] (WebCore::ResourceRequestRequester requester, WebCore::FetchOptions::Destination destination) {
        switch (requester) {
        case WebCore::ResourceRequestRequester::XHR:
            return ResourceLoadInfo::Type::XMLHTTPRequest;
        case WebCore::ResourceRequestRequester::Fetch:
            return ResourceLoadInfo::Type::Fetch;
        case WebCore::ResourceRequestRequester::Ping:
            return ResourceLoadInfo::Type::Ping;
        case WebCore::ResourceRequestRequester::Beacon:
            return ResourceLoadInfo::Type::Beacon;
        default:
            break;
        }

        switch (destination) {
        case WebCore::FetchOptions::Destination::EmptyString:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Audio:
            return ResourceLoadInfo::Type::Media;
        case WebCore::FetchOptions::Destination::Audioworklet:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Document:
        case WebCore::FetchOptions::Destination::Iframe:
            return ResourceLoadInfo::Type::Document;
        case WebCore::FetchOptions::Destination::Json:
            return ResourceLoadInfo::Type::Script;
        case WebCore::FetchOptions::Destination::Embed:
            return ResourceLoadInfo::Type::Object;
        case WebCore::FetchOptions::Destination::Environmentmap:
            return ResourceLoadInfo::Type::Media;
        case WebCore::FetchOptions::Destination::Font:
            return ResourceLoadInfo::Type::Font;
        case WebCore::FetchOptions::Destination::Image:
            return ResourceLoadInfo::Type::Image;
        case WebCore::FetchOptions::Destination::Manifest:
            return ResourceLoadInfo::Type::ApplicationManifest;
        case WebCore::FetchOptions::Destination::Model:
            return ResourceLoadInfo::Type::Media;
        case WebCore::FetchOptions::Destination::Object:
            return ResourceLoadInfo::Type::Object;
        case WebCore::FetchOptions::Destination::Paintworklet:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Report:
            return ResourceLoadInfo::Type::CSPReport;
        case WebCore::FetchOptions::Destination::Script:
            return ResourceLoadInfo::Type::Script;
        case WebCore::FetchOptions::Destination::Serviceworker:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Sharedworker:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Speculationrules:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Style:
            return ResourceLoadInfo::Type::Stylesheet;
        case WebCore::FetchOptions::Destination::Track:
            return ResourceLoadInfo::Type::Media;
        case WebCore::FetchOptions::Destination::Video:
            return ResourceLoadInfo::Type::Media;
        case WebCore::FetchOptions::Destination::Worker:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Xslt:
            return ResourceLoadInfo::Type::XSLT;
        }

        ASSERT_NOT_REACHED();
        return ResourceLoadInfo::Type::Other;
    };

    return {
        m_resourceLoadID,
        m_parameters.webFrameID,
        m_parameters.parentFrameID,
        m_parameters.options.resultingClientIdentifier ?: m_parameters.options.clientIdentifier,
        originalRequest().url(),
        originalRequest().httpMethod(),
        WallTime::now(),
        loadedFromCache(m_response),
        resourceType(originalRequest().requester(), m_parameters.options.destination)
    };
}

void NetworkResourceLoader::cleanup(LoadResult result)
{
    ASSERT(RunLoop::isMain());
    LOADER_RELEASE_LOG("cleanup: (result=%u)", static_cast<unsigned>(result));

    NetworkActivityTracker::CompletionCode code { };
    switch (result) {
    case LoadResult::Unknown:
        code = NetworkActivityTracker::CompletionCode::Undefined;
        break;
    case LoadResult::Success:
        code = NetworkActivityTracker::CompletionCode::Success;
        break;
    case LoadResult::Failure:
        code = NetworkActivityTracker::CompletionCode::Failure;
        break;
    case LoadResult::Cancel:
        code = NetworkActivityTracker::CompletionCode::Cancel;
        break;
    }

    Ref connection = m_connection;
    connection->stopTrackingResourceLoad(coreIdentifier(), code);

    m_bufferingTimer.stop();

    invalidateSandboxExtensions();

    if (RefPtr networkLoad = std::exchange(m_networkLoad, nullptr))
        networkLoad->clearClient();

    // This will cause NetworkResourceLoader to be destroyed and therefore we do it last.
    connection->didCleanupResourceLoader(*this);
}

void NetworkResourceLoader::convertToDownload(DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    LOADER_RELEASE_LOG("convertToDownload: (downloadID=%" PRIu64 ", hasNetworkLoad=%d, hasResponseCompletionHandler=%d)", downloadID.toUInt64(), !!m_networkLoad, !!m_responseCompletionHandler);

    RefPtr task = m_serviceWorkerFetchTask;
    if (task && task->convertToDownload(protectedConnectionToWebProcess()->networkProcess().checkedDownloadManager(), downloadID, request, response))
        return;

    // This can happen if the resource came from the disk cache.
    if (!m_networkLoad) {
        protectedConnectionToWebProcess()->networkProcess().checkedDownloadManager()->startDownload(sessionID(), downloadID, request, m_parameters.topOrigin ? std::optional { m_parameters.topOrigin->data() } : std::nullopt, m_parameters.isNavigatingToAppBoundDomain);
        abort();
        return;
    }

    auto networkLoad = std::exchange(m_networkLoad, nullptr);

    if (m_responseCompletionHandler)
        protectedConnectionToWebProcess()->networkProcess().checkedDownloadManager()->convertNetworkLoadToDownload(downloadID, networkLoad.releaseNonNull(), WTFMove(m_responseCompletionHandler), WTFMove(m_fileReferences), request, response);
}

void NetworkResourceLoader::abort()
{
    LOADER_RELEASE_LOG("abort: (hasNetworkLoad=%d)", !!m_networkLoad);
    ASSERT(RunLoop::isMain());

    if (m_parameters.options.keepAlive && m_response.isNull() && !m_isKeptAlive) {
        m_isKeptAlive = true;
        LOADER_RELEASE_LOG("abort: Keeping network load alive due to keepalive option");
        protectedConnectionToWebProcess()->transferKeptAliveLoad(*this);
        return;
    }

    if (auto task = WTFMove(m_serviceWorkerFetchTask)) {
        LOADER_RELEASE_LOG("abort: Cancelling pending service worker fetch task (fetchIdentifier=%" PRIu64 ")", task->fetchIdentifier().toUInt64());
        task->cancelFromClient();
    }

    if (RefPtr networkLoad = m_networkLoad) {
        if (canUseCache(networkLoad->currentRequest())) {
            // We might already have used data from this incomplete load. Ensure older versions don't remain in the cache after cancel.
            if (!m_response.isNull())
                protectedCache()->remove(networkLoad->currentRequest());
        }
        LOADER_RELEASE_LOG("abort: Cancelling network load");
        networkLoad->cancel();
    }

    if (isSynchronous()) {
        m_synchronousLoadData->error = ResourceError { ResourceError::Type::Cancellation };
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr, { });
    }

    cleanup(LoadResult::Cancel);
}

std::optional<NetworkLoadMetrics> NetworkResourceLoader::computeResponseMetrics(const ResourceResponse& response) const
{
    if (parameters().options.mode != FetchOptions::Mode::Navigate)
        return { };

    NetworkLoadMetrics networkLoadMetrics;
    if (auto* metrics = response.deprecatedNetworkLoadMetricsOrNull())
        networkLoadMetrics = *metrics;
    networkLoadMetrics.redirectCount = m_redirectCount;

    return networkLoadMetrics;
}

void NetworkResourceLoader::transferToNewWebProcess(NetworkConnectionToWebProcess& newConnection, const NetworkResourceLoadParameters& parameters)
{
    m_connection = newConnection;
    m_parameters.identifier = parameters.identifier;
    m_parameters.webPageProxyID = parameters.webPageProxyID;
    m_parameters.webPageID = parameters.webPageID;
    m_parameters.webFrameID = parameters.webFrameID;
    m_parameters.options.clientIdentifier = parameters.options.clientIdentifier;

    if (parameters.options.resultingClientIdentifier && m_parameters.options.resultingClientIdentifier)
        send(Messages::WebResourceLoader::UpdateResultingClientIdentifier { *parameters.options.resultingClientIdentifier, *m_parameters.options.resultingClientIdentifier });

    ASSERT(m_responseCompletionHandler || m_cacheEntryWaitingForContinueDidReceiveResponse || m_serviceWorkerFetchTask);
    if (RefPtr serviceWorkerRegistration = m_serviceWorkerRegistration.get()) {
        if (RefPtr swConnection = newConnection.swConnection())
            swConnection->transferServiceWorkerLoadToNewWebProcess(*this, *serviceWorkerRegistration, parameters.request);
    }
    if (m_workerStart)
        send(Messages::WebResourceLoader::SetWorkerStart { m_workerStart }, coreIdentifier());
    bool willWaitForContinueDidReceiveResponse = true;
    send(Messages::WebResourceLoader::DidReceiveResponse { m_response, m_privateRelayed, willWaitForContinueDidReceiveResponse, computeResponseMetrics(m_response) });
}

bool NetworkResourceLoader::shouldInterruptLoadForXFrameOptions(const String& xFrameOptions, const URL& url)
{
    if (isMainFrameLoad())
        return false;

    switch (parseXFrameOptionsHeader(xFrameOptions)) {
    case XFrameOptionsDisposition::None:
    case XFrameOptionsDisposition::AllowAll:
        return false;
    case XFrameOptionsDisposition::Deny:
        return true;
    case XFrameOptionsDisposition::SameOrigin: {
        auto origin = SecurityOrigin::create(url);
        auto topFrameOrigin = m_parameters.frameAncestorOrigins.last();
        if (!origin->isSameSchemeHostPort(topFrameOrigin))
            return true;
        for (auto& ancestorOrigin : m_parameters.frameAncestorOrigins) {
            if (!origin->isSameSchemeHostPort(ancestorOrigin))
                return true;
        }
        return false;
    }
    case XFrameOptionsDisposition::Conflict: {
        auto errorMessage = makeString("Multiple 'X-Frame-Options' headers with conflicting values ('"_s, xFrameOptions, "') encountered when loading '"_s, url.stringCenterEllipsizedToLength(), "'. Falling back to 'DENY'."_s);
        send(Messages::WebPage::AddConsoleMessage { frameID(),  MessageSource::JS, MessageLevel::Error, errorMessage, coreIdentifier() }, pageID());
        return true;
    }
    case XFrameOptionsDisposition::Invalid: {
        auto errorMessage = makeString("Invalid 'X-Frame-Options' header encountered when loading '"_s, url.stringCenterEllipsizedToLength(), "': '"_s, xFrameOptions, "' is not a recognized directive. The header will be ignored."_s);
        send(Messages::WebPage::AddConsoleMessage { frameID(),  MessageSource::JS, MessageLevel::Error, errorMessage, coreIdentifier() }, pageID());
        return false;
    }
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool NetworkResourceLoader::shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(const ResourceResponse& response)
{
    ASSERT(isMainResource());

    auto sharedPreferences = connectionToWebProcess().sharedPreferencesForWebProcess();
    if (!sharedPreferences || sharedPreferences->ignoreIframeEmbeddingProtectionsEnabled)
        return false;

#if USE(QUICK_LOOK)
    if (PreviewConverter::supportsMIMEType(response.mimeType()))
        return false;
#endif

    auto url = response.url();
    ContentSecurityPolicy contentSecurityPolicy { URL { url }, this, this };
    contentSecurityPolicy.didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, originalRequest().httpReferrer());
    if (!contentSecurityPolicy.allowFrameAncestors(m_parameters.frameAncestorOrigins, url))
        return true;

    if (shouldInterruptNavigationForCrossOriginEmbedderPolicy(response))
        return true;

    if (!contentSecurityPolicy.overridesXFrameOptions()) {
        String xFrameOptions = response.httpHeaderField(HTTPHeaderName::XFrameOptions);
        if (!xFrameOptions.isNull() && shouldInterruptLoadForXFrameOptions(xFrameOptions, response.url())) {
            String errorMessage = makeString("Refused to display '"_s, response.url().stringCenterEllipsizedToLength(), "' in a frame because it set 'X-Frame-Options' to '"_s, xFrameOptions, "'."_s);
            send(Messages::WebPage::AddConsoleMessage { frameID(),  MessageSource::Security, MessageLevel::Error, errorMessage, coreIdentifier() }, pageID());
            return true;
        }
    }

    return false;
}

bool NetworkResourceLoader::shouldInterruptNavigationForCrossOriginEmbedderPolicy(const ResourceResponse& response)
{
    ASSERT(isMainResource());

    // https://html.spec.whatwg.org/multipage/origin.html#check-a-navigation-response's-adherence-to-its-embedder-policy
    if (m_parameters.parentCrossOriginEmbedderPolicy.value == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP || m_parameters.parentCrossOriginEmbedderPolicy.reportOnlyValue == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP) {
        auto responseCOEP = WebCore::obtainCrossOriginEmbedderPolicy(response, nullptr);
        if (m_parameters.parentCrossOriginEmbedderPolicy.reportOnlyValue == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP && responseCOEP.value != WebCore::CrossOriginEmbedderPolicyValue::RequireCORP)
            sendCOEPInheritenceViolation(*this, m_parameters.parentFrameURL.isValid() ? m_parameters.parentFrameURL : aboutBlankURL(), m_parameters.parentCrossOriginEmbedderPolicy.reportOnlyReportingEndpoint, COEPDisposition::Reporting, "navigation"_s, m_firstResponseURL);

        if (m_parameters.parentCrossOriginEmbedderPolicy.value != WebCore::CrossOriginEmbedderPolicyValue::UnsafeNone && responseCOEP.value != WebCore::CrossOriginEmbedderPolicyValue::RequireCORP) {
            String errorMessage = makeString("Refused to display '"_s, response.url().stringCenterEllipsizedToLength(), "' in a frame because of Cross-Origin-Embedder-Policy."_s);
            send(Messages::WebPage::AddConsoleMessage { frameID(),  MessageSource::Security, MessageLevel::Error, errorMessage, coreIdentifier() }, pageID());
            sendCOEPInheritenceViolation(*this, m_parameters.parentFrameURL.isValid() ? m_parameters.parentFrameURL : aboutBlankURL(), m_parameters.parentCrossOriginEmbedderPolicy.reportingEndpoint, COEPDisposition::Enforce, "navigation"_s, m_firstResponseURL);
            return true;
        }
    }

    return false;
}

// https://html.spec.whatwg.org/multipage/origin.html#check-a-global-object's-embedder-policy
bool NetworkResourceLoader::shouldInterruptWorkerLoadForCrossOriginEmbedderPolicy(const ResourceResponse& response)
{
    if (m_parameters.options.destination != FetchOptions::Destination::Worker)
        return false;

    if (m_parameters.crossOriginEmbedderPolicy.value == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP || m_parameters.crossOriginEmbedderPolicy.reportOnlyValue == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP) {
        auto responseCOEP = WebCore::obtainCrossOriginEmbedderPolicy(response, nullptr);
        if (m_parameters.crossOriginEmbedderPolicy.reportOnlyValue == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP && responseCOEP.value == WebCore::CrossOriginEmbedderPolicyValue::UnsafeNone)
            sendCOEPInheritenceViolation(*this, m_parameters.frameURL.isValid() ? m_parameters.frameURL : aboutBlankURL(), m_parameters.crossOriginEmbedderPolicy.reportOnlyReportingEndpoint, COEPDisposition::Reporting, "worker initialization"_s, m_firstResponseURL);

        if (m_parameters.crossOriginEmbedderPolicy.value == WebCore::CrossOriginEmbedderPolicyValue::RequireCORP && responseCOEP.value == WebCore::CrossOriginEmbedderPolicyValue::UnsafeNone) {
            String errorMessage = makeString("Refused to load '"_s, response.url().stringCenterEllipsizedToLength(), "' worker because of Cross-Origin-Embedder-Policy."_s);
            send(Messages::WebPage::AddConsoleMessage { frameID(),  MessageSource::Security, MessageLevel::Error, errorMessage, coreIdentifier() }, pageID());
            sendCOEPInheritenceViolation(*this, m_parameters.frameURL.isValid() ? m_parameters.frameURL : aboutBlankURL(), m_parameters.crossOriginEmbedderPolicy.reportingEndpoint, COEPDisposition::Enforce, "worker initialization"_s, m_firstResponseURL);
            return true;
        }
    }

    return false;
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch (Step 12.5.6)
std::optional<ResourceError> NetworkResourceLoader::doCrossOriginOpenerHandlingOfResponse(const ResourceResponse& response)
{
    // COOP only applies to top-level browsing contexts.
    if (!isMainFrameLoad())
        return std::nullopt;

    if (!m_parameters.isCrossOriginOpenerPolicyEnabled)
        return std::nullopt;

    std::unique_ptr<ContentSecurityPolicy> contentSecurityPolicy;
    if (!response.httpHeaderField(HTTPHeaderName::ContentSecurityPolicy).isNull()) {
        contentSecurityPolicy = makeUnique<ContentSecurityPolicy>(URL { response.url() }, nullptr, this);
        contentSecurityPolicy->didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, originalRequest().httpReferrer(), ContentSecurityPolicy::ReportParsingErrors::No);
    }

    if (!m_currentCoopEnforcementResult) {
        auto sourceOrigin = m_parameters.sourceOrigin ? Ref { *m_parameters.sourceOrigin } : SecurityOrigin::createOpaque();
        m_currentCoopEnforcementResult = CrossOriginOpenerPolicyEnforcementResult::from(m_parameters.documentURL, WTFMove(sourceOrigin), m_parameters.sourceCrossOriginOpenerPolicy, m_parameters.navigationRequester, m_parameters.openerURL);
    }

    m_currentCoopEnforcementResult = WebCore::doCrossOriginOpenerHandlingOfResponse(*this, response, m_parameters.navigationRequester, CheckedPtr { contentSecurityPolicy.get() }.get(), m_parameters.effectiveSandboxFlags, originalRequest().httpReferrer(), m_parameters.isDisplayingInitialEmptyDocument, *m_currentCoopEnforcementResult);
    if (!m_currentCoopEnforcementResult)
        return ResourceError { errorDomainWebKitInternal, 0, response.url(), "Navigation was blocked by Cross-Origin-Opener-Policy"_s, ResourceError::Type::AccessControl };
    return std::nullopt;
}

void NetworkResourceLoader::processClearSiteDataHeader(const WebCore::ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    if (!m_parameters.isClearSiteDataHeaderEnabled)
        return completionHandler();

    auto clearSiteDataValues = parseClearSiteDataHeader(response);
    OptionSet<WebsiteDataType> typesToRemove;
    if (clearSiteDataValues.contains(ClearSiteDataValue::Cache))
        typesToRemove.add({ WebsiteDataType::DiskCache, WebsiteDataType::MemoryCache });
    if (clearSiteDataValues.contains(ClearSiteDataValue::Cookies))
        typesToRemove.add(WebsiteDataType::Cookies);
    if (clearSiteDataValues.contains(ClearSiteDataValue::Storage)) {
        typesToRemove.add({ WebsiteDataType::LocalStorage, WebsiteDataType::SessionStorage, WebsiteDataType::IndexedDBDatabases, WebsiteDataType::DOMCache, WebsiteDataType::FileSystem, WebsiteDataType::WebSQLDatabases });
        typesToRemove.add(WebsiteDataType::ServiceWorkerRegistrations);
    }

    bool shouldReloadExecutionContexts = m_parameters.isClearSiteDataExecutionContextEnabled && clearSiteDataValues.contains(ClearSiteDataValue::ExecutionContexts);
    if (!typesToRemove && !shouldReloadExecutionContexts)
        return completionHandler();

    LOADER_RELEASE_LOG("processClearSiteDataHeader: BEGIN");

    auto origin = SecurityOrigin::create(response.url())->data();
    ClientOrigin clientOrigin {
        m_parameters.topOrigin ? m_parameters.topOrigin->data() : origin,
        origin
    };

    auto callbackAggregator = CallbackAggregator::create([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler();

        LOADER_RELEASE_LOG_WITH_THIS(protectedThis, "processClearSiteDataHeader: END");
        completionHandler();
    });
    if (typesToRemove)
        protectedConnectionToWebProcess()->networkProcess().deleteWebsiteDataForOrigin(sessionID(), typesToRemove, clientOrigin, [callbackAggregator] { });

    if (WebsiteDataStore::computeWebProcessAccessTypeForDataRemoval(typesToRemove, sessionID().isEphemeral()) != WebsiteDataStore::ProcessAccessType::None)
        protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::DeleteWebsiteDataInWebProcessesForOrigin(typesToRemove, clientOrigin, sessionID(), webPageProxyID()), [callbackAggregator] { });

    if (shouldReloadExecutionContexts) {
        std::optional<WebCore::FrameIdentifier> triggeringFrame;
        if (isMainResource())
            triggeringFrame = frameID();
        protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::ReloadExecutionContextsForOrigin(clientOrigin, sessionID(), triggeringFrame), [callbackAggregator] { });
    }
}

static BrowsingContextGroupSwitchDecision toBrowsingContextGroupSwitchDecision(const std::optional<CrossOriginOpenerPolicyEnforcementResult>& currentCoopEnforcementResult)
{
    if (!currentCoopEnforcementResult || !currentCoopEnforcementResult->needsBrowsingContextGroupSwitch)
        return BrowsingContextGroupSwitchDecision::StayInGroup;
    if (currentCoopEnforcementResult->crossOriginOpenerPolicy.value == CrossOriginOpenerPolicyValue::SameOriginPlusCOEP)
        return BrowsingContextGroupSwitchDecision::NewIsolatedGroup;
    return BrowsingContextGroupSwitchDecision::NewSharedGroup;
}

void NetworkResourceLoader::didReceiveInformationalResponse(ResourceResponse&& response)
{
    if (response.httpStatusCode() != httpStatus103EarlyHints)
        return;

    if (!m_earlyHintsResourceLoader)
        m_earlyHintsResourceLoader = WTF::makeUnique<EarlyHintsResourceLoader>(*this);
    m_earlyHintsResourceLoader->handleEarlyHintsResponse(WTFMove(response));
}

void NetworkResourceLoader::didReceiveResponse(ResourceResponse&& receivedResponse, PrivateRelayed privateRelayed, ResponseCompletionHandler&& completionHandler)
{
    LOADER_RELEASE_LOG("didReceiveResponse: (httpStatusCode=%d, MIMEType=%" PUBLIC_LOG_STRING ", expectedContentLength=%lld, hasCachedEntryForValidation=%d, hasNetworkLoadChecker=%d)", receivedResponse.httpStatusCode(), receivedResponse.mimeType().utf8().data(), receivedResponse.expectedContentLength(), !!m_cacheEntryForValidation, !!m_networkLoadChecker);

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !checkedContentFilter()->continueAfterResponseReceived(receivedResponse))
        return completionHandler(PolicyAction::Ignore);
#endif

    if (isMainResource())
        didReceiveMainResourceResponse(receivedResponse);

    m_response = WTFMove(receivedResponse);
    m_privateRelayed = privateRelayed;
    if (!m_firstResponseURL.isValid())
        m_firstResponseURL = m_response.url();

    Ref connection = m_connection;
    RefPtr networkLoadChecker = m_networkLoadChecker;

    if (shouldCaptureExtraNetworkLoadMetrics() && networkLoadChecker) {
        auto information = networkLoadChecker->takeNetworkLoadInformation();
        information.response = m_response;
        connection->addNetworkLoadInformation(coreIdentifier(), WTFMove(information));
    }

    if (CheckedPtr networkSession = m_response.url().protocolIs("https"_s) ? connection->networkSession() : nullptr) {
        if (auto metrics = computeResponseMetrics(m_response))
            networkSession->recordHTTPSConnectionTiming(*metrics);
    }

    auto resourceLoadInfo = this->resourceLoadInfo();

    auto isFetchOrXHR = [] (const ResourceLoadInfo& info) {
        return info.type == ResourceLoadInfo::Type::Fetch
            || info.type == ResourceLoadInfo::Type::XMLHTTPRequest;
    };

    auto isMediaMIMEType = [] (const String& mimeType) {
        return startsWithLettersIgnoringASCIICase(mimeType, "audio/"_s)
            || startsWithLettersIgnoringASCIICase(mimeType, "video/"_s)
            || equalLettersIgnoringASCIICase(mimeType, "application/octet-stream"_s);
    };

    if (!m_bufferedData
        && m_response.expectedContentLength() > static_cast<long long>(1 * MB)
        && isFetchOrXHR(resourceLoadInfo)
        && isMediaMIMEType(m_response.mimeType())) {
        m_bufferedData.empty();
        m_parameters.maximumBufferingTime = WebLoaderStrategy::mediaMaximumBufferingTime;
    }

    // For multipart/x-mixed-replace didReceiveResponseAsync gets called multiple times and buffering would require special handling.
    if (!isSynchronous() && m_response.isMultipart())
        m_bufferedData.reset();

    if (m_response.isMultipart())
        m_bufferedDataForCache.reset();

    if (m_cacheEntryForValidation) {
        bool validationSucceeded = m_response.httpStatusCode() == httpStatus304NotModified;
        LOADER_RELEASE_LOG("didReceiveResponse: Received revalidation response (validationSucceeded=%d, wasOriginalRequestConditional=%d)", validationSucceeded, originalRequest().isConditional());
        if (validationSucceeded) {
            m_cacheEntryForValidation = protectedCache()->update(originalRequest(), *m_cacheEntryForValidation, m_response, m_privateRelayed);
            // If the request was conditional then this revalidation was not triggered by the network cache and we pass the 304 response to WebCore.
            if (originalRequest().isConditional()) {
                // Add CORP header to the 304 response if previously set to avoid being blocked by load checker due to COEP.
                auto crossOriginResourcePolicy = m_cacheEntryForValidation->response().httpHeaderField(HTTPHeaderName::CrossOriginResourcePolicy);
                if (!crossOriginResourcePolicy.isEmpty())
                    m_response.setHTTPHeaderField(HTTPHeaderName::CrossOriginResourcePolicy, crossOriginResourcePolicy);
                auto crossOriginEmbedderPolicy = m_cacheEntryForValidation->response().httpHeaderField(HTTPHeaderName::CrossOriginEmbedderPolicy);
                if (!crossOriginEmbedderPolicy.isEmpty())
                    m_response.setHTTPHeaderField(HTTPHeaderName::CrossOriginEmbedderPolicy, crossOriginEmbedderPolicy);
                m_cacheEntryForValidation = nullptr;
            }
        } else
            m_cacheEntryForValidation = nullptr;
    }
    if (m_cacheEntryForValidation)
        return completionHandler(PolicyAction::Use);

    if (networkLoadChecker) {
        auto error = networkLoadChecker->validateResponse(m_networkLoad ? m_networkLoad->currentRequest() : originalRequest(), m_response);
        if (!error.isNull()) {
            LOADER_RELEASE_LOG_ERROR("didReceiveResponse: NetworkLoadChecker::validateResponse returned an error (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d)", error.domain().utf8().data(), error.errorCode());
            RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, error = WTFMove(error)] {
                if (protectedThis->m_networkLoad)
                    protectedThis->didFailLoading(error);
            });
            return completionHandler(PolicyAction::Ignore);
        }
        if (RefPtr networkLoad = m_networkLoad; networkLoad && networkLoadChecker->timingAllowFailedFlag())
            networkLoad->setTimingAllowFailedFlag();
    }

    initializeReportingEndpoints(m_response);

    if (isMainResource() && shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(m_response)) {
        LOADER_RELEASE_LOG_ERROR("didReceiveResponse: Interrupting main resource load due to CSP frame-ancestors or X-Frame-Options");
        auto response = sanitizeResponseIfPossible(ResourceResponse { m_response }, ResourceResponse::SanitizationType::CrossOriginSafe);
        send(Messages::WebResourceLoader::StopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied { response });
        return completionHandler(PolicyAction::Ignore);
    }

    // https://html.spec.whatwg.org/multipage/origin.html#check-a-global-object's-embedder-policy
    if (shouldInterruptWorkerLoadForCrossOriginEmbedderPolicy(m_response)) {
        LOADER_RELEASE_LOG_ERROR("didReceiveResponse: Interrupting worker load due to Cross-Origin-Opener-Policy");
        RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, url = m_response.url()] {
            if (protectedThis->m_networkLoad)
                protectedThis->didFailLoading(ResourceError { errorDomainWebKitInternal, 0, url, "Worker load was blocked by Cross-Origin-Embedder-Policy"_s, ResourceError::Type::AccessControl });
        });
        return completionHandler(PolicyAction::Ignore);
    }

    if (auto error = doCrossOriginOpenerHandlingOfResponse(m_response)) {
        LOADER_RELEASE_LOG_ERROR("didReceiveResponse: Interrupting load due to Cross-Origin-Opener-Policy");
        RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, error = WTFMove(*error)] {
            if (protectedThis->m_networkLoad)
                protectedThis->didFailLoading(error);
        });
        return completionHandler(PolicyAction::Ignore);
    }

    processClearSiteDataHeader(m_response, [this, protectedThis = Ref { *this }, privateRelayed, resourceLoadInfo = WTFMove(resourceLoadInfo), completionHandler = WTFMove(completionHandler)] () mutable {
        auto response = sanitizeResponseIfPossible(ResourceResponse { m_response }, ResourceResponse::SanitizationType::CrossOriginSafe);
        if (isSynchronous()) {
            LOADER_RELEASE_LOG("didReceiveResponse: Using response for synchronous load");
            m_synchronousLoadData->response = WTFMove(response);
            return completionHandler(PolicyAction::Use);
        }

        if (isCrossOriginPrefetch()) {
            LOADER_RELEASE_LOG("didReceiveResponse: Using response for cross-origin prefetch");
            if (response.httpHeaderField(HTTPHeaderName::Vary).contains("Cookie"_s)) {
                LOADER_RELEASE_LOG("didReceiveResponse: Canceling cross-origin prefetch for Vary: Cookie");
                abort();
                return completionHandler(PolicyAction::Ignore);
            }
            return completionHandler(PolicyAction::Use);
        }

        // We wait to receive message NetworkResourceLoader::ContinueDidReceiveResponse before continuing a load for
        // a main resource because the embedding client must decide whether to allow the load.
        bool willWaitForContinueDidReceiveResponse = isMainResource();
        LOADER_RELEASE_LOG("didReceiveResponse: Sending WebResourceLoader::DidReceiveResponse IPC (willWaitForContinueDidReceiveResponse=%d)", willWaitForContinueDidReceiveResponse);
        sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup(response, privateRelayed, willWaitForContinueDidReceiveResponse);

        if (shouldSendResourceLoadMessages())
            protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidReceiveResponse(webPageProxyID(), resourceLoadInfo, response), 0);

        if (willWaitForContinueDidReceiveResponse) {
            m_responseCompletionHandler = WTFMove(completionHandler);
            return;
        }

        if (m_isKeptAlive) {
            LOADER_RELEASE_LOG("didReceiveResponse: Ignoring response because of keepalive option");
            return completionHandler(PolicyAction::Ignore);
        }

        LOADER_RELEASE_LOG("didReceiveResponse: Using response");
        completionHandler(PolicyAction::Use);
    });
}

void NetworkResourceLoader::sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup(const WebCore::ResourceResponse& response, PrivateRelayed privateRelayed, bool needsContinueDidReceiveResponseMessage)
{
    Ref connection = m_connection;
    auto browsingContextGroupSwitchDecision = connection->usesSingleWebProcess()? BrowsingContextGroupSwitchDecision::StayInGroup: toBrowsingContextGroupSwitchDecision(m_currentCoopEnforcementResult);
    if (browsingContextGroupSwitchDecision == BrowsingContextGroupSwitchDecision::StayInGroup) {
        send(Messages::WebResourceLoader::DidReceiveResponse { response, privateRelayed, needsContinueDidReceiveResponseMessage, computeResponseMetrics(response) });
        return;
    }

    auto loader = connection->takeNetworkResourceLoader(coreIdentifier());
    if (!loader) {
        LOADER_RELEASE_LOG_FAULT("sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup: Failed to find loader with identifier %" PRIu64 ", m_isKeptAlive=%d, needsContinueDidReceiveResponseMessage=%d", coreIdentifier().toUInt64(), m_isKeptAlive, needsContinueDidReceiveResponseMessage);
        send(Messages::WebResourceLoader::DidReceiveResponse { response, privateRelayed, needsContinueDidReceiveResponseMessage, computeResponseMetrics(response) });
        return;
    }
    if (!m_parameters.navigationID) {
        LOADER_RELEASE_LOG_FAULT("sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup: Missing navigationID, loaderIdentifier %" PRIu64 ", m_isKeptAlive=%d, needsContinueDidReceiveResponseMessage=%d", coreIdentifier().toUInt64(), m_isKeptAlive, needsContinueDidReceiveResponseMessage);
        send(Messages::WebResourceLoader::DidReceiveResponse { response, privateRelayed, needsContinueDidReceiveResponseMessage, computeResponseMetrics(response) });
        return;
    }

    ASSERT(loader == this);
    auto existingNetworkResourceLoadIdentifierToResume = loader->identifier();
    if (CheckedPtr session = connection->networkSession())
        session->addLoaderAwaitingWebProcessTransfer(loader.releaseNonNull());
    Site responseSite { response.url() };
    connection->networkProcess().protectedParentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::TriggerBrowsingContextGroupSwitchForNavigation(webPageProxyID(), *m_parameters.navigationID, browsingContextGroupSwitchDecision, responseSite, existingNetworkResourceLoadIdentifierToResume), [existingNetworkResourceLoadIdentifierToResume, session = WeakPtr { connection->networkSession() }](bool success) {
        if (success)
            return;
        if (session)
            session->removeLoaderWaitingWebProcessTransfer(existingNetworkResourceLoadIdentifierToResume);
    });
}

void NetworkResourceLoader::didReceiveBuffer(const WebCore::FragmentedSharedBuffer& buffer)
{
    if (!m_numBytesReceived)
        LOADER_RELEASE_LOG("didReceiveData: Started receiving data");
    m_numBytesReceived += buffer.size();

    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data and limit size of entries in the cache.
        const size_t maximumCacheBufferSize = protectedCache()->capacity() / 8;
        if (m_bufferedDataForCache.size() + buffer.size() <= maximumCacheBufferSize)
            m_bufferedDataForCache.append(buffer);
        else
            m_bufferedDataForCache.reset();
    }
    if (isCrossOriginPrefetch())
        return;

    if (m_bufferedData) {
        m_bufferedData.append(buffer);
        startBufferingTimerIfNeeded();
        return;
    }
    sendBuffer(buffer);
}

void NetworkResourceLoader::didFinishLoading(const NetworkLoadMetrics& networkLoadMetrics)
{
    ASSERT(!m_networkLoadChecker || networkLoadMetrics.failsTAOCheck == m_networkLoadChecker->timingAllowFailedFlag());

    LOADER_RELEASE_LOG("didFinishLoading: (numBytesReceived=%zd, hasCacheEntryForValidation=%d)", m_numBytesReceived, !!m_cacheEntryForValidation);

    // rdar://149080634: We can remove this when we finish investigating the logout issues.
    if (m_response.httpStatusCode() >= httpStatus400BadRequest && networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector) {
        const auto& requestMetrics = networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector;
        const auto& cookies = requestMetrics->requestHeaders.get(HTTPHeaderName::Cookie);
        LOADER_RELEASE_LOG_DEBUG("didFinishLoading: (httpStatusCode=%d, headerCount=%u, cookieHeaderLength=%u, cookieCount=%zu)", m_response.httpStatusCode(), requestMetrics->requestHeaders.size(), cookies.length(), cookies.split("; "_s).size());
    }

    Ref connection = m_connection;
    if (shouldCaptureExtraNetworkLoadMetrics())
        connection->addNetworkLoadInformationMetrics(coreIdentifier(), networkLoadMetrics);

    if (m_cacheEntryForValidation) {
        ASSERT(m_response.httpStatusCode() == httpStatus304NotModified);
        LOG(NetworkCache, "(NetworkProcess) revalidated");
        didRetrieveCacheEntry(WTFMove(m_cacheEntryForValidation));
        return;
    }

#if !RELEASE_LOG_DISABLED
    if (shouldLogCookieInformation(connection, sessionID()))
        logCookieInformation();
#endif

    if (isSynchronous())
        sendReplyToSynchronousRequest(*m_synchronousLoadData, m_bufferedData.get().get(), networkLoadMetrics);
    else {
        if (!m_bufferedData.isEmpty()) {
            sendBuffer(*m_bufferedData.get());
        }
#if ENABLE(CONTENT_FILTERING)
        if (CheckedPtr contentFilter = m_contentFilter.get()) {
            if (!contentFilter->continueAfterNotifyFinished(m_parameters.request.url()))
                return;
            contentFilter->stopFilteringMainResource();
        }
#endif
        send(Messages::WebResourceLoader::DidFinishResourceLoad(networkLoadMetrics));
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (networkLoadMetrics.responseBodyBytesReceived != std::numeric_limits<uint64_t>::max())
        updateBytesTransferredOverNetwork(networkLoadMetrics.responseBodyBytesReceived);
#endif

    tryStoreAsCacheEntry();

    if (shouldSendResourceLoadMessages())
        protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidCompleteWithError(webPageProxyID(), resourceLoadInfo(), m_response, { }), 0);

    cleanup(LoadResult::Success);
}

void NetworkResourceLoader::didFailLoading(const ResourceError& error)
{
    bool wasServiceWorkerLoad = false;
    wasServiceWorkerLoad = !!m_serviceWorkerFetchTask;
    LOADER_RELEASE_LOG_ERROR("didFailLoading: (wasServiceWorkerLoad=%d, isTimeout=%d, isCancellation=%d, isAccessControl=%d, errorCode=%d)", wasServiceWorkerLoad, error.isTimeout(), error.isCancellation(), error.isAccessControl(), error.errorCode());
    UNUSED_VARIABLE(wasServiceWorkerLoad);

    Ref connection = m_connection;
    if (shouldCaptureExtraNetworkLoadMetrics())
        connection->removeNetworkLoadInformation(coreIdentifier());

    ASSERT(!error.isNull());

    m_cacheEntryForValidation = nullptr;

    if (isSynchronous()) {
        m_synchronousLoadData->error = error;
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr, { });
    } else if (RefPtr connection = messageSenderConnection()) {
        if (m_serviceWorkerFetchTask)
            connection->send(Messages::WebResourceLoader::DidFailServiceWorkerLoad(error), messageSenderDestinationID());
        else
            connection->send(Messages::WebResourceLoader::DidFailResourceLoad(error), messageSenderDestinationID());
    }

    if (shouldSendResourceLoadMessages())
        connection->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidCompleteWithError(webPageProxyID(), resourceLoadInfo(), { }, error), 0);
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    if (error.blockedKnownTracker()) {
        auto effectiveBlockedURL = error.failingURL();
        if (auto hostName = error.blockedTrackerHostName(); !hostName.isEmpty())
            effectiveBlockedURL.setHost(hostName);
        connection->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::DidBlockLoadToKnownTracker(webPageProxyID(), WTFMove(effectiveBlockedURL)), 0);
    }
#endif
    cleanup(LoadResult::Failure);
}

void NetworkResourceLoader::didBlockAuthenticationChallenge()
{
    LOADER_RELEASE_LOG("didBlockAuthenticationChallenge:");
    send(Messages::WebResourceLoader::DidBlockAuthenticationChallenge());
}

void NetworkResourceLoader::didReceiveChallenge(const AuthenticationChallenge& challenge)
{
    if (shouldSendResourceLoadMessages())
        protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidReceiveChallenge(webPageProxyID(), resourceLoadInfo(), challenge), 0);
}

std::optional<Seconds> NetworkResourceLoader::validateCacheEntryForMaxAgeCapValidation(const ResourceRequest& request, const ResourceRequest& redirectRequest, const ResourceResponse& redirectResponse)
{
    bool existingCacheEntryMatchesNewResponse = false;
    if (m_cacheEntryForMaxAgeCapValidation) {
        ASSERT(redirectResponse.source() == ResourceResponse::Source::Network);
        ASSERT(redirectResponse.isRedirection());
        if (redirectResponse.httpHeaderField(WebCore::HTTPHeaderName::Location) == m_cacheEntryForMaxAgeCapValidation->response().httpHeaderField(WebCore::HTTPHeaderName::Location))
            existingCacheEntryMatchesNewResponse = true;

        protectedCache()->remove(m_cacheEntryForMaxAgeCapValidation->key());
        m_cacheEntryForMaxAgeCapValidation = nullptr;
    }
    
    if (!existingCacheEntryMatchesNewResponse) {
        if (CheckedPtr networkStorageSession = protectedConnectionToWebProcess()->networkProcess().storageSession(sessionID()))
            return networkStorageSession->maxAgeCacheCap(request, NetworkSession::isRequestToKnownCrossSiteTracker(request));
    }
    return std::nullopt;
}

void NetworkResourceLoader::willSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    willSendRedirectedRequestInternal(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), IsFromServiceWorker::No, WTFMove(completionHandler));
}

void NetworkResourceLoader::willSendServiceWorkerRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    willSendRedirectedRequestInternal(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), IsFromServiceWorker::Yes, [] (auto) { });
}

void NetworkResourceLoader::willSendRedirectedRequestInternal(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse, IsFromServiceWorker isFromServiceWorker, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    LOADER_RELEASE_LOG("willSendRedirectedRequest:");
    ++m_redirectCount;
    m_redirectResponse = redirectResponse;
    if (!m_firstResponseURL.isValid())
        m_firstResponseURL = redirectResponse.url();

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !checkedContentFilter()->continueAfterWillSendRequest(redirectRequest, redirectResponse)) {
        if (RefPtr networkLoad = std::exchange(m_networkLoad, nullptr))
            networkLoad->clearClient();
        return completionHandler({ });
    }
#endif

    std::optional<WebCore::PCM::AttributionTriggerData> privateClickMeasurementAttributionTriggerData;
    if (auto result = WebCore::PrivateClickMeasurement::parseAttributionRequest(redirectRequest.url())) {
        privateClickMeasurementAttributionTriggerData = result.value();
        if (privateClickMeasurementAttributionTriggerData)
            privateClickMeasurementAttributionTriggerData->destinationSite = WebCore::RegistrableDomain { request.firstPartyForCookies() };
    } else if (!result.error().isEmpty())
        addConsoleMessage(MessageSource::PrivateClickMeasurement, MessageLevel::Error, result.error());

    if (isFromServiceWorker == IsFromServiceWorker::No) {
        auto maxAgeCap = validateCacheEntryForMaxAgeCapValidation(request, redirectRequest, redirectResponse);
        if (redirectResponse.source() == ResourceResponse::Source::Network && canUseCachedRedirect(request))
            protectedCache()->storeRedirect(request, redirectResponse, redirectRequest, maxAgeCap);
    }

    if (isMainResource() && shouldInterruptNavigationForCrossOriginEmbedderPolicy(redirectResponse)) {
        this->didFailLoading(ResourceError { errorDomainWebKitInternal, 0, redirectRequest.url(), "Redirection was blocked by Cross-Origin-Embedder-Policy"_s, ResourceError::Type::AccessControl });
        return completionHandler({ });
    }

    if (auto error = doCrossOriginOpenerHandlingOfResponse(redirectResponse)) {
        didFailLoading(*error);
        return completionHandler({ });
    }

    if (auto authorization = request.httpHeaderField(WebCore::HTTPHeaderName::Authorization); !authorization.isNull()
#if PLATFORM(COCOA)
        && linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::AuthorizationHeaderOnSameOriginRedirects)
#endif
        && protocolHostAndPortAreEqual(request.url(), redirectRequest.url())) {
        redirectRequest.setHTTPHeaderField(WebCore::HTTPHeaderName::Authorization, authorization);
    }

    if (request.wasSchemeOptimisticallyUpgraded()) {
        LOADER_RELEASE_LOG("willSendRedirectedRequest: Resetting request timeout to the default value after redirect");

        redirectRequest.resetTimeoutInterval();
    }

    if (RefPtr networkLoadChecker = m_networkLoadChecker) {
        if (privateClickMeasurementAttributionTriggerData)
            networkLoadChecker->enableContentExtensionsCheck();
        networkLoadChecker->storeRedirectionIfNeeded(request, redirectResponse);

        LOADER_RELEASE_LOG("willSendRedirectedRequest: Checking redirect using NetworkLoadChecker");
        auto continueAfterRedirectionCheck = [
            this,
            protectedThis = Ref { *this },
            storedCredentialsPolicy = networkLoadChecker->storedCredentialsPolicy(),
            privateClickMeasurementAttributionTriggerData = WTFMove(privateClickMeasurementAttributionTriggerData),
            completionHandler = WTFMove(completionHandler)
        ] (auto&& result) mutable {
            if (!result.has_value()) {
                if (result.error().isCancellation()) {
                    LOADER_RELEASE_LOG("willSendRedirectedRequest: NetworkLoadChecker::checkRedirection returned with a cancellation");
                    return completionHandler({ });
                }

                LOADER_RELEASE_LOG_ERROR("willSendRedirectedRequest: NetworkLoadChecker::checkRedirection returned an error");
                this->didFailLoading(result.error());
                return completionHandler({ });
            }

            if (RefPtr networkLoad = m_networkLoad; networkLoad && m_networkLoadChecker && m_networkLoadChecker->timingAllowFailedFlag())
                networkLoad->setTimingAllowFailedFlag();

            LOADER_RELEASE_LOG("willSendRedirectedRequest: NetworkLoadChecker::checkRedirection is done");
            if (m_parameters.options.redirect == FetchOptions::Redirect::Manual) {
                this->didFinishWithRedirectResponse(WTFMove(result->request), WTFMove(result->redirectRequest), WTFMove(result->redirectResponse));
                return completionHandler({ });
            }

            if (this->isSynchronous()) {
                if (storedCredentialsPolicy != m_networkLoadChecker->storedCredentialsPolicy()) {
                    // We need to restart the load to update the session according the new credential policy.
                    LOADER_RELEASE_LOG("willSendRedirectedRequest: Restarting network load due to credential policy change for synchronous load");
                    this->restartNetworkLoad(WTFMove(result->redirectRequest), WTFMove(completionHandler));
                    return;
                }

                // We do not support prompting for credentials for synchronous loads. If we ever change this policy then
                // we need to take care to prompt if and only if request and redirectRequest are not mixed content.
                this->continueWillSendRequest(WTFMove(result->redirectRequest), false, WTFMove(completionHandler));
                return;
            }

            m_shouldRestartLoad = storedCredentialsPolicy != m_networkLoadChecker->storedCredentialsPolicy();
            this->continueWillSendRedirectedRequest(WTFMove(result->request), WTFMove(result->redirectRequest), WTFMove(result->redirectResponse), WTFMove(privateClickMeasurementAttributionTriggerData), WTFMove(completionHandler));
        };
        networkLoadChecker->checkRedirection(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), this, WTFMove(continueAfterRedirectionCheck));
        return;
    }
    continueWillSendRedirectedRequest(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), WTFMove(privateClickMeasurementAttributionTriggerData), WTFMove(completionHandler));
}

void NetworkResourceLoader::continueWillSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse, std::optional<WebCore::PCM::AttributionTriggerData>&& privateClickMeasurementAttributionTriggerData, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    redirectRequest.setIsAppInitiated(request.isAppInitiated());

    LOADER_RELEASE_LOG("continueWillSendRedirectedRequest: (m_isKeptAlive=%d, hasAdClickConversion=%d)", m_isKeptAlive, !!privateClickMeasurementAttributionTriggerData);
    ASSERT(!isSynchronous());

    if (privateClickMeasurementAttributionTriggerData) {
        if (CheckedPtr networkSession = protectedConnectionToWebProcess()->networkProcess().networkSession(sessionID())) {
            RefPtr networkLoad = m_networkLoad;
            auto attributedBundleIdentifier = networkLoad ? networkLoad->attributedBundleIdentifier(webPageProxyID()) : String();
            networkSession->handlePrivateClickMeasurementConversion(WTFMove(*privateClickMeasurementAttributionTriggerData), request.url(), redirectRequest, WTFMove(attributedBundleIdentifier));
        }
    }

    if (m_isKeptAlive) {
        continueWillSendRequest(WTFMove(redirectRequest), false, WTFMove(completionHandler));
        return;
    }

    // We send the request body separately because the ResourceRequest body normally does not get encoded when sent over IPC, as an optimization.
    // However, we really need the body here because a redirect cross-site may cause a process-swap and the request to start again in a new WebContent process.
    sendWithAsyncReply(Messages::WebResourceLoader::WillSendRequest(redirectRequest, IPC::FormDataReference { redirectRequest.httpBody() }, sanitizeResponseIfPossible(WTFMove(redirectResponse), ResourceResponse::SanitizationType::Redirection)), [weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& newRequest, bool isAllowedToAskUserForCredentials) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler({ });
        protectedThis->continueWillSendRequest(WTFMove(newRequest), isAllowedToAskUserForCredentials, WTFMove(completionHandler));
    });
}

void NetworkResourceLoader::didFinishWithRedirectResponse(WebCore::ResourceRequest&& request, WebCore::ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    LOADER_RELEASE_LOG("didFinishWithRedirectResponse:");
    redirectResponse.setType(ResourceResponse::Type::Opaqueredirect);
    if (!isCrossOriginPrefetch())
        didReceiveResponse(WTFMove(redirectResponse), PrivateRelayed::No, [] (auto) { });
    else if (CheckedPtr session = protectedConnectionToWebProcess()->networkProcess().networkSession(sessionID()))
        session->checkedPrefetchCache()->storeRedirect(request.url(), WTFMove(redirectResponse), WTFMove(redirectRequest));

    WebCore::NetworkLoadMetrics networkLoadMetrics;
    networkLoadMetrics.markComplete();
    networkLoadMetrics.responseBodyBytesReceived = 0;
    networkLoadMetrics.responseBodyDecodedSize = 0;

    if (m_serviceWorkerFetchTask)
        networkLoadMetrics.fetchStart = protectedServiceWorkerFetchTask()->startTime();
    send(Messages::WebResourceLoader::DidFinishResourceLoad { networkLoadMetrics });

    cleanup(LoadResult::Success);
}

static bool shouldSanitizeResponse(const NetworkProcess& process, std::optional<PageIdentifier> pageIdentifier, const FetchOptions& options, const URL& url)
{
    if (!pageIdentifier || options.destination != FetchOptions::Destination::EmptyString || options.mode != FetchOptions::Mode::NoCors)
        return true;
    return !process.shouldDisableCORSForRequestTo(*pageIdentifier, url);
}

ResourceResponse NetworkResourceLoader::sanitizeResponseIfPossible(ResourceResponse&& response, ResourceResponse::SanitizationType type)
{
    if (!m_parameters.shouldRestrictHTTPResponseAccess)
        return WTFMove(response);

    if (shouldSanitizeResponse(Ref { m_connection->networkProcess() }.get(), pageID(), parameters().options, originalRequest().url()))
        response.sanitizeHTTPHeaderFields(type);

    return WTFMove(response);
}

void NetworkResourceLoader::restartNetworkLoad(WebCore::ResourceRequest&& newRequest, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    LOADER_RELEASE_LOG("restartNetworkLoad: (hasNetworkLoad=%d)", !!m_networkLoad);

    if (RefPtr networkLoad = m_networkLoad) {
        LOADER_RELEASE_LOG("restartNetworkLoad: Cancelling existing network load so we can restart the load.");
        networkLoad->cancel();
        networkLoad->clearClient();
        m_networkLoad = nullptr;
    }

    completionHandler({ });

    if (!newRequest.isEmpty())
        startNetworkLoad(WTFMove(newRequest), FirstLoad::No);
}

static bool shouldTryToMatchRegistrationOnRedirection(const FetchOptions& options, bool isServiceWorkerLoaded)
{
    if (options.mode == FetchOptions::Mode::Navigate)
        return true;
    return isServiceWorkerLoaded && (options.destination == FetchOptions::Destination::Worker || options.destination == FetchOptions::Destination::Sharedworker);
}

void NetworkResourceLoader::continueWillSendRequest(ResourceRequest&& newRequest, bool isAllowedToAskUserForCredentials, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler)
{
    LOADER_RELEASE_LOG("continueWillSendRequest: (isAllowedToAskUserForCredentials=%d)", isAllowedToAskUserForCredentials);

    if (m_redirectionForCurrentNavigation) {
        LOADER_RELEASE_LOG("continueWillSendRequest: using stored redirect response");
        auto redirection = std::exchange(m_redirectionForCurrentNavigation, { });
        auto redirectRequest = newRequest.redirectedRequest(*redirection, parameters().shouldClearReferrerOnHTTPSToHTTPRedirect);
        m_shouldRestartLoad = true;
        willSendRedirectedRequest(WTFMove(newRequest), WTFMove(redirectRequest), WTFMove(*redirection), WTFMove(completionHandler));
        return;
    }

    if (shouldTryToMatchRegistrationOnRedirection(parameters().options, !!m_serviceWorkerFetchTask)) {
        m_serviceWorkerRegistration = { };
        setWorkerStart({ });
        if (auto serviceWorkerFetchTask = protectedConnectionToWebProcess()->createFetchTask(*this, newRequest)) {
            LOADER_RELEASE_LOG("continueWillSendRequest: Created a ServiceWorkerFetchTask to handle the redirect (fetchIdentifier=%" PRIu64 ")", serviceWorkerFetchTask->fetchIdentifier().toUInt64());
            if (RefPtr networkLoad = std::exchange(m_networkLoad, nullptr))
                networkLoad->clearClient();
            m_serviceWorkerFetchTask = WTFMove(serviceWorkerFetchTask);
            return completionHandler({ });
        }
        LOADER_RELEASE_LOG("continueWillSendRequest: Navigation is not using service workers");
        m_shouldRestartLoad = !!m_serviceWorkerFetchTask;
        m_serviceWorkerFetchTask = nullptr;
    }
    if (m_serviceWorkerFetchTask) {
        LOADER_RELEASE_LOG("continueWillSendRequest: Continuing fetch task with redirect (fetchIdentifier=%" PRIu64 ")", m_serviceWorkerFetchTask->fetchIdentifier().toUInt64());
        protectedServiceWorkerFetchTask()->continueFetchTaskWith(WTFMove(newRequest));
        return completionHandler({ });
    }

    if (m_shouldRestartLoad) {
        m_shouldRestartLoad = false;

        if (RefPtr networkLoad = m_networkLoad)
            networkLoad->updateRequestAfterRedirection(newRequest);

        LOADER_RELEASE_LOG("continueWillSendRequest: Restarting network load");
        restartNetworkLoad(WTFMove(newRequest), WTFMove(completionHandler));
        return;
    }

    if (m_networkLoadChecker) {
        // FIXME: We should be doing this check when receiving the redirection and not allow about protocol as per fetch spec.
        if (!newRequest.url().protocolIsInHTTPFamily() && !newRequest.url().protocolIsAbout() && m_redirectCount) {
            LOADER_RELEASE_LOG_ERROR("continueWillSendRequest: Failing load because it redirected to a scheme that is not HTTP(S)");
            didFailLoading(ResourceError { String { }, 0, newRequest.url(), "Redirection to URL with a scheme that is not HTTP(S)"_s, ResourceError::Type::AccessControl });
            return completionHandler({ });
        }
    }

    m_isAllowedToAskUserForCredentials = isAllowedToAskUserForCredentials;

    // If there is a match in the network cache, we need to reuse the original cache policy and partition.
    newRequest.setCachePolicy(originalRequest().cachePolicy());
    newRequest.setCachePartition(originalRequest().cachePartition());

    if (m_isWaitingContinueWillSendRequestForCachedRedirect) {
        m_isWaitingContinueWillSendRequestForCachedRedirect = false;

        LOG(NetworkCache, "(NetworkProcess) Retrieving cached redirect");
        LOADER_RELEASE_LOG("continueWillSendRequest: m_isWaitingContinueWillSendRequestForCachedRedirect was set");

        if (canUseCachedRedirect(newRequest))
            retrieveCacheEntry(newRequest);
        else
            startNetworkLoad(WTFMove(newRequest), FirstLoad::Yes);
        return completionHandler({ });
    }

    if (m_networkLoad) {
        LOADER_RELEASE_LOG("continueWillSendRequest: Telling NetworkLoad to proceed with the redirect");

        if (shouldSendResourceLoadMessages() && !newRequest.isNull())
            protectedConnectionToWebProcess()->networkProcess().protectedParentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidPerformHTTPRedirection(webPageProxyID(), resourceLoadInfo(), m_redirectResponse, newRequest), 0);

        completionHandler(WTFMove(newRequest));
    } else
        completionHandler({ });
}

void NetworkResourceLoader::continueDidReceiveResponse()
{
    LOADER_RELEASE_LOG("continueDidReceiveResponse: (hasCacheEntryWaitingForContinueDidReceiveResponse=%d, hasResponseCompletionHandler=%d)", !!m_cacheEntryWaitingForContinueDidReceiveResponse, !!m_responseCompletionHandler);
    if (m_serviceWorkerFetchTask) {
        LOADER_RELEASE_LOG("continueDidReceiveResponse: continuing with ServiceWorkerFetchTask (fetchIdentifier=%" PRIu64 ")", m_serviceWorkerFetchTask->fetchIdentifier().toUInt64());
        protectedServiceWorkerFetchTask()->continueDidReceiveFetchResponse();
        return;
    }

    if (m_cacheEntryWaitingForContinueDidReceiveResponse) {
        sendResultForCacheEntry(WTFMove(m_cacheEntryWaitingForContinueDidReceiveResponse));
        cleanup(LoadResult::Success);
        return;
    }

    if (m_responseCompletionHandler)
        m_responseCompletionHandler(PolicyAction::Use);
}

void NetworkResourceLoader::didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    if (!isSynchronous())
        send(Messages::WebResourceLoader::DidSendData(bytesSent, totalBytesToBeSent));
}

void NetworkResourceLoader::startBufferingTimerIfNeeded()
{
    if (isSynchronous())
        return;
    if (m_bufferingTimer.isActive())
        return;
    m_bufferingTimer.startOneShot(m_parameters.maximumBufferingTime);
}

void NetworkResourceLoader::bufferingTimerFired()
{
    ASSERT(m_bufferedData);
    ASSERT(m_networkLoad);

    if (m_bufferedData.isEmpty())
        return;

#if ENABLE(CONTENT_FILTERING)
    auto sharedBuffer = m_bufferedData.takeAsContiguous();
    bool shouldFilter = m_contentFilter && !checkedContentFilter()->continueAfterDataReceived(sharedBuffer);
    if (!shouldFilter)
        sendDidReceiveDataMessage(sharedBuffer);
#else
    sendDidReceiveDataMessage(m_bufferedData.takeAsContiguous());
#endif
    m_bufferedData.empty();
}

void NetworkResourceLoader::sendBuffer(const FragmentedSharedBuffer& buffer)
{
    ASSERT(!isSynchronous());

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !checkedContentFilter()->continueAfterDataReceived(buffer.makeContiguous()))
        return;
#endif

    sendDidReceiveDataMessage(buffer);
}

void NetworkResourceLoader::tryStoreAsCacheEntry()
{
    if (!canUseCache(m_networkLoad->currentRequest())) {
        LOADER_RELEASE_LOG("tryStoreAsCacheEntry: Not storing cache entry because request is not eligible");
        return;
    }
    if (!m_bufferedDataForCache) {
        LOADER_RELEASE_LOG("tryStoreAsCacheEntry: Not storing cache entry because m_bufferedDataForCache is null");
        return;
    }

    if (isCrossOriginPrefetch()) {
        if (CheckedPtr session = protectedConnectionToWebProcess()->networkProcess().networkSession(sessionID())) {
            LOADER_RELEASE_LOG("tryStoreAsCacheEntry: Storing entry in prefetch cache");
            session->checkedPrefetchCache()->store(m_networkLoad->currentRequest().url(), WTFMove(m_response), m_privateRelayed, m_bufferedDataForCache.take());
        }
        return;
    }
    LOADER_RELEASE_LOG("tryStoreAsCacheEntry: Storing entry in HTTP disk cache");
    protectedCache()->store(m_networkLoad->currentRequest(), m_response, m_privateRelayed, m_bufferedDataForCache.take(), [loader = Ref { *this }](auto&& mappedBody) mutable {
#if ENABLE(SHAREABLE_RESOURCE)
        if (!mappedBody.shareableResourceHandle)
            return;
        LOG(NetworkCache, "(NetworkProcess) sending DidCacheResource");
        loader->send(Messages::NetworkProcessConnection::DidCacheResource(loader->originalRequest(), WTFMove(*mappedBody.shareableResourceHandle)));
#endif
    });
}

void NetworkResourceLoader::didReceiveMainResourceResponse(const WebCore::ResourceResponse& response)
{
    LOADER_RELEASE_LOG("didReceiveMainResourceResponse:");
    if (CheckedPtr speculativeLoadManager = m_cache ? m_cache->speculativeLoadManager() : nullptr)
        speculativeLoadManager->registerMainResourceLoadResponse(globalFrameID(), originalRequest(), response);
}

void NetworkResourceLoader::initializeReportingEndpoints(const ResourceResponse& response)
{
    auto reportingEndpoints = response.httpHeaderField(HTTPHeaderName::ReportingEndpoints);
    if (!reportingEndpoints.isEmpty())
        m_reportingEndpoints = ReportingScope::parseReportingEndpointsFromHeader(reportingEndpoints, response.url());
}

void NetworkResourceLoader::didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    LOADER_RELEASE_LOG("didRetrieveCacheEntry:");
    auto response = entry->response();

#if ENABLE(CONTENT_FILTERING)
    CheckedPtr contentFilter = m_contentFilter.get();
    if (contentFilter && !contentFilter->responseReceived() && !contentFilter->continueAfterResponseReceived(response))
        return;
#endif

    if (isMainResource())
        didReceiveMainResourceResponse(response);

    initializeReportingEndpoints(response);

    if (isMainResource() && shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(response)) {
        LOADER_RELEASE_LOG_ERROR("didRetrieveCacheEntry: Stopping load due to CSP Frame-Ancestors or X-Frame-Options");
        response = sanitizeResponseIfPossible(WTFMove(response), ResourceResponse::SanitizationType::CrossOriginSafe);
        send(Messages::WebResourceLoader::StopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied { response });
        return;
    }
    if (RefPtr networkLoadChecker = m_networkLoadChecker) {
        auto error = networkLoadChecker->validateResponse(originalRequest(), response);
        if (!error.isNull()) {
            LOADER_RELEASE_LOG_ERROR("didRetrieveCacheEntry: Failing load due to NetworkLoadChecker::validateResponse");
            didFailLoading(error);
            return;
        }
    }

    if (auto error = doCrossOriginOpenerHandlingOfResponse(response)) {
        LOADER_RELEASE_LOG_ERROR("didRetrieveCacheEntry: Interrupting load due to Cross-Origin-Opener-Policy");
        didFailLoading(*error);
        return;
    }

    response = sanitizeResponseIfPossible(WTFMove(response), ResourceResponse::SanitizationType::CrossOriginSafe);
    if (isSynchronous()) {
        m_synchronousLoadData->response = WTFMove(response);
        sendReplyToSynchronousRequest(*m_synchronousLoadData, entry->protectedBuffer().get(), { });
        cleanup(LoadResult::Success);
        return;
    }

    bool needsContinueDidReceiveResponseMessage = isMainResource();
    LOADER_RELEASE_LOG("didRetrieveCacheEntry: Sending WebResourceLoader::DidReceiveResponse IPC (needsContinueDidReceiveResponseMessage=%d)", needsContinueDidReceiveResponseMessage);
    sendDidReceiveResponsePotentiallyInNewBrowsingContextGroup(response, entry->privateRelayed(), needsContinueDidReceiveResponseMessage);

    if (needsContinueDidReceiveResponseMessage) {
        m_response = WTFMove(response);
        m_privateRelayed = entry->privateRelayed();
        m_cacheEntryWaitingForContinueDidReceiveResponse = WTFMove(entry);
    } else {
        sendResultForCacheEntry(WTFMove(entry));
        cleanup(LoadResult::Success);
    }
}

void NetworkResourceLoader::sendResultForCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    auto dispatchDidFinishResourceLoad = [&] {
        NetworkLoadMetrics metrics;
        metrics.markComplete();
        if (shouldCaptureExtraNetworkLoadMetrics()) {
            auto additionalMetrics = WebCore::AdditionalNetworkLoadMetricsForWebInspector::create();
            additionalMetrics->requestHeaderBytesSent = 0;
            additionalMetrics->requestBodyBytesSent = 0;
            additionalMetrics->responseHeaderBytesReceived = 0;
            metrics.additionalNetworkLoadMetricsForWebInspector = WTFMove(additionalMetrics);
        }
        metrics.responseBodyBytesReceived = 0;
        metrics.responseBodyDecodedSize = 0;
        send(Messages::WebResourceLoader::DidFinishResourceLoad(WTFMove(metrics)));
    };

    LOADER_RELEASE_LOG("sendResultForCacheEntry:");

#if ENABLE(CONTENT_FILTERING)
    CheckedPtr contentFilter = m_contentFilter.get();
#endif

#if ENABLE(SHAREABLE_RESOURCE)
    if (auto handle = entry->shareableResourceHandle()) {
#if ENABLE(CONTENT_FILTERING)
        if (contentFilter && !contentFilter->continueAfterDataReceived(entry->protectedBuffer()->makeContiguous())) {
            contentFilter->continueAfterNotifyFinished(m_parameters.request.url());
            contentFilter->stopFilteringMainResource();
            dispatchDidFinishResourceLoad();
            return;
        }
#endif
        send(Messages::WebResourceLoader::DidReceiveResource(WTFMove(*handle)));
        return;
    }
#endif

#if !RELEASE_LOG_DISABLED
    if (shouldLogCookieInformation(m_connection, sessionID()))
        logCookieInformation();
#endif

    RefPtr buffer = entry->buffer();
    sendBuffer(*buffer);
#if ENABLE(CONTENT_FILTERING)
    if (contentFilter) {
        contentFilter->continueAfterNotifyFinished(m_parameters.request.url());
        contentFilter->stopFilteringMainResource();
    }
#endif
    dispatchDidFinishResourceLoad();
}

void NetworkResourceLoader::validateCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    LOADER_RELEASE_LOG("validateCacheEntry:");
    ASSERT(!m_networkLoad);

    // If the request is already conditional then the revalidation was not triggered by the disk cache
    // and we should not overwrite the existing conditional headers.
    ResourceRequest revalidationRequest = originalRequest();
    if (!revalidationRequest.isConditional()) {
        String eTag = entry->response().httpHeaderField(HTTPHeaderName::ETag);
        String lastModified = entry->response().httpHeaderField(HTTPHeaderName::LastModified);
        if (!eTag.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);
        if (!lastModified.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);
    }

    m_cacheEntryForValidation = WTFMove(entry);

    startNetworkLoad(WTFMove(revalidationRequest), FirstLoad::Yes);
}

void NetworkResourceLoader::dispatchWillSendRequestForCacheEntry(ResourceRequest&& request, std::unique_ptr<NetworkCache::Entry>&& entry)
{
    LOADER_RELEASE_LOG("dispatchWillSendRequestForCacheEntry:");
    ASSERT(entry->redirectRequest());
    ASSERT(!m_isWaitingContinueWillSendRequestForCachedRedirect);

    LOG(NetworkCache, "(NetworkProcess) Executing cached redirect");

    m_isWaitingContinueWillSendRequestForCachedRedirect = true;
    willSendRedirectedRequest(WTFMove(request), ResourceRequest { *entry->redirectRequest() }, ResourceResponse { entry->response() }, [](auto) { });
}

IPC::Connection* NetworkResourceLoader::messageSenderConnection() const
{
    return &connectionToWebProcess().connection();
}

void NetworkResourceLoader::consumeSandboxExtensionsIfNeeded()
{
    if (!m_didConsumeSandboxExtensions)
        consumeSandboxExtensions();
}

void NetworkResourceLoader::consumeSandboxExtensions()
{
    ASSERT(!m_didConsumeSandboxExtensions);

    for (auto& handle : std::exchange(m_parameters.requestBodySandboxExtensions, { })) {
        if (auto extension = SandboxExtension::create(WTFMove(handle))) {
            extension->consume();
            m_extensionsToRevoke.append(extension.releaseNonNull());
        }
    }

    if (auto handle = std::exchange(m_parameters.resourceSandboxExtension, { })) {
        if (auto extension = SandboxExtension::create(WTFMove(*handle))) {
            extension->consume();
            m_extensionsToRevoke.append(extension.releaseNonNull());
        }
    }

    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    m_didConsumeSandboxExtensions = true;
}

void NetworkResourceLoader::invalidateSandboxExtensions()
{
    if (m_didConsumeSandboxExtensions) {
        for (auto extension : std::exchange(m_extensionsToRevoke, { }))
            extension->revoke();

        for (auto& fileReference : m_fileReferences)
            fileReference->revokeFileAccess();

        m_didConsumeSandboxExtensions = false;
    }

    m_fileReferences.clear();
}

bool NetworkResourceLoader::shouldCaptureExtraNetworkLoadMetrics() const
{
    return m_shouldCaptureExtraNetworkLoadMetrics;
}

bool NetworkResourceLoader::crossOriginAccessControlCheckEnabled() const
{
    return m_parameters.crossOriginAccessControlCheckEnabled;
}

#if !RELEASE_LOG_DISABLED
bool NetworkResourceLoader::shouldLogCookieInformation(NetworkConnectionToWebProcess& connection, PAL::SessionID sessionID)
{
    if (CheckedPtr session = connection.networkProcess().networkSession(sessionID))
        return session->shouldLogCookieInformation();
    return false;
}

static String escapeForJSON(const String& s)
{
    return makeStringByReplacingAll(makeStringByReplacingAll(s, '\\', "\\\\"_s), '"', "\\\""_s);
}

template<typename IdentifierType, typename ThreadSafety, typename RawValue>
static String escapeIDForJSON(const std::optional<ObjectIdentifierGeneric<IdentifierType, ThreadSafety, RawValue>>& value)
{
    return value ? String::number(value->toUInt64()) : "None"_str;
}

template<typename IdentifierType, typename ThreadSafety, typename RawValue>
static String escapeIDForJSON(const std::optional<ProcessQualified<ObjectIdentifierGeneric<IdentifierType, ThreadSafety, RawValue>>>& value)
{
    return value ? String::number(value->object().toUInt64()) : "None"_str;
}

void NetworkResourceLoader::logCookieInformation() const
{
    ASSERT(shouldLogCookieInformation(m_connection, sessionID()));

    CheckedPtr networkStorageSession = protectedConnectionToWebProcess()->networkProcess().storageSession(sessionID());
    ASSERT(networkStorageSession);

    logCookieInformation(m_connection, "NetworkResourceLoader"_s, reinterpret_cast<const void*>(this), *networkStorageSession, originalRequest().firstPartyForCookies(), SameSiteInfo::create(originalRequest()), originalRequest().url(), originalRequest().httpReferrer(), frameID(), pageID(), coreIdentifier());
}

static void logBlockedCookieInformation(NetworkConnectionToWebProcess& connection, ASCIILiteral label, const void* loggedObject, const WebCore::NetworkStorageSession& networkStorageSession, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, std::optional<WebCore::ResourceLoaderIdentifier> identifier)
{
    ASSERT(NetworkResourceLoader::shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    auto escapedURL = escapeForJSON(url.string());
    auto escapedFirstParty = escapeForJSON(firstParty.string());
    auto escapedFrameID = escapeIDForJSON(frameID);
    auto escapedPageID = escapeIDForJSON(pageID);
    auto escapedIdentifier = escapeIDForJSON(identifier);
    auto escapedReferrer = escapeForJSON(referrer);

#define LOCAL_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(connection.isAlwaysOnLoggingAllowed(), Network, "%p - %s::" fmt, loggedObject, label.characters(), ##__VA_ARGS__)
#define LOCAL_LOG(str, ...) \
    LOCAL_LOG_IF_ALLOWED("logCookieInformation: BLOCKED cookie access for webPageID=%s, frameID=%s, resourceID=%s, firstParty=%s: " str, escapedPageID.utf8().data(), escapedFrameID.utf8().data(), escapedIdentifier.utf8().data(), escapedFirstParty.utf8().data(), ##__VA_ARGS__)

    LOCAL_LOG("{ \"url\": \"%" PUBLIC_LOG_STRING "\",", escapedURL.utf8().data());
    LOCAL_LOG("  \"partition\": \"%" PUBLIC_LOG_STRING "\",", "BLOCKED");
    LOCAL_LOG("  \"hasStorageAccess\": %" PUBLIC_LOG_STRING ",", "false");
    LOCAL_LOG("  \"referer\": \"%" PUBLIC_LOG_STRING "\",", escapedReferrer.utf8().data());
    LOCAL_LOG("  \"isSameSite\": \"%" PUBLIC_LOG_STRING "\",", sameSiteInfo.isSameSite ? "true" : "false");
    LOCAL_LOG("  \"isTopSite\": \"%" PUBLIC_LOG_STRING "\",", sameSiteInfo.isTopSite ? "true" : "false");
    LOCAL_LOG("  \"cookies\": []");
    LOCAL_LOG("  }");
#undef LOCAL_LOG
#undef LOCAL_LOG_IF_ALLOWED
}

static void logCookieInformationInternal(NetworkConnectionToWebProcess& connection, ASCIILiteral label, const void* loggedObject, const WebCore::NetworkStorageSession& networkStorageSession, const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, std::optional<WebCore::ResourceLoaderIdentifier> identifier)
{
    ASSERT(NetworkResourceLoader::shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    Vector<WebCore::Cookie> cookies;
    if (!networkStorageSession.getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::Yes, ShouldRelaxThirdPartyCookieBlocking::No, cookies))
        return;

    auto escapedURL = escapeForJSON(url.string());
    auto escapedPartition = escapeForJSON(emptyString());
    auto escapedReferrer = escapeForJSON(referrer);
    auto escapedFrameID = escapeIDForJSON(frameID);
    auto escapedPageID = escapeIDForJSON(pageID);
    auto escapedIdentifier = escapeIDForJSON(identifier);
    bool hasStorageAccess = (frameID && pageID) ? networkStorageSession.hasStorageAccess(WebCore::RegistrableDomain { url }, WebCore::RegistrableDomain { firstParty }, frameID.value(), pageID.value()) : false;

#define LOCAL_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(connection.isAlwaysOnLoggingAllowed(), Network, "%p - %s::" fmt, loggedObject, label.characters(), ##__VA_ARGS__)
#define LOCAL_LOG(str, ...) \
    LOCAL_LOG_IF_ALLOWED("logCookieInformation: webPageID=%s, frameID=%s, resourceID=%s: " str, escapedPageID.utf8().data(), escapedFrameID.utf8().data(), escapedIdentifier.utf8().data(), ##__VA_ARGS__)

    LOCAL_LOG("{ \"url\": \"%" PUBLIC_LOG_STRING "\",", escapedURL.utf8().data());
    LOCAL_LOG("  \"partition\": \"%" PUBLIC_LOG_STRING "\",", escapedPartition.utf8().data());
    LOCAL_LOG("  \"hasStorageAccess\": %" PUBLIC_LOG_STRING ",", hasStorageAccess ? "true" : "false");
    LOCAL_LOG("  \"referer\": \"%" PUBLIC_LOG_STRING "\",", escapedReferrer.utf8().data());
    LOCAL_LOG("  \"isSameSite\": \"%" PUBLIC_LOG_STRING "\",", sameSiteInfo.isSameSite ? "true" : "false");
    LOCAL_LOG("  \"isTopSite\": \"%" PUBLIC_LOG_STRING "\",", sameSiteInfo.isTopSite ? "true" : "false");
    LOCAL_LOG("  \"cookies\": [");

    auto size = cookies.size();
    decltype(size) count = 0;
    for (const auto& cookie : cookies) {
        auto trailingComma = ","_s;
        if (++count == size)
            trailingComma = ""_s;

        auto escapedName = escapeForJSON(cookie.name);
        auto escapedValue = escapeForJSON(cookie.value);
        auto escapedDomain = escapeForJSON(cookie.domain);
        auto escapedPath = escapeForJSON(cookie.path);
        auto escapedComment = escapeForJSON(cookie.comment);
        auto escapedCommentURL = escapeForJSON(cookie.commentURL.string());
        // FIXME: Log Same-Site policy for each cookie. See <https://bugs.webkit.org/show_bug.cgi?id=184894>.

        LOCAL_LOG("  { \"name\": \"%" PUBLIC_LOG_STRING "\",", escapedName.utf8().data());
        LOCAL_LOG("    \"value\": \"%" PUBLIC_LOG_STRING "\",", escapedValue.utf8().data());
        LOCAL_LOG("    \"domain\": \"%" PUBLIC_LOG_STRING "\",", escapedDomain.utf8().data());
        LOCAL_LOG("    \"path\": \"%" PUBLIC_LOG_STRING "\",", escapedPath.utf8().data());
        LOCAL_LOG("    \"created\": %f,", cookie.created);
        LOCAL_LOG("    \"expires\": %f,", cookie.expires.value_or(0));
        LOCAL_LOG("    \"httpOnly\": %" PUBLIC_LOG_STRING ",", cookie.httpOnly ? "true" : "false");
        LOCAL_LOG("    \"secure\": %" PUBLIC_LOG_STRING ",", cookie.secure ? "true" : "false");
        LOCAL_LOG("    \"session\": %" PUBLIC_LOG_STRING ",", cookie.session ? "true" : "false");
        LOCAL_LOG("    \"comment\": \"%" PUBLIC_LOG_STRING "\",", escapedComment.utf8().data());
        LOCAL_LOG("    \"commentURL\": \"%" PUBLIC_LOG_STRING "\"", escapedCommentURL.utf8().data());
        LOCAL_LOG("  }%" PUBLIC_LOG_STRING, trailingComma.characters());
    }
    LOCAL_LOG("]}");
#undef LOCAL_LOG
#undef LOCAL_LOG_IF_ALLOWED
}

void NetworkResourceLoader::logCookieInformation(NetworkConnectionToWebProcess& connection, ASCIILiteral label, const void* loggedObject, const NetworkStorageSession& networkStorageSession, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, std::optional<WebCore::ResourceLoaderIdentifier> identifier)
{
    ASSERT(shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    if (networkStorageSession.shouldBlockCookies(firstParty, url, frameID, pageID, ShouldRelaxThirdPartyCookieBlocking::No, IsKnownCrossSiteTracker::No))
        logBlockedCookieInformation(connection, label, loggedObject, networkStorageSession, firstParty, sameSiteInfo, url, referrer, frameID, pageID, identifier);
    else
        logCookieInformationInternal(connection, label, loggedObject, networkStorageSession, firstParty, sameSiteInfo, url, referrer, frameID, pageID, identifier);
}
#endif // !RELEASE_LOG_DISABLED

void NetworkResourceLoader::addConsoleMessage(MessageSource messageSource, MessageLevel messageLevel, const String& message, unsigned long)
{
    send(Messages::WebPage::AddConsoleMessage { frameID(),  messageSource, messageLevel, message, coreIdentifier() }, pageID());
}

void NetworkResourceLoader::enqueueSecurityPolicyViolationEvent(WebCore::SecurityPolicyViolationEventInit&& eventInit)
{
    send(Messages::WebPage::EnqueueSecurityPolicyViolationEvent { frameID(), WTFMove(eventInit) }, pageID());
}

void NetworkResourceLoader::logSlowCacheRetrieveIfNeeded(const NetworkCache::Cache::RetrieveInfo& info)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(info);
#else
    auto duration = info.completionTime - info.startTime;
    if (duration < 1_s)
        return;
    LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Took %.0fms, priority %d", duration.milliseconds(), info.priority);
    if (info.speculativeLoadDecision && *info.speculativeLoadDecision == NetworkCache::SpeculativeLoadDecision::Yes)
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Was speculative load");
    if (!info.storageTimings.startTime)
        return;
    LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Storage retrieve time %.0fms", (info.storageTimings.completionTime - info.storageTimings.startTime).milliseconds());
    if (info.storageTimings.dispatchTime) {
        auto time = (info.storageTimings.dispatchTime - info.storageTimings.startTime).milliseconds();
        auto count = info.storageTimings.dispatchCountAtDispatch - info.storageTimings.dispatchCountAtStart;
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Dispatch delay %.0fms, dispatched %lu resources first", time, count);
    }
    if (info.storageTimings.recordIOStartTime)
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Record I/O time %.0fms", (info.storageTimings.recordIOEndTime - info.storageTimings.recordIOStartTime).milliseconds());
    if (info.storageTimings.blobIOStartTime)
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Blob I/O time %.0fms", (info.storageTimings.blobIOEndTime - info.storageTimings.blobIOStartTime).milliseconds());
    if (info.storageTimings.synchronizationInProgressAtDispatch)
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Synchronization was in progress");
    if (info.storageTimings.shrinkInProgressAtDispatch)
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Shrink was in progress");
    if (info.storageTimings.wasCanceled)
        LOADER_RELEASE_LOG("logSlowCacheRetrieveIfNeeded: Retrieve was canceled");
#endif
}

bool NetworkResourceLoader::isCrossOriginPrefetch() const
{
    return parameters().isInitiatorPrefetch && !m_parameters.protectedSourceOrigin()->canRequest(originalRequest().url(), connectionToWebProcess().originAccessPatterns());
}

void NetworkResourceLoader::setWorkerStart(MonotonicTime value)
{
    m_workerStart = value;
    send(Messages::WebResourceLoader::SetWorkerStart { m_workerStart }, coreIdentifier());
}

void NetworkResourceLoader::startWithServiceWorker()
{
    LOADER_RELEASE_LOG("startWithServiceWorker:");

    CompletionHandler<void(ResourceRequest)> completionHandler = [protectedThis = Ref { *this }](ResourceRequest&& request) {
        ASSERT(RunLoop::isMain());

        if (request.isNull())
            return;

        ASSERT(!protectedThis->m_serviceWorkerFetchTask);
        protectedThis->m_serviceWorkerFetchTask = protectedThis->protectedConnectionToWebProcess()->createFetchTask(protectedThis, request);
        if (protectedThis->m_serviceWorkerFetchTask) {
            LOADER_RELEASE_LOG_WITH_THIS(protectedThis, "startWithServiceWorker: Created a ServiceWorkerFetchTask (fetchIdentifier=%" PRIu64 ")", protectedThis->m_serviceWorkerFetchTask->fetchIdentifier().toUInt64());
            return;
        }

        if (protectedThis->abortIfServiceWorkersOnly())
            return;

        protectedThis->startRequest(WTFMove(request));
    };

    ResourceRequest newRequest { originalRequest() };
#if ENABLE(CONTENT_FILTERING)
    startContentFiltering(WTFMove(newRequest), WTFMove(completionHandler));
#else
    completionHandler(WTFMove(newRequest));
#endif
}

bool NetworkResourceLoader::abortIfServiceWorkersOnly()
{
    if (m_parameters.serviceWorkersMode != ServiceWorkersMode::Only)
        return false;

    LOADER_RELEASE_LOG_ERROR("abortIfServiceWorkersOnly: Aborting load because the service worker did not handle the load and serviceWorkerMode only allows service workers");
    send(Messages::WebResourceLoader::ServiceWorkerDidNotHandle { }, coreIdentifier());
    abort();
    return true;
}

void NetworkResourceLoader::serviceWorkerDidNotHandle(ServiceWorkerFetchTask* fetchTask)
{
    LOADER_RELEASE_LOG("serviceWorkerDidNotHandle: (fetchIdentifier=%" PRIu64 ")", fetchTask ? fetchTask->fetchIdentifier().toUInt64() : 0);
    RELEASE_ASSERT(m_serviceWorkerFetchTask.get() == fetchTask);

    if (abortIfServiceWorkersOnly())
        return;

    if (RefPtr serviceWorkerFetchTask = m_serviceWorkerFetchTask) {
        auto newRequest = serviceWorkerFetchTask->takeRequest();
        m_serviceWorkerFetchTask = nullptr;

        if (RefPtr networkLoad = m_networkLoad)
            networkLoad->updateRequestAfterRedirection(newRequest);

        LOADER_RELEASE_LOG("serviceWorkerDidNotHandle: Restarting network load for redirect");
        restartNetworkLoad(WTFMove(newRequest), [] (auto) { });
        return;
    }
    start();
}

bool NetworkResourceLoader::isAppInitiated()
{
    return m_parameters.request.isAppInitiated();
}

WebCore::FrameIdentifier NetworkResourceLoader::frameIdentifierForReport() const
{
    // Reports are sent to the parent frame when they are for a main resource.
    if (isMainResource() && m_parameters.parentFrameID)
        return *m_parameters.parentFrameID;
    return frameID();
}

void NetworkResourceLoader::notifyReportObservers(Ref<Report>&& report)
{
    send(Messages::WebPage::NotifyReportObservers { frameIdentifierForReport(), report }, pageID());
}

String NetworkResourceLoader::endpointURIForToken(const String& reportTo) const
{
    return m_reportingEndpoints.get(reportTo);
}

void NetworkResourceLoader::sendReportToEndpoints(const URL& baseURL, std::span<const String> endpointURIs, std::span<const String> endpointTokens, Ref<FormData>&& report, WebCore::ViolationReportType reportType)
{
    Vector<String> updatedEndpointURIs = endpointURIs;
    Vector<String> updatedEndpointTokens;
    for (auto& token : endpointTokens) {
        if (auto url = endpointURIForToken(token); !url.isEmpty())
            updatedEndpointURIs.append(WTFMove(url));
        else
            updatedEndpointTokens.append(token);
    }

    send(Messages::WebPage::SendReportToEndpoints { frameIdentifierForReport(), baseURL, updatedEndpointURIs, updatedEndpointTokens, IPC::FormDataReference { WTFMove(report) }, reportType }, pageID());
}

#if ENABLE(CONTENT_FILTERING)
bool NetworkResourceLoader::continueAfterServiceWorkerReceivedData(const WebCore::SharedBuffer& buffer)
{
    if (!m_contentFilter)
        return true;
    return checkedContentFilter()->continueAfterDataReceived(buffer);
}

bool NetworkResourceLoader::continueAfterServiceWorkerReceivedResponse(const ResourceResponse& response)
{
    if (!m_contentFilter)
        return true;
    return checkedContentFilter()->continueAfterResponseReceived(response);
}

void NetworkResourceLoader::serviceWorkerDidFinish()
{
    CheckedPtr contentFilter = m_contentFilter.get();
    if (!contentFilter)
        return;
    contentFilter->continueAfterNotifyFinished(m_parameters.request.url());
    contentFilter->stopFilteringMainResource();
}

void NetworkResourceLoader::dataReceivedThroughContentFilter(const SharedBuffer& buffer)
{
    sendDidReceiveDataMessage(buffer);
}

WebCore::ResourceError NetworkResourceLoader::contentFilterDidBlock(WebCore::ContentFilterUnblockHandler&& unblockHandler, String&& unblockRequestDeniedScript)
{
    auto error = WebKit::blockedByContentFilterError(m_parameters.request);
    CheckedPtr contentFilter = m_contentFilter.get();

    m_unblockHandler = WTFMove(unblockHandler);
    m_unblockRequestDeniedScript = unblockRequestDeniedScript;
    
    if (m_unblockHandler.needsUIProcess()) {
        contentFilter->setBlockedError(error);
        contentFilter->handleProvisionalLoadFailure(error);
    } else {
        m_unblockHandler.requestUnblockAsync([this, protectedThis = Ref { *this }, contentFilter](bool unblocked) mutable {
            m_unblockHandler.setUnblockedAfterRequest(unblocked);

            ResourceRequest request;
            if (m_wasStarted || unblocked)
                request = m_parameters.request;
            else
                request = ResourceRequest(URL { aboutBlankURL() });
            auto error = WebKit::blockedByContentFilterError(request);
            contentFilter->setBlockedError(error);
            contentFilter->handleProvisionalLoadFailure(error);
        });
    }
    return error;
}

void NetworkResourceLoader::cancelMainResourceLoadForContentFilter(const WebCore::ResourceError& error)
{
    RELEASE_ASSERT(m_contentFilter);
}

void NetworkResourceLoader::handleProvisionalLoadFailureFromContentFilter(const URL& blockedPageURL, WebCore::SubstituteData&& substituteData)
{
    protectedConnectionToWebProcess()->networkProcess().addAllowedFirstPartyForCookies(m_connection->webProcessIdentifier(), RegistrableDomain { WebCore::ContentFilter::blockedPageURL() }, LoadedWebArchive::No, [] { });
    send(Messages::WebResourceLoader::ContentFilterDidBlockLoad(m_unblockHandler, m_unblockRequestDeniedScript, m_contentFilter->blockedError(), blockedPageURL, substituteData));
}

CheckedPtr<WebCore::ContentFilter> NetworkResourceLoader::checkedContentFilter()
{
    return m_contentFilter.get();
}

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
String NetworkResourceLoader::webContentRestrictionsConfigurationPath() const
{
    if (CheckedPtr session = protectedConnectionToWebProcess()->networkProcess().networkSession(sessionID()))
        return session->webContentRestrictionsConfigurationFile();

    return emptyString();
}
#endif
#endif // ENABLE(CONTENT_FILTERING)

void NetworkResourceLoader::useRedirectionForCurrentNavigation(WebCore::ResourceResponse&& response)
{
    LOADER_RELEASE_LOG("useRedirectionForCurrentNavigation");

    ASSERT(isMainFrameLoad());
    ASSERT(response.isRedirection());

    m_redirectionForCurrentNavigation = makeUnique<WebCore::ResourceResponse>(WTFMove(response));
}

void NetworkResourceLoader::sendDidReceiveDataMessage(const FragmentedSharedBuffer& buffer)
{
    RefPtr networkLoad = m_networkLoad;
    auto bytesTransferredOverNetwork = networkLoad ? networkLoad->bytesTransferredOverNetwork() : 0;

#if ENABLE(CONTENT_EXTENSIONS)
    updateBytesTransferredOverNetwork(bytesTransferredOverNetwork);
#endif

    send(Messages::WebResourceLoader::DidReceiveData(IPC::SharedBufferReference(buffer), bytesTransferredOverNetwork));
}

#if ENABLE(CONTENT_EXTENSIONS)
void NetworkResourceLoader::updateBytesTransferredOverNetwork(size_t bytesTransferredOverNetwork)
{
    CheckedSize delta = bytesTransferredOverNetwork - m_bytesTransferredOverNetwork;
    ASSERT(!delta.hasOverflowed());
    m_bytesTransferredOverNetwork = bytesTransferredOverNetwork;

    if (!delta)
        return;

    WTF::switchOn(m_parameters.workerIdentifier,
        [&] (std::monostate) { },
        [protectedThis = Ref { *this }, delta] (WebCore::SharedWorkerIdentifier& workerIdentifier) {
            protectedThis->reportNetworkUsageToAllSharedWorkerObjects(workerIdentifier, delta);
        },
        [protectedThis = Ref { *this }, delta] (WebCore::ServiceWorkerIdentifier& workerIdentifier) {
            protectedThis->reportNetworkUsageToAllServiceWorkerClients(workerIdentifier, delta);
        }
    );
}

void NetworkResourceLoader::reportNetworkUsageToAllSharedWorkerObjects(WebCore::SharedWorkerIdentifier identifier, size_t delta)
{
    ASSERT(delta);

    Ref connection = m_connection;
    if (CheckedPtr session = connection->networkProcess().networkSession(sessionID())) {
        if (CheckedPtr server = session->sharedWorkerServer())
            server->reportNetworkUsageToAllSharedWorkerObjects(identifier, delta);
    }
}

void NetworkResourceLoader::reportNetworkUsageToAllServiceWorkerClients(WebCore::ServiceWorkerIdentifier identifier, size_t delta)
{
    ASSERT(delta);

    Ref connection = m_connection;
    if (CheckedPtr session = connection->networkProcess().networkSession(sessionID())) {
        if (RefPtr server = session->swServer())
            server->reportNetworkUsageToAllWorkerClients(identifier, delta);
    }
}
#endif

} // namespace WebKit

#undef LOADER_RELEASE_LOG
#undef LOADER_RELEASE_LOG_ERROR
