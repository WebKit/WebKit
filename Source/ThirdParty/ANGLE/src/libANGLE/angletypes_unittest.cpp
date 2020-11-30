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
        ASSERT_EQ(blendStateExt.mMaxEquationMask, is64Bit ? 0xFFu : 0xFu);
        ASSERT_EQ(blendStateExt.mMaxFactorMask, 0xFFu);
        checkInitState(blendStateExt);
    }

    {
        const gl::BlendStateExt blendStateExt = gl::BlendStateExt(4);
        ASSERT_EQ(blendStateExt.mMaxDrawBuffers, 4u);
        ASSERT_EQ(blendStateExt.mMaxEnabledMask.to_ulong(), 0xFu);
        ASSERT_EQ(blendStateExt.mMaxColorMask, is64Bit ? 0xFFFFFFFFu : 0xFFFFu);
        ASSERT_EQ(blendStateExt.mMaxEquationMask, is64Bit ? 0xFFFFFFFFu : 0xFFFFu);
        ASSERT_EQ(blendStateExt.mMaxFactorMask, 0xFFFFFFFFu);
        checkInitState(blendStateExt);
    }

    {
        const gl::BlendStateExt blendStateExt = gl::BlendStateExt(8);
        ASSERT_EQ(blendStateExt.mMaxDrawBuffers, 8u);
        ASSERT_EQ(blendStateExt.mMaxEnabledMask.to_ulong(), 0xFFu);
        ASSERT_EQ(blendStateExt.mMaxColorMask, is64Bit ? 0xFFFFFFFFFFFFFFFFu : 0xFFFFFFFFu);
        ASSERT_EQ(blendStateExt.mMaxEquationMask, is64Bit ? 0xFFFFFFFFFFFFFFFFu : 0xFFFFFFFFu);
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
    ASSERT_EQ(blendStateExt.mEquationColor, is64Bit ? 0x01010101010101u : 0x1111111u);
    ASSERT_EQ(blendStateExt.mEquationAlpha, is64Bit ? 0x04040404040404u : 0x4444444u);

    blendStateExt.setEquationsIndexed(3, GL_MAX, GL_FUNC_SUBTRACT);
    blendStateExt.setEquationsIndexed(5, GL_MIN, GL_FUNC_ADD);
    ASSERT_EQ(blendStateExt.mEquationColor, is64Bit ? 0x01010102010101u : 0x1112111u);
    ASSERT_EQ(blendStateExt.mEquationAlpha, is64Bit ? 0x04000404040404u : 0x4044444u);
    ASSERT_EQ(blendStateExt.getEquationColorIndexed(3), static_cast<GLenum>(GL_MAX));
    ASSERT_EQ(blendStateExt.getEquationAlphaIndexed(5), static_cast<GLenum>(GL_FUNC_ADD));

    gl::BlendStateExt::EquationStorage::Type otherEquationColor =
        blendStateExt.expandEquationColorIndexed(0);
    gl::BlendStateExt::EquationStorage::Type otherEquationAlpha =
        blendStateExt.expandEquationAlphaIndexed(0);

    ASSERT_EQ(otherEquationColor, is64Bit ? 0x01010101010101u : 0x1111111u);
    ASSERT_EQ(otherEquationAlpha, is64Bit ? 0x04040404040404u : 0x4444444u);

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

}  // namespace angle
