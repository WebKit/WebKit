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
#import <WebCore/UUID.h>

using namespace WebCore;

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

void ViewSnapshotStore::recordSnapshot(WebPageProxy& webPageProxy)
{
    RefPtr<WebBackForwardListItem> item = webPageProxy.backForwardList().currentItem();

    if (!m_enabled)
        return;

    if (!item)
        return;

    RetainPtr<CGImageRef> snapshot = webPageProxy.takeViewSnapshot();

    String oldSnapshotUUID = item->snapshotUUID();
    if (!oldSnapshotUUID.isEmpty())
        m_snapshotMap.remove(oldSnapshotUUID);

    item->setSnapshotUUID(createCanonicalUUIDString());
    m_snapshotMap.add(item->snapshotUUID(), std::make_pair(snapshot, webPageProxy.renderTreeSize()));
}

std::pair<RetainPtr<CGImageRef>, uint64_t> ViewSnapshotStore::snapshotAndRenderTreeSize(WebBackForwardListItem* item)
{
    const auto& snapshotAndRenderTreeSize = m_snapshotMap.find(item->snapshotUUID());

    if (snapshotAndRenderTreeSize == m_snapshotMap.end())
        return std::make_pair(nullptr, 0);

    return std::make_pair(snapshotAndRenderTreeSize->value.first.get(), snapshotAndRenderTreeSize->value.second);
}

} // namespace WebKit
