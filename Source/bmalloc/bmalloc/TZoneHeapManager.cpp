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

#include "TZoneHeapManager.h"

#if BUSE(TZONE)

#include "BPlatform.h"
#include "ProcessCheck.h"
#include "Sizes.h"
#include "TZoneLog.h"
#include "VMAllocate.h"
#include "bmalloc.h"
#include "bmalloc_heap_inlines.h"

#if BOS(DARWIN)
#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

namespace bmalloc { namespace api {

TZoneHeapManager* TZoneHeapManager::theTZoneHeapManager = nullptr;

static const char base64Chars[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned SizeBase64Size = 3;
static const unsigned AlignmentBase64Size = 1;
static const unsigned IndexSize = 2;
static const unsigned typeNameLen = 12;

typedef union {
    struct {
        char prefix[3];
        char sizeBase64[SizeBase64Size];
        char underscore1;
        char alignmentBase64[AlignmentBase64Size];
        char underscore2;
        char index[IndexSize];
        char terminatingNull;
    } nameTemplate;
    char string[typeNameLen];
} TypeNameTemplate;

static_assert(sizeof(TypeNameTemplate) == typeNameLen);

static TypeNameTemplate typeNameTemplate;

static void dumpRegisterdTypesAtExit(void)
{
    TZoneHeapManager::singleton().dumpRegisterdTypes();
}

void TZoneHeapManager::init()
{
    RELEASE_BASSERT(m_state == TZoneHeapManager::Uninitialized);

    if constexpr (verbose)
        TZONE_LOG_DEBUG("TZoneHeapManager initialization ");

#if BOS(DARWIN)
    // Use the boot UUID and the process' name to seed the key.
    static const size_t rawSeedLength = 128;
    char rawSeed[rawSeedLength] = { };

    uint64_t primordialSeed;
    struct timeval timeValue;
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    size_t size = sizeof(timeValue);

    auto sysctlResult = sysctl(mib, 2, &timeValue, &size, nullptr, 0);
    if (sysctlResult) {
        TZONE_LOG_DEBUG("kern.boottime is required for TZoneHeap initialization: %d\n", sysctlResult);
        RELEASE_BASSERT(!sysctlResult);
    }
    primordialSeed = timeValue.tv_sec * 1000 * 1000 + timeValue.tv_usec;

    if constexpr (verbose)
        TZONE_LOG_DEBUG("primordialSeed: 0x%llx\n", primordialSeed);

    const char* procName = processNameString();

    if constexpr (verbose)
        TZONE_LOG_DEBUG("Process Name: \"%s\"\n", procName);

    unsigned byteIdx = 0;

    while (primordialSeed && byteIdx < rawSeedLength) {
        int digit = primordialSeed & 0xf;
        rawSeed[byteIdx++] = 'Z' - digit;
        primordialSeed >>= 4;
    }

    auto procNameLen = strlen(procName);

    for (unsigned i = 0; i < procNameLen && byteIdx < rawSeedLength; byteIdx++, i++)
        rawSeed[byteIdx] = procName[i];

    for (; byteIdx < rawSeedLength; byteIdx++)
        rawSeed[byteIdx] = 'Q' - (byteIdx & 0xf);

    (void)CC_SHA256(&rawSeed, rawSeedLength, (unsigned char*)&m_tzoneKey.seed);
#else // OS(DARWIN) => !OS(DARWIN)
    if constexpr (verbose)
        TZONE_LOG_DEBUG("using static seed\n");

    const unsigned char defaultSeed[CC_SHA1_DIGEST_LENGTH] = { "DefaultSeed\x12\x34\x56\x78\x9a\xbc\xde\xf0" };
    memcpy(m_tzoneKey.seed, defaultSeed, CC_SHA1_DIGEST_LENGTH);
#endif // OS(DARWIN) => !OS(DARWIN)

    if constexpr (verbose) {
        TZONE_LOG_DEBUG("    Computed key {");
        for (unsigned i = 0; i < CC_SHA1_DIGEST_LENGTH; ++i)
            TZONE_LOG_DEBUG(" %02x",  m_tzoneKey.seed[i]);
        TZONE_LOG_DEBUG(" }\n");
    }

    m_state = TZoneHeapManager::Seeded;

    atexit(dumpRegisterdTypesAtExit);
}

bool TZoneHeapManager::isReady()
{
    return m_state >= TZoneHeapManager::Seeded;
}

void TZoneHeapManager::initTypenameTemplate()
{
    typeNameTemplate.nameTemplate.prefix[0] = 'T';
    typeNameTemplate.nameTemplate.prefix[1] = 'Z';
    typeNameTemplate.nameTemplate.prefix[2] = '_';
    typeNameTemplate.nameTemplate.underscore1 = '_';
    typeNameTemplate.nameTemplate.underscore2 = '_';
    typeNameTemplate.nameTemplate.terminatingNull = '\0';
}

static char* nameForType(UniqueLockHolder&, unsigned typeSize, unsigned alignment, unsigned index)
{
    for (unsigned i = 0; i < SizeBase64Size; ++i) {
        typeNameTemplate.nameTemplate.sizeBase64[SizeBase64Size - i - 1] = base64Chars[typeSize % 64];
        typeSize <<= 6;
    }

    for (unsigned i = 0; i < AlignmentBase64Size; ++i) {
        typeNameTemplate.nameTemplate.alignmentBase64[AlignmentBase64Size - i - 1] = base64Chars[alignment % 64];
        alignment <<= 6;
    }

    for (unsigned i = 0; i < IndexSize; ++i) {
        typeNameTemplate.nameTemplate.index[IndexSize - i - 1] = '0' + index % 10;
        index /= 10;
    }

    return &typeNameTemplate.string[0];
}

static char* nameForTypeUpdateIndex(UniqueLockHolder&, unsigned index)
{
    for (unsigned i = 0; i < IndexSize; ++i) {
        typeNameTemplate.nameTemplate.index[IndexSize - i - 1] = '0' + index % 10;
        index /= 10;
    }

    return &typeNameTemplate.string[0];
}

void TZoneHeapManager::dumpRegisterdTypes()
{
    if (verbose && m_state >= TZoneHeapManager::Seeded) {
        if (!m_typeSizes.size())
            return;

        std::sort(m_typeSizes.begin(), m_typeSizes.end());
        auto typeSizesEnd = m_typeSizes.end();

        Vector<unsigned> bucketCountHistogram;
        unsigned totalTypeCount = 0;
        unsigned totalUseBucketCount = 0;

        TZONE_LOG_DEBUG("TZoneHeap registered size classes: %zu\n", m_typeSizes.size());
        TZONE_LOG_DEBUG("        Size      Align    Buckets    TypeCnt   UsedBkts\n");
        TZONE_LOG_DEBUG("    --------   --------   --------   --------   --------\n");
        for (auto iter = m_typeSizes.begin(); iter < typeSizesEnd; iter++) {
            SizeAndAlign typeSizeAlign = *iter;

            auto typeCount = m_typeCountBySizeAndAlignment.get(typeSizeAlign);
            totalTypeCount += typeCount;

            TZoneTypeBuckets* bucketsForSize = m_heapRefsBySizeAndAlignment.get(typeSizeAlign);
            unsigned usedBuckets = 0;

            for (unsigned bucket = 0; bucket < bucketsForSize->numberOfBuckets; ++bucket) {
                if (bucketsForSize->usedBucketBitmap & 1<<bucket)
                    usedBuckets++;
            }

            totalUseBucketCount += usedBuckets;

            TZONE_LOG_DEBUG("    %8u   %8u   %8u   %8u   %8u\n", typeSizeAlign.size(), typeSizeAlign.alignment(), bucketCountForSizeClass(typeSizeAlign), typeCount, usedBuckets);

            auto bucketCount = bucketCountForSizeClass(typeSizeAlign);

            if (bucketCountHistogram.size() <= bucketCount)
                bucketCountHistogram.resize(bucketCount + 1);

            bucketCountHistogram[bucketCount] = bucketCountHistogram[bucketCount] + 1;
            if (typeCount > largestSizeClassCount) {
                largestSizeClassCount = typeCount;
                m_largestSizeClass = typeSizeAlign;
            }
        }

        TZONE_LOG_DEBUG("    TZone types in use %u  total buckets (heaps) in use: %u\n", totalTypeCount, totalUseBucketCount);

        TZONE_LOG_DEBUG("    Size class bucket histogram:");
        for (unsigned i = 1; i < bucketCountHistogram.size(); ++i) {
            if (bucketCountHistogram[i])
                TZONE_LOG_DEBUG(" count %u: %u", i, bucketCountHistogram[i]);
        }
        TZONE_LOG_DEBUG("\n");

        TZONE_LOG_DEBUG("    Most populated size class:  size: %u alignment %u type count: %u\n", m_largestSizeClass.size(), m_largestSizeClass.alignment(), largestSizeClassCount);
    }
}

void TZoneHeapManager::ensureSingleton()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            theTZoneHeapManager = new TZoneHeapManager();
            theTZoneHeapManager->init();
        }
    );
};

TZoneHeapManager::TZoneTypeBuckets* TZoneHeapManager::populateBucketsForSizeClass(UniqueLockHolder& lock, SizeAndAlign typeSizeAlign)
{
    RELEASE_BASSERT(m_state >= TZoneHeapManager::Seeded);
    BASSERT(!m_heapRefsBySizeAndAlignment.contains(typeSizeAlign));

    auto bucketCount = bucketCountForSizeClass(typeSizeAlign);

    if (verbose) {
        m_typeSizes.push(typeSizeAlign);

        m_typeCountBySizeAndAlignment.set(typeSizeAlign, 1);
    }

    TZoneTypeBuckets* buckets = static_cast<TZoneTypeBuckets*>(zeroedMalloc(SIZE_TZONE_TYPE_BUCKETS(bucketCount), CompactAllocationMode::NonCompact));

    buckets->numberOfBuckets = bucketCount;

    for (unsigned i = 0; i < bucketCount; ++i) {
        char* typeName = !i ? nameForType(lock, typeSizeAlign.size(), typeSizeAlign.alignment(), i) : nameForTypeUpdateIndex(lock, i);
        memcpy(buckets->buckets[i].typeName, typeName, typeNameLen);
        buckets->buckets[i].type.size = typeSizeAlign.size();
        buckets->buckets[i].type.alignment = typeSizeAlign.alignment();
        buckets->buckets[i].type.name = buckets->buckets[i].typeName;
        buckets->buckets[i].heapref.type = (const pas_heap_type*)(&buckets->buckets[i].type);
    }

    m_heapRefsBySizeAndAlignment.set(typeSizeAlign, buckets);

    m_state = TZoneHeapManager::TypesRegistered;

    return buckets;
}

pas_heap_ref* TZoneHeapManager::heapRefForTZoneType(bmalloc_type* classType)
{
    RELEASE_BASSERT(m_state >= TZoneHeapManager::Seeded);

    static unsigned registerHeapCount = 0;

    UniqueLockHolder lock(mutex());

    SizeAndAlign typeSizeAlign = SizeAndAlign(classType);

    TZoneTypeBuckets* bucketsForSize = nullptr;

    if (m_heapRefsBySizeAndAlignment.contains(typeSizeAlign)) {
        bucketsForSize = m_heapRefsBySizeAndAlignment.get(typeSizeAlign);
        if (verbose) {
            unsigned count = m_typeCountBySizeAndAlignment.get(typeSizeAlign);
            m_typeCountBySizeAndAlignment.set(typeSizeAlign, ++count);
        }
    } else
        bucketsForSize = populateBucketsForSizeClass(lock, typeSizeAlign);

    unsigned bucket = tzoneBucketForKey(lock, classType, bucketsForSize->numberOfBuckets);

    if (verbose) {
        bucketsForSize->usedBucketBitmap |= 1 << bucket;
        if (!(++registerHeapCount % 5))
            dumpRegisterdTypes();
    }

    return &bucketsForSize->buckets[bucket].heapref;
}

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
