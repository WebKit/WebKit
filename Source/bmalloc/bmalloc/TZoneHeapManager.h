/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "BExport.h"

#if BUSE(TZONE)

#include "Map.h"
#include "Mutex.h"
#include "SegmentedVector.h"
#include "TZoneHeap.h"
#include <CommonCrypto/CommonDigest.h>
#include <mutex>

#if BUSE(LIBPAS)
#include "bmalloc_heap_ref.h"

namespace bmalloc { namespace api {

#define TZONE_VERBOSE_DEBUG 0

extern BEXPORT class TZoneHeapManager* tzoneHeapManager;
#if BUSE_DYNAMIC_TZONE_COMPACTION
BEXPORT extern bool g_tzoneDynamicCompactModeEnabled;
#endif

class TZoneHeapManager {
    enum class State {
        Uninitialized,
        Seeded,
        StartedRegisteringTypes
    };

    static const unsigned typeNameLen = 12;

    typedef uint64_t SHA256ResultAsUnsigned[CC_SHA256_DIGEST_LENGTH / sizeof(uint64_t)];
    static_assert(!(CC_SHA256_DIGEST_LENGTH % sizeof(uint64_t)));

    struct TZoneBucket {
        bmalloc_type type;
        pas_heap_ref heapref;
        char typeName[typeNameLen];
    };

    struct TZoneTypeBuckets {
        unsigned numberOfBuckets;
#if TZONE_VERBOSE_DEBUG
        unsigned numberOfTypesThisSizeClass;
        unsigned usedBucketBitmap;
        Vector<unsigned> bucketUseCounts;
#endif
        TZoneBucket nonCompactBucket;
        TZoneBucket buckets[1];
    };

// TZoneTypeBuckets already includes room for 1 bucket. Hence, we only need to add count - 1 buckets.
#define SIZE_TZONE_TYPE_BUCKETS(count) (sizeof(struct TZoneTypeBuckets) + (count - 1) * sizeof(TZoneBucket))

    struct TZoneTypeKey {
        TZoneTypeKey() = default;
        TZoneTypeKey(void* address, unsigned size, unsigned alignment)
            : address(address)
            , size(size)
            , alignment(alignment)
        {
            m_key = reinterpret_cast<uintptr_t>(address) << 12 ^ size << 3 ^ alignment >> 3;
        }

        inline unsigned long key() const { return m_key; }

        static unsigned long hash(TZoneTypeKey value)
        {
            return value.m_key;
        }

        bool operator==(const TZoneTypeKey& other) const
        {
            return address == other.address
                && size == other.size
                && alignment == other.alignment;
        }

        bool operator<(const TZoneTypeKey& other) const
        {
            if (address != other.address)
                return address < other.address;

            if (size != other.size)
                return size < other.size;

            return alignment < other.alignment;
        }

        operator bool() const
        {
            return !!key();
        }

        void* address = nullptr;
        unsigned size = 0;
        unsigned alignment = 0;
        uintptr_t m_key = 0;
    };

protected:
    TZoneHeapManager();

public:
    TZoneHeapManager(TZoneHeapManager &other) = delete;
    void operator=(const TZoneHeapManager &) = delete;

    BEXPORT static void requirePerBootSeed();
    BEXPORT static void setBucketParams(unsigned smallSizeCount, unsigned largeSizeCount = 0, unsigned smallSizeLimit = 0);
#if BUSE_DYNAMIC_TZONE_COMPACTION
    BINLINE uint64_t dynamicCompactionSalt() { return m_tzoneDynamicCompactModeSalt; }
    BINLINE static bool tzoneDynamicCompactModeEnabled() { return g_tzoneDynamicCompactModeEnabled; }
#endif

    BEXPORT static bool isReady();

    BEXPORT static void ensureSingleton();
    BINLINE static TZoneHeapManager& singleton()
    {
        BASSERT(tzoneHeapManager);
        return *tzoneHeapManager;
    }

    static void setHasDisableTZoneEntitlementCallback(bool (*hasDisableTZoneEntitlement)());

    pas_heap_ref* heapRefForTZoneType(const TZoneSpecification&);
    pas_heap_ref* heapRefForTZoneTypeDifferentSize(size_t requestedSize, const TZoneSpecification&);

    BEXPORT void dumpRegisteredTypes();

    enum class AllocationMode {
        TZoneEnabled,
        TZoneDisabled,
    };

    static bool s_tzoneEnabled;
private:
    void init();

    BINLINE Mutex& mutex() { return m_mutex; }
    BINLINE Mutex& differentSizeMutex() { return m_differentSizeMutex; }

    BINLINE pas_heap_ref* heapRefForTZoneType(const TZoneSpecification&, LockHolder&);

    inline static unsigned bucketCountForSizeClass(SizeAndAlignment::Value);

    void initializeTZoneDynamicCompactMode();

    inline unsigned tzoneBucketForKey(const TZoneSpecification&, unsigned bucketCountForSize, LockHolder&);
    TZoneTypeBuckets* populateBucketsForSizeClass(LockHolder&, SizeAndAlignment::Value);

    static TZoneHeapManager::State s_state;
    Mutex m_mutex;
    Mutex m_differentSizeMutex;
    uint64_t m_tzoneKeySeed;
#if TZONE_VERBOSE_DEBUG
    unsigned largestBucketCount { 0 };
    Vector<SizeAndAlignment::Value> m_typeSizes;
#endif
#if BUSE_DYNAMIC_TZONE_COMPACTION
    uint64_t m_tzoneDynamicCompactModeSeed;
    uint64_t m_tzoneDynamicCompactModeSalt;
#endif
    Map<SizeAndAlignment::Value, TZoneTypeBuckets*, SizeAndAlignment> m_heapRefsBySizeAndAlignment;
    Map<TZoneTypeKey, pas_heap_ref*, TZoneTypeKey> m_differentSizedHeapRefs;
};

} } // namespace bmalloc::api

#endif // BUSE(LIBPAS)

#endif // BUSE(TZONE)
