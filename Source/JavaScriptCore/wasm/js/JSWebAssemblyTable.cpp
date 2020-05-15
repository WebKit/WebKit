/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyTable.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyInstance.h"

namespace JSC {

const ClassInfo JSWebAssemblyTable::s_info = { "WebAssembly.Table", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyTable) };

JSWebAssemblyTable* JSWebAssemblyTable::tryCreate(JSGlobalObject* globalObject, VM& vm, Structure* structure, Ref<Wasm::Table>&& table)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (!globalObject->webAssemblyEnabled()) {
        throwException(globalObject, throwScope, createEvalError(globalObject, globalObject->webAssemblyDisabledErrorMessage()));
        return nullptr;
    }

    auto* instance = new (NotNull, allocateCell<JSWebAssemblyTable>(vm.heap)) JSWebAssemblyTable(vm, structure, WTFMove(table));
    instance->table()->setOwner(instance);
    instance->finishCreation(vm);
    return instance;
}

Structure* JSWebAssemblyTable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyTable::JSWebAssemblyTable(VM& vm, Structure* structure, Ref<Wasm::Table>&& table)
    : Base(vm, structure)
    , m_table(WTFMove(table))
{
}

void JSWebAssemblyTable::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void JSWebAssemblyTable::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyTable*>(cell)->JSWebAssemblyTable::~JSWebAssemblyTable();
}

void JSWebAssemblyTable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSWebAssemblyTable* thisObject = jsCast<JSWebAssemblyTable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    thisObject->table()->visitAggregate(visitor);
}

bool JSWebAssemblyTable::grow(uint32_t delta)
{
    if (delta == 0)
        return true;
    return !!m_table->grow(delta);
}

JSValue JSWebAssemblyTable::get(uint32_t index)
{
    RELEASE_ASSERT(index < length());
    return m_table->get(index);
}

void JSWebAssemblyTable::set(uint32_t index, JSValue value)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_table->isAnyrefTable());
    m_table->set(index, value);
}

void JSWebAssemblyTable::set(uint32_t index, WebAssemblyFunction* function)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_table->asFuncrefTable());
    auto& subThis = *static_cast<Wasm::FuncRefTable*>(&m_table.get());
    subThis.setFunction(index, function, function->importableFunction(), &function->instance()->instance());
}

void JSWebAssemblyTable::set(uint32_t index, WebAssemblyWrapperFunction* function)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_table->asFuncrefTable());
    auto& subThis = *static_cast<Wasm::FuncRefTable*>(&m_table.get());
    subThis.setFunction(index, function, function->importableFunction(), &function->instance()->instance());
}

void JSWebAssemblyTable::clear(uint32_t index)
{
    RELEASE_ASSERT(index < length());
    m_table->clear(index);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
