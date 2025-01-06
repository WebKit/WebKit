/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
 * Copyright (C) 2024 Tetsuharu Ohzeki <tetsuharu.ohzeki@gmail.com>.
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
#include "JSIteratorConstructor.h"

#include "AbstractSlotVisitor.h"
#include "BuiltinNames.h"
#include "GetterSetter.h"
#include "JSCInlines.h"
#include "JSIterator.h"
#include "JSIteratorPrototype.h"
#include "SlotVisitor.h"

namespace JSC {

const ClassInfo JSIteratorConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSIteratorConstructor) };

Structure* JSIteratorConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

JSIteratorConstructor* JSIteratorConstructor::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, JSIteratorPrototype* iteratorPrototype)
{
    JSIteratorConstructor* constructor = new (NotNull, allocateCell<JSIteratorConstructor>(vm)) JSIteratorConstructor(vm, structure);
    constructor->finishCreation(vm, globalObject, iteratorPrototype);
    return constructor;
}

void JSIteratorConstructor::finishCreation(VM& vm, JSGlobalObject* globalObject, JSIteratorPrototype* iteratorPrototype)
{
    Base::finishCreation(vm, 0, vm.propertyNames->Iterator.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, iteratorPrototype, static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->from, jsIteratorConstructorFromCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::useIteratorHelpers() && Options::useIteratorSequencing())
        JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().concatPublicName(), jsIteratorConstructorConcatCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

template<typename Visitor>
void JSIteratorConstructor::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSIteratorConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSIteratorConstructor);

static JSC_DECLARE_HOST_FUNCTION(callIterator);
static JSC_DECLARE_HOST_FUNCTION(constructIterator);

JSIteratorConstructor::JSIteratorConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIterator, constructIterator)
{
}

JSC_DEFINE_HOST_FUNCTION(callIterator, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Iterator"_s));
}

// https://tc39.es/proposal-iterator-helpers/#sec-iterator
JSC_DEFINE_HOST_FUNCTION(constructIterator, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    JSIteratorConstructor* iteratorConstructor = jsCast<JSIteratorConstructor*>(callFrame->jsCallee());
    if (newTarget == iteratorConstructor)
        return JSValue::encode(throwTypeError(globalObject, scope, "Iterator cannot be constructed directly"_s));

    Structure* iteratorStructure = JSC_GET_DERIVED_STRUCTURE(vm, iteratorStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(JSIterator::create(vm, iteratorStructure)));
}

} // namespace JSC
