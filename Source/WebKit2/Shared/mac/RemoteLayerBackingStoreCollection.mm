 /*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteLayerBackingStoreCollection.h"

#import "PlatformCALayerRemote.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"

const std::chrono::seconds purgeableBackingStoreAgeThreshold = 1_s;
const std::chrono::seconds purgeabilityTimerInterval = 1_s;

namespace WebKit {

RemoteLayerBackingStoreCollection::RemoteLayerBackingStoreCollection(RemoteLayerTreeContext* context)
    : m_context(context)
    , m_purgeabilityTimer(this, &RemoteLayerBackingStoreCollection::purgeabilityTimerFired)
{
}

void RemoteLayerBackingStoreCollection::backingStoreWasCreated(RemoteLayerBackingStore* backingStore)
{
    m_liveBackingStore.add(backingStore);
}

void RemoteLayerBackingStoreCollection::backingStoreWillBeDestroyed(RemoteLayerBackingStore* backingStore)
{
    m_liveBackingStore.remove(backingStore);
}

void RemoteLayerBackingStoreCollection::purgeabilityTimerFired(WebCore::Timer<RemoteLayerBackingStoreCollection>&)
{
    auto now = std::chrono::steady_clock::now();
    bool hadRecentlyPaintedBackingStore = false;
    bool successfullyMadeBackingStorePurgeable = true;

    for (const auto& backingStore : m_liveBackingStore) {
        if (now - backingStore->lastDisplayTime() < purgeableBackingStoreAgeThreshold) {
            hadRecentlyPaintedBackingStore = true;
            continue;
        }

        // FIXME: If the layer is unparented, we should make all buffers volatile.
        if (!backingStore->setVolatility(RemoteLayerBackingStore::Volatility::BackBufferVolatile))
            successfullyMadeBackingStorePurgeable = false;
    }

    if (!hadRecentlyPaintedBackingStore && successfullyMadeBackingStorePurgeable)
        m_purgeabilityTimer.stop();
}

void RemoteLayerBackingStoreCollection::schedulePurgeabilityTimer()
{
    if (m_purgeabilityTimer.isActive())
        return;

    m_purgeabilityTimer.startRepeating(purgeabilityTimerInterval);
}

} // namespace WebKit
