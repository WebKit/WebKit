/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSDOMConvertStrings.h"

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "JSDOMGlobalObject.h"
#include "JSTrustedHTML.h"
#include "JSTrustedScript.h"
#include "JSTrustedScriptURL.h"
#include "ScriptExecutionContext.h"
#include "TrustedTypePolicyFactory.h"
#include "WindowOrWorkerGlobalScopeTrustedTypes.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCast.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>


namespace WebCore {
using namespace JSC;

struct TrustedTypeVisitor {
    String operator()(std::nullptr_t)
    {
        return nullString();
    }
    String operator()(Ref<TrustedHTML> value)
    {
        return value->toString();
    }
    String operator()(Ref<TrustedScript> value)
    {
        return value->toString();
    }
    String operator()(Ref<TrustedScriptURL> value)
    {
        return value->toString();
    }
};

String identifierToString(JSGlobalObject& lexicalGlobalObject, const Identifier& identifier)
{
    if (UNLIKELY(identifier.isSymbol())) {
        auto scope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
        throwTypeError(&lexicalGlobalObject, scope, SymbolCoercionError);
        return { };
    }

    return identifier.string();
}

static inline bool throwIfInvalidByteString(JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const String& string)
{
    if (UNLIKELY(!string.containsOnlyLatin1())) {
        throwTypeError(&lexicalGlobalObject, scope);
        return true;
    }
    return false;
}

String identifierToByteString(JSGlobalObject& lexicalGlobalObject, const Identifier& identifier)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = identifierToString(lexicalGlobalObject, identifier);
    RETURN_IF_EXCEPTION(scope, { });
    if (UNLIKELY(throwIfInvalidByteString(lexicalGlobalObject, scope, string)))
        return { };
    return string;
}

String valueToByteString(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toWTFString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (UNLIKELY(throwIfInvalidByteString(lexicalGlobalObject, scope, string)))
        return { };
    return string;
}

AtomString valueToByteAtomString(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toString(&lexicalGlobalObject)->toAtomString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (UNLIKELY(throwIfInvalidByteString(lexicalGlobalObject, scope, string.string())))
        return nullAtom();

    return string;
}

String identifierToUSVString(JSGlobalObject& lexicalGlobalObject, const Identifier& identifier)
{
    return replaceUnpairedSurrogatesWithReplacementCharacter(identifierToString(lexicalGlobalObject, identifier));
}

String valueToUSVString(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toWTFString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    return replaceUnpairedSurrogatesWithReplacementCharacter(WTFMove(string));
}

AtomString valueToUSVAtomString(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toString(&lexicalGlobalObject)->toAtomString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    return replaceUnpairedSurrogatesWithReplacementCharacter(WTFMove(string));
}

// https://w3c.github.io/trusted-types/dist/spec/#process-value-with-a-default-policy-algorithm
static inline std::variant<std::nullptr_t, Ref<TrustedHTML>, Ref<TrustedScript>, Ref<TrustedScriptURL>> processValueWithDefaultPolicy(ScriptExecutionContext& scriptExecutionContext, const String& expectedType, const String& input, const String& sink)
{
    VM& vm = scriptExecutionContext.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RefPtr<TrustedTypePolicy> protector = nullptr;
    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
        if (auto window = document->domWindow()) {
            auto trustedTypesFactory = WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(*window);
            protector = trustedTypesFactory->defaultPolicy();
        }
    } else if (RefPtr workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext)) {
        auto trustedTypesFactory = WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(*workerGlobalScope);
        protector = trustedTypesFactory->defaultPolicy();
    }

    if (!protector)
        return nullptr;

    auto jsExpectedType = JSC::jsString(vm, expectedType);
    JSC::Strong<JSC::Unknown> strongExpectedType(vm, jsExpectedType);
    auto jsSink = JSC::jsString(vm, sink);
    JSC::Strong<JSC::Unknown> strongSink(vm, jsSink);
    FixedVector<JSC::Strong<JSC::Unknown>> arguments({ strongExpectedType, strongSink });
    auto policyValueHolder = protector->getPolicyValue(expectedType, input, WTFMove(arguments));
    if (policyValueHolder.hasException()) {
        propagateException(*scriptExecutionContext.globalObject(), scope, policyValueHolder.releaseException());
        return nullptr;
    }

    auto policyValue = policyValueHolder.releaseReturnValue();
    if (policyValue.isNull())
        return nullptr;

    std::variant<std::nullptr_t, Ref<TrustedHTML>, Ref<TrustedScript>, Ref<TrustedScriptURL>> result;
    if (expectedType == "TrustedHTML"_s) {
        result.emplace<1>(TrustedHTML::create(policyValue));
        return result;
    }
    if (expectedType == "TrustedScript"_s) {
        result.emplace<2>(TrustedScript::create(policyValue));
        return result;
    }
    if (expectedType == "TrustedScriptURL"_s) {
        result.emplace<3>(TrustedScriptURL::create(policyValue));
        return result;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-string-algorithm
String getTrustedTypeCompliantString(const String& expectedType, JSGlobalObject& lexicalGlobalObject, JSValue value, const String& sink, ShouldConvertNullToEmptyString shouldConvertNullToEmptyString)
{
    VM& vm = lexicalGlobalObject.vm();

    if (expectedType == "TrustedHTML"_s) {
        if (auto* trustedHTML = JSTrustedHTML::toWrapped(vm, value))
            return trustedHTML->toString();
    } else if (expectedType == "TrustedScript"_s) {
        if (auto* trustedScript = JSTrustedScript::toWrapped(vm, value))
            return trustedScript->toString();
    } else if (expectedType == "TrustedScriptURL"_s) {
        if (auto* trustedScriptURL = JSTrustedScriptURL::toWrapped(vm, value))
            return trustedScriptURL->toString();
    } else {
        ASSERT_NOT_REACHED();
        return nullString();
    }
    auto scriptExecutionContext = jsDynamicCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->scriptExecutionContext();

    auto stringValue = expectedType == "TrustedScriptURL"_s
        ? valueToUSVString(lexicalGlobalObject, value)
        : valueToByteString(lexicalGlobalObject, value);

    stringValue = getTrustedTypeCompliantString(expectedType, *scriptExecutionContext, stringValue, sink);

    if (stringValue.isNull() && shouldConvertNullToEmptyString == ShouldConvertNullToEmptyString::Yes)
        return emptyString();

    return stringValue;
}

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-string-algorithm
String getTrustedTypeCompliantString(const String& expectedType, ScriptExecutionContext& scriptExecutionContext, const String& value, const String& sink)
{
    VM& vm = scriptExecutionContext.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto contentSecurityPolicy = scriptExecutionContext.contentSecurityPolicy();

    auto requireTrustedTypes = contentSecurityPolicy
        ? contentSecurityPolicy->requireTrustedTypesForSinkGroup("script"_s)
        : false;

    String stringValue(value);

    if (requireTrustedTypes) {
        std::variant<std::nullptr_t, Ref<TrustedHTML>, Ref<TrustedScript>, Ref<TrustedScriptURL>> convertedType = processValueWithDefaultPolicy(scriptExecutionContext, expectedType, stringValue, sink);
        RETURN_IF_EXCEPTION(scope, { });

        if (!std::holds_alternative<std::nullptr_t>(convertedType)) {
            auto dataString = std::visit(TrustedTypeVisitor { }, convertedType);
            stringValue = dataString;
        }

        // FIXME: Implement CSP reporting and enforcement.
    }

    return stringValue;
}

} // namespace WebCore
