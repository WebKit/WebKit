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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/CallLinkInfo.h>
#include <JavaScriptCore/JSDestructibleObject.h>
#include <JavaScriptCore/JSWebAssemblyGlobal.h>
#include <JavaScriptCore/JSWebAssemblyMemory.h>
#include <JavaScriptCore/JSWebAssemblyTable.h>
#include <JavaScriptCore/StackManager.h>
#include <JavaScriptCore/WasmCalleeGroup.h>
#include <JavaScriptCore/WasmCreationMode.h>
#include <JavaScriptCore/WasmFormat.h>
#include <JavaScriptCore/WasmGlobal.h>
#include <JavaScriptCore/WasmInstanceAnchor.h>
#include <JavaScriptCore/WasmMemory.h>
#include <JavaScriptCore/WasmModule.h>
#include <JavaScriptCore/WasmModuleInformation.h>
#include <JavaScriptCore/WasmTable.h>
#include <JavaScriptCore/WebAssemblyBuiltin.h>
#include <JavaScriptCore/WebAssemblyFunction.h>
#include <JavaScriptCore/WebAssemblyGCStructure.h>
#include <JavaScriptCore/WriteBarrier.h>
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
class WebAssemblyModuleRecord;

namespace Wasm {

class BaselineData;

}

// The layout of a JSWebAssemblyInstance is
//     { struct JSWebAssemblyInstance }[ WasmOrJSImportableFunctionCallLinkInfo ][ Wasm::Table* ][ Global::Value ][ Wasm::BaselineData* ][ WebAssemblyGCStructure* ][ Allocator* ]
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

    DECLARE_VISIT_CHILDREN;

    void initializeImports(JSGlobalObject*, JSObject* importObject, Wasm::CreationMode);
    void finalizeCreation(VM&, JSGlobalObject*, Ref<Wasm::CalleeGroup>&&, Wasm::CreationMode);
    
    WebAssemblyModuleRecord* moduleRecord() { return m_moduleRecord.get(); }

    // FIXME(wasm-multimemory): eventually get rid of memory() with no parameters
    // Temporarily keep memory() so the code still compiles
    JSWebAssemblyMemory* memory() const { return m_memories[0].get(); }

    JSWebAssemblyMemory* memory(unsigned i) const { return m_memories[i].get(); }

    void setMemory(VM& vm, unsigned i, JSWebAssemblyMemory* value)
    {
        if (!i)
            RELEASE_ASSERT(!m_wasmMemory);

        m_memories[i].set(vm, this, value);
        if (!i) {
            WTF::storeStoreFence();
            m_wasmMemory = value->memory();
            m_wasmMemory->registerInstance(*this);
        }
    }

    // FIXME: is setDummyMemory necessary at all?
    void setDummyMemory(VM& vm, JSWebAssemblyMemory* value)
    {
        // Do not set m_wasmMemory.
        RELEASE_ASSERT(!m_wasmMemory);

        // If there are any imported memories, they will be filled in later
        // If there are zero memories then there will be space for a dummy memory (handled in constructor)

        m_memories[0].set(vm, this, value);
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
    const Wasm::ModuleInformation& moduleInformation() const { return m_moduleInformation.get(); }

    void clearJSCallICs(VM&);
    void finalizeUnconditionally(VM&, CollectionScope);

    static constexpr ptrdiff_t offsetOfJSModule() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_jsModule); }
    static constexpr ptrdiff_t offsetOfVM() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_vm); }
    static constexpr ptrdiff_t offsetOfModuleRecord() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_moduleRecord); }

    using FunctionWrapperMap = UncheckedKeyHashMap<uint32_t, WriteBarrier<Unknown>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    static constexpr ptrdiff_t offsetOfSoftStackLimit() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_stackMirror) + StackManager::Mirror::offsetOfSoftStackLimit(); }

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

    bool memoryInit(uint64_t dstAddress, uint32_t srcAddress, uint32_t length, uint32_t dataSegmentIndex);

    void dataDrop(uint32_t dataSegmentIndex);

    void* cachedMemory() const { return m_cachedMemory.getMayBeNull(); }
    size_t cachedBoundsCheckingSize() const { return m_cachedBoundsCheckingSize; }
    size_t cachedMemorySize() const { return m_cachedMemorySize; }

    void updateCachedMemory()
    {
        if (m_wasmMemory) {
            // Note: In MemoryMode::BoundsChecking, mappedCapacity() == size().
            // We assert this in the constructor of MemoryHandle.
#if CPU(ARM)
            // Shared memory requires signaling memory which is not available
            // on ARMv7 yet. In order to get more of the test suite to run, we
            // can still use a shared memory by using bounds checking, by using
            // the actual size here, but this means we cannot grow the shared
            // memory safely in case it's used by multiple threads. Once the
            // signal handler are available, m_cachedBoundsCheckingSize should
            // be set to use m_wasmMemory->mappedCapacity() like other platforms,
            // and at that point growing the shared memory will be safe.
            m_cachedBoundsCheckingSize = m_wasmMemory->size();
#else
            m_cachedBoundsCheckingSize = m_wasmMemory->mappedCapacity();
#endif
            // only memory 0 is cached
            m_cachedMemorySize = m_wasmMemory->size();
            m_cachedMemory = CagedPtr<Gigacage::Primitive, void>(m_wasmMemory->basePointer());
            m_cachedIsMemory64 = moduleInformation().memory(0).isMemory64();
            ASSERT(m_wasmMemory->basePointer() == cachedMemory());
        }
    }

    uint32_t cachedTable0Length() const { return m_cachedTable0Length; }
    Wasm::FuncRefTable::Function* cachedTable0Buffer() const { return m_cachedTable0Buffer; }

    bool cachedIsMemory64() const { return m_cachedIsMemory64; }

    void updateCachedTable0();

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
    void setBuiltinCalleeBits(uint32_t builtinID, CalleeBits calleeBits) { m_builtinCalleeBits[builtinID] = calleeBits; }

    Wasm::Global* getGlobalBinding(unsigned i)
    {
        ASSERT(m_globalsToBinding.get(i));
        Wasm::Global::Value* pointer = m_globals[i].m_pointer;
        if (!pointer)
            return nullptr;
        return &Wasm::Global::fromBinding(*pointer);
    }

    static constexpr ptrdiff_t offsetOfCachedMemory() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedMemory); }
    static constexpr ptrdiff_t offsetOfCachedBoundsCheckingSize() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedBoundsCheckingSize); }
    static constexpr ptrdiff_t offsetOfCachedMemorySize() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedMemorySize); }
    static constexpr ptrdiff_t offsetOfCachedTable0Buffer() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedTable0Buffer); }
    static constexpr ptrdiff_t offsetOfCachedTable0Length() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedTable0Length); }
    static constexpr ptrdiff_t offsetOfTemporaryCallFrame() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_temporaryCallFrame); }
    static constexpr ptrdiff_t offsetOfBuiltinCalleeBits() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_builtinCalleeBits); }
    static constexpr ptrdiff_t offsetOfCachedIsMemory64() { return OBJECT_OFFSETOF(JSWebAssemblyInstance, m_cachedIsMemory64); }

    // Tail accessors.
    static_assert(sizeof(WasmOrJSImportableFunctionCallLinkInfo) == WTF::roundUpToMultipleOf<sizeof(uint64_t)>(sizeof(WasmOrJSImportableFunctionCallLinkInfo)), "We rely on this for the alignment to be correct");
    static constexpr ptrdiff_t offsetOfImportFunctionInfo(unsigned index)
    {
        return WTF::roundUpToMultipleOf<alignof(WasmOrJSImportableFunctionCallLinkInfo)>(sizeof(JSWebAssemblyInstance)) + sizeof(WasmOrJSImportableFunctionCallLinkInfo) * index;
    }

    static ptrdiff_t offsetOfImportFunctionInfo(const Wasm::ModuleInformation&, unsigned index)
    {
        return offsetOfImportFunctionInfo(index);
    }

    static ptrdiff_t offsetOfTable(const Wasm::ModuleInformation& info, unsigned index)
    {
        return roundUpToMultipleOf<alignof(RefPtr<Wasm::Table>)>(offsetOfImportFunctionInfo(info, info.importFunctionCount())) + sizeof(RefPtr<Wasm::Table>) * index;
    }

    static ptrdiff_t offsetOfGlobal(const Wasm::ModuleInformation& info, unsigned index)
    {
        return roundUpToMultipleOf<alignof(Wasm::Global::Value)>(offsetOfTable(info, info.tableCount())) + sizeof(Wasm::Global::Value) * index;
    }

    static ptrdiff_t offsetOfBaselineData(const Wasm::ModuleInformation& info, unsigned index)
    {
        return roundUpToMultipleOf<alignof(RefPtr<Wasm::BaselineData>)>(offsetOfGlobal(info, info.globalCount())) + sizeof(RefPtr<Wasm::BaselineData>) * index;
    }

    static ptrdiff_t offsetOfGCObjectStructureID(const Wasm::ModuleInformation& info, unsigned index)
    {
        return roundUpToMultipleOf<alignof(WriteBarrierStructureID)>(offsetOfBaselineData(info, info.internalFunctionCount())) + sizeof(WriteBarrierStructureID) * index;
    }

    static ptrdiff_t offsetOfAllocatorForGCObject(const Wasm::ModuleInformation& info, unsigned index)
    {
        return roundUpToMultipleOf<alignof(Allocator)>(offsetOfGCObjectStructureID(info, info.typeCount())) + sizeof(Allocator) * index;
    }

    static size_t offsetOfTargetInstance(size_t importFunctionNum) { return offsetOfImportFunctionInfo(importFunctionNum) + OBJECT_OFFSETOF(Wasm::WasmOrJSImportableFunctionCallLinkInfo, targetInstance); }
    static size_t offsetOfEntrypointLoadLocation(size_t importFunctionNum) { return offsetOfImportFunctionInfo(importFunctionNum) + OBJECT_OFFSETOF(Wasm::WasmOrJSImportableFunctionCallLinkInfo, entrypointLoadLocation); }
    static size_t offsetOfBoxedCallee(size_t importFunctionNum) { return offsetOfImportFunctionInfo(importFunctionNum) + OBJECT_OFFSETOF(Wasm::WasmOrJSImportableFunctionCallLinkInfo, boxedCallee); }
    static size_t offsetOfImportFunctionStub(size_t importFunctionNum) { return offsetOfImportFunctionInfo(importFunctionNum) + OBJECT_OFFSETOF(WasmOrJSImportableFunctionCallLinkInfo, importFunctionStub); }
    static size_t offsetOfImportFunction(size_t importFunctionNum) { return offsetOfImportFunctionInfo(importFunctionNum) + OBJECT_OFFSETOF(WasmOrJSImportableFunctionCallLinkInfo, importFunction); }
    static size_t offsetOfCallLinkInfo(size_t importFunctionNum) { return offsetOfImportFunctionInfo(importFunctionNum) + WasmOrJSImportableFunctionCallLinkInfo::offsetOfCallLinkInfo(); }

    std::span<WasmOrJSImportableFunctionCallLinkInfo> importFunctionInfos()
    {
        return std::span { std::bit_cast<WasmOrJSImportableFunctionCallLinkInfo*>(std::bit_cast<uint8_t*>(this) + offsetOfImportFunctionInfo(0)), m_moduleInformation->importFunctionCount() };
    }

    std::span<RefPtr<Wasm::Table>> tables()
    {
        return std::span { std::bit_cast<RefPtr<Wasm::Table>*>(std::bit_cast<uint8_t*>(this) + offsetOfTable(m_moduleInformation, 0)), m_moduleInformation->tableCount() };
    }

    std::span<Wasm::Global::Value> globals()
    {
        return std::span { std::bit_cast<Wasm::Global::Value*>(std::bit_cast<uint8_t*>(this) + offsetOfGlobal(m_moduleInformation, 0)), m_moduleInformation->globalCount() };
    }

    std::span<RefPtr<Wasm::BaselineData>> baselineDatas()
    {
        return std::span { std::bit_cast<RefPtr<Wasm::BaselineData>*>(std::bit_cast<uint8_t*>(this) + offsetOfBaselineData(m_moduleInformation, 0)), m_moduleInformation->internalFunctionCount() };
    }

    std::span<WriteBarrierStructureID> gcObjectStructureIDs()
    {
        return std::span { std::bit_cast<WriteBarrierStructureID*>(std::bit_cast<uint8_t*>(this) + offsetOfGCObjectStructureID(m_moduleInformation, 0)), m_moduleInformation->typeCount() };
    }

    std::span<Allocator, MarkedSpace::numSizeClasses> allocators()
    {
        return unsafeMakeSpan<Allocator, MarkedSpace::numSizeClasses>(std::bit_cast<Allocator*>(std::bit_cast<uint8_t*>(this) + offsetOfAllocatorForGCObject(m_moduleInformation, 0)), MarkedSpace::numSizeClasses);
    }

    unsigned numImportFunctions() const { return m_numImportFunctions; }
    WasmOrJSImportableFunctionCallLinkInfo* importFunctionInfo(size_t importFunctionNum)
    {
        return &importFunctionInfos()[importFunctionNum];
    }
    WriteBarrier<JSObject>& importFunction(unsigned importFunctionNum) { return importFunctionInfo(importFunctionNum)->importFunction; }

    JSObject* getImportFunctionObject(unsigned importFunctionIndex, JSGlobalObject*);

    RefPtr<Wasm::BaselineData>& baselineData(Wasm::FunctionCodeIndex index)
    {
        return baselineDatas()[index];
    }
    Wasm::BaselineData& ensureBaselineData(Wasm::FunctionCodeIndex);

    WriteBarrierStructureID& gcObjectStructureID(unsigned index) { return gcObjectStructureIDs()[index]; }

    WebAssemblyGCStructure* gcObjectStructure(unsigned typeIndex) { return jsCast<WebAssemblyGCStructure*>(gcObjectStructureID(typeIndex).get()); }

    Allocator& allocatorForGCObject(unsigned index) { ASSERT(moduleInformation().hasGCObjectTypes()); return allocators()[index]; }

    const Wasm::Tag& tag(unsigned i) const { return *m_tags[i]; }
    void setTag(unsigned, Ref<const Wasm::Tag>&&);

    CallFrame* temporaryCallFrame() const { return m_temporaryCallFrame; }
    void setTemporaryCallFrame(CallFrame* callFrame)
    {
        m_temporaryCallFrame = callFrame;
    }

    void* softStackLimit() const { return m_stackMirror.softStackLimit(); }

    void setFaultPC(Wasm::ExceptionType exception, void* pc)
    {
        m_exception = exception;
        m_faultPC = pc;
    }
    Wasm::ExceptionType exception() const { return m_exception; }
    void* faultPC() const { return m_faultPC; }

    void setDebugId(uint32_t id) { m_debugId = id; }
    uint32_t debugId() const { return m_debugId; }

    RefPtr<Wasm::InstanceAnchor> anchor() const { return m_anchor; }

private:
    JSWebAssemblyInstance(VM&, Structure*, JSWebAssemblyModule*, WebAssemblyModuleRecord*, RefPtr<SourceProvider>&&);
    ~JSWebAssemblyInstance();
    void finishCreation(VM&);

    static size_t allocationSize(const Wasm::ModuleInformation&);
    bool evaluateConstantExpression(uint64_t, Wasm::Type, uint64_t&);

    VM* const m_vm;
    WriteBarrier<JSWebAssemblyModule> m_jsModule;
    WriteBarrier<WebAssemblyModuleRecord> m_moduleRecord;
    FixedVector<WriteBarrier<JSWebAssemblyMemory>> m_memories;
    FixedVector<WriteBarrier<JSWebAssemblyTable>> m_tables;
    StackManager::Mirror m_stackMirror;
    CagedPtr<Gigacage::Primitive, void> m_cachedMemory; // only memory 0 is cached
    size_t m_cachedBoundsCheckingSize { 0 };
    size_t m_cachedMemorySize { 0 };
    Wasm::FuncRefTable::Function* m_cachedTable0Buffer { nullptr };
    uint32_t m_cachedTable0Length { 0 };
    const Ref<Wasm::Module> m_module;
    const Ref<const Wasm::ModuleInformation> m_moduleInformation;
    RefPtr<Wasm::InstanceAnchor> m_anchor;
    RefPtr<SourceProvider> m_sourceProvider;
    bool m_cachedIsMemory64 { false };

    RefPtr<Wasm::Memory> m_wasmMemory;
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
    void* m_faultPC { nullptr };
    // Used by builtin trampolines to quickly fetch callee bits to store in the call frame.
    // The actual callees are owned by builtins. Populated by WebAssemblyModuleRecord::initializeImports().
    CalleeBits m_builtinCalleeBits[WASM_BUILTIN_COUNT];
    Wasm::ExceptionType m_exception { Wasm::ExceptionType::Termination };
    uint32_t m_debugId { 0 };
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
