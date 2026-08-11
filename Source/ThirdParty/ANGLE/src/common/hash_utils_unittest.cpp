//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hash_utils_unittest: Hashing helper functions tests.

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include <gtest/gtest.h>

#include "common/hash_utils.h"
#include "common/span.h"

using namespace angle;

namespace
{
// Basic functionality test.
TEST(HashUtilsTest, ComputeGenericHash)
{
    std::string a = "aSimpleString!!!";
    std::string b = "anotherString???";

    // Requires a string size aligned to 4 bytes.
    ASSERT_TRUE(a.size() % 4 == 0);
    ASSERT_TRUE(b.size() % 4 == 0);

    size_t aHash = ComputeGenericHash(angle::as_byte_span(a));
    size_t bHash = ComputeGenericHash(angle::as_byte_span(b));

    EXPECT_NE(aHash, bHash);
}

// Streaming hasher functionality test.
TEST(HashUtilsTest, StreamingHasher)
{
    const std::string header  = "aHeaderString!!!";
    const std::string body    = "mainBodyString!!!";
    const std::string footer  = "aFooterString!!!";
    const uint16_t chunkIndex = 5;
    const float version       = 3.1415;

    // StreamingHasher produces a 16-byte hash
    constexpr size_t kHashSize = angle::StreamingHasher::kHashSize;
    using Hash                 = std::array<uint8_t, kHashSize>;

    // Initialize the streaming hasher with a random seed
    angle::StreamingHasher hasher(0xABCDEF);

    Hash hash             = {0};
    Hash intermediateHash = {0};

    // Test null input
    hasher.Update(nullptr, 0);

    // Test 0 sized input
    uint32_t test = 0;
    hasher.Update(&test, 0);

    // Feed in the header and get intermediate hash
    hasher.Update(header.data(), header.size());
    memcpy(intermediateHash.data(), hasher.Digest(), kHashSize);
    EXPECT_NE(hash, intermediateHash);
    hash = intermediateHash;

    // Feed in the body and get intermediate hash
    hasher.Update(body.data(), body.size());
    memcpy(intermediateHash.data(), hasher.Digest(), kHashSize);
    EXPECT_NE(hash, intermediateHash);
    hash = intermediateHash;

    // Feed in the footer and get intermediate hash
    hasher.Update(footer.data(), footer.size());
    memcpy(intermediateHash.data(), hasher.Digest(), kHashSize);
    EXPECT_NE(hash, intermediateHash);
    hash = intermediateHash;

    // Feed in the chunkIndex and get intermediate hash
    angle::UpdateHashWithValue(hasher, chunkIndex);
    memcpy(intermediateHash.data(), hasher.Digest(), kHashSize);
    EXPECT_NE(hash, intermediateHash);
    hash = intermediateHash;

    // Feed in the version and get final hash
    angle::UpdateHashWithValue(hasher, version);
    memcpy(intermediateHash.data(), hasher.Digest(), kHashSize);
    EXPECT_NE(hash, intermediateHash);
    hash = intermediateHash;
}
}  // anonymous namespace
