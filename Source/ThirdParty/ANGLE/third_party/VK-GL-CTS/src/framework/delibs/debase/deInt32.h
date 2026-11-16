#ifndef _DEINT32_H
#define _DEINT32_H
/*-------------------------------------------------------------------------
 * drawElements Base Portability Library
 * -------------------------------------
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
 * \brief 32-bit integer math.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_COMPILER == DE_COMPILER_MSC)
#include <intrin.h>
#endif

DE_BEGIN_EXTERN_C

enum
{
    DE_RCP_FRAC_BITS = 30 /*!< Number of fractional bits in deRcp32() result. */
};

void deRcp32(uint32_t a, uint32_t *rcp, int *exp);
void deInt32_computeLUTs(void);
void deInt32_selfTest(void);

/*--------------------------------------------------------------------*//*!
 * \brief Compute the absolute of an int.
 * \param a    Input value.
 * \return Absolute of the input value.
 *
 * \note The input 0x80000000u (for which the abs value cannot be
 * represented), is asserted and returns the value itself.
 *//*--------------------------------------------------------------------*/
static inline int deAbs32(int a)
{
    DE_ASSERT((unsigned int)a != 0x80000000u);
    return (a < 0) ? -a : a;
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute the signed minimum of two values.
 * \param a    First input value.
 * \param b Second input value.
 * \return The smallest of the two input values.
 *//*--------------------------------------------------------------------*/
static inline int deMin32(int a, int b)
{
    return (a <= b) ? a : b;
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute the signed maximum of two values.
 * \param a    First input value.
 * \param b Second input value.
 * \return The largest of the two input values.
 *//*--------------------------------------------------------------------*/
static inline int deMax32(int a, int b)
{
    return (a >= b) ? a : b;
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute the unsigned minimum of two values.
 * \param a    First input value.
 * \param b Second input value.
 * \return The smallest of the two input values.
 *//*--------------------------------------------------------------------*/
static inline uint32_t deMinu32(uint32_t a, uint32_t b)
{
    return (a <= b) ? a : b;
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute the unsigned minimum of two values.
 * \param a    First input value.
 * \param b Second input value.
 * \return The smallest of the two input values.
 *//*--------------------------------------------------------------------*/
static inline uint64_t deMinu64(uint64_t a, uint64_t b)
{
    return (a <= b) ? a : b;
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute the unsigned maximum of two values.
 * \param a    First input value.
 * \param b Second input value.
 * \return The largest of the two input values.
 *//*--------------------------------------------------------------------*/
static inline uint32_t deMaxu32(uint32_t a, uint32_t b)
{
    return (a >= b) ? a : b;
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a value is in the <b>inclusive<b> range [mn, mx].
 * \param a        Value to check for range.
 * \param mn    Range minimum value.
 * \param mx    Range maximum value.
 * \return True if (a >= mn) and (a <= mx), false otherwise.
 *
 * \see deInBounds32()
 *//*--------------------------------------------------------------------*/
static inline bool deInRange32(int a, int mn, int mx)
{
    return (a >= mn) && (a <= mx);
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a value is in the half-inclusive bounds [mn, mx[.
 * \param a        Value to check for range.
 * \param mn    Range minimum value.
 * \param mx    Range maximum value.
 * \return True if (a >= mn) and (a < mx), false otherwise.
 *
 * \see deInRange32()
 *//*--------------------------------------------------------------------*/
static inline bool deInBounds32(int a, int mn, int mx)
{
    return (a >= mn) && (a < mx);
}

/*--------------------------------------------------------------------*//*!
 * \brief Clamp a value into the range [mn, mx].
 * \param a        Value to clamp.
 * \param mn    Minimum value.
 * \param mx    Maximum value.
 * \return The clamped value in [mn, mx] range.
 *//*--------------------------------------------------------------------*/
static inline int deClamp32(int a, int mn, int mx)
{
    DE_ASSERT(mn <= mx);
    if (a < mn)
        return mn;
    if (a > mx)
        return mx;
    return a;
}

/*--------------------------------------------------------------------*//*!
 * \brief Get the sign of an integer.
 * \param a    Input value.
 * \return +1 if a>0, 0 if a==0, -1 if a<0.
 *//*--------------------------------------------------------------------*/
static inline int deSign32(int a)
{
    if (a > 0)
        return +1;
    if (a < 0)
        return -1;
    return 0;
}

/*--------------------------------------------------------------------*//*!
 * \brief Extract the sign bit of a.
 * \param a    Input value.
 * \return 0x80000000 if a<0, 0 otherwise.
 *//*--------------------------------------------------------------------*/
static inline int32_t deSignBit32(int32_t a)
{
    return (int32_t)((uint32_t)a & 0x80000000u);
}

/*--------------------------------------------------------------------*//*!
 * \brief Integer rotate right.
 * \param val    Value to rotate.
 * \param r        Number of bits to rotate (in range [0, 32]).
 * \return The rotated value.
 *//*--------------------------------------------------------------------*/
static inline int deRor32(int val, int r)
{
    DE_ASSERT(r >= 0 && r <= 32);
    if (r == 0 || r == 32)
        return val;
    else
        return (int)(((uint32_t)val >> r) | ((uint32_t)val << (32 - r)));
}

/*--------------------------------------------------------------------*//*!
 * \brief Integer rotate left.
 * \param val    Value to rotate.
 * \param r        Number of bits to rotate (in range [0, 32]).
 * \return The rotated value.
 *//*--------------------------------------------------------------------*/
static inline int deRol32(int val, int r)
{
    DE_ASSERT(r >= 0 && r <= 32);
    if (r == 0 || r == 32)
        return val;
    else
        return (int)(((uint32_t)val << r) | ((uint32_t)val >> (32 - r)));
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a value is a power-of-two.
 * \param a Input value.
 * \return True if input is a power-of-two value, false otherwise.
 *
 * \note Also returns true for zero.
 *//*--------------------------------------------------------------------*/
static inline bool deIsPowerOfTwo32(int a)
{
    return ((a & (a - 1)) == 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a value is a power-of-two.
 * \param a Input value.
 * \return True if input is a power-of-two value, false otherwise.
 *
 * \note Also returns true for zero.
 *//*--------------------------------------------------------------------*/
static inline bool deIsPowerOfTwo64(uint64_t a)
{
    return ((a & (a - 1ull)) == 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a value is a power-of-two.
 * \param a Input value.
 * \return True if input is a power-of-two value, false otherwise.
 *
 * \note Also returns true for zero.
 *//*--------------------------------------------------------------------*/
static inline bool deIsPowerOfTwoSize(size_t a)
{
#if (DE_PTR_SIZE == 4)
    return deIsPowerOfTwo32(a);
#elif (DE_PTR_SIZE == 8)
    return deIsPowerOfTwo64(a);
#else
#error "Invalid DE_PTR_SIZE"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Round a value up to a power-of-two.
 * \param a Input value.
 * \return Smallest power-of-two value that is greater or equal to an input value.
 *//*--------------------------------------------------------------------*/
static inline uint32_t deSmallestGreaterOrEquallPowerOfTwoU32(uint32_t a)
{
    --a;
    a |= a >> 1u;
    a |= a >> 2u;
    a |= a >> 4u;
    a |= a >> 8u;
    a |= a >> 16u;
    return ++a;
}

/*--------------------------------------------------------------------*//*!
 * \brief Round a value up to a power-of-two.
 * \param a Input value.
 * \return Smallest power-of-two value that is greater or equal to an input value.
 *//*--------------------------------------------------------------------*/
static inline uint64_t deSmallestGreaterOrEquallPowerOfTwoU64(uint64_t a)
{
    --a;
    a |= a >> 1u;
    a |= a >> 2u;
    a |= a >> 4u;
    a |= a >> 8u;
    a |= a >> 16u;
    a |= a >> 32u;
    return ++a;
}

/*--------------------------------------------------------------------*//*!
 * \brief Round a value up to a power-of-two.
 * \param a Input value.
 * \return Smallest power-of-two value that is greater or equal to an input value.
 *//*--------------------------------------------------------------------*/
static inline size_t deSmallestGreaterOrEquallPowerOfTwoSize(size_t a)
{
#if (DE_PTR_SIZE == 4)
    return deSmallestGreaterOrEquallPowerOfTwoU32(a);
#elif (DE_PTR_SIZE == 8)
    return deSmallestGreaterOrEquallPowerOfTwoU64(a);
#else
#error "Invalid DE_PTR_SIZE"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if an integer is aligned to given power-of-two size.
 * \param a        Input value.
 * \param align    Alignment to check for.
 * \return True if input is aligned, false otherwise.
 *//*--------------------------------------------------------------------*/
static inline bool deIsAligned32(int a, int align)
{
    DE_ASSERT(deIsPowerOfTwo32(align));
    return ((a & (align - 1)) == 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if an integer is aligned to given power-of-two size.
 * \param a        Input value.
 * \param align    Alignment to check for.
 * \return True if input is aligned, false otherwise.
 *//*--------------------------------------------------------------------*/
static inline bool deIsAligned64(int64_t a, int64_t align)
{
    DE_ASSERT(deIsPowerOfTwo64(align));
    return ((a & (align - 1)) == 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if a pointer is aligned to given power-of-two size.
 * \param ptr    Input pointer.
 * \param align    Alignment to check for (power-of-two).
 * \return True if input is aligned, false otherwise.
 *//*--------------------------------------------------------------------*/
static inline bool deIsAlignedPtr(const void *ptr, uintptr_t align)
{
    DE_ASSERT((align & (align - 1)) == 0); /* power of two */
    return (((uintptr_t)ptr & (align - 1)) == 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Align an integer to given power-of-two size.
 * \param val    Input to align.
 * \param align    Alignment to check for (power-of-two).
 * \return The aligned value (larger or equal to input).
 *//*--------------------------------------------------------------------*/
static inline int32_t deAlign32(int32_t val, int32_t align)
{
    DE_ASSERT(deIsPowerOfTwo32(align));
    return (val + align - 1) & ~(align - 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Align an integer to given power-of-two size.
 * \param val    Input to align.
 * \param align    Alignment to check for (power-of-two).
 * \return The aligned value (larger or equal to input).
 *//*--------------------------------------------------------------------*/
static inline int64_t deAlign64(int64_t val, int64_t align)
{
    DE_ASSERT(deIsPowerOfTwo64(align));
    return (val + align - 1) & ~(align - 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Align a pointer to given power-of-two size.
 * \param ptr    Input pointer to align.
 * \param align    Alignment to check for (power-of-two).
 * \return The aligned pointer (larger or equal to input).
 *//*--------------------------------------------------------------------*/
static inline void *deAlignPtr(void *ptr, uintptr_t align)
{
    uintptr_t val = (uintptr_t)ptr;
    DE_ASSERT((align & (align - 1)) == 0); /* power of two */
    return (void *)((val + align - 1) & ~(align - 1));
}

/*--------------------------------------------------------------------*//*!
 * \brief Align a size_t value to given power-of-two size.
 * \param ptr    Input value to align.
 * \param align    Alignment to check for (power-of-two).
 * \return The aligned size (larger or equal to input).
 *//*--------------------------------------------------------------------*/
static inline size_t deAlignSize(size_t val, size_t align)
{
    DE_ASSERT(deIsPowerOfTwoSize(align));
    return (val + align - 1) & ~(align - 1);
}

extern const int8_t g_clzLUT[256];

/*--------------------------------------------------------------------*//*!
 * \brief Compute number of leading zeros in an integer.
 * \param a    Input value.
 * \return The number of leading zero bits in the input.
 *//*--------------------------------------------------------------------*/
static inline int deClz32(uint32_t a)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    unsigned long i;
    if (_BitScanReverse(&i, (unsigned long)a) == 0)
        return 32;
    else
        return 31 - i;
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    if (a == 0)
        return 32;
    else
        return __builtin_clz((unsigned int)a);
#else
    if ((a & 0xFF000000u) != 0)
        return (int)g_clzLUT[a >> 24];
    if ((a & 0x00FF0000u) != 0)
        return 8 + (int)g_clzLUT[a >> 16];
    if ((a & 0x0000FF00u) != 0)
        return 16 + (int)g_clzLUT[a >> 8];
    return 24 + (int)g_clzLUT[a];
#endif
}

extern const int8_t g_ctzLUT[256];

/*--------------------------------------------------------------------*//*!
 * \brief Compute number of trailing zeros in an integer.
 * \param a    Input value.
 * \return The number of trailing zero bits in the input.
 *//*--------------------------------------------------------------------*/
static inline int deCtz32(uint32_t a)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    unsigned long i;
    if (_BitScanForward(&i, (unsigned long)a) == 0)
        return 32;
    else
        return i;
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    if (a == 0)
        return 32;
    else
        return __builtin_ctz((unsigned int)a);
#else
    if ((a & 0x00FFFFFFu) == 0)
        return (int)g_ctzLUT[a >> 24] + 24;
    if ((a & 0x0000FFFFu) == 0)
        return (int)g_ctzLUT[(a >> 16) & 0xffu] + 16;
    if ((a & 0x000000FFu) == 0)
        return (int)g_ctzLUT[(a >> 8) & 0xffu] + 8;
    return (int)g_ctzLUT[a & 0xffu];
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute integer 'floor' of 'log2' for a positive integer.
 * \param a    Input value.
 * \return floor(log2(a)).
 *//*--------------------------------------------------------------------*/
static inline int deLog2Floor32(int32_t a)
{
    DE_ASSERT(a > 0);
    return 31 - deClz32((uint32_t)a);
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute integer 'ceil' of 'log2' for a positive integer.
 * \param a    Input value.
 * \return ceil(log2(a)).
 *//*--------------------------------------------------------------------*/
static inline int deLog2Ceil32(int32_t a)
{
    int log2floor = deLog2Floor32(a);
    if (deIsPowerOfTwo32(a))
        return log2floor;
    else
        return log2floor + 1;
}

/*--------------------------------------------------------------------*//*!
 * \brief Compute the bit population count of an integer.
 * \param a    Input value.
 * \return The number of one bits in the input.
 *//*--------------------------------------------------------------------*/
static inline int dePop32(uint32_t a)
{
    uint32_t mask0 = 0x55555555; /* 1-bit values. */
    uint32_t mask1 = 0x33333333; /* 2-bit values. */
    uint32_t mask2 = 0x0f0f0f0f; /* 4-bit values. */
    uint32_t mask3 = 0x00ff00ff; /* 8-bit values. */
    uint32_t mask4 = 0x0000ffff; /* 16-bit values. */
    uint32_t t     = (uint32_t)a;
    t              = (t & mask0) + ((t >> 1) & mask0);
    t              = (t & mask1) + ((t >> 2) & mask1);
    t              = (t & mask2) + ((t >> 4) & mask2);
    t              = (t & mask3) + ((t >> 8) & mask3);
    t              = (t & mask4) + (t >> 16);
    return (int)t;
}

static inline int dePop64(uint64_t a)
{
    return dePop32((uint32_t)(a & 0xffffffffull)) + dePop32((uint32_t)(a >> 32));
}

/*--------------------------------------------------------------------*//*!
 * \brief Reverse bytes in 32-bit integer (for example MSB -> LSB).
 * \param a    Input value.
 * \return The input with bytes reversed
 *//*--------------------------------------------------------------------*/
static inline uint32_t deReverseBytes32(uint32_t v)
{
    uint32_t b0 = v << 24;
    uint32_t b1 = (v & 0x0000ff00) << 8;
    uint32_t b2 = (v & 0x00ff0000) >> 8;
    uint32_t b3 = v >> 24;
    return b0 | b1 | b2 | b3;
}

/*--------------------------------------------------------------------*//*!
 * \brief Reverse bytes in 16-bit integer (for example MSB -> LSB).
 * \param a    Input value.
 * \return The input with bytes reversed
 *//*--------------------------------------------------------------------*/
static inline uint16_t deReverseBytes16(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline int32_t deSafeMul32(int32_t a, int32_t b)
{
    int32_t res = a * b;
    DE_ASSERT((int64_t)res == ((int64_t)a * (int64_t)b));
    return res;
}

static inline int32_t deSafeAdd32(int32_t a, int32_t b)
{
    DE_ASSERT((int64_t)a + (int64_t)b == (int64_t)(a + b));
    return (a + b);
}

static inline int32_t deDivRoundUp32(int32_t a, int32_t b)
{
    return a / b + ((a % b) ? 1 : 0);
}

/*--------------------------------------------------------------------*//*!
 * \brief Return value a rounded up to nearest multiple of b.
 * \param a        Input value.
 * \param b        Alignment to use.
 * \return a if already aligned to b, otherwise next largest aligned value
 *//*--------------------------------------------------------------------*/
static inline int32_t deRoundUp32(int32_t a, int32_t b)
{
    int32_t d = a / b;
    return d * b == a ? a : (d + 1) * b;
}

static inline uint32_t deRoundUp32u(uint32_t a, uint32_t b)
{
    uint32_t d = a / b;
    return (d * b == a) ? a : (d + 1) * b;
}

/* \todo [petri] Move to int64_t.h? */

static inline int32_t deMulAsr32(int32_t a, int32_t b, int shift)
{
    return (int32_t)(((int64_t)a * (int64_t)b) >> shift);
}

static inline int32_t deSafeMulAsr32(int32_t a, int32_t b, int shift)
{
    int64_t res = ((int64_t)a * (int64_t)b) >> shift;
    DE_ASSERT(res == (int64_t)(int32_t)res);
    return (int32_t)res;
}

static inline uint32_t deSafeMuluAsr32(uint32_t a, uint32_t b, int shift)
{
    uint64_t res = ((uint64_t)a * (uint64_t)b) >> shift;
    DE_ASSERT(res == (uint64_t)(uint32_t)res);
    return (uint32_t)res;
}

static inline int64_t deMul32_32_64(int32_t a, int32_t b)
{
    return ((int64_t)a * (int64_t)b);
}

static inline int64_t deAbs64(int64_t a)
{
    DE_ASSERT((uint64_t)a != 0x8000000000000000LL);
    return (a >= 0) ? a : -a;
}

static inline int deClz64(uint64_t a)
{
    if ((a >> 32) != 0)
        return deClz32((uint32_t)(a >> 32));
    return deClz32((uint32_t)a) + 32;
}

/* Common hash & compare functions. */

static inline uint32_t deInt32Hash(int32_t a)
{
    /* From: http://www.concentric.net/~Ttwang/tech/inthash.htm */
    uint32_t key = (uint32_t)a;
    key          = (key ^ 61) ^ (key >> 16);
    key          = key + (key << 3);
    key          = key ^ (key >> 4);
    key          = key * 0x27d4eb2d; /* prime/odd constant */
    key          = key ^ (key >> 15);
    return key;
}

static inline uint32_t deInt64Hash(int64_t a)
{
    /* From: http://www.concentric.net/~Ttwang/tech/inthash.htm */
    uint64_t key = (uint64_t)a;
    key          = (~key) + (key << 21); /* key = (key << 21) - key - 1; */
    key          = key ^ (key >> 24);
    key          = (key + (key << 3)) + (key << 8); /* key * 265 */
    key          = key ^ (key >> 14);
    key          = (key + (key << 2)) + (key << 4); /* key * 21 */
    key          = key ^ (key >> 28);
    key          = key + (key << 31);
    return (uint32_t)key;
}

static inline uint32_t deInt16Hash(int16_t v)
{
    return deInt32Hash(v);
}
static inline uint32_t deUint16Hash(uint16_t v)
{
    return deInt32Hash((int32_t)v);
}
static inline uint32_t deUint32Hash(uint32_t v)
{
    return deInt32Hash((int32_t)v);
}
static inline uint32_t deUint64Hash(uint64_t v)
{
    return deInt64Hash((int64_t)v);
}

static inline bool deInt16Equal(int16_t a, int16_t b)
{
    return (a == b);
}
static inline bool deUint16Equal(uint16_t a, uint16_t b)
{
    return (a == b);
}
static inline bool deInt32Equal(int32_t a, int32_t b)
{
    return (a == b);
}
static inline bool deUint32Equal(uint32_t a, uint32_t b)
{
    return (a == b);
}
static inline bool deInt64Equal(int64_t a, int64_t b)
{
    return (a == b);
}
static inline bool deUint64Equal(uint64_t a, uint64_t b)
{
    return (a == b);
}

static inline uint32_t dePointerHash(const void *ptr)
{
    uintptr_t val = (uintptr_t)ptr;
#if (DE_PTR_SIZE == 4)
    return deInt32Hash((int)val);
#elif (DE_PTR_SIZE == 8)
    return deInt64Hash((int64_t)val);
#else
#error Unsupported pointer size.
#endif
}

static inline bool dePointerEqual(const void *a, const void *b)
{
    return (a == b);
}

/**
 *    \brief    Modulo that generates the same sign as divisor and rounds toward
 *            negative infinity -- assuming c99 %-operator.
 */
static inline int32_t deInt32ModF(int32_t n, int32_t d)
{
    int32_t r = n % d;
    if ((r > 0 && d < 0) || (r < 0 && d > 0))
        r = r + d;
    return r;
}

static inline bool deInt64InInt32Range(int64_t x)
{
    return ((x >= (((int64_t)((int32_t)(-0x7FFFFFFF - 1))))) && (x <= ((1ll << 31) - 1)));
}

static inline uint32_t deBitMask32(int leastSignificantBitNdx, int numBits)
{
    DE_ASSERT(deInRange32(leastSignificantBitNdx, 0, 32));
    DE_ASSERT(deInRange32(numBits, 0, 32));
    DE_ASSERT(deInRange32(leastSignificantBitNdx + numBits, 0, 32));

    if (numBits < 32 && leastSignificantBitNdx < 32)
        return ((1u << numBits) - 1u) << (uint32_t)leastSignificantBitNdx;
    else if (numBits == 0 && leastSignificantBitNdx == 32)
        return 0u;
    else
    {
        DE_ASSERT(numBits == 32 && leastSignificantBitNdx == 0);
        return 0xFFFFFFFFu;
    }
}

static inline uint32_t deUintMaxValue32(int numBits)
{
    DE_ASSERT(deInRange32(numBits, 1, 32));
    if (numBits < 32)
        return ((1u << numBits) - 1u);
    else
        return 0xFFFFFFFFu;
}

static inline int32_t deIntMaxValue32(int numBits)
{
    DE_ASSERT(deInRange32(numBits, 1, 32));
    if (numBits < 32)
        return ((int32_t)1 << (numBits - 1)) - 1;
    else
    {
        /* avoid undefined behavior of int overflow when shifting */
        return 0x7FFFFFFF;
    }
}

static inline int32_t deIntMinValue32(int numBits)
{
    DE_ASSERT(deInRange32(numBits, 1, 32));
    if (numBits < 32)
        return -((int32_t)1 << (numBits - 1));
    else
    {
        /* avoid undefined behavior of int overflow when shifting */
        return (int32_t)(-0x7FFFFFFF - 1);
    }
}

static inline int32_t deSignExtendTo32(int32_t value, int numBits)
{
    DE_ASSERT(deInRange32(numBits, 1, 32));

    if (numBits < 32)
    {
        bool signSet      = ((uint32_t)value & (1u << (numBits - 1))) != 0;
        uint32_t signMask = deBitMask32(numBits, 32 - numBits);

        DE_ASSERT(((uint32_t)value & signMask) == 0u);

        return (int32_t)((uint32_t)value | (signSet ? signMask : 0u));
    }
    else
        return value;
}

static inline int deIntIsPow2(int powerOf2)
{
    if (powerOf2 <= 0)
        return 0;
    return (powerOf2 & (powerOf2 - (int)1)) == (int)0;
}

static inline int deIntRoundToPow2(int number, int powerOf2)
{
    DE_ASSERT(deIntIsPow2(powerOf2));
    return (number + (int)powerOf2 - (int)1) & (int)(~(powerOf2 - 1));
}

/*--------------------------------------------------------------------*//*!
 * \brief Destructively loop over all of the bits in a mask as in:
 *
 *   while (mymask) {
 *     int i = bitScan(&mymask);
 *     ... process element i
 *   }
 * \param mask        mask value, it will remove LSB that is enabled.
 * \return LSB position that was enabled before overwriting the mask.
 *//*--------------------------------------------------------------------*/
static inline int32_t deInt32BitScan(int32_t *mask)
{
    const int32_t i = deCtz32(*mask);
    if (i == 32)
        return i;
    *mask ^= (1u << i);
    return i;
}

DE_END_EXTERN_C

#endif /* _DEINT32_H */
