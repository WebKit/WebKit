/*
 * Copyright (C) 2007, 2016 Apple Inc. All rights reserved.
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

#include "CustomElementRegistry.h"
#include "DOMWindow.h"
#include "Document.h"
#include "HTMLFormElement.h"
#include "JSCustomElementInterface.h"
#include "JSNodeCustom.h"
#include "ScriptExecutionContext.h"
#include <runtime/InternalFunction.h>
#include <runtime/JSWithScope.h>

namespace WebCore {

using namespace JSC;

#if ENABLE(CUSTOM_ELEMENTS)
EncodedJSValue JSC_HOST_CALL constructJSHTMLElement(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* jsConstructor = jsCast<DOMConstructorObject*>(exec.callee());
    ASSERT(jsConstructor);

    auto* context = jsConstructor->scriptExecutionContext();
    if (!context)
        return throwConstructorScriptExecutionContextUnavailableError(exec, scope, "HTMLElement");
    ASSERT(context->isDocument());

    auto& document = downcast<Document>(*context);

    auto* window = document.domWindow();
    if (!window)
        return throwVMTypeError(&exec, scope, ASCIILiteral("new.target is not a valid custom element constructor"));

    auto* registry = window->customElementRegistry();
    if (!registry)
        return throwVMTypeError(&exec, scope, ASCIILiteral("new.target is not a valid custom element constructor"));

    JSValue newTargetValue = exec.thisValue();
    JSObject* newTarget = newTargetValue.getObject();
    auto* elementInterface = registry->findInterface(newTarget);
    if (!elementInterface)
        return throwVMTypeError(&exec, scope, ASCIILiteral("new.target does not define a custom element"));

    if (!elementInterface->isUpgradingElement()) {
        auto* globalObject = jsConstructor->globalObject();
        Structure* baseStructure = getDOMStructure<JSHTMLElement>(vm, *globalObject);
        auto* newElementStructure = InternalFunction::createSubclassStructure(&exec, newTargetValue, baseStructure);
        if (UNLIKELY(exec.hadException()))
            return JSValue::encode(jsUndefined());

        Ref<HTMLElement> element = HTMLElement::create(elementInterface->name(), document);
        element->setIsUnresolvedCustomElement();
        auto* jsElement = JSHTMLElement::create(newElementStructure, globalObject, element.get());
        cacheWrapper(globalObject->world(), element.ptr(), jsElement);
        return JSValue::encode(jsElement);
    }

    Element* elementToUpgrade = elementInterface->lastElementInConstructionStack();
    if (!elementToUpgrade) {
        throwInvalidStateError(exec, scope, ASCIILiteral("Cannot instantiate a custom element inside its own constrcutor during upgrades"));
        return JSValue::encode(jsUndefined());
    }

    JSValue elementWrapperValue = toJS(&exec, jsConstructor->globalObject(), *elementToUpgrade);
    ASSERT(elementWrapperValue.isObject());

    JSValue newPrototype = newTarget->get(&exec, vm.propertyNames->prototype);
    if (exec.hadException())
        return JSValue::encode(jsUndefined());

    JSObject* elementWrapperObject = asObject(elementWrapperValue);
    JSObject::setPrototype(elementWrapperObject, &exec, newPrototype, true /* shouldThrowIfCantSet */);
    if (exec.hadException())
        return JSValue::encode(jsUndefined());

    elementInterface->didUpgradeLastElementInConstructionStack();

    return JSValue::encode(elementWrapperValue);
}
#endif

JSScope* JSHTMLElement::pushEventHandlerScope(ExecState* exec, JSScope* scope) const
{
    HTMLElement& element = wrapped();

    // The document is put on first, fall back to searching it only after the element and form.
    // FIXME: This probably may use the wrong global object. If this is called from a native
    // function, then it would be correct but not optimal since the native function would *know*
    // the global object. But, it may be that globalObject() is more correct.
    // https://bugs.webkit.org/show_bug.cgi?id=134932
    VM& vm = exec->vm();
    JSGlobalObject* lexicalGlobalObject = exec->lexicalGlobalObject();
    
    scope = JSWithScope::create(vm, lexicalGlobalObject, asObject(toJS(exec, globalObject(), element.document())), scope);

    // The form is next, searched before the document, but after the element itself.
    if (HTMLFormElement* form = element.form())
        scope = JSWithScope::create(vm, lexicalGlobalObject, asObject(toJS(exec, globalObject(), *form)), scope);

    // The element is on top, searched first.
    return JSWithScope::create(vm, lexicalGlobalObject, asObject(toJS(exec, globalObject(), element)), scope);
}

} // namespace WebCore
