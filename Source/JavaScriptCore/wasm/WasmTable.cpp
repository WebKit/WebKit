/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "WasmTable.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include <wtf/CheckedArithmetic.h>
#include <type_traits>

namespace JSC { namespace Wasm {

template<typename Visitor> constexpr decltype(auto) Table::visitDerived(Visitor&& visitor)
{
    switch (type()) {
    case TableElementType::Externref:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<ExternRefTable&>(*this));
    case TableElementType::Funcref:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<FuncRefTable&>(*this));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor> constexpr decltype(auto) Table::visitDerived(Visitor&& visitor) const
{
    return const_cast<Table&>(*this).visitDerived([&](auto& value) {
        return std::invoke(std::forward<Visitor>(visitor), std::as_const(value));
    });
}

uint32_t Table::allocatedLength(uint32_t length)
{
    return WTF::roundUpToPowerOfTwo(length);
}

void Table::setLength(uint32_t length)
{
    m_length = length;
    ASSERT(isValidLength(length));
}

void Table::operator delete(Table* table, std::destroying_delete_t)
{
    table->visitDerived([](auto& table) {
        std::destroy_at(&table);
        std::decay_t<decltype(table)>::freeAfterDestruction(&table);
    });
}

Table::Table(uint32_t initial, std::optional<uint32_t> maximum, TableElementType type)
    : m_type(type)
    , m_maximum(maximum)
    , m_owner(nullptr)
{
    setLength(initial);
    ASSERT(!m_maximum || *m_maximum >= m_length);
}

RefPtr<Table> Table::tryCreate(uint32_t initial, std::optional<uint32_t> maximum, TableElementType type)
{
    if (!isValidLength(initial))
        return nullptr;
    switch (type) {
    case TableElementType::Externref:
        return adoptRef(new ExternRefTable(initial, maximum));
    case TableElementType::Funcref:
        return adoptRef(new FuncRefTable(initial, maximum));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

std::optional<uint32_t> Table::grow(uint32_t delta, JSValue defaultValue)
{
    RELEASE_ASSERT(m_owner);
    if (delta == 0)
        return length();

    Locker locker { m_owner->cellLock() };

    CheckedUint32 newLengthChecked = length();
    newLengthChecked += delta;
    if (newLengthChecked.hasOverflowed())
        return std::nullopt;

    uint32_t newLength = newLengthChecked;
    if (maximum() && newLength > *maximum())
        return std::nullopt;
    if (!isValidLength(newLength))
        return std::nullopt;

    auto checkedGrow = [&] (auto& container, auto initializer) {
        if (newLengthChecked > allocatedLength(m_length)) {
            CheckedUint32 reallocSizeChecked = allocatedLength(newLengthChecked);
            reallocSizeChecked *= sizeof(*container.get());
            if (reallocSizeChecked.hasOverflowed())
                return false;
            // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
            container.realloc(reallocSizeChecked);
        }
        for (uint32_t i = m_length; i < allocatedLength(newLength); ++i) {
            new (&container.get()[i]) std::remove_reference_t<decltype(*container.get())>();
            initializer(container.get()[i]);
        }
        return true;
    };

    VM& vm = m_owner->vm();
    switch (type()) {
    case TableElementType::Externref: {
        bool success = checkedGrow(static_cast<ExternRefTable*>(this)->m_jsValues, [&](auto& slot) {
            slot.set(vm, m_owner, defaultValue);
        });
        if (UNLIKELY(!success))
            return std::nullopt;
        break;
    }
    case TableElementType::Funcref: {
        bool success = checkedGrow(static_cast<FuncRefTable*>(this)->m_importableFunctions, [&](auto& slot) {
            slot.m_value.set(vm, m_owner, defaultValue);
        });
        if (UNLIKELY(!success))
            return std::nullopt;
        break;
    }
    }

    setLength(newLength);
    return newLength;
}

void Table::copy(const Table* srcTable, uint32_t dstIndex, uint32_t srcIndex)
{
    RELEASE_ASSERT(isExternrefTable());
    RELEASE_ASSERT(srcTable->isExternrefTable());

    set(dstIndex, srcTable->get(srcIndex));
}

void Table::clear(uint32_t index)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    visitDerived([&](auto& table) {
        table.clear(index);
    });
}

void Table::set(uint32_t index, JSValue value)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(isExternrefTable());
    RELEASE_ASSERT(m_owner);
    visitDerived([&](auto& table) {
        table.set(index, value);
    });
}

JSValue Table::get(uint32_t index) const
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    return visitDerived([&](auto& table) {
        return table.get(index);
    });
}

template<typename Visitor>
void Table::visitAggregateImpl(Visitor& visitor)
{
    RELEASE_ASSERT(m_owner);
    Locker locker { m_owner->cellLock() };
    switch (type()) {
    case TableElementType::Externref: {
        auto* table = static_cast<ExternRefTable*>(this);
        for (unsigned i = 0; i < m_length; ++i)
            visitor.append(table->m_jsValues.get()[i]);
        break;
    }
    case TableElementType::Funcref: {
        auto* table = static_cast<FuncRefTable*>(this);
        for (unsigned i = 0; i < m_length; ++i)
            visitor.append(table->m_importableFunctions.get()[i].m_value);
        break;
    }
    }
}

DEFINE_VISIT_AGGREGATE(Table);

Type Table::wasmType() const
{
    if (isExternrefTable())
        return externrefType();
    ASSERT(isFuncrefTable());
    return funcrefType();
}

FuncRefTable* Table::asFuncrefTable()
{
    return m_type == TableElementType::Funcref ? static_cast<FuncRefTable*>(this) : nullptr;
}

ExternRefTable::ExternRefTable(uint32_t initial, std::optional<uint32_t> maximum)
    : Table(initial, maximum, TableElementType::Externref)
{
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_jsValues = MallocPtr<WriteBarrier<Unknown>, VMMalloc>::malloc(sizeof(WriteBarrier<Unknown>) * Checked<size_t>(allocatedLength(m_length)));
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i)
        new (&m_jsValues.get()[i]) WriteBarrier<Unknown>(NullWriteBarrierTag);
}

void ExternRefTable::clear(uint32_t index)
{
    m_jsValues.get()[index].setStartingValue(jsNull());
}

void ExternRefTable::set(uint32_t index, JSValue value)
{
    m_jsValues.get()[index].set(m_owner->vm(), m_owner, value);
}

FuncRefTable::FuncRefTable(uint32_t initial, std::optional<uint32_t> maximum)
    : Table(initial, maximum, TableElementType::Funcref)
{
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_importableFunctions = MallocPtr<Function, VMMalloc>::malloc(sizeof(Function) * Checked<size_t>(allocatedLength(m_length)));
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i) {
        new (&m_importableFunctions.get()[i]) Function();
        ASSERT(m_importableFunctions.get()[i].m_function.typeIndex == Wasm::TypeDefinition::invalidIndex); // We rely on this in compiled code.
        ASSERT(!m_importableFunctions.get()[i].m_instance);
        ASSERT(m_importableFunctions.get()[i].m_value.isNull());
    }
}

void FuncRefTable::setFunction(uint32_t index, JSObject* optionalWrapper, WasmToWasmImportableFunction function, Instance* instance)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    auto& slot = m_importableFunctions.get()[index];
    slot.m_function = function;
    slot.m_instance = instance;
    if (optionalWrapper)
        slot.m_value.set(m_owner->vm(), m_owner, optionalWrapper);
    else
        slot.m_value.setWithoutWriteBarrier(jsNull());
}

const FuncRefTable::Function& FuncRefTable::function(uint32_t index) const
{
    return m_importableFunctions.get()[index];
}

void FuncRefTable::copyFunction(const FuncRefTable* srcTable, uint32_t dstIndex, uint32_t srcIndex)
{
    if (srcTable->get(srcIndex).isNull()) {
        clear(dstIndex);
        return;
    }

    auto& function = srcTable->function(srcIndex);
    setFunction(dstIndex, jsCast<JSObject*>(srcTable->get(srcIndex)), function.m_function, function.m_instance);
}

void FuncRefTable::clear(uint32_t index)
{
    m_importableFunctions.get()[index] = FuncRefTable::Function { };
    ASSERT(m_importableFunctions.get()[index].m_function.typeIndex == Wasm::TypeDefinition::invalidIndex); // We rely on this in compiled code.
    ASSERT(!m_importableFunctions.get()[index].m_instance);
    ASSERT(m_importableFunctions.get()[index].m_value.isNull());
}

void FuncRefTable::set(uint32_t index, JSValue value)
{
    clear(index);
    m_importableFunctions.get()[index].m_value.set(m_owner->vm(), m_owner, value);
}

} } // namespace JSC::Table

#endif // ENABLE(WEBASSEMBLY)
