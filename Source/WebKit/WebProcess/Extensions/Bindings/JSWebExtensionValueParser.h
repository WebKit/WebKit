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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionWrapper.h"
#include "Protected.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionMatchPattern.h"
#include "WebExtensionTabIdentifier.h"
#include "WebExtensionUtilities.h"
#include "WebExtensionWindowIdentifier.h"
#include <optional>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/URL.h>
#include <wtf/UUID.h>
#include <wtf/Variant.h>

// Parsing of the JavaScript values that the IDL declares into C++ values, for arguments, dictionary members, sequence
// elements and union alternatives. Only generated code includes this.

namespace WebKit {

using WTF::UUID;

// Describes a value for error messages, for example "a string", "an array", "null" or "NaN".
String describeValueType(JSContextRef, JSValueRef);

// An error from parsing a value. The key replaces the argument name as the error source when it is not null,
// which semantic errors use to name the member.
struct WebExtensionValueError {
    WebExtensionValueError(const String& message)
        : message(message)
    {
    }

    WebExtensionValueError(const String& key, const String& message)
        : key(key)
        , message(message)
    {
    }

    String key;
    String message;
};

// Returns an error for a failed parse. The error names the argument unless it has a key of its own.
JSObjectRef toJSError(JSContextRef, const String& callingAPIName, const String& argumentName, const WebExtensionValueError&);

// Joins a parent path and a member name, or an array index like "[0]", into a path like "target.frameIds[0]".
String joinValuePath(const String& parentPath, const String& key);

// The last member name in a path, ignoring array indexes, or null when there is none.
String valuePathLeaf(const String& path);

// Joins descriptions as "a", "a or b", or "a, b, or c".
String joinValueChoices(const Vector<String>&);

// Reads a member. The value is nullptr when the member is undefined or JavaScript null, which dictionaries treat as missing.
std::expected<JSValueRef, WebExtensionValueError> readDictionaryMemberValue(JSContextRef, JSObjectRef, ASCIILiteral key, const String& parentPath);

// Reads a member. The value is nullptr only when the member is undefined, so a nullable member can tell JavaScript null apart.
std::expected<JSValueRef, WebExtensionValueError> readDictionaryRawMemberValue(JSContextRef, JSObjectRef, ASCIILiteral key, const String& parentPath);

// Converts a JavaScript value to T, for dictionary members, arguments, sequence elements and union alternatives.
// Generated code defines this for dictionaries and enumerations, and a custom IDL type defines it by hand:
//
//   template<> struct WebExtensionValueParser<Foo> : WebExtensionNumberParserBase {
//       static std::expected<Foo, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
//   };
//
// The description is for errors, like "a number". Matches is a cheap check of the JavaScript type that a union uses
// to pick an alternative. The path names the value in errors, and is empty for an argument.
template<typename T> struct WebExtensionValueParser;

struct WebExtensionValueParserBase {
    static String description() { return "a value"_s; }
    static String pluralDescription() { return "values"_s; }
    static bool matches(JSContextRef, JSValueRef) { return true; }
};

struct WebExtensionNumberParserBase : WebExtensionValueParserBase {
    static String description() { return "a number"_s; }
    static String pluralDescription() { return "numbers"_s; }
    static bool matches(JSContextRef context, JSValueRef value) { return JSValueIsNumber(context, value); }
};

struct WebExtensionStringParserBase : WebExtensionValueParserBase {
    static String description() { return "a string"_s; }
    static String pluralDescription() { return "strings"_s; }
    static bool matches(JSContextRef context, JSValueRef value) { return JSValueIsString(context, value); }
};

struct WebExtensionObjectParserBase : WebExtensionValueParserBase {
    static String description() { return "an object"_s; }
    static String pluralDescription() { return "objects"_s; }
    static bool matches(JSContextRef context, JSValueRef value) { return isDictionary(context, value); }
};

template<> struct WebExtensionValueParser<String> : WebExtensionStringParserBase {
    static std::expected<String, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

template<> struct WebExtensionValueParser<double> : WebExtensionNumberParserBase {
    static bool matches(JSContextRef context, JSValueRef value) { return JSValueIsNumber(context, value) && std::isfinite(JSValueToNumber(context, value, nullptr)); }
    static std::expected<double, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

template<> struct WebExtensionValueParser<int32_t> : WebExtensionNumberParserBase {
    static std::expected<int32_t, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

template<> struct WebExtensionValueParser<uint32_t> : WebExtensionNumberParserBase {
    static std::expected<uint32_t, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

template<> struct WebExtensionValueParser<bool> : WebExtensionValueParserBase {
    static String description() { return "a boolean"_s; }
    static String pluralDescription() { return "booleans"_s; }
    static bool matches(JSContextRef context, JSValueRef value) { return JSValueIsBoolean(context, value); }
    static std::expected<bool, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// IDL `any`. The value is kept alive so it can be used after parsing.
template<> struct WebExtensionValueParser<Protected<JSValueRef>> : WebExtensionValueParserBase {
    static String pluralDescription() { return nullString(); }
    static std::expected<Protected<JSValueRef>, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// A value converted to JSON, for members that are stored or sent as JSON, such as declarativeNetRequest rules.
// The IDL names these with `[CustomType=WebExtensionJSONValue] any` or `[CustomType=WebExtensionJSONObject] any`,
// and a sequence of them with `sequence<any>`.
using WebExtensionJSONValue = Ref<JSON::Value>;
using WebExtensionJSONObject = Ref<JSON::Object>;

template<> struct WebExtensionValueParser<Ref<JSON::Value>> : WebExtensionValueParserBase {
    static String pluralDescription() { return nullString(); }
    static std::expected<Ref<JSON::Value>, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

template<> struct WebExtensionValueParser<Ref<JSON::Object>> : WebExtensionObjectParserBase {
    static std::expected<Ref<JSON::Object>, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// IDL `record<DOMString, any>`.
template<> struct WebExtensionValueParser<HashMap<String, Protected<JSValueRef>>> : WebExtensionObjectParserBase {
    static std::expected<HashMap<String, Protected<JSValueRef>>, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// The names and values of an object's own properties, which a record parses from. A value that isn't an object is an error.
std::expected<Vector<std::pair<String, Protected<JSValueRef>>>, WebExtensionValueError> readRecordEntries(JSContextRef, JSValueRef, const String& path);

// IDL `record<DOMString, T>` for any other T.
template<typename T> struct WebExtensionValueParser<HashMap<String, T>> : WebExtensionObjectParserBase {
    static std::expected<HashMap<String, T>, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path)
    {
        auto entries = readRecordEntries(context, value, path);
        if (!entries)
            return makeUnexpected(WTF::move(entries.error()));

        HashMap<String, T> result;
        for (auto& [key, entry] : *entries) {
            auto parsed = WebExtensionValueParser<T>::parse(context, entry.get(), joinValuePath(path, String { key }));
            if (!parsed)
                return makeUnexpected(WTF::move(parsed.error()));

            result.set(key, WTF::move(*parsed));
        }

        return result;
    }
};

// IDL `sequence<T>`.
template<typename T> struct WebExtensionValueParser<Vector<T>> {
    static String description()
    {
        auto elements = WebExtensionValueParser<T>::pluralDescription();
        return elements.isEmpty() ? "an array"_s : makeString("an array of "_s, elements);
    }

    static String pluralDescription() { return "arrays"_s; }
    static bool matches(JSContextRef context, JSValueRef value) { return JSValueIsArray(context, value); }

    static std::expected<Vector<T>, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path)
    {
        if (!JSValueIsArray(context, value))
            return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, description(), describeValueType(context, value)) });

        auto array = JSValueToObject(context, value, nullptr);
        RefPtr lengthKey = toJSString("length"_s);
        auto length = JSValueToUInt32(context, JSObjectGetProperty(context, array, lengthKey.get(), nullptr), nullptr);

        Vector<T> elements;
        if (!elements.tryReserveInitialCapacity(length))
            return makeUnexpected(WebExtensionValueError { "it is too large"_s });

        for (unsigned i = 0; i < length; ++i) {
            auto element = JSObjectGetPropertyAtIndex(context, array, i, nullptr);
            if (!WebExtensionValueParser<T>::matches(context, element))
                return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, description(), describeValueType(context, element), true) });

            auto parsed = WebExtensionValueParser<T>::parse(context, element, joinValuePath(path, makeString('[', i, ']')));
            if (!parsed)
                return makeUnexpected(WTF::move(parsed.error()));

            elements.append(WTF::move(*parsed));
        }

        return elements;
    }
};

// IDL union types. The first alternative that matches the JavaScript value parses it.
template<typename... Alternatives> struct WebExtensionValueParser<Variant<Alternatives...>> {
    using Result = Variant<Alternatives...>;

    static String description() { return joinValueChoices({ WebExtensionValueParser<Alternatives>::description()... }); }
    static String pluralDescription()
    {
        // Elements of a union are described by what each alternative is made of, unless one of them has no description.
        Vector<String> choices { WebExtensionValueParser<Alternatives>::pluralDescription()... };
        if (choices.containsIf([](auto& choice) { return choice.isEmpty(); }))
            return "values"_s;

        return joinValueChoices(choices);
    }

    static bool matches(JSContextRef context, JSValueRef value) { return (WebExtensionValueParser<Alternatives>::matches(context, value) || ...); }

    static std::expected<Result, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path)
    {
        std::optional<std::expected<Result, WebExtensionValueError>> result;

        (tryAlternative<Alternatives>(context, value, path, result), ...);

        if (result)
            return WTF::move(*result);

        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, description(), describeValueType(context, value)) });
    }

private:
    template<typename Alternative>
    static void tryAlternative(JSContextRef context, JSValueRef value, const String& path, std::optional<std::expected<Result, WebExtensionValueError>>& result)
    {
        if (result || !WebExtensionValueParser<Alternative>::matches(context, value))
            return;

        auto parsed = WebExtensionValueParser<Alternative>::parse(context, value, path);
        if (!parsed)
            result = makeUnexpected(WTF::move(parsed.error()));
        else
            result = Result { WTF::InPlaceType<Alternative>, WTF::move(*parsed) };
    }
};

// Reads a member as T. The result is empty when the member is undefined or null.
template<typename T>
std::expected<std::optional<T>, WebExtensionValueError> readDictionaryMember(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    auto value = readDictionaryMemberValue(context, object, key, parentPath);
    if (!value)
        return makeUnexpected(WTF::move(value.error()));

    if (!*value)
        return std::nullopt;

    auto parsed = WebExtensionValueParser<T>::parse(context, *value, joinValuePath(parentPath, String { key }));
    if (!parsed)
        return makeUnexpected(WTF::move(parsed.error()));

    return std::optional<T> { WTF::move(*parsed) };
}

// Reads a member as T, where null is a value for the parser and not the same as undefined. The result is empty only when the
// member is undefined.
template<typename T>
std::expected<std::optional<T>, WebExtensionValueError> readDictionaryMemberKeepingNull(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    auto value = readDictionaryRawMemberValue(context, object, key, parentPath);
    if (!value)
        return makeUnexpected(WTF::move(value.error()));

    if (!*value)
        return std::nullopt;

    auto parsed = WebExtensionValueParser<T>::parse(context, *value, joinValuePath(parentPath, String { key }));
    if (!parsed)
        return makeUnexpected(WTF::move(parsed.error()));

    return std::optional<T> { WTF::move(*parsed) };
}

// Whether a string, a sequence, or a union of them has nothing in it. CannotBeEmpty checks this on a parsed argument.
template<typename T>
bool webExtensionValueIsEmpty(const T& value)
{
    return value.isEmpty();
}

template<typename... Alternatives>
bool webExtensionValueIsEmpty(const Variant<Alternatives...>& value)
{
    return WTF::visit([](const auto& alternative) {
        return webExtensionValueIsEmpty(alternative);
    }, value);
}

// Whether a sequence of strings, or a union that holds one, has an empty string in it.
inline bool webExtensionValueHasEmptyString(const String&)
{
    return false;
}

inline bool webExtensionValueHasEmptyString(const Vector<String>& values)
{
    return values.containsIf([](const String& value) {
        return value.isEmpty();
    });
}

template<typename... Alternatives>
bool webExtensionValueHasEmptyString(const Variant<Alternatives...>& value)
{
    return WTF::visit([](const auto& alternative) {
        return webExtensionValueHasEmptyString(alternative);
    }, value);
}

// Makes a type error from parsing a nullable member read "is expected to be a number or null".
WebExtensionValueError addNullToExpectedType(WebExtensionValueError&&, const String& expected);

// Reads a nullable member as T. The outer optional is empty when the member is undefined, and the inner one when it is null.
template<typename T>
std::expected<std::optional<std::optional<T>>, WebExtensionValueError> readDictionaryNullableMember(JSContextRef context, JSObjectRef object, ASCIILiteral key, const String& parentPath)
{
    auto value = readDictionaryRawMemberValue(context, object, key, parentPath);
    if (!value)
        return makeUnexpected(WTF::move(value.error()));

    if (!*value)
        return std::nullopt;

    if (JSValueIsNull(context, *value))
        return std::optional<std::optional<T>> { std::optional<T> { } };

    auto path = joinValuePath(parentPath, String { key });
    auto parsed = WebExtensionValueParser<T>::parse(context, *value, path);
    if (!parsed)
        return makeUnexpected(addNullToExpectedType(WTF::move(parsed.error()), WebExtensionValueParser<T>::description()));

    return std::optional<std::optional<T>> { std::optional<T> { WTF::move(*parsed) } };
}

// Reads a member that must be a function.
std::expected<std::optional<Protected<JSValueRef>>, WebExtensionValueError> readDictionaryFunctionMember(JSContextRef, JSObjectRef, ASCIILiteral key, const String& parentPath);

// Returns the index of the enumeration value that matches. Matching ignores ASCII case and treats '-' and '_' as the
// same, and the values are lowercase.
std::expected<size_t, WebExtensionValueError> parseWebExtensionEnumValue(JSContextRef, JSValueRef, const String& path, std::span<const ASCIILiteral> values);

// Parses an optional argument, which is empty when it is null or undefined.
template<typename T>
std::expected<std::optional<T>, WebExtensionValueError> parseOptionalWebExtensionValue(JSContextRef context, JSValueRef value)
{
    if (JSValueIsNull(context, value) || JSValueIsUndefined(context, value))
        return std::optional<T> { };

    auto parsed = WebExtensionValueParser<T>::parse(context, value, { });
    if (!parsed)
        return makeUnexpected(WTF::move(parsed.error()));

    return std::optional<T> { WTF::move(*parsed) };
}

// Identifiers, which convert from numbers or strings and name the member in their errors. A tab identifier is a real tab,
// so -1 (tabs.TAB_ID_NONE) is not valid here.
template<> struct WebExtensionValueParser<WebExtensionTabIdentifier> : WebExtensionNumberParserBase {
    static std::expected<WebExtensionTabIdentifier, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// A cookie store is a session, which extensions name by a string like "persistent-1" or "ephemeral-2".
using WebExtensionCookieStoreIdentifier = PAL::SessionID;

template<> struct WebExtensionValueParser<WebExtensionCookieStoreIdentifier> : WebExtensionStringParserBase {
    static std::expected<WebExtensionCookieStoreIdentifier, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// A URL that parses. An empty string is an empty URL, so a member or an argument that needs one adds CannotBeEmpty.
template<> struct WebExtensionValueParser<URL> : WebExtensionStringParserBase {
    static std::expected<URL, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

template<> struct WebExtensionValueParser<WebExtensionFrameIdentifier> : WebExtensionNumberParserBase {
    static std::expected<WebExtensionFrameIdentifier, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// A window identifier is a real window or the current window, so -1 (windows.WINDOW_ID_NONE) is not valid here.
template<> struct WebExtensionValueParser<WebExtensionWindowIdentifier> : WebExtensionNumberParserBase {
    static std::expected<WebExtensionWindowIdentifier, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// A UUID in its string form, such as a document identifier.
template<> struct WebExtensionValueParser<UUID> : WebExtensionStringParserBase {
    static std::expected<UUID, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

// A match pattern string that the extension API supports. The IDL names this with `[CustomType=WebExtensionMatchPattern]`, and
// the implementation receives the pattern.
template<> struct WebExtensionValueParser<Ref<WebExtensionMatchPattern>> : WebExtensionStringParserBase {
    static std::expected<Ref<WebExtensionMatchPattern>, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
