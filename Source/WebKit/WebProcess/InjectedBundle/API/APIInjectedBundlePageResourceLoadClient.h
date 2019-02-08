/*
 * Copyright (C) 2017 Igalia S.L.
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

#pragma once

namespace WebKit {
class WebFrame;
class WebPage;
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace API {

namespace InjectedBundle {

class ResourceLoadClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ResourceLoadClient() = default;

    virtual void didInitiateLoadForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/, const WebCore::ResourceRequest&, bool /*pageIsProvisionallyLoading*/) { }
    virtual void willSendRequestForFrame(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/, WebCore::ResourceRequest&, const WebCore::ResourceResponse&) { }
    virtual void didReceiveResponseForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/, const WebCore::ResourceResponse&) { }
    virtual void didReceiveContentLengthForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/, uint64_t contentLength) { }
    virtual void didFinishLoadForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/) { }
    virtual void didFailLoadForResource(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/, const WebCore::ResourceError&) { }
    virtual bool shouldCacheResponse(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/) { return true; }
    virtual bool shouldUseCredentialStorage(WebKit::WebPage&, WebKit::WebFrame&, uint64_t /*identifier*/) { return true; }
};

} // namespace InjectedBundle

} // namespace API
