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

#include "Untrusted.h"
#include "WebProcessProxy.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Site.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

enum class ShouldCheckWithoutSiteIsolation : bool { No, Yes };
enum class ShouldIgnoreWithoutRegistrableDomain : bool { No, Yes };

// Without site isolation, WebProcessProxy records only the main frame's site while the process hosts
// every site the page pulls in, so no first-party question has a useful answer.
inline bool firstPartyAccessIsAnswerable(const WebProcessProxy& process)
{
    auto& preferences = process.sharedPreferencesForWebProcessValue();
    return preferences.siteIsolationEnabled && !preferences.usesSingleWebProcess;
}

// Maps WebProcessProxy's three-way answer onto a validation outcome: no failure for Pass, and a
// drop rather than a termination for SilentFailure, which happens legitimately as a new load starts.
inline std::optional<IPC::ValidationFailure> checkFirstPartyAccessResult(WebProcessProxy::FirstPartyAccessResult result)
{
    switch (result) {
    case WebProcessProxy::FirstPartyAccessResult::Pass:
        return std::nullopt;
    case WebProcessProxy::FirstPartyAccessResult::SilentFailure:
        return IPC::ValidationFailure::Ignore;
    case WebProcessProxy::FirstPartyAccessResult::HardFailure:
        return IPC::ValidationFailure::Terminate;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline std::optional<IPC::ValidationFailure> checkFirstPartyAccess(const WebProcessProxy& process, const WebCore::RegistrableDomain& domain)
{
    return checkFirstPartyAccessResult(process.allowsFirstPartyAccess(domain));
}

// Use for a value whose registrable domain the sending process is claiming as its own. Note the
// granularity: a process serving a.example.com speaks for b.example.com too.
class ProcessSpeaksForDomain : public IPC::CanValidateUntrusted<ProcessSpeaksForDomain> {
public:
    // Callers that answered the question before this validation existed, and so must keep answering
    // it as they did, pass Yes for both.
    explicit ProcessSpeaksForDomain(const WebProcessProxy& process, ShouldCheckWithoutSiteIsolation shouldCheckWithoutSiteIsolation = ShouldCheckWithoutSiteIsolation::No, ShouldIgnoreWithoutRegistrableDomain shouldIgnoreWithoutRegistrableDomain = ShouldIgnoreWithoutRegistrableDomain::No)
        : m_process(process)
        , m_shouldCheckWithoutSiteIsolation(shouldCheckWithoutSiteIsolation)
        , m_shouldIgnoreWithoutRegistrableDomain(shouldIgnoreWithoutRegistrableDomain)
    {
    }

    // For the common case where the process being asked is the one that sent the message.
    explicit ProcessSpeaksForDomain(const IPC::Connection& connection, ShouldCheckWithoutSiteIsolation shouldCheckWithoutSiteIsolation = ShouldCheckWithoutSiteIsolation::No, ShouldIgnoreWithoutRegistrableDomain shouldIgnoreWithoutRegistrableDomain = ShouldIgnoreWithoutRegistrableDomain::No)
        : ProcessSpeaksForDomain(WebProcessProxy::fromConnection(connection), shouldCheckWithoutSiteIsolation, shouldIgnoreWithoutRegistrableDomain)
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
    // Takes the domain as a function rather than a value because deriving one from an origin
    // consults the public suffix list under a global lock, and the guard below usually discards it.
    template<typename DomainFunction>
    std::optional<IPC::ValidationFailure> checkUntrustedDomain(NOESCAPE const DomainFunction& domain) const
    {
        RefPtr process = m_process.get();
        if (!process)
            return IPC::ValidationFailure::Ignore;

        if (m_shouldCheckWithoutSiteIsolation == ShouldCheckWithoutSiteIsolation::No && !firstPartyAccessIsAnswerable(*process))
            return std::nullopt;

        if (m_shouldIgnoreWithoutRegistrableDomain == ShouldIgnoreWithoutRegistrableDomain::No && process->hasNoRegistrableDomain())
            return std::nullopt;

        // An opaque origin has no host and so no registrable domain to compare
        auto&& derivedDomain = domain();
        if (derivedDomain.isEmpty())
            return std::nullopt;

        return checkFirstPartyAccess(*process, derivedDomain);
    }

    // Weak, so that constructing a validator cannot extend a process's life, and a validator that
    // outlives its process drops the message rather than crashing.
    WeakPtr<const WebProcessProxy> m_process;
    ShouldCheckWithoutSiteIsolation m_shouldCheckWithoutSiteIsolation;
    ShouldIgnoreWithoutRegistrableDomain m_shouldIgnoreWithoutRegistrableDomain;
};

// Use for a value naming the top-level site of a page rather than something the sending process
// speaks for itself: a cross-origin subframe's process legitimately observes and relays facts about
// the page it is part of, so the sender is not the judge. Whichever process hosts each participating
// page's main frame is asked instead.
class ProcessParticipatesInPageWithSite : public IPC::CanValidateUntrusted<ProcessParticipatesInPageWithSite> {
public:
    explicit ProcessParticipatesInPageWithSite(const WebProcessProxy& process)
        : m_process(process)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::Site& site) const
    {
        RefPtr process = m_process.get();
        if (!process)
            return IPC::ValidationFailure::Ignore;

        if (!firstPartyAccessIsAnswerable(*process))
            return std::nullopt;

        return checkFirstPartyAccessResult(process->participatesInPageWithFirstPartySite(site));
    }

private:
    WeakPtr<const WebProcessProxy> m_process;
};

// Use to check if this process has committed a load for this exact (top origin, client origin) pair.
class ProcessCommittedClientOrigin : public IPC::CanValidateUntrusted<ProcessCommittedClientOrigin> {
public:
    explicit ProcessCommittedClientOrigin(const WebProcessProxy& process)
        : m_process(process)
    {
    }

    explicit ProcessCommittedClientOrigin(const IPC::Connection& connection)
        : ProcessCommittedClientOrigin(WebProcessProxy::fromConnection(connection))
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::ClientOrigin& origin) const
    {
        RefPtr process = m_process.get();
        if (!process)
            return IPC::ValidationFailure::Ignore;

        if (!process->hasCommittedClientOrigin(origin))
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

private:
    WeakPtr<const WebProcessProxy> m_process;
};

} // namespace WebKit

namespace IPC {

template<> struct IsValidationProcedureFor<WebKit::ProcessSpeaksForDomain, WebCore::SecurityOriginData> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::ProcessSpeaksForDomain, WebCore::RegistrableDomain> : std::true_type { };
template<> struct IsValidationProcedureFor<WebKit::ProcessSpeaksForDomain, WebCore::Site> : std::true_type { };

template<> struct IsValidationProcedureFor<WebKit::ProcessParticipatesInPageWithSite, WebCore::Site> : std::true_type { };

template<> struct IsValidationProcedureFor<WebKit::ProcessCommittedClientOrigin, WebCore::ClientOrigin> : std::true_type { };

} // namespace IPC
