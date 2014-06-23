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
class RemoteLayerTreeTransaction;

class RemoteLayerBackingStoreCollection {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStoreCollection);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerBackingStoreCollection();

    void backingStoreWasCreated(RemoteLayerBackingStore&);
    void backingStoreWillBeDestroyed(RemoteLayerBackingStore&);

    void backingStoreWillBeDisplayed(RemoteLayerBackingStore&);
    void backingStoreBecameUnreachable(RemoteLayerBackingStore&);

    void willFlushLayers();
    void willCommitLayerTree(RemoteLayerTreeTransaction&);
    void didFlushLayers();

    void volatilityTimerFired(WebCore::Timer<RemoteLayerBackingStoreCollection>&);
    bool markAllBackingStoreVolatileImmediatelyIfPossible();

    void scheduleVolatilityTimer();

private:
    enum VolatilityMarkingFlag {
        MarkBuffersIgnoringReachability = 1 << 0
    };
    typedef unsigned VolatilityMarkingFlags;
    bool markBackingStoreVolatileImmediately(RemoteLayerBackingStore&, VolatilityMarkingFlags volatilityMarkingFlags = 0);
    bool markBackingStoreVolatile(RemoteLayerBackingStore&, std::chrono::steady_clock::time_point now);

    HashSet<RemoteLayerBackingStore*> m_liveBackingStore;
    HashSet<RemoteLayerBackingStore*> m_unparentedBackingStore;
    HashSet<RemoteLayerBackingStore*> m_reachableBackingStoreInLatestFlush;

    WebCore::Timer<RemoteLayerBackingStoreCollection> m_volatilityTimer;

    bool m_inLayerFlush;
};

} // namespace WebKit

#endif // RemoteLayerBackingStoreCollection_h
