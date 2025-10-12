/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ScriptTrackingPrivacyFilter.h"

#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOrigin.h>

namespace WebKit {

static void initializeFilterRules(Vector<ScriptTrackingPrivacyHost>&& source, HostToAllowedCategoriesMap& target, WebCore::ScriptTrackingPrivacyFlags& categoriesWithAllowedHosts)
{
    target.reserveInitialCapacity(source.size());
    for (auto&& [host, allowedCategories] : WTFMove(source)) {
        if (host.isEmpty()) {
            ASSERT_NOT_REACHED();
            continue;
        }

        categoriesWithAllowedHosts.add(allowedCategories);
        target.add(WTFMove(host), WTFMove(allowedCategories));
    }
}

ScriptTrackingPrivacyFilter::ScriptTrackingPrivacyFilter(ScriptTrackingPrivacyRules&& rules)
{
    initializeFilterRules(WTFMove(rules.thirdPartyHosts), m_thirdPartyHosts, m_categoriesWithAllowedHosts);
    initializeFilterRules(WTFMove(rules.thirdPartyTopDomains), m_thirdPartyTopDomains, m_categoriesWithAllowedHosts);
    initializeFilterRules(WTFMove(rules.firstPartyHosts), m_firstPartyHosts, m_categoriesWithAllowedHosts);
    initializeFilterRules(WTFMove(rules.firstPartyTopDomains), m_firstPartyTopDomains, m_categoriesWithAllowedHosts);
}

auto ScriptTrackingPrivacyFilter::lookup(const URL& url, const WebCore::SecurityOrigin& topOrigin) -> LookupResult
{
    WebCore::RegistrableDomain scriptTopDomain { url };

    auto scriptTopDomainName = scriptTopDomain.string();
    if (scriptTopDomainName.isEmpty())
        return { };

    auto hostName = url.host().toStringWithoutCopying();
    if (hostName.isEmpty())
        return { };

    if (!scriptTopDomain.matches(topOrigin.data())) {
        if (auto entry = m_thirdPartyHosts.getOptional(hostName))
            return { true, *entry };

        if (auto entry = m_thirdPartyTopDomains.getOptional(scriptTopDomainName))
            return { true, *entry };
    }

    if (auto entry = m_firstPartyHosts.getOptional(hostName)) [[unlikely]]
        return { true, *entry };

    if (auto entry = m_firstPartyTopDomains.getOptional(scriptTopDomainName)) [[unlikely]]
        return { true, *entry };

    return { };
}

bool ScriptTrackingPrivacyFilter::matches(const URL& url, const WebCore::SecurityOrigin& topOrigin)
{
    return lookup(url, topOrigin).foundMatch;
}

bool ScriptTrackingPrivacyFilter::shouldAllowAccess(const URL& url, const WebCore::SecurityOrigin& topOrigin, WebCore::ScriptTrackingPrivacyCategory category)
{
    if (url.isEmpty())
        return false;

    auto categoryFlag = WebCore::scriptCategoryAsFlag(category);
    if (!m_categoriesWithAllowedHosts.contains(categoryFlag))
        return false;

    return lookup(url, topOrigin).allowedCategories.contains(categoryFlag);
}

} // namespace WebKit
