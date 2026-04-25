/*
 * Copyright (C) 2024, 2026 Apple Inc. All rights reserved.
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

#ifdef __cplusplus

#include "BExport.h"
#include "BPlatform.h"

#if BUSE(TZONE)

#include "Map.h"
#include "Mutex.h"
#include "TZoneHeap.h"
#include <CommonCrypto/CommonDigest.h>
#include <mutex>

#if BUSE(LIBPAS)
#include "bmalloc_heap_ref.h"

namespace bmalloc { namespace api {

#define TZONE_VERBOSE_DEBUG 0

extern BEXPORT class TZoneHeapManager* tzoneHeapManager;

// The TZone heaps are organized as follows (from top to bottom):
//
//                        Category
//               ┌────────────┼────────────┐
//               ▼                         ▼
//             Group                      Group
//       ┌───────┼───────┐         ┌───────┼───────┐
//       ▼       ▼       ▼         ▼       ▼       ▼
//    Bucket  Bucket  Bucket    Bucket  Bucket  Bucket
//
// A TZone (TZONE_ALLOCATED) type can only belong in one and only one category. At
// present, there is only 1 category: sorting by SizeAndAlignment. So, all TZone types
// will belong in the SizeAndAlignment catgeory by default. Eventually, we'll add other
// categories that types / type hierarchies can opt themselves into.
//
// 1. Category: a set of Groups. Each category sorts its TZone types into Groups based on
//    the sorting algorithm for that category. The sorting order is deterministic and is
//    based on properties of the type determined at compile time. For example, the
//    SizeAndAlignment category sorts types based on their size and alignment.
//
// 2. Group: a group of Buckets. Each group sorts its TZone types into Buckets by some
//    randomness criteria. Hence, for any different launch of the process, a TZone type
//    can randomly end up in a different Bucket based on the randomness criteria.
//    See TZoneHeapManager::Group.
//
// 3. Bucket: corresponds to a single isoheap, from which a TZone type's allocations are
//    made. See TZoneHeapManager::Bucket.
//
// Each TZone type has a TZoneSpecification with a TZoneDescriptor. The descriptor is
// used to determine which category and group the instances of TZone type will be sorted
// into, and hence, will be allocated allocated out of a bucket in that group.

class TZoneHeapManager {
    enum class State {
        Uninitialized,
        Seeded,
        StartedRegisteringTypes
    };

    static const unsigned typeNameLen = 12;

    typedef uint64_t SHA256ResultAsUnsigned[CC_SHA256_DIGEST_LENGTH / sizeof(uint64_t)];
    static_assert(!(CC_SHA256_DIGEST_LENGTH % sizeof(uint64_t)));

    struct Bucket {
        bmalloc_type type;
        pas_heap_ref heapref;
        char typeName[typeNameLen];
    };

    struct Group {
        unsigned numberOfBuckets;
#if TZONE_VERBOSE_DEBUG
        unsigned numberOfTypesInGroup;
        unsigned usedBucketBitmap;
        Vector<unsigned> bucketUseCounts;
#endif
        Bucket nonCompactBucket;
        Bucket buckets[1];
    };

// Group already includes room for 1 bucket. Hence, we only need to add count - 1 buckets.
#define TZONE_GROUP_SIZE(count) (sizeof(struct Group) + (count - 1) * sizeof(Bucket))

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

    inline static unsigned bucketCountForSizeClass(unsigned sizeClass);

    inline unsigned bucketForKey(const TZoneSpecification&, unsigned bucketCountForSize, LockHolder&);
    Group* populateGroupBuckets(LockHolder&, const TZoneSpecification&);

    static TZoneHeapManager::State s_state;
    Mutex m_mutex;
    Mutex m_differentSizeMutex;
    uint64_t m_tzoneKeySeed;
#if TZONE_VERBOSE_DEBUG
    unsigned largestBucketCount { 0 };
    Vector<TZoneDescriptor> m_registeredDescriptors;
#endif
    Map<TZoneDescriptor, Group*, TZoneDescriptorHashTrait> m_groupByDescriptor;
    Map<TZoneTypeKey, pas_heap_ref*, TZoneTypeKey> m_differentSizedHeapRefs;
};

} } // namespace bmalloc::api

#endif // BUSE(LIBPAS)

#endif // BUSE(TZONE)

#endif // __cplusplus
