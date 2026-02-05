/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JITCompilation.h>
#include <JavaScriptCore/NativeCallee.h>
#include <JavaScriptCore/PCToCodeOriginMap.h>
#include <JavaScriptCore/RegisterAtOffsetList.h>
#include <JavaScriptCore/StackAlignment.h>
#include <JavaScriptCore/WasmCompilationMode.h>
#include <JavaScriptCore/WasmFormat.h>
#include <JavaScriptCore/WasmFunctionIPIntMetadataGenerator.h>
#include <JavaScriptCore/WasmHandlerInfo.h>
#include <JavaScriptCore/WasmIPIntGenerator.h>
#include <JavaScriptCore/WasmIPIntTierUpCounter.h>
#include <JavaScriptCore/WasmIndexOrName.h>
#include <JavaScriptCore/WasmTierUpCount.h>
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/FixedVector.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class LLIntOffsetsExtractor;
class WebAssemblyBuiltin;

namespace B3 {
class PCToOriginMap;
}

namespace Wasm {

class BaselineData;
class CallProfile;
class CalleeGroup;

class Callee : public NativeCallee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(Callee);
    friend class JSC::LLIntOffsetsExtractor;
public:
    IndexOrName indexOrName() const { return m_indexOrName; }
    FunctionSpaceIndex index() const { return m_index; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    CodePtr<WasmEntryPtrTag> entrypoint() const;
    const RegisterAtOffsetList* calleeSaveRegisters();
    // Used by Wasm's fault signal handler to determine if the fault came from Wasm.
    std::tuple<void*, void*> range() const;

    const HandlerInfo* handlerForIndex(JSWebAssemblyInstance&, unsigned, const Tag*);

    bool hasExceptionHandlers() const { return !m_exceptionHandlers.isEmpty(); }

    void dump(PrintStream&) const;
    void dumpSimpleName(PrintStream&) const;
    String nameWithHash() const;

    static void destroy(Callee*);

    void reportToVMsForDestruction();

    unsigned computeCodeHashImpl() const
    {
        return 0;
    }

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, FunctionSpaceIndex, std::pair<const Name*, RefPtr<NameSection>>&&);

    template<typename Func>
    void runWithDowncast(const Func&);
    template<typename Func>
    void runWithDowncast(const Func&) const;

private:
    const CompilationMode m_compilationMode;
    const FunctionSpaceIndex m_index;
    const IndexOrName m_indexOrName;

protected:
    FixedVector<HandlerInfo> m_exceptionHandlers;
};

class JITCallee : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(JITCallee);
public:
    friend class Callee;
    FixedVector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

#if ENABLE(JIT)
    void setEntrypoint(Wasm::Entrypoint&&);
#endif

protected:
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, FunctionSpaceIndex, std::pair<const Name*, RefPtr<NameSection>>&&);

#if ENABLE(JIT)
    std::tuple<void*, void*> rangeImpl() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }

    const RegisterAtOffsetList* calleeSaveRegistersImpl() { return &m_entrypoint.calleeSaveRegisters; }
#else
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }
    const RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
#endif

    FixedVector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
#if ENABLE(JIT)
    Wasm::Entrypoint m_entrypoint;
#endif
};

class JSToWasmCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(JSToWasmCallee);
public:
    friend class Callee;
    friend class JSC::LLIntOffsetsExtractor;

    static inline Ref<JSToWasmCallee> create(TypeIndex typeIndex, bool usesSIMD)
    {
        return adoptRef(*new JSToWasmCallee(typeIndex, usesSIMD));
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const;
    static JS_EXPORT_PRIVATE const RegisterAtOffsetList* calleeSaveRegistersImpl();
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    static constexpr ptrdiff_t offsetOfWasmCallee() { return OBJECT_OFFSETOF(JSToWasmCallee, m_wasmCallee); }
    static constexpr ptrdiff_t offsetOfFrameSize() { return OBJECT_OFFSETOF(JSToWasmCallee, m_frameSize); }

    // Space for callee-saves; Not included in frameSize
    static constexpr unsigned SpillStackSpaceAligned = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(3 * sizeof(UCPURegister));
    // Extra space used to return argument register values from cpp before they get filled. Included in frameSize
    static constexpr unsigned RegisterStackSpaceAligned = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(
        FPRInfo::numberOfArgumentRegisters * bytesForWidth(Width::Width64) + GPRInfo::numberOfArgumentRegisters * sizeof(UCPURegister));

    unsigned frameSize() const { return m_frameSize; }
    CalleeBits wasmCallee() const { return m_wasmCallee; }
    TypeIndex typeIndex() const { return m_typeIndex; }

    void setWasmCallee(CalleeBits wasmCallee)
    {
        m_wasmCallee = wasmCallee;
    }

private:
    JSToWasmCallee(TypeIndex, bool);

    unsigned m_frameSize { };
    // This must be initialized after the callee is created unfortunately.
    CalleeBits m_wasmCallee;
    const TypeIndex m_typeIndex;
};

class WasmToJSCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(WasmToJSCallee);
public:
    friend class Callee;
    friend class JSC::LLIntOffsetsExtractor;

    static WasmToJSCallee& singleton();

private:
    WasmToJSCallee();
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }
    const RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
};

#if ENABLE(JIT)

class JSToWasmICCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(JSToWasmICCallee);
public:
    static Ref<JSToWasmICCallee> create(RegisterAtOffsetList&& calleeSaves)
    {
        return adoptRef(*new JSToWasmICCallee(WTF::move(calleeSaves)));
    }

    const RegisterAtOffsetList* calleeSaveRegistersImpl() { return &m_calleeSaves; }
    CodePtr<JSEntryPtrTag> jsToWasm() { return m_jsToWasmICEntrypoint.code(); }

    void setEntrypoint(MacroAssemblerCodeRef<JSEntryPtrTag>&&);

private:
    friend class Callee;
    JSToWasmICCallee(RegisterAtOffsetList&& calleeSaves)
        : Callee(Wasm::CompilationMode::JSToWasmICMode)
        , m_calleeSaves(WTF::move(calleeSaves))
    {
    }

    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }

    MacroAssemblerCodeRef<JSEntryPtrTag> m_jsToWasmICEntrypoint;
    RegisterAtOffsetList m_calleeSaves;
};

#endif

#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)

struct WasmCodeOrigin {
    unsigned firstInlineCSI;
    unsigned lastInlineCSI;
    unsigned functionIndex;
    unsigned moduleIndex;
};

class OptimizingJITCallee : public JITCallee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(OptimizingJITCallee);
public:
    const StackMap& stackmap(CallSiteIndex) const;

    void addCodeOrigin(unsigned firstInlineCSI, unsigned lastInlineCSI, const Wasm::ModuleInformation&, uint32_t functionIndex);
    const WasmCodeOrigin* getCodeOrigin(unsigned csi, unsigned depth, bool& isInlined) const;
    IndexOrName getOrigin(unsigned csi, unsigned depth, bool& isInlined) const;
    IndexOrName getIndexOrName(const WasmCodeOrigin*) const;
    std::optional<CallSiteIndex> tryGetCallSiteIndex(const void*) const;

    Box<PCToCodeOriginMap> materializePCToOriginMap(B3::PCToOriginMap&&, LinkBuffer&);

    unsigned computeCodeHashImpl() const;

protected:
    OptimizingJITCallee(Wasm::CompilationMode mode, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee)
        : JITCallee(mode, index, WTF::move(name))
        , m_profiledCallee(WTF::move(profiledCallee))
    {
    }

    void setEntrypoint(Wasm::Entrypoint&& entrypoint, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& unlinkedExceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        m_wasmToWasmCallsites = WTF::move(unlinkedCalls);
        m_stackmaps = WTF::move(stackmaps);
        RELEASE_ASSERT(unlinkedExceptionHandlers.size() == exceptionHandlerLocations.size());
        linkExceptionHandlers(WTF::move(unlinkedExceptionHandlers), WTF::move(exceptionHandlerLocations));
        JITCallee::setEntrypoint(WTF::move(entrypoint));
    }

private:
    void linkExceptionHandlers(Vector<UnlinkedHandlerInfo>, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>);

    StackMaps m_stackmaps;
    Vector<WasmCodeOrigin, 0> codeOrigins;
    Vector<Ref<NameSection>, 0> nameSections;
    Box<PCToCodeOriginMap> m_callSiteIndexMap;
    const Ref<IPIntCallee> m_profiledCallee;
};

constexpr int32_t stackCheckUnset = 0;
constexpr int32_t stackCheckNotNeeded = -1;

class OMGOSREntryCallee final : public OptimizingJITCallee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(OMGOSREntryCallee);
public:
    static Ref<OMGOSREntryCallee> create(FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee, uint32_t loopIndex)
    {
        return adoptRef(*new OMGOSREntryCallee(index, WTF::move(name), WTF::move(profiledCallee), loopIndex));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }


    void setEntrypoint(Wasm::Entrypoint&& entrypoint, unsigned osrEntryScratchBufferSize, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        m_osrEntryScratchBufferSize = osrEntryScratchBufferSize;
        OptimizingJITCallee::setEntrypoint(WTF::move(entrypoint), WTF::move(unlinkedCalls), WTF::move(stackmaps), WTF::move(exceptionHandlers), WTF::move(exceptionHandlerLocations));
    }

    void setStackCheckSize(int32_t stackCheckSize)
    {
        ASSERT(m_stackCheckSize == stackCheckUnset);
        ASSERT(stackCheckSize > 0 || stackCheckSize == stackCheckNotNeeded);
        m_stackCheckSize = stackCheckSize;
    }

    int32_t stackCheckSize() const
    {
        ASSERT(m_stackCheckSize > 0 || m_stackCheckSize == stackCheckNotNeeded);
        return m_stackCheckSize;
    }

private:
    OMGOSREntryCallee(FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee, uint32_t loopIndex)
        : OptimizingJITCallee(CompilationMode::OMGForOSREntryMode, index, WTF::move(name), WTF::move(profiledCallee))
        , m_loopIndex(loopIndex)
    {
    }

    unsigned m_osrEntryScratchBufferSize { 0 };
    uint32_t m_loopIndex;
    int32_t m_stackCheckSize { stackCheckUnset };
};

#endif // ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)

#if ENABLE(WEBASSEMBLY_OMGJIT)

class OMGCallee final : public OptimizingJITCallee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(OMGCallee);
public:
    static Ref<OMGCallee> create(FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee)
    {
        return adoptRef(*new OMGCallee(index, WTF::move(name), WTF::move(profiledCallee)));
    }

    using OptimizingJITCallee::setEntrypoint;

private:
    OMGCallee(FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee)
        : OptimizingJITCallee(Wasm::CompilationMode::OMGMode, index, WTF::move(name), WTF::move(profiledCallee))
    {
    }
};

#endif // ENABLE(WEBASSEMBLY_OMGJIT)

#if ENABLE(WEBASSEMBLY_BBQJIT)

class BBQCallee final : public OptimizingJITCallee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(BBQCallee);
    friend class Callee;
public:
    static constexpr unsigned extraOSRValuesForLoopIndex = 1;

    static Ref<BBQCallee> create(FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee)
    {
        return adoptRef(*new BBQCallee(index, WTF::move(name), WTF::move(profiledCallee)));
    }
    ~BBQCallee();

    OMGOSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OMGOSREntryCallee>&& osrEntryCallee, MemoryMode)
    {
        ASSERT(!m_osrEntryCallee);
        m_osrEntryCallee = WTF::move(osrEntryCallee);
    }

    bool didStartCompilingOSREntryCallee() const { return m_didStartCompilingOSREntryCallee; }
    void setDidStartCompilingOSREntryCallee(bool value) { m_didStartCompilingOSREntryCallee = value; }

    TierUpCount& tierUpCounter() { return m_tierUpCounter; }

    std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint() { return m_sharedLoopEntrypoint; }
    const Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints() { return m_loopEntrypoints; }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }

    void setEntrypoint(Wasm::Entrypoint&& entrypoint, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations, Vector<CodeLocationLabel<WasmEntryPtrTag>>&& loopEntrypoints, std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint, unsigned osrEntryScratchBufferSize)
    {
        m_sharedLoopEntrypoint = sharedLoopEntrypoint;
        m_loopEntrypoints = WTF::move(loopEntrypoints);
        m_osrEntryScratchBufferSize = osrEntryScratchBufferSize;
        OptimizingJITCallee::setEntrypoint(WTF::move(entrypoint), WTF::move(unlinkedCalls), WTF::move(stackmaps), WTF::move(exceptionHandlers), WTF::move(exceptionHandlerLocations));
        m_switchJumpTables.shrinkToFit();
    }

    EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>* addJumpTable(unsigned size)
    {
        m_switchJumpTables.append(EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>::create(size));
        return m_switchJumpTables.last().ptr();
    }

    void setStackCheckSize(unsigned stackCheckSize)
    {
        ASSERT(m_stackCheckSize == stackCheckUnset);
        ASSERT(stackCheckSize > 0 || int32_t(stackCheckSize) == stackCheckNotNeeded);
        m_stackCheckSize = stackCheckSize;
    }
    int32_t stackCheckSize() const
    {
        ASSERT(m_stackCheckSize > 0 || int32_t(m_stackCheckSize) == stackCheckNotNeeded);
        return m_stackCheckSize;
    }

private:
    BBQCallee(FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name, Ref<IPIntCallee>&& profiledCallee)
        : OptimizingJITCallee(Wasm::CompilationMode::BBQMode, index, WTF::move(name), WTF::move(profiledCallee))
    {
    }

    JS_EXPORT_PRIVATE const RegisterAtOffsetList* calleeSaveRegistersImpl();

    RefPtr<OMGOSREntryCallee> m_osrEntryCallee;
    TierUpCount m_tierUpCounter;
    std::optional<CodeLocationLabel<WasmEntryPtrTag>> m_sharedLoopEntrypoint;
    Vector<CodeLocationLabel<WasmEntryPtrTag>> m_loopEntrypoints;
    unsigned m_osrEntryScratchBufferSize { 0 };
    unsigned m_stackCheckSize { 0 };
    bool m_didStartCompilingOSREntryCallee { false };
    Vector<UniqueRef<EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>>> m_switchJumpTables;
};
#endif


class IPIntCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(IPIntCallee);
    friend class JSC::LLIntOffsetsExtractor;
    friend class Callee;
public:
    static Ref<IPIntCallee> create(FunctionIPIntMetadataGenerator& generator, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new IPIntCallee(generator, index, WTF::move(name)));
    }

    FunctionCodeIndex functionIndex() const { return m_functionIndex; }
    void setEntrypoint(CodePtr<WasmEntryPtrTag>);
    const uint8_t* bytecode() const { return m_bytecode; }
    const uint8_t* bytecodeEnd() const { return m_bytecodeEnd; }
    const uint8_t* metadata() const { return m_metadata.span().data(); }

    unsigned numLocals() const { return m_numLocals; }
    unsigned localSizeToAlloc() const { return m_localSizeToAlloc; }
    unsigned rethrowSlots() const { return m_numRethrowSlotsToAlloc; }

    const Vector<FunctionSpaceIndex>& callTargets() const { return m_callTargets; }
    unsigned numCallProfiles() const { return m_callTargets.size(); }

    IPIntTierUpCounter& tierUpCounter() { return m_tierUpCounter; }
    const IPIntTierUpCounter& tierUpCounter() const { return m_tierUpCounter; }

    FunctionSpaceIndex callTarget(unsigned callProfileIndex) const { return m_callTargets[callProfileIndex]; }

    using OutOfLineJumpTargets = UncheckedKeyHashMap<unsigned, int>;

    unsigned computeCodeHashImpl() const;

private:
    IPIntCallee(FunctionIPIntMetadataGenerator&, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&&);

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint; }
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; };
    JS_EXPORT_PRIVATE const RegisterAtOffsetList* calleeSaveRegistersImpl();

    FunctionCodeIndex m_functionIndex;
    CodePtr<WasmEntryPtrTag> m_entrypoint;

    const uint8_t* m_bytecode;
    const uint8_t* m_bytecodeEnd;
    Vector<uint8_t> m_metadata;
    Vector<uint8_t> m_argumINTBytecode;
    Vector<uint8_t> m_uINTBytecode;
    Vector<FunctionSpaceIndex> m_callTargets;

    unsigned m_topOfReturnStackFPOffset;

    unsigned m_localSizeToAlloc;
    unsigned m_numRethrowSlotsToAlloc;
    unsigned m_numLocals;
    unsigned m_numArgumentsOnStack;
    unsigned m_maxFrameSizeInV128;
    mutable unsigned m_codeHash { 0 };

    IPIntTierUpCounter m_tierUpCounter;
};

using IPIntCallees = ThreadSafeRefCountedFixedVector<Ref<IPIntCallee>>;

/// A helper deleter to ensure that the pro forma unique_ptr to a builtin in WasmBuiltinCallee
/// never tries to actually destroy the builtin.
struct MustNotBeDestroyed {
    NO_RETURN_DUE_TO_ASSERT void operator()(const WebAssemblyBuiltin*) const
    {
        ASSERT_NOT_REACHED();
    }
};

class WasmBuiltinCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(WasmBuiltinCallee);
    friend class Callee;
    friend class JSC::LLIntOffsetsExtractor;
public:
    WasmBuiltinCallee(const WebAssemblyBuiltin*, std::pair<const Name*, RefPtr<NameSection>>&&);

    const WebAssemblyBuiltin* builtin() { return m_builtin.get(); }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_trampoline; };

protected:
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    const RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }

private:
    MacroAssemblerCodeRef<WasmEntryPtrTag> m_code;
    CodePtr<WasmEntryPtrTag> m_trampoline;
    // Safer CPP checks do not allow a simple 'const WebAssemblyBuiltin *' because it's forward-declared.
    // We hold the pointer as a pro forma unique_ptr. It is never actually destroyed because
    // the builtin and this callee are part of a singleton structure expected to live forever.
    std::unique_ptr<const WebAssemblyBuiltin, MustNotBeDestroyed> m_builtin;
};

} } // namespace JSC::Wasm

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::Callee)
    static bool isType(const JSC::NativeCallee& callee)
    {
        return callee.category() == JSC::NativeCallee::Category::Wasm;
    }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::IPIntCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::IPIntMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::OptimizingJITCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::OMGMode
            || callee.compilationMode() == JSC::Wasm::CompilationMode::OMGForOSREntryMode
            || callee.compilationMode() == JSC::Wasm::CompilationMode::BBQMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::BBQCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::BBQMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(WEBASSEMBLY_OMGJIT)
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::OMGCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::OMGMode;
    }
SPECIALIZE_TYPE_TRAITS_END()
#endif

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::OMGOSREntryCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::OMGForOSREntryMode;
    }
SPECIALIZE_TYPE_TRAITS_END()
#endif // ENABLE(WEBASSEMBLY_BBQJIT) || ENABLE(WEBASSEMBLY_OMGJIT)

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::JSToWasmCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::JSToWasmMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::JSToWasmICCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::JSToWasmICMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::WasmToJSCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::WasmToJSMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::WasmBuiltinCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::WasmBuiltinMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBASSEMBLY)
