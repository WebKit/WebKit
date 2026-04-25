//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ComputeGenericHashPertTest:
//   Performance benchmark for angle::ComputeGenericHash(...)
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "ANGLEPerfTest.h"

#include <chrono>

#include "common/hash_utils.h"
#include "util/random_utils.h"

using namespace testing;

namespace
{
constexpr unsigned int kIterationsPerStep = 5;

template <typename T>
class ComputeGenericHashPerfTest : public ANGLEPerfTest
{
  public:
    ComputeGenericHashPerfTest();

    void SetUp() override;
    void step() override;

  private:
    angle::RNG mRNG;
    std::set<T> mKeys;
};

template <typename T>
ComputeGenericHashPerfTest<T>::ComputeGenericHashPerfTest()
    : ANGLEPerfTest("ComputeGenericHashPerfTest", "", "", kIterationsPerStep), mRNG(0x12345678u)
{}

template <typename T>
void ComputeGenericHashPerfTest<T>::SetUp()
{
    // Generate 1000 keys with random data
    for (int keyCount = 0; keyCount < 1000; keyCount++)
    {
        T key;
        constexpr size_t kKeySize = key.size();
        std::vector<uint8_t> bytes(kKeySize);
        FillVectorWithRandomUBytes(&mRNG, &bytes);
        memcpy(key.data(), bytes.data(), kKeySize);
        mKeys.insert(key);
    }
}

template <typename T>
void ComputeGenericHashPerfTest<T>::step()
{
    for (const T &key : mKeys)
    {
        angle::Span span(key);
        size_t hash = angle::ComputeGenericHash(span);
        ANGLE_UNUSED_VARIABLE(hash);
    }
}

template <size_t N>
using HashKey = std::array<uint8_t, N>;

using TestTypes = Types<HashKey<4>,
                        HashKey<8>,
                        HashKey<16>,
                        HashKey<32>,
                        HashKey<64>,
                        HashKey<128>,
                        HashKey<256>>;

constexpr char kTestTypeNames[][100] = {"KeySize_4_bytes",  "KeySize_8_bytes",  "KeySize_16_bytes",
                                        "KeySize_32_bytes", "KeySize_64_bytes", "KeySize_128_bytes",
                                        "KeySize_256_bytes"};

class KeySizeNames
{
  public:
    template <typename BitSetType>
    static std::string GetName(int typeIndex)
    {
        return kTestTypeNames[typeIndex];
    }
};

TYPED_TEST_SUITE(ComputeGenericHashPerfTest, TestTypes, KeySizeNames);

// Test hash computation speed for keys with different sizes
TYPED_TEST(ComputeGenericHashPerfTest, Run)
{
    this->run();
}

}  // anonymous namespace
