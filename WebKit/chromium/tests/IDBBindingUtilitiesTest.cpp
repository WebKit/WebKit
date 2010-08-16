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
#include "IDBBindingUtilities.h"
#include "IDBKey.h"
#include "IDBKeyPath.h"
#include "SerializedScriptValue.h"

#include <gtest/gtest.h>
#include <wtf/Vector.h>

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace {

class LocalContext {
public:
    LocalContext()
        : m_context(v8::Context::New())
    {
        m_context->Enter();
    }

    virtual ~LocalContext()
    {
        m_context->Exit();
        m_context.Dispose();
    }

private:
    v8::Locker m_locker;
    v8::HandleScope m_scope;
    v8::Persistent<v8::Context> m_context;
};

PassRefPtr<IDBKey> checkKeyFromValueAndKeyPathInternal(SerializedScriptValue* value, const String& keyPath)
{
    Vector<IDBKeyPathElement> idbKeyPath;
    IDBKeyPathParseError parseError;
    IDBParseKeyPath(keyPath, idbKeyPath, parseError);
    EXPECT_EQ(IDBKeyPathParseErrorNone, parseError);
    return createIDBKeyFromSerializedValueAndKeyPath(value, idbKeyPath);
}

void checkKeyPathNullValue(SerializedScriptValue* value, const String& keyPath)
{
    RefPtr<IDBKey> idbKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    ASSERT_FALSE(idbKey.get());
}

void checkKeyPathStringValue(SerializedScriptValue* value, const String& keyPath, const String& expected)
{
    RefPtr<IDBKey> idbKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    ASSERT_TRUE(idbKey.get());
    ASSERT_EQ(IDBKey::StringType, idbKey->type());
    ASSERT_TRUE(expected == idbKey->string());
}

void checkKeyPathNumberValue(SerializedScriptValue* value, const String& keyPath, int expected)
{
    RefPtr<IDBKey> idbKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    ASSERT_TRUE(idbKey.get());
    ASSERT_EQ(IDBKey::NumberType, idbKey->type());
    ASSERT_TRUE(expected == idbKey->number());
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue)
{
    LocalContext v8context;
    v8::Local<v8::Object> object = v8::Object::New();
    object->Set(v8::String::New("foo"), v8::String::New("zoo"));

    RefPtr<SerializedScriptValue> serializedScriptValue = SerializedScriptValue::create(object);

    checkKeyPathStringValue(serializedScriptValue.get(), "foo", "zoo");
    checkKeyPathNullValue(serializedScriptValue.get(), "bar");
    checkKeyPathNullValue(serializedScriptValue.get(), "[3]");
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue)
{
    LocalContext v8context;
    v8::Local<v8::Object> object = v8::Object::New();
    object->Set(v8::String::New("foo"), v8::Number::New(456));

    RefPtr<SerializedScriptValue> serializedScriptValue = SerializedScriptValue::create(object);

    checkKeyPathNumberValue(serializedScriptValue.get(), "foo", 456);
    checkKeyPathNullValue(serializedScriptValue.get(), "bar");
    checkKeyPathNullValue(serializedScriptValue.get(), "[3]");
}

TEST(IDBKeyFromValueAndKeyPathTest, TopLevelArrayElement)
{
    LocalContext v8context;
    v8::Local<v8::Array> array = v8::Array::New();
    array->Set(3, v8::String::New("zoo"));

    RefPtr<SerializedScriptValue> serializedScriptValue = SerializedScriptValue::create(array);

    checkKeyPathStringValue(serializedScriptValue.get(), "[3]", "zoo");
    checkKeyPathNullValue(serializedScriptValue.get(), "foo");
    checkKeyPathNullValue(serializedScriptValue.get(), "bar");
}

TEST(IDBKeyFromValueAndKeyPathTest, SubProperty)
{
    LocalContext v8context;
    v8::Local<v8::Object> object = v8::Object::New();
    v8::Local<v8::Object> subProperty = v8::Object::New();
    subProperty->Set(v8::String::New("bar"), v8::String::New("zee"));
    object->Set(v8::String::New("foo"), subProperty);

    RefPtr<SerializedScriptValue> serializedScriptValue = SerializedScriptValue::create(object);

    checkKeyPathStringValue(serializedScriptValue.get(), "foo.bar", "zee");
    checkKeyPathNullValue(serializedScriptValue.get(), "bar");
    checkKeyPathNullValue(serializedScriptValue.get(), "[3]");
}

TEST(IDBKeyFromValueAndKeyPathTest, Array2D)
{
    LocalContext v8context;
    v8::Local<v8::Object> object = v8::Object::New();
    v8::Local<v8::Array> array = v8::Array::New();
    v8::Local<v8::Array> subArray = v8::Array::New();
    subArray->Set(7, v8::String::New("zee"));
    array->Set(3, subArray);
    object->Set(v8::String::New("foo"), array);

    RefPtr<SerializedScriptValue> serializedScriptValue = SerializedScriptValue::create(object);

    checkKeyPathStringValue(serializedScriptValue.get(), "foo[3][7]", "zee");
    checkKeyPathNullValue(serializedScriptValue.get(), "bar");
    checkKeyPathNullValue(serializedScriptValue.get(), "[4]");
}

} // namespace

#endif // ENABLE(INDEXED_DATABASE)
