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
#import <IOSurface/IOSurface.h>
#import <WebCore/UUID.h>

#if defined(__has_include) && __has_include(<CoreGraphics/CoreGraphicsPrivate.h>)
#import <CoreGraphics/CoreGraphicsPrivate.h>
#endif

extern "C" CGContextRef CGIOSurfaceContextCreate(IOSurfaceRef surface, size_t width, size_t height, size_t bitsPerComponent, size_t bitsPerPixel, CGColorSpaceRef space, CGBitmapInfo bitmapInfo);

#if defined(__has_include) && __has_include(<IOSurface/IOSurfacePrivate.h>)
#import <IOSurface/IOSurfacePrivate.h>
#else
enum {
    kIOSurfacePurgeableVolatile = 1,
};
#endif

extern "C" IOReturn IOSurfaceSetPurgeable(IOSurfaceRef buffer, uint32_t newState, uint32_t *oldState);

using namespace WebCore;

static const int maximumSnapshotCount = 20;

namespace WebKit {

ViewSnapshotStore::ViewSnapshotStore()
    : m_enabled(true)
{
}

ViewSnapshotStore::~ViewSnapshotStore()
{
}

ViewSnapshotStore& ViewSnapshotStore::shared()
{
    static ViewSnapshotStore& store = *new ViewSnapshotStore;
    return store;
}

void ViewSnapshotStore::pruneSnapshots(WebPageProxy& webPageProxy)
{
    if (m_snapshotMap.size() <= maximumSnapshotCount)
        return;

    uint32_t currentIndex = webPageProxy.backForwardList().currentIndex();
    uint32_t maxDistance = 0;
    WebBackForwardListItem* mostDistantSnapshottedItem = nullptr;
    auto backForwardEntries = webPageProxy.backForwardList().entries();

    // First, try to evict the snapshot for the page farthest from the current back-forward item.
    for (uint32_t i = 0, entryCount = webPageProxy.backForwardList().entries().size(); i < entryCount; i++) {
        uint32_t distance = std::max(currentIndex, i) - std::min(currentIndex, i);

        if (i == currentIndex || distance < maxDistance)
            continue;

        WebBackForwardListItem* item = backForwardEntries[i].get();
        String snapshotUUID = item->snapshotUUID();
        if (!snapshotUUID.isEmpty() && m_snapshotMap.contains(snapshotUUID)) {
            mostDistantSnapshottedItem = item;
            maxDistance = distance;
        }
    }

    if (mostDistantSnapshottedItem) {
        m_snapshotMap.remove(mostDistantSnapshottedItem->snapshotUUID());
        return;
    }

    // If we can't find a most distant item (perhaps because all the snapshots are from
    // a different WebPageProxy's back-forward list), we should evict the the oldest item.
    std::chrono::steady_clock::time_point oldestSnapshotTime = std::chrono::steady_clock::time_point::max();
    String oldestSnapshotUUID;

    for (const auto& uuidAndSnapshot : m_snapshotMap) {
        if (uuidAndSnapshot.value.creationTime < oldestSnapshotTime) {
            oldestSnapshotTime = uuidAndSnapshot.value.creationTime;
            oldestSnapshotUUID = uuidAndSnapshot.key;
        }
    }

    m_snapshotMap.remove(oldestSnapshotUUID);
}

static std::pair<RetainPtr<IOSurfaceRef>, RetainPtr<CGContextRef>> createIOSurfaceFromImage(CGImageRef image)
{
    unsigned pixelFormat = 'BGRA';
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 32;
    size_t bytesPerElement = 4;
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerElement);
    ASSERT(bytesPerRow);

    size_t allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    ASSERT(allocSize);

    NSDictionary *properties = @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
        (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
        (id)kIOSurfaceAllocSize: @(allocSize)
    };

    RetainPtr<IOSurfaceRef> surface = adoptCF(IOSurfaceCreate((CFDictionaryRef)properties));
    CGBitmapInfo bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
    RetainPtr<CGContextRef> surfaceContext = adoptCF(CGIOSurfaceContextCreate(surface.get(), width, height, bitsPerComponent, bitsPerPixel, CGImageGetColorSpace(image), bitmapInfo));
    CGContextDrawImage(surfaceContext.get(), CGRectMake(0, 0, width, height), image);
    CGContextFlush(surfaceContext.get());

    return std::make_pair(surface, surfaceContext);
}

void ViewSnapshotStore::recordSnapshot(WebPageProxy& webPageProxy)
{
    WebBackForwardListItem* item = webPageProxy.backForwardList().currentItem();

    if (!m_enabled)
        return;

    if (!item)
        return;

    RetainPtr<CGImageRef> snapshotImage = webPageProxy.takeViewSnapshot();
    if (!snapshotImage)
        return;

    pruneSnapshots(webPageProxy);

    String oldSnapshotUUID = item->snapshotUUID();
    if (!oldSnapshotUUID.isEmpty()) {
        m_snapshotMap.remove(oldSnapshotUUID);
        m_renderTreeSizeMap.remove(oldSnapshotUUID);
    }

    item->setSnapshotUUID(createCanonicalUUIDString());

    auto surfaceAndContext = createIOSurfaceFromImage(snapshotImage.get());

    Snapshot snapshot;
    snapshot.surface = surfaceAndContext.first;
    snapshot.surfaceContext = surfaceAndContext.second;
    snapshot.creationTime = std::chrono::steady_clock::now();

    IOSurfaceSetPurgeable(snapshot.surface.get(), kIOSurfacePurgeableVolatile, nullptr);

    m_snapshotMap.add(item->snapshotUUID(), snapshot);
    m_renderTreeSizeMap.add(item->snapshotUUID(), webPageProxy.renderTreeSize());
}

std::pair<RetainPtr<IOSurfaceRef>, uint64_t> ViewSnapshotStore::snapshotAndRenderTreeSize(WebBackForwardListItem* item)
{
    if (item->snapshotUUID().isEmpty())
        return std::make_pair(nullptr, 0);

    const auto& renderTreeSize = m_renderTreeSizeMap.find(item->snapshotUUID());
    if (renderTreeSize == m_renderTreeSizeMap.end())
        return std::make_pair(nullptr, 0);

    const auto& snapshot = m_snapshotMap.find(item->snapshotUUID());
    RetainPtr<IOSurfaceRef> surface;

    if (snapshot != m_snapshotMap.end())
        surface = snapshot->value.surface;

    return std::make_pair(surface, renderTreeSize->value);
}

} // namespace WebKit
