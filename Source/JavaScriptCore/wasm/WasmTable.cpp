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

uint32_t Table::allocatedLength(uint32_t length)
{
    return WTF::roundUpToPowerOfTwo(length);
}

void Table::setLength(uint32_t length)
{
    m_length = length;
    ASSERT(isValidLength(length));
}

Table::Table(uint32_t initial, std::optional<uint32_t> maximum, TableElementType type)
    : m_type(type)
    , m_maximum(maximum)
    , m_owner(nullptr)
{
    setLength(initial);
    ASSERT(!m_maximum || *m_maximum >= m_length);

    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_jsValues = MallocPtr<WriteBarrier<Unknown>, VMMalloc>::malloc(sizeof(WriteBarrier<Unknown>) * Checked<size_t>(allocatedLength(m_length)));
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i) {
        new (&m_jsValues.get()[i]) WriteBarrier<Unknown>();
        m_jsValues.get()[i].setStartingValue(jsNull());
    }
}

RefPtr<Table> Table::tryCreate(uint32_t initial, std::optional<uint32_t> maximum, TableElementType type)
{
    if (!isValidLength(initial))
        return nullptr;
    switch (type) {
    case TableElementType::Funcref:
        return adoptRef(new FuncRefTable(initial, maximum));
    case TableElementType::Externref:
        return adoptRef(new Table(initial, maximum));
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

    if (auto* funcRefTable = asFuncrefTable()) {
        if (!checkedGrow(funcRefTable->m_importableFunctions, [](auto&) { }))
            return std::nullopt;
        if (!checkedGrow(funcRefTable->m_instances, [](auto&) { }))
            return std::nullopt;
    }

    VM& vm = m_owner->vm();
    if (!checkedGrow(m_jsValues, [&](WriteBarrier<Unknown>& slot) { slot.set(vm, m_owner, defaultValue); }))
        return std::nullopt;

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
    if (auto* funcRefTable = asFuncrefTable()) {
        funcRefTable->m_importableFunctions.get()[index] = WasmToWasmImportableFunction();
        ASSERT(funcRefTable->m_importableFunctions.get()[index].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
        funcRefTable->m_instances.get()[index] = nullptr;
    }
    m_jsValues.get()[index].setStartingValue(jsNull());
}

void Table::set(uint32_t index, JSValue value)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(isExternrefTable());
    RELEASE_ASSERT(m_owner);
    clear(index);
    m_jsValues.get()[index].set(m_owner->vm(), m_owner, value);
}

JSValue Table::get(uint32_t index) const
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    return m_jsValues.get()[index].get();
}

template<typename Visitor>
void Table::visitAggregateImpl(Visitor& visitor)
{
    RELEASE_ASSERT(m_owner);
    Locker locker { m_owner->cellLock() };
    for (unsigned i = 0; i < m_length; ++i)
        visitor.append(m_jsValues.get()[i]);
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

FuncRefTable::FuncRefTable(uint32_t initial, std::optional<uint32_t> maximum)
    : Table(initial, maximum, TableElementType::Funcref)
{
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    m_importableFunctions = MallocPtr<WasmToWasmImportableFunction, VMMalloc>::malloc(sizeof(WasmToWasmImportableFunction) * Checked<size_t>(allocatedLength(m_length)));
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_instances = MallocPtr<Instance*, VMMalloc>::malloc(sizeof(Instance*) * Checked<size_t>(allocatedLength(m_length)));
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i) {
        new (&m_importableFunctions.get()[i]) WasmToWasmImportableFunction();
        ASSERT(m_importableFunctions.get()[i].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
        m_instances.get()[i] = nullptr;
    }
}

void FuncRefTable::setFunction(uint32_t index, JSObject* optionalWrapper, WasmToWasmImportableFunction function, Instance* instance)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    clear(index);
    if (optionalWrapper)
        m_jsValues.get()[index].set(m_owner->vm(), m_owner, optionalWrapper);
    m_importableFunctions.get()[index] = function;
    m_instances.get()[index] = instance;
}

const WasmToWasmImportableFunction& FuncRefTable::function(uint32_t index) const
{
    return m_importableFunctions.get()[index];
}

Instance* FuncRefTable::instance(uint32_t index) const
{
    return m_instances.get()[index];
}

void FuncRefTable::copyFunction(const FuncRefTable* srcTable, uint32_t dstIndex, uint32_t srcIndex)
{
    if (srcTable->get(srcIndex).isNull()) {
        clear(dstIndex);
        return;
    }

    setFunction(dstIndex, jsCast<JSObject*>(srcTable->get(srcIndex)), srcTable->function(srcIndex), srcTable->instance(srcIndex));
}

} } // namespace JSC::Table

#endif // ENABLE(WEBASSEMBLY)
