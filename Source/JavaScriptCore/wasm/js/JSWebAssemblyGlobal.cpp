/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

const ClassInfo JSWebAssemblyGlobal::s_info = { "WebAssembly.Global"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyGlobal) };

JSWebAssemblyGlobal* JSWebAssemblyGlobal::tryCreate(JSGlobalObject* globalObject, VM& vm, Structure* structure, Ref<Wasm::Global>&& global)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (!globalObject->webAssemblyEnabled()) {
        throwException(globalObject, throwScope, createEvalError(globalObject, globalObject->webAssemblyDisabledErrorMessage()));
        return nullptr;
    }

    auto* instance = new (NotNull, allocateCell<JSWebAssemblyGlobal>(vm)) JSWebAssemblyGlobal(vm, structure, WTFMove(global));
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

    result->putDirect(vm, Identifier::fromString(vm, "mutable"_s), jsBoolean(m_global->mutability() == Wasm::Mutable));

    Wasm::Type valueType = m_global->type();
    JSString* valueString = nullptr;
    switch (valueType.kind) {
    case Wasm::TypeKind::I32:
        valueString = jsNontrivialString(vm, "i32"_s);
        break;
    case Wasm::TypeKind::I64:
        valueString = jsNontrivialString(vm, "i64"_s);
        break;
    case Wasm::TypeKind::F32:
        valueString = jsNontrivialString(vm, "f32"_s);
        break;
    case Wasm::TypeKind::F64:
        valueString = jsNontrivialString(vm, "f64"_s);
        break;
    case Wasm::TypeKind::V128:
        valueString = jsNontrivialString(vm, "v128"_s);
        break;
    default: {
        if (Wasm::isFuncref(valueType))
            valueString = jsNontrivialString(vm, "funcref"_s);
        else if (Wasm::isExternref(valueType))
            valueString = jsNontrivialString(vm, "externref"_s);
        else
            RELEASE_ASSERT_NOT_REACHED();
    }
    }
    result->putDirect(vm, Identifier::fromString(vm, "value"_s), valueString);

    return result;
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyGlobal);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
