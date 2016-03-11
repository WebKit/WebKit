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

#include <wtf/text/SymbolImpl.h>
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
    const char d[] = "aBcDeFG";
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    RefPtr<StringImpl> shorter = StringImpl::createFromLiteral("abcdef");
    RefPtr<StringImpl> different = StringImpl::createFromLiteral("abcrefg");

    // Identity.
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), a.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(c.get(), c.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), d));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), d));
    ASSERT_TRUE(equalIgnoringASCIICase(c.get(), d));

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
    ASSERT_FALSE(equalIgnoringASCIICase(a.get(), different.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(b.get(), different.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(c.get(), different.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(empty.get(), d));
    ASSERT_FALSE(equalIgnoringASCIICase(shorter.get(), d));
    ASSERT_FALSE(equalIgnoringASCIICase(different.get(), d));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithNull)
{
    RefPtr<StringImpl> reference = StringImpl::createFromLiteral("aBcDeFG");
    StringImpl* nullStringImpl = nullptr;
    ASSERT_FALSE(equalIgnoringASCIICase(nullStringImpl, reference.get()));
    ASSERT_FALSE(equalIgnoringASCIICase(reference.get(), nullStringImpl));
    ASSERT_TRUE(equalIgnoringASCIICase(nullStringImpl, nullStringImpl));
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithEmpty)
{
    RefPtr<StringImpl> a = StringImpl::create(reinterpret_cast<const LChar*>(""));
    RefPtr<StringImpl> b = StringImpl::create(reinterpret_cast<const LChar*>(""));
    ASSERT_TRUE(equalIgnoringASCIICase(a.get(), b.get()));
    ASSERT_TRUE(equalIgnoringASCIICase(b.get(), a.get()));
}

static RefPtr<StringImpl> stringFromUTF8(const char* characters)
{
    return String::fromUTF8(characters).impl();
}

TEST(WTF, StringImplEqualIgnoringASCIICaseWithLatin1Characters)
{
    RefPtr<StringImpl> a = stringFromUTF8("aBcéeFG");
    RefPtr<StringImpl> b = stringFromUTF8("ABCÉEFG");
    RefPtr<StringImpl> c = stringFromUTF8("ABCéEFG");
    RefPtr<StringImpl> d = stringFromUTF8("abcéefg");
    const char e[] = "aBcéeFG";

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
    ASSERT_FALSE(equalIgnoringASCIICase(a.get(), e));
    ASSERT_FALSE(equalIgnoringASCIICase(b.get(), e));
    ASSERT_FALSE(equalIgnoringASCIICase(c.get(), e));
    ASSERT_FALSE(equalIgnoringASCIICase(d.get(), e));
}

TEST(WTF, StringImplFindIgnoringASCIICaseBasic)
{
    RefPtr<StringImpl> referenceA = stringFromUTF8("aBcéeFG");
    RefPtr<StringImpl> referenceB = stringFromUTF8("ABCÉEFG");

    // Search the exact string.
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(referenceA.get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(referenceB.get()));

    // A and B are distinct by the non-ascii character é/É.
    EXPECT_EQ(static_cast<size_t>(notFound), referenceA->findIgnoringASCIICase(referenceB.get()));
    EXPECT_EQ(static_cast<size_t>(notFound), referenceB->findIgnoringASCIICase(referenceA.get()));

    // Find the prefix.
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("a").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(stringFromUTF8("abcé").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("A").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceA->findIgnoringASCIICase(stringFromUTF8("ABCé").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("a").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(stringFromUTF8("abcÉ").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("A").get()));
    EXPECT_EQ(static_cast<size_t>(0), referenceB->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get()));

    // Not a prefix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("x").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("accé").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("abcÉ").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("X").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("ABDé").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("y").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("accÉ").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("abcé").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("Y").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("ABdÉ").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("ABCé").get()));

    // Find the infix.
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("cée").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("ée").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("cé").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("c").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("é").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("Cée").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("éE").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("Cé").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceA->findIgnoringASCIICase(stringFromUTF8("C").get()));

    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("cÉe").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("Ée").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("cÉ").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("c").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("É").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("CÉe").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("ÉE").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("CÉ").get()));
    EXPECT_EQ(static_cast<size_t>(2), referenceB->findIgnoringASCIICase(stringFromUTF8("C").get()));

    // Not an infix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("céd").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("Ée").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("bé").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("x").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("É").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("CÉe").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("éd").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("CÉ").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("Y").get()));

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("cée").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("Éc").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("cé").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("W").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("é").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("bÉe").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("éE").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("BÉ").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("z").get()));

    // Find the suffix.
    EXPECT_EQ(static_cast<size_t>(6), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("g").get()));
    EXPECT_EQ(static_cast<size_t>(4), referenceA->findIgnoringASCIICase(stringFromUTF8("efg").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("éefg").get()));
    EXPECT_EQ(static_cast<size_t>(6), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("G").get()));
    EXPECT_EQ(static_cast<size_t>(4), referenceA->findIgnoringASCIICase(stringFromUTF8("EFG").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceA->findIgnoringASCIICase(stringFromUTF8("éEFG").get()));

    EXPECT_EQ(static_cast<size_t>(6), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("g").get()));
    EXPECT_EQ(static_cast<size_t>(4), referenceB->findIgnoringASCIICase(stringFromUTF8("efg").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("Éefg").get()));
    EXPECT_EQ(static_cast<size_t>(6), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("G").get()));
    EXPECT_EQ(static_cast<size_t>(4), referenceB->findIgnoringASCIICase(stringFromUTF8("EFG").get()));
    EXPECT_EQ(static_cast<size_t>(3), referenceB->findIgnoringASCIICase(stringFromUTF8("ÉEFG").get()));

    // Not a suffix.
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("X").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("edg").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("Éefg").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(StringImpl::createFromLiteral("w").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("dFG").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceA->findIgnoringASCIICase(stringFromUTF8("ÉEFG").get()));

    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("Z").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("ffg").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("éefg").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(StringImpl::createFromLiteral("r").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("EgG").get()));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), referenceB->findIgnoringASCIICase(stringFromUTF8("éEFG").get()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseWithValidOffset)
{
    RefPtr<StringImpl> reference = stringFromUTF8("ABCÉEFGaBcéeFG");
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(stringFromUTF8("ABC").get(), 0));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(stringFromUTF8("ABC").get(), 1));
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get(), 0));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get(), 1));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(stringFromUTF8("ABCé").get(), 0));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(stringFromUTF8("ABCé").get(), 1));
}

TEST(WTF, StringImplFindIgnoringASCIICaseWithInvalidOffset)
{
    RefPtr<StringImpl> reference = stringFromUTF8("ABCÉEFGaBcéeFG");
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABC").get(), 15));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABC").get(), 16));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get(), 17));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get(), 42));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(stringFromUTF8("ABCÉ").get(), std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseOnNull)
{
    RefPtr<StringImpl> reference = stringFromUTF8("ABCÉEFG");
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr, 0));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr, 3));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr, 7));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr, 8));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr, 42));
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(nullptr, std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseOnEmpty)
{
    RefPtr<StringImpl> reference = stringFromUTF8("ABCÉEFG");
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(empty.get()));
    EXPECT_EQ(static_cast<size_t>(0), reference->findIgnoringASCIICase(empty.get(), 0));
    EXPECT_EQ(static_cast<size_t>(3), reference->findIgnoringASCIICase(empty.get(), 3));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.get(), 7));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.get(), 8));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.get(), 42));
    EXPECT_EQ(static_cast<size_t>(7), reference->findIgnoringASCIICase(empty.get(), std::numeric_limits<unsigned>::max()));
}

TEST(WTF, StringImplFindIgnoringASCIICaseWithPatternLongerThanReference)
{
    RefPtr<StringImpl> reference = stringFromUTF8("ABCÉEFG");
    RefPtr<StringImpl> pattern = stringFromUTF8("XABCÉEFG");
    EXPECT_EQ(static_cast<size_t>(WTF::notFound), reference->findIgnoringASCIICase(pattern.get()));
    EXPECT_EQ(static_cast<size_t>(1), pattern->findIgnoringASCIICase(reference.get()));
}

TEST(WTF, StringImplStartsWithIgnoringASCIICaseBasic)
{
    RefPtr<StringImpl> reference = stringFromUTF8("aBcéX");
    RefPtr<StringImpl> referenceEquivalent = stringFromUTF8("AbCéx");

    // Identity.
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(reference.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*reference.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(referenceEquivalent.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*referenceEquivalent.get()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(reference.get()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(*reference.get()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(referenceEquivalent.get()));
    ASSERT_TRUE(referenceEquivalent->startsWithIgnoringASCIICase(*referenceEquivalent.get()));

    // Proper prefixes.
    RefPtr<StringImpl> aLower = StringImpl::createFromLiteral("a");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(aLower.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*aLower.get()));
    RefPtr<StringImpl> aUpper = StringImpl::createFromLiteral("A");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(aUpper.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*aUpper.get()));

    RefPtr<StringImpl> abcLower = StringImpl::createFromLiteral("abc");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcLower.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcLower.get()));
    RefPtr<StringImpl> abcUpper = StringImpl::createFromLiteral("ABC");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcUpper.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcUpper.get()));

    RefPtr<StringImpl> abcAccentLower = stringFromUTF8("abcé");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcAccentLower.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcAccentLower.get()));
    RefPtr<StringImpl> abcAccentUpper = stringFromUTF8("ABCé");
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(abcAccentUpper.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*abcAccentUpper.get()));

    // Negative cases.
    RefPtr<StringImpl> differentFirstChar = stringFromUTF8("bBcéX");
    RefPtr<StringImpl> differentFirstCharProperPrefix = stringFromUTF8("CBcé");
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(differentFirstChar.get()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*differentFirstChar.get()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(differentFirstCharProperPrefix.get()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*differentFirstCharProperPrefix.get()));

    RefPtr<StringImpl> uppercaseAccent = stringFromUTF8("aBcÉX");
    RefPtr<StringImpl> uppercaseAccentProperPrefix = stringFromUTF8("aBcÉX");
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(uppercaseAccent.get()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*uppercaseAccent.get()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(uppercaseAccentProperPrefix.get()));
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(*uppercaseAccentProperPrefix.get()));
}

TEST(WTF, StringImplStartsWithIgnoringASCIICaseWithNull)
{
    RefPtr<StringImpl> reference = StringImpl::createFromLiteral("aBcDeFG");
    ASSERT_FALSE(reference->startsWithIgnoringASCIICase(nullptr));

    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    ASSERT_FALSE(empty->startsWithIgnoringASCIICase(nullptr));
}

TEST(WTF, StringImplStartsWithIgnoringASCIICaseWithEmpty)
{
    RefPtr<StringImpl> reference = StringImpl::createFromLiteral("aBcDeFG");
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(empty.get()));
    ASSERT_TRUE(reference->startsWithIgnoringASCIICase(*empty.get()));
    ASSERT_TRUE(empty->startsWithIgnoringASCIICase(empty.get()));
    ASSERT_TRUE(empty->startsWithIgnoringASCIICase(*empty.get()));
    ASSERT_FALSE(empty->startsWithIgnoringASCIICase(reference.get()));
    ASSERT_FALSE(empty->startsWithIgnoringASCIICase(*reference.get()));
}

TEST(WTF, StartsWithLettersIgnoringASCIICase)
{
    String string("Test tEST");
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, "test t"));
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, "test te"));
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, "test test"));
    ASSERT_FALSE(startsWithLettersIgnoringASCIICase(string, "test tex"));

    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(string, ""));
    ASSERT_TRUE(startsWithLettersIgnoringASCIICase(String(""), ""));

    ASSERT_FALSE(startsWithLettersIgnoringASCIICase(String(), "t"));
    ASSERT_FALSE(startsWithLettersIgnoringASCIICase(String(), ""));
}

TEST(WTF, StringImplEndsWithIgnoringASCIICaseBasic)
{
    RefPtr<StringImpl> reference = stringFromUTF8("XÉCbA");
    RefPtr<StringImpl> referenceEquivalent = stringFromUTF8("xÉcBa");

    // Identity.
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(reference.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*reference.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(referenceEquivalent.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*referenceEquivalent.get()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(reference.get()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(*reference.get()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(referenceEquivalent.get()));
    ASSERT_TRUE(referenceEquivalent->endsWithIgnoringASCIICase(*referenceEquivalent.get()));

    // Proper suffixes.
    RefPtr<StringImpl> aLower = StringImpl::createFromLiteral("a");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(aLower.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*aLower.get()));
    RefPtr<StringImpl> aUpper = StringImpl::createFromLiteral("a");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(aUpper.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*aUpper.get()));

    RefPtr<StringImpl> abcLower = StringImpl::createFromLiteral("cba");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcLower.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcLower.get()));
    RefPtr<StringImpl> abcUpper = StringImpl::createFromLiteral("CBA");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcUpper.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcUpper.get()));

    RefPtr<StringImpl> abcAccentLower = stringFromUTF8("Écba");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcAccentLower.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcAccentLower.get()));
    RefPtr<StringImpl> abcAccentUpper = stringFromUTF8("ÉCBA");
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(abcAccentUpper.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*abcAccentUpper.get()));

    // Negative cases.
    RefPtr<StringImpl> differentLastChar = stringFromUTF8("XÉCbB");
    RefPtr<StringImpl> differentLastCharProperSuffix = stringFromUTF8("ÉCbb");
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(differentLastChar.get()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*differentLastChar.get()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(differentLastCharProperSuffix.get()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*differentLastCharProperSuffix.get()));

    RefPtr<StringImpl> lowercaseAccent = stringFromUTF8("aBcéX");
    RefPtr<StringImpl> loweraseAccentProperSuffix = stringFromUTF8("aBcéX");
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(lowercaseAccent.get()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*lowercaseAccent.get()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(loweraseAccentProperSuffix.get()));
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(*loweraseAccentProperSuffix.get()));
}

TEST(WTF, StringImplEndsWithIgnoringASCIICaseWithNull)
{
    RefPtr<StringImpl> reference = StringImpl::createFromLiteral("aBcDeFG");
    ASSERT_FALSE(reference->endsWithIgnoringASCIICase(nullptr));

    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    ASSERT_FALSE(empty->endsWithIgnoringASCIICase(nullptr));
}

TEST(WTF, StringImplEndsWithIgnoringASCIICaseWithEmpty)
{
    RefPtr<StringImpl> reference = StringImpl::createFromLiteral("aBcDeFG");
    RefPtr<StringImpl> empty = StringImpl::create(reinterpret_cast<const LChar*>(""));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(empty.get()));
    ASSERT_TRUE(reference->endsWithIgnoringASCIICase(*empty.get()));
    ASSERT_TRUE(empty->endsWithIgnoringASCIICase(empty.get()));
    ASSERT_TRUE(empty->endsWithIgnoringASCIICase(*empty.get()));
    ASSERT_FALSE(empty->endsWithIgnoringASCIICase(reference.get()));
    ASSERT_FALSE(empty->endsWithIgnoringASCIICase(*reference.get()));
}

TEST(WTF, StringImplCreateSymbolEmpty)
{
    RefPtr<StringImpl> reference = StringImpl::createSymbolEmpty();
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtomic());
    ASSERT_EQ(0u, reference->length());
    ASSERT_TRUE(equal(reference.get(), ""));
}

TEST(WTF, StringImplCreateSymbol)
{
    RefPtr<StringImpl> original = stringFromUTF8("original");
    RefPtr<StringImpl> reference = StringImpl::createSymbol(original);
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtomic());
    ASSERT_FALSE(original->isSymbol());
    ASSERT_FALSE(original->isAtomic());
    ASSERT_EQ(original->length(), reference->length());
    ASSERT_TRUE(equal(reference.get(), "original"));
}

TEST(WTF, StringImplSymbolToAtomicString)
{
    RefPtr<StringImpl> original = stringFromUTF8("original");
    RefPtr<StringImpl> reference = StringImpl::createSymbol(original);
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtomic());

    RefPtr<StringImpl> atomic = AtomicStringImpl::add(reference.get());
    ASSERT_TRUE(atomic->isAtomic());
    ASSERT_FALSE(atomic->isSymbol());
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtomic());
}

TEST(WTF, StringImplSymbolEmptyToAtomicString)
{
    RefPtr<StringImpl> reference = StringImpl::createSymbolEmpty();
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtomic());

    RefPtr<StringImpl> atomic = AtomicStringImpl::add(reference.get());
    ASSERT_TRUE(atomic->isAtomic());
    ASSERT_FALSE(atomic->isSymbol());
    ASSERT_TRUE(reference->isSymbol());
    ASSERT_FALSE(reference->isAtomic());
}

} // namespace TestWebKitAPI
