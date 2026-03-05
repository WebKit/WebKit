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

#include "TZoneHeapManager.h"

#if BUSE(TZONE)

#include "BCompiler.h"
#include "BPlatform.h"
#include "ProcessCheck.h"
#include "Sizes.h"
#include "TZoneLog.h"
#include "bmalloc.h"

#if BOS(DARWIN)
#include <CommonCrypto/CommonHMAC.h>
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
TZoneHeapManager::State TZoneHeapManager::s_state;

static constexpr unsigned defaultBucketsForSmallSizes = 5;
static constexpr unsigned defaultBucketsForLargeSizes = 3;
static constexpr unsigned defaultMaxSmallSize = 128;

static unsigned bucketsForSmallSizes { defaultBucketsForSmallSizes };
static unsigned bucketsForLargeSizes { defaultBucketsForLargeSizes };
static unsigned maxSmallSize { defaultMaxSmallSize };

static bool requirePerBootPrimordialSeed;

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

    // Ensures that the default value for s_state is State::Uninitialized.
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
    if (tzoneMallocFallback != TZoneMallocFallback::Undecided)
        return;

    if (Environment::get()->shouldBmallocAllocateThroughSystemHeap()) {
        tzoneMallocFallback = TZoneMallocFallback::ForceDebugMalloc;
        return;
    }

    const char* env = getenv("bmalloc_TZoneHeap");
    if (env && (!strcasecmp(env, "false") || !strcasecmp(env, "no") || !strcmp(env, "0"))) {
        tzoneMallocFallback = TZoneMallocFallback::ForceDebugMalloc;
        return;
    }

    tzoneMallocFallback = TZoneMallocFallback::DoNotFallBack;
}

void TZoneHeapManager::requirePerBootSeed()
{
    RELEASE_BASSERT(s_state < State::Seeded);
    requirePerBootPrimordialSeed = true;
}

void TZoneHeapManager::setBucketParams(unsigned smallSizeCount, unsigned largeSizeCount, unsigned smallSizeLimit)
{
    RELEASE_BASSERT(s_state < State::StartedRegisteringTypes);

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
    RELEASE_BASSERT(s_state == State::Uninitialized);

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
        TZONE_LOG_DEBUG("kern.boottime is required for TZoneHeap initialization: %d errno %d\n", sysctlResult, errno);
        RELEASE_BASSERT(!sysctlResult || !requirePerBootPrimordialSeed);
        // Some clients of JSC may not have access to kern.boottime. In those cases, use a fallback.
        gettimeofday(&timeValue, NULL);
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

    alignas(8) std::array<unsigned char, CC_SHA256_DIGEST_LENGTH> defaultSeed;
    (void)CC_SHA256(&rawSeed, rawSeedLength, defaultSeed.data());
#else // OS(DARWIN) => !OS(DARWIN)
    if constexpr (verbose)
        TZONE_LOG_DEBUG("using static seed\n");

    const std::array<unsigned char, CC_SHA1_DIGEST_LENGTH> defaultSeed = { "DefaultSeed\x12\x34\x56\x78\x9a\xbc\xde\xf0" };
#endif // OS(DARWIN) => !OS(DARWIN)

    const uint64_t* seedPtr = reinterpret_cast<const uint64_t*>(defaultSeed.data());
    m_tzoneKeySeed = 0;
    unsigned remainingBytes = defaultSeed.size();
    while (remainingBytes > sizeof(m_tzoneKeySeed)) {
        m_tzoneKeySeed = m_tzoneKeySeed ^ *seedPtr++;
        remainingBytes -= sizeof(m_tzoneKeySeed);
    }
    uint64_t remainingSeed = 0;
    const unsigned char* seedBytes = reinterpret_cast<const unsigned char*>(seedPtr);
    while (remainingBytes) {
        remainingSeed = (remainingSeed << 8) | *seedBytes++;
        remainingBytes--;
    }
    m_tzoneKeySeed = m_tzoneKeySeed ^ remainingSeed;

    if constexpr (verbose) {
        TZONE_LOG_DEBUG("    Computed key {");
        for (unsigned char byte : defaultSeed)
            TZONE_LOG_DEBUG(" %02x", byte);
        TZONE_LOG_DEBUG(" }\n");
    }

    s_state = State::Seeded;

    if (verbose)
        atexit(dumpRegisteredTypesAtExit);
}

bool TZoneHeapManager::isReady()
{
    return s_state >= State::Seeded;
}

#if TZONE_VERBOSE_DEBUG

class TZoneDescriptorDecoder {
public:
    TZoneDescriptorDecoder(TZoneDescriptor value)
        : m_value(value)
    { }

    inline TZoneDescriptor value() const { return m_value; }

    inline TZoneCategory category() const
    {
        return static_cast<TZoneCategory>(m_value >> TZoneSpecification::categoryShift);
    }

    inline unsigned sizeClass() const
    {
        constexpr unsigned sizeClassDividedBy16Mask = (1ull << TZoneSpecification::numSizeClassDividedBy16Bits) - 1;
        unsigned sizeClassDividedBy16 = (m_value >> TZoneSpecification::sizeClassDividedBy16Shift) & sizeClassDividedBy16Mask;
        return sizeClassDividedBy16 * 16;
    }

    inline unsigned alignment() const
    {
        constexpr unsigned alignmentLog2Mask = (1ull << TZoneSpecification::numAlignmentLog2Bits) - 1;
        unsigned alignmentLog2 = (m_value >> TZoneSpecification::alignmentLog2Shift) & alignmentLog2Mask;
        return 1ull << alignmentLog2;
    }

private:
    TZoneDescriptor m_value { 0 };
};

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

static char* nameForTypeNonCompact(LockHolder&, unsigned typeSize, unsigned alignment)
{
    for (unsigned i = 0; i < SizeBase64Size; ++i) {
        typeNameTemplate.nameTemplate.sizeBase64[SizeBase64Size - i - 1] = base64Chars[typeSize % 64];
        typeSize >>= 6;
    }

    for (unsigned i = 0; i < AlignmentBase64Size; ++i) {
        typeNameTemplate.nameTemplate.alignmentBase64[AlignmentBase64Size - i - 1] = base64Chars[alignment % 64];
        alignment >>= 6;
    }

    // Use the index bytes to say that this heap is non-compact.
    typeNameTemplate.nameTemplate.index[0] = 'N';
    typeNameTemplate.nameTemplate.index[1] = 'C';

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
    if (verbose && s_state >= State::Seeded) {
        if (!m_registeredDescriptors.size())
            return;

        std::ranges::sort(m_registeredDescriptors);
        auto registeredDescriptorsEnd = m_registeredDescriptors.end();

        Vector<unsigned> bucketCountHistogram;
        unsigned totalTypeCount = 0;
        unsigned totalUseBucketCount = 0;
        unsigned typeCountHighWatermark = 0;

        TZoneDescriptor typeCountHighWatermarkDescriptor = 0;

        TZONE_LOG_DEBUG("TZoneHeap registered descriptors: %zu @ <pid%u>\n", m_registeredDescriptors.size(), getpid());

        TZONE_LOG_DEBUG("    Cat    Size  Align  Bckts  Types  Inuse ");
        TZONE_LOG_DEBUG("\n");

        TZONE_LOG_DEBUG("    ---  ------  -----  -----  -----  ----- ");
        TZONE_LOG_DEBUG("\n");

        for (auto iter = m_registeredDescriptors.begin(); iter < registeredDescriptorsEnd; iter++) {
            TZoneDescriptorDecoder descriptor = *iter;
            unsigned bucketCount = bucketCountForSizeClass(descriptor.sizeClass());

            Group* group = m_groupByDescriptor.get(descriptor.value());
            unsigned typeCount = group->numberOfTypesInGroup;
            totalTypeCount += group->numberOfTypesInGroup;

            unsigned usedBuckets = 0;

            for (unsigned bucket = 0; bucket < group->numberOfBuckets; ++bucket) {
                if (group->bucketUseCounts[bucket])
                    usedBuckets++;
            }

            totalUseBucketCount += usedBuckets;

            TZONE_LOG_DEBUG("    %2uc  %6u  %5u  %5u  %5u  %5u : ", static_cast<unsigned>(descriptor.category()), descriptor.sizeClass(), descriptor.alignment(), bucketCount, typeCount, usedBuckets);

            for (unsigned bucket = 0; bucket < group->numberOfBuckets; ++bucket)
                TZONE_LOG_DEBUG("  %5u", group->bucketUseCounts[bucket]);
            TZONE_LOG_DEBUG(" descriptor: %" PRIx64 "\n", descriptor.value());

            if (bucketCountHistogram.size() <= bucketCount)
                bucketCountHistogram.resize(bucketCount + 1);

            bucketCountHistogram[bucketCount] = bucketCountHistogram[bucketCount] + 1;
            if (typeCount > typeCountHighWatermark) {
                typeCountHighWatermark = typeCount;
                typeCountHighWatermarkDescriptor = descriptor.value();
            }
        }

        TZONE_LOG_DEBUG("    TZone types in use %u  total buckets (heaps) in use: %u\n", totalTypeCount, totalUseBucketCount);

        TZONE_LOG_DEBUG("    Size class bucket histogram:");
        for (unsigned i = 1; i < bucketCountHistogram.size(); ++i) {
            if (bucketCountHistogram[i])
                TZONE_LOG_DEBUG(" count %u: %u", i, bucketCountHistogram[i]);
        }
        TZONE_LOG_DEBUG("\n");

        TZoneDescriptorDecoder descriptor = typeCountHighWatermarkDescriptor;
        TZONE_LOG_DEBUG("    Most populated size class:  size: %u alignment %u type count: %u\n", descriptor.sizeClass(), descriptor.alignment(), typeCountHighWatermark);
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

BINLINE unsigned TZoneHeapManager::bucketCountForSizeClass(unsigned sizeClass)
{
    if (sizeClass > maxSmallSize)
        return bucketsForLargeSizes;

    return bucketsForSmallSizes;
}

class WeakRandom final {
public:
    static constexpr std::pair<uint64_t, uint64_t> nextState(uint64_t x, uint64_t y)
    {
        x ^= x << 23;
        x ^= x >> 17;
        x ^= y ^ (y >> 26);
        x += y * 0x10101;
        return { y, x };
    }

    static constexpr std::pair<uint64_t, uint64_t> generate(uint64_t seed1, uint64_t seed2)
    {
        if (!seed1)
            seed1 = 1;
        if (!seed2)
            seed2 = 1;
        uint64_t low = seed1;
        uint64_t high = seed2;
        return nextState(low, high);
    }
};

BALLOW_UNSAFE_BUFFER_USAGE_BEGIN

BINLINE unsigned TZoneHeapManager::bucketForKey(const TZoneSpecification& spec, unsigned bucketCountForSize, LockHolder&)
{
    static constexpr bool verboseBucketSelection = false;

#if BOS(DARWIN)
    // The output of HMAC_SHA256 is the same length as the SHA256 digest, since HMAC's final step is running
    // the hash function H on a value derived from the original message.
    uint64_t hmac[CC_SHA256_DIGEST_LENGTH / sizeof(uint64_t)];
    static_assert(sizeof(hmac) == 32);

    uintptr_t addressOfHeapRef = std::bit_cast<uintptr_t>(spec.addressOfHeapRef);

    CCHmac(kCCHmacAlgSHA256, &m_tzoneKeySeed, sizeof(m_tzoneKeySeed), &addressOfHeapRef, sizeof(addressOfHeapRef), hmac);
    uint64_t random = hmac[0];
    unsigned bucket = random % bucketCountForSize;

#else
    // We don't necessarily have CommonCrypto included. We'll fall back to WeakRandom here.
    auto [randomLow, randomHigh] = WeakRandom::generate(m_tzoneKeySeed, std::bit_cast<uintptr_t>(spec.addressOfHeapRef));
    std::tie(randomLow, randomHigh) = WeakRandom::nextState(randomLow, randomHigh);
    std::tie(randomLow, randomHigh) = WeakRandom::nextState(randomLow, randomHigh);
    std::tie(randomLow, randomHigh) = WeakRandom::nextState(randomLow, randomHigh);
    uint64_t random = randomLow;
    unsigned bucket = random % bucketCountForSize;
#endif

    if constexpr (verboseBucketSelection) {
        TZONE_LOG_DEBUG("Choosing Bucket heapRef: %p size: %u align: %u", spec.addressOfHeapRef, spec.size, spec.alignment);
        TZONE_LOG_DEBUG(" seed { %llu }\n", m_tzoneKeySeed);
        TZONE_LOG_DEBUG("Result: { %llu }  bucket: %u\n", random, bucket);
    }

    return bucket;
}

BALLOW_UNSAFE_BUFFER_USAGE_END

TZoneHeapManager::Group* TZoneHeapManager::populateGroupBuckets(LockHolder& lock, const TZoneSpecification& spec)
{
    TZoneDescriptor descriptor = spec.descriptor;
    unsigned sizeClass = spec.sizeClass();
    RELEASE_BASSERT(s_state >= State::Seeded);
    BASSERT(!m_groupByDescriptor.contains(descriptor));
    s_state = State::StartedRegisteringTypes;

    auto bucketCount = bucketCountForSizeClass(sizeClass);

#if TZONE_VERBOSE_DEBUG
    if constexpr (verbose) {
        if (bucketCount > largestBucketCount)
            largestBucketCount = bucketCount;

        m_registeredDescriptors.push(descriptor);
    }
#endif

    Group* group = static_cast<Group*>(zeroedMalloc(TZONE_GROUP_SIZE(bucketCount), CompactAllocationMode::NonCompact));

    group->numberOfBuckets = bucketCount;
#if TZONE_VERBOSE_DEBUG
    group->numberOfTypesInGroup = 0;
    group->bucketUseCounts.resize(bucketCount);
#endif

    // Fill in non-compact bucket.

#if TZONE_VERBOSE_DEBUG
    char* typeName = nameForTypeNonCompact(lock, sizeClass, spec.alignment);
    memcpy(group->nonCompactBucket.typeName, typeName, typeNameLen);
#else
    PAS_UNUSED_PARAM(lock);
    setNextTypeName(group->nonCompactBucket.typeName, typeNameLen);
#endif
    group->nonCompactBucket.type.size = sizeClass;
    group->nonCompactBucket.type.alignment = spec.alignment;
    group->nonCompactBucket.type.name = group->nonCompactBucket.typeName;
    group->nonCompactBucket.heapref.type = (const pas_heap_type*)(&group->nonCompactBucket.type);
    group->nonCompactBucket.heapref.is_non_compact_heap = true;

    for (unsigned i = 0; i < bucketCount; ++i) {
#if TZONE_VERBOSE_DEBUG
        char* typeName = !i ? nameForType(lock, sizeClass, spec.alignment, i) : nameForTypeUpdateIndex(lock, i);
        memcpy(group->buckets[i].typeName, typeName, typeNameLen);
#else
        PAS_UNUSED_PARAM(lock);
        setNextTypeName(group->buckets[i].typeName, typeNameLen);
#endif
        group->buckets[i].type.size = sizeClass;
        group->buckets[i].type.alignment = spec.alignment;
        group->buckets[i].type.name = group->buckets[i].typeName;

        group->buckets[i].heapref.type = (const pas_heap_type*)(&group->buckets[i].type);
        group->buckets[i].heapref.is_non_compact_heap = false;
    }

    m_groupByDescriptor.set(descriptor, group);

    return group;
}

BINLINE pas_heap_ref* TZoneHeapManager::heapRefForTZoneType(const TZoneSpecification& spec, LockHolder& lock)
{
    Group* group = nullptr;
    if (auto cachedGroup = m_groupByDescriptor.getOptional(spec.descriptor))
        group = cachedGroup.value();
    else
        group = populateGroupBuckets(lock, spec);

    if (spec.allocationMode == CompactAllocationMode::NonCompact && PAS_USE_COMPACT_ONLY_TZONE_HEAP)
        return &group->nonCompactBucket.heapref;

    unsigned bucket = bucketForKey(spec, group->numberOfBuckets, lock);

#if TZONE_VERBOSE_DEBUG
    if constexpr (verbose) {
        static unsigned registerHeapCount = 0;

        group->numberOfTypesInGroup++;
        group->bucketUseCounts[bucket]++;

        group->usedBucketBitmap |= 1 << bucket;
        if (!(++registerHeapCount % 10))
            dumpRegisteredTypes();
    }
#endif

    return &group->buckets[bucket].heapref;
}

pas_heap_ref* TZoneHeapManager::heapRefForTZoneType(const TZoneSpecification& spec)
{
    RELEASE_BASSERT(s_state >= State::Seeded);
    RELEASE_BASSERT(tzoneMallocFallback != TZoneMallocFallback::Undecided);

    LockHolder lock(mutex());
    return heapRefForTZoneType(spec, lock);
}

pas_heap_ref* TZoneHeapManager::TZoneHeapManager::heapRefForTZoneTypeDifferentSize(size_t requestedSize, const TZoneSpecification& spec)
{
    LockHolder lock(differentSizeMutex());
    RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::DoNotFallBack);

    unsigned newSizeClass = TZone::sizeClassFor(requestedSize);
    unsigned alignment = spec.alignment;
    TZoneTypeKey key(spec.addressOfHeapRef, newSizeClass, alignment);

    if (auto bucket = m_differentSizedHeapRefs.getOptional(key))
        return bucket.value();

    TZONE_LOG_DEBUG("Unannotated TZone type with actual: size %zu, expected: size %u alignment %u\n", requestedSize, spec.size, alignment);
#if BUSE_TZONE_SPEC_NAME_ARG
    TZONE_LOG_DEBUG("  Super Class: %s @ %s:%u\n", spec.name, spec.file, spec.line);
#endif

    // We can reuse spec.addressOfHeapRef because it is only used in a hash for
    // selecting a TZone bucket. The size being different is what will allow the
    // bucket selection to be different. The spec.name part is only used for
    // debugging.
    TZoneSpecification newSpec = {
        spec.addressOfHeapRef,
        static_cast<unsigned>(requestedSize),
        spec.alignment,
        TZoneCategory::SizeAndAlignment,
        spec.allocationMode,
        TZoneSpecification::encodeDefaultDescriptor(TZoneCategory::SizeAndAlignment, newSizeClass, spec.alignment),
#if BUSE_TZONE_SPEC_NAME_ARG
        // These values are only for debugging. We have no curent way to get these
        // values from the child TZone class that failed to declare itself
        // TZONE_ALLOCATED. So, just populate them with the parent class' values
        // just to placate the compiler though they are wrong values.
        spec.name,
        spec.file,
        spec.line,
#endif
    };
    pas_heap_ref* result = heapRefForTZoneType(newSpec);

    m_differentSizedHeapRefs.set(key, result);

    return result;
}

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
