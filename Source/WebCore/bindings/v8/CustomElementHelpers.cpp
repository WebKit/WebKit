/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CustomElementHelpers.h"

#include "CustomElementRegistry.h"
#include "DOMWrapperWorld.h"
#include "ScriptController.h"
#include "V8CustomElementConstructor.h"
#include "V8HTMLElementWrapperFactory.h"
#include "V8HTMLParagraphElement.h"
#include "V8HTMLSpanElement.h"

namespace WebCore {

#if ENABLE(CUSTOM_ELEMENTS)

bool CustomElementHelpers::initializeConstructorWrapper(CustomElementConstructor* constructor, const ScriptValue& prototype, ScriptState* state)
{
    ASSERT(isFeatureAllowed(state));
    ASSERT(!prototype.v8Value().IsEmpty() && prototype.v8Value()->IsObject());
    v8::Handle<v8::Value> wrapperValue = toV8(constructor, state->context()->Global(), state->context()->GetIsolate());
    if (wrapperValue.IsEmpty() || !wrapperValue->IsObject())
        return false;
    v8::Handle<v8::Function> wrapper = v8::Handle<v8::Function>::Cast(wrapperValue);
    // - Object::ForceSet() nor Object::SetAccessor Doesn't work against the "prototype" property of function objects.
    // - Set()-ing here is safe because
    //   - Hooking Object.prototype's defineProperty() with "prototype" or "constructor" also doesn't affect on these properties of function objects and
    //   - Using Set() is okay becaues each function has "prototype" property from start and Objects.prototype cannot intercept the property access.
    v8::Handle<v8::String> prototypeKey = v8String("prototype", state->context()->GetIsolate());
    ASSERT(wrapper->HasOwnProperty(prototypeKey));
    wrapper->Set(prototypeKey, prototype.v8Value(), v8::ReadOnly);

    v8::Handle<v8::String> constructorKey = v8String("constructor", state->context()->GetIsolate());
    v8::Handle<v8::Object> prototypeObject = v8::Handle<v8::Object>::Cast(prototype.v8Value());
    ASSERT(!prototypeObject->HasOwnProperty(constructorKey));
    prototypeObject->ForceSet(constructorKey, wrapper, v8::ReadOnly);
    return true;
}

static bool hasValidPrototypeChain(v8::Handle<v8::Object> requiredAncestor, v8::Handle<v8::Value> chain)
{
    while (!chain.IsEmpty() && chain->IsObject()) {
        if (chain == requiredAncestor)
            return true;
        chain = v8::Handle<v8::Object>::Cast(chain)->GetPrototype();
    }

    return false;
}

bool CustomElementHelpers::isValidPrototypeParameter(const ScriptValue& prototype, ScriptState* state)
{
    if (prototype.v8Value().IsEmpty() || !prototype.v8Value()->IsObject())
        return false;

    // document.register() sets the constructor property, so the prototype shouldn't have one.
    v8::Handle<v8::Object> prototypeObject = v8::Handle<v8::Object>::Cast(prototype.v8Value());
    if (prototypeObject->HasOwnProperty(v8String("constructor", state->context()->GetIsolate())))
        return false;
    V8PerContextData* perContextData = V8PerContextData::from(state->context());
    // FIXME: non-HTML subclasses should be also supported: https://bugs.webkit.org/show_bug.cgi?id=111693
    v8::Handle<v8::Object> htmlConstructor = v8::Handle<v8::Object>::Cast(perContextData->constructorForType(&V8HTMLElement::info));
    if (htmlConstructor.IsEmpty())
        return false;
    v8::Handle<v8::Object> htmlPrototype = v8::Handle<v8::Object>::Cast(htmlConstructor->Get(v8String("prototype", state->context()->GetIsolate())));
    if (htmlPrototype.IsEmpty())
        return false;
    if (!hasValidPrototypeChain(htmlPrototype, prototypeObject))
        return false;
    return true;
}

bool CustomElementHelpers::isFeatureAllowed(ScriptState* state)
{
    return isFeatureAllowed(state->context());
}

bool CustomElementHelpers::isFeatureAllowed(v8::Handle<v8::Context> context)
{
    if (DOMWrapperWorld* world = DOMWrapperWorld::isolatedWorld(context))
        return world->isMainWorld();
    return true;
}

const QualifiedName* CustomElementHelpers::findLocalName(const ScriptValue& prototype)
{
    if (prototype.v8Value().IsEmpty() || !prototype.v8Value()->IsObject())
        return 0;
    return findLocalName(v8::Handle<v8::Object>::Cast(prototype.v8Value()));
}

WrapperTypeInfo* CustomElementHelpers::findWrapperType(v8::Handle<v8::Value> chain)
{
    while (!chain.IsEmpty() && chain->IsObject()) {
        v8::Handle<v8::Object> chainObject = v8::Handle<v8::Object>::Cast(chain);
        // Only prototype objects of native-backed types have the extra internal field storing WrapperTypeInfo.
        if (v8PrototypeInternalFieldcount == chainObject->InternalFieldCount())
            return reinterpret_cast<WrapperTypeInfo*>(chainObject->GetAlignedPointerFromInternalField(v8PrototypeTypeIndex));
        chain = chainObject->GetPrototype();
    }

    return 0;
}

// This can return null. In that case, we should take the element name as its local name.
const QualifiedName* CustomElementHelpers::findLocalName(v8::Handle<v8::Object> chain)
{
    WrapperTypeInfo* type = CustomElementHelpers::findWrapperType(chain);
    if (!type)
        return 0;
    return findHTMLTagNameOfV8Type(type);
}

void CustomElementHelpers::invokeReadyCallbackIfNeeded(Element* element, v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Value> wrapperValue = toV8(element, context->Global(), context->GetIsolate());
    if (wrapperValue.IsEmpty() || !wrapperValue->IsObject())
        return;
    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(wrapperValue);
    v8::Handle<v8::Value> prototypeValue = wrapper->GetPrototype();
    if (prototypeValue.IsEmpty() || !prototypeValue->IsObject())
        return;
    v8::Handle<v8::Object> prototype = v8::Handle<v8::Object>::Cast(prototypeValue);
    v8::Handle<v8::Value> functionValue = prototype->Get(v8::String::NewSymbol("readyCallback"));
    if (functionValue.IsEmpty() || !functionValue->IsFunction())
        return;

    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(functionValue);
    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    v8::Handle<v8::Value> args[] = { v8::Handle<v8::Value>() };
    ScriptController::callFunctionWithInstrumentation(element->document(), function, wrapper, 0, args);
}


void CustomElementHelpers::invokeReadyCallbacksIfNeeded(ScriptExecutionContext* executionContext, const Vector<CustomElementInvocation>& invocations)
{
    ASSERT(!invocations.isEmpty());

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(executionContext, mainThreadNormalWorld());
    if (context.IsEmpty())
        return;
    v8::Context::Scope scope(context);

    for (size_t i = 0; i < invocations.size(); ++i) {
        ASSERT(executionContext == invocations[i].element()->document());
        invokeReadyCallbackIfNeeded(invocations[i].element(), context);
    }
}

#endif // ENABLE(CUSTOM_ELEMENTS)

} // namespace WebCore
