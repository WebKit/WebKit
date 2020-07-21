/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkCache.h"
#include "NetworkCacheSpeculativeLoadManager.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkLoad.h"
#include "NetworkLoadChecker.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkSession.h"
#include "ResourceLoadInfo.h"
#include "ServiceWorkerFetchTask.h"
#include "SharedBufferDataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebPageMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWServerConnection.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Expected.h>
#include <wtf/RunLoop.h>

#if USE(QUICK_LOOK)
#include <WebCore/PreviewConverter.h>
#endif

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, this, m_parameters.webPageProxyID.toUInt64(), m_parameters.webPageID.toUInt64(), m_parameters.webFrameID.toUInt64(), m_parameters.identifier, isMainResource(), static_cast<unsigned>(m_parameters.options.destination), isSynchronous(), ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), Network, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 ", isMainResource=%d, destination=%u, isSynchronous=%d] NetworkResourceLoader::" fmt, this, m_parameters.webPageProxyID.toUInt64(), m_parameters.webPageID.toUInt64(), m_parameters.webFrameID.toUInt64(), m_parameters.identifier, isMainResource(), static_cast<unsigned>(m_parameters.options.destination), isSynchronous(), ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

struct NetworkResourceLoader::SynchronousLoadData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    SynchronousLoadData(Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply&& reply)
        : delayedReply(WTFMove(reply))
    {
        ASSERT(delayedReply);
    }
    ResourceRequest currentRequest;
    Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply delayedReply;
    ResourceResponse response;
    ResourceError error;
};

static void sendReplyToSynchronousRequest(NetworkResourceLoader::SynchronousLoadData& data, const SharedBuffer* buffer)
{
    ASSERT(data.delayedReply);
    ASSERT(!data.response.isNull() || !data.error.isNull());

    Vector<char> responseBuffer;
    if (buffer && buffer->size())
        responseBuffer.append(buffer->data(), buffer->size());

    data.delayedReply(data.error, data.response, responseBuffer);
    data.delayedReply = nullptr;
}

NetworkResourceLoader::NetworkResourceLoader(NetworkResourceLoadParameters&& parameters, NetworkConnectionToWebProcess& connection, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::DelayedReply&& synchronousReply)
    : m_parameters { WTFMove(parameters) }
    , m_connection { connection }
    , m_fileReferences(connection.resolveBlobReferences(m_parameters))
    , m_isAllowedToAskUserForCredentials { m_parameters.clientCredentialPolicy == ClientCredentialPolicy::MayAskClientForCredentials }
    , m_bufferingTimer { *this, &NetworkResourceLoader::bufferingTimerFired }
    , m_shouldCaptureExtraNetworkLoadMetrics(m_connection->captureExtraNetworkLoadMetricsEnabled())
    , m_resourceLoadID { NetworkResourceLoadIdentifier::generate() }
{
    ASSERT(RunLoop::isMain());

    if (auto* session = connection.networkProcess().networkSession(sessionID()))
        m_cache = session->cache();

    // FIXME: This is necessary because of the existence of EmptyFrameLoaderClient in WebCore.
    //        Once bug 116233 is resolved, this ASSERT can just be "m_webPageID && m_webFrameID"
    ASSERT((m_parameters.webPageID && m_parameters.webFrameID) || m_parameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials);

    if (synchronousReply || parameters.shouldRestrictHTTPResponseAccess || parameters.options.keepAlive) {
        NetworkLoadChecker::LoadType requestLoadType = isMainFrameLoad() ? NetworkLoadChecker::LoadType::MainFrame : NetworkLoadChecker::LoadType::Other;
        m_networkLoadChecker = makeUnique<NetworkLoadChecker>(connection.networkProcess(), this,  &connection.schemeRegistry(), FetchOptions { m_parameters.options }, sessionID(), m_parameters.webPageProxyID, HTTPHeaderMap { m_parameters.originalRequestHeaders }, URL { m_parameters.request.url() }, m_parameters.sourceOrigin.copyRef(), m_parameters.topOrigin.copyRef(), m_parameters.preflightPolicy, originalRequest().httpReferrer(), m_parameters.isHTTPSUpgradeEnabled, shouldCaptureExtraNetworkLoadMetrics(), requestLoadType);
        if (m_parameters.cspResponseHeaders)
            m_networkLoadChecker->setCSPResponseHeaders(ContentSecurityPolicyResponseHeaders { m_parameters.cspResponseHeaders.value() });
#if ENABLE(CONTENT_EXTENSIONS)
        m_networkLoadChecker->setContentExtensionController(URL { m_parameters.mainDocumentURL }, m_parameters.userContentControllerIdentifier);
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
    ASSERT(RunLoop::isMain());
    RELEASE_LOG_IF_ALLOWED("start: hasNetworkLoadChecker=%d", !!m_networkLoadChecker);

    m_networkActivityTracker = m_connection->startTrackingResourceLoad(m_parameters.webPageID, m_parameters.identifier, isMainFrameLoad());

    ASSERT(!m_wasStarted);
    m_wasStarted = true;

    if (m_networkLoadChecker) {
        m_networkLoadChecker->check(ResourceRequest { originalRequest() }, this, [this, weakThis = makeWeakPtr(*this)] (auto&& result) {
            if (!weakThis)
                return;

            WTF::switchOn(result,
                [this] (ResourceError& error) {
                    RELEASE_LOG_IF_ALLOWED("start: NetworkLoadChecker::check returned an error (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d, isCancellation=%d)", error.domain().utf8().data(), error.errorCode(), error.isCancellation());
                    if (!error.isCancellation())
                        this->didFailLoading(error);
                },
                [this] (NetworkLoadChecker::RedirectionTriplet& triplet) {
                    RELEASE_LOG_IF_ALLOWED("start: NetworkLoadChecker::check returned a synthetic redirect");
                    this->m_isWaitingContinueWillSendRequestForCachedRedirect = true;
                    this->willSendRedirectedRequest(WTFMove(triplet.request), WTFMove(triplet.redirectRequest), WTFMove(triplet.redirectResponse));
                },
                [this] (ResourceRequest& request) {
                    RELEASE_LOG_IF_ALLOWED("start: NetworkLoadChecker::check is done");
                    if (this->canUseCache(request)) {
                        this->retrieveCacheEntry(request);
                        return;
                    }

                    this->startNetworkLoad(WTFMove(request), FirstLoad::Yes);
                }
            );
        });
        return;
    }
    // FIXME: Remove that code path once m_networkLoadChecker is used for all network loads.
    if (canUseCache(originalRequest())) {
        retrieveCacheEntry(originalRequest());
        return;
    }

    startNetworkLoad(ResourceRequest { originalRequest() }, FirstLoad::Yes);
}

void NetworkResourceLoader::retrieveCacheEntry(const ResourceRequest& request)
{
    RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: isMainFrameLoad=%d", isMainFrameLoad());
    ASSERT(canUseCache(request));

    auto protectedThis = makeRef(*this);
    if (isMainFrameLoad()) {
        ASSERT(m_parameters.options.mode == FetchOptions::Mode::Navigate);
        if (auto* session = m_connection->networkProcess().networkSession(sessionID())) {
            if (auto entry = session->prefetchCache().take(request.url())) {
                RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: retrieved an entry from the prefetch cache (isRedirect=%d)", !entry->redirectRequest.isNull());
                if (!entry->redirectRequest.isNull()) {
                    auto cacheEntry = m_cache->makeRedirectEntry(request, entry->response, entry->redirectRequest);
                    retrieveCacheEntryInternal(WTFMove(cacheEntry), ResourceRequest { request });
                    auto maxAgeCap = validateCacheEntryForMaxAgeCapValidation(request, entry->redirectRequest, entry->response);
                    m_cache->storeRedirect(request, entry->response, entry->redirectRequest, maxAgeCap);
                    return;
                }
                auto buffer = entry->releaseBuffer();
                auto cacheEntry = m_cache->makeEntry(request, entry->response, buffer.copyRef());
                retrieveCacheEntryInternal(WTFMove(cacheEntry), ResourceRequest { request });
                m_cache->store(request, entry->response, WTFMove(buffer));
                return;
            }
        }
    }

    RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Checking the HTTP disk cache");
    m_cache->retrieve(request, globalFrameID(), m_parameters.isNavigatingToAppBoundDomain, [this, weakThis = makeWeakPtr(*this), request = ResourceRequest { request }](auto entry, auto info) mutable {
        if (!weakThis)
            return;

        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Done checking the HTTP disk cache (foundCachedEntry=%d)", !!entry);
        logSlowCacheRetrieveIfNeeded(info);

        if (!entry) {
            startNetworkLoad(WTFMove(request), FirstLoad::Yes);
            return;
        }
        retrieveCacheEntryInternal(WTFMove(entry), WTFMove(request));
    });
}

void NetworkResourceLoader::retrieveCacheEntryInternal(std::unique_ptr<NetworkCache::Entry>&& entry, WebCore::ResourceRequest&& request)
{
    RELEASE_LOG_IF_ALLOWED("retrieveCacheEntryInternal:");
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (entry->hasReachedPrevalentResourceAgeCap()) {
        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntryInternal: Revalidating cached entry because it reached the prevalent resource age cap");
        m_cacheEntryForMaxAgeCapValidation = WTFMove(entry);
        ResourceRequest revalidationRequest = originalRequest();
        startNetworkLoad(WTFMove(revalidationRequest), FirstLoad::Yes);
        return;
    }
#endif
    if (entry->redirectRequest()) {
        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntryInternal: Cached entry is a redirect");
        dispatchWillSendRequestForCacheEntry(WTFMove(request), WTFMove(entry));
        return;
    }
    if (m_parameters.needsCertificateInfo && !entry->response().certificateInfo()) {
        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntryInternal: Cached entry is missing certificate information so we are not using it");
        startNetworkLoad(WTFMove(request), FirstLoad::Yes);
        return;
    }
    if (entry->needsValidation() || request.cachePolicy() == WebCore::ResourceRequestCachePolicy::RefreshAnyCacheData) {
        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntryInternal: Cached entry needs revalidation");
        validateCacheEntry(WTFMove(entry));
        return;
    }
    RELEASE_LOG_IF_ALLOWED("retrieveCacheEntryInternal: Cached entry is directly usable");
    didRetrieveCacheEntry(WTFMove(entry));
}

void NetworkResourceLoader::startNetworkLoad(ResourceRequest&& request, FirstLoad load)
{
    RELEASE_LOG_IF_ALLOWED("startNetworkLoad: (isFirstLoad=%d, timeout=%f)", load == FirstLoad::Yes, request.timeoutInterval());
    if (load == FirstLoad::Yes) {
        consumeSandboxExtensions();

        if (isSynchronous() || m_parameters.maximumBufferingTime > 0_s)
            m_bufferedData = SharedBuffer::create();

        if (canUseCache(request))
            m_bufferedDataForCache = SharedBuffer::create();
    }

    NetworkLoadParameters parameters = m_parameters;
    parameters.networkActivityTracker = m_networkActivityTracker;
    if (parameters.storedCredentialsPolicy == WebCore::StoredCredentialsPolicy::Use && m_networkLoadChecker)
        parameters.storedCredentialsPolicy = m_networkLoadChecker->storedCredentialsPolicy();

    auto* networkSession = m_connection->networkSession();
    if (!networkSession) {
        WTFLogAlways("Attempted to create a NetworkLoad with a session (id=%" PRIu64 ") that does not exist.", sessionID().toUInt64());
        RELEASE_LOG_ERROR_IF_ALLOWED("startNetworkLoad: Attempted to create a NetworkLoad for a session that does not exist (sessionID=%" PRIu64 ")", sessionID().toUInt64());
        m_connection->networkProcess().logDiagnosticMessage(m_parameters.webPageProxyID, WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::invalidSessionIDKey(), WebCore::ShouldSample::No);
        didFailLoading(internalError(request.url()));
        return;
    }

    if (request.url().protocolIsBlob())
        parameters.blobFileReferences = networkSession->blobRegistry().filesInBlob(originalRequest().url());

    if (m_parameters.pageHasResourceLoadClient) {
        Optional<IPC::FormDataReference> httpBody;
        if (auto* formData = request.httpBody()) {
            static constexpr auto maxSerializedRequestSize = 1024 * 1024;
            if (formData->lengthInBytes() <= maxSerializedRequestSize)
                httpBody = IPC::FormDataReference { formData };
        }
        m_connection->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidSendRequest(m_parameters.webPageProxyID, resourceLoadInfo(), request, httpBody), 0);
    }

    parameters.request = WTFMove(request);
    parameters.isNavigatingToAppBoundDomain = m_parameters.isNavigatingToAppBoundDomain;
    m_networkLoad = makeUnique<NetworkLoad>(*this, &networkSession->blobRegistry(), WTFMove(parameters), *networkSession);
    m_networkLoad->start();

    RELEASE_LOG_IF_ALLOWED("startNetworkLoad: Going to the network (description=%" PUBLIC_LOG_STRING ")", m_networkLoad->description().utf8().data());
}

ResourceLoadInfo NetworkResourceLoader::resourceLoadInfo()
{
    auto loadedFromCache = [] (const ResourceResponse& response) {
        switch (response.source()) {
        case ResourceResponse::Source::DiskCache:
        case ResourceResponse::Source::DiskCacheAfterValidation:
        case ResourceResponse::Source::MemoryCache:
        case ResourceResponse::Source::MemoryCacheAfterValidation:
        case ResourceResponse::Source::ApplicationCache:
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

    auto resourceType = [] (WebCore::ResourceRequestBase::Requester requester, WebCore::FetchOptions::Destination destination) {
        switch (requester) {
        case WebCore::ResourceRequestBase::Requester::XHR:
            return ResourceLoadInfo::Type::XMLHTTPRequest;
        case WebCore::ResourceRequestBase::Requester::Fetch:
            return ResourceLoadInfo::Type::Fetch;
        case WebCore::ResourceRequestBase::Requester::Ping:
            return ResourceLoadInfo::Type::Ping;
        case WebCore::ResourceRequestBase::Requester::Beacon:
            return ResourceLoadInfo::Type::Beacon;
        default:
            break;
        }

        switch (destination) {
        case WebCore::FetchOptions::Destination::EmptyString:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Audio:
            return ResourceLoadInfo::Type::Media;
        case WebCore::FetchOptions::Destination::Document:
            return ResourceLoadInfo::Type::Document;
        case WebCore::FetchOptions::Destination::Embed:
            return ResourceLoadInfo::Type::Object;
        case WebCore::FetchOptions::Destination::Font:
            return ResourceLoadInfo::Type::Font;
        case WebCore::FetchOptions::Destination::Image:
            return ResourceLoadInfo::Type::Image;
        case WebCore::FetchOptions::Destination::Manifest:
            return ResourceLoadInfo::Type::ApplicationManifest;
        case WebCore::FetchOptions::Destination::Object:
            return ResourceLoadInfo::Type::Object;
        case WebCore::FetchOptions::Destination::Report:
            return ResourceLoadInfo::Type::CSPReport;
        case WebCore::FetchOptions::Destination::Script:
            return ResourceLoadInfo::Type::Script;
        case WebCore::FetchOptions::Destination::Serviceworker:
            return ResourceLoadInfo::Type::Other;
        case WebCore::FetchOptions::Destination::Sharedworker:
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

    return ResourceLoadInfo {
        m_resourceLoadID,
        m_parameters.webFrameID,
        m_parameters.parentFrameID,
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
    RELEASE_LOG_IF_ALLOWED("cleanup: (result=%u)", static_cast<unsigned>(result));

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

    m_connection->stopTrackingResourceLoad(m_parameters.identifier, code);

    m_bufferingTimer.stop();

    invalidateSandboxExtensions();

    m_networkLoad = nullptr;

    // This will cause NetworkResourceLoader to be destroyed and therefore we do it last.
    m_connection->didCleanupResourceLoader(*this);
}

void NetworkResourceLoader::convertToDownload(DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    RELEASE_LOG_IF_ALLOWED("convertToDownload: (downloadID=%" PRIu64 ", hasNetworkLoad=%d, hasResponseCompletionHandler=%d)", downloadID.downloadID(), !!m_networkLoad, !!m_responseCompletionHandler);

    // This can happen if the resource came from the disk cache.
    if (!m_networkLoad) {
        m_connection->networkProcess().downloadManager().startDownload(sessionID(), downloadID, request, m_parameters.isNavigatingToAppBoundDomain);
        abort();
        return;
    }

    if (m_responseCompletionHandler)
        m_connection->networkProcess().downloadManager().convertNetworkLoadToDownload(downloadID, std::exchange(m_networkLoad, nullptr), WTFMove(m_responseCompletionHandler), WTFMove(m_fileReferences), request, response);
}

void NetworkResourceLoader::abort()
{
    RELEASE_LOG_IF_ALLOWED("abort: (hasNetworkLoad=%d)", !!m_networkLoad);
    ASSERT(RunLoop::isMain());

    if (m_parameters.options.keepAlive && m_response.isNull() && !m_isKeptAlive) {
        m_isKeptAlive = true;
        RELEASE_LOG_IF_ALLOWED("abort: Keeping network load alive due to keepalive option");
        m_connection->transferKeptAliveLoad(*this);
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (auto task = WTFMove(m_serviceWorkerFetchTask)) {
        RELEASE_LOG_IF_ALLOWED("abort: Cancelling pending service worker fetch task (fetchIdentifier=%" PRIu64 ")", task->fetchIdentifier().toUInt64());
        task->cancelFromClient();
    }
#endif

    if (m_networkLoad) {
        if (canUseCache(m_networkLoad->currentRequest())) {
            // We might already have used data from this incomplete load. Ensure older versions don't remain in the cache after cancel.
            if (!m_response.isNull())
                m_cache->remove(m_networkLoad->currentRequest());
        }
        RELEASE_LOG_IF_ALLOWED("abort: Cancelling network load");
        m_networkLoad->cancel();
    }

    cleanup(LoadResult::Cancel);
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
        if (!origin->isSameSchemeHostPort(*topFrameOrigin))
            return true;
        for (auto& ancestorOrigin : m_parameters.frameAncestorOrigins) {
            if (!origin->isSameSchemeHostPort(*ancestorOrigin))
                return true;
        }
        return false;
    }
    case XFrameOptionsDisposition::Conflict: {
        String errorMessage = "Multiple 'X-Frame-Options' headers with conflicting values ('" + xFrameOptions + "') encountered when loading '" + url.stringCenterEllipsizedToLength() + "'. Falling back to 'DENY'.";
        send(Messages::WebPage::AddConsoleMessage { m_parameters.webFrameID,  MessageSource::JS, MessageLevel::Error, errorMessage, identifier() }, m_parameters.webPageID);
        return true;
    }
    case XFrameOptionsDisposition::Invalid: {
        String errorMessage = "Invalid 'X-Frame-Options' header encountered when loading '" + url.stringCenterEllipsizedToLength() + "': '" + xFrameOptions + "' is not a recognized directive. The header will be ignored.";
        send(Messages::WebPage::AddConsoleMessage { m_parameters.webFrameID,  MessageSource::JS, MessageLevel::Error, errorMessage, identifier() }, m_parameters.webPageID);
        return false;
    }
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool NetworkResourceLoader::shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(const ResourceResponse& response)
{
    ASSERT(isMainResource());

#if USE(QUICK_LOOK)
    if (PreviewConverter::supportsMIMEType(response.mimeType()))
        return false;
#endif

    auto url = response.url();
    ContentSecurityPolicy contentSecurityPolicy { URL { url }, this };
    contentSecurityPolicy.didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, originalRequest().httpReferrer());
    if (!contentSecurityPolicy.allowFrameAncestors(m_parameters.frameAncestorOrigins, url))
        return true;

    if (!contentSecurityPolicy.overridesXFrameOptions()) {
        String xFrameOptions = m_response.httpHeaderField(HTTPHeaderName::XFrameOptions);
        if (!xFrameOptions.isNull() && shouldInterruptLoadForXFrameOptions(xFrameOptions, response.url())) {
            String errorMessage = "Refused to display '" + response.url().stringCenterEllipsizedToLength() + "' in a frame because it set 'X-Frame-Options' to '" + xFrameOptions + "'.";
            send(Messages::WebPage::AddConsoleMessage { m_parameters.webFrameID,  MessageSource::Security, MessageLevel::Error, errorMessage, identifier() }, m_parameters.webPageID);
            return true;
        }
    }
    return false;
}

void NetworkResourceLoader::didReceiveResponse(ResourceResponse&& receivedResponse, ResponseCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: (httpStatusCode=%d, MIMEType=%" PUBLIC_LOG_STRING ", expectedContentLength=%" PRId64 ", hasCachedEntryForValidation=%d, hasNetworkLoadChecker=%d)", receivedResponse.httpStatusCode(), receivedResponse.mimeType().utf8().data(), receivedResponse.expectedContentLength(), !!m_cacheEntryForValidation, !!m_networkLoadChecker);

    if (isMainResource())
        didReceiveMainResourceResponse(receivedResponse);

    m_response = WTFMove(receivedResponse);

    if (shouldCaptureExtraNetworkLoadMetrics() && m_networkLoadChecker) {
        auto information = m_networkLoadChecker->takeNetworkLoadInformation();
        information.response = m_response;
        m_connection->addNetworkLoadInformation(identifier(), WTFMove(information));
    }

    // For multipart/x-mixed-replace didReceiveResponseAsync gets called multiple times and buffering would require special handling.
    if (!isSynchronous() && m_response.isMultipart())
        m_bufferedData = nullptr;

    if (m_response.isMultipart())
        m_bufferedDataForCache = nullptr;

    if (m_cacheEntryForValidation) {
        bool validationSucceeded = m_response.httpStatusCode() == 304; // 304 Not Modified
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Received revalidation response (validationSucceeded=%d, wasOriginalRequestConditional=%d)", validationSucceeded, originalRequest().isConditional());
        if (validationSucceeded) {
            m_cacheEntryForValidation = m_cache->update(originalRequest(), *m_cacheEntryForValidation, m_response);
            // If the request was conditional then this revalidation was not triggered by the network cache and we pass the 304 response to WebCore.
            if (originalRequest().isConditional())
                m_cacheEntryForValidation = nullptr;
        } else
            m_cacheEntryForValidation = nullptr;
    }
    if (m_cacheEntryForValidation)
        return completionHandler(PolicyAction::Use);

    if (isMainResource() && shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(m_response)) {
        RELEASE_LOG_ERROR_IF_ALLOWED("didReceiveResponse: Interrupting main resource load due to CSP frame-ancestors or X-Frame-Options");
        auto response = sanitizeResponseIfPossible(ResourceResponse { m_response }, ResourceResponse::SanitizationType::CrossOriginSafe);
        send(Messages::WebResourceLoader::StopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied { response });
        return completionHandler(PolicyAction::Ignore);
    }

    if (m_networkLoadChecker) {
        auto error = m_networkLoadChecker->validateResponse(m_networkLoad ? m_networkLoad->currentRequest() : originalRequest(), m_response);
        if (!error.isNull()) {
            RELEASE_LOG_ERROR_IF_ALLOWED("didReceiveResponse: NetworkLoadChecker::validateResponse returned an error (error.domain=%" PUBLIC_LOG_STRING ", error.code=%d)", error.domain().utf8().data(), error.errorCode());
            RunLoop::main().dispatch([protectedThis = makeRef(*this), error = WTFMove(error)] {
                if (protectedThis->m_networkLoad)
                    protectedThis->didFailLoading(error);
            });
            return completionHandler(PolicyAction::Ignore);
        }
    }

    auto response = sanitizeResponseIfPossible(ResourceResponse { m_response }, ResourceResponse::SanitizationType::CrossOriginSafe);
    if (isSynchronous()) {
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Using response for synchronous load");
        m_synchronousLoadData->response = WTFMove(response);
        return completionHandler(PolicyAction::Use);
    }

    if (isCrossOriginPrefetch()) {
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Using response for cross-origin prefetch");
        if (response.httpHeaderField(HTTPHeaderName::Vary).contains("Cookie")) {
            RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Canceling cross-origin prefetch for Vary: Cookie");
            abort();
            return completionHandler(PolicyAction::Ignore);
        }
        return completionHandler(PolicyAction::Use);
    }

    // We wait to receive message NetworkResourceLoader::ContinueDidReceiveResponse before continuing a load for
    // a main resource because the embedding client must decide whether to allow the load.
    bool willWaitForContinueDidReceiveResponse = isMainResource();
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Sending WebResourceLoader::DidReceiveResponse IPC (willWaitForContinueDidReceiveResponse=%d)", willWaitForContinueDidReceiveResponse);
    send(Messages::WebResourceLoader::DidReceiveResponse { response, willWaitForContinueDidReceiveResponse });

    if (m_parameters.pageHasResourceLoadClient)
        m_connection->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidReceiveResponse(m_parameters.webPageProxyID, resourceLoadInfo(), response), 0);

    if (willWaitForContinueDidReceiveResponse) {
        m_responseCompletionHandler = WTFMove(completionHandler);
        return;
    }

    if (m_isKeptAlive) {
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Ignoring response because of keepalive option");
        return completionHandler(PolicyAction::Ignore);
    }

    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: Using response");
    completionHandler(PolicyAction::Use);
}

void NetworkResourceLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    if (!m_numBytesReceived)
        RELEASE_LOG_IF_ALLOWED("didReceiveBuffer: Started receiving data (reportedEncodedDataLength=%d)", reportedEncodedDataLength);
    m_numBytesReceived += buffer->size();

    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data and limit size of entries in the cache.
        const size_t maximumCacheBufferSize = m_cache->capacity() / 8;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
    if (isCrossOriginPrefetch())
        return;
    // FIXME: At least on OS X Yosemite we always get -1 from the resource handle.
    unsigned encodedDataLength = reportedEncodedDataLength >= 0 ? reportedEncodedDataLength : buffer->size();

    if (m_bufferedData) {
        m_bufferedData->append(buffer.get());
        m_bufferedDataEncodedDataLength += encodedDataLength;
        startBufferingTimerIfNeeded();
        return;
    }
    sendBuffer(buffer, encodedDataLength);
}

void NetworkResourceLoader::didFinishLoading(const NetworkLoadMetrics& networkLoadMetrics)
{
    RELEASE_LOG_IF_ALLOWED("didFinishLoading: (numBytesReceived=%zd, hasCacheEntryForValidation=%d)", m_numBytesReceived, !!m_cacheEntryForValidation);

    if (shouldCaptureExtraNetworkLoadMetrics())
        m_connection->addNetworkLoadInformationMetrics(identifier(), networkLoadMetrics);

    if (m_cacheEntryForValidation) {
        // 304 Not Modified
        ASSERT(m_response.httpStatusCode() == 304);
        LOG(NetworkCache, "(NetworkProcess) revalidated");
        didRetrieveCacheEntry(WTFMove(m_cacheEntryForValidation));
        return;
    }

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (shouldLogCookieInformation(m_connection, sessionID()))
        logCookieInformation();
#endif

    if (isSynchronous())
        sendReplyToSynchronousRequest(*m_synchronousLoadData, m_bufferedData.get());
    else {
        if (m_bufferedData && !m_bufferedData->isEmpty()) {
            // FIXME: Pass a real value or remove the encoded data size feature.
            sendBuffer(*m_bufferedData, -1);
        }
        send(Messages::WebResourceLoader::DidFinishResourceLoad(networkLoadMetrics));
    }

    tryStoreAsCacheEntry();

    if (m_parameters.pageHasResourceLoadClient)
        m_connection->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidCompleteWithError(m_parameters.webPageProxyID, resourceLoadInfo(), m_response, { }), 0);

    cleanup(LoadResult::Success);
}

void NetworkResourceLoader::didFailLoading(const ResourceError& error)
{
    bool wasServiceWorkerLoad = false;
#if ENABLE(SERVICE_WORKER)
    wasServiceWorkerLoad = !!m_serviceWorkerFetchTask;
#endif
    RELEASE_LOG_ERROR_IF_ALLOWED("didFailLoading: (wasServiceWorkerLoad=%d, isTimeout=%d, isCancellation=%d, isAccessControl=%d, errorCode=%d)", wasServiceWorkerLoad, error.isTimeout(), error.isCancellation(), error.isAccessControl(), error.errorCode());
    UNUSED_VARIABLE(wasServiceWorkerLoad);

    if (shouldCaptureExtraNetworkLoadMetrics())
        m_connection->removeNetworkLoadInformation(identifier());

    ASSERT(!error.isNull());

    m_cacheEntryForValidation = nullptr;

    if (isSynchronous()) {
        m_synchronousLoadData->error = error;
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr);
    } else if (auto* connection = messageSenderConnection()) {
#if ENABLE(SERVICE_WORKER)
        if (m_serviceWorkerFetchTask)
            connection->send(Messages::WebResourceLoader::DidFailServiceWorkerLoad(error), messageSenderDestinationID());
        else
            connection->send(Messages::WebResourceLoader::DidFailResourceLoad(error), messageSenderDestinationID());
#else
        connection->send(Messages::WebResourceLoader::DidFailResourceLoad(error), messageSenderDestinationID());
#endif
    }

    if (m_parameters.pageHasResourceLoadClient)
        m_connection->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidCompleteWithError(m_parameters.webPageProxyID, resourceLoadInfo(), { }, error), 0);

    cleanup(LoadResult::Failure);
}

void NetworkResourceLoader::didBlockAuthenticationChallenge()
{
    RELEASE_LOG_IF_ALLOWED("didBlockAuthenticationChallenge:");
    send(Messages::WebResourceLoader::DidBlockAuthenticationChallenge());
}

void NetworkResourceLoader::didReceiveChallenge(const AuthenticationChallenge& challenge)
{
    if (m_parameters.pageHasResourceLoadClient)
        m_connection->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidReceiveChallenge(m_parameters.webPageProxyID, resourceLoadInfo(), challenge), 0);
}

Optional<Seconds> NetworkResourceLoader::validateCacheEntryForMaxAgeCapValidation(const ResourceRequest& request, const ResourceRequest& redirectRequest, const ResourceResponse& redirectResponse)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool existingCacheEntryMatchesNewResponse = false;
    if (m_cacheEntryForMaxAgeCapValidation) {
        ASSERT(redirectResponse.source() == ResourceResponse::Source::Network);
        ASSERT(redirectResponse.isRedirection());
        if (redirectResponse.httpHeaderField(WebCore::HTTPHeaderName::Location) == m_cacheEntryForMaxAgeCapValidation->response().httpHeaderField(WebCore::HTTPHeaderName::Location))
            existingCacheEntryMatchesNewResponse = true;

        m_cache->remove(m_cacheEntryForMaxAgeCapValidation->key());
        m_cacheEntryForMaxAgeCapValidation = nullptr;
    }
    
    if (!existingCacheEntryMatchesNewResponse) {
        if (auto* networkStorageSession = m_connection->networkProcess().storageSession(sessionID()))
            return networkStorageSession->maxAgeCacheCap(request);
    }
#endif
    return WTF::nullopt;
}

void NetworkResourceLoader::willSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    RELEASE_LOG_IF_ALLOWED("willSendRedirectedRequest:");
    ++m_redirectCount;
    m_redirectResponse = redirectResponse;

    Optional<AdClickAttribution::Conversion> adClickConversion;
    if (!sessionID().isEphemeral()) {
        if (auto result = AdClickAttribution::parseConversionRequest(redirectRequest.url()))
            adClickConversion = result.value();
        else if (!result.error().isEmpty())
            addConsoleMessage(MessageSource::AdClickAttribution, MessageLevel::Error, result.error());
    }

    auto maxAgeCap = validateCacheEntryForMaxAgeCapValidation(request, redirectRequest, redirectResponse);
    if (redirectResponse.source() == ResourceResponse::Source::Network && canUseCachedRedirect(request))
        m_cache->storeRedirect(request, redirectResponse, redirectRequest, maxAgeCap);

    if (m_networkLoadChecker) {
        if (adClickConversion)
            m_networkLoadChecker->enableContentExtensionsCheck();
        m_networkLoadChecker->storeRedirectionIfNeeded(request, redirectResponse);

        RELEASE_LOG_IF_ALLOWED("willSendRedirectedRequest: Checking redirect using NetworkLoadChecker");
        m_networkLoadChecker->checkRedirection(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), this, [protectedThis = makeRef(*this), this, storedCredentialsPolicy = m_networkLoadChecker->storedCredentialsPolicy(), adClickConversion = WTFMove(adClickConversion)](auto&& result) mutable {
            if (!result.has_value()) {
                if (result.error().isCancellation()) {
                    RELEASE_LOG_IF_ALLOWED("willSendRedirectedRequest: NetworkLoadChecker::checkRedirection returned with a cancellation");
                    return;
                }

                RELEASE_LOG_ERROR_IF_ALLOWED("willSendRedirectedRequest: NetworkLoadChecker::checkRedirection returned an error");
                this->didFailLoading(result.error());
                return;
            }

            RELEASE_LOG_IF_ALLOWED("willSendRedirectedRequest: NetworkLoadChecker::checkRedirection is done");
            if (m_parameters.options.redirect == FetchOptions::Redirect::Manual) {
                this->didFinishWithRedirectResponse(WTFMove(result->request), WTFMove(result->redirectRequest), WTFMove(result->redirectResponse));
                return;
            }

            if (this->isSynchronous()) {
                if (storedCredentialsPolicy != m_networkLoadChecker->storedCredentialsPolicy()) {
                    // We need to restart the load to update the session according the new credential policy.
                    RELEASE_LOG_IF_ALLOWED("willSendRedirectedRequest: Restarting network load due to credential policy change for synchronous load");
                    this->restartNetworkLoad(WTFMove(result->redirectRequest));
                    return;
                }

                // We do not support prompting for credentials for synchronous loads. If we ever change this policy then
                // we need to take care to prompt if and only if request and redirectRequest are not mixed content.
                this->continueWillSendRequest(WTFMove(result->redirectRequest), false);
                return;
            }

            m_shouldRestartLoad = storedCredentialsPolicy != m_networkLoadChecker->storedCredentialsPolicy();
            this->continueWillSendRedirectedRequest(WTFMove(result->request), WTFMove(result->redirectRequest), WTFMove(result->redirectResponse), WTFMove(adClickConversion));
        });
        return;
    }
    continueWillSendRedirectedRequest(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), WTFMove(adClickConversion));
}

void NetworkResourceLoader::continueWillSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse, Optional<AdClickAttribution::Conversion>&& adClickConversion)
{
    RELEASE_LOG_IF_ALLOWED("continueWillSendRedirectedRequest: (m_isKeptAlive=%d, hasAdClickConversion=%d)", m_isKeptAlive, !!adClickConversion);
    ASSERT(!isSynchronous());

    if (m_isKeptAlive) {
        continueWillSendRequest(WTFMove(redirectRequest), false);
        return;
    }

    NetworkSession* networkSession = nullptr;
    if (adClickConversion && (networkSession = m_connection->networkProcess().networkSession(sessionID())))
        networkSession->handleAdClickAttributionConversion(WTFMove(*adClickConversion), request.url(), redirectRequest);

    // We send the request body separately because the ResourceRequest body normally does not get encoded when sent over IPC, as an optimization.
    // However, we really need the body here because a redirect cross-site may cause a process-swap and the request to start again in a new WebContent process.
    send(Messages::WebResourceLoader::WillSendRequest(redirectRequest, IPC::FormDataReference { redirectRequest.httpBody() }, sanitizeResponseIfPossible(WTFMove(redirectResponse), ResourceResponse::SanitizationType::Redirection)));
}

void NetworkResourceLoader::didFinishWithRedirectResponse(WebCore::ResourceRequest&& request, WebCore::ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    RELEASE_LOG_IF_ALLOWED("didFinishWithRedirectResponse:");
    redirectResponse.setType(ResourceResponse::Type::Opaqueredirect);
    if (!isCrossOriginPrefetch())
        didReceiveResponse(WTFMove(redirectResponse), [] (auto) { });
    else if (auto* session = m_connection->networkProcess().networkSession(sessionID()))
        session->prefetchCache().storeRedirect(request.url(), WTFMove(redirectResponse), WTFMove(redirectRequest));

    WebCore::NetworkLoadMetrics networkLoadMetrics;
    networkLoadMetrics.markComplete();
    networkLoadMetrics.responseBodyBytesReceived = 0;
    networkLoadMetrics.responseBodyDecodedSize = 0;
    send(Messages::WebResourceLoader::DidFinishResourceLoad { networkLoadMetrics });

    cleanup(LoadResult::Success);
}

static bool shouldSanitizeResponse(const NetworkProcess& process, Optional<PageIdentifier> pageIdentifier, const FetchOptions& options, const URL& url)
{
    if (!pageIdentifier || options.destination != FetchOptions::Destination::EmptyString || options.mode != FetchOptions::Mode::NoCors)
        return true;
    return !process.shouldDisableCORSForRequestTo(*pageIdentifier, url);
}

ResourceResponse NetworkResourceLoader::sanitizeResponseIfPossible(ResourceResponse&& response, ResourceResponse::SanitizationType type)
{
    if (!m_parameters.shouldRestrictHTTPResponseAccess)
        return WTFMove(response);

    if (shouldSanitizeResponse(m_connection->networkProcess(), pageID(), parameters().options, originalRequest().url()))
        response.sanitizeHTTPHeaderFields(type);

    return WTFMove(response);
}

void NetworkResourceLoader::restartNetworkLoad(WebCore::ResourceRequest&& newRequest)
{
    RELEASE_LOG_IF_ALLOWED("restartNetworkLoad: (hasNetworkLoad=%d)", !!m_networkLoad);

    if (m_networkLoad)
        m_networkLoad->cancel();

    startNetworkLoad(WTFMove(newRequest), FirstLoad::No);
}

void NetworkResourceLoader::continueWillSendRequest(ResourceRequest&& newRequest, bool isAllowedToAskUserForCredentials)
{
    RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: (isAllowedToAskUserForCredentials=%d)", isAllowedToAskUserForCredentials);

#if ENABLE(SERVICE_WORKER)
    if (parameters().options.mode == FetchOptions::Mode::Navigate) {
        if (auto serviceWorkerFetchTask = m_connection->createFetchTask(*this, newRequest)) {
            RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: Created a ServiceWorkerFetchTask to handle the redirect (fetchIdentifier=%" PRIu64 ")", serviceWorkerFetchTask->fetchIdentifier().toUInt64());
            m_networkLoad = nullptr;
            m_serviceWorkerFetchTask = WTFMove(serviceWorkerFetchTask);
            return;
        }
        RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: Navigation is not using service workers");
        m_shouldRestartLoad = !!m_serviceWorkerFetchTask;
        m_serviceWorkerFetchTask = nullptr;
    }
    if (m_serviceWorkerFetchTask) {
        RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: Continuing fetch task with redirect (fetchIdentifier=%" PRIu64 ")", m_serviceWorkerFetchTask->fetchIdentifier().toUInt64());
        m_serviceWorkerFetchTask->continueFetchTaskWith(WTFMove(newRequest));
        return;
    }
#endif

    if (m_shouldRestartLoad) {
        m_shouldRestartLoad = false;

        if (m_networkLoad)
            m_networkLoad->updateRequestAfterRedirection(newRequest);

        RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: Restarting network load");
        restartNetworkLoad(WTFMove(newRequest));
        return;
    }

    if (m_networkLoadChecker) {
        // FIXME: We should be doing this check when receiving the redirection and not allow about protocol as per fetch spec.
        if (!newRequest.url().protocolIsInHTTPFamily() && !newRequest.url().protocolIsAbout() && m_redirectCount) {
            RELEASE_LOG_ERROR_IF_ALLOWED("continueWillSendRequest: Failing load because it redirected to a scheme that is not HTTP(S)");
            didFailLoading(ResourceError { String { }, 0, newRequest.url(), "Redirection to URL with a scheme that is not HTTP(S)"_s, ResourceError::Type::AccessControl });
            return;
        }
    }

    m_isAllowedToAskUserForCredentials = isAllowedToAskUserForCredentials;

    // If there is a match in the network cache, we need to reuse the original cache policy and partition.
    newRequest.setCachePolicy(originalRequest().cachePolicy());
    newRequest.setCachePartition(originalRequest().cachePartition());

    if (m_isWaitingContinueWillSendRequestForCachedRedirect) {
        m_isWaitingContinueWillSendRequestForCachedRedirect = false;

        LOG(NetworkCache, "(NetworkProcess) Retrieving cached redirect");
        RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: m_isWaitingContinueWillSendRequestForCachedRedirect was set");

        if (canUseCachedRedirect(newRequest))
            retrieveCacheEntry(newRequest);
        else
            startNetworkLoad(WTFMove(newRequest), FirstLoad::Yes);

        return;
    }

    if (m_networkLoad) {
        RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: Telling NetworkLoad to proceed with the redirect");

        if (m_parameters.pageHasResourceLoadClient && !newRequest.isNull())
            m_connection->networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::ResourceLoadDidPerformHTTPRedirection(m_parameters.webPageProxyID, resourceLoadInfo(), m_redirectResponse, newRequest), 0);

        m_networkLoad->continueWillSendRequest(WTFMove(newRequest));
    }
}

void NetworkResourceLoader::continueDidReceiveResponse()
{
    RELEASE_LOG_IF_ALLOWED("continueDidReceiveResponse: (hasCacheEntryWaitingForContinueDidReceiveResponse=%d, hasResponseCompletionHandler=%d)", !!m_cacheEntryWaitingForContinueDidReceiveResponse, !!m_responseCompletionHandler);
#if ENABLE(SERVICE_WORKER)
    if (m_serviceWorkerFetchTask) {
        RELEASE_LOG_IF_ALLOWED("continueDidReceiveResponse: continuing with ServiceWorkerFetchTask (fetchIdentifier=%" PRIu64 ")", m_serviceWorkerFetchTask->fetchIdentifier().toUInt64());
        m_serviceWorkerFetchTask->continueDidReceiveFetchResponse();
        return;
    }
#endif

    if (m_cacheEntryWaitingForContinueDidReceiveResponse) {
        sendResultForCacheEntry(WTFMove(m_cacheEntryWaitingForContinueDidReceiveResponse));
        cleanup(LoadResult::Success);
        return;
    }

    if (m_responseCompletionHandler)
        m_responseCompletionHandler(PolicyAction::Use);
}

void NetworkResourceLoader::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
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

    if (m_bufferedData->isEmpty())
        return;

    send(Messages::WebResourceLoader::DidReceiveSharedBuffer({ *m_bufferedData }, m_bufferedDataEncodedDataLength));

    m_bufferedData = SharedBuffer::create();
    m_bufferedDataEncodedDataLength = 0;
}

void NetworkResourceLoader::sendBuffer(SharedBuffer& buffer, size_t encodedDataLength)
{
    ASSERT(!isSynchronous());

    send(Messages::WebResourceLoader::DidReceiveSharedBuffer({ buffer }, encodedDataLength));
}

void NetworkResourceLoader::tryStoreAsCacheEntry()
{
    if (!canUseCache(m_networkLoad->currentRequest())) {
        RELEASE_LOG_IF_ALLOWED("tryStoreAsCacheEntry: Not storing cache entry because request is not eligible");
        return;
    }
    if (!m_bufferedDataForCache) {
        RELEASE_LOG_IF_ALLOWED("tryStoreAsCacheEntry: Not storing cache entry because m_bufferedDataForCache is null");
        return;
    }

    if (isCrossOriginPrefetch()) {
        if (auto* session = m_connection->networkProcess().networkSession(sessionID())) {
            RELEASE_LOG_IF_ALLOWED("tryStoreAsCacheEntry: Storing entry in prefetch cache");
            session->prefetchCache().store(m_networkLoad->currentRequest().url(), WTFMove(m_response), WTFMove(m_bufferedDataForCache));
        }
        return;
    }
    RELEASE_LOG_IF_ALLOWED("tryStoreAsCacheEntry: Storing entry in HTTP disk cache");
    m_cache->store(m_networkLoad->currentRequest(), m_response, WTFMove(m_bufferedDataForCache), [loader = makeRef(*this)](auto& mappedBody) mutable {
#if ENABLE(SHAREABLE_RESOURCE)
        if (mappedBody.shareableResourceHandle.isNull())
            return;
        LOG(NetworkCache, "(NetworkProcess) sending DidCacheResource");
        loader->send(Messages::NetworkProcessConnection::DidCacheResource(loader->originalRequest(), mappedBody.shareableResourceHandle));
#endif
    });
}

void NetworkResourceLoader::didReceiveMainResourceResponse(const WebCore::ResourceResponse& response)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveMainResourceResponse:");
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (auto* speculativeLoadManager = m_cache ? m_cache->speculativeLoadManager() : nullptr)
        speculativeLoadManager->registerMainResourceLoadResponse(globalFrameID(), originalRequest(), response);
#endif
}

void NetworkResourceLoader::didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    RELEASE_LOG_IF_ALLOWED("didRetrieveCacheEntry:");
    auto response = entry->response();

    if (isMainResource())
        didReceiveMainResourceResponse(response);

    if (isMainResource() && shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(response)) {
        RELEASE_LOG_ERROR_IF_ALLOWED("didRetrieveCacheEntry: Stopping load due to CSP Frame-Ancestors or X-Frame-Options");
        response = sanitizeResponseIfPossible(WTFMove(response), ResourceResponse::SanitizationType::CrossOriginSafe);
        send(Messages::WebResourceLoader::StopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied { response });
        return;
    }
    if (m_networkLoadChecker) {
        auto error = m_networkLoadChecker->validateResponse(originalRequest(), response);
        if (!error.isNull()) {
            RELEASE_LOG_ERROR_IF_ALLOWED("didRetrieveCacheEntry: Failing load due to NetworkLoadChecker::validateResponse");
            didFailLoading(error);
            return;
        }
    }

    response = sanitizeResponseIfPossible(WTFMove(response), ResourceResponse::SanitizationType::CrossOriginSafe);
    if (isSynchronous()) {
        m_synchronousLoadData->response = WTFMove(response);
        sendReplyToSynchronousRequest(*m_synchronousLoadData, entry->buffer());
        cleanup(LoadResult::Success);
        return;
    }

    bool needsContinueDidReceiveResponseMessage = isMainResource();
    RELEASE_LOG_IF_ALLOWED("didRetrieveCacheEntry: Sending WebResourceLoader::DidReceiveResponse IPC (needsContinueDidReceiveResponseMessage=%d)", needsContinueDidReceiveResponseMessage);
    send(Messages::WebResourceLoader::DidReceiveResponse { response, needsContinueDidReceiveResponseMessage });

    if (needsContinueDidReceiveResponseMessage)
        m_cacheEntryWaitingForContinueDidReceiveResponse = WTFMove(entry);
    else {
        sendResultForCacheEntry(WTFMove(entry));
        cleanup(LoadResult::Success);
    }
}

void NetworkResourceLoader::sendResultForCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    RELEASE_LOG_IF_ALLOWED("sendResultForCacheEntry:");
#if ENABLE(SHAREABLE_RESOURCE)
    if (!entry->shareableResourceHandle().isNull()) {
        send(Messages::WebResourceLoader::DidReceiveResource(entry->shareableResourceHandle()));
        return;
    }
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (shouldLogCookieInformation(m_connection, sessionID()))
        logCookieInformation();
#endif

    WebCore::NetworkLoadMetrics networkLoadMetrics;
    networkLoadMetrics.markComplete();
    networkLoadMetrics.requestHeaderBytesSent = 0;
    networkLoadMetrics.requestBodyBytesSent = 0;
    networkLoadMetrics.responseHeaderBytesReceived = 0;
    networkLoadMetrics.responseBodyBytesReceived = 0;
    networkLoadMetrics.responseBodyDecodedSize = 0;

    sendBuffer(*entry->buffer(), entry->buffer()->size());
    send(Messages::WebResourceLoader::DidFinishResourceLoad(networkLoadMetrics));
}

void NetworkResourceLoader::validateCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    RELEASE_LOG_IF_ALLOWED("validateCacheEntry:");
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
    RELEASE_LOG_IF_ALLOWED("dispatchWillSendRequestForCacheEntry:");
    ASSERT(entry->redirectRequest());
    ASSERT(!m_isWaitingContinueWillSendRequestForCachedRedirect);

    LOG(NetworkCache, "(NetworkProcess) Executing cached redirect");

    m_isWaitingContinueWillSendRequestForCachedRedirect = true;
    willSendRedirectedRequest(WTFMove(request), ResourceRequest { *entry->redirectRequest() }, ResourceResponse { entry->response() });
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

    for (auto& extension : m_parameters.requestBodySandboxExtensions)
        extension->consume();

    if (auto& extension = m_parameters.resourceSandboxExtension)
        extension->consume();

    for (auto& fileReference : m_fileReferences)
        fileReference->prepareForFileAccess();

    m_didConsumeSandboxExtensions = true;
}

void NetworkResourceLoader::invalidateSandboxExtensions()
{
    if (m_didConsumeSandboxExtensions) {
        for (auto& extension : m_parameters.requestBodySandboxExtensions)
            extension->revoke();
        if (auto& extension = m_parameters.resourceSandboxExtension)
            extension->revoke();
        for (auto& fileReference : m_fileReferences)
            fileReference->revokeFileAccess();

        m_didConsumeSandboxExtensions = false;
    }

    m_fileReferences.clear();
}

bool NetworkResourceLoader::isAlwaysOnLoggingAllowed() const
{
    if (m_connection->networkProcess().sessionIsControlledByAutomation(sessionID()))
        return true;

    return sessionID().isAlwaysOnLoggingAllowed();
}

bool NetworkResourceLoader::shouldCaptureExtraNetworkLoadMetrics() const
{
    return m_shouldCaptureExtraNetworkLoadMetrics;
}

bool NetworkResourceLoader::crossOriginAccessControlCheckEnabled() const
{
    return m_parameters.crossOriginAccessControlCheckEnabled;
}

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
bool NetworkResourceLoader::shouldLogCookieInformation(NetworkConnectionToWebProcess& connection, const PAL::SessionID& sessionID)
{
    if (auto* session = connection.networkProcess().networkSession(sessionID))
        return session->shouldLogCookieInformation();
    return false;
}

static String escapeForJSON(String s)
{
    return s.replace('\\', "\\\\").replace('"', "\\\"");
}

static String escapeIDForJSON(const Optional<uint64_t>& value)
{
    return value ? String::number(value.value()) : String("None"_s);
}

static String escapeIDForJSON(const Optional<FrameIdentifier>& value)
{
    return value ? String::number(value->toUInt64()) : String("None"_s);
}

static String escapeIDForJSON(const Optional<PageIdentifier>& value)
{
    return value ? String::number(value->toUInt64()) : String("None"_s);
}

void NetworkResourceLoader::logCookieInformation() const
{
    ASSERT(shouldLogCookieInformation(m_connection, sessionID()));

    auto* networkStorageSession = m_connection->networkProcess().storageSession(sessionID());
    ASSERT(networkStorageSession);

    logCookieInformation(m_connection, "NetworkResourceLoader", reinterpret_cast<const void*>(this), *networkStorageSession, originalRequest().firstPartyForCookies(), SameSiteInfo::create(originalRequest()), originalRequest().url(), originalRequest().httpReferrer(), frameID(), pageID(), identifier());
}

static void logBlockedCookieInformation(NetworkConnectionToWebProcess& connection, const String& label, const void* loggedObject, const WebCore::NetworkStorageSession& networkStorageSession, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, Optional<uint64_t> identifier)
{
    ASSERT(NetworkResourceLoader::shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    auto escapedURL = escapeForJSON(url.string());
    auto escapedFirstParty = escapeForJSON(firstParty.string());
    auto escapedFrameID = escapeIDForJSON(frameID);
    auto escapedPageID = escapeIDForJSON(pageID);
    auto escapedIdentifier = escapeIDForJSON(identifier);
    auto escapedReferrer = escapeForJSON(referrer);

#define LOCAL_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(networkStorageSession.sessionID().isAlwaysOnLoggingAllowed(), Network, "%p - %s::" fmt, loggedObject, label.utf8().data(), ##__VA_ARGS__)
#define LOCAL_LOG(str, ...) \
    LOCAL_LOG_IF_ALLOWED("logCookieInformation: BLOCKED cookie access for webPageID=%s, frameID=%s, resourceID=%s, firstParty=%s: " str, escapedPageID.utf8().data(), escapedFrameID.utf8().data(), escapedIdentifier.utf8().data(), escapedFirstParty.utf8().data(), ##__VA_ARGS__)

    LOCAL_LOG(R"({ "url": "%{public}s",)", escapedURL.utf8().data());
    LOCAL_LOG(R"(  "partition": "%{public}s",)", "BLOCKED");
    LOCAL_LOG(R"(  "hasStorageAccess": %{public}s,)", "false");
    LOCAL_LOG(R"(  "referer": "%{public}s",)", escapedReferrer.utf8().data());
    LOCAL_LOG(R"(  "isSameSite": "%{public}s",)", sameSiteInfo.isSameSite ? "true" : "false");
    LOCAL_LOG(R"(  "isTopSite": "%{public}s",)", sameSiteInfo.isTopSite ? "true" : "false");
    LOCAL_LOG(R"(  "cookies": [])");
    LOCAL_LOG(R"(  })");
#undef LOCAL_LOG
#undef LOCAL_LOG_IF_ALLOWED
}

static void logCookieInformationInternal(NetworkConnectionToWebProcess& connection, const String& label, const void* loggedObject, const WebCore::NetworkStorageSession& networkStorageSession, const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, Optional<uint64_t> identifier)
{
    ASSERT(NetworkResourceLoader::shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    Vector<WebCore::Cookie> cookies;
    if (!networkStorageSession.getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, ShouldAskITP::Yes, ShouldRelaxThirdPartyCookieBlocking::No, cookies))
        return;

    auto escapedURL = escapeForJSON(url.string());
    auto escapedPartition = escapeForJSON(emptyString());
    auto escapedReferrer = escapeForJSON(referrer);
    auto escapedFrameID = escapeIDForJSON(frameID);
    auto escapedPageID = escapeIDForJSON(pageID);
    auto escapedIdentifier = escapeIDForJSON(identifier);
    bool hasStorageAccess = (frameID && pageID) ? networkStorageSession.hasStorageAccess(WebCore::RegistrableDomain { url }, WebCore::RegistrableDomain { firstParty }, frameID.value(), pageID.value()) : false;

#define LOCAL_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(networkStorageSession.sessionID().isAlwaysOnLoggingAllowed(), Network, "%p - %s::" fmt, loggedObject, label.utf8().data(), ##__VA_ARGS__)
#define LOCAL_LOG(str, ...) \
    LOCAL_LOG_IF_ALLOWED("logCookieInformation: webPageID=%s, frameID=%s, resourceID=%s: " str, escapedPageID.utf8().data(), escapedFrameID.utf8().data(), escapedIdentifier.utf8().data(), ##__VA_ARGS__)

    LOCAL_LOG(R"({ "url": "%{public}s",)", escapedURL.utf8().data());
    LOCAL_LOG(R"(  "partition": "%{public}s",)", escapedPartition.utf8().data());
    LOCAL_LOG(R"(  "hasStorageAccess": %{public}s,)", hasStorageAccess ? "true" : "false");
    LOCAL_LOG(R"(  "referer": "%{public}s",)", escapedReferrer.utf8().data());
    LOCAL_LOG(R"(  "isSameSite": "%{public}s",)", sameSiteInfo.isSameSite ? "true" : "false");
    LOCAL_LOG(R"(  "isTopSite": "%{public}s",)", sameSiteInfo.isTopSite ? "true" : "false");
    LOCAL_LOG(R"(  "cookies": [)");

    auto size = cookies.size();
    decltype(size) count = 0;
    for (const auto& cookie : cookies) {
        const char* trailingComma = ",";
        if (++count == size)
            trailingComma = "";

        auto escapedName = escapeForJSON(cookie.name);
        auto escapedValue = escapeForJSON(cookie.value);
        auto escapedDomain = escapeForJSON(cookie.domain);
        auto escapedPath = escapeForJSON(cookie.path);
        auto escapedComment = escapeForJSON(cookie.comment);
        auto escapedCommentURL = escapeForJSON(cookie.commentURL.string());
        // FIXME: Log Same-Site policy for each cookie. See <https://bugs.webkit.org/show_bug.cgi?id=184894>.

        LOCAL_LOG(R"(  { "name": "%{public}s",)", escapedName.utf8().data());
        LOCAL_LOG(R"(    "value": "%{public}s",)", escapedValue.utf8().data());
        LOCAL_LOG(R"(    "domain": "%{public}s",)", escapedDomain.utf8().data());
        LOCAL_LOG(R"(    "path": "%{public}s",)", escapedPath.utf8().data());
        LOCAL_LOG(R"(    "created": %f,)", cookie.created);
        LOCAL_LOG(R"(    "expires": %f,)", cookie.expires.valueOr(0));
        LOCAL_LOG(R"(    "httpOnly": %{public}s,)", cookie.httpOnly ? "true" : "false");
        LOCAL_LOG(R"(    "secure": %{public}s,)", cookie.secure ? "true" : "false");
        LOCAL_LOG(R"(    "session": %{public}s,)", cookie.session ? "true" : "false");
        LOCAL_LOG(R"(    "comment": "%{public}s",)", escapedComment.utf8().data());
        LOCAL_LOG(R"(    "commentURL": "%{public}s")", escapedCommentURL.utf8().data());
        LOCAL_LOG(R"(  }%{public}s)", trailingComma);
    }
    LOCAL_LOG(R"(]})");
#undef LOCAL_LOG
#undef LOCAL_LOG_IF_ALLOWED
}

void NetworkResourceLoader::logCookieInformation(NetworkConnectionToWebProcess& connection, const String& label, const void* loggedObject, const NetworkStorageSession& networkStorageSession, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, Optional<FrameIdentifier> frameID, Optional<PageIdentifier> pageID, Optional<uint64_t> identifier)
{
    ASSERT(shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    if (networkStorageSession.shouldBlockCookies(firstParty, url, frameID, pageID, ShouldRelaxThirdPartyCookieBlocking::No))
        logBlockedCookieInformation(connection, label, loggedObject, networkStorageSession, firstParty, sameSiteInfo, url, referrer, frameID, pageID, identifier);
    else
        logCookieInformationInternal(connection, label, loggedObject, networkStorageSession, firstParty, sameSiteInfo, url, referrer, frameID, pageID, identifier);
}
#endif

void NetworkResourceLoader::addConsoleMessage(MessageSource messageSource, MessageLevel messageLevel, const String& message, unsigned long)
{
    send(Messages::WebPage::AddConsoleMessage { m_parameters.webFrameID,  messageSource, messageLevel, message, identifier() }, m_parameters.webPageID);
}

void NetworkResourceLoader::sendCSPViolationReport(URL&& reportURL, Ref<FormData>&& report)
{
    send(Messages::WebPage::SendCSPViolationReport { m_parameters.webFrameID, WTFMove(reportURL), IPC::FormDataReference { WTFMove(report) } }, m_parameters.webPageID);
}

void NetworkResourceLoader::enqueueSecurityPolicyViolationEvent(WebCore::SecurityPolicyViolationEvent::Init&& eventInit)
{
    send(Messages::WebPage::EnqueueSecurityPolicyViolationEvent { m_parameters.webFrameID, WTFMove(eventInit) }, m_parameters.webPageID);
}

void NetworkResourceLoader::logSlowCacheRetrieveIfNeeded(const NetworkCache::Cache::RetrieveInfo& info)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(info);
#else
    if (!isAlwaysOnLoggingAllowed())
        return;
    auto duration = info.completionTime - info.startTime;
    if (duration < 1_s)
        return;
    RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Took %.0fms, priority %d", duration.milliseconds(), info.priority);
    if (info.wasSpeculativeLoad)
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Was speculative load");
    if (!info.storageTimings.startTime)
        return;
    RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Storage retrieve time %.0fms", (info.storageTimings.completionTime - info.storageTimings.startTime).milliseconds());
    if (info.storageTimings.dispatchTime) {
        auto time = (info.storageTimings.dispatchTime - info.storageTimings.startTime).milliseconds();
        auto count = info.storageTimings.dispatchCountAtDispatch - info.storageTimings.dispatchCountAtStart;
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Dispatch delay %.0fms, dispatched %lu resources first", time, count);
    }
    if (info.storageTimings.recordIOStartTime)
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Record I/O time %.0fms", (info.storageTimings.recordIOEndTime - info.storageTimings.recordIOStartTime).milliseconds());
    if (info.storageTimings.blobIOStartTime)
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Blob I/O time %.0fms", (info.storageTimings.blobIOEndTime - info.storageTimings.blobIOStartTime).milliseconds());
    if (info.storageTimings.synchronizationInProgressAtDispatch)
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Synchronization was in progress");
    if (info.storageTimings.shrinkInProgressAtDispatch)
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Shrink was in progress");
    if (info.storageTimings.wasCanceled)
        RELEASE_LOG_IF_ALLOWED("logSlowCacheRetrieveIfNeeded: Retrieve was canceled");
#endif
}

bool NetworkResourceLoader::isCrossOriginPrefetch() const
{
    auto& request = originalRequest();
    return request.httpHeaderField(HTTPHeaderName::Purpose) == "prefetch" && !m_parameters.sourceOrigin->canRequest(request.url());
}

#if ENABLE(SERVICE_WORKER)
void NetworkResourceLoader::startWithServiceWorker()
{
    RELEASE_LOG_IF_ALLOWED("startWithServiceWorker:");
    ASSERT(!m_serviceWorkerFetchTask);
    m_serviceWorkerFetchTask = m_connection->createFetchTask(*this, originalRequest());
    if (m_serviceWorkerFetchTask) {
        RELEASE_LOG_IF_ALLOWED("startWithServiceWorker: Created a ServiceWorkerFetchTask (fetchIdentifier=%" PRIu64 ")", m_serviceWorkerFetchTask->fetchIdentifier().toUInt64());
        return;
    }

    serviceWorkerDidNotHandle(nullptr);
}

void NetworkResourceLoader::serviceWorkerDidNotHandle(ServiceWorkerFetchTask* fetchTask)
{
    RELEASE_LOG_IF_ALLOWED("serviceWorkerDidNotHandle: (fetchIdentifier=%" PRIu64 ")", fetchTask ? fetchTask->fetchIdentifier().toUInt64() : 0);
    RELEASE_ASSERT(m_serviceWorkerFetchTask.get() == fetchTask);
    if (m_parameters.serviceWorkersMode == ServiceWorkersMode::Only) {
        RELEASE_LOG_ERROR_IF_ALLOWED("serviceWorkerDidNotHandle: Aborting load because the service worker did not handle the load and serviceWorkerMode only allows service workers");
        send(Messages::WebResourceLoader::ServiceWorkerDidNotHandle { }, identifier());
        abort();
        return;
    }

    if (m_serviceWorkerFetchTask) {
        auto newRequest = m_serviceWorkerFetchTask->takeRequest();
        m_serviceWorkerFetchTask = nullptr;

        if (m_networkLoad)
            m_networkLoad->updateRequestAfterRedirection(newRequest);

        RELEASE_LOG_IF_ALLOWED("serviceWorkerDidNotHandle: Restarting network load for redirect");
        restartNetworkLoad(WTFMove(newRequest));
        return;
    }
    start();
}
#endif

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
