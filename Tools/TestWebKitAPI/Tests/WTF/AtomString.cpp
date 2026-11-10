/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <numbers>
#include <wtf/text/AtomString.h>

namespace TestWebKitAPI {

TEST(WTF, AtomStringCreationFromLiteral)
{
    RefPtr<AtomStringImpl> stringWithTemplate("Template Literal"_atom);
    ASSERT_EQ(strlen("Template Literal"), length(stringWithTemplate));
    ASSERT_TRUE(equal(stringWithTemplate, "Template Literal"_s));
    ASSERT_TRUE(is8Bit(stringWithTemplate));

    ASCIILiteral literal("Source literal");
    RefPtr<AtomStringImpl> stringFromLiteral(AtomStringImpl::add(literal));
    ASSERT_EQ(strlen("Source literal"), length(stringFromLiteral));
    ASSERT_TRUE(equal(stringFromLiteral, "Source literal"_s));
    ASSERT_TRUE(is8Bit(stringFromLiteral));
    ASSERT_TRUE(std::bit_cast<uintptr_t>(stringFromLiteral.get()->span8().data()) == std::bit_cast<uintptr_t>(literal.span().data()));
}

TEST(WTF, AtomStringCreationFromLiteralUniqueness)
{
    RefPtr<AtomStringImpl> string1("Template Literal"_atom);
    RefPtr<AtomStringImpl> string2("Template Literal"_atom);
    ASSERT_EQ(string1.get(), string2.get());

    RefPtr<AtomStringImpl> string3("Template Literal"_atom);
    ASSERT_EQ(string1.get(), string3.get());
}

TEST(WTF, AtomStringExistingHash)
{
    RefPtr<AtomStringImpl> string1("Template Literal"_atom);
    ASSERT_EQ(existingHash(string1), string1.get()->existingHash());
    RefPtr<AtomStringImpl> string2;
    ASSERT_EQ(existingHash(string2), 0u);
}

static inline UTF8CString testAtomStringNumber(double number)
{
    return utf8(AtomStringImpl::number(number));
}

TEST(WTF, AtomStringCreationFromNullASCIILiteral)
{
    RefPtr<AtomStringImpl> stringFromNull { ASCIILiteral() };
    ASSERT_TRUE(!stringFromNull);
    ASSERT_TRUE(isEmpty(stringFromNull));

    RefPtr<AtomStringImpl> stringFromEmpty(""_atom);
    ASSERT_FALSE(!stringFromEmpty);
    ASSERT_TRUE(isEmpty(stringFromEmpty));
}

TEST(WTF, AtomStringNumberDouble)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_EQ("Infinity"_s, testAtomStringNumber(Limits::infinity()));
    EXPECT_EQ("-Infinity"_s, testAtomStringNumber(-Limits::infinity()));

    EXPECT_EQ("NaN"_s, testAtomStringNumber(-Limits::quiet_NaN()));

    EXPECT_EQ("0"_s, testAtomStringNumber(0));
    EXPECT_EQ("0"_s, testAtomStringNumber(-0));

    EXPECT_EQ("2.2250738585072014e-308"_s, testAtomStringNumber(Limits::min()));
    EXPECT_EQ("-1.7976931348623157e+308"_s, testAtomStringNumber(Limits::lowest()));
    EXPECT_EQ("1.7976931348623157e+308"_s, testAtomStringNumber(Limits::max()));

    EXPECT_EQ("3.141592653589793"_s, testAtomStringNumber(std::numbers::pi));
    EXPECT_EQ("3.1415927410125732"_s, testAtomStringNumber(std::numbers::pi_v<float>));
    EXPECT_EQ("1.5707963267948966"_s, testAtomStringNumber(piOverTwoDouble));
    EXPECT_EQ("1.5707963705062866"_s, testAtomStringNumber(piOverTwoFloat));
    EXPECT_EQ("0.7853981633974483"_s, testAtomStringNumber(piOverFourDouble));
    EXPECT_EQ("0.7853981852531433"_s, testAtomStringNumber(piOverFourFloat));

    EXPECT_EQ("2.718281828459045"_s, testAtomStringNumber(2.71828182845904523536028747135266249775724709369995));

    EXPECT_EQ("299792458"_s, testAtomStringNumber(299792458));

    EXPECT_EQ("1.618033988749895"_s, testAtomStringNumber(1.6180339887498948482));

    EXPECT_EQ("1000"_s, testAtomStringNumber(1e3));
    EXPECT_EQ("10000000000"_s, testAtomStringNumber(1e10));
    EXPECT_EQ("100000000000000000000"_s, testAtomStringNumber(1e20));
    EXPECT_EQ("1e+21"_s, testAtomStringNumber(1e21));
    EXPECT_EQ("1e+30"_s, testAtomStringNumber(1e30));

    EXPECT_EQ("1100"_s, testAtomStringNumber(1.1e3));
    EXPECT_EQ("11000000000"_s, testAtomStringNumber(1.1e10));
    EXPECT_EQ("110000000000000000000"_s, testAtomStringNumber(1.1e20));
    EXPECT_EQ("1.1e+21"_s, testAtomStringNumber(1.1e21));
    EXPECT_EQ("1.1e+30"_s, testAtomStringNumber(1.1e30));
}

} // namespace TestWebKitAPI
