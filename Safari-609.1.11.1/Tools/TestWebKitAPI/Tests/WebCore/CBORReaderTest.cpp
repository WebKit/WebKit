// Copyright 2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#if ENABLE(WEB_AUTHN)

#include <WebCore/CBORReader.h>
#include <limits>
#include <utility>

// Leveraging RFC 7049 examples from https://github.com/cbor/test-vectors/blob/master/appendix_a.json.
namespace TestWebKitAPI {

using namespace cbor;

TEST(CBORReaderTest, TestReadUint)
{
    struct UintTestCase {
        const int64_t value;
        const Vector<uint8_t> cborData;
    };

    static const UintTestCase kUintTestCases[] = {
        { 0, { 0x00 } },
        { 1, { 0x01 } },
        { 23, { 0x17 } },
        { 24, { 0x18, 0x18 } },
        { std::numeric_limits<uint8_t>::max(), { 0x18, 0xff } },
        { 1LL << 8, { 0x19, 0x01, 0x00 } },
        { std::numeric_limits<uint16_t>::max(), { 0x19, 0xff, 0xff } },
        { 1LL << 16, { 0x1a, 0x00, 0x01, 0x00, 0x00 } },
        { std::numeric_limits<uint32_t>::max(), { 0x1a, 0xff, 0xff, 0xff, 0xff } },
        { 1LL << 32, { 0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 } },
        { std::numeric_limits<int64_t>::max(),
            { 0x1b, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } },
    };

    for (const UintTestCase& testCase : kUintTestCases) {
        Optional<CBORValue> cbor = CBORReader::read(testCase.cborData);
        ASSERT_TRUE(cbor.hasValue());
        ASSERT_TRUE(cbor.value().type() == CBORValue::Type::Unsigned);
        EXPECT_EQ(cbor.value().getInteger(), testCase.value);
    }
}

TEST(CBORReaderTest, TestUintEncodedWithNonMinimumByteLength)
{
    static const Vector<uint8_t> nonMinimalUintEncodings[] = {
        // Uint 23 encoded with 1 byte.
        { 0x18, 0x17 },
        // Uint 255 encoded with 2 bytes.
        { 0x19, 0x00, 0xff },
        // Uint 65535 encoded with 4 byte.
        { 0x1a, 0x00, 0x00, 0xff, 0xff },
        // Uint 4294967295 encoded with 8 byte.
        { 0x1b, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff },
        // When decoding byte has more than one syntax error, the first syntax
        // error encountered during deserialization is returned as the error code.
        {
            0xa2, // map with non-minimally encoded key
            0x17, // key 24
            0x61, 0x42, // value :"B"
            0x18, 0x17, // key 23 encoded with extra byte
            0x61, 0x45 // value "E"
        },
        {
            0xa2, // map with out of order and non-minimally encoded key
            0x18, 0x17, // key 23 encoded with extra byte
            0x61, 0x45, // value "E"
            0x17, // key 23
            0x61, 0x42 // value :"B"
        },
        {
            0xa2, // map with duplicate non-minimally encoded key
            0x18, 0x17, // key 23 encoded with extra byte
            0x61, 0x45, // value "E"
            0x18, 0x17, // key 23 encoded with extra byte
            0x61, 0x42 // value :"B"
        },
    };

    CBORReader::DecoderError errorCode;
    for (const auto& nonMinimalUint : nonMinimalUintEncodings) {
        Optional<CBORValue> cbor = CBORReader::read(nonMinimalUint, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::NonMinimalCBOREncoding);
    }
}

TEST(CBORReaderTest, TestReadNegativeInt)
{
    struct NegativeIntTestCase {
        const int64_t negativeInt;
        const Vector<uint8_t> cborData;
    };

    static const NegativeIntTestCase kNegativeIntTestCases[] = {
        { -1LL, { 0x20 } },
        { -24LL, { 0x37 } },
        { -25LL, { 0x38, 0x18 } },
        { -256LL, { 0x38, 0xff } },
        { -1000LL, { 0x39, 0x03, 0xe7 } },
        { -1000000LL, { 0x3a, 0x00, 0x0f, 0x42, 0x3f } },
        { -4294967296LL, { 0x3a, 0xff, 0xff, 0xff, 0xff } },
        { std::numeric_limits<int64_t>::min(),
            { 0x3b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } }
    };

    for (const NegativeIntTestCase& testCase : kNegativeIntTestCases) {
        Optional<CBORValue> cbor = CBORReader::read(testCase.cborData);
        ASSERT_TRUE(cbor.hasValue());
        ASSERT_TRUE(cbor.value().type() == CBORValue::Type::Negative);
        EXPECT_EQ(cbor.value().getInteger(), testCase.negativeInt);
    }
}

TEST(CBORReaderTest, TestReadBytes)
{
    struct ByteTestCase {
        const Vector<uint8_t> value;
        const Vector<uint8_t> cborData;
    };

    static const ByteTestCase kByteStringTestCases[] = {
        {{ }, {0x40}},
        {{0x01, 0x02, 0x03, 0x04}, {0x44, 0x01, 0x02, 0x03, 0x04}},
    };

    for (const ByteTestCase& testCase : kByteStringTestCases) {
        Optional<CBORValue> cbor = CBORReader::read(testCase.cborData);
        ASSERT_TRUE(cbor.hasValue());
        ASSERT_TRUE(cbor.value().type() == CBORValue::Type::ByteString);
        EXPECT_TRUE(cbor.value().getByteString() == testCase.value);
    }
}

TEST(CBORReaderTest, TestReadString)
{
    struct StringTestCase {
        const String value;
        const Vector<uint8_t> cborData;
    };

    static const StringTestCase kStringTestCases[] = {
        { "", { 0x60 } },
        { "a", { 0x61, 0x61 } },
        { "IETF", { 0x64, 0x49, 0x45, 0x54, 0x46 } },
        { "\"\\", { 0x62, 0x22, 0x5c } },
        { String::fromUTF8("\xc3\xbc"), { 0x62, 0xc3, 0xbc } },
        { String::fromUTF8("\xe6\xb0\xb4"), { 0x63, 0xe6, 0xb0, 0xb4 } },
        { String::fromUTF8("\xf0\x90\x85\x91"), { 0x64, 0xf0, 0x90, 0x85, 0x91 } },
    };

    for (const StringTestCase& testCase : kStringTestCases) {
        Optional<CBORValue> cbor = CBORReader::read(testCase.cborData);
        ASSERT_TRUE(cbor.hasValue());
        ASSERT_TRUE(cbor.value().type() == CBORValue::Type::String);
        EXPECT_TRUE(cbor.value().getString() == testCase.value);
    }
}

TEST(CBORReaderTest, TestReadStringWithNUL)
{
    static const struct {
        const String value;
        const Vector<uint8_t> cborData;
    } kStringTestCases[] = {
        { String("string_without_nul"),
            { 0x72, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x5F, 0x77, 0x69, 0x74, 0x68,
                0x6F, 0x75, 0x74, 0x5F, 0x6E, 0x75, 0x6C } },
        { String("nul_terminated_string\0", 22),
            { 0x76, 0x6E, 0x75, 0x6C, 0x5F, 0x74, 0x65, 0x72, 0x6D, 0x69, 0x6E, 0x61,
                0x74, 0x65, 0x64, 0x5F, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67, 0x00 } },
        { String("embedded\0nul", 12),
            { 0x6C, 0x65, 0x6D, 0x62, 0x65, 0x64, 0x64, 0x65, 0x64, 0x00, 0x6E, 0x75,
                0x6C } },
        { String("trailing_nuls\0\0", 15),
            { 0x6F, 0x74, 0x72, 0x61, 0x69, 0x6C, 0x69, 0x6E, 0x67, 0x5F, 0x6E, 0x75,
                0x6C, 0x73, 0x00, 0x00 } },
    };

    for (const auto& testCase : kStringTestCases) {
        Optional<CBORValue> cbor = CBORReader::read(testCase.cborData);
        ASSERT_TRUE(cbor.hasValue());
        ASSERT_TRUE(cbor.value().type() == CBORValue::Type::String);
        EXPECT_TRUE(cbor.value().getString() == testCase.value);
    }
}

TEST(CBORReaderTest, TestReadStringWithInvalidByteSequenceAfterNUL)
{
    // UTF-8 validation should not stop at the first NUL character in the string.
    // That is, a string with an invalid byte sequence should fail UTF-8
    // validation even if the invalid character is located after one or more NUL
    // characters. Here, 0xA6 is an unexpected continuation byte.
    static const Vector<uint8_t> stringWithInvalidContinuationByte = {
        0x63, 0x00, 0x00, 0xA6
    };
    CBORReader::DecoderError errorCode;
    Optional<CBORValue> cbor = CBORReader::read(stringWithInvalidContinuationByte, &errorCode);
    EXPECT_FALSE(cbor.hasValue());
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::InvalidUTF8);
}

TEST(CBORReaderTest, TestReadArray)
{
    static const Vector<uint8_t> kArrayTestCaseCBOR = {
        0x98, 0x19, // array of 25 elements
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x18, 0x18, 0x19,
    };

    Optional<CBORValue> cbor = CBORReader::read(kArrayTestCaseCBOR);
    ASSERT_TRUE(cbor.hasValue());
    const CBORValue cborArray = WTFMove(cbor.value());
    ASSERT_TRUE(cborArray.type() == CBORValue::Type::Array);
    ASSERT_EQ(cborArray.getArray().size(), 25u);

    Vector<CBORValue> array;
    for (int i = 0; i < 25; i++) {
        ASSERT_TRUE(cborArray.getArray()[i].type() == CBORValue::Type::Unsigned);
        EXPECT_EQ(cborArray.getArray()[i].getInteger(), static_cast<int64_t>(i + 1));
    }
}

TEST(CBORReaderTest, TestReadMapWithMapValue)
{
    static const Vector<uint8_t> kMapTestCaseCBOR = {
        0xa4, // map with 4 key value pairs:
        0x18, 0x18, // 24
        0x63, 0x61, 0x62, 0x63, // "abc"

        0x60, // ""
        0x61, 0x2e, // "."

        0x61, 0x62, // "b"
        0x61, 0x42, // "B"

        0x62, 0x61, 0x61, // "aa"
        0x62, 0x41, 0x41, // "AA"
    };

    Optional<CBORValue> cbor = CBORReader::read(kMapTestCaseCBOR);
    ASSERT_TRUE(cbor.hasValue());
    const CBORValue cborVal = WTFMove(cbor.value());
    ASSERT_TRUE(cborVal.type() == CBORValue::Type::Map);
    ASSERT_EQ(cborVal.getMap().size(), 4u);

    const CBORValue keyUint(24);
    ASSERT_EQ(cborVal.getMap().count(keyUint), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyUint)->second.type() == CBORValue::Type::String);
    EXPECT_TRUE(cborVal.getMap().find(keyUint)->second.getString() == "abc");

    const CBORValue keyEmptyString("");
    ASSERT_EQ(cborVal.getMap().count(keyEmptyString), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyEmptyString)->second.type() == CBORValue::Type::String);
    EXPECT_TRUE(cborVal.getMap().find(keyEmptyString)->second.getString() == ".");

    const CBORValue keyB("b");
    ASSERT_EQ(cborVal.getMap().count(keyB), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyB)->second.type() == CBORValue::Type::String);
    EXPECT_TRUE(cborVal.getMap().find(keyB)->second.getString() == "B");

    const CBORValue keyAa("aa");
    ASSERT_EQ(cborVal.getMap().count(keyAa), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyAa)->second.type() == CBORValue::Type::String);
    EXPECT_TRUE(cborVal.getMap().find(keyAa)->second.getString() == "AA");
}

TEST(CBORReaderTest, TestReadMapWithIntegerKeys)
{
    static const Vector<uint8_t> kMapWithIntegerKeyCBOR = {
        0xA4, // map with 4 key value pairs
        0x01, // key : 1
        0x61, 0x61, // value : "a"

        0x09, // key : 9
        0x61, 0x62, // value : "b"

        0x19, 0x03, 0xE7, // key : 999
        0x61, 0x63, // value "c"

        0x19, 0x04, 0x57, // key : 1111
        0x61, 0x64, // value : "d"
    };

    Optional<CBORValue> cbor = CBORReader::read(kMapWithIntegerKeyCBOR);
    ASSERT_TRUE(cbor.hasValue());
    const CBORValue cborVal = WTFMove(cbor.value());
    ASSERT_TRUE(cborVal.type() == CBORValue::Type::Map);
    ASSERT_EQ(cborVal.getMap().size(), 4u);

    const CBORValue key1(1);
    ASSERT_EQ(cborVal.getMap().count(key1), 1u);
    ASSERT_TRUE(cborVal.getMap().find(key1)->second.type() == CBORValue::Type::String);
    ASSERT_TRUE(cborVal.getMap().find(key1)->second.getString() == "a");

    const CBORValue key9(9);
    ASSERT_EQ(cborVal.getMap().count(key9), 1u);
    ASSERT_TRUE(cborVal.getMap().find(key9)->second.type() == CBORValue::Type::String);
    ASSERT_TRUE(cborVal.getMap().find(key9)->second.getString() == "b");

    const CBORValue key999(999);
    ASSERT_EQ(cborVal.getMap().count(key999), 1u);
    ASSERT_TRUE(cborVal.getMap().find(key999)->second.type() == CBORValue::Type::String);
    ASSERT_TRUE(cborVal.getMap().find(key999)->second.getString() == "c");

    const CBORValue key1111(1111);
    ASSERT_EQ(cborVal.getMap().count(key1111), 1u);
    ASSERT_TRUE(cborVal.getMap().find(key1111)->second.type() == CBORValue::Type::String);
    ASSERT_TRUE(cborVal.getMap().find(key1111)->second.getString() == "d");
}

TEST(CBORReaderTest, TestReadMapWithArray)
{
    static const Vector<uint8_t> kMapArrayTestCaseCBOR = {
        0xa2, // map of 2 pairs
        0x61, 0x61, // "a"
        0x01,

        0x61, 0x62, // "b"
        0x82, // array with 2 elements
        0x02,
        0x03,
    };

    Optional<CBORValue> cbor = CBORReader::read(kMapArrayTestCaseCBOR);
    ASSERT_TRUE(cbor.hasValue());
    const CBORValue cborVal = WTFMove(cbor.value());
    ASSERT_TRUE(cborVal.type() == CBORValue::Type::Map);
    ASSERT_EQ(cborVal.getMap().size(), 2u);

    const CBORValue keyA("a");
    ASSERT_EQ(cborVal.getMap().count(keyA), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyA)->second.type() == CBORValue::Type::Unsigned);
    EXPECT_EQ(cborVal.getMap().find(keyA)->second.getInteger(), 1u);

    const CBORValue keyB("b");
    ASSERT_EQ(cborVal.getMap().count(keyB), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyB)->second.type() == CBORValue::Type::Array);

    const CBORValue nestedArray = cborVal.getMap().find(keyB)->second.clone();
    ASSERT_EQ(nestedArray.getArray().size(), 2u);
    for (int i = 0; i < 2; i++) {
        ASSERT_TRUE(nestedArray.getArray()[i].type() == CBORValue::Type::Unsigned);
        EXPECT_EQ(nestedArray.getArray()[i].getInteger(), static_cast<int64_t>(i + 2));
    }
}

TEST(CBORReaderTest, TestReadNestedMap)
{
    static const Vector<uint8_t> kNestedMapTestCase = {
        0xa2, // map of 2 pairs
        0x61, 0x61, // "a"
        0x01,

        0x61, 0x62, // "b"
        0xa2, // map of 2 pairs
        0x61, 0x63, // "c"
        0x02,

        0x61, 0x64, // "d"
        0x03,
    };

    Optional<CBORValue> cbor = CBORReader::read(kNestedMapTestCase);
    ASSERT_TRUE(cbor.hasValue());
    const CBORValue cborVal = WTFMove(cbor.value());
    ASSERT_TRUE(cborVal.type() == CBORValue::Type::Map);
    ASSERT_EQ(cborVal.getMap().size(), 2u);

    const CBORValue keyA("a");
    ASSERT_EQ(cborVal.getMap().count(keyA), 1u);
    ASSERT_TRUE(cborVal.getMap().find(keyA)->second.type() == CBORValue::Type::Unsigned);
    EXPECT_EQ(cborVal.getMap().find(keyA)->second.getInteger(), 1u);

    const CBORValue keyB("b");
    ASSERT_EQ(cborVal.getMap().count(keyB), 1u);
    const CBORValue nestedMap = cborVal.getMap().find(keyB)->second.clone();
    ASSERT_TRUE(nestedMap.type() == CBORValue::Type::Map);
    ASSERT_EQ(nestedMap.getMap().size(), 2u);

    const CBORValue keyC("c");
    ASSERT_EQ(nestedMap.getMap().count(keyC), 1u);
    ASSERT_TRUE(nestedMap.getMap().find(keyC)->second.type() == CBORValue::Type::Unsigned);
    EXPECT_EQ(nestedMap.getMap().find(keyC)->second.getInteger(), 2u);

    const CBORValue keyD("d");
    ASSERT_EQ(nestedMap.getMap().count(keyD), 1u);
    ASSERT_TRUE(nestedMap.getMap().find(keyD)->second.type() == CBORValue::Type::Unsigned);
    EXPECT_EQ(nestedMap.getMap().find(keyD)->second.getInteger(), 3u);
}

TEST(CBORReaderTest, TestIntegerRange)
{
    static const Vector<uint8_t> kMaxPositiveInt = {
        0x1b, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    static const Vector<uint8_t> kMinNegativeInt = {
        0x3b, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    Optional<CBORValue> maxPositiveInt = CBORReader::read(kMaxPositiveInt);
    ASSERT_TRUE(maxPositiveInt.hasValue());
    EXPECT_EQ(maxPositiveInt.value().getInteger(), INT64_MAX);

    Optional<CBORValue> minNegativeInt = CBORReader::read(kMinNegativeInt);
    ASSERT_TRUE(minNegativeInt.hasValue());
    EXPECT_EQ(minNegativeInt.value().getInteger(), INT64_MIN);
}

TEST(CBORReaderTest, TestIntegerOutOfRangeError)
{
    static const Vector<uint8_t> kOutOfRangePositiveInt = {
        0x1b, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    static const Vector<uint8_t> kOutOfRangeNegativeInt = {
        0x3b, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    CBORReader::DecoderError errorCode;
    Optional<CBORValue> positiveIntOutOfRangeCBOR = CBORReader::read(kOutOfRangePositiveInt, &errorCode);
    EXPECT_FALSE(positiveIntOutOfRangeCBOR);
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::OutOfRangeIntegerValue);

    Optional<CBORValue> negativeIntOutOfRangeCBOR = CBORReader::read(kOutOfRangeNegativeInt, &errorCode);
    EXPECT_FALSE(negativeIntOutOfRangeCBOR);
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::OutOfRangeIntegerValue);
}

TEST(CBORReaderTest, TestReadSimpleValue)
{
    static const struct {
        const CBORValue::SimpleValue value;
        const Vector<uint8_t> cborData;
    } kSimpleValueTestCases[] = {
        { CBORValue::SimpleValue::FalseValue, { 0xf4 } },
        { CBORValue::SimpleValue::TrueValue, { 0xf5 } },
        { CBORValue::SimpleValue::NullValue, { 0xf6 } },
        { CBORValue::SimpleValue::Undefined, { 0xf7 } },
    };

    for (const auto& testCase : kSimpleValueTestCases) {
        Optional<CBORValue> cbor = CBORReader::read(testCase.cborData);
        ASSERT_TRUE(cbor.hasValue());
        ASSERT_TRUE(cbor.value().type() == CBORValue::Type::SimpleValue);
        ASSERT_TRUE(cbor.value().getSimpleValue() == testCase.value);
    }
}

TEST(CBORReaderTest, TestReadUnsupportedFloatingPointNumbers)
{
    static const Vector<uint8_t> floatingPointCbors[] = {
        // 16 bit floating point value.
        { 0xf9, 0x10, 0x00 },
        // 32 bit floating point value.
        { 0xfa, 0x10, 0x00, 0x00, 0x00 },
        // 64 bit floating point value.
        { 0xfb, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
    };

    for (const auto& unsupported_floating_point : floatingPointCbors) {
        CBORReader::DecoderError errorCode;
        Optional<CBORValue> cbor = CBORReader::read(unsupported_floating_point, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::UnsupportedFloatingPointValue);
    }
}

TEST(CBORReaderTest, TestIncompleteCBORDataError)
{
    static const Vector<uint8_t> incompleteCborList[] = {
        // Additional info byte corresponds to unsigned int that corresponds
        // to 2 additional bytes. But actual data encoded  in one byte.
        { 0x19, 0x03 },
        // CBOR bytestring of length 3 encoded with additional info of length 4.
        { 0x44, 0x01, 0x02, 0x03 },
        // CBOR string data "IETF" of length 4 encoded with additional info of
        // length 5.
        { 0x65, 0x49, 0x45, 0x54, 0x46 },
        // CBOR array of length 1 encoded with additional info of length 2.
        { 0x82, 0x02 },
        // CBOR map with single key value pair encoded with additional info of
        // length 2.
        { 0xa2, 0x61, 0x61, 0x01 },
    };

    for (const auto& incomplete_data : incompleteCborList) {
        CBORReader::DecoderError errorCode;
        Optional<CBORValue> cbor = CBORReader::read(incomplete_data, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::IncompleteCBORData);
    }
}

// While RFC 7049 allows CBOR map keys with all types, current decoder only
// supports unsigned integer and string keys.
TEST(CBORReaderTest, TestUnsupportedMapKeyFormatError)
{
    static const Vector<uint8_t> kMapWithUintKey = {
        0xa2, // map of 2 pairs

        0x82, 0x01, 0x02, // invalid key : [1, 2]
        0x02, // value : 2

        0x61, 0x64, // key : "d"
        0x03, // value : 3
    };

    CBORReader::DecoderError errorCode;
    Optional<CBORValue> cbor = CBORReader::read(kMapWithUintKey, &errorCode);
    EXPECT_FALSE(cbor.hasValue());
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::IncorrectMapKeyType);
}

TEST(CBORReaderTest, TestUnknownAdditionalInfoError)
{
    static const Vector<uint8_t> kUnknownAdditionalInfoList[] = {
        // "IETF" encoded with major type 3 and additional info of 28.
        { 0x7C, 0x49, 0x45, 0x54, 0x46 },
        // "\"\\" encoded with major type 3 and additional info of 29.
        { 0x7D, 0x22, 0x5c },
        // "\xc3\xbc" encoded with major type 3 and additional info of 30.
        { 0x7E, 0xc3, 0xbc },
        // "\xe6\xb0\xb4" encoded with major type 3 and additional info of 31.
        { 0x7F, 0xe6, 0xb0, 0xb4 },
        // Major type 7, additional information 28: unassigned.
        { 0xFC },
        // Major type 7, additional information 29: unassigned.
        { 0xFD },
        // Major type 7, additional information 30: unassigned.
        { 0xFE },
        // Major type 7, additional information 31: "break" stop code for
        // indefinite-length items.
        { 0xFF },
    };

    for (const auto& incorrect_cbor : kUnknownAdditionalInfoList) {
        CBORReader::DecoderError errorCode;
        Optional<CBORValue> cbor = CBORReader::read(incorrect_cbor, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::UnknownAdditionalInfo);
    }
}

TEST(CBORReaderTest, TestTooMuchNestingError)
{
    static const Vector<uint8_t> kZeroDepthCBORList[] = {
        // Unsigned int with value 100.
        { 0x18, 0x64 },
        // CBOR bytestring of length 4.
        { 0x44, 0x01, 0x02, 0x03, 0x04 },
        // CBOR string of corresponding to "IETF.
        { 0x64, 0x49, 0x45, 0x54, 0x46 },
        // Empty CBOR array.
        { 0x80 },
        // Empty CBOR Map
        { 0xa0 },
    };

    for (const auto& zeroDepthData : kZeroDepthCBORList) {
        CBORReader::DecoderError errorCode;
        Optional<CBORValue> cbor = CBORReader::read(zeroDepthData, &errorCode, 0);
        EXPECT_TRUE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::CBORNoError);
    }

    // Corresponds to a CBOR structure with a nesting depth of 2:
    //      {"a": 1,
    //       "b": [2, 3]}
    static const Vector<uint8_t> kNestedCBORData = {
        0xa2, // map of 2 pairs
        0x61, 0x61, // "a"
        0x01,

        0x61, 0x62, // "b"
        0x82, // array with 2 elements
        0x02,
        0x03,
    };

    CBORReader::DecoderError errorCode;
    Optional<CBORValue> cborSingleLayerMax = CBORReader::read(kNestedCBORData, &errorCode, 1);
    EXPECT_FALSE(cborSingleLayerMax.hasValue());
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::TooMuchNesting);

    Optional<CBORValue> cborDoubleLayerMax = CBORReader::read(kNestedCBORData, &errorCode, 2);
    EXPECT_TRUE(cborDoubleLayerMax.hasValue());
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::CBORNoError);
}

TEST(CBORReaderTest, TestOutOfOrderKeyError)
{
    static const Vector<uint8_t> kMapsWithUnsortedKeys[] = {
        {0xa2, // map with 2 keys with same major type and length
            0x61, 0x62, // key "b"
            0x61, 0x42, // value :"B"

            0x61, 0x61, // key "a" (out of order byte-wise lexically)
            0x61, 0x45, // value "E"
        },
        {0xa2, // map with 2 keys with different major type
            0x61, 0x62, // key "b"
            0x02, // value 2

            // key 1000 (out of order since lower major type sorts first)
            0x19, 0x03, 0xe8,
            0x61, 0x61, // value a
        },
        {0xa2, // map with 2 keys with same major type
            0x19, 0x03, 0xe8, // key 1000  (out of order due to longer length)
            0x61, 0x61, // value "a"

            0x0a, // key 10
            0x61, 0x62}, // value "b"
    };

    CBORReader::DecoderError errorCode;
    for (const auto& unsortedMap : kMapsWithUnsortedKeys) {
        Optional<CBORValue> cbor =
        CBORReader::read(unsortedMap, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::OutOfOrderKey);
    }
}

TEST(CBORReaderTest, TestDuplicateKeyError)
{
    static const Vector<uint8_t> kMapWithDuplicateKey = {
        0xa6, // map of 6 pairs:
        0x60, // ""
        0x61, 0x2e, // "."

        0x61, 0x62, // "b"
        0x61, 0x42, // "B"

        0x61, 0x62, // "b" (Duplicate key)
        0x61, 0x43, // "C"

        0x61, 0x64, // "d"
        0x61, 0x44, // "D"

        0x61, 0x65, // "e"
        0x61, 0x44, // "D"

        0x62, 0x61, 0x61, // "aa"
        0x62, 0x41, 0x41, // "AA"
    };

    CBORReader::DecoderError errorCode;

    Optional<CBORValue> cbor = CBORReader::read(kMapWithDuplicateKey, &errorCode);
    EXPECT_FALSE(cbor.hasValue());
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::DuplicateKey);
}

// Leveraging Markus Kuhn’s UTF-8 decoder stress test. See
// http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt for details.
TEST(CBORReaderTest, TestIncorrectStringEncodingError)
{
    static const Vector<uint8_t> utf8CharacterEncodings[] = {
        // Corresponds to utf8 encoding of "퟿" (section 2.3.1 of stress test).
        { 0x63, 0xED, 0x9F, 0xBF },
        // Corresponds to utf8 encoding of "" (section 2.3.2 of stress test).
        { 0x63, 0xEE, 0x80, 0x80 },
        // Corresponds to utf8 encoding of "�"  (section 2.3.3 of stress test).
        { 0x63, 0xEF, 0xBF, 0xBD },
    };

    CBORReader::DecoderError errorCode;
    for (const auto& cbor_byte : utf8CharacterEncodings) {
        Optional<CBORValue> correctlyEncodedCbor = CBORReader::read(cbor_byte, &errorCode);
        EXPECT_TRUE(correctlyEncodedCbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::CBORNoError);
    }

    // Incorrect UTF8 encoding referenced by section 3.5.3 of the stress test.
    Vector<uint8_t> impossible_utf_byte { 0x64, 0xfe, 0xfe, 0xff, 0xff };
    Optional<CBORValue> incorrectlyEncodedCbor = CBORReader::read(impossible_utf_byte, &errorCode);
    EXPECT_FALSE(incorrectlyEncodedCbor);
    EXPECT_TRUE(errorCode == CBORReader::DecoderError::InvalidUTF8);
}

TEST(CBORReaderTest, TestExtraneousCBORDataError)
{
    static const Vector<uint8_t> zeroPaddedCborList[] = {
        // 1 extra byte after a 2-byte unsigned int.
        { 0x19, 0x03, 0x05, 0x00 },
        // 1 extra byte after a 4-byte cbor byte array.
        { 0x44, 0x01, 0x02, 0x03, 0x04, 0x00 },
        // 1 extra byte after a 4-byte string.
        { 0x64, 0x49, 0x45, 0x54, 0x46, 0x00 },
        // 1 extra byte after CBOR array of length 2.
        { 0x82, 0x01, 0x02, 0x00 },
        // 1 extra key value pair after CBOR map of size 2.
        { 0xa1, 0x61, 0x63, 0x02, 0x61, 0x64, 0x03 },
    };

    for (const auto& extraneous_cborData : zeroPaddedCborList) {
        CBORReader::DecoderError errorCode;
        Optional<CBORValue> cbor = CBORReader::read(extraneous_cborData, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::ExtraneousData);
    }
}

TEST(CBORReaderTest, TestUnsupportedSimplevalue)
{
    static const Vector<uint8_t> unsupportedSimpleValues[] = {
        // Simple value (0, unassigned)
        { 0xE0 },
        // Simple value (19, unassigned)
        { 0xF3 },
        // Simple value (24, reserved)
        { 0xF8, 0x18 },
        // Simple value (28, reserved)
        { 0xF8, 0x1C },
        // Simple value (29, reserved)
        { 0xF8, 0x1D },
        // Simple value (30, reserved)
        { 0xF8, 0x1E },
        // Simple value (31, reserved)
        { 0xF8, 0x1F },
        // Simple value (32, unassigned)
        { 0xF8, 0x20 },
        // Simple value (255, unassigned)
        { 0xF8, 0xFF },
    };

    for (const auto& unsupportedSimpleVal : unsupportedSimpleValues) {
        CBORReader::DecoderError errorCode;
        Optional<CBORValue> cbor = CBORReader::read(unsupportedSimpleVal, &errorCode);
        EXPECT_FALSE(cbor.hasValue());
        EXPECT_TRUE(errorCode == CBORReader::DecoderError::UnsupportedSimpleValue);
    }
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
