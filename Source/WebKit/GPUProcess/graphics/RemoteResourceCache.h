/*
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "QualifiedRenderingResourceIdentifier.h"
#include "QualifiedResourceHeap.h"
#include <WebCore/Font.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>

namespace WebKit {

class RemoteRenderingBackend;

class RemoteResourceCache {
public:
    RemoteResourceCache(WebCore::ProcessIdentifier webProcessIdentifier);
    RemoteResourceCache(RemoteResourceCache&&);

    void cacheImageBuffer(Ref<WebCore::ImageBuffer>&&, QualifiedRenderingResourceIdentifier);
    WebCore::ImageBuffer* cachedImageBuffer(QualifiedRenderingResourceIdentifier) const;
    void cacheNativeImage(Ref<WebCore::NativeImage>&&, QualifiedRenderingResourceIdentifier);
    WebCore::NativeImage* cachedNativeImage(QualifiedRenderingResourceIdentifier) const;
    void cacheFont(Ref<WebCore::Font>&&, QualifiedRenderingResourceIdentifier);
    WebCore::Font* cachedFont(QualifiedRenderingResourceIdentifier) const;
    void deleteAllFonts();
    bool releaseRemoteResource(QualifiedRenderingResourceIdentifier, uint64_t useCount);
    void recordResourceUse(QualifiedRenderingResourceIdentifier);

    const WebCore::DisplayList::ResourceHeap& resourceHeap() const { return m_resourceHeap; }

    bool hasActiveDrawables() const { return m_hasActiveDrawables; }

private:
    // Because the cache/release messages are sent asynchronously from the display list items which
    // reference the resources, it's totally possible that we see a release message before we've
    // executed all the display list items which reference the resource. The web process tells us
    // how many display list items will reference this resource, and we defer deletion of the resource
    // until we execute that many display list items. It's actually a bit worse than this, though,
    // because we may actually see a *new* cache message during the time when deletion is deferred.
    //
    // We can only safely delete a resource when:
    // 1. All the cache messages have an accompanying release message, and
    // 2. We've processed as many display list items that reference a particular resource as the web
    // process has encoded.
    enum class ResourceState {
        Alive,
        ToBeDeleted
    };
    struct ResourceUseCounter {
        ResourceState state { ResourceState::Alive };
        int64_t useOrPendingCount { 0 };
    };
    using ResourceUseCountersMap = HashMap<QualifiedRenderingResourceIdentifier, ResourceUseCounter>;

    bool maybeRemoveResource(QualifiedRenderingResourceIdentifier, ResourceUseCountersMap::iterator&);
    void ensureResourceUseCounter(QualifiedRenderingResourceIdentifier);

    void updateHasActiveDrawables();

    QualifiedResourceHeap m_resourceHeap;

    ResourceUseCountersMap m_resourceUseCounters;

    std::atomic<bool> m_hasActiveDrawables { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
