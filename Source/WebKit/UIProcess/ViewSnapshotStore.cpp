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

#include "config.h"
#include "ViewSnapshotStore.h"

#include "WebBackForwardList.h"
#include "WebPageProxy.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS_FAMILY)
static const size_t maximumSnapshotCacheSize = 50 * (1024 * 1024);
#else
static const size_t maximumSnapshotCacheSize = 400 * (1024 * 1024);
#endif

namespace WebKit {
using namespace WebCore;

ViewSnapshotStore::ViewSnapshotStore()
{
}

ViewSnapshotStore::~ViewSnapshotStore()
{
    discardSnapshotImages();
}

ViewSnapshotStore& ViewSnapshotStore::singleton()
{
    static NeverDestroyed<ViewSnapshotStore> store;
    return store;
}

void ViewSnapshotStore::didAddImageToSnapshot(ViewSnapshot& snapshot)
{
    bool isNewEntry = m_snapshotsWithImages.add(&snapshot).isNewEntry;
    ASSERT_UNUSED(isNewEntry, isNewEntry);
    m_snapshotCacheSize += snapshot.imageSizeInBytes();
}

void ViewSnapshotStore::willRemoveImageFromSnapshot(ViewSnapshot& snapshot)
{
    bool removed = m_snapshotsWithImages.remove(&snapshot);
    ASSERT_UNUSED(removed, removed);
    m_snapshotCacheSize -= snapshot.imageSizeInBytes();
}

void ViewSnapshotStore::pruneSnapshots(WebPageProxy& webPageProxy)
{
    if (m_snapshotCacheSize <= maximumSnapshotCacheSize)
        return;

    ASSERT(!m_snapshotsWithImages.isEmpty());

    // FIXME: We have enough information to do smarter-than-LRU eviction (making use of the back-forward lists, etc.)

    m_snapshotsWithImages.first()->clearImage();
}

void ViewSnapshotStore::recordSnapshot(WebPageProxy& webPageProxy, WebBackForwardListItem& item)
{
    if (webPageProxy.isShowingNavigationGestureSnapshot())
        return;

    pruneSnapshots(webPageProxy);

    webPageProxy.willRecordNavigationSnapshot(item);

    auto snapshot = webPageProxy.takeViewSnapshot();
    if (!snapshot)
        return;

    snapshot->setRenderTreeSize(webPageProxy.renderTreeSize());
    snapshot->setDeviceScaleFactor(webPageProxy.deviceScaleFactor());
    snapshot->setBackgroundColor(webPageProxy.pageExtendedBackgroundColor());
    snapshot->setViewScrollPosition(WebCore::roundedIntPoint(webPageProxy.viewScrollPosition()));

    item.setSnapshot(WTFMove(snapshot));
}

void ViewSnapshotStore::discardSnapshotImages()
{
    while (!m_snapshotsWithImages.isEmpty())
        m_snapshotsWithImages.first()->clearImage();
}

ViewSnapshot::~ViewSnapshot()
{
    clearImage();
}

} // namespace WebKit
