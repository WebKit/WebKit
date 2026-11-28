/*
 * Copyright (C) 2026-2026 Apple Inc. All rights reserved.
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
#include "JSWebExtensionValueParser.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#if PLATFORM(COCOA)
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#endif
#include <wtf/Scope.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

String describeValueType(JSContextRef context, JSValueRef value)
{
    if (JSValueIsNull(context, value))
        return "null"_s;

    if (JSValueIsNumber(context, value)) {
        double number = JSValueToNumber(context, value, nullptr);
        if (std::isnan(number))
            return "NaN"_s;

        if (std::isinf(number))
            return "Infinity"_s;

        return "a number"_s;
    }

    if (JSValueIsBoolean(context, value))
        return "a boolean"_s;

    if (JSValueIsString(context, value))
        return "a string"_s;

    if (JSValueIsArray(context, value))
        return "an array"_s;

    return "an object"_s;
}

JSObjectRef toJSError(JSContextRef context, const String& callingAPIName, const String& argumentName, const WebExtensionValueError& error)
{
    return toJSError(context, callingAPIName, error.key.isNull() ? argumentName : error.key, error.message);
}

String joinValuePath(const String& parentPath, const String& key)
{
    if (parentPath.isEmpty())
        return key;

    if (key.startsWith('['))
        return makeString(parentPath, key);

    return makeString(parentPath, '.', key);
}

String valuePathLeaf(const String& path)
{
    auto end = path.length();
    while (end && path[end - 1] == ']') {
        auto open = path.reverseFind('[', end - 1);
        if (open == notFound)
            break;

        end = open;
    }

    if (!end)
        return nullString();

    auto prefix = path.left(end);
    auto separator = prefix.reverseFind('.');
    return separator == notFound ? prefix : prefix.substring(separator + 1);
}

String joinValueChoices(const Vector<String>& choices)
{
    if (choices.size() == 1)
        return choices[0];

    if (choices.size() == 2)
        return makeString(choices[0], " or "_s, choices[1]);

    StringBuilder builder;
    size_t index = 0;
    for (const auto& choice : borrow(choices)) {
        if (index)
            builder.append(", "_s);

        if (index == choices.size() - 1)
            builder.append("or "_s);

        builder.append(choice);
        ++index;
    }

    return builder.toString();
}

std::expected<JSValueRef, WebExtensionValueError> readDictionaryMemberValue(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    RefPtr jsKey = toJSString(String { key });

    JSValueRef exception = nullptr;
    JSValueRef value = JSObjectGetProperty(context, object, jsKey.get(), &exception);
    if (exception)
        return makeUnexpected(WebExtensionValueError { makeString("'"_s, joinValuePath(parentPath, String { key }), "' could not be read"_s) });

    if (JSValueIsUndefined(context, value) || JSValueIsNull(context, value))
        return nullptr;

    return value;
}

static WebExtensionValueError typeMismatch(JSContextRef context, JSValueRef value, const String& path, const String& expected)
{
    return WebExtensionValueError { constructExpectedMessage(path, expected, describeValueType(context, value)) };
}

std::expected<String, WebExtensionValueError> WebExtensionValueParser<String>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    if (!matches(context, value))
        return makeUnexpected(typeMismatch(context, value, path, description()));

    return toString(context, value);
}

std::expected<double, WebExtensionValueError> WebExtensionValueParser<double>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    if (!matches(context, value))
        return makeUnexpected(typeMismatch(context, value, path, description()));

    return JSValueToNumber(context, value, nullptr);
}

std::expected<bool, WebExtensionValueError> WebExtensionValueParser<bool>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    if (!matches(context, value))
        return makeUnexpected(typeMismatch(context, value, path, description()));

    return JSValueToBoolean(context, value);
}

// Integers are numbers that must also be whole and in range.
template<typename Integer>
static std::expected<Integer, WebExtensionValueError> parseInteger(JSContextRef context, JSValueRef value, const String& path)
{
    auto number = WebExtensionValueParser<double>::parse(context, value, path);
    if (!number)
        return makeUnexpected(WTF::move(number.error()));

    double ignored;
    if (std::modf(*number, &ignored) != 0.0 || *number < static_cast<double>(std::numeric_limits<Integer>::min()) || *number > static_cast<double>(std::numeric_limits<Integer>::max()))
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must be an integer"_s });

    return static_cast<Integer>(*number);
}

std::expected<int32_t, WebExtensionValueError> WebExtensionValueParser<int32_t>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    return parseInteger<int32_t>(context, value, path);
}

std::expected<uint32_t, WebExtensionValueError> WebExtensionValueParser<uint32_t>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    return parseInteger<uint32_t>(context, value, path);
}

std::expected<Protected<JSValueRef>, WebExtensionValueError> WebExtensionValueParser<Protected<JSValueRef>>::parse(JSContextRef context, JSValueRef value, const String&)
{
    return Protected<JSValueRef> { JSContextGetGlobalContext(context), value };
}

std::expected<std::optional<Protected<JSValueRef>>, WebExtensionValueError> readDictionaryFunctionMember(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    auto value = readDictionaryMemberValue(context, object, key, parentPath);
    if (!value)
        return makeUnexpected(WTF::move(value.error()));

    if (!*value)
        return std::nullopt;

    if (!isFunction(context, *value))
        return makeUnexpected(typeMismatch(context, *value, joinValuePath(parentPath, String { key }), "a function"_s));

    return std::optional<Protected<JSValueRef>> { Protected<JSValueRef> { JSContextGetGlobalContext(context), *value } };
}

static String normalizedEnumValue(const String& value)
{
    return makeStringByReplacingAll(value.convertToASCIILowercase(), '-', '_');
}

std::expected<size_t, WebExtensionValueError> parseWebExtensionEnumValue(JSContextRef context, JSValueRef value, const String& path, std::span<const ASCIILiteral> values)
{
    if (!JSValueIsString(context, value))
        return makeUnexpected(typeMismatch(context, value, path, "a string"_s));

    auto normalizedInput = normalizedEnumValue(toString(context, value));
    for (size_t i = 0; i < values.size(); ++i) {
        if (normalizedInput == normalizedEnumValue(String { values[i] }))
            return i;
    }

    Vector<String> quotedValues;
    for (auto value : values)
        quotedValues.append(makeString('\'', value, '\''));

    return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it must specify either "_s, joinValueChoices(quotedValues)) });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
