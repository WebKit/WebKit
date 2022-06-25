/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

const char* const nullToken = "null";
const char* const trueToken = "true";
const char* const falseToken = "false";

template<typename CodeUnit>
bool parseConstToken(const CodeUnit* start, const CodeUnit* end, const CodeUnit** tokenEnd, const char* token)
{
    while (start < end && *token != '\0' && *start++ == *token++) { }

    if (*token != '\0')
        return false;

    *tokenEnd = start;
    return true;
}

template<typename CodeUnit>
bool readInt(const CodeUnit* start, const CodeUnit* end, const CodeUnit** tokenEnd, bool canHaveLeadingZeros)
{
    if (start == end)
        return false;

    bool haveLeadingZero = '0' == *start;
    int length = 0;
    while (start < end && '0' <= *start && *start <= '9') {
        ++start;
        ++length;
    }

    if (!length)
        return false;

    if (!canHaveLeadingZeros && length > 1 && haveLeadingZero)
        return false;

    *tokenEnd = start;
    return true;
}

template<typename CodeUnit>
bool parseNumberToken(const CodeUnit* start, const CodeUnit* end, const CodeUnit** tokenEnd)
{
    // We just grab the number here. We validate the size in DecodeNumber.
    // According to RFC 4627, a valid number is: [minus] int [frac] [exp]
    if (start == end)
        return false;

    CodeUnit c = *start;
    if ('-' == c)
        ++start;

    if (!readInt(start, end, &start, false))
        return false;

    if (start == end) {
        *tokenEnd = start;
        return true;
    }

    // Optional fraction part.
    c = *start;
    if ('.' == c) {
        ++start;
        if (!readInt(start, end, &start, true))
            return false;
        if (start == end) {
            *tokenEnd = start;
            return true;
        }
        c = *start;
    }

    // Optional exponent part.
    if ('e' == c || 'E' == c) {
        ++start;
        if (start == end)
            return false;
        c = *start;
        if ('-' == c || '+' == c) {
            ++start;
            if (start == end)
                return false;
        }
        if (!readInt(start, end, &start, true))
            return false;
    }

    *tokenEnd = start;
    return true;
}

template<typename CodeUnit>
bool readHexDigits(const CodeUnit* start, const CodeUnit* end, const CodeUnit** tokenEnd, int digits)
{
    if (end - start < digits)
        return false;

    for (int i = 0; i < digits; ++i) {
        if (!isASCIIHexDigit(*start++))
            return false;
    }

    *tokenEnd = start;
    return true;
}

template<typename CodeUnit>
bool parseStringToken(const CodeUnit* start, const CodeUnit* end, const CodeUnit** tokenEnd)
{
    while (start < end) {
        CodeUnit c = *start++;
        if ('\\' == c && start < end) {
            c = *start++;
            // Make sure the escaped char is valid.
            switch (c) {
            case 'x':
                if (!readHexDigits(start, end, &start, 2))
                    return false;
                break;
            case 'u':
                if (!readHexDigits(start, end, &start, 4))
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
            *tokenEnd = start;
            return true;
        }
    }

    return false;
}

template<typename CodeUnit>
Token parseToken(const CodeUnit* start, const CodeUnit* end, const CodeUnit** tokenStart, const CodeUnit** tokenEnd)
{
    while (start < end && isSpaceOrNewline(*start))
        ++start;

    if (start == end)
        return Token::Invalid;

    *tokenStart = start;

    switch (*start) {
    case 'n':
        if (parseConstToken(start, end, tokenEnd, nullToken))
            return Token::Null;
        break;
    case 't':
        if (parseConstToken(start, end, tokenEnd, trueToken))
            return Token::BoolTrue;
        break;
    case 'f':
        if (parseConstToken(start, end, tokenEnd, falseToken))
            return Token::BoolFalse;
        break;
    case '[':
        *tokenEnd = start + 1;
        return Token::ArrayBegin;
    case ']':
        *tokenEnd = start + 1;
        return Token::ArrayEnd;
    case ',':
        *tokenEnd = start + 1;
        return Token::ListSeparator;
    case '{':
        *tokenEnd = start + 1;
        return Token::ObjectBegin;
    case '}':
        *tokenEnd = start + 1;
        return Token::ObjectEnd;
    case ':':
        *tokenEnd = start + 1;
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
        if (parseNumberToken(start, end, tokenEnd))
            return Token::Number;
        break;
    case '"':
        if (parseStringToken(start + 1, end, tokenEnd))
            return Token::String;
        break;
    }

    return Token::Invalid;
}

template<typename CodeUnit>
bool decodeString(const CodeUnit* start, const CodeUnit* end, StringBuilder& output)
{
    while (start < end) {
        UChar c = *start++;
        if ('\\' != c) {
            output.append(c);
            continue;
        }
        if (UNLIKELY(start >= end))
            return false;
        c = *start++;
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
            if (UNLIKELY(start + 1 >= end))
                return false;
            c = toASCIIHexValue(start[0], start[1]);
            start += 2;
            break;
        case 'u':
            if (UNLIKELY(start + 3 >= end))
                return false;
            c = toASCIIHexValue(start[0], start[1]) << 8 | toASCIIHexValue(start[2], start[3]);
            start += 4;
            break;
        default:
            return false;
        }
        output.append(c);
    }

    return true;
}

template<typename CodeUnit>
bool decodeString(const CodeUnit* start, const CodeUnit* end, String& output)
{
    if (start == end) {
        output = emptyString();
        return true;
    }

    if (start > end)
        return false;

    StringBuilder buffer;
    buffer.reserveCapacity(end - start);
    if (!decodeString(start, end, buffer))
        return false;

    output = buffer.toString();
    return true;
}

template<typename CodeUnit>
RefPtr<JSON::Value> buildValue(const CodeUnit* start, const CodeUnit* end, const CodeUnit** valueTokenEnd, int depth)
{
    if (depth > stackLimit)
        return nullptr;

    RefPtr<JSON::Value> result;
    const CodeUnit* tokenStart;
    const CodeUnit* tokenEnd;
    Token token = parseToken(start, end, &tokenStart, &tokenEnd);
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
        double value = charactersToDouble(tokenStart, tokenEnd - tokenStart, &ok);
        if (!ok)
            return nullptr;
        result = JSON::Value::create(value);
        break;
    }
    case Token::String: {
        String value;
        bool ok = decodeString(tokenStart + 1, tokenEnd - 1, value);
        if (!ok)
            return nullptr;
        result = JSON::Value::create(value);
        break;
    }
    case Token::ArrayBegin: {
        Ref<JSON::Array> array = JSON::Array::create();
        start = tokenEnd;
        token = parseToken(start, end, &tokenStart, &tokenEnd);
        while (token != Token::ArrayEnd) {
            RefPtr<JSON::Value> arrayNode = buildValue(start, end, &tokenEnd, depth + 1);
            if (!arrayNode)
                return nullptr;
            array->pushValue(arrayNode.releaseNonNull());

            // After a list value, we expect a comma or the end of the list.
            start = tokenEnd;
            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token == Token::ListSeparator) {
                start = tokenEnd;
                token = parseToken(start, end, &tokenStart, &tokenEnd);
                if (token == Token::ArrayEnd)
                    return nullptr;
            } else if (token != Token::ArrayEnd) {
                // Unexpected value after list value. Bail out.
                return nullptr;
            }
        }
        if (token != Token::ArrayEnd)
            return nullptr;
        result = WTFMove(array);
        break;
    }
    case Token::ObjectBegin: {
        Ref<JSON::Object> object = JSON::Object::create();
        start = tokenEnd;
        token = parseToken(start, end, &tokenStart, &tokenEnd);
        while (token != Token::ObjectEnd) {
            if (token != Token::String)
                return nullptr;
            String key;
            if (!decodeString(tokenStart + 1, tokenEnd - 1, key))
                return nullptr;
            start = tokenEnd;

            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token != Token::ObjectPairSeparator)
                return nullptr;
            start = tokenEnd;

            RefPtr<JSON::Value> value = buildValue(start, end, &tokenEnd, depth + 1);
            if (!value)
                return nullptr;
            object->setValue(key, value.releaseNonNull());
            start = tokenEnd;

            // After a key/value pair, we expect a comma or the end of the
            // object.
            token = parseToken(start, end, &tokenStart, &tokenEnd);
            if (token == Token::ListSeparator) {
                start = tokenEnd;
                token = parseToken(start, end, &tokenStart, &tokenEnd);
                if (token == Token::ObjectEnd)
                    return nullptr;
            } else if (token != Token::ObjectEnd) {
                // Unexpected value after last object value. Bail out.
                return nullptr;
            }
        }
        if (token != Token::ObjectEnd)
            return nullptr;
        result = WTFMove(object);
        break;
    }

    default:
        // We got a token that's not a value.
        return nullptr;
    }
    *valueTokenEnd = tokenEnd;
    return result;
}

inline void appendDoubleQuotedString(StringBuilder& builder, StringView string)
{
    builder.append('"');
    Value::escapeString(builder, string);
    builder.append('"');
}

} // anonymous namespace

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

RefPtr<Value> Value::asValue()
{
    return this;
}

RefPtr<Object> Value::asObject()
{
    return nullptr;
}

RefPtr<const Object> Value::asObject() const
{
    return nullptr;
}

RefPtr<Array> Value::asArray()
{
    return nullptr;
}

RefPtr<Value> Value::parseJSON(const String& json)
{
    auto containsNonSpace = [] (const auto* begin, const auto* end) {
        if (!begin)
            return false;
        for (const auto* it = begin; it < end; it++) {
            if (!isSpaceOrNewline(*it))
                return true;
        }
        return false;
    };

    RefPtr<Value> result;
    if (json.is8Bit()) {
        const LChar* start = json.characters8();
        const LChar* end = start + json.length();
        const LChar* tokenEnd { nullptr };
        result = buildValue(start, end, &tokenEnd, 0);
        if (containsNonSpace(tokenEnd, end))
            return nullptr;
    } else {
        const UChar* start = json.characters16();
        const UChar* end = start + json.length();
        const UChar* tokenEnd { nullptr };
        result = buildValue(start, end, &tokenEnd, 0);
        if (containsNonSpace(tokenEnd, end))
            return nullptr;
    }
    return result;
}

void Value::escapeString(StringBuilder& builder, StringView string)
{
    for (UChar codeUnit : string.codeUnits()) {
        switch (codeUnit) {
        case '\b':
            builder.append("\\b");
            continue;
        case '\f':
            builder.append("\\f");
            continue;
        case '\n':
            builder.append("\\n");
            continue;
        case '\r':
            builder.append("\\r");
            continue;
        case '\t':
            builder.append("\\t");
            continue;
        case '\\':
            builder.append("\\\\");
            continue;
        case '"':
            builder.append("\\\"");
            continue;
        }
        // We escape < and > to prevent script execution.
        if (codeUnit >= 32 && codeUnit < 127 && codeUnit != '<' && codeUnit != '>') {
            builder.append(codeUnit);
            continue;
        }
        // We could encode characters >= 127 as UTF-8 instead of \u escape sequences.
        // We could handle surrogates here if callers wanted that; for now we just
        // write them out as a \u sequence, so a surrogate pair appears as two of them.
        builder.append("\\u",
            upperNibbleToASCIIHexDigit(codeUnit >> 8), lowerNibbleToASCIIHexDigit(codeUnit >> 8),
            upperNibbleToASCIIHexDigit(codeUnit), lowerNibbleToASCIIHexDigit(codeUnit));
    }
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

void Value::writeJSON(StringBuilder& output) const
{
    switch (m_type) {
    case Type::Null:
        output.append("null");
        break;
    case Type::Boolean:
        if (m_value.boolean)
            output.append("true");
        else
            output.append("false");
        break;
    case Type::String:
        appendDoubleQuotedString(output, m_value.string);
        break;
    case Type::Double:
    case Type::Integer: {
        if (!std::isfinite(m_value.number))
            output.append("null");
        else
            output.append(m_value.number);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
}

size_t Value::memoryCost() const
{
    size_t memoryCost = sizeof(this);
    if (m_type == Type::String && m_value.string)
        memoryCost += m_value.string->sizeInBytes();
    return memoryCost;
}

ObjectBase::~ObjectBase()
{
}

RefPtr<Object> ObjectBase::asObject()
{
    static_assert(sizeof(Object) == sizeof(ObjectBase), "cannot cast");
    return static_cast<Object*>(this);
}

RefPtr<const Object> ObjectBase::asObject() const
{
    static_assert(sizeof(Object) == sizeof(ObjectBase), "cannot cast");
    return static_cast<const Object*>(this);
}

size_t ObjectBase::memoryCost() const
{
    size_t memoryCost = Value::memoryCost();
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

void ObjectBase::writeJSON(StringBuilder& output) const
{
    output.append('{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        auto findResult = m_map.find(m_order[i]);
        ASSERT(findResult != m_map.end());
        if (i)
            output.append(',');
        appendDoubleQuotedString(output, findResult->key);
        output.append(':');
        findResult->value->writeJSON(output);
    }
    output.append('}');
}

ObjectBase::ObjectBase()
    : Value(Type::Object)
{
}

ArrayBase::~ArrayBase()
{
}

RefPtr<Array> ArrayBase::asArray()
{
    static_assert(sizeof(ArrayBase) == sizeof(Array), "cannot cast");
    return static_cast<Array*>(this);
}

void ArrayBase::writeJSON(StringBuilder& output) const
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

size_t ArrayBase::memoryCost() const
{
    size_t memoryCost = Value::memoryCost();
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
