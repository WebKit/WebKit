/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"

#include <wtf/BloomFilter.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/SHA1.h>

namespace TestWebKitAPI {

static Vector<unsigned> generateRandomHashes(size_t hashCount)
{
    Vector<unsigned> hashes;
    for (unsigned i = 0; i < hashCount; ++i)
        hashes.append(cryptographicallyRandomNumber<uint32_t>());
    return hashes;
}

static Vector<SHA1::Digest> generateRandomDigests(size_t hashCount)
{
    Vector<SHA1::Digest> hashes;
    SHA1 sha1;
    for (unsigned i = 0; i < hashCount; ++i) {
        double random = cryptographicallyRandomUnitInterval();
        sha1.addBytes(reinterpret_cast<uint8_t*>(&random), sizeof(double));
        SHA1::Digest digest;
        sha1.computeHash(digest);
        hashes.append(digest);
    }
    return hashes;
}

TEST(WTF_BloomFilter, Basic)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomHashes(hashCount);

    BloomFilter<16> filter;
    for (auto hash : hashes)
        filter.add(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));

    auto moreHashes = generateRandomHashes(hashCount);
    unsigned mayContainCount = 0;
    for (auto hash : moreHashes)
        mayContainCount += filter.mayContain(hash) ? 1 : 0;
    // False positive rate is ~0.09% so this should always be true.
    EXPECT_TRUE(mayContainCount < hashCount / 10);

    for (auto hash : moreHashes)
        filter.add(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));
    for (auto hash : moreHashes)
        EXPECT_TRUE(filter.mayContain(hash));
}

TEST(WTF_BloomFilter, BasicDigest)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomDigests(hashCount);

    BloomFilter<20> filter;
    for (auto hash : hashes)
        filter.add(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));

    auto moreHashes = generateRandomDigests(hashCount);
    unsigned mayContainCount = 0;
    for (auto hash : moreHashes)
        mayContainCount += filter.mayContain(hash) ? 1 : 0;
    // False positive rate is ~0.000004% so this should always be true.
    EXPECT_TRUE(mayContainCount < hashCount / 10);

    for (auto hash : moreHashes)
        filter.add(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));
    for (auto hash : moreHashes)
        EXPECT_TRUE(filter.mayContain(hash));
}

TEST(WTF_BloomFilter, BasicCounting)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomHashes(hashCount);

    CountingBloomFilter<16> filter;
    for (auto hash : hashes)
        filter.add(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));

    for (auto hash : hashes)
        filter.add(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));

    for (auto hash : hashes)
        filter.remove(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));

    auto moreHashes = generateRandomHashes(hashCount);
    unsigned mayContainCount = 0;
    for (auto hash : moreHashes)
        mayContainCount += filter.mayContain(hash) ? 1 : 0;
    // False positive rate is ~0.09% so this should always be true.
    EXPECT_TRUE(mayContainCount < hashCount / 10);

    for (auto hash : moreHashes)
        filter.add(hash);
    for (auto hash : hashes)
        filter.remove(hash);

    for (auto hash : moreHashes)
        EXPECT_TRUE(filter.mayContain(hash));

    for (auto hash : moreHashes)
        filter.remove(hash);

    for (auto hash : hashes)
        EXPECT_TRUE(!filter.mayContain(hash));
    for (auto hash : moreHashes)
        EXPECT_TRUE(!filter.mayContain(hash));
}

TEST(WTF_BloomFilter, Clear)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomHashes(hashCount);

    BloomFilter<16> filter;
    for (auto hash : hashes)
        filter.add(hash);

    filter.clear();

    for (auto hash : hashes)
        EXPECT_TRUE(!filter.mayContain(hash));
}

TEST(WTF_BloomFilter, ClearCounting)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomHashes(hashCount);

    CountingBloomFilter<16> filter;
    for (auto hash : hashes)
        filter.add(hash);
    for (auto hash : hashes)
        filter.add(hash);

    filter.clear();

    for (auto hash : hashes)
        EXPECT_TRUE(!filter.mayContain(hash));
}

TEST(WTF_BloomFilter, CountingOverflow)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomHashes(hashCount);

    CountingBloomFilter<16> filter;
    for (auto hash : hashes)
        filter.add(hash);

    for (unsigned i = 0; i < filter.maximumCount() + 100; ++i)
        filter.add(hashes[0]);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));

    for (auto hash : hashes)
        filter.remove(hash);

    unsigned mayContainCount = 0;
    for (auto hash : hashes) {
        if (hash == hashes[0])
            EXPECT_TRUE(filter.mayContain(hash));
        else
            mayContainCount += filter.mayContain(hash) ? 1 : 0;
    }
    // False positive rate should be very low.
    EXPECT_TRUE(mayContainCount < hashCount / 100);

    for (unsigned i = 0; i < filter.maximumCount() + 100; ++i)
        filter.remove(hashes[0]);

    // The bucket has overflowed and is stuck.
    EXPECT_TRUE(filter.mayContain(hashes[0]));
}

TEST(WTF_BloomFilter, Combine)
{
    const unsigned hashCount = 1000;
    auto hashes = generateRandomHashes(hashCount);

    BloomFilter<16> filter;
    for (auto hash : hashes)
        filter.add(hash);

    auto moreHashes = generateRandomHashes(hashCount);

    BloomFilter<16> anotherFilter;
    for (auto hash : moreHashes)
        anotherFilter.add(hash);

    filter.add(anotherFilter);

    for (auto hash : hashes)
        EXPECT_TRUE(filter.mayContain(hash));
    for (auto hash : moreHashes)
        EXPECT_TRUE(filter.mayContain(hash));
}

}
