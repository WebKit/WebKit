/*
 * Copyright (C) 2014 Igalia S.L
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

#if ENABLE(VIDEO)

#include "PolicyChecker.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class PlatformMediaResource;
class ResourceError;
class ResourceRequest;
class ResourceResponse;

class PlatformMediaResourceClient {
public:
    virtual ~PlatformMediaResourceClient() = default;

    virtual void responseReceived(PlatformMediaResource&, const ResourceResponse&, CompletionHandler<void(PolicyChecker::ShouldContinue)>&& completionHandler) { completionHandler(PolicyChecker::ShouldContinue::Yes); }
    virtual void redirectReceived(PlatformMediaResource&, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) { completionHandler(WTFMove(request)); }
    virtual bool shouldCacheResponse(PlatformMediaResource&, const ResourceResponse&) { return true; }
    virtual void dataSent(PlatformMediaResource&, unsigned long long, unsigned long long) { }
    virtual void dataReceived(PlatformMediaResource&, const char*, int) { }
    virtual void accessControlCheckFailed(PlatformMediaResource&, const ResourceError&) { }
    virtual void loadFailed(PlatformMediaResource&, const ResourceError&) { }
    virtual void loadFinished(PlatformMediaResource&) { }
};

class PlatformMediaResourceLoader : public ThreadSafeRefCounted<PlatformMediaResourceLoader, WTF::DestructionThread::Main> {
    WTF_MAKE_NONCOPYABLE(PlatformMediaResourceLoader); WTF_MAKE_FAST_ALLOCATED;
public:
    enum LoadOption {
        BufferData = 1 << 0,
        DisallowCaching = 1 << 1,
    };
    typedef unsigned LoadOptions;

    virtual ~PlatformMediaResourceLoader() = default;

    virtual RefPtr<PlatformMediaResource> requestResource(ResourceRequest&&, LoadOptions) = 0;

protected:
    PlatformMediaResourceLoader() = default;
};

class PlatformMediaResource : public ThreadSafeRefCounted<PlatformMediaResource, WTF::DestructionThread::Main> {
    WTF_MAKE_NONCOPYABLE(PlatformMediaResource); WTF_MAKE_FAST_ALLOCATED;
public:
    PlatformMediaResource() = default;
    virtual ~PlatformMediaResource() = default;
    virtual void stop() { }
    virtual bool didPassAccessControlCheck() const { return false; }

    void setClient(std::unique_ptr<PlatformMediaResourceClient>&& client) { m_client = WTFMove(client); }
    PlatformMediaResourceClient* client() { return m_client.get(); }

protected:
    std::unique_ptr<PlatformMediaResourceClient> m_client;
};

} // namespace WebCore

#endif
