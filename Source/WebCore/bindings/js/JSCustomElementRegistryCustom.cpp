/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "JSCustomElementRegistry.h"

#include "CustomElementRegistry.h"
#include "Document.h"
#include "HTMLNames.h"
#include "JSCustomElementInterface.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertSequences.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/SetForScope.h>


namespace WebCore {
using namespace JSC;

static JSObject* getCustomElementCallback(ExecState& state, JSObject& prototype, const Identifier& id)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue callback = prototype.get(&state, id);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (callback.isUndefined())
        return nullptr;
    if (!callback.isFunction(vm)) {
        throwTypeError(&state, scope, "A custom element callback must be a function"_s);
        return nullptr;
    }
    return callback.getObject();
}

static bool validateCustomElementNameAndThrowIfNeeded(ExecState& state, const AtomicString& name)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());
    switch (Document::validateCustomElementName(name)) {
    case CustomElementNameValidationStatus::Valid:
        return true;
    case CustomElementNameValidationStatus::FirstCharacterIsNotLowercaseASCIILetter:
        throwDOMSyntaxError(state, scope, "Custom element name must have a lowercase ASCII letter as its first character"_s);
        return false;
    case CustomElementNameValidationStatus::ContainsUppercaseASCIILetter:
        throwDOMSyntaxError(state, scope, "Custom element name cannot contain an uppercase ASCII letter"_s);
        return false;
    case CustomElementNameValidationStatus::ContainsNoHyphen:
        throwDOMSyntaxError(state, scope, "Custom element name must contain a hyphen"_s);
        return false;
    case CustomElementNameValidationStatus::ContainsDisallowedCharacter:
        throwDOMSyntaxError(state, scope, "Custom element name contains a character that is not allowed"_s);
        return false;
    case CustomElementNameValidationStatus::ConflictsWithStandardElementName:
        throwDOMSyntaxError(state, scope, "Custom element name cannot be same as one of the standard elements"_s);
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// https://html.spec.whatwg.org/#dom-customelementregistry-define
JSValue JSCustomElementRegistry::define(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    AtomicString localName(state.uncheckedArgument(0).toString(&state)->toAtomicString(&state));
    RETURN_IF_EXCEPTION(scope, JSValue());

    JSValue constructorValue = state.uncheckedArgument(1);
    if (!constructorValue.isConstructor(vm))
        return throwTypeError(&state, scope, "The second argument must be a constructor"_s);
    JSObject* constructor = constructorValue.getObject();

    if (!validateCustomElementNameAndThrowIfNeeded(state, localName))
        return jsUndefined();

    CustomElementRegistry& registry = wrapped();

    if (registry.elementDefinitionIsRunning()) {
        throwNotSupportedError(state, scope, "Cannot define a custom element while defining another custom element"_s);
        return jsUndefined();
    }
    SetForScope<bool> change(registry.elementDefinitionIsRunning(), true);

    if (registry.findInterface(localName)) {
        throwNotSupportedError(state, scope, "Cannot define multiple custom elements with the same tag name"_s);
        return jsUndefined();
    }

    if (registry.containsConstructor(constructor)) {
        throwNotSupportedError(state, scope, "Cannot define multiple custom elements with the same class"_s);
        return jsUndefined();
    }

    JSValue prototypeValue = constructor->get(&state, vm.propertyNames->prototype);
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (!prototypeValue.isObject())
        return throwTypeError(&state, scope, "Custom element constructor's prototype must be an object"_s);
    JSObject& prototypeObject = *asObject(prototypeValue);

    QualifiedName name(nullAtom(), localName, HTMLNames::xhtmlNamespaceURI);
    auto elementInterface = JSCustomElementInterface::create(name, constructor, globalObject());

    auto* connectedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "connectedCallback"));
    if (connectedCallback)
        elementInterface->setConnectedCallback(connectedCallback);
    RETURN_IF_EXCEPTION(scope, JSValue());

    auto* disconnectedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "disconnectedCallback"));
    if (disconnectedCallback)
        elementInterface->setDisconnectedCallback(disconnectedCallback);
    RETURN_IF_EXCEPTION(scope, JSValue());

    auto* adoptedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "adoptedCallback"));
    if (adoptedCallback)
        elementInterface->setAdoptedCallback(adoptedCallback);
    RETURN_IF_EXCEPTION(scope, JSValue());

    auto* attributeChangedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "attributeChangedCallback"));
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (attributeChangedCallback) {
        auto observedAttributesValue = constructor->get(&state, Identifier::fromString(&state, "observedAttributes"));
        RETURN_IF_EXCEPTION(scope, JSValue());
        if (!observedAttributesValue.isUndefined()) {
            auto observedAttributes = convert<IDLSequence<IDLDOMString>>(state, observedAttributesValue);
            RETURN_IF_EXCEPTION(scope, JSValue());
            elementInterface->setAttributeChangedCallback(attributeChangedCallback, observedAttributes);
        }
    }

    auto addToGlobalObjectWithPrivateName = [&] (JSObject* objectToAdd) {
        if (objectToAdd) {
            PrivateName uniquePrivateName;
            globalObject()->putDirect(vm, uniquePrivateName, objectToAdd);
        }
    };

    addToGlobalObjectWithPrivateName(constructor);
    addToGlobalObjectWithPrivateName(connectedCallback);
    addToGlobalObjectWithPrivateName(disconnectedCallback);
    addToGlobalObjectWithPrivateName(adoptedCallback);
    addToGlobalObjectWithPrivateName(attributeChangedCallback);

    registry.addElementDefinition(WTFMove(elementInterface));

    return jsUndefined();
}

// https://html.spec.whatwg.org/#dom-customelementregistry-whendefined
static JSValue whenDefinedPromise(ExecState& state, JSDOMGlobalObject& globalObject, CustomElementRegistry& registry, JSPromiseDeferred& promiseDeferred)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    AtomicString localName(state.uncheckedArgument(0).toString(&state)->toAtomicString(&state));
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!validateCustomElementNameAndThrowIfNeeded(state, localName)) {
        EXCEPTION_ASSERT(scope.exception());
        return jsUndefined();
    }

    if (registry.findInterface(localName)) {
        DeferredPromise::create(globalObject, promiseDeferred)->resolve();
        return promiseDeferred.promise();
    }

    auto result = registry.promiseMap().ensure(localName, [&] {
        return DeferredPromise::create(globalObject, promiseDeferred);
    });

    return result.iterator->value->promise();
}

JSValue JSCustomElementRegistry::whenDefined(ExecState& state)
{
    auto scope = DECLARE_CATCH_SCOPE(state.vm());

    ASSERT(globalObject());
    auto promiseDeferred = JSPromiseDeferred::tryCreate(&state, globalObject());
    RELEASE_ASSERT(promiseDeferred);
    JSValue promise = whenDefinedPromise(state, *globalObject(), wrapped(), *promiseDeferred);

    if (UNLIKELY(scope.exception())) {
        rejectPromiseWithExceptionIfAny(state, *globalObject(), *promiseDeferred);
        scope.assertNoException();
        return promiseDeferred->promise();
    }

    return promise;
}

}
