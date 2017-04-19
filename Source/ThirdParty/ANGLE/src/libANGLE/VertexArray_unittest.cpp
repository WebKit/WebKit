//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for VertexArray and related classes.
//

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/bitset_utils.h"
#include "common/utilities.h"
#include "libANGLE/VertexArray.h"

using namespace gl;

// Tests that function GetAttribIndex computes the index properly.
TEST(VertexArrayTest, VerifyGetAttribIndex)
{
    VertexArray::DirtyBits dirtyBits;
    size_t bits[] = {1, 4, 9, 16, 25, 36, 49, 64, 81, 90};
    int count     = sizeof(bits) / sizeof(size_t);
    for (int i = 0; i < count; i++)
    {
        dirtyBits.set(bits[i]);
    }

    for (unsigned long dirtyBit : angle::IterateBitSet(dirtyBits))
    {
        size_t index = VertexArray::GetAttribIndex(dirtyBit);
        if (dirtyBit < VertexArray::DIRTY_BIT_ATTRIB_MAX_ENABLED)
        {
            EXPECT_EQ(dirtyBit - VertexArray::DIRTY_BIT_ATTRIB_0_ENABLED, index);
        }
        else if (dirtyBit < VertexArray::DIRTY_BIT_ATTRIB_MAX_POINTER)
        {
            EXPECT_EQ(dirtyBit - VertexArray::DIRTY_BIT_ATTRIB_0_POINTER, index);
        }
        else if (dirtyBit < VertexArray::DIRTY_BIT_ATTRIB_MAX_FORMAT)
        {
            EXPECT_EQ(dirtyBit - VertexArray::DIRTY_BIT_ATTRIB_0_FORMAT, index);
        }
        else if (dirtyBit < VertexArray::DIRTY_BIT_ATTRIB_MAX_BINDING)
        {
            EXPECT_EQ(dirtyBit - VertexArray::DIRTY_BIT_ATTRIB_0_BINDING, index);
        }
        else if (dirtyBit < VertexArray::DIRTY_BIT_BINDING_MAX_BUFFER)
        {
            EXPECT_EQ(dirtyBit - VertexArray::DIRTY_BIT_BINDING_0_BUFFER, index);
        }
        else if (dirtyBit < VertexArray::DIRTY_BIT_BINDING_MAX_DIVISOR)
        {
            EXPECT_EQ(dirtyBit - VertexArray::DIRTY_BIT_BINDING_0_DIVISOR, index);
        }
        else
            ASSERT_TRUE(false);
    }
}
