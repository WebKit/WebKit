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
#include "WasmGlobal.h"
#include "WasmMemory.h"
#include "WasmModule.h"
#include "WasmTable.h"
#include "WriteBarrier.h"
#include <wtf/BitVector.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class LLIntOffsetsExtractor;
class JSWebAssemblyInstance;

namespace Wasm {

struct Context;
class Instance;

class Instance : public ThreadSafeRefCounted<Instance>, public CanMakeWeakPtr<Instance> {
    friend LLIntOffsetsExtractor;

public:
    using StoreTopCallFrameCallback = WTF::Function<void(void*)>;
    using FunctionWrapperMap = HashMap<uint32_t, WriteBarrier<Unknown>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

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
    Table* table(unsigned);
    void setTable(unsigned, Ref<Table>&&);

    void* cachedMemory() const { return m_cachedMemory.getMayBeNull(cachedMemorySize()); }
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
            m_cachedMemory = CagedPtr<Gigacage::Primitive, void, tagCagedPtr>(memory()->memory(), memory()->size());
            m_cachedMemorySize = memory()->size();
        }
    }

    int32_t loadI32Global(unsigned i) const
    {
        Global::Value* slot = m_globals.get() + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return 0;
        }
        return slot->m_primitive;
    }
    int64_t loadI64Global(unsigned i) const
    {
        Global::Value* slot = m_globals.get() + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return 0;
        }
        return slot->m_primitive;
    }
    float loadF32Global(unsigned i) const { return bitwise_cast<float>(loadI32Global(i)); }
    double loadF64Global(unsigned i) const { return bitwise_cast<double>(loadI64Global(i)); }
    void setGlobal(unsigned i, int64_t bits)
    {
        Global::Value* slot = m_globals.get() + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return;
        }
        slot->m_primitive = bits;
    }
    void setGlobal(unsigned, JSValue);
    void linkGlobal(unsigned, Ref<Global>&&);
    const BitVector& globalsToMark() { return m_globalsToMark; }
    const BitVector& globalsToBinding() { return m_globalsToBinding; }
    JSValue getFunctionWrapper(unsigned) const;
    typename FunctionWrapperMap::ValuesConstIteratorRange functionWrappers() const { return m_functionWrappers.values(); }
    void setFunctionWrapper(unsigned, JSValue);

    Wasm::Global* getGlobalBinding(unsigned i)
    {
        ASSERT(m_globalsToBinding.get(i));
        Wasm::Global::Value* pointer = m_globals.get()[i].m_pointer;
        if (!pointer)
            return nullptr;
        return &Wasm::Global::fromBinding(*pointer);
    }

    static ptrdiff_t offsetOfMemory() { return OBJECT_OFFSETOF(Instance, m_memory); }
    static ptrdiff_t offsetOfGlobals() { return OBJECT_OFFSETOF(Instance, m_globals); }
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
    static constexpr size_t offsetOfTail() { return WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(Instance)); }
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

    static_assert(sizeof(ImportFunctionInfo) == WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(ImportFunctionInfo)), "We rely on this for the alignment to be correct");
    static constexpr size_t offsetOfTablePtr(unsigned numImportFunctions, unsigned i) { return offsetOfTail() + sizeof(ImportFunctionInfo) * numImportFunctions + sizeof(Table*) * i; }

    void storeTopCallFrame(void* callFrame)
    {
        m_storeTopCallFrame(callFrame);
    }

private:
    Instance(Context*, Ref<Module>&&, EntryFrame**, void**, StoreTopCallFrameCallback&&);
    
    static size_t allocationSize(Checked<size_t> numImportFunctions, Checked<size_t> numTables)
    {
        return (offsetOfTail() + sizeof(ImportFunctionInfo) * numImportFunctions + sizeof(Table*) * numTables).unsafeGet();
    }
    void* m_owner { nullptr }; // In a JS embedding, this is a JSWebAssemblyInstance*.
    Context* m_context { nullptr };
    CagedPtr<Gigacage::Primitive, void, tagCagedPtr> m_cachedMemory;
    size_t m_cachedMemorySize { 0 };
    Ref<Module> m_module;
    RefPtr<CodeBlock> m_codeBlock;
    RefPtr<Memory> m_memory;

    MallocPtr<Global::Value, VMMalloc> m_globals;
    FunctionWrapperMap m_functionWrappers;
    BitVector m_globalsToMark;
    BitVector m_globalsToBinding;
    EntryFrame** m_pointerToTopEntryFrame { nullptr };
    void** m_pointerToActualStackLimit { nullptr };
    void* m_cachedStackLimit { bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()) };
    StoreTopCallFrameCallback m_storeTopCallFrame;
    unsigned m_numImportFunctions { 0 };
    HashMap<uint32_t, Ref<Global>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_linkedGlobals;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
