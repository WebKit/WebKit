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

#pragma once

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include <WebCore/AXCoreObject.h>
#include <WebCore/IntRectHash.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class AXObjectCache;

struct HitTestCacheEntry {
    IntPoint hitPoint;
    AXID resultID;
    MonotonicTime expirationTime;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXGeometryManager);
class AXGeometryManager final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AXGeometryManager> {
    WTF_MAKE_NONCOPYABLE(AXGeometryManager);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXGeometryManager, AXGeometryManager);
public:
    explicit AXGeometryManager(AXObjectCache&);
    AXGeometryManager();
    static Ref<AXGeometryManager> create(AXObjectCache& cache)
    {
        return adoptRef(*new AXGeometryManager(cache));
    }
    ~AXGeometryManager();

    void willUpdateObjectRegions();
    void scheduleObjectRegionsUpdate(bool /* scheduleImmediately */);

    // Returns true if the given rect was cached.
    bool cacheRectIfNeeded(AXID, IntRect&&);
    // std::nullopt if there is no cached rect for the given ID (i.e. because it hasn't been cached yet via paint or otherwise, or cannot be painted / cached at all).
    std::optional<IntRect> cachedRectForID(AXID);

    void remove(AXID axID) { m_cachedRects.remove(axID); }

    std::optional<AXID> cachedHitTestResult(const IntPoint& screenPoint);
    void cacheHitTestResult(AXID resultID, const IntPoint& hitPoint);
    void expandHitTestCacheAroundPoint(const IntPoint& center, AXTreeID);
    void invalidateHitTestCacheForID(AXID);
    void clearHitTestCache();

#if PLATFORM(MAC)
    void initializePrimaryScreenRect();
    FloatRect primaryScreenRect();
#endif

private:
    void updateObjectRegionsTimerFired() { scheduleRenderingUpdate(); }
    void scheduleRenderingUpdate();

    // The cache that owns this instance.
    const WeakPtr<AXObjectCache> m_cache;
    HashMap<AXID, IntRect> m_cachedRects;
    Timer m_updateObjectRegionsTimer;

    Lock m_hitTestCacheLock;
    static constexpr size_t HitTestCacheSize = 32;
    Vector<HitTestCacheEntry, HitTestCacheSize> m_hitTestCache WTF_GUARDED_BY_LOCK(m_hitTestCacheLock);

    std::atomic<uint64_t> m_probeGeneration { 0 };
    void incrementProbeGeneration() { m_probeGeneration++; }
    uint64_t currentProbeGeneration() const { return m_probeGeneration.load(); }

#if PLATFORM(MAC)
    FloatRect m_primaryScreenRect WTF_GUARDED_BY_LOCK(m_primaryScreenRectLock);
    Lock m_primaryScreenRectLock;
#endif
};

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
