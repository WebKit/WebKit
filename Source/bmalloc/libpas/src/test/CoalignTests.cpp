/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "TestHarness.h"
#include "pas_coalign.h"

using namespace std;

namespace {

void testCoalignOneSided(uintptr_t begin, uintptr_t typeSize, uintptr_t alignment, uintptr_t result)
{
    pas_coalign_result actualResult = pas_coalign_one_sided(begin, typeSize, alignment);
    CHECK(actualResult.has_result);
    CHECK_EQUAL(actualResult.result, result);
    CHECK(!(actualResult.result % alignment));
    CHECK(!((actualResult.result - begin) % typeSize));
}

void testCoalignOneSidedError(uintptr_t begin, uintptr_t typeSize, uintptr_t alignment)
{
    pas_coalign_result actualResult = pas_coalign_one_sided(begin, typeSize, alignment);
    CHECK(!actualResult.has_result);
}

void testCoalign(uintptr_t beginLeft, uintptr_t leftSize,
                 uintptr_t beginRight, uintptr_t rightSize,
                 uintptr_t result)
{
    pas_coalign_result actualResult = pas_coalign(beginLeft, leftSize, beginRight, rightSize);
    CHECK(actualResult.has_result);
    CHECK_EQUAL(actualResult.result, result);
    CHECK(!((actualResult.result - beginLeft) % leftSize));
    CHECK(!((actualResult.result - beginRight) % rightSize));
}

} // anonymous namespace

void addCoalignTests()
{
    ADD_TEST(testCoalignOneSided(459318, 666, 1024, 795648));
    ADD_TEST(testCoalignOneSided(4096 + 48 * 4, 48, 64, 4288));
    ADD_TEST(testCoalignOneSided(4096 * 11 + 7 * 13, 13, 1024, 58368));
    ADD_TEST(testCoalignOneSided(4096 * 13 + 7 * 64 * 11, 7 * 64, 1024, 60416));
    ADD_TEST(testCoalignOneSided(10000, 5000, 4096, 2560000));
    ADD_TEST(testCoalignOneSidedError(11111, 5000, 4096));
    ADD_TEST(testCoalignOneSidedError(6422642, 40, 256));
    ADD_TEST(testCoalignOneSided(6422648, 40, 256, 6423808));
    ADD_TEST(testCoalign(500, 155, 999, 1024, 61415));
    ADD_TEST(testCoalign(6426, 531, 24, 647, 219357));
}

