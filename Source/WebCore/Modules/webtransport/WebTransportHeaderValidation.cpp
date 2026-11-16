/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebTransportHeaderValidation.h"

#include "ExceptionOr.h"
#include "HTTPParsers.h"

namespace WebCore {

// https://fetch.spec.whatwg.org/#headers-validate with guard "request", plus the
// wt-available-protocols step of https://w3c.github.io/webtransport/#webtransport-constructor.
// Unlike the spec, this also normalizes name and value in place to the form sent over the wire,
// so that receivers can verify a header list is already normalized.
static ExceptionOr<bool> validateWebTransportHeader(String& name, String& value)
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError };
    value = value.trim(isASCIIWhitespaceWithoutFF<char16_t>);
    if (!isValidHTTPHeaderValue(value))
        return Exception { ExceptionCode::TypeError };
    name = name.convertToASCIILowercase();
    if (name == "wt-available-protocols"_s)
        return Exception { ExceptionCode::TypeError, "The 'wt-available-protocols' header cannot be set."_s };
    if (isForbiddenHeader(name, value))
        return false;
    return true;
}

ExceptionOr<Vector<KeyValuePair<String, String>>> validateAndNormalizeWebTransportHeaders(const FetchHeaders::Init& init)
{
    Vector<KeyValuePair<String, String>> headers;
    auto validateAndAppend = [&](String name, String value) -> ExceptionOr<void> {
        auto result = validateWebTransportHeader(name, value);
        if (result.hasException())
            return result.releaseException();
        if (result.returnValue())
            headers.constructAndAppend(WTF::move(name), WTF::move(value));
        return { };
    };

    if (std::holds_alternative<Vector<Vector<String>>>(init)) {
        for (auto& header : std::get<Vector<Vector<String>>>(init)) {
            if (header.size() != 2)
                return Exception { ExceptionCode::TypeError, "Header sub-sequence must contain exactly two items"_s };
            auto result = validateAndAppend(header[0], header[1]);
            if (result.hasException())
                return result.releaseException();
        }
    } else {
        for (auto& header : std::get<Vector<KeyValuePair<String, String>>>(init)) {
            auto result = validateAndAppend(header.key, header.value);
            if (result.hasException())
                return result.releaseException();
        }
    }
    return headers;
}

bool areValidWebTransportHeaders(const Vector<KeyValuePair<String, String>>& headers)
{
    for (auto& header : headers) {
        auto name = header.key;
        auto value = header.value;
        auto result = validateWebTransportHeader(name, value);
        if (result.hasException() || !result.returnValue())
            return false;
        if (name != header.key || value != header.value)
            return false;
    }
    return true;
}

} // namespace WebCore
