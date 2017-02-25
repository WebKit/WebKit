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
#include "DocumentThreadableLoader.h"
#include "InspectorInstrumentation.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

CrossOriginPreflightChecker::CrossOriginPreflightChecker(DocumentThreadableLoader& loader, ResourceRequest&& request)
    : m_loader(loader)
    , m_request(WTFMove(request))
{
}

CrossOriginPreflightChecker::~CrossOriginPreflightChecker()
{
    if (m_resource)
        m_resource->removeClient(*this);
}

void CrossOriginPreflightChecker::validatePreflightResponse(DocumentThreadableLoader& loader, ResourceRequest&& request, unsigned long identifier, const ResourceResponse& response)
{
    Frame* frame = loader.document().frame();
    ASSERT(frame);

    if (!response.isSuccessful()) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), ASCIILiteral("Preflight response is not successful"), ResourceError::Type::AccessControl));
        return;
    }

    String description;
    if (!passesAccessControlCheck(response, loader.options().allowCredentials, loader.securityOrigin(), description)) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), description, ResourceError::Type::AccessControl));
        return;
    }

    auto result = std::make_unique<CrossOriginPreflightResultCacheItem>(loader.options().allowCredentials);
    if (!result->parse(response, description)
        || !result->allowsCrossOriginMethod(request.httpMethod(), description)
        || !result->allowsCrossOriginHeaders(request.httpHeaderFields(), description)) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), description, ResourceError::Type::AccessControl));
        return;
    }

    // FIXME: <https://webkit.org/b/164889> Web Inspector: Show Preflight Request information in inspector
    // This is only showing success preflight requests and responses but we should show network events
    // for preflight failures and distinguish them better from non-preflight requests.
    InspectorInstrumentation::didReceiveResourceResponse(*frame, identifier, frame->loader().documentLoader(), response, nullptr);
    InspectorInstrumentation::didFinishLoading(frame, frame->loader().documentLoader(), identifier, 0);

    CrossOriginPreflightResultCache::singleton().appendEntry(loader.securityOrigin().toString(), request.url(), WTFMove(result));
    loader.preflightSuccess(WTFMove(request));
}

void CrossOriginPreflightChecker::notifyFinished(CachedResource& resource)
{
    ASSERT_UNUSED(resource, &resource == m_resource);
    if (m_resource->loadFailedOrCanceled()) {
        ResourceError preflightError = m_resource->resourceError();
        // If the preflight was cancelled by underlying code, it probably means the request was blocked due to some access control policy.
        // FIXME:: According fetch, we should just pass the error to the layer above. But this may impact some clients like XHR or EventSource.
        if (preflightError.isNull() || preflightError.isCancellation() || preflightError.isGeneral())
            preflightError.setType(ResourceError::Type::AccessControl);

        m_loader.preflightFailure(m_resource->identifier(), preflightError);
        return;
    }
    validatePreflightResponse(m_loader, WTFMove(m_request), m_resource->identifier(), m_resource->response());
}

void CrossOriginPreflightChecker::startPreflight()
{
    ResourceLoaderOptions options;
    options.referrerPolicy = m_loader.options().referrerPolicy;
    options.redirect = FetchOptions::Redirect::Manual;
    options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;

    CachedResourceRequest preflightRequest(createAccessControlPreflightRequest(m_request, m_loader.securityOrigin(), m_loader.referrer()), options);
    if (RuntimeEnabledFeatures::sharedFeatures().resourceTimingEnabled())
        preflightRequest.setInitiator(m_loader.options().initiator);

    ASSERT(!m_resource);
    m_resource = m_loader.document().cachedResourceLoader().requestRawResource(WTFMove(preflightRequest));
    if (m_resource)
        m_resource->addClient(*this);
}

void CrossOriginPreflightChecker::doPreflight(DocumentThreadableLoader& loader, ResourceRequest&& request)
{
    if (!loader.document().frame())
        return;

    ResourceRequest preflightRequest = createAccessControlPreflightRequest(request, loader.securityOrigin(), loader.referrer());
    ResourceError error;
    ResourceResponse response;
    RefPtr<SharedBuffer> data;

    unsigned identifier = loader.document().frame()->loader().loadResourceSynchronously(preflightRequest, DoNotAllowStoredCredentials, ClientCredentialPolicy::CannotAskClientForCredentials, error, response, data);

    if (!error.isNull()) {
        // If the preflight was cancelled by underlying code, it probably means the request was blocked due to some access control policy.
        // FIXME:: According fetch, we should just pass the error to the layer above. But this may impact some clients like XHR or EventSource.
        if (error.isCancellation() || error.isGeneral())
            error.setType(ResourceError::Type::AccessControl);
        loader.preflightFailure(identifier, error);
        return;
    }

    // FIXME: Ideally, we should ask platformLoadResourceSynchronously to set ResourceResponse isRedirected and use it here.
    bool isRedirect = preflightRequest.url().strippedForUseAsReferrer() != response.url().strippedForUseAsReferrer();
    if (isRedirect || !response.isSuccessful()) {
        loader.preflightFailure(identifier, ResourceError(errorDomainWebKitInternal, 0, request.url(), ASCIILiteral("Preflight response is not successful"), ResourceError::Type::AccessControl));
        return;
    }

    validatePreflightResponse(loader, WTFMove(request), identifier, response);
}

void CrossOriginPreflightChecker::setDefersLoading(bool value)
{
    if (m_resource)
        m_resource->setDefersLoading(value);
}

bool CrossOriginPreflightChecker::isXMLHttpRequest() const
{
    return m_loader.isXMLHttpRequest();
}

} // namespace WebCore
