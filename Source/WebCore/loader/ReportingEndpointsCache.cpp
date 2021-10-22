/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ReportingEndpointsCache.h"

#include "HTTPHeaderNames.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include <wtf/JSONValues.h>

namespace WebCore {

struct ReportingEndpointsCache::Endpoint {
    Endpoint() = default;
    Endpoint(URL&&, Seconds maxAge);

    bool hasExpired() const;

    URL url;
    MonotonicTime addTime;
    Seconds maxAge;
};

ReportingEndpointsCache::Endpoint::Endpoint(URL&& url, Seconds maxAge)
    : url(WTFMove(url))
    , addTime(MonotonicTime::now())
    , maxAge(maxAge)
{
}

bool ReportingEndpointsCache::Endpoint::hasExpired() const
{
    return MonotonicTime::now() - addTime > maxAge;
}

Ref<ReportingEndpointsCache> ReportingEndpointsCache::create()
{
    return adoptRef(*new ReportingEndpointsCache);
}

ReportingEndpointsCache::ReportingEndpointsCache() = default;
ReportingEndpointsCache::~ReportingEndpointsCache() = default;

// https://www.w3.org/TR/reporting/#process-header
void ReportingEndpointsCache::addEndpointsFromResponse(const ResourceResponse& response)
{
    return addEndpointsFromReportToHeader(response.url(), response.httpHeaderField(HTTPHeaderName::ReportTo));
}

void ReportingEndpointsCache::addEndpointsFromReportToHeader(const URL& responseURL, const String& reportToHeaderValue)
{
    if (reportToHeaderValue.isEmpty())
        return;

    auto securityOrigin = SecurityOrigin::create(responseURL);
    if (securityOrigin->isUnique() || !securityOrigin->isPotentiallyTrustworthy())
        return;

    auto findNextTopLevelComma = [&reportToHeaderValue](size_t startIndex) {
        unsigned depth = 0;
        for (size_t i = startIndex; i < reportToHeaderValue.length(); ++i) {
            auto c = reportToHeaderValue[i];
            if (c == '{')
                ++depth;
            else if (c == '}') {
                if (!depth)
                    break;
                --depth;
            } else if (c == ',' && !depth)
                return i;
        }
        return notFound;
    };
    size_t dictionaryStart = 0;
    while (dictionaryStart < reportToHeaderValue.length()) {
        auto indexOfNextTopLevelComma = findNextTopLevelComma(dictionaryStart);
        if (indexOfNextTopLevelComma == notFound) {
            addEndpointFromDictionary(securityOrigin->data(), responseURL, reportToHeaderValue.substring(dictionaryStart));
            break;
        }
        addEndpointFromDictionary(securityOrigin->data(), responseURL, reportToHeaderValue.substring(dictionaryStart, indexOfNextTopLevelComma - dictionaryStart));
        dictionaryStart = indexOfNextTopLevelComma + 1;
    }
}

// https://www.w3.org/TR/reporting/#process-header
void ReportingEndpointsCache::addEndpointFromDictionary(const SecurityOriginData& securityOrigin, const URL& responseURL, StringView dictionaryString)
{
    auto json = JSON::Value::parseJSON(dictionaryString.toStringWithoutCopying());
    if (!json)
        return;

    auto jsonDictionary = json->asObject();
    if (!jsonDictionary)
        return;

    auto group = jsonDictionary->getString("group"_s);
    if (group.isEmpty())
        group = "default"_s;

    auto maxAge = jsonDictionary->getInteger("max_age");
    if (!maxAge || *maxAge < 0)
        return;

    if (!*maxAge) {
        // A value of 0 indicates we should remove the group from the cache.
        auto it = m_endpointsPerOrigin.find(securityOrigin);
        if (it == m_endpointsPerOrigin.end())
            return;
        it->value.remove(group);
        if (it->value.isEmpty())
            m_endpointsPerOrigin.remove(it);
        return;
    }

    auto endpoints = jsonDictionary->getArray("endpoints"_s);
    if (!endpoints || !endpoints->length())
        return;

    for (size_t i = 0; i < endpoints->length(); ++i) {
        auto endpoint = endpoints->get(i)->asObject();
        if (!endpoint)
            continue;

        auto endpointURLString = endpoint->getString("url"_s);
        if (endpointURLString.isEmpty())
            continue;

        auto endpointURL = URL(responseURL, endpointURLString);
        if (!endpointURL.isValid())
            continue;

        auto& endpointsForOrigin = m_endpointsPerOrigin.ensure(securityOrigin, [] {
            return HashMap<String, Endpoint> { };
        }).iterator->value;
        endpointsForOrigin.add(WTFMove(group), Endpoint(WTFMove(endpointURL), Seconds { static_cast<double>(*maxAge) }));
        return;
    }
}

URL ReportingEndpointsCache::endpointURL(const SecurityOriginData& origin, const String& group) const
{
    auto outerIterator = m_endpointsPerOrigin.find(origin);
    if (outerIterator == m_endpointsPerOrigin.end())
        return { };
    auto& endpointsForOrigin = outerIterator->value;
    auto innerIterator = endpointsForOrigin.find(group);
    if (innerIterator == endpointsForOrigin.end())
        return { };
    if (innerIterator->value.hasExpired()) {
        endpointsForOrigin.remove(innerIterator);
        if (endpointsForOrigin.isEmpty())
            m_endpointsPerOrigin.remove(outerIterator);
        return { };
    }
    return innerIterator->value.url;
}

} // namespace WebCore
