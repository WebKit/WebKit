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

#include <WebCore/RegistrableDomain.h>
#include <WebCore/Site.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WallTime.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class IsolatedSitePersistence;

class IsolatedSiteStore final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<IsolatedSiteStore, WTF::DestructionThread::MainRunLoop> {
    WTF_MAKE_TZONE_ALLOCATED(IsolatedSiteStore);
public:
    enum class Signal : uint8_t {
        Autofill = 1 << 0,
        FirstPartyVisit = 1 << 1,
        FirstPartyUserGesture = 1 << 2,
        HighValueFraudTarget = 1 << 3,
    };
    // HighValueFraudTarget is derived from WebPrivacy's list on every lookup, so it is deliberately
    // absent here: it must never be written to or read back from IsolatedSitePersistence.
    static constexpr OptionSet<Signal> persistedSignals { Signal::Autofill, Signal::FirstPartyVisit, Signal::FirstPartyUserGesture };

    using UserInteractionDomainFetcher = Function<void(CompletionHandler<void(std::optional<HashMap<WebCore::RegistrableDomain, WallTime>>&&)>&&)>;
    static Ref<IsolatedSiteStore> create(const String& databaseDirectoryPath, UserInteractionDomainFetcher&&, bool highValueFraudTargetDomainsEnabled, HashSet<WebCore::RegistrableDomain>&& additionalSitesForTesting);
    ~IsolatedSiteStore();

    void addSite(const WebCore::Site&, Signal);
    OptionSet<Signal> reasonsFor(const WebCore::Site&) const;
    bool contains(const WebCore::Site&) const;
    bool containsDomain(const WebCore::RegistrableDomain&) const;
    void allDomains(CompletionHandler<void(Vector<WebCore::RegistrableDomain>&&)>&&);
    void removeAllSites(CompletionHandler<void()>&&);
    void removeSitesUpdatedSince(WallTime, CompletionHandler<void()>&&);
    void removeSites(Vector<WebCore::RegistrableDomain>&&, CompletionHandler<void()>&&);

    // Aggregated across every page using the owning WebsiteDataStore; see
    // WebsiteDataStore::updateIsolatedSiteStoreSettings().
    void setHighValueFraudTargetDomainsEnabled(bool);

    bool isReady() const { return m_isReady; }
    void whenReady(CompletionHandler<void()>&&);

private:
    IsolatedSiteStore(const String& databaseDirectoryPath, UserInteractionDomainFetcher&&, bool highValueFraudTargetDomainsEnabled, HashSet<WebCore::RegistrableDomain>&&);

    static WorkQueue& sharedWorkQueueSingleton();

    struct Entry {
        OptionSet<Signal> signals;
        WallTime lastUpdated;
    };

    bool isHighValueFraudTargetDomain(const WebCore::RegistrableDomain&) const;

    void didLoadSites(HashMap<WebCore::RegistrableDomain, Entry>&&, bool didImportUserInteractions);
    void whenLoaded(CompletionHandler<void()>&&);
    void importUserInteractions();
    void becomeReady();

    void skipImport();
    void recordSignals(const WebCore::RegistrableDomain&, OptionSet<Signal>, WallTime lastUpdated);
    void saveSite(const WebCore::RegistrableDomain&, const Entry&);

    HashMap<WebCore::RegistrableDomain, Entry> m_sites;
    Vector<CompletionHandler<void()>> m_loadCompletionHandlers;
    Vector<CompletionHandler<void()>> m_readyCompletionHandlers;
    UserInteractionDomainFetcher m_userInteractionDomainFetcher;
    HashSet<WebCore::RegistrableDomain> m_additionalSitesForTesting;
    const bool m_isPersistent { false };
    bool m_highValueFraudTargetDomainsEnabled { false };
    bool m_isLoaded { false };
    bool m_isReady { false };
    bool m_importSuppressed { false };
    std::unique_ptr<IsolatedSitePersistence> m_persistence WTF_GUARDED_BY_CAPABILITY(sharedWorkQueueSingleton());
};

} // namespace WebKit
