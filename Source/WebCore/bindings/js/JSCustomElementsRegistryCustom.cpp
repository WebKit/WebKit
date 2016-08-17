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
#include "JSCustomElementsRegistry.h"

#include "CustomElementsRegistry.h"
#include "Document.h"
#include "HTMLNames.h"
#include "JSCustomElementInterface.h"
#include "JSDOMBinding.h"
#include "JSDOMConvert.h"

using namespace JSC;

namespace WebCore {

#if ENABLE(CUSTOM_ELEMENTS)

static JSObject* getLifecycleCallback(ExecState& state, JSObject& prototype, const Identifier& id)
{
    JSValue callback = prototype.get(&state, id);
    if (state.hadException())
        return nullptr;
    if (callback.isUndefined())
        return nullptr;
    if (!callback.isFunction()) {
        throwTypeError(&state, ASCIILiteral("A lifecycle callback must be a function"));
        return nullptr;
    }
    return callback.getObject();
}

// https://html.spec.whatwg.org/#dom-customelementsregistry-define
JSValue JSCustomElementsRegistry::define(ExecState& state)
{
    if (UNLIKELY(state.argumentCount() < 2))
        return state.vm().throwException(&state, createNotEnoughArgumentsError(&state));

    AtomicString localName(state.uncheckedArgument(0).toString(&state)->toAtomicString(&state));
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    JSValue constructorValue = state.uncheckedArgument(1);
    if (!constructorValue.isConstructor())
        return throwTypeError(&state, ASCIILiteral("The second argument must be a constructor"));
    JSObject* constructor = constructorValue.getObject();

    // FIXME: Throw a TypeError if constructor doesn't inherit from HTMLElement.
    // https://github.com/w3c/webcomponents/issues/541

    switch (Document::validateCustomElementName(localName)) {
    case CustomElementNameValidationStatus::Valid:
        break;
    case CustomElementNameValidationStatus::ConflictsWithBuiltinNames:
        return throwSyntaxError(&state, ASCIILiteral("Custom element name cannot be same as one of the builtin elements"));
    case CustomElementNameValidationStatus::NoHyphen:
        return throwSyntaxError(&state, ASCIILiteral("Custom element name must contain a hyphen"));
    case CustomElementNameValidationStatus::ContainsUpperCase:
        return throwSyntaxError(&state, ASCIILiteral("Custom element name cannot contain an upper case letter"));
    }

    // FIXME: Check re-entrancy here.
    // https://github.com/w3c/webcomponents/issues/545

    CustomElementsRegistry& registry = wrapped();
    if (registry.findInterface(localName)) {
        throwNotSupportedError(state, ASCIILiteral("Cannot define multiple custom elements with the same tag name"));
        return jsUndefined();
    }

    if (registry.containsConstructor(constructor)) {
        throwNotSupportedError(state, ASCIILiteral("Cannot define multiple custom elements with the same class"));
        return jsUndefined();
    }

    auto& vm = globalObject()->vm();
    JSValue prototypeValue = constructor->get(&state, vm.propertyNames->prototype);
    if (state.hadException())
        return jsUndefined();
    if (!prototypeValue.isObject())
        return throwTypeError(&state, ASCIILiteral("Custom element constructor's prototype must be an object"));
    JSObject& prototypeObject = *asObject(prototypeValue);

    // FIXME: Add the support for connectedCallback.
    getLifecycleCallback(state, prototypeObject, Identifier::fromString(&vm, "connectedCallback"));
    if (state.hadException())
        return jsUndefined();

    // FIXME: Add the support for disconnectedCallback.
    getLifecycleCallback(state, prototypeObject, Identifier::fromString(&vm, "disconnectedCallback"));
    if (state.hadException())
        return jsUndefined();

    // FIXME: Add the support for adoptedCallback.
    getLifecycleCallback(state, prototypeObject, Identifier::fromString(&vm, "adoptedCallback"));
    if (state.hadException())
        return jsUndefined();

    QualifiedName name(nullAtom, localName, HTMLNames::xhtmlNamespaceURI);
    Ref<JSCustomElementInterface> interface = JSCustomElementInterface::create(name, constructor, globalObject());

    auto* attributeChangedCallback = getLifecycleCallback(state, prototypeObject, Identifier::fromString(&vm, "attributeChangedCallback"));
    if (state.hadException())
        return jsUndefined();
    if (attributeChangedCallback) {
        auto value = convertOptional<Vector<String>>(state, constructor->get(&state, Identifier::fromString(&state, "observedAttributes")));
        if (state.hadException())
            return jsUndefined();
        if (value)
            interface->setAttributeChangedCallback(attributeChangedCallback, *value);
    }

    PrivateName uniquePrivateName;
    globalObject()->putDirect(vm, uniquePrivateName, constructor);

    registry.addElementDefinition(WTFMove(interface));

    // FIXME: 17. Let map be registry's upgrade candidates map.
    // FIXME: 18. Upgrade a newly-defined element given map and definition.
    // FIXME: 19. Resolve whenDefined promise.

    return jsUndefined();
}
#endif

}
