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

#include <WebCore/CBORWriter.h>
#include <limits>

// Leveraging RFC 7049 examples from https://github.com/cbor/test-vectors/blob/master/appendix_a.json.
namespace TestWebKitAPI {

using namespace cbor;

bool eq(const Vector<uint8_t>& cbor, const CString& expect)
{
    if (cbor.size() != expect.length())
        return false;
    return !memcmp(cbor.data(), reinterpret_cast<const uint8_t*>(expect.data()), cbor.size());
}

bool eq(const Vector<uint8_t>& cbor, const uint8_t* expect, const size_t expectLength)
{
    if (cbor.size() != expectLength)
        return false;
    return !memcmp(cbor.data(), expect, expectLength);
}

TEST(CBORWriterTest, TestWriteUint)
{
    typedef struct {
        const int64_t value;
        const CString cbor;
    } UintTestCase;

    static const UintTestCase kUintTestCases[] = {
        // Reminder: must specify length when creating string pieces
        // with null bytes, else the string will truncate prematurely.
        {0, CString("\x00", 1)},
        {1, CString("\x01")},
        {10, CString("\x0a")},
        {23, CString("\x17")},
        {24, CString("\x18\x18")},
        {25, CString("\x18\x19")},
        {100, CString("\x18\x64")},
        {1000, CString("\x19\x03\xe8")},
        {1000000, CString("\x1a\x00\x0f\x42\x40", 5)},
        {0xFFFFFFFF, CString("\x1a\xff\xff\xff\xff")},
        {0x100000000,
            CString("\x1b\x00\x00\x00\x01\x00\x00\x00\x00", 9)},
        {std::numeric_limits<int64_t>::max(),
            CString("\x1b\x7f\xff\xff\xff\xff\xff\xff\xff")}
    };

    for (const UintTestCase& testCase : kUintTestCases) {
        auto cbor = CBORWriter::write(CBORValue(testCase.value));
        ASSERT_TRUE(cbor.hasValue());
        EXPECT_TRUE(eq(cbor.value(), testCase.cbor));
    }
}

TEST(CBORWriterTest, TestWriteNegativeInteger)
{
    static const struct {
        const int64_t negativeInt;
        const CString cbor;
    } kNegativeIntTestCases[] = {
        { -1LL, CString("\x20") },
        { -10LL, CString("\x29") },
        { -23LL, CString("\x36") },
        { -24LL, CString("\x37") },
        { -25LL, CString("\x38\x18") },
        { -100LL, CString("\x38\x63") },
        { -1000LL, CString("\x39\x03\xe7") },
        { -4294967296LL, CString("\x3a\xff\xff\xff\xff") },
        { -4294967297LL,
            CString("\x3b\x00\x00\x00\x01\x00\x00\x00\x00", 9) },
        { std::numeric_limits<int64_t>::min(),
            CString("\x3b\x7f\xff\xff\xff\xff\xff\xff\xff") },
    };

    for (const auto& testCase : kNegativeIntTestCases) {
        auto cbor = CBORWriter::write(CBORValue(testCase.negativeInt));
        ASSERT_TRUE(cbor.hasValue());
        EXPECT_TRUE(eq(cbor.value(), testCase.cbor));
    }
}

TEST(CBORWriterTest, TestWriteBytes)
{
    typedef struct {
        const Vector<uint8_t> bytes;
        const CString cbor;
    } BytesTestCase;

    static const BytesTestCase kBytesTestCases[] = {
        { { }, CString("\x40") },
        { { 0x01, 0x02, 0x03, 0x04 }, CString("\x44\x01\x02\x03\x04") },
    };

    for (const BytesTestCase& testCase : kBytesTestCases) {
        auto cbor = CBORWriter::write(CBORValue(testCase.bytes));
        ASSERT_TRUE(cbor.hasValue());
        EXPECT_TRUE(eq(cbor.value(), testCase.cbor));
    }
}

TEST(CBORWriterTest, TestWriteString)
{
    typedef struct {
        const String string;
        const CString cbor;
    } StringTestCase;

    static const StringTestCase kStringTestCases[] = {
        { "", CString("\x60") },
        { "a", CString("\x61\x61") },
        { "IETF", CString("\x64\x49\x45\x54\x46") },
        { "\"\\", CString("\x62\x22\x5c") },
        { String::fromUTF8("\xc3\xbc"), CString("\x62\xc3\xbc") },
        { String::fromUTF8("\xe6\xb0\xb4"), CString("\x63\xe6\xb0\xb4") },
        { String::fromUTF8("\xf0\x90\x85\x91"), CString("\x64\xf0\x90\x85\x91") }
    };

    for (const StringTestCase& testCase : kStringTestCases) {
        auto cbor = CBORWriter::write(CBORValue(testCase.string));
        ASSERT_TRUE(cbor.hasValue());
        EXPECT_TRUE(eq(cbor.value(), testCase.cbor));
    }
}

TEST(CBORWriterTest, TestWriteArray)
{
    static const uint8_t kArrayTestCaseCbor[] = {
        0x98, 0x19, // array of 25 elements
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x18, 0x18, 0x19,
    };
    Vector<CBORValue> array;
    for (int64_t i = 1; i <= 25; i++)
        array.append(CBORValue(i));
    auto cbor = CBORWriter::write(CBORValue(array));
    ASSERT_TRUE(cbor.hasValue());
    EXPECT_TRUE(eq(cbor.value(), kArrayTestCaseCbor, sizeof(kArrayTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteMapWithMapValue)
{
    static const uint8_t kMapTestCaseCbor[] = {
        0xb6, // map of 8 pairs:
        0x00, // key 0
        0x61, 0x61, // value "a"

        0x17, // key 23
        0x61,  0x62, // value "b"

        0x18, 0x18, // key 24
        0x61, 0x63, // value "c"

        0x18, 0xFF, // key 255
        0x61,  0x64, // value "d"

        0x19, 0x01, 0x00, // key 256
        0x61, 0x65, // value "e"

        0x19, 0xFF, 0xFF, // key 65535
        0x61,  0x66, // value "f"

        0x1A, 0x00, 0x01, 0x00, 0x00, // key 65536
        0x61, 0x67, // value "g"

        0x1A, 0xFF, 0xFF, 0xFF, 0xFF, // key 4294967295
        0x61, 0x68, // value "h"

        // key 4294967296
        0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x61, 0x69, //  value "i"

        // key INT64_MAX
        0x1b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x61, 0x6a, //  value "j"

        0x20, // key -1
        0x61, 0x6b, // value "k"

        0x37, // key -24
        0x61,  0x6c, // value "l"

        0x38, 0x18, // key -25
        0x61, 0x6d, // value "m"

        0x38, 0xFF, // key -256
        0x61, 0x6e, // value "n"

        0x39, 0x01, 0x00, // key -257
        0x61, 0x6f, // value "o"

        0x3A, 0x00, 0x01, 0x00, 0x00, // key -65537
        0x61, 0x70, // value "p"

        0x3A, 0xFF, 0xFF, 0xFF, 0xFF, // key -4294967296
        0x61, 0x71, // value "q"

        // key -4294967297
        0x3B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x61, 0x72, //  value "r"

        // key INT64_MIN
        0x3b, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x61, 0x73, //  value "s"

        0x60, // key ""
        0x61, 0x2e, // value "."

        0x61, 0x65, // key "e"
        0x61, 0x45, // value "E"

        0x62, 0x61, 0x61, // key "aa"
        0x62, 0x41, 0x41, // value "AA"
    };
    CBORValue::MapValue map;
    // Shorter strings sort first in CTAP, thus the “aa” value should be
    // serialised last in the map.
    map[CBORValue("aa")] = CBORValue("AA");
    map[CBORValue("e")] = CBORValue("E");
    // The empty string is shorter than all others, so should appear first among
    // the strings.
    map[CBORValue("")] = CBORValue(".");
    // Map keys are sorted by major type, by byte length, and then by
    // byte-wise lexical order. So all integer type keys should appear before
    // key "" and all positive integer keys should appear before negative integer
    // keys.
    map[CBORValue(-1)] = CBORValue("k");
    map[CBORValue(-24)] = CBORValue("l");
    map[CBORValue(-25)] = CBORValue("m");
    map[CBORValue(-256)] = CBORValue("n");
    map[CBORValue(-257)] = CBORValue("o");
    map[CBORValue(-65537)] = CBORValue("p");
    map[CBORValue(int64_t(-4294967296))] = CBORValue("q");
    map[CBORValue(int64_t(-4294967297))] = CBORValue("r");
    map[CBORValue(std::numeric_limits<int64_t>::min())] = CBORValue("s");
    map[CBORValue(0)] = CBORValue("a");
    map[CBORValue(23)] = CBORValue("b");
    map[CBORValue(24)] = CBORValue("c");
    map[CBORValue(std::numeric_limits<uint8_t>::max())] = CBORValue("d");
    map[CBORValue(256)] = CBORValue("e");
    map[CBORValue(std::numeric_limits<uint16_t>::max())] = CBORValue("f");
    map[CBORValue(65536)] = CBORValue("g");
    map[CBORValue(int64_t(std::numeric_limits<uint32_t>::max()))] = CBORValue("h");
    map[CBORValue(int64_t(4294967296))] = CBORValue("i");
    map[CBORValue(std::numeric_limits<int64_t>::max())] = CBORValue("j");
    auto cbor = CBORWriter::write(CBORValue(map));
    ASSERT_TRUE(cbor.hasValue());
    EXPECT_TRUE(eq(cbor.value(), kMapTestCaseCbor, sizeof(kMapTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteMapWithArray)
{
    static const uint8_t kMapArrayTestCaseCbor[] = {
        0xa2, // map of 2 pairs
        0x61, 0x61, // "a"
        0x01,

        0x61, 0x62, // "b"
        0x82, // array with 2 elements
        0x02,
        0x03,
    };
    CBORValue::MapValue map;
    map[CBORValue("a")] = CBORValue(1);
    CBORValue::ArrayValue array;
    array.append(CBORValue(2));
    array.append(CBORValue(3));
    map[CBORValue("b")] = CBORValue(array);
    auto cbor = CBORWriter::write(CBORValue(map));
    ASSERT_TRUE(cbor.hasValue());
    EXPECT_TRUE(eq(cbor.value(), kMapArrayTestCaseCbor, sizeof(kMapArrayTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteNestedMap)
{
    static const uint8_t kNestedMapTestCase[] = {
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
    CBORValue::MapValue map;
    map[CBORValue("a")] = CBORValue(1);
    CBORValue::MapValue nestedMap;
    nestedMap[CBORValue("c")] = CBORValue(2);
    nestedMap[CBORValue("d")] = CBORValue(3);
    map[CBORValue("b")] = CBORValue(nestedMap);
    auto cbor = CBORWriter::write(CBORValue(map));
    ASSERT_TRUE(cbor.hasValue());
    EXPECT_TRUE(eq(cbor.value(), kNestedMapTestCase, sizeof(kNestedMapTestCase)));
}

TEST(CBORWriterTest, TestWriteSimpleValue)
{
    static const struct {
        CBORValue::SimpleValue simpleValue;
        const CString cbor;
    } kSimpleTestCase[] = {
        { CBORValue::SimpleValue::FalseValue, CString("\xf4") },
        { CBORValue::SimpleValue::TrueValue, CString("\xf5") },
        { CBORValue::SimpleValue::NullValue, CString("\xf6") },
        { CBORValue::SimpleValue::Undefined, CString("\xf7") }
    };

    for (const auto& testCase : kSimpleTestCase) {
        auto cbor = CBORWriter::write(CBORValue(testCase.simpleValue));
        ASSERT_TRUE(cbor.hasValue());
        EXPECT_TRUE(eq(cbor.value(), testCase.cbor));
    }
}

// For major type 0, 2, 3, empty CBOR array, and empty CBOR map, the nesting
// depth is expected to be 0 since the CBOR decoder does not need to parse
// any nested CBOR value elements.
TEST(CBORWriterTest, TestWriteSingleLayer)
{
    const CBORValue simpleUint = CBORValue(1);
    const CBORValue simpleString = CBORValue("a");
    const Vector<uint8_t> byteData = { 0x01, 0x02, 0x03, 0x04 };
    const CBORValue simpleBytestring = CBORValue(byteData);
    CBORValue::ArrayValue emptyCborArray;
    CBORValue::MapValue emptyCborMap;
    const CBORValue emptyArrayValue = CBORValue(emptyCborArray);
    const CBORValue emptyMapValue = CBORValue(emptyCborMap);
    CBORValue::ArrayValue simpleArray;
    simpleArray.append(CBORValue(2));
    CBORValue::MapValue simpleMap;
    simpleMap[CBORValue("b")] = CBORValue(3);
    const CBORValue singleLayerCborMap = CBORValue(simpleMap);
    const CBORValue singleLayerCborArray = CBORValue(simpleArray);

    EXPECT_TRUE(CBORWriter::write(simpleUint, 0).hasValue());
    EXPECT_TRUE(CBORWriter::write(simpleString, 0).hasValue());
    EXPECT_TRUE(CBORWriter::write(simpleBytestring, 0).hasValue());

    EXPECT_TRUE(CBORWriter::write(emptyArrayValue, 0).hasValue());
    EXPECT_TRUE(CBORWriter::write(emptyMapValue, 0).hasValue());

    EXPECT_FALSE(CBORWriter::write(singleLayerCborArray, 0).hasValue());
    EXPECT_TRUE(CBORWriter::write(singleLayerCborArray, 1).hasValue());

    EXPECT_FALSE(CBORWriter::write(singleLayerCborMap, 0).hasValue());
    EXPECT_TRUE(CBORWriter::write(singleLayerCborMap, 1).hasValue());
}

// Major type 5 nested CBOR map value with following structure.
//     {"a": 1,
//      "b": {"c": 2,
//            "d": 3}}
TEST(CBORWriterTest, NestedMaps)
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue("a")] = CBORValue(1);
    CBORValue::MapValue nestedMap;
    nestedMap[CBORValue("c")] = CBORValue(2);
    nestedMap[CBORValue("d")] = CBORValue(3);
    cborMap[CBORValue("b")] = CBORValue(nestedMap);
    EXPECT_TRUE(CBORWriter::write(CBORValue(cborMap), 2).hasValue());
    EXPECT_FALSE(CBORWriter::write(CBORValue(cborMap), 1).hasValue());
}

// Testing Write() function for following CBOR structure with depth of 3.
//     [1,
//      2,
//      3,
//      {"a": 1,
//       "b": {"c": 2,
//             "d": 3}}]
TEST(CBORWriterTest, UnbalancedNestedContainers)
{
    CBORValue::ArrayValue cborArray;
    CBORValue::MapValue cborMap;
    CBORValue::MapValue nestedMap;

    cborMap[CBORValue("a")] = CBORValue(1);
    nestedMap[CBORValue("c")] = CBORValue(2);
    nestedMap[CBORValue("d")] = CBORValue(3);
    cborMap[CBORValue("b")] = CBORValue(nestedMap);
    cborArray.append(CBORValue(1));
    cborArray.append(CBORValue(2));
    cborArray.append(CBORValue(3));
    cborArray.append(CBORValue(cborMap));

    EXPECT_TRUE(CBORWriter::write(CBORValue(cborArray), 3).hasValue());
    EXPECT_FALSE(CBORWriter::write(CBORValue(cborArray), 2).hasValue());
}

// Testing Write() function for following CBOR structure.
//     {"a": 1,
//      "b": {"c": 2,
//            "d": 3
//            "h": { "e": 4,
//                   "f": 5,
//                   "g": [6, 7, [8]]}}}
// Since above CBOR contains 5 nesting levels. Thus, Write() is expected to
// return empty optional object when maximum nesting layer size is set to 4.
TEST(CBORWriterTest, OverlyNestedCBOR)
{
    CBORValue::MapValue map;
    CBORValue::MapValue nestedMap;
    CBORValue::MapValue innerNestedMap;
    CBORValue::ArrayValue innerArray;
    CBORValue::ArrayValue array;

    map[CBORValue("a")] = CBORValue(1);
    nestedMap[CBORValue("c")] = CBORValue(2);
    nestedMap[CBORValue("d")] = CBORValue(3);
    innerNestedMap[CBORValue("e")] = CBORValue(4);
    innerNestedMap[CBORValue("f")] = CBORValue(5);
    innerArray.append(CBORValue(6));
    array.append(CBORValue(6));
    array.append(CBORValue(7));
    array.append(CBORValue(innerArray));
    innerNestedMap[CBORValue("g")] = CBORValue(array);
    nestedMap[CBORValue("h")] = CBORValue(innerNestedMap);
    map[CBORValue("b")] = CBORValue(nestedMap);

    EXPECT_TRUE(CBORWriter::write(CBORValue(map), 5).hasValue());
    EXPECT_FALSE(CBORWriter::write(CBORValue(map), 4).hasValue());
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
