/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AXGeometryManager.h"

#include "AXLoggerBase.h"
#include "DocumentPage.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include "AXIsolatedTree.h"
#include "AXObjectCache.h"
#include "Page.h"
#include <array>

#if PLATFORM(MAC)
#include "PlatformScreen.h"
#endif

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXGeometryManager);

AXGeometryManager::AXGeometryManager(AXObjectCache& owningCache)
    : m_cache(owningCache)
    , m_updateObjectRegionsTimer(*this, &AXGeometryManager::updateObjectRegionsTimerFired)
{
}

AXGeometryManager::AXGeometryManager()
    : m_cache(nullptr)
    , m_updateObjectRegionsTimer(*this, &AXGeometryManager::updateObjectRegionsTimerFired)
{
}

AXGeometryManager::~AXGeometryManager()
{
    if (m_updateObjectRegionsTimer.isActive())
        m_updateObjectRegionsTimer.stop();
}

std::optional<IntRect> AXGeometryManager::cachedRectForID(AXID axID)
{
    auto rectIterator = m_cachedRects.find(axID);
    if (rectIterator != m_cachedRects.end())
        return rectIterator->value;
    return std::nullopt;
}

bool AXGeometryManager::cacheRectIfNeeded(AXID axID, IntRect&& rect)
{
    AX_ASSERT(AXObjectCache::isIsolatedTreeEnabled());

    auto rectIterator = m_cachedRects.find(axID);

    bool rectChanged = false;
    if (rectIterator != m_cachedRects.end()) {
        rectChanged = rectIterator->value != rect;
        if (rectChanged)
            rectIterator->value = rect;
    } else {
        rectChanged = true;
        m_cachedRects.set(axID, rect);
    }

    if (!rectChanged)
        return false;

    invalidateHitTestCacheForID(axID);

    RefPtr tree = AXIsolatedTree::treeForFrameID(m_cache->frameID());
    if (!tree)
        return false;
    tree->updateFrame(axID, WTF::move(rect));
    return true;
}

void AXGeometryManager::scheduleObjectRegionsUpdate(bool scheduleImmediately)
{
    if (!scheduleImmediately) [[likely]] {
        if (!m_updateObjectRegionsTimer.isActive())
            m_updateObjectRegionsTimer.startOneShot(1_s);
        return;
    }

    if (m_updateObjectRegionsTimer.isActive())
        m_updateObjectRegionsTimer.stop();
    scheduleRenderingUpdate();
}

// The page is about to update accessibility object regions, so the deferred
// update queued with this timer is unnecessary.
void AXGeometryManager::willUpdateObjectRegions()
{
    if (m_updateObjectRegionsTimer.isActive())
        m_updateObjectRegionsTimer.stop();

    if (!m_cache)
        return;

    if (RefPtr tree = AXIsolatedTree::treeForFrameID(m_cache->frameID()))
        tree->updateRootScreenRelativePosition();
}

void AXGeometryManager::scheduleRenderingUpdate()
{
    if (!m_cache || !m_cache->document())
        return;

    if (RefPtr page = m_cache->document()->page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::AccessibilityRegionUpdate);
}

#if PLATFORM(MAC)
void AXGeometryManager::initializePrimaryScreenRect()
{
    Locker locker { m_primaryScreenRectLock };
    m_primaryScreenRect = screenRectForPrimaryScreen();
}

FloatRect AXGeometryManager::primaryScreenRect()
{
    Locker locker { m_primaryScreenRectLock };
    return m_primaryScreenRect;
}
#endif

std::optional<AXID> AXGeometryManager::cachedHitTestResult(const IntPoint& screenPoint)
{
    Locker locker { m_hitTestCacheLock };

    static constexpr int MaxCacheRadius = 5;
    static constexpr int MaxCacheRadiusSquared = MaxCacheRadius * MaxCacheRadius;

    std::optional<AXID> closestMatch;
    int closestDistanceSquared = INT_MAX;
    auto now = MonotonicTime::now();

    // m_hitTestCache contains a mapping of points to elements at those points.
    // Iterate the cache to find the closest cached point to |screenPoint|.
    // Only consider entries within MaxCacheRadius pixels, and return the
    // element at the nearest cached point.
    //
    // We compare squared distances to test closeness in both x and y, and also
    // to avoid expensive sqrt() calls.
    //
    // e.g., if the |screenPoint| is 3px off in the x-coordinate, and 4px off in the y-coordinate:
    //   3² + 4² = 25 — Less than or equal to MaxCacheRadiusSquared, and thus is acceptably close.
    // But take the case where the hit-point is 4px off in both x and y:
    //   4² + 4² = 32 — Too far from MaxCacheRadiusSquared, so not a match.
    for (auto& entry : m_hitTestCache) {
        if (now > entry.expirationTime)
            continue;

        int dx = screenPoint.x() - entry.hitPoint.x();
        int dy = screenPoint.y() - entry.hitPoint.y();
        int distanceSquared = dx * dx + dy * dy;

        if (distanceSquared <= MaxCacheRadiusSquared && distanceSquared < closestDistanceSquared) {
            closestDistanceSquared = distanceSquared;
            closestMatch = entry.resultID;
        }
    }

    return closestMatch;
}

constexpr MonotonicTime hitTestExpirationTime()
{
    return MonotonicTime::now() + Accessibility::HitTestCacheExpiration;
}

void AXGeometryManager::cacheHitTestResult(AXID resultID, const IntPoint& hitPoint)
{
    Locker locker { m_hitTestCacheLock };

    // Check if we already have an entry for this exact point and update it.
    auto now = MonotonicTime::now();
    // Also track whether any entries are expired for cleanup.
    bool hasExpiredEntries = false;
    for (auto& entry : m_hitTestCache) {
        if (entry.hitPoint == hitPoint) {
            entry.resultID = resultID;
            entry.expirationTime = hitTestExpirationTime();
            return;
        }
        if (now > entry.expirationTime)
            hasExpiredEntries = true;
    }

    if (hasExpiredEntries) {
        m_hitTestCache.removeAllMatching([now](const HitTestCacheEntry& entry) {
            return now > entry.expirationTime;
        });
    }

    // If cache is full, remove the oldest entry (first one, since we append new ones at the end).
    if (m_hitTestCache.size() >= HitTestCacheSize)
        m_hitTestCache.removeAt(0);

    m_hitTestCache.append({ hitPoint, resultID, hitTestExpirationTime() });
}

void AXGeometryManager::expandHitTestCacheAroundPoint(const IntPoint& center, AXTreeID treeID)
{
    incrementProbeGeneration();
    uint64_t capturedGeneration = currentProbeGeneration();

    constexpr int ProbeDistance = 5;
    std::array<IntPoint, 4> probePoints = {
        IntPoint(center.x() - ProbeDistance, center.y()),
        IntPoint(center.x() + ProbeDistance, center.y()),
        IntPoint(center.x(), center.y() - ProbeDistance),
        IntPoint(center.x(), center.y() + ProbeDistance),
    };

    for (const auto& probePoint : probePoints) {
        // Skip probes with negative coordinates to avoid unnecessary main-thread trips.
        if (probePoint.x() < 0 || probePoint.y() < 0)
            continue;

        ensureOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, probePoint, capturedGeneration, treeID] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            // Check if the probe was cancelled by an assistive technology requesting
            // a new hit-test location (which will fire new probes from a different spot).
            if (protectedThis->currentProbeGeneration() != capturedGeneration)
                return;

            // Perform hit test.
            if (WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID)) {
                auto pageRelativePoint = cache->mapScreenPointToPagePoint(probePoint);
                RefPtr root = cache->rootWebArea();
                if (RefPtr hitResult = root ? root->accessibilityHitTest(pageRelativePoint) : nullptr)
                    protectedThis->cacheHitTestResult(hitResult->objectID(), probePoint);
            }
        });
    }
}

void AXGeometryManager::invalidateHitTestCacheForID(AXID axID)
{
    Locker locker { m_hitTestCacheLock };

    m_hitTestCache.removeAllMatching([axID](const HitTestCacheEntry& entry) {
        return entry.resultID == axID;
    });
}

void AXGeometryManager::clearHitTestCache()
{
    Locker locker { m_hitTestCacheLock };
    m_hitTestCache.clear();
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
