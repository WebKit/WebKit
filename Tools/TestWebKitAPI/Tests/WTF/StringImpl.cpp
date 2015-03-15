/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF, StringImplCreationFromLiteral)
{
    // Constructor using the template to determine the size.
    RefPtr<StringImpl> stringWithTemplate = StringImpl::createFromLiteral("Template Literal");
    ASSERT_EQ(strlen("Template Literal"), stringWithTemplate->length());
    ASSERT_TRUE(equal(stringWithTemplate.get(), "Template Literal"));
    ASSERT_TRUE(stringWithTemplate->is8Bit());

    // Constructor taking the size explicitely.
    const char* programmaticStringData = "Explicit Size Literal";
    RefPtr<StringImpl> programmaticString = StringImpl::createFromLiteral(programmaticStringData, strlen(programmaticStringData));
    ASSERT_EQ(strlen(programmaticStringData), programmaticString->length());
    ASSERT_TRUE(equal(programmaticString.get(), programmaticStringData));
    ASSERT_EQ(programmaticStringData, reinterpret_cast<const char*>(programmaticString->characters8()));
    ASSERT_TRUE(programmaticString->is8Bit());

    // Constructor without explicit size.
    const char* stringWithoutLengthLiteral = "No Size Literal";
    RefPtr<StringImpl> programmaticStringNoLength = StringImpl::createFromLiteral(stringWithoutLengthLiteral);
    ASSERT_EQ(strlen(stringWithoutLengthLiteral), programmaticStringNoLength->length());
    ASSERT_TRUE(equal(programmaticStringNoLength.get(), stringWithoutLengthLiteral));
    ASSERT_EQ(stringWithoutLengthLiteral, reinterpret_cast<const char*>(programmaticStringNoLength->characters8()));
    ASSERT_TRUE(programmaticStringNoLength->is8Bit());
}

TEST(WTF, StringImplReplaceWithLiteral)
{
    RefPtr<StringImpl> testStringImpl = StringImpl::createFromLiteral("1224");
    ASSERT_TRUE(testStringImpl->is8Bit());

    // Cases for 8Bit source.
    testStringImpl = testStringImpl->replace('2', "", 0);
    ASSERT_TRUE(equal(testStringImpl.get(), "14"));

    testStringImpl = StringImpl::createFromLiteral("1224");
    ASSERT_TRUE(testStringImpl->is8Bit());

    testStringImpl = testStringImpl->replace('3', "NotFound", 8);
    ASSERT_TRUE(equal(testStringImpl.get(), "1224"));

    testStringImpl = testStringImpl->replace('2', "3", 1);
    ASSERT_TRUE(equal(testStringImpl.get(), "1334"));

    testStringImpl = StringImpl::createFromLiteral("1224");
    ASSERT_TRUE(testStringImpl->is8Bit());
    testStringImpl = testStringImpl->replace('2', "555", 3);
    ASSERT_TRUE(equal(testStringImpl.get(), "15555554"));

    // Cases for 16Bit source.
    String testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.impl()->is8Bit());

    testStringImpl = testString.impl()->replace('2', "NotFound", 8);
    ASSERT_TRUE(equal(testStringImpl.get(), String::fromUTF8("résumé").impl()));

    testStringImpl = testString.impl()->replace(UChar(0x00E9 /*U+00E9 is 'é'*/), "e", 1);
    ASSERT_TRUE(equal(testStringImpl.get(), "resume"));

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.impl()->is8Bit());
    testStringImpl = testString.impl()->replace(UChar(0x00E9 /*U+00E9 is 'é'*/), "", 0);
    ASSERT_TRUE(equal(testStringImpl.get(), "rsum"));

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.impl()->is8Bit());
    testStringImpl = testString.impl()->replace(UChar(0x00E9 /*U+00E9 is 'é'*/), "555", 3);
    ASSERT_TRUE(equal(testStringImpl.get(), "r555sum555"));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseBasic)
{
    RefPtr<StringImpl> a = StringImpl::createFromLiteral("aBcDeFG");
    RefPtr<StringImpl> b = StringImpl::createFromLiteral("ABCDEFG");
    RefPtr<StringImpl> c = StringImpl::createFromLiteral("abcdefg");
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    RefPtr<StringImpl> shorter = StringImpl::createFromLiteral("abcdef");

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), a.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.get(), c.get()));

    // Transitivity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), c.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), c.get()));

    // Negative cases.
    ASSERT_FALSE(equalIgnoringASCIICase(a.get(), empty.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.get(), empty.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(c.get(), empty.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(a.get(), shorter.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.get(), shorter.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(c.get(), shorter.get()));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithNull)
{
    RefPtr<StringImpl> reference = StringImpl::createFromLiteral("aBcDeFG");
    ASSERT_FALSE(equalIgnoringASCIICase(nullptr, reference.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(reference.get(), nullptr));
    ASSERT_TRUE(equalIgnoringASCIICase(nullptr, nullptr));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithEmpty)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>(""));
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>(""));
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), a.get()));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithLatin1Characters)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>("aBcéeFG"));
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>("ABCÉEFG"));
    RefPtr<StringImpl> c = StringImpl::create(reinterpret_cast<const LChar*>("ABCéEFG"));
    RefPtr<StringImpl> d = StringImpl::create(reinterpret_cast<const LChar*>("abcéefg"));

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), a.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.get(), c.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(d.get(), d.get()));

    // All combination.
    ASSERT_FALSE(equalIgnoringASCIICase(a.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), c.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), d.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.get(), c.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.get(), d.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.get(), d.get()));
}

} // namespace TestWebKitAPI
