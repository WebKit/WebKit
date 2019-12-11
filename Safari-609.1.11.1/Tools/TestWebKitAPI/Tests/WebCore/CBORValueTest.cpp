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

#include <WebCore/CBORValue.h>
#include <utility>

namespace TestWebKitAPI {

using namespace cbor;

// Test constructors
TEST(CBORValueTest, ConstructUnsigned)
{
    CBORValue value(37);
    ASSERT_TRUE(CBORValue::Type::Unsigned == value.type());
    EXPECT_EQ(37u, value.getInteger());
}

TEST(CBORValueTest, ConstructNegative)
{
    CBORValue value(-1);
    ASSERT_TRUE(CBORValue::Type::Negative == value.type());
    EXPECT_EQ(-1, value.getInteger());
}

TEST(CBORValueTest, ConstructStringFromConstCharPtr)
{
    const char* str = "foobar";
    CBORValue value(str);
    ASSERT_TRUE(CBORValue::Type::String == value.type());
    EXPECT_TRUE("foobar" == value.getString());
}

TEST(CBORValueTest, ConstructStringFromWTFStringConstRef)
{
    String str = "foobar";
    CBORValue value(str);
    ASSERT_TRUE(CBORValue::Type::String == value.type());
    EXPECT_TRUE("foobar" == value.getString());
}

TEST(CBORValueTest, ConstructStringFromWTFStringRefRef)
{
    String str = "foobar";
    CBORValue value(WTFMove(str));
    ASSERT_TRUE(CBORValue::Type::String == value.type());
    EXPECT_TRUE("foobar" == value.getString());
}

TEST(CBORValueTest, ConstructBytestring)
{
    CBORValue value(CBORValue::BinaryValue({ 0xF, 0x0, 0x0, 0xB, 0xA, 0x2 }));
    ASSERT_TRUE(CBORValue::Type::ByteString == value.type());
    EXPECT_TRUE(CBORValue::BinaryValue({ 0xF, 0x0, 0x0, 0xB, 0xA, 0x2 }) == value.getByteString());
}

TEST(CBORValueTest, ConstructArray)
{
    CBORValue::ArrayValue array;
    array.append(CBORValue("foo"));
    {
        CBORValue value(array);
        ASSERT_TRUE(CBORValue::Type::Array == value.type());
        ASSERT_EQ(1u, value.getArray().size());
        ASSERT_TRUE(CBORValue::Type::String == value.getArray()[0].type());
        EXPECT_TRUE("foo" == value.getArray()[0].getString());
    }

    array.last() = CBORValue("bar");
    {
        CBORValue value(WTFMove(array));
        ASSERT_TRUE(CBORValue::Type::Array == value.type());
        ASSERT_EQ(1u, value.getArray().size());
        ASSERT_TRUE(CBORValue::Type::String == value.getArray()[0].type());
        EXPECT_TRUE("bar" == value.getArray()[0].getString());
    }
}

TEST(CBORValueTest, ConstructMap)
{
    CBORValue::MapValue map;
    const CBORValue keyFoo("foo");
    map[CBORValue("foo")] = CBORValue("bar");
    {
        CBORValue value(map);
        ASSERT_TRUE(CBORValue::Type::Map == value.type());
        ASSERT_EQ(value.getMap().count(keyFoo), 1u);
        ASSERT_TRUE(CBORValue::Type::String == value.getMap().find(keyFoo)->second.type());
        EXPECT_TRUE("bar" == value.getMap().find(keyFoo)->second.getString());
    }

    map[CBORValue("foo")] = CBORValue("baz");
    {
        CBORValue value(WTFMove(map));
        ASSERT_TRUE(CBORValue::Type::Map == value.type());
        ASSERT_EQ(value.getMap().count(keyFoo), 1u);
        ASSERT_TRUE(CBORValue::Type::String == value.getMap().find(keyFoo)->second.type());
        EXPECT_TRUE("baz" == value.getMap().find(keyFoo)->second.getString());
    }
}

TEST(CBORValueTest, ConstructSimpleValue)
{
    CBORValue falseValue(CBORValue::SimpleValue::FalseValue);
    ASSERT_TRUE(CBORValue::Type::SimpleValue == falseValue.type());
    EXPECT_TRUE(CBORValue::SimpleValue::FalseValue == falseValue.getSimpleValue());

    CBORValue trueValue(CBORValue::SimpleValue::TrueValue);
    ASSERT_TRUE(CBORValue::Type::SimpleValue == trueValue.type());
    EXPECT_TRUE(CBORValue::SimpleValue::TrueValue == trueValue.getSimpleValue());

    CBORValue nullValue(CBORValue::SimpleValue::NullValue);
    ASSERT_TRUE(CBORValue::Type::SimpleValue == nullValue.type());
    EXPECT_TRUE(CBORValue::SimpleValue::NullValue == nullValue.getSimpleValue());

    CBORValue undefinedValue(CBORValue::SimpleValue::Undefined);
    ASSERT_TRUE(CBORValue::Type::SimpleValue == undefinedValue.type());
    EXPECT_TRUE(CBORValue::SimpleValue::Undefined == undefinedValue.getSimpleValue());
}

TEST(CBORValueTest, ConstructSimpleBooleanValue)
{
    CBORValue trueValue(true);
    ASSERT_EQ(CBORValue::Type::SimpleValue, trueValue.type());
    EXPECT_TRUE(trueValue.getBool());

    CBORValue falseValue(false);
    ASSERT_EQ(CBORValue::Type::SimpleValue, falseValue.type());
    EXPECT_FALSE(falseValue.getBool());
}

// Test copy constructors
TEST(CBORValueTest, CopyUnsigned)
{
    CBORValue value(74);
    CBORValue copiedValue(value.clone());
    ASSERT_TRUE(value.type() == copiedValue.type());
    EXPECT_EQ(value.getInteger(), copiedValue.getInteger());

    CBORValue blank;

    blank = value.clone();
    ASSERT_TRUE(value.type() == blank.type());
    EXPECT_EQ(value.getInteger(), blank.getInteger());
}

TEST(CBORValueTest, CopyNegativeInt)
{
    CBORValue value(-74);
    CBORValue copiedValue(value.clone());
    ASSERT_TRUE(value.type() == copiedValue.type());
    EXPECT_EQ(value.getInteger(), copiedValue.getInteger());

    CBORValue blank;

    blank = value.clone();
    ASSERT_TRUE(value.type() == blank.type());
    EXPECT_EQ(value.getInteger(), blank.getInteger());
}

TEST(CBORValueTest, CopyString)
{
    CBORValue value("foobar");
    CBORValue copiedValue(value.clone());
    ASSERT_TRUE(value.type() == copiedValue.type());
    EXPECT_TRUE(value.getString() == copiedValue.getString());

    CBORValue blank;

    blank = value.clone();
    ASSERT_TRUE(value.type() == blank.type());
    EXPECT_TRUE(value.getString() == blank.getString());
}

TEST(CBORValueTest, CopyBytestring)
{
    CBORValue value(CBORValue::BinaryValue({ 0xF, 0x0, 0x0, 0xB, 0xA, 0x2 }));
    CBORValue copiedValue(value.clone());
    ASSERT_TRUE(value.type() == copiedValue.type());
    EXPECT_TRUE(value.getByteString() == copiedValue.getByteString());

    CBORValue blank;

    blank = value.clone();
    ASSERT_TRUE(value.type() == blank.type());
    EXPECT_TRUE(value.getByteString() == blank.getByteString());
}

TEST(CBORValueTest, CopyArray)
{
    CBORValue::ArrayValue array;
    array.append(123);
    CBORValue value(WTFMove(array));

    CBORValue copiedValue(value.clone());
    ASSERT_EQ(1u, copiedValue.getArray().size());
    ASSERT_TRUE(copiedValue.getArray()[0].isUnsigned());
    EXPECT_EQ(value.getArray()[0].getInteger(), copiedValue.getArray()[0].getInteger());

    CBORValue blank;
    blank = value.clone();
    EXPECT_EQ(1u, blank.getArray().size());
}

TEST(CBORValueTest, CopyMap)
{
    CBORValue::MapValue map;
    CBORValue keyA("a");
    map[CBORValue("a")] = CBORValue(123);
    CBORValue value(WTFMove(map));

    CBORValue copiedValue(value.clone());
    EXPECT_EQ(1u, copiedValue.getMap().size());
    ASSERT_EQ(value.getMap().count(keyA), 1u);
    ASSERT_EQ(copiedValue.getMap().count(keyA), 1u);
    ASSERT_TRUE(copiedValue.getMap().find(keyA)->second.isUnsigned());
    EXPECT_EQ(value.getMap().find(keyA)->second.getInteger(), copiedValue.getMap().find(keyA)->second.getInteger());

    CBORValue blank;
    blank = value.clone();
    EXPECT_EQ(1u, blank.getMap().size());
    ASSERT_EQ(blank.getMap().count(keyA), 1u);
    ASSERT_TRUE(blank.getMap().find(keyA)->second.isUnsigned());
    EXPECT_EQ(value.getMap().find(keyA)->second.getInteger(), blank.getMap().find(keyA)->second.getInteger());
}

TEST(CBORValueTest, CopySimpleValue)
{
    CBORValue value(CBORValue::SimpleValue::TrueValue);
    CBORValue copiedValue(value.clone());
    EXPECT_TRUE(value.type() == copiedValue.type());
    EXPECT_TRUE(value.getSimpleValue() == copiedValue.getSimpleValue());

    CBORValue blank;

    blank = value.clone();
    EXPECT_TRUE(value.type() == blank.type());
    EXPECT_TRUE(value.getSimpleValue() == blank.getSimpleValue());
}

// Test move constructors and move-assignment
TEST(CBORValueTest, MoveUnsigned)
{
    CBORValue value(74);
    CBORValue moved_value(WTFMove(value));
    EXPECT_TRUE(CBORValue::Type::Unsigned == moved_value.type());
    EXPECT_EQ(74u, moved_value.getInteger());

    CBORValue blank;

    blank = CBORValue(654);
    EXPECT_TRUE(CBORValue::Type::Unsigned == blank.type());
    EXPECT_EQ(654u, blank.getInteger());
}

TEST(CBORValueTest, MoveNegativeInteger)
{
    CBORValue value(-74);
    CBORValue moved_value(WTFMove(value));
    EXPECT_TRUE(CBORValue::Type::Negative == moved_value.type());
    EXPECT_EQ(-74, moved_value.getInteger());

    CBORValue blank;

    blank = CBORValue(-654);
    EXPECT_TRUE(CBORValue::Type::Negative == blank.type());
    EXPECT_EQ(-654, blank.getInteger());
}

TEST(CBORValueTest, MoveString)
{
    CBORValue value("foobar");
    CBORValue moved_value(WTFMove(value));
    EXPECT_TRUE(CBORValue::Type::String == moved_value.type());
    EXPECT_TRUE("foobar" == moved_value.getString());

    CBORValue blank;

    blank = CBORValue("foobar");
    EXPECT_TRUE(CBORValue::Type::String == blank.type());
    EXPECT_TRUE("foobar" == blank.getString());
}

TEST(CBORValueTest, MoveBytestring)
{
    const CBORValue::BinaryValue bytes({ 0xF, 0x0, 0x0, 0xB, 0xA, 0x2 });
    CBORValue value(bytes);
    CBORValue moved_value(WTFMove(value));
    EXPECT_TRUE(CBORValue::Type::ByteString == moved_value.type());
    EXPECT_TRUE(bytes == moved_value.getByteString());

    CBORValue blank;

    blank = CBORValue(bytes);
    EXPECT_TRUE(CBORValue::Type::ByteString == blank.type());
    EXPECT_TRUE(bytes == blank.getByteString());
}

TEST(CBORValueTest, MoveConstructMap)
{
    CBORValue::MapValue map;
    const CBORValue keyA("a");
    map[CBORValue("a")] = CBORValue(123);

    CBORValue value(WTFMove(map));
    CBORValue moved_value(WTFMove(value));
    ASSERT_TRUE(CBORValue::Type::Map == moved_value.type());
    ASSERT_EQ(moved_value.getMap().count(keyA), 1u);
    ASSERT_TRUE(moved_value.getMap().find(keyA)->second.isUnsigned());
    EXPECT_EQ(123u, moved_value.getMap().find(keyA)->second.getInteger());
}

TEST(CBORValueTest, MoveAssignMap)
{
    CBORValue::MapValue map;
    const CBORValue keyA("a");
    map[CBORValue("a")] = CBORValue(123);

    CBORValue blank;
    blank = CBORValue(WTFMove(map));
    ASSERT_TRUE(blank.isMap());
    ASSERT_EQ(blank.getMap().count(keyA), 1u);
    ASSERT_TRUE(blank.getMap().find(keyA)->second.isUnsigned());
    EXPECT_EQ(123u, blank.getMap().find(keyA)->second.getInteger());
}

TEST(CBORValueTest, MoveArray)
{
    CBORValue::ArrayValue array;
    array.append(123);
    CBORValue value(array);
    CBORValue moved_value(WTFMove(value));
    EXPECT_TRUE(CBORValue::Type::Array == moved_value.type());
    EXPECT_EQ(123u, moved_value.getArray().last().getInteger());

    CBORValue blank;
    blank = CBORValue(WTFMove(array));
    EXPECT_TRUE(CBORValue::Type::Array == blank.type());
    EXPECT_EQ(123u, blank.getArray().last().getInteger());
}

TEST(CBORValueTest, MoveSimpleValue)
{
    CBORValue value(CBORValue::SimpleValue::Undefined);
    CBORValue moved_value(WTFMove(value));
    EXPECT_TRUE(CBORValue::Type::SimpleValue == moved_value.type());
    EXPECT_TRUE(CBORValue::SimpleValue::Undefined == moved_value.getSimpleValue());

    CBORValue blank;

    blank = CBORValue(CBORValue::SimpleValue::Undefined);
    EXPECT_TRUE(CBORValue::Type::SimpleValue == blank.type());
    EXPECT_TRUE(CBORValue::SimpleValue::Undefined == blank.getSimpleValue());
}

TEST(CBORValueTest, SelfSwap)
{
    CBORValue test(1);
    std::swap(test, test);
    EXPECT_EQ(test.getInteger(), 1u);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
