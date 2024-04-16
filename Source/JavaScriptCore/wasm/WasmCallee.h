/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "JITCompilation.h"
#include "NativeCallee.h"
#include "RegisterAtOffsetList.h"
#include "WasmCompilationMode.h"
#include "WasmFormat.h"
#include "WasmFunctionCodeBlockGenerator.h"
#include "WasmFunctionIPIntMetadataGenerator.h"
#include "WasmHandlerInfo.h"
#include "WasmIPIntGenerator.h"
#include "WasmIPIntTierUpCounter.h"
#include "WasmIndexOrName.h"
#include "WasmLLIntTierUpCounter.h"
#include "WasmTierUpCount.h"
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/FixedVector.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

// This class is fast allocated (instead of using TZone) because
// the subclass JSEntrypointInterpreterCalleeMetadata is variable-sized
class Callee : public NativeCallee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Callee);
public:
    IndexOrName indexOrName() const { return m_indexOrName; }
    CompilationMode compilationMode() const { return m_compilationMode; }

    CodePtr<WasmEntryPtrTag> entrypoint() const;
    RegisterAtOffsetList* calleeSaveRegisters();
    std::tuple<void*, void*> range() const;

    const HandlerInfo* handlerForIndex(Instance&, unsigned, const Tag*);

    bool hasExceptionHandlers() const { return !m_exceptionHandlers.isEmpty(); }

    void dump(PrintStream&) const;

    static void destroy(Callee*);

protected:
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE Callee(Wasm::CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

    template<typename Func>
    void runWithDowncast(const Func&);
    template<typename Func>
    void runWithDowncast(const Func&) const;

private:
    const CompilationMode m_compilationMode;
    const IndexOrName m_indexOrName;

protected:
    FixedVector<HandlerInfo> m_exceptionHandlers;
};

class JITCallee : public Callee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(JITCallee);
public:
    friend class Callee;
    FixedVector<UnlinkedWasmToWasmCall>& wasmToWasmCallsites() { return m_wasmToWasmCallsites; }

#if ENABLE(JIT)
    void setEntrypoint(Wasm::Entrypoint&&);
#endif

protected:
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode);
    JS_EXPORT_PRIVATE JITCallee(Wasm::CompilationMode, size_t, std::pair<const Name*, RefPtr<NameSection>>&&);

#if ENABLE(JIT)
    std::tuple<void*, void*> rangeImpl() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return &m_entrypoint.calleeSaveRegisters; }
#else
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }
    RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
#endif

    FixedVector<UnlinkedWasmToWasmCall> m_wasmToWasmCallsites;
#if ENABLE(JIT)
    Wasm::Entrypoint m_entrypoint;
#endif
};

class JSEntrypointCallee : public Callee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(JSEntrypointCallee);
protected:
    JS_EXPORT_PRIVATE JSEntrypointCallee(Wasm::CompilationMode mode) : Callee(mode) { }
};

class JSEntrypointJITCallee final : public JSEntrypointCallee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(JSEntrypointJITCallee);
public:
    friend class Callee;

#if ENABLE(JIT)
    void setEntrypoint(Wasm::Entrypoint&&);
#endif

    static inline Ref<JSEntrypointJITCallee> create()
    {
        return adoptRef(*new JSEntrypointJITCallee);
    }

private:
    inline JSEntrypointJITCallee()
        : JSEntrypointCallee(Wasm::CompilationMode::JSEntrypointJITMode)
    {
    }

#if ENABLE(JIT)
    std::tuple<void*, void*> rangeImpl() const
    {
        void* start = m_entrypoint.compilation->codeRef().executableMemory()->start().untaggedPtr();
        void* end = m_entrypoint.compilation->codeRef().executableMemory()->end().untaggedPtr();
        return { start, end };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint.compilation->code().retagged<WasmEntryPtrTag>(); }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return &m_entrypoint.calleeSaveRegisters; }
#else
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }
    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }
    RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
#endif

#if ENABLE(JIT)
    Wasm::Entrypoint m_entrypoint;
#endif
};

#define FOR_EACH_JS_TO_WASM_WRAPPER_METADATA_OPCODE(macro) \
/* Load/Store accumulator; followed by (offset from cfr : int8_t) */ \
macro(0, LoadI32) \
macro(1, LoadI64) \
macro(2, LoadF32) \
macro(3, LoadF64) \
macro(4, StoreI32) \
macro(5, StoreI64) \
macro(6, StoreF32) \
macro(7, StoreF64) \
/* Box/unbox accumulator */ \
macro(8, BoxInt32) \
macro(9, BoxInt64) \
macro(10, BoxFloat32) \
macro(11, BoxFloat64) \
macro(12, UnBoxInt32) \
macro(13, UnBoxInt64) \
macro(14, UnBoxFloat32) \
macro(15, UnBoxFloat64) \
/* Move constant to accumulator */ \
macro(16, Zero) \
macro(17, Undefined) \
/* JSValue32_64 only: copy tag accumulator to payload accumulator. */ \
macro(18, ShiftTag) \
/* No args; mark a phase in stack creation */ \
macro(19, Memory) \
macro(20, Call) \
macro(21, Done) \
/* Write accumulator to register */ \
macro(22, WA0) \
macro(23, WA1) \
macro(24, WA2) \
macro(25, WA3) \
macro(26, WA4) \
macro(27, WA5) \
macro(28, WA6) \
macro(29, WA7) \
/* Result registers */ \
macro(30, WR0) \
macro(31, WA0_READ) \
macro(32, WR1) \
/* FP registers */ \
macro(33, WAF0) \


#define DEFINE_OP(i, n) \
    n = i,


// See WasmLLIntPlan to see how this bytecode is constructed.
// This is a simple accumulator-based bytecode for setting up a frame and marshalling args
// Each argument reads or writes to the accumulator
// On JSValue32_64 platforms, reg is actually two registers for 64-bit operations
enum class JSEntrypointInterpreterCalleeMetadata : int8_t {
    FOR_EACH_JS_TO_WASM_WRAPPER_METADATA_OPCODE(DEFINE_OP)

    InvalidRegister,

    // We mask opcodes in the llint to prevent arbitrary jumps.
    OpcodeMask = 63,

    // This is the first byte of every program
    FrameSize, // size : int8_t

    InvalidOp,
};

#undef DEFINE_OP

enum class MetadataReadMode {
    Read,
    Write
};

#if USE(JSVALUE64)

constexpr inline JSEntrypointInterpreterCalleeMetadata jsEntrypointMetadataForGPR(GPRReg g, MetadataReadMode mode)
{
    using enum JSEntrypointInterpreterCalleeMetadata;
    using enum MetadataReadMode;
    if (mode == Write) {
        for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; ++i) {
            if (g == GPRInfo::toArgumentRegister(i)) {
                return static_cast<JSEntrypointInterpreterCalleeMetadata>(
                    static_cast<int8_t>(WA0) + i);
            }
        }

        if (g == GPRInfo::returnValueGPR)
            return WR0;
        if (g == GPRInfo::returnValueGPR2)
            return WR1;

        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(false);
        return InvalidRegister;
    }

    if (g == GPRInfo::argumentGPR0)
        return WA0_READ;

    RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(false);
    return InvalidRegister;
}

constexpr inline JSEntrypointInterpreterCalleeMetadata jsEntrypointMetadataForFPR(FPRReg f, MetadataReadMode mode)
{
    using enum JSEntrypointInterpreterCalleeMetadata;
    using enum MetadataReadMode;
    RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(mode == Write);

    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; ++i) {
        if (f == FPRInfo::toArgumentRegister(i)) {
            return static_cast<JSEntrypointInterpreterCalleeMetadata>(
                static_cast<int8_t>(WAF0) + i);
        }
    }
    RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(false);
    return InvalidRegister;
}

inline void dumpJSEntrypointInterpreterCalleeMetadata(const Vector<JSEntrypointInterpreterCalleeMetadata>& data)
{
    using enum JSEntrypointInterpreterCalleeMetadata;
    constexpr auto printOp = [](JSEntrypointInterpreterCalleeMetadata o) {
        switch (o) {
        case Memory: dataLog("Memory"); break;
        case Done: dataLog("Done"); break;
        case FrameSize: dataLog("FrameSize"); break;
        case LoadI32: dataLog("LoadI32"); break;
        case LoadF32: dataLog("LoadF32"); break;
        case LoadF64: dataLog("LoadF64"); break;
        case StoreI32: dataLog("StoreI32"); break;
        case StoreF32: dataLog("StoreF32"); break;
        case StoreF64: dataLog("StoreF64"); break;
        case LoadI64: dataLog("LoadI64"); break;
        case StoreI64: dataLog("StoreI64"); break;
        case BoxInt32: dataLog("BoxInt32"); break;
        case BoxFloat32: dataLog("BoxFloat32"); break;
        case BoxFloat64: dataLog("BoxFloat64"); break;
        case UnBoxInt32: dataLog("UnBoxInt32"); break;
        case UnBoxFloat32: dataLog("UnBoxFloat32"); break;
        case UnBoxFloat64: dataLog("UnBoxFloat64"); break;
        case Zero: dataLog("Zero"); break;
        case Undefined: dataLog("Undefined"); break;
        case BoxInt64: dataLog("BoxInt64"); break;
        case UnBoxInt64: dataLog("UnBoxInt64"); break;
        case Call: dataLog("Call"); break;
        case WA0_READ: dataLog("WA0_READ"); break;
        default: {
            RELEASE_ASSERT(o >= WA0);
            RELEASE_ASSERT(o < InvalidRegister);
            if (o < WR0) {
                dataLog("WA");
                dataLog(static_cast<int8_t>(o) - static_cast<int8_t>(WA0));
            } else if (o < WAF0) {
                dataLog("R");
                dataLog(static_cast<int8_t>(o) - static_cast<int8_t>(WR0));
            } else {
                dataLog("FA");
                dataLog(static_cast<int8_t>(o) - static_cast<int8_t>(WAF0));
            }
        }
        }
        dataLog(" ");
    };
    constexpr auto printOffset = [](JSEntrypointInterpreterCalleeMetadata o) {
        dataLog("cfr[", static_cast<int8_t>(o), "]");
        dataLog(" ");
    };
    for (unsigned pc = 0; pc < data.size();) {
        switch (data[pc]) {
        case FrameSize:
            printOp(data[pc++]);
            dataLog(static_cast<int>(data[pc++]));
            break;
        case LoadI32:
        case LoadF32:
        case LoadF64:
        case StoreI32:
        case StoreF32:
        case StoreF64:
        case LoadI64:
        case StoreI64:
            printOp(data[pc++]);
            printOffset(data[pc++]);
            break;
        default: printOp(data[pc++]);
        }
        dataLogLn();
    }
}

#endif // USE(JSVALUE64)

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(JSEntrypointInterpreterCallee);
class JSEntrypointInterpreterCallee final : public JSEntrypointCallee,
    public TrailingArray<JSEntrypointInterpreterCallee, JSEntrypointInterpreterCalleeMetadata> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(JSEntrypointInterpreterCallee);
public:
    static Ref<JSEntrypointInterpreterCallee> create(Vector<JSEntrypointInterpreterCalleeMetadata>&& metadata, LLIntCallee* callee)
    {
        auto metadataSize = metadata.size();
        return adoptRef(*new (NotNull, JSEntrypointInterpreterCalleeMalloc::malloc(JSEntrypointInterpreterCallee::allocationSize(metadataSize))) JSEntrypointInterpreterCallee(WTFMove(metadata), callee));
    }

    void setReplacement(RefPtr<Wasm::Callee> callee)
    {
        ASSERT(!m_replacementCallee);
        ASSERT(callee);
        m_replacementCallee = WTFMove(callee);
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const;
    JS_EXPORT_PRIVATE RegisterAtOffsetList* calleeSaveRegistersImpl();
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; }

    static ptrdiff_t offsetOfWasmCallee() { return OBJECT_OFFSETOF(JSEntrypointInterpreterCallee, wasmCallee); }
    static ptrdiff_t offsetOfWasmFunctionPrologue() { return OBJECT_OFFSETOF(JSEntrypointInterpreterCallee, wasmFunctionPrologue); }
    static constexpr ptrdiff_t offsetOfMetadataStorage() { return offsetOfData(); }

private:
    JSEntrypointInterpreterCallee(Vector<JSEntrypointInterpreterCalleeMetadata>&&, LLIntCallee*);

public:
    const intptr_t ident { 0xBF };
    const intptr_t wasmCallee;
    // In the JIT case, we want to always call the llint prologue from a jit function.
    // In the no-jit case, we dont' care.
    CodePtr<LLintToWasmEntryPtrTag> wasmFunctionPrologue;
    RefPtr<Wasm::Callee> m_replacementCallee { nullptr };
};

class WasmToJSCallee final : public Callee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(WasmToJSCallee);
public:
    friend class Callee;

    static Ref<WasmToJSCallee> create()
    {
        return adoptRef(*new WasmToJSCallee);
    }

private:
    WasmToJSCallee();

    std::tuple<void*, void*> rangeImpl() const
    {
        return { nullptr, nullptr };
    }

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return { }; }

    RegisterAtOffsetList* calleeSaveRegistersImpl() { return nullptr; }
};

#if ENABLE(JIT)
class JSToWasmICCallee final : public JITCallee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(JSToWasmICCallee);
public:
    static Ref<JSToWasmICCallee> create()
    {
        return adoptRef(*new JSToWasmICCallee);
    }

    using JITCallee::setEntrypoint;

private:
    JSToWasmICCallee()
        : JITCallee(Wasm::CompilationMode::JSToWasmICMode)
    {
    }
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(OptimizingJITCallee);
public:
    const StackMap& stackmap(CallSiteIndex) const;

    void addCodeOrigin(unsigned firstInlineCSI, unsigned lastInlineCSI, const Wasm::ModuleInformation&, uint32_t functionIndex);
    IndexOrName getOrigin(unsigned csi, unsigned depth, bool& isInlined) const;

protected:
    OptimizingJITCallee(Wasm::CompilationMode mode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
        : JITCallee(mode, index, WTFMove(name))
    {
    }

    void setEntrypoint(Wasm::Entrypoint&& entrypoint, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& unlinkedExceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        m_wasmToWasmCallsites = WTFMove(unlinkedCalls);
        m_stackmaps = WTFMove(stackmaps);
        RELEASE_ASSERT(unlinkedExceptionHandlers.size() == exceptionHandlerLocations.size());
        linkExceptionHandlers(WTFMove(unlinkedExceptionHandlers), WTFMove(exceptionHandlerLocations));
        JITCallee::setEntrypoint(WTFMove(entrypoint));
    }

private:
    void linkExceptionHandlers(Vector<UnlinkedHandlerInfo>, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>);

    StackMaps m_stackmaps;
    Vector<WasmCodeOrigin, 0> codeOrigins;
    Vector<Ref<NameSection>, 0> nameSections;
};

constexpr int32_t stackCheckUnset = 0;
constexpr int32_t stackCheckNotNeeded = -1;

class OSREntryCallee final : public OptimizingJITCallee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(OSREntryCallee);
public:
    static Ref<OSREntryCallee> create(CompilationMode compilationMode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, uint32_t loopIndex)
    {
        return adoptRef(*new OSREntryCallee(compilationMode, index, WTFMove(name), loopIndex));
    }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    uint32_t loopIndex() const { return m_loopIndex; }


    void setEntrypoint(Wasm::Entrypoint&& entrypoint, unsigned osrEntryScratchBufferSize, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations)
    {
        m_osrEntryScratchBufferSize = osrEntryScratchBufferSize;
        OptimizingJITCallee::setEntrypoint(WTFMove(entrypoint), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations));
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
    OSREntryCallee(CompilationMode compilationMode, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, uint32_t loopIndex)
        : OptimizingJITCallee(compilationMode, index, WTFMove(name))
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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(OMGCallee);
public:
    static Ref<OMGCallee> create(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new OMGCallee(index, WTFMove(name)));
    }

    using OptimizingJITCallee::setEntrypoint;

private:
    OMGCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
        : OptimizingJITCallee(Wasm::CompilationMode::OMGMode, index, WTFMove(name))
    {
    }
};

#endif // ENABLE(WEBASSEMBLY_OMGJIT)

#if ENABLE(WEBASSEMBLY_BBQJIT)


class BBQCallee final : public OptimizingJITCallee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(BBQCallee);
public:
    static constexpr unsigned extraOSRValuesForLoopIndex = 1;

    static Ref<BBQCallee> create(size_t index, std::pair<const Name*,
        RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, SavedFPWidth savedFPWidth)
    {
        return adoptRef(*new BBQCallee(index, WTFMove(name), WTFMove(tierUpCount), savedFPWidth));
    }

    OSREntryCallee* osrEntryCallee() { return m_osrEntryCallee.get(); }
    void setOSREntryCallee(Ref<OSREntryCallee>&& osrEntryCallee, MemoryMode)
    {
        m_osrEntryCallee = WTFMove(osrEntryCallee);
    }

    bool didStartCompilingOSREntryCallee() const { return m_didStartCompilingOSREntryCallee; }
    void setDidStartCompilingOSREntryCallee(bool value) { m_didStartCompilingOSREntryCallee = value; }

#if ENABLE(WEBASSEMBLY_OMGJIT)
    OMGCallee* replacement() { return m_replacement.get(); }
    void setReplacement(Ref<OMGCallee>&& replacement)
    {
        m_replacement = WTFMove(replacement);
    }
#else

#endif

    TierUpCount* tierUpCount() { return m_tierUpCount.get(); }

    std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint() { return m_sharedLoopEntrypoint; }
    const Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints() { return m_loopEntrypoints; }

    unsigned osrEntryScratchBufferSize() const { return m_osrEntryScratchBufferSize; }
    SavedFPWidth savedFPWidth() const { return m_savedFPWidth; }

    void setEntrypoint(Wasm::Entrypoint&& entrypoint, Vector<UnlinkedWasmToWasmCall>&& unlinkedCalls, StackMaps&& stackmaps, Vector<UnlinkedHandlerInfo>&& exceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>&& exceptionHandlerLocations, Vector<CodeLocationLabel<WasmEntryPtrTag>>&& loopEntrypoints, std::optional<CodeLocationLabel<WasmEntryPtrTag>> sharedLoopEntrypoint, unsigned osrEntryScratchBufferSize)
    {
        m_sharedLoopEntrypoint = sharedLoopEntrypoint;
        m_loopEntrypoints = WTFMove(loopEntrypoints);
        m_osrEntryScratchBufferSize = osrEntryScratchBufferSize;
        OptimizingJITCallee::setEntrypoint(WTFMove(entrypoint), WTFMove(unlinkedCalls), WTFMove(stackmaps), WTFMove(exceptionHandlers), WTFMove(exceptionHandlerLocations));
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
    BBQCallee(size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name, std::unique_ptr<TierUpCount>&& tierUpCount, SavedFPWidth savedFPWidth)
        : OptimizingJITCallee(Wasm::CompilationMode::BBQMode, index, WTFMove(name))
        , m_tierUpCount(WTFMove(tierUpCount))
        , m_savedFPWidth(savedFPWidth)
    {
    }

    RefPtr<OSREntryCallee> m_osrEntryCallee;
#if ENABLE(WEBASSEMBLY_OMGJIT)
    RefPtr<OMGCallee> m_replacement;
#endif
    std::unique_ptr<TierUpCount> m_tierUpCount;
    std::optional<CodeLocationLabel<WasmEntryPtrTag>> m_sharedLoopEntrypoint;
    Vector<CodeLocationLabel<WasmEntryPtrTag>> m_loopEntrypoints;
    unsigned m_osrEntryScratchBufferSize { 0 };
    unsigned m_stackCheckSize { 0 };
    bool m_didStartCompilingOSREntryCallee { false };
    SavedFPWidth m_savedFPWidth { SavedFPWidth::DontSaveVectors };
    Vector<UniqueRef<EmbeddedFixedVector<CodeLocationLabel<JSSwitchPtrTag>>>> m_switchJumpTables;
};
#endif


class IPIntCallee final : public Callee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(IPIntCallee);
    friend class JSC::LLIntOffsetsExtractor;
    friend class Callee;
public:
    static Ref<IPIntCallee> create(FunctionIPIntMetadataGenerator& generator, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new IPIntCallee(generator, index, WTFMove(name)));
    }

    uint32_t functionIndex() const { return m_functionIndex; }
    void setEntrypoint(CodePtr<WasmEntryPtrTag>);
    const uint8_t* getBytecode() const { return m_bytecode; }
    const uint8_t* getMetadata() const { return m_metadata; }

    const TypeDefinition& signature(unsigned index) const
    {
        return *m_signatures[index];
    }

    IPIntTierUpCounter& tierUpCounter() { return m_tierUpCounter; }

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    JITCallee* replacement(MemoryMode mode) { return m_replacements[static_cast<uint8_t>(mode)].get(); }
    void setReplacement(Ref<OptimizingJITCallee>&& replacement, MemoryMode mode)
    {
        m_replacements[static_cast<uint8_t>(mode)] = WTFMove(replacement);
    }

    OSREntryCallee* osrEntryCallee(MemoryMode mode) { return m_osrEntryCallees[static_cast<uint8_t>(mode)].get(); }
    void setOSREntryCallee(Ref<OSREntryCallee>&& osrEntryCallee, MemoryMode mode)
    {
        m_osrEntryCallees[static_cast<uint8_t>(mode)] = WTFMove(osrEntryCallee);
    }
#endif

    using OutOfLineJumpTargets = HashMap<WasmInstructionStream::Offset, int>;

private:
    IPIntCallee(FunctionIPIntMetadataGenerator&, size_t index, std::pair<const Name*, RefPtr<NameSection>>&&);

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint; }
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; };
    JS_EXPORT_PRIVATE RegisterAtOffsetList* calleeSaveRegistersImpl();

    uint32_t m_functionIndex { 0 };
#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<OptimizingJITCallee> m_replacements[numberOfMemoryModes];
    RefPtr<OSREntryCallee> m_osrEntryCallees[numberOfMemoryModes];
#endif
    CodePtr<WasmEntryPtrTag> m_entrypoint;
    FixedVector<const TypeDefinition*> m_signatures;
public:
    // I couldn't figure out how to stop LLIntOffsetsExtractor.cpp from yelling at me.
    // So just making these public.
    const uint8_t* m_bytecode;
    const uint32_t m_bytecodeLength;
    Vector<uint8_t> m_metadataVector;
    const uint8_t* m_metadata;
    Vector<uint8_t> m_argumINTBytecode;
    const uint8_t* m_argumINTBytecodePointer;
    const uint32_t m_returnMetadata;

    unsigned m_localSizeToAlloc;
    unsigned m_numRethrowSlotsToAlloc;
    unsigned m_numLocals;
    unsigned m_numArgumentsOnStack;
    unsigned m_maxFrameSizeInV128;

    IPIntTierUpCounter m_tierUpCounter;
};

class LLIntCallee final : public Callee {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(LLIntCallee);
    friend JSC::LLIntOffsetsExtractor;
    friend class Callee;
public:
    static Ref<LLIntCallee> create(FunctionCodeBlockGenerator& generator, size_t index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    {
        return adoptRef(*new LLIntCallee(generator, index, WTFMove(name)));
    }

    void setEntrypoint(CodePtr<WasmEntryPtrTag>);

    uint32_t functionIndex() const { return m_functionIndex; }
    unsigned numVars() const { return m_numVars; }
    unsigned numCalleeLocals() const { return m_numCalleeLocals; }
    uint32_t numArguments() const { return m_numArguments; }
    const FixedVector<Type>& constantTypes() const { return m_constantTypes; }
    const FixedVector<uint64_t>& constants() const { return m_constants; }
    const WasmInstructionStream& instructions() const { return *m_instructions; }

    ALWAYS_INLINE uint64_t getConstant(VirtualRegister reg) const { return m_constants[reg.toConstantIndex()]; }
    ALWAYS_INLINE Type getConstantType(VirtualRegister reg) const
    {
        ASSERT(Options::dumpGeneratedWasmBytecodes());
        return m_constantTypes[reg.toConstantIndex()];
    }

    WasmInstructionStream::Offset numberOfJumpTargets() { return m_jumpTargets.size(); }
    WasmInstructionStream::Offset lastJumpTarget() { return m_jumpTargets.last(); }

    const WasmInstruction* outOfLineJumpTarget(const WasmInstruction*);
    WasmInstructionStream::Offset outOfLineJumpOffset(WasmInstructionStream::Offset);
    WasmInstructionStream::Offset outOfLineJumpOffset(const WasmInstructionStream::Ref& instruction)
    {
        return outOfLineJumpOffset(instruction.offset());
    }

    inline WasmInstructionStream::Offset bytecodeOffset(const WasmInstruction* returnAddress)
    {
        const auto* instructionsBegin = m_instructions->at(0).ptr();
        const auto* instructionsEnd = reinterpret_cast<const WasmInstruction*>(reinterpret_cast<uintptr_t>(instructionsBegin) + m_instructions->size());
        RELEASE_ASSERT(returnAddress >= instructionsBegin && returnAddress < instructionsEnd);
        return returnAddress - instructionsBegin;
    }

    LLIntTierUpCounter& tierUpCounter() { return m_tierUpCounter; }

    const TypeDefinition& signature(unsigned index) const
    {
        return *m_signatures[index];
    }

    const JumpTable& jumpTable(unsigned tableIndex) const;
    unsigned numberOfJumpTables() const;

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    JITCallee* replacement(MemoryMode mode) { return m_replacements[static_cast<uint8_t>(mode)].get(); }
    void setReplacement(Ref<OptimizingJITCallee>&& replacement, MemoryMode mode)
    {
        m_replacements[static_cast<uint8_t>(mode)] = WTFMove(replacement);
    }

    OSREntryCallee* osrEntryCallee(MemoryMode mode) { return m_osrEntryCallees[static_cast<uint8_t>(mode)].get(); }
    void setOSREntryCallee(Ref<OSREntryCallee>&& osrEntryCallee, MemoryMode mode)
    {
        m_osrEntryCallees[static_cast<uint8_t>(mode)] = WTFMove(osrEntryCallee);
    }
#endif

    using OutOfLineJumpTargets = HashMap<WasmInstructionStream::Offset, int>;

private:
    LLIntCallee(FunctionCodeBlockGenerator&, size_t index, std::pair<const Name*, RefPtr<NameSection>>&&);

    CodePtr<WasmEntryPtrTag> entrypointImpl() const { return m_entrypoint; }
    std::tuple<void*, void*> rangeImpl() const { return { nullptr, nullptr }; };
    JS_EXPORT_PRIVATE RegisterAtOffsetList* calleeSaveRegistersImpl();

    uint32_t m_functionIndex { 0 };

    // Used for the number of WebAssembly locals, as in https://webassembly.github.io/spec/core/syntax/modules.html#syntax-local
    unsigned m_numVars { 0 };
    // Number of VirtualRegister. The naming is unfortunate, but has to match UnlinkedCodeBlock
    unsigned m_numCalleeLocals { 0 };
    uint32_t m_numArguments { 0 };
    FixedVector<Type> m_constantTypes;
    FixedVector<uint64_t> m_constants;
    std::unique_ptr<WasmInstructionStream> m_instructions;
    const void* m_instructionsRawPointer { nullptr };
    FixedVector<WasmInstructionStream::Offset> m_jumpTargets;
    FixedVector<const TypeDefinition*> m_signatures;
    OutOfLineJumpTargets m_outOfLineJumpTargets;
    LLIntTierUpCounter m_tierUpCounter;
    FixedVector<JumpTable> m_jumpTables;

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
    RefPtr<OptimizingJITCallee> m_replacements[numberOfMemoryModes];
    RefPtr<OSREntryCallee> m_osrEntryCallees[numberOfMemoryModes];
#endif
    CodePtr<WasmEntryPtrTag> m_entrypoint;
};

using LLIntCallees = ThreadSafeRefCountedFixedVector<Ref<LLIntCallee>>;
using IPIntCallees = ThreadSafeRefCountedFixedVector<Ref<IPIntCallee>>;

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
