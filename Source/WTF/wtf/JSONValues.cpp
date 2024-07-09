/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/JSONValues.h>

#include <functional>
#include <wtf/CommaPrinter.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WTF {
namespace JSONImpl {

namespace {

static constexpr int stackLimit = 1000;

enum class Token {
    ObjectBegin,
    ObjectEnd,
    ArrayBegin,
    ArrayEnd,
    String,
    Number,
    BoolTrue,
    BoolFalse,
    Null,
    ListSeparator,
    ObjectPairSeparator,
    Invalid,
};

constexpr auto nullToken = "null"_s;
constexpr auto trueToken = "true"_s;
constexpr auto falseToken = "false"_s;

template<typename CodeUnit>
bool parseConstToken(std::span<const CodeUnit> data, std::span<const CodeUnit>& tokenEnd, const char* token)
{
    while (!data.empty() && *token != '\0') {
        auto character = data.front();
        data = data.subspan(1);
        if (character != *token++)
            break;
    }

    if (*token != '\0')
        return false;

    tokenEnd = data;
    return true;
}

template<typename CodeUnit>
bool readInt(std::span<const CodeUnit> data, std::span<const CodeUnit>& tokenEnd, bool canHaveLeadingZeros)
{
    if (data.empty())
        return false;

    bool haveLeadingZero = '0' == data.front();
    int length = 0;
    while (!data.empty() && '0' <= data.front() && data.front() <= '9') {
        data = data.subspan(1);
        ++length;
    }

    if (!length)
        return false;

    if (!canHaveLeadingZeros && length > 1 && haveLeadingZero)
        return false;

    tokenEnd = data;
    return true;
}

template<typename CodeUnit>
bool parseNumberToken(std::span<const CodeUnit> data, std::span<const CodeUnit>& tokenEnd)
{
    // We just grab the number here. We validate the size in DecodeNumber.
    // According to RFC 4627, a valid number is: [minus] int [frac] [exp]
    if (data.empty())
        return false;

    CodeUnit c = data.front();
    if ('-' == c)
        data = data.subspan(1);

    if (!readInt(data, data, false))
        return false;

    if (data.empty()) {
        tokenEnd = data;
        return true;
    }

    // Optional fraction part.
    c = data.front();
    if ('.' == c) {
        data = data.subspan(1);
        if (!readInt(data, data, true))
            return false;
        if (data.empty()) {
            tokenEnd = data;
            return true;
        }
        c = data.front();
    }

    // Optional exponent part.
    if ('e' == c || 'E' == c) {
        data = data.subspan(1);
        if (data.empty())
            return false;
        c = data.front();
        if ('-' == c || '+' == c) {
            data = data.subspan(1);
            if (data.empty())
                return false;
        }
        if (!readInt(data, data, true))
            return false;
    }

    tokenEnd = data;
    return true;
}

template<typename CodeUnit>
bool readHexDigits(std::span<const CodeUnit> data, std::span<const CodeUnit>& tokenEnd, unsigned digits)
{
    if (data.size() < digits)
        return false;

    for (unsigned i = 0; i < digits; ++i) {
        if (!isASCIIHexDigit(data.front()))
            return false;
        data = data.subspan(1);
    }

    tokenEnd = data;
    return true;
}

template<typename CodeUnit>
bool parseStringToken(std::span<const CodeUnit> data, std::span<const CodeUnit>& tokenEnd)
{
    while (!data.empty()) {
        CodeUnit c = data.front();
        data = data.subspan(1);
        if ('\\' == c && !data.empty()) {
            c = data.front();
            data = data.subspan(1);
            // Make sure the escaped char is valid.
            switch (c) {
            case 'x':
                if (!readHexDigits(data, data, 2))
                    return false;
                break;
            case 'u':
                if (!readHexDigits(data, data, 4))
                    return false;
                break;
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            case '"':
                break;
            default:
                return false;
            }
        } else if ('"' == c) {
            tokenEnd = data;
            return true;
        }
    }

    return false;
}

template<typename CodeUnit>
Token parseToken(std::span<const CodeUnit> data, std::span<const CodeUnit>& tokenStart, std::span<const CodeUnit>& tokenEnd)
{
    while (!data.empty() && isASCIIWhitespaceWithoutFF(data.front()))
        data = data.subspan(1);

    if (data.empty())
        return Token::Invalid;

    tokenStart = data;

    switch (data.front()) {
    case 'n':
        if (parseConstToken(data, tokenEnd, nullToken))
            return Token::Null;
        break;
    case 't':
        if (parseConstToken(data, tokenEnd, trueToken))
            return Token::BoolTrue;
        break;
    case 'f':
        if (parseConstToken(data, tokenEnd, falseToken))
            return Token::BoolFalse;
        break;
    case '[':
        tokenEnd = data.subspan(1);
        return Token::ArrayBegin;
    case ']':
        tokenEnd = data.subspan(1);
        return Token::ArrayEnd;
    case ',':
        tokenEnd = data.subspan(1);
        return Token::ListSeparator;
    case '{':
        tokenEnd = data.subspan(1);
        return Token::ObjectBegin;
    case '}':
        tokenEnd = data.subspan(1);
        return Token::ObjectEnd;
    case ':':
        tokenEnd = data.subspan(1);
        return Token::ObjectPairSeparator;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        if (parseNumberToken(data, tokenEnd))
            return Token::Number;
        break;
    case '"':
        if (parseStringToken(data.subspan(1), tokenEnd))
            return Token::String;
        break;
    }

    return Token::Invalid;
}

template<typename CodeUnit>
bool decodeString(std::span<const CodeUnit> data, StringBuilder& output)
{
    while (!data.empty()) {
        UChar c = data.front();
        data = data.subspan(1);
        if ('\\' != c) {
            output.append(c);
            continue;
        }
        if (UNLIKELY(data.empty()))
            return false;
        c = data.front();
        data = data.subspan(1);
        switch (c) {
        case '"':
        case '/':
        case '\\':
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'x':
            if (UNLIKELY(data.size() < 2))
                return false;
            c = toASCIIHexValue(data[0], data[1]);
            data = data.subspan(2);
            break;
        case 'u':
            if (UNLIKELY(data.size() < 4))
                return false;
            c = toASCIIHexValue(data[0], data[1]) << 8 | toASCIIHexValue(data[2], data[3]);
            data = data.subspan(4);
            break;
        default:
            return false;
        }
        output.append(c);
    }

    return true;
}

template<typename CodeUnit>
bool decodeString(std::span<const CodeUnit> data, String& output)
{
    if (data.empty()) {
        output = emptyString();
        return true;
    }

    StringBuilder buffer;
    buffer.reserveCapacity(data.size());
    if (!decodeString(data, buffer))
        return false;

    output = buffer.toString();
    return true;
}

template<typename CodeUnit>
RefPtr<JSON::Value> buildValue(std::span<const CodeUnit> data, std::span<const CodeUnit>& valueTokenEnd, int depth)
{
    if (depth > stackLimit)
        return nullptr;

    RefPtr<JSON::Value> result;
    std::span<const CodeUnit> tokenStart;
    std::span<const CodeUnit> tokenEnd;
    Token token = parseToken(data, tokenStart, tokenEnd);
    switch (token) {
    case Token::Invalid:
        return nullptr;
    case Token::Null:
        result = JSON::Value::null();
        break;
    case Token::BoolTrue:
        result = JSON::Value::create(true);
        break;
    case Token::BoolFalse:
        result = JSON::Value::create(false);
        break;
    case Token::Number: {
        bool ok;
        double value = charactersToDouble(tokenStart.first(tokenEnd.data() - tokenStart.data()), &ok);
        if (!ok)
            return nullptr;
        result = JSON::Value::create(value);
        break;
    }
    case Token::String: {
        String value;
        if (tokenEnd.data() - tokenStart.data() < 2)
            return nullptr;
        bool ok = decodeString(std::span { tokenStart.data() + 1, tokenEnd.data() - 1 }, value);
        if (!ok)
            return nullptr;
        result = JSON::Value::create(value);
        break;
    }
    case Token::ArrayBegin: {
        Ref<JSON::Array> array = JSON::Array::create();
        data = tokenEnd;
        token = parseToken(data, tokenStart, tokenEnd);
        while (token != Token::ArrayEnd) {
            RefPtr<JSON::Value> arrayNode = buildValue(data, tokenEnd, depth + 1);
            if (!arrayNode)
                return nullptr;
            array->pushValue(arrayNode.releaseNonNull());

            // After a list value, we expect a comma or the end of the list.
            data = tokenEnd;
            token = parseToken(data, tokenStart, tokenEnd);
            if (token == Token::ListSeparator) {
                data = tokenEnd;
                token = parseToken(data, tokenStart, tokenEnd);
                if (token == Token::ArrayEnd)
                    return nullptr;
            } else if (token != Token::ArrayEnd) {
                // Unexpected value after list value. Bail out.
                return nullptr;
            }
        }
        ASSERT(token == Token::ArrayEnd);
        result = WTFMove(array);
        break;
    }
    case Token::ObjectBegin: {
        Ref<JSON::Object> object = JSON::Object::create();
        data = tokenEnd;
        token = parseToken(data, tokenStart, tokenEnd);
        while (token != Token::ObjectEnd) {
            if (token != Token::String)
                return nullptr;
            String key;
            if (tokenEnd.data() - tokenStart.data() < 2)
                return nullptr;
            if (!decodeString(std::span { tokenStart.data() + 1, tokenEnd.data() - 1 }, key))
                return nullptr;
            data = tokenEnd;

            token = parseToken(data, tokenStart, tokenEnd);
            if (token != Token::ObjectPairSeparator)
                return nullptr;
            data = tokenEnd;

            RefPtr<JSON::Value> value = buildValue(data, tokenEnd, depth + 1);
            if (!value)
                return nullptr;
            object->setValue(key, value.releaseNonNull());
            data = tokenEnd;

            // After a key/value pair, we expect a comma or the end of the
            // object.
            token = parseToken(data, tokenStart, tokenEnd);
            if (token == Token::ListSeparator) {
                data = tokenEnd;
                token = parseToken(data, tokenStart, tokenEnd);
                if (token == Token::ObjectEnd)
                    return nullptr;
            } else if (token != Token::ObjectEnd) {
                // Unexpected value after last object value. Bail out.
                return nullptr;
            }
        }
        ASSERT(token == Token::ObjectEnd);
        result = WTFMove(object);
        break;
    }

    default:
        // We got a token that's not a value.
        return nullptr;
    }
    valueTokenEnd = tokenEnd;
    return result;
}

} // anonymous namespace

template<typename Visitor> constexpr decltype(auto) Value::visitDerived(Visitor&& visitor)
{
    switch (m_type) {
    case Type::Null:
    case Type::Boolean:
    case Type::Double:
    case Type::Integer:
    case Type::String:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<Value&>(*this));
    case Type::Object:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<Object&>(*this));
    case Type::Array:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<Array&>(*this));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor> constexpr decltype(auto) Value::visitDerived(Visitor&& visitor) const
{
    return const_cast<Value&>(*this).visitDerived([&](auto& derived) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(derived));
    });
}

void Value::operator delete(Value* value, std::destroying_delete_t)
{
    value->visitDerived([](auto& derived) {
        std::destroy_at(&derived);
        std::decay_t<decltype(derived)>::freeAfterDestruction(&derived);
    });
}

Ref<Value> Value::null()
{
    return adoptRef(*new Value);
}

Ref<Value> Value::create(bool value)
{
    return adoptRef(*new Value(value));
}

Ref<Value> Value::create(int value)
{
    return adoptRef(*new Value(value));
}

Ref<Value> Value::create(double value)
{
    return adoptRef(*new Value(value));
}

Ref<Value> Value::create(const String& value)
{
    return adoptRef(*new Value(value));
}

RefPtr<Value> Value::parseJSON(StringView json)
{
    auto containsNonSpace = [] (auto span) {
        if (!span.data())
            return false;
        for (auto character : span) {
            if (!isASCIIWhitespaceWithoutFF(character))
                return true;
        }
        return false;
    };

    RefPtr<Value> result;
    if (json.is8Bit()) {
        auto data = json.span8();
        std::span<const LChar> tokenEnd;
        result = buildValue(data, tokenEnd, 0);
        if (containsNonSpace(tokenEnd))
            return nullptr;
    } else {
        auto data = json.span16();
        std::span<const UChar> tokenEnd;
        result = buildValue(data, tokenEnd, 0);
        if (containsNonSpace(tokenEnd))
            return nullptr;
    }
    return result;
}

String Value::toJSONString() const
{
    StringBuilder result;
    result.reserveCapacity(512);
    writeJSON(result);
    return result.toString();
}

std::optional<bool> Value::asBoolean() const
{
    if (type() != Type::Boolean)
        return std::nullopt;
    return m_value.boolean;
}

std::optional<double> Value::asDouble() const
{
    if (type() != Type::Double && type() != Type::Integer)
        return std::nullopt;
    return m_value.number;
}

std::optional<int> Value::asInteger() const
{
    if (type() != Type::Double && type() != Type::Integer)
        return std::nullopt;
    return static_cast<int>(m_value.number);
}

String Value::asString() const
{
    if (type() != Type::String)
        return nullString();
    return m_value.string;
}

void Value::dump(PrintStream& out) const
{
    switch (m_type) {
    case Type::Null:
        out.print("null"_s);
        break;
    case Type::Boolean:
        out.print(m_value.boolean ? "true"_s : "false"_s);
        break;
    case Type::String: {
        StringBuilder builder;
        builder.appendQuotedJSONString(m_value.string);
        out.print(builder.toString());
        break;
    }
    case Type::Double:
    case Type::Integer: {
        if (!std::isfinite(m_value.number))
            out.print("null"_s);
        else
            out.print(makeString(m_value.number));
        break;
    }
    case Type::Object: {
        auto& object = *static_cast<const ObjectBase*>(this);
        CommaPrinter comma(","_s);
        out.print("{"_s);
        for (const auto& key : object.m_order) {
            auto findResult = object.m_map.find(key);
            ASSERT(findResult != object.m_map.end());
            StringBuilder builder;
            builder.appendQuotedJSONString(findResult->key);
            out.print(comma, builder.toString(), ":"_s, findResult->value.get());
        }
        out.print("}"_s);
        break;
    }
    case Type::Array: {
        auto& array = *static_cast<const ArrayBase*>(this);
        CommaPrinter comma(","_s);
        out.print("["_s);
        for (auto& value : array.m_map)
            out.print(comma, value.get());
        out.print("]"_s);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

void Value::writeJSON(StringBuilder& output) const
{
    visitDerived([&](auto& derived) {
        derived.writeJSONImpl(output);
    });
}

void Value::writeJSONImpl(StringBuilder& output) const
{
    switch (m_type) {
    case Type::Null:
        output.append("null"_s);
        break;
    case Type::Boolean:
        if (m_value.boolean)
            output.append("true"_s);
        else
            output.append("false"_s);
        break;
    case Type::String:
        output.appendQuotedJSONString(m_value.string);
        break;
    case Type::Double:
    case Type::Integer: {
        if (!std::isfinite(m_value.number))
            output.append("null"_s);
        else
            output.append(m_value.number);
        break;
    }
    case Type::Object:
    case Type::Array:
        ASSERT_NOT_REACHED();
    }
}

size_t Value::memoryCost() const
{
    return visitDerived([&](auto& derived) {
        return derived.memoryCostImpl();
    });
}

size_t Value::memoryCostImpl() const
{
    size_t memoryCost = sizeof(*this);
    if (m_type == Type::String && m_value.string)
        memoryCost += m_value.string->sizeInBytes();
    return memoryCost;
}

ObjectBase::~ObjectBase() = default;

size_t ObjectBase::memoryCostImpl() const
{
    size_t memoryCost = sizeof(*this);
    for (const auto& entry : m_map)
        memoryCost += entry.key.sizeInBytes() + entry.value->memoryCost();
    return memoryCost;
}

std::optional<bool> ObjectBase::getBoolean(const String& name) const
{
    auto value = getValue(name);
    if (!value)
        return std::nullopt;
    return value->asBoolean();
}

std::optional<double> ObjectBase::getDouble(const String& name) const
{
    auto value = getValue(name);
    if (!value)
        return std::nullopt;
    return value->asDouble();
}

std::optional<int> ObjectBase::getInteger(const String& name) const
{
    auto value = getValue(name);
    if (!value)
        return std::nullopt;
    return value->asInteger();
}

String ObjectBase::getString(const String& name) const
{
    auto value = getValue(name);
    if (!value)
        return nullString();
    return value->asString();
}

RefPtr<Object> ObjectBase::getObject(const String& name) const
{
    auto value = getValue(name);
    if (!value)
        return nullptr;
    return value->asObject();
}

RefPtr<Array> ObjectBase::getArray(const String& name) const
{
    auto value = getValue(name);
    if (!value)
        return nullptr;
    return value->asArray();
}

RefPtr<Value> ObjectBase::getValue(const String& name) const
{
    auto findResult = m_map.find(name);
    if (findResult == m_map.end())
        return nullptr;
    return findResult->value.copyRef();
}

void ObjectBase::remove(const String& name)
{
    m_map.remove(name);
    m_order.removeFirst(name);
}

void ObjectBase::writeJSONImpl(StringBuilder& output) const
{
    output.append('{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        auto findResult = m_map.find(m_order[i]);
        ASSERT(findResult != m_map.end());
        if (i)
            output.append(',');
        output.appendQuotedJSONString(findResult->key);
        output.append(':');
        findResult->value->writeJSON(output);
    }
    output.append('}');
}

ObjectBase::ObjectBase()
    : Value(Type::Object)
{
}

ArrayBase::~ArrayBase() = default;

void ArrayBase::writeJSONImpl(StringBuilder& output) const
{
    output.append('[');
    for (auto it = m_map.begin(); it != m_map.end(); ++it) {
        if (it != m_map.begin())
            output.append(',');
        (*it)->writeJSON(output);
    }
    output.append(']');
}

ArrayBase::ArrayBase()
    : Value(Type::Array)
{
}

Ref<Value> ArrayBase::get(size_t index) const
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(index < m_map.size());
    return m_map[index];
}

Ref<Object> Object::create()
{
    return adoptRef(*new Object);
}

Ref<Array> Array::create()
{
    return adoptRef(*new Array);
}

size_t ArrayBase::memoryCostImpl() const
{
    size_t memoryCost = sizeof(*this);
    for (const auto& item : m_map)
        memoryCost += item->memoryCost();
    return memoryCost;
}

// FIXME: <http://webkit.org/b/179847> remove these functions when legacy InspectorObject symbols are no longer needed.

bool Value::asDouble(double& output) const
{
    auto x = asDouble();
    if (!x)
        return false;
    output = *x;
    return true;
}

bool Value::asInteger(int& output) const
{
    auto x = asInteger();
    if (!x)
        return false;
    output = *x;
    return true;
}

bool Value::asString(String& output) const
{
    auto x = asString();
    if (!x)
        return false;
    output = x;
    return true;
}

bool ObjectBase::getBoolean(const String& name, bool& output) const
{
    auto x = getBoolean(name);
    if (!x)
        return false;
    output = *x;
    return true;
}

bool ObjectBase::getString(const String& name, String& output) const
{
    auto x = getString(name);
    if (!x)
        return false;
    output = x;
    return true;
}

bool ObjectBase::getObject(const String& name, RefPtr<Object>& output) const
{
    auto x = getObject(name);
    if (!x)
        return false;
    output = x.releaseNonNull();
    return true;
}

bool ObjectBase::getArray(const String& name, RefPtr<Array>& output) const
{
    auto x = getArray(name);
    if (!x)
        return false;
    output = x.releaseNonNull();
    return true;
}

bool ObjectBase::getValue(const String& name, RefPtr<Value>& output) const
{
    auto x = getValue(name);
    if (!x)
        return false;
    output = x.releaseNonNull();
    return true;
}

} // namespace JSONImpl
} // namespace WTF
