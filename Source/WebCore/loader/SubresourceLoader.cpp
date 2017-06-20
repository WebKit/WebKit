/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "Page.h"
#include "ResourceLoadObserver.h"
#include "ResourceTiming.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/Ref.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS)
#include <RuntimeApplicationChecks.h>
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include "ResourceLoadInfo.h"
#endif

#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

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
    , m_loadingMultipartContent(false)
    , m_state(Uninitialized)
    , m_requestCountTracker(std::in_place, frame.document()->cachedResourceLoader(), resource)
{
#ifndef NDEBUG
    subresourceLoaderCounter.increment();
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    m_resourceType = toResourceType(resource.type());
#endif
}

SubresourceLoader::~SubresourceLoader()
{
    ASSERT(m_state != Initialized);
    ASSERT(reachedTerminalState());
#ifndef NDEBUG
    subresourceLoaderCounter.decrement();
#endif
}

RefPtr<SubresourceLoader> SubresourceLoader::create(Frame& frame, CachedResource& resource, const ResourceRequest& request, const ResourceLoaderOptions& options)
{
    RefPtr<SubresourceLoader> subloader(adoptRef(new SubresourceLoader(frame, resource, options)));
#if PLATFORM(IOS)
    if (!IOSApplication::isWebProcess()) {
        // On iOS, do not invoke synchronous resource load delegates while resource load scheduling
        // is disabled to avoid re-entering style selection from a different thread (see <rdar://problem/9121719>).
        // FIXME: This should be fixed for all ports in <https://bugs.webkit.org/show_bug.cgi?id=56647>.
        subloader->m_iOSOriginalRequest = request;
        return subloader.release();
    }
#endif
    if (!subloader->init(request))
        return nullptr;
    return subloader;
}
    
#if PLATFORM(IOS)
bool SubresourceLoader::startLoading()
{
    ASSERT(!IOSApplication::isWebProcess());
    if (!init(m_iOSOriginalRequest))
        return false;
    m_iOSOriginalRequest = ResourceRequest();
    start();
    return true;
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

bool SubresourceLoader::init(const ResourceRequest& request)
{
    if (!ResourceLoader::init(request))
        return false;

    ASSERT(!reachedTerminalState());
    m_state = Initialized;
    m_documentLoader->addSubresourceLoader(this);

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=155633.
    // SubresourceLoader could use the document origin as a default and set PotentiallyCrossOriginEnabled requests accordingly.
    // This would simplify resource loader users as they would only need to set fetch mode to Cors.
    m_origin = m_resource->origin();

    return true;
}

bool SubresourceLoader::isSubresourceLoader()
{
    return true;
}

void SubresourceLoader::willSendRequestInternal(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Store the previous URL because the call to ResourceLoader::willSendRequest will modify it.
    URL previousURL = request().url();
    Ref<SubresourceLoader> protectedThis(*this);

    if (!newRequest.url().isValid()) {
        cancel(cannotShowURLError());
        return;
    }

    if (newRequest.requester() != ResourceRequestBase::Requester::Main)
        ResourceLoadObserver::sharedObserver().logSubresourceLoading(m_frame.get(), newRequest, redirectResponse);

    ASSERT(!newRequest.isNull());
    if (!redirectResponse.isNull()) {
        if (options().redirect != FetchOptions::Redirect::Follow) {
            if (options().redirect == FetchOptions::Redirect::Error) {
                cancel();
                return;
            }

            ResourceResponse opaqueRedirectedResponse;
            opaqueRedirectedResponse.setURL(redirectResponse.url());
            opaqueRedirectedResponse.setType(ResourceResponse::Type::Opaqueredirect);
            m_resource->responseReceived(opaqueRedirectedResponse);
            didFinishLoading(currentTime());
            return;
        } else if (m_redirectCount++ >= options().maxRedirectCount) {
            cancel(ResourceError(String(), 0, request().url(), ASCIILiteral("Too many redirections"), ResourceError::Type::General));
            return;
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
            cancel();
            return;
        }

        String errorDescription;
        if (!checkRedirectionCrossOriginAccessControl(request(), redirectResponse, newRequest, errorDescription)) {
            String errorMessage = "Cross-origin redirection to " + newRequest.url().string() + " denied by Cross-Origin Resource Sharing policy: " + errorDescription;
            if (m_frame && m_frame->document())
                m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, errorMessage);
            cancel(ResourceError(String(), 0, request().url(), errorMessage, ResourceError::Type::AccessControl));
            return;
        }

        if (m_resource->isImage() && m_documentLoader->cachedResourceLoader().shouldDeferImageLoad(newRequest.url())) {
            cancel();
            return;
        }
        m_loadTiming.addRedirect(redirectResponse.url(), newRequest.url());
        m_resource->redirectReceived(newRequest, redirectResponse);
    }

    if (newRequest.isNull() || reachedTerminalState())
        return;

    ResourceLoader::willSendRequestInternal(newRequest, redirectResponse);

    if (reachedTerminalState())
        return;

    if (newRequest.isNull()) {
        cancel();
        return;
    }

    if (m_resource->type() == CachedResource::MainResource && !redirectResponse.isNull())
        m_documentLoader->willContinueMainResourceLoadAfterRedirect(newRequest);
}

void SubresourceLoader::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_state == Initialized);
    Ref<SubresourceLoader> protectedThis(*this);
    m_resource->didSendData(bytesSent, totalBytesToBeSent);
}

#if USE(QUICK_LOOK)

bool SubresourceLoader::shouldCreateQuickLookHandleForResponse(const ResourceResponse& response) const
{
    if (m_resource->type() != CachedResource::MainResource)
        return false;

    if (m_quickLookHandle)
        return false;

    return QuickLookHandle::shouldCreateForMIMEType(response.mimeType());
}

#endif

void SubresourceLoader::didReceiveResponse(const ResourceResponse& response)
{
    ASSERT(!response.isNull());
    ASSERT(m_state == Initialized);

#if USE(QUICK_LOOK)
    if (shouldCreateQuickLookHandleForResponse(response)) {
        m_quickLookHandle = QuickLookHandle::create(*this, response);
        return;
    }
#endif

    // We want redirect responses to be processed through willSendRequestInternal. The only exception is redirection with no Location headers.
    ASSERT(response.httpStatusCode() < 300 || response.httpStatusCode() >= 400 || response.httpStatusCode() == 304 || !response.httpHeaderField(HTTPHeaderName::Location));

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    Ref<SubresourceLoader> protectedThis(*this);

    if (shouldIncludeCertificateInfo())
        response.includeCertificateInfo();

    if (m_resource->resourceToRevalidate()) {
        if (response.httpStatusCode() == 304) {
            // 304 Not modified / Use local copy
            // Existing resource is ok, just use it updating the expiration time.
            m_resource->setResponse(response);
            MemoryCache::singleton().revalidationSucceeded(*m_resource, response);
            if (m_frame && m_frame->page())
                m_frame->page()->diagnosticLoggingClient().logDiagnosticMessageWithResult(DiagnosticLoggingKeys::cachedResourceRevalidationKey(), emptyString(), DiagnosticLoggingResultPass, ShouldSample::Yes);
            if (!reachedTerminalState())
                ResourceLoader::didReceiveResponse(response);
            return;
        }
        // Did not get 304 response, continue as a regular resource load.
        MemoryCache::singleton().revalidationFailed(*m_resource);
        if (m_frame && m_frame->page())
            m_frame->page()->diagnosticLoggingClient().logDiagnosticMessageWithResult(DiagnosticLoggingKeys::cachedResourceRevalidationKey(), emptyString(), DiagnosticLoggingResultFail, ShouldSample::Yes);
    }

    String errorDescription;
    if (!checkResponseCrossOriginAccessControl(response, errorDescription)) {
        if (m_frame && m_frame->document())
            m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, errorDescription);
        cancel(ResourceError(String(), 0, request().url(), errorDescription, ResourceError::Type::AccessControl));
        return;
    }

    m_resource->responseReceived(response);
    if (reachedTerminalState())
        return;

    ResourceLoader::didReceiveResponse(response);
    if (reachedTerminalState())
        return;

    // FIXME: Main resources have a different set of rules for multipart than images do.
    // Hopefully we can merge those 2 paths.
    if (response.isMultipart() && m_resource->type() != CachedResource::MainResource) {
        m_loadingMultipartContent = true;

        // We don't count multiParts in a CachedResourceLoader's request count
        m_requestCountTracker = std::nullopt;
        if (!m_resource->isImage()) {
            cancel();
            return;
        }
    }

    auto* buffer = resourceData();
    if (m_loadingMultipartContent && buffer && buffer->size()) {
        // The resource data will change as the next part is loaded, so we need to make a copy.
        m_resource->finishLoading(buffer->copy().ptr());
        clearResourceData();
        // Since a subresource loader does not load multipart sections progressively, data was delivered to the loader all at once.
        // After the first multipart section is complete, signal to delegates that this load is "finished"
        m_documentLoader->subresourceLoaderFinishedLoadingOnePart(this);
        didFinishLoadingOnePart(0);
    }

    checkForHTTPStatusCodeError();
}

void SubresourceLoader::didReceiveData(const char* data, unsigned length, long long encodedDataLength, DataPayloadType dataPayloadType)
{
#if USE(QUICK_LOOK)
    if (auto quickLookHandle = m_quickLookHandle.get()) {
        if (quickLookHandle->didReceiveData(data, length))
            return;
    }
#endif

    didReceiveDataOrBuffer(data, length, nullptr, encodedDataLength, dataPayloadType);
}

void SubresourceLoader::didReceiveBuffer(Ref<SharedBuffer>&& buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
#if USE(QUICK_LOOK)
    if (auto quickLookHandle = m_quickLookHandle.get()) {
        if (quickLookHandle->didReceiveBuffer(buffer.get()))
            return;
    }
#endif

    didReceiveDataOrBuffer(nullptr, 0, WTFMove(buffer), encodedDataLength, dataPayloadType);
}

void SubresourceLoader::didReceiveDataOrBuffer(const char* data, int length, RefPtr<SharedBuffer>&& buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
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
            m_resource->addDataBuffer(*resourceData);
        else
            m_resource->addData(buffer ? buffer->data() : data, buffer ? buffer->size() : length);
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
    case CachedResource::MainResource:
        resourceType = DiagnosticLoggingKeys::mainResourceKey();
        break;
    case CachedResource::ImageResource:
        resourceType = DiagnosticLoggingKeys::imageKey();
        break;
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
    case CachedResource::CSSStyleSheet:
        resourceType = DiagnosticLoggingKeys::styleSheetKey();
        break;
    case CachedResource::Script:
        resourceType = DiagnosticLoggingKeys::scriptKey();
        break;
    case CachedResource::FontResource:
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
        resourceType = DiagnosticLoggingKeys::fontKey();
        break;
    case CachedResource::MediaResource:
    case CachedResource::RawResource:
        resourceType = DiagnosticLoggingKeys::rawKey();
        break;
    case CachedResource::SVGDocumentResource:
        resourceType = DiagnosticLoggingKeys::svgDocumentKey();
        break;
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
    case CachedResource::LinkSubresource:
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
#endif
        resourceType = DiagnosticLoggingKeys::otherKey();
        break;
    }
    frame->page()->diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::resourceLoadedKey(), resourceType, ShouldSample::Yes);
}

bool SubresourceLoader::checkResponseCrossOriginAccessControl(const ResourceResponse& response, String& errorDescription)
{
    if (!m_resource->isCrossOrigin() || options().mode != FetchOptions::Mode::Cors)
        return true;

    ASSERT(m_origin);
    return passesAccessControlCheck(response, options().allowCredentials, *m_origin, errorDescription);
}

bool SubresourceLoader::checkRedirectionCrossOriginAccessControl(const ResourceRequest& previousRequest, const ResourceResponse& redirectResponse, ResourceRequest& newRequest, String& errorMessage)
{
    bool crossOriginFlag = m_resource->isCrossOrigin();
    bool isNextRequestCrossOrigin = m_origin && !m_origin->canRequest(newRequest.url());

    if (isNextRequestCrossOrigin)
        m_resource->setCrossOrigin();

    ASSERT(options().mode != FetchOptions::Mode::SameOrigin || !m_resource->isCrossOrigin());

    if (options().mode != FetchOptions::Mode::Cors)
        return true;

    // Implementing https://fetch.spec.whatwg.org/#concept-http-redirect-fetch step 8 & 9.
    if (m_resource->isCrossOrigin() && !isValidCrossOriginRedirectionURL(newRequest.url())) {
        errorMessage = ASCIILiteral("URL is either a non-HTTP URL or contains credentials.");
        return false;
    }

    ASSERT(m_origin);
    if (crossOriginFlag && !passesAccessControlCheck(redirectResponse, options().allowCredentials, *m_origin, errorMessage))
        return false;

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

    if (redirectingToNewOrigin) {
        cleanRedirectedRequestForAccessControl(newRequest);
        updateRequestForAccessControl(newRequest, *m_origin, options().allowCredentials);
    }

    return true;
}

void SubresourceLoader::didFinishLoading(double finishTime)
{
#if USE(QUICK_LOOK)
    if (auto quickLookHandle = m_quickLookHandle.get()) {
        if (quickLookHandle->didFinishLoading())
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

    // FIXME: <https://webkit.org/b/168351> [Resource Timing] Gather timing information with reliable responseEnd time
    // The finishTime that is passed in is from the NetworkProcess and is more accurate.
    // However, all other load times are generated from the web process or offsets.
    // Mixing times from different processes can cause the finish time to be earlier than
    // the response received time due to inter-process communication lag. This could be solved
    // by gathering NetworkLoadTiming information at completion time instead of at
    // didReceiveResponse time.
    UNUSED_PARAM(finishTime);
    MonotonicTime responseEndTime = MonotonicTime::now();
    m_loadTiming.setResponseEnd(responseEndTime);

#if ENABLE(WEB_TIMING)
    reportResourceTiming();
#endif

    m_state = Finishing;
    m_resource->setLoadFinishTime(responseEndTime.secondsSinceEpoch().seconds()); // FIXME: Users of the loadFinishTime should use the LoadTiming struct instead.
    m_resource->finishLoading(resourceData());

    if (wasCancelled())
        return;
    m_resource->finish();
    ASSERT(!reachedTerminalState());
    didFinishLoadingOnePart(responseEndTime.secondsSinceEpoch().seconds());
    notifyDone();
    if (reachedTerminalState())
        return;
    releaseResources();
}

void SubresourceLoader::didFail(const ResourceError& error)
{
#if USE(QUICK_LOOK)
    if (auto quickLookHandle = m_quickLookHandle.get())
        quickLookHandle->didFail();
#endif

    if (m_state != Initialized)
        return;
    ASSERT(!reachedTerminalState());
    LOG(ResourceLoading, "Failed to load '%s'.\n", m_resource->url().string().latin1().data());

    Ref<SubresourceLoader> protectedThis(*this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    if (m_resource->resourceToRevalidate())
        MemoryCache::singleton().revalidationFailed(*m_resource);
    m_resource->setResourceError(error);
    if (!m_resource->isPreloaded())
        MemoryCache::singleton().remove(*m_resource);
    m_resource->error(CachedResource::LoadError);
    cleanupForError(error);
    notifyDone();
    if (reachedTerminalState())
        return;
    releaseResources();
}

void SubresourceLoader::willCancel(const ResourceError& error)
{
#if PLATFORM(IOS)
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
#if PLATFORM(IOS)
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
    if (m_state == Uninitialized)
        return;

    m_resource->cancelLoad();
    notifyDone();
}

void SubresourceLoader::didRetrieveDerivedDataFromCache(const String& type, SharedBuffer& buffer)
{
    if (m_state != Initialized)
        return;
    m_resource->didRetrieveDerivedDataFromCache(type, buffer);
}

void SubresourceLoader::notifyDone()
{
    if (reachedTerminalState())
        return;

    m_requestCountTracker = std::nullopt;
#if PLATFORM(IOS)
    m_documentLoader->cachedResourceLoader().loadDone(m_state != CancelledWhileInitializing);
#else
    m_documentLoader->cachedResourceLoader().loadDone();
#endif
    if (reachedTerminalState())
        return;
    m_documentLoader->removeSubresourceLoader(this);
}

void SubresourceLoader::releaseResources()
{
    ASSERT(!reachedTerminalState());
#if PLATFORM(IOS)
    if (m_state != Uninitialized && m_state != CancelledWhileInitializing)
#else
    if (m_state != Uninitialized)
#endif
        m_resource->clearLoader();
    m_resource = nullptr;
    ResourceLoader::releaseResources();
}

#if ENABLE(WEB_TIMING)
void SubresourceLoader::reportResourceTiming()
{
    if (!RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
        return;

    if (!ResourceTimingInformation::shouldAddResourceTiming(*m_resource))
        return;

    Document* document = m_documentLoader->cachedResourceLoader().document();
    if (!document)
        return;
    
    SecurityOrigin& origin = m_origin ? *m_origin : document->securityOrigin();
    ResourceTiming resourceTiming = ResourceTiming::fromLoad(*m_resource, m_resource->initiatorName(), m_loadTiming, origin);

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
#endif

}
