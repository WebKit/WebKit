/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "IDBLevelDBCoding.h"

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "LevelDBSlice.h"
#include <gtest/gtest.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace IDBLevelDBCoding;

namespace {

static PassRefPtr<IDBKey> createArrayIDBKey()
{
    return IDBKey::createArray(IDBKey::KeyArray());
}

static PassRefPtr<IDBKey> createArrayIDBKey(PassRefPtr<IDBKey> prpKey1)
{
    RefPtr<IDBKey> key1 = prpKey1;

    IDBKey::KeyArray array;
    array.append(key1);
    return IDBKey::createArray(array);
}

static PassRefPtr<IDBKey> createArrayIDBKey(PassRefPtr<IDBKey> prpKey1, PassRefPtr<IDBKey> prpKey2)
{
    RefPtr<IDBKey> key1 = prpKey1;
    RefPtr<IDBKey> key2 = prpKey2;

    IDBKey::KeyArray array;
    array.append(key1);
    array.append(key2);
    return IDBKey::createArray(array);
}

TEST(IDBLevelDBCodingTest, EncodeByte)
{
    Vector<char> expected;
    expected.append(0);
    unsigned char c;

    c = 0;
    expected[0] = c;
    EXPECT_EQ(expected, encodeByte(c));

    c = 1;
    expected[0] = c;
    EXPECT_EQ(expected, encodeByte(c));

    c = 255;
    expected[0] = c;
    EXPECT_EQ(expected, encodeByte(c));
}

TEST(IDBLevelDBCodingTest, DecodeByte)
{
    Vector<unsigned char> testCases;
    testCases.append(0);
    testCases.append(1);
    testCases.append(255);

    for (size_t i = 0; i < testCases.size(); ++i) {
        unsigned char n = testCases[i];
        Vector<char> v = encodeByte(n);

        unsigned char res;
        const char* p = decodeByte(v.data(), v.data() + v.size(), res);
        EXPECT_EQ(n, res);
        EXPECT_EQ(v.data() + v.size(), p);
    }
}
   
TEST(IDBLevelDBCodingTest, EncodeBool)
{
    {
        Vector<char> expected;
        expected.append(1);
        EXPECT_EQ(expected, encodeBool(true));
    }
    {
        Vector<char> expected;
        expected.append(0);
        EXPECT_EQ(expected, encodeBool(false));
    }
}

static int compareKeys(const Vector<char>& a, const Vector<char>& b)
{
    bool ok;
    int result = compareEncodedIDBKeys(a, b, ok);
    EXPECT_TRUE(ok);
    return result;
}

TEST(IDBLevelDBCodingTest, MaxIDBKey)
{
    Vector<char> maxKey = maxIDBKey();

    Vector<char> minKey = minIDBKey();
    Vector<char> arrayKey = encodeIDBKey(*IDBKey::createArray(IDBKey::KeyArray()));
    Vector<char> stringKey = encodeIDBKey(*IDBKey::createString("Hello world"));
    Vector<char> numberKey = encodeIDBKey(*IDBKey::createNumber(3.14));
    Vector<char> dateKey = encodeIDBKey(*IDBKey::createDate(1000000));

    EXPECT_GT(compareKeys(maxKey, minKey), 0);
    EXPECT_GT(compareKeys(maxKey, arrayKey), 0);
    EXPECT_GT(compareKeys(maxKey, stringKey), 0);
    EXPECT_GT(compareKeys(maxKey, numberKey), 0);
    EXPECT_GT(compareKeys(maxKey, dateKey), 0);
}

TEST(IDBLevelDBCodingTest, MinIDBKey)
{
    Vector<char> minKey = minIDBKey();

    Vector<char> maxKey = maxIDBKey();
    Vector<char> arrayKey = encodeIDBKey(*IDBKey::createArray(IDBKey::KeyArray()));
    Vector<char> stringKey = encodeIDBKey(*IDBKey::createString("Hello world"));
    Vector<char> numberKey = encodeIDBKey(*IDBKey::createNumber(3.14));
    Vector<char> dateKey = encodeIDBKey(*IDBKey::createDate(1000000));

    EXPECT_LT(compareKeys(minKey, maxKey), 0);
    EXPECT_LT(compareKeys(minKey, arrayKey), 0);
    EXPECT_LT(compareKeys(minKey, stringKey), 0);
    EXPECT_LT(compareKeys(minKey, numberKey), 0);
    EXPECT_LT(compareKeys(minKey, dateKey), 0);
}

TEST(IDBLevelDBCodingTest, EncodeInt)
{
    EXPECT_EQ(static_cast<size_t>(1), encodeInt(0).size());
    EXPECT_EQ(static_cast<size_t>(1), encodeInt(1).size());
    EXPECT_EQ(static_cast<size_t>(1), encodeInt(255).size());
    EXPECT_EQ(static_cast<size_t>(2), encodeInt(256).size());
    EXPECT_EQ(static_cast<size_t>(4), encodeInt(0xffffffff).size());
#ifdef NDEBUG
    EXPECT_EQ(static_cast<size_t>(8), encodeInt(-1).size());
#endif
}

TEST(IDBLevelDBCodingTest, DecodeBool)
{
    {
        Vector<char> encoded;
        encoded.append(1);
        EXPECT_TRUE(decodeBool(encoded.data(), encoded.data() + encoded.size()));
    }
    {
        Vector<char> encoded;
        encoded.append(0);
        EXPECT_FALSE(decodeBool(encoded.data(), encoded.data() + encoded.size()));
    }
}

TEST(IDBLevelDBCodingTest, DecodeInt)
{
    Vector<int64_t> testCases;
    testCases.append(0);
    testCases.append(1);
    testCases.append(255);
    testCases.append(256);
    testCases.append(65535);
    testCases.append(655536);
    testCases.append(7711192431755665792ll);
    testCases.append(0x7fffffffffffffffll);
#ifdef NDEBUG
    testCases.append(-3);
#endif

    for (size_t i = 0; i < testCases.size(); ++i) {
        int64_t n = testCases[i];
        Vector<char> v = encodeInt(n);
        EXPECT_EQ(n, decodeInt(v.data(), v.data() + v.size()));
    }
}

TEST(IDBLevelDBCodingTest, EncodeVarInt)
{
    EXPECT_EQ(static_cast<size_t>(1), encodeVarInt(0).size());
    EXPECT_EQ(static_cast<size_t>(1), encodeVarInt(1).size());
    EXPECT_EQ(static_cast<size_t>(2), encodeVarInt(255).size());
    EXPECT_EQ(static_cast<size_t>(2), encodeVarInt(256).size());
    EXPECT_EQ(static_cast<size_t>(5), encodeVarInt(0xffffffff).size());
    EXPECT_EQ(static_cast<size_t>(8), encodeVarInt(0xfffffffffffffLL).size());
    EXPECT_EQ(static_cast<size_t>(9), encodeVarInt(0x7fffffffffffffffLL).size());
#ifdef NDEBUG
    EXPECT_EQ(static_cast<size_t>(10), encodeVarInt(-100).size());
#endif
}

TEST(IDBLevelDBCodingTest, DecodeVarInt)
{
    Vector<int64_t> testCases;
    testCases.append(0);
    testCases.append(1);
    testCases.append(255);
    testCases.append(256);
    testCases.append(65535);
    testCases.append(655536);
    testCases.append(7711192431755665792ll);
    testCases.append(0x7fffffffffffffffll);
#ifdef NDEBUG
    testCases.append(-3);
#endif

    for (size_t i = 0; i < testCases.size(); ++i) {
        int64_t n = testCases[i];
        Vector<char> v = encodeVarInt(n);

        int64_t res;
        const char* p = decodeVarInt(v.data(), v.data() + v.size(), res);
        EXPECT_EQ(n, res);
        EXPECT_EQ(v.data() + v.size(), p);

        p = decodeVarInt(v.data(), v.data() + v.size() - 1, res);
        EXPECT_EQ(0, p);
        p = decodeVarInt(v.data(), v.data(), res);
        EXPECT_EQ(0, p);
    }
}

TEST(IDBLevelDBCodingTest, EncodeString)
{
    const UChar testStringA[] = {'f', 'o', 'o', '\0'};
    const UChar testStringB[] = {0xdead, 0xbeef, '\0'};

    EXPECT_EQ(static_cast<size_t>(0), encodeString(String("")).size());
    EXPECT_EQ(static_cast<size_t>(2), encodeString(String("a")).size());
    EXPECT_EQ(static_cast<size_t>(6), encodeString(String("foo")).size());
    EXPECT_EQ(static_cast<size_t>(6), encodeString(String(testStringA)).size());
    EXPECT_EQ(static_cast<size_t>(4), encodeString(String(testStringB)).size());
}

TEST(IDBLevelDBCodingTest, DecodeString)
{
    const UChar testStringA[] = {'f', 'o', 'o', '\0'};
    const UChar testStringB[] = {0xdead, 0xbeef, '\0'};
    Vector<char> v;

    v = encodeString(String(""));
    EXPECT_EQ(String(""), decodeString(v.data(), v.data() + v.size()));

    v = encodeString(String("a"));
    EXPECT_EQ(String("a"), decodeString(v.data(), v.data() + v.size()));

    v = encodeString(String("foo"));
    EXPECT_EQ(String("foo"), decodeString(v.data(), v.data() + v.size()));

    v = encodeString(String(testStringA));
    EXPECT_EQ(String(testStringA), decodeString(v.data(), v.data() + v.size()));

    v = encodeString(String(testStringB));
    EXPECT_EQ(String(testStringB), decodeString(v.data(), v.data() + v.size()));
}

TEST(IDBLevelDBCodingTest, EncodeStringWithLength)
{
    const UChar testStringA[] = {'f', 'o', 'o', '\0'};
    const UChar testStringB[] = {0xdead, 0xbeef, '\0'};

    EXPECT_EQ(static_cast<size_t>(1), encodeStringWithLength(String("")).size());
    EXPECT_EQ(static_cast<size_t>(3), encodeStringWithLength(String("a")).size());
    EXPECT_EQ(static_cast<size_t>(7), encodeStringWithLength(String(testStringA)).size());
    EXPECT_EQ(static_cast<size_t>(5), encodeStringWithLength(String(testStringB)).size());
}

TEST(IDBLevelDBCodingTest, DecodeStringWithLength)
{
    const UChar testStringA[] = {'f', 'o', 'o', '\0'};
    const UChar testStringB[] = {0xdead, 0xbeef, '\0'};

    const int kLongStringLen = 1234;
    UChar longString[kLongStringLen + 1];
    for (int i = 0; i < kLongStringLen; ++i)
        longString[i] = i;
    longString[kLongStringLen] = 0;

    Vector<String> testCases;
    testCases.append(String(""));
    testCases.append(String("a"));
    testCases.append(String("foo"));
    testCases.append(String(testStringA));
    testCases.append(String(testStringB));
    testCases.append(String(longString));

    for (size_t i = 0; i < testCases.size(); ++i) {
        String s = testCases[i];
        Vector<char> v = encodeStringWithLength(s);
        String res;
        const char* p = decodeStringWithLength(v.data(), v.data() + v.size(), res);
        EXPECT_EQ(s, res);
        EXPECT_EQ(v.data() + v.size(), p);

        EXPECT_EQ(0, decodeStringWithLength(v.data(), v.data() + v.size() - 1, res));
        EXPECT_EQ(0, decodeStringWithLength(v.data(), v.data(), res));
    }
}

static int compareStrings(const char* p, const char* limitP, const char* q, const char* limitQ)
{
    bool ok;
    int result = compareEncodedStringsWithLength(p, limitP, q, limitQ, ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(p, limitP);
    EXPECT_EQ(q, limitQ);
    return result;
}

TEST(IDBLevelDBCodingTest, CompareEncodedStringsWithLength)
{
    const UChar testStringA[] = {0x1000, 0x1000, '\0'};
    const UChar testStringB[] = {0x1000, 0x1000, 0x1000, '\0'};
    const UChar testStringC[] = {0x1000, 0x1000, 0x1001, '\0'};
    const UChar testStringD[] = {0x1001, 0x1000, 0x1000, '\0'};
    const UChar testStringE[] = {0xd834, 0xdd1e, '\0'};
    const UChar testStringF[] = {0xfffd, '\0'};

    Vector<String> testCases;
    testCases.append(String(""));
    testCases.append(String("a"));
    testCases.append(String("b"));
    testCases.append(String("baaa"));
    testCases.append(String("baab"));
    testCases.append(String("c"));
    testCases.append(String(testStringA));
    testCases.append(String(testStringB));
    testCases.append(String(testStringC));
    testCases.append(String(testStringD));
    testCases.append(String(testStringE));
    testCases.append(String(testStringF));

    for (size_t i = 0; i < testCases.size() - 1; ++i) {
        String a = testCases[i];
        String b = testCases[i + 1];

        EXPECT_LT(codePointCompare(a, b), 0);
        EXPECT_GT(codePointCompare(b, a), 0);
        EXPECT_EQ(codePointCompare(a, a), 0);
        EXPECT_EQ(codePointCompare(b, b), 0);

        Vector<char> encodedA = encodeStringWithLength(a);
        EXPECT_TRUE(encodedA.size());
        Vector<char> encodedB = encodeStringWithLength(b);
        EXPECT_TRUE(encodedA.size());

        const char* p = encodedA.data();
        const char* limitP = p + encodedA.size();
        const char* q = encodedB.data();
        const char* limitQ = q + encodedB.size();

        EXPECT_LT(compareStrings(p, limitP, q, limitQ), 0);
        EXPECT_GT(compareStrings(q, limitQ, p, limitP), 0);
        EXPECT_EQ(compareStrings(p, limitP, p, limitP), 0);
        EXPECT_EQ(compareStrings(q, limitQ, q, limitQ), 0);
    }
}

TEST(IDBLevelDBCodingTest, EncodeDouble)
{
    EXPECT_EQ(static_cast<size_t>(8), encodeDouble(0).size());
    EXPECT_EQ(static_cast<size_t>(8), encodeDouble(3.14).size());
}

TEST(IDBLevelDBCodingTest, DecodeDouble)
{
    Vector<char> v;
    const char* p;
    double d;

    v = encodeDouble(3.14);
    p = decodeDouble(v.data(), v.data() + v.size(), &d);
    EXPECT_EQ(3.14, d);
    EXPECT_EQ(v.data() + v.size(), p);

    v = encodeDouble(-3.14);
    p = decodeDouble(v.data(), v.data() + v.size(), &d);
    EXPECT_EQ(-3.14, d);
    EXPECT_EQ(v.data() + v.size(), p);

    v = encodeDouble(3.14);
    p = decodeDouble(v.data(), v.data() + v.size() - 1, &d);
    EXPECT_EQ(0, p);
}

TEST(IDBLevelDBCodingTest, EncodeDecodeIDBKey)
{
    RefPtr<IDBKey> expectedKey;
    RefPtr<IDBKey> decodedKey;
    Vector<char> v;
    const char* p;

    expectedKey = IDBKey::createNumber(1234);
    v = encodeIDBKey(*expectedKey);
    p = decodeIDBKey(v.data(), v.data() + v.size(), decodedKey);
    EXPECT_TRUE(decodedKey->isEqual(expectedKey.get()));
    EXPECT_EQ(v.data() + v.size(), p);
    EXPECT_EQ(0, decodeIDBKey(v.data(), v.data() + v.size() - 1, decodedKey));

    expectedKey = IDBKey::createString("Hello World!");
    v = encodeIDBKey(*expectedKey);
    p = decodeIDBKey(v.data(), v.data() + v.size(), decodedKey);
    EXPECT_TRUE(decodedKey->isEqual(expectedKey.get()));
    EXPECT_EQ(v.data() + v.size(), p);
    EXPECT_EQ(0, decodeIDBKey(v.data(), v.data() + v.size() - 1, decodedKey));

    expectedKey = createArrayIDBKey();
    v = encodeIDBKey(*expectedKey);
    p = decodeIDBKey(v.data(), v.data() + v.size(), decodedKey);
    EXPECT_TRUE(decodedKey->isEqual(expectedKey.get()));
    EXPECT_EQ(v.data() + v.size(), p);
    EXPECT_EQ(0, decodeIDBKey(v.data(), v.data() + v.size() - 1, decodedKey));

    expectedKey = IDBKey::createDate(7890);
    v = encodeIDBKey(*expectedKey);
    p = decodeIDBKey(v.data(), v.data() + v.size(), decodedKey);
    EXPECT_TRUE(decodedKey->isEqual(expectedKey.get()));
    EXPECT_EQ(v.data() + v.size(), p);
    EXPECT_EQ(0, decodeIDBKey(v.data(), v.data() + v.size() - 1, decodedKey));

    IDBKey::KeyArray array;
    array.append(IDBKey::createNumber(1234));
    array.append(IDBKey::createString("Hello World!"));
    array.append(IDBKey::createDate(7890));
    expectedKey = IDBKey::createArray(array);
    v = encodeIDBKey(*expectedKey);
    p = decodeIDBKey(v.data(), v.data() + v.size(), decodedKey);
    EXPECT_TRUE(decodedKey->isEqual(expectedKey.get()));
    EXPECT_EQ(v.data() + v.size(), p);
    EXPECT_EQ(0, decodeIDBKey(v.data(), v.data() + v.size() - 1, decodedKey));
}

TEST(IDBLevelDBCodingTest, EncodeIDBKeyPath)
{
    const unsigned char kIDBKeyPathTypeCodedByte1 = 0;
    const unsigned char kIDBKeyPathTypeCodedByte2 = 0;
    {
        IDBKeyPath keyPath;
        EXPECT_EQ(keyPath.type(), IDBKeyPath::NullType);
        Vector<char> v = encodeIDBKeyPath(keyPath);
        EXPECT_EQ(v.size(), 3U);
        EXPECT_EQ(v[0], kIDBKeyPathTypeCodedByte1);
        EXPECT_EQ(v[1], kIDBKeyPathTypeCodedByte2);
        EXPECT_EQ(v[2], IDBKeyPath::NullType);
    }

    {
        Vector<String> testCases;
        testCases.append("");
        testCases.append("foo");
        testCases.append("foo.bar");

        for (size_t i = 0; i < testCases.size(); ++i) {
            IDBKeyPath keyPath = IDBKeyPath(testCases[i]);
            Vector<char> v = encodeIDBKeyPath(keyPath);
            EXPECT_EQ(v.size(), encodeStringWithLength(testCases[i]).size() + 3);
            const char* p = v.data();
            const char* limit = v.data() + v.size();
            EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte1);
            EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte2);
            EXPECT_EQ(*p++, IDBKeyPath::StringType);
            String string;
            p = decodeStringWithLength(p, limit, string);
            EXPECT_EQ(string, testCases[i]);
            EXPECT_EQ(p, limit);
        }
    }

    {
        Vector<String> testCase;
        testCase.append("");
        testCase.append("foo");
        testCase.append("foo.bar");

        IDBKeyPath keyPath(testCase);
        EXPECT_EQ(keyPath.type(), IDBKeyPath::ArrayType);
        Vector<char> v = encodeIDBKeyPath(keyPath);
        const char* p = v.data();
        const char* limit = v.data() + v.size();
        EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte1);
        EXPECT_EQ(*p++, kIDBKeyPathTypeCodedByte2);
        EXPECT_EQ(*p++, IDBKeyPath::ArrayType);
        int64_t count;
        p = decodeVarInt(p, limit, count);
        EXPECT_EQ(count, static_cast<int64_t>(testCase.size()));
        for (size_t i = 0; i < static_cast<size_t>(count); ++i) {
            String string;
            p = decodeStringWithLength(p, limit, string);
            EXPECT_EQ(string, testCase[i]);
        }
        EXPECT_EQ(p, limit);
    }
}

TEST(IDBLevelDBCodingTest, DecodeIDBKeyPath)
{
    const unsigned char kIDBKeyPathTypeCodedByte1 = 0;
    const unsigned char kIDBKeyPathTypeCodedByte2 = 0;
    {
        // Legacy encoding of string key paths.
        Vector<String> testCases;
        testCases.append("");
        testCases.append("foo");
        testCases.append("foo.bar");

        for (size_t i = 0; i < testCases.size(); ++i) {
            Vector<char> v = encodeString(testCases[i]);
            IDBKeyPath keyPath = decodeIDBKeyPath(v.data(), v.data() + v.size());
            EXPECT_EQ(keyPath.type(), IDBKeyPath::StringType);
            EXPECT_EQ(testCases[i], keyPath.string());
        }
    }
    {
        Vector<char> v;
        v.append(kIDBKeyPathTypeCodedByte1);
        v.append(kIDBKeyPathTypeCodedByte2);
        v.append(IDBKeyPath::NullType);
        IDBKeyPath keyPath = decodeIDBKeyPath(v.data(), v.data() + v.size());
        EXPECT_EQ(keyPath.type(), IDBKeyPath::NullType);
        EXPECT_TRUE(keyPath.isNull());
    }
    {
        Vector<String> testCases;
        testCases.append("");
        testCases.append("foo");
        testCases.append("foo.bar");

        for (size_t i = 0; i < testCases.size(); ++i) {
            Vector<char> v;
            v.append(kIDBKeyPathTypeCodedByte1);
            v.append(kIDBKeyPathTypeCodedByte2);
            v.append(IDBKeyPath::StringType);
            v.append(encodeStringWithLength(testCases[i]));
            IDBKeyPath keyPath = decodeIDBKeyPath(v.data(), v.data() + v.size());
            EXPECT_EQ(keyPath.type(), IDBKeyPath::StringType);
            EXPECT_EQ(testCases[i], keyPath.string());
        }
    }
    {
        Vector<String> testCase;
        testCase.append("");
        testCase.append("foo");
        testCase.append("foo.bar");

        Vector<char> v;
        v.append(kIDBKeyPathTypeCodedByte1);
        v.append(kIDBKeyPathTypeCodedByte2);
        v.append(IDBKeyPath::ArrayType);
        v.append(encodeVarInt(testCase.size()));
        for (size_t i = 0; i < testCase.size(); ++i)
            v.append(encodeStringWithLength(testCase[i]));
        IDBKeyPath keyPath = decodeIDBKeyPath(v.data(), v.data() + v.size());
        EXPECT_EQ(keyPath.type(), IDBKeyPath::ArrayType);
        EXPECT_EQ(keyPath.array().size(), testCase.size());
        for (size_t i = 0; i < testCase.size(); ++i)
            EXPECT_EQ(keyPath.array()[i], testCase[i]);
    }
}

TEST(IDBLevelDBCodingTest, ExtractAndCompareIDBKeys)
{
    Vector<RefPtr<IDBKey> > keys;

    keys.append(IDBKey::createNumber(-10));
    keys.append(IDBKey::createNumber(0));
    keys.append(IDBKey::createNumber(3.14));

    keys.append(IDBKey::createDate(0));
    keys.append(IDBKey::createDate(100));
    keys.append(IDBKey::createDate(100000));

    keys.append(IDBKey::createString(""));
    keys.append(IDBKey::createString("a"));
    keys.append(IDBKey::createString("b"));
    keys.append(IDBKey::createString("baaa"));
    keys.append(IDBKey::createString("baab"));
    keys.append(IDBKey::createString("c"));

    keys.append(createArrayIDBKey());
    keys.append(createArrayIDBKey(IDBKey::createNumber(0)));
    keys.append(createArrayIDBKey(IDBKey::createNumber(0), IDBKey::createNumber(3.14)));
    keys.append(createArrayIDBKey(IDBKey::createDate(0)));
    keys.append(createArrayIDBKey(IDBKey::createDate(0), IDBKey::createDate(0)));
    keys.append(createArrayIDBKey(IDBKey::createString("")));
    keys.append(createArrayIDBKey(IDBKey::createString(""), IDBKey::createString("a")));
    keys.append(createArrayIDBKey(createArrayIDBKey()));
    keys.append(createArrayIDBKey(createArrayIDBKey(), createArrayIDBKey()));
    keys.append(createArrayIDBKey(createArrayIDBKey(createArrayIDBKey())));
    keys.append(createArrayIDBKey(createArrayIDBKey(createArrayIDBKey(createArrayIDBKey()))));

    for (size_t i = 0; i < keys.size() - 1; ++i) {
        RefPtr<IDBKey> keyA = keys[i];
        RefPtr<IDBKey> keyB = keys[i + 1];

        EXPECT_TRUE(keyA->isLessThan(keyB.get()));

        Vector<char> encodedA = encodeIDBKey(*keyA);
        EXPECT_TRUE(encodedA.size());
        Vector<char> encodedB = encodeIDBKey(*keyB);
        EXPECT_TRUE(encodedB.size());

        Vector<char> extractedA;
        Vector<char> extractedB;

        const char* p = extractEncodedIDBKey(encodedA.data(), encodedA.data() + encodedA.size(), &extractedA);
        EXPECT_EQ(encodedA.data() + encodedA.size(), p);
        EXPECT_EQ(encodedA, extractedA);

        const char* q = extractEncodedIDBKey(encodedB.data(), encodedB.data() + encodedB.size(), &extractedB);
        EXPECT_EQ(encodedB.data() + encodedB.size(), q);
        EXPECT_EQ(encodedB, extractedB);

        EXPECT_LT(compareKeys(extractedA, extractedB), 0);
        EXPECT_GT(compareKeys(extractedB, extractedA), 0);
        EXPECT_EQ(compareKeys(extractedA, extractedA), 0);
        EXPECT_EQ(compareKeys(extractedB, extractedB), 0);

        EXPECT_EQ(0, extractEncodedIDBKey(encodedA.data(), encodedA.data() + encodedA.size() - 1, &extractedA));
    }
}

TEST(IDBLevelDBCodingTest, ComparisonTest)
{
    Vector<Vector<char> > keys;
    keys.append(SchemaVersionKey::encode());
    keys.append(MaxDatabaseIdKey::encode());
    keys.append(DatabaseFreeListKey::encode(0));
    keys.append(DatabaseFreeListKey::encodeMaxKey());
    keys.append(DatabaseNameKey::encode("", ""));
    keys.append(DatabaseNameKey::encode("", "a"));
    keys.append(DatabaseNameKey::encode("a", "a"));
    keys.append(DatabaseMetaDataKey::encode(1, DatabaseMetaDataKey::OriginName));
    keys.append(DatabaseMetaDataKey::encode(1, DatabaseMetaDataKey::DatabaseName));
    keys.append(DatabaseMetaDataKey::encode(1, DatabaseMetaDataKey::UserVersion));
    keys.append(DatabaseMetaDataKey::encode(1, DatabaseMetaDataKey::MaxObjectStoreId));
    keys.append(DatabaseMetaDataKey::encode(1, DatabaseMetaDataKey::UserIntVersion));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::Name));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::KeyPath));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::AutoIncrement));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::Evictable));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::LastVersion));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::MaxIndexId));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::HasKeyPath));
    keys.append(ObjectStoreMetaDataKey::encode(1, 1, ObjectStoreMetaDataKey::KeyGeneratorCurrentNumber));
    keys.append(ObjectStoreMetaDataKey::encodeMaxKey(1, 1));
    keys.append(ObjectStoreMetaDataKey::encodeMaxKey(1, 2));
    keys.append(ObjectStoreMetaDataKey::encodeMaxKey(1));
    keys.append(IndexMetaDataKey::encode(1, 1, 30, IndexMetaDataKey::Name));
    keys.append(IndexMetaDataKey::encode(1, 1, 30, IndexMetaDataKey::Unique));
    keys.append(IndexMetaDataKey::encode(1, 1, 30, IndexMetaDataKey::KeyPath));
    keys.append(IndexMetaDataKey::encode(1, 1, 30, IndexMetaDataKey::MultiEntry));
    keys.append(IndexMetaDataKey::encode(1, 1, 31, 0));
    keys.append(IndexMetaDataKey::encode(1, 1, 31, 1));
    keys.append(IndexMetaDataKey::encodeMaxKey(1, 1, 31));
    keys.append(IndexMetaDataKey::encodeMaxKey(1, 1, 32));
    keys.append(IndexMetaDataKey::encodeMaxKey(1, 1));
    keys.append(IndexMetaDataKey::encodeMaxKey(1, 2));
    keys.append(ObjectStoreFreeListKey::encode(1, 1));
    keys.append(ObjectStoreFreeListKey::encodeMaxKey(1));
    keys.append(IndexFreeListKey::encode(1, 1, MinimumIndexId));
    keys.append(IndexFreeListKey::encodeMaxKey(1, 1));
    keys.append(IndexFreeListKey::encode(1, 2, MinimumIndexId));
    keys.append(IndexFreeListKey::encodeMaxKey(1, 2));
    keys.append(ObjectStoreNamesKey::encode(1, ""));
    keys.append(ObjectStoreNamesKey::encode(1, "a"));
    keys.append(IndexNamesKey::encode(1, 1, ""));
    keys.append(IndexNamesKey::encode(1, 1, "a"));
    keys.append(IndexNamesKey::encode(1, 2, "a"));
    keys.append(ObjectStoreDataKey::encode(1, 1, minIDBKey()));
    keys.append(ObjectStoreDataKey::encode(1, 1, maxIDBKey()));
    keys.append(ExistsEntryKey::encode(1, 1, minIDBKey()));
    keys.append(ExistsEntryKey::encode(1, 1, maxIDBKey()));
    keys.append(IndexDataKey::encode(1, 1, 30, minIDBKey(), minIDBKey(), 0));
    keys.append(IndexDataKey::encode(1, 1, 30, minIDBKey(), minIDBKey(), 1));
    keys.append(IndexDataKey::encode(1, 1, 30, minIDBKey(), maxIDBKey(), 0));
    keys.append(IndexDataKey::encode(1, 1, 30, minIDBKey(), maxIDBKey(), 1));
    keys.append(IndexDataKey::encode(1, 1, 30, maxIDBKey(), minIDBKey(), 0));
    keys.append(IndexDataKey::encode(1, 1, 30, maxIDBKey(), minIDBKey(), 1));
    keys.append(IndexDataKey::encode(1, 1, 30, maxIDBKey(), maxIDBKey(), 0));
    keys.append(IndexDataKey::encode(1, 1, 30, maxIDBKey(), maxIDBKey(), 1));
    keys.append(IndexDataKey::encode(1, 1, 31, minIDBKey(), minIDBKey(), 0));
    keys.append(IndexDataKey::encode(1, 2, 30, minIDBKey(), minIDBKey(), 0));
    keys.append(IndexDataKey::encodeMaxKey(1, 2, INT32_MAX));

    for (size_t i = 0; i < keys.size(); ++i) {
        const LevelDBSlice keyA(keys[i]);
        EXPECT_EQ(compare(keyA, keyA), 0);

        for (size_t j = i + 1; j < keys.size(); ++j) {
            const LevelDBSlice keyB(keys[j]);
            EXPECT_LT(compare(keyA, keyB), 0);
            EXPECT_GT(compare(keyB, keyA), 0);
        }
    }
}

TEST(IDBLevelDBCodingTest, EncodeVarIntVSEncodeByteTest)
{
    Vector<unsigned char> testCases;
    testCases.append(0);
    testCases.append(1);
    testCases.append(127);

    for (size_t i = 0; i < testCases.size(); ++i) {
        unsigned char n = testCases[i];

        Vector<char> vA = encodeByte(n);
        Vector<char> vB = encodeVarInt(static_cast<int64_t>(n));

        EXPECT_EQ(vA.size(), vB.size());
        EXPECT_EQ(*(vA.data()), *(vB.data()));
    }
}

} // namespace

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
