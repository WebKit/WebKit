/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo JSWebAssemblyTable::s_info = { "WebAssembly.Table"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyTable) };

JSWebAssemblyTable* JSWebAssemblyTable::create(VM& vm, Structure* structure, Ref<Wasm::Table>&& table)
{
    auto* instance = new (NotNull, allocateCell<JSWebAssemblyTable>(vm)) JSWebAssemblyTable(vm, structure, WTFMove(table));
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

void JSWebAssemblyTable::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyTable*>(cell)->JSWebAssemblyTable::~JSWebAssemblyTable();
}

template<typename Visitor>
void JSWebAssemblyTable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWebAssemblyTable* thisObject = jsCast<JSWebAssemblyTable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    thisObject->table()->visitAggregate(visitor);
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyTable);

std::optional<uint32_t> JSWebAssemblyTable::grow(JSGlobalObject* globalObject, uint32_t delta, JSValue defaultValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint64_t wasmValue = 0;
    if (UNLIKELY(isExnref(m_table->wasmType()))) {
        if (!defaultValue.isNull()) {
            throwTypeError(globalObject, scope, "Table.grow cannot handle exnref table"_s);
            return { };
        }
        wasmValue = JSValue::encode(defaultValue);
    } else {
        wasmValue = toWebAssemblyValue(globalObject, m_table->wasmType(), defaultValue);
        RETURN_IF_EXCEPTION(scope, false);
    }

    return m_table->grow(delta, JSValue::decode(wasmValue));
}

JSValue JSWebAssemblyTable::get(JSGlobalObject* globalObject, uint32_t index)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(isExnref(m_table->wasmType()))) {
        throwTypeError(globalObject, scope, "Table.get cannot handle exnref table"_s);
        return { };
    }

    return m_table->get(index);
}

void JSWebAssemblyTable::set(JSGlobalObject* globalObject, uint32_t index, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(isExnref(m_table->wasmType()))) {
        throwTypeError(globalObject, scope, "Table.set cannot handle exnref table"_s);
        return;
    }

    uint64_t wasmValue = toWebAssemblyValue(globalObject, m_table->wasmType(), value);
    RETURN_IF_EXCEPTION(scope, void());

    if (index < length())
        return m_table->set(index, JSValue::decode(wasmValue));

    throwRangeError(globalObject, scope, "Table.set expects an index less than the length of the table"_s);
}

void JSWebAssemblyTable::set(uint32_t index, JSValue value)
{
    m_table->set(index, value);
}

void JSWebAssemblyTable::clear(uint32_t index)
{
    ASSERT(index < length());
    m_table->clear(index);
}

JSObject* JSWebAssemblyTable::type(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();

    Wasm::TableElementType element = m_table->type();
    JSString* elementString = nullptr;
    switch (element) {
    case Wasm::TableElementType::Funcref:
        if (m_table->wasmType().isNullable())
            elementString = jsNontrivialString(vm, "funcref"_s);
        else
            return nullptr;
        break;
    case Wasm::TableElementType::Externref:
        if (isExternref(m_table->wasmType()) && m_table->wasmType().isNullable())
            elementString = jsNontrivialString(vm, "externref"_s);
        else
            return nullptr;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    JSObject* result;
    auto maximum = m_table->maximum();
    if (maximum) {
        result = constructEmptyObject(globalObject, globalObject->objectPrototype(), 3);
        result->putDirect(vm, Identifier::fromString(vm, "maximum"_s), jsNumber(*maximum));
    } else
        result = constructEmptyObject(globalObject, globalObject->objectPrototype(), 2);

    uint32_t minimum = m_table->length();
    result->putDirect(vm, Identifier::fromString(vm, "minimum"_s), jsNumber(minimum));
    result->putDirect(vm, Identifier::fromString(vm, "element"_s), elementString);
    return result;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
