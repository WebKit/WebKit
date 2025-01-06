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

#include "BCompiler.h"
#include "BPlatform.h"
#include "ProcessCheck.h"
#include "Sizes.h"
#include "TZoneLog.h"
#include "bmalloc.h"

#if BOS(DARWIN)
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

namespace bmalloc { namespace api {

static bool (*hasDisableTZoneEntitlement)();

#define BUSE_BUCKETS_FOR_SIZE_CLASSES_FROM_ENVVAR 0

TZoneHeapManager* tzoneHeapManager;
TZoneMallocFallback tzoneMallocFallback;
TZoneHeapManager::State TZoneHeapManager::m_state;

static constexpr unsigned defaultBucketsForSmallSizes = 5;
static constexpr unsigned defaultBucketsForLargeSizes = 3;
static constexpr unsigned defaultMaxSmallSize = 128;

unsigned bucketsForSmallSizes { defaultBucketsForSmallSizes };
unsigned bucketsForLargeSizes { defaultBucketsForLargeSizes };
unsigned maxSmallSize { defaultMaxSmallSize };

static constexpr bool verbose = false;

#if TZONE_VERBOSE_DEBUG
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
#endif

static void dumpRegisteredTypesAtExit(void)
{
    TZoneHeapManager::singleton().dumpRegisteredTypes();
}

void TZoneHeapManager::setHasDisableTZoneEntitlementCallback(bool (*disableTZoneEntitlementCheck)())
{
    hasDisableTZoneEntitlement = disableTZoneEntitlementCheck;
}

TZoneHeapManager::TZoneHeapManager()
{
    determineTZoneMallocFallback();

    // Ensures that the default value for m_state is State::Uninitialized.
    static_assert(!static_cast<unsigned>(TZoneHeapManager::State::Uninitialized));

#if TZONE_VERBOSE_DEBUG
    typeNameTemplate.nameTemplate.prefix[0] = 'T';
    typeNameTemplate.nameTemplate.prefix[1] = 'Z';
    typeNameTemplate.nameTemplate.prefix[2] = '_';
    typeNameTemplate.nameTemplate.underscore1 = '_';
    typeNameTemplate.nameTemplate.underscore2 = '_';
    typeNameTemplate.nameTemplate.terminatingNull = '\0';
#endif
    init();
}

void determineTZoneMallocFallback()
{
    static Mutex s_mutex;
    LockHolder lock(s_mutex);
    {
        if (tzoneMallocFallback != TZoneMallocFallback::Undecided)
            return;

        tzoneFreeWithFastFallback = tzoneFreeFast;
        tzoneFreeWithIsoFallback = tzoneFreeFast;

        if (Environment::get()->isDebugHeapEnabled())
            tzoneMallocFallback = TZoneMallocFallback::ForceDebugMalloc;
        else {
            const char* env = getenv("bmalloc_IsoHeap");
            if (env && (!strcasecmp(env, "false") || !strcasecmp(env, "no") || !strcmp(env, "0")))
                tzoneMallocFallback = TZoneMallocFallback::ForceDebugMalloc;
        }

        if (tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc) {
            tzoneFreeWithFastFallback = tzoneFreeWithDebugMalloc;
            tzoneFreeWithIsoFallback = tzoneFreeWithDebugMalloc;
            return;
        }

        const char* tzoneEnv = getenv("bmalloc_TZoneHeap");
        if (tzoneEnv && (!strcasecmp(tzoneEnv, "false") || !strcasecmp(tzoneEnv, "no") || !strcmp(tzoneEnv, "0"))) {
            tzoneMallocFallback = TZoneMallocFallback::ForceSpecifiedFallBack;
            return;
        }

        if (hasDisableTZoneEntitlement && hasDisableTZoneEntitlement()) {
            tzoneMallocFallback = TZoneMallocFallback::ForceSpecifiedFallBack;
            return;
        }

        tzoneMallocFallback = TZoneMallocFallback::DoNotFallBack;
    }
}

void TZoneHeapManager::setBucketParams(unsigned smallSizeCount, unsigned largeSizeCount, unsigned smallSizeLimit)
{
    RELEASE_BASSERT(m_state < State::StartedRegisteringTypes);

    bucketsForSmallSizes = smallSizeCount;
    if (largeSizeCount)
        bucketsForLargeSizes = largeSizeCount;
    if (smallSizeLimit)
        maxSmallSize = smallSizeLimit;

    if constexpr (verbose)
        TZONE_LOG_DEBUG("Buckets params set to smallSizes: %u, largeSizes: %u, small sizes <= %u bytes\n",   bucketsForSmallSizes, bucketsForLargeSizes, maxSmallSize);
}

void TZoneHeapManager::init()
{
    RELEASE_BASSERT(m_state == State::Uninitialized);

    if constexpr (verbose)
        TZONE_LOG_DEBUG("TZoneHeapManager initialization ");

#if BUSE(BUCKETS_FOR_SIZE_CLASSES_FROM_ENVVAR)
    // Allow the setting the bucket per size class params via an environment variable.
    // TZONE_BUCKET_CONFIG=<bucket-for-small-sizes>:<buckets-for-large-sizes>:<max-small-size>
    // Note that one, two or all three params can be given.
    // For example, to specific 1 bucket for all size classes use TZONE_BUCKET_CONFIG=1:1
    char* bucketsForSizeClassesValue = getenv("TZONE_BUCKET_CONFIG");
    if (bucketsForSizeClassesValue && *bucketsForSizeClassesValue) {
        constexpr unsigned numParams = 3;
        constexpr unsigned bufferLength = 30;
        char buffer[bufferLength + 1];
        unsigned paramsAsNumbers[numParams];
        unsigned paramsProvided = 0;

        memcpy(buffer, bucketsForSizeClassesValue, bufferLength);
        buffer[bufferLength] = '\0';
        auto tempString = buffer;
        char* param = nullptr;

        unsigned smallSizeCount { 0 };
        unsigned largeSizeCount { 0 };
        unsigned smallSizeLimit { 0 };

        for (paramsProvided = 0; paramsProvided < numParams && (param = strsep(&tempString, ":")) != nullptr; paramsProvided++)
            paramsAsNumbers[paramsProvided] = static_cast<unsigned>(atol(param));

        if (paramsProvided > 0)
            smallSizeCount = paramsAsNumbers[0];
        if (paramsProvided > 1)
            largeSizeCount = paramsAsNumbers[1];
        if (paramsProvided > 2)
            smallSizeLimit = paramsAsNumbers[2];

        if constexpr (verbose)
            TZONE_LOG_DEBUG("Buckets from env (%s):\n",  bucketsForSizeClassesValue);

        setBucketParams(smallSizeCount, largeSizeCount, smallSizeLimit);
    }
#endif

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

    const char* procName = processNameString();

    if constexpr (verbose) {
        TZONE_LOG_DEBUG("primordialSeed: 0x%llx\n", primordialSeed);
        TZONE_LOG_DEBUG("Process Name: \"%s\"\n", procName);
    }

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

    if constexpr (verbose) {
        TZONE_LOG_DEBUG("rawSeed (len %zu): 0x", rawSeedLength);
        size_t i = 0;
        while (true) {
            TZONE_LOG_DEBUG("%02x", rawSeed[i]);
            if (++i >= rawSeedLength)
                break;
            if (!(i % 32)) {
                TZONE_LOG_DEBUG("\n");
                TZONE_LOG_DEBUG("                 ... ");
            }
        }
        TZONE_LOG_DEBUG("\n");
    }

    unsigned char seed[CC_SHA1_DIGEST_LENGTH];
    (void)CC_SHA256(&rawSeed, rawSeedLength, seed);
#else // OS(DARWIN) => !OS(DARWIN)
    if constexpr (verbose)
        TZONE_LOG_DEBUG("using static seed\n");

    const unsigned char defaultSeed[CC_SHA1_DIGEST_LENGTH] = { "DefaultSeed\x12\x34\x56\x78\x9a\xbc\xde\xf0" };
    memcpy(m_tzoneKey.seed, defaultSeed, CC_SHA1_DIGEST_LENGTH);
#endif // OS(DARWIN) => !OS(DARWIN)

    uint64_t* seedPtr = reinterpret_cast<uint64_t*>(seed);
    m_tzoneKeySeed = 0;
    unsigned remainingBytes = CC_SHA1_DIGEST_LENGTH;
    while (remainingBytes > sizeof(m_tzoneKeySeed)) {
        m_tzoneKeySeed = m_tzoneKeySeed ^ *seedPtr++;
        remainingBytes -= sizeof(m_tzoneKeySeed);
    }
    uint64_t remainingSeed = 0;
    unsigned char* seedBytes = reinterpret_cast<unsigned char*>(seedPtr);
    while (remainingBytes > sizeof(m_tzoneKeySeed)) {
        remainingSeed = (remainingSeed << 8) | *seedBytes++;
        remainingBytes--;
    }
    m_tzoneKeySeed = m_tzoneKeySeed ^ remainingSeed;

    if constexpr (verbose) {
        TZONE_LOG_DEBUG("    Computed key {");
        for (unsigned i = 0; i < CC_SHA1_DIGEST_LENGTH; ++i)
            TZONE_LOG_DEBUG(" %02x", seed[i]);
        TZONE_LOG_DEBUG(" }\n");
    }

    m_state = State::Seeded;

    if (verbose)
        atexit(dumpRegisteredTypesAtExit);
}

bool TZoneHeapManager::isReady()
{
    return m_state >= State::Seeded;
}

#if TZONE_VERBOSE_DEBUG

static char* nameForType(LockHolder&, unsigned typeSize, unsigned alignment, unsigned index)
{
    for (unsigned i = 0; i < SizeBase64Size; ++i) {
        typeNameTemplate.nameTemplate.sizeBase64[SizeBase64Size - i - 1] = base64Chars[typeSize % 64];
        typeSize >>= 6;
    }

    for (unsigned i = 0; i < AlignmentBase64Size; ++i) {
        typeNameTemplate.nameTemplate.alignmentBase64[AlignmentBase64Size - i - 1] = base64Chars[alignment % 64];
        alignment >>= 6;
    }

    for (unsigned i = 0; i < IndexSize; ++i) {
        typeNameTemplate.nameTemplate.index[IndexSize - i - 1] = '0' + index % 10;
        index /= 10;
    }

    return &typeNameTemplate.string[0];
}

static char* nameForTypeUpdateIndex(LockHolder&, unsigned index)
{
    for (unsigned i = 0; i < IndexSize; ++i) {
        typeNameTemplate.nameTemplate.index[IndexSize - i - 1] = '0' + index % 10;
        index /= 10;
    }

    return &typeNameTemplate.string[0];
}

#else // not TZONE_VERBOSE_DEBUG

static void setNextTypeName(char* typeName, size_t length)
{
    static unsigned numberOfTypes = 0;
    unsigned typeIndex = ++numberOfTypes;
    char* p = typeName;
    char* end = &typeName[length - 1];

    // All we want is a somewhat unique string for the name. For speed reasons, we'll
    // just map the digits into a base64 encoded ASCII starting at character '0'.
    while (typeIndex && p < end) {
        *p++ = '0' + (typeIndex & 0x3f);
        typeIndex >>= 6;
    }
    *p++ = '\0';
}

#endif // TZONE_VERBOSE_DEBUG

void TZoneHeapManager::dumpRegisteredTypes()
{
#if TZONE_VERBOSE_DEBUG
    if (verbose && m_state >= State::Seeded) {
        if (!m_typeSizes.size())
            return;

        std::sort(m_typeSizes.begin(), m_typeSizes.end());
        auto typeSizesEnd = m_typeSizes.end();

        Vector<unsigned> bucketCountHistogram;
        unsigned totalTypeCount = 0;
        unsigned totalUseBucketCount = 0;
        unsigned largestSizeClassCount = 0;

        SizeAndAlignment::Value largestSizeClass;

        TZONE_LOG_DEBUG("TZoneHeap registered size classes: %zu\n", m_typeSizes.size());

        TZONE_LOG_DEBUG("      Size  Align  Bckts  Types  Inuse ");
        for (unsigned i = 0; i < largestBucketCount; i++)
            TZONE_LOG_DEBUG("  %sBkt%u", i < 10 ? " " : "", i);
        TZONE_LOG_DEBUG("\n");

        TZONE_LOG_DEBUG("    ------  -----  -----  -----  ----- ");
        for (unsigned i = 0; i < largestBucketCount; i++)
            TZONE_LOG_DEBUG("  -----");
        TZONE_LOG_DEBUG("\n");

        for (auto iter = m_typeSizes.begin(); iter < typeSizesEnd; iter++) {
            SizeAndAlignment::Value sizeAndAlignment = *iter;
            unsigned numBucketsThisSizeClass = bucketCountForSizeClass(sizeAndAlignment);

            TZoneTypeBuckets* bucketsForSize = m_heapRefsBySizeAndAlignment.get(sizeAndAlignment);
            unsigned typeCount = bucketsForSize->numberOfTypesThisSizeClass;
            totalTypeCount += bucketsForSize->numberOfTypesThisSizeClass;

            unsigned usedBuckets = 0;

            for (unsigned bucket = 0; bucket < bucketsForSize->numberOfBuckets; ++bucket) {
                if (bucketsForSize->bucketUseCounts[bucket])
                    usedBuckets++;
            }

            totalUseBucketCount += usedBuckets;

            TZONE_LOG_DEBUG("    %6u  %5u  %5u  %5u  %5u ", SizeAndAlignment::decodeSize(sizeAndAlignment), SizeAndAlignment::decodeAlignment(sizeAndAlignment), numBucketsThisSizeClass, typeCount, usedBuckets);

            for (unsigned bucket = 0; bucket < bucketsForSize->numberOfBuckets; ++bucket)
                TZONE_LOG_DEBUG("  %5u", bucketsForSize->bucketUseCounts[bucket]);
            TZONE_LOG_DEBUG("\n");

            auto bucketCount = bucketCountForSizeClass(sizeAndAlignment);

            if (bucketCountHistogram.size() <= bucketCount)
                bucketCountHistogram.resize(bucketCount + 1);

            bucketCountHistogram[bucketCount] = bucketCountHistogram[bucketCount] + 1;
            if (typeCount > largestSizeClassCount) {
                largestSizeClassCount = typeCount;
                largestSizeClass = sizeAndAlignment;
            }
        }

        TZONE_LOG_DEBUG("    TZone types in use %u  total buckets (heaps) in use: %u\n", totalTypeCount, totalUseBucketCount);

        TZONE_LOG_DEBUG("    Size class bucket histogram:");
        for (unsigned i = 1; i < bucketCountHistogram.size(); ++i) {
            if (bucketCountHistogram[i])
                TZONE_LOG_DEBUG(" count %u: %u", i, bucketCountHistogram[i]);
        }
        TZONE_LOG_DEBUG("\n");

        TZONE_LOG_DEBUG("    Most populated size class:  size: %u alignment %u type count: %u\n", SizeAndAlignment::decodeSize(largestSizeClass), SizeAndAlignment::decodeAlignment(largestSizeClass), largestSizeClassCount);
    }
#endif // TZONE_VERBOSE_DEBUG
}

void TZoneHeapManager::ensureSingleton()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        tzoneHeapManager = new TZoneHeapManager();
    });
};

BINLINE unsigned TZoneHeapManager::bucketCountForSizeClass(SizeAndAlignment::Value sizeAndAlignment)
{
    if (SizeAndAlignment::decodeSize(sizeAndAlignment) > maxSmallSize)
        return bucketsForLargeSizes;

    return bucketsForSmallSizes;
}

class WeakRandom final {
public:
    static constexpr uint64_t nextState(uint64_t x, uint64_t y)
    {
        x ^= x << 23;
        x ^= x >> 17;
        x ^= y ^ (y >> 26);
        return x;
    }

    static constexpr uint64_t generate(uint64_t seed)
    {
        if (!seed)
            seed = 1;
        uint64_t low = seed;
        uint64_t high = seed;
        high = nextState(low, high);
        return low + high;
    }
};

BALLOW_UNSAFE_BUFFER_USAGE_BEGIN

BINLINE unsigned TZoneHeapManager::tzoneBucketForKey(const TZoneSpecification& spec, unsigned bucketCountForSize, LockHolder&)
{
    static constexpr bool verboseBucketSelection = false;
    uint64_t random = WeakRandom::generate(m_tzoneKeySeed);
    random = WeakRandom::nextState(random, std::bit_cast<uintptr_t>(spec.addressOfHeapRef));
    random = WeakRandom::nextState(random, spec.size);
    random = WeakRandom::nextState(random, SizeAndAlignment::decodeAlignment(spec.sizeAndAlignment));
    unsigned bucket = random % bucketCountForSize;

    if constexpr (verboseBucketSelection) {
        TZONE_LOG_DEBUG("Choosing Bucket heapRef: %p size: %u align: %u", spec.addressOfHeapRef, spec.size, SizeAndAlignment::decodeAlignment(spec.sizeAndAlignment));
        TZONE_LOG_DEBUG(" seed { %llu }\n", m_tzoneKeySeed);
        TZONE_LOG_DEBUG("Result: { %llu }  bucket: %u\n", random, bucket);
    }

    return bucket;
}

BALLOW_UNSAFE_BUFFER_USAGE_END

TZoneHeapManager::TZoneTypeBuckets* TZoneHeapManager::populateBucketsForSizeClass(LockHolder& lock, SizeAndAlignment::Value sizeAndAlignment)
{
    RELEASE_BASSERT(m_state >= State::Seeded);
    BASSERT(!m_heapRefsBySizeAndAlignment.contains(sizeAndAlignment));
    m_state = State::StartedRegisteringTypes;

    auto bucketCount = bucketCountForSizeClass(sizeAndAlignment);
    if (tzoneMallocFallback == TZoneMallocFallback::ForceSpecifiedFallBack) {
        // The only way we can get here is if the TZoneAllocationFallback type is Iso.
        // heapRefForTZoneType would already have handled the Fast case.
        bucketCount = 1;
    }

#if TZONE_VERBOSE_DEBUG
    if constexpr (verbose) {
        if (bucketCount > largestBucketCount)
            largestBucketCount = bucketCount;

        m_typeSizes.push(sizeAndAlignment);
    }
#endif

    TZoneTypeBuckets* buckets = static_cast<TZoneTypeBuckets*>(zeroedMalloc(SIZE_TZONE_TYPE_BUCKETS(bucketCount), CompactAllocationMode::NonCompact));

    buckets->numberOfBuckets = bucketCount;
#if TZONE_VERBOSE_DEBUG
    buckets->numberOfTypesThisSizeClass = 0;
    buckets->bucketUseCounts.resize(bucketCount);
#endif

    for (unsigned i = 0; i < bucketCount; ++i) {
#if TZONE_VERBOSE_DEBUG
        char* typeName = !i ? nameForType(lock, SizeAndAlignment::decodeSize(sizeAndAlignment), SizeAndAlignment::decodeAlignment(sizeAndAlignment), i) : nameForTypeUpdateIndex(lock, i);
        memcpy(buckets->buckets[i].typeName, typeName, typeNameLen);
#else
        PAS_UNUSED_PARAM(lock);
        setNextTypeName(buckets->buckets[i].typeName, typeNameLen);
#endif
        buckets->buckets[i].type.size = SizeAndAlignment::decodeSize(sizeAndAlignment);
        buckets->buckets[i].type.alignment = SizeAndAlignment::decodeAlignment(sizeAndAlignment);
        buckets->buckets[i].type.name = buckets->buckets[i].typeName;

        buckets->buckets[i].heapref.type = (const pas_heap_type*)(&buckets->buckets[i].type);
    }

    m_heapRefsBySizeAndAlignment.set(sizeAndAlignment, buckets);

    return buckets;
}

pas_heap_ref* TZoneHeapManager::heapRefForIsoFallback(const TZoneSpecification& spec)
{
    LockHolder lock(mutex());
    m_isoTypeDescriptors.append(BMALLOC_TYPE_INITIALIZER(SizeAndAlignment::decodeSize(spec.sizeAndAlignment), SizeAndAlignment::decodeAlignment(spec.sizeAndAlignment), "TZoneIsoHeap"));
    m_isoHeapRefs.append(BMALLOC_HEAP_REF_INITIALIZER(&m_isoTypeDescriptors.last()));
    return &m_isoHeapRefs.last();
}

BINLINE pas_heap_ref* TZoneHeapManager::heapRefForTZoneType(const TZoneSpecification& spec, LockHolder& lock)
{
    TZoneTypeBuckets* bucketsForSize = nullptr;
    if (auto bucket = m_heapRefsBySizeAndAlignment.getOptional(spec.sizeAndAlignment))
        bucketsForSize = bucket.value();
    else
        bucketsForSize = populateBucketsForSizeClass(lock, spec.sizeAndAlignment);

    unsigned bucket = tzoneBucketForKey(spec, bucketsForSize->numberOfBuckets, lock);

#if TZONE_VERBOSE_DEBUG
    if constexpr (verbose) {
        static unsigned registerHeapCount = 0;

        bucketsForSize->numberOfTypesThisSizeClass++;
        bucketsForSize->bucketUseCounts[bucket]++;

        bucketsForSize->usedBucketBitmap |= 1 << bucket;
        if (!(++registerHeapCount % 10))
            dumpRegisteredTypes();
    }
#endif

    return &bucketsForSize->buckets[bucket].heapref;
}

pas_heap_ref* TZoneHeapManager::heapRefForTZoneType(const TZoneSpecification& spec)
{
    RELEASE_BASSERT(m_state >= State::Seeded);
    RELEASE_BASSERT(tzoneMallocFallback != TZoneMallocFallback::Undecided);

    LockHolder lock(mutex());
    return heapRefForTZoneType(spec, lock);
}

pas_heap_ref* TZoneHeapManager::TZoneHeapManager::heapRefForTZoneTypeDifferentSize(size_t requestedSize, const TZoneSpecification& spec)
{
    LockHolder lock(differentSizeMutex());
    RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::DoNotFallBack);

    unsigned newSize = sizeClassFor(requestedSize);
    unsigned alignment = SizeAndAlignment::decodeAlignment(spec.sizeAndAlignment);
    TZoneTypeKey key(spec.addressOfHeapRef, newSize, alignment);

    if (auto bucket = m_differentSizedHeapRefs.getOptional(key))
        return bucket.value();

    TZONE_LOG_DEBUG("Unannotated TZone type with actual: size %zu, expected: size %u alignment %u\n", requestedSize, spec.size, alignment);
#if BUSE_TZONE_SPEC_NAME_ARG
    TZONE_LOG_DEBUG("  Super Class: %s\n", spec.name);
#endif

    // We can reuse spec.addressOfHeapRef because it is only used in a hash for
    // selecting a TZone bucket. The size being different is what will allow the
    // bucket selection to be different. The spec.name part is only used for
    // debugging.
    TZoneSpecification newSpec = {
        spec.addressOfHeapRef,
        static_cast<unsigned>(requestedSize),
        SizeAndAlignment::encode(newSize, alignment),
#if BUSE_TZONE_SPEC_NAME_ARG
        spec.name
#endif
    };
    pas_heap_ref* result = heapRefForTZoneType(newSpec);

    m_differentSizedHeapRefs.set(key, result);

    return result;
}

#if BUSE_TZONE_PREINITIALIZATION

void TZoneHeapManager::preInitializeHeapRefs(const TZoneSpecification* start, const TZoneSpecification* end)
{
    LockHolder lock(mutex());
    for (auto* curr = start; curr < end; curr++) {
        HeapRef& heapRef = *curr->addressOfHeapRef;
        if (heapRef)
            continue;
        heapRef = heapRefForTZoneType(*curr, lock);
    }
}

#endif // BUSE_TZONE_PREINITIALIZATION

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
