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
#include "ScriptTelemetry.h"

#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOrigin.h>

namespace WebKit {

static void initializeFilterRules(Vector<String>&& source, MemoryCompactRobinHoodHashSet<String>& target)
{
    target.reserveInitialCapacity(source.size());
    for (auto& host : source)
        target.add(host);
}

ScriptTelemetryFilter::ScriptTelemetryFilter(ScriptTelemetryRules&& rules)
{
    initializeFilterRules(WTFMove(rules.thirdPartyHosts), m_thirdPartyHosts);
    initializeFilterRules(WTFMove(rules.thirdPartyTopDomains), m_thirdPartyTopDomains);
    initializeFilterRules(WTFMove(rules.firstPartyHosts), m_firstPartyHosts);
    initializeFilterRules(WTFMove(rules.firstPartyTopDomains), m_firstPartyTopDomains);
}

bool ScriptTelemetryFilter::matches(const URL& url, const WebCore::SecurityOrigin& topOrigin)
{
    WebCore::RegistrableDomain scriptTopDomain { url };

    auto scriptTopDomainName = scriptTopDomain.string();
    if (scriptTopDomainName.isEmpty())
        return false;

    auto hostName = url.host().toStringWithoutCopying();
    if (hostName.isEmpty())
        return false;

    if (!scriptTopDomain.matches(topOrigin.data())) {
        if (m_thirdPartyHosts.contains(hostName))
            return true;

        if (m_thirdPartyTopDomains.contains(scriptTopDomainName))
            return true;
    }

    if (UNLIKELY(m_firstPartyHosts.contains(hostName)))
        return true;

    if (UNLIKELY(m_firstPartyTopDomains.contains(scriptTopDomainName)))
        return true;

    return false;
}

} // namespace WebKit
