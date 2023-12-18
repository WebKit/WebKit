/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "SlotVisitorMacros.h"
#include "WasmFormat.h"
#include "WasmLimits.h"
#include "WriteBarrier.h"
#include <wtf/MallocPtr.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class JSWebAssemblyTable;

namespace Wasm {

class Instance;
class FuncRefTable;

class Table : public ThreadSafeRefCounted<Table> {
    WTF_MAKE_NONCOPYABLE(Table);
    WTF_MAKE_TZONE_ALLOCATED(Table);
public:
    static RefPtr<Table> tryCreate(uint32_t initial, std::optional<uint32_t> maximum, TableElementType, Type);

    JS_EXPORT_PRIVATE ~Table() = default;

    std::optional<uint32_t> maximum() const { return m_maximum; }
    uint32_t length() const { return m_length; }

    static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(Table, m_length); }

    static uint32_t allocatedLength(uint32_t length);

    JSWebAssemblyTable* owner() const { return m_owner; }
    void setOwner(JSWebAssemblyTable* owner)
    {
        ASSERT(!m_owner);
        ASSERT(owner);
        m_owner = owner;
    }

    TableElementType type() const { return m_type; }
    bool isExternrefTable() const { return m_type == TableElementType::Externref; }
    bool isFuncrefTable() const { return m_type == TableElementType::Funcref; }
    Type wasmType() const { return m_wasmType; }
    FuncRefTable* asFuncrefTable();

    static bool isValidLength(uint32_t length) { return length < maxTableEntries; }

    void clear(uint32_t);
    void set(uint32_t, JSValue);
    JSValue get(uint32_t) const;

    std::optional<uint32_t> grow(uint32_t delta, JSValue defaultValue);
    void copy(const Table* srcTable, uint32_t dstIndex, uint32_t srcIndex);

    DECLARE_VISIT_AGGREGATE;

    void operator delete(Table*, std::destroying_delete_t);

protected:
    Table(uint32_t initial, std::optional<uint32_t> maximum, Type, TableElementType = TableElementType::Externref);

    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&);
    template<typename Visitor> constexpr decltype(auto) visitDerived(Visitor&&) const;

    void setLength(uint32_t);

    bool isFixedSized() const { return m_isFixedSized; }

    uint32_t m_length;
    NO_UNIQUE_ADDRESS const std::optional<uint32_t> m_maximum;
    const TableElementType m_type;
    Type m_wasmType;
    bool m_isFixedSized { false };
    JSWebAssemblyTable* m_owner;
};

class ExternRefTable final : public Table {
    WTF_MAKE_TZONE_ALLOCATED(ExternRefTable);
public:
    friend class Table;

    void clear(uint32_t);
    void set(uint32_t, JSValue);
    JSValue get(uint32_t index) const { return m_jsValues.get()[index].get(); }

private:
    ExternRefTable(uint32_t initial, std::optional<uint32_t> maximum, Type wasmType);

    MallocPtr<WriteBarrier<Unknown>, VMMalloc> m_jsValues;
};

class FuncRefTable final : public Table {
    WTF_MAKE_TZONE_ALLOCATED(FuncRefTable);
public:
    friend class Table;

    JS_EXPORT_PRIVATE ~FuncRefTable();

    // call_indirect needs to do an Instance check to potentially context switch when calling a function to another instance. We can hold raw pointers to Instance here because the js ensures that Table keeps all the instances alive. We couldn't hold a Ref here because it would cause cycles.
    struct Function {
        WasmToWasmImportableFunction m_function;
        Instance* m_instance { nullptr };
        WriteBarrier<Unknown> m_value { NullWriteBarrierTag };

        static ptrdiff_t offsetOfFunction() { return OBJECT_OFFSETOF(Function, m_function); }
        static ptrdiff_t offsetOfInstance() { return OBJECT_OFFSETOF(Function, m_instance); }
        static ptrdiff_t offsetOfValue() { return OBJECT_OFFSETOF(Function, m_value); }
    };

    void setFunction(uint32_t, JSObject*, WasmToWasmImportableFunction, Instance*);
    const Function& function(uint32_t) const;
    void copyFunction(const FuncRefTable* srcTable, uint32_t dstIndex, uint32_t srcIndex);

    static ptrdiff_t offsetOfFunctions() { return OBJECT_OFFSETOF(FuncRefTable, m_importableFunctions); }
    static constexpr ptrdiff_t offsetOfTail() { return WTF::roundUpToMultipleOf<alignof(Function)>(sizeof(FuncRefTable)); }
    static constexpr ptrdiff_t offsetOfFunctionsForFixedSizedTable() { return offsetOfTail(); }

    static size_t allocationSize(uint32_t size)
    {
        return offsetOfTail() + sizeof(Function) * size;
    }

    void clear(uint32_t);
    void set(uint32_t, JSValue);
    JSValue get(uint32_t index) const { return m_importableFunctions.get()[index].m_value.get(); }

private:
    FuncRefTable(uint32_t initial, std::optional<uint32_t> maximum, Type wasmType);

    Function* tailPointer() { return bitwise_cast<Function*>(bitwise_cast<uint8_t*>(this) + offsetOfTail()); }

    static Ref<FuncRefTable> createFixedSized(uint32_t size, Type wasmType);

    MallocPtr<Function, VMMalloc> m_importableFunctions;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
