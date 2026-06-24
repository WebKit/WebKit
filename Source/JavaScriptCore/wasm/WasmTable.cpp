/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include "IntegrityInlines.h"
#include "JSCJSValueInlines.h"
#include "JSWebAssemblyInstance.h"
#include "WasmTypeDefinitionInlines.h"
#include "WeakGCSetInlines.h"
#include <type_traits>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Table);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ExternOrAnyRefTable);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FuncRefTable);

template<typename Visitor> constexpr decltype(auto) Table::visitDerived(Visitor&& visitor)
{
    switch (type()) {
    case TableElementType::Externref:
        return std::invoke(std::forward<Visitor>(visitor), static_cast<ExternOrAnyRefTable&>(*this));
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
    return roundUpToPowerOfTwo(length);
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

Table::Table(uint32_t initial, std::optional<uint32_t> maximum, Type wasmType, TableElementType type)
    : m_maximum(maximum)
    , m_type(type)
    , m_wasmType(wasmType)
    , m_wasmTypeRTT(TypeInformation::tryGetRTT(wasmType.index))
    , m_isFixedSized(maximum && maximum.value() == initial)
    , m_owner(nullptr)
{
    setLength(initial);
    ASSERT(!m_maximum || *m_maximum >= m_length);
}

RefPtr<Table> Table::tryCreate(VM& vm, uint32_t initial, std::optional<uint32_t> maximum, TableElementType type, Type wasmType)
{
    if (!isValidLength(initial))
        return nullptr;
    switch (type) {
    case TableElementType::Externref:
        return adoptRef(new ExternOrAnyRefTable(initial, maximum, wasmType));
    case TableElementType::Funcref: {
        if (maximum && maximum.value() == initial) {
            // If the table is fixed-sized, we should put table slots inline to avoid one-level indirection.
            return FuncRefTable::createFixedSized(vm, initial, wasmType);
        }
        return adoptRef(new FuncRefTable(vm, initial, maximum, wasmType));
    }
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
        bool success = checkedGrow(static_cast<ExternOrAnyRefTable*>(this)->m_jsValues, [&](auto& slot) {
            slot.set(vm, m_owner, defaultValue);
        });
        if (!success) [[unlikely]]
            return std::nullopt;
        setLength(newLength);
        break;
    }
    case TableElementType::Funcref: {
        auto* funcTable = static_cast<FuncRefTable*>(this);
        auto* defaultFunction = dynamicDowncast<WebAssemblyFunctionBase>(defaultValue);
        ASSERT(defaultFunction || defaultValue.isNull());
        bool success = checkedGrow(funcTable->m_importableFunctions, [&](auto& slot) {
            if (defaultFunction)
                slot = defaultFunction->importableFunction();
        });
        if (!success) [[unlikely]]
            return std::nullopt;
        success = checkedGrow(funcTable->m_wrappers, [&](auto& slot) {
            if (defaultFunction)
                slot.set(vm, m_owner, defaultFunction);
        });
        RELEASE_ASSERT_WITH_MESSAGE(success, "First grow size succeeded so the second should too");
        setLength(newLength);
        for (auto& instance : funcTable->m_instances) {
            if (auto* strongReference = instance.get())
                strongReference->updateCachedTable0();
        }
        break;
    }
    }
    return newLength;
}

void Table::copy(Table* srcTable, uint32_t dstIndex, uint32_t srcIndex)
{
    ASSERT(isExternrefTable());
    ASSERT(srcTable->isExternrefTable());

    set(dstIndex, srcTable->get(srcIndex));
}

void Table::clear(uint32_t index)
{
    ASSERT(index < length());
    ASSERT(m_owner);
    visitDerived([&](auto& table) {
        table.clear(index);
    });
}

void Table::set(uint32_t index, JSValue value)
{
    ASSERT(index < length());
    ASSERT(m_owner);
    Integrity::auditCell<Integrity::AuditLevel::Full>(owner()->vm(), value);
    visitDerived([&](auto& table) {
        table.set(index, value);
    });
}

JSValue Table::get(uint32_t index)
{
    ASSERT(index < length());
    ASSERT(m_owner);
    if (auto* funcTable = asFuncrefTable()) {
        auto* wrapper = funcTable->get(index);
        return wrapper ? JSValue(wrapper) : jsNull();
    }
    return static_cast<ExternOrAnyRefTable*>(this)->get(index);
}

template<typename Visitor>
void Table::visitAggregateImpl(Visitor& visitor)
{
    RELEASE_ASSERT(m_owner);
    Locker locker { m_owner->cellLock() };
    switch (type()) {
    case TableElementType::Externref: {
        auto* table = static_cast<ExternOrAnyRefTable*>(this);
        for (unsigned i = 0; i < m_length; ++i)
            visitor.append(table->m_jsValues.get()[i]);
        break;
    }
    case TableElementType::Funcref: {
        auto* table = static_cast<FuncRefTable*>(this);
        for (unsigned i = 0; i < m_length; ++i) {
            visitor.append(table->m_wrappers.get()[i]);
            visitor.append(table->m_importableFunctions.get()[i].targetInstance);
        }
        break;
    }
    }
}

DEFINE_VISIT_AGGREGATE(Table);

FuncRefTable* Table::asFuncrefTable()
{
    return m_type == TableElementType::Funcref ? static_cast<FuncRefTable*>(this) : nullptr;
}

ExternOrAnyRefTable::ExternOrAnyRefTable(uint32_t initial, std::optional<uint32_t> maximum, Type wasmType)
    : Table(initial, maximum, wasmType, TableElementType::Externref)
{
    RELEASE_ASSERT(isRefType(wasmType));
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_jsValues = MallocPtr<WriteBarrier<Unknown>, VMMalloc>::malloc(sizeof(WriteBarrier<Unknown>) * Checked<size_t>(allocatedLength(m_length)));
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i)
        new (&m_jsValues.get()[i]) WriteBarrier<Unknown>(NullWriteBarrierTag);
}

void ExternOrAnyRefTable::clear(uint32_t index)
{
    m_jsValues.get()[index].setStartingValue(jsNull());
}

void ExternOrAnyRefTable::set(uint32_t index, JSValue value)
{
    m_jsValues.get()[index].set(m_owner->vm(), m_owner, value);
}

FuncRefTable::FuncRefTable(VM& vm, uint32_t initial, std::optional<uint32_t> maximum, Type wasmType)
    : Table(initial, maximum, wasmType, TableElementType::Funcref)
    , m_instances(vm)
{
    ASSERT(isSubtype(wasmType, funcrefType()));
    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    if (isFixedSized())
        m_importableFunctions = adoptMallocPtr<Function, VMMalloc>(tailPointer());
    else
        m_importableFunctions = MallocPtr<Function, VMMalloc>::malloc(sizeof(Function) * Checked<size_t>(allocatedLength(m_length)));

    m_wrappers = MallocPtr<WriteBarrier<WebAssemblyFunctionBase>, VMMalloc>::malloc(sizeof(WriteBarrier<WebAssemblyFunctionBase>) * Checked<size_t>(allocatedLength(m_length)));

    for (uint32_t i = 0; i < allocatedLength(m_length); ++i) {
        new (&m_importableFunctions.get()[i]) Function();
        ASSERT(m_importableFunctions.get()[i].isEmpty()); // We rely on this in compiled code.
        new (&m_wrappers.get()[i]) WriteBarrier<WebAssemblyFunctionBase>();
    }
}

FuncRefTable::~FuncRefTable()
{
    // If FuncRefTable is fixed-sized, this pointer is not managed by this handle.
    if (isFixedSized()) {
        auto* unmanagedPointer = m_importableFunctions.leakPtr();
        UNUSED_PARAM(unmanagedPointer);
    }
}

Ref<FuncRefTable> FuncRefTable::createFixedSized(VM& vm, uint32_t size, Type wasmType)
{
    return adoptRef(*new (NotNull, fastMalloc(allocationSize(allocatedLength(size)))) FuncRefTable(vm, size, size, wasmType));
}

void FuncRefTable::setFunction(uint32_t index, WebAssemblyFunctionBase* function)
{
    ASSERT(index < length());
    ASSERT_WITH_SECURITY_IMPLICATION(isSubtype(function->type(), wasmType()));
    VM& vm = function->instance()->vm();
    auto& slot = m_importableFunctions.get()[index];
    slot = function->importableFunction();
    m_wrappers.get()[index].set(vm, m_owner, function);
}

void FuncRefTable::setLazy(uint32_t index, JSWebAssemblyInstance* targetInstance, FunctionSpaceIndex functionIndex)
{
    ASSERT(index < length());
    auto* calleeGroup = targetInstance->calleeGroup();
    auto wasmCallee = calleeGroup->wasmCalleeFromFunctionIndexSpace(functionIndex);
    ASSERT(wasmCallee);

    auto& slot = m_importableFunctions.get()[index];
    slot.boxedCallee = wasmCallee.get();
    slot.targetInstance.set(m_owner->vm(), m_owner, targetInstance);
    slot.entrypointLoadLocation = calleeGroup->entrypointLoadLocationFromFunctionIndexSpace(functionIndex);
    slot.rtt = &targetInstance->module().rttFromFunctionIndexSpace(functionIndex);

    m_wrappers.get()[index].clear();
}

WebAssemblyFunctionBase* FuncRefTable::get(uint32_t index)
{
    auto& slot = m_importableFunctions.get()[index];
    if (auto* cached = m_wrappers.get()[index].get())
        return cached;
    if (slot.isEmpty())
        return nullptr;

    auto* wasmCallee = uncheckedDowncast<Wasm::Callee>(slot.boxedCallee.asNativeCallee());
    FunctionSpaceIndex functionIndex = wasmCallee->index();
    JSWebAssemblyInstance* targetInstance = slot.targetInstance.get();
    ASSERT(targetInstance);
    auto* wrapper = uncheckedDowncast<WebAssemblyFunctionBase>(targetInstance->ensureFunctionWrapper(functionIndex));
    ASSERT(wrapper->isCallable());
    m_wrappers.get()[index].set(m_owner->vm(), m_owner, wrapper);
    return wrapper;
}

const FuncRefTable::Function& FuncRefTable::function(uint32_t index) const
{
    return m_importableFunctions.get()[index];
}

void FuncRefTable::copyFunction(FuncRefTable* srcTable, uint32_t dstIndex, uint32_t srcIndex)
{
    ASSERT(dstIndex < length());
    const auto& srcSlot = srcTable->m_importableFunctions.get()[srcIndex];
    if (srcSlot.isEmpty()) {
        clear(dstIndex);
        return;
    }

    m_importableFunctions.get()[dstIndex] = srcSlot;
    m_wrappers.get()[dstIndex].copyFrom(srcTable->m_wrappers.get()[srcIndex]);
    // Write barrier our owner for good measure.
    m_owner->vm().writeBarrier(m_owner);
}

void FuncRefTable::clear(uint32_t index)
{
    ASSERT(wasmType().isNullable());
    m_importableFunctions.get()[index] = FuncRefTable::Function { };
    ASSERT(m_importableFunctions.get()[index].isEmpty()); // We rely on this in compiled code.
    m_wrappers.get()[index].clear();
}

void FuncRefTable::set(uint32_t index, JSValue value)
{
    if (value.isNull())
        clear(index);
    else
        setFunction(index, uncheckedDowncast<WebAssemblyFunctionBase>(value));
}

void FuncRefTable::registerInstance(JSWebAssemblyInstance& instance)
{
    m_instances.add(&instance);
}

} } // namespace JSC::Table

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
