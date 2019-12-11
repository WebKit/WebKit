/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "Test.h"
#include "WTFStringUtilities.h"
#include <wtf/JSONValues.h>

namespace TestWebKitAPI {

TEST(JSONValue, Construct)
{
    {
        Ref<JSON::Value> value = JSON::Value::null();
        EXPECT_TRUE(value->type() == JSON::Value::Type::Null);
        EXPECT_TRUE(value->isNull());
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(true);
        EXPECT_TRUE(value->type() == JSON::Value::Type::Boolean);
        bool booleanValue;
        EXPECT_TRUE(value->asBoolean(booleanValue));
        EXPECT_EQ(booleanValue, true);

        value = JSON::Value::create(false);
        EXPECT_TRUE(value->type() == JSON::Value::Type::Boolean);
        EXPECT_TRUE(value->asBoolean(booleanValue));
        EXPECT_EQ(booleanValue, false);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(1);
        EXPECT_TRUE(value->type() == JSON::Value::Type::Integer);
        int integerValue;
        EXPECT_TRUE(value->asInteger(integerValue));
        EXPECT_EQ(integerValue, 1);

        // Doubles can be get as integers, but not the other way around.
        float floatValue;
        EXPECT_FALSE(value->asDouble(floatValue));

        value = JSON::Value::create(std::numeric_limits<int>::max());
        EXPECT_TRUE(value->asInteger(integerValue));
        EXPECT_EQ(integerValue, std::numeric_limits<int>::max());
        unsigned unsignedValue;
        EXPECT_TRUE(value->asInteger(unsignedValue));
        long longValue;
        EXPECT_TRUE(value->asInteger(longValue));
        long long longLongValue;
        EXPECT_TRUE(value->asInteger(longLongValue));
        unsigned long unsignedLongValue;
        EXPECT_TRUE(value->asInteger(unsignedLongValue));
        unsigned long long unsignedLongLongValue;
        EXPECT_TRUE(value->asInteger(unsignedLongLongValue));
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(1.5);
        EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
        double doubleValue;
        EXPECT_TRUE(value->asDouble(doubleValue));
        EXPECT_EQ(doubleValue, 1.5);

        float floatValue;
        EXPECT_TRUE(value->asDouble(floatValue));

        int integerValue;
        EXPECT_TRUE(value->asInteger(integerValue));
        EXPECT_EQ(integerValue, static_cast<int>(1.5));

        unsigned unsignedValue;
        EXPECT_TRUE(value->asInteger(unsignedValue));
        long longValue;
        EXPECT_TRUE(value->asInteger(longValue));
        long long longLongValue;
        EXPECT_TRUE(value->asInteger(longLongValue));
        unsigned long unsignedLongValue;
        EXPECT_TRUE(value->asInteger(unsignedLongValue));
        unsigned long long unsignedLongLongValue;
        EXPECT_TRUE(value->asInteger(unsignedLongLongValue));
    }

    {
        Ref<JSON::Value> value = JSON::Value::create("webkit");
        EXPECT_TRUE(value->type() == JSON::Value::Type::String);
        String stringValue;
        EXPECT_TRUE(value->asString(stringValue));
        EXPECT_EQ(stringValue, "webkit");

        String nullString;
        value = JSON::Value::create(nullString);
        EXPECT_TRUE(value->asString(stringValue));
        EXPECT_TRUE(stringValue.isNull());

        value = JSON::Value::create(emptyString());
        EXPECT_TRUE(value->asString(stringValue));
        EXPECT_FALSE(stringValue.isNull());
        EXPECT_TRUE(stringValue.isEmpty());
    }

    {
        Ref<JSON::Array> array = JSON::Array::create();
        EXPECT_TRUE(array->type() == JSON::Value::Type::Array);
        EXPECT_EQ(array->length(), 0U);
    }

    {
        Ref<JSON::Object> object = JSON::Object::create();
        EXPECT_TRUE(object->type() == JSON::Value::Type::Object);
        EXPECT_EQ(object->size(), 0);
    }

    {
        auto array = JSON::ArrayOf<String>::create();
        EXPECT_TRUE(array->type() == JSON::Value::Type::Array);
        EXPECT_EQ(array->length(), 0U);
    }
}

TEST(JSONArray, Basic)
{
    Ref<JSON::Array> array = JSON::Array::create();

    array->pushValue(JSON::Value::null());
    EXPECT_EQ(array->length(), 1U);
    auto value = array->get(0);
    EXPECT_TRUE(value->isNull());

    array->pushBoolean(true);
    EXPECT_EQ(array->length(), 2U);
    value = array->get(1);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Boolean);
    bool booleanValue;
    EXPECT_TRUE(value->asBoolean(booleanValue));
    EXPECT_EQ(booleanValue, true);

    array->pushInteger(1);
    EXPECT_EQ(array->length(), 3U);
    value = array->get(2);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Integer);
    int integerValue;
    EXPECT_TRUE(value->asInteger(integerValue));
    EXPECT_EQ(integerValue, 1);

    array->pushDouble(1.5);
    EXPECT_EQ(array->length(), 4U);
    value = array->get(3);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
    double doubleValue;
    EXPECT_TRUE(value->asDouble(doubleValue));
    EXPECT_EQ(doubleValue, 1.5);

    array->pushString("webkit");
    EXPECT_EQ(array->length(), 5U);
    value = array->get(4);
    EXPECT_TRUE(value->type() == JSON::Value::Type::String);
    String stringValue;
    EXPECT_TRUE(value->asString(stringValue));
    EXPECT_EQ(stringValue, "webkit");

    array->pushObject(JSON::Object::create());
    EXPECT_EQ(array->length(), 6U);
    value = array->get(5);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Object);
    RefPtr<JSON::Object> objectValue;
    EXPECT_TRUE(value->asObject(objectValue));
    EXPECT_EQ(objectValue->size(), 0);

    array->pushArray(JSON::Array::create());
    EXPECT_EQ(array->length(), 7U);
    value = array->get(6);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Array);
    RefPtr<JSON::Array> arrayValue;
    EXPECT_TRUE(value->asArray(arrayValue));
    EXPECT_EQ(arrayValue->length(), 0U);

    auto it = array->begin();
    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Null);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Boolean);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Integer);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::String);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Object);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Array);
    ++it;
    EXPECT_TRUE(it == array->end());
}

TEST(JSONArrayOf, Basic)
{
    Ref<JSON::ArrayOf<JSON::Value>> array = JSON::ArrayOf<JSON::Value>::create();

    array->addItem(JSON::Value::null());
    EXPECT_EQ(array->length(), 1U);
    auto value = array->get(0);
    EXPECT_TRUE(value->isNull());

    // Note: addItem(bool) is not implemented because it would be ambiguous
    // with addItem(RefPtr<T>), and the latter is much more common to do.

    array->addItem(1);
    EXPECT_EQ(array->length(), 2U);
    value = array->get(1);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Integer);
    int integerValue;
    EXPECT_TRUE(value->asInteger(integerValue));
    EXPECT_EQ(integerValue, 1);

    array->addItem(1.5);
    EXPECT_EQ(array->length(), 3U);
    value = array->get(2);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
    double doubleValue;
    EXPECT_TRUE(value->asDouble(doubleValue));
    EXPECT_EQ(doubleValue, 1.5);

    array->addItem("webkit");
    EXPECT_EQ(array->length(), 4U);
    value = array->get(3);
    EXPECT_TRUE(value->type() == JSON::Value::Type::String);
    String stringValue;
    EXPECT_TRUE(value->asString(stringValue));
    EXPECT_EQ(stringValue, "webkit");

    array->addItem(JSON::Object::create());
    EXPECT_EQ(array->length(), 5U);
    value = array->get(4);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Object);
    RefPtr<JSON::Object> objectValue;
    EXPECT_TRUE(value->asObject(objectValue));
    EXPECT_EQ(objectValue->size(), 0);

    array->addItem(JSON::Array::create());
    EXPECT_EQ(array->length(), 6U);
    value = array->get(5);
    EXPECT_TRUE(value->type() == JSON::Value::Type::Array);
    RefPtr<JSON::Array> arrayValue;
    EXPECT_TRUE(value->asArray(arrayValue));
    EXPECT_EQ(arrayValue->length(), 0U);

    auto it = array->begin();
    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Null);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Integer);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::String);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Object);
    ++it;
    EXPECT_FALSE(it == array->end());

    value = it->get();
    EXPECT_TRUE(value->type() == JSON::Value::Type::Array);
    ++it;
    EXPECT_TRUE(it == array->end());
}

TEST(JSONObject, Basic)
{
    Ref<JSON::Object> object = JSON::Object::create();

    object->setValue("null", JSON::Value::null());
    EXPECT_EQ(object->size(), 1);
    RefPtr<JSON::Value> value;
    EXPECT_TRUE(object->getValue("null", value));
    EXPECT_TRUE(value->isNull());

    object->setBoolean("boolean", true);
    EXPECT_EQ(object->size(), 2);
    bool booleanValue;
    EXPECT_TRUE(object->getBoolean("boolean", booleanValue));
    EXPECT_EQ(booleanValue, true);

    object->setInteger("integer", 1);
    EXPECT_EQ(object->size(), 3);
    int integerValue;
    EXPECT_TRUE(object->getInteger("integer", integerValue));
    EXPECT_EQ(integerValue, 1);

    object->setDouble("double", 1.5);
    EXPECT_EQ(object->size(), 4);
    double doubleValue;
    EXPECT_TRUE(object->getDouble("double", doubleValue));
    EXPECT_EQ(doubleValue, 1.5);

    object->setString("string", "webkit");
    EXPECT_EQ(object->size(), 5);
    String stringValue;
    EXPECT_TRUE(object->getString("string", stringValue));
    EXPECT_EQ(stringValue, "webkit");

    object->setObject("object", JSON::Object::create());
    EXPECT_EQ(object->size(), 6);
    RefPtr<JSON::Object> objectValue;
    EXPECT_TRUE(object->getObject("object", objectValue));
    EXPECT_EQ(objectValue->size(), 0);

    object->setArray("array", JSON::Array::create());
    EXPECT_EQ(object->size(), 7);
    RefPtr<JSON::Array> arrayValue;
    EXPECT_TRUE(object->getArray("array", arrayValue));
    EXPECT_EQ(arrayValue->length(), 0U);

    Vector<const char*> keys = { "null", "boolean", "integer", "double", "string", "object", "array" };
    auto end = object->end();
    for (auto it = object->begin(); it != end; ++it) {
        auto position = keys.find(it->key);
        EXPECT_NE(position, notFound);
        EXPECT_TRUE(it == object->find(keys[position]));
        keys.remove(position);
    }
    EXPECT_TRUE(keys.isEmpty());

    object->remove("null");
    EXPECT_EQ(object->size(), 6);
    EXPECT_TRUE(object->find("null") == object->end());

    object->remove("boolean");
    EXPECT_EQ(object->size(), 5);
    EXPECT_TRUE(object->find("boolean") == object->end());

    object->remove("integer");
    EXPECT_EQ(object->size(), 4);
    EXPECT_TRUE(object->find("integer") == object->end());

    object->remove("double");
    EXPECT_EQ(object->size(), 3);
    EXPECT_TRUE(object->find("double") == object->end());

    object->remove("string");
    EXPECT_EQ(object->size(), 2);
    EXPECT_TRUE(object->find("string") == object->end());

    object->remove("object");
    EXPECT_EQ(object->size(), 1);
    EXPECT_TRUE(object->find("object") == object->end());

    object->remove("array");
    EXPECT_EQ(object->size(), 0);
    EXPECT_TRUE(object->find("array") == object->end());
}

TEST(JSONValue, ToJSONString)
{
    {
        Ref<JSON::Value> value = JSON::Value::null();
        EXPECT_EQ(value->toJSONString(), "null");
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(true);
        EXPECT_EQ(value->toJSONString(), "true");

        value = JSON::Value::create(false);
        EXPECT_EQ(value->toJSONString(), "false");
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(1);
        EXPECT_EQ(value->toJSONString(), "1");
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(1.5);
        EXPECT_EQ(value->toJSONString(), "1.5");
    }

    {
        Ref<JSON::Value> value = JSON::Value::create("webkit");
        EXPECT_EQ(value->toJSONString(), "\"webkit\"");
    }

    {
        Ref<JSON::Array> array = JSON::Array::create();
        EXPECT_EQ(array->toJSONString(), "[]");
        array->pushValue(JSON::Value::null());
        EXPECT_EQ(array->toJSONString(), "[null]");
        array->pushBoolean(true);
        array->pushBoolean(false);
        EXPECT_EQ(array->toJSONString(), "[null,true,false]");
        array->pushInteger(1);
        array->pushDouble(1.5);
        EXPECT_EQ(array->toJSONString(), "[null,true,false,1,1.5]");
        array->pushString("webkit");
        EXPECT_EQ(array->toJSONString(), "[null,true,false,1,1.5,\"webkit\"]");
        Ref<JSON::Array> subArray = JSON::Array::create();
        subArray->pushString("string");
        subArray->pushObject(JSON::Object::create());
        EXPECT_EQ(subArray->toJSONString(), "[\"string\",{}]");
        array->pushArray(WTFMove(subArray));
        EXPECT_EQ(array->toJSONString(), "[null,true,false,1,1.5,\"webkit\",[\"string\",{}]]");
    }

    {
        Ref<JSON::Object> object = JSON::Object::create();
        EXPECT_EQ(object->toJSONString(), "{}");
        object->setValue("null", JSON::Value::null());
        EXPECT_EQ(object->toJSONString(), "{\"null\":null}");
        object->setBoolean("boolean", true);
        EXPECT_EQ(object->toJSONString(), "{\"null\":null,\"boolean\":true}");
        object->setDouble("double", 1.5);
        EXPECT_EQ(object->toJSONString(), "{\"null\":null,\"boolean\":true,\"double\":1.5}");
        object->setString("string", "webkit");
        EXPECT_EQ(object->toJSONString(), "{\"null\":null,\"boolean\":true,\"double\":1.5,\"string\":\"webkit\"}");
        object->setArray("array", JSON::Array::create());
        EXPECT_EQ(object->toJSONString(), "{\"null\":null,\"boolean\":true,\"double\":1.5,\"string\":\"webkit\",\"array\":[]}");
        Ref<JSON::Object> subObject = JSON::Object::create();
        subObject->setString("foo", "bar");
        subObject->setInteger("baz", 25);
        object->setObject("object", WTFMove(subObject));
        EXPECT_EQ(object->toJSONString(), "{\"null\":null,\"boolean\":true,\"double\":1.5,\"string\":\"webkit\",\"array\":[],\"object\":{\"foo\":\"bar\",\"baz\":25}}");
    }
}

TEST(JSONValue, ParseJSON)
{
    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON("null", value));
        EXPECT_TRUE(value->isNull());
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON("true", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Boolean);
        bool booleanValue;
        EXPECT_TRUE(value->asBoolean(booleanValue));
        EXPECT_EQ(booleanValue, true);

        EXPECT_TRUE(JSON::Value::parseJSON("false", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Boolean);
        EXPECT_TRUE(value->asBoolean(booleanValue));
        EXPECT_EQ(booleanValue, false);
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON("1", value));
        // Numbers are always parsed as double.
        EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
        double doubleValue;
        EXPECT_TRUE(value->asDouble(doubleValue));
        EXPECT_EQ(doubleValue, 1.0);

        EXPECT_TRUE(JSON::Value::parseJSON("1.5", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Double);
        EXPECT_TRUE(value->asDouble(doubleValue));
        EXPECT_EQ(doubleValue, 1.5);
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON("\"string\"", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::String);
        String stringValue;
        EXPECT_TRUE(value->asString(stringValue));
        EXPECT_EQ(stringValue, "string");
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON("[]", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Array);
        RefPtr<JSON::Array> arrayValue;
        EXPECT_TRUE(value->asArray(arrayValue));
        EXPECT_EQ(arrayValue->length(), 0U);

        EXPECT_TRUE(JSON::Value::parseJSON("[null, 1 ,2.5,[{\"foo\":\"bar\"},{\"baz\":false}],\"webkit\"]", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Array);
        EXPECT_TRUE(value->asArray(arrayValue));
        EXPECT_EQ(arrayValue->length(), 5U);
        auto it = arrayValue->begin();
        RefPtr<JSON::Value> itemValue = it->get();
        EXPECT_TRUE(itemValue->type() == JSON::Value::Type::Null);
        ++it;
        EXPECT_FALSE(it == arrayValue->end());

        itemValue = it->get();
        EXPECT_TRUE(itemValue->type() == JSON::Value::Type::Double);
        int integerValue;
        EXPECT_TRUE(itemValue->asInteger(integerValue));
        EXPECT_EQ(integerValue, 1);
        ++it;
        EXPECT_FALSE(it == arrayValue->end());

        itemValue = it->get();
        EXPECT_TRUE(itemValue->type() == JSON::Value::Type::Double);
        double doubleValue;
        EXPECT_TRUE(itemValue->asDouble(doubleValue));
        EXPECT_EQ(doubleValue, 2.5);
        ++it;
        EXPECT_FALSE(it == arrayValue->end());

        itemValue = it->get();
        EXPECT_TRUE(itemValue->type() == JSON::Value::Type::Array);
        RefPtr<JSON::Array> subArrayValue;
        EXPECT_TRUE(itemValue->asArray(subArrayValue));
        EXPECT_EQ(subArrayValue->length(), 2U);
        RefPtr<JSON::Value> objectValue = subArrayValue->get(0);
        EXPECT_TRUE(objectValue->type() == JSON::Value::Type::Object);
        RefPtr<JSON::Object> object;
        EXPECT_TRUE(objectValue->asObject(object));
        EXPECT_EQ(object->size(), 1);
        String stringValue;
        EXPECT_TRUE(object->getString("foo", stringValue));
        EXPECT_EQ(stringValue, "bar");
        objectValue = subArrayValue->get(1);
        EXPECT_TRUE(objectValue->type() == JSON::Value::Type::Object);
        EXPECT_TRUE(objectValue->asObject(object));
        EXPECT_EQ(object->size(), 1);
        bool booleanValue;
        EXPECT_TRUE(object->getBoolean("baz", booleanValue));
        EXPECT_EQ(booleanValue, false);
        ++it;
        EXPECT_FALSE(it == arrayValue->end());

        itemValue = it->get();
        EXPECT_TRUE(itemValue->type() == JSON::Value::Type::String);
        EXPECT_TRUE(itemValue->asString(stringValue));
        EXPECT_EQ(stringValue, "webkit");
        ++it;
        EXPECT_TRUE(it == arrayValue->end());
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON("{}", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Object);
        RefPtr<JSON::Object> objectValue;
        EXPECT_TRUE(value->asObject(objectValue));
        EXPECT_EQ(objectValue->size(), 0);

        EXPECT_TRUE(JSON::Value::parseJSON("{\"foo\": \"bar\", \"baz\": {\"sub\":[null,false]}}", value));
        EXPECT_TRUE(value->type() == JSON::Value::Type::Object);
        EXPECT_TRUE(value->asObject(objectValue));
        EXPECT_EQ(objectValue->size(), 2);

        String stringValue;
        EXPECT_TRUE(objectValue->getString("foo", stringValue));
        EXPECT_EQ(stringValue, "bar");

        RefPtr<JSON::Object> object;
        EXPECT_TRUE(objectValue->getObject("baz", object));
        EXPECT_EQ(object->size(), 1);
        RefPtr<JSON::Array> array;
        EXPECT_TRUE(object->getArray("sub", array));
        EXPECT_EQ(array->length(), 2U);
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_FALSE(JSON::Value::parseJSON(",", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"foo", value));
        EXPECT_FALSE(JSON::Value::parseJSON("foo\"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("TrUe", value));
        EXPECT_FALSE(JSON::Value::parseJSON("False", value));
        EXPECT_FALSE(JSON::Value::parseJSON("1.d", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[1,]", value));
        EXPECT_FALSE(JSON::Value::parseJSON("1,2]", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[1,2", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[1 2]", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[1,2]]", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[1,2],", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{foo:\"bar\"}", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{\"foo\":bar}", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{\"foo:\"bar\"}", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{foo\":\"bar\"}", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{{\"foo\":\"bar\"}}", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{\"foo\":\"bar\"},", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[{\"foo\":\"bar\"},{\"baz\":false},]", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[{\"foo\":{\"baz\":false}]", value));
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON(" \"foo\" \n", value));
        String stringValue;
        EXPECT_TRUE(value->asString(stringValue));
        EXPECT_EQ("foo", stringValue);
    }

    {
        RefPtr<JSON::Value> value;
        EXPECT_TRUE(JSON::Value::parseJSON(" 1", value));
        EXPECT_TRUE(JSON::Value::parseJSON("\t1", value));
        EXPECT_TRUE(JSON::Value::parseJSON("\n1", value));
        EXPECT_TRUE(JSON::Value::parseJSON("1 ", value));
        EXPECT_TRUE(JSON::Value::parseJSON("1\t", value));
        EXPECT_TRUE(JSON::Value::parseJSON("1\n", value));
        EXPECT_TRUE(JSON::Value::parseJSON(" 1 ", value));
        EXPECT_TRUE(JSON::Value::parseJSON(" {} ", value));
        EXPECT_TRUE(JSON::Value::parseJSON(" [] ", value));
        EXPECT_TRUE(JSON::Value::parseJSON("\"\\xFF\"", value));
        EXPECT_TRUE(JSON::Value::parseJSON("\"\\u1234\"", value));

        EXPECT_FALSE(JSON::Value::parseJSON("1 1", value));
        EXPECT_FALSE(JSON::Value::parseJSON("{} {}", value));
        EXPECT_FALSE(JSON::Value::parseJSON("[] []", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\xF", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\xF\"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\xF \"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u1", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u1\"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u1   \"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u12", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u12\"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u12  \"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u123", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u123\"", value));
        EXPECT_FALSE(JSON::Value::parseJSON("\"\\u123 \"", value));
    }
}

TEST(JSONValue, MemoryCost)
{
    {
        Ref<JSON::Value> value = JSON::Value::null();
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(true);
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(1.0);
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(1);
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create("test");
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 36U);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create("");
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 32U);
    }

    {
        Ref<JSON::Value> value = JSON::Value::create(String());
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Value> valueA = JSON::Value::create("t");
        Ref<JSON::Value> valueB = JSON::Value::create("te");
        Ref<JSON::Value> valueC = JSON::Value::create("tes");
        Ref<JSON::Value> valueD = JSON::Value::create("test");
        EXPECT_LT(valueA->memoryCost(), valueB->memoryCost());
        EXPECT_LT(valueB->memoryCost(), valueC->memoryCost());
        EXPECT_LT(valueC->memoryCost(), valueD->memoryCost());
    }

    {
        Ref<JSON::Value> valueA = JSON::Value::create("t");
        Ref<JSON::Value> valueB = JSON::Value::create("😀");
        EXPECT_LT(valueA->memoryCost(), valueB->memoryCost());
    }

    {
        Ref<JSON::Object> value = JSON::Object::create();
        value->setValue("test", JSON::Value::null());
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 20U);
    }

    {
        Ref<JSON::Object> value = JSON::Object::create();
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Object> value = JSON::Object::create();

        value->setValue("1", JSON::Value::null());
        size_t memoryCost1 = value->memoryCost();

        value->setValue("2", JSON::Value::null());
        size_t memoryCost2 = value->memoryCost();

        value->setValue("3", JSON::Value::null());
        size_t memoryCost3 = value->memoryCost();

        value->setValue("4", JSON::Value::null());
        size_t memoryCost4 = value->memoryCost();

        EXPECT_LT(memoryCost1, memoryCost2);
        EXPECT_LT(memoryCost2, memoryCost3);
        EXPECT_LT(memoryCost3, memoryCost4);
    }

    {
        Ref<JSON::Array> value = JSON::Array::create();
        value->pushValue(JSON::Value::null());
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 16U);
    }

    {
        Ref<JSON::Array> value = JSON::Array::create();
        size_t memoryCost = value->memoryCost();
        EXPECT_GT(memoryCost, 0U);
        EXPECT_LE(memoryCost, 8U);
    }

    {
        Ref<JSON::Array> value = JSON::Array::create();

        value->pushValue(JSON::Value::null());
        size_t memoryCost1 = value->memoryCost();

        value->pushValue(JSON::Value::null());
        size_t memoryCost2 = value->memoryCost();

        value->pushValue(JSON::Value::null());
        size_t memoryCost3 = value->memoryCost();

        value->pushValue(JSON::Value::null());
        size_t memoryCost4 = value->memoryCost();

        EXPECT_LT(memoryCost1, memoryCost2);
        EXPECT_LT(memoryCost2, memoryCost3);
        EXPECT_LT(memoryCost3, memoryCost4);
    }
}

} // namespace TestWebKitAPI
