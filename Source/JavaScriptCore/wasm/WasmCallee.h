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

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JITCompilation.h>
#include <JavaScriptCore/NativeCallee.h>
#include <JavaScriptCore/PCToCodeOriginMap.h>
#include <JavaScriptCore/RegisterAtOffsetList.h>
#include <JavaScriptCore/StackAlignment.h>
#include <JavaScriptCore/WasmCompilationMode.h>
#include <JavaScriptCore/WasmFormat.h>
#include <JavaScriptCore/WasmFunctionIPIntMetadataGenerator.h>
#include <JavaScriptCore/WasmHandlerInfo.h>
#include <JavaScriptCore/WasmIPIntTierUpCounter.h>
#include <JavaScriptCore/WasmIndexOrName.h>
#include <JavaScriptCore/WasmTierUpCount.h>
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/FixedVector.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeLazyUniquePtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TrailingArray.h>

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

#if ENABLE(JIT)
    Box<PCToCodeOriginMap> pcToCodeOriginMap() const;
#endif

    const HandlerInfo* handlerForIndex(JSWebAssemblyInstance&, unsigned, const Tag*);

    const FixedVector<HandlerInfo>& exceptionHandlers() const LIFETIME_BOUND;
    bool hasExceptionHandlers() const { return !exceptionHandlers().isEmpty(); }

    void dump(PrintStream&) const;
    void dumpSimpleName(PrintStream&) const;
    String nameWithHash() const;

    static void destroy(Callee*);

    void reportToVMsForDestruction();

    unsigned computeCodeHashImpl() const
    {
        return 0;
    }

#if ENABLE(JIT)
    Box<PCToCodeOriginMap> pcToCodeOriginMapImpl() const { return nullptr; }
#endif

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, FunctionSpaceIndex, std::pair<const Name*, RefPtr<NameSection>>&&);

    const FixedVector<HandlerInfo>& exceptionHandlersImpl() const LIFETIME_BOUND { return m_exceptionHandlers; }

    template<typename Func>
    void runWithDowncast(const Func&);
    template<typename Func>
    void runWithDowncast(const Func&) const;

    void setIndexOrName(IndexOrName&&);

private:
    const CompilationMode m_compilationMode;
    const FunctionSpaceIndex m_index;
    IndexOrName m_indexOrName;

protected:
    FixedVector<HandlerInfo> m_exceptionHandlers;
};

class JITCallee : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(JITCallee);
public:
    friend class Callee;
    FixedVector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() LIFETIME_BOUND { return m_wasmToWasmCallsites; }

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

    const RegisterAtOffsetList* calleeSaveRegistersImpl() LIFETIME_BOUND { return &m_entrypoint.calleeSaveRegisters; }
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

    static inline Ref<JSToWasmCallee> create(Ref<const RTT>&& rtt)
    {
        return adoptRef(*new JSToWasmCallee(WTF::move(rtt)));
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
    const RTT& rtt() const LIFETIME_BOUND { return m_rtt; }

    void setWasmCallee(CalleeBits wasmCallee)
    {
        m_wasmCallee = wasmCallee;
    }

private:
    JSToWasmCallee(Ref<const RTT>&&);

    unsigned m_frameSize { };
    // This must be initialized after the callee is created unfortunately.
    CalleeBits m_wasmCallee;
    const Ref<const RTT> m_rtt;
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

class RestoreFrameCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(RestoreFrameCallee);
public:
    friend class Callee;
    friend class JSC::LLIntOffsetsExtractor;

    static constexpr size_t restoreFrameSizeInBytes = (static_cast<size_t>(CallFrameSlot::callee) + 1) * sizeof(Register);

    static RestoreFrameCallee& singleton();

private:
    RestoreFrameCallee();
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }
    const RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
};

extern "C" EncodedJSValue g_restoreFrameCalleeBoxed;
extern "C" void wasm_restore_frame_return();

#if ENABLE(JIT)

class JSToWasmICCallee final : public Callee {
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(JSToWasmICCallee);
public:
    static Ref<JSToWasmICCallee> create(RegisterAtOffsetList&& calleeSaves)
    {
        return adoptRef(*new JSToWasmICCallee(WTF::move(calleeSaves)));
    }

    const RegisterAtOffsetList* calleeSaveRegistersImpl() LIFETIME_BOUND { return &m_calleeSaves; }
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

    Box<PCToCodeOriginMap> pcToCodeOriginMapImpl() const { return m_pcToCodeOriginMap; }
    void setPCToCodeOriginMap(Box<PCToCodeOriginMap>&& map) { m_pcToCodeOriginMap = WTF::move(map); }

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
    Box<PCToCodeOriginMap> m_pcToCodeOriginMap;
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

    TierUpCount& tierUpCounter() LIFETIME_BOUND { return m_tierUpCounter; }

    std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint() { return m_sharedLoopEntrypoint; }
    const Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints() LIFETIME_BOUND { return m_loopEntrypoints; }

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
    using Base = Callee;
    WTF_MAKE_COMPACT_TZONE_ALLOCATED(IPIntCallee);
    friend class JSC::LLIntOffsetsExtractor;
    friend class Callee;
public:
    using IPIntMetadata = uint8_t;
    class IPIntData final : public TrailingArray<IPIntData, IPIntMetadata> {
        WTF_MAKE_NONMOVABLE(IPIntData);
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(IPIntData);
        using TrailingArrayType = TrailingArray<IPIntData, IPIntMetadata>;
        friend TrailingArrayType;
        friend class IPIntCallee;
        friend class JSC::LLIntOffsetsExtractor;
    public:
        static std::unique_ptr<IPIntData> create(size_t metadataSize)
        {
            return std::unique_ptr<IPIntData>(new (fastMalloc(allocationSize(metadataSize))) IPIntData(metadataSize));
        }

        // Inline capacity matches FunctionIPIntMetadataGenerator's, so that handing the
        // generator's buffer over here is a move rather than a copy: WTF::Vector only offers
        // cross-inline-capacity assignment by copy.
        Vector<uint8_t, 8> m_localInitBytecode;
        Vector<FunctionSpaceIndex> m_callTargets;
        FixedVector<HandlerInfo> m_exceptionHandlers;
        IPIntTierUpCounter m_tierUpCounter;

        // First opcode of the body, i.e. IPIntCallee::m_bytecode + m_bytecodeOffset.
        const uint8_t* m_bytecodeStart { nullptr };
        // Argument- and result-marshalling bytecode for this function's signature. Owned by the
        // signature's RTT, which outlives every callee naming it, and copied here so that the
        // interpreter reaches it through the pointer that publishes this metadata. Reading it off
        // the RTT instead would be a second, independent load, which on a weakly ordered CPU can
        // be satisfied from before the RTT's own copy was installed.
        const uint8_t* m_argumINTBytecode { nullptr };
        const uint8_t* m_uINTBytecode { nullptr };
        // Offset from raw function bytecode to the first opcode (post locals header).
        unsigned m_bytecodeOffset { 0 };
        unsigned m_maxFrameSizeInV128 { 0 };
        unsigned m_localSizeToAlloc { 0 };
        unsigned m_numRethrowSlotsToAlloc { 0 };
        unsigned m_numLocals { 0 };
        unsigned m_numArgumentsOnStack { 0 };
        unsigned m_maxCalleeStackSize { 0 };

        static constexpr ptrdiff_t offsetOfMetadata() { return TrailingArrayType::offsetOfData(); }
        static constexpr ptrdiff_t offsetOfLocalInitBytecode() { return OBJECT_OFFSETOF(IPIntData, m_localInitBytecode); }
        static constexpr ptrdiff_t offsetOfTierUpCounter() { return OBJECT_OFFSETOF(IPIntData, m_tierUpCounter); }
        static constexpr ptrdiff_t offsetOfBytecodeStart() { return OBJECT_OFFSETOF(IPIntData, m_bytecodeStart); }
        static constexpr ptrdiff_t offsetOfArgumINTBytecode() { return OBJECT_OFFSETOF(IPIntData, m_argumINTBytecode); }
        static constexpr ptrdiff_t offsetOfUINTBytecode() { return OBJECT_OFFSETOF(IPIntData, m_uINTBytecode); }
        static constexpr ptrdiff_t offsetOfBytecodeOffset() { return OBJECT_OFFSETOF(IPIntData, m_bytecodeOffset); }
        static constexpr ptrdiff_t offsetOfMaxFrameSizeInV128() { return OBJECT_OFFSETOF(IPIntData, m_maxFrameSizeInV128); }
        static constexpr ptrdiff_t offsetOfLocalSizeToAlloc() { return OBJECT_OFFSETOF(IPIntData, m_localSizeToAlloc); }
        static constexpr ptrdiff_t offsetOfNumRethrowSlotsToAlloc() { return OBJECT_OFFSETOF(IPIntData, m_numRethrowSlotsToAlloc); }

    private:
        explicit IPIntData(size_t metadataSize)
            : TrailingArrayType(metadataSize)
            , m_tierUpCounter(UncheckedKeyHashMap<IPIntPC, IPIntTierUpCounter::OSREntryData>())
        {
        }
    };

    static Ref<IPIntCallee> create(FunctionCodeIndex functionIndex, FunctionSpaceIndex index, const RTT& signatureRTT, std::pair<const Name*, RefPtr<NameSection>>&& name, const uint8_t* bytecode, const uint8_t* bytecodeEnd)
    {
        return adoptRef(*new IPIntCallee(functionIndex, index, signatureRTT, WTF::move(name), bytecode, bytecodeEnd));
    }

    FunctionCodeIndex functionIndex() const { return m_functionIndex; }
    bool isLazy() const { return !data(); }
    void initializeMetadata(FunctionIPIntMetadataGenerator&);

    // A body that does not parse fails identically for every caller and on every attempt, so the
    // diagnostic is kept and the body is never parsed a second time. Null until that happens.
    const String* parseFailure() const { return m_parseFailure.get(); }
    const String& recordParseFailure(String&& message)
    {
        return m_parseFailure.ensure([&] {
            return makeUnique<const String>(WTF::move(message));
        });
    }
    void setEntrypoint(CodePtr<WasmEntryPtrTag>);
    void setEntrypointWithoutRegistration(CodePtr<WasmEntryPtrTag>);
    void setName(std::pair<const Name*, RefPtr<NameSection>>&& name) { setIndexOrName(IndexOrName(index(), WTF::move(name))); }
    const uint8_t* bytecode() const { return m_bytecode; }
    const uint8_t* bytecodeEnd() const { return m_bytecodeEnd; }
    // First opcode of the body. bytecode() is the raw body, which starts with the locals
    // declaration, so anything addressing executable code wants this instead.
    const uint8_t* bytecodeStart() const { return parsedData().m_bytecodeStart; }
    IPIntData* data() const { return m_data.get(); }
    const uint8_t* metadata() const LIFETIME_BOUND { return data() ? data()->data() : nullptr; }

    // Everything below describes a body that has been parsed, so a caller has to have entered
    // the function or run ensureNotLazy() first. Asking earlier is a bug, not an empty answer.
    IPIntData& parsedData() const LIFETIME_BOUND
    {
        ASSERT(!isLazy());
        return *m_data.get();
    }

    unsigned numLocals() const { return parsedData().m_numLocals; }
    unsigned localSizeToAlloc() const { return parsedData().m_localSizeToAlloc; }
    unsigned rethrowSlots() const { return parsedData().m_numRethrowSlotsToAlloc; }
    unsigned maxFrameSizeInV128() const { return parsedData().m_maxFrameSizeInV128; }
    unsigned maxCalleeStackSize() const { return parsedData().m_maxCalleeStackSize; }

    const Vector<FunctionSpaceIndex>& callTargets() const LIFETIME_BOUND { return parsedData().m_callTargets; }
    unsigned numCallProfiles() const { return parsedData().m_callTargets.size(); }

    IPIntTierUpCounter& tierUpCounter() LIFETIME_BOUND { return parsedData().m_tierUpCounter; }
    const IPIntTierUpCounter& tierUpCounter() const LIFETIME_BOUND { return parsedData().m_tierUpCounter; }

    FunctionSpaceIndex callTarget(unsigned callProfileIndex) const { return parsedData().m_callTargets[callProfileIndex]; }

    const RTT& signatureRTT() const LIFETIME_BOUND { return *m_signatureRTT; }

    using OutOfLineJumpTargets = UncheckedKeyHashMap<unsigned, int>;

    unsigned computeCodeHashImpl() const;

    static constexpr ptrdiff_t offsetOfData() { return OBJECT_OFFSETOF(IPIntCallee, m_data); }

private:
    IPIntCallee(FunctionCodeIndex, FunctionSpaceIndex, const RTT& signatureRTT, std::pair<const Name*, RefPtr<NameSection>>&&, const uint8_t* bytecode, const uint8_t* bytecodeEnd);

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint; }
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; };
    JS_EXPORT_PRIVATE const RegisterAtOffsetList* calleeSaveRegistersImpl();
    const FixedVector<HandlerInfo>& exceptionHandlersImpl() const LIFETIME_BOUND;

    // FIXME: Do we really need this. We only use it for gating tier up so we can
    // compute it from the instance + m_functionSpaceIndex in Callee.
    FunctionCodeIndex m_functionIndex;
    CodePtr<WasmEntryPtrTag> m_entrypoint;

    // m_bytecode points at raw function body (including locals header).
    // Opcodes start at m_bytecode + m_data->m_bytecodeOffset.
    // FIXME: Maybe this should be an offset to the start of the code. Could also be a tagged union with
    // m_data as we won't need it after we've initialzed.
    const uint8_t* m_bytecode { nullptr };
    const uint8_t* m_bytecodeEnd { nullptr };
    RefPtr<const RTT> m_signatureRTT;
    ThreadSafeLazyUniquePtr<IPIntData> m_data;
    ThreadSafeLazyUniquePtr<const String> m_parseFailure;

    mutable unsigned m_codeHash { 0 };
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

    const WebAssemblyBuiltin* builtin() LIFETIME_BOUND { return m_builtin.get(); }
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

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Wasm::RestoreFrameCallee)
    static bool isType(const JSC::Wasm::Callee& callee)
    {
        return callee.compilationMode() == JSC::Wasm::CompilationMode::RestoreFrameMode;
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBASSEMBLY)
