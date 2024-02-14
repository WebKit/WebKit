/*
 * Copyright (C) 2016 Canon Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Canon Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CrossOriginPreflightChecker.h"

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "CrossOriginPreflightResultCache.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentThreadableLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "InspectorInstrumentation.h"
#include "NetworkLoadMetrics.h"
#include "Quirks.h"
#include "SharedBuffer.h"

namespace WebCore {

CrossOriginPreflightChecker::CrossOriginPreflightChecker(DocumentThreadableLoader& loader, ResourceRequest&& request)
    : m_loader(loader)
    , m_request(WTFMove(request))
{
}

CrossOriginPreflightChecker::~CrossOriginPreflightChecker()
{
    if (CachedResourceHandle resource = m_resource)
        resource->removeClient(*this);
}

void CrossOriginPreflightChecker::validatePreflightResponse(DocumentThreadableLoader& loader, ResourceRequest&& request, ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    RefPtr frame = loader.document().frame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr page = loader.document().page();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto result = WebCore::validatePreflightResponse(page->sessionID(), request, response, loader.options().storedCredentialsPolicy, loader.securityOrigin(), &CrossOriginAccessControlCheckDisabler::singleton());
    if (!result) {
        loader.document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, result.error());
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), result.error(), ResourceError::Type::AccessControl));
        return;
    }

    // FIXME: <https://webkit.org/b/164889> Web Inspector: Show Preflight Request information in inspector
    // This is only showing success preflight requests and responses but we should show network events
    // for preflight failures and distinguish them better from non-preflight requests.
    NetworkLoadMetrics emptyMetrics;
    RefPtr documentLoader = frame->loader().documentLoader();
    InspectorInstrumentation::didReceiveResourceResponse(*frame, identifier, documentLoader.get(), response, nullptr);
    InspectorInstrumentation::didFinishLoading(frame.get(), documentLoader.get(), identifier, emptyMetrics, nullptr);

    loader.preflightSuccess(WTFMove(request));
}

void CrossOriginPreflightChecker::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    Ref loader = m_loader.get();
    if (m_resource->loadFailedOrCanceled()) {
        ResourceError preflightError = m_resource->resourceError();
        // If the preflight was cancelled by underlying code, it probably means the request was blocked due to some access control policy.
        // FIXME:: According fetch, we should just pass the error to the layer above. But this may impact some clients like XHR or EventSource.
        if (preflightError.isNull() || preflightError.isCancellation() || preflightError.isGeneral())
            preflightError.setType(ResourceError::Type::AccessControl);

        if (!preflightError.isTimeout())
            loader->document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, "CORS-preflight request was blocked"_s);
        loader->preflightFailure(m_resource->identifier(), preflightError);
        return;
    }
    validatePreflightResponse(loader, WTFMove(m_request), m_resource->identifier(), m_resource->response());
}

Ref<DocumentThreadableLoader> CrossOriginPreflightChecker::protectedLoader() const
{
    return m_loader.get();
}

void CrossOriginPreflightChecker::redirectReceived(CachedResource& resource, ResourceRequest&&, const ResourceResponse& response, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    validatePreflightResponse(protectedLoader(), WTFMove(m_request), m_resource->identifier(), response);
    completionHandler(ResourceRequest { });
}

void CrossOriginPreflightChecker::startPreflight()
{
    Ref loader = m_loader.get();
    ResourceLoaderOptions options;
    options.referrerPolicy = loader->options().referrerPolicy;
    options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;
    options.serviceWorkersMode = ServiceWorkersMode::None;
    options.initiatorContext = loader->options().initiatorContext;

    bool includeFetchMetadata = loader->document().settings().fetchMetadataEnabled() && !loader->document().quirks().shouldDisableFetchMetadata();
    CachedResourceRequest preflightRequest(createAccessControlPreflightRequest(m_request, loader->securityOrigin(), loader->referrer(), includeFetchMetadata), options);
    preflightRequest.setInitiatorType(AtomString { loader->options().initiatorType });

    ASSERT(!m_resource);
    m_resource = loader->document().protectedCachedResourceLoader()->requestRawResource(WTFMove(preflightRequest)).value_or(nullptr);
    if (CachedResourceHandle resource = m_resource)
        resource->addClient(*this);
}

void CrossOriginPreflightChecker::doPreflight(DocumentThreadableLoader& loader, ResourceRequest&& request)
{
    if (!loader.document().frame())
        return;

    bool includeFetchMetadata = loader.document().settings().fetchMetadataEnabled() && !loader.document().quirks().shouldDisableFetchMetadata();
    ResourceRequest preflightRequest = createAccessControlPreflightRequest(request, loader.securityOrigin(), loader.referrer(), includeFetchMetadata);
    ResourceError error;
    ResourceResponse response;
    RefPtr<SharedBuffer> data;

    auto identifier = loader.document().protectedFrame()->checkedLoader()->loadResourceSynchronously(preflightRequest, ClientCredentialPolicy::CannotAskClientForCredentials, FetchOptions { }, { }, error, response, data);

    if (!error.isNull()) {
        // If the preflight was cancelled by underlying code, it probably means the request was blocked due to some access control policy.
        // FIXME:: According fetch, we should just pass the error to the layer above. But this may impact some clients like XHR or EventSource.
        if (error.isCancellation() || error.isGeneral())
            error.setType(ResourceError::Type::AccessControl);

        if (!error.isTimeout())
            loader.protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "CORS-preflight request was blocked"_s);

        loader.preflightFailure(identifier, error);
        return;
    }

    // FIXME: Ideally, we should ask platformLoadResourceSynchronously to set ResourceResponse isRedirected and use it here.
    bool isRedirect = preflightRequest.url().strippedForUseAsReferrer() != response.url().strippedForUseAsReferrer();
    if (isRedirect || !response.isSuccessful()) {
        auto errorMessage = makeString("Preflight response is not successful. Status code: ", response.httpStatusCode());
        loader.protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, errorMessage);

        loader.preflightFailure(identifier, ResourceError { errorDomainWebKitInternal, 0, request.url(), errorMessage, ResourceError::Type::AccessControl });
        return;
    }

    validatePreflightResponse(loader, WTFMove(request), identifier, response);
}

void CrossOriginPreflightChecker::setDefersLoading(bool value)
{
    if (CachedResourceHandle resource = m_resource)
        resource->setDefersLoading(value);
}

} // namespace WebCore
