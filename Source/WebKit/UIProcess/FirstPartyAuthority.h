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

namespace WebKit {

class FirstPartyAuthority : public IPC::UntrustedValidation<FirstPartyAuthority> {
public:
    explicit FirstPartyAuthority(const WebProcessProxy& process)
        : m_process(process)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::SecurityOriginData& origin) const
    {
        return failureFor(WebCore::RegistrableDomain { origin });
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::RegistrableDomain& domain) const
    {
        return failureFor(domain);
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::Site& site) const
    {
        return failureFor(site.domain());
    }

private:
    std::optional<IPC::ValidationFailure> failureFor(const WebCore::RegistrableDomain& domain) const
    {
        // Without site isolation, WebProcessProxy records only the main frame's site while the
        // process hosts every site the page pulls in, so this question has no useful answer.
        auto& preferences = m_process->sharedPreferencesForWebProcessValue();
        if (!preferences.siteIsolationEnabled || preferences.usesSingleWebProcess)
            return std::nullopt;

        switch (m_process->allowsFirstPartyAccess(domain)) {
        case WebProcessProxy::FirstPartyAccessResult::Pass:
            return std::nullopt;
        case WebProcessProxy::FirstPartyAccessResult::SilentFailure:
            return IPC::ValidationFailure::Ignore;
        case WebProcessProxy::FirstPartyAccessResult::HardFailure:
            return IPC::ValidationFailure::Terminate;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    Ref<const WebProcessProxy> m_process;
};

class CommittedClientOriginAuthority : public IPC::UntrustedValidation<CommittedClientOriginAuthority> {
public:
    explicit CommittedClientOriginAuthority(const WebProcessProxy& process)
        : m_process(process)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::ClientOrigin& origin) const
    {
        if (!m_process->hasCommittedClientOrigin(origin))
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

private:
    Ref<const WebProcessProxy> m_process;
};

} // namespace WebKit

namespace IPC {

template<> struct IsPreordainedValidator<WebKit::FirstPartyAuthority, WebCore::SecurityOriginData> : std::true_type { };
template<> struct IsPreordainedValidator<WebKit::FirstPartyAuthority, WebCore::RegistrableDomain> : std::true_type { };
template<> struct IsPreordainedValidator<WebKit::FirstPartyAuthority, WebCore::Site> : std::true_type { };

template<> struct IsPreordainedValidator<WebKit::CommittedClientOriginAuthority, WebCore::ClientOrigin> : std::true_type { };

} // namespace IPC
