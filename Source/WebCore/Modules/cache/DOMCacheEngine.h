
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

#pragma once

#include "FetchHeaders.h"
#include "FetchOptions.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

struct CacheQueryOptions;

namespace DOMCacheEngine {

enum class Error {
    NotImplemented,
    ReadDisk,
    WriteDisk,
    QuotaExceeded,
    Internal,
    Stopped
};

Exception convertToExceptionAndLog(ScriptExecutionContext*, Error);

WEBCORE_EXPORT bool queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse&, const CacheQueryOptions&);
WEBCORE_EXPORT bool queryCacheMatch(const ResourceRequest& request, const URL& url, bool hasVaryStar, const HashMap<String, String>& varyHeaders, const CacheQueryOptions&);

using ResponseBody = Variant<std::nullptr_t, Ref<FormData>, Ref<SharedBuffer>>;
ResponseBody isolatedResponseBody(const ResponseBody&);
WEBCORE_EXPORT ResponseBody copyResponseBody(const ResponseBody&);

struct Record {
    WEBCORE_EXPORT Record copy() const;

    uint64_t identifier;
    uint64_t updateResponseCounter;

    FetchHeaders::Guard requestHeadersGuard;
    ResourceRequest request;
    FetchOptions options;
    String referrer;

    FetchHeaders::Guard responseHeadersGuard;
    ResourceResponse response;
    ResponseBody responseBody;
    uint64_t responseBodySize;
};

struct CacheInfo {
    uint64_t identifier;
    String name;
};

struct CacheInfos {
    CacheInfos isolatedCopy();

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<CacheInfos> decode(Decoder&);

    Vector<CacheInfo> infos;
    uint64_t updateCounter;
};

struct CacheIdentifierOperationResult {
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<CacheIdentifierOperationResult> decode(Decoder&);

    uint64_t identifier { 0 };
    // True in case storing cache list on the filesystem failed.
    bool hadStorageError { false };
};

using CacheIdentifierOrError = Expected<CacheIdentifierOperationResult, Error>;
using CacheIdentifierCallback = CompletionHandler<void(const CacheIdentifierOrError&)>;

using RecordIdentifiersOrError = Expected<Vector<uint64_t>, Error>;
using RecordIdentifiersCallback = CompletionHandler<void(RecordIdentifiersOrError&&)>;


using CacheInfosOrError = Expected<CacheInfos, Error>;
using CacheInfosCallback = CompletionHandler<void(CacheInfosOrError&&)>;

using RecordsOrError = Expected<Vector<Record>, Error>;
using RecordsCallback = CompletionHandler<void(RecordsOrError&&)>;

using CompletionCallback = CompletionHandler<void(Optional<Error>&&)>;

template<class Encoder> inline void CacheInfos::encode(Encoder& encoder) const
{
    encoder << infos;
    encoder << updateCounter;
}

template<class Decoder> inline Optional<CacheInfos> CacheInfos::decode(Decoder& decoder)
{
    Optional<Vector<CacheInfo>> infos;
    decoder >> infos;
    if (!infos)
        return WTF::nullopt;

    Optional<uint64_t> updateCounter;
    decoder >> updateCounter;
    if (!updateCounter)
        return WTF::nullopt;

    return {{ WTFMove(*infos), WTFMove(*updateCounter) }};
}

template<class Encoder> inline void CacheIdentifierOperationResult::encode(Encoder& encoder) const
{
    encoder << identifier;
    encoder << hadStorageError;
}

template<class Decoder> inline Optional<CacheIdentifierOperationResult> CacheIdentifierOperationResult::decode(Decoder& decoder)
{
    Optional<uint64_t> identifier;
    decoder >> identifier;
    if (!identifier)
        return WTF::nullopt;

    Optional<bool> hadStorageError;
    decoder >> hadStorageError;
    if (!hadStorageError)
        return WTF::nullopt;
    return {{ WTFMove(*identifier), WTFMove(*hadStorageError) }};
}

} // namespace DOMCacheEngine

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::DOMCacheEngine::Error> {
    using values = EnumValues<
        WebCore::DOMCacheEngine::Error,
        WebCore::DOMCacheEngine::Error::NotImplemented,
        WebCore::DOMCacheEngine::Error::ReadDisk,
        WebCore::DOMCacheEngine::Error::WriteDisk,
        WebCore::DOMCacheEngine::Error::QuotaExceeded,
        WebCore::DOMCacheEngine::Error::Internal
    >;
};
}
