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

#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

// Bound speculative work per navigation.
static constexpr unsigned maxPreloadsPerNavigation { 20 };
static const Seconds preloadExpirationTimeout { 10_s };

WTF_MAKE_TZONE_ALLOCATED_IMPL(EarlyHintsPreloadCache);

EarlyHintsPreloadCache::Entry::Entry(ResourceResponse&& response, PrivateRelayed privateRelayed, RefPtr<FragmentedSharedBuffer>&& buffer, String&& destination, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, String&& cachePartition)
    : response(WTF::move(response))
    , privateRelayed(privateRelayed)
    , buffer(WTF::move(buffer))
    , destination(WTF::move(destination))
    , mode(mode)
    , storedCredentialsPolicy(storedCredentialsPolicy)
    , cachePartition(WTF::move(cachePartition))
{
}

EarlyHintsPreloadCache::EarlyHintsPreloadCache()
    : m_expirationTimer(*this, &EarlyHintsPreloadCache::clearExpiredEntries)
{
}

EarlyHintsPreloadCache::~EarlyHintsPreloadCache() = default;

void EarlyHintsPreloadCache::registerNavigation(const NetworkCache::GlobalFrameID& frameID, const SecurityOriginData& hintingOrigin)
{
    auto& navigation = m_navigations.ensure(frameID, [] {
        return makeUniqueRef<Navigation>();
    }).iterator->value;

    // Early hints preloads survive a same-origin redirect but are discarded across a cross-origin one.
    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#create-navigation-params-by-fetching
    if (navigation->origin != hintingOrigin) {
        navigation->entries.clear();
        navigation->origin = hintingOrigin;
        navigation->expiry = WallTime::now() + preloadExpirationTimeout;
    }

    if (!m_expirationTimer.isActive())
        m_expirationTimer.startOneShot(preloadExpirationTimeout);
}

void EarlyHintsPreloadCache::store(const NetworkCache::GlobalFrameID& frameID, const SecurityOriginData& hintingOrigin, const URL& url, String&& destination, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, String&& cachePartition, ResourceResponse&& response, PrivateRelayed privateRelayed, RefPtr<FragmentedSharedBuffer>&& buffer)
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
    navigation->entries.add(url, makeUniqueRef<Entry>(WTF::move(response), privateRelayed, WTF::move(buffer), WTF::move(destination), mode, storedCredentialsPolicy, WTF::move(cachePartition)));
}

std::unique_ptr<EarlyHintsPreloadCache::Entry> EarlyHintsPreloadCache::take(const NetworkCache::GlobalFrameID& frameID, const URL& url, FetchOptions::Mode mode, StoredCredentialsPolicy storedCredentialsPolicy, const String& cachePartition)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return nullptr;

    auto& entries = navigationIterator->value->entries;
    auto entryIterator = entries.find(url);
    if (entryIterator == entries.end())
        return nullptr;

    // Ensure we don't match on wrong CORS mode, credentials mode or cache partition.
    if (entryIterator->value->mode != mode || entryIterator->value->storedCredentialsPolicy != storedCredentialsPolicy || entryIterator->value->cachePartition != cachePartition)
        return nullptr;

    auto entry = entryIterator->value.moveToUniquePtr();
    entries.remove(entryIterator);
    return entry;
}

void EarlyHintsPreloadCache::pruneForFinalResponse(const NetworkCache::GlobalFrameID& frameID, const ContentSecurityPolicy& contentSecurityPolicy, const URL& baseURL)
{
    auto navigationIterator = m_navigations.find(frameID);
    if (navigationIterator == m_navigations.end())
        return;

    navigationIterator->value->entries.removeIf([&](auto& entry) {
        return !contentSecurityPolicy.decisionForSupportedPreload(entry.value->destination, entry.key, baseURL).value_or(false);
    });
}

void EarlyHintsPreloadCache::clear(const NetworkCache::GlobalFrameID& frameID)
{
    m_navigations.remove(frameID);
}

void EarlyHintsPreloadCache::clear()
{
    m_expirationTimer.stop();
    m_navigations.clear();
}

void EarlyHintsPreloadCache::clearExpiredEntries()
{
    auto now = WallTime::now();
    m_navigations.removeIf([&](auto& navigation) {
        return navigation.value->expiry <= now;
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
