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
#include <wtf/CheckedArithmetic.h>

namespace JSC {

const ClassInfo JSWebAssemblyTable::s_info = { "WebAssembly.Table", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyTable) };

JSWebAssemblyTable* JSWebAssemblyTable::create(ExecState* exec, VM& vm, Structure* structure, Ref<Wasm::Table>&& table)
{
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto* globalObject = exec->lexicalGlobalObject();

    if (!globalObject->webAssemblyEnabled()) {
        throwException(exec, throwScope, createEvalError(exec, globalObject->webAssemblyDisabledErrorMessage()));
        return nullptr;
    }

    auto* instance = new (NotNull, allocateCell<JSWebAssemblyTable>(vm.heap)) JSWebAssemblyTable(vm, structure, WTFMove(table));
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
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_jsFunctions = MallocPtr<WriteBarrier<JSObject>>::malloc((sizeof(WriteBarrier<JSObject>) * Checked<size_t>(allocatedLength())).unsafeGet());
    for (uint32_t i = 0; i < allocatedLength(); ++i)
        new(&m_jsFunctions.get()[i]) WriteBarrier<JSObject>();
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

    for (unsigned i = 0; i < thisObject->length(); ++i)
        visitor.append(thisObject->m_jsFunctions.get()[i]);
}

bool JSWebAssemblyTable::grow(uint32_t delta)
{
    if (delta == 0)
        return true;

    size_t oldLength = length();

    auto grew = m_table->grow(delta);
    if (!grew)
        return false;

    size_t newLength = grew.value();
    if (newLength > m_table->allocatedLength(oldLength))
        // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
        m_jsFunctions.realloc((sizeof(WriteBarrier<JSObject>) * Checked<size_t>(m_table->allocatedLength(newLength))).unsafeGet());

    for (size_t i = oldLength; i < m_table->allocatedLength(newLength); ++i)
        new (&m_jsFunctions.get()[i]) WriteBarrier<JSObject>();

    return true;
}

JSObject* JSWebAssemblyTable::getFunction(uint32_t index)
{
    RELEASE_ASSERT(index < length());
    return m_jsFunctions.get()[index & m_table->mask()].get();
}

void JSWebAssemblyTable::clearFunction(uint32_t index)
{
    m_table->clearFunction(index);
    m_jsFunctions.get()[index & m_table->mask()] = WriteBarrier<JSObject>();
}

void JSWebAssemblyTable::setFunction(VM& vm, uint32_t index, WebAssemblyFunction* function)
{
    m_table->setFunction(index, function->importableFunction(), &function->instance()->instance());
    m_jsFunctions.get()[index & m_table->mask()].set(vm, this, function);
}

void JSWebAssemblyTable::setFunction(VM& vm, uint32_t index, WebAssemblyWrapperFunction* function)
{
    m_table->setFunction(index, function->importableFunction(), &function->instance()->instance());
    m_jsFunctions.get()[index & m_table->mask()].set(vm, this, function);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
