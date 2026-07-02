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

#include "deInt32.h"

DE_BEGIN_EXTERN_C

const int8_t g_clzLUT[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const int8_t g_ctzLUT[256] = {
    8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2,
    0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0,
    1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1,
    0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0,
    2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3,
    0, 1, 0, 2, 0, 1, 0, 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0,
    1, 0, 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0, 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

/*--------------------------------------------------------------------*//*!
 * \brief Compute the reciprocal of an integer.
 * \param a        Input value.
 * \param rcp    Pointer to resulting reciprocal "mantissa" value.
 * \param exp    Pointer to resulting exponent value.
 *
 * The returned value is split into an exponent part and a mantissa part for
 * practical reasons. The actual reciprocal can be computed with the formula:
 * result = exp2(exp) * rcp / (1<<DE_RCP_FRAC_BITS).
 *//*--------------------------------------------------------------------*/
void deRcp32(uint32_t a, uint32_t *rcp, int *exp)
{
    enum
    {
        RCP_LUT_BITS = 8
    };
    static const uint32_t s_rcpLUT[1 << RCP_LUT_BITS] = {
        0x40000000, 0x3fc03fc0, 0x3f80fe03, 0x3f423954, 0x3f03f03f, 0x3ec62159, 0x3e88cb3c, 0x3e4bec88, 0x3e0f83e0,
        0x3dd38ff0, 0x3d980f66, 0x3d5d00f5, 0x3d226357, 0x3ce8354b, 0x3cae7592, 0x3c7522f3, 0x3c3c3c3c, 0x3c03c03c,
        0x3bcbadc7, 0x3b9403b9, 0x3b5cc0ed, 0x3b25e446, 0x3aef6ca9, 0x3ab95900, 0x3a83a83a, 0x3a4e5947, 0x3a196b1e,
        0x39e4dcb8, 0x39b0ad12, 0x397cdb2c, 0x3949660a, 0x39164cb5, 0x38e38e38, 0x38b129a2, 0x387f1e03, 0x384d6a72,
        0x381c0e07, 0x37eb07dd, 0x37ba5713, 0x3789facb, 0x3759f229, 0x372a3c56, 0x36fad87b, 0x36cbc5c7, 0x369d0369,
        0x366e9095, 0x36406c80, 0x36129663, 0x35e50d79, 0x35b7d0ff, 0x358ae035, 0x355e3a5f, 0x3531dec0, 0x3505cca2,
        0x34da034d, 0x34ae820e, 0x34834834, 0x3458550f, 0x342da7f2, 0x34034034, 0x33d91d2a, 0x33af3e2e, 0x3385a29d,
        0x335c49d4, 0x33333333, 0x330a5e1b, 0x32e1c9f0, 0x32b97617, 0x329161f9, 0x32698cff, 0x3241f693, 0x321a9e24,
        0x31f3831f, 0x31cca4f5, 0x31a6031a, 0x317f9d00, 0x3159721e, 0x313381ec, 0x310dcbe1, 0x30e84f79, 0x30c30c30,
        0x309e0184, 0x30792ef5, 0x30549403, 0x30303030, 0x300c0300, 0x2fe80bfa, 0x2fc44aa2, 0x2fa0be82, 0x2f7d6724,
        0x2f5a4411, 0x2f3754d7, 0x2f149902, 0x2ef21023, 0x2ecfb9c8, 0x2ead9584, 0x2e8ba2e8, 0x2e69e18a, 0x2e4850fe,
        0x2e26f0db, 0x2e05c0b8, 0x2de4c02d, 0x2dc3eed6, 0x2da34c4d, 0x2d82d82d, 0x2d629215, 0x2d4279a2, 0x2d228e75,
        0x2d02d02d, 0x2ce33e6c, 0x2cc3d8d4, 0x2ca49f0a, 0x2c8590b2, 0x2c66ad71, 0x2c47f4ee, 0x2c2966d0, 0x2c0b02c0,
        0x2becc868, 0x2bceb771, 0x2bb0cf87, 0x2b931057, 0x2b75798c, 0x2b580ad6, 0x2b3ac3e2, 0x2b1da461, 0x2b00ac02,
        0x2ae3da78, 0x2ac72f74, 0x2aaaaaaa, 0x2a8e4bcd, 0x2a721291, 0x2a55fead, 0x2a3a0fd5, 0x2a1e45c2, 0x2a02a02a,
        0x29e71ec5, 0x29cbc14e, 0x29b0877d, 0x2995710e, 0x297a7dbb, 0x295fad40, 0x2944ff5a, 0x292a73c7, 0x29100a44,
        0x28f5c28f, 0x28db9c68, 0x28c1978f, 0x28a7b3c5, 0x288df0ca, 0x28744e61, 0x285acc4b, 0x28416a4c, 0x28282828,
        0x280f05a2, 0x27f6027f, 0x27dd1e85, 0x27c45979, 0x27abb323, 0x27932b48, 0x277ac1b2, 0x27627627, 0x274a4870,
        0x27323858, 0x271a45a6, 0x27027027, 0x26eab7a3, 0x26d31be7, 0x26bb9cbf, 0x26a439f6, 0x268cf359, 0x2675c8b6,
        0x265eb9da, 0x2647c694, 0x2630eeb1, 0x261a3202, 0x26039055, 0x25ed097b, 0x25d69d43, 0x25c04b80, 0x25aa1402,
        0x2593f69b, 0x257df31c, 0x2568095a, 0x25523925, 0x253c8253, 0x2526e4b7, 0x25116025, 0x24fbf471, 0x24e6a171,
        0x24d166f9, 0x24bc44e1, 0x24a73afd, 0x24924924, 0x247d6f2e, 0x2468acf1, 0x24540245, 0x243f6f02, 0x242af300,
        0x24168e18, 0x24024024, 0x23ee08fb, 0x23d9e878, 0x23c5de76, 0x23b1eace, 0x239e0d5b, 0x238a45f8, 0x23769480,
        0x2362f8cf, 0x234f72c2, 0x233c0233, 0x2328a701, 0x23156107, 0x23023023, 0x22ef1432, 0x22dc0d12, 0x22c91aa1,
        0x22b63cbe, 0x22a37347, 0x2290be1c, 0x227e1d1a, 0x226b9022, 0x22591713, 0x2246b1ce, 0x22346033, 0x22222222,
        0x220ff77c, 0x21fde021, 0x21ebdbf5, 0x21d9ead7, 0x21c80cab, 0x21b64151, 0x21a488ac, 0x2192e29f, 0x21814f0d,
        0x216fcdd8, 0x215e5ee4, 0x214d0214, 0x213bb74d, 0x212a7e72, 0x21195766, 0x21084210, 0x20f73e53, 0x20e64c14,
        0x20d56b38, 0x20c49ba5, 0x20b3dd40, 0x20a32fef, 0x20929398, 0x20820820, 0x20718d6f, 0x2061236a, 0x2050c9f8,
        0x20408102, 0x2030486c, 0x20202020, 0x20100804};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_rcpLUT) == (1 << RCP_LUT_BITS));

    int shift           = deClz32(a);
    uint32_t normalized = (uint32_t)a << shift; /* Highest bit is always 1. */
    int lookupNdx =
        (normalized >> (31 - RCP_LUT_BITS)) &
        ((1 << RCP_LUT_BITS) - 1); /* Discard high bit, leave 8 next highest bits to lowest bits of normalized. */
    uint32_t result;
    uint32_t tmp;

    /* Must be non-negative. */
    DE_ASSERT(a > 0);

    /* First approximation from lookup table. */
    DE_ASSERT(lookupNdx >= 0 && lookupNdx < (1 << RCP_LUT_BITS));
    result = s_rcpLUT[lookupNdx];

    /* Newton-Raphson iteration for improved accuracy (has 16 bits of accuracy afterwards). */
    tmp    = deSafeMuluAsr32(result, normalized, 31);
    result = deSafeMuluAsr32(result, (2u << DE_RCP_FRAC_BITS) - tmp, DE_RCP_FRAC_BITS);

    /* Second Newton-Raphson iteration (now has full accuracy). */
    tmp    = deSafeMuluAsr32(result, normalized, 31);
    result = deSafeMuluAsr32(result, (2u << DE_RCP_FRAC_BITS) - tmp, DE_RCP_FRAC_BITS);

    /* Return results. */
    *rcp = result;
    *exp = 31 - shift;
}

DE_END_EXTERN_C
