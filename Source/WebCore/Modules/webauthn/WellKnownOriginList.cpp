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
#include "WellKnownOriginList.h"

#include "PublicSuffixStore.h"
#include "SecurityOriginData.h"
#include <wtf/JSONValues.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>

namespace WebCore {

URL wellKnownURL(StringView host, StringView path)
{
    if (host.isEmpty() || !path.startsWith('/'))
        return { };

    URL url { makeString("https://"_s, host, path) };
    if (!url.isValid() || !url.protocolIs("https"_s))
        return { };

    if (!equalIgnoringASCIICase(url.host(), host))
        return { };
    if (url.path() != path)
        return { };
    if (url.port() || url.hasCredentials() || url.hasQuery() || url.hasFragmentIdentifier())
        return { };

    return url;
}

bool isWellKnownResponseAcceptable(int httpStatusCode, StringView mimeType)
{
    return httpStatusCode == 200 && equalLettersIgnoringASCIICase(mimeType, "application/json"_s);
}

bool isWellKnownRedirectAllowed(const URL& url)
{
    return url.isValid() && url.protocolIs("https"_s);
}

static String registrableOriginLabel(StringView host)
{
    return PublicSuffixStore::singleton().topPrivatelyControlledDomainWithoutPublicSuffix(host);
}

WellKnownOriginListResult findOriginInWellKnownList(const SecurityOriginData& callerOrigin, std::span<const uint8_t> resource, ASCIILiteral memberName, const WellKnownOriginListPolicy& policy)
{
    if (resource.empty() || resource.size() > policy.maxResourceSize)
        return WellKnownOriginListResult::Malformed;

    RefPtr parsedValue = JSON::Value::parseJSON(String::fromUTF8(resource));
    if (!parsedValue)
        return WellKnownOriginListResult::Malformed;

    RefPtr object = parsedValue->asObject();
    if (!object)
        return WellKnownOriginListResult::Malformed;

    RefPtr member = object->getValue(memberName);
    if (!member)
        return WellKnownOriginListResult::Malformed;

    RefPtr candidates = member->asArray();
    if (!candidates)
        return WellKnownOriginListResult::Malformed;

    Vector<String> labelsSeen;
    labelsSeen.reserveInitialCapacity(policy.maxRegistrableOriginLabels);
    size_t examined = 0;

    for (Ref candidate : *candidates) {
        if (examined >= policy.maxCandidates)
            break;

        auto candidateString = candidate->asString();
        if (!candidateString)
            continue;

        URL url { candidateString };
        if (!url.isValid())
            continue;

        auto label = registrableOriginLabel(url.host());
        if (label.isEmpty())
            continue;

        ++examined;

        bool alreadySeen = labelsSeen.contains(label);
        if (labelsSeen.size() >= policy.maxRegistrableOriginLabels && !alreadySeen)
            continue;

        if (SecurityOriginData::fromURL(url) == callerOrigin)
            return WellKnownOriginListResult::Found;

        if (!alreadySeen)
            labelsSeen.append(WTF::move(label));
    }

    return WellKnownOriginListResult::NotFound;
}

Vector<String> parseOriginsFromWellKnownList(std::span<const uint8_t> resource, ASCIILiteral memberName, const WellKnownOriginListPolicy& policy)
{
    Vector<String> result;

    if (resource.empty() || resource.size() > policy.maxResourceSize)
        return result;

    RefPtr parsedValue = JSON::Value::parseJSON(String::fromUTF8(resource));
    if (!parsedValue)
        return result;

    RefPtr object = parsedValue->asObject();
    if (!object)
        return result;

    RefPtr member = object->getValue(memberName);
    if (!member)
        return result;

    RefPtr candidates = member->asArray();
    if (!candidates)
        return result;

    for (Ref candidate : *candidates) {
        if (result.size() >= policy.maxCandidates)
            break;
        auto candidateString = candidate->asString();
        if (!candidateString)
            continue;
        result.append(candidateString);
    }

    return result;
}

} // namespace WebCore
