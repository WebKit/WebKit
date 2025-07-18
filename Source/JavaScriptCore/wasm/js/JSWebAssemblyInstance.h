/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "CallLinkInfo.h"
#include "JSDestructibleObject.h"
#include "JSWebAssemblyGlobal.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyTable.h"
#include "WasmCalleeGroup.h"
#include "WasmCreationMode.h"
#include "WasmFormat.h"
#include "WasmGlobal.h"
#include "WasmMemory.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmTable.h"
#include "WebAssemblyFunction.h"
#include "WriteBarrier.h"
#include <wtf/BitVector.h>
#include <wtf/FixedVector.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class JSModuleNamespaceObject;
class JSWebAssemblyArray;
class JSWebAssemblyModule;
class WebAssemblyGCStructure;
class WebAssemblyModuleRecord;

// The layout of a JSWebAssemblyInstance is
//     { struct JSWebAssemblyInstance }[ WasmOrJSImportableFunctionCallLinkInfo ][ Wasm::Table* ][ Global::Value ][ WebAssemblyGCStructure* ][ Allocator* ]
// in a compound TrailingArray-like format.
class JSWebAssemblyInstance final : public JSNonFinalObject {
    friend class LLIntOffsetsExtractor;
    using WasmOrJSImportableFunctionCallLinkInfo = Wasm::WasmOrJSImportableFunctionCallLinkInfo;

public:
    using Base = JSNonFinalObject;
    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess mode>
    static GCClient::PreciseSubspace* subspaceFor(VM& vm)
    {
        return vm.webAssemblyInstanceSpace<mode>();
    }

    static Identifier createPrivateModuleKey();

    static JSWebAssemblyInstance* tryCreate(VM&, Structure*, JSGlobalObject*, const Identifier& moduleKey, JSWebAssemblyModule*, JSObject* importObject, Wasm::CreationMode, RefPtr<SourceProvider>&&);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_EXPORT_INFO;

    void initializeImports(JSGlobalObject*, JSObject* importObject, Wasm::CreationMode);
    void finalizeCreation(VM&, JSGlobalObject*, Ref<Wasm::CalleeGroup>&&, Wasm::CreationMode);
    
    WebAssemblyModuleRecord* moduleRecord() { return m_moduleRecord.get(); }

    JSWebAssemblyMemory* memory() const { return m_memory.get(); }
    void setMemory(VM& vm, JSWebAssemblyMemory* value)
    {
        m_memory.set(vm, this, value);
        memory()->memory().registerInstance(*this);
        updateCachedMemory();
    }
    MemoryMode memoryMode() const { return memory()->memory().mode(); }

    JSWebAssemblyTable* jsTable(unsigned i) { return m_tables[i].get(); }
    void setTable(VM& vm, uint32_t index, JSWebAssemblyTable* value)
    {
        ASSERT(index < m_tables.size());
        ASSERT(!table(index));
        m_tables[index].set(vm, this, value);
        setTable(index, *value->table());
    }

    void linkGlobal(VM& vm, uint32_t index, JSWebAssemblyGlobal* value)
    {
        ASSERT(value == value->global()->owner());
        linkGlobal(index, *value->global());
        vm.writeBarrier(this, value);
    }

    JSWebAssemblyModule* jsModule() const { return m_jsModule.get(); }
    const Wasm::ModuleInformation& moduleInformation() const { return m_module->moduleInformation(); }

    void clearJSCallICs(VM&);
    void finalizeUnconditionally(VM&, CollectionScope);

    static constexpr ptrdiff_t offsetOfJSModule() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_jsModule); }
    static constexpr ptrdiff_t offsetOfJSMemory() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_memory); }
    static constexpr ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_vm); }
    static constexpr ptrdiff_t offsetOfModuleRecord() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_moduleRecord); }


    using FunctionWrapperMap = UncheckedKeyHashMap<uint32_t, WriteBarrier<Unknown>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    static constexpr ptrdiff_t offsetOfSoftStackLimit() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_softStackLimit); }

    void updateSoftStackLimit(void* softStackLimit) { m_softStackLimit = softStackLimit; }

    Wasm::Module& module() const { return m_module.get(); }
    SourceTaintedOrigin taintedness() const { return m_sourceProvider->sourceTaintedOrigin(); }
    URL sourceURL() const { return m_sourceProvider->sourceOrigin().url(); }
    Wasm::CalleeGroup* calleeGroup() const { return module().calleeGroupFor(memoryMode()); }
    Wasm::Table* table(unsigned);
    void setTable(unsigned, Ref<Wasm::Table>&&);
    const Wasm::Element* elementAt(unsigned) const;

    // FIXME: Make this take a span.
    void initElementSegment(uint32_t tableIndex, const Wasm::Element& segment, uint32_t dstOffset, uint32_t srcOffset, uint32_t length);
    bool copyDataSegment(JSWebAssemblyArray*, uint32_t segmentIndex, uint32_t offset, uint32_t lengthInBytes, uint8_t* values);
    void copyElementSegment(JSWebAssemblyArray*, const Wasm::Element& segment, uint32_t srcOffset, uint32_t length, uint64_t* values);

    bool isImportFunction(uint32_t functionIndex) const
    {
        return functionIndex < calleeGroup()->functionImportCount();
    }

    void tableInit(uint32_t dstOffset, uint32_t srcOffset, uint32_t length, uint32_t elementIndex, uint32_t tableIndex);

    void tableCopy(uint32_t dstOffset, uint32_t srcOffset, uint32_t length, uint32_t dstTableIndex, uint32_t srcTableIndex);

    void elemDrop(uint32_t elementIndex);

    bool memoryInit(uint32_t dstAddress, uint32_t srcAddress, uint32_t length, uint32_t dataSegmentIndex);

    void dataDrop(uint32_t dataSegmentIndex);

    void* cachedMemory() const { return m_cachedMemory.getMayBeNull(); }
    size_t cachedBoundsCheckingSize() const { return m_cachedBoundsCheckingSize; }

    void updateCachedMemory()
    {
        if (m_memory) {
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
            m_cachedBoundsCheckingSize = memory()->memory().size();
#else
            m_cachedBoundsCheckingSize = memory()->memory().mappedCapacity();
#endif
            m_cachedMemory = CagedPtr<Gigacage::Primitive, void>(memory()->memory().basePointer());
            ASSERT(memory()->memory().basePointer() == cachedMemory());
        }
    }

    int32_t loadI32Global(unsigned i) const
    {
        Wasm::Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return 0;
        }
        return slot->m_primitive;
    }
    int64_t loadI64Global(unsigned i) const
    {
        Wasm::Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return 0;
        }
        return slot->m_primitive;
    }
    void setGlobal(unsigned i, int64_t bits)
    {
        Wasm::Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return;
        }
        slot->m_primitive = bits;
    }

    v128_t loadV128Global(unsigned i) const
    {
        Wasm::Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return { };
        }
        return slot->m_vector;
    }
    void setGlobal(unsigned i, v128_t bits)
    {
        Wasm::Global::Value* slot = m_globals + i;
        if (m_globalsToBinding.get(i)) {
            slot = slot->m_pointer;
            if (!slot)
                return;
        }
        slot->m_vector = bits;
    }
    void setGlobal(unsigned, JSValue);
    void linkGlobal(unsigned, Ref<Wasm::Global>&&);
    const BitVector& globalsToMark() { return m_globalsToMark; }
    const BitVector& globalsToBinding() { return m_globalsToBinding; }
    JSValue getFunctionWrapper(unsigned) const;
    typename FunctionWrapperMap::ValuesConstIteratorRange functionWrappers() const { return m_functionWrappers.values(); }
    void setFunctionWrapper(unsigned, JSValue);

    Wasm::Global* getGlobalBinding(unsigned i)
    {
        ASSERT(m_globalsToBinding.get(i));
        Wasm::Global::Value* pointer = m_globals[i].m_pointer;
        if (!pointer)
            return nullptr;
        return &Wasm::Global::fromBinding(*pointer);
    }

    static constexpr ptrdiff_t offsetOfGlobals() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_globals); }
    static constexpr ptrdiff_t offsetOfCachedMemory() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedMemory); }
    static constexpr ptrdiff_t offsetOfCachedBoundsCheckingSize() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedBoundsCheckingSize); }
    static constexpr ptrdiff_t offsetOfTemporaryCallFrame() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_temporaryCallFrame); }

    // Tail accessors.
    static constexpr size_t offsetOfTail() { return WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(JSWebAssemblyInstance)); }

    unsigned numImportFunctions() const { return m_numImportFunctions; }
    WasmOrJSImportableFunctionCallLinkInfo* importFunctionInfo(size_t importFunctionNum)
    {
        RELEASE_ASSERT(importFunctionNum < m_numImportFunctions);
        return &std::bit_cast<WasmOrJSImportableFunctionCallLinkInfo*>(std::bit_cast<char*>(this) + offsetOfTail())[importFunctionNum];
    }
    static size_t offsetOfTargetInstance(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(WasmOrJSImportableFunctionCallLinkInfo) + OBJECT_OFFSETOF(Wasm::WasmOrJSImportableFunctionCallLinkInfo, targetInstance); }
    static size_t offsetOfEntrypointLoadLocation(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(WasmOrJSImportableFunctionCallLinkInfo) + OBJECT_OFFSETOF(Wasm::WasmOrJSImportableFunctionCallLinkInfo, entrypointLoadLocation); }
    static size_t offsetOfBoxedWasmCalleeLoadLocation(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(WasmOrJSImportableFunctionCallLinkInfo) + OBJECT_OFFSETOF(Wasm::WasmOrJSImportableFunctionCallLinkInfo, boxedWasmCalleeLoadLocation); }
    static size_t offsetOfImportFunctionStub(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(WasmOrJSImportableFunctionCallLinkInfo) + OBJECT_OFFSETOF(WasmOrJSImportableFunctionCallLinkInfo, importFunctionStub); }
    static size_t offsetOfImportFunction(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(WasmOrJSImportableFunctionCallLinkInfo) + OBJECT_OFFSETOF(WasmOrJSImportableFunctionCallLinkInfo, importFunction); }
    static size_t offsetOfCallLinkInfo(size_t importFunctionNum) { return offsetOfTail() + importFunctionNum * sizeof(WasmOrJSImportableFunctionCallLinkInfo) + WasmOrJSImportableFunctionCallLinkInfo::offsetOfCallLinkInfo(); }
    WriteBarrier<JSObject>& importFunction(unsigned importFunctionNum) { return importFunctionInfo(importFunctionNum)->importFunction; }

    static_assert(sizeof(WasmOrJSImportableFunctionCallLinkInfo) == WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(WasmOrJSImportableFunctionCallLinkInfo)), "We rely on this for the alignment to be correct");
    static constexpr size_t offsetOfTablePtr(unsigned numImportFunctions, unsigned i) { return offsetOfTail() + sizeof(WasmOrJSImportableFunctionCallLinkInfo) * numImportFunctions + sizeof(Wasm::Table*) * i; }
    static constexpr size_t offsetOfGlobalPtr(unsigned numImportFunctions, unsigned numTables, unsigned i) { return roundUpToMultipleOf<sizeof(Wasm::Global::Value)>(offsetOfTablePtr(numImportFunctions, numTables)) + sizeof(Wasm::Global::Value) * i; }
    static constexpr size_t offsetOfGCObjectStructure(unsigned numImportFunctions, unsigned numTables, unsigned numGlobals, unsigned i) { return offsetOfGlobalPtr(numImportFunctions, numTables, numGlobals) + sizeof(WriteBarrier<WebAssemblyGCStructure>) * i; }
    WriteBarrier<WebAssemblyGCStructure>& gcObjectStructure(unsigned numImportFunctions, unsigned numTables, unsigned numGlobals, unsigned i) { return *std::bit_cast<WriteBarrier<WebAssemblyGCStructure>*>(reinterpret_cast<char*>(this) + offsetOfGCObjectStructure(numImportFunctions, numTables, numGlobals, i)); }
    WriteBarrier<WebAssemblyGCStructure>& gcObjectStructure(unsigned typeIndex) { return gcObjectStructure(numImportFunctions(), moduleInformation().tableCount(), moduleInformation().globalCount(), typeIndex); }

    static constexpr size_t offsetOfAllocatorForGCObject(unsigned numImportFunctions, unsigned numTables, unsigned numGlobals, unsigned numTypes, unsigned i) { return offsetOfGCObjectStructure(numImportFunctions, numTables, numGlobals, numTypes) + sizeof(Allocator) * i; }
    Allocator& allocatorForGCObject(unsigned numImportFunctions, unsigned numTables, unsigned numGlobals, unsigned numTypes, unsigned i) { ASSERT(moduleInformation().hasGCObjectTypes()); return *std::bit_cast<Allocator*>(reinterpret_cast<char*>(this) + offsetOfAllocatorForGCObject(numImportFunctions, numTables, numGlobals, numTypes, i)); }
    Allocator& allocatorForGCObject(unsigned sizeClassIndex) { return allocatorForGCObject(numImportFunctions(), moduleInformation().tableCount(), moduleInformation().globalCount(), moduleInformation().typeCount(), sizeClassIndex); }

    const Wasm::Tag& tag(unsigned i) const { return *m_tags[i]; }
    void setTag(unsigned, Ref<const Wasm::Tag>&&);

    CallFrame* temporaryCallFrame() const { return m_temporaryCallFrame; }
    void setTemporaryCallFrame(CallFrame* callFrame)
    {
        m_temporaryCallFrame = callFrame;
    }

    void* softStackLimit() const { return m_softStackLimit; }

    void setFaultPC(void* pc) { m_faultPC = pc; };
    void* faultPC() const { return m_faultPC; }

private:
    JSWebAssemblyInstance(VM&, Structure*, JSWebAssemblyModule*, WebAssemblyModuleRecord*, RefPtr<SourceProvider>&&);
    ~JSWebAssemblyInstance();
    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;

    static size_t allocationSize(const Wasm::ModuleInformation&);
    bool evaluateConstantExpression(uint64_t, Wasm::Type, uint64_t&);

    VM* const m_vm;
    WriteBarrier<JSWebAssemblyModule> m_jsModule;
    WriteBarrier<WebAssemblyModuleRecord> m_moduleRecord;
    WriteBarrier<JSWebAssemblyMemory> m_memory;
    FixedVector<WriteBarrier<JSWebAssemblyTable>> m_tables;
    void* m_softStackLimit { nullptr };
    CagedPtr<Gigacage::Primitive, void> m_cachedMemory;
    size_t m_cachedBoundsCheckingSize { 0 };
    const Ref<Wasm::Module> m_module;
    RefPtr<SourceProvider> m_sourceProvider;

    CallFrame* m_temporaryCallFrame { nullptr };
    Wasm::Global::Value* m_globals { nullptr };
    FunctionWrapperMap m_functionWrappers;
    BitVector m_globalsToMark;
    BitVector m_globalsToBinding;
    unsigned m_numImportFunctions { 0 };
    UncheckedKeyHashMap<uint32_t, Ref<Wasm::Global>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_linkedGlobals;
    BitVector m_passiveElements;
    BitVector m_passiveDataSegments;
    FixedVector<RefPtr<const Wasm::Tag>> m_tags;
    Vector<Ref<Wasm::WasmToJSCallee>> importCallees;
    void* m_faultPC { nullptr };
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
