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

#include "RemoteLayerBackingStore.h"
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class RemoteGraphicsContextProxy;
class RemoteImageBufferSetProxy;

class RemoteLayerWithRemoteRenderingBackingStore final : public RemoteLayerBackingStore, public ImageBufferSetClient {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerWithRemoteRenderingBackingStore);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerWithRemoteRenderingBackingStore);
public:
    RemoteLayerWithRemoteRenderingBackingStore(PlatformCALayerRemote&);
    ~RemoteLayerWithRemoteRenderingBackingStore();

    bool isRemoteLayerWithRemoteRenderingBackingStore() const final { return true; }
    ProcessModel processModel() const final { return ProcessModel::Remote; }

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    void prepareToDisplay() final;
    void clearBackingStore() final;
    void createContextAndPaintContents() final;

    void setNeedsDisplay() final;

    RemoteImageBufferSetProxy* bufferSet() { return m_bufferSet.get(); }

    std::unique_ptr<ThreadSafeImageBufferSetFlusher> createFlusher(ThreadSafeImageBufferSetFlusher::FlushType) final;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> displayListHandle() const final;
#endif
    std::optional<ImageBufferSetIdentifier> bufferSetIdentifier() const final;

    void ensureBackingStore(const Parameters&) final;
    bool hasFrontBuffer() const final;
    bool frontBufferMayBeVolatile() const final;

    void dump(WTF::TextStream&) const final;
private:
    const RefPtr<RemoteImageBufferSetProxy> m_bufferSet;
    bool m_cleared { true };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteLayerWithRemoteRenderingBackingStore)
    static bool isType(const WebKit::RemoteLayerBackingStore& backingStore) { return backingStore.isRemoteLayerWithRemoteRenderingBackingStore(); }
SPECIALIZE_TYPE_TRAITS_END()
