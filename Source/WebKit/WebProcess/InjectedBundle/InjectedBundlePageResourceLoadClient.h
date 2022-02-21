/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef InjectedBundlePageResourceLoadClient_h
#define InjectedBundlePageResourceLoadClient_h

#include "APIClient.h"
#include "APIInjectedBundlePageResourceLoadClient.h"
#include "WKBundlePageResourceLoadClient.h"

namespace API {
template<> struct ClientTraits<WKBundlePageResourceLoadClientBase> {
    typedef std::tuple<WKBundlePageResourceLoadClientV0, WKBundlePageResourceLoadClientV1> Versions;
};
}

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class WebPage;
class WebFrame;

class InjectedBundlePageResourceLoadClient : public API::InjectedBundle::ResourceLoadClient, public API::Client<WKBundlePageResourceLoadClientBase> {
public:
    explicit InjectedBundlePageResourceLoadClient(const WKBundlePageResourceLoadClientBase*);

    void didInitiateLoadForResource(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceRequest&, bool /*pageIsProvisionallyLoading*/) override;
    void willSendRequestForFrame(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse&) override;
    void didReceiveResponseForResource(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&) override;
    void didReceiveContentLengthForResource(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier, uint64_t contentLength) override;
    void didFinishLoadForResource(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier) override;
    void didFailLoadForResource(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier, const WebCore::ResourceError&) override;
    bool shouldCacheResponse(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier) override;
    bool shouldUseCredentialStorage(WebPage&, WebFrame&, WebCore::ResourceLoaderIdentifier) override;
};

} // namespace WebKit

#endif // InjectedBundlePageResourceLoadClient_h
