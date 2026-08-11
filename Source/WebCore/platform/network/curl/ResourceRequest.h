/*
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#include <WebCore/ResourceRequestBase.h>

namespace WebCore {

class ResourceRequest : public ResourceRequestBase {
public:
    explicit ResourceRequest(String&& url)
        : ResourceRequestBase(URL({ }, WTF::move(url)), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(URL&& url)
        : ResourceRequestBase(WTF::move(url), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(URL&& url, const String& referrer, ResourceRequestCachePolicy policy = ResourceRequestCachePolicy::UseProtocolCachePolicy)
        : ResourceRequestBase(WTF::move(url), policy)
    {
        setHTTPReferrer(referrer);
    }

    ResourceRequest()
        : ResourceRequestBase(URL(), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(ResourceRequestBase&& base)
        : ResourceRequestBase(WTF::move(base))
    {
    }

    WEBCORE_EXPORT static ResourceRequest fromResourceRequestData(ResourceRequestBase::RequestData&&);
    WEBCORE_EXPORT ResourceRequestBase::RequestData getRequestDataToSerialize() const;

    WEBCORE_EXPORT void updateFromDelegatePreservingOldProperties(const ResourceRequest&);

private:
    friend class ResourceRequestBase;

    void doUpdatePlatformRequest() { }
    void doUpdateResourceRequest() { }
    void doUpdatePlatformHTTPBody() { }
    void doUpdateResourceHTTPBody() { }

    void doPlatformSetAsIsolatedCopy(const ResourceRequest&) { }
};

} // namespace WebCore

namespace WTF {

template<> struct MarkableTraits<WebCore::ResourceRequest> {
    static bool isEmptyValue(const WebCore::ResourceRequest& request) { return request.isNull(); };
    static WebCore::ResourceRequest emptyValue() { return WebCore::ResourceRequest { }; };
};

} // namespace WTF
