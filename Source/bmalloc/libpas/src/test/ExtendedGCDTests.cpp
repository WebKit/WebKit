/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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
#include "pas_extended_gcd.h"

using namespace std;

namespace {

void testExtendedGCD(int64_t left, int64_t right,
                     int64_t result, int64_t leftBezoutCoefficient, int64_t rightBezoutCoefficient)
{
    pas_extended_gcd_result actualResult = pas_extended_gcd(left, right);
    CHECK_EQUAL(actualResult.result, result);
    CHECK_EQUAL(actualResult.left_bezout_coefficient, leftBezoutCoefficient);
    CHECK_EQUAL(actualResult.right_bezout_coefficient, rightBezoutCoefficient);
    
    actualResult = pas_extended_gcd(right, left);
    CHECK_EQUAL(actualResult.result, result);
    CHECK_EQUAL(actualResult.left_bezout_coefficient, rightBezoutCoefficient);
    CHECK_EQUAL(actualResult.right_bezout_coefficient, leftBezoutCoefficient);
}

} // anonymous namespace

void addExtendedGCDTests()
{
    ADD_TEST(testExtendedGCD(1, 0, 1, 1, 0));
    ADD_TEST(testExtendedGCD(2, 0, 2, 1, 0));
    ADD_TEST(testExtendedGCD(1, 64, 1, 1, 0));
    ADD_TEST(testExtendedGCD(666, 42, 6, -1, 16));
    ADD_TEST(testExtendedGCD(666, 1024, 2, -123, 80));
    ADD_TEST(testExtendedGCD(65536, 32768, 32768, 0, 1));
}

