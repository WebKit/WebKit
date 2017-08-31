
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DOMCache.h"

#include "CacheQueryOptions.h"
#include "Exception.h"
#include "HTTPParsers.h"

namespace WebCore {

namespace DOMCache {

Exception errorToException(Error error)
{
    switch (error) {
    case Error::NotImplemented:
        return Exception { NotSupportedError, ASCIILiteral("Not implemented") };
    case Error::ReadDisk:
        return Exception { TypeError, ASCIILiteral("Failed reading data from the file system") };
    case Error::WriteDisk:
        return Exception { TypeError, ASCIILiteral("Failed writing data to the file system") };
    default:
        return Exception { TypeError, ASCIILiteral("Internal error") };
    }
}

bool queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse& cachedResponse, const CacheQueryOptions& options)
{
    ASSERT(options.ignoreMethod || request.httpMethod() == "GET");

    URL requestURL = request.url();
    URL cachedRequestURL = cachedRequest.url();

    if (options.ignoreSearch) {
        requestURL.setQuery({ });
        cachedRequestURL.setQuery({ });
    }
    if (!equalIgnoringFragmentIdentifier(requestURL, cachedRequestURL))
        return false;

    if (options.ignoreVary)
        return true;

    String varyValue = cachedResponse.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isNull())
        return true;

    // FIXME: This is inefficient, we should be able to split and trim whitespaces at the same time.
    Vector<String> varyHeaderNames;
    varyValue.split(',', false, varyHeaderNames);
    for (auto& name : varyHeaderNames) {
        if (stripLeadingAndTrailingHTTPSpaces(name) == "*")
            return false;
        if (cachedRequest.httpHeaderField(name) != request.httpHeaderField(name))
            return false;
    }
    return true;
}

ResponseBody isolatedResponseBody(const ResponseBody& body)
{
    return WTF::switchOn(body, [](const Ref<FormData>& formData) {
        return formData->isolatedCopy();
    }, [](const Ref<SharedBuffer>& buffer) {
        return buffer->copy();
    }, [](const std::nullptr_t&) {
        return DOMCache::ResponseBody { };
    });
}

static inline ResponseBody copyResponseBody(const DOMCache::ResponseBody& body)
{
    return WTF::switchOn(body, [](const Ref<FormData>& formData) {
        return formData.copyRef();
    }, [](const Ref<SharedBuffer>& buffer) {
        return buffer.copyRef();
    }, [](const std::nullptr_t&) {
        return DOMCache::ResponseBody { };
    });
}

Record Record::copy() const
{
    return Record { identifier, updateResponseCounter, requestHeadersGuard, request, options, referrer, responseHeadersGuard, response, copyResponseBody(responseBody) };
}

CacheInfos CacheInfos::isolatedCopy()
{
    Vector<CacheInfo> isolatedCaches;
    isolatedCaches.reserveInitialCapacity(infos.size());
    for (const auto& info : infos)
        isolatedCaches.uncheckedAppend(CacheInfo { info.identifier, info.name.isolatedCopy() });
    return { WTFMove(isolatedCaches), updateCounter };
}

} // namespace DOMCache

} // namespace WebCore

