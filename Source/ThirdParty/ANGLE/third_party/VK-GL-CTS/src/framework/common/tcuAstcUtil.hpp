#ifndef _TCUASTCUTIL_HPP
#define _TCUASTCUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
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
 * \brief ASTC Utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuCompressedTexture.hpp"

#include <vector>

namespace tcu
{
namespace astc
{

enum BlockTestType
{
    BLOCK_TEST_TYPE_VOID_EXTENT_LDR = 0,
    BLOCK_TEST_TYPE_VOID_EXTENT_HDR,
    BLOCK_TEST_TYPE_WEIGHT_GRID,
    BLOCK_TEST_TYPE_WEIGHT_ISE,
    BLOCK_TEST_TYPE_CEMS,
    BLOCK_TEST_TYPE_PARTITION_SEED,
    BLOCK_TEST_TYPE_ENDPOINT_VALUE_LDR,
    BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15,
    BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15,
    BLOCK_TEST_TYPE_ENDPOINT_ISE,
    BLOCK_TEST_TYPE_CCS,
    BLOCK_TEST_TYPE_RANDOM,

    BLOCK_TEST_TYPE_LAST
};

enum
{
    BLOCK_SIZE_BYTES = 128 / 8,
};

const char *getBlockTestTypeName(BlockTestType testType);
const char *getBlockTestTypeDescription(BlockTestType testType);
bool isBlockTestTypeHDROnly(BlockTestType testType);
Vec4 getBlockTestTypeColorScale(BlockTestType testType);
Vec4 getBlockTestTypeColorBias(BlockTestType testType);

void generateBlockCaseTestData(std::vector<uint8_t> &dst, CompressedTexFormat format, BlockTestType testType);

void generateRandomBlocks(uint8_t *dst, size_t numBlocks, CompressedTexFormat format, uint32_t seed);
void generateRandomValidBlocks(uint8_t *dst, size_t numBlocks, CompressedTexFormat format,
                               TexDecompressionParams::AstcMode mode, uint32_t seed);

void generateDefaultVoidExtentBlocks(uint8_t *dst, size_t numBlocks);
void generateDefaultNormalBlocks(uint8_t *dst, size_t numBlocks, int blockWidth, int blockHeight, int blockDepth = 1);

bool isValidBlock(const uint8_t *data, CompressedTexFormat format, TexDecompressionParams::AstcMode mode);

void decompress(const PixelBufferAccess &dst, const uint8_t *data, CompressedTexFormat format,
                TexDecompressionParams::AstcMode mode);
void decompress3D(const PixelBufferAccess &dst, const uint8_t *data, CompressedTexFormat format,
                  TexDecompressionParams::AstcMode mode);

} // namespace astc
} // namespace tcu

#endif // _TCUASTCUTIL_HPP
