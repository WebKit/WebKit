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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include "AXIsolatedTree.h"
#include "AXObjectCache.h"
#include "Page.h"

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

void AXGeometryManager::cacheRect(AXID axID, IntRect&& rect)
{
    // We shouldn't call this method on a geometry manager that has no page ID.
    ASSERT(m_cache->pageID());
    ASSERT(AXObjectCache::isIsolatedTreeEnabled());

    if (!axID.isValid())
        return;
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
        return;

    RefPtr tree = AXIsolatedTree::treeForPageID(*m_cache->pageID());
    if (!tree)
        return;
    tree->updateFrame(axID, WTFMove(rect));
}

void AXGeometryManager::scheduleObjectRegionsUpdate(bool scheduleImmediately)
{
    if (LIKELY(!scheduleImmediately)) {
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

    if (RefPtr tree = AXIsolatedTree::treeForPageID(m_cache->pageID()))
        tree->updateRootScreenRelativePosition();
}

void AXGeometryManager::scheduleRenderingUpdate()
{
    if (!m_cache)
        return;

    if (auto* page = m_cache->document().page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::AccessibilityRegionUpdate);
}

#if PLATFORM(MAC)
void AXGeometryManager::initializePrimaryScreenRect()
{
    Locker locker { m_lock };
    m_primaryScreenRect = screenRectForPrimaryScreen();
}

FloatRect AXGeometryManager::primaryScreenRect()
{
    Locker locker { m_lock };
    return m_primaryScreenRect;
}
#endif

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
