/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>
#include <type_traits>

namespace JSC { namespace Wasm {

uint32_t Table::allocatedLength(uint32_t length)
{
    return WTF::roundUpToPowerOfTwo(length);
}

void Table::setLength(uint32_t length)
{
    m_length = length;
    m_mask = WTF::maskForSize(length);
    ASSERT(isValidLength(length));
    ASSERT(m_mask == WTF::maskForSize(allocatedLength(length)));
}

Table::Table(uint32_t initial, Optional<uint32_t> maximum, TableElementType type)
    : m_type(type)
    , m_maximum(maximum)
    , m_owner(nullptr)
{
    setLength(initial);
    ASSERT(!m_maximum || *m_maximum >= m_length);

    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_jsValues = MallocPtr<WriteBarrier<Unknown>>::malloc((sizeof(WriteBarrier<Unknown>) * Checked<size_t>(allocatedLength(m_length))).unsafeGet());
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i) {
        new (&m_jsValues.get()[i]) WriteBarrier<Unknown>();
        m_jsValues.get()[i].setStartingValue(jsNull());
    }
}

RefPtr<Table> Table::tryCreate(uint32_t initial, Optional<uint32_t> maximum, TableElementType type)
{
    if (!isValidLength(initial))
        return nullptr;
    switch (type) {
    case TableElementType::Funcref:
        return adoptRef(new FuncRefTable(initial, maximum));
    case TableElementType::Anyref:
        return adoptRef(new Table(initial, maximum));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

Optional<uint32_t> Table::grow(uint32_t delta)
{
    RELEASE_ASSERT(m_owner);
    if (delta == 0)
        return length();

    auto locker = holdLock(m_owner->cellLock());

    using Checked = Checked<uint32_t, RecordOverflow>;
    Checked newLengthChecked = length();
    newLengthChecked += delta;
    uint32_t newLength;
    if (newLengthChecked.safeGet(newLength) == CheckedState::DidOverflow)
        return WTF::nullopt;

    if (maximum() && newLength > *maximum())
        return WTF::nullopt;
    if (!isValidLength(newLength))
        return WTF::nullopt;

    auto checkedGrow = [&] (auto& container, auto initializer) {
        if (newLengthChecked.unsafeGet() > allocatedLength(m_length)) {
            Checked reallocSizeChecked = allocatedLength(newLengthChecked.unsafeGet());
            reallocSizeChecked *= sizeof(*container.get());
            uint32_t reallocSize;
            if (reallocSizeChecked.safeGet(reallocSize) == CheckedState::DidOverflow)
                return false;
            // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
            container.realloc(reallocSize);
        }
        for (uint32_t i = m_length; i < allocatedLength(newLength); ++i) {
            new (&container.get()[i]) std::remove_reference_t<decltype(*container.get())>();
            initializer(container.get()[i]);
        }
        return true;
    };

    if (auto* funcRefTable = asFuncrefTable()) {
        if (!checkedGrow(funcRefTable->m_importableFunctions, [] (auto&) { }))
            return WTF::nullopt;
        if (!checkedGrow(funcRefTable->m_instances, [] (auto&) { }))
            return WTF::nullopt;
    }

    if (!checkedGrow(m_jsValues, [] (WriteBarrier<Unknown>& slot) { slot.setStartingValue(jsNull()); }))
        return WTF::nullopt;

    setLength(newLength);
    return newLength;
}

void Table::clear(uint32_t index)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    if (auto* funcRefTable = asFuncrefTable()) {
        funcRefTable->m_importableFunctions.get()[index & m_mask] = WasmToWasmImportableFunction();
        ASSERT(funcRefTable->m_importableFunctions.get()[index & m_mask].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
        funcRefTable->m_instances.get()[index & m_mask] = nullptr;
    }
    m_jsValues.get()[index & m_mask].setStartingValue(jsNull());
}

void Table::set(uint32_t index, JSValue value)
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(isAnyrefTable());
    RELEASE_ASSERT(m_owner);
    clear(index);
    m_jsValues.get()[index & m_mask].set(m_owner->vm(), m_owner, value);
}

JSValue Table::get(uint32_t index) const
{
    RELEASE_ASSERT(index < length());
    RELEASE_ASSERT(m_owner);
    return m_jsValues.get()[index & m_mask].get();
}

void Table::visitAggregate(SlotVisitor& visitor)
{
    RELEASE_ASSERT(m_owner);
    auto locker = holdLock(m_owner->cellLock());
    for (unsigned i = 0; i < m_length; ++i)
        visitor.append(m_jsValues.get()[i]);
}

FuncRefTable* Table::asFuncrefTable()
{
    return m_type == TableElementType::Funcref ? static_cast<FuncRefTable*>(this) : nullptr;
}

FuncRefTable::FuncRefTable(uint32_t initial, Optional<uint32_t> maximum)
    : Table(initial, maximum, TableElementType::Funcref)
{
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    m_importableFunctions = MallocPtr<WasmToWasmImportableFunction>::malloc((sizeof(WasmToWasmImportableFunction) * Checked<size_t>(allocatedLength(m_length))).unsafeGet());
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_instances = MallocPtr<Instance*>::malloc((sizeof(Instance*) * Checked<size_t>(allocatedLength(m_length))).unsafeGet());
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
        m_jsValues.get()[index & m_mask].set(m_owner->vm(), m_owner, optionalWrapper);
    m_importableFunctions.get()[index & m_mask] = function;
    m_instances.get()[index & m_mask] = instance;
}

} } // namespace JSC::Table

#endif // ENABLE(WEBASSEMBLY)
