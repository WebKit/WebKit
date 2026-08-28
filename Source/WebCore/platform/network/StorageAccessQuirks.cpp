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
#include "StorageAccessQuirks.h"

#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebCore {

static HashSet<OrganizationStorageAccessPromptQuirk>& NODELETE updatableStorageAccessPromptQuirks()
{
    ASSERT(RunLoop::isMain());
    static MainThreadNeverDestroyed<HashSet<OrganizationStorageAccessPromptQuirk>> set;
    return set.get();
}

const HashMap<RegistrableDomain, HashSet<RegistrableDomain>>& storageAccessQuirks()
{
    static NeverDestroyed<HashMap<RegistrableDomain, HashSet<RegistrableDomain>>> map = [] {
        HashMap<RegistrableDomain, HashSet<RegistrableDomain>> map;
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("microsoft.com"_s),
            HashSet { RegistrableDomain::uncheckedCreateFromRegistrableDomainString("microsoftonline.com"_s) });
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("playstation.com"_s), HashSet {
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sonyentertainmentnetwork.com"_s),
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sony.com"_s) });
        map.add(RegistrableDomain::uncheckedCreateFromRegistrableDomainString("bbc.co.uk"_s), HashSet {
            RegistrableDomain::uncheckedCreateFromRegistrableDomainString("radioplayer.co.uk"_s) });
        return map;
    }();
    return map.get();
}

void updateStorageAccessPromptQuirks(Vector<OrganizationStorageAccessPromptQuirk>&& organizationStorageAccessPromptQuirks)
{
    auto& quirks = updatableStorageAccessPromptQuirks();
    quirks.clear();
    for (auto&& quirk : organizationStorageAccessPromptQuirks)
        quirks.add(quirk);
}

bool loginDomainMatchesRequestingDomain(const RegistrableDomain& topFrameDomain, const RegistrableDomain& resourceDomain)
{
    auto loginDomains = subResourceDomainsInNeedOfStorageAccessForFirstParty(topFrameDomain);
    return (loginDomains && loginDomains.value().contains(resourceDomain)) || !!storageAccessQuirkForDomainPair(topFrameDomain, resourceDomain);
}

bool canRequestStorageAccessForLoginOrCompatibilityPurposesWithoutPriorUserInteraction(const RegistrableDomain& resourceDomain, const RegistrableDomain& topFrameDomain)
{
    ASSERT(RunLoop::isMain());
    return loginDomainMatchesRequestingDomain(topFrameDomain, resourceDomain);
}

std::optional<HashSet<RegistrableDomain>> subResourceDomainsInNeedOfStorageAccessForFirstParty(const RegistrableDomain& topFrameDomain)
{
    auto it = storageAccessQuirks().find(topFrameDomain);
    if (it != storageAccessQuirks().end())
        return it->value;
    return std::nullopt;
}

std::optional<RegistrableDomain> findAdditionalLoginDomain(const RegistrableDomain& topDomain, const RegistrableDomain& subDomain)
{
    if (subDomain.string() == "sony.com"_s && topDomain.string() == "playstation.com"_s)
        return RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sonyentertainmentnetwork.com"_s);

    if (subDomain.string() == "sonyentertainmentnetwork.com"_s && topDomain.string() == "playstation.com"_s)
        return RegistrableDomain::uncheckedCreateFromRegistrableDomainString("sony.com"_s);

    return std::nullopt;
}

Vector<RegistrableDomain> storageAccessQuirkForTopFrameDomain(const URL& topFrameURL)
{
    for (auto&& quirk : updatableStorageAccessPromptQuirks()) {
        if (!quirk.triggerPages.isEmpty() && !quirk.triggerPages.contains(topFrameURL))
            continue;

        auto quirkDomains = quirk.quirkDomains;
        auto entry = quirkDomains.find(RegistrableDomain { topFrameURL });
        if (entry == quirkDomains.end())
            continue;
        return entry->value;
    }
    return { };
}

std::optional<OrganizationStorageAccessPromptQuirk> storageAccessQuirkForDomainPair(const RegistrableDomain& topDomain, const RegistrableDomain& subDomain)
{
    for (auto&& quirk : updatableStorageAccessPromptQuirks()) {
        auto& quirkDomains = quirk.quirkDomains;
        auto entry = quirkDomains.find(topDomain);
        if (entry == quirkDomains.end())
            continue;
        if (std::ranges::none_of(entry->value, [&subDomain](auto&& entry) { return entry == subDomain; }))
            break;
        return quirk;
    }
    return std::nullopt;
}

} // namespace WebCore
