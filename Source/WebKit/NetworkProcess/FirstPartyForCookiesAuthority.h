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

#pragma once

#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "Untrusted.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Site.h>

namespace WebKit {

// Without site isolation one process hosts every site a page pulls in, so neither the
// first-party set nor the hosted-domain set says anything about what it may name.
inline bool canCheckDomainAuthority(NetworkProcess& networkProcess, WebCore::ProcessIdentifier identifier)
{
    CheckedPtr connection = networkProcess.webProcessConnection(identifier);
    if (!connection)
        return true;
    auto preferences = connection->sharedPreferencesForWebProcessValue();
    return preferences.siteIsolationEnabled && !preferences.usesSingleWebProcess;
}

class FirstPartyForCookiesAuthority : public IPC::CanValidateUntrusted<FirstPartyForCookiesAuthority> {
public:
    FirstPartyForCookiesAuthority(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier)
        : m_networkProcess(networkProcess)
        , m_webProcessIdentifier(webProcessIdentifier)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::SecurityOriginData& origin) const
    {
        return checkUntrustedDomain([&] {
            return WebCore::RegistrableDomain { origin };
        });
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::RegistrableDomain& domain) const
    {
        return checkUntrustedDomain([&]() -> const WebCore::RegistrableDomain& {
            return domain;
        });
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::Site& site) const
    {
        return checkUntrustedDomain([&]() -> const WebCore::RegistrableDomain& {
            return site.domain();
        });
    }

private:
    // Takes the domain as a function rather than a value because deriving one from an origin or
    // URL consults the public suffix list under a global lock, and the guard below usually
    // discards it.
    template<typename DomainFunction>
    std::optional<IPC::ValidationFailure> checkUntrustedDomain(NOESCAPE DomainFunction&& domain) const
    {
        if (!canCheckDomainAuthority(m_networkProcess, m_webProcessIdentifier))
            return std::nullopt;

        switch (m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, domain())) {
        case NetworkProcess::AllowCookieAccess::Allow:
            return std::nullopt;
        case NetworkProcess::AllowCookieAccess::Disallow:
            return IPC::ValidationFailure::Ignore;
        case NetworkProcess::AllowCookieAccess::Terminate:
            return IPC::ValidationFailure::Terminate;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    Ref<NetworkProcess> m_networkProcess;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
};

class HostedDomainAuthority : public IPC::CanValidateUntrusted<HostedDomainAuthority> {
public:
    HostedDomainAuthority(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier)
        : m_networkProcess(networkProcess)
        , m_webProcessIdentifier(webProcessIdentifier)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::RegistrableDomain& domain) const
    {
        return checkUntrustedDomain([&]() -> const WebCore::RegistrableDomain& {
            return domain;
        });
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::SecurityOriginData& origin) const
    {
        return checkUntrustedDomain([&] {
            return WebCore::RegistrableDomain { origin };
        });
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::ClientOrigin& origin) const
    {
        auto clientDomain = [&] {
            return WebCore::RegistrableDomain { origin.clientOrigin };
        };
        if (auto failure = checkUntrustedDomain(clientDomain))
            return failure;
        return FirstPartyForCookiesAuthority { m_networkProcess, m_webProcessIdentifier }.checkUntrusted(origin.topOrigin);
    }

private:
    // Takes the domain as a function rather than a value because deriving one from an origin or
    // URL consults the public suffix list under a global lock, and the guard below usually
    // discards it.
    template<typename DomainFunction>
    std::optional<IPC::ValidationFailure> checkUntrustedDomain(NOESCAPE DomainFunction&& domain) const
    {
        if (!canCheckDomainAuthority(m_networkProcess, m_webProcessIdentifier))
            return std::nullopt;
        if (!m_networkProcess->hostsDomain(m_webProcessIdentifier, domain()))
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

    Ref<NetworkProcess> m_networkProcess;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
};

} // namespace WebKit

namespace IPC {

template<> struct IsValidationProcedureFor<WebKit::FirstPartyForCookiesAuthority, WebCore::SecurityOriginData> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::FirstPartyForCookiesAuthority, WebCore::RegistrableDomain> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::FirstPartyForCookiesAuthority, WebCore::Site> : std::true_type { };

template<> struct IsValidationProcedureFor<WebKit::HostedDomainAuthority, WebCore::RegistrableDomain> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::HostedDomainAuthority, WebCore::SecurityOriginData> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::HostedDomainAuthority, WebCore::ClientOrigin> : std::true_type { };

} // namespace IPC
