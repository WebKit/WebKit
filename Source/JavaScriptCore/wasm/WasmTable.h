/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "WasmFormat.h"
#include "WasmLimits.h"
#include "WriteBarrier.h"
#include <wtf/MallocPtr.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

class Instance;
class FuncRefTable;

class Table : public ThreadSafeRefCounted<Table> {
    WTF_MAKE_NONCOPYABLE(Table);
    WTF_MAKE_FAST_ALLOCATED(Table);
public:
    static RefPtr<Table> tryCreate(uint32_t initial, Optional<uint32_t> maximum, TableElementType);

    JS_EXPORT_PRIVATE ~Table() = default;

    Optional<uint32_t> maximum() const { return m_maximum; }
    uint32_t length() const { return m_length; }

    static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(Table, m_length); }
    static ptrdiff_t offsetOfMask() { return OBJECT_OFFSETOF(Table, m_mask); }

    static uint32_t allocatedLength(uint32_t length);
    uint32_t mask() const { return m_mask; }

    template<typename T> T* owner() const { return reinterpret_cast<T*>(m_owner); }
    void setOwner(JSObject* owner)
    {
        ASSERT(!m_owner);
        ASSERT(owner);
        m_owner = owner;
    }

    TableElementType type() const { return m_type; }
    bool isAnyrefTable() const { return m_type == TableElementType::Anyref; }
    FuncRefTable* asFuncrefTable();

    static bool isValidLength(uint32_t length) { return length < maxTableEntries; }

    void clear(uint32_t);
    void set(uint32_t, JSValue);
    JSValue get(uint32_t) const;

    Optional<uint32_t> grow(uint32_t delta);

    void visitAggregate(SlotVisitor&);

protected:
    Table(uint32_t initial, Optional<uint32_t> maximum, TableElementType = TableElementType::Anyref);

    void setLength(uint32_t);

    uint32_t m_length;
    uint32_t m_mask;
    const TableElementType m_type;
    const Optional<uint32_t> m_maximum;

    MallocPtr<WriteBarrier<Unknown>> m_jsValues;
    JSObject* m_owner;
};

class FuncRefTable : public Table {
public:
    JS_EXPORT_PRIVATE ~FuncRefTable() = default;

    void setFunction(uint32_t, JSObject*, WasmToWasmImportableFunction, Instance*);

    static ptrdiff_t offsetOfFunctions() { return OBJECT_OFFSETOF(FuncRefTable, m_importableFunctions); }
    static ptrdiff_t offsetOfInstances() { return OBJECT_OFFSETOF(FuncRefTable, m_instances); }

private:
    FuncRefTable(uint32_t initial, Optional<uint32_t> maximum);

    MallocPtr<WasmToWasmImportableFunction> m_importableFunctions;
    // call_indirect needs to do an Instance check to potentially context switch when calling a function to another instance. We can hold raw pointers to Instance here because the embedder ensures that Table keeps all the instances alive. We couldn't hold a Ref here because it would cause cycles.
    MallocPtr<Instance*> m_instances;

    friend class Table;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
