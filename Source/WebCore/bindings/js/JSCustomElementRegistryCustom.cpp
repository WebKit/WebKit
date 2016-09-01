/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "JSDOMConvert.h"
#include "JSDOMPromise.h"

using namespace JSC;

namespace WebCore {

#if ENABLE(CUSTOM_ELEMENTS)

static JSObject* getCustomElementCallback(ExecState& state, JSObject& prototype, const Identifier& id)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue callback = prototype.get(&state, id);
    if (state.hadException())
        return nullptr;
    if (callback.isUndefined())
        return nullptr;
    if (!callback.isFunction()) {
        throwTypeError(&state, scope, ASCIILiteral("A custom element callback must be a function"));
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
    case CustomElementNameValidationStatus::ConflictsWithBuiltinNames:
        throwSyntaxError(&state, scope, ASCIILiteral("Custom element name cannot be same as one of the builtin elements"));
        return false;
    case CustomElementNameValidationStatus::NoHyphen:
        throwSyntaxError(&state, scope, ASCIILiteral("Custom element name must contain a hyphen"));
        return false;
    case CustomElementNameValidationStatus::ContainsUpperCase:
        throwSyntaxError(&state, scope, ASCIILiteral("Custom element name cannot contain an upper case letter"));
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
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    JSValue constructorValue = state.uncheckedArgument(1);
    if (!constructorValue.isConstructor())
        return throwTypeError(&state, scope, ASCIILiteral("The second argument must be a constructor"));
    JSObject* constructor = constructorValue.getObject();

    if (!validateCustomElementNameAndThrowIfNeeded(state, localName))
        return jsUndefined();

    CustomElementRegistry& registry = wrapped();

    if (registry.elementDefinitionIsRunning()) {
        throwNotSupportedError(state, scope, ASCIILiteral("Cannot define a custom element while defining another custom element"));
        return jsUndefined();
    }
    TemporaryChange<bool> change(registry.elementDefinitionIsRunning(), true);

    if (registry.findInterface(localName)) {
        throwNotSupportedError(state, scope, ASCIILiteral("Cannot define multiple custom elements with the same tag name"));
        return jsUndefined();
    }

    if (registry.containsConstructor(constructor)) {
        throwNotSupportedError(state, scope, ASCIILiteral("Cannot define multiple custom elements with the same class"));
        return jsUndefined();
    }

    JSValue prototypeValue = constructor->get(&state, vm.propertyNames->prototype);
    if (state.hadException())
        return jsUndefined();
    if (!prototypeValue.isObject())
        return throwTypeError(&state, scope, ASCIILiteral("Custom element constructor's prototype must be an object"));
    JSObject& prototypeObject = *asObject(prototypeValue);

    QualifiedName name(nullAtom, localName, HTMLNames::xhtmlNamespaceURI);
    auto elementInterface = JSCustomElementInterface::create(name, constructor, globalObject());

    auto* connectedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "connectedCallback"));
    if (state.hadException())
        return jsUndefined();
    if (connectedCallback)
        elementInterface->setConnectedCallback(connectedCallback);

    auto* disconnectedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "disconnectedCallback"));
    if (state.hadException())
        return jsUndefined();
    if (disconnectedCallback)
        elementInterface->setDisconnectedCallback(disconnectedCallback);

    auto* adoptedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "adoptedCallback"));
    if (state.hadException())
        return jsUndefined();
    if (adoptedCallback)
        elementInterface->setAdoptedCallback(adoptedCallback);

    auto* attributeChangedCallback = getCustomElementCallback(state, prototypeObject, Identifier::fromString(&vm, "attributeChangedCallback"));
    if (state.hadException())
        return jsUndefined();
    if (attributeChangedCallback) {
        auto value = convertOptional<Vector<String>>(state, constructor->get(&state, Identifier::fromString(&state, "observedAttributes")));
        if (state.hadException())
            return jsUndefined();
        if (value)
            elementInterface->setAttributeChangedCallback(attributeChangedCallback, *value);
    }

    PrivateName uniquePrivateName;
    globalObject()->putDirect(vm, uniquePrivateName, constructor);

    registry.addElementDefinition(WTFMove(elementInterface));

    // FIXME: 17. Let map be registry's upgrade candidates map.
    // FIXME: 18. Upgrade a newly-defined element given map and definition.

    auto& promiseMap = registry.promiseMap();
    auto promise = promiseMap.take(localName);
    if (promise)
        promise.value()->resolve(nullptr);

    return jsUndefined();
}

// https://html.spec.whatwg.org/#dom-customelementregistry-whendefined
static JSValue whenDefinedPromise(ExecState& state, JSDOMGlobalObject& globalObject, CustomElementRegistry& registry)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    AtomicString localName(state.uncheckedArgument(0).toString(&state)->toAtomicString(&state));
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    if (!validateCustomElementNameAndThrowIfNeeded(state, localName))
        return jsUndefined();

    if (registry.findInterface(localName)) {
        auto& jsPromise = *JSPromiseDeferred::create(&state, &globalObject);
        DeferredWrapper::create(&state, &globalObject, &jsPromise)->resolve(nullptr);
        return jsPromise.promise();
    }

    auto result = registry.promiseMap().ensure(localName, [&] {
        return DeferredWrapper::create(&state, &globalObject, JSPromiseDeferred::create(&state, &globalObject));
    });

    return result.iterator->value->promise();
}

JSValue JSCustomElementRegistry::whenDefined(ExecState& state)
{
    JSDOMGlobalObject& globalObject = *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject());
    auto& promiseDeferred = *JSPromiseDeferred::create(&state, &globalObject);
    JSValue promise = whenDefinedPromise(state, globalObject, wrapped());

    if (state.hadException()) {
        rejectPromiseWithExceptionIfAny(state, globalObject, promiseDeferred);
        ASSERT(!state.hadException());
        return promiseDeferred.promise();
    }

    return promise;
}

#endif

}
