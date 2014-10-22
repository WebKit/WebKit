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
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedRawResource;
class Document;

class MediaResourceLoader final : public PlatformMediaResourceLoader, CachedRawResourceClient {
public:
    MediaResourceLoader(Document&, const String& crossOriginMode, std::unique_ptr<PlatformMediaResourceLoaderClient>);
    virtual ~MediaResourceLoader();

    virtual bool start(const ResourceRequest&, LoadOptions) override;
    virtual void stop() override;
    virtual void setDefersLoading(bool) override;
    virtual bool didPassAccessControlCheck() const override { return m_didPassAccessControlCheck; }

    // CachedResourceClient
    virtual void responseReceived(CachedResource*, const ResourceResponse&) override;
    virtual void dataReceived(CachedResource*, const char*, int) override;
    virtual void notifyFinished(CachedResource*) override;
#if USE(SOUP)
    virtual char* getOrCreateReadBuffer(CachedResource*, size_t /*requestedSize*/, size_t& /*actualSize*/) override;
#endif

private:
    Document& m_document;
    String m_crossOriginMode;
    bool m_didPassAccessControlCheck;
    CachedResourceHandle<CachedRawResource> m_resource;
};


} // namespace WebCore

#endif
#endif
