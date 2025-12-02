/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(PARKABLE_STRINGS)

#include "ParkableString.h"

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Manages all the ParkableStrings, and parks eligible strings after the
// renderer has been backgrounded.
// Main Thread only.
// When a `ParkableString` is unparked on a background thread, a task is posted
// to the main thread to update the entries in the manager. Hence, it is
// possible to temporarily have an unparked `ParkableString` inaccessible
// through `unparked_strings_`. This can cause aging of the string to be
// delayed or a variation on the sizes recorded in 'ComputeStatistics()`.
class WEBCORE_EXPORT ParkableStringManager {
public:
    // Aging system constants
    static const Seconds kFirstParkingDelay;
    static const Seconds kAgingInterval;
    static const Seconds kLessAggressiveAgingInterval;

    // Hash traits for secure digest-based deduplication
    // Compares digest contents, not pointers, and uses first 4 bytes as hash
    struct SecureDigestHashTraits : WTF::GenericHashTraits<const ParkableStringImpl::SecureDigest*> {
        static unsigned hash(const ParkableStringImpl::SecureDigest* digest)
        {
            // The first bytes of the hash are as good as anything else.
            return *reinterpret_cast<const unsigned*>(digest->begin());
        }

        static bool equal(const ParkableStringImpl::SecureDigest* const a, const ParkableStringImpl::SecureDigest* const b)
        {
            return a == b || *a == *b;
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = false;
    };

    // Hash-based string map for deduplication
    using StringMap = WTF::HashMap<const ParkableStringImpl::SecureDigest*, WTF::RefPtr<ParkableStringImpl>, SecureDigestHashTraits>;

    static ParkableStringManager& instance();

    ~ParkableStringManager();

    WTF::RefPtr<ParkableStringImpl> add(WTF::RefPtr<WTF::StringImpl>);
    WTF::RefPtr<ParkableStringImpl> add(WTF::RefPtr<WTF::StringImpl>, std::unique_ptr<ParkableStringImpl::SecureDigest>);
    void remove(ParkableStringImpl*);

    static bool shouldPark(const WTF::StringImpl&);

    // Centralized work queue for background operations
    ConcurrentWorkQueue& workQueue();

    // State transition callbacks (called from ParkableStringImpl)
    void completeParked(ParkableStringImpl*);
    void completeUnpark(ParkableStringImpl*, Seconds elapsed = 0_s);

    // Main thread callbacks
    void onParked(ParkableStringImpl*);
    void onUnparked(ParkableStringImpl*);

    // Statistics and testing
    void recordParkingThreadTime(Seconds time);
    void recordUnparkingTime(Seconds time);
    void recordCompression();
    void recordDecompression();

    size_t size() const;
    bool isOnParkedMapForTesting(ParkableStringImpl*);
    void resetForTesting();

    // Test control methods
    void setTestMode(bool enabled);
    void fastForwardAgingForTesting();

    // Background aging system
    void scheduleAgingTaskIfNeeded();
    void ageStringsAndPark();
    void parkAll(ParkableStringImpl::ParkingMode);

    // Memory pressure handling
    void purgeMemory();
    void onMemoryPressure();
    void setRendererBackgrounded(bool);

    // Memory reporting and dump provider functionality
    struct MemoryStatistics {
        size_t totalStrings { 0 };
        size_t unparkedStrings { 0 };
        size_t parkedStrings { 0 };
        size_t totalUncompressedSize { 0 };
        size_t totalCompressedSize { 0 };
        size_t metadataOverhead { 0 };
        double averageCompressionRatio { 0.0 };
        size_t compressionSavings { 0 };

        // Performance metrics
        size_t totalCompressions { 0 };
        size_t totalDecompressions { 0 };
        Seconds totalCompressionTime { 0_s };
        Seconds totalDecompressionTime { 0_s };
    };

    MemoryStatistics getMemoryStatistics() const;
    size_t memoryFootprint() const;

private:
    ParkableStringManager();

    // Friend class to allow NeverDestroyed access to constructor
    friend class WTF::NeverDestroyed<ParkableStringManager>;

    // Move string between different state maps using digest-based lookup
    bool moveString(ParkableStringImpl*, StringMap* from, StringMap* to);

    void removeOnMainThread(ParkableStringImpl*);

    // Background aging helpers
    WTF::Vector<ParkableStringImpl*> enumerateStrings(const StringMap& strings);
    bool hasPendingWork() const;
    bool isPaused() const;
    Seconds agingInterval();

    mutable WTF::Lock m_lock;

    WTF::RefPtr<ConcurrentWorkQueue> m_workQueue;

    StringMap m_unparkedStrings;
    StringMap m_parkedStrings;

    // Statistics tracking
    Seconds m_totalParkingThreadTime;
    Seconds m_totalUnparkingTime;

    // Performance counters
    mutable size_t m_totalCompressions { 0 };
    mutable size_t m_totalDecompressions { 0 };

    // Background aging system state
    bool m_hasPendingAgingTask { false };
    bool m_firstStringAgingWasDelayed { false };
    bool m_rendererBackgrounded { false };

    // Test control state
    bool m_testModeEnabled { false };

    static ParkableStringManager* s_instance;
};

} // namespace WebCore

#endif // ENABLE(PARKABLE_STRINGS)
