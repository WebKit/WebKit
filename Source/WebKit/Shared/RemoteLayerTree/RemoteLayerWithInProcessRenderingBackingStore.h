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

#include "RemoteImageBufferSet.h"
#include "RemoteLayerBackingStore.h"
#include <WebCore/DynamicContentScalingResourceCache.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class RemoteLayerWithInProcessRenderingBackingStore final : public RemoteLayerBackingStore {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerWithInProcessRenderingBackingStore);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerWithInProcessRenderingBackingStore);
public:
    using RemoteLayerBackingStore::RemoteLayerBackingStore;

    bool isRemoteLayerWithInProcessRenderingBackingStore() const final { return true; }
    ProcessModel processModel() const final { return ProcessModel::InProcess; }

    void prepareToDisplay() final;
    void createContextAndPaintContents() final;
    std::unique_ptr<ThreadSafeImageBufferSetFlusher> createFlusher(ThreadSafeImageBufferSetFlusher::FlushType) final;
    std::optional<ImageBufferSetIdentifier> bufferSetIdentifier() const final;

    void clearBackingStore() final;

    bool setBufferVolatile(BufferType, bool forcePurge = false);

    std::optional<ImageBufferBackendHandle> frontBufferHandle() const;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> displayListHandle() const final;
#endif

    void dump(WTF::TextStream&) const final;

private:
    RefPtr<WebCore::ImageBuffer> allocateBuffer();
    void ensureFrontBuffer();
    bool hasFrontBuffer() const final;
    bool frontBufferMayBeVolatile() const final;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::DynamicContentScalingResourceCache ensureDynamicContentScalingResourceCache();
#endif

    struct Buffer {
        RefPtr<WebCore::ImageBuffer> imageBuffer;
        bool isCleared { false };

        explicit operator bool() const
        {
            return !!imageBuffer;
        }

        void discard();
    };

    // Returns true if it was able to fulfill the request. This can fail when trying to mark an in-use surface as volatile.
    bool setBufferVolatile(RefPtr<WebCore::ImageBuffer>&, bool forcePurge = false);
    WebCore::SetNonVolatileResult setBufferNonVolatile(Buffer&);

    ImageBufferSet m_bufferSet;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::DynamicContentScalingResourceCache m_dynamicContentScalingResourceCache;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteLayerWithInProcessRenderingBackingStore)
    static bool isType(const WebKit::RemoteLayerBackingStore& backingStore) { return backingStore.isRemoteLayerWithInProcessRenderingBackingStore(); }
SPECIALIZE_TYPE_TRAITS_END()
