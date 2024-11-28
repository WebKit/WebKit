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
#include "TZoneLog.h"
#include <CommonCrypto/CommonDigest.h>
#include <mutex>

#if BUSE(LIBPAS)
#include "bmalloc_heap_ref.h"

namespace bmalloc { namespace api {

static constexpr unsigned defaultBucketsForSmallSizes = 5;
static constexpr unsigned defaultBucketsForLargeSizes = 3;
static constexpr unsigned defaultMaxSmallSize = 128;

class TZoneHeapManager {
    enum State {
        Uninitialized,
        Seeded,
        TypesRegistered
    };

    static const unsigned typeNameLen = 12;

    typedef uint64_t SHA256ResultAsUnsigned[CC_SHA256_DIGEST_LENGTH / sizeof(uint64_t)];
    static_assert(!(CC_SHA256_DIGEST_LENGTH % sizeof(uint64_t)));

    struct TZoneHeapRandomizeKey {
        unsigned char seed[CC_SHA1_DIGEST_LENGTH];
        const char* className;
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
        unsigned numberOfTypesThisSizeClass;
        unsigned usedBucketBitmap;
        Vector<unsigned> bucketUseCounts;
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
            return (value.size() ^ value.alignment()) >> 3;
        }

        bool operator==(const SizeAndAlign& other) const
        {
            return key() == other.key();
        }

        bool operator<(const SizeAndAlign& other) const
        {
            if (size() != other.size())
                return size() < other.size();

            return alignment() < other.alignment();
        }

        operator bool() const
        {
            return !!key();
        }

        union {
            struct {
                unsigned size;
                unsigned alignment;
            } u;
            unsigned long key;
        } m_value;
    };

    struct TZoneTypeAndSize : public bmalloc_type {
        TZoneTypeAndSize()
        {
            name = nullptr;
            size = 0;
            alignment = 0;
            m_key = 0;
        }

        TZoneTypeAndSize(const bmalloc_type* type)
        {
            name = bmalloc_type_name(type);
            size = bmalloc_type_size(type);
            alignment = bmalloc_type_alignment(type);
            m_key = reinterpret_cast<uintptr_t>(name) << 12 ^ size << 3 ^ alignment >> 3;
        }

        inline unsigned long key() const { return m_key; }

        static unsigned long hash(TZoneTypeAndSize value)
        {
            return value.m_key;
        }

        bool operator==(const TZoneTypeAndSize& other) const
        {
            return name == other.name
                && size == other.size
                && alignment == other.alignment;
        }

        bool operator<(const TZoneTypeAndSize& other) const
        {
            if (name != other.name)
                return name < other.name;

            if (size != other.size)
                return size < other.size;

            return alignment < other.alignment;
        }

        operator bool() const
        {
            return !!key();
        }

        uintptr_t m_key;
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

    BEXPORT bool isReady();

    BINLINE static TZoneHeapManager& singleton()
    {
        if (!theTZoneHeapManager) {
            ensureSingleton();
            theTZoneHeapManager->init();
        }
        BASSERT(theTZoneHeapManager);
        return *theTZoneHeapManager;
    }

    BINLINE static void initWithBucketSettings(unsigned smallSizeCount, unsigned largeSizeCount = 0, unsigned smallSizeLimit = 0)
    {
        RELEASE_BASSERT(!theTZoneHeapManager);
        if (!theTZoneHeapManager) {
            ensureSingleton();
            RELEASE_BASSERT(theTZoneHeapManager);
            theTZoneHeapManager->setBucketParams(smallSizeCount, largeSizeCount, smallSizeLimit);
            theTZoneHeapManager->init();
        }
    }

    BEXPORT pas_heap_ref* heapRefForTZoneType(const bmalloc_type* classType);
    BEXPORT pas_heap_ref* heapRefForTZoneTypeDifferentSize(const bmalloc_type* classType);
    BEXPORT void dumpRegisteredTypes();

private:
    BEXPORT static void ensureSingleton();

    BEXPORT void setBucketParams(unsigned smallSizeCount, unsigned largeSizeCount = 0, unsigned smallSizeLimit = 0);

    BEXPORT void init();

    void initTypenameTemplate();

    BINLINE Mutex& mutex() { return m_mutex; }
    BINLINE Mutex& differentSizeMutex() { return m_differentSizeMutex; }

    inline unsigned bucketCountForSizeClass(SizeAndAlign typeSizeAlign);

    inline unsigned tzoneBucketForKey(UniqueLockHolder&, const bmalloc_type*, unsigned bucketCountForSize);
    TZoneTypeBuckets* populateBucketsForSizeClass(UniqueLockHolder&, SizeAndAlign);

    BEXPORT static TZoneHeapManager* theTZoneHeapManager;

    TZoneHeapManager::State m_state;
    Mutex m_mutex;
    Mutex m_differentSizeMutex;
    TZoneHeapRandomizeKey m_tzoneKey;
    unsigned bucketsForSmallSizes { defaultBucketsForSmallSizes };
    unsigned bucketsForLargeSizes { defaultBucketsForLargeSizes };
    unsigned maxSmallSize { defaultMaxSmallSize };
    unsigned largestBucketCount { 0 };
    Vector<SizeAndAlign> m_typeSizes;
    Map<SizeAndAlign, TZoneTypeBuckets*, SizeAndAlign> m_heapRefsBySizeAndAlignment;
    Map<TZoneTypeAndSize, pas_heap_ref*, TZoneTypeAndSize> m_heapRefsByTypeAndSize;
};

} } // namespace bmalloc::api

#endif // BUSE(LIBPAS)

#endif // BUSE(TZONE)
