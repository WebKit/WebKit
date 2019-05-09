//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IndexDataManagerPerfTest:
//   Performance test for index buffer management.
//

#include "ANGLEPerfTest.h"

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
BitSetIteratorPerfTest<T>::BitSetIteratorPerfTest() : ANGLEPerfTest("BitSetIteratorPerf", "_run", 1)
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

// These type names unfortunately don't get printed correctly in Gtest.
#if defined(ANGLE_IS_64_BIT_CPU)
using TestTypes = Types<angle::IterableBitSet<32>, angle::BitSet32<32>, angle::BitSet64<32>>;
#else
using TestTypes = Types<angle::IterableBitSet<32>, angle::BitSet32<32>>;
#endif  // defined(ANGLE_IS_64_BIT_CPU)
TYPED_TEST_SUITE(BitSetIteratorPerfTest, TestTypes);

TYPED_TEST(BitSetIteratorPerfTest, Run)
{
    this->run();
}

}  // anonymous namespace
