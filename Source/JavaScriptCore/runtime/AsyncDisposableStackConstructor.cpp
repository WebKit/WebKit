/*
 * Copyright (C) 2025 Sosuke Suzuki <aosukeke@gmail.com>.
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
#include "AsyncDisposableStackConstructor.h"

#include "AbstractSlotVisitor.h"
#include "BuiltinNames.h"
#include "GetterSetter.h"
#include "JSAsyncDisposableStack.h"
#include "JSCInlines.h"
#include "AsyncDisposableStackPrototype.h"
#include "SlotVisitor.h"

namespace JSC {

const ClassInfo AsyncDisposableStackConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AsyncDisposableStackConstructor) };

Structure* AsyncDisposableStackConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

AsyncDisposableStackConstructor* AsyncDisposableStackConstructor::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, AsyncDisposableStackPrototype* disposableStackPrototype)
{
    AsyncDisposableStackConstructor* constructor = new (NotNull, allocateCell<AsyncDisposableStackConstructor>(vm)) AsyncDisposableStackConstructor(vm, structure);
    constructor->finishCreation(vm, globalObject, disposableStackPrototype);
    return constructor;
}

void AsyncDisposableStackConstructor::finishCreation(VM& vm, JSGlobalObject*, AsyncDisposableStackPrototype* prototype)
{
    Base::finishCreation(vm, 0, vm.propertyNames->AsyncDisposableStack.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, static_cast<unsigned>(PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly));
}

template<typename Visitor>
void AsyncDisposableStackConstructor::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<AsyncDisposableStackConstructor>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(AsyncDisposableStackConstructor);

static JSC_DECLARE_HOST_FUNCTION(callAsyncDisposableStack);
static JSC_DECLARE_HOST_FUNCTION(constructAsyncDisposableStack);

AsyncDisposableStackConstructor::AsyncDisposableStackConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callAsyncDisposableStack, constructAsyncDisposableStack)
{
}

JSC_DEFINE_HOST_FUNCTION(callAsyncDisposableStack, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "AsyncDisposableStack"_s));
}

JSC_DEFINE_HOST_FUNCTION(constructAsyncDisposableStack, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* asyncDisposableStackStructure = JSC_GET_DERIVED_STRUCTURE(vm, asyncDisposableStackStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    auto* asyncDisposableStack = JSAsyncDisposableStack::create(vm, asyncDisposableStackStructure);
    auto* capabilityArray = constructEmptyArray(globalObject, nullptr);
    if (!capabilityArray) [[unlikely]]
        RETURN_IF_EXCEPTION(scope, { });
    asyncDisposableStack->internalField(JSAsyncDisposableStack::Field::Capability).set(vm, asyncDisposableStack, capabilityArray);

    RELEASE_AND_RETURN(scope, JSValue::encode(asyncDisposableStack));
}

} // namespace JSC
