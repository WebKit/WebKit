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
#include "EventNames.h"
#include "HTMLElement.h"
#include "JSDOMExceptionHandling.h"
#include "JSTrustedScript.h"
#include "LocalDOMWindow.h"
#include "TrustedTypePolicy.h"
#include "TrustedTypePolicyFactory.h"
#include "WindowOrWorkerGlobalScopeTrustedTypes.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ArgList.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <pal/text/TextEncoding.h>
#include <wtf/text/MakeString.h>

namespace WebCore {
using namespace JSC;

struct TrustedTypeVisitor {
    NODELETE RefPtr<StringImpl> operator()(std::monostate)
    {
        return nullString();
    }
    RefPtr<StringImpl> operator()(Exception)
    {
        return nullString();
    }
    RefPtr<StringImpl> operator()(const Ref<TrustedHTML>& value)
    {
        return value->toString();
    }
    RefPtr<StringImpl> operator()(const Ref<TrustedScript>& value)
    {
        return value->toString();
    }
    RefPtr<StringImpl> operator()(const Ref<TrustedScriptURL>& value)
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

TrustedType stringToTrustedType(RefPtr<StringImpl> str)
{
    if (equal(str, "TrustedHTML"_s))
        return TrustedType::TrustedHTML;
    if (equal(str, "TrustedScript"_s))
        return TrustedType::TrustedScript;
    if (equal(str, "TrustedScriptURL"_s))
        return TrustedType::TrustedScriptURL;

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
Variant<std::monostate, Exception, Ref<TrustedHTML>, Ref<TrustedScript>, Ref<TrustedScriptURL>> processValueWithDefaultPolicy(ScriptExecutionContext& scriptExecutionContext, TrustedType expectedType, StringImpl* input, StringImpl* sink)
{
    RefPtr<TrustedTypePolicy> protectedPolicy;
    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext)) {
        if (RefPtr window = document->window())
            protectedPolicy = WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(*window)->defaultPolicy();
    } else if (RefPtr workerGlobalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext))
        protectedPolicy = WindowOrWorkerGlobalScopeTrustedTypes::trustedTypes(*workerGlobalScope)->defaultPolicy();

    if (!protectedPolicy)
        return std::monostate();

    VM& vm = scriptExecutionContext.vm();
    JSC::JSLockHolder locker(vm);

    auto jsExpectedType = JSC::jsString(vm, StringImpl::create(trustedTypeToString(expectedType)));
    auto jsSink = JSC::jsString(vm, sink);
    FixedVector<JSC::Strong<JSC::Unknown>> arguments({ { vm, jsExpectedType }, { vm, jsSink } });
    auto policyValueHolder = protectedPolicy->getPolicyValue(expectedType, input, WTF::move(arguments), IfMissing::ReturnNull);
    if (policyValueHolder.hasException())
        return { policyValueHolder.releaseException() };

    auto policyValue = policyValueHolder.releaseReturnValue();
    if (!policyValue)
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
ExceptionOr<RefPtr<StringImpl>> trustedTypeCompliantString(TrustedType expectedType, ScriptExecutionContext& scriptExecutionContext, StringImpl* input, StringImpl* sink)
{
    RefPtr<StringImpl> stringValue(input);

    if (!scriptExecutionContext.settingsValues().trustedTypesEnabled)
        return stringValue;

    auto requireTrustedTypes = false;

    if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext))
        requireTrustedTypes = document->requiresTrustedTypes();
    else {
        CheckedPtr contentSecurityPolicy = scriptExecutionContext.contentSecurityPolicy();

        requireTrustedTypes = contentSecurityPolicy && contentSecurityPolicy->requireTrustedTypesForSinkGroup("script"_si);
    }

    if (!requireTrustedTypes)
        return stringValue;

    auto convertedInput = processValueWithDefaultPolicy(scriptExecutionContext, expectedType, stringValue, sink);
    if (std::holds_alternative<Exception>(convertedInput))
        return WTF::move(std::get<Exception>(convertedInput));

    if (!std::holds_alternative<std::monostate>(convertedInput)) {
        stringValue = WTF::visit(TrustedTypeVisitor { }, convertedInput);
        if (!stringValue)
            convertedInput = std::monostate();
    }

    if (std::holds_alternative<std::monostate>(convertedInput)) {
        auto allowMissingTrustedTypes = protect(scriptExecutionContext.contentSecurityPolicy())->allowMissingTrustedTypesForSinkGroup(StringImpl::create(trustedTypeToString(expectedType)).ptr(), sink, "script"_si, stringValue);

        if (!allowMissingTrustedTypes)
            return Exception { ExceptionCode::TypeError, makeString("This assignment requires a "_s, trustedTypeToString(expectedType)) };
    }

    return stringValue;
}

ExceptionOr<RefPtr<StringImpl>> trustedTypeCompliantString(ScriptExecutionContext& scriptExecutionContext, Variant<Ref<TrustedHTML>, RefPtr<StringImpl>>&& input, StringImpl* sink)
{
    return WTF::switchOn(WTF::move(input),
        [&scriptExecutionContext, &sink](StringImpl* string) -> ExceptionOr<RefPtr<StringImpl>> {
            return trustedTypeCompliantString(TrustedType::TrustedHTML, scriptExecutionContext, string, sink);
        },
        [](const Ref<TrustedHTML>& html) -> ExceptionOr<RefPtr<StringImpl>> {
            return html->toString();
        }
    );
}

ExceptionOr<RefPtr<StringImpl>> trustedTypeCompliantString(ScriptExecutionContext& scriptExecutionContext, Variant<Ref<TrustedScript>, RefPtr<StringImpl>>&& input, StringImpl* sink)
{
    return WTF::switchOn(WTF::move(input),
        [&scriptExecutionContext, &sink](StringImpl* string) -> ExceptionOr<RefPtr<StringImpl>> {
            return trustedTypeCompliantString(TrustedType::TrustedScript, scriptExecutionContext, string, sink);
        },
        [](const Ref<TrustedScript>& script) -> ExceptionOr<RefPtr<StringImpl>> {
            return script->toString();
        }
    );
}

ExceptionOr<RefPtr<StringImpl>> trustedTypeCompliantString(ScriptExecutionContext& scriptExecutionContext, Variant<Ref<TrustedScriptURL>, RefPtr<StringImpl>>&& input, StringImpl* sink)
{
    return WTF::switchOn(WTF::move(input),
        [&scriptExecutionContext, &sink](StringImpl* string) -> ExceptionOr<RefPtr<StringImpl>> {
            return trustedTypeCompliantString(TrustedType::TrustedScriptURL, scriptExecutionContext, string, sink);
        },
        [](const Ref<TrustedScriptURL>& scriptURL) -> ExceptionOr<RefPtr<StringImpl>> {
            return scriptURL->toString();
        }
    );
}

AttributeTypeAndSink trustedTypeForAttribute(StringImpl* elementName, StringImpl* attributeName, StringImpl* elementNamespace, StringImpl* attributeNamespace)
{
    // TrustedTypePolicyFactory is Exposed=(Window,Worker), so this runs on worker threads too. It
    // must therefore not use AtomStrings, QualifiedNames or the HTMLNames / SVGNames / MathMLNames
    // globals: atom string tables and qualified name caches are per-thread and compare by pointer
    // identity, and the generated name globals are main-thread-only. Compare by content instead.
    static constexpr auto xhtmlNamespace = "http://www.w3.org/1999/xhtml"_s;
    static constexpr auto svgNamespace = "http://www.w3.org/2000/svg"_s;
    static constexpr auto mathmlNamespace = "http://www.w3.org/1998/Math/MathML"_s;
    static constexpr auto xlinkNamespace = "http://www.w3.org/1999/xlink"_s;

    AttributeTypeAndSink returnValues;
    auto localName = convertToASCIILowercase(elementName);

    StringView elementNS = isEmpty(elementNamespace) ? xhtmlNamespace : StringView(elementNamespace);
    bool hasNoAttributeNamespace = isEmpty(attributeNamespace);

    if (hasNoAttributeNamespace && attributeName && (elementNS == xhtmlNamespace || elementNS == svgNamespace || elementNS == mathmlNamespace)) {
        if (startsWith(attributeName, "on"_s) && isEventHandlerAttributeName(attributeName)) {
            returnValues.sink = makeString("Element "_s, attributeName);
            returnValues.attributeType = trustedTypeToString(TrustedType::TrustedScript);
            return returnValues;
        }
    }

    if (elementNS == xhtmlNamespace && hasNoAttributeNamespace) {
        if (equal(localName, "iframe"_s) && equal(attributeName, "srcdoc"_s)) {
            returnValues.sink = "HTMLIFrameElement srcdoc"_si;
            returnValues.attributeType = trustedTypeToString(TrustedType::TrustedHTML);
        }
        if (equal(localName, "script"_s) && equal(attributeName, "src"_s)) {
            returnValues.sink = "HTMLScriptElement src"_si;
            returnValues.attributeType = trustedTypeToString(TrustedType::TrustedScriptURL);
        }
    }

    if (elementNS == svgNamespace && equal(localName, "script"_s) && equal(attributeName, "href"_s)
        && (hasNoAttributeNamespace || equal(attributeNamespace, xlinkNamespace))) {
        returnValues.sink = "SVGScriptElement href"_si;
        returnValues.attributeType = trustedTypeToString(TrustedType::TrustedScriptURL);
    }

    return returnValues;
}

// https://w3c.github.io/trusted-types/dist/spec/#require-trusted-types-for-pre-navigation-check
ExceptionOr<RefPtr<StringImpl>> requireTrustedTypesForPreNavigationCheckPasses(ScriptExecutionContext& scriptExecutionContext, StringImpl* urlString)
{
    auto sinkGroup = "script"_s;
    auto sink = "Location href"_s;
    auto expectedType = TrustedType::TrustedScript;

    CheckedPtr contentSecurityPolicy = scriptExecutionContext.contentSecurityPolicy();

    auto requireTrustedTypes = contentSecurityPolicy && contentSecurityPolicy->requireTrustedTypesForSinkGroup(StringImpl::create(sinkGroup).ptr());

    if (!requireTrustedTypes || !scriptExecutionContext.settingsValues().trustedTypesEnabled)
        return RefPtr<StringImpl>(urlString);

    const int javascriptSchemeLength = sizeof("javascript:") - 1;
    auto decodedURLString = PAL::decodeURLEscapeSequences(urlString);
    auto scriptSource = substring(decodedURLString, javascriptSchemeLength);

    VM& vm = scriptExecutionContext.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto convertedScriptSource = processValueWithDefaultPolicy(scriptExecutionContext, expectedType, scriptSource, StringImpl::create(sink).ptr());
    if (std::holds_alternative<Exception>(convertedScriptSource))
        TRY_CLEAR_EXCEPTION(throwScope, WTF::move(std::get<Exception>(convertedScriptSource)));
    else if (!std::holds_alternative<std::monostate>(convertedScriptSource)) {
        auto stringifiedConvertedScriptSource = WTF::visit(TrustedTypeVisitor { }, convertedScriptSource);

        auto newURL = URL(makeString("javascript:"_s, stringifiedConvertedScriptSource));

        if (newURL.isValid())
            return RefPtr<StringImpl>(newURL.string());
    }

    auto allowMissingTrustedTypes = contentSecurityPolicy->allowMissingTrustedTypesForSinkGroup(StringImpl::create(trustedTypeToString(expectedType)).ptr(), StringImpl::create(sink).ptr(), StringImpl::create(sinkGroup).ptr(), scriptSource);

    if (!allowMissingTrustedTypes)
        return Exception { ExceptionCode::TypeError, makeString("This assignment requires a "_s, trustedTypeToString(expectedType)) };

    return RefPtr<StringImpl>(urlString);
}

ExceptionOr<bool> canCompile(ScriptExecutionContext& scriptExecutionContext, JSC::CompilationType compilationType, RefPtr<StringImpl> codeString, const JSC::ArgList& args)
{
    if (compilationType == CompilationType::Function) {
        VM& vm = scriptExecutionContext.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        bool isTrusted = true;

        for (size_t i = 0; i < args.size(); i++) {
            auto arg = args.at(i);
            if (!arg.isObject()) {
                isTrusted = false;
                break;
            }
            if (RefPtr trustedScript = JSTrustedScript::toWrapped(vm, arg)) {
                auto argString = arg.toWTFString(scriptExecutionContext.globalObject());
                RETURN_IF_EXCEPTION(scope, Exception { ExceptionCode::ExistingExceptionError });
                if (!equal(trustedScript->toString(), argString)) {
                    isTrusted = false;
                    break;
                }
            } else {
                isTrusted = false;
                break;
            }
        }

        if (isTrusted)
            return true;
    }

    auto sink = compilationType == CompilationType::Function ? "Function"_s : "eval"_s;

    auto stringValueHolder = trustedTypeCompliantString(TrustedType::TrustedScript, scriptExecutionContext, codeString, StringImpl::create(sink).ptr());
    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    return equal(codeString, stringValueHolder.releaseReturnValue());
}

bool isEventHandlerAttribute(const QualifiedName& attributeName)
{
    ASSERT(attributeName.localName());

    // Event handler attributes have no namespace.
    if (attributeName.namespaceURI())
        return false;

    // Fast early return for names that don't start with "on".
    if (!startsWith(attributeName.localName(), "on"_s))
        return false;

    return isEventHandlerAttributeName(attributeName.localName());
}

ExceptionOr<RefPtr<AtomStringImpl>> trustedTypesCompliantAttributeValue(ScriptExecutionContext& scriptExecutionContext, StringImpl* attributeType, const TrustedTypeOrString& value, StringImpl* sink)
{
    auto stringValueHolder = WTF::switchOn(value,
        [&](StringImpl* string) -> ExceptionOr<RefPtr<StringImpl>> {
            if (!attributeType)
                return RefPtr<StringImpl>(string);
            return trustedTypeCompliantString(stringToTrustedType(attributeType), scriptExecutionContext, string, sink);
        },
        [&](const Ref<TrustedHTML>& trustedHTML) -> ExceptionOr<RefPtr<StringImpl>> {
            if (!attributeType || equal(attributeType, "TrustedHTML"_s))
                return trustedHTML->toString();
            return trustedTypeCompliantString(stringToTrustedType(attributeType), scriptExecutionContext, trustedHTML->toString(), sink);
        },
        [&](const Ref<TrustedScript>& trustedScript) -> ExceptionOr<RefPtr<StringImpl>> {
            if (!attributeType || equal(attributeType, "TrustedScript"_s))
                return trustedScript->toString();
            return trustedTypeCompliantString(stringToTrustedType(attributeType), scriptExecutionContext, trustedScript->toString(), sink);
        },
        [&](const Ref<TrustedScriptURL>& trustedScriptURL) -> ExceptionOr<RefPtr<StringImpl>> {
            if (!attributeType || equal(attributeType, "TrustedScriptURL"_s))
                return trustedScriptURL->toString();
            return trustedTypeCompliantString(stringToTrustedType(attributeType), scriptExecutionContext, trustedScriptURL->toString(), sink);
        }
    );
    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    return AtomStringImpl::add(stringValueHolder.releaseReturnValue());
}

} // namespace WebCore
