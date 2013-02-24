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

#include "DOMWrapperWorld.h"
#include "V8CustomElementConstructor.h"
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

// See FIXME on the caller side comment.
static bool hasNoBuiltinsInPrototype(v8::Handle<v8::Object> htmlPrototype, v8::Handle<v8::Value> chain)
{
    while (!chain.IsEmpty()) {
        if (chain == htmlPrototype)
            return true;
        if (!chain->IsObject())
            return false;
        // The internal field count indicates the object might be a native backed, built-in object.
        if (v8::Handle<v8::Object>::Cast(chain)->InternalFieldCount())
            return false;
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
    //
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=110436
    //   document.register() should allow arbitrary HTMLElement subclassses.
    //   Currently it supports custom elements which are
    //   - direct subclasses of HTMLElement or
    //   - subclasses of other custom elements
    //
    v8::Handle<v8::Object> htmlConstructor = v8::Handle<v8::Object>::Cast(perContextData->constructorForType(&V8HTMLElement::info));
    if (htmlConstructor.IsEmpty())
        return false;
    v8::Handle<v8::Object> htmlPrototype = v8::Handle<v8::Object>::Cast(htmlConstructor->Get(v8String("prototype", state->context()->GetIsolate())));
    if (htmlPrototype.IsEmpty())
        return false;
    if (!hasNoBuiltinsInPrototype(htmlPrototype, prototypeObject))
        return false;
    return true;
}

bool CustomElementHelpers::isFeatureAllowed(ScriptState* state)
{
    return isFeatureAllowed(state->context());
}

bool CustomElementHelpers::isFeatureAllowed(v8::Handle<v8::Context> context)
{
    if (DOMWrapperWorld* world = DOMWrapperWorld::getWorld(context))
        return world->isMainWorld();
    return true;
}


#endif // ENABLE(CUSTOM_ELEMENTS)

} // namespace WebCore
