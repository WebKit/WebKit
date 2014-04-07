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

#ifndef RemoteLayerBackingStoreCollection_h
#define RemoteLayerBackingStoreCollection_h

#import <WebCore/Timer.h>
#import <wtf/HashSet.h>
#import <wtf/Noncopyable.h>

namespace WebKit {

class RemoteLayerBackingStore;
class RemoteLayerTreeContext;

class RemoteLayerBackingStoreCollection {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStoreCollection);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<RemoteLayerBackingStoreCollection> create(RemoteLayerTreeContext*);
    RemoteLayerBackingStoreCollection(RemoteLayerTreeContext*);

    void backingStoreWasCreated(RemoteLayerBackingStore*);
    void backingStoreWillBeDestroyed(RemoteLayerBackingStore*);

    void purgeabilityTimerFired(WebCore::Timer<RemoteLayerBackingStoreCollection>&);

    void schedulePurgeabilityTimer();

private:
    HashSet<RemoteLayerBackingStore*> m_liveBackingStore;

    void markInactiveBackingStorePurgeable();

    RemoteLayerTreeContext* m_context;
    WebCore::Timer<RemoteLayerBackingStoreCollection> m_purgeabilityTimer;
};

} // namespace WebKit

#endif // RemoteLayerBackingStoreCollection_h
