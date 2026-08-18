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
#include "IsolatedSiteStore.h"

#include "IsolatedSitePersistence.h"
#include <cmath>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include "WebPrivacyHelpers.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IsolatedSiteStore);

static WallTime roundDownToDay(WallTime time)
{
    constexpr double secondsPerDay = 24 * 60 * 60;
    auto seconds = time.secondsSinceEpoch().value();
    if (!std::isfinite(seconds) || seconds <= 0)
        return { };

    return WallTime::fromRawSeconds(std::floor(seconds / secondsPerDay) * secondsPerDay);
}

static WallTime today()
{
    return roundDownToDay(WallTime::now());
}

WorkQueue& IsolatedSiteStore::sharedWorkQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue = WorkQueue::create("IsolatedSiteStore Work Queue"_s);
    return workQueue.get();
}

Ref<IsolatedSiteStore> IsolatedSiteStore::create(const String& databaseDirectoryPath, UserInteractionDomainFetcher&& userInteractionDomainFetcher, bool highValueFraudTargetDomainsEnabled, HashSet<WebCore::RegistrableDomain>&& additionalSitesForTesting)
{
    return adoptRef(*new IsolatedSiteStore(databaseDirectoryPath, WTF::move(userInteractionDomainFetcher), highValueFraudTargetDomainsEnabled, WTF::move(additionalSitesForTesting)));
}

IsolatedSiteStore::IsolatedSiteStore(const String& databaseDirectoryPath, UserInteractionDomainFetcher&& userInteractionDomainFetcher, bool highValueFraudTargetDomainsEnabled, HashSet<WebCore::RegistrableDomain>&& additionalSitesForTesting)
    : m_userInteractionDomainFetcher(WTF::move(userInteractionDomainFetcher))
    , m_additionalSitesForTesting(WTF::move(additionalSitesForTesting))
    , m_isPersistent(!databaseDirectoryPath.isEmpty())
    , m_highValueFraudTargetDomainsEnabled(highValueFraudTargetDomainsEnabled)
{
    ASSERT(isMainRunLoop());

    if (!m_isPersistent) {
        m_isLoaded = true;
        becomeReady();
        return;
    }

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, path = crossThreadCopy(databaseDirectoryPath)] mutable {
        assertIsCurrent(sharedWorkQueueSingleton());

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_persistence = makeUnique<IsolatedSitePersistence>(WTF::move(path));

        HashMap<WebCore::RegistrableDomain, Entry> sites;
        for (auto& [domain, record] : protectedThis->m_persistence->allSites()) {
            // A bit written by a newer WebKit has no meaning here.
            auto signals = OptionSet<Signal>::fromRaw(record.signals & persistedSignals.toRaw());
            if (signals.isEmpty())
                continue;

            sites.set(domain, Entry { signals, record.lastUpdated });
        }
        bool didImportUserInteractions = protectedThis->m_persistence->didImportUserInteractions();

        callOnMainRunLoop([protectedThis = WTF::move(protectedThis), sites = WTF::move(sites), didImportUserInteractions] mutable {
            protectedThis->didLoadSites(WTF::move(sites), didImportUserInteractions);
        });
    });
}

IsolatedSiteStore::~IsolatedSiteStore()
{
    ASSERT(isMainRunLoop());

    // Every pending handler holds a strong reference to this store or to its owner.
    ASSERT(m_loadCompletionHandlers.isEmpty());
    ASSERT(m_readyCompletionHandlers.isEmpty());

    sharedWorkQueueSingleton().dispatch([persistence = WTF::move(m_persistence)] { });
}

void IsolatedSiteStore::didLoadSites(HashMap<WebCore::RegistrableDomain, Entry>&& sites, bool didImportUserInteractions)
{
    ASSERT(isMainRunLoop());
    ASSERT(!m_isReady);

    for (auto& [domain, loadedEntry] : sites) {
        auto addResult = m_sites.add(domain, loadedEntry);
        if (addResult.isNewEntry)
            continue;

        // A signal recorded while the load was in flight was written after the load read the table, so
        // the row on disk no longer carries what was just read back.
        auto& entry = addResult.iterator->value;
        entry.signals.add(loadedEntry.signals);
        entry.lastUpdated = std::max(entry.lastUpdated, loadedEntry.lastUpdated);
        saveSite(domain, entry);
    }

    m_isLoaded = true;
    for (auto& completionHandler : std::exchange(m_loadCompletionHandlers, { }))
        completionHandler();

    if (didImportUserInteractions || !m_userInteractionDomainFetcher)
        return becomeReady();

    importUserInteractions();
}

void IsolatedSiteStore::whenLoaded(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    if (m_isLoaded)
        return completionHandler();

    m_loadCompletionHandlers.append(WTF::move(completionHandler));
}

void IsolatedSiteStore::skipImport()
{
    ASSERT(isMainRunLoop());

    if (m_importSuppressed)
        return;

    m_importSuppressed = true;

    m_userInteractionDomainFetcher = nullptr;

    if (m_isPersistent) {
        sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
            assertIsCurrent(sharedWorkQueueSingleton());

            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_persistence)
                protectedThis->m_persistence->setDidImportUserInteractions();
        });
    }

    if (m_isLoaded && !m_isReady)
        becomeReady();
}

void IsolatedSiteStore::importUserInteractions()
{
    ASSERT(isMainRunLoop());
    ASSERT(!m_isReady);

    std::exchange(m_userInteractionDomainFetcher, { })([protectedThis = Ref { *this }](std::optional<HashMap<WebCore::RegistrableDomain, WallTime>>&& domains) mutable {
        // A removal arrived while this was in flight and has already recorded the import as done.
        if (protectedThis->m_importSuppressed)
            return;

        // Leave the import flag in database unset so a later launch can import if tracking prevention is turned on.
        if (!domains)
            return protectedThis->becomeReady();

        auto now = today();
        for (auto& [domain, interactionTime] : *domains) {
            if (domain.isEmpty())
                continue;
            auto lastUpdated = roundDownToDay(interactionTime);
            if (!lastUpdated || lastUpdated > now)
                lastUpdated = now;
            protectedThis->recordSignals(domain, { Signal::FirstPartyVisit, Signal::FirstPartyUserGesture }, lastUpdated);
        }

        sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { protectedThis.get() }] {
            assertIsCurrent(sharedWorkQueueSingleton());

            RefPtr protectedThis = weakThis.get();
            if (protectedThis && protectedThis->m_persistence)
                protectedThis->m_persistence->setDidImportUserInteractions();
        });

        protectedThis->becomeReady();
    });
}

void IsolatedSiteStore::becomeReady()
{
    ASSERT(isMainRunLoop());
    ASSERT(!m_isReady);

    // Injected after the load and any import so that nothing written here reaches the database. Only
    // the signal is added: stamping lastUpdated would make a real visit on the same day look already
    // recorded and so never be written.
    for (auto& domain : std::exchange(m_additionalSitesForTesting, { }))
        m_sites.add(domain, Entry { }).iterator->value.signals.add(Signal::FirstPartyVisit);

    m_isReady = true;

    for (auto& completionHandler : std::exchange(m_readyCompletionHandlers, { }))
        completionHandler();
}

void IsolatedSiteStore::whenReady(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    if (m_isReady)
        return completionHandler();

    m_readyCompletionHandlers.append(WTF::move(completionHandler));
}

void IsolatedSiteStore::saveSite(const WebCore::RegistrableDomain& domain, const Entry& entry)
{
    ASSERT(isMainRunLoop());

    if (!m_isPersistent)
        return;

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, domain = crossThreadCopy(domain), record = IsolatedSitePersistence::SiteRecord { entry.signals.toRaw(), entry.lastUpdated }] {
        assertIsCurrent(sharedWorkQueueSingleton());

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_persistence)
            return;

        protectedThis->m_persistence->setRecordForSite(domain, record);
    });
}

void IsolatedSiteStore::recordSignals(const WebCore::RegistrableDomain& domain, OptionSet<Signal> signals, WallTime lastUpdated)
{
    ASSERT(isMainRunLoop());

    auto& entry = m_sites.add(domain, Entry { }).iterator->value;
    if (entry.signals.containsAll(signals) && entry.lastUpdated >= lastUpdated)
        return;

    entry.signals.add(signals);
    entry.lastUpdated = std::max(entry.lastUpdated, lastUpdated);
    saveSite(domain, entry);
}

void IsolatedSiteStore::addSite(const WebCore::Site& site, Signal signal)
{
    recordSignals(site.domain(), signal, today());
}

void IsolatedSiteStore::allDomains(CompletionHandler<void(Vector<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    whenLoaded([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler(copyToVector(protectedThis->m_sites.keys()));
    });
}

void IsolatedSiteStore::removeAllSites(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    skipImport();

    whenLoaded([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] mutable {
        protectedThis->m_sites.clear();

        if (!protectedThis->m_isPersistent)
            return completionHandler();

        sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { protectedThis.get() }, completionHandler = WTF::move(completionHandler)] mutable {
            assertIsCurrent(sharedWorkQueueSingleton());

            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_persistence)
                protectedThis->m_persistence->deleteAllSites();

            callOnMainRunLoop(WTF::move(completionHandler));
        });
    });
}

void IsolatedSiteStore::removeSitesUpdatedSince(WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    // Rounding the cutoff down too makes this over-delete rather than under-delete.
    auto cutoff = roundDownToDay(modifiedSince);

    whenLoaded([protectedThis = Ref { *this }, cutoff, completionHandler = WTF::move(completionHandler)] mutable {
        protectedThis->m_sites.removeIf([cutoff](auto& entry) {
            return entry.value.lastUpdated >= cutoff;
        });

        if (!protectedThis->m_isPersistent)
            return completionHandler();

        sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { protectedThis.get() }, cutoff, completionHandler = WTF::move(completionHandler)] mutable {
            assertIsCurrent(sharedWorkQueueSingleton());

            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_persistence)
                protectedThis->m_persistence->deleteSitesUpdatedSince(cutoff);

            callOnMainRunLoop(WTF::move(completionHandler));
        });
    });
}

void IsolatedSiteStore::removeSites(Vector<WebCore::RegistrableDomain>&& domains, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    if (domains.isEmpty())
        return completionHandler();

    whenLoaded([protectedThis = Ref { *this }, domains = WTF::move(domains), completionHandler = WTF::move(completionHandler)] mutable {
        for (auto& domain : domains)
            protectedThis->m_sites.remove(domain);

        if (!protectedThis->m_isPersistent)
            return completionHandler();

        sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { protectedThis.get() }, domains = crossThreadCopy(WTF::move(domains)), completionHandler = WTF::move(completionHandler)] mutable {
            assertIsCurrent(sharedWorkQueueSingleton());

            if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_persistence)
                protectedThis->m_persistence->deleteSites(domains);

            callOnMainRunLoop(WTF::move(completionHandler));
        });
    });
}

OptionSet<IsolatedSiteStore::Signal> IsolatedSiteStore::reasonsFor(const WebCore::Site& site) const
{
    ASSERT(isMainRunLoop());

    OptionSet<Signal> signals;
    if (auto it = m_sites.find(site.domain()); it != m_sites.end())
        signals = it->value.signals;

    if (isHighValueFraudTargetDomain(site.domain()))
        signals.add(Signal::HighValueFraudTarget);

    return signals;
}

bool IsolatedSiteStore::contains(const WebCore::Site& site) const
{
    return containsDomain(site.domain());
}

bool IsolatedSiteStore::containsDomain(const WebCore::RegistrableDomain& domain) const
{
    ASSERT(isMainRunLoop());

    return m_sites.contains(domain) || isHighValueFraudTargetDomain(domain);
}

bool IsolatedSiteStore::isHighValueFraudTargetDomain(const WebCore::RegistrableDomain& domain) const
{
    ASSERT(isMainRunLoop());

    if (!m_highValueFraudTargetDomainsEnabled)
        return false;

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    return HighValueFraudTargetDomainsController::singleton().contains(domain);
#else
    UNUSED_PARAM(domain);
    return false;
#endif
}

void IsolatedSiteStore::setHighValueFraudTargetDomainsEnabled(bool enabled)
{
    ASSERT(isMainRunLoop());

    m_highValueFraudTargetDomainsEnabled = enabled;
}

} // namespace WebKit
