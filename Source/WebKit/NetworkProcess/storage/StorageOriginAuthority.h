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

#include "NetworkStorageManager.h"
#include "Untrusted.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/URL.h>

namespace WebKit {

// The designated validation procedure for a WebCore::ClientOrigin naming a storage
// bucket, run on the storage work queue.
//
// KNOWN INCOMPLETE, in the same way as TopOriginOnlyAuthority: it validates
// origin.topOrigin against the sites the UI process authorised for this connection, and
// cannot validate origin.clientOrigin because the network process is never told which
// client origins a web process hosts - see the comment on
// NetworkStorageManager::canConnectionAccessSiteForWebStorage(). clientOrigin is the
// storage partition key and part of the on-disk path, so this remains the largest
// residual gap in the audit.
// Which of NetworkStorageManager's two site checks to apply. They are not
// interchangeable: the WebStorage one deliberately passes everything when the storage
// blocking policy is AllowAll, because third-party contexts then use non-partitioned
// storage that m_allowedSitesForConnections does not track.
enum class StoragePolicyScope : bool {
    WebStorage,
    Strict,
};

class StorageOriginAuthority : public IPC::CanValidateUntrusted<StorageOriginAuthority> {
public:
    StorageOriginAuthority(const NetworkStorageManager& manager, IPC::Connection& connection, StoragePolicyScope scope)
        : m_manager(manager)
        , m_connection(connection)
        , m_scope(scope)
    {
    }

    // A bare origin has no top origin to partition by, so it stands as its own site.
    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::SecurityOrigin& origin) const
    {
        return checkUntrustedDomain(WebCore::RegistrableDomain { origin.data() });
    }

    // The URL in a cache-storage retrieval names the record being looked for, not the site the
    // lookup is performed as.
    std::optional<IPC::ValidationFailure> checkUntrusted(const URL&) const
    {
        return IPC::unvalidated(IPC::UnvalidatedReason::RequestTarget);
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::ClientOrigin& origin) const
    {
        return checkUntrustedDomain(WebCore::RegistrableDomain { origin.topOrigin });
    }

private:
    std::optional<IPC::ValidationFailure> checkUntrustedDomain(const WebCore::RegistrableDomain& domain) const
    {
        bool allowed = m_scope == StoragePolicyScope::WebStorage
            ? m_manager->canConnectionAccessSiteForWebStorage(m_connection, domain)
            : m_manager->isSiteAllowedForConnection(m_connection->uniqueID(), domain);
        if (!allowed)
            return IPC::ValidationFailure::Terminate;
        return std::nullopt;
    }

    Ref<const NetworkStorageManager> m_manager;
    Ref<IPC::Connection> m_connection;
    StoragePolicyScope m_scope;
};

} // namespace WebKit

namespace IPC {

template<> struct IsValidationProcedureFor<WebKit::StorageOriginAuthority, WebCore::ClientOrigin> : std::true_type { };

} // namespace IPC
