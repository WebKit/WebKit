/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyGlobal.h"
#include "ObjectConstructor.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSWebAssemblyGlobal::s_info = { "WebAssembly.Global", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyGlobal) };

JSWebAssemblyGlobal* JSWebAssemblyGlobal::tryCreate(JSGlobalObject* globalObject, VM& vm, Structure* structure, Ref<Wasm::Global>&& global)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (!globalObject->webAssemblyEnabled()) {
        throwException(globalObject, throwScope, createEvalError(globalObject, globalObject->webAssemblyDisabledErrorMessage()));
        return nullptr;
    }

    auto* instance = new (NotNull, allocateCell<JSWebAssemblyGlobal>(vm.heap)) JSWebAssemblyGlobal(vm, structure, WTFMove(global));
    instance->global()->setOwner(instance);
    instance->finishCreation(vm);
    return instance;
}

Structure* JSWebAssemblyGlobal::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyGlobal::JSWebAssemblyGlobal(VM& vm, Structure* structure, Ref<Wasm::Global>&& global)
    : Base(vm, structure)
    , m_global(WTFMove(global))
{
}

void JSWebAssemblyGlobal::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void JSWebAssemblyGlobal::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyGlobal*>(cell)->JSWebAssemblyGlobal::~JSWebAssemblyGlobal();
}

template<typename Visitor>
void JSWebAssemblyGlobal::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyGlobal* thisObject = jsCast<JSWebAssemblyGlobal*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    thisObject->global()->visitAggregate(visitor);
}

JSObject* JSWebAssemblyGlobal::type(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();

    JSObject* result = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);

    result->putDirect(vm, Identifier::fromString(vm, "mutable"), jsBoolean(m_global->mutability() == Wasm::GlobalInformation::Mutable));

    Wasm::Type valueType = m_global->type();
    JSString* valueString = nullptr;
    switch (valueType.kind) {
    case Wasm::TypeKind::I32:
        valueString = jsNontrivialString(vm, "i32");
        break;
    case Wasm::TypeKind::I64:
        valueString = jsNontrivialString(vm, "i64");
        break;
    case Wasm::TypeKind::F32:
        valueString = jsNontrivialString(vm, "f32");
        break;
    case Wasm::TypeKind::F64:
        valueString = jsNontrivialString(vm, "f64");
        break;
    case Wasm::TypeKind::Externref:
        valueString = jsNontrivialString(vm, "externref");
        break;
    case Wasm::TypeKind::Funcref:
        valueString = jsNontrivialString(vm, "funcref");
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    result->putDirect(vm, Identifier::fromString(vm, "value"), valueString);

    return result;
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyGlobal);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
