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

#ifndef MediaResourceLoader_h
#define MediaResourceLoader_h

#if ENABLE(VIDEO)
#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "PlatformMediaResourceLoader.h"
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedRawResource;
class Document;
class MediaResource;

class MediaResourceLoader final : public PlatformMediaResourceLoader {
public:
    WEBCORE_EXPORT MediaResourceLoader(Document&, const String& crossOriginMode);
    WEBCORE_EXPORT virtual ~MediaResourceLoader();

    RefPtr<PlatformMediaResource> requestResource(const ResourceRequest&, LoadOptions) override;
    void removeResource(MediaResource&);

    Document& document() { return m_document; }
    const String& crossOriginMode() const { return m_crossOriginMode; }

private:
    Document& m_document;
    String m_crossOriginMode;
    HashSet<MediaResource*> m_resources;
};

class MediaResource : public PlatformMediaResource, CachedRawResourceClient {
public:
    static Ref<MediaResource> create(MediaResourceLoader&, CachedResourceHandle<CachedRawResource>);
    virtual ~MediaResource();

    // PlatformMediaResource
    void stop() override;
    void setDefersLoading(bool) override;
    bool didPassAccessControlCheck() const override { return m_didPassAccessControlCheck; }

    // CachedResourceClient
    void responseReceived(CachedResource*, const ResourceResponse&) override;
    void redirectReceived(CachedResource*, ResourceRequest&, const ResourceResponse&) override;
    bool shouldCacheResponse(CachedResource*, const ResourceResponse&) override;
    void dataSent(CachedResource*, unsigned long long, unsigned long long) override;
    void dataReceived(CachedResource*, const char*, int) override;
    void notifyFinished(CachedResource*) override;
#if USE(SOUP)
    char* getOrCreateReadBuffer(CachedResource*, size_t /*requestedSize*/, size_t& /*actualSize*/) override;
#endif

private:
    MediaResource(MediaResourceLoader&, CachedResourceHandle<CachedRawResource>);
    Ref<MediaResourceLoader> m_loader;
    bool m_didPassAccessControlCheck { false };
    CachedResourceHandle<CachedRawResource> m_resource;
};


} // namespace WebCore

#endif
#endif
