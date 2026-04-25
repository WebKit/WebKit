//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// StreamingHasherPertTest:
//   Performance benchmark for streaming hashers
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "ANGLEPerfTest.h"

#include "anglebase/sha1.h"
#include "common/hash_utils.h"
#include "util/random_utils.h"

using namespace testing;

namespace
{
constexpr unsigned int kIterationsPerStep = 5;

template <typename T>
class StreamingHasherPerfTest : public ANGLEPerfTest
{
  public:
    StreamingHasherPerfTest();

    void SetUp() override;
    void TearDown() override;

    void step() override;

  private:
    angle::RNG mRNG;
    std::vector<uint8_t> mHeader;
    std::vector<uint8_t> mBody;
    std::vector<uint8_t> mFooter;
};

template <typename T>
StreamingHasherPerfTest<T>::StreamingHasherPerfTest()
    : ANGLEPerfTest(::testing::UnitTest::GetInstance()->current_test_suite()->name(),
                    "",
                    "",
                    kIterationsPerStep),
      mRNG(0x12345678u)
{}

template <typename T>
void StreamingHasherPerfTest<T>::SetUp()
{
    constexpr size_t kSize = 1024;

    // Set up header
    mHeader.resize(kSize);
    FillVectorWithRandomUBytes(&mRNG, &mHeader);

    // Set up body
    mBody.resize(kSize * 100);
    FillVectorWithRandomUBytes(&mRNG, &mBody);

    // Set up footer
    mFooter.resize(kSize);
    FillVectorWithRandomUBytes(&mRNG, &mFooter);
}

template <typename T>
void StreamingHasherPerfTest<T>::TearDown()
{
    mHeader.clear();
    mBody.clear();
    mFooter.clear();
}

template <typename T>
void StreamingHasherPerfTest<T>::step()
{
    const uint16_t chunkIndex = 5;
    const uint32_t type       = 0xABCD;
    const size_t size         = std::numeric_limits<size_t>::max();
    const float version       = 3.1415;

    // Perform 100 iterations
    for (int iteration = 0; iteration < 100; iteration++)
    {
        T hasher;
        hasher.Init();
        hasher.Update(mHeader.data(), mHeader.size());
        hasher.Update(mBody.data(), mBody.size());
        hasher.Update(mFooter.data(), mFooter.size());
        angle::UpdateHashWithValue(hasher, chunkIndex);
        angle::UpdateHashWithValue(hasher, type);
        angle::UpdateHashWithValue(hasher, size);
        angle::UpdateHashWithValue(hasher, version);
        hasher.Final();
        const unsigned char *hash = hasher.Digest();
        (void)hash;
    }
}

using TestTypes = Types<angle::base::SecureHashAlgorithm, angle::StreamingHasher>;

constexpr const char *kTestTypeNames[] = {"SecureHashAlgorithm", "StreamingHasher"};

class StreamingHasherNames
{
  public:
    template <typename BitSetType>
    static std::string GetName(int typeIndex)
    {
        return kTestTypeNames[typeIndex];
    }
};

TYPED_TEST_SUITE(StreamingHasherPerfTest, TestTypes, StreamingHasherNames);

// Test hashing speed for different streaming hashers
TYPED_TEST(StreamingHasherPerfTest, Run)
{
    this->run();
}

}  // anonymous namespace
