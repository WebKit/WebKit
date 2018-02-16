/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ApplicationCacheResourceLoader.h"

#include "CachedResourceLoader.h"

namespace WebCore {

RefPtr<ApplicationCacheResourceLoader> ApplicationCacheResourceLoader::create(CachedResourceLoader& loader, ResourceRequest&& request, CompletionHandler<void(ResourceOrError&&)>&& callback)
{
    ResourceLoaderOptions options;
    options.storedCredentialsPolicy = StoredCredentialsPolicy::Use;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    options.applicationCacheMode = ApplicationCacheMode::Bypass;
    CachedResourceRequest cachedResourceRequest { WTFMove(request), options };
    auto resource = loader.requestRawResource(WTFMove(cachedResourceRequest));
    if (!resource.has_value()) {
        callback(makeUnexpected(Error::CannotCreateResource));
        return nullptr;
    }
    return adoptRef(*new ApplicationCacheResourceLoader { WTFMove(resource.value()), WTFMove(callback) });
}

ApplicationCacheResourceLoader::ApplicationCacheResourceLoader(CachedResourceHandle<CachedRawResource>&& resource, CompletionHandler<void(ResourceOrError&&)>&& callback)
    : m_resource(WTFMove(resource))
    , m_callback(WTFMove(callback))
{
    m_resource->addClient(*this);
}

ApplicationCacheResourceLoader::~ApplicationCacheResourceLoader()
{
    cancel(Error::Abort);
}

void ApplicationCacheResourceLoader::cancel(Error error)
{
    if (auto callback = WTFMove(m_callback))
        callback(makeUnexpected(error));

    CachedResourceHandle<CachedRawResource> resource;
    std::swap(resource, m_resource);
    if (resource)
        resource->removeClient(*this);
}

void ApplicationCacheResourceLoader::responseReceived(CachedResource& resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    receivedManifestResponse(response);
}

void ApplicationCacheResourceLoader::dataReceived(CachedResource&, const char* data, int length)
{
    m_applicationCacheResource->data().append(data, length);
}

void ApplicationCacheResourceLoader::redirectReceived(CachedResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& callback)
{
    cancel(Error::RedirectForbidden);
    callback({ });
}

void ApplicationCacheResourceLoader::notifyFinished(CachedResource& resource)
{
    auto protectedThis = makeRef(*this);

    ASSERT_UNUSED(resource, &resource == m_resource);

    if (m_resource->errorOccurred()) {
        cancel(Error::NetworkError);
        return;
    }
    if (auto callback = WTFMove(m_callback))
        callback(WTFMove(m_applicationCacheResource));

    CachedResourceHandle<CachedRawResource> resourceHandle;
    std::swap(resourceHandle, m_resource);
    if (resourceHandle)
        resourceHandle->removeClient(*this);
}

void ApplicationCacheResourceLoader::receivedManifestResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() == 404 || response.httpStatusCode() == 410) {
        cancel(Error::NotFound);
        return;
    }

    if (response.httpStatusCode() == 304) {
        notifyFinished(*m_resource);
        return;
    }

    if (response.httpStatusCode() / 100 != 2) {
        cancel(Error::NotOK);
        return;
    }

    m_applicationCacheResource = ApplicationCacheResource::create(m_resource->url(), response, ApplicationCacheResource::Manifest);
}

} // namespace WebCore
