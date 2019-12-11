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
#include "CBORValue.h"

#if ENABLE(WEB_AUTHN)

#include <new>
#include <utility>

namespace cbor {

CBORValue::CBORValue()
    : m_type(Type::None)
{
}

CBORValue::CBORValue(CBORValue&& that)
{
    internalMoveConstructFrom(WTFMove(that));
}

CBORValue::CBORValue(Type type)
    : m_type(type)
{
    // Initialize with the default value.
    switch (m_type) {
    case Type::Unsigned:
    case Type::Negative:
        m_integerValue = 0;
        return;
    case Type::ByteString:
        new (&m_byteStringValue) BinaryValue();
        return;
    case Type::String:
        new (&m_stringValue) String();
        return;
    case Type::Array:
        new (&m_arrayValue) ArrayValue();
        return;
    case Type::Map:
        new (&m_mapValue) MapValue();
        return;
    case Type::SimpleValue:
        m_simpleValue = CBORValue::SimpleValue::Undefined;
        return;
    case Type::None:
        return;
    }
    ASSERT_NOT_REACHED();
}

CBORValue::CBORValue(int integerValue)
    : CBORValue(static_cast<int64_t>(integerValue))
{
}

CBORValue::CBORValue(int64_t integerValue)
    : m_integerValue(integerValue)
{
    m_type = integerValue >= 0 ? Type::Unsigned : Type::Negative;
}

CBORValue::CBORValue(const BinaryValue& inBytes)
    : m_type(Type::ByteString)
    , m_byteStringValue(inBytes)
{
}

CBORValue::CBORValue(BinaryValue&& inBytes)
    : m_type(Type::ByteString)
    , m_byteStringValue(WTFMove(inBytes))
{
}

CBORValue::CBORValue(const char* inString)
    : CBORValue(String(inString))
{
}

CBORValue::CBORValue(String&& inString)
    : m_type(Type::String)
    , m_stringValue(WTFMove(inString))
{
}

CBORValue::CBORValue(const String& inString)
    : m_type(Type::String)
    , m_stringValue(inString)
{
}

CBORValue::CBORValue(const ArrayValue& inArray)
    : m_type(Type::Array)
    , m_arrayValue()
{
    m_arrayValue.reserveCapacity(inArray.size());
    for (const auto& val : inArray)
        m_arrayValue.append(val.clone());
}

CBORValue::CBORValue(ArrayValue&& inArray)
    : m_type(Type::Array)
    , m_arrayValue(WTFMove(inArray))
{
}

CBORValue::CBORValue(const MapValue& inMap)
    : m_type(Type::Map)
    , m_mapValue()
{
    for (const auto& it : inMap)
        m_mapValue.emplace_hint(m_mapValue.end(), it.first.clone(), it.second.clone());
}

CBORValue::CBORValue(MapValue&& inMap)
    : m_type(Type::Map)
    , m_mapValue(WTFMove(inMap))
{
}

CBORValue::CBORValue(SimpleValue inSimple)
    : m_type(Type::SimpleValue)
    , m_simpleValue(inSimple)
{
    ASSERT(static_cast<int>(inSimple) >= 20 && static_cast<int>(inSimple) <= 23);
}

CBORValue::CBORValue(bool inBool)
    : m_type(Type::SimpleValue)
{
    m_simpleValue = inBool ? CBORValue::SimpleValue::TrueValue : CBORValue::SimpleValue::FalseValue;
}

CBORValue& CBORValue::operator=(CBORValue&& that)
{
    internalCleanup();
    internalMoveConstructFrom(WTFMove(that));

    return *this;
}

CBORValue::~CBORValue()
{
    internalCleanup();
}

CBORValue CBORValue::clone() const
{
    switch (m_type) {
    case Type::None:
        return CBORValue();
    case Type::Unsigned:
    case Type::Negative:
        return CBORValue(m_integerValue);
    case Type::ByteString:
        return CBORValue(m_byteStringValue);
    case Type::String:
        return CBORValue(m_stringValue);
    case Type::Array:
        return CBORValue(m_arrayValue);
    case Type::Map:
        return CBORValue(m_mapValue);
    case Type::SimpleValue:
        return CBORValue(m_simpleValue);
    }

    ASSERT_NOT_REACHED();
    return CBORValue();
}

const int64_t& CBORValue::getInteger() const
{
    ASSERT(isInteger());
    return m_integerValue;
}

const int64_t& CBORValue::getUnsigned() const
{
    ASSERT(isUnsigned());
    ASSERT(m_integerValue >= 0);
    return m_integerValue;
}

const int64_t& CBORValue::getNegative() const
{
    ASSERT(isNegative());
    ASSERT(m_integerValue < 0);
    return m_integerValue;
}

const String& CBORValue::getString() const
{
    ASSERT(isString());
    return m_stringValue;
}

const CBORValue::BinaryValue& CBORValue::getByteString() const
{
    ASSERT(isByteString());
    return m_byteStringValue;
}

const CBORValue::ArrayValue& CBORValue::getArray() const
{
    ASSERT(isArray());
    return m_arrayValue;
}

const CBORValue::MapValue& CBORValue::getMap() const
{
    ASSERT(isMap());
    return m_mapValue;
}

CBORValue::SimpleValue CBORValue::getSimpleValue() const
{
    ASSERT(isSimple());
    return m_simpleValue;
}

bool CBORValue::getBool() const
{
    ASSERT(isBool());
    return m_simpleValue == SimpleValue::TrueValue;
}

void CBORValue::internalMoveConstructFrom(CBORValue&& that)
{
    m_type = that.m_type;

    switch (m_type) {
    case Type::Unsigned:
    case Type::Negative:
        m_integerValue = that.m_integerValue;
        return;
    case Type::ByteString:
        new (&m_byteStringValue) BinaryValue(WTFMove(that.m_byteStringValue));
        return;
    case Type::String:
        new (&m_stringValue) String(WTFMove(that.m_stringValue));
        return;
    case Type::Array:
        new (&m_arrayValue) ArrayValue(WTFMove(that.m_arrayValue));
        return;
    case Type::Map:
        new (&m_mapValue) MapValue(WTFMove(that.m_mapValue));
        return;
    case Type::SimpleValue:
        m_simpleValue = that.m_simpleValue;
        return;
    case Type::None:
        return;
    }
    ASSERT_NOT_REACHED();
}

void CBORValue::internalCleanup()
{
    switch (m_type) {
    case Type::ByteString:
        m_byteStringValue.~BinaryValue();
        break;
    case Type::String:
        m_stringValue.~String();
        break;
    case Type::Array:
        m_arrayValue.~ArrayValue();
        break;
    case Type::Map:
        m_mapValue.~MapValue();
        break;
    case Type::None:
    case Type::Unsigned:
    case Type::Negative:
    case Type::SimpleValue:
        break;
    }
    m_type = Type::None;
}

} // namespace cbor

#endif // ENABLE(WEB_AUTHN)
