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
#include "Sizes.h"
#include "VMAllocate.h"
#include "bmalloc.h"
#include "bmalloc_heap_inlines.h"

#if BOS(DARWIN)
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

namespace bmalloc { namespace api {

static constexpr bool verbose = false;

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

void TZoneHeapManager::init(const char* seed)
{
    RELEASE_BASSERT(m_state == TZoneHeapManager::Uninitialized);

    if (verbose)
        fprintf(stderr, "TZoneHeapManager::init(seed: %s)", seed ? seed : "<null>");

    if (seed && seed[0]) {
        if (verbose)
            fprintf(stderr, " using provided seed");
        // Convert seed string to hex values.
        enum { HighNibble, LowNibble } currentNibble = HighNibble;
        unsigned char currentByte = 0;
        for (unsigned i = 0; i < CC_SHA1_DIGEST_LENGTH * 2 && seed[i]; ++i) {
            char currentChar = seed[i];
            unsigned char nibble = currentChar >= '0' && currentChar <= '9' ? currentChar - '0' : (((currentChar |= 0x20) >= 'a' && currentChar <= 'f') ? 10 + currentChar - 'a' : 0);
            if (currentNibble == HighNibble) {
                currentByte = nibble << 4;
                currentNibble = LowNibble;
            } else {
                currentByte |= nibble;
                m_tzoneKey.seed[i >> 1] = currentByte;
                currentNibble = HighNibble;
            }
        }
    } else {
#if BOS(DARWIN)
        if (verbose)
            fprintf(stderr, " using boot time");

#define MIB_SIZE 2

        int mib[MIB_SIZE];
        size_t size;

        struct timeval bootTime;
        mib[0] = CTL_KERN;
        mib[1] = KERN_BOOTTIME;
        size = sizeof(bootTime);
        int result = sysctl(mib, MIB_SIZE, &bootTime, &size, nullptr, 0);
        BUNUSED(result);
        BASSERT(!result);

        if (verbose)
            fprintf(stderr, "boottime: { tv_sec: 0x%lx  tv_usec: )x%x }", bootTime.tv_sec, bootTime.tv_usec);

        // Convert bootTime to hex values.
        unsigned i = CC_SHA1_DIGEST_LENGTH;
        for (size_t byteCount = 0; i && byteCount < sizeof(bootTime.tv_usec); ++byteCount, --i) {
            m_tzoneKey.seed[i - 1] = bootTime.tv_usec & 0xff;
            bootTime.tv_usec >>= 8;
        }

        for (size_t byteCount = 0; i && byteCount < sizeof(bootTime.tv_sec); ++byteCount, --i) {
            m_tzoneKey.seed[i - 1] = bootTime.tv_sec & 0xff;
            bootTime.tv_sec >>= 8;
        }

        for (; i; --i)
            m_tzoneKey.seed[i - 1] = 0;
#else // OS(DARWIN) => !OS(DARWIN)
        if (verbose)
            fprintf(stderr, " using static seed");

        const unsigned char defaultSeed[CC_SHA1_DIGEST_LENGTH] = { "DefaultSeed\x12\x34\x56\x78\x9a\xbc\xde\xf0" };
        memcpy(m_tzoneKey.seed, defaultSeed, CC_SHA1_DIGEST_LENGTH);
#endif // OS(DARWIN) => !OS(DARWIN)
    }

    if (verbose) {
        fprintf(stderr, "\n    tzone seed: {");
        for (unsigned i = 0; i < CC_SHA1_DIGEST_LENGTH; ++i)
            fprintf(stderr, " %x",  m_tzoneKey.seed[i]);
        fprintf(stderr, " }\n");
    }

    m_state = TZoneHeapManager::Seeded;
}

bool TZoneHeapManager::isReady()
{
    return m_state == TZoneHeapManager::TypesRegistered;
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

void TZoneHeapManager::registerTZoneTypes(const bmalloc_type* begin, const bmalloc_type* end)
{
    RELEASE_BASSERT(m_state == TZoneHeapManager::Seeded || m_state == TZoneHeapManager::RegisteringTypes);

    m_state = TZoneHeapManager::RegisteringTypes;

    for (const auto* current = begin; current != end; ++current) {
        registeredTypeCount++;
        SizeAndAlign typeSizeAlign = SizeAndAlign(current);
        unsigned count = 1;
        if (m_typeCountBySizeAndAlignment.contains(typeSizeAlign)) {
            count = m_typeCountBySizeAndAlignment.get(typeSizeAlign);
            ++count;
        } else
            m_typeSizes.push(typeSizeAlign);

        m_typeCountBySizeAndAlignment.set(typeSizeAlign, count);
    }
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

void TZoneHeapManager::closeRegistration()
{
    static_assert(!(sizeof(TZoneBucket) % sizeof(void*)));
    static_assert(!(sizeof(TZoneTypeBuckets) % sizeof(void*)));

    RELEASE_BASSERT(m_state == TZoneHeapManager::Seeded || m_state == TZoneHeapManager::RegisteringTypes);

    if (verbose) {
        auto typeSizesEnd = m_typeSizes.end();

        Vector<unsigned> bucketCountHistogram;

        bucketCountHistogram.grow(6);

        for (auto iter = m_typeSizes.begin(); iter < typeSizesEnd; iter++) {
            SizeAndAlign typeSizeAlign = *iter;
            auto typeCount = m_typeCountBySizeAndAlignment.get(typeSizeAlign);
            auto bucketCount = bucketCountForTypeCount(typeCount);

            bucketCountHistogram[bucketCount] = bucketCountHistogram[bucketCount] + 1;
            if (typeCount > largestSizeClassCount) {
                largestSizeClassCount = typeCount;
                largestSizeClass = typeSizeAlign;
            }
        }

        fprintf(stderr, "TZoneHeapManager configured: registered types: %u  size classes: %zu\n", registeredTypeCount, m_typeSizes.size());
        fprintf(stderr, "    most populated size class:  size: %u  alignment %u  type count: %u\n", largestSizeClass.size(), largestSizeClass.alignment(), largestSizeClassCount);
        fprintf(stderr, "    size class bucket histogram:");
        for (unsigned i = 1; i <= 5; ++i)
            fprintf(stderr, " count %u: %u", i, bucketCountHistogram[i]);
        fprintf(stderr, "\n");
    }

    m_state = TZoneHeapManager::TypesRegistered;
}

void TZoneHeapManager::ensureInstance()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            theTZoneHeapManager = new TZoneHeapManager();
        }
    );
};

TZoneHeapManager::TZoneTypeBuckets* TZoneHeapManager::populateBucketsForSizeClass(UniqueLockHolder& lock, SizeAndAlign typeSizeAlign)
{
    BASSERT(!m_heapRefsBySizeAndAlignment.contains(typeSizeAlign));

    auto typeCount = m_typeCountBySizeAndAlignment.get(typeSizeAlign);
    auto bucketCount = bucketCountForTypeCount(typeCount);

    TZoneTypeBuckets* buckets = static_cast<TZoneTypeBuckets*>(zeroedMalloc(SIZE_TZONE_TYPE_BUCKETS(bucketCount)));

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

    return buckets;
}


pas_heap_ref* TZoneHeapManager::heapRefForTZoneType(bmalloc_type* classType)
{
    RELEASE_BASSERT(m_state == TZoneHeapManager::TypesRegistered);

    UniqueLockHolder lock(mutex());

    SizeAndAlign typeSizeAlign = SizeAndAlign(classType);

    TZoneTypeBuckets* bucketsForSize = nullptr;

    if (m_heapRefsBySizeAndAlignment.contains(typeSizeAlign))
        bucketsForSize = m_heapRefsBySizeAndAlignment.get(typeSizeAlign);
    else if (m_typeCountBySizeAndAlignment.contains(typeSizeAlign))
        bucketsForSize = populateBucketsForSizeClass(lock, typeSizeAlign);
    else {
        // This typically means classType wasn't registered.
        RELEASE_BASSERT_NOT_REACHED();
    }

    unsigned bucket = tzoneBucketForKey(lock, classType, bucketsForSize->numberOfBuckets);

    return &bucketsForSize->buckets[bucket].heapref;
}

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
