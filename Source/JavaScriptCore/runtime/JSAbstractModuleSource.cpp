/*
 * Copyright (C) 2026 Sergey Rubanov. All rights reserved.
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
#include "JSAbstractModuleSource.h"

#include "CustomGetterSetter.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSWebAssemblyModule.h"

namespace JSC {

const ClassInfo JSAbstractModuleSourcePrototype::s_info = { "AbstractModuleSource"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAbstractModuleSourcePrototype) };

static JSC_DECLARE_CUSTOM_GETTER(abstractModuleSourceProtoToStringTag);

JSAbstractModuleSourcePrototype* JSAbstractModuleSourcePrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<JSAbstractModuleSourcePrototype>(vm)) JSAbstractModuleSourcePrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* JSAbstractModuleSourcePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSAbstractModuleSourcePrototype::JSAbstractModuleSourcePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSAbstractModuleSourcePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    putDirectCustomAccessor(vm, vm.propertyNames->toStringTagSymbol, CustomGetterSetter::create(vm, abstractModuleSourceProtoToStringTag, nullptr), static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor));
}

JSC_DEFINE_CUSTOM_GETTER(abstractModuleSourceProtoToStringTag, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    JSObject* thisObject = JSValue::decode(thisValue).getObject();
#if ENABLE(WEBASSEMBLY)
    if (thisObject && thisObject->inherits<JSWebAssemblyModule>())
        return JSValue::encode(jsNontrivialString(globalObject->vm(), "WebAssembly.Module"_s));
#else
    UNUSED_PARAM(globalObject);
#endif
    UNUSED_PARAM(thisObject);
    return JSValue::encode(jsUndefined());
}

const ClassInfo JSAbstractModuleSourceConstructor::s_info = { "AbstractModuleSource"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAbstractModuleSourceConstructor) };

static JSC_DECLARE_HOST_FUNCTION(callAbstractModuleSource);
static JSC_DECLARE_HOST_FUNCTION(constructAbstractModuleSource);

JSC_DEFINE_HOST_FUNCTION(callAbstractModuleSource, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "AbstractModuleSource cannot be called or constructed"_s);
}

JSC_DEFINE_HOST_FUNCTION(constructAbstractModuleSource, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "AbstractModuleSource cannot be called or constructed"_s);
}

JSAbstractModuleSourceConstructor* JSAbstractModuleSourceConstructor::create(VM& vm, Structure* structure, JSAbstractModuleSourcePrototype* prototype)
{
    auto* constructor = new (NotNull, allocateCell<JSAbstractModuleSourceConstructor>(vm)) JSAbstractModuleSourceConstructor(vm, structure);
    constructor->finishCreation(vm, prototype);
    return constructor;
}

Structure* JSAbstractModuleSourceConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

JSAbstractModuleSourceConstructor::JSAbstractModuleSourceConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callAbstractModuleSource, constructAbstractModuleSource)
{
}

void JSAbstractModuleSourceConstructor::finishCreation(VM& vm, JSAbstractModuleSourcePrototype* prototype)
{
    Base::finishCreation(vm, 0, "AbstractModuleSource"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    prototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

} // namespace JSC
