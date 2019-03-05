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
#include "WasmMemory.h"
#include "WasmModule.h"
#include "WasmTable.h"
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

struct Context;

class Instance : public ThreadSafeRefCounted<Instance>, public CanMakeWeakPtr<Instance> {
public:
    using StoreTopCallFrameCallback = WTF::Function<void(void*)>;

    static Ref<Instance> create(Context*, Ref<Module>&&, EntryFrame** pointerToTopEntryFrame, void** pointerToActualStackLimit, StoreTopCallFrameCallback&&);

    void finalizeCreation(void* owner, Ref<CodeBlock>&& codeBlock)
    {
        m_owner = owner;
        m_codeBlock = WTFMove(codeBlock);
    }

    JS_EXPORT_PRIVATE ~Instance();

    template<typename T> T* owner() const { return reinterpret_cast<T*>(m_owner); }
    static ptrdiff_t offsetOfOwner() { return OBJECT_OFFSETOF(Instance, m_owner); }

    size_t extraMemoryAllocated() const;

    Wasm::Context* context() const { return m_context; }

    Module& module() { return m_module.get(); }
    CodeBlock* codeBlock() { return m_codeBlock.get(); }
    Memory* memory() { return m_memory.get(); }
    Table* table() { return m_table.get(); }

    void* cachedMemory() const { return m_cachedMemory; }
    size_t cachedMemorySize() const { return m_cachedMemorySize; }

    void setMemory(Ref<Memory>&& memory)
    {
        m_memory = WTFMove(memory);
        m_memory.get()->registerInstance(this);
        updateCachedMemory();
    }
    void updateCachedMemory()
    {
        if (m_memory != nullptr) {
            m_cachedMemory = memory()->memory();
            m_cachedMemorySize = memory()->size();
        }
    }
    void setTable(Ref<Table>&& table) { m_table = WTFMove(table); }

    int32_t loadI32Global(unsigned i) const { return m_globals.get()[i]; }
    int64_t loadI64Global(unsigned i) const { return m_globals.get()[i]; }
    float loadF32Global(unsigned i) const { return bitwise_cast<float>(loadI32Global(i)); }
    double loadF64Global(unsigned i) const { return bitwise_cast<double>(loadI64Global(i)); }
    void setGlobal(unsigned i, int64_t bits) { m_globals.get()[i] = bits; }

    static ptrdiff_t offsetOfMemory() { return OBJECT_OFFSETOF(Instance, m_memory); }
    static ptrdiff_t offsetOfGlobals() { return OBJECT_OFFSETOF(Instance, m_globals); }
    static ptrdiff_t offsetOfTable() { return OBJECT_OFFSETOF(Instance, m_table); }
    static ptrdiff_t offsetOfCachedMemory() { return OBJECT_OFFSETOF(Instance, m_cachedMemory); }
    static ptrdiff_t offsetOfCachedMemorySize() { return OBJECT_OFFSETOF(Instance, m_cachedMemorySize); }
    static ptrdiff_t offsetOfPointerToTopEntryFrame() { return OBJECT_OFFSETOF(Instance, m_pointerToTopEntryFrame); }

    static ptrdiff_t offsetOfPointerToActualStackLimit() { return OBJECT_OFFSETOF(Instance, m_pointerToActualStackLimit); }
    static ptrdiff_t offsetOfCachedStackLimit() { return OBJECT_OFFSETOF(Instance, m_cachedStackLimit); }
    void* cachedStackLimit() const
    {
        ASSERT(*m_pointerToActualStackLimit == m_cachedStackLimit);
        return m_cachedStackLimit;
    }
    void setCachedStackLimit(void* limit)
    {
        ASSERT(*m_pointerToActualStackLimit == limit || bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()) == limit);
        m_cachedStackLimit = limit;
    }

    // Tail accessors.
    static size_t offsetOfTail() { return WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(Instance)); }
    struct ImportFunctionInfo {
        // Target instance and entrypoint are only set for wasm->wasm calls, and are otherwise nullptr. The embedder-specific logic occurs through import function.
        Instance* targetInstance { nullptr };
        WasmToWasmImportableFunction::LoadLocation wasmEntrypointLoadLocation { nullptr };
        MacroAssemblerCodePtr<WasmEntryPtrTag> wasmToEmbedderStub;
        void* importFunction { nullptr }; // In a JS embedding, this is a WriteBarrier<JSObject>.
    };
    unsigned numImportFunctions() const { return m_numImportFunctions; }
    ImportFunctionInfo* importFunctionInfo(size_t importFunctionNum)
    {
        RELEASE_ASSERT(importFunctionNum < m_numImportFunctions);
        return &bitwise_cast<ImportFunctionInfo*>(bitwise_cast<char*>(this) + offsetOfTail())[importFunctionNum];
    }
    static size_t offsetOfTargetInstance(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, targetInstance); }
    static size_t offsetOfWasmEntrypointLoadLocation(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, wasmEntrypointLoadLocation); }
    static size_t offsetOfWasmToEmbedderStub(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, wasmToEmbedderStub); }
    static size_t offsetOfImportFunction(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(ImportFunctionInfo) + OBJECT_OFFSETOF(ImportFunctionInfo, importFunction); }
    template<typename T> T* importFunction(unsigned importFunctionNum) { return reinterpret_cast<T*>(&importFunctionInfo(importFunctionNum)->importFunction); }

    void storeTopCallFrame(void* callFrame)
    {
        m_storeTopCallFrame(callFrame);
    }

private:
    Instance(Context*, Ref<Module>&&, EntryFrame**, void**, StoreTopCallFrameCallback&&);
    
    static size_t allocationSize(Checked<size_t> numImportFunctions)
    {
        return (offsetOfTail() + sizeof(ImportFunctionInfo) * numImportFunctions).unsafeGet();
    }
    void* m_owner { nullptr }; // In a JS embedding, this is a JSWebAssemblyInstance*.
    Context* m_context { nullptr };
    void* m_cachedMemory { nullptr };
    size_t m_cachedMemorySize { 0 };
    Ref<Module> m_module;
    RefPtr<CodeBlock> m_codeBlock;
    RefPtr<Memory> m_memory;
    RefPtr<Table> m_table;
    MallocPtr<uint64_t> m_globals;
    EntryFrame** m_pointerToTopEntryFrame { nullptr };
    void** m_pointerToActualStackLimit { nullptr };
    void* m_cachedStackLimit { bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()) };
    StoreTopCallFrameCallback m_storeTopCallFrame;
    unsigned m_numImportFunctions { 0 };
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
