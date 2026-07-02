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

#include "tcuAstcUtil.hpp"
#include "deFloat16.h"
#include "deRandom.hpp"
#include "deMeta.hpp"

#include <algorithm>

namespace tcu
{
namespace astc
{

using std::vector;

namespace
{

// Common utilities

enum
{
    MAX_BLOCK_WIDTH     = 12,
    MAX_BLOCK_HEIGHT    = 12,
    MAX_BLOCK_WIDTH_3D  = 6,
    MAX_BLOCK_HEIGHT_3D = 6,
    MAX_BLOCK_DEPTH_3D  = 6,
};

inline uint32_t getBit(uint32_t src, int ndx)
{
    DE_ASSERT(de::inBounds(ndx, 0, 32));
    return (src >> ndx) & 1;
}

inline uint32_t getBits(uint32_t src, int low, int high)
{
    const int numBits = (high - low) + 1;

    DE_ASSERT(de::inRange(numBits, 1, 32));

    if (numBits < 32)
        return (uint32_t)((src >> low) & ((1u << numBits) - 1));
    else
        return (uint32_t)((src >> low) & 0xFFFFFFFFu);
}

inline bool isBitSet(uint32_t src, int ndx)
{
    return getBit(src, ndx) != 0;
}

inline uint32_t reverseBits(uint32_t src, int numBits)
{
    DE_ASSERT(de::inRange(numBits, 0, 32));
    uint32_t result = 0;
    for (int i = 0; i < numBits; i++)
        result |= ((src >> i) & 1) << (numBits - 1 - i);
    return result;
}

inline uint32_t bitReplicationScale(uint32_t src, int numSrcBits, int numDstBits)
{
    DE_ASSERT(numSrcBits <= numDstBits);
    DE_ASSERT((src & ((1 << numSrcBits) - 1)) == src);
    uint32_t dst = 0;
    for (int shift = numDstBits - numSrcBits; shift > -numSrcBits; shift -= numSrcBits)
        dst |= shift >= 0 ? src << shift : src >> -shift;
    return dst;
}

inline int32_t signExtend(int32_t src, int numSrcBits)
{
    DE_ASSERT(de::inRange(numSrcBits, 2, 31));
    const bool negative = (src & (1 << (numSrcBits - 1))) != 0;
    return src | (negative ? ~((1 << numSrcBits) - 1) : 0);
}

inline bool isFloat16InfOrNan(deFloat16 v)
{
    return getBits(v, 10, 14) == 31;
}

enum ISEMode
{
    ISEMODE_TRIT = 0,
    ISEMODE_QUINT,
    ISEMODE_PLAIN_BIT,

    ISEMODE_LAST
};

struct ISEParams
{
    ISEMode mode;
    int numBits;

    ISEParams(ISEMode mode_, int numBits_) : mode(mode_), numBits(numBits_)
    {
    }
};

inline int computeNumRequiredBits(const ISEParams &iseParams, int numValues)
{
    switch (iseParams.mode)
    {
    case ISEMODE_TRIT:
        return deDivRoundUp32(numValues * 8, 5) + numValues * iseParams.numBits;
    case ISEMODE_QUINT:
        return deDivRoundUp32(numValues * 7, 3) + numValues * iseParams.numBits;
    case ISEMODE_PLAIN_BIT:
        return numValues * iseParams.numBits;
    default:
        DE_ASSERT(false);
        return -1;
    }
}

ISEParams computeMaximumRangeISEParams(int numAvailableBits, int numValuesInSequence)
{
    int curBitsForTritMode     = 6;
    int curBitsForQuintMode    = 5;
    int curBitsForPlainBitMode = 8;

    while (true)
    {
        DE_ASSERT(curBitsForTritMode > 0 || curBitsForQuintMode > 0 || curBitsForPlainBitMode > 0);

        const int tritRange     = curBitsForTritMode > 0 ? (3 << curBitsForTritMode) - 1 : -1;
        const int quintRange    = curBitsForQuintMode > 0 ? (5 << curBitsForQuintMode) - 1 : -1;
        const int plainBitRange = curBitsForPlainBitMode > 0 ? (1 << curBitsForPlainBitMode) - 1 : -1;
        const int maxRange      = de::max(de::max(tritRange, quintRange), plainBitRange);

        if (maxRange == tritRange)
        {
            const ISEParams params(ISEMODE_TRIT, curBitsForTritMode);
            if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
                return ISEParams(ISEMODE_TRIT, curBitsForTritMode);
            curBitsForTritMode--;
        }
        else if (maxRange == quintRange)
        {
            const ISEParams params(ISEMODE_QUINT, curBitsForQuintMode);
            if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
                return ISEParams(ISEMODE_QUINT, curBitsForQuintMode);
            curBitsForQuintMode--;
        }
        else
        {
            const ISEParams params(ISEMODE_PLAIN_BIT, curBitsForPlainBitMode);
            DE_ASSERT(maxRange == plainBitRange);
            if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
                return ISEParams(ISEMODE_PLAIN_BIT, curBitsForPlainBitMode);
            curBitsForPlainBitMode--;
        }
    }
}

inline int computeNumColorEndpointValues(uint32_t endpointMode)
{
    DE_ASSERT(endpointMode < 16);
    return (endpointMode / 4 + 1) * 2;
}

// Decompression utilities

enum DecompressResult
{
    DECOMPRESS_RESULT_VALID_BLOCK = 0, //!< Decompressed valid block
    DECOMPRESS_RESULT_ERROR,           //!< Encountered error while decompressing, error color written

    DECOMPRESS_RESULT_LAST
};

// A helper for getting bits from a 128-bit block.
class Block128
{
private:
    typedef uint64_t Word;

    enum
    {
        WORD_BYTES = sizeof(Word),
        WORD_BITS  = 8 * WORD_BYTES,
        NUM_WORDS  = 128 / WORD_BITS
    };

    DE_STATIC_ASSERT(128 % WORD_BITS == 0);

public:
    Block128(const uint8_t *src)
    {
        for (int wordNdx = 0; wordNdx < NUM_WORDS; wordNdx++)
        {
            m_words[wordNdx] = 0;
            for (int byteNdx = 0; byteNdx < WORD_BYTES; byteNdx++)
                m_words[wordNdx] |= (Word)src[wordNdx * WORD_BYTES + byteNdx] << (8 * byteNdx);
        }
    }

    uint32_t getBit(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, 128));
        return (m_words[ndx / WORD_BITS] >> (ndx % WORD_BITS)) & 1;
    }

    uint32_t getBits(int low, int high) const
    {
        DE_ASSERT(de::inBounds(low, 0, 128));
        DE_ASSERT(de::inBounds(high, 0, 128));
        DE_ASSERT(de::inRange(high - low + 1, 0, 32));

        if (high - low + 1 == 0)
            return 0;

        const int word0Ndx = low / WORD_BITS;
        const int word1Ndx = high / WORD_BITS;

        // \note "foo << bar << 1" done instead of "foo << (bar+1)" to avoid overflow, i.e. shift amount being too big.

        if (word0Ndx == word1Ndx)
            return (uint32_t)((m_words[word0Ndx] & ((((Word)1 << high % WORD_BITS << 1) - 1))) >>
                              ((Word)low % WORD_BITS));
        else
        {
            DE_ASSERT(word1Ndx == word0Ndx + 1);

            return (uint32_t)(m_words[word0Ndx] >> (low % WORD_BITS)) |
                   (uint32_t)((m_words[word1Ndx] & (((Word)1 << high % WORD_BITS << 1) - 1))
                              << (high - low - high % WORD_BITS));
        }
    }

    bool isBitSet(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, 128));
        return getBit(ndx) != 0;
    }

private:
    Word m_words[NUM_WORDS];
};

// A helper for sequential access into a Block128.
class BitAccessStream
{
public:
    BitAccessStream(const Block128 &src, int startNdxInSrc, int length, bool forward)
        : m_src(src)
        , m_startNdxInSrc(startNdxInSrc)
        , m_length(length)
        , m_forward(forward)
        , m_ndx(0)
    {
    }

    // Get the next num bits. Bits at positions greater than or equal to m_length are zeros.
    uint32_t getNext(int num)
    {
        if (num == 0 || m_ndx >= m_length)
            return 0;

        const int end            = m_ndx + num;
        const int numBitsFromSrc = de::max(0, de::min(m_length, end) - m_ndx);
        const int low            = m_ndx;
        const int high           = m_ndx + numBitsFromSrc - 1;

        m_ndx += num;

        return m_forward ? m_src.getBits(m_startNdxInSrc + low, m_startNdxInSrc + high) :
                           reverseBits(m_src.getBits(m_startNdxInSrc - high, m_startNdxInSrc - low), numBitsFromSrc);
    }

private:
    const Block128 &m_src;
    const int m_startNdxInSrc;
    const int m_length;
    const bool m_forward;

    int m_ndx;
};

struct ISEDecodedResult
{
    uint32_t m;
    uint32_t tq; //!< Trit or quint value, depending on ISE mode.
    uint32_t v;
};

// Data from an ASTC block's "block mode" part (i.e. bits [0,10]).
struct ASTCBlockMode
{
    bool isError;
    // \note Following fields only relevant if !isError.
    bool isVoidExtent;
    // \note Following fields only relevant if !isVoidExtent.
    bool isDualPlane;
    int weightGridWidth;
    int weightGridHeight;
    int weightGridDepth;
    ISEParams weightISEParams;

    ASTCBlockMode(void)
        : isError(true)
        , isVoidExtent(true)
        , isDualPlane(true)
        , weightGridWidth(-1)
        , weightGridHeight(-1)
        , weightGridDepth(-1)
        , weightISEParams(ISEMODE_LAST, -1)
    {
    }
};

inline int computeNumWeights(const ASTCBlockMode &mode)
{
    return mode.weightGridWidth * mode.weightGridHeight * mode.weightGridDepth * (mode.isDualPlane ? 2 : 1);
}

struct ColorEndpointPair
{
    UVec4 e0;
    UVec4 e1;
};

struct TexelWeightPair
{
    uint32_t w[2];
};

ASTCBlockMode getASTCBlockMode(uint32_t blockModeData)
{
    ASTCBlockMode blockMode;
    blockMode.weightGridDepth = 1;
    blockMode.isError         = true; // \note Set to false later, if not error.

    blockMode.isVoidExtent = getBits(blockModeData, 0, 8) == 0x1fc;

    if (!blockMode.isVoidExtent)
    {
        if ((getBits(blockModeData, 0, 1) == 0 && getBits(blockModeData, 6, 8) == 7) ||
            getBits(blockModeData, 0, 3) == 0)
            return blockMode; // Invalid ("reserved").

        uint32_t r = (uint32_t)-1; // \note Set in the following branches.

        if (getBits(blockModeData, 0, 1) == 0)
        {
            const uint32_t r0  = getBit(blockModeData, 4);
            const uint32_t r1  = getBit(blockModeData, 2);
            const uint32_t r2  = getBit(blockModeData, 3);
            const uint32_t i78 = getBits(blockModeData, 7, 8);

            r = (r2 << 2) | (r1 << 1) | (r0 << 0);

            if (i78 == 3)
            {
                const bool i5              = isBitSet(blockModeData, 5);
                blockMode.weightGridWidth  = i5 ? 10 : 6;
                blockMode.weightGridHeight = i5 ? 6 : 10;
            }
            else
            {
                const uint32_t a = getBits(blockModeData, 5, 6);
                switch (i78)
                {
                case 0:
                    blockMode.weightGridWidth  = 12;
                    blockMode.weightGridHeight = a + 2;
                    break;
                case 1:
                    blockMode.weightGridWidth  = a + 2;
                    blockMode.weightGridHeight = 12;
                    break;
                case 2:
                    blockMode.weightGridWidth  = a + 6;
                    blockMode.weightGridHeight = getBits(blockModeData, 9, 10) + 6;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
        else
        {
            const uint32_t r0  = getBit(blockModeData, 4);
            const uint32_t r1  = getBit(blockModeData, 0);
            const uint32_t r2  = getBit(blockModeData, 1);
            const uint32_t i23 = getBits(blockModeData, 2, 3);
            const uint32_t a   = getBits(blockModeData, 5, 6);

            r = (r2 << 2) | (r1 << 1) | (r0 << 0);

            if (i23 == 3)
            {
                const uint32_t b           = getBit(blockModeData, 7);
                const bool i8              = isBitSet(blockModeData, 8);
                blockMode.weightGridWidth  = i8 ? b + 2 : a + 2;
                blockMode.weightGridHeight = i8 ? a + 2 : b + 6;
            }
            else
            {
                const uint32_t b = getBits(blockModeData, 7, 8);

                switch (i23)
                {
                case 0:
                    blockMode.weightGridWidth  = b + 4;
                    blockMode.weightGridHeight = a + 2;
                    break;
                case 1:
                    blockMode.weightGridWidth  = b + 8;
                    blockMode.weightGridHeight = a + 2;
                    break;
                case 2:
                    blockMode.weightGridWidth  = a + 2;
                    blockMode.weightGridHeight = b + 8;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }

        const bool zeroDH     = getBits(blockModeData, 0, 1) == 0 && getBits(blockModeData, 7, 8) == 2;
        const bool h          = zeroDH ? 0 : isBitSet(blockModeData, 9);
        blockMode.isDualPlane = zeroDH ? 0 : isBitSet(blockModeData, 10);

        {
            ISEMode &m = blockMode.weightISEParams.mode;
            int &b     = blockMode.weightISEParams.numBits;
            m          = ISEMODE_PLAIN_BIT;
            b          = 0;

            if (h)
            {
                switch (r)
                {
                case 2:
                    m = ISEMODE_QUINT;
                    b = 1;
                    break;
                case 3:
                    m = ISEMODE_TRIT;
                    b = 2;
                    break;
                case 4:
                    b = 4;
                    break;
                case 5:
                    m = ISEMODE_QUINT;
                    b = 2;
                    break;
                case 6:
                    m = ISEMODE_TRIT;
                    b = 3;
                    break;
                case 7:
                    b = 5;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else
            {
                switch (r)
                {
                case 2:
                    b = 1;
                    break;
                case 3:
                    m = ISEMODE_TRIT;
                    break;
                case 4:
                    b = 2;
                    break;
                case 5:
                    m = ISEMODE_QUINT;
                    break;
                case 6:
                    m = ISEMODE_TRIT;
                    b = 1;
                    break;
                case 7:
                    b = 3;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
    }

    blockMode.isError = false;
    return blockMode;
}

ASTCBlockMode getASTCBlockMode3D(uint32_t blockModeData)
{
    ASTCBlockMode blockMode;
    blockMode.isError = true; // \note Set to false later, if not error.

    blockMode.isVoidExtent = getBits(blockModeData, 0, 8) == 0x1fc;

    if (!blockMode.isVoidExtent)
    {
        if ((getBits(blockModeData, 0, 1) == 0 && getBits(blockModeData, 5, 8) == 7) ||
            getBits(blockModeData, 0, 3) == 0)
            return blockMode; // Invalid ("reserved").

        uint32_t r = (uint32_t)-1; // \note Set in the following branches.

        if (getBits(blockModeData, 0, 1) == 0)
        {
            const uint32_t r0  = getBit(blockModeData, 4);
            const uint32_t r1  = getBit(blockModeData, 2);
            const uint32_t r2  = getBit(blockModeData, 3);
            const uint32_t i78 = getBits(blockModeData, 7, 8);

            r = (r2 << 2) | (r1 << 1) | (r0 << 0);

            if (i78 == 3)
            {
                const bool i5              = isBitSet(blockModeData, 5);
                const bool i6              = isBitSet(blockModeData, 6);
                blockMode.weightGridWidth  = i5 ? 2 : i6 ? 2 : 6;
                blockMode.weightGridHeight = i5 ? 6 : i6 ? 2 : 2;
                blockMode.weightGridDepth  = i5 ? 2 : i6 ? 6 : 2;
            }
            else
            {
                const uint32_t a = getBits(blockModeData, 9, 10);
                const uint32_t b = getBits(blockModeData, 5, 6);
                switch (i78)
                {
                case 0:
                    blockMode.weightGridWidth  = 6;
                    blockMode.weightGridHeight = a + 2;
                    blockMode.weightGridDepth  = b + 2;
                    break;
                case 1:
                    blockMode.weightGridWidth  = b + 2;
                    blockMode.weightGridHeight = 6;
                    blockMode.weightGridDepth  = a + 2;
                    break;
                case 2:
                    blockMode.weightGridWidth  = b + 2;
                    blockMode.weightGridHeight = a + 2;
                    blockMode.weightGridDepth  = 6;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
        else
        {
            const uint32_t r0 = getBit(blockModeData, 4);
            const uint32_t r1 = getBit(blockModeData, 0);
            const uint32_t r2 = getBit(blockModeData, 1);

            r = (r2 << 2) | (r1 << 1) | (r0 << 0);

            blockMode.weightGridWidth  = getBits(blockModeData, 5, 6) + 2;
            blockMode.weightGridHeight = getBits(blockModeData, 7, 8) + 2;
            blockMode.weightGridDepth  = getBits(blockModeData, 2, 3) + 2;
        }

        const bool zeroDH     = getBits(blockModeData, 0, 1) == 0 && getBits(blockModeData, 7, 8) != 3;
        const bool h          = zeroDH ? 0 : isBitSet(blockModeData, 9);
        blockMode.isDualPlane = zeroDH ? 0 : isBitSet(blockModeData, 10);

        {
            ISEMode &m = blockMode.weightISEParams.mode;
            int &b     = blockMode.weightISEParams.numBits;
            m          = ISEMODE_PLAIN_BIT;
            b          = 0;

            if (h)
            {
                switch (r)
                {
                case 2:
                    m = ISEMODE_QUINT;
                    b = 1;
                    break;
                case 3:
                    m = ISEMODE_TRIT;
                    b = 2;
                    break;
                case 4:
                    b = 4;
                    break;
                case 5:
                    m = ISEMODE_QUINT;
                    b = 2;
                    break;
                case 6:
                    m = ISEMODE_TRIT;
                    b = 3;
                    break;
                case 7:
                    b = 5;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
            else
            {
                switch (r)
                {
                case 2:
                    b = 1;
                    break;
                case 3:
                    m = ISEMODE_TRIT;
                    break;
                case 4:
                    b = 2;
                    break;
                case 5:
                    m = ISEMODE_QUINT;
                    break;
                case 6:
                    m = ISEMODE_TRIT;
                    b = 1;
                    break;
                case 7:
                    b = 3;
                    break;
                default:
                    DE_ASSERT(false);
                }
            }
        }
    }

    blockMode.isError = false;
    return blockMode;
}

inline void setASTCErrorColorBlock(void *dst, int blockWidth, int blockHeight, int blockDepth, bool isSRGB)
{
    if (isSRGB)
    {
        uint8_t *const dstU = (uint8_t *)dst;

        for (int i = 0; i < blockWidth * blockHeight * blockDepth; i++)
        {
            dstU[4 * i + 0] = 0xff;
            dstU[4 * i + 1] = 0;
            dstU[4 * i + 2] = 0xff;
            dstU[4 * i + 3] = 0xff;
        }
    }
    else
    {
        float *const dstF = (float *)dst;

        for (int i = 0; i < blockWidth * blockHeight * blockDepth; i++)
        {
            dstF[4 * i + 0] = 1.0f;
            dstF[4 * i + 1] = 0.0f;
            dstF[4 * i + 2] = 1.0f;
            dstF[4 * i + 3] = 1.0f;
        }
    }
}

DecompressResult decodeVoidExtentBlock(void *dst, const Block128 &blockData, int blockWidth, int blockHeight,
                                       int blockDepth, bool isSRGB, bool isLDRMode)
{
    const uint32_t minSExtent = blockData.getBits(12, 24);
    const uint32_t maxSExtent = blockData.getBits(25, 37);
    const uint32_t minTExtent = blockData.getBits(38, 50);
    const uint32_t maxTExtent = blockData.getBits(51, 63);
    const bool allExtentsAllOnes =
        minSExtent == 0x1fff && maxSExtent == 0x1fff && minTExtent == 0x1fff && maxTExtent == 0x1fff;
    const bool isHDRBlock = blockData.isBitSet(9);

    if ((isLDRMode && isHDRBlock) || (!allExtentsAllOnes && (minSExtent >= maxSExtent || minTExtent >= maxTExtent)))
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, blockDepth, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }

    const uint32_t rgba[4] = {blockData.getBits(64, 79), blockData.getBits(80, 95), blockData.getBits(96, 111),
                              blockData.getBits(112, 127)};

    if (isSRGB)
    {
        uint8_t *const dstU = (uint8_t *)dst;
        for (int i = 0; i < blockWidth * blockHeight * blockDepth; i++)
            for (int c = 0; c < 4; c++)
                dstU[i * 4 + c] = (uint8_t)((rgba[c] & 0xff00) >> 8);
    }
    else
    {
        float *const dstF = (float *)dst;

        if (isHDRBlock)
        {
            for (int c = 0; c < 4; c++)
            {
                if (isFloat16InfOrNan((deFloat16)rgba[c]))
                    throw InternalError("Infinity or NaN color component in HDR void extent block in ASTC texture "
                                        "(behavior undefined by ASTC specification)");
            }

            for (int i = 0; i < blockWidth * blockHeight * blockDepth; i++)
                for (int c = 0; c < 4; c++)
                    dstF[i * 4 + c] = deFloat16To32((deFloat16)rgba[c]);
        }
        else
        {
            for (int i = 0; i < blockWidth * blockHeight * blockDepth; i++)
                for (int c = 0; c < 4; c++)
                    dstF[i * 4 + c] = rgba[c] == 65535 ? 1.0f : (float)rgba[c] / 65536.0f;
        }
    }

    return DECOMPRESS_RESULT_VALID_BLOCK;
}

void decodeColorEndpointModes(uint32_t *endpointModesDst, const Block128 &blockData, int numPartitions,
                              int extraCemBitsStart)
{
    if (numPartitions == 1)
        endpointModesDst[0] = blockData.getBits(13, 16);
    else
    {
        const uint32_t highLevelSelector = blockData.getBits(23, 24);

        if (highLevelSelector == 0)
        {
            const uint32_t mode = blockData.getBits(25, 28);
            for (int i = 0; i < numPartitions; i++)
                endpointModesDst[i] = mode;
        }
        else
        {
            for (int partNdx = 0; partNdx < numPartitions; partNdx++)
            {
                const uint32_t cemClass   = highLevelSelector - (blockData.isBitSet(25 + partNdx) ? 0 : 1);
                const uint32_t lowBit0Ndx = numPartitions + 2 * partNdx;
                const uint32_t lowBit1Ndx = numPartitions + 2 * partNdx + 1;
                const uint32_t lowBit0 =
                    blockData.getBit(lowBit0Ndx < 4 ? 25 + lowBit0Ndx : extraCemBitsStart + lowBit0Ndx - 4);
                const uint32_t lowBit1 =
                    blockData.getBit(lowBit1Ndx < 4 ? 25 + lowBit1Ndx : extraCemBitsStart + lowBit1Ndx - 4);

                endpointModesDst[partNdx] = (cemClass << 2) | (lowBit1 << 1) | lowBit0;
            }
        }
    }
}

int computeNumColorEndpointValues(const uint32_t *endpointModes, int numPartitions)
{
    int result = 0;
    for (int i = 0; i < numPartitions; i++)
        result += computeNumColorEndpointValues(endpointModes[i]);
    return result;
}

void decodeISETritBlock(ISEDecodedResult *dst, int numValues, BitAccessStream &data, int numBits)
{
    DE_ASSERT(de::inRange(numValues, 1, 5));

    uint32_t m[5];

    m[0]         = data.getNext(numBits);
    uint32_t T01 = data.getNext(2);
    m[1]         = data.getNext(numBits);
    uint32_t T23 = data.getNext(2);
    m[2]         = data.getNext(numBits);
    uint32_t T4  = data.getNext(1);
    m[3]         = data.getNext(numBits);
    uint32_t T56 = data.getNext(2);
    m[4]         = data.getNext(numBits);
    uint32_t T7  = data.getNext(1);

    switch (numValues)
    {
    case 1:
        T23 = 0;
    // Fallthrough
    case 2:
        T4 = 0;
    // Fallthrough
    case 3:
        T56 = 0;
    // Fallthrough
    case 4:
        T7 = 0;
    // Fallthrough
    case 5:
        break;
    default:
        DE_ASSERT(false);
    }

    const uint32_t T = (T7 << 7) | (T56 << 5) | (T4 << 4) | (T23 << 2) | (T01 << 0);

    static const uint32_t tritsFromT[256][5] = {
        {0, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {2, 0, 0, 0, 0}, {0, 0, 2, 0, 0}, {0, 1, 0, 0, 0}, {1, 1, 0, 0, 0},
        {2, 1, 0, 0, 0}, {1, 0, 2, 0, 0}, {0, 2, 0, 0, 0}, {1, 2, 0, 0, 0}, {2, 2, 0, 0, 0}, {2, 0, 2, 0, 0},
        {0, 2, 2, 0, 0}, {1, 2, 2, 0, 0}, {2, 2, 2, 0, 0}, {2, 0, 2, 0, 0}, {0, 0, 1, 0, 0}, {1, 0, 1, 0, 0},
        {2, 0, 1, 0, 0}, {0, 1, 2, 0, 0}, {0, 1, 1, 0, 0}, {1, 1, 1, 0, 0}, {2, 1, 1, 0, 0}, {1, 1, 2, 0, 0},
        {0, 2, 1, 0, 0}, {1, 2, 1, 0, 0}, {2, 2, 1, 0, 0}, {2, 1, 2, 0, 0}, {0, 0, 0, 2, 2}, {1, 0, 0, 2, 2},
        {2, 0, 0, 2, 2}, {0, 0, 2, 2, 2}, {0, 0, 0, 1, 0}, {1, 0, 0, 1, 0}, {2, 0, 0, 1, 0}, {0, 0, 2, 1, 0},
        {0, 1, 0, 1, 0}, {1, 1, 0, 1, 0}, {2, 1, 0, 1, 0}, {1, 0, 2, 1, 0}, {0, 2, 0, 1, 0}, {1, 2, 0, 1, 0},
        {2, 2, 0, 1, 0}, {2, 0, 2, 1, 0}, {0, 2, 2, 1, 0}, {1, 2, 2, 1, 0}, {2, 2, 2, 1, 0}, {2, 0, 2, 1, 0},
        {0, 0, 1, 1, 0}, {1, 0, 1, 1, 0}, {2, 0, 1, 1, 0}, {0, 1, 2, 1, 0}, {0, 1, 1, 1, 0}, {1, 1, 1, 1, 0},
        {2, 1, 1, 1, 0}, {1, 1, 2, 1, 0}, {0, 2, 1, 1, 0}, {1, 2, 1, 1, 0}, {2, 2, 1, 1, 0}, {2, 1, 2, 1, 0},
        {0, 1, 0, 2, 2}, {1, 1, 0, 2, 2}, {2, 1, 0, 2, 2}, {1, 0, 2, 2, 2}, {0, 0, 0, 2, 0}, {1, 0, 0, 2, 0},
        {2, 0, 0, 2, 0}, {0, 0, 2, 2, 0}, {0, 1, 0, 2, 0}, {1, 1, 0, 2, 0}, {2, 1, 0, 2, 0}, {1, 0, 2, 2, 0},
        {0, 2, 0, 2, 0}, {1, 2, 0, 2, 0}, {2, 2, 0, 2, 0}, {2, 0, 2, 2, 0}, {0, 2, 2, 2, 0}, {1, 2, 2, 2, 0},
        {2, 2, 2, 2, 0}, {2, 0, 2, 2, 0}, {0, 0, 1, 2, 0}, {1, 0, 1, 2, 0}, {2, 0, 1, 2, 0}, {0, 1, 2, 2, 0},
        {0, 1, 1, 2, 0}, {1, 1, 1, 2, 0}, {2, 1, 1, 2, 0}, {1, 1, 2, 2, 0}, {0, 2, 1, 2, 0}, {1, 2, 1, 2, 0},
        {2, 2, 1, 2, 0}, {2, 1, 2, 2, 0}, {0, 2, 0, 2, 2}, {1, 2, 0, 2, 2}, {2, 2, 0, 2, 2}, {2, 0, 2, 2, 2},
        {0, 0, 0, 0, 2}, {1, 0, 0, 0, 2}, {2, 0, 0, 0, 2}, {0, 0, 2, 0, 2}, {0, 1, 0, 0, 2}, {1, 1, 0, 0, 2},
        {2, 1, 0, 0, 2}, {1, 0, 2, 0, 2}, {0, 2, 0, 0, 2}, {1, 2, 0, 0, 2}, {2, 2, 0, 0, 2}, {2, 0, 2, 0, 2},
        {0, 2, 2, 0, 2}, {1, 2, 2, 0, 2}, {2, 2, 2, 0, 2}, {2, 0, 2, 0, 2}, {0, 0, 1, 0, 2}, {1, 0, 1, 0, 2},
        {2, 0, 1, 0, 2}, {0, 1, 2, 0, 2}, {0, 1, 1, 0, 2}, {1, 1, 1, 0, 2}, {2, 1, 1, 0, 2}, {1, 1, 2, 0, 2},
        {0, 2, 1, 0, 2}, {1, 2, 1, 0, 2}, {2, 2, 1, 0, 2}, {2, 1, 2, 0, 2}, {0, 2, 2, 2, 2}, {1, 2, 2, 2, 2},
        {2, 2, 2, 2, 2}, {2, 0, 2, 2, 2}, {0, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {2, 0, 0, 0, 1}, {0, 0, 2, 0, 1},
        {0, 1, 0, 0, 1}, {1, 1, 0, 0, 1}, {2, 1, 0, 0, 1}, {1, 0, 2, 0, 1}, {0, 2, 0, 0, 1}, {1, 2, 0, 0, 1},
        {2, 2, 0, 0, 1}, {2, 0, 2, 0, 1}, {0, 2, 2, 0, 1}, {1, 2, 2, 0, 1}, {2, 2, 2, 0, 1}, {2, 0, 2, 0, 1},
        {0, 0, 1, 0, 1}, {1, 0, 1, 0, 1}, {2, 0, 1, 0, 1}, {0, 1, 2, 0, 1}, {0, 1, 1, 0, 1}, {1, 1, 1, 0, 1},
        {2, 1, 1, 0, 1}, {1, 1, 2, 0, 1}, {0, 2, 1, 0, 1}, {1, 2, 1, 0, 1}, {2, 2, 1, 0, 1}, {2, 1, 2, 0, 1},
        {0, 0, 1, 2, 2}, {1, 0, 1, 2, 2}, {2, 0, 1, 2, 2}, {0, 1, 2, 2, 2}, {0, 0, 0, 1, 1}, {1, 0, 0, 1, 1},
        {2, 0, 0, 1, 1}, {0, 0, 2, 1, 1}, {0, 1, 0, 1, 1}, {1, 1, 0, 1, 1}, {2, 1, 0, 1, 1}, {1, 0, 2, 1, 1},
        {0, 2, 0, 1, 1}, {1, 2, 0, 1, 1}, {2, 2, 0, 1, 1}, {2, 0, 2, 1, 1}, {0, 2, 2, 1, 1}, {1, 2, 2, 1, 1},
        {2, 2, 2, 1, 1}, {2, 0, 2, 1, 1}, {0, 0, 1, 1, 1}, {1, 0, 1, 1, 1}, {2, 0, 1, 1, 1}, {0, 1, 2, 1, 1},
        {0, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {2, 1, 1, 1, 1}, {1, 1, 2, 1, 1}, {0, 2, 1, 1, 1}, {1, 2, 1, 1, 1},
        {2, 2, 1, 1, 1}, {2, 1, 2, 1, 1}, {0, 1, 1, 2, 2}, {1, 1, 1, 2, 2}, {2, 1, 1, 2, 2}, {1, 1, 2, 2, 2},
        {0, 0, 0, 2, 1}, {1, 0, 0, 2, 1}, {2, 0, 0, 2, 1}, {0, 0, 2, 2, 1}, {0, 1, 0, 2, 1}, {1, 1, 0, 2, 1},
        {2, 1, 0, 2, 1}, {1, 0, 2, 2, 1}, {0, 2, 0, 2, 1}, {1, 2, 0, 2, 1}, {2, 2, 0, 2, 1}, {2, 0, 2, 2, 1},
        {0, 2, 2, 2, 1}, {1, 2, 2, 2, 1}, {2, 2, 2, 2, 1}, {2, 0, 2, 2, 1}, {0, 0, 1, 2, 1}, {1, 0, 1, 2, 1},
        {2, 0, 1, 2, 1}, {0, 1, 2, 2, 1}, {0, 1, 1, 2, 1}, {1, 1, 1, 2, 1}, {2, 1, 1, 2, 1}, {1, 1, 2, 2, 1},
        {0, 2, 1, 2, 1}, {1, 2, 1, 2, 1}, {2, 2, 1, 2, 1}, {2, 1, 2, 2, 1}, {0, 2, 1, 2, 2}, {1, 2, 1, 2, 2},
        {2, 2, 1, 2, 2}, {2, 1, 2, 2, 2}, {0, 0, 0, 1, 2}, {1, 0, 0, 1, 2}, {2, 0, 0, 1, 2}, {0, 0, 2, 1, 2},
        {0, 1, 0, 1, 2}, {1, 1, 0, 1, 2}, {2, 1, 0, 1, 2}, {1, 0, 2, 1, 2}, {0, 2, 0, 1, 2}, {1, 2, 0, 1, 2},
        {2, 2, 0, 1, 2}, {2, 0, 2, 1, 2}, {0, 2, 2, 1, 2}, {1, 2, 2, 1, 2}, {2, 2, 2, 1, 2}, {2, 0, 2, 1, 2},
        {0, 0, 1, 1, 2}, {1, 0, 1, 1, 2}, {2, 0, 1, 1, 2}, {0, 1, 2, 1, 2}, {0, 1, 1, 1, 2}, {1, 1, 1, 1, 2},
        {2, 1, 1, 1, 2}, {1, 1, 2, 1, 2}, {0, 2, 1, 1, 2}, {1, 2, 1, 1, 2}, {2, 2, 1, 1, 2}, {2, 1, 2, 1, 2},
        {0, 2, 2, 2, 2}, {1, 2, 2, 2, 2}, {2, 2, 2, 2, 2}, {2, 1, 2, 2, 2}};

    const uint32_t(&trits)[5] = tritsFromT[T];

    for (int i = 0; i < numValues; i++)
    {
        dst[i].m  = m[i];
        dst[i].tq = trits[i];
        dst[i].v  = (trits[i] << numBits) + m[i];
    }
}

void decodeISEQuintBlock(ISEDecodedResult *dst, int numValues, BitAccessStream &data, int numBits)
{
    DE_ASSERT(de::inRange(numValues, 1, 3));

    uint32_t m[3];

    m[0]          = data.getNext(numBits);
    uint32_t Q012 = data.getNext(3);
    m[1]          = data.getNext(numBits);
    uint32_t Q34  = data.getNext(2);
    m[2]          = data.getNext(numBits);
    uint32_t Q56  = data.getNext(2);

    switch (numValues)
    {
    case 1:
        Q34 = 0;
    // Fallthrough
    case 2:
        Q56 = 0;
    // Fallthrough
    case 3:
        break;
    default:
        DE_ASSERT(false);
    }

    const uint32_t Q = (Q56 << 5) | (Q34 << 3) | (Q012 << 0);

    static const uint32_t quintsFromQ[256][3] = {
        {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}, {0, 4, 0}, {4, 4, 0}, {4, 4, 4}, {0, 1, 0}, {1, 1, 0},
        {2, 1, 0}, {3, 1, 0}, {4, 1, 0}, {1, 4, 0}, {4, 4, 1}, {4, 4, 4}, {0, 2, 0}, {1, 2, 0}, {2, 2, 0}, {3, 2, 0},
        {4, 2, 0}, {2, 4, 0}, {4, 4, 2}, {4, 4, 4}, {0, 3, 0}, {1, 3, 0}, {2, 3, 0}, {3, 3, 0}, {4, 3, 0}, {3, 4, 0},
        {4, 4, 3}, {4, 4, 4}, {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {4, 0, 1}, {0, 4, 1}, {4, 0, 4}, {0, 4, 4},
        {0, 1, 1}, {1, 1, 1}, {2, 1, 1}, {3, 1, 1}, {4, 1, 1}, {1, 4, 1}, {4, 1, 4}, {1, 4, 4}, {0, 2, 1}, {1, 2, 1},
        {2, 2, 1}, {3, 2, 1}, {4, 2, 1}, {2, 4, 1}, {4, 2, 4}, {2, 4, 4}, {0, 3, 1}, {1, 3, 1}, {2, 3, 1}, {3, 3, 1},
        {4, 3, 1}, {3, 4, 1}, {4, 3, 4}, {3, 4, 4}, {0, 0, 2}, {1, 0, 2}, {2, 0, 2}, {3, 0, 2}, {4, 0, 2}, {0, 4, 2},
        {2, 0, 4}, {3, 0, 4}, {0, 1, 2}, {1, 1, 2}, {2, 1, 2}, {3, 1, 2}, {4, 1, 2}, {1, 4, 2}, {2, 1, 4}, {3, 1, 4},
        {0, 2, 2}, {1, 2, 2}, {2, 2, 2}, {3, 2, 2}, {4, 2, 2}, {2, 4, 2}, {2, 2, 4}, {3, 2, 4}, {0, 3, 2}, {1, 3, 2},
        {2, 3, 2}, {3, 3, 2}, {4, 3, 2}, {3, 4, 2}, {2, 3, 4}, {3, 3, 4}, {0, 0, 3}, {1, 0, 3}, {2, 0, 3}, {3, 0, 3},
        {4, 0, 3}, {0, 4, 3}, {0, 0, 4}, {1, 0, 4}, {0, 1, 3}, {1, 1, 3}, {2, 1, 3}, {3, 1, 3}, {4, 1, 3}, {1, 4, 3},
        {0, 1, 4}, {1, 1, 4}, {0, 2, 3}, {1, 2, 3}, {2, 2, 3}, {3, 2, 3}, {4, 2, 3}, {2, 4, 3}, {0, 2, 4}, {1, 2, 4},
        {0, 3, 3}, {1, 3, 3}, {2, 3, 3}, {3, 3, 3}, {4, 3, 3}, {3, 4, 3}, {0, 3, 4}, {1, 3, 4}};

    const uint32_t(&quints)[3] = quintsFromQ[Q];

    for (int i = 0; i < numValues; i++)
    {
        dst[i].m  = m[i];
        dst[i].tq = quints[i];
        dst[i].v  = (quints[i] << numBits) + m[i];
    }
}

inline void decodeISEBitBlock(ISEDecodedResult *dst, BitAccessStream &data, int numBits)
{
    dst[0].m = data.getNext(numBits);
    dst[0].v = dst[0].m;
}

void decodeISE(ISEDecodedResult *dst, int numValues, BitAccessStream &data, const ISEParams &params)
{
    if (params.mode == ISEMODE_TRIT)
    {
        const int numBlocks = deDivRoundUp32(numValues, 5);
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const int numValuesInBlock = blockNdx == numBlocks - 1 ? numValues - 5 * (numBlocks - 1) : 5;
            decodeISETritBlock(&dst[5 * blockNdx], numValuesInBlock, data, params.numBits);
        }
    }
    else if (params.mode == ISEMODE_QUINT)
    {
        const int numBlocks = deDivRoundUp32(numValues, 3);
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const int numValuesInBlock = blockNdx == numBlocks - 1 ? numValues - 3 * (numBlocks - 1) : 3;
            decodeISEQuintBlock(&dst[3 * blockNdx], numValuesInBlock, data, params.numBits);
        }
    }
    else
    {
        DE_ASSERT(params.mode == ISEMODE_PLAIN_BIT);
        for (int i = 0; i < numValues; i++)
            decodeISEBitBlock(&dst[i], data, params.numBits);
    }
}

void unquantizeColorEndpoints(uint32_t *dst, const ISEDecodedResult *iseResults, int numEndpoints,
                              const ISEParams &iseParams)
{
    if (iseParams.mode == ISEMODE_TRIT || iseParams.mode == ISEMODE_QUINT)
    {
        const int rangeCase = iseParams.numBits * 2 - (iseParams.mode == ISEMODE_TRIT ? 2 : 1);
        DE_ASSERT(de::inRange(rangeCase, 0, 10));
        static const uint32_t Ca[11] = {204, 113, 93, 54, 44, 26, 22, 13, 11, 6, 5};
        const uint32_t C             = Ca[rangeCase];

        for (int endpointNdx = 0; endpointNdx < numEndpoints; endpointNdx++)
        {
            const uint32_t a = getBit(iseResults[endpointNdx].m, 0);
            const uint32_t b = getBit(iseResults[endpointNdx].m, 1);
            const uint32_t c = getBit(iseResults[endpointNdx].m, 2);
            const uint32_t d = getBit(iseResults[endpointNdx].m, 3);
            const uint32_t e = getBit(iseResults[endpointNdx].m, 4);
            const uint32_t f = getBit(iseResults[endpointNdx].m, 5);

            const uint32_t A = a == 0 ? 0 : (1 << 9) - 1;
            const uint32_t B = rangeCase == 0  ? 0 :
                               rangeCase == 1  ? 0 :
                               rangeCase == 2  ? (b << 8) | (b << 4) | (b << 2) | (b << 1) :
                               rangeCase == 3  ? (b << 8) | (b << 3) | (b << 2) :
                               rangeCase == 4  ? (c << 8) | (b << 7) | (c << 3) | (b << 2) | (c << 1) | (b << 0) :
                               rangeCase == 5  ? (c << 8) | (b << 7) | (c << 2) | (b << 1) | (c << 0) :
                               rangeCase == 6  ? (d << 8) | (c << 7) | (b << 6) | (d << 2) | (c << 1) | (b << 0) :
                               rangeCase == 7  ? (d << 8) | (c << 7) | (b << 6) | (d << 1) | (c << 0) :
                               rangeCase == 8  ? (e << 8) | (d << 7) | (c << 6) | (b << 5) | (e << 1) | (d << 0) :
                               rangeCase == 9  ? (e << 8) | (d << 7) | (c << 6) | (b << 5) | (e << 0) :
                               rangeCase == 10 ? (f << 8) | (e << 7) | (d << 6) | (c << 5) | (b << 4) | (f << 0) :
                                                 (uint32_t)-1;
            DE_ASSERT(B != (uint32_t)-1);

            dst[endpointNdx] = (((iseResults[endpointNdx].tq * C + B) ^ A) >> 2) | (A & 0x80);
        }
    }
    else
    {
        DE_ASSERT(iseParams.mode == ISEMODE_PLAIN_BIT);

        for (int endpointNdx = 0; endpointNdx < numEndpoints; endpointNdx++)
            dst[endpointNdx] = bitReplicationScale(iseResults[endpointNdx].v, iseParams.numBits, 8);
    }
}

inline void bitTransferSigned(int32_t &a, int32_t &b)
{
    b >>= 1;
    b |= a & 0x80;
    a >>= 1;
    a &= 0x3f;
    if (isBitSet(a, 5))
        a -= 0x40;
}

inline UVec4 clampedRGBA(const IVec4 &rgba)
{
    return UVec4(de::clamp(rgba.x(), 0, 0xff), de::clamp(rgba.y(), 0, 0xff), de::clamp(rgba.z(), 0, 0xff),
                 de::clamp(rgba.w(), 0, 0xff));
}

inline IVec4 blueContract(int r, int g, int b, int a)
{
    return IVec4((r + b) >> 1, (g + b) >> 1, b, a);
}

inline bool isColorEndpointModeHDR(uint32_t mode)
{
    return mode == 2 || mode == 3 || mode == 7 || mode == 11 || mode == 14 || mode == 15;
}

void decodeHDREndpointMode7(UVec4 &e0, UVec4 &e1, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
{
    const uint32_t m10     = getBit(v1, 7) | (getBit(v2, 7) << 1);
    const uint32_t m23     = getBits(v0, 6, 7);
    const uint32_t majComp = m10 != 3 ? m10 : m23 != 3 ? m23 : 0;
    const uint32_t mode    = m10 != 3 ? m23 : m23 != 3 ? 4 : 5;

    int32_t red   = (int32_t)getBits(v0, 0, 5);
    int32_t green = (int32_t)getBits(v1, 0, 4);
    int32_t blue  = (int32_t)getBits(v2, 0, 4);
    int32_t scale = (int32_t)getBits(v3, 0, 4);

    {
#define SHOR(DST_VAR, SHIFT, BIT_VAR) (DST_VAR) |= (BIT_VAR) << (SHIFT)
#define ASSIGN_X_BITS(V0, S0, V1, S1, V2, S2, V3, S3, V4, S4, V5, S5, V6, S6) \
    do                                                                        \
    {                                                                         \
        SHOR(V0, S0, x0);                                                     \
        SHOR(V1, S1, x1);                                                     \
        SHOR(V2, S2, x2);                                                     \
        SHOR(V3, S3, x3);                                                     \
        SHOR(V4, S4, x4);                                                     \
        SHOR(V5, S5, x5);                                                     \
        SHOR(V6, S6, x6);                                                     \
    } while (false)

        const uint32_t x0 = getBit(v1, 6);
        const uint32_t x1 = getBit(v1, 5);
        const uint32_t x2 = getBit(v2, 6);
        const uint32_t x3 = getBit(v2, 5);
        const uint32_t x4 = getBit(v3, 7);
        const uint32_t x5 = getBit(v3, 6);
        const uint32_t x6 = getBit(v3, 5);

        int32_t &R = red;
        int32_t &G = green;
        int32_t &B = blue;
        int32_t &S = scale;

        switch (mode)
        {
        case 0:
            ASSIGN_X_BITS(R, 9, R, 8, R, 7, R, 10, R, 6, S, 6, S, 5);
            break;
        case 1:
            ASSIGN_X_BITS(R, 8, G, 5, R, 7, B, 5, R, 6, R, 10, R, 9);
            break;
        case 2:
            ASSIGN_X_BITS(R, 9, R, 8, R, 7, R, 6, S, 7, S, 6, S, 5);
            break;
        case 3:
            ASSIGN_X_BITS(R, 8, G, 5, R, 7, B, 5, R, 6, S, 6, S, 5);
            break;
        case 4:
            ASSIGN_X_BITS(G, 6, G, 5, B, 6, B, 5, R, 6, R, 7, S, 5);
            break;
        case 5:
            ASSIGN_X_BITS(G, 6, G, 5, B, 6, B, 5, R, 6, S, 6, S, 5);
            break;
        default:
            DE_ASSERT(false);
        }

#undef ASSIGN_X_BITS
#undef SHOR
    }

    static const int shiftAmounts[] = {1, 1, 2, 3, 4, 5};
    DE_ASSERT(mode < DE_LENGTH_OF_ARRAY(shiftAmounts));

    red <<= shiftAmounts[mode];
    green <<= shiftAmounts[mode];
    blue <<= shiftAmounts[mode];
    scale <<= shiftAmounts[mode];

    if (mode != 5)
    {
        green = red - green;
        blue  = red - blue;
    }

    if (majComp == 1)
        std::swap(red, green);
    else if (majComp == 2)
        std::swap(red, blue);

    e0 = UVec4(de::clamp(red - scale, 0, 0xfff), de::clamp(green - scale, 0, 0xfff), de::clamp(blue - scale, 0, 0xfff),
               0x780);

    e1 = UVec4(de::clamp(red, 0, 0xfff), de::clamp(green, 0, 0xfff), de::clamp(blue, 0, 0xfff), 0x780);
}

void decodeHDREndpointMode11(UVec4 &e0, UVec4 &e1, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4,
                             uint32_t v5)
{
    const uint32_t major = (getBit(v5, 7) << 1) | getBit(v4, 7);

    if (major == 3)
    {
        e0 = UVec4(v0 << 4, v2 << 4, getBits(v4, 0, 6) << 5, 0x780);
        e1 = UVec4(v1 << 4, v3 << 4, getBits(v5, 0, 6) << 5, 0x780);
    }
    else
    {
        const uint32_t mode = (getBit(v3, 7) << 2) | (getBit(v2, 7) << 1) | getBit(v1, 7);

        int32_t a  = (int32_t)((getBit(v1, 6) << 8) | v0);
        int32_t c  = (int32_t)(getBits(v1, 0, 5));
        int32_t b0 = (int32_t)(getBits(v2, 0, 5));
        int32_t b1 = (int32_t)(getBits(v3, 0, 5));
        int32_t d0 = (int32_t)(getBits(v4, 0, 4));
        int32_t d1 = (int32_t)(getBits(v5, 0, 4));

        {
#define SHOR(DST_VAR, SHIFT, BIT_VAR) (DST_VAR) |= (BIT_VAR) << (SHIFT)
#define ASSIGN_X_BITS(V0, S0, V1, S1, V2, S2, V3, S3, V4, S4, V5, S5) \
    do                                                                \
    {                                                                 \
        SHOR(V0, S0, x0);                                             \
        SHOR(V1, S1, x1);                                             \
        SHOR(V2, S2, x2);                                             \
        SHOR(V3, S3, x3);                                             \
        SHOR(V4, S4, x4);                                             \
        SHOR(V5, S5, x5);                                             \
    } while (false)

            const uint32_t x0 = getBit(v2, 6);
            const uint32_t x1 = getBit(v3, 6);
            const uint32_t x2 = getBit(v4, 6);
            const uint32_t x3 = getBit(v5, 6);
            const uint32_t x4 = getBit(v4, 5);
            const uint32_t x5 = getBit(v5, 5);

            switch (mode)
            {
            case 0:
                ASSIGN_X_BITS(b0, 6, b1, 6, d0, 6, d1, 6, d0, 5, d1, 5);
                break;
            case 1:
                ASSIGN_X_BITS(b0, 6, b1, 6, b0, 7, b1, 7, d0, 5, d1, 5);
                break;
            case 2:
                ASSIGN_X_BITS(a, 9, c, 6, d0, 6, d1, 6, d0, 5, d1, 5);
                break;
            case 3:
                ASSIGN_X_BITS(b0, 6, b1, 6, a, 9, c, 6, d0, 5, d1, 5);
                break;
            case 4:
                ASSIGN_X_BITS(b0, 6, b1, 6, b0, 7, b1, 7, a, 9, a, 10);
                break;
            case 5:
                ASSIGN_X_BITS(a, 9, a, 10, c, 7, c, 6, d0, 5, d1, 5);
                break;
            case 6:
                ASSIGN_X_BITS(b0, 6, b1, 6, a, 11, c, 6, a, 9, a, 10);
                break;
            case 7:
                ASSIGN_X_BITS(a, 9, a, 10, a, 11, c, 6, d0, 5, d1, 5);
                break;
            default:
                DE_ASSERT(false);
            }

#undef ASSIGN_X_BITS
#undef SHOR
        }

        static const int numDBits[] = {7, 6, 7, 6, 5, 6, 5, 6};
        DE_ASSERT(mode < DE_LENGTH_OF_ARRAY(numDBits));

        d0 = signExtend(d0, numDBits[mode]);
        d1 = signExtend(d1, numDBits[mode]);

        const int shiftAmount = (mode >> 1) ^ 3;
        a <<= shiftAmount;
        c <<= shiftAmount;
        b0 <<= shiftAmount;
        b1 <<= shiftAmount;
        d0 <<= shiftAmount;
        d1 <<= shiftAmount;

        e0 = UVec4(de::clamp(a - c, 0, 0xfff), de::clamp(a - b0 - c - d0, 0, 0xfff),
                   de::clamp(a - b1 - c - d1, 0, 0xfff), 0x780);

        e1 = UVec4(de::clamp(a, 0, 0xfff), de::clamp(a - b0, 0, 0xfff), de::clamp(a - b1, 0, 0xfff), 0x780);

        if (major == 1)
        {
            std::swap(e0.x(), e0.y());
            std::swap(e1.x(), e1.y());
        }
        else if (major == 2)
        {
            std::swap(e0.x(), e0.z());
            std::swap(e1.x(), e1.z());
        }
    }
}

void decodeHDREndpointMode15(UVec4 &e0, UVec4 &e1, uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4,
                             uint32_t v5, uint32_t v6In, uint32_t v7In)
{
    decodeHDREndpointMode11(e0, e1, v0, v1, v2, v3, v4, v5);

    const uint32_t mode = (getBit(v7In, 7) << 1) | getBit(v6In, 7);
    int32_t v6          = (int32_t)getBits(v6In, 0, 6);
    int32_t v7          = (int32_t)getBits(v7In, 0, 6);

    if (mode == 3)
    {
        e0.w() = v6 << 5;
        e1.w() = v7 << 5;
    }
    else
    {
        v6 |= (v7 << (mode + 1)) & 0x780;
        v7 &= (0x3f >> mode);
        v7 ^= 0x20 >> mode;
        v7 -= 0x20 >> mode;
        v6 <<= 4 - mode;
        v7 <<= 4 - mode;

        v7 += v6;
        v7     = de::clamp(v7, 0, 0xfff);
        e0.w() = v6;
        e1.w() = v7;
    }
}

void decodeColorEndpoints(ColorEndpointPair *dst, const uint32_t *unquantizedEndpoints, const uint32_t *endpointModes,
                          int numPartitions)
{
    int unquantizedNdx = 0;

    for (int partitionNdx = 0; partitionNdx < numPartitions; partitionNdx++)
    {
        const uint32_t endpointMode = endpointModes[partitionNdx];
        const uint32_t *v           = &unquantizedEndpoints[unquantizedNdx];
        UVec4 &e0                   = dst[partitionNdx].e0;
        UVec4 &e1                   = dst[partitionNdx].e1;

        unquantizedNdx += computeNumColorEndpointValues(endpointMode);

        switch (endpointMode)
        {
        case 0:
            e0 = UVec4(v[0], v[0], v[0], 0xff);
            e1 = UVec4(v[1], v[1], v[1], 0xff);
            break;

        case 1:
        {
            const uint32_t L0 = (v[0] >> 2) | (getBits(v[1], 6, 7) << 6);
            const uint32_t L1 = de::min(0xffu, L0 + getBits(v[1], 0, 5));
            e0                = UVec4(L0, L0, L0, 0xff);
            e1                = UVec4(L1, L1, L1, 0xff);
            break;
        }

        case 2:
        {
            const uint32_t v1Gr = v[1] >= v[0];
            const uint32_t y0   = v1Gr ? v[0] << 4 : (v[1] << 4) + 8;
            const uint32_t y1   = v1Gr ? v[1] << 4 : (v[0] << 4) - 8;

            e0 = UVec4(y0, y0, y0, 0x780);
            e1 = UVec4(y1, y1, y1, 0x780);
            break;
        }

        case 3:
        {
            const bool m      = isBitSet(v[0], 7);
            const uint32_t y0 = m ? (getBits(v[1], 5, 7) << 9) | (getBits(v[0], 0, 6) << 2) :
                                    (getBits(v[1], 4, 7) << 8) | (getBits(v[0], 0, 6) << 1);
            const uint32_t d  = m ? getBits(v[1], 0, 4) << 2 : getBits(v[1], 0, 3) << 1;
            const uint32_t y1 = de::min(0xfffu, y0 + d);

            e0 = UVec4(y0, y0, y0, 0x780);
            e1 = UVec4(y1, y1, y1, 0x780);
            break;
        }

        case 4:
            e0 = UVec4(v[0], v[0], v[0], v[2]);
            e1 = UVec4(v[1], v[1], v[1], v[3]);
            break;

        case 5:
        {
            int32_t v0 = (int32_t)v[0];
            int32_t v1 = (int32_t)v[1];
            int32_t v2 = (int32_t)v[2];
            int32_t v3 = (int32_t)v[3];
            bitTransferSigned(v1, v0);
            bitTransferSigned(v3, v2);

            e0 = clampedRGBA(IVec4(v0, v0, v0, v2));
            e1 = clampedRGBA(IVec4(v0 + v1, v0 + v1, v0 + v1, v2 + v3));
            break;
        }

        case 6:
            e0 = UVec4((v[0] * v[3]) >> 8, (v[1] * v[3]) >> 8, (v[2] * v[3]) >> 8, 0xff);
            e1 = UVec4(v[0], v[1], v[2], 0xff);
            break;

        case 7:
            decodeHDREndpointMode7(e0, e1, v[0], v[1], v[2], v[3]);
            break;

        case 8:
            if (v[1] + v[3] + v[5] >= v[0] + v[2] + v[4])
            {
                e0 = UVec4(v[0], v[2], v[4], 0xff);
                e1 = UVec4(v[1], v[3], v[5], 0xff);
            }
            else
            {
                e0 = blueContract(v[1], v[3], v[5], 0xff).asUint();
                e1 = blueContract(v[0], v[2], v[4], 0xff).asUint();
            }
            break;

        case 9:
        {
            int32_t v0 = (int32_t)v[0];
            int32_t v1 = (int32_t)v[1];
            int32_t v2 = (int32_t)v[2];
            int32_t v3 = (int32_t)v[3];
            int32_t v4 = (int32_t)v[4];
            int32_t v5 = (int32_t)v[5];
            bitTransferSigned(v1, v0);
            bitTransferSigned(v3, v2);
            bitTransferSigned(v5, v4);

            if (v1 + v3 + v5 >= 0)
            {
                e0 = clampedRGBA(IVec4(v0, v2, v4, 0xff));
                e1 = clampedRGBA(IVec4(v0 + v1, v2 + v3, v4 + v5, 0xff));
            }
            else
            {
                e0 = clampedRGBA(blueContract(v0 + v1, v2 + v3, v4 + v5, 0xff));
                e1 = clampedRGBA(blueContract(v0, v2, v4, 0xff));
            }
            break;
        }

        case 10:
            e0 = UVec4((v[0] * v[3]) >> 8, (v[1] * v[3]) >> 8, (v[2] * v[3]) >> 8, v[4]);
            e1 = UVec4(v[0], v[1], v[2], v[5]);
            break;

        case 11:
            decodeHDREndpointMode11(e0, e1, v[0], v[1], v[2], v[3], v[4], v[5]);
            break;

        case 12:
            if (v[1] + v[3] + v[5] >= v[0] + v[2] + v[4])
            {
                e0 = UVec4(v[0], v[2], v[4], v[6]);
                e1 = UVec4(v[1], v[3], v[5], v[7]);
            }
            else
            {
                e0 = clampedRGBA(blueContract(v[1], v[3], v[5], v[7]));
                e1 = clampedRGBA(blueContract(v[0], v[2], v[4], v[6]));
            }
            break;

        case 13:
        {
            int32_t v0 = (int32_t)v[0];
            int32_t v1 = (int32_t)v[1];
            int32_t v2 = (int32_t)v[2];
            int32_t v3 = (int32_t)v[3];
            int32_t v4 = (int32_t)v[4];
            int32_t v5 = (int32_t)v[5];
            int32_t v6 = (int32_t)v[6];
            int32_t v7 = (int32_t)v[7];
            bitTransferSigned(v1, v0);
            bitTransferSigned(v3, v2);
            bitTransferSigned(v5, v4);
            bitTransferSigned(v7, v6);

            if (v1 + v3 + v5 >= 0)
            {
                e0 = clampedRGBA(IVec4(v0, v2, v4, v6));
                e1 = clampedRGBA(IVec4(v0 + v1, v2 + v3, v4 + v5, v6 + v7));
            }
            else
            {
                e0 = clampedRGBA(blueContract(v0 + v1, v2 + v3, v4 + v5, v6 + v7));
                e1 = clampedRGBA(blueContract(v0, v2, v4, v6));
            }

            break;
        }

        case 14:
            decodeHDREndpointMode11(e0, e1, v[0], v[1], v[2], v[3], v[4], v[5]);
            e0.w() = v[6];
            e1.w() = v[7];
            break;

        case 15:
            decodeHDREndpointMode15(e0, e1, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
            break;

        default:
            DE_ASSERT(false);
        }
    }
}

void computeColorEndpoints(ColorEndpointPair *dst, const Block128 &blockData, const uint32_t *endpointModes,
                           int numPartitions, int numColorEndpointValues, const ISEParams &iseParams,
                           int numBitsAvailable)
{
    const int colorEndpointDataStart = numPartitions == 1 ? 17 : 29;
    ISEDecodedResult colorEndpointData[18];

    {
        BitAccessStream dataStream(blockData, colorEndpointDataStart, numBitsAvailable, true);
        decodeISE(&colorEndpointData[0], numColorEndpointValues, dataStream, iseParams);
    }

    {
        uint32_t unquantizedEndpoints[18];
        unquantizeColorEndpoints(&unquantizedEndpoints[0], &colorEndpointData[0], numColorEndpointValues, iseParams);
        decodeColorEndpoints(dst, &unquantizedEndpoints[0], &endpointModes[0], numPartitions);
    }
}

void unquantizeWeights(uint32_t dst[64], const ISEDecodedResult *weightGrid, const ASTCBlockMode &blockMode)
{
    const int numWeights       = computeNumWeights(blockMode);
    const ISEParams &iseParams = blockMode.weightISEParams;

    if (iseParams.mode == ISEMODE_TRIT || iseParams.mode == ISEMODE_QUINT)
    {
        const int rangeCase = iseParams.numBits * 2 + (iseParams.mode == ISEMODE_QUINT ? 1 : 0);

        if (rangeCase == 0 || rangeCase == 1)
        {
            static const uint32_t map0[3] = {0, 32, 63};
            static const uint32_t map1[5] = {0, 16, 32, 47, 63};
            const uint32_t *const map     = rangeCase == 0 ? &map0[0] : &map1[0];
            for (int i = 0; i < numWeights; i++)
            {
                DE_ASSERT(weightGrid[i].v < (rangeCase == 0 ? 3u : 5u));
                dst[i] = map[weightGrid[i].v];
            }
        }
        else
        {
            DE_ASSERT(rangeCase <= 6);
            static const uint32_t Ca[5] = {50, 28, 23, 13, 11};
            const uint32_t C            = Ca[rangeCase - 2];

            for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
            {
                const uint32_t a = getBit(weightGrid[weightNdx].m, 0);
                const uint32_t b = getBit(weightGrid[weightNdx].m, 1);
                const uint32_t c = getBit(weightGrid[weightNdx].m, 2);

                const uint32_t A = a == 0 ? 0 : (1 << 7) - 1;
                const uint32_t B = rangeCase == 2 ? 0 :
                                   rangeCase == 3 ? 0 :
                                   rangeCase == 4 ? (b << 6) | (b << 2) | (b << 0) :
                                   rangeCase == 5 ? (b << 6) | (b << 1) :
                                   rangeCase == 6 ? (c << 6) | (b << 5) | (c << 1) | (b << 0) :
                                                    (uint32_t)-1;

                dst[weightNdx] = (((weightGrid[weightNdx].tq * C + B) ^ A) >> 2) | (A & 0x20);
            }
        }
    }
    else
    {
        DE_ASSERT(iseParams.mode == ISEMODE_PLAIN_BIT);

        for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
            dst[weightNdx] = bitReplicationScale(weightGrid[weightNdx].v, iseParams.numBits, 6);
    }

    for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
        dst[weightNdx] += dst[weightNdx] > 32 ? 1 : 0;

    // Initialize nonexistent weights to poison values
    for (int weightNdx = numWeights; weightNdx < 64; weightNdx++)
        dst[weightNdx] = ~0u;
}

void interpolateWeights(TexelWeightPair *dst, const uint32_t (&unquantizedWeights)[64], int blockWidth, int blockHeight,
                        const ASTCBlockMode &blockMode)
{
    const int numWeightsPerTexel = blockMode.isDualPlane ? 2 : 1;
    const uint32_t scaleX        = (1024 + blockWidth / 2) / (blockWidth - 1);
    const uint32_t scaleY        = (1024 + blockHeight / 2) / (blockHeight - 1);

    DE_ASSERT(blockMode.weightGridWidth * blockMode.weightGridHeight * numWeightsPerTexel <=
              DE_LENGTH_OF_ARRAY(unquantizedWeights));

    for (int texelY = 0; texelY < blockHeight; texelY++)
    {
        for (int texelX = 0; texelX < blockWidth; texelX++)
        {
            const uint32_t gX = (scaleX * texelX * (blockMode.weightGridWidth - 1) + 32) >> 6;
            const uint32_t gY = (scaleY * texelY * (blockMode.weightGridHeight - 1) + 32) >> 6;
            const uint32_t jX = gX >> 4;
            const uint32_t jY = gY >> 4;
            const uint32_t fX = gX & 0xf;
            const uint32_t fY = gY & 0xf;

            const uint32_t w11 = (fX * fY + 8) >> 4;
            const uint32_t w10 = fY - w11;
            const uint32_t w01 = fX - w11;
            const uint32_t w00 = 16 - fX - fY + w11;

            const uint32_t i00 = jY * blockMode.weightGridWidth + jX;
            const uint32_t i01 = i00 + 1;
            const uint32_t i10 = i00 + blockMode.weightGridWidth;
            const uint32_t i11 = i00 + blockMode.weightGridWidth + 1;

            // These addresses can be out of bounds, but respective weights will be 0 then.
            DE_ASSERT(deInBounds32(i00, 0, blockMode.weightGridWidth * blockMode.weightGridHeight) || w00 == 0);
            DE_ASSERT(deInBounds32(i01, 0, blockMode.weightGridWidth * blockMode.weightGridHeight) || w01 == 0);
            DE_ASSERT(deInBounds32(i10, 0, blockMode.weightGridWidth * blockMode.weightGridHeight) || w10 == 0);
            DE_ASSERT(deInBounds32(i11, 0, blockMode.weightGridWidth * blockMode.weightGridHeight) || w11 == 0);

            for (int texelWeightNdx = 0; texelWeightNdx < numWeightsPerTexel; texelWeightNdx++)
            {
                // & 0x3f clamps address to bounds of unquantizedWeights
                const uint32_t p00 = unquantizedWeights[(i00 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                const uint32_t p01 = unquantizedWeights[(i01 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                const uint32_t p10 = unquantizedWeights[(i10 * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                const uint32_t p11 = unquantizedWeights[(i11 * numWeightsPerTexel + texelWeightNdx) & 0x3f];

                dst[texelY * blockWidth + texelX].w[texelWeightNdx] =
                    (p00 * w00 + p01 * w01 + p10 * w10 + p11 * w11 + 8) >> 4;
            }
        }
    }
}

void interpolateWeights3D(TexelWeightPair *dst, const uint32_t (&unquantizedWeights)[64], int blockWidth,
                          int blockHeight, int blockDepth, const ASTCBlockMode &blockMode)
{
    const int numWeightsPerTexel = blockMode.isDualPlane ? 2 : 1;
    const uint32_t scaleX        = (1024 + blockWidth / 2) / (blockWidth - 1);
    const uint32_t scaleY        = (1024 + blockHeight / 2) / (blockHeight - 1);
    const uint32_t scaleZ        = (1024 + blockDepth / 2) / (blockDepth - 1);

    DE_ASSERT(blockMode.weightGridWidth * blockMode.weightGridHeight * blockMode.weightGridDepth * numWeightsPerTexel <=
              DE_LENGTH_OF_ARRAY(unquantizedWeights));

    for (int texelZ = 0; texelZ < blockDepth; texelZ++)
    {
        for (int texelY = 0; texelY < blockHeight; texelY++)
        {
            for (int texelX = 0; texelX < blockWidth; texelX++)
            {
                const uint32_t gX = (scaleX * texelX * (blockMode.weightGridWidth - 1) + 32) >> 6;
                const uint32_t gY = (scaleY * texelY * (blockMode.weightGridHeight - 1) + 32) >> 6;
                const uint32_t gZ = (scaleZ * texelZ * (blockMode.weightGridDepth - 1) + 32) >> 6;
                const uint32_t jX = gX >> 4;
                const uint32_t jY = gY >> 4;
                const uint32_t jZ = gZ >> 4;
                const uint32_t fX = gX & 0xf;
                const uint32_t fY = gY & 0xf;
                const uint32_t fZ = gZ & 0xf;

                int qweight[4];
                qweight[0] = (jZ * blockMode.weightGridHeight + jY) * blockMode.weightGridWidth + jX;
                qweight[3] = ((jZ + 1) * blockMode.weightGridHeight + (jY + 1)) * blockMode.weightGridWidth + (jX + 1);

                // simplex interpolation

                int cas = ((fX > fY) << 2) + ((fY > fZ) << 1) + ((fX > fZ));
                int N   = blockMode.weightGridWidth;
                int NM  = blockMode.weightGridWidth * blockMode.weightGridHeight;

                int s1, s2, w0, w1, w2, w3;
                switch (cas)
                {
                case 7:
                    s1 = 1;
                    s2 = N;
                    w0 = 16 - fX;
                    w1 = fX - fY;
                    w2 = fY - fZ;
                    w3 = fZ;
                    break;
                case 3:
                    s1 = N;
                    s2 = 1;
                    w0 = 16 - fY;
                    w1 = fY - fX;
                    w2 = fX - fZ;
                    w3 = fZ;
                    break;
                case 5:
                    s1 = 1;
                    s2 = NM;
                    w0 = 16 - fX;
                    w1 = fX - fZ;
                    w2 = fZ - fY;
                    w3 = fY;
                    break;
                case 4:
                    s1 = NM;
                    s2 = 1;
                    w0 = 16 - fZ;
                    w1 = fZ - fX;
                    w2 = fX - fY;
                    w3 = fY;
                    break;
                case 2:
                    s1 = N;
                    s2 = NM;
                    w0 = 16 - fY;
                    w1 = fY - fZ;
                    w2 = fZ - fX;
                    w3 = fX;
                    break;
                case 0:
                    s1 = NM;
                    s2 = N;
                    w0 = 16 - fZ;
                    w1 = fZ - fY;
                    w2 = fY - fX;
                    w3 = fX;
                    break;
                default:
                    s1 = NM;
                    s2 = N;
                    w0 = 16 - fZ;
                    w1 = fZ - fY;
                    w2 = fY - fX;
                    w3 = fX;
                    break;
                }

                qweight[1] = qweight[0] + s1;
                qweight[2] = qweight[1] + s2;

                for (int texelWeightNdx = 0; texelWeightNdx < numWeightsPerTexel; texelWeightNdx++)
                {
                    // & 0x3f clamps address to bounds of unquantizedWeights
                    const uint32_t p00 = unquantizedWeights[(qweight[0] * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                    const uint32_t p01 = unquantizedWeights[(qweight[1] * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                    const uint32_t p10 = unquantizedWeights[(qweight[2] * numWeightsPerTexel + texelWeightNdx) & 0x3f];
                    const uint32_t p11 = unquantizedWeights[(qweight[3] * numWeightsPerTexel + texelWeightNdx) & 0x3f];

                    dst[texelZ * blockHeight * blockWidth + texelY * blockWidth + texelX].w[texelWeightNdx] =
                        (p00 * w0 + p01 * w1 + p10 * w2 + p11 * w3 + 8) >> 4;
                }
            }
        }
    }
}

void computeTexelWeights(TexelWeightPair *dst, const Block128 &blockData, int blockWidth, int blockHeight,
                         int blockDepth, const ASTCBlockMode &blockMode)
{
    ISEDecodedResult weightGrid[64];

    {
        BitAccessStream dataStream(
            blockData, 127, computeNumRequiredBits(blockMode.weightISEParams, computeNumWeights(blockMode)), false);
        decodeISE(&weightGrid[0], computeNumWeights(blockMode), dataStream, blockMode.weightISEParams);
    }

    {
        uint32_t unquantizedWeights[64];
        unquantizeWeights(&unquantizedWeights[0], &weightGrid[0], blockMode);
        if (blockDepth > 1)
        {
            interpolateWeights3D(dst, unquantizedWeights, blockWidth, blockHeight, blockDepth, blockMode);
        }
        else
        {
            interpolateWeights(dst, unquantizedWeights, blockWidth, blockHeight, blockMode);
        }
    }
}

inline uint32_t hash52(uint32_t v)
{
    uint32_t p = v;
    p ^= p >> 15;
    p -= p << 17;
    p += p << 7;
    p += p << 4;
    p ^= p >> 5;
    p += p << 16;
    p ^= p >> 7;
    p ^= p >> 3;
    p ^= p << 6;
    p ^= p >> 17;
    return p;
}

int computeTexelPartition(uint32_t seedIn, uint32_t xIn, uint32_t yIn, uint32_t zIn, int numPartitions, bool smallBlock)
{
    const uint32_t x    = smallBlock ? xIn << 1 : xIn;
    const uint32_t y    = smallBlock ? yIn << 1 : yIn;
    const uint32_t z    = smallBlock ? zIn << 1 : zIn;
    const uint32_t seed = seedIn + 1024 * (numPartitions - 1);
    const uint32_t rnum = hash52(seed);
    uint8_t seed1       = (uint8_t)(rnum & 0xf);
    uint8_t seed2       = (uint8_t)((rnum >> 4) & 0xf);
    uint8_t seed3       = (uint8_t)((rnum >> 8) & 0xf);
    uint8_t seed4       = (uint8_t)((rnum >> 12) & 0xf);
    uint8_t seed5       = (uint8_t)((rnum >> 16) & 0xf);
    uint8_t seed6       = (uint8_t)((rnum >> 20) & 0xf);
    uint8_t seed7       = (uint8_t)((rnum >> 24) & 0xf);
    uint8_t seed8       = (uint8_t)((rnum >> 28) & 0xf);
    uint8_t seed9       = (uint8_t)((rnum >> 18) & 0xf);
    uint8_t seed10      = (uint8_t)((rnum >> 22) & 0xf);
    uint8_t seed11      = (uint8_t)((rnum >> 26) & 0xf);
    uint8_t seed12      = (uint8_t)(((rnum >> 30) | (rnum << 2)) & 0xf);

    seed1  = (uint8_t)(seed1 * seed1);
    seed2  = (uint8_t)(seed2 * seed2);
    seed3  = (uint8_t)(seed3 * seed3);
    seed4  = (uint8_t)(seed4 * seed4);
    seed5  = (uint8_t)(seed5 * seed5);
    seed6  = (uint8_t)(seed6 * seed6);
    seed7  = (uint8_t)(seed7 * seed7);
    seed8  = (uint8_t)(seed8 * seed8);
    seed9  = (uint8_t)(seed9 * seed9);
    seed10 = (uint8_t)(seed10 * seed10);
    seed11 = (uint8_t)(seed11 * seed11);
    seed12 = (uint8_t)(seed12 * seed12);

    const int shA = (seed & 2) != 0 ? 4 : 5;
    const int shB = numPartitions == 3 ? 6 : 5;
    const int sh1 = (seed & 1) != 0 ? shA : shB;
    const int sh2 = (seed & 1) != 0 ? shB : shA;
    const int sh3 = (seed & 0x10) != 0 ? sh1 : sh2;

    seed1  = (uint8_t)(seed1 >> sh1);
    seed2  = (uint8_t)(seed2 >> sh2);
    seed3  = (uint8_t)(seed3 >> sh1);
    seed4  = (uint8_t)(seed4 >> sh2);
    seed5  = (uint8_t)(seed5 >> sh1);
    seed6  = (uint8_t)(seed6 >> sh2);
    seed7  = (uint8_t)(seed7 >> sh1);
    seed8  = (uint8_t)(seed8 >> sh2);
    seed9  = (uint8_t)(seed9 >> sh3);
    seed10 = (uint8_t)(seed10 >> sh3);
    seed11 = (uint8_t)(seed11 >> sh3);
    seed12 = (uint8_t)(seed12 >> sh3);

    const int a = 0x3f & (seed1 * x + seed2 * y + seed11 * z + (rnum >> 14));
    const int b = 0x3f & (seed3 * x + seed4 * y + seed12 * z + (rnum >> 10));
    const int c = numPartitions >= 3 ? 0x3f & (seed5 * x + seed6 * y + seed9 * z + (rnum >> 6)) : 0;
    const int d = numPartitions >= 4 ? 0x3f & (seed7 * x + seed8 * y + seed10 * z + (rnum >> 2)) : 0;

    return a >= b && a >= c && a >= d ? 0 : b >= c && b >= d ? 1 : c >= d ? 2 : 3;
}

DecompressResult setTexelColors(void *dst, ColorEndpointPair *colorEndpoints, TexelWeightPair *texelWeights, int ccs,
                                uint32_t partitionIndexSeed, int numPartitions, int blockWidth, int blockHeight,
                                int blockDepth, bool isSRGB, bool isLDRMode, const uint32_t *colorEndpointModes)
{
    const bool smallBlock   = blockWidth * blockHeight * blockDepth < 31;
    DecompressResult result = DECOMPRESS_RESULT_VALID_BLOCK;
    bool isHDREndpoint[4];

    for (int i = 0; i < numPartitions; i++)
        isHDREndpoint[i] = isColorEndpointModeHDR(colorEndpointModes[i]);

    for (int texelZ = 0; texelZ < blockDepth; texelZ++)
        for (int texelY = 0; texelY < blockHeight; texelY++)
            for (int texelX = 0; texelX < blockWidth; texelX++)
            {
                const int texelNdx = texelZ * blockWidth * blockHeight + texelY * blockWidth + texelX;
                const int colorEndpointNdx =
                    numPartitions == 1 ?
                        0 :
                        computeTexelPartition(partitionIndexSeed, texelX, texelY, texelZ, numPartitions, smallBlock);
                DE_ASSERT(colorEndpointNdx < numPartitions);
                const UVec4 &e0               = colorEndpoints[colorEndpointNdx].e0;
                const UVec4 &e1               = colorEndpoints[colorEndpointNdx].e1;
                const TexelWeightPair &weight = texelWeights[texelNdx];

                if (isLDRMode && isHDREndpoint[colorEndpointNdx])
                {
                    if (isSRGB)
                    {
                        ((uint8_t *)dst)[texelNdx * 4 + 0] = 0xff;
                        ((uint8_t *)dst)[texelNdx * 4 + 1] = 0;
                        ((uint8_t *)dst)[texelNdx * 4 + 2] = 0xff;
                        ((uint8_t *)dst)[texelNdx * 4 + 3] = 0xff;
                    }
                    else
                    {
                        ((float *)dst)[texelNdx * 4 + 0] = 1.0f;
                        ((float *)dst)[texelNdx * 4 + 1] = 0;
                        ((float *)dst)[texelNdx * 4 + 2] = 1.0f;
                        ((float *)dst)[texelNdx * 4 + 3] = 1.0f;
                    }

                    result = DECOMPRESS_RESULT_ERROR;
                }
                else
                {
                    for (int channelNdx = 0; channelNdx < 4; channelNdx++)
                    {
                        if (!isHDREndpoint[colorEndpointNdx] ||
                            (channelNdx == 3 && colorEndpointModes[colorEndpointNdx] ==
                                                    14)) // \note Alpha for mode 14 is treated the same as LDR.
                        {
                            const uint32_t c0 = (e0[channelNdx] << 8) | (isSRGB ? 0x80 : e0[channelNdx]);
                            const uint32_t c1 = (e1[channelNdx] << 8) | (isSRGB ? 0x80 : e1[channelNdx]);
                            const uint32_t w  = weight.w[ccs == channelNdx ? 1 : 0];
                            const uint32_t c  = (c0 * (64 - w) + c1 * w + 32) / 64;

                            if (isSRGB)
                                ((uint8_t *)dst)[texelNdx * 4 + channelNdx] = (uint8_t)((c & 0xff00) >> 8);
                            else
                                ((float *)dst)[texelNdx * 4 + channelNdx] = c == 65535 ? 1.0f : (float)c / 65536.0f;
                        }
                        else
                        {
                            DE_STATIC_ASSERT((de::meta::TypesSame<deFloat16, uint16_t>::Value));
                            const uint32_t c0  = e0[channelNdx] << 4;
                            const uint32_t c1  = e1[channelNdx] << 4;
                            const uint32_t w   = weight.w[ccs == channelNdx ? 1 : 0];
                            const uint32_t c   = (c0 * (64 - w) + c1 * w + 32) / 64;
                            const uint32_t e   = getBits(c, 11, 15);
                            const uint32_t m   = getBits(c, 0, 10);
                            const uint32_t mt  = m < 512 ? 3 * m : m >= 1536 ? 5 * m - 2048 : 4 * m - 512;
                            const deFloat16 cf = (deFloat16)((e << 10) + (mt >> 3));

                            ((float *)dst)[texelNdx * 4 + channelNdx] =
                                deFloat16To32(isFloat16InfOrNan(cf) ? 0x7bff : cf);
                        }
                    }
                }
            }

    return result;
}

DecompressResult decompressBlock(void *dst, const Block128 &blockData, int blockWidth, int blockHeight, int blockDepth,
                                 bool isSRGB, bool isLDR)
{
    DE_ASSERT(isLDR || !isSRGB);

    // Decode block mode.

    const ASTCBlockMode blockMode =
        blockDepth > 1 ? getASTCBlockMode3D(blockData.getBits(0, 10)) : getASTCBlockMode(blockData.getBits(0, 10));

    // Check for block mode errors.

    if (blockMode.isError)
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, blockDepth, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }

    // Separate path for void-extent.

    if (blockMode.isVoidExtent)
        return decodeVoidExtentBlock(dst, blockData, blockWidth, blockHeight, blockDepth, isSRGB, isLDR);

    // Compute weight grid values.

    const int numWeights        = computeNumWeights(blockMode);
    const int numWeightDataBits = computeNumRequiredBits(blockMode.weightISEParams, numWeights);
    const int numPartitions     = (int)blockData.getBits(11, 12) + 1;

    // Check for errors in weight grid, partition and dual-plane parameters.

    if (numWeights > 64 || numWeightDataBits > 96 || numWeightDataBits < 24 || blockMode.weightGridWidth > blockWidth ||
        blockMode.weightGridDepth > blockDepth || blockMode.weightGridHeight > blockHeight ||
        (numPartitions == 4 && blockMode.isDualPlane))
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, blockDepth, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }

    // Compute number of bits available for color endpoint data.

    const bool isSingleUniqueCem = numPartitions == 1 || blockData.getBits(23, 24) == 0;
    const int numConfigDataBits  = (numPartitions == 1 ? 17 :
                                    isSingleUniqueCem  ? 29 :
                                                         25 + 3 * numPartitions) +
                                  (blockMode.isDualPlane ? 2 : 0);
    const int numBitsForColorEndpoints = 128 - numWeightDataBits - numConfigDataBits;
    const int extraCemBitsStart        = 127 - numWeightDataBits -
                                  (isSingleUniqueCem  ? -1 :
                                   numPartitions == 4 ? 7 :
                                   numPartitions == 3 ? 4 :
                                   numPartitions == 2 ? 1 :
                                                        0);
    // Decode color endpoint modes.

    uint32_t colorEndpointModes[4];
    decodeColorEndpointModes(&colorEndpointModes[0], blockData, numPartitions, extraCemBitsStart);

    const int numColorEndpointValues = computeNumColorEndpointValues(colorEndpointModes, numPartitions);

    // Check for errors in color endpoint value count.

    if (numColorEndpointValues > 18 || numBitsForColorEndpoints < deDivRoundUp32(13 * numColorEndpointValues, 5))
    {
        setASTCErrorColorBlock(dst, blockWidth, blockHeight, blockDepth, isSRGB);
        return DECOMPRESS_RESULT_ERROR;
    }

    // Compute color endpoints.

    ColorEndpointPair colorEndpoints[4];
    computeColorEndpoints(&colorEndpoints[0], blockData, &colorEndpointModes[0], numPartitions, numColorEndpointValues,
                          computeMaximumRangeISEParams(numBitsForColorEndpoints, numColorEndpointValues),
                          numBitsForColorEndpoints);

    // Compute texel weights.
    // 3D has larger size than 2D. Use 3D size to be more compatible.
    TexelWeightPair texelWeights[MAX_BLOCK_WIDTH_3D * MAX_BLOCK_HEIGHT_3D * MAX_BLOCK_DEPTH_3D];
    computeTexelWeights(&texelWeights[0], blockData, blockWidth, blockHeight, blockDepth, blockMode);

    // Set texel colors.

    const int ccs = blockMode.isDualPlane ? (int)blockData.getBits(extraCemBitsStart - 2, extraCemBitsStart - 1) : -1;
    const uint32_t partitionIndexSeed = numPartitions > 1 ? blockData.getBits(13, 22) : (uint32_t)-1;

    return setTexelColors(dst, &colorEndpoints[0], &texelWeights[0], ccs, partitionIndexSeed, numPartitions, blockWidth,
                          blockHeight, blockDepth, isSRGB, isLDR, &colorEndpointModes[0]);
}

void decompress(const PixelBufferAccess &dst, const uint8_t *data, bool isSRGB, bool isLDR)
{
    DE_ASSERT(isLDR || !isSRGB);

    const int blockWidth  = dst.getWidth();
    const int blockHeight = dst.getHeight();

    union
    {
        uint8_t sRGB[MAX_BLOCK_WIDTH * MAX_BLOCK_HEIGHT * 4];
        float linear[MAX_BLOCK_WIDTH * MAX_BLOCK_HEIGHT * 4];
    } decompressedBuffer;

    const Block128 blockData(data);
    decompressBlock(isSRGB ? (void *)&decompressedBuffer.sRGB[0] : (void *)&decompressedBuffer.linear[0], blockData,
                    dst.getWidth(), dst.getHeight(), dst.getDepth(), isSRGB, isLDR);

    if (isSRGB)
    {
        for (int i = 0; i < blockHeight; i++)
            for (int j = 0; j < blockWidth; j++)
            {
                dst.setPixel(IVec4(decompressedBuffer.sRGB[(i * blockWidth + j) * 4 + 0],
                                   decompressedBuffer.sRGB[(i * blockWidth + j) * 4 + 1],
                                   decompressedBuffer.sRGB[(i * blockWidth + j) * 4 + 2],
                                   decompressedBuffer.sRGB[(i * blockWidth + j) * 4 + 3]),
                             j, i, 0);
            }
    }
    else
    {
        for (int i = 0; i < blockHeight; i++)
            for (int j = 0; j < blockWidth; j++)
            {
                dst.setPixel(Vec4(decompressedBuffer.linear[(i * blockWidth + j) * 4 + 0],
                                  decompressedBuffer.linear[(i * blockWidth + j) * 4 + 1],
                                  decompressedBuffer.linear[(i * blockWidth + j) * 4 + 2],
                                  decompressedBuffer.linear[(i * blockWidth + j) * 4 + 3]),
                             j, i, 0);
            }
    }
}

void decompress3D(const PixelBufferAccess &dst, const uint8_t *data, bool isSRGB, bool isLDR)
{
    DE_ASSERT(isLDR || !isSRGB);

    const int blockWidth  = dst.getWidth();
    const int blockHeight = dst.getHeight();
    const int blockDepth  = dst.getDepth();

    union
    {
        uint8_t sRGB[MAX_BLOCK_WIDTH_3D * MAX_BLOCK_HEIGHT_3D * MAX_BLOCK_DEPTH_3D * 4];
        float linear[MAX_BLOCK_WIDTH_3D * MAX_BLOCK_HEIGHT_3D * MAX_BLOCK_DEPTH_3D * 4];
    } decompressedBuffer3D;

    const Block128 blockData(data);
    decompressBlock(isSRGB ? (void *)&decompressedBuffer3D.sRGB[0] : (void *)&decompressedBuffer3D.linear[0], blockData,
                    dst.getWidth(), dst.getHeight(), dst.getDepth(), isSRGB, isLDR);

    if (isSRGB)
    {
        for (int z = 0; z < blockDepth; z++)
            for (int i = 0; i < blockHeight; i++)
                for (int j = 0; j < blockWidth; j++)
                {
                    dst.setPixel(
                        IVec4(decompressedBuffer3D.sRGB[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 0],
                              decompressedBuffer3D.sRGB[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 1],
                              decompressedBuffer3D.sRGB[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 2],
                              decompressedBuffer3D.sRGB[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 3]),
                        j, i, z);
                }
    }
    else
    {
        for (int z = 0; z < blockDepth; z++)
            for (int i = 0; i < blockHeight; i++)
                for (int j = 0; j < blockWidth; j++)
                {
                    dst.setPixel(
                        Vec4(decompressedBuffer3D.linear[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 0],
                             decompressedBuffer3D.linear[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 1],
                             decompressedBuffer3D.linear[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 2],
                             decompressedBuffer3D.linear[(z * blockHeight * blockWidth + i * blockWidth + j) * 4 + 3]),
                        j, i, z);
                }
    }
}

// Helper class for setting bits in a 128-bit block.
class AssignBlock128
{
private:
    typedef uint64_t Word;

    enum
    {
        WORD_BYTES = sizeof(Word),
        WORD_BITS  = 8 * WORD_BYTES,
        NUM_WORDS  = 128 / WORD_BITS
    };

    DE_STATIC_ASSERT(128 % WORD_BITS == 0);

public:
    AssignBlock128(void)
    {
        for (int wordNdx = 0; wordNdx < NUM_WORDS; wordNdx++)
            m_words[wordNdx] = 0;
    }

    void setBit(int ndx, uint32_t val)
    {
        DE_ASSERT(de::inBounds(ndx, 0, 128));
        DE_ASSERT((val & 1) == val);
        const int wordNdx = ndx / WORD_BITS;
        const int bitNdx  = ndx % WORD_BITS;
        m_words[wordNdx]  = (m_words[wordNdx] & ~((Word)1 << bitNdx)) | ((Word)val << bitNdx);
    }

    void setBits(int low, int high, uint32_t bits)
    {
        DE_ASSERT(de::inBounds(low, 0, 128));
        DE_ASSERT(de::inBounds(high, 0, 128));
        DE_ASSERT(de::inRange(high - low + 1, 0, 32));
        DE_ASSERT((bits & (((Word)1 << (high - low + 1)) - 1)) == bits);

        if (high - low + 1 == 0)
            return;

        const int word0Ndx   = low / WORD_BITS;
        const int word1Ndx   = high / WORD_BITS;
        const int lowNdxInW0 = low % WORD_BITS;

        if (word0Ndx == word1Ndx)
            m_words[word0Ndx] =
                (m_words[word0Ndx] & ~((((Word)1 << (high - low + 1)) - 1) << lowNdxInW0)) | ((Word)bits << lowNdxInW0);
        else
        {
            DE_ASSERT(word1Ndx == word0Ndx + 1);

            const int highNdxInW1      = high % WORD_BITS;
            const int numBitsToSetInW0 = WORD_BITS - lowNdxInW0;
            const Word bitsLowMask     = ((Word)1 << numBitsToSetInW0) - 1;

            m_words[word0Ndx] =
                (m_words[word0Ndx] & (((Word)1 << lowNdxInW0) - 1)) | (((Word)bits & bitsLowMask) << lowNdxInW0);
            m_words[word1Ndx] = (m_words[word1Ndx] & ~(((Word)1 << (highNdxInW1 + 1)) - 1)) |
                                (((Word)bits & ~bitsLowMask) >> numBitsToSetInW0);
        }
    }

    void assignToMemory(uint8_t *dst) const
    {
        for (int wordNdx = 0; wordNdx < NUM_WORDS; wordNdx++)
        {
            for (int byteNdx = 0; byteNdx < WORD_BYTES; byteNdx++)
                dst[wordNdx * WORD_BYTES + byteNdx] = (uint8_t)((m_words[wordNdx] >> (8 * byteNdx)) & 0xff);
        }
    }

    void pushBytesToVector(vector<uint8_t> &dst) const
    {
        const int assignStartIndex = (int)dst.size();
        dst.resize(dst.size() + BLOCK_SIZE_BYTES);
        assignToMemory(&dst[assignStartIndex]);
    }

private:
    Word m_words[NUM_WORDS];
};

// A helper for sequential access into a AssignBlock128.
class BitAssignAccessStream
{
public:
    BitAssignAccessStream(AssignBlock128 &dst, int startNdxInSrc, int length, bool forward)
        : m_dst(dst)
        , m_startNdxInSrc(startNdxInSrc)
        , m_length(length)
        , m_forward(forward)
        , m_ndx(0)
    {
    }

    // Set the next num bits. Bits at positions greater than or equal to m_length are not touched.
    void setNext(int num, uint32_t bits)
    {
        DE_ASSERT((bits & (((uint64_t)1 << num) - 1)) == bits);

        if (num == 0 || m_ndx >= m_length)
            return;

        const int end             = m_ndx + num;
        const int numBitsToDst    = de::max(0, de::min(m_length, end) - m_ndx);
        const int low             = m_ndx;
        const int high            = m_ndx + numBitsToDst - 1;
        const uint32_t actualBits = getBits(bits, 0, numBitsToDst - 1);

        m_ndx += num;

        return m_forward ?
                   m_dst.setBits(m_startNdxInSrc + low, m_startNdxInSrc + high, actualBits) :
                   m_dst.setBits(m_startNdxInSrc - high, m_startNdxInSrc - low, reverseBits(actualBits, numBitsToDst));
    }

private:
    AssignBlock128 &m_dst;
    const int m_startNdxInSrc;
    const int m_length;
    const bool m_forward;

    int m_ndx;
};

struct VoidExtentParams
{
    DE_STATIC_ASSERT((de::meta::TypesSame<deFloat16, uint16_t>::Value));
    bool isHDR;
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
    // \note Currently extent coordinates are all set to all-ones.

    VoidExtentParams(bool isHDR_, uint16_t r_, uint16_t g_, uint16_t b_, uint16_t a_)
        : isHDR(isHDR_)
        , r(r_)
        , g(g_)
        , b(b_)
        , a(a_)
    {
    }
};

static AssignBlock128 generateVoidExtentBlock(const VoidExtentParams &params)
{
    AssignBlock128 block;

    block.setBits(0, 8, 0x1fc); // \note Marks void-extent block.
    block.setBit(9, params.isHDR);
    block.setBits(10, 11, 3); // \note Spec shows that these bits are both set, although they serve no purpose.

    // Extent coordinates - currently all-ones.
    block.setBits(12, 24, 0x1fff);
    block.setBits(25, 37, 0x1fff);
    block.setBits(38, 50, 0x1fff);
    block.setBits(51, 63, 0x1fff);

    DE_ASSERT(!params.isHDR || (!isFloat16InfOrNan(params.r) && !isFloat16InfOrNan(params.g) &&
                                !isFloat16InfOrNan(params.b) && !isFloat16InfOrNan(params.a)));

    block.setBits(64, 79, params.r);
    block.setBits(80, 95, params.g);
    block.setBits(96, 111, params.b);
    block.setBits(112, 127, params.a);

    return block;
}

// An input array of ISE inputs for an entire ASTC block. Can be given as either single values in the
// range [0, maximumValueOfISERange] or as explicit block value specifications. The latter is needed
// so we can test all possible values of T and Q in a block, since multiple T or Q values may map
// to the same set of decoded values.
struct ISEInput
{
    struct Block
    {
        uint32_t tOrQValue; //!< The 8-bit T or 7-bit Q in a trit or quint ISE block.
        uint32_t bitValues[5];
    };

    bool isGivenInBlockForm;
    union
    {
        //!< \note 64 comes from the maximum number of weight values in an ASTC block.
        uint32_t plain[64];
        Block block[64];
    } value;

    ISEInput(void) : isGivenInBlockForm(false)
    {
    }
};

static inline uint32_t computeISERangeMax(const ISEParams &iseParams)
{
    switch (iseParams.mode)
    {
    case ISEMODE_TRIT:
        return (1u << iseParams.numBits) * 3 - 1;
    case ISEMODE_QUINT:
        return (1u << iseParams.numBits) * 5 - 1;
    case ISEMODE_PLAIN_BIT:
        return (1u << iseParams.numBits) - 1;
    default:
        DE_ASSERT(false);
        return -1;
    }
}

struct NormalBlockParams
{
    int weightGridWidth;
    int weightGridHeight;
    int weightGridDepth;
    ISEParams weightISEParams;
    bool isDualPlane;
    uint32_t ccs; //! \note Irrelevant if !isDualPlane.
    int numPartitions;
    uint32_t colorEndpointModes[4];
    // \note Below members are irrelevant if numPartitions == 1.
    bool isMultiPartSingleCemMode; //! \note If true, the single CEM is at colorEndpointModes[0].
    uint32_t partitionSeed;

    NormalBlockParams(void)
        : weightGridWidth(-1)
        , weightGridHeight(-1)
        , weightGridDepth(-1)
        , weightISEParams(ISEMODE_LAST, -1)
        , isDualPlane(true)
        , ccs((uint32_t)-1)
        , numPartitions(-1)
        , isMultiPartSingleCemMode(false)
        , partitionSeed((uint32_t)-1)
    {
        colorEndpointModes[0] = 0;
        colorEndpointModes[1] = 0;
        colorEndpointModes[2] = 0;
        colorEndpointModes[3] = 0;
    }
};

struct NormalBlockISEInputs
{
    ISEInput weight;
    ISEInput endpoint;

    NormalBlockISEInputs(void) : weight(), endpoint()
    {
    }
};

static inline int computeNumWeights(const NormalBlockParams &params)
{
    return params.weightGridWidth * params.weightGridHeight * params.weightGridDepth * (params.isDualPlane ? 2 : 1);
}

static inline int computeNumBitsForColorEndpoints(const NormalBlockParams &params)
{
    const int numWeightBits     = computeNumRequiredBits(params.weightISEParams, computeNumWeights(params));
    const int numConfigDataBits = (params.numPartitions == 1       ? 17 :
                                   params.isMultiPartSingleCemMode ? 29 :
                                                                     25 + 3 * params.numPartitions) +
                                  (params.isDualPlane ? 2 : 0);

    return 128 - numWeightBits - numConfigDataBits;
}

static inline int computeNumColorEndpointValues(const uint32_t *endpointModes, int numPartitions,
                                                bool isMultiPartSingleCemMode)
{
    if (isMultiPartSingleCemMode)
        return numPartitions * computeNumColorEndpointValues(endpointModes[0]);
    else
    {
        int result = 0;
        for (int i = 0; i < numPartitions; i++)
            result += computeNumColorEndpointValues(endpointModes[i]);
        return result;
    }
}

static inline bool isValidBlockParams(const NormalBlockParams &params, int blockWidth, int blockHeight, int blockDepth)
{
    const int numWeights             = computeNumWeights(params);
    const int numWeightBits          = computeNumRequiredBits(params.weightISEParams, numWeights);
    const int numColorEndpointValues = computeNumColorEndpointValues(
        &params.colorEndpointModes[0], params.numPartitions, params.isMultiPartSingleCemMode);
    const int numBitsForColorEndpoints = computeNumBitsForColorEndpoints(params);

    return numWeights <= 64 && de::inRange(numWeightBits, 24, 96) && params.weightGridWidth <= blockWidth &&
           params.weightGridDepth <= blockDepth && params.weightGridHeight <= blockHeight &&
           !(params.numPartitions == 4 && params.isDualPlane) && numColorEndpointValues <= 18 &&
           numBitsForColorEndpoints >= deDivRoundUp32(13 * numColorEndpointValues, 5);
}

// Write bits 0 to 10 of an ASTC block.
static void writeBlockMode(AssignBlock128 &dst, const NormalBlockParams &blockParams, bool isAstc3D)
{
    const uint32_t d = blockParams.isDualPlane != 0;
    // r and h initialized in switch below.
    uint32_t r;
    uint32_t h;
    // a, b and blockModeLayoutNdx initialized in block mode layout index detecting loop below.
    uint32_t a = (uint32_t)-1;
    uint32_t b = (uint32_t)-1;
    uint32_t c = (uint32_t)-1;
    int blockModeLayoutNdx;

    // Find the values of r and h (ISE range).
    switch (computeISERangeMax(blockParams.weightISEParams))
    {
    case 1:
        r = 2;
        h = 0;
        break;
    case 2:
        r = 3;
        h = 0;
        break;
    case 3:
        r = 4;
        h = 0;
        break;
    case 4:
        r = 5;
        h = 0;
        break;
    case 5:
        r = 6;
        h = 0;
        break;
    case 7:
        r = 7;
        h = 0;
        break;

    case 9:
        r = 2;
        h = 1;
        break;
    case 11:
        r = 3;
        h = 1;
        break;
    case 15:
        r = 4;
        h = 1;
        break;
    case 19:
        r = 5;
        h = 1;
        break;
    case 23:
        r = 6;
        h = 1;
        break;
    case 31:
        r = 7;
        h = 1;
        break;

    default:
        DE_ASSERT(false);
        r = (uint32_t)-1;
        h = (uint32_t)-1;
    }

    // Find block mode layout index, i.e. appropriate row in the "2d block mode layout" table in ASTC spec.
#define SB(NDX, VAL) dst.setBit((NDX), (VAL))
#define ASSIGN_BITS(B10, B9, B8, B7, B6, B5, B4, B3, B2, B1, B0) \
    do                                                           \
    {                                                            \
        SB(10, (B10));                                           \
        SB(9, (B9));                                             \
        SB(8, (B8));                                             \
        SB(7, (B7));                                             \
        SB(6, (B6));                                             \
        SB(5, (B5));                                             \
        SB(4, (B4));                                             \
        SB(3, (B3));                                             \
        SB(2, (B2));                                             \
        SB(1, (B1));                                             \
        SB(0, (B0));                                             \
    } while (false)

    if (!isAstc3D)
    {
        enum BlockModeLayoutABVariable
        {
            Z = 0,
            A = 1,
            B = 2
        };

        static const struct BlockModeLayout
        {
            int aNumBits;
            int bNumBits;
            BlockModeLayoutABVariable gridWidthVariableTerm;
            int gridWidthConstantTerm;
            BlockModeLayoutABVariable gridHeightVariableTerm;
            int gridHeightConstantTerm;
        } blockModeLayouts[] = {{2, 2, B, 4, A, 2},  {2, 2, B, 8, A, 2},  {2, 2, A, 2, B, 8},  {2, 1, A, 2, B, 6},
                                {2, 1, B, 2, A, 2},  {2, 0, Z, 12, A, 2}, {2, 0, A, 2, Z, 12}, {0, 0, Z, 6, Z, 10},
                                {0, 0, Z, 10, Z, 6}, {2, 2, A, 6, B, 6}};

        for (blockModeLayoutNdx = 0; blockModeLayoutNdx < DE_LENGTH_OF_ARRAY(blockModeLayouts); blockModeLayoutNdx++)
        {
            const BlockModeLayout &layout   = blockModeLayouts[blockModeLayoutNdx];
            const int aMax                  = (1 << layout.aNumBits) - 1;
            const int bMax                  = (1 << layout.bNumBits) - 1;
            const int variableOffsetsMax[3] = {0, aMax, bMax};
            const int widthMin              = layout.gridWidthConstantTerm;
            const int heightMin             = layout.gridHeightConstantTerm;
            const int widthMax              = widthMin + variableOffsetsMax[layout.gridWidthVariableTerm];
            const int heightMax             = heightMin + variableOffsetsMax[layout.gridHeightVariableTerm];

            DE_ASSERT(layout.gridWidthVariableTerm != layout.gridHeightVariableTerm ||
                      layout.gridWidthVariableTerm == Z);

            if (de::inRange(blockParams.weightGridWidth, widthMin, widthMax) &&
                de::inRange(blockParams.weightGridHeight, heightMin, heightMax))
            {
                uint32_t defaultvalue    = 0;
                uint32_t &widthVariable  = layout.gridWidthVariableTerm == A ? a :
                                           layout.gridWidthVariableTerm == B ? b :
                                                                               defaultvalue;
                uint32_t &heightVariable = layout.gridHeightVariableTerm == A ? a :
                                           layout.gridHeightVariableTerm == B ? b :
                                                                                defaultvalue;

                widthVariable  = blockParams.weightGridWidth - layout.gridWidthConstantTerm;
                heightVariable = blockParams.weightGridHeight - layout.gridHeightConstantTerm;

                break;
            }
        }

        // Set block mode bits.

        const uint32_t a0 = getBit(a, 0);
        const uint32_t a1 = getBit(a, 1);
        const uint32_t b0 = getBit(b, 0);
        const uint32_t b1 = getBit(b, 1);
        const uint32_t r0 = getBit(r, 0);
        const uint32_t r1 = getBit(r, 1);
        const uint32_t r2 = getBit(r, 2);

        switch (blockModeLayoutNdx)
        {
        case 0:
            ASSIGN_BITS(d, h, b1, b0, a1, a0, r0, 0, 0, r2, r1);
            break;
        case 1:
            ASSIGN_BITS(d, h, b1, b0, a1, a0, r0, 0, 1, r2, r1);
            break;
        case 2:
            ASSIGN_BITS(d, h, b1, b0, a1, a0, r0, 1, 0, r2, r1);
            break;
        case 3:
            ASSIGN_BITS(d, h, 0, b, a1, a0, r0, 1, 1, r2, r1);
            break;
        case 4:
            ASSIGN_BITS(d, h, 1, b, a1, a0, r0, 1, 1, r2, r1);
            break;
        case 5:
            ASSIGN_BITS(d, h, 0, 0, a1, a0, r0, r2, r1, 0, 0);
            break;
        case 6:
            ASSIGN_BITS(d, h, 0, 1, a1, a0, r0, r2, r1, 0, 0);
            break;
        case 7:
            ASSIGN_BITS(d, h, 1, 1, 0, 0, r0, r2, r1, 0, 0);
            break;
        case 8:
            ASSIGN_BITS(d, h, 1, 1, 0, 1, r0, r2, r1, 0, 0);
            break;
        case 9:
            ASSIGN_BITS(b1, b0, 1, 0, a1, a0, r0, r2, r1, 0, 0);
            DE_ASSERT(d == 0 && h == 0);
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
    {
        {
            enum BlockModeLayoutABVariable
            {
                Z = 0,
                A = 1,
                B = 2,
                C = 3
            };

            static const struct BlockModeLayout
            {
                int aNumBits;
                int bNumBits;
                int cNumBits;
                BlockModeLayoutABVariable gridWidthVariableTerm;
                int gridWidthConstantTerm;
                BlockModeLayoutABVariable gridHeightVariableTerm;
                int gridHeightConstantTerm;
                BlockModeLayoutABVariable gridDepthVariableTerm;
                int gridDepthConstantTerm;
            } blockModeLayouts[] = {{2, 2, 2, B, 2, C, 2, A, 2}, {0, 0, 0, Z, 6, Z, 2, Z, 2},
                                    {0, 0, 0, Z, 2, Z, 6, Z, 2}, {0, 0, 0, Z, 2, Z, 2, Z, 6},
                                    {2, 0, 2, Z, 6, C, 2, A, 2}, {2, 0, 2, A, 2, Z, 6, C, 2},
                                    {2, 0, 2, A, 2, C, 2, Z, 6}};

            for (blockModeLayoutNdx = 0; blockModeLayoutNdx < DE_LENGTH_OF_ARRAY(blockModeLayouts);
                 blockModeLayoutNdx++)
            {
                const BlockModeLayout &layout   = blockModeLayouts[blockModeLayoutNdx];
                const int aMax                  = (1 << layout.aNumBits) - 1;
                const int bMax                  = (1 << layout.bNumBits) - 1;
                const int cMax                  = (1 << layout.cNumBits) - 1;
                const int variableOffsetsMax[4] = {0, aMax, bMax, cMax};
                const int widthMin              = layout.gridWidthConstantTerm;
                const int heightMin             = layout.gridHeightConstantTerm;
                const int depthMin              = layout.gridDepthConstantTerm;
                const int widthMax              = widthMin + variableOffsetsMax[layout.gridWidthVariableTerm];
                const int heightMax             = heightMin + variableOffsetsMax[layout.gridHeightVariableTerm];
                const int depthMax              = depthMin + variableOffsetsMax[layout.gridDepthVariableTerm];

                if (de::inRange(blockParams.weightGridWidth, widthMin, widthMax) &&
                    de::inRange(blockParams.weightGridHeight, heightMin, heightMax) &&
                    de::inRange(blockParams.weightGridDepth, depthMin, depthMax))
                {
                    uint32_t defaultvalue    = 0;
                    uint32_t &widthVariable  = layout.gridWidthVariableTerm == A ? a :
                                               layout.gridWidthVariableTerm == B ? b :
                                               layout.gridWidthVariableTerm == C ? c :
                                                                                   defaultvalue;
                    uint32_t &heightVariable = layout.gridHeightVariableTerm == A ? a :
                                               layout.gridHeightVariableTerm == B ? b :
                                               layout.gridHeightVariableTerm == C ? c :
                                                                                    defaultvalue;
                    uint32_t &depthVariable  = layout.gridDepthVariableTerm == A ? a :
                                               layout.gridDepthVariableTerm == B ? b :
                                               layout.gridDepthVariableTerm == C ? c :
                                                                                   defaultvalue;

                    widthVariable  = blockParams.weightGridWidth - layout.gridWidthConstantTerm;
                    heightVariable = blockParams.weightGridHeight - layout.gridHeightConstantTerm;
                    depthVariable  = blockParams.weightGridDepth - layout.gridDepthConstantTerm;

                    break;
                }
            }
        }

        // Set block mode bits.

        const uint32_t a0 = getBit(a, 0);
        const uint32_t a1 = getBit(a, 1);
        const uint32_t b0 = getBit(b, 0);
        const uint32_t b1 = getBit(b, 1);
        const uint32_t c0 = getBit(c, 0);
        const uint32_t c1 = getBit(c, 1);
        const uint32_t r0 = getBit(r, 0);
        const uint32_t r1 = getBit(r, 1);
        const uint32_t r2 = getBit(r, 2);

        switch (blockModeLayoutNdx)
        {
        case 0:
            ASSIGN_BITS(d, h, c1, c0, b1, b0, r0, a1, a0, r2, r1);
            break;
        case 1:
            ASSIGN_BITS(d, h, 1, 1, 0, 0, r0, r2, r1, 0, 0);
            break;
        case 2:
            ASSIGN_BITS(d, h, 1, 1, 0, 1, r0, r2, r1, 0, 0);
            break;
        case 3:
            ASSIGN_BITS(d, h, 1, 1, 1, 0, r0, r2, r1, 0, 0);
            break;
        case 4:
            ASSIGN_BITS(c1, c0, 0, 0, b1, b0, r0, r2, r1, 0, 0);
            DE_ASSERT(d == 0 && h == 0);
            break;
        case 5:
            ASSIGN_BITS(c1, c0, 0, 1, b1, b0, r0, r2, r1, 0, 0);
            DE_ASSERT(d == 0 && h == 0);
            break;
        case 6:
            ASSIGN_BITS(c1, c0, 1, 0, b1, b0, r0, r2, r1, 0, 0);
            DE_ASSERT(d == 0 && h == 0);
            break;
        default:
            DE_ASSERT(false);
        }
    }

#undef ASSIGN_BITS
#undef SB
}

// Write color endpoint mode data of an ASTC block.
static void writeColorEndpointModes(AssignBlock128 &dst, const uint32_t *colorEndpointModes,
                                    bool isMultiPartSingleCemMode, int numPartitions, int extraCemBitsStart)
{
    if (numPartitions == 1)
        dst.setBits(13, 16, colorEndpointModes[0]);
    else
    {
        if (isMultiPartSingleCemMode)
        {
            dst.setBits(23, 24, 0);
            dst.setBits(25, 28, colorEndpointModes[0]);
        }
        else
        {
            DE_ASSERT(numPartitions > 0);
            const uint32_t minCem      = *std::min_element(&colorEndpointModes[0], &colorEndpointModes[numPartitions]);
            const uint32_t maxCem      = *std::max_element(&colorEndpointModes[0], &colorEndpointModes[numPartitions]);
            const uint32_t minCemClass = minCem / 4;
            const uint32_t maxCemClass = maxCem / 4;
            DE_ASSERT(maxCemClass - minCemClass <= 1);
            DE_UNREF(minCemClass); // \note For non-debug builds.
            const uint32_t highLevelSelector = de::max(1u, maxCemClass);

            dst.setBits(23, 24, highLevelSelector);

            for (int partNdx = 0; partNdx < numPartitions; partNdx++)
            {
                const uint32_t c           = colorEndpointModes[partNdx] / 4 == highLevelSelector ? 1 : 0;
                const uint32_t m           = colorEndpointModes[partNdx] % 4;
                const uint32_t lowMBit0Ndx = numPartitions + 2 * partNdx;
                const uint32_t lowMBit1Ndx = numPartitions + 2 * partNdx + 1;
                dst.setBit(25 + partNdx, c);
                dst.setBit(lowMBit0Ndx < 4 ? 25 + lowMBit0Ndx : extraCemBitsStart + lowMBit0Ndx - 4, getBit(m, 0));
                dst.setBit(lowMBit1Ndx < 4 ? 25 + lowMBit1Ndx : extraCemBitsStart + lowMBit1Ndx - 4, getBit(m, 1));
            }
        }
    }
}

static void encodeISETritBlock(BitAssignAccessStream &dst, int numBits, bool fromExplicitInputBlock,
                               const ISEInput::Block &blockInput, const uint32_t *nonBlockInput, int numValues)
{
    // tritBlockTValue[t0][t1][t2][t3][t4] is a value of T (not necessarily the only one) that will yield the given trits when decoded.
    static const uint32_t tritBlockTValue[3][3][3][3][3] = {{{{{0, 128, 96}, {32, 160, 224}, {64, 192, 28}},
                                                              {{16, 144, 112}, {48, 176, 240}, {80, 208, 156}},
                                                              {{3, 131, 99}, {35, 163, 227}, {67, 195, 31}}},
                                                             {{{4, 132, 100}, {36, 164, 228}, {68, 196, 60}},
                                                              {{20, 148, 116}, {52, 180, 244}, {84, 212, 188}},
                                                              {{19, 147, 115}, {51, 179, 243}, {83, 211, 159}}},
                                                             {{{8, 136, 104}, {40, 168, 232}, {72, 200, 92}},
                                                              {{24, 152, 120}, {56, 184, 248}, {88, 216, 220}},
                                                              {{12, 140, 108}, {44, 172, 236}, {76, 204, 124}}}},
                                                            {{{{1, 129, 97}, {33, 161, 225}, {65, 193, 29}},
                                                              {{17, 145, 113}, {49, 177, 241}, {81, 209, 157}},
                                                              {{7, 135, 103}, {39, 167, 231}, {71, 199, 63}}},
                                                             {{{5, 133, 101}, {37, 165, 229}, {69, 197, 61}},
                                                              {{21, 149, 117}, {53, 181, 245}, {85, 213, 189}},
                                                              {{23, 151, 119}, {55, 183, 247}, {87, 215, 191}}},
                                                             {{{9, 137, 105}, {41, 169, 233}, {73, 201, 93}},
                                                              {{25, 153, 121}, {57, 185, 249}, {89, 217, 221}},
                                                              {{13, 141, 109}, {45, 173, 237}, {77, 205, 125}}}},
                                                            {{{{2, 130, 98}, {34, 162, 226}, {66, 194, 30}},
                                                              {{18, 146, 114}, {50, 178, 242}, {82, 210, 158}},
                                                              {{11, 139, 107}, {43, 171, 235}, {75, 203, 95}}},
                                                             {{{6, 134, 102}, {38, 166, 230}, {70, 198, 62}},
                                                              {{22, 150, 118}, {54, 182, 246}, {86, 214, 190}},
                                                              {{27, 155, 123}, {59, 187, 251}, {91, 219, 223}}},
                                                             {{{10, 138, 106}, {42, 170, 234}, {74, 202, 94}},
                                                              {{26, 154, 122}, {58, 186, 250}, {90, 218, 222}},
                                                              {{14, 142, 110}, {46, 174, 238}, {78, 206, 126}}}}};

    DE_ASSERT(de::inRange(numValues, 1, 5));

    uint32_t tritParts[5];
    uint32_t bitParts[5];

    for (int i = 0; i < 5; i++)
    {
        if (i < numValues)
        {
            if (fromExplicitInputBlock)
            {
                bitParts[i]  = blockInput.bitValues[i];
                tritParts[i] = -1; // \note Won't be used, but silences warning.
            }
            else
            {
                // \todo [2016-01-20 pyry] numBits = 0 doesn't make sense
                bitParts[i]  = numBits > 0 ? getBits(nonBlockInput[i], 0, numBits - 1) : 0;
                tritParts[i] = nonBlockInput[i] >> numBits;
            }
        }
        else
        {
            bitParts[i]  = 0;
            tritParts[i] = 0;
        }
    }

    const uint32_t T = fromExplicitInputBlock ?
                           blockInput.tOrQValue :
                           tritBlockTValue[tritParts[0]][tritParts[1]][tritParts[2]][tritParts[3]][tritParts[4]];

    dst.setNext(numBits, bitParts[0]);
    dst.setNext(2, getBits(T, 0, 1));
    dst.setNext(numBits, bitParts[1]);
    dst.setNext(2, getBits(T, 2, 3));
    dst.setNext(numBits, bitParts[2]);
    dst.setNext(1, getBit(T, 4));
    dst.setNext(numBits, bitParts[3]);
    dst.setNext(2, getBits(T, 5, 6));
    dst.setNext(numBits, bitParts[4]);
    dst.setNext(1, getBit(T, 7));
}

static void encodeISEQuintBlock(BitAssignAccessStream &dst, int numBits, bool fromExplicitInputBlock,
                                const ISEInput::Block &blockInput, const uint32_t *nonBlockInput, int numValues)
{
    // quintBlockQValue[q0][q1][q2] is a value of Q (not necessarily the only one) that will yield the given quints when decoded.
    static const uint32_t quintBlockQValue[5][5][5] = {{{0, 32, 64, 96, 102},
                                                        {8, 40, 72, 104, 110},
                                                        {16, 48, 80, 112, 118},
                                                        {24, 56, 88, 120, 126},
                                                        {5, 37, 69, 101, 39}},
                                                       {{1, 33, 65, 97, 103},
                                                        {9, 41, 73, 105, 111},
                                                        {17, 49, 81, 113, 119},
                                                        {25, 57, 89, 121, 127},
                                                        {13, 45, 77, 109, 47}},
                                                       {{2, 34, 66, 98, 70},
                                                        {10, 42, 74, 106, 78},
                                                        {18, 50, 82, 114, 86},
                                                        {26, 58, 90, 122, 94},
                                                        {21, 53, 85, 117, 55}},
                                                       {{3, 35, 67, 99, 71},
                                                        {11, 43, 75, 107, 79},
                                                        {19, 51, 83, 115, 87},
                                                        {27, 59, 91, 123, 95},
                                                        {29, 61, 93, 125, 63}},
                                                       {{4, 36, 68, 100, 38},
                                                        {12, 44, 76, 108, 46},
                                                        {20, 52, 84, 116, 54},
                                                        {28, 60, 92, 124, 62},
                                                        {6, 14, 22, 30, 7}}};

    DE_ASSERT(de::inRange(numValues, 1, 3));

    uint32_t quintParts[3];
    uint32_t bitParts[3];

    for (int i = 0; i < 3; i++)
    {
        if (i < numValues)
        {
            if (fromExplicitInputBlock)
            {
                bitParts[i]   = blockInput.bitValues[i];
                quintParts[i] = -1; // \note Won't be used, but silences warning.
            }
            else
            {
                // \todo [2016-01-20 pyry] numBits = 0 doesn't make sense
                bitParts[i]   = numBits > 0 ? getBits(nonBlockInput[i], 0, numBits - 1) : 0;
                quintParts[i] = nonBlockInput[i] >> numBits;
            }
        }
        else
        {
            bitParts[i]   = 0;
            quintParts[i] = 0;
        }
    }

    const uint32_t Q =
        fromExplicitInputBlock ? blockInput.tOrQValue : quintBlockQValue[quintParts[0]][quintParts[1]][quintParts[2]];

    dst.setNext(numBits, bitParts[0]);
    dst.setNext(3, getBits(Q, 0, 2));
    dst.setNext(numBits, bitParts[1]);
    dst.setNext(2, getBits(Q, 3, 4));
    dst.setNext(numBits, bitParts[2]);
    dst.setNext(2, getBits(Q, 5, 6));
}

static void encodeISEBitBlock(BitAssignAccessStream &dst, int numBits, uint32_t value)
{
    DE_ASSERT(de::inRange(value, 0u, (1u << numBits) - 1));
    dst.setNext(numBits, value);
}

static void encodeISE(BitAssignAccessStream &dst, const ISEParams &params, const ISEInput &input, int numValues)
{
    if (params.mode == ISEMODE_TRIT)
    {
        const int numBlocks = deDivRoundUp32(numValues, 5);
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const int numValuesInBlock = blockNdx == numBlocks - 1 ? numValues - 5 * (numBlocks - 1) : 5;
            encodeISETritBlock(dst, params.numBits, input.isGivenInBlockForm,
                               input.isGivenInBlockForm ? input.value.block[blockNdx] : ISEInput::Block(),
                               input.isGivenInBlockForm ? nullptr : &input.value.plain[5 * blockNdx], numValuesInBlock);
        }
    }
    else if (params.mode == ISEMODE_QUINT)
    {
        const int numBlocks = deDivRoundUp32(numValues, 3);
        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
        {
            const int numValuesInBlock = blockNdx == numBlocks - 1 ? numValues - 3 * (numBlocks - 1) : 3;
            encodeISEQuintBlock(dst, params.numBits, input.isGivenInBlockForm,
                                input.isGivenInBlockForm ? input.value.block[blockNdx] : ISEInput::Block(),
                                input.isGivenInBlockForm ? nullptr : &input.value.plain[3 * blockNdx],
                                numValuesInBlock);
        }
    }
    else
    {
        DE_ASSERT(params.mode == ISEMODE_PLAIN_BIT);
        for (int i = 0; i < numValues; i++)
            encodeISEBitBlock(dst, params.numBits,
                              input.isGivenInBlockForm ? input.value.block[i].bitValues[0] : input.value.plain[i]);
    }
}

static void writeWeightData(AssignBlock128 &dst, const ISEParams &iseParams, const ISEInput &input, int numWeights)
{
    const int numWeightBits = computeNumRequiredBits(iseParams, numWeights);
    BitAssignAccessStream access(dst, 127, numWeightBits, false);
    encodeISE(access, iseParams, input, numWeights);
}

static void writeColorEndpointData(AssignBlock128 &dst, const ISEParams &iseParams, const ISEInput &input,
                                   int numEndpoints, int numBitsForColorEndpoints, int colorEndpointDataStartNdx)
{
    BitAssignAccessStream access(dst, colorEndpointDataStartNdx, numBitsForColorEndpoints, true);
    encodeISE(access, iseParams, input, numEndpoints);
}

static AssignBlock128 generateNormalBlock(const NormalBlockParams &blockParams, int blockWidth, int blockHeight,
                                          int blockDepth, const NormalBlockISEInputs &iseInputs)
{
    DE_ASSERT(isValidBlockParams(blockParams, blockWidth, blockHeight, blockDepth));
    DE_UNREF(blockWidth);  // \note For non-debug builds.
    DE_UNREF(blockHeight); // \note For non-debug builds.
    DE_UNREF(blockDepth);  // \note For non-debug builds.

    AssignBlock128 block;
    const int numWeights    = computeNumWeights(blockParams);
    const int numWeightBits = computeNumRequiredBits(blockParams.weightISEParams, numWeights);

    writeBlockMode(block, blockParams, blockDepth > 1);

    block.setBits(11, 12, blockParams.numPartitions - 1);
    if (blockParams.numPartitions > 1)
        block.setBits(13, 22, blockParams.partitionSeed);

    {
        const int extraCemBitsStart = 127 - numWeightBits -
                                      (blockParams.numPartitions == 1 || blockParams.isMultiPartSingleCemMode ? -1 :
                                       blockParams.numPartitions == 4                                         ? 7 :
                                       blockParams.numPartitions == 3                                         ? 4 :
                                       blockParams.numPartitions == 2                                         ? 1 :
                                                                                                                0);

        writeColorEndpointModes(block, &blockParams.colorEndpointModes[0], blockParams.isMultiPartSingleCemMode,
                                blockParams.numPartitions, extraCemBitsStart);

        if (blockParams.isDualPlane)
            block.setBits(extraCemBitsStart - 2, extraCemBitsStart - 1, blockParams.ccs);
    }

    writeWeightData(block, blockParams.weightISEParams, iseInputs.weight, numWeights);

    {
        const int numColorEndpointValues = computeNumColorEndpointValues(
            &blockParams.colorEndpointModes[0], blockParams.numPartitions, blockParams.isMultiPartSingleCemMode);
        const int numBitsForColorEndpoints  = computeNumBitsForColorEndpoints(blockParams);
        const int colorEndpointDataStartNdx = blockParams.numPartitions == 1 ? 17 : 29;
        const ISEParams &colorEndpointISEParams =
            computeMaximumRangeISEParams(numBitsForColorEndpoints, numColorEndpointValues);

        writeColorEndpointData(block, colorEndpointISEParams, iseInputs.endpoint, numColorEndpointValues,
                               numBitsForColorEndpoints, colorEndpointDataStartNdx);
    }

    return block;
}

// Generate default ISE inputs for weight and endpoint data - gradient-ish values.
static NormalBlockISEInputs generateDefaultISEInputs(const NormalBlockParams &blockParams)
{
    NormalBlockISEInputs result;

    {
        result.weight.isGivenInBlockForm = false;

        const int numWeights     = computeNumWeights(blockParams);
        const int weightRangeMax = computeISERangeMax(blockParams.weightISEParams);

        if (blockParams.isDualPlane)
        {
            for (int i = 0; i < numWeights; i += 2)
                result.weight.value.plain[i] = (i * weightRangeMax + (numWeights - 1) / 2) / (numWeights - 1);

            for (int i = 1; i < numWeights; i += 2)
                result.weight.value.plain[i] =
                    weightRangeMax - (i * weightRangeMax + (numWeights - 1) / 2) / (numWeights - 1);
        }
        else
        {
            for (int i = 0; i < numWeights; i++)
                result.weight.value.plain[i] = (i * weightRangeMax + (numWeights - 1) / 2) / (numWeights - 1);
        }
    }

    {
        result.endpoint.isGivenInBlockForm = false;

        const int numColorEndpointValues = computeNumColorEndpointValues(
            &blockParams.colorEndpointModes[0], blockParams.numPartitions, blockParams.isMultiPartSingleCemMode);
        const int numBitsForColorEndpoints = computeNumBitsForColorEndpoints(blockParams);
        const ISEParams &colorEndpointISEParams =
            computeMaximumRangeISEParams(numBitsForColorEndpoints, numColorEndpointValues);
        const int colorEndpointRangeMax = computeISERangeMax(colorEndpointISEParams);

        for (int i = 0; i < numColorEndpointValues; i++)
            result.endpoint.value.plain[i] =
                (i * colorEndpointRangeMax + (numColorEndpointValues - 1) / 2) / (numColorEndpointValues - 1);
    }

    return result;
}

static const ISEParams s_weightISEParamsCandidates[] = {
    ISEParams(ISEMODE_PLAIN_BIT, 1), ISEParams(ISEMODE_TRIT, 0), ISEParams(ISEMODE_PLAIN_BIT, 2),
    ISEParams(ISEMODE_QUINT, 0),     ISEParams(ISEMODE_TRIT, 1), ISEParams(ISEMODE_PLAIN_BIT, 3),
    ISEParams(ISEMODE_QUINT, 1),     ISEParams(ISEMODE_TRIT, 2), ISEParams(ISEMODE_PLAIN_BIT, 4),
    ISEParams(ISEMODE_QUINT, 2),     ISEParams(ISEMODE_TRIT, 3), ISEParams(ISEMODE_PLAIN_BIT, 5)};

void generateRandomBlock(uint8_t *dst, const IVec3 &blockSize, de::Random &rnd)
{
    if (rnd.getFloat() < 0.1f)
    {
        // Void extent block.
        const bool isVoidExtentHDR = rnd.getBool();
        const uint16_t r = isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : (uint16_t)rnd.getInt(0, 0xffff);
        const uint16_t g = isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : (uint16_t)rnd.getInt(0, 0xffff);
        const uint16_t b = isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : (uint16_t)rnd.getInt(0, 0xffff);
        const uint16_t a = isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : (uint16_t)rnd.getInt(0, 0xffff);
        generateVoidExtentBlock(VoidExtentParams(isVoidExtentHDR, r, g, b, a)).assignToMemory(dst);
    }
    else
    {
        // Not void extent block.

        // Generate block params.

        NormalBlockParams blockParams;

        do
        {
            blockParams.weightGridWidth  = rnd.getInt(2, blockSize.x());
            blockParams.weightGridHeight = rnd.getInt(2, blockSize.y());
            blockParams.weightGridDepth  = blockSize.z() > 1 ? rnd.getInt(2, blockSize.z()) : 1;
            blockParams.weightISEParams =
                s_weightISEParamsCandidates[rnd.getInt(0, DE_LENGTH_OF_ARRAY(s_weightISEParamsCandidates) - 1)];
            blockParams.numPartitions            = rnd.getInt(1, 4);
            blockParams.isMultiPartSingleCemMode = rnd.getFloat() < 0.25f;
            blockParams.isDualPlane              = blockParams.numPartitions != 4 && rnd.getBool();
            blockParams.ccs                      = rnd.getInt(0, 3);
            blockParams.partitionSeed            = rnd.getInt(0, 1023);

            blockParams.colorEndpointModes[0] = rnd.getInt(0, 15);

            {
                const int cemDiff = blockParams.isMultiPartSingleCemMode    ? 0 :
                                    blockParams.colorEndpointModes[0] == 0  ? 1 :
                                    blockParams.colorEndpointModes[0] == 15 ? -1 :
                                    rnd.getBool()                           ? 1 :
                                                                              -1;

                for (int i = 1; i < blockParams.numPartitions; i++)
                    blockParams.colorEndpointModes[i] =
                        blockParams.colorEndpointModes[0] + (cemDiff == -1 ? rnd.getInt(-1, 0) :
                                                             cemDiff == 1  ? rnd.getInt(0, 1) :
                                                                             0);
            }
        } while (!isValidBlockParams(blockParams, blockSize.x(), blockSize.y(), blockSize.z()));

        // Generate ISE inputs for both weight and endpoint data.

        NormalBlockISEInputs iseInputs;

        for (int weightOrEndpoints = 0; weightOrEndpoints <= 1; weightOrEndpoints++)
        {
            const bool setWeights = weightOrEndpoints == 0;
            const int numValues   = setWeights ? computeNumWeights(blockParams) :
                                                 computeNumColorEndpointValues(&blockParams.colorEndpointModes[0],
                                                                               blockParams.numPartitions,
                                                                               blockParams.isMultiPartSingleCemMode);
            const ISEParams iseParams =
                setWeights ? blockParams.weightISEParams :
                             computeMaximumRangeISEParams(computeNumBitsForColorEndpoints(blockParams), numValues);
            ISEInput &iseInput = setWeights ? iseInputs.weight : iseInputs.endpoint;

            iseInput.isGivenInBlockForm = rnd.getBool();

            if (iseInput.isGivenInBlockForm)
            {
                const int numValuesPerISEBlock = iseParams.mode == ISEMODE_TRIT  ? 5 :
                                                 iseParams.mode == ISEMODE_QUINT ? 3 :
                                                                                   1;
                const int iseBitMax            = (1 << iseParams.numBits) - 1;
                const int numISEBlocks         = deDivRoundUp32(numValues, numValuesPerISEBlock);

                for (int iseBlockNdx = 0; iseBlockNdx < numISEBlocks; iseBlockNdx++)
                {
                    iseInput.value.block[iseBlockNdx].tOrQValue = rnd.getInt(0, 255);
                    for (int i = 0; i < numValuesPerISEBlock; i++)
                        iseInput.value.block[iseBlockNdx].bitValues[i] = rnd.getInt(0, iseBitMax);
                }
            }
            else
            {
                const int rangeMax = computeISERangeMax(iseParams);

                for (int valueNdx = 0; valueNdx < numValues; valueNdx++)
                    iseInput.value.plain[valueNdx] = rnd.getInt(0, rangeMax);
            }
        }

        generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(), iseInputs).assignToMemory(dst);
    }
}

} // namespace

// Generate block data for a given BlockTestType and format.
void generateBlockCaseTestData(vector<uint8_t> &dst, CompressedTexFormat format, BlockTestType testType)
{
    DE_ASSERT(isAstcFormat(format));
    DE_ASSERT(!(isAstcSRGBFormat(format) && isBlockTestTypeHDROnly(testType)));
    const bool isAstc3D = isAstc3DFormat(format);

    const IVec3 blockSize = getBlockPixelSize(format);

    switch (testType)
    {
    case BLOCK_TEST_TYPE_VOID_EXTENT_LDR:
        // Generate a gradient-like set of LDR void-extent blocks.
        {
            const int numBlocks      = 1 << 13;
            const uint32_t numValues = 1 << 16;
            dst.reserve(numBlocks * BLOCK_SIZE_BYTES);

            for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
            {
                const uint32_t baseValue = blockNdx * (numValues - 1) / (numBlocks - 1);
                const uint16_t r         = (uint16_t)((baseValue + numValues * 0 / 4) % numValues);
                const uint16_t g         = (uint16_t)((baseValue + numValues * 1 / 4) % numValues);
                const uint16_t b         = (uint16_t)((baseValue + numValues * 2 / 4) % numValues);
                const uint16_t a         = (uint16_t)((baseValue + numValues * 3 / 4) % numValues);
                AssignBlock128 block;

                generateVoidExtentBlock(VoidExtentParams(false, r, g, b, a)).pushBytesToVector(dst);
            }

            break;
        }

    case BLOCK_TEST_TYPE_VOID_EXTENT_HDR:
        // Generate a gradient-like set of HDR void-extent blocks, with values ranging from the largest finite negative to largest finite positive of fp16.
        {
            const float minValue = -65504.0f;
            const float maxValue = +65504.0f;
            const int numBlocks  = 1 << 13;
            dst.reserve(numBlocks * BLOCK_SIZE_BYTES);

            for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
            {
                const int rNdx = (blockNdx + numBlocks * 0 / 4) % numBlocks;
                const int gNdx = (blockNdx + numBlocks * 1 / 4) % numBlocks;
                const int bNdx = (blockNdx + numBlocks * 2 / 4) % numBlocks;
                const int aNdx = (blockNdx + numBlocks * 3 / 4) % numBlocks;
                const deFloat16 r =
                    deFloat32To16(minValue + (float)rNdx * (maxValue - minValue) / (float)(numBlocks - 1));
                const deFloat16 g =
                    deFloat32To16(minValue + (float)gNdx * (maxValue - minValue) / (float)(numBlocks - 1));
                const deFloat16 b =
                    deFloat32To16(minValue + (float)bNdx * (maxValue - minValue) / (float)(numBlocks - 1));
                const deFloat16 a =
                    deFloat32To16(minValue + (float)aNdx * (maxValue - minValue) / (float)(numBlocks - 1));

                generateVoidExtentBlock(VoidExtentParams(true, r, g, b, a)).pushBytesToVector(dst);
            }

            break;
        }

    case BLOCK_TEST_TYPE_WEIGHT_GRID:
        // Generate different combinations of plane count, weight ISE params, and grid size.
        {
            for (int isDualPlane = 0; isDualPlane <= 1; isDualPlane++)
                for (int iseParamsNdx = 0; iseParamsNdx < DE_LENGTH_OF_ARRAY(s_weightISEParamsCandidates);
                     iseParamsNdx++)
                    for (int weightGridDepth = (isAstc3D ? 2 : 1); weightGridDepth <= (isAstc3D ? 6 : 1);
                         weightGridDepth++)
                        for (int weightGridWidth = 2; weightGridWidth <= 12; weightGridWidth++)
                            for (int weightGridHeight = 2; weightGridHeight <= 12; weightGridHeight++)
                            {
                                NormalBlockParams blockParams;
                                NormalBlockISEInputs iseInputs;

                                blockParams.weightGridWidth       = weightGridWidth;
                                blockParams.weightGridHeight      = weightGridHeight;
                                blockParams.weightGridDepth       = weightGridDepth;
                                blockParams.isDualPlane           = isDualPlane != 0;
                                blockParams.weightISEParams       = s_weightISEParamsCandidates[iseParamsNdx];
                                blockParams.ccs                   = 0;
                                blockParams.numPartitions         = 1;
                                blockParams.colorEndpointModes[0] = 0;

                                if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y(), blockSize.z()))
                                    generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(),
                                                        generateDefaultISEInputs(blockParams))
                                        .pushBytesToVector(dst);
                            }

            break;
        }

    case BLOCK_TEST_TYPE_WEIGHT_ISE:
        // For each weight ISE param set, generate blocks that cover:
        // - each single value of the ISE's range, at each position inside an ISE block
        // - for trit and quint ISEs, each single T or Q value of an ISE block
        {
            for (int iseParamsNdx = 0; iseParamsNdx < DE_LENGTH_OF_ARRAY(s_weightISEParamsCandidates); iseParamsNdx++)
            {
                const ISEParams &iseParams = s_weightISEParamsCandidates[iseParamsNdx];
                NormalBlockParams blockParams;

                blockParams.weightGridWidth  = 4;
                blockParams.weightGridHeight = 4;
                blockParams.weightGridDepth  = isAstc3D ? 4 : 1;
                blockParams.weightISEParams  = iseParams;
                blockParams.numPartitions    = 1;
                blockParams.isDualPlane =
                    blockParams.weightGridWidth * blockParams.weightGridHeight < 24 ? true : false;
                blockParams.ccs                   = 0;
                blockParams.colorEndpointModes[0] = 0;

                while (!isValidBlockParams(blockParams, blockSize.x(), blockSize.y(), blockSize.z()))
                {
                    blockParams.weightGridWidth--;
                    blockParams.weightGridHeight--;
                    if (isAstc3D)
                        blockParams.weightGridDepth--;
                }

                const int numValuesInISEBlock = iseParams.mode == ISEMODE_TRIT  ? 5 :
                                                iseParams.mode == ISEMODE_QUINT ? 3 :
                                                                                  1;
                const int numWeights          = computeNumWeights(blockParams);

                {
                    const int numWeightValues           = (int)computeISERangeMax(iseParams) + 1;
                    const int numBlocks                 = deDivRoundUp32(numWeightValues, numWeights);
                    NormalBlockISEInputs iseInputs      = generateDefaultISEInputs(blockParams);
                    iseInputs.weight.isGivenInBlockForm = false;

                    for (int offset = 0; offset < numValuesInISEBlock; offset++)
                        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
                        {
                            for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
                                iseInputs.weight.value.plain[weightNdx] =
                                    (blockNdx * numWeights + weightNdx + offset) % numWeightValues;

                            generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(), iseInputs)
                                .pushBytesToVector(dst);
                        }
                }

                if (iseParams.mode == ISEMODE_TRIT || iseParams.mode == ISEMODE_QUINT)
                {
                    NormalBlockISEInputs iseInputs      = generateDefaultISEInputs(blockParams);
                    iseInputs.weight.isGivenInBlockForm = true;

                    const int numTQValues          = 1 << (iseParams.mode == ISEMODE_TRIT ? 8 : 7);
                    const int numISEBlocksPerBlock = deDivRoundUp32(numWeights, numValuesInISEBlock);
                    const int numBlocks            = deDivRoundUp32(numTQValues, numISEBlocksPerBlock);

                    for (int offset = 0; offset < numValuesInISEBlock; offset++)
                        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
                        {
                            for (int iseBlockNdx = 0; iseBlockNdx < numISEBlocksPerBlock; iseBlockNdx++)
                            {
                                for (int i = 0; i < numValuesInISEBlock; i++)
                                    iseInputs.weight.value.block[iseBlockNdx].bitValues[i] = 0;
                                iseInputs.weight.value.block[iseBlockNdx].tOrQValue =
                                    (blockNdx * numISEBlocksPerBlock + iseBlockNdx + offset) % numTQValues;
                            }

                            generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(), iseInputs)
                                .pushBytesToVector(dst);
                        }
                }
            }

            break;
        }

    case BLOCK_TEST_TYPE_CEMS:
        // For each plane count & partition count combination, generate all color endpoint mode combinations.
        {
            for (int isDualPlane = 0; isDualPlane <= 1; isDualPlane++)
                for (int numPartitions = 1; numPartitions <= (isDualPlane != 0 ? 3 : 4); numPartitions++)
                {
                    // Multi-partition, single-CEM mode.
                    if (numPartitions > 1)
                    {
                        for (uint32_t singleCem = 0; singleCem < 16; singleCem++)
                        {
                            NormalBlockParams blockParams;
                            blockParams.weightGridWidth          = 4;
                            blockParams.weightGridHeight         = 4;
                            blockParams.weightGridDepth          = isAstc3D ? 4 : 1;
                            blockParams.isDualPlane              = isDualPlane != 0;
                            blockParams.ccs                      = 0;
                            blockParams.numPartitions            = numPartitions;
                            blockParams.isMultiPartSingleCemMode = true;
                            blockParams.colorEndpointModes[0]    = singleCem;
                            blockParams.partitionSeed            = 634;

                            for (int iseParamsNdx = 0; iseParamsNdx < DE_LENGTH_OF_ARRAY(s_weightISEParamsCandidates);
                                 iseParamsNdx++)
                            {
                                blockParams.weightISEParams = s_weightISEParamsCandidates[iseParamsNdx];
                                if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y(), blockSize.z()))
                                {
                                    generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(),
                                                        generateDefaultISEInputs(blockParams))
                                        .pushBytesToVector(dst);
                                    break;
                                }
                            }
                        }
                    }

                    // Separate-CEM mode.
                    for (uint32_t cem0 = 0; cem0 < 16; cem0++)
                        for (uint32_t cem1 = 0; cem1 < (numPartitions >= 2 ? 16u : 1u); cem1++)
                            for (uint32_t cem2 = 0; cem2 < (numPartitions >= 3 ? 16u : 1u); cem2++)
                                for (uint32_t cem3 = 0; cem3 < (numPartitions >= 4 ? 16u : 1u); cem3++)
                                {
                                    NormalBlockParams blockParams;
                                    blockParams.weightGridWidth          = 4;
                                    blockParams.weightGridHeight         = 4;
                                    blockParams.weightGridDepth          = isAstc3D ? 4 : 1;
                                    blockParams.isDualPlane              = isDualPlane != 0;
                                    blockParams.ccs                      = 0;
                                    blockParams.numPartitions            = numPartitions;
                                    blockParams.isMultiPartSingleCemMode = false;
                                    blockParams.colorEndpointModes[0]    = cem0;
                                    blockParams.colorEndpointModes[1]    = cem1;
                                    blockParams.colorEndpointModes[2]    = cem2;
                                    blockParams.colorEndpointModes[3]    = cem3;
                                    blockParams.partitionSeed            = 634;

                                    {
                                        const uint32_t minCem =
                                            *std::min_element(&blockParams.colorEndpointModes[0],
                                                              &blockParams.colorEndpointModes[numPartitions]);
                                        const uint32_t maxCem =
                                            *std::max_element(&blockParams.colorEndpointModes[0],
                                                              &blockParams.colorEndpointModes[numPartitions]);
                                        const uint32_t minCemClass = minCem / 4;
                                        const uint32_t maxCemClass = maxCem / 4;

                                        if (maxCemClass - minCemClass > 1)
                                            continue;
                                    }

                                    for (int iseParamsNdx = 0;
                                         iseParamsNdx < DE_LENGTH_OF_ARRAY(s_weightISEParamsCandidates); iseParamsNdx++)
                                    {
                                        blockParams.weightISEParams = s_weightISEParamsCandidates[iseParamsNdx];
                                        if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y(),
                                                               blockSize.z()))
                                        {
                                            generateNormalBlock(blockParams, blockSize.x(), blockSize.y(),
                                                                blockSize.z(), generateDefaultISEInputs(blockParams))
                                                .pushBytesToVector(dst);
                                            break;
                                        }
                                    }
                                }
                }

            break;
        }

    case BLOCK_TEST_TYPE_PARTITION_SEED:
        // Test all partition seeds ("partition pattern indices").
        {
            for (int numPartitions = 2; numPartitions <= 4; numPartitions++)
                for (uint32_t partitionSeed = 0; partitionSeed < 1 << 10; partitionSeed++)
                {
                    NormalBlockParams blockParams;
                    blockParams.weightGridWidth          = 4;
                    blockParams.weightGridHeight         = 4;
                    blockParams.weightGridDepth          = isAstc3D ? 4 : 1;
                    blockParams.weightISEParams          = ISEParams(ISEMODE_PLAIN_BIT, 2);
                    blockParams.isDualPlane              = false;
                    blockParams.numPartitions            = numPartitions;
                    blockParams.isMultiPartSingleCemMode = true;
                    blockParams.colorEndpointModes[0]    = 0;
                    blockParams.partitionSeed            = partitionSeed;

                    generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(),
                                        generateDefaultISEInputs(blockParams))
                        .pushBytesToVector(dst);
                }

            break;
        }

    // \note Fall-through.
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_LDR:
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15:
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15:
        // For each endpoint mode, for each pair of components in the endpoint value, test 10x10 combinations of values for that pair.
        // \note Separate modes for HDR and mode 15 due to different color scales and biases.
        {
            for (uint32_t cem = 0; cem < 16; cem++)
            {
                const bool isHDRCem = cem == 2 || cem == 3 || cem == 7 || cem == 11 || cem == 14 || cem == 15;

                if ((testType == BLOCK_TEST_TYPE_ENDPOINT_VALUE_LDR && isHDRCem) ||
                    (testType == BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15 && (!isHDRCem || cem == 15)) ||
                    (testType == BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15 && cem != 15))
                    continue;

                NormalBlockParams blockParams;
                blockParams.weightGridWidth       = 3;
                blockParams.weightGridHeight      = 4;
                blockParams.weightGridDepth       = 1;
                blockParams.weightISEParams       = ISEParams(ISEMODE_PLAIN_BIT, 2);
                blockParams.isDualPlane           = false;
                blockParams.numPartitions         = 1;
                blockParams.colorEndpointModes[0] = cem;

                {
                    const int numBitsForEndpoints = computeNumBitsForColorEndpoints(blockParams);
                    const int numEndpointParts    = computeNumColorEndpointValues(cem);
                    const ISEParams endpointISE   = computeMaximumRangeISEParams(numBitsForEndpoints, numEndpointParts);
                    const int endpointISERangeMax = computeISERangeMax(endpointISE);

                    for (int endpointPartNdx0 = 0; endpointPartNdx0 < numEndpointParts; endpointPartNdx0++)
                        for (int endpointPartNdx1 = endpointPartNdx0 + 1; endpointPartNdx1 < numEndpointParts;
                             endpointPartNdx1++)
                        {
                            NormalBlockISEInputs iseInputs = generateDefaultISEInputs(blockParams);
                            const int numEndpointValues    = de::min(10, endpointISERangeMax + 1);

                            for (int endpointValueNdx0 = 0; endpointValueNdx0 < numEndpointValues; endpointValueNdx0++)
                                for (int endpointValueNdx1 = 0; endpointValueNdx1 < numEndpointValues;
                                     endpointValueNdx1++)
                                {
                                    const int endpointValue0 =
                                        endpointValueNdx0 * endpointISERangeMax / (numEndpointValues - 1);
                                    const int endpointValue1 =
                                        endpointValueNdx1 * endpointISERangeMax / (numEndpointValues - 1);

                                    iseInputs.endpoint.value.plain[endpointPartNdx0] = endpointValue0;
                                    iseInputs.endpoint.value.plain[endpointPartNdx1] = endpointValue1;

                                    generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(),
                                                        iseInputs)
                                        .pushBytesToVector(dst);
                                }
                        }
                }
            }

            break;
        }

    case BLOCK_TEST_TYPE_ENDPOINT_ISE:
        // Similar to BLOCK_TEST_TYPE_WEIGHT_ISE, see above.
        {
            static const uint32_t endpointRangeMaximums[] = {5, 9, 11, 19, 23, 39, 47, 79, 95, 159, 191};

            for (int endpointRangeNdx = 0; endpointRangeNdx < DE_LENGTH_OF_ARRAY(endpointRangeMaximums);
                 endpointRangeNdx++)
            {
                bool validCaseGenerated = false;

                for (int numPartitions = 1; !validCaseGenerated && numPartitions <= 4; numPartitions++)
                    for (int isDual = 0; !validCaseGenerated && isDual <= 1; isDual++)
                        for (int weightISEParamsNdx = 0;
                             !validCaseGenerated &&
                             weightISEParamsNdx < DE_LENGTH_OF_ARRAY(s_weightISEParamsCandidates);
                             weightISEParamsNdx++)
                            for (int weightGridDepth = (isAstc3D ? 2 : 1);
                                 !validCaseGenerated && weightGridDepth <= (isAstc3D ? 6 : 1); weightGridDepth++)
                                for (int weightGridWidth = 2; !validCaseGenerated && weightGridWidth <= 12;
                                     weightGridWidth++)
                                    for (int weightGridHeight = 2; !validCaseGenerated && weightGridHeight <= 12;
                                         weightGridHeight++)
                                    {
                                        NormalBlockParams blockParams;
                                        blockParams.weightGridWidth  = weightGridWidth;
                                        blockParams.weightGridHeight = weightGridHeight;
                                        blockParams.weightGridDepth  = weightGridDepth;
                                        blockParams.weightISEParams  = s_weightISEParamsCandidates[weightISEParamsNdx];
                                        blockParams.isDualPlane      = isDual != 0;
                                        blockParams.ccs              = 0;
                                        blockParams.numPartitions    = numPartitions;
                                        blockParams.isMultiPartSingleCemMode = true;
                                        blockParams.colorEndpointModes[0]    = 12;
                                        blockParams.partitionSeed            = 634;

                                        if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y(),
                                                               blockSize.z()))
                                        {
                                            const ISEParams endpointISEParams = computeMaximumRangeISEParams(
                                                computeNumBitsForColorEndpoints(blockParams),
                                                computeNumColorEndpointValues(&blockParams.colorEndpointModes[0],
                                                                              numPartitions, true));

                                            if (computeISERangeMax(endpointISEParams) ==
                                                endpointRangeMaximums[endpointRangeNdx])
                                            {
                                                validCaseGenerated = true;

                                                const int numColorEndpoints = computeNumColorEndpointValues(
                                                    &blockParams.colorEndpointModes[0], numPartitions,
                                                    blockParams.isMultiPartSingleCemMode);
                                                const int numValuesInISEBlock =
                                                    endpointISEParams.mode == ISEMODE_TRIT  ? 5 :
                                                    endpointISEParams.mode == ISEMODE_QUINT ? 3 :
                                                                                              1;

                                                {
                                                    const int numColorEndpointValues =
                                                        (int)computeISERangeMax(endpointISEParams) + 1;
                                                    const int numBlocks =
                                                        deDivRoundUp32(numColorEndpointValues, numColorEndpoints);
                                                    NormalBlockISEInputs iseInputs =
                                                        generateDefaultISEInputs(blockParams);
                                                    iseInputs.endpoint.isGivenInBlockForm = false;

                                                    for (int offset = 0; offset < numValuesInISEBlock; offset++)
                                                        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
                                                        {
                                                            for (int endpointNdx = 0; endpointNdx < numColorEndpoints;
                                                                 endpointNdx++)
                                                                iseInputs.endpoint.value.plain[endpointNdx] =
                                                                    (blockNdx * numColorEndpoints + endpointNdx +
                                                                     offset) %
                                                                    numColorEndpointValues;

                                                            generateNormalBlock(blockParams, blockSize.x(),
                                                                                blockSize.y(), blockSize.z(), iseInputs)
                                                                .pushBytesToVector(dst);
                                                        }
                                                }

                                                if (endpointISEParams.mode == ISEMODE_TRIT ||
                                                    endpointISEParams.mode == ISEMODE_QUINT)
                                                {
                                                    NormalBlockISEInputs iseInputs =
                                                        generateDefaultISEInputs(blockParams);
                                                    iseInputs.endpoint.isGivenInBlockForm = true;

                                                    const int numTQValues =
                                                        1 << (endpointISEParams.mode == ISEMODE_TRIT ? 8 : 7);
                                                    const int numISEBlocksPerBlock =
                                                        deDivRoundUp32(numColorEndpoints, numValuesInISEBlock);
                                                    const int numBlocks =
                                                        deDivRoundUp32(numTQValues, numISEBlocksPerBlock);

                                                    for (int offset = 0; offset < numValuesInISEBlock; offset++)
                                                        for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
                                                        {
                                                            for (int iseBlockNdx = 0;
                                                                 iseBlockNdx < numISEBlocksPerBlock; iseBlockNdx++)
                                                            {
                                                                for (int i = 0; i < numValuesInISEBlock; i++)
                                                                    iseInputs.endpoint.value.block[iseBlockNdx]
                                                                        .bitValues[i] = 0;
                                                                iseInputs.endpoint.value.block[iseBlockNdx].tOrQValue =
                                                                    (blockNdx * numISEBlocksPerBlock + iseBlockNdx +
                                                                     offset) %
                                                                    numTQValues;
                                                            }

                                                            generateNormalBlock(blockParams, blockSize.x(),
                                                                                blockSize.y(), blockSize.z(), iseInputs)
                                                                .pushBytesToVector(dst);
                                                        }
                                                }
                                            }
                                        }
                                    }

                DE_ASSERT(validCaseGenerated);
            }

            break;
        }

    case BLOCK_TEST_TYPE_CCS:
        // For all partition counts, test all values of the CCS (color component selector).
        {
            for (int numPartitions = 1; numPartitions <= 3; numPartitions++)
                for (uint32_t ccs = 0; ccs < 4; ccs++)
                {
                    NormalBlockParams blockParams;
                    blockParams.weightGridWidth          = 3;
                    blockParams.weightGridHeight         = 3;
                    blockParams.weightGridDepth          = isAstc3D ? 3 : 1;
                    blockParams.weightISEParams          = ISEParams(ISEMODE_PLAIN_BIT, 2);
                    blockParams.isDualPlane              = true;
                    blockParams.ccs                      = ccs;
                    blockParams.numPartitions            = numPartitions;
                    blockParams.isMultiPartSingleCemMode = true;
                    blockParams.colorEndpointModes[0]    = 8;
                    blockParams.partitionSeed            = 634;

                    generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), blockSize.z(),
                                        generateDefaultISEInputs(blockParams))
                        .pushBytesToVector(dst);
                }

            break;
        }

    case BLOCK_TEST_TYPE_RANDOM:
        // Generate a number of random (including invalid) blocks.
        {
            const int numBlocks = 16384;
            const uint32_t seed = 1;

            dst.resize(numBlocks * BLOCK_SIZE_BYTES);

            generateRandomBlocks(&dst[0], numBlocks, format, seed);

            break;
        }

    default:
        DE_ASSERT(false);
    }
}

void generateRandomBlocks(uint8_t *dst, size_t numBlocks, CompressedTexFormat format, uint32_t seed)
{
    const IVec3 blockSize = getBlockPixelSize(format);
    de::Random rnd(seed);
    size_t numBlocksGenerated = 0;

    DE_ASSERT(isAstcFormat(format));
    DE_ASSERT(blockSize.z() == 1);

    for (numBlocksGenerated = 0; numBlocksGenerated < numBlocks; numBlocksGenerated++)
    {
        uint8_t *const curBlockPtr = dst + numBlocksGenerated * BLOCK_SIZE_BYTES;

        generateRandomBlock(curBlockPtr, blockSize, rnd);
    }
}

void generateRandomValidBlocks(uint8_t *dst, size_t numBlocks, CompressedTexFormat format,
                               TexDecompressionParams::AstcMode mode, uint32_t seed)
{
    const IVec3 blockSize = getBlockPixelSize(format);
    de::Random rnd(seed);
    size_t numBlocksGenerated = 0;

    DE_ASSERT(isAstcFormat(format));

    for (numBlocksGenerated = 0; numBlocksGenerated < numBlocks; numBlocksGenerated++)
    {
        uint8_t *const curBlockPtr = dst + numBlocksGenerated * BLOCK_SIZE_BYTES;

        do
        {
            generateRandomBlock(curBlockPtr, blockSize, rnd);
        } while (!isValidBlock(curBlockPtr, format, mode));
    }
}

// Generate a number of trivial blocks to fill unneeded space in a texture.
void generateDefaultVoidExtentBlocks(uint8_t *dst, size_t numBlocks)
{
    AssignBlock128 block = generateVoidExtentBlock(VoidExtentParams(false, 0, 0, 0, 0));
    for (size_t ndx = 0; ndx < numBlocks; ndx++)
        block.assignToMemory(&dst[ndx * BLOCK_SIZE_BYTES]);
}

void generateDefaultNormalBlocks(uint8_t *dst, size_t numBlocks, int blockWidth, int blockHeight, int blockDepth)
{
    NormalBlockParams blockParams;

    blockParams.weightGridWidth       = 3;
    blockParams.weightGridHeight      = 3;
    blockParams.weightGridDepth       = blockDepth > 1 ? 3 : 1;
    blockParams.weightISEParams       = ISEParams(ISEMODE_PLAIN_BIT, 5);
    blockParams.isDualPlane           = false;
    blockParams.numPartitions         = 1;
    blockParams.colorEndpointModes[0] = 8;

    NormalBlockISEInputs iseInputs      = generateDefaultISEInputs(blockParams);
    iseInputs.weight.isGivenInBlockForm = false;

    const int numWeights     = computeNumWeights(blockParams);
    const int weightRangeMax = computeISERangeMax(blockParams.weightISEParams);

    for (size_t blockNdx = 0; blockNdx < numBlocks; blockNdx++)
    {
        for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
            iseInputs.weight.value.plain[weightNdx] =
                (uint32_t)((blockNdx * numWeights + weightNdx) * weightRangeMax / (numBlocks * numWeights - 1));

        generateNormalBlock(blockParams, blockWidth, blockHeight, blockDepth, iseInputs)
            .assignToMemory(dst + blockNdx * BLOCK_SIZE_BYTES);
    }
}

bool isValidBlock(const uint8_t *data, CompressedTexFormat format, TexDecompressionParams::AstcMode mode)
{
    const tcu::IVec3 blockPixelSize = getBlockPixelSize(format);
    const bool isSRGB               = isAstcSRGBFormat(format);
    const bool isLDR                = isSRGB || mode == TexDecompressionParams::ASTCMODE_LDR;
    const bool is3D                 = isAstc3DFormat(format);

    // sRGB is not supported in HDR mode
    DE_ASSERT(!(mode == TexDecompressionParams::ASTCMODE_HDR && isSRGB));

    DecompressResult result;
    if (!is3D)
    {
        union
        {
            uint8_t sRGB[MAX_BLOCK_WIDTH * MAX_BLOCK_HEIGHT * 4];
            float linear[MAX_BLOCK_WIDTH * MAX_BLOCK_HEIGHT * 4];
        } tmpBuffer;
        const Block128 blockData(data);
        result = decompressBlock((isSRGB ? (void *)&tmpBuffer.sRGB[0] : (void *)&tmpBuffer.linear[0]), blockData,
                                 blockPixelSize.x(), blockPixelSize.y(), blockPixelSize.z(), isSRGB, isLDR);
    }
    else
    {
        union
        {
            uint8_t sRGB[MAX_BLOCK_WIDTH_3D * MAX_BLOCK_HEIGHT_3D * MAX_BLOCK_DEPTH_3D * 4];
            float linear[MAX_BLOCK_WIDTH_3D * MAX_BLOCK_HEIGHT_3D * MAX_BLOCK_DEPTH_3D * 4];
        } tmpBuffer;
        const Block128 blockData(data);
        result = decompressBlock((isSRGB ? (void *)&tmpBuffer.sRGB[0] : (void *)&tmpBuffer.linear[0]), blockData,
                                 blockPixelSize.x(), blockPixelSize.y(), blockPixelSize.z(), isSRGB, isLDR);
    }

    return result == DECOMPRESS_RESULT_VALID_BLOCK;
}

void decompress(const PixelBufferAccess &dst, const uint8_t *data, CompressedTexFormat format,
                TexDecompressionParams::AstcMode mode)
{
    const bool isSRGBFormat = isAstcSRGBFormat(format);

#if defined(DE_DEBUG)
    const tcu::IVec3 blockPixelSize = getBlockPixelSize(format);

    DE_ASSERT(dst.getWidth() == blockPixelSize.x() && dst.getHeight() == blockPixelSize.y());
    DE_ASSERT(blockPixelSize.z() == 1);
    DE_ASSERT(mode == TexDecompressionParams::ASTCMODE_LDR || mode == TexDecompressionParams::ASTCMODE_HDR);
#endif

    // sRGB is not supported in HDR mode
    DE_ASSERT(!(mode == TexDecompressionParams::ASTCMODE_HDR && isSRGBFormat));

    decompress(dst, data, isSRGBFormat, isSRGBFormat || mode == TexDecompressionParams::ASTCMODE_LDR);
}

void decompress3D(const PixelBufferAccess &dst, const uint8_t *data, CompressedTexFormat format,
                  TexDecompressionParams::AstcMode mode)
{
    const bool isSRGBFormat = isAstcSRGBFormat(format);
#if defined(DE_DEBUG)
    const tcu::IVec3 blockPixelSize = getBlockPixelSize(format);

    DE_ASSERT(dst.getWidth() == blockPixelSize.x() && dst.getHeight() == blockPixelSize.y() &&
              dst.getDepth() == blockPixelSize.z());
    DE_ASSERT(blockPixelSize.z() != 1);
    DE_ASSERT(mode == TexDecompressionParams::ASTCMODE_LDR || mode == TexDecompressionParams::ASTCMODE_HDR);
#endif

    // sRGB is not supported in HDR mode
    DE_ASSERT(!(mode == TexDecompressionParams::ASTCMODE_HDR && isSRGBFormat));
    // SFLOAT is not supported in LDR mode
    DE_ASSERT(!(mode == TexDecompressionParams::ASTCMODE_LDR && isAstcSFLOATFormat(format)));

    decompress3D(dst, data, isSRGBFormat, isSRGBFormat || mode == TexDecompressionParams::ASTCMODE_LDR);
}

const char *getBlockTestTypeName(BlockTestType testType)
{
    switch (testType)
    {
    case BLOCK_TEST_TYPE_VOID_EXTENT_LDR:
        return "void_extent_ldr";
    case BLOCK_TEST_TYPE_VOID_EXTENT_HDR:
        return "void_extent_hdr";
    case BLOCK_TEST_TYPE_WEIGHT_GRID:
        return "weight_grid";
    case BLOCK_TEST_TYPE_WEIGHT_ISE:
        return "weight_ise";
    case BLOCK_TEST_TYPE_CEMS:
        return "color_endpoint_modes";
    case BLOCK_TEST_TYPE_PARTITION_SEED:
        return "partition_pattern_index";
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_LDR:
        return "endpoint_value_ldr";
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15:
        return "endpoint_value_hdr_cem_not_15";
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15:
        return "endpoint_value_hdr_cem_15";
    case BLOCK_TEST_TYPE_ENDPOINT_ISE:
        return "endpoint_ise";
    case BLOCK_TEST_TYPE_CCS:
        return "color_component_selector";
    case BLOCK_TEST_TYPE_RANDOM:
        return "random";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *getBlockTestTypeDescription(BlockTestType testType)
{
    switch (testType)
    {
    case BLOCK_TEST_TYPE_VOID_EXTENT_LDR:
        return "Test void extent block, LDR mode";
    case BLOCK_TEST_TYPE_VOID_EXTENT_HDR:
        return "Test void extent block, HDR mode";
    case BLOCK_TEST_TYPE_WEIGHT_GRID:
        return "Test combinations of plane count, weight integer sequence encoding parameters, and weight grid size";
    case BLOCK_TEST_TYPE_WEIGHT_ISE:
        return "Test different integer sequence encoding block values for weight grid";
    case BLOCK_TEST_TYPE_CEMS:
        return "Test different color endpoint mode combinations, combined with different plane and partition counts";
    case BLOCK_TEST_TYPE_PARTITION_SEED:
        return "Test different partition pattern indices";
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_LDR:
        return "Test various combinations of each pair of color endpoint values, for each LDR color endpoint mode";
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15:
        return "Test various combinations of each pair of color endpoint values, for each HDR color endpoint mode "
               "other than mode 15";
    case BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15:
        return "Test various combinations of each pair of color endpoint values, HDR color endpoint mode 15";
    case BLOCK_TEST_TYPE_ENDPOINT_ISE:
        return "Test different integer sequence encoding block values for color endpoints";
    case BLOCK_TEST_TYPE_CCS:
        return "Test color component selector, for different partition counts";
    case BLOCK_TEST_TYPE_RANDOM:
        return "Random block test";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

bool isBlockTestTypeHDROnly(BlockTestType testType)
{
    return testType == BLOCK_TEST_TYPE_VOID_EXTENT_HDR || testType == BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15 ||
           testType == BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15;
}

Vec4 getBlockTestTypeColorScale(BlockTestType testType)
{
    switch (testType)
    {
    case tcu::astc::BLOCK_TEST_TYPE_VOID_EXTENT_HDR:
        return Vec4(0.5f / 65504.0f);
    case tcu::astc::BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_NO_15:
        return Vec4(1.0f / 65504.0f, 1.0f / 65504.0f, 1.0f / 65504.0f, 1.0f);
    case tcu::astc::BLOCK_TEST_TYPE_ENDPOINT_VALUE_HDR_15:
        return Vec4(1.0f / 65504.0f);
    default:
        return Vec4(1.0f);
    }
}

Vec4 getBlockTestTypeColorBias(BlockTestType testType)
{
    switch (testType)
    {
    case tcu::astc::BLOCK_TEST_TYPE_VOID_EXTENT_HDR:
        return Vec4(0.5f);
    default:
        return Vec4(0.0f);
    }
}

} // namespace astc
} // namespace tcu
