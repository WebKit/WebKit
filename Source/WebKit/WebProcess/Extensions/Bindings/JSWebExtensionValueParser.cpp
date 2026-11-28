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

static std::expected<JSValueRef, WebExtensionValueError> readMemberValue(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath, bool treatNullAsMissing)
{
    RefPtr jsKey = toJSString(String { key });

    JSValueRef exception = nullptr;
    JSValueRef value = JSObjectGetProperty(context, object, jsKey.get(), &exception);
    if (exception)
        return makeUnexpected(WebExtensionValueError { makeString("'"_s, joinValuePath(parentPath, String { key }), "' could not be read"_s) });

    if (JSValueIsUndefined(context, value) || (treatNullAsMissing && JSValueIsNull(context, value)))
        return nullptr;

    return value;
}

std::expected<JSValueRef, WebExtensionValueError> readDictionaryMemberValue(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    return readMemberValue(context, object, key, parentPath, true);
}

std::expected<JSValueRef, WebExtensionValueError> readDictionaryRawMemberValue(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    return readMemberValue(context, object, key, parentPath, false);
}

static WebExtensionValueError typeMismatch(JSContextRef context, JSValueRef value, const String& path, const String& expected)
{
    return WebExtensionValueError { constructExpectedMessage(path, expected, describeValueType(context, value)) };
}

WebExtensionValueError addNullToExpectedType(WebExtensionValueError&& error, const String& expected)
{
    error.message = makeStringByReplacingAll(error.message, makeString("be "_s, expected, ", but"_s), makeString("be "_s, expected, " or null, but"_s));
    return WTF::move(error);
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

std::expected<Ref<JSON::Value>, WebExtensionValueError> WebExtensionValueParser<Ref<JSON::Value>>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    // toJSONValue only converts objects, and fromJSValue converts every kind of value.
    RefPtr jsonValue = fromJSValue(context, value, NullValuePolicy::Allowed);
    if (!jsonValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it is not a value that JSON can represent"_s });

    return jsonValue.releaseNonNull();
}

std::expected<Ref<JSON::Object>, WebExtensionValueError> WebExtensionValueParser<Ref<JSON::Object>>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    if (!matches(context, value))
        return makeUnexpected(typeMismatch(context, value, path, description()));

    RefPtr jsonValue = toJSONValue(context, value, NullValuePolicy::Allowed);
    RefPtr object = jsonValue ? jsonValue->asObject() : nullptr;
    if (!object)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it is not an object that JSON can represent"_s });

    return object.releaseNonNull();
}

std::expected<HashMap<String, Protected<JSValueRef>>, WebExtensionValueError> WebExtensionValueParser<HashMap<String, Protected<JSValueRef>>>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    if (!matches(context, value))
        return makeUnexpected(typeMismatch(context, value, path, description()));

    auto object = JSValueToObject(context, value, nullptr);
    auto globalContext = JSContextGetGlobalContext(context);
    auto propertyNames = JSObjectCopyPropertyNames(context, object);
    auto releasePropertyNames = makeScopeExit([&] {
        JSPropertyNameArrayRelease(propertyNames);
    });

    HashMap<String, Protected<JSValueRef>> result;
    for (size_t i = 0; i < JSPropertyNameArrayGetCount(propertyNames); ++i) {
        RefPtr propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        auto key = toString(propertyName.get());

        JSValueRef exception = nullptr;
        JSValueRef entry = JSObjectGetProperty(context, object, propertyName.get(), &exception);
        if (exception || !entry)
            return makeUnexpected(WebExtensionValueError { makeString("'"_s, joinValuePath(path, key), "' could not be read"_s) });

        result.set(WTF::move(key), Protected<JSValueRef> { globalContext, entry });
    }

    return result;
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

std::expected<WebExtensionTabIdentifier, WebExtensionValueError> WebExtensionValueParser<WebExtensionTabIdentifier>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto number = WebExtensionValueParser<double>::parse(context, value, path);
    if (!number)
        return makeUnexpected(WTF::move(number.error()));

    auto identifier = toWebExtensionTabIdentifier(*number);
    if (isNone(identifier))
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "'tabs.TAB_ID_NONE' is not allowed"_s });

    if (!isValid(identifier))
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', String::number(*number), "' is not a tab identifier"_s) });

    return *identifier;
}

std::expected<WebExtensionCookieStoreIdentifier, WebExtensionValueError> WebExtensionValueParser<WebExtensionCookieStoreIdentifier>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto string = WebExtensionValueParser<String>::parse(context, value, path);
    if (!string)
        return makeUnexpected(WTF::move(string.error()));

    if (string->isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must not be empty"_s });

    auto invalid = [&] {
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', *string, "' is not a valid cookie store identifier"_s) });
    };

    uint64_t sessionMask = 0;
    StringView remainder = *string;

    if (remainder.startsWith("ephemeral-"_s)) {
        sessionMask = PAL::SessionID::EphemeralSessionMask;
        remainder = remainder.substring(10);
    } else if (remainder.startsWith("persistent-"_s))
        remainder = remainder.substring(11);
    else
        return invalid();

    auto number = parseInteger<uint64_t>(remainder);
    if (!number)
        return invalid();

    PAL::SessionID sessionID { *number | sessionMask };
    if (!sessionID.isValid())
        return invalid();

    return sessionID;
}

std::expected<URL, WebExtensionValueError> WebExtensionValueParser<URL>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto string = WebExtensionValueParser<String>::parse(context, value, path);
    if (!string)
        return makeUnexpected(WTF::move(string.error()));

    if (string->isEmpty())
        return URL { };

    URL url { *string };
    if (!url.isValid())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', *string, "' is not a valid URL"_s) });

    return url;
}

std::expected<WebExtensionFrameIdentifier, WebExtensionValueError> WebExtensionValueParser<WebExtensionFrameIdentifier>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto number = WebExtensionValueParser<double>::parse(context, value, path);
    if (!number)
        return makeUnexpected(WTF::move(number.error()));

    auto identifier = toWebExtensionFrameIdentifier(*number);
    if (!isValid(identifier))
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', String::number(*number), "' is not a frame identifier"_s) });

    return *identifier;
}

std::expected<WebExtensionWindowIdentifier, WebExtensionValueError> WebExtensionValueParser<WebExtensionWindowIdentifier>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto number = WebExtensionValueParser<double>::parse(context, value, path);
    if (!number)
        return makeUnexpected(WTF::move(number.error()));

    auto identifier = toWebExtensionWindowIdentifier(*number);
    if (isNone(identifier))
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "'windows.WINDOW_ID_NONE' is not allowed"_s });

    if (!isValid(identifier))
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', String::number(*number), "' is not a window identifier"_s) });

    return *identifier;
}

std::expected<UUID, WebExtensionValueError> WebExtensionValueParser<UUID>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto string = WebExtensionValueParser<String>::parse(context, value, path);
    if (!string)
        return makeUnexpected(WTF::move(string.error()));

    auto identifier = UUID::parse(*string);
    if (!identifier)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', *string, "' is not a valid UUID"_s) });

    return *identifier;
}

std::expected<Vector<std::pair<String, Protected<JSValueRef>>>, WebExtensionValueError> readRecordEntries(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(typeMismatch(context, value, path, "an object"_s));

    auto object = JSValueToObject(context, value, nullptr);
    auto globalContext = JSContextGetGlobalContext(context);
    auto propertyNames = JSObjectCopyPropertyNames(context, object);
    auto releasePropertyNames = makeScopeExit([&] {
        JSPropertyNameArrayRelease(propertyNames);
    });

    Vector<std::pair<String, Protected<JSValueRef>>> entries;
    for (size_t i = 0; i < JSPropertyNameArrayGetCount(propertyNames); ++i) {
        RefPtr propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        auto key = toString(propertyName.get());

        JSValueRef exception = nullptr;
        JSValueRef entry = JSObjectGetProperty(context, object, propertyName.get(), &exception);
        if (exception || !entry)
            return makeUnexpected(WebExtensionValueError { makeString("'"_s, joinValuePath(path, key), "' could not be read"_s) });

        entries.append({ WTF::move(key), Protected<JSValueRef> { globalContext, entry } });
    }

    return entries;
}

std::expected<Ref<WebExtensionMatchPattern>, WebExtensionValueError> WebExtensionValueParser<Ref<WebExtensionMatchPattern>>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto string = WebExtensionValueParser<String>::parse(context, value, path);
    if (!string)
        return makeUnexpected(WTF::move(string.error()));

    RefPtr pattern = WebExtensionMatchPattern::getOrCreate(*string);
    if (!pattern || !pattern->isSupported())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString('\'', *string, "' is not a valid pattern"_s) });

    return pattern.releaseNonNull();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
