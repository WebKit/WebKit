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
 * \brief Testing of int32_t functions.
 *//*--------------------------------------------------------------------*/

#include "deInt32.h"
#include "deRandom.h"

#include <stdio.h> /* printf() */

DE_BEGIN_EXTERN_C

void deInt32_computeLUTs(void)
{
    enum
    {
        RCP_LUT_BITS = 8
    };

    int ndx;

    printf("enum { RCP_LUT_BITS = %d };\n", RCP_LUT_BITS);
    printf("static const uint32_t s_rcpLUT[1<<RCP_LUT_BITS] =\n");
    printf("{\n");

    for (ndx = 0; ndx < (1 << RCP_LUT_BITS); ndx++)
    {
        uint32_t val = (1u << RCP_LUT_BITS) | (uint32_t)ndx;
        uint32_t rcp = (uint32_t)((1u << DE_RCP_FRAC_BITS) / ((double)val / (1 << RCP_LUT_BITS)));

        if ((ndx & 3) == 0)
            printf("\t");

        printf("0x%08x", rcp);

        if ((ndx & 3) == 3)
        {
            if (ndx != (1 << RCP_LUT_BITS) - 1)
                printf(",");
            printf("\n");
        }
        else
            printf(", ");
    }

    printf("};\n");
}

void deInt32_selfTest(void)
{
    const int NUM_ACCURATE_BITS = 29;

    deRandom rnd;
    uint32_t rcp;
    int exp;
    int numBits;

    deRandom_init(&rnd, 0xdeadbeefu - 1);

    /* Test deClz32(). */
    {
        int i;
        for (i = 0; i < 32; i++)
        {
            DE_TEST_ASSERT(deClz32(1u << i) == 31 - i);
            DE_TEST_ASSERT(deClz32((1u << i) | ((1u << i) - 1u)) == 31 - i);
        }
    }

    DE_TEST_ASSERT(deClz32(0) == 32);
    DE_TEST_ASSERT(deClz32(1) == 31);
    DE_TEST_ASSERT(deClz32(0xF1) == 24);
    DE_TEST_ASSERT(deClz32(0xBC12) == 16);
    DE_TEST_ASSERT(deClz32(0xABBACD) == 8);
    DE_TEST_ASSERT(deClz32(0x10000000) == 3);
    DE_TEST_ASSERT(deClz32(0x20000000) == 2);
    DE_TEST_ASSERT(deClz32(0x40000000) == 1);
    DE_TEST_ASSERT(deClz32(0x80000000) == 0);

    /* Test deCtz32(). */
    {
        int i;
        for (i = 0; i < 32; i++)
        {
            DE_TEST_ASSERT(deCtz32(1u << i) == i);
            DE_TEST_ASSERT(deCtz32(~((1u << i) - 1u)) == i);
        }
    }

    DE_TEST_ASSERT(deCtz32(0) == 32);
    DE_TEST_ASSERT(deCtz32(1) == 0);
    DE_TEST_ASSERT(deCtz32(0x3F4) == 2);
    DE_TEST_ASSERT(deCtz32(0x3F40) == 6);
    DE_TEST_ASSERT(deCtz32(0xFFFFFFFF) == 0);

    /* Test simple inputs for dePop32(). */
    DE_TEST_ASSERT(dePop32(0u) == 0);
    DE_TEST_ASSERT(dePop32(~0u) == 32);
    DE_TEST_ASSERT(dePop32(0xFF) == 8);
    DE_TEST_ASSERT(dePop32(0xFF00FF) == 16);
    DE_TEST_ASSERT(dePop32(0x3333333) == 14);
    DE_TEST_ASSERT(dePop32(0x33333333) == 16);

    /* dePop32(): Check exp2(N) values and inverses. */
    for (numBits = 0; numBits < 32; numBits++)
    {
        DE_TEST_ASSERT(dePop32(1u << numBits) == 1);
        DE_TEST_ASSERT(dePop32(~(1u << numBits)) == 31);
    }

    /* Check exp2(N) values. */
    for (numBits = 0; numBits < 32; numBits++)
    {
        uint32_t val = (1u << numBits);
        deRcp32(val, &rcp, &exp);

        DE_TEST_ASSERT(rcp == (1u << DE_RCP_FRAC_BITS));
        DE_TEST_ASSERT(exp == numBits);
    }

    /* Check random values. */
    for (numBits = 0; numBits < 32; numBits++)
    {
        int NUM_ITERS = deMax32(16, 1 << (numBits / 2));
        int iter;

        for (iter = 0; iter < NUM_ITERS; iter++)
        {
            const uint32_t EPS = 1u << (DE_RCP_FRAC_BITS - NUM_ACCURATE_BITS);

            uint32_t val = (deRandom_getUint32(&rnd) & ((1u << numBits) - 1)) | (1u << numBits);
            uint32_t ref =
                (uint32_t)(((1.0f / (double)val) * (double)(1 << DE_RCP_FRAC_BITS)) * (double)(1u << numBits));

            deRcp32(val, &rcp, &exp);

            DE_TEST_ASSERT(rcp >= ref - EPS && rcp < ref + EPS);
            DE_TEST_ASSERT(exp == numBits);
        }
    }

    DE_TEST_ASSERT(deBitMask32(0, 0) == 0);
    DE_TEST_ASSERT(deBitMask32(8, 0) == 0);
    DE_TEST_ASSERT(deBitMask32(16, 0) == 0);
    DE_TEST_ASSERT(deBitMask32(31, 0) == 0);
    DE_TEST_ASSERT(deBitMask32(32, 0) == 0);

    DE_TEST_ASSERT(deBitMask32(0, 2) == 3);
    DE_TEST_ASSERT(deBitMask32(0, 32) == 0xFFFFFFFFu);

    DE_TEST_ASSERT(deBitMask32(16, 16) == 0xFFFF0000u);
    DE_TEST_ASSERT(deBitMask32(31, 1) == 0x80000000u);
    DE_TEST_ASSERT(deBitMask32(8, 4) == 0xF00u);

    DE_TEST_ASSERT(deUintMaxValue32(1) == 1);
    DE_TEST_ASSERT(deUintMaxValue32(2) == 3);
    DE_TEST_ASSERT(deUintMaxValue32(32) == 0xFFFFFFFFu);

    DE_TEST_ASSERT(deIntMaxValue32(1) == 0);
    DE_TEST_ASSERT(deIntMaxValue32(2) == 1);
    DE_TEST_ASSERT(deIntMaxValue32(32) == 0x7FFFFFFF);

    DE_TEST_ASSERT(deIntMinValue32(1) == -1);
    DE_TEST_ASSERT(deIntMinValue32(2) == -2);
    DE_TEST_ASSERT(deIntMinValue32(32) == -0x7FFFFFFF - 1);

    DE_TEST_ASSERT(deSignExtendTo32((int)0x0, 1) == 0);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x1, 1) == (int)0xFFFFFFFF);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x3, 3) == 3);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x6, 3) == (int)0xFFFFFFFE);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x3, 4) == 3);
    DE_TEST_ASSERT(deSignExtendTo32((int)0xC, 4) == (int)0xFFFFFFFC);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x7FC3, 16) == (int)0x7FC3);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x84A0, 16) == (int)0xFFFF84A0);
    DE_TEST_ASSERT(deSignExtendTo32((int)0xFFC3, 17) == (int)0xFFC3);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x184A0, 17) == (int)0xFFFF84A0);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x7A016601, 32) == (int)0x7A016601);
    DE_TEST_ASSERT(deSignExtendTo32((int)0x8A016601, 32) == (int)0x8A016601);

    DE_TEST_ASSERT(deReverseBytes32(0x11223344) == 0x44332211);
    DE_TEST_ASSERT(deReverseBytes32(0xfecddeef) == 0xefdecdfe);
    DE_TEST_ASSERT(deReverseBytes16(0x1122) == 0x2211);
    DE_TEST_ASSERT(deReverseBytes16(0xdeef) == 0xefde);

    DE_TEST_ASSERT(deInt64InInt32Range((int64_t)0x7FFFFFF));
    DE_TEST_ASSERT(deInt64InInt32Range(0));
    DE_TEST_ASSERT(deInt64InInt32Range(1));
    DE_TEST_ASSERT(deInt64InInt32Range(-1));
    DE_TEST_ASSERT(deInt64InInt32Range(-((int64_t)0x7FFFFFF)));
    DE_TEST_ASSERT(deInt64InInt32Range(-((int64_t)0x8000 << 16)));
    DE_TEST_ASSERT(deInt64InInt32Range((int64_t)deIntMinValue32(32)));

    DE_TEST_ASSERT(!deInt64InInt32Range((((int64_t)0x7FFFFFF) << 32) | (int64_t)0xFFFFFFFF));
    DE_TEST_ASSERT(!deInt64InInt32Range((int64_t)0x7FFFFFFF + 1));
    DE_TEST_ASSERT(!deInt64InInt32Range(-((int64_t)0x7FFFFFFF + 2)));
    DE_TEST_ASSERT(!deInt64InInt32Range(-((((int64_t)0x7FFFFFF) << 32) | (int64_t)0xFFFFFFFF)));
    DE_TEST_ASSERT(!deInt64InInt32Range((int64_t)deIntMinValue32(32) - 1));
}

DE_END_EXTERN_C
