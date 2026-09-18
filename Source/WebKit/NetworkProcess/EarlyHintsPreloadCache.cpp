/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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
#include "EarlyHintsPreloadCache.h"

#include "NetworkCacheValidation.h"
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

static const Seconds preloadExpirationTimeout { 10_s };

WTF_MAKE_TZONE_ALLOCATED_IMPL(EarlyHintsPreloadCache);

EarlyHintsPreloadCache::Entry::Entry(ResourceResponse&& response, PrivateRelayed privateRelayed, RefPtr<FragmentedSharedBuffer>&& buffer, String&& destination, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, Vector<std::pair<String, String>>&& varyingRequestHeaders)
    : response(WTF::move(response))
    , privateRelayed(privateRelayed)
    , buffer(WTF::move(buffer))
    , destination(WTF::move(destination))
    , mode(mode)
    , storedCredentialsPolicy(storedCredentialsPolicy)
    , varyingRequestHeaders(WTF::move(varyingRequestHeaders))
{
}

EarlyHintsPreloadCache::EarlyHintsPreloadCache()
    : m_expirationTimer(*this, &EarlyHintsPreloadCache::clearExpiredEntries)
{
}

EarlyHintsPreloadCache::~EarlyHintsPreloadCache() = default;

void EarlyHintsPreloadCache::registerNavigation(const NetworkCache::GlobalFrameID& frameID, const SecurityOriginData& hintingOrigin, const String& cachePartition)
{
    auto& navigation = m_navigations.ensure(frameID, [] {
        return makeUniqueRef<Navigation>();
    }).iterator->value;

    // Early hints preloads survive a same-origin redirect but are discarded across a cross-origin one.
    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#create-navigation-params-by-fetching
    // Reset on either, so a navigation's entries always share one origin and partition.
    if (navigation->origin != hintingOrigin || navigation->cachePartition != cachePartition) {
        navigation->entries.clear();
        navigation->origin = hintingOrigin;
        navigation->cachePartition = cachePartition;
        navigation->expiry = WallTime::now() + preloadExpirationTimeout;
    }

    if (!m_expirationTimer.isActive())
        m_expirationTimer.startOneShot(preloadExpirationTimeout);
}

void EarlyHintsPreloadCache::store(const NetworkCache::GlobalFrameID& frameID, const SecurityOriginData& hintingOrigin, const URL& url, String&& destination, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, Vector<std::pair<String, String>>&& varyingRequestHeaders, ResourceResponse&& response, PrivateRelayed privateRelayed, RefPtr<FragmentedSharedBuffer>&& buffer)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return;

    // Reject a preload from a superseded navigation.
    auto& navigation = navigationIterator->value;
    if (navigation->origin != hintingOrigin)
        return;

    if (navigation->entries.size() >= maxPreloadsPerNavigation)
        return;
    navigation->entries.add(url, makeUniqueRef<Entry>(WTF::move(response), privateRelayed, WTF::move(buffer), WTF::move(destination), mode, storedCredentialsPolicy, WTF::move(varyingRequestHeaders)));
}

void EarlyHintsPreloadCache::addInFlightPreload(const NetworkCache::GlobalFrameID& frameID, const URL& url, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return;

    navigationIterator->value->inFlightPreloads.add(url, makeUniqueRef<InFlightPreload>(InFlightPreload { mode, storedCredentialsPolicy, { } }));
}

void EarlyHintsPreloadCache::settleInFlightPreload(const NetworkCache::GlobalFrameID& frameID, const URL& url)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return;

    auto inFlight = navigationIterator->value->inFlightPreloads.take(url);
    if (!inFlight)
        return;

    // Run after the entry has been stored, so a waiter's retry finds it.
    for (auto& waiter : std::exchange(inFlight->waiters, { }))
        waiter();
}

bool EarlyHintsPreloadCache::hasMatchingInFlightPreload(const NetworkCache::GlobalFrameID& frameID, const ResourceRequest& request, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy) const
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return false;

    auto& navigation = navigationIterator->value;
    if (navigation->cachePartition != request.cachePartition())
        return false;

    auto inFlightIterator = navigation->inFlightPreloads.find(request.url());
    if (inFlightIterator == navigation->inFlightPreloads.end())
        return false;

    // Vary cannot be checked until the response arrives, so a waiter may still miss and load normally.
    return inFlightIterator->value->mode == mode && inFlightIterator->value->storedCredentialsPolicy == storedCredentialsPolicy;
}

void EarlyHintsPreloadCache::addInFlightPreloadWaiter(const NetworkCache::GlobalFrameID& frameID, const URL& url, CompletionHandler<void()>&& waiter)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return waiter();

    auto inFlightIterator = navigationIterator->value->inFlightPreloads.find(url);
    if (inFlightIterator == navigationIterator->value->inFlightPreloads.end())
        return waiter();

    inFlightIterator->value->waiters.append(WTF::move(waiter));
}

std::unique_ptr<EarlyHintsPreloadCache::Entry> EarlyHintsPreloadCache::take(const NetworkCache::GlobalFrameID& frameID, const ResourceRequest& request, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, NetworkStorageSession* storageSession)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return nullptr;

    // Responding to a fetch with a 103 from another partition would leak authenticated content.
    auto& navigation = navigationIterator->value;
    if (navigation->cachePartition != request.cachePartition())
        return nullptr;

    auto& entries = navigation->entries;
    auto entryIterator = entries.find(request.url());
    if (entryIterator == entries.end())
        return nullptr;

    // Ensure we don't match on wrong CORS mode or credentials mode.
    if (entryIterator->value->mode != mode || entryIterator->value->storedCredentialsPolicy != storedCredentialsPolicy)
        return nullptr;

    // Ensure that request headers the response depended on haven't changed between preload and use.
    if (!NetworkCache::verifyVaryingRequestHeaders(storageSession, entryIterator->value->varyingRequestHeaders, request))
        return nullptr;

    auto entry = entryIterator->value.moveToUniquePtr();
    entries.remove(entryIterator);
    return entry;
}

void EarlyHintsPreloadCache::pruneForFinalResponse(const NetworkCache::GlobalFrameID& frameID, const ContentSecurityPolicy& contentSecurityPolicy)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return;

    navigationIterator->value->entries.removeIf([&](auto& entry) {
        return !contentSecurityPolicy.decisionForSupportedPreload(entry.value->destination, entry.key).value_or(false);
    });
}

void EarlyHintsPreloadCache::settleAll(Navigation& navigation)
{
    for (auto& inFlight : navigation.inFlightPreloads.values()) {
        for (auto& waiter : std::exchange(inFlight->waiters, { }))
            waiter();
    }
    navigation.inFlightPreloads.clear();
}

void EarlyHintsPreloadCache::clear(const NetworkCache::GlobalFrameID& frameID)
{
    if (auto navigation = m_navigations.take(frameID))
        settleAll(*navigation);
}

void EarlyHintsPreloadCache::clear()
{
    m_expirationTimer.stop();
    for (auto& navigation : std::exchange(m_navigations, { }).values())
        settleAll(navigation);
}

void EarlyHintsPreloadCache::clearExpiredEntries()
{
    auto now = WallTime::now();
    m_navigations.removeIf([&](auto& navigation) {
        if (navigation.value->expiry > now)
            return false;
        // The preload is not coming; release anyone waiting on it rather than stalling their load.
        settleAll(navigation.value);
        return true;
    });

    // Reschedule for the next expiry.
    std::optional<WallTime> nextExpiry;
    for (auto& navigation : m_navigations.values()) {
        if (!nextExpiry || navigation->expiry < *nextExpiry)
            nextExpiry = navigation->expiry;
    }
    if (nextExpiry)
        m_expirationTimer.startOneShot(std::max(0_s, *nextExpiry - now));
}

} // namespace WebKit
