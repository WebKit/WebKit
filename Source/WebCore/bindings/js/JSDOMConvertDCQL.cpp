/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "JSDOMConvertDCQL.h"

#include "JSDCQLMsoMdocMeta.h"
#include "JSDCQLSdJwtMeta.h"
#include "JSDOMConvertDictionary.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObject.h"
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <cmath>
#include <wtf/Box.h>
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace JSC;

static constexpr double maxSafeInteger = 9007199254740991.0;

static std::optional<String> stringFromValue(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value)
{
    if (!value.isString()) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }
    auto result = convert<IDLDOMString>(globalObject, value);
    if (result.hasException(scope)) [[unlikely]]
        return std::nullopt;
    return result.releaseReturnValue();
}

static std::optional<String> stringProperty(JSGlobalObject& globalObject, ThrowScope& scope, JSObject& object, ASCIILiteral key)
{
    auto value = object.get(&globalObject, Identifier::fromString(globalObject.vm(), key));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    return stringFromValue(globalObject, scope, value);
}

static std::optional<JSValue> property(JSGlobalObject& globalObject, ThrowScope& scope, JSObject& object, ASCIILiteral key)
{
    auto value = object.get(&globalObject, Identifier::fromString(globalObject.vm(), key));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    return value;
}

template<typename Element, typename ParseElement>
static std::optional<Vector<Element>> parseArray(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value, NOESCAPE ParseElement&& parseElement)
{
    if (!isJSArray(value)) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }
    auto& array = *asArray(value);
    unsigned length = array.length();
    Vector<Element> result;
    for (unsigned i = 0; i < length; ++i) {
        auto element = array.getIndex(&globalObject, i);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        auto parsed = parseElement(element);
        if (!parsed)
            return std::nullopt;
        result.append(WTF::move(*parsed));
    }
    return result;
}

static bool parseOptionalBoolean(JSGlobalObject& globalObject, ThrowScope& scope, JSObject& object, ASCIILiteral key, std::optional<bool>& result)
{
    auto value = property(globalObject, scope, object, key);
    if (!value)
        return false;
    if (value->isUndefinedOrNull())
        return true;
    if (!value->isBoolean()) {
        throwTypeError(&globalObject, scope);
        return false;
    }
    result = value->asBoolean();
    return true;
}

template<typename Container, typename ParseValue>
static bool parseOptionalMember(JSGlobalObject& globalObject, ThrowScope& scope, JSObject& object, ASCIILiteral key, Container& result, NOESCAPE ParseValue&& parseValue)
{
    auto value = property(globalObject, scope, object, key);
    if (!value)
        return false;
    if (value->isUndefinedOrNull())
        return true;
    auto parsed = parseValue(*value);
    if (!parsed)
        return false;
    result = WTF::move(*parsed);
    return true;
}

static std::optional<DCQLValue> jsToDCQLValue(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value, unsigned depth)
{
    if (!depth) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }

    if (value.isUndefinedOrNull())
        return DCQLValue { nullptr };
    if (value.isBoolean())
        return DCQLValue { value.asBoolean() };
    if (value.isNumber())
        return DCQLValue { value.asNumber() };
    if (value.isString()) {
        auto string = stringFromValue(globalObject, scope, value);
        if (!string)
            return std::nullopt;
        return DCQLValue { WTF::move(*string) };
    }

    VM& vm = globalObject.vm();
    if (isJSArray(value)) {
        auto elements = parseArray<Box<DCQLValue>>(globalObject, scope, value, [&](JSValue element) -> std::optional<Box<DCQLValue>> {
            auto parsed = jsToDCQLValue(globalObject, scope, element, depth - 1);
            if (!parsed)
                return std::nullopt;
            return Box<DCQLValue>::create(WTF::move(*parsed));
        });
        if (!elements)
            return std::nullopt;
        return DCQLValue { WTF::move(*elements) };
    }

    if (value.isObject()) {
        auto& object = *value.getObject();
        PropertyNameArrayBuilder propertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
        object.methodTable()->getOwnPropertyNames(&object, &globalObject, propertyNames, DontEnumPropertiesMode::Exclude);
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        HashMap<String, Box<DCQLValue>> map;
        for (auto& name : propertyNames) {
            auto propertyValue = object.get(&globalObject, name);
            RETURN_IF_EXCEPTION(scope, std::nullopt);
            auto parsed = jsToDCQLValue(globalObject, scope, propertyValue, depth - 1);
            if (!parsed)
                return std::nullopt;
            map.set(name.string(), Box<DCQLValue>::create(WTF::move(*parsed)));
        }
        return DCQLValue { WTF::move(map) };
    }

    // Symbols and other exotic values have no JSON representation.
    throwTypeError(&globalObject, scope);
    return std::nullopt;
}

static std::optional<DCQLMeta> parseMeta(JSGlobalObject& globalObject, ThrowScope& scope, JSObject& credential, const String& format)
{
    VM& vm = globalObject.vm();
    auto metaValue = credential.get(&globalObject, Identifier::fromString(vm, "meta"_s));
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    // meta is REQUIRED by OpenID4VP, though it may be an empty object.
    if (!metaValue.isObject()) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }

    if (format == "mso_mdoc"_s) {
        auto result = convertDictionary<DCQLMsoMdocMeta>(globalObject, metaValue);
        if (result.hasException(scope)) [[unlikely]]
            return std::nullopt;
        return DCQLMeta { result.releaseReturnValue() };
    }

    if (format == "dc+sd-jwt"_s) {
        auto result = convertDictionary<DCQLSdJwtMeta>(globalObject, metaValue);
        if (result.hasException(scope)) [[unlikely]]
            return std::nullopt;
        return DCQLMeta { result.releaseReturnValue() };
    }

    auto any = jsToDCQLValue(globalObject, scope, metaValue, JSON::Value::maxDepth);
    if (!any)
        return std::nullopt;
    return DCQLMeta { WTF::move(*any) };
}

static std::optional<DCQLClaimPathComponent> parsePathComponent(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value)
{
    if (value.isNull())
        return DCQLClaimPathComponent { nullptr };
    if (value.isString()) {
        auto string = stringFromValue(globalObject, scope, value);
        if (!string)
            return std::nullopt;
        return DCQLClaimPathComponent { WTF::move(*string) };
    }
    if (value.isNumber()) {
        double number = value.asNumber();
        if (!std::isfinite(number) || std::trunc(number) != number || number < 0 || number > maxSafeInteger) {
            throwTypeError(&globalObject, scope);
            return std::nullopt;
        }
        return DCQLClaimPathComponent { static_cast<uint64_t>(number) };
    }
    throwTypeError(&globalObject, scope);
    return std::nullopt;
}

static std::optional<DCQLClaimsQuery> parseClaimsQuery(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value)
{
    if (!value.isObject()) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }
    auto& claim = *value.getObject();
    VM& vm = globalObject.vm();
    DCQLClaimsQuery result;

    auto idValue = claim.get(&globalObject, Identifier::fromString(vm, "id"_s));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (!idValue.isUndefinedOrNull()) {
        auto id = stringFromValue(globalObject, scope, idValue);
        if (!id)
            return std::nullopt;
        result.id = WTF::move(*id);
    }

    auto pathValue = claim.get(&globalObject, Identifier::fromString(vm, "path"_s));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    auto path = parseArray<DCQLClaimPathComponent>(globalObject, scope, pathValue, [&](JSValue component) {
        return parsePathComponent(globalObject, scope, component);
    });
    if (!path)
        return std::nullopt;
    result.path = WTF::move(*path);
    return result;
}

static std::optional<Vector<Vector<String>>> parseIdentifierListArray(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value)
{
    return parseArray<Vector<String>>(globalObject, scope, value, [&](JSValue inner) {
        return parseArray<String>(globalObject, scope, inner, [&](JSValue element) {
            return stringFromValue(globalObject, scope, element);
        });
    });
}

static std::optional<DCQLCredentialQuery> parseCredentialQuery(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value)
{
    if (!value.isObject()) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }
    auto& credential = *value.getObject();
    DCQLCredentialQuery result;

    auto id = stringProperty(globalObject, scope, credential, "id"_s);
    if (!id)
        return std::nullopt;
    result.id = WTF::move(*id);

    auto format = stringProperty(globalObject, scope, credential, "format"_s);
    if (!format)
        return std::nullopt;
    result.format = WTF::move(*format);

    auto meta = parseMeta(globalObject, scope, credential, result.format);
    if (!meta)
        return std::nullopt;
    result.meta = WTF::move(*meta);

    if (!parseOptionalMember(globalObject, scope, credential, "claims"_s, result.claims, [&](JSValue claimsValue) {
        return parseArray<DCQLClaimsQuery>(globalObject, scope, claimsValue, [&](JSValue claimValue) {
            return parseClaimsQuery(globalObject, scope, claimValue);
        });
    }))
        return std::nullopt;

    if (!parseOptionalMember(globalObject, scope, credential, "claim_sets"_s, result.claimSets, [&](JSValue claimSetsValue) {
        return parseIdentifierListArray(globalObject, scope, claimSetsValue);
    }))
        return std::nullopt;

    return result;
}

static std::optional<DCQLCredentialSetQuery> parseCredentialSetQuery(JSGlobalObject& globalObject, ThrowScope& scope, JSValue value)
{
    if (!value.isObject()) {
        throwTypeError(&globalObject, scope);
        return std::nullopt;
    }
    auto& set = *value.getObject();
    VM& vm = globalObject.vm();
    DCQLCredentialSetQuery result;

    auto optionsValue = set.get(&globalObject, Identifier::fromString(vm, "options"_s));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    auto options = parseIdentifierListArray(globalObject, scope, optionsValue);
    if (!options)
        return std::nullopt;
    result.options = WTF::move(*options);

    if (!parseOptionalBoolean(globalObject, scope, set, "required"_s, result.required))
        return std::nullopt;

    return result;
}

ConversionResult<IDLDCQLQuery> Converter<IDLDCQLQuery>::convert(JSGlobalObject& globalObject, JSValue value)
{
    VM& vm = globalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* queryObject = value.getObject();
    if (!queryObject) {
        throwTypeError(&globalObject, scope);
        return ConversionResultException { };
    }

    DCQLQuery result;

    auto credentialsValue = queryObject->get(&globalObject, Identifier::fromString(vm, "credentials"_s));
    RETURN_IF_EXCEPTION(scope, ConversionResultException { });
    auto credentials = parseArray<DCQLCredentialQuery>(globalObject, scope, credentialsValue, [&](JSValue credentialValue) {
        return parseCredentialQuery(globalObject, scope, credentialValue);
    });
    if (!credentials)
        return ConversionResultException { };
    result.credentials = WTF::move(*credentials);

    if (!parseOptionalMember(globalObject, scope, *queryObject, "credential_sets"_s, result.credentialSets, [&](JSValue credentialSetsValue) {
        return parseArray<DCQLCredentialSetQuery>(globalObject, scope, credentialSetsValue, [&](JSValue setValue) {
            return parseCredentialSetQuery(globalObject, scope, setValue);
        });
    }))
        return ConversionResultException { };

    return result;
}

} // namespace WebCore
