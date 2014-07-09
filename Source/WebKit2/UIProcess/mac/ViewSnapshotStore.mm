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

#import "config.h"
#import "ViewSnapshotStore.h"

#import "WebBackForwardList.h"
#import "WebPageProxy.h"
#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/IOSurface.h>
#import <WebCore/UUID.h>

#if PLATFORM(IOS)
#import <QuartzCore/QuartzCorePrivate.h>
#endif

using namespace WebCore;

#if USE_IOSURFACE_VIEW_SNAPSHOTS
static const size_t maximumSnapshotCacheSize = 400 * (1024 * 1024);
#elif USE_RENDER_SERVER_VIEW_SNAPSHOTS
// Because render server snapshots are not purgeable, we should keep fewer around.
static const size_t maximumSnapshotCacheSize = 50 * (1024 * 1024);
#endif

namespace WebKit {

ViewSnapshotStore::ViewSnapshotStore()
    : m_enabled(true)
    , m_snapshotCacheSize(0)
{
}

ViewSnapshotStore::~ViewSnapshotStore()
{
    discardSnapshots();
}

ViewSnapshotStore& ViewSnapshotStore::shared()
{
    static ViewSnapshotStore& store = *new ViewSnapshotStore;
    return store;
}

#if PLATFORM(IOS)
CAContext *ViewSnapshotStore::snapshottingContext()
{
    static CAContext *context;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDictionary *options = @{
            kCAContextDisplayName: @"WebKitSnapshotting",
            kCAContextIgnoresHitTest: @YES,
            kCAContextDisplayId : @20000
        };
        context = [[CAContext remoteContextWithOptions:options] retain];
    });

    return context;
}
#endif

void ViewSnapshotStore::removeSnapshotImage(ViewSnapshot& snapshot)
{
    if (!snapshot.hasImage())
        return;

    m_snapshotCacheSize -= snapshot.imageSizeInBytes;
    snapshot.clearImage();
}

void ViewSnapshotStore::pruneSnapshots(WebPageProxy& webPageProxy)
{
    if (m_snapshotCacheSize <= maximumSnapshotCacheSize)
        return;

    uint32_t currentIndex = webPageProxy.backForwardList().currentIndex();
    uint32_t maxDistance = 0;
    auto mostDistantSnapshotIter = m_snapshotMap.end();
    auto backForwardEntries = webPageProxy.backForwardList().entries();

    // First, try to evict the snapshot for the page farthest from the current back-forward item.
    for (uint32_t i = 0, entryCount = webPageProxy.backForwardList().entries().size(); i < entryCount; i++) {
        uint32_t distance = std::max(currentIndex, i) - std::min(currentIndex, i);

        if (i == currentIndex || distance < maxDistance)
            continue;

        WebBackForwardListItem* item = backForwardEntries[i].get();
        String snapshotUUID = item->snapshotUUID();
        if (snapshotUUID.isEmpty())
            continue;

        const auto& snapshotIter = m_snapshotMap.find(snapshotUUID);
        if (snapshotIter == m_snapshotMap.end())
            continue;

        // We're only interested in evicting snapshots that still have images.
        if (!snapshotIter->value.hasImage())
            continue;

        mostDistantSnapshotIter = snapshotIter;
        maxDistance = distance;
    }

    if (mostDistantSnapshotIter != m_snapshotMap.end()) {
        removeSnapshotImage(mostDistantSnapshotIter->value);
        return;
    }

    // If we can't find a most distant item (perhaps because all the snapshots are from
    // a different WebPageProxy's back-forward list), we should evict the the oldest item.
    std::chrono::steady_clock::time_point oldestSnapshotTime = std::chrono::steady_clock::time_point::max();
    String oldestSnapshotUUID;

    for (const auto& uuidAndSnapshot : m_snapshotMap) {
        if (uuidAndSnapshot.value.creationTime < oldestSnapshotTime && uuidAndSnapshot.value.hasImage()) {
            oldestSnapshotTime = uuidAndSnapshot.value.creationTime;
            oldestSnapshotUUID = uuidAndSnapshot.key;
        }
    }

    const auto& snapshotIter = m_snapshotMap.find(oldestSnapshotUUID);
    removeSnapshotImage(snapshotIter->value);
}

void ViewSnapshotStore::recordSnapshot(WebPageProxy& webPageProxy)
{
    WebBackForwardListItem* item = webPageProxy.backForwardList().currentItem();

    if (!m_enabled)
        return;

    if (!item)
        return;

    pruneSnapshots(webPageProxy);

    ViewSnapshot snapshot = webPageProxy.takeViewSnapshot();
    if (!snapshot.hasImage())
        return;

    String oldSnapshotUUID = item->snapshotUUID();
    if (!oldSnapshotUUID.isEmpty()) {
        const auto& oldSnapshotIter = m_snapshotMap.find(oldSnapshotUUID);
        if (oldSnapshotIter != m_snapshotMap.end()) {
            removeSnapshotImage(oldSnapshotIter->value);
            m_snapshotMap.remove(oldSnapshotIter);
        }
    }

    snapshot.creationTime = std::chrono::steady_clock::now();
    snapshot.renderTreeSize = webPageProxy.renderTreeSize();
    snapshot.deviceScaleFactor = webPageProxy.deviceScaleFactor();

    item->setSnapshotUUID(createCanonicalUUIDString());

    m_snapshotMap.add(item->snapshotUUID(), snapshot);
    m_snapshotCacheSize += snapshot.imageSizeInBytes;
}

bool ViewSnapshotStore::getSnapshot(WebBackForwardListItem* item, ViewSnapshot& snapshot)
{
    if (item->snapshotUUID().isEmpty())
        return false;

    const auto& snapshotIterator = m_snapshotMap.find(item->snapshotUUID());
    if (snapshotIterator == m_snapshotMap.end())
        return false;
    snapshot = snapshotIterator->value;
    return true;
}

void ViewSnapshotStore::discardSnapshots()
{
    for (auto& snapshot : m_snapshotMap.values())
        removeSnapshotImage(snapshot);
}

bool ViewSnapshot::hasImage() const
{
    return imageSizeInBytes;
}

void ViewSnapshot::clearImage()
{
#if USE_IOSURFACE_VIEW_SNAPSHOTS
    surface = nullptr;
#elif USE_RENDER_SERVER_VIEW_SNAPSHOTS
    if (slotID)
        [ViewSnapshotStore::snapshottingContext() deleteSlot:slotID];
    slotID = 0;
#endif
    imageSizeInBytes = 0;
}

id ViewSnapshot::asLayerContents()
{
#if USE_IOSURFACE_VIEW_SNAPSHOTS
    if (!surface)
        return nullptr;

    // FIXME: This should destroy the surface and inform the ViewSnapshotStore to reduce m_snapshotCacheSize.
    if (surface->setIsVolatile(false) != IOSurface::SurfaceState::Valid)
        return nullptr;

    return (id)surface->surface();
#elif USE_RENDER_SERVER_VIEW_SNAPSHOTS
    return [CAContext objectForSlot:slotID];
#endif
}

} // namespace WebKit
