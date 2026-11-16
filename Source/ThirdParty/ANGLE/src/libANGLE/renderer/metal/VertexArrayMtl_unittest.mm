//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VertexArrayMtl_unittest.mm: Unit tests for Metal vertex array draw command computation.

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/VertexArrayMtl.h"

namespace
{

using rx::BufferMtl;
using rx::DrawCommandRange;
using rx::DrawIndexRange;

inline std::vector<DrawCommandRange> ComputeDrawCommandRanges(
    gl::PrimitiveMode mode,
    uint32_t count,
    size_t firstIndex,
    const std::vector<DrawIndexRange> &drawIndexRanges,
    gl::PrimitiveMode drawMode,
    gl::DrawElementsType indexBufferType)
{
    std::vector<DrawCommandRange> drawCommands;
    rx::AppendSimpleDrawCommandRanges(drawCommands, mode, count, firstIndex, drawIndexRanges,
                                      drawMode, indexBufferType);
    return drawCommands;
}

TEST(ComputeDrawCommandRangesTest, UnsignedShortClientData)
{
    std::vector<uint16_t> shortIndices = {0, 1, 2, 0xffff, 3, 4, 5};
    auto ranges                        = BufferMtl::GetDrawIndexRangesFromClientData(
        gl::DrawElementsType::UnsignedShort, static_cast<GLint>(shortIndices.size()),
        shortIndices.data());
    ASSERT_EQ(2u, ranges.size());
    EXPECT_EQ(0u, ranges[0].begin);
    EXPECT_EQ(2u, ranges[0].end);
    EXPECT_EQ(4u, ranges[1].begin);
    EXPECT_EQ(6u, ranges[1].end);
}

TEST(ComputeDrawCommandRangesTest, UnsignedIntClientData)
{
    std::vector<uint32_t> intIndices = {0, 1, 2, 0xffffffff, 3, 4, 5};
    auto ranges = BufferMtl::GetDrawIndexRangesFromClientData(gl::DrawElementsType::UnsignedInt,
                                                              static_cast<GLint>(intIndices.size()),
                                                              intIndices.data());
    ASSERT_EQ(2u, ranges.size());
    EXPECT_EQ(0u, ranges[0].begin);
    EXPECT_EQ(2u, ranges[0].end);
    EXPECT_EQ(4u, ranges[1].begin);
    EXPECT_EQ(6u, ranges[1].end);
}

// Helper to build DrawIndexRanges using BufferMtl::GetDrawIndexRangesFromClientData.
// Values of 0xff are converted to 0xff (primitive restart marker for UnsignedByte).
std::vector<DrawIndexRange> MakeDrawIndexRanges(const std::vector<uint8_t> &indices)
{
    return BufferMtl::GetDrawIndexRangesFromClientData(
        gl::DrawElementsType::UnsignedByte, static_cast<GLint>(indices.size()), indices.data());
}

TEST(ComputeDrawCommandRangesTest, Triangles_NoRestart)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 3, 4, 5});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 6, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_SingleRestartMiddle)
{
    // Buffer: 0 1 2 ff 3 4 5 -> DrawIndexRanges: (0,2), (4,6)
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 0xff, 3, 4, 5});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 7, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(3u, cmds[1].count);
    EXPECT_EQ(8u, cmds[1].offset);  // index 4 * 2
}

TEST(ComputeDrawCommandRangesTest, Triangles_RestartAtBegin)
{
    // Buffer: ff 0 1 2 -> DrawIndexRanges: (1,3)
    auto ranges = MakeDrawIndexRanges({0xff, 0, 1, 2});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 4, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(2u, cmds[0].offset);  // index 1 * 2
}

TEST(ComputeDrawCommandRangesTest, Triangles_RestartAtEnd)
{
    // Buffer: 0 1 2 ff -> DrawIndexRanges: (0,2)
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 0xff});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 4, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_ConsecutiveRestarts)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 0xff, 0xff, 3, 4, 5});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 8, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(3u, cmds[1].count);
    EXPECT_EQ(10u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_IncompletePrimitiveDropped)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 0xff, 2, 3, 4});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 6, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(6u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_WithOffset)
{
    auto ranges = MakeDrawIndexRanges({9, 0, 1, 2, 0xff, 3, 4, 5});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 7, 1, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(2u, cmds[0].offset);
    EXPECT_EQ(3u, cmds[1].count);
    EXPECT_EQ(10u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, Lines_WithRestart)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 0xff, 2, 3, 4});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Lines, 6, 0, ranges, gl::PrimitiveMode::Lines,
                                 gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(2u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(2u, cmds[1].count);
    EXPECT_EQ(6u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, Points_WithRestart)
{
    auto ranges = MakeDrawIndexRanges({0, 0xff, 1});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Points, 3, 0, ranges, gl::PrimitiveMode::Points,
                                 gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(1u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(1u, cmds[1].count);
    EXPECT_EQ(4u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, EmptyBuffer)
{
    std::vector<DrawIndexRange> ranges;
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 0, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    EXPECT_EQ(0u, cmds.size());
}

TEST(ComputeDrawCommandRangesTest, AllRestarts)
{
    auto ranges = MakeDrawIndexRanges({0xff, 0xff, 0xff});
    EXPECT_EQ(0u, ranges.size());
}

TEST(ComputeDrawCommandRangesTest, Triangles_Uint32IndexType)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 0xff, 3, 4, 5});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 7, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedInt);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(3u, cmds[1].count);
    EXPECT_EQ(16u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_CountLimitsWindow)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 3, 4, 5});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 4, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_SplitContentWithRestartAtBegin)
{
    auto ranges = MakeDrawIndexRanges({0xff, 0, 1, 2, 0xff, 2, 1, 3});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 8, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(2u, cmds[0].offset);
    EXPECT_EQ(3u, cmds[1].count);
    EXPECT_EQ(10u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_SplitContentWithRestartAtEnd)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 0xff, 2, 1, 3, 0xff});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 8, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(3u, cmds[1].count);
    EXPECT_EQ(8u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_DegenerateBeforeContent)
{
    auto ranges = MakeDrawIndexRanges({0, 0xff, 0, 1, 2, 2, 1, 3});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 8, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(4u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, Triangles_WithAddOffsetDegenerateAndRestartAtBegin)
{
    auto ranges = MakeDrawIndexRanges({9, 0xff, 0, 0xff, 0, 1, 2, 2, 1, 3});
    ASSERT_EQ(3u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::Triangles, 9, 1, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(8u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, TriangleStrip_NoRestart)
{
    // 4 source -> (4-2)*3 = 6 draw indices
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 3});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::TriangleStrip, 4, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, TriangleStrip_WithRestart)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 3, 0xff, 4, 5, 6, 7});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::TriangleStrip, 9, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(6u, cmds[1].count);
    EXPECT_EQ(30u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, TriangleStrip_TooFewIndices)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 0xff, 2, 3, 4});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::TriangleStrip, 6, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(3u, cmds[0].count);
    EXPECT_EQ(18u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, TriangleStrip_RestartAtBeginAndEnd)
{
    auto ranges = MakeDrawIndexRanges({0xff, 0, 1, 2, 3, 0xff});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::TriangleStrip, 6, 0, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(6u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, LineStrip_NoRestart)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 3});
    ASSERT_EQ(1u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::LineStrip, 4, 0, ranges,
                                 gl::PrimitiveMode::Lines, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, LineStrip_WithRestart)
{
    auto ranges = MakeDrawIndexRanges({0, 1, 2, 0xff, 3, 4, 5});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::LineStrip, 7, 0, ranges,
                                 gl::PrimitiveMode::Lines, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(2u, cmds.size());
    EXPECT_EQ(4u, cmds[0].count);
    EXPECT_EQ(0u, cmds[0].offset);
    EXPECT_EQ(4u, cmds[1].count);
    EXPECT_EQ(16u, cmds[1].offset);
}

TEST(ComputeDrawCommandRangesTest, LineStrip_TooFewIndices)
{
    auto ranges = MakeDrawIndexRanges({0, 0xff, 1, 2});
    ASSERT_EQ(2u, ranges.size());
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::LineStrip, 4, 0, ranges,
                                 gl::PrimitiveMode::Lines, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(2u, cmds[0].count);
    EXPECT_EQ(8u, cmds[0].offset);
}

TEST(ComputeDrawCommandRangesTest, TriangleStrip_WithBufferOffset)
{
    auto ranges = MakeDrawIndexRanges({9, 0, 1, 2, 3});
    auto cmds =
        ComputeDrawCommandRanges(gl::PrimitiveMode::TriangleStrip, 4, 1, ranges,
                                 gl::PrimitiveMode::Triangles, gl::DrawElementsType::UnsignedShort);
    ASSERT_EQ(1u, cmds.size());
    EXPECT_EQ(6u, cmds[0].count);
    EXPECT_EQ(6u, cmds[0].offset);
}

}  // namespace
