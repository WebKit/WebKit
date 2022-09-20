/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/cf/VectorCF.h>

namespace TestWebKitAPI {

TEST(VectorCF, CreateCFArray_CFString)
{
    const size_t elementCount = 3;
    Vector<String> strings = { "one"_str, "two"_str, "three"_str };

    auto cfStrings = createCFArray(strings);

    CFIndex count = CFArrayGetCount(cfStrings.get());
    EXPECT_EQ(elementCount, Checked<size_t>(count));
    EXPECT_EQ(elementCount, std::size(strings));
    EXPECT_TRUE(CFStringCompare(checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(cfStrings.get(), 0)), CFSTR("one"), 0) == kCFCompareEqualTo);
    EXPECT_TRUE(CFStringCompare(checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(cfStrings.get(), 1)), CFSTR("two"), 0) == kCFCompareEqualTo);
    EXPECT_TRUE(CFStringCompare(checked_cf_cast<CFStringRef>(CFArrayGetValueAtIndex(cfStrings.get(), 2)), CFSTR("three"), 0) == kCFCompareEqualTo);
}

TEST(VectorCF, CreateCFArray_CFNumber)
{
    const size_t elementCount = 3;
    Vector<float> floats = { 1, 2, 3 };

    auto cfNumbers = createCFArray(floats);

    CFIndex count = CFArrayGetCount(cfNumbers.get());
    EXPECT_EQ(elementCount, Checked<size_t>(count));
    EXPECT_EQ(elementCount, std::size(floats));
    float number = 0;
    CFNumberGetValue(checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(cfNumbers.get(), 0)), kCFNumberFloatType, &number);
    EXPECT_EQ(1.0, number);
    CFNumberGetValue(checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(cfNumbers.get(), 1)), kCFNumberFloatType, &number);
    EXPECT_EQ(2.0, number);
    CFNumberGetValue(checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(cfNumbers.get(), 2)), kCFNumberFloatType, &number);
    EXPECT_EQ(3.0, number);
}

TEST(VectorCF, CreateCFArrayWithFunctor_CFNumber)
{
    const size_t elementCount = 3;
    Vector<double> doubles = { 1, 2, 3 };

    auto cfNumbers = createCFArray(doubles, [] (const double& number) {
        return adoptCF(CFNumberCreate(nullptr, kCFNumberDoubleType, &number));
    });

    CFIndex count = CFArrayGetCount(cfNumbers.get());
    EXPECT_EQ(elementCount, Checked<size_t>(count));
    EXPECT_EQ(elementCount, std::size(doubles));
    double number = 0;
    CFNumberGetValue(checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(cfNumbers.get(), 0)), kCFNumberDoubleType, &number);
    EXPECT_EQ(1.0, number);
    CFNumberGetValue(checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(cfNumbers.get(), 1)), kCFNumberDoubleType, &number);
    EXPECT_EQ(2.0, number);
    CFNumberGetValue(checked_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(cfNumbers.get(), 2)), kCFNumberDoubleType, &number);
    EXPECT_EQ(3.0, number);
}

TEST(VectorCF, MakeVector_CFString)
{
    const size_t elementCount = 3;
    auto cfStrings = adoptCF(CFArrayCreateMutable(nullptr, elementCount, &kCFTypeArrayCallBacks));
    CFArrayAppendValue(cfStrings.get(), CFSTR("one"));
    CFArrayAppendValue(cfStrings.get(), CFSTR("two"));
    CFArrayAppendValue(cfStrings.get(), CFSTR("three"));
    CFIndex count = CFArrayGetCount(cfStrings.get());

    auto strings = makeVector<String>(cfStrings.get());

    EXPECT_EQ(elementCount, Checked<size_t>(count));
    EXPECT_EQ(elementCount, std::size(strings));
    EXPECT_EQ("one"_str, strings[0]);
    EXPECT_EQ("two"_str, strings[1]);
    EXPECT_EQ("three"_str, strings[2]);
}

TEST(VectorCF, MakeVector_CFNumber)
{
    const size_t elementCount = 3;
    auto cfNumbers = adoptCF(CFArrayCreateMutable(nullptr, elementCount, &kCFTypeArrayCallBacks));
    float number = 1;
    CFArrayAppendValue(cfNumbers.get(), CFNumberCreate(nullptr, kCFNumberFloatType, &number));
    number = 2;
    CFArrayAppendValue(cfNumbers.get(), CFNumberCreate(nullptr, kCFNumberFloatType, &number));
    number = 3;
    CFArrayAppendValue(cfNumbers.get(), CFNumberCreate(nullptr, kCFNumberFloatType, &number));

    auto floats = makeVector<float, CFNumberRef>(cfNumbers.get());

    CFIndex count = CFArrayGetCount(cfNumbers.get());
    EXPECT_EQ(elementCount, Checked<size_t>(count));
    EXPECT_EQ(elementCount, std::size(floats));
    EXPECT_EQ(1.0, floats[0]);
    EXPECT_EQ(2.0, floats[1]);
    EXPECT_EQ(3.0, floats[2]);
}

TEST(VectorCF, MakeVectorWithFunctor)
{
    const size_t elementCount = 3;
    auto cfNumbers = adoptCF(CFArrayCreateMutable(nullptr, elementCount, &kCFTypeArrayCallBacks));
    double number = 1;
    CFArrayAppendValue(cfNumbers.get(), CFNumberCreate(nullptr, kCFNumberDoubleType, &number));
    number = 2;
    CFArrayAppendValue(cfNumbers.get(), CFNumberCreate(nullptr, kCFNumberDoubleType, &number));
    number = 3;
    CFArrayAppendValue(cfNumbers.get(), CFNumberCreate(nullptr, kCFNumberDoubleType, &number));

    auto doubles = makeVector(cfNumbers.get(), [] (CFNumberRef cfNumber) -> std::optional<double> {
        double number = 0;
        CFNumberGetValue(cfNumber, kCFNumberDoubleType, &number);
        return { number };
    });

    CFIndex count = CFArrayGetCount(cfNumbers.get());
    EXPECT_EQ(elementCount, Checked<size_t>(count));
    EXPECT_EQ(elementCount, std::size(doubles));
    EXPECT_EQ(1.0, doubles[0]);
    EXPECT_EQ(2.0, doubles[1]);
    EXPECT_EQ(3.0, doubles[2]);
}

TEST(VectorCF, VectorFromCFData)
{
    const size_t elementCount = 4;
    uint8_t bytes[] = { 0x01, 0x02, 0x03, 0x04 };
    auto byteLength = sizeof(bytes);
    EXPECT_EQ(elementCount, byteLength);
    auto cfData = adoptCF(CFDataCreate(nullptr, static_cast<const UInt8*>(&bytes[0]), Checked<CFIndex>(byteLength)));

    auto vectorData = vectorFromCFData(cfData.get());

    CFIndex cfDataLength = CFDataGetLength(cfData.get());
    EXPECT_EQ(byteLength, Checked<size_t>(cfDataLength));
    EXPECT_EQ(byteLength, std::size(vectorData));
    for (size_t i = 0; i < byteLength; ++i) {
        EXPECT_EQ(bytes[i], CFDataGetBytePtr(cfData.get())[i]);
        EXPECT_EQ(bytes[i], vectorData[i]);
    }
    EXPECT_TRUE(&bytes[0] != CFDataGetBytePtr(cfData.get()));
    EXPECT_TRUE(&bytes[0] != &vectorData[0]);
}

} // namespace TestWebKitAPI
