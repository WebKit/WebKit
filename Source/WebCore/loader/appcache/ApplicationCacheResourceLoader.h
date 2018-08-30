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

#pragma once

#include "ApplicationCacheResource.h"
#include "CachedRawResource.h"
#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CachedResourceLoader;
class ResourceRequest;

class ApplicationCacheResourceLoader final : public RefCounted<ApplicationCacheResourceLoader>, private CachedRawResourceClient {
public:
    enum class Error { Abort, NetworkError, CannotCreateResource, NotFound, NotOK, RedirectForbidden };
    using ResourceOrError = Expected<RefPtr<ApplicationCacheResource>, Error>;

    static RefPtr<ApplicationCacheResourceLoader> create(unsigned, CachedResourceLoader&, ResourceRequest&&, CompletionHandler<void(ResourceOrError&&)>&&);
    ~ApplicationCacheResourceLoader();

    void cancel(Error = Error::Abort);

    const CachedResource* resource() const { return m_resource.get(); }
    bool hasRedirection() const { return m_hasRedirection; }
    unsigned type() const { return m_type; }

private:
    explicit ApplicationCacheResourceLoader(unsigned, CachedResourceHandle<CachedRawResource>&&, CompletionHandler<void(ResourceOrError&&)>&&);

    // CachedRawResourceClient
    void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) final;
    void dataReceived(CachedResource&, const char* data, int dataLength) final;
    void redirectReceived(CachedResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&) final;
    void notifyFinished(CachedResource&) final;

    unsigned m_type;
    CachedResourceHandle<CachedRawResource> m_resource;
    RefPtr<ApplicationCacheResource> m_applicationCacheResource;
    CompletionHandler<void(ResourceOrError&&)> m_callback;
    bool m_hasRedirection { false };
};

} // namespace WebCore
