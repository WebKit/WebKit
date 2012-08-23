/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "IDBKeyPath.h"

#include "IDBBindingUtilities.h"
#include "IDBKey.h"
#include "SerializedScriptValue.h"

#include <gtest/gtest.h>
#include <wtf/Vector.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

void checkKeyPath(const String& keyPath, const Vector<String>& expected, int parserError)
{
    IDBKeyPath idbKeyPath(keyPath);
    ASSERT_EQ(idbKeyPath.type(), IDBKeyPath::StringType);
    ASSERT_EQ(idbKeyPath.isValid(), (parserError == IDBKeyPathParseErrorNone));

    IDBKeyPathParseError error;
    Vector<String> keyPathElements;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT_EQ(parserError, error);
    if (error != IDBKeyPathParseErrorNone)
        return;
    ASSERT_EQ(expected.size(), keyPathElements.size());
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_TRUE(expected[i] == keyPathElements[i]) << i;
}

TEST(IDBKeyPathTest, ValidKeyPath0)
{
    Vector<String> expected;
    String keyPath("");
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath1)
{
    Vector<String> expected;
    String keyPath("foo");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, ValidKeyPath2)
{
    Vector<String> expected;
    String keyPath("foo.bar.baz");
    expected.append(String("foo"));
    expected.append(String("bar"));
    expected.append(String("baz"));
    checkKeyPath(keyPath, expected, 0);
}

TEST(IDBKeyPathTest, InvalidKeyPath0)
{
    Vector<String> expected;
    String keyPath(" ");
    checkKeyPath(keyPath, expected, 1);
}

TEST(IDBKeyPathTest, InvalidKeyPath1)
{
    Vector<String> expected;
    String keyPath("+foo.bar.baz");
    checkKeyPath(keyPath, expected, 1);
}

TEST(IDBKeyPathTest, InvalidKeyPath2)
{
    Vector<String> expected;
    String keyPath("foo bar baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 2);
}

TEST(IDBKeyPathTest, InvalidKeyPath3)
{
    Vector<String> expected;
    String keyPath("foo .bar .baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 2);
}

TEST(IDBKeyPathTest, InvalidKeyPath4)
{
    Vector<String> expected;
    String keyPath("foo. bar. baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, InvalidKeyPath5)
{
    Vector<String> expected;
    String keyPath("foo..bar..baz");
    expected.append(String("foo"));
    checkKeyPath(keyPath, expected, 3);
}

TEST(IDBKeyPathTest, Extract)
{
    IDBKeyPath keyPath("foo");
    RefPtr<IDBKey> stringZooKey(IDBKey::createString("zoo"));
    RefPtr<IDBKey> invalidKey(IDBKey::createInvalid());
    RefPtr<SerializedScriptValue> ssv;
    RefPtr<IDBKey> result;

    // keypath: "foo", value: {foo: "zoo"}, expected: "zoo"
    UChar dataFooZoo[] = {0x0353, 0x6f66, 0x536f, 0x7a03, 0x6f6f, 0x017b};
    ssv = SerializedScriptValue::createFromWire(String(dataFooZoo, WTF_ARRAY_LENGTH(dataFooZoo)));
    result = createIDBKeyFromSerializedValueAndKeyPath(ssv, keyPath);
    EXPECT_TRUE(stringZooKey->isEqual(result.get()));

    // keypath: "foo", value: {foo: null}, expected: invalid
    UChar dataFooNull[] = {0x0353, 0x6f66, 0x306f, 0x017b};
    ssv = SerializedScriptValue::createFromWire(String(dataFooNull, WTF_ARRAY_LENGTH(dataFooNull)));
    result = createIDBKeyFromSerializedValueAndKeyPath(ssv, keyPath);
    EXPECT_FALSE(result->isValid());

    // keypath: "foo", value: {}, expected: null
    UChar dataObject[] = {0x017b};
    ssv = SerializedScriptValue::createFromWire(String(dataObject, WTF_ARRAY_LENGTH(dataObject)));
    result = createIDBKeyFromSerializedValueAndKeyPath(ssv, keyPath);
    EXPECT_EQ(0, result.get());

    // keypath: "foo", value: null, expected: null
    ssv = SerializedScriptValue::nullValue();
    result = createIDBKeyFromSerializedValueAndKeyPath(ssv, keyPath);
    EXPECT_EQ(0, result.get());
}

TEST(IDBKeyPathTest, IDBKeyPathPropertyNotAvailable)
{
    IDBKeyPath keyPath("PropertyNotAvailable");
    RefPtr<SerializedScriptValue> ssv;
    // {foo: "zoo", bar: null}
    UChar data[] = {0x0353, 0x6f66, 0x536f, 0x7a03, 0x6f6f, 0x0353, 0x6162,
                    0x3072, 0x027b};
    ssv = SerializedScriptValue::createFromWire(String(data, WTF_ARRAY_LENGTH(data)));
    RefPtr<IDBKey> result = createIDBKeyFromSerializedValueAndKeyPath(ssv, keyPath);
    EXPECT_EQ(0, result.get());

    // null
    ssv = SerializedScriptValue::nullValue();
    result = createIDBKeyFromSerializedValueAndKeyPath(ssv, keyPath);
    EXPECT_EQ(0, result.get());
}

TEST(IDBKeyPathTest, InjectIDBKey)
{
    // {foo: 'zoo'}
    const UChar initialData[] = {0x0353, 0x6f66, 0x536f, 0x7a03, 0x6f6f, 0x017b};
    RefPtr<SerializedScriptValue> value = SerializedScriptValue::createFromWire(String(initialData, WTF_ARRAY_LENGTH(initialData)));

    RefPtr<IDBKey> key = IDBKey::createString("myNewKey");
    IDBKeyPath keyPath("bar");

    // {foo: 'zoo', bar: 'myNewKey'}
    const UChar expectedData[] = {0x01ff, 0x003f, 0x3f6f, 0x5301, 0x6603,
                                  0x6f6f, 0x013f, 0x0353, 0x6f7a, 0x3f6f,
                                  0x5301, 0x6203, 0x7261, 0x013f, 0x0853,
                                  0x796d, 0x654e, 0x4b77, 0x7965, 0x027b};
    RefPtr<SerializedScriptValue> expectedValue = 
            SerializedScriptValue::createFromWire(String(expectedData, WTF_ARRAY_LENGTH(expectedData)));
    RefPtr<SerializedScriptValue> result = injectIDBKeyIntoSerializedValue(key, value, keyPath);
    EXPECT_EQ(expectedValue->toWireString(), result->toWireString());

    // Should fail - can't apply properties to string value of key foo
    keyPath = IDBKeyPath("foo.bad.path");
    result = injectIDBKeyIntoSerializedValue(key, value, keyPath);
    EXPECT_EQ(0, result.get());

    // {foo: 'zoo', bar: {baz: 'myNewKey'}}
    const UChar expectedData2[] = {0x01ff, 0x003f, 0x3f6f, 0x5301, 0x6603,
                                   0x6f6f, 0x013f, 0x0353, 0x6f7a, 0x3f6f,
                                   0x5301, 0x6203, 0x7261, 0x013f, 0x3f6f,
                                   0x5302, 0x6203, 0x7a61, 0x023f, 0x0853,
                                   0x796d, 0x654e, 0x4b77, 0x7965, 0x017b,
                                   0x027b};
    RefPtr<SerializedScriptValue> expectedValue2 = SerializedScriptValue::createFromWire(String(expectedData2, WTF_ARRAY_LENGTH(expectedData2)));
    keyPath = IDBKeyPath("bar.baz");
    result = injectIDBKeyIntoSerializedValue(key, value, keyPath);
    EXPECT_EQ(expectedValue2->toWireString(), result->toWireString());
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
