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

std::optional<IntRect> AXGeometryManager::paintRectForID(AXID axID)
{
    auto paintRectIterator = m_paintRects.find(axID);
    if (paintRectIterator != m_paintRects.end())
        return paintRectIterator->value;
    return std::nullopt;
}

void AXGeometryManager::onPaint(AXID axID, IntRect&& paintRect)
{
    // We shouldn't call this method on a geometry manager that has no page ID.
    ASSERT(m_cache->pageID());
    ASSERT(AXObjectCache::isIsolatedTreeEnabled());

    if (!axID.isValid())
        return;
    auto paintRectIterator = m_paintRects.find(axID);

    bool paintRectChanged = false;
    if (paintRectIterator != m_paintRects.end()) {
        paintRectChanged = paintRectIterator->value != paintRect;
        if (paintRectChanged)
            paintRectIterator->value = paintRect;
    } else {
        paintRectChanged = true;
        m_paintRects.set(axID, paintRect);
    }

    if (!paintRectChanged)
        return;

    RefPtr tree = AXIsolatedTree::treeForPageID(*m_cache->pageID());
    if (!tree)
        return;
    tree->updateFrame(axID, WTFMove(paintRect));
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
