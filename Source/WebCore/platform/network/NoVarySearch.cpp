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
#include "config.h"
#include "NoVarySearch.h"

#include "RFC8941.h"
#include "ResourceResponse.h"
#include <algorithm>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringView.h>

namespace WebCore {

// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html#name-parse-a-key
static std::optional<String> parseKey(const String& keyString)
{
    return WTF::URLParser::formURLDecode(makeStringByReplacingAll(keyString, '+', ' '));
}

// Returns nullopt if any item is not a string.
static std::optional<Vector<String>> parseKeyList(const RFC8941::InnerList& innerList)
{
    Vector<String> keys;
    keys.reserveInitialCapacity(innerList.size());
    for (auto& [item, parameters] : innerList) {
        auto* string = std::get_if<String>(&item);
        if (!string)
            return std::nullopt;
        auto key = parseKey(*string);
        if (!key)
            return std::nullopt;
        keys.append(WTF::move(*key));
    }
    return keys;
}

// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html#name-parse-a-url-variation-confi
// Anything unrecognized yields the default config, which only costs cache hits.
NoVarySearch NoVarySearch::parse(StringView headerValue)
{
    auto dictionary = RFC8941::parseDictionaryStructuredFieldValue(headerValue);
    if (!dictionary)
        return { };

    NoVarySearch result;

    if (auto it = dictionary->find<HashTranslatorASCIILiteral>("key-order"_s); it != dictionary->end()) {
        auto* bareItem = std::get_if<RFC8941::BareItem>(&it->value.first);
        auto* keyOrder = bareItem ? std::get_if<bool>(bareItem) : nullptr;
        if (!keyOrder)
            return { };
        result.m_varyOnKeyOrder = !*keyOrder;
    }

    // As shipped by other browsers, "params" is either a boolean (all parameters, optionally with exceptions) or a list of parameters.
    bool hasParams = false;
    bool paramsIsWildcard = false;
    const RFC8941::InnerList* paramsList = nullptr;
    if (auto it = dictionary->find<HashTranslatorASCIILiteral>("params"_s); it != dictionary->end()) {
        hasParams = true;
        if (auto* innerList = std::get_if<RFC8941::InnerList>(&it->value.first))
            paramsList = innerList;
        else {
            auto* bareItem = std::get_if<RFC8941::BareItem>(&it->value.first);
            auto* boolean = bareItem ? std::get_if<bool>(bareItem) : nullptr;
            if (!boolean)
                return { };
            paramsIsWildcard = *boolean;
        }
    }

    auto exceptIterator = dictionary->find<HashTranslatorASCIILiteral>("except"_s);
    bool hasExcept = exceptIterator != dictionary->end();

    if (paramsList) {
        // "params" and "except" cannot be combined when "params" is a list.
        if (hasExcept)
            return { };
        auto keys = parseKeyList(*paramsList);
        if (!keys)
            return { };
        result.m_noVaryParams = WTF::move(*keys);
        return result;
    }

    if (hasExcept) {
        // "except" only refines a params that ignores everything. With params=?0 nothing is ignored, so it is the default config.
        if (hasParams && !paramsIsWildcard)
            return { };
        auto* exceptList = std::get_if<RFC8941::InnerList>(&exceptIterator->value.first);
        if (!exceptList)
            return { };
        auto keys = parseKeyList(*exceptList);
        if (!keys)
            return { };
        result.m_noVaryParamsIsWildcard = true;
        result.m_varyParams = WTF::move(*keys);
        return result;
    }

    if (paramsIsWildcard)
        result.m_noVaryParamsIsWildcard = true;
    return result;
}

NoVarySearch NoVarySearch::obtain(const ResourceResponse& response)
{
    return parse(response.httpHeaderField("No-Vary-Search"_s));
}

// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html#name-comparing
bool NoVarySearch::equivalent(const URL& a, const URL& b) const
{
    if (!protocolHostAndPortAreEqual(a, b) || a.path() != b.path())
        return false;

    if (isDefault())
        return a.queryWithLeadingQuestionMark() == b.queryWithLeadingQuestionMark();

    auto filteredParameters = [&](const URL& url) {
        auto parameters = WTF::URLParser::parseURLEncodedForm(url.query());
        if (m_noVaryParamsIsWildcard) {
            // Only the "except" parameters matter.
            parameters.removeAllMatching([&](auto& pair) {
                return !m_varyParams.contains(pair.key);
            });
        } else {
            parameters.removeAllMatching([&](auto& pair) {
                return m_noVaryParams.contains(pair.key);
            });
        }
        if (!m_varyOnKeyOrder) {
            std::ranges::stable_sort(parameters, [](auto& x, auto& y) {
                return codePointCompareLessThan(x.key, y.key);
            });
        }
        return parameters;
    };

    auto parametersA = filteredParameters(a);
    auto parametersB = filteredParameters(b);
    if (parametersA.size() != parametersB.size())
        return false;
    for (size_t i = 0; i < parametersA.size(); ++i) {
        if (parametersA[i].key != parametersB[i].key || parametersA[i].value != parametersB[i].value)
            return false;
    }
    return true;
}

} // namespace WebCore
