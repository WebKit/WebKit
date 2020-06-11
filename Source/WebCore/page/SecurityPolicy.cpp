/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SecurityPolicy.h"

#include "OriginAccessEntry.h"
#include "SecurityOrigin.h"
#include "UserContentURLPattern.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static SecurityPolicy::LocalLoadPolicy localLoadPolicy = SecurityPolicy::AllowLocalLoadsForLocalOnly;

typedef Vector<OriginAccessEntry> OriginAccessAllowlist;
typedef HashMap<String, std::unique_ptr<OriginAccessAllowlist>> OriginAccessMap;

static Lock originAccessMapLock;
static OriginAccessMap& originAccessMap()
{
    ASSERT(originAccessMapLock.isHeld());
    static NeverDestroyed<OriginAccessMap> originAccessMap;
    return originAccessMap;
}

static Lock originAccessPatternLock;
static Vector<UserContentURLPattern>& originAccessPatterns()
{
    ASSERT(originAccessPatternLock.isHeld());
    static NeverDestroyed<Vector<UserContentURLPattern>> originAccessPatterns;
    return originAccessPatterns;
}

bool SecurityPolicy::shouldHideReferrer(const URL& url, const String& referrer)
{
    bool referrerIsSecureURL = protocolIs(referrer, "https");
    bool referrerIsWebURL = referrerIsSecureURL || protocolIs(referrer, "http");

    if (!referrerIsWebURL)
        return true;

    if (!referrerIsSecureURL)
        return false;

    bool URLIsSecureURL = url.protocolIs("https");

    return !URLIsSecureURL;
}

String SecurityPolicy::referrerToOriginString(const String& referrer)
{
    String originString = SecurityOrigin::createFromString(referrer)->toString();
    if (originString == "null")
        return String();
    // A security origin is not a canonical URL as it lacks a path. Add /
    // to turn it into a canonical URL we can use as referrer.
    return originString + "/";
}

String SecurityPolicy::generateReferrerHeader(ReferrerPolicy referrerPolicy, const URL& url, const String& referrer)
{
    ASSERT(referrer == URL(URL(), referrer).strippedForUseAsReferrer()
        || referrer == SecurityOrigin::create(URL(URL(), referrer))->toString());

    if (referrer.isEmpty())
        return String();

    if (!protocolIsInHTTPFamily(referrer))
        return String();

    switch (referrerPolicy) {
    case ReferrerPolicy::EmptyString:
        ASSERT_NOT_REACHED();
        break;
    case ReferrerPolicy::NoReferrer:
        return String();
    case ReferrerPolicy::NoReferrerWhenDowngrade:
        break;
    case ReferrerPolicy::SameOrigin: {
        auto origin = SecurityOrigin::createFromString(referrer);
        if (!origin->canRequest(url))
            return String();
        break;
    }
    case ReferrerPolicy::Origin:
        return referrerToOriginString(referrer);
    case ReferrerPolicy::StrictOrigin:
        if (shouldHideReferrer(url, referrer))
            return String();
        return referrerToOriginString(referrer);
    case ReferrerPolicy::OriginWhenCrossOrigin: {
        auto origin = SecurityOrigin::createFromString(referrer);
        if (!origin->canRequest(url))
            return referrerToOriginString(referrer);
        break;
    }
    case ReferrerPolicy::StrictOriginWhenCrossOrigin: {
        auto origin = SecurityOrigin::createFromString(referrer);
        if (!origin->canRequest(url)) {
            if (shouldHideReferrer(url, referrer))
                return String();
            return referrerToOriginString(referrer);
        }
        break;
    }
    case ReferrerPolicy::UnsafeUrl:
        return referrer;
    }

    return shouldHideReferrer(url, referrer) ? String() : referrer;
}

String SecurityPolicy::generateOriginHeader(ReferrerPolicy referrerPolicy, const URL& url, const SecurityOrigin& securityOrigin)
{
    switch (referrerPolicy) {
    case ReferrerPolicy::NoReferrer:
        return "null"_s;
    case ReferrerPolicy::NoReferrerWhenDowngrade:
    case ReferrerPolicy::StrictOrigin:
    case ReferrerPolicy::StrictOriginWhenCrossOrigin:
        if (protocolIs(securityOrigin.protocol(), "https") && !url.protocolIs("https"))
            return "null"_s;
        break;
    case ReferrerPolicy::SameOrigin:
        if (!securityOrigin.canRequest(url))
            return "null"_s;
        break;
    case ReferrerPolicy::EmptyString:
    case ReferrerPolicy::Origin:
    case ReferrerPolicy::OriginWhenCrossOrigin:
    case ReferrerPolicy::UnsafeUrl:
        break;
    }

    return securityOrigin.toString();
}

bool SecurityPolicy::shouldInheritSecurityOriginFromOwner(const URL& url)
{
    // Paraphrased from <https://html.spec.whatwg.org/multipage/browsers.html#origin> (8 July 2016)
    //
    // If a Document has the address "about:blank"
    //      The origin of the document is the origin it was assigned when its browsing context was created.
    // If a Document has the address "about:srcdoc"
    //      The origin of the document is the origin of its parent document.
    //
    // Note: We generalize this to invalid URLs because we treat such URLs as about:blank.
    // FIXME: We also allow some URLs like "about:BLANK". We should probably block navigation to these URLs, see rdar://problem/57966056.
    return url.isEmpty() || url.isAboutBlank() || url.isAboutSrcDoc() || equalIgnoringASCIICase(url.string(), aboutBlankURL().string());
}

bool SecurityPolicy::isBaseURLSchemeAllowed(const URL& url)
{
    // See <https://github.com/whatwg/html/issues/2249>.
    return !url.protocolIsData() && !url.protocolIsJavaScript();
}

void SecurityPolicy::setLocalLoadPolicy(LocalLoadPolicy policy)
{
    localLoadPolicy = policy;
}

bool SecurityPolicy::restrictAccessToLocal()
{
    return localLoadPolicy != SecurityPolicy::AllowLocalLoadsForAll;
}

bool SecurityPolicy::allowSubstituteDataAccessToLocal()
{
    return localLoadPolicy != SecurityPolicy::AllowLocalLoadsForLocalOnly;
}

bool SecurityPolicy::isAccessAllowed(const SecurityOrigin& activeOrigin, const SecurityOrigin& targetOrigin, const URL& targetURL)
{
    ASSERT(targetOrigin.equal(SecurityOrigin::create(targetURL).ptr()));
    {
        Locker<Lock> locker(originAccessMapLock);
        if (OriginAccessAllowlist* list = originAccessMap().get(activeOrigin.toString())) {
            for (auto& entry : *list) {
                if (entry.matchesOrigin(targetOrigin))
                    return true;
            }
        }
    }
    Locker<Lock> locker(originAccessPatternLock);
    for (const auto& pattern : originAccessPatterns()) {
        if (pattern.matches(targetURL))
            return true;
    }
    return false;
}

bool SecurityPolicy::isAccessAllowed(const SecurityOrigin& activeOrigin, const URL& url)
{
    return isAccessAllowed(activeOrigin, SecurityOrigin::create(url).get(), url);
}

void SecurityPolicy::addOriginAccessAllowlistEntry(const SecurityOrigin& sourceOrigin, const String& destinationProtocol, const String& destinationDomain, bool allowDestinationSubdomains)
{
    ASSERT(!sourceOrigin.isUnique());
    if (sourceOrigin.isUnique())
        return;

    String sourceString = sourceOrigin.toString();

    Locker<Lock> locker(originAccessMapLock);
    OriginAccessMap::AddResult result = originAccessMap().add(sourceString, nullptr);
    if (result.isNewEntry)
        result.iterator->value = makeUnique<OriginAccessAllowlist>();

    OriginAccessAllowlist* list = result.iterator->value.get();
    list->append(OriginAccessEntry(destinationProtocol, destinationDomain, allowDestinationSubdomains ? OriginAccessEntry::AllowSubdomains : OriginAccessEntry::DisallowSubdomains, OriginAccessEntry::TreatIPAddressAsIPAddress));
}

void SecurityPolicy::removeOriginAccessAllowlistEntry(const SecurityOrigin& sourceOrigin, const String& destinationProtocol, const String& destinationDomain, bool allowDestinationSubdomains)
{
    ASSERT(!sourceOrigin.isUnique());
    if (sourceOrigin.isUnique())
        return;

    String sourceString = sourceOrigin.toString();

    Locker<Lock> locker(originAccessMapLock);
    OriginAccessMap& map = originAccessMap();
    OriginAccessMap::iterator it = map.find(sourceString);
    if (it == map.end())
        return;

    OriginAccessAllowlist& list = *it->value;
    OriginAccessEntry originAccessEntry(destinationProtocol, destinationDomain, allowDestinationSubdomains ? OriginAccessEntry::AllowSubdomains : OriginAccessEntry::DisallowSubdomains, OriginAccessEntry::TreatIPAddressAsIPAddress);
    if (!list.removeFirst(originAccessEntry))
        return;

    if (list.isEmpty())
        map.remove(it);
}

void SecurityPolicy::resetOriginAccessAllowlists()
{
    Locker<Lock> locker(originAccessMapLock);
    originAccessMap().clear();
}

void SecurityPolicy::allowAccessTo(const UserContentURLPattern& pattern)
{
    Locker<Lock> locker(originAccessPatternLock);
    originAccessPatterns().append(pattern);
}

} // namespace WebCore
