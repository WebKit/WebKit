#ifndef _ES3FASTCDECOMPRESSIONCASES_HPP
#define _ES3FASTCDECOMPRESSIONCASES_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief ASTC decompression tests
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "tes3TestCase.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuAstcUtil.hpp"
#include "deUniquePtr.hpp"

#include <vector>

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace ASTCDecompressionCaseInternal
{

class ASTCRenderer2D;

}

// General ASTC block test class.
class ASTCBlockCase2D : public TestCase
{
public:
    ASTCBlockCase2D(Context &context, const char *name, const char *description, tcu::astc::BlockTestType testType,
                    tcu::CompressedTexFormat format);
    ~ASTCBlockCase2D(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    ASTCBlockCase2D(const ASTCBlockCase2D &other);
    ASTCBlockCase2D &operator=(const ASTCBlockCase2D &other);

    const tcu::astc::BlockTestType m_testType;
    const tcu::CompressedTexFormat m_format;
    std::vector<uint8_t> m_blockData;

    int m_numBlocksTested;
    int m_currentIteration;

    de::UniquePtr<ASTCDecompressionCaseInternal::ASTCRenderer2D> m_renderer;
};

// For a format with block size (W, H), test with texture sizes {(k*W + a, k*H + b) | 0 <= a < W, 0 <= b < H } .
class ASTCBlockSizeRemainderCase2D : public TestCase
{
public:
    ASTCBlockSizeRemainderCase2D(Context &context, const char *name, const char *description,
                                 tcu::CompressedTexFormat format);
    ~ASTCBlockSizeRemainderCase2D(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    enum
    {
        MAX_NUM_BLOCKS_X = 5,
        MAX_NUM_BLOCKS_Y = 5
    };

    ASTCBlockSizeRemainderCase2D(const ASTCBlockSizeRemainderCase2D &other);
    ASTCBlockSizeRemainderCase2D &operator=(const ASTCBlockSizeRemainderCase2D &other);

    const tcu::CompressedTexFormat m_format;

    int m_currentIteration;

    de::UniquePtr<ASTCDecompressionCaseInternal::ASTCRenderer2D> m_renderer;
};

} // namespace Functional
} // namespace gles3
} // namespace deqp

#endif // _ES3FASTCDECOMPRESSIONCASES_HPP
