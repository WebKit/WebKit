/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TrustedType.h"

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "HTMLScriptElement.h"
#include "JSDOMExceptionHandling.h"
#include "LocalDOMWindow.h"
#include "Node.h"
#include "Text.h"
#include "TrustedTypePolicy.h"
#include "TrustedTypePolicyFactory.h"
#include "WindowOrWorkerGlobalScopeTrustedTypes.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCast.h>
#include <pal/text/TextEncoding.h>

namespace WebCore {
using namespace JSC;

struct TrustedTypeVisitor {
    String operator()(std::monostate)
    {
        return nullString();
    }
    String operator()(Exception)
    {
        return nullString();
    }
    String operator()(const Ref<TrustedHTML>& value)
    {
        return value->toString();
    }
    String operator()(const Ref<TrustedScript>& value)
    {
        return value->toString();
    }
    String operator()(const Ref<TrustedScriptURL>& value)
    {
        return value->toString();
    }
};

ASCIILiteral trustedTypeToString(TrustedType trustedType)
{
    switch (trustedType) {
    case TrustedType::TrustedHTML:
        return "TrustedHTML"_s;
    case TrustedType::TrustedScript:
        return "TrustedScript"_s;
    case TrustedType::TrustedScriptURL:
        return "TrustedScriptURL"_s;
    }

    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral trustedTypeToCallbackName(TrustedType trustedType)
{
    switch (trustedType) {
    case TrustedType::TrustedHTML:
        return "createHTML"_s;
    case TrustedType::TrustedScript:
        return "createScript"_s;
    case TrustedType::TrustedScriptURL:
        return "createScriptURL"_s;
    }

    ASSERT_NOT_REACHED();
    return { };
}

// https://w3c.github.io/trusted-types/dist/spec/#process-value-with-a-default-policy-algorithm
std::variant<std::monostate, Exception, Ref<TrustedHTML>, Ref<TrustedScript>, Ref<TrustedScriptURL>> processValueWithDefaultPolicy(ScriptExecutionContext& scriptExecutionContext, TrustedType expectedType, const String& input, const String& sink)
{
    RefPtr<TrustedTypePolicy> protectedPolicy;
    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
        if (RefPtr window = document->domWindow())
            protectedPolicy = WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(*window)->defaultPolicy();
    } else if (RefPtr workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext))
        protectedPolicy = WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(*workerGlobalScope)->defaultPolicy();

    if (!protectedPolicy)
        return std::monostate();

    VM& vm = scriptExecutionContext.vm();

    auto jsExpectedType = JSC::jsString(vm, String(trustedTypeToString(expectedType)));
    auto jsSink = JSC::jsString(vm, sink);
    FixedVector<JSC::Strong<JSC::Unknown>> arguments({ { vm, jsExpectedType }, { vm, jsSink } });
    auto policyValueHolder = protectedPolicy->getPolicyValue(expectedType, input, WTFMove(arguments), IfMissing::ReturnNull);
    if (policyValueHolder.hasException())
        return { policyValueHolder.releaseException() };

    auto policyValue = policyValueHolder.releaseReturnValue();
    if (policyValue.isNull())
        return std::monostate();

    switch (expectedType) {
    case TrustedType::TrustedHTML:
        return { TrustedHTML::create(policyValue) };
    case TrustedType::TrustedScript:
        return { TrustedScript::create(policyValue) };
    case TrustedType::TrustedScriptURL:
        return { TrustedScriptURL::create(policyValue) };
    }

    ASSERT_NOT_REACHED();
    return std::monostate();
}

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-string-algorithm
ExceptionOr<String> trustedTypeCompliantString(TrustedType expectedType, ScriptExecutionContext& scriptExecutionContext, const String& input, const String& sink)
{
    String stringValue(input);

    if (!scriptExecutionContext.settingsValues().trustedTypesEnabled)
        return stringValue;

    CheckedPtr contentSecurityPolicy = scriptExecutionContext.checkedContentSecurityPolicy();

    auto requireTrustedTypes = contentSecurityPolicy && contentSecurityPolicy->requireTrustedTypesForSinkGroup("script"_s);

    if (!requireTrustedTypes)
        return stringValue;

    auto convertedInput = processValueWithDefaultPolicy(scriptExecutionContext, expectedType, stringValue, sink);
    if (std::holds_alternative<Exception>(convertedInput))
        return WTFMove(std::get<Exception>(convertedInput));

    if (!std::holds_alternative<std::monostate>(convertedInput)) {
        stringValue = std::visit(TrustedTypeVisitor { }, convertedInput);
        if (stringValue.isNull())
            convertedInput = std::monostate();
    }

    if (std::holds_alternative<std::monostate>(convertedInput)) {
        auto allowMissingTrustedTypes = contentSecurityPolicy->allowMissingTrustedTypesForSinkGroup(trustedTypeToString(expectedType), sink, "script"_s, stringValue);

        if (!allowMissingTrustedTypes)
            return Exception { ExceptionCode::TypeError, makeString("This assignment requires a ", trustedTypeToString(expectedType)) };
    }

    return stringValue;
}

ExceptionOr<String> trustedTypeCompliantString(ScriptExecutionContext& scriptExecutionContext, std::variant<RefPtr<TrustedScript>, String>&& input, const String& sink)
{
    return WTF::switchOn(
        WTFMove(input),
        [&scriptExecutionContext, &sink](const String& string) -> ExceptionOr<String> {
            return trustedTypeCompliantString(TrustedType::TrustedScript, scriptExecutionContext, string, sink);
        },
        [](const RefPtr<TrustedScript>& script) -> ExceptionOr<String> {
            return script->toString();
        }
    );
}

ExceptionOr<String> trustedTypeCompliantString(ScriptExecutionContext& scriptExecutionContext, std::variant<RefPtr<TrustedScriptURL>, String>&& input, const String& sink)
{
    return WTF::switchOn(
        WTFMove(input),
        [&scriptExecutionContext, &sink](const String& string) -> ExceptionOr<String> {
            return trustedTypeCompliantString(TrustedType::TrustedScriptURL, scriptExecutionContext, string, sink);
        },
        [](const RefPtr<TrustedScriptURL>& scriptURL) -> ExceptionOr<String> {
            return scriptURL->toString();
        }
    );
}

// https://w3c.github.io/trusted-types/dist/spec/#require-trusted-types-for-pre-navigation-check
ExceptionOr<String> requireTrustedTypesForPreNavigationCheckPasses(ScriptExecutionContext& scriptExecutionContext, const String& urlString)
{
    auto sinkGroup = "script"_s;
    auto sink = "Location href"_s;
    auto expectedType = TrustedType::TrustedScript;

    auto contentSecurityPolicy = scriptExecutionContext.contentSecurityPolicy();

    auto requireTrustedTypes = contentSecurityPolicy && contentSecurityPolicy->requireTrustedTypesForSinkGroup(sinkGroup);

    if (!requireTrustedTypes || !scriptExecutionContext.settingsValues().trustedTypesEnabled)
        return String(urlString);

    const int javascriptSchemeLength = sizeof("javascript:") - 1;
    auto decodedURLString = PAL::decodeURLEscapeSequences(urlString);
    auto scriptSource = decodedURLString.substring(javascriptSchemeLength);

    auto convertedScriptSource = processValueWithDefaultPolicy(scriptExecutionContext, expectedType, scriptSource, sink);
    if (std::holds_alternative<Exception>(convertedScriptSource))
        return WTFMove(std::get<Exception>(convertedScriptSource));

    if (std::holds_alternative<std::monostate>(convertedScriptSource)) {
        auto allowMissingTrustedTypes = contentSecurityPolicy->allowMissingTrustedTypesForSinkGroup(trustedTypeToString(expectedType), sink, sinkGroup, scriptSource);

        if (!allowMissingTrustedTypes)
            return Exception { ExceptionCode::TypeError, makeString("This assignment requires a ", trustedTypeToString(expectedType)) };

        return String(urlString);
    }

    auto stringifiedConvertedScriptSource = std::visit(TrustedTypeVisitor { }, convertedScriptSource);

    auto newURL = URL(makeString("javascript:"_s, stringifiedConvertedScriptSource));
    return String(newURL.isValid()
        ? newURL.string()
        : nullString());
}

ExceptionOr<RefPtr<Text>> processNodeOrStringAsTrustedType(Ref<Document> document, RefPtr<Node> parent, std::variant<RefPtr<Node>, String, RefPtr<TrustedScript>> variant)
{
    RefPtr<Text> text;
    if (std::holds_alternative<String>(variant))
        text = Text::create(document, WTFMove(std::get<String>(variant)));
    else if (std::holds_alternative<RefPtr<Node>>(variant)) {
        if (RefPtr textNode = dynamicDowncast<Text>(std::get<RefPtr<Node>>(variant)))
            text = textNode;
    }

    if (text) {
        if (UNLIKELY(is<HTMLScriptElement>(parent))) {
            auto holder = trustedTypeCompliantString(TrustedType::TrustedScript, *document->scriptExecutionContext(), text->wholeText(), "HTMLScriptElement text"_s);
            if (holder.hasException())
                return holder.releaseException();

            text->replaceWholeText(holder.releaseReturnValue());
        }
    } else if (std::holds_alternative<RefPtr<TrustedScript>>(variant))
        text = Text::create(document, std::get<RefPtr<TrustedScript>>(variant)->toString());

    return text;
}

} // namespace WebCore
