/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
#include "SubresourceLoader.h"

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CrossOriginAccessControl.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPParsers.h"
#include "LinkLoader.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "ResourceLoadObserver.h"
#include "ResourceTiming.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#include <RuntimeApplicationChecks.h>
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include "ResourceLoadInfo.h"
#endif

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoader.h"
#include "PreviewConverter.h"
#endif

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
#define PAGE_ID ((frame() ? frame()->pageID().valueOr(PageIdentifier()) : PageIdentifier()).toUInt64())
#define FRAME_ID ((frame() ? frame()->frameID().valueOr(FrameIdentifier()) : FrameIdentifier()).toUInt64())
#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), ResourceLoading, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", frameLoader=%p, resourceID=%lu] SubresourceLoader::" fmt, this, PAGE_ID, FRAME_ID, frameLoader(), identifier(), ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), ResourceLoading, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", frameLoader=%p, resourceID=%lu] SubresourceLoader::" fmt, this, PAGE_ID, FRAME_ID, frameLoader(), identifier(), ##__VA_ARGS__)

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, subresourceLoaderCounter, ("SubresourceLoader"));

SubresourceLoader::RequestCountTracker::RequestCountTracker(CachedResourceLoader& cachedResourceLoader, const CachedResource& resource)
    : m_cachedResourceLoader(cachedResourceLoader)
    , m_resource(resource)
{
    m_cachedResourceLoader.incrementRequestCount(m_resource);
}

SubresourceLoader::RequestCountTracker::~RequestCountTracker()
{
    m_cachedResourceLoader.decrementRequestCount(m_resource);
}

SubresourceLoader::SubresourceLoader(Frame& frame, CachedResource& resource, const ResourceLoaderOptions& options)
    : ResourceLoader(frame, options)
    , m_resource(&resource)
    , m_state(Uninitialized)
    , m_requestCountTracker(std::in_place, frame.document()->cachedResourceLoader(), resource)
{
#ifndef NDEBUG
    subresourceLoaderCounter.increment();
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    m_resourceType = ContentExtensions::toResourceType(resource.type());
#endif
    m_canCrossOriginRequestsAskUserForCredentials = resource.type() == CachedResource::Type::MainResource || frame.settings().allowCrossOriginSubresourcesToAskForCredentials();
}

SubresourceLoader::~SubresourceLoader()
{
    ASSERT(m_state != Initialized);
    ASSERT(reachedTerminalState());
#ifndef NDEBUG
    subresourceLoaderCounter.decrement();
#endif
}

void SubresourceLoader::create(Frame& frame, CachedResource& resource, ResourceRequest&& request, const ResourceLoaderOptions& options, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&& completionHandler)
{
    auto subloader(adoptRef(*new SubresourceLoader(frame, resource, options)));
#if PLATFORM(IOS_FAMILY)
    if (!IOSApplication::isWebProcess()) {
        // On iOS, do not invoke synchronous resource load delegates while resource load scheduling
        // is disabled to avoid re-entering style selection from a different thread (see <rdar://problem/9121719>).
        // FIXME: This should be fixed for all ports in <https://bugs.webkit.org/show_bug.cgi?id=56647>.
        subloader->m_iOSOriginalRequest = request;
        return completionHandler(WTFMove(subloader));
    }
#endif
    subloader->init(WTFMove(request), [subloader, completionHandler = WTFMove(completionHandler)] (bool initialized) mutable {
        if (!initialized)
            return completionHandler(nullptr);
        completionHandler(WTFMove(subloader));
    });
}
    
#if PLATFORM(IOS_FAMILY)
void SubresourceLoader::startLoading()
{
    // FIXME: this should probably be removed.
    ASSERT(!IOSApplication::isWebProcess());
    init(ResourceRequest(m_iOSOriginalRequest), [this, protectedThis = makeRef(*this)] (bool success) {
        if (!success)
            return;
        m_iOSOriginalRequest = ResourceRequest();
        start();
    });
}
#endif

CachedResource* SubresourceLoader::cachedResource()
{
    return m_resource;
}

void SubresourceLoader::cancelIfNotFinishing()
{
    if (m_state != Initialized)
        return;

    ResourceLoader::cancel();
}

void SubresourceLoader::init(ResourceRequest&& request, CompletionHandler<void(bool)>&& completionHandler)
{
    ResourceLoader::init(WTFMove(request), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (bool initialized) mutable {
        if (!initialized)
            return completionHandler(false);
        if (!m_documentLoader) {
            ASSERT_NOT_REACHED();
            RELEASE_LOG_ERROR_IF_ALLOWED("init: resource load canceled because document loader is null");
            return completionHandler(false);
        }
        ASSERT(!reachedTerminalState());
        m_state = Initialized;
        m_documentLoader->addSubresourceLoader(this);
        m_origin = m_resource->origin();
        completionHandler(true);
    });
}

bool SubresourceLoader::isSubresourceLoader() const
{
    return true;
}

void SubresourceLoader::willSendRequestInternal(ResourceRequest&& newRequest, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    // Store the previous URL because the call to ResourceLoader::willSendRequest will modify it.
    URL previousURL = request().url();
    Ref<SubresourceLoader> protectedThis(*this);

    if (!newRequest.url().isValid()) {
        RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because new request is invalid");
        cancel(cannotShowURLError());
        return completionHandler(WTFMove(newRequest));
    }

    if (newRequest.requester() != ResourceRequestBase::Requester::Main) {
        tracePoint(SubresourceLoadWillStart);
        ResourceLoadObserver::shared().logSubresourceLoading(m_frame.get(), newRequest, redirectResponse,
            (isScriptLikeDestination(options().destination) ? ResourceLoadObserver::FetchDestinationIsScriptLike::Yes : ResourceLoadObserver::FetchDestinationIsScriptLike::No));
    }

    auto continueWillSendRequest = [this, protectedThis = makeRef(*this), redirectResponse] (CompletionHandler<void(ResourceRequest&&)>&& completionHandler, ResourceRequest&& newRequest) mutable {
        if (newRequest.isNull() || reachedTerminalState()) {
            if (newRequest.isNull())
                RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because new request is NULL (1)");
            else
                RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because reached terminal state");
            return completionHandler(WTFMove(newRequest));
        }

        ResourceLoader::willSendRequestInternal(WTFMove(newRequest), redirectResponse, [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), redirectResponse] (ResourceRequest&& request) mutable {
            if (reachedTerminalState())
                return completionHandler(WTFMove(request));

            if (request.isNull()) {
                RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because request is NULL (2)");
                cancel();
                return completionHandler(WTFMove(request));
            }

            if (m_resource->type() == CachedResource::Type::MainResource && !redirectResponse.isNull())
                m_documentLoader->willContinueMainResourceLoadAfterRedirect(request);

            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load finished; calling completion handler");
            completionHandler(WTFMove(request));
        });
    };

    ASSERT(!newRequest.isNull());
    if (!redirectResponse.isNull()) {
        if (options().redirect != FetchOptions::Redirect::Follow) {
            if (options().redirect == FetchOptions::Redirect::Error) {
                ResourceError error { errorDomainWebKitInternal, 0, request().url(), makeString("Not allowed to follow a redirection while loading ", request().url().string()), ResourceError::Type::AccessControl };

                if (m_frame && m_frame->document())
                    m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, error.localizedDescription());

                RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because not allowed to follow a redirect");

                cancel(error);
                return completionHandler(WTFMove(newRequest));
            }

            ResourceResponse opaqueRedirectedResponse = redirectResponse;
            opaqueRedirectedResponse.setType(ResourceResponse::Type::Opaqueredirect);
            opaqueRedirectedResponse.setTainting(ResourceResponse::Tainting::Opaqueredirect);
            m_resource->responseReceived(opaqueRedirectedResponse);
            if (reachedTerminalState()) {
                RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: reached terminal state");
                return completionHandler(WTFMove(newRequest));
            }

            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load completed");

            NetworkLoadMetrics emptyMetrics;
            didFinishLoading(emptyMetrics);
            return completionHandler(WTFMove(newRequest));
        } else if (m_redirectCount++ >= options().maxRedirectCount) {
            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because too many redirects");
            cancel(ResourceError(String(), 0, request().url(), "Too many redirections"_s, ResourceError::Type::General));
            return completionHandler(WTFMove(newRequest));
        }

        // CachedResources are keyed off their original request URL.
        // Requesting the same original URL a second time can redirect to a unique second resource.
        // Therefore, if a redirect to a different destination URL occurs, we should no longer consider this a revalidation of the first resource.
        // Doing so would have us reusing the resource from the first request if the second request's revalidation succeeds.
        if (newRequest.isConditional() && m_resource->resourceToRevalidate() && newRequest.url() != m_resource->resourceToRevalidate()->response().url()) {
            newRequest.makeUnconditional();
            MemoryCache::singleton().revalidationFailed(*m_resource);
            if (m_frame && m_frame->page())
                m_frame->page()->diagnosticLoggingClient().logDiagnosticMessageWithResult(DiagnosticLoggingKeys::cachedResourceRevalidationKey(), emptyString(), DiagnosticLoggingResultFail, ShouldSample::Yes);
        }

        if (!m_documentLoader->cachedResourceLoader().updateRequestAfterRedirection(m_resource->type(), newRequest, options())) {
            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because CachedResourceLoader::updateRequestAfterRedirection (really CachedResourceLoader::canRequestAfterRedirection) said no");
            cancel();
            return completionHandler(WTFMove(newRequest));
        }

        auto accessControlCheckResult = checkRedirectionCrossOriginAccessControl(request(), redirectResponse, newRequest);
        if (!accessControlCheckResult) {
            auto errorMessage = makeString("Cross-origin redirection to ", newRequest.url().string(), " denied by Cross-Origin Resource Sharing policy: ", accessControlCheckResult.error());
            if (m_frame && m_frame->document())
                m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, errorMessage);
            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because crosss-origin redirection denied by CORS policy");
            cancel(ResourceError(String(), 0, request().url(), errorMessage, ResourceError::Type::AccessControl));
            return completionHandler(WTFMove(newRequest));
        }

        if (m_resource->isImage() && m_documentLoader->cachedResourceLoader().shouldDeferImageLoad(newRequest.url())) {
            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource load canceled because it's an image that should be defered");
            cancel();
            return completionHandler(WTFMove(newRequest));
        }
        m_loadTiming.addRedirect(redirectResponse.url(), newRequest.url());
        m_resource->redirectReceived(WTFMove(newRequest), redirectResponse, [this, protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), continueWillSendRequest = WTFMove(continueWillSendRequest)] (ResourceRequest&& request) mutable {
            RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: resource done notifying clients");
            continueWillSendRequest(WTFMove(completionHandler), WTFMove(request));
        });
        return;
    }

    RELEASE_LOG_IF_ALLOWED("willSendRequestInternal: redirect response is NULL");
    continueWillSendRequest(WTFMove(completionHandler), WTFMove(newRequest));
}

void SubresourceLoader::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_state == Initialized);
    Ref<SubresourceLoader> protectedThis(*this);
    m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

#if USE(QUICK_LOOK)

bool SubresourceLoader::shouldCreatePreviewLoaderForResponse(const ResourceResponse& response) const
{
    if (m_resource->type() != CachedResource::Type::MainResource)
        return false;

    if (m_previewLoader)
        return false;

    return PreviewConverter::supportsMIMEType(response.mimeType());
}

void SubresourceLoader::didReceivePreviewResponse(const ResourceResponse& response)
{
    ASSERT(m_state == Initialized);
    ASSERT(!response.isNull());
    ASSERT(m_resource);
    m_resource->previewResponseReceived(response);
    ResourceLoader::didReceivePreviewResponse(response);
}

#endif

static bool isLocationURLFailure(const ResourceResponse& response)
{
    auto locationString = response.httpHeaderField(HTTPHeaderName::Location);
    return !locationString.isNull() && locationString.isEmpty();
}

void SubresourceLoader::didReceiveResponse(const ResourceResponse& response, CompletionHandler<void()>&& policyCompletionHandler)
{
    ASSERT(!response.isNull());
    ASSERT(m_state == Initialized);

    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(policyCompletionHandler));

    if (response.containsInvalidHTTPHeaders()) {
        didFail(ResourceError(errorDomainWebKitInternal, 0, request().url(), "Response contained invalid HTTP headers", ResourceError::Type::General));
        return;
    }

#if USE(QUICK_LOOK)
    if (shouldCreatePreviewLoaderForResponse(response)) {
        m_previewLoader = makeUnique<LegacyPreviewLoader>(*this, response);
        if (m_previewLoader->didReceiveResponse(response))
            return;
    }
#endif
#if ENABLE(SERVICE_WORKER)
    // Implementing step 10 of https://fetch.spec.whatwg.org/#main-fetch for service worker responses.
    if (response.source() == ResourceResponse::Source::ServiceWorker && response.url() != request().url()) {
        auto& loader = m_documentLoader->cachedResourceLoader();
        if (!loader.allowedByContentSecurityPolicy(m_resource->type(), response.url(), options(), ContentSecurityPolicy::RedirectResponseReceived::Yes)) {
            RELEASE_LOG_IF_ALLOWED("didReceiveResponse: canceling load because not allowed by content policy");
            cancel(ResourceError({ }, 0, response.url(), { }, ResourceError::Type::General));
            return;
        }
    }
#endif

    if (auto error = validateRangeRequestedFlag(request(), response)) {
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: canceling load because receiving a range requested response for a non-range request");
        cancel(WTFMove(*error));
        return;
    }

    // We want redirect responses to be processed through willSendRequestInternal. Exceptions are
    // redirection with no Location headers and fetch in manual redirect mode. Or in rare circumstances,
    // cases of too many redirects from CFNetwork (<rdar://problem/30610988>).
#if !PLATFORM(COCOA)
    ASSERT(response.httpStatusCode() < 300 || response.httpStatusCode() >= 400 || response.httpStatusCode() == 304 || !response.httpHeaderField(HTTPHeaderName::Location) || response.type() == ResourceResponse::Type::Opaqueredirect);
#endif

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    Ref<SubresourceLoader> protectedThis(*this);

    if (shouldIncludeCertificateInfo())
        response.includeCertificateInfo();

    if (m_resource->resourceToRevalidate()) {
        if (response.httpStatusCode() == 304) {
            // 304 Not modified / Use local copy
            // Existing resource is ok, just use it updating the expiration time.
            ResourceResponse revalidationResponse = response;
            revalidationResponse.setSource(ResourceResponse::Source::MemoryCacheAfterValidation);
            m_resource->setResponse(revalidationResponse);
            MemoryCache::singleton().revalidationSucceeded(*m_resource, revalidationResponse);
            if (m_frame && m_frame->page())
                m_frame->page()->diagnosticLoggingClient().logDiagnosticMessageWithResult(DiagnosticLoggingKeys::cachedResourceRevalidationKey(), emptyString(), DiagnosticLoggingResultPass, ShouldSample::Yes);
            if (!reachedTerminalState())
                ResourceLoader::didReceiveResponse(revalidationResponse, [completionHandlerCaller = WTFMove(completionHandlerCaller)] { });
            return;
        }
        // Did not get 304 response, continue as a regular resource load.
        MemoryCache::singleton().revalidationFailed(*m_resource);
        if (m_frame && m_frame->page())
            m_frame->page()->diagnosticLoggingClient().logDiagnosticMessageWithResult(DiagnosticLoggingKeys::cachedResourceRevalidationKey(), emptyString(), DiagnosticLoggingResultFail, ShouldSample::Yes);
    }

    auto accessControlCheckResult = checkResponseCrossOriginAccessControl(response);
    if (!accessControlCheckResult) {
        if (m_frame && m_frame->document())
            m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, accessControlCheckResult.error());
        RELEASE_LOG_IF_ALLOWED("didReceiveResponse: canceling load because of cross origin access control");
        cancel(ResourceError(String(), 0, request().url(), accessControlCheckResult.error(), ResourceError::Type::AccessControl));
        return;
    }

    if (response.isRedirection()) {
        if (options().redirect == FetchOptions::Redirect::Follow && isLocationURLFailure(response)) {
            // Implementing https://fetch.spec.whatwg.org/#concept-http-redirect-fetch step 3
            cancel();
            return;
        }
        if (options().redirect == FetchOptions::Redirect::Manual) {
            ResourceResponse opaqueRedirectedResponse = response;
            opaqueRedirectedResponse.setType(ResourceResponse::Type::Opaqueredirect);
            opaqueRedirectedResponse.setTainting(ResourceResponse::Tainting::Opaqueredirect);
            m_resource->responseReceived(opaqueRedirectedResponse);
            if (!reachedTerminalState())
                ResourceLoader::didReceiveResponse(opaqueRedirectedResponse, [completionHandlerCaller = WTFMove(completionHandlerCaller)] { });
            return;
        }
    }

    m_resource->responseReceived(response);
    if (reachedTerminalState())
        return;

    bool isResponseMultipart = response.isMultipart();
    if (options().mode != FetchOptions::Mode::Navigate)
        LinkLoader::loadLinksFromHeader(response.httpHeaderField(HTTPHeaderName::Link), m_documentLoader->url(), *m_frame->document(), LinkLoader::MediaAttributeCheck::SkipMediaAttributeCheck);
    ResourceLoader::didReceiveResponse(response, [this, protectedThis = WTFMove(protectedThis), isResponseMultipart, completionHandlerCaller = WTFMove(completionHandlerCaller)]() mutable {
        if (reachedTerminalState())
            return;

        // FIXME: Main resources have a different set of rules for multipart than images do.
        // Hopefully we can merge those 2 paths.
        if (isResponseMultipart && m_resource->type() != CachedResource::Type::MainResource) {
            m_loadingMultipartContent = true;

            // We don't count multiParts in a CachedResourceLoader's request count
            m_requestCountTracker = WTF::nullopt;
            if (!m_resource->isImage()) {
                RELEASE_LOG_IF_ALLOWED("didReceiveResponse: canceling load because something about a multi-part non-image");
                cancel();
                return;
            }
        }

        auto* buffer = resourceData();
        if (m_loadingMultipartContent && buffer && buffer->size()) {
            // The resource data will change as the next part is loaded, so we need to make a copy.
            m_resource->finishLoading(buffer->copy().ptr(), { });
            clearResourceData();
            // Since a subresource loader does not load multipart sections progressively, data was delivered to the loader all at once.
            // After the first multipart section is complete, signal to delegates that this load is "finished"
            NetworkLoadMetrics emptyMetrics;
            m_documentLoader->subresourceLoaderFinishedLoadingOnePart(this);
            didFinishLoadingOnePart(emptyMetrics);
        }

        checkForHTTPStatusCodeError();

        if (m_inAsyncResponsePolicyCheck)
            m_policyForResponseCompletionHandler = completionHandlerCaller.release();
    });
}

void SubresourceLoader::didReceiveResponsePolicy()
{
    ASSERT(m_inAsyncResponsePolicyCheck);
    m_inAsyncResponsePolicyCheck = false;
    if (auto completionHandler = WTFMove(m_policyForResponseCompletionHandler))
        completionHandler();
}

void SubresourceLoader::didReceiveData(const char* data, unsigned length, long long encodedDataLength, DataPayloadType dataPayloadType)
{
#if USE(QUICK_LOOK)
    if (auto previewLoader = m_previewLoader.get()) {
        if (previewLoader->didReceiveData(data, length))
            return;
    }
#endif

    didReceiveDataOrBuffer(data, length, nullptr, encodedDataLength, dataPayloadType);
}

void SubresourceLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
#if USE(QUICK_LOOK)
    if (auto previewLoader = m_previewLoader.get()) {
        if (previewLoader->didReceiveBuffer(buffer.get()))
            return;
    }
#endif

    didReceiveDataOrBuffer(nullptr, 0, WTFMove(buffer), encodedDataLength, dataPayloadType);
}

void SubresourceLoader::didReceiveDataOrBuffer(const char* data, int length, RefPtr<SharedBuffer>&& buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
    ASSERT(m_resource);

    if (m_resource->response().httpStatusCode() >= 400 && !m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return;
    ASSERT(!m_resource->resourceToRevalidate());
    ASSERT(!m_resource->errorOccurred());
    ASSERT(m_state == Initialized);
    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    Ref<SubresourceLoader> protectedThis(*this);

    ResourceLoader::didReceiveDataOrBuffer(data, length, buffer.copyRef(), encodedDataLength, dataPayloadType);

    if (!m_loadingMultipartContent) {
        if (auto* resourceData = this->resourceData())
            m_resource->updateBuffer(*resourceData);
        else
            m_resource->updateData(buffer ? buffer->data() : data, buffer ? buffer->size() : length);
    }
}

bool SubresourceLoader::checkForHTTPStatusCodeError()
{
    if (m_resource->response().httpStatusCode() < 400 || m_resource->shouldIgnoreHTTPStatusCodeErrors())
        return false;

    m_state = Finishing;
    m_resource->error(CachedResource::LoadError);
    cancel();
    return true;
}

static void logResourceLoaded(Frame* frame, CachedResource::Type type)
{
    if (!frame || !frame->page())
        return;

    String resourceType;
    switch (type) {
    case CachedResource::Type::MainResource:
        resourceType = DiagnosticLoggingKeys::mainResourceKey();
        break;
    case CachedResource::Type::ImageResource:
        resourceType = DiagnosticLoggingKeys::imageKey();
        break;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::CSSStyleSheet:
        resourceType = DiagnosticLoggingKeys::styleSheetKey();
        break;
    case CachedResource::Type::Script:
        resourceType = DiagnosticLoggingKeys::scriptKey();
        break;
    case CachedResource::Type::FontResource:
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
        resourceType = DiagnosticLoggingKeys::fontKey();
        break;
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
        resourceType = DiagnosticLoggingKeys::rawKey();
        break;
    case CachedResource::Type::SVGDocumentResource:
        resourceType = DiagnosticLoggingKeys::svgDocumentKey();
        break;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        resourceType = DiagnosticLoggingKeys::applicationManifestKey();
        break;
#endif
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::TextTrackResource:
        resourceType = DiagnosticLoggingKeys::otherKey();
        break;
    }
    
    frame->page()->diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::resourceLoadedKey(), resourceType, ShouldSample::Yes);
}

Expected<void, String> SubresourceLoader::checkResponseCrossOriginAccessControl(const ResourceResponse& response)
{
    if (!m_resource->isCrossOrigin() || options().mode != FetchOptions::Mode::Cors)
        return { };

#if ENABLE(SERVICE_WORKER)
    if (response.source() == ResourceResponse::Source::ServiceWorker) {
        if (response.tainting() == ResourceResponse::Tainting::Opaque) {
            // FIXME: This should have an error message.
            return makeUnexpected(String());
        }
        return { };
    }
#endif

    ASSERT(m_origin);

    return passesAccessControlCheck(response, options().credentials == FetchOptions::Credentials::Include ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse, *m_origin, &CrossOriginAccessControlCheckDisabler::singleton());
}

Expected<void, String> SubresourceLoader::checkRedirectionCrossOriginAccessControl(const ResourceRequest& previousRequest, const ResourceResponse& redirectResponse, ResourceRequest& newRequest)
{
    bool crossOriginFlag = m_resource->isCrossOrigin();
    bool isNextRequestCrossOrigin = m_origin && !m_origin->canRequest(newRequest.url());

    if (isNextRequestCrossOrigin)
        m_resource->setCrossOrigin();

    ASSERT(options().mode != FetchOptions::Mode::SameOrigin || !m_resource->isCrossOrigin());

    // Implementing https://fetch.spec.whatwg.org/#concept-http-redirect-fetch step 7 & 8.
    if (options().mode == FetchOptions::Mode::Cors) {
        if (m_resource->isCrossOrigin()) {
            auto locationString = redirectResponse.httpHeaderField(HTTPHeaderName::Location);
            String errorMessage = validateCrossOriginRedirectionURL(URL(redirectResponse.url(), locationString));
            if (!errorMessage.isNull())
                return makeUnexpected(WTFMove(errorMessage));
        }

        ASSERT(m_origin);
        if (crossOriginFlag) {
            auto accessControlCheckResult = passesAccessControlCheck(redirectResponse, options().storedCredentialsPolicy, *m_origin, &CrossOriginAccessControlCheckDisabler::singleton());
            if (!accessControlCheckResult)
                return accessControlCheckResult;
        }
    }

    bool redirectingToNewOrigin = false;
    if (m_resource->isCrossOrigin()) {
        if (!crossOriginFlag && isNextRequestCrossOrigin)
            redirectingToNewOrigin = true;
        else
            redirectingToNewOrigin = !protocolHostAndPortAreEqual(previousRequest.url(), newRequest.url());
    }

    // Implementing https://fetch.spec.whatwg.org/#concept-http-redirect-fetch step 10.
    if (crossOriginFlag && redirectingToNewOrigin)
        m_origin = SecurityOrigin::createUnique();

    newRequest.redirectAsGETIfNeeded(previousRequest, redirectResponse);

    // Implementing https://fetch.spec.whatwg.org/#concept-http-redirect-fetch step 14.
    updateReferrerPolicy(redirectResponse.httpHeaderField(HTTPHeaderName::ReferrerPolicy));

    if (options().mode == FetchOptions::Mode::Cors && redirectingToNewOrigin) {
        cleanHTTPRequestHeadersForAccessControl(newRequest, options().httpHeadersToKeep);
        updateRequestForAccessControl(newRequest, *m_origin, options().storedCredentialsPolicy);
    }

    updateRequestReferrer(newRequest, referrerPolicy(), previousRequest.httpReferrer());

    FrameLoader::addHTTPOriginIfNeeded(newRequest, m_origin ? m_origin->toString() : String());

    return { };
}

void SubresourceLoader::updateReferrerPolicy(const String& referrerPolicyValue)
{
    if (auto referrerPolicy = parseReferrerPolicy(referrerPolicyValue, ReferrerPolicySource::HTTPHeader)) {
        ASSERT(*referrerPolicy != ReferrerPolicy::EmptyString);
        setReferrerPolicy(*referrerPolicy);
    }
}

void SubresourceLoader::didFinishLoading(const NetworkLoadMetrics& networkLoadMetrics)
{
    RELEASE_LOG_IF_ALLOWED("didFinishLoading:");

#if USE(QUICK_LOOK)
    if (auto previewLoader = m_previewLoader.get()) {
        if (previewLoader->didFinishLoading())
            return;
    }
#endif

    if (m_state != Initialized)
        return;
    ASSERT(!reachedTerminalState());
    ASSERT(!m_resource->resourceToRevalidate());
    // FIXME (129394): We should cancel the load when a decode error occurs instead of continuing the load to completion.
    ASSERT(!m_resource->errorOccurred() || m_resource->status() == CachedResource::DecodeError || !m_resource->isLoading());
    LOG(ResourceLoading, "Received '%s'.", m_resource->url().string().latin1().data());
    logResourceLoaded(m_frame.get(), m_resource->type());

    Ref<SubresourceLoader> protectedThis(*this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);

    // FIXME: Remove this with deprecatedNetworkLoadMetrics.
    m_loadTiming.setResponseEnd(MonotonicTime::now());

    if (networkLoadMetrics.isComplete())
        reportResourceTiming(networkLoadMetrics);
    else {
        // This is the legacy path for platforms (and ResourceHandle paths) that do not provide
        // complete load metrics in didFinishLoad. In those cases, fall back to the possibility
        // that they populated partial load timing information on the ResourceResponse.
        const auto* timing = m_resource->response().deprecatedNetworkLoadMetricsOrNull();
        Optional<NetworkLoadMetrics> empty;
        if (!timing) {
            empty.emplace();
            timing = &empty.value();
        }
        reportResourceTiming(*timing);
    }

    if (m_resource->type() != CachedResource::Type::MainResource)
        tracePoint(SubresourceLoadDidEnd);

    m_state = Finishing;
    m_resource->finishLoading(resourceData(), networkLoadMetrics);

    if (wasCancelled()) {
        RELEASE_LOG_IF_ALLOWED("didFinishLoading: was canceled");
        return;
    }

    m_resource->finish();
    ASSERT(!reachedTerminalState());
    didFinishLoadingOnePart(networkLoadMetrics);
    notifyDone(LoadCompletionType::Finish);

    if (reachedTerminalState()) {
        RELEASE_LOG_IF_ALLOWED("didFinishLoading: reached terminal state");
        return;
    }
    releaseResources();
}

void SubresourceLoader::didFail(const ResourceError& error)
{
    RELEASE_LOG_IF_ALLOWED("didFail: (type = %d, code = %d)", static_cast<int>(error.type()), error.errorCode());

#if USE(QUICK_LOOK)
    if (auto previewLoader = m_previewLoader.get())
        previewLoader->didFail();
#endif

    if (m_state != Initialized)
        return;

    ASSERT(!reachedTerminalState());
    LOG(ResourceLoading, "Failed to load '%s'.\n", m_resource->url().string().latin1().data());

    if (m_frame->document() && error.isAccessControl() && m_resource->type() != CachedResource::Type::Ping)
        m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, error.localizedDescription());

    Ref<SubresourceLoader> protectedThis(*this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;

    if (m_resource->type() != CachedResource::Type::MainResource)
        tracePoint(SubresourceLoadDidEnd);

    if (m_resource->resourceToRevalidate())
        MemoryCache::singleton().revalidationFailed(*m_resource);
    m_resource->setResourceError(error);
    if (!m_resource->isPreloaded())
        MemoryCache::singleton().remove(*m_resource);
    m_resource->error(CachedResource::LoadError);
    cleanupForError(error);
    notifyDone(LoadCompletionType::Cancel);
    if (reachedTerminalState())
        return;
    releaseResources();
}

void SubresourceLoader::willCancel(const ResourceError& error)
{
    RELEASE_LOG_IF_ALLOWED("willCancel: (type = %d, code = %d)", static_cast<int>(error.type()), error.errorCode());

#if PLATFORM(IOS_FAMILY)
    // Since we defer initialization to scheduling time on iOS but
    // CachedResourceLoader stores resources in the memory cache immediately,
    // m_resource might be cached despite its loader not being initialized.
    if (m_state != Initialized && m_state != Uninitialized)
#else
    if (m_state != Initialized)
#endif
        return;

    ASSERT(!reachedTerminalState());
    LOG(ResourceLoading, "Cancelled load of '%s'.\n", m_resource->url().string().latin1().data());

    Ref<SubresourceLoader> protectedThis(*this);
#if PLATFORM(IOS_FAMILY)
    m_state = m_state == Uninitialized ? CancelledWhileInitializing : Finishing;
#else
    m_state = Finishing;
#endif
    auto& memoryCache = MemoryCache::singleton();
    if (m_resource->resourceToRevalidate())
        memoryCache.revalidationFailed(*m_resource);
    m_resource->setResourceError(error);
    memoryCache.remove(*m_resource);
}

void SubresourceLoader::didCancel(const ResourceError&)
{
    if (m_state == Uninitialized || reachedTerminalState())
        return;

    ASSERT(m_resource);

    if (m_resource->type() != CachedResource::Type::MainResource)
        tracePoint(SubresourceLoadDidEnd);

    m_resource->cancelLoad();
    notifyDone(LoadCompletionType::Cancel);
}

void SubresourceLoader::notifyDone(LoadCompletionType type)
{
    if (reachedTerminalState())
        return;

    m_requestCountTracker = WTF::nullopt;
    bool shouldPerformPostLoadActions = true;
#if PLATFORM(IOS_FAMILY)
    if (m_state == CancelledWhileInitializing)
        shouldPerformPostLoadActions = false;
#endif
    m_documentLoader->cachedResourceLoader().loadDone(type, shouldPerformPostLoadActions);
    if (reachedTerminalState())
        return;
    m_documentLoader->removeSubresourceLoader(type, this);
}

void SubresourceLoader::releaseResources()
{
    ASSERT(!reachedTerminalState());
#if PLATFORM(IOS_FAMILY)
    if (m_state != Uninitialized && m_state != CancelledWhileInitializing)
#else
    if (m_state != Uninitialized)
#endif
        m_resource->clearLoader();
    m_resource = nullptr;
    ResourceLoader::releaseResources();
}

void SubresourceLoader::reportResourceTiming(const NetworkLoadMetrics& networkLoadMetrics)
{
    if (!ResourceTimingInformation::shouldAddResourceTiming(*m_resource))
        return;

    Document* document = m_documentLoader->cachedResourceLoader().document();
    if (!document)
        return;

    SecurityOrigin& origin = m_origin ? *m_origin : document->securityOrigin();
    auto resourceTiming = ResourceTiming::fromLoad(*m_resource, m_resource->initiatorName(), m_loadTiming, networkLoadMetrics, origin);

    // Worker resources loaded here are all CachedRawResources loaded through WorkerThreadableLoader.
    // Pass the ResourceTiming information on so that WorkerThreadableLoader may add them to the
    // Worker's Performance object.
    if (options().initiatorContext == InitiatorContext::Worker) {
        ASSERT(m_origin);
        ASSERT(is<CachedRawResource>(m_resource));
        downcast<CachedRawResource>(*m_resource).finishedTimingForWorkerLoad(WTFMove(resourceTiming));
        return;
    }

    ASSERT(options().initiatorContext == InitiatorContext::Document);
    m_documentLoader->cachedResourceLoader().resourceTimingInformation().addResourceTiming(*m_resource, *document, WTFMove(resourceTiming));
}

const HTTPHeaderMap* SubresourceLoader::originalHeaders() const
{
    return (m_resource  && m_resource->originalRequest()) ? &m_resource->originalRequest()->httpHeaderFields() : nullptr;
}

} // namespace WebCore

#undef PAGE_ID
#undef FRAME_ID
#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
