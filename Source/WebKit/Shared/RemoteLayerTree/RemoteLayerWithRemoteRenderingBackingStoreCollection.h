/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "RemoteLayerBackingStoreCollection.h"
#import <WebCore/RenderingResourceIdentifier.h>

namespace WebKit {

class RemoteRenderingBackendProxy;

// "RemoteLayer" refers to UI-side compositing.
// "RemoteRendering" refers to GPU-process rendering.
class RemoteLayerWithRemoteRenderingBackingStoreCollection : public RemoteLayerBackingStoreCollection {
public:
    RemoteLayerWithRemoteRenderingBackingStoreCollection(RemoteLayerTreeContext&);
    virtual ~RemoteLayerWithRemoteRenderingBackingStoreCollection() = default;

private:
    RefPtr<WebCore::ImageBuffer> allocateBufferForBackingStore(const RemoteLayerBackingStore&) final;
    
    RemoteRenderingBackendProxy& remoteRenderingBackendProxy();

    bool backingStoreNeedsDisplay(const RemoteLayerBackingStore&) final;
    void prepareBackingStoresForDisplay(RemoteLayerTreeTransaction&) final;

    bool collectBackingStoreBufferIdentifiersToMarkVolatile(RemoteLayerBackingStore&, OptionSet<VolatilityMarkingBehavior>, MonotonicTime now, Vector<WebCore::RenderingResourceIdentifier>&);

    bool collectAllBufferIdentifiersToMarkVolatile(OptionSet<VolatilityMarkingBehavior> liveBackingStoreMarkingBehavior, OptionSet<VolatilityMarkingBehavior> unparentedBackingStoreMarkingBehavior, Vector<WebCore::RenderingResourceIdentifier>&);

    void markBackingStoreVolatileAfterReachabilityChange(RemoteLayerBackingStore&) final;
    void tryMarkAllBackingStoreVolatile(CompletionHandler<void(bool)>&&) final;
    void markAllBackingStoreVolatileFromTimer() final;
    
    void sendMarkBuffersVolatile(Vector<WebCore::RenderingResourceIdentifier>&&, CompletionHandler<void(bool)>&&);
};

} // namespace WebKit
