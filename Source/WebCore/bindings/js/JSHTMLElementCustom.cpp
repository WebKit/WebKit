/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "JSHTMLElement.h"

#include "CustomElementDefinitions.h"
#include "Document.h"
#include "HTMLFormElement.h"
#include "JSNodeCustom.h"
#include <runtime/InternalFunction.h>
#include <runtime/JSWithScope.h>

namespace WebCore {

using namespace JSC;

#if ENABLE(CUSTOM_ELEMENTS)
EncodedJSValue JSC_HOST_CALL constructJSHTMLElement(ExecState* state)
{
    auto* jsConstructor = jsCast<DOMConstructorObject*>(state->callee());

    auto* context = jsConstructor->scriptExecutionContext();
    if (!is<Document>(context))
        return throwConstructorDocumentUnavailableError(*state, "HTMLElement");
    auto& document = downcast<Document>(*context);

    auto* definitions = document.customElementDefinitions();
    if (!definitions)
        return throwVMTypeError(state, "new.target is not a valid custom element constructor");

    VM& vm = state->vm();
    JSValue newTargetValue = state->thisValue();
    JSObject* newTarget = newTargetValue.getObject();
    auto* interface = definitions->findInterface(newTarget);
    if (!interface)
        return throwVMTypeError(state, "new.target does not define a custom element");

    if (!interface->isUpgradingElement()) {
        auto* globalObject = jsConstructor->globalObject();
        Structure* baseStructure = getDOMStructure<JSHTMLElement>(vm, *globalObject);
        auto* newElementStructure = InternalFunction::createSubclassStructure(state, newTargetValue, baseStructure);
        if (UNLIKELY(state->hadException()))
            return JSValue::encode(jsUndefined());

        Ref<HTMLElement> element = HTMLElement::create(interface->name(), document);
        auto* jsElement = JSHTMLElement::create(newElementStructure, globalObject, element.get());
        cacheWrapper(globalObject->world(), element.ptr(), jsElement);
        return JSValue::encode(jsElement);
    }

    Element* elementToUpgrade = interface->lastElementInConstructionStack();
    if (!elementToUpgrade) {
        throwInvalidStateError(*state, "Cannot instantiate a custom element inside its own constrcutor during upgrades");
        return JSValue::encode(jsUndefined());
    }

    JSValue elementWrapperValue = toJS(state, jsConstructor->globalObject(), elementToUpgrade);
    ASSERT(elementWrapperValue.isObject());

    JSValue newPrototype = newTarget->get(state, vm.propertyNames->prototype);
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    JSObject* elementWrapperObject = asObject(elementWrapperValue);
    JSObject::setPrototype(elementWrapperObject, state, newPrototype, true /* shouldThrowIfCantSet */);
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    interface->didUpgradeLastElementInConstructionStack();

    return JSValue::encode(elementWrapperValue);
}
#endif

JSScope* JSHTMLElement::pushEventHandlerScope(ExecState* exec, JSScope* scope) const
{
    HTMLElement& element = wrapped();

    // The document is put on first, fall back to searching it only after the element and form.
    scope = JSWithScope::create(exec, asObject(toJS(exec, globalObject(), &element.document())), scope);

    // The form is next, searched before the document, but after the element itself.
    if (HTMLFormElement* form = element.form())
        scope = JSWithScope::create(exec, asObject(toJS(exec, globalObject(), form)), scope);

    // The element is on top, searched first.
    return JSWithScope::create(exec, asObject(toJS(exec, globalObject(), &element)), scope);
}

} // namespace WebCore
