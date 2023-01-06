/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include <wtf/ThreadSafeWeakPtr.h>

namespace JSC {

class LLIntOffsetsExtractor;
class JSWebAssemblyInstance;

namespace Wasm {

class Instance;

class Instance : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Instance> {
    friend LLIntOffsetsExtractor;

public:
    using FunctionWrapperMap = HashMap<uint32_t, WriteBarrier<Unknown>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    static Ref<Instance> create(VM&, Ref<Module>&&);

    void setOwner(void* owner)
    {
        m_owner = owner;
    }

    JS_EXPORT_PRIVATE ~Instance();

    template<typename T> T* owner() const { return reinterpret_cast<T*>(m_owner); }
    static ptrdiff_t offsetOfOwner() { return OBJECT_OFFSETOF(Instance, m_owner); }
    static ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(Instance, m_vm); }

    size_t extraMemoryAllocated() const;

    VM& vm() const { return *m_vm; }
    Module& module() const { return m_module.get(); }
    CalleeGroup* calleeGroup() const { return module().calleeGroupFor(memory()->mode()); }
    Memory* memory() const { return m_memory.get(); }
    Table* table(unsigned);
    void setTable(unsigned, Ref<Table>&&);
    const Element* elementAt(unsigned) const;

    void initElementSegment(uint32_t tableIndex, const Element& segment, uint32_t dstOffset, uint32_t srcOffset, uint32_t length);

    bool isImportFunction(uint32_t functionIndex) const
    {
        return functionIndex < calleeGroup()->functionImportCount();
    }

    void tableInit(uint32_t dstOffset, uint32_t srcOffset, uint32_t length, uint32_t elementIndex, uint32_t tableIndex);

    void tableCopy(uint32_t dstOffset, uint32_t srcOffset, uint32_t length, uint32_t dstTableIndex, uint32_t srcTableIndex);

    void elemDrop(uint32_t elementIndex);

    bool memoryInit(uint32_t dstAddress, uint32_t srcAddress, uint32_t length, uint32_t dataSegmentIndex);

    void dataDrop(uint32_t dataSegmentIndex);

    void* cachedMemory() const { return m_cachedMemory.getMayBeNull(cachedBoundsCheckingSize()); }
    size_t cachedBoundsCheckingSize() const { return m_cachedBoundsCheckingSize; }

    void setMemory(Ref<Memory>&& memory)
    {
        m_memory = WTFMove(memory);
        m_memory.get()->registerInstance(*this);
        updateCachedMemory();
    }
    void updateCachedMemory()
    {
        if (m_memory != nullptr) {
            // Note: In MemoryMode::BoundsChecking, mappedCapacity() == size().
            // We assert this in the constructor of MemoryHandle.
#if CPU(ARM)
            // Shared memory requires signaling memory which is not available
            // on ARMv7 yet. In order to get more of the test suite to run, we
            // can still use a shared memory by using bounds checking, by using
            // the actual size here, but this means we cannot grow the shared
            // memory safely in case it's used by multiple threads. Once the
            // signal handler are available, m_cachedBoundsCheckingSize should
            // be set to use memory()->mappedCapacity() like other platforms,
            // and at that point growing the shared memory will be safe.
            m_cachedBoundsCheckingSize = memory()->size();
#else
            m_cachedBoundsCheckingSize = memory()->mappedCapacity();
#endif
            m_cachedMemory = CagedPtr<Gigacage::Primitive, void, tagCagedPtr>(memory()->basePointer(), m_cachedBoundsCheckingSize);
            ASSERT(memory()->basePointer() == cachedMemory());
        }
    }

    int32_t loadI32Global(unsigned i) const
    {
        Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return 0;
        }
        return slot->m_primitive;
    }
    int64_t loadI64Global(unsigned i) const
    {
        Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return 0;
        }
        return slot->m_primitive;
    }
    void setGlobal(unsigned i, int64_t bits)
    {
        Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return;
        }
        slot->m_primitive = bits;
    }

    v128_t loadV128Global(unsigned i) const
    {
        Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return { };
        }
        return slot->m_vector;
    }
    void setGlobal(unsigned i, v128_t bits)
    {
        Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return;
        }
        slot->m_vector = bits;
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
        Global::Value* pointer = m_globals[i].m_pointer;
        if (!pointer)
            return nullptr;
        return &Wasm::Global::fromBinding(*pointer);
    }

    static ptrdiff_t offsetOfMemory() { return OBJECT_OFFSETOF(Instance, m_memory); }
    static ptrdiff_t offsetOfGlobals() { return OBJECT_OFFSETOF(Instance, m_globals); }
    static ptrdiff_t offsetOfCachedMemory() { return OBJECT_OFFSETOF(Instance, m_cachedMemory); }
    static ptrdiff_t offsetOfCachedBoundsCheckingSize() { return OBJECT_OFFSETOF(Instance, m_cachedBoundsCheckingSize); }

    // Tail accessors.
    static constexpr size_t offsetOfTail() { return WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(Instance)); }
    struct ImportFunctionInfo {
        // Target instance and entrypoint are only set for wasm->wasm calls, and are otherwise nullptr. The embedder-specific logic occurs through import function.
        Instance* targetInstance { nullptr };
        WasmToWasmImportableFunction::LoadLocation wasmEntrypointLoadLocation { nullptr };
        CodePtr<WasmEntryPtrTag> wasmToEmbedderStub;
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
    static constexpr size_t offsetOfGlobalPtr(unsigned numImportFunctions, unsigned numTables, unsigned i) { return roundUpToMultipleOf<sizeof(Global::Value)>(offsetOfTail() + sizeof(ImportFunctionInfo) * numImportFunctions + sizeof(Table*) * numTables) + sizeof(Global::Value) * i; }

    void storeTopCallFrame(void* callFrame)
    {
        vm().topCallFrame = bitwise_cast<CallFrame*>(callFrame);
    }

    const Tag& tag(unsigned i) const { return *m_tags[i]; }
    void setTag(unsigned, Ref<const Tag>&&);

private:
    Instance(VM&, Ref<Module>&&);
    
    static size_t allocationSize(Checked<size_t> numImportFunctions, Checked<size_t> numTables, Checked<size_t> numGlobals)
    {
        return roundUpToMultipleOf<sizeof(Global::Value)>(offsetOfTail() + sizeof(ImportFunctionInfo) * numImportFunctions + sizeof(Table*) * numTables) + sizeof(Global::Value) * numGlobals;
    }
    void* m_owner { nullptr }; // In a JS embedding, this is a JSWebAssemblyInstance*.
    VM* m_vm;
    CagedPtr<Gigacage::Primitive, void, tagCagedPtr> m_cachedMemory;
    size_t m_cachedBoundsCheckingSize { 0 };
    Ref<Module> m_module;
    RefPtr<Memory> m_memory;

    Global::Value* m_globals { nullptr };
    FunctionWrapperMap m_functionWrappers;
    BitVector m_globalsToMark;
    BitVector m_globalsToBinding;
    unsigned m_numImportFunctions { 0 };
    HashMap<uint32_t, Ref<Global>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_linkedGlobals;
    BitVector m_passiveElements;
    BitVector m_passiveDataSegments;
    FixedVector<RefPtr<const Tag>> m_tags;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
