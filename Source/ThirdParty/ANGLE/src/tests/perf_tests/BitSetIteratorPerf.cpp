//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BitSetIteratorPerf.cpp:
//   Performance test for bitset iterators.
//

#include "ANGLEPerfTest.h"
#include "common/unsafe_buffers.h"

#include <gmock/gmock.h>

#include "common/bitset_utils.h"

using namespace testing;

namespace
{
template <typename T>
class BitSetIteratorPerfTest : public ANGLEPerfTest
{
  public:
    BitSetIteratorPerfTest();

    void step() override;

    T mBits;
};

template <typename T>
BitSetIteratorPerfTest<T>::BitSetIteratorPerfTest()
    : ANGLEPerfTest(::testing::UnitTest::GetInstance()->current_test_suite()->name(), "", "_run", 1)
{}

template <typename T>
void BitSetIteratorPerfTest<T>::step()
{
    mBits.flip();

    for (size_t bit : mBits)
    {
        ANGLE_UNUSED_VARIABLE(bit);
    }

    mBits.reset();
}

using TestTypes = Types<angle::BitSet32<32>,
                        angle::BitSet64<32>,
                        angle::BitSet64<64>,
                        angle::BitSet<96>,
                        angle::BitSetArray<96>,
                        angle::BitSetArray<300>>;

constexpr char kTestTypeNames[][100] = {"BitSet32_32", "BitSet64_32",    "BitSet64_64",
                                        "BitSet_96",   "BitSetArray_96", "BitSetArray_300"};

class BitSetIteratorTypeNames
{
  public:
    template <typename BitSetType>
    static std::string GetName(int typeIndex)
    {
        return ANGLE_UNSAFE_TODO(kTestTypeNames[typeIndex]);
    }
};

TYPED_TEST_SUITE(BitSetIteratorPerfTest, TestTypes, BitSetIteratorTypeNames);

TYPED_TEST(BitSetIteratorPerfTest, Run)
{
    this->run();
}

}  // anonymous namespace
