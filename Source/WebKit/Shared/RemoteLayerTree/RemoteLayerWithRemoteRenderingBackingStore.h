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

namespace WebKit {
class RemoteLayerWithRemoteRenderingBackingStore;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::RemoteLayerWithRemoteRenderingBackingStore> : std::true_type { };
}

namespace WebKit {

class RemoteDisplayListRecorderProxy;
class RemoteImageBufferSetProxy;

class RemoteLayerWithRemoteRenderingBackingStore : public RemoteLayerBackingStore {
public:
    RemoteLayerWithRemoteRenderingBackingStore(PlatformCALayerRemote&);
    ~RemoteLayerWithRemoteRenderingBackingStore();

    bool isRemoteLayerWithRemoteRenderingBackingStore() const final { return true; }
    ProcessModel processModel() const final { return ProcessModel::Remote; }

    void prepareToDisplay() final;
    void clearBackingStore() final;
    void createContextAndPaintContents() final;

    RefPtr<RemoteImageBufferSetProxy> protectedBufferSet() { return m_bufferSet; }

    std::unique_ptr<ThreadSafeImageBufferSetFlusher> createFlusher(ThreadSafeImageBufferSetFlusher::FlushType) final;
    std::optional<ImageBufferBackendHandle> frontBufferHandle() const final { return std::exchange(const_cast<RemoteLayerWithRemoteRenderingBackingStore*>(this)->m_backendHandle, std::nullopt); }
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<ImageBufferBackendHandle> displayListHandle() const final;
#endif
    void encodeBufferAndBackendInfos(IPC::Encoder&) const final;
    std::optional<RemoteImageBufferSetIdentifier> bufferSetIdentifier() const final;

    void ensureBackingStore(const Parameters&) final;
    bool hasFrontBuffer() const final;
    bool frontBufferMayBeVolatile() const final;

    void setBufferCacheIdentifiers(BufferIdentifierSet&& identifiers)
    {
        m_bufferCacheIdentifiers = WTFMove(identifiers);
    }
    void setBackendHandle(std::optional<ImageBufferBackendHandle>&& backendHandle)
    {
        m_backendHandle = WTFMove(backendHandle);
    }

    void dump(WTF::TextStream&) const final;
private:
    RefPtr<RemoteImageBufferSetProxy> m_bufferSet;
    BufferIdentifierSet m_bufferCacheIdentifiers;
    std::optional<ImageBufferBackendHandle> m_backendHandle;
    bool m_cleared { true };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteLayerWithRemoteRenderingBackingStore)
    static bool isType(const WebKit::RemoteLayerBackingStore& backingStore) { return backingStore.isRemoteLayerWithRemoteRenderingBackingStore(); }
SPECIALIZE_TYPE_TRAITS_END()
