
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
#include "DOMCacheEngine.h"

#include "CacheQueryOptions.h"
#include "CrossOriginAccessControl.h"
#include "Exception.h"
#include "HTTPParsers.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

namespace DOMCacheEngine {

Exception convertToException(Error error)
{
    switch (error) {
    case Error::NotImplemented:
        return Exception { ExceptionCode::NotSupportedError, "Not implemented"_s };
    case Error::ReadDisk:
        return Exception { ExceptionCode::TypeError, "Failed reading data from the file system"_s };
    case Error::WriteDisk:
        return Exception { ExceptionCode::TypeError, "Failed writing data to the file system"_s };
    case Error::QuotaExceeded:
        return Exception { ExceptionCode::QuotaExceededError, "Quota exceeded"_s };
    case Error::Internal:
        return Exception { ExceptionCode::TypeError, "Internal error"_s };
    case Error::Stopped:
        return Exception { ExceptionCode::TypeError, "Context is stopped"_s };
    case Error::CORP:
        return Exception { ExceptionCode::TypeError, "Cross-Origin-Resource-Policy failure"_s };
    }
    ASSERT_NOT_REACHED();
    return Exception { ExceptionCode::TypeError, "Connection stopped"_s };
}

Exception convertToExceptionAndLog(ScriptExecutionContext* context, Error error)
{
    auto exception = convertToException(error);
    if (context)
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Cache API operation failed: ", exception.message()));
    return exception;
}

static inline bool matchURLs(const ResourceRequest& request, const URL& cachedURL, const CacheQueryOptions& options)
{
    ASSERT(options.ignoreMethod || request.httpMethod() == "GET"_s);

    URL requestURL = request.url();
    URL cachedRequestURL = cachedURL;

    if (options.ignoreSearch) {
        requestURL.setQuery({ });
        cachedRequestURL.setQuery({ });
    }
    return equalIgnoringFragmentIdentifier(requestURL, cachedRequestURL);
}

bool queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse& cachedResponse, const CacheQueryOptions& options)
{
    if (!matchURLs(request, cachedRequest.url(), options))
        return false;

    if (options.ignoreVary)
        return true;

    String varyValue = cachedResponse.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isNull())
        return true;

    bool isVarying = false;
    varyValue.split(',', [&](StringView view) {
        if (isVarying)
            return;
        auto nameView = view.trim(isASCIIWhitespaceWithoutFF<UChar>);
        if (nameView == "*"_s) {
            isVarying = true;
            return;
        }
        isVarying = cachedRequest.httpHeaderField(nameView) != request.httpHeaderField(nameView);
    });

    return !isVarying;
}

bool queryCacheMatch(const ResourceRequest& request, const URL& url, bool hasVaryStar, const HashMap<String, String>& varyHeaders, const CacheQueryOptions& options)
{
    if (!matchURLs(request, url, options))
        return false;

    if (options.ignoreVary)
        return true;

    if (hasVaryStar)
        return false;

    for (const auto& pair : varyHeaders) {
        if (pair.value != request.httpHeaderField(pair.key))
            return false;
    }
    return true;
}

ResponseBody isolatedResponseBody(const ResponseBody& body)
{
    return WTF::switchOn(body, [](const Ref<FormData>& formData) -> ResponseBody {
        return formData->isolatedCopy();
    }, [](const Ref<SharedBuffer>& buffer) -> ResponseBody {
        return buffer.copyRef(); // SharedBuffer are immutable and can be returned as-is.
    }, [](const std::nullptr_t&) -> ResponseBody {
        return DOMCacheEngine::ResponseBody { };
    });
}

ResponseBody copyResponseBody(const ResponseBody& body)
{
    return WTF::switchOn(body, [](const Ref<FormData>& formData) -> ResponseBody {
        return formData.copyRef();
    }, [](const Ref<SharedBuffer>& buffer) -> ResponseBody {
        return buffer.copyRef();
    }, [](const std::nullptr_t&) -> ResponseBody {
        return DOMCacheEngine::ResponseBody { };
    });
}

Record Record::copy() const
{
    return Record { identifier, updateResponseCounter, requestHeadersGuard, request, options, referrer, responseHeadersGuard, response, copyResponseBody(responseBody), responseBodySize };
}

CrossThreadRecord toCrossThreadRecord(Record&& record)
{
    return CrossThreadRecord {
        record.identifier,
        record.updateResponseCounter,
        record.requestHeadersGuard,
        WTFMove(record.request).isolatedCopy(),
        WTFMove(record.options).isolatedCopy(),
        WTFMove(record.referrer).isolatedCopy(),
        record.responseHeadersGuard,
        record.response.crossThreadData(),
        isolatedResponseBody(record.responseBody),
        record.responseBodySize
    };
}

Record fromCrossThreadRecord(CrossThreadRecord&& record)
{
    return Record {
        record.identifier,
        record.updateResponseCounter,
        record.requestHeadersGuard,
        WTFMove(record.request),
        WTFMove(record.options),
        WTFMove(record.referrer),
        record.responseHeadersGuard,
        ResourceResponse::fromCrossThreadData(WTFMove(record.response)),
        WTFMove(record.responseBody),
        record.responseBodySize
    };
}

CrossThreadRecord CrossThreadRecord::isolatedCopy() &&
{
    return CrossThreadRecord {
        identifier,
        updateResponseCounter,
        requestHeadersGuard,
        WTFMove(request).isolatedCopy(),
        WTFMove(options).isolatedCopy(),
        WTFMove(referrer).isolatedCopy(),
        responseHeadersGuard,
        WTFMove(response).isolatedCopy(),
        isolatedResponseBody(responseBody),
        responseBodySize
    };
}

} // namespace DOMCacheEngine

} // namespace WebCore

