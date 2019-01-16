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
#include "NetworkBlobRegistry.h"
#include "NetworkCache.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkLoad.h"
#include "NetworkLoadChecker.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "SharedBufferDataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebPageMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/BlobDataFileReference.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/SameSiteInfo.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/RunLoop.h>

#if USE(QUICK_LOOK)
#include <WebCore/PreviewLoader.h>
#endif

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkResourceLoader::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), Network, "%p - NetworkResourceLoader::" fmt, this, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

struct NetworkResourceLoader::SynchronousLoadData {
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
    , m_defersLoading { parameters.defersLoading }
    , m_isAllowedToAskUserForCredentials { m_parameters.clientCredentialPolicy == ClientCredentialPolicy::MayAskClientForCredentials }
    , m_bufferingTimer { *this, &NetworkResourceLoader::bufferingTimerFired }
    , m_cache { sessionID().isEphemeral() ? nullptr : connection.networkProcess().cache() }
    , m_shouldCaptureExtraNetworkLoadMetrics(m_connection->captureExtraNetworkLoadMetricsEnabled())
{
    ASSERT(RunLoop::isMain());
    // FIXME: This is necessary because of the existence of EmptyFrameLoaderClient in WebCore.
    //        Once bug 116233 is resolved, this ASSERT can just be "m_webPageID && m_webFrameID"
    ASSERT((m_parameters.webPageID && m_parameters.webFrameID) || m_parameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials);

    if (originalRequest().httpBody()) {
        for (const auto& element : originalRequest().httpBody()->elements()) {
            if (auto* blobData = WTF::get_if<FormDataElement::EncodedBlobData>(element.data))
                m_fileReferences.appendVector(NetworkBlobRegistry::singleton().filesInBlob(connection, blobData->url));
        }
    }

    if (synchronousReply || parameters.shouldRestrictHTTPResponseAccess) {
        NetworkLoadChecker::LoadType requestLoadType = isMainFrameLoad() ? NetworkLoadChecker::LoadType::MainFrame : NetworkLoadChecker::LoadType::Other;
        m_networkLoadChecker = std::make_unique<NetworkLoadChecker>(connection.networkProcess(), FetchOptions { m_parameters.options }, m_parameters.sessionID, m_parameters.webPageID, m_parameters.webFrameID, HTTPHeaderMap { m_parameters.originalRequestHeaders }, URL { m_parameters.request.url() }, m_parameters.sourceOrigin.copyRef(), m_parameters.preflightPolicy, originalRequest().httpReferrer(), m_parameters.isHTTPSUpgradeEnabled, shouldCaptureExtraNetworkLoadMetrics(), requestLoadType);
        if (m_parameters.cspResponseHeaders)
            m_networkLoadChecker->setCSPResponseHeaders(ContentSecurityPolicyResponseHeaders { m_parameters.cspResponseHeaders.value() });
#if ENABLE(CONTENT_EXTENSIONS)
        m_networkLoadChecker->setContentExtensionController(URL { m_parameters.mainDocumentURL }, m_parameters.userContentControllerIdentifier);
#endif
    }
    if (synchronousReply)
        m_synchronousLoadData = std::make_unique<SynchronousLoadData>(WTFMove(synchronousReply));
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_networkLoad);
    ASSERT(!isSynchronous() || !m_synchronousLoadData->delayedReply);
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

    m_networkActivityTracker = m_connection->startTrackingResourceLoad(m_parameters.webPageID, m_parameters.identifier, isMainResource(), sessionID());

    if (m_defersLoading) {
        RELEASE_LOG_IF_ALLOWED("start: Loading is deferred (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d, parentPID = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous(), m_parameters.parentPID);
        return;
    }

    ASSERT(!m_wasStarted);
    m_wasStarted = true;

    if (m_networkLoadChecker) {
        m_networkLoadChecker->check(ResourceRequest { originalRequest() }, this, [this] (auto&& result) {
            WTF::switchOn(result,
                [this] (ResourceError& error) {
                    RELEASE_LOG_IF_ALLOWED("start: error checking (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d, parentPID = %d, error.domain = %{public}s, error.code = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, this->isMainResource(), this->isSynchronous(), m_parameters.parentPID, error.domain().utf8().data(), error.errorCode());
                    if (!error.isCancellation())
                        this->didFailLoading(error);
                },
                [this] (NetworkLoadChecker::RedirectionTriplet& triplet) {
                    this->m_isWaitingContinueWillSendRequestForCachedRedirect = true;
                    this->willSendRedirectedRequest(WTFMove(triplet.request), WTFMove(triplet.redirectRequest), WTFMove(triplet.redirectResponse));
                    RELEASE_LOG_IF_ALLOWED("start: synthetic redirect sent because request URL was modified (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d, parentPID = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, this->isMainResource(), this->isSynchronous(), m_parameters.parentPID);
                },
                [this] (ResourceRequest& request) {
                    if (this->canUseCache(request)) {
                        RELEASE_LOG_IF_ALLOWED("start: Checking cache for resource (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d, parentPID = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, this->isMainResource(), this->isSynchronous(), m_parameters.parentPID);
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
        RELEASE_LOG_IF_ALLOWED("start: Checking cache for resource (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d, parentPID = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous(), m_parameters.parentPID);
        retrieveCacheEntry(originalRequest());
        return;
    }

    startNetworkLoad(ResourceRequest { originalRequest() }, FirstLoad::Yes);
}

void NetworkResourceLoader::retrieveCacheEntry(const ResourceRequest& request)
{
    ASSERT(canUseCache(request));

    RefPtr<NetworkResourceLoader> loader(this);
    m_cache->retrieve(request, { m_parameters.webPageID, m_parameters.webFrameID }, [this, loader = WTFMove(loader), request = ResourceRequest { request }](auto entry, auto info) mutable {
        if (loader->hasOneRef()) {
            // The loader has been aborted and is only held alive by this lambda.
            return;
        }

        loader->logSlowCacheRetrieveIfNeeded(info);

        if (!entry) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Resource not in cache (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->startNetworkLoad(WTFMove(request), FirstLoad::Yes);
            return;
        }
#if ENABLE(RESOURCE_LOAD_STATISTICS)
        if (entry->hasReachedPrevalentResourceAgeCap()) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Resource has reached prevalent resource age cap (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            m_cacheEntryForMaxAgeCapValidation = WTFMove(entry);
            ResourceRequest revalidationRequest = originalRequest();
            loader->startNetworkLoad(WTFMove(revalidationRequest), FirstLoad::Yes);
            return;
        }
#endif
        if (entry->redirectRequest()) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Handling redirect (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->dispatchWillSendRequestForCacheEntry(WTFMove(request), WTFMove(entry));
            return;
        }
        if (loader->m_parameters.needsCertificateInfo && !entry->response().certificateInfo()) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Resource does not have required certificate (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->startNetworkLoad(WTFMove(request), FirstLoad::Yes);
            return;
        }
        if (entry->needsValidation() || request.cachePolicy() == WebCore::ResourceRequestCachePolicy::RefreshAnyCacheData) {
            RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Validating cache entry (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
            loader->validateCacheEntry(WTFMove(entry));
            return;
        }
        RELEASE_LOG_IF_ALLOWED("retrieveCacheEntry: Retrieved resource from cache (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());
        loader->didRetrieveCacheEntry(WTFMove(entry));
    });
}

void NetworkResourceLoader::startNetworkLoad(ResourceRequest&& request, FirstLoad load)
{
    if (load == FirstLoad::Yes) {
        RELEASE_LOG_IF_ALLOWED("startNetworkLoad: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isMainResource = %d, isSynchronous = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, isMainResource(), isSynchronous());

        consumeSandboxExtensions();

        if (isSynchronous() || m_parameters.maximumBufferingTime > 0_s)
            m_bufferedData = SharedBuffer::create();

        if (canUseCache(request))
            m_bufferedDataForCache = SharedBuffer::create();
    }

    NetworkLoadParameters parameters = m_parameters;
    parameters.defersLoading = m_defersLoading;
    parameters.networkActivityTracker = m_networkActivityTracker;
    if (m_networkLoadChecker)
        parameters.storedCredentialsPolicy = m_networkLoadChecker->storedCredentialsPolicy();

    if (request.url().protocolIsBlob())
        parameters.blobFileReferences = NetworkBlobRegistry::singleton().filesInBlob(m_connection, originalRequest().url());

    auto* networkSession = m_connection->networkProcess().networkSession(parameters.sessionID);
    if (!networkSession && parameters.sessionID.isEphemeral()) {
        m_connection->networkProcess().addWebsiteDataStore(WebsiteDataStoreParameters::privateSessionParameters(parameters.sessionID));
        networkSession = m_connection->networkProcess().networkSession(parameters.sessionID);
    }
    if (!networkSession) {
        WTFLogAlways("Attempted to create a NetworkLoad with a session (id=%" PRIu64 ") that does not exist.", parameters.sessionID.sessionID());
        RELEASE_LOG_ERROR_IF_ALLOWED("startNetworkLoad: Attempted to create a NetworkLoad with a session that does not exist (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", sessionID=%" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, parameters.sessionID.sessionID());
        m_connection->networkProcess().logDiagnosticMessage(m_parameters.webPageID, WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::invalidSessionIDKey(), WebCore::ShouldSample::No);
        didFailLoading(internalError(request.url()));
        return;
    }

    parameters.request = WTFMove(request);
    m_networkLoad = std::make_unique<NetworkLoad>(*this, WTFMove(parameters), *networkSession);

    RELEASE_LOG_IF_ALLOWED("startNetworkLoad: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", description = %{public}s)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, m_networkLoad->description().utf8().data());

    if (m_defersLoading) {
        RELEASE_LOG_IF_ALLOWED("startNetworkLoad: Created, but deferred (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")",
            m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    }
}

void NetworkResourceLoader::setDefersLoading(bool defers)
{
    if (m_defersLoading == defers)
        return;
    m_defersLoading = defers;

    if (defers)
        RELEASE_LOG_IF_ALLOWED("setDefersLoading: Deferring resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    else
        RELEASE_LOG_IF_ALLOWED("setDefersLoading: Resuming deferred resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);

    if (m_networkLoad) {
        m_networkLoad->setDefersLoading(defers);
        return;
    }

    if (!m_defersLoading && !m_wasStarted)
        start();
    else
        RELEASE_LOG_IF_ALLOWED("setDefersLoading: defers = %d, but nothing to do (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_defersLoading, m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
}

void NetworkResourceLoader::cleanup(LoadResult result)
{
    ASSERT(RunLoop::isMain());

    m_connection->stopTrackingResourceLoad(m_parameters.identifier,
        result == LoadResult::Success ? NetworkActivityTracker::CompletionCode::Success :
        result == LoadResult::Failure ? NetworkActivityTracker::CompletionCode::Failure :
        NetworkActivityTracker::CompletionCode::None);

    m_bufferingTimer.stop();

    invalidateSandboxExtensions();

    m_networkLoad = nullptr;

    // This will cause NetworkResourceLoader to be destroyed and therefore we do it last.
    m_connection->didCleanupResourceLoader(*this);
}

void NetworkResourceLoader::convertToDownload(DownloadID downloadID, const ResourceRequest& request, const ResourceResponse& response)
{
    // This can happen if the resource came from the disk cache.
    if (!m_networkLoad) {
        m_connection->networkProcess().downloadManager().startDownload(m_connection.ptr(), m_parameters.sessionID, downloadID, request);
        abort();
        return;
    }

    ASSERT(m_responseCompletionHandler);
    m_connection->networkProcess().downloadManager().convertNetworkLoadToDownload(downloadID, std::exchange(m_networkLoad, nullptr), WTFMove(m_responseCompletionHandler), WTFMove(m_fileReferences), request, response);
}

void NetworkResourceLoader::abort()
{
    ASSERT(RunLoop::isMain());

    RELEASE_LOG_IF_ALLOWED("abort: Canceling resource load (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")",
        m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);

    if (m_networkLoad) {
        if (canUseCache(m_networkLoad->currentRequest())) {
            // We might already have used data from this incomplete load. Ensure older versions don't remain in the cache after cancel.
            if (!m_response.isNull())
                m_cache->remove(m_networkLoad->currentRequest());
        }
        m_networkLoad->cancel();
    }

    cleanup(LoadResult::Cancel);
}

bool NetworkResourceLoader::shouldInterruptLoadForXFrameOptions(const String& xFrameOptions, const URL& url)
{
    if (isMainFrameLoad())
        return false;

    switch (parseXFrameOptionsHeader(xFrameOptions)) {
    case XFrameOptionsNone:
    case XFrameOptionsAllowAll:
        return false;
    case XFrameOptionsDeny:
        return true;
    case XFrameOptionsSameOrigin: {
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
    case XFrameOptionsConflict: {
        String errorMessage = "Multiple 'X-Frame-Options' headers with conflicting values ('" + xFrameOptions + "') encountered when loading '" + url.stringCenterEllipsizedToLength() + "'. Falling back to 'DENY'.";
        send(Messages::WebPage::AddConsoleMessage { m_parameters.webFrameID,  MessageSource::JS, MessageLevel::Error, errorMessage, identifier() }, m_parameters.webPageID);
        return true;
    }
    case XFrameOptionsInvalid: {
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
    if (PreviewLoader::shouldCreateForMIMEType(response.mimeType()))
        return false;
#endif

    auto url = response.url();
    ContentSecurityPolicy contentSecurityPolicy { URL { url }, this };
    contentSecurityPolicy.didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, originalRequest().httpReferrer());
    if (!contentSecurityPolicy.allowFrameAncestors(m_parameters.frameAncestorOrigins, url))
        return true;
    String xFrameOptions = m_response.httpHeaderField(HTTPHeaderName::XFrameOptions);
    if (!xFrameOptions.isNull() && shouldInterruptLoadForXFrameOptions(xFrameOptions, response.url())) {
        String errorMessage = "Refused to display '" + response.url().stringCenterEllipsizedToLength() + "' in a frame because it set 'X-Frame-Options' to '" + xFrameOptions + "'.";
        send(Messages::WebPage::AddConsoleMessage { m_parameters.webFrameID,  MessageSource::Security, MessageLevel::Error, errorMessage, identifier() }, m_parameters.webPageID);
        return true;
    }
    return false;
}

void NetworkResourceLoader::didReceiveResponse(ResourceResponse&& receivedResponse, ResponseCompletionHandler&& completionHandler)
{
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", httpStatusCode = %d, length = %" PRId64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, receivedResponse.httpStatusCode(), receivedResponse.expectedContentLength());

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
        if (validationSucceeded) {
            m_cacheEntryForValidation = m_cache->update(originalRequest(), { m_parameters.webPageID, m_parameters.webFrameID }, *m_cacheEntryForValidation, m_response);
            // If the request was conditional then this revalidation was not triggered by the network cache and we pass the 304 response to WebCore.
            if (originalRequest().isConditional())
                m_cacheEntryForValidation = nullptr;
        } else
            m_cacheEntryForValidation = nullptr;
    }
    if (m_cacheEntryForValidation)
        return completionHandler(PolicyAction::Use);

    if (isMainResource() && shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(m_response)) {
        auto response = sanitizeResponseIfPossible(ResourceResponse { m_response }, ResourceResponse::SanitizationType::CrossOriginSafe);
        send(Messages::WebResourceLoader::StopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied { response });
        return completionHandler(PolicyAction::Ignore);
    }

    if (m_networkLoadChecker) {
        auto error = m_networkLoadChecker->validateResponse(m_response);
        if (!error.isNull()) {
            RunLoop::main().dispatch([protectedThis = makeRef(*this), error = WTFMove(error)] {
                if (protectedThis->m_networkLoad)
                    protectedThis->didFailLoading(error);
            });
            return completionHandler(PolicyAction::Ignore);
        }
    }

    auto response = sanitizeResponseIfPossible(ResourceResponse { m_response }, ResourceResponse::SanitizationType::CrossOriginSafe);
    if (isSynchronous()) {
        m_synchronousLoadData->response = WTFMove(response);
        return completionHandler(PolicyAction::Use);
    }

    // We wait to receive message NetworkResourceLoader::ContinueDidReceiveResponse before continuing a load for
    // a main resource because the embedding client must decide whether to allow the load.
    bool willWaitForContinueDidReceiveResponse = isMainResource();
    send(Messages::WebResourceLoader::DidReceiveResponse { response, willWaitForContinueDidReceiveResponse });
    if (willWaitForContinueDidReceiveResponse)
        m_responseCompletionHandler = WTFMove(completionHandler);
    else
        completionHandler(PolicyAction::Use);
}

void NetworkResourceLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, int reportedEncodedDataLength)
{
    if (!m_numBytesReceived)
        RELEASE_LOG_IF_ALLOWED("didReceiveBuffer: Started receiving data (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);
    m_numBytesReceived += buffer->size();

    ASSERT(!m_cacheEntryForValidation);

    if (m_bufferedDataForCache) {
        // Prevent memory growth in case of streaming data.
        const size_t maximumCacheBufferSize = 10 * 1024 * 1024;
        if (m_bufferedDataForCache->size() + buffer->size() <= maximumCacheBufferSize)
            m_bufferedDataForCache->append(buffer.get());
        else
            m_bufferedDataForCache = nullptr;
    }
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
    RELEASE_LOG_IF_ALLOWED("didFinishLoading: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", length = %zd)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, m_numBytesReceived);

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

    cleanup(LoadResult::Success);
}

void NetworkResourceLoader::didFailLoading(const ResourceError& error)
{
    RELEASE_LOG_IF_ALLOWED("didFailLoading: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", isTimeout = %d, isCancellation = %d, isAccessControl = %d, errCode = %d)", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier, error.isTimeout(), error.isCancellation(), error.isAccessControl(), error.errorCode());

    if (shouldCaptureExtraNetworkLoadMetrics())
        m_connection->removeNetworkLoadInformation(identifier());

    ASSERT(!error.isNull());

    m_cacheEntryForValidation = nullptr;

    if (isSynchronous()) {
        m_synchronousLoadData->error = error;
        sendReplyToSynchronousRequest(*m_synchronousLoadData, nullptr);
    } else if (auto* connection = messageSenderConnection())
        connection->send(Messages::WebResourceLoader::DidFailResourceLoad(error), messageSenderDestinationID());

    cleanup(LoadResult::Failure);
}

void NetworkResourceLoader::didBlockAuthenticationChallenge()
{
    send(Messages::WebResourceLoader::DidBlockAuthenticationChallenge());
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
        if (auto networkStorageSession = WebCore::NetworkStorageSession::storageSession(sessionID()))
            return networkStorageSession->maxAgeCacheCap(request);
    }
#endif
    return WTF::nullopt;
}

void NetworkResourceLoader::willSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    ++m_redirectCount;

    auto maxAgeCap = validateCacheEntryForMaxAgeCapValidation(request, redirectRequest, redirectResponse);
    if (redirectResponse.source() == ResourceResponse::Source::Network && canUseCachedRedirect(request))
        m_cache->storeRedirect(request, redirectResponse, redirectRequest, maxAgeCap);

    if (m_networkLoadChecker) {
        m_networkLoadChecker->storeRedirectionIfNeeded(request, redirectResponse);
        m_networkLoadChecker->checkRedirection(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse), this, [protectedThis = makeRef(*this), this, storedCredentialsPolicy = m_networkLoadChecker->storedCredentialsPolicy()](auto&& result) mutable {
            if (!result.has_value()) {
                if (result.error().isCancellation())
                    return;

                this->didFailLoading(result.error());
                return;
            }

            if (m_parameters.options.redirect == FetchOptions::Redirect::Manual) {
                this->didFinishWithRedirectResponse(WTFMove(result->redirectResponse));
                return;
            }

            if (this->isSynchronous()) {
                if (storedCredentialsPolicy != m_networkLoadChecker->storedCredentialsPolicy()) {
                    // We need to restart the load to update the session according the new credential policy.
                    this->restartNetworkLoad(WTFMove(result->redirectRequest));
                    return;
                }

                // We do not support prompting for credentials for synchronous loads. If we ever change this policy then
                // we need to take care to prompt if and only if request and redirectRequest are not mixed content.
                this->continueWillSendRequest(WTFMove(result->redirectRequest), false);
                return;
            }

            m_shouldRestartLoad = storedCredentialsPolicy != m_networkLoadChecker->storedCredentialsPolicy();
            this->continueWillSendRedirectedRequest(WTFMove(result->request), WTFMove(result->redirectRequest), WTFMove(result->redirectResponse));
        });
        return;
    }
    continueWillSendRedirectedRequest(WTFMove(request), WTFMove(redirectRequest), WTFMove(redirectResponse));
}

void NetworkResourceLoader::continueWillSendRedirectedRequest(ResourceRequest&& request, ResourceRequest&& redirectRequest, ResourceResponse&& redirectResponse)
{
    ASSERT(!isSynchronous());

    send(Messages::WebResourceLoader::WillSendRequest(redirectRequest, sanitizeResponseIfPossible(WTFMove(redirectResponse), ResourceResponse::SanitizationType::Redirection)));
}

void NetworkResourceLoader::didFinishWithRedirectResponse(ResourceResponse&& redirectResponse)
{
    redirectResponse.setType(ResourceResponse::Type::Opaqueredirect);
    didReceiveResponse(WTFMove(redirectResponse), [] (auto) { });

    WebCore::NetworkLoadMetrics networkLoadMetrics;
    networkLoadMetrics.markComplete();
    networkLoadMetrics.responseBodyBytesReceived = 0;
    networkLoadMetrics.responseBodyDecodedSize = 0;
    send(Messages::WebResourceLoader::DidFinishResourceLoad { networkLoadMetrics });

    cleanup(LoadResult::Success);
}

ResourceResponse NetworkResourceLoader::sanitizeResponseIfPossible(ResourceResponse&& response, ResourceResponse::SanitizationType type)
{
    if (m_parameters.shouldRestrictHTTPResponseAccess)
        response.sanitizeHTTPHeaderFields(type);

    return WTFMove(response);
}

void NetworkResourceLoader::restartNetworkLoad(WebCore::ResourceRequest&& newRequest)
{
    if (m_networkLoad)
        m_networkLoad->cancel();

    startNetworkLoad(WTFMove(newRequest), FirstLoad::No);
}

void NetworkResourceLoader::continueWillSendRequest(ResourceRequest&& newRequest, bool isAllowedToAskUserForCredentials)
{
    if (m_shouldRestartLoad) {
        m_shouldRestartLoad = false;
        restartNetworkLoad(WTFMove(newRequest));
        return;
    }

    if (m_networkLoadChecker) {
        // FIXME: We should be doing this check when receiving the redirection and not allow about protocol as per fetch spec.
        if (!newRequest.url().protocolIsInHTTPFamily() && !newRequest.url().protocolIsAbout() && m_redirectCount) {
            didFailLoading(ResourceError { String { }, 0, newRequest.url(), "Redirection to URL with a scheme that is not HTTP(S)"_s, ResourceError::Type::AccessControl });
            return;
        }
    }

    RELEASE_LOG_IF_ALLOWED("continueWillSendRequest: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_parameters.webPageID, m_parameters.webFrameID, m_parameters.identifier);

    m_isAllowedToAskUserForCredentials = isAllowedToAskUserForCredentials;

    // If there is a match in the network cache, we need to reuse the original cache policy and partition.
    newRequest.setCachePolicy(originalRequest().cachePolicy());
    newRequest.setCachePartition(originalRequest().cachePartition());

    if (m_isWaitingContinueWillSendRequestForCachedRedirect) {
        m_isWaitingContinueWillSendRequestForCachedRedirect = false;

        LOG(NetworkCache, "(NetworkProcess) Retrieving cached redirect");

        if (canUseCachedRedirect(newRequest))
            retrieveCacheEntry(newRequest);
        else
            startNetworkLoad(WTFMove(newRequest), FirstLoad::Yes);

        return;
    }

    if (m_networkLoad)
        m_networkLoad->continueWillSendRequest(WTFMove(newRequest));
}

void NetworkResourceLoader::continueDidReceiveResponse()
{
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

    send(Messages::WebResourceLoader::DidReceiveData({ *m_bufferedData }, m_bufferedDataEncodedDataLength));

    m_bufferedData = SharedBuffer::create();
    m_bufferedDataEncodedDataLength = 0;
}

void NetworkResourceLoader::sendBuffer(SharedBuffer& buffer, size_t encodedDataLength)
{
    ASSERT(!isSynchronous());

    send(Messages::WebResourceLoader::DidReceiveData({ buffer }, encodedDataLength));
}

void NetworkResourceLoader::tryStoreAsCacheEntry()
{
    if (!canUseCache(m_networkLoad->currentRequest()))
        return;
    if (!m_bufferedDataForCache)
        return;

    m_cache->store(m_networkLoad->currentRequest(), m_response, WTFMove(m_bufferedDataForCache), [loader = makeRef(*this)](auto& mappedBody) mutable {
#if ENABLE(SHAREABLE_RESOURCE)
        if (mappedBody.shareableResourceHandle.isNull())
            return;
        LOG(NetworkCache, "(NetworkProcess) sending DidCacheResource");
        loader->send(Messages::NetworkProcessConnection::DidCacheResource(loader->originalRequest(), mappedBody.shareableResourceHandle, loader->sessionID()));
#endif
    });
}

void NetworkResourceLoader::didRetrieveCacheEntry(std::unique_ptr<NetworkCache::Entry> entry)
{
    auto response = entry->response();

    if (isMainResource() && shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(response)) {
        response = sanitizeResponseIfPossible(WTFMove(response), ResourceResponse::SanitizationType::CrossOriginSafe);
        send(Messages::WebResourceLoader::StopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied { response });
        return;
    }
    if (m_networkLoadChecker) {
        auto error = m_networkLoadChecker->validateResponse(response);
        if (!error.isNull()) {
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
    ASSERT(entry->redirectRequest());
    ASSERT(!m_isWaitingContinueWillSendRequestForCachedRedirect);

    LOG(NetworkCache, "(NetworkProcess) Executing cached redirect");

    m_isWaitingContinueWillSendRequestForCachedRedirect = true;
    willSendRedirectedRequest(WTFMove(request), ResourceRequest { *entry->redirectRequest() }, ResourceResponse { entry->response() });
}

IPC::Connection* NetworkResourceLoader::messageSenderConnection()
{
    return &connectionToWebProcess().connection();
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

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
bool NetworkResourceLoader::shouldLogCookieInformation(NetworkConnectionToWebProcess& connection, const PAL::SessionID& sessionID)
{
    if (auto session = connection.networkProcess().networkSession(sessionID))
        return session->shouldLogCookieInformation();
    return false;
}

static String escapeForJSON(String s)
{
    return s.replace('\\', "\\\\").replace('"', "\\\"");
}

static String escapeIDForJSON(const Optional<uint64_t>& value)
{
    return value ? String::number(value.value()) : String("None");
};

void NetworkResourceLoader::logCookieInformation() const
{
    ASSERT(shouldLogCookieInformation(m_connection, sessionID()));

    auto networkStorageSession = WebCore::NetworkStorageSession::storageSession(sessionID());
    ASSERT(networkStorageSession);

    logCookieInformation(m_connection, "NetworkResourceLoader", reinterpret_cast<const void*>(this), *networkStorageSession, originalRequest().firstPartyForCookies(), SameSiteInfo::create(originalRequest()), originalRequest().url(), originalRequest().httpReferrer(), frameID(), pageID(), identifier());
}

static void logBlockedCookieInformation(NetworkConnectionToWebProcess& connection, const String& label, const void* loggedObject, const WebCore::NetworkStorageSession& networkStorageSession, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Optional<uint64_t> identifier)
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
    LOCAL_LOG_IF_ALLOWED("logCookieInformation: BLOCKED cookie access for pageID = %s, frameID = %s, resourceID = %s, firstParty = %s: " str, escapedPageID.utf8().data(), escapedFrameID.utf8().data(), escapedIdentifier.utf8().data(), escapedFirstParty.utf8().data(), ##__VA_ARGS__)

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

static void logCookieInformationInternal(NetworkConnectionToWebProcess& connection, const String& label, const void* loggedObject, const WebCore::NetworkStorageSession& networkStorageSession, const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Optional<uint64_t> identifier)
{
    ASSERT(NetworkResourceLoader::shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    Vector<WebCore::Cookie> cookies;
    if (!networkStorageSession.getRawCookies(firstParty, sameSiteInfo, url, frameID, pageID, cookies))
        return;

    auto escapedURL = escapeForJSON(url.string());
    auto escapedPartition = escapeForJSON(emptyString());
    auto escapedReferrer = escapeForJSON(referrer);
    auto escapedFrameID = escapeIDForJSON(frameID);
    auto escapedPageID = escapeIDForJSON(pageID);
    auto escapedIdentifier = escapeIDForJSON(identifier);
    bool hasStorageAccess = (frameID && pageID) ? networkStorageSession.hasStorageAccess(url.string(), firstParty.string(), frameID.value(), pageID.value()) : false;

#define LOCAL_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(networkStorageSession.sessionID().isAlwaysOnLoggingAllowed(), Network, "%p - %s::" fmt, loggedObject, label.utf8().data(), ##__VA_ARGS__)
#define LOCAL_LOG(str, ...) \
    LOCAL_LOG_IF_ALLOWED("logCookieInformation: pageID = %s, frameID = %s, resourceID = %s: " str, escapedPageID.utf8().data(), escapedFrameID.utf8().data(), escapedIdentifier.utf8().data(), ##__VA_ARGS__)

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
        LOCAL_LOG(R"(    "expires": %f,)", cookie.expires);
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

void NetworkResourceLoader::logCookieInformation(NetworkConnectionToWebProcess& connection, const String& label, const void* loggedObject, const NetworkStorageSession& networkStorageSession, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, const String& referrer, Optional<uint64_t> frameID, Optional<uint64_t> pageID, Optional<uint64_t> identifier)
{
    ASSERT(shouldLogCookieInformation(connection, networkStorageSession.sessionID()));

    if (networkStorageSession.shouldBlockCookies(firstParty, url, frameID, pageID))
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

} // namespace WebKit

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
