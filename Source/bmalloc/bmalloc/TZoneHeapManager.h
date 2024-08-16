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


class TZoneHeapManager {
    enum State {
        Uninitialized,
        Seeded,
        TypesRegistered
    };

    static constexpr bool verbose = false;
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
        unsigned usedBucketBitmap;
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
        if (!theTZoneHeapManager)
            ensureSingleton();
        BASSERT(theTZoneHeapManager);
        return *theTZoneHeapManager;
    }

    BEXPORT pas_heap_ref* heapRefForTZoneType(bmalloc_type* classType);
    BEXPORT void dumpRegisterdTypes();

private:
    BEXPORT static void ensureSingleton();

    BEXPORT void init();

    void initTypenameTemplate();

    BINLINE Mutex& mutex() { return m_mutex; }

    BINLINE unsigned bucketCountForSizeClass(SizeAndAlign typeSizeAlign)
    {
        if (typeSizeAlign.size() > 128)
            return 4;

        return 8;
    }

    BINLINE unsigned tzoneBucketForKey(UniqueLockHolder&, bmalloc_type* type, unsigned bucketCountForSize)
    {
        SHA256ResultAsUnsigned sha256Result;

        m_tzoneKey.className = type->name;
        m_tzoneKey.sizeOfType = type->size;
        m_tzoneKey.alignmentOfType = type->alignment;

        if (verbose) {
            TZONE_LOG_DEBUG("Choosing Bucket name: %p  size: %u  align: %u  ", m_tzoneKey.className, m_tzoneKey.sizeOfType, m_tzoneKey.alignmentOfType);
            TZONE_LOG_DEBUG(" seed { ");
            for (unsigned i = 0; i < CC_SHA1_DIGEST_LENGTH; ++i)
                TZONE_LOG_DEBUG("%02x",  m_tzoneKey.seed[i]);
            TZONE_LOG_DEBUG(" }\n");
        }

        (void)CC_SHA256(&m_tzoneKey, sizeof(TZoneHeapRandomizeKey), (unsigned char*)&sha256Result);

        if (verbose) {
            TZONE_LOG_DEBUG("Result: {");
            for (unsigned i = 0; i < 4; ++i)
                TZONE_LOG_DEBUG(" %02llx",  sha256Result[i]);
            TZONE_LOG_DEBUG(" }  bucket: %llu\n", sha256Result[3] % bucketCountForSize);
        }

        return sha256Result[3] % bucketCountForSize;
    }

    TZoneTypeBuckets* populateBucketsForSizeClass(UniqueLockHolder&, SizeAndAlign);

    BEXPORT static TZoneHeapManager* theTZoneHeapManager;

    TZoneHeapManager::State m_state;
    Mutex m_mutex;
    unsigned largestSizeClassCount { 0 };
    TZoneHeapRandomizeKey m_tzoneKey;
    SizeAndAlign m_largestSizeClass;
    Map<SizeAndAlign, unsigned, SizeAndAlign> m_typeCountBySizeAndAlignment;
    Vector<SizeAndAlign> m_typeSizes;
    Map<SizeAndAlign, TZoneTypeBuckets*, SizeAndAlign> m_heapRefsBySizeAndAlignment;
};

} } // namespace bmalloc::api

#endif // BUSE(LIBPAS)

#endif // BUSE(TZONE)
