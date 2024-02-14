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
#include "BPlatform.h"

#if BUSE(TZONE)

#include "IsoConfig.h"
#include "Map.h"
#include "Mutex.h"
#include <CommonCrypto/CommonDigest.h>
#include <mutex>

#if BUSE(LIBPAS)
#include "bmalloc_heap_ref.h"

namespace bmalloc { namespace api {

class TZoneHeapManager {
    enum State {
        Uninitialized,
        Seeded,
        RegisteringTypes,
        TypesRegistered
    };

    static const unsigned typeNameLen = 12;

    typedef uint64_t SHA256ResultAsUnsigned[CC_SHA256_DIGEST_LENGTH / sizeof(uint64_t)];
    static_assert(!(CC_SHA256_DIGEST_LENGTH % sizeof(uint64_t)));

    struct TZoneHeapRandomizeKey {
        unsigned char seed[CC_SHA1_DIGEST_LENGTH];
        bmalloc_type* classType;
        unsigned sizeOfType;
        unsigned alignmentOfType;
    };

    struct TZoneBucket {
        bmalloc_type type;
        pas_heap_ref heapref;
        char typeName[typeNameLen];
    };

    struct TZoneTypeBuckets {
        unsigned numberOfBuckets;
        unsigned unusedSpaceFiller;
        TZoneBucket buckets[0];
    };

#define SIZE_TZONE_TYPE_BUCKETS(count) (sizeof(struct TZoneTypeBuckets) + count * sizeof(TZoneBucket))

    struct SizeAndAlign {
        SizeAndAlign()
        {
            m_value.key = 0;
        }

        SizeAndAlign(unsigned size, unsigned alignment)
        {
            m_value.u.size = size;
            m_value.u.alignment = alignment;
        }

        SizeAndAlign(const bmalloc_type* type)
        {
            m_value.u.size = type->size;
            m_value.u.alignment = type->alignment;
        }

        inline unsigned size() const { return m_value.u.size; }
        inline unsigned alignment() const { return m_value.u.alignment; }
        inline unsigned long key() const { return m_value.key; }

        static unsigned long hash(SizeAndAlign value)
        {
            return value.key() >> 4;
        }

        bool operator==(const SizeAndAlign& other) const
        {
            return key() == other.key();
        }

        operator bool() const
        {
            return !!key();
        }

        union {
            struct {
                unsigned alignment;
                unsigned size;
            } u;
            unsigned long key;
        } m_value;
    };

protected:
    TZoneHeapManager()
        : m_state(TZoneHeapManager::Uninitialized)
    {
        initTypenameTemplate();
    }

public:
    TZoneHeapManager(TZoneHeapManager &other) = delete;
    void operator=(const TZoneHeapManager &) = delete;

    BEXPORT void init(const char* seed);

    BINLINE static TZoneHeapManager& getInstance()
    {
        if (!theTZoneHeapManager)
            ensureInstance();
        BASSERT(theTZoneHeapManager);
        return *theTZoneHeapManager;
    }

    BEXPORT void registerTZoneTypes(const bmalloc_type* start, const bmalloc_type* end);
    BEXPORT void closeRegistration();
    BEXPORT pas_heap_ref* heapRefForTZoneType(bmalloc_type* classType);

private:
    BEXPORT static void ensureInstance();

    void initTypenameTemplate();

    BINLINE Mutex& mutex() { return m_mutex; }

    BINLINE unsigned bucketCountForTypeCount(unsigned typeCount)
    {
        BASSERT(typeCount);

        // Returns number of buckets for the typeCount.
        // The goal here is to have a sufficient number of buckets to to provide enough randomness without an adverse impact on memory use.
        if (typeCount == 1)
            return 1;
        if (typeCount <= 4)
            return 2;
        if (typeCount <= 9)
            return 3;
        if (typeCount <= 19)
            return 4;

        return 5;
    }

    BINLINE unsigned tzoneBucketForKey(UniqueLockHolder&, bmalloc_type* type, unsigned bucketCountForSize)
    {
        SHA256ResultAsUnsigned sha256Result;

        m_tzoneKey.classType = type;
        m_tzoneKey.sizeOfType = type->size;
        m_tzoneKey.alignmentOfType = type->alignment;

        (void)CC_SHA256(&m_tzoneKey, sizeof(TZoneHeapRandomizeKey), (unsigned char*)&sha256Result);

        return sha256Result[3] % bucketCountForSize;
    }

    TZoneTypeBuckets* populateBucketsForSizeClass(UniqueLockHolder&, SizeAndAlign);

    BEXPORT static TZoneHeapManager* theTZoneHeapManager;

    TZoneHeapManager::State m_state;
    Mutex m_mutex;
    unsigned registeredTypeCount { 0 };
    unsigned largestSizeClassCount { 0 };
    TZoneHeapRandomizeKey m_tzoneKey;
    SizeAndAlign largestSizeClass;
    Map<SizeAndAlign, unsigned, SizeAndAlign> m_typeCountBySizeAndAlignment;
    Vector<SizeAndAlign> m_typeSizes;
    Map<SizeAndAlign, TZoneTypeBuckets*, SizeAndAlign> m_heapRefsBySizeAndAlignment;
};

#define BTZONE_INIT(seed) bmalloc::api::TZoneHeapManager::getInstance().init(seed)
#define BTZONE_REGISTER_TYPES(begin, end) bmalloc::api::TZoneHeapManager::getInstance().registerTZoneTypes(begin, end)
#define BTZONE_REGISTRATION_DONE() bmalloc::api::TZoneHeapManager::getInstance().closeRegistration()

} } // namespace bmalloc::api

#else // BUSE(LIBPAS) -> so !BUSE(LIBPAS)

#define BTZONE_INIT(seed)
#define BTZONE_REGISTER_TYPES(begin, end)
#define BTZONE_REGISTRATION_DONE()

#endif // BUSE(LIBPAS) -> so end of !BUSE(LIBPAS)

#endif // BUSE(TZONE)
