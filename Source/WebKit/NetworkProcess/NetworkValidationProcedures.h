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

inline bool processUsesSiteIsolation(NetworkProcess& networkProcess, WebCore::ProcessIdentifier identifier)
{
    RefPtr connection = networkProcess.webProcessConnection(identifier);
    if (!connection)
        return true;
    auto& preferences = connection->sharedPreferencesForWebProcessValue();
    return preferences.siteIsolationEnabled && !preferences.usesSingleWebProcess;
}

class ProcessMayClaimFirstPartyForCookies : public IPC::CanValidateUntrusted<ProcessMayClaimFirstPartyForCookies> {
public:
    ProcessMayClaimFirstPartyForCookies(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, ShouldCheckWithoutSiteIsolation shouldCheckWithoutSiteIsolation)
        : m_networkProcess(networkProcess)
        , m_webProcessIdentifier(webProcessIdentifier)
        , m_shouldCheckWithoutSiteIsolation(shouldCheckWithoutSiteIsolation)
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
        return checkUntrustedDomain([&] -> const WebCore::RegistrableDomain& {
            return domain;
        });
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::Site& site) const
    {
        return checkUntrustedDomain([&] -> const WebCore::RegistrableDomain& {
            return site.domain();
        });
    }

private:
    // Takes the domain as a function rather than a value because deriving one from an origin
    // consults the public suffix list under a global lock, and the guard below usually discards it.
    template<typename DomainFunction>
    std::optional<IPC::ValidationFailure> checkUntrustedDomain(NOESCAPE DomainFunction&& domain) const
    {
        if (m_shouldCheckWithoutSiteIsolation == ShouldCheckWithoutSiteIsolation::No && !processUsesSiteIsolation(m_networkProcess, m_webProcessIdentifier))
            return std::nullopt;

        // An opaque origin has no registrable domain to compare.
        auto&& derivedDomain = domain();
        if (derivedDomain.isEmpty())
            return std::nullopt;

        if (!m_networkProcess->allowsFirstPartyForCookies(m_webProcessIdentifier, derivedDomain))
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

    const Ref<NetworkProcess> m_networkProcess;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
    ShouldCheckWithoutSiteIsolation m_shouldCheckWithoutSiteIsolation;
};

class ProcessHostsDomain : public IPC::CanValidateUntrusted<ProcessHostsDomain> {
public:
    ProcessHostsDomain(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, ShouldCheckWithoutSiteIsolation shouldCheckWithoutSiteIsolation)
        : m_networkProcess(networkProcess)
        , m_webProcessIdentifier(webProcessIdentifier)
        , m_shouldCheckWithoutSiteIsolation(shouldCheckWithoutSiteIsolation)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::RegistrableDomain& domain) const
    {
        return checkUntrustedDomain([&] -> const WebCore::RegistrableDomain& {
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
        return ProcessMayClaimFirstPartyForCookies { m_networkProcess, m_webProcessIdentifier, m_shouldCheckWithoutSiteIsolation }.checkUntrusted(origin.topOrigin);
    }

private:
    // Takes the domain as a function rather than a value because deriving one from an origin
    // consults the public suffix list under a global lock, and the guard below usually discards it.
    template<typename DomainFunction>
    std::optional<IPC::ValidationFailure> checkUntrustedDomain(NOESCAPE DomainFunction&& domain) const
    {
        // Only a shortcut: hostsDomain() accepts every domain for such a process.
        if (RefPtr connection = m_networkProcess->webProcessConnection(m_webProcessIdentifier); connection && connection->mayHostAnyDomain())
            return std::nullopt;

        // An opaque origin has no registrable domain to compare.
        auto&& derivedDomain = domain();
        if (derivedDomain.isEmpty())
            return std::nullopt;
        if (!m_networkProcess->hostsDomain(m_webProcessIdentifier, derivedDomain))
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

    const Ref<NetworkProcess> m_networkProcess;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
    ShouldCheckWithoutSiteIsolation m_shouldCheckWithoutSiteIsolation;
};

// A WebSocket's ClientOrigin. A remote worker's shadow document reports the worker's own origin as its
// top origin (rdar://117922051), so a top origin equal to the client origin is accepted as well as a first party.
class ProcessHostsWebSocketClientOrigin : public IPC::CanValidateUntrusted<ProcessHostsWebSocketClientOrigin> {
public:
    ProcessHostsWebSocketClientOrigin(NetworkProcess& networkProcess, WebCore::ProcessIdentifier webProcessIdentifier, ShouldCheckWithoutSiteIsolation shouldCheckWithoutSiteIsolation)
        : m_processHostsDomain(networkProcess, webProcessIdentifier, shouldCheckWithoutSiteIsolation)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::ClientOrigin& origin) const
    {
        if (origin.topOrigin == origin.clientOrigin)
            return m_processHostsDomain.checkUntrusted(origin.clientOrigin);
        return m_processHostsDomain.checkUntrusted(origin);
    }

private:
    const ProcessHostsDomain m_processHostsDomain;
};

} // namespace WebKit

namespace IPC {

template<> struct IsValidationProcedureFor<WebKit::ProcessMayClaimFirstPartyForCookies, WebCore::SecurityOriginData> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::ProcessMayClaimFirstPartyForCookies, WebCore::RegistrableDomain> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::ProcessMayClaimFirstPartyForCookies, WebCore::Site> : std::true_type { };

template<> struct IsValidationProcedureFor<WebKit::ProcessHostsDomain, WebCore::RegistrableDomain> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::ProcessHostsDomain, WebCore::SecurityOriginData> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::ProcessHostsDomain, WebCore::ClientOrigin> : std::true_type { };

template<> struct IsValidationProcedureFor<WebKit::ProcessHostsWebSocketClientOrigin, WebCore::ClientOrigin> : std::true_type { };

} // namespace IPC
