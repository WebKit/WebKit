//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes_unittest.cpp: Unit tests of the BlendStateExt class.

#include <gtest/gtest.h>

#include "libANGLE/angletypes.h"

namespace angle
{

#if defined(ANGLE_IS_64_BIT_CPU)
constexpr bool is64Bit = true;
#else
constexpr bool is64Bit = false;
#endif

void checkInitState(const gl::BlendStateExt &blendStateExt)
{
    for (size_t i = 0; i < blendStateExt.mMaxDrawBuffers; ++i)
    {
        ASSERT_FALSE(blendStateExt.mEnabledMask.test(i));

        bool r, g, b, a;
        blendStateExt.getColorMaskIndexed(i, &r, &g, &b, &a);
        ASSERT_TRUE(r);
        ASSERT_TRUE(g);
        ASSERT_TRUE(b);
        ASSERT_TRUE(a);

        ASSERT_EQ(blendStateExt.getEquationColorIndexed(i), static_cast<GLenum>(GL_FUNC_ADD));
        ASSERT_EQ(blendStateExt.getEquationAlphaIndexed(i), static_cast<GLenum>(GL_FUNC_ADD));

        ASSERT_EQ(blendStateExt.getSrcColorIndexed(i), static_cast<GLenum>(GL_ONE));
        ASSERT_EQ(blendStateExt.getDstColorIndexed(i), static_cast<GLenum>(GL_ZERO));
        ASSERT_EQ(blendStateExt.getSrcAlphaIndexed(i), static_cast<GLenum>(GL_ONE));
        ASSERT_EQ(blendStateExt.getDstAlphaIndexed(i), static_cast<GLenum>(GL_ZERO));
    }
}

// Test the initial state of BlendStateExt
TEST(BlendStateExt, Init)
{
    {
        const gl::BlendStateExt blendStateExt = gl::BlendStateExt(1);
        ASSERT_EQ(blendStateExt.mMaxDrawBuffers, 1u);
        ASSERT_EQ(blendStateExt.mMaxEnabledMask.to_ulong(), 1u);
        ASSERT_EQ(blendStateExt.mMaxColorMask, is64Bit ? 0xFFu : 0xFu);
        ASSERT_EQ(blendStateExt.mMaxEquationMask, 0xFFu);
        ASSERT_EQ(blendStateExt.mMaxFactorMask, 0xFFu);
        checkInitState(blendStateExt);
    }

    {
        const gl::BlendStateExt blendStateExt = gl::BlendStateExt(4);
        ASSERT_EQ(blendStateExt.mMaxDrawBuffers, 4u);
        ASSERT_EQ(blendStateExt.mMaxEnabledMask.to_ulong(), 0xFu);
        ASSERT_EQ(blendStateExt.mMaxColorMask, is64Bit ? 0xFFFFFFFFu : 0xFFFFu);
        ASSERT_EQ(blendStateExt.mMaxEquationMask, 0xFFFFFFFFu);
        ASSERT_EQ(blendStateExt.mMaxFactorMask, 0xFFFFFFFFu);
        checkInitState(blendStateExt);
    }

    {
        const gl::BlendStateExt blendStateExt = gl::BlendStateExt(8);
        ASSERT_EQ(blendStateExt.mMaxDrawBuffers, 8u);
        ASSERT_EQ(blendStateExt.mMaxEnabledMask.to_ulong(), 0xFFu);
        ASSERT_EQ(blendStateExt.mMaxColorMask, is64Bit ? 0xFFFFFFFFFFFFFFFFu : 0xFFFFFFFFu);
        ASSERT_EQ(blendStateExt.mMaxEquationMask, 0xFFFFFFFFFFFFFFFFu);
        ASSERT_EQ(blendStateExt.mMaxFactorMask, 0xFFFFFFFFFFFFFFFFu);
        checkInitState(blendStateExt);
    }
}

// Test blend enabled flags
TEST(BlendStateExt, BlendEnabled)
{
    gl::BlendStateExt blendStateExt = gl::BlendStateExt(3);

    blendStateExt.setEnabled(true);
    ASSERT_EQ(blendStateExt.mEnabledMask.to_ulong(), 7u);

    blendStateExt.setEnabledIndexed(1, false);
    ASSERT_EQ(blendStateExt.mEnabledMask.to_ulong(), 5u);
}

// Test color write mask manipulations
TEST(BlendStateExt, ColorMask)
{
    gl::BlendStateExt blendStateExt = gl::BlendStateExt(5);

    blendStateExt.setColorMask(true, false, true, false);
    ASSERT_EQ(blendStateExt.mColorMask, is64Bit ? 0x0505050505u : 0x55555u);

    blendStateExt.setColorMaskIndexed(3, false, true, false, true);
    ASSERT_EQ(blendStateExt.mColorMask, is64Bit ? 0x050A050505u : 0x5A555u);

    blendStateExt.setColorMaskIndexed(3, 0xF);
    ASSERT_EQ(blendStateExt.getColorMaskIndexed(3), 0xF);

    blendStateExt.setColorMaskIndexed(3, 0xA);
    ASSERT_EQ(blendStateExt.getColorMaskIndexed(3), 0xA);

    bool r, g, b, a;
    blendStateExt.getColorMaskIndexed(3, &r, &g, &b, &a);
    ASSERT_FALSE(r);
    ASSERT_TRUE(g);
    ASSERT_FALSE(b);
    ASSERT_TRUE(a);

    gl::BlendStateExt::ColorMaskStorage::Type otherColorMask =
        blendStateExt.expandColorMaskIndexed(3);
    ASSERT_EQ(otherColorMask, is64Bit ? 0x0A0A0A0A0Au : 0xAAAAAu);

    const gl::DrawBufferMask diff = blendStateExt.compareColorMask(otherColorMask);
    ASSERT_EQ(diff.to_ulong(), 23u);
}

// Test blend equations manipulations
TEST(BlendStateExt, BlendEquations)
{
    gl::BlendStateExt blendStateExt = gl::BlendStateExt(7);

    blendStateExt.setEquations(GL_MIN, GL_FUNC_SUBTRACT);
    ASSERT_EQ(blendStateExt.mEquationColor, 0x01010101010101u);
    ASSERT_EQ(blendStateExt.mEquationAlpha, 0x04040404040404u);

    blendStateExt.setEquationsIndexed(3, GL_MAX, GL_FUNC_SUBTRACT);
    blendStateExt.setEquationsIndexed(5, GL_MIN, GL_FUNC_ADD);
    ASSERT_EQ(blendStateExt.mEquationColor, 0x01010102010101u);
    ASSERT_EQ(blendStateExt.mEquationAlpha, 0x04000404040404u);
    ASSERT_EQ(blendStateExt.getEquationColorIndexed(3), static_cast<GLenum>(GL_MAX));
    ASSERT_EQ(blendStateExt.getEquationAlphaIndexed(5), static_cast<GLenum>(GL_FUNC_ADD));

    gl::BlendStateExt::EquationStorage::Type otherEquationColor =
        blendStateExt.expandEquationColorIndexed(0);
    gl::BlendStateExt::EquationStorage::Type otherEquationAlpha =
        blendStateExt.expandEquationAlphaIndexed(0);

    ASSERT_EQ(otherEquationColor, 0x01010101010101u);
    ASSERT_EQ(otherEquationAlpha, 0x04040404040404u);

    const gl::DrawBufferMask diff =
        blendStateExt.compareEquations(otherEquationColor, otherEquationAlpha);
    ASSERT_EQ(diff.to_ulong(), 40u);

    // Copy buffer 3 to buffer 0
    blendStateExt.setEquationsIndexed(0, 3, blendStateExt);
    ASSERT_EQ(blendStateExt.getEquationColorIndexed(0), static_cast<GLenum>(GL_MAX));
    ASSERT_EQ(blendStateExt.getEquationAlphaIndexed(0), static_cast<GLenum>(GL_FUNC_SUBTRACT));

    // Copy buffer 5 to buffer 0
    blendStateExt.setEquationsIndexed(0, 5, blendStateExt);
    ASSERT_EQ(blendStateExt.getEquationColorIndexed(0), static_cast<GLenum>(GL_MIN));
    ASSERT_EQ(blendStateExt.getEquationAlphaIndexed(0), static_cast<GLenum>(GL_FUNC_ADD));
}

// Test blend factors manipulations
TEST(BlendStateExt, BlendFactors)
{
    gl::BlendStateExt blendStateExt = gl::BlendStateExt(8);

    blendStateExt.setFactors(GL_SRC_COLOR, GL_DST_COLOR, GL_SRC_ALPHA, GL_DST_ALPHA);
    ASSERT_EQ(blendStateExt.mSrcColor, 0x0202020202020202u);
    ASSERT_EQ(blendStateExt.mDstColor, 0x0808080808080808u);
    ASSERT_EQ(blendStateExt.mSrcAlpha, 0x0404040404040404u);
    ASSERT_EQ(blendStateExt.mDstAlpha, 0x0606060606060606u);

    blendStateExt.setFactorsIndexed(0, GL_ONE, GL_DST_COLOR, GL_SRC_ALPHA, GL_DST_ALPHA);
    blendStateExt.setFactorsIndexed(3, GL_SRC_COLOR, GL_ONE, GL_SRC_ALPHA, GL_DST_ALPHA);
    blendStateExt.setFactorsIndexed(5, GL_SRC_COLOR, GL_DST_COLOR, GL_ONE, GL_DST_ALPHA);
    blendStateExt.setFactorsIndexed(7, GL_SRC_COLOR, GL_DST_COLOR, GL_SRC_ALPHA, GL_ONE);
    ASSERT_EQ(blendStateExt.mSrcColor, 0x0202020202020201u);
    ASSERT_EQ(blendStateExt.mDstColor, 0x0808080801080808u);
    ASSERT_EQ(blendStateExt.mSrcAlpha, 0x0404010404040404u);
    ASSERT_EQ(blendStateExt.mDstAlpha, 0x0106060606060606u);

    ASSERT_EQ(blendStateExt.getSrcColorIndexed(0), static_cast<GLenum>(GL_ONE));
    ASSERT_EQ(blendStateExt.getDstColorIndexed(3), static_cast<GLenum>(GL_ONE));
    ASSERT_EQ(blendStateExt.getSrcAlphaIndexed(5), static_cast<GLenum>(GL_ONE));
    ASSERT_EQ(blendStateExt.getDstAlphaIndexed(7), static_cast<GLenum>(GL_ONE));

    gl::BlendStateExt::FactorStorage::Type otherSrcColor = blendStateExt.expandSrcColorIndexed(1);
    gl::BlendStateExt::FactorStorage::Type otherDstColor = blendStateExt.expandDstColorIndexed(1);
    gl::BlendStateExt::FactorStorage::Type otherSrcAlpha = blendStateExt.expandSrcAlphaIndexed(1);
    gl::BlendStateExt::FactorStorage::Type otherDstAlpha = blendStateExt.expandDstAlphaIndexed(1);

    ASSERT_EQ(otherSrcColor, 0x0202020202020202u);
    ASSERT_EQ(otherDstColor, 0x0808080808080808u);
    ASSERT_EQ(otherSrcAlpha, 0x0404040404040404u);
    ASSERT_EQ(otherDstAlpha, 0x0606060606060606u);

    const gl::DrawBufferMask diff =
        blendStateExt.compareFactors(otherSrcColor, otherDstColor, otherSrcAlpha, otherDstAlpha);
    ASSERT_EQ(diff.to_ulong(), 169u);

    // Copy buffer 0 to buffer 1
    blendStateExt.setFactorsIndexed(1, 0, blendStateExt);
    ASSERT_EQ(blendStateExt.getSrcColorIndexed(1), static_cast<GLenum>(GL_ONE));
    ASSERT_EQ(blendStateExt.getDstColorIndexed(1), static_cast<GLenum>(GL_DST_COLOR));
    ASSERT_EQ(blendStateExt.getSrcAlphaIndexed(1), static_cast<GLenum>(GL_SRC_ALPHA));
    ASSERT_EQ(blendStateExt.getDstAlphaIndexed(1), static_cast<GLenum>(GL_DST_ALPHA));

    // Copy buffer 3 to buffer 1
    blendStateExt.setFactorsIndexed(1, 3, blendStateExt);
    ASSERT_EQ(blendStateExt.getSrcColorIndexed(1), static_cast<GLenum>(GL_SRC_COLOR));
    ASSERT_EQ(blendStateExt.getDstColorIndexed(1), static_cast<GLenum>(GL_ONE));
    ASSERT_EQ(blendStateExt.getSrcAlphaIndexed(1), static_cast<GLenum>(GL_SRC_ALPHA));
    ASSERT_EQ(blendStateExt.getDstAlphaIndexed(1), static_cast<GLenum>(GL_DST_ALPHA));

    // Copy buffer 5 to buffer 1
    blendStateExt.setFactorsIndexed(1, 5, blendStateExt);
    ASSERT_EQ(blendStateExt.getSrcColorIndexed(1), static_cast<GLenum>(GL_SRC_COLOR));
    ASSERT_EQ(blendStateExt.getDstColorIndexed(1), static_cast<GLenum>(GL_DST_COLOR));
    ASSERT_EQ(blendStateExt.getSrcAlphaIndexed(1), static_cast<GLenum>(GL_ONE));
    ASSERT_EQ(blendStateExt.getDstAlphaIndexed(1), static_cast<GLenum>(GL_DST_ALPHA));

    // Copy buffer 7 to buffer 1
    blendStateExt.setFactorsIndexed(1, 7, blendStateExt);
    ASSERT_EQ(blendStateExt.getSrcColorIndexed(1), static_cast<GLenum>(GL_SRC_COLOR));
    ASSERT_EQ(blendStateExt.getDstColorIndexed(1), static_cast<GLenum>(GL_DST_COLOR));
    ASSERT_EQ(blendStateExt.getSrcAlphaIndexed(1), static_cast<GLenum>(GL_SRC_ALPHA));
    ASSERT_EQ(blendStateExt.getDstAlphaIndexed(1), static_cast<GLenum>(GL_ONE));
}

// Test clip rectangle
TEST(Rectangle, Clip)
{
    const gl::Rectangle source(0, 0, 100, 200);
    const gl::Rectangle clip1(0, 0, 50, 100);
    gl::Rectangle result;

    ASSERT_TRUE(gl::ClipRectangle(source, clip1, &result));
    ASSERT_EQ(result.x, 0);
    ASSERT_EQ(result.y, 0);
    ASSERT_EQ(result.width, 50);
    ASSERT_EQ(result.height, 100);

    gl::Rectangle clip2(10, 20, 30, 40);

    ASSERT_TRUE(gl::ClipRectangle(source, clip2, &result));
    ASSERT_EQ(result.x, 10);
    ASSERT_EQ(result.y, 20);
    ASSERT_EQ(result.width, 30);
    ASSERT_EQ(result.height, 40);

    gl::Rectangle clip3(-20, -30, 10000, 400000);

    ASSERT_TRUE(gl::ClipRectangle(source, clip3, &result));
    ASSERT_EQ(result.x, 0);
    ASSERT_EQ(result.y, 0);
    ASSERT_EQ(result.width, 100);
    ASSERT_EQ(result.height, 200);

    gl::Rectangle clip4(50, 100, -20, -30);

    ASSERT_TRUE(gl::ClipRectangle(source, clip4, &result));
    ASSERT_EQ(result.x, 30);
    ASSERT_EQ(result.y, 70);
    ASSERT_EQ(result.width, 20);
    ASSERT_EQ(result.height, 30);

    // Non-overlapping rectangles
    gl::Rectangle clip5(-100, 0, 99, 200);
    ASSERT_FALSE(gl::ClipRectangle(source, clip5, nullptr));

    gl::Rectangle clip6(0, -100, 100, 99);
    ASSERT_FALSE(gl::ClipRectangle(source, clip6, nullptr));

    gl::Rectangle clip7(101, 0, 99, 200);
    ASSERT_FALSE(gl::ClipRectangle(source, clip7, nullptr));

    gl::Rectangle clip8(0, 201, 100, 99);
    ASSERT_FALSE(gl::ClipRectangle(source, clip8, nullptr));

    // Zero-width/height rectangles
    gl::Rectangle clip9(50, 0, 0, 200);
    ASSERT_FALSE(gl::ClipRectangle(source, clip9, nullptr));
    ASSERT_FALSE(gl::ClipRectangle(clip9, source, nullptr));

    gl::Rectangle clip10(0, 100, 100, 0);
    ASSERT_FALSE(gl::ClipRectangle(source, clip10, nullptr));
    ASSERT_FALSE(gl::ClipRectangle(clip10, source, nullptr));
}

// Test combine rectangles
TEST(Rectangle, Combine)
{
    const gl::Rectangle rect1(0, 0, 100, 200);
    const gl::Rectangle rect2(0, 0, 50, 100);
    gl::Rectangle result;

    gl::GetEnclosingRectangle(rect1, rect2, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    const gl::Rectangle rect3(50, 100, 100, 200);

    gl::GetEnclosingRectangle(rect1, rect3, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 150);
    ASSERT_EQ(result.y1(), 300);

    const gl::Rectangle rect4(-20, -30, 100, 200);

    gl::GetEnclosingRectangle(rect1, rect4, &result);
    ASSERT_EQ(result.x0(), -20);
    ASSERT_EQ(result.y0(), -30);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    const gl::Rectangle rect5(10, -30, 100, 200);

    gl::GetEnclosingRectangle(rect1, rect5, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), -30);
    ASSERT_EQ(result.x1(), 110);
    ASSERT_EQ(result.y1(), 200);
}

// Test extend rectangles
TEST(Rectangle, Extend)
{
    const gl::Rectangle source(0, 0, 100, 200);
    const gl::Rectangle extend1(0, 0, 50, 100);
    gl::Rectangle result;

    //  +------+       +------+
    //  |   |  |       |      |
    //  +---+  |  -->  |      |
    //  |      |       |      |
    //  +------+       +------+
    //
    gl::ExtendRectangle(source, extend1, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    //  +------+           +------+
    //  |S     |           |      |
    //  |   +--+---+  -->  |      |
    //  |   |  |   |       |      |
    //  +---+--+   +       +------+
    //      |      |
    //      +------+
    //
    const gl::Rectangle extend2(50, 100, 100, 200);

    gl::ExtendRectangle(source, extend2, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    //    +------+           +------+
    //    |S     |           |      |
    //  +-+------+---+  -->  |      |
    //  | |      |   |       |      |
    //  | +------+   +       |      |
    //  |            |       |      |
    //  +------------+       +------+
    //
    const gl::Rectangle extend3(-10, 100, 200, 200);

    gl::ExtendRectangle(source, extend3, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 300);

    //    +------+           +------+
    //    |S     |           |      |
    //    |      |      -->  |      |
    //    |      |           |      |
    //  +-+------+---+       |      |
    //  |            |       |      |
    //  +------------+       +------+
    //
    for (int offsetLeft = 10; offsetLeft >= 0; offsetLeft -= 10)
    {
        for (int offsetRight = 10; offsetRight >= 0; offsetRight -= 10)
        {
            const gl::Rectangle extend4(-offsetLeft, 200, 100 + offsetLeft + offsetRight, 100);

            gl::ExtendRectangle(source, extend4, &result);
            ASSERT_EQ(result.x0(), 0) << offsetLeft << " " << offsetRight;
            ASSERT_EQ(result.y0(), 0) << offsetLeft << " " << offsetRight;
            ASSERT_EQ(result.x1(), 100) << offsetLeft << " " << offsetRight;
            ASSERT_EQ(result.y1(), 300) << offsetLeft << " " << offsetRight;
        }
    }

    // Similar to extend4, but with the second rectangle on the top, left and right.
    for (int offsetLeft = 10; offsetLeft >= 0; offsetLeft -= 10)
    {
        for (int offsetRight = 10; offsetRight >= 0; offsetRight -= 10)
        {
            const gl::Rectangle extend4(-offsetLeft, -100, 100 + offsetLeft + offsetRight, 100);

            gl::ExtendRectangle(source, extend4, &result);
            ASSERT_EQ(result.x0(), 0) << offsetLeft << " " << offsetRight;
            ASSERT_EQ(result.y0(), -100) << offsetLeft << " " << offsetRight;
            ASSERT_EQ(result.x1(), 100) << offsetLeft << " " << offsetRight;
            ASSERT_EQ(result.y1(), 200) << offsetLeft << " " << offsetRight;
        }
    }
    for (int offsetTop = 10; offsetTop >= 0; offsetTop -= 10)
    {
        for (int offsetBottom = 10; offsetBottom >= 0; offsetBottom -= 10)
        {
            const gl::Rectangle extend4(-50, -offsetTop, 50, 200 + offsetTop + offsetBottom);

            gl::ExtendRectangle(source, extend4, &result);
            ASSERT_EQ(result.x0(), -50) << offsetTop << " " << offsetBottom;
            ASSERT_EQ(result.y0(), 0) << offsetTop << " " << offsetBottom;
            ASSERT_EQ(result.x1(), 100) << offsetTop << " " << offsetBottom;
            ASSERT_EQ(result.y1(), 200) << offsetTop << " " << offsetBottom;
        }
    }
    for (int offsetTop = 10; offsetTop >= 0; offsetTop -= 10)
    {
        for (int offsetBottom = 10; offsetBottom >= 0; offsetBottom -= 10)
        {
            const gl::Rectangle extend4(100, -offsetTop, 50, 200 + offsetTop + offsetBottom);

            gl::ExtendRectangle(source, extend4, &result);
            ASSERT_EQ(result.x0(), 0) << offsetTop << " " << offsetBottom;
            ASSERT_EQ(result.y0(), 0) << offsetTop << " " << offsetBottom;
            ASSERT_EQ(result.x1(), 150) << offsetTop << " " << offsetBottom;
            ASSERT_EQ(result.y1(), 200) << offsetTop << " " << offsetBottom;
        }
    }

    //    +------+           +------+
    //    |S     |           |      |
    //    |      |      -->  |      |
    //    |      |           |      |
    //    +------+           +------+
    //  +------------+
    //  |            |
    //  +------------+
    //
    const gl::Rectangle extend5(-10, 201, 120, 100);

    gl::ExtendRectangle(source, extend5, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    // Similar to extend5, but with the second rectangle on the top, left and right.
    const gl::Rectangle extend6(-10, -101, 120, 100);

    gl::ExtendRectangle(source, extend6, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    const gl::Rectangle extend7(-101, -10, 100, 220);

    gl::ExtendRectangle(source, extend7, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    const gl::Rectangle extend8(101, -10, 100, 220);

    gl::ExtendRectangle(source, extend8, &result);
    ASSERT_EQ(result.x0(), 0);
    ASSERT_EQ(result.y0(), 0);
    ASSERT_EQ(result.x1(), 100);
    ASSERT_EQ(result.y1(), 200);

    //  +-------------+       +-------------+
    //  |   +------+  |       |             |
    //  |   |S     |  |       |             |
    //  |   |      |  |  -->  |             |
    //  |   |      |  |       |             |
    //  |   +------+  |       |             |
    //  +-------------+       +-------------+
    //
    const gl::Rectangle extend9(-100, -100, 300, 400);

    gl::ExtendRectangle(source, extend9, &result);
    ASSERT_EQ(result.x0(), -100);
    ASSERT_EQ(result.y0(), -100);
    ASSERT_EQ(result.x1(), 200);
    ASSERT_EQ(result.y1(), 300);
}

}  // namespace angle
