/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#include <WebCore/Font.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>

namespace WebKit {

class RemoteRenderingBackend;

class RemoteResourceCache {
public:
    RemoteResourceCache() = default;

    void cacheImageBuffer(Ref<WebCore::ImageBuffer>&&);
    WebCore::ImageBuffer* cachedImageBuffer(WebCore::RenderingResourceIdentifier);
    void cacheNativeImage(Ref<WebCore::NativeImage>&&);
    void cacheFont(Ref<WebCore::Font>&&);
    void deleteAllFonts();
    bool releaseRemoteResource(WebCore::RenderingResourceIdentifier, uint64_t useCount);
    void recordResourceUse(WebCore::RenderingResourceIdentifier);

    const WebCore::ImageBufferHashMap& imageBuffers() const { return m_imageBuffers; }
    const WebCore::NativeImageHashMap& nativeImages() const { return m_nativeImages; }
    const WebCore::FontRenderingResourceMap& fonts() const { return m_fonts; }

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
    using ResourceUseCountersMap = HashMap<WebCore::RenderingResourceIdentifier, ResourceUseCounter>;

    bool maybeRemoveResource(WebCore::RenderingResourceIdentifier, ResourceUseCountersMap::iterator&);
    void ensureResourceUseCounter(WebCore::RenderingResourceIdentifier);

    WebCore::ImageBufferHashMap m_imageBuffers;
    WebCore::NativeImageHashMap m_nativeImages;
    WebCore::FontRenderingResourceMap m_fonts;

    ResourceUseCountersMap m_resourceUseCounters;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
