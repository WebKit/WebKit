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

#include "config.h"
#include "WasmCallee.h"

#if ENABLE(WEBASSEMBLY)

#include "InPlaceInterpreter.h"
#include "JSCJSValueInlines.h"
#include "JSToWasm.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "LLIntThunks.h"
#include "NativeCalleeRegistry.h"
#include "PCToCodeOriginMap.h"
#include "VMManager.h"
#include "WasmBaselineData.h"
#include "WasmCallProfile.h"
#include "WasmCallingConvention.h"
#include "WasmModuleInformation.h"
#include "WebAssemblyBuiltin.h"
#include "WebAssemblyBuiltinTrampoline.h"
#include <wtf/SHA1.h>
#include <wtf/SixCharacterHash.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::Wasm {

WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(Callee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(JITCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(JSToWasmCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(WasmToJSCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(JSToWasmICCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(OptimizingJITCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(OMGCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(OMGOSREntryCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(BBQCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(IPIntCallee);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(WasmBuiltinCallee);

Callee::Callee(Wasm::CompilationMode compilationMode)
    : NativeCallee(NativeCallee::Category::Wasm, ImplementationVisibility::Private)
    , m_compilationMode(compilationMode)
    , m_index(0xBADBADBA)
{
}

Callee::Callee(Wasm::CompilationMode compilationMode, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : NativeCallee(NativeCallee::Category::Wasm, ImplementationVisibility::Public)
    , m_compilationMode(compilationMode)
    , m_index(index)
    , m_indexOrName(index, WTF::move(name))
{
}

void Callee::reportToVMsForDestruction()
{
    // We don't know which VMs a Module has ever run on so we just report to all of them.
    VMManager::forEachVM([&] (VM& vm) {
        if (vm.isInService())
            vm.heap.reportWasmCalleePendingDestruction(Ref(*this));
        return IterationStatus::Continue;
    });
}

template<typename Func>
inline void Callee::runWithDowncast(const Func& func)
{
    switch (m_compilationMode) {
    case CompilationMode::IPIntMode:
        func(uncheckedDowncast<IPIntCallee>(this));
        break;
    case CompilationMode::JSToWasmMode:
        func(uncheckedDowncast<JSToWasmCallee>(this));
        break;
#if ENABLE(WEBASSEMBLY_BBQJIT)
    case CompilationMode::BBQMode:
        func(uncheckedDowncast<BBQCallee>(this));
        break;
#else
    case CompilationMode::BBQMode:
        break;
#endif
#if ENABLE(WEBASSEMBLY_OMGJIT)
    case CompilationMode::OMGMode:
        func(uncheckedDowncast<OMGCallee>(this));
        break;
    case CompilationMode::OMGForOSREntryMode:
        func(uncheckedDowncast<OMGOSREntryCallee>(this));
        break;
#else
    case CompilationMode::OMGMode:
    case CompilationMode::OMGForOSREntryMode:
        break;
#endif
    case CompilationMode::JSToWasmICMode:
#if ENABLE(JIT)
        func(uncheckedDowncast<JSToWasmICCallee>(this));
#endif
        break;
    case CompilationMode::WasmToJSMode:
        func(uncheckedDowncast<WasmToJSCallee>(this));
        break;
    case CompilationMode::WasmBuiltinMode:
        func(uncheckedDowncast<WasmBuiltinCallee>(this));
        break;
    }
}

template<typename Func>
inline void Callee::runWithDowncast(const Func& func) const
{
    const_cast<Callee*>(this)->runWithDowncast(func);
}

void Callee::dump(PrintStream& out) const
{
    out.print(makeString(m_indexOrName));
}

void Callee::dumpSimpleName(PrintStream& out) const
{
    unsigned hash = 0;
    runWithDowncast([&](const auto* derived) {
        hash = derived->computeCodeHashImpl();
    });

    if (hash) {
        auto buffer = integerToSixCharacterHashString(hash);
        out.print(m_indexOrName, '#', std::span<const char> { buffer });
    } else
        out.print(m_indexOrName);
}

String Callee::nameWithHash() const
{
    StringPrintStream out;
    dumpSimpleName(out);
    return out.toString();
}

CodePtr<WasmEntryPtrTag> Callee::entrypoint() const
{
    CodePtr<WasmEntryPtrTag> codePtr;
    runWithDowncast([&](auto* derived) {
        codePtr = derived->entrypointImpl();
    });
    return codePtr;
}

std::tuple<void*, void*> Callee::range() const
{
    std::tuple<void*, void*> result;
    runWithDowncast([&](auto* derived) {
        result = derived->rangeImpl();
    });
    return result;
}

const RegisterAtOffsetList* Callee::calleeSaveRegisters()
{
    const RegisterAtOffsetList* result = nullptr;
    runWithDowncast([&](auto* derived) {
        result = derived->calleeSaveRegistersImpl();
    });
    return result;
}

void Callee::destroy(Callee* callee)
{
    callee->runWithDowncast([](auto* derived) {
        std::destroy_at(derived);
        std::decay_t<decltype(*derived)>::freeAfterDestruction(derived);
    });
}

const HandlerInfo* Callee::handlerForIndex(JSWebAssemblyInstance& instance, unsigned index, const Tag* tag)
{
    ASSERT(hasExceptionHandlers());
    return HandlerInfo::handlerForIndex(instance, m_exceptionHandlers, index, tag);
}

JITCallee::JITCallee(Wasm::CompilationMode compilationMode)
    : Callee(compilationMode)
{
}

JITCallee::JITCallee(Wasm::CompilationMode compilationMode, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : Callee(compilationMode, index, WTF::move(name))
{
}

#if ENABLE(JIT)
void JITCallee::setEntrypoint(Wasm::Entrypoint&& entrypoint)
{
    m_entrypoint = WTF::move(entrypoint);
    NativeCalleeRegistry::singleton().registerCallee(this);
}

void JSToWasmICCallee::setEntrypoint(MacroAssemblerCodeRef<JSEntryPtrTag>&& entrypoint)
{
    ASSERT(!m_jsToWasmICEntrypoint);
    m_jsToWasmICEntrypoint = WTF::move(entrypoint);
    NativeCalleeRegistry::singleton().registerCallee(this);
}
#endif

WasmToJSCallee::WasmToJSCallee()
    : Callee(Wasm::CompilationMode::WasmToJSMode)
{
    NativeCalleeRegistry::singleton().registerCallee(this);
}

WasmToJSCallee& WasmToJSCallee::singleton()
{
    static LazyNeverDestroyed<Ref<WasmToJSCallee>> callee;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&]() {
        callee.construct(adoptRef(*new WasmToJSCallee));
    });
    return callee.get().get();
}

IPIntCallee::IPIntCallee(FunctionIPIntMetadataGenerator& generator, FunctionSpaceIndex index, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : Callee(Wasm::CompilationMode::IPIntMode, index, WTF::move(name))
    , m_functionIndex(generator.m_functionIndex)
    , m_bytecode(generator.m_bytecode.data() + generator.m_bytecodeOffset)
    , m_bytecodeEnd(m_bytecode + (generator.m_bytecode.size() - generator.m_bytecodeOffset - 1))
    , m_metadata(WTF::move(generator.m_metadata))
    , m_argumINTBytecode(WTF::move(generator.m_argumINTBytecode))
    , m_uINTBytecode(WTF::move(generator.m_uINTBytecode))
    , m_callTargets(WTF::move(generator.m_callTargets))
    , m_topOfReturnStackFPOffset(generator.m_topOfReturnStackFPOffset)
    , m_localSizeToAlloc(roundUpToMultipleOf<2>(generator.m_numLocals))
    , m_numRethrowSlotsToAlloc(generator.m_numAlignedRethrowSlots)
    , m_numLocals(generator.m_numLocals)
    , m_numArgumentsOnStack(generator.m_numArgumentsOnStack)
    , m_maxFrameSizeInV128(generator.m_maxFrameSizeInV128)
    , m_tierUpCounter(WTF::move(generator.m_tierUpCounter))
{
    if (size_t count = generator.m_exceptionHandlers.size()) {
        m_exceptionHandlers = FixedVector<HandlerInfo>(count);
        for (size_t i = 0; i < count; i++) {
            const UnlinkedHandlerInfo& unlinkedHandler = generator.m_exceptionHandlers[i];
            HandlerInfo& handler = m_exceptionHandlers[i];
            CodeLocationLabel<ExceptionHandlerPtrTag> target;
            switch (unlinkedHandler.m_type) {
            case HandlerType::Catch:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::inPlaceInterpreterCatchEntryThunk().retaggedCode<ExceptionHandlerPtrTag>());
                break;
            case HandlerType::CatchAll:
            case HandlerType::Delegate:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::inPlaceInterpreterCatchAllEntryThunk().retaggedCode<ExceptionHandlerPtrTag>());
                break;
            case HandlerType::TryTableCatch:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::inPlaceInterpreterTableCatchEntryThunk().retaggedCode<ExceptionHandlerPtrTag>());
                break;
            case HandlerType::TryTableCatchRef:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::inPlaceInterpreterTableCatchRefEntryThunk().retaggedCode<ExceptionHandlerPtrTag>());
                break;
            case HandlerType::TryTableCatchAll:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::inPlaceInterpreterTableCatchAllEntryThunk().retaggedCode<ExceptionHandlerPtrTag>());
                break;
            case HandlerType::TryTableCatchAllRef:
                target = CodeLocationLabel<ExceptionHandlerPtrTag>(LLInt::inPlaceInterpreterTableCatchAllrefEntryThunk().retaggedCode<ExceptionHandlerPtrTag>());
                break;
            }

            handler.initialize(unlinkedHandler, target);
        }
    }
}

void IPIntCallee::setEntrypoint(CodePtr<WasmEntryPtrTag> entrypoint)
{
    ASSERT(!m_entrypoint);
    m_entrypoint = entrypoint;
    NativeCalleeRegistry::singleton().registerCallee(this);
}

const RegisterAtOffsetList* IPIntCallee::calleeSaveRegistersImpl()
{
    ASSERT(RegisterAtOffsetList::ipintCalleeSaveRegisters().registerCount() == numberOfIPIntCalleeSaveRegisters);
    return &RegisterAtOffsetList::ipintCalleeSaveRegisters();
}

unsigned IPIntCallee::computeCodeHashImpl() const
{
    unsigned hash = m_codeHash;
    if (hash)
        return hash;

    SHA1 sha1;

    // The maxSourceCodeLengthToHash is a heuristic to avoid crashing fuzzers
    // due to resource exhaustion. This is OK to do because:
    // 1. Hash is not a critical hash.
    // 2. In practice, reasonable source code are not 500 MB or more long.
    // 3. And if they are that long, then we are still diversifying the hash on
    //    their length. But if they do collide, it's OK.
    // The only invariant here is that we should always produce the same hash
    // for the same source string. The algorithm below achieves that.
    std::span bytecode { m_bytecode, m_bytecodeEnd };
    constexpr unsigned maxSourceCodeLengthToHash = 500 * MB;
    if (bytecode.size() < maxSourceCodeLengthToHash)
        sha1.addBytes(bytecode);
    else {
        // Just hash with the length and samples of the source string instead.
        unsigned index = 0;
        unsigned oldIndex = 0;
        unsigned length = bytecode.size();
        unsigned step = (length >> 10) + 1;

        sha1.addBytes(std::span { std::bit_cast<uint8_t*>(&length), sizeof(length) });
        do {
            auto character = bytecode[index];
            sha1.addBytes(std::span { std::bit_cast<uint8_t*>(&character), sizeof(character) });
            oldIndex = index;
            index += step;
        } while (index > oldIndex && index < length);
    }

    SHA1::Digest digest;
    sha1.computeHash(digest);
    hash = digest[0] | (digest[1] << 8) | (digest[2] << 16) | (digest[3] << 24);

    if (hash == 0)
        hash += 0x2d5a93d0;
    m_codeHash = hash;
    return hash;
}

#if ENABLE(WEBASSEMBLY_OMGJIT)
void OptimizingJITCallee::addCodeOrigin(unsigned firstInlineCSI, unsigned lastInlineCSI, const Wasm::ModuleInformation& info, uint32_t functionIndex)
{
    if (!nameSections.size())
        nameSections.append(info.nameSection);
    // The inline frame list is stored in postorder. For example:
    // A { B() C() D { E() } F() } -> B C E D F A
#if ASSERT_ENABLED
    ASSERT(firstInlineCSI <= lastInlineCSI);
    for (unsigned i = 0; i + 1 < codeOrigins.size(); ++i)
        ASSERT(codeOrigins[i].lastInlineCSI <= codeOrigins[i + 1].lastInlineCSI);
    for (unsigned i = 0; i < codeOrigins.size(); ++i)
        ASSERT(codeOrigins[i].lastInlineCSI <= lastInlineCSI);
    ASSERT(nameSections.size() == 1);
    ASSERT(nameSections[0].ptr() == info.nameSection.ptr());
#endif
    codeOrigins.append({ firstInlineCSI, lastInlineCSI, functionIndex, 0 });
}

const WasmCodeOrigin* OptimizingJITCallee::getCodeOrigin(unsigned csi, unsigned depth, bool& isInlined) const
{
    isInlined = false;
    auto iter = std::lower_bound(codeOrigins.begin(), codeOrigins.end(), WasmCodeOrigin { 0, csi, 0, 0 }, [&](const auto& a, const auto& b) {
        return b.lastInlineCSI - a.lastInlineCSI;
    });
    if (!iter || iter == codeOrigins.end())
        iter = codeOrigins.begin();
    while (iter != codeOrigins.end()) {
        if (iter->firstInlineCSI <= csi && iter->lastInlineCSI >= csi && !(depth--)) {
            isInlined = true;
            return iter;
        }
        ++iter;
    }

    return nullptr;
}

IndexOrName OptimizingJITCallee::getIndexOrName(const WasmCodeOrigin* codeOrigin) const
{
    if (!codeOrigin)
        return indexOrName();
    return IndexOrName(codeOrigin->functionIndex, nameSections[codeOrigin->moduleIndex]->get(codeOrigin->functionIndex));
}

IndexOrName OptimizingJITCallee::getOrigin(unsigned csi, unsigned depth, bool& isInlined) const
{
    if (auto* codeOrigin = getCodeOrigin(csi, depth, isInlined))
        return getIndexOrName(codeOrigin);
    return indexOrName();
}

std::optional<CallSiteIndex> OptimizingJITCallee::tryGetCallSiteIndex(const void* pc) const
{
    constexpr bool verbose = false;
    if (m_callSiteIndexMap) {
        dataLogLnIf(verbose, "Querying ", RawPointer(pc));
        if (std::optional<CodeOrigin> codeOrigin = m_callSiteIndexMap->findPC(removeCodePtrTag<void*>(pc))) {
            dataLogLnIf(verbose, "Found ", *codeOrigin);
            return CallSiteIndex { codeOrigin->bytecodeIndex().offset() };
        }
    }
    return std::nullopt;
}

const StackMap& OptimizingJITCallee::stackmap(CallSiteIndex callSiteIndex) const
{
    auto iter = m_stackmaps.find(callSiteIndex);
    if (iter == m_stackmaps.end()) {
        for (auto pair : m_stackmaps) {
            dataLog(pair.key.bits(), ": ");
            for (auto value : pair.value)
                dataLog(value, ", ");
            dataLogLn("");
        }
    }
    RELEASE_ASSERT(iter != m_stackmaps.end());
    return iter->value;
}

Box<PCToCodeOriginMap> OptimizingJITCallee::materializePCToOriginMap(B3::PCToOriginMap&& originMap, LinkBuffer& linkBuffer)
{
    constexpr bool verbose = false;
    ASSERT(originMap.ranges().size());
    dataLogLnIf(verbose, "Materializing PCToOriginMap of size: ", originMap.ranges().size());
    constexpr bool shouldBuildMapping = true;
    PCToCodeOriginMapBuilder builder(shouldBuildMapping);
    for (const B3::PCToOriginMap::OriginRange& originRange : originMap.ranges()) {
        B3::Origin b3Origin = originRange.origin;
        if (auto* origin = b3Origin.maybeWasmOrigin()) {
            // We stash the location into a BytecodeIndex.
            builder.appendItem(originRange.label, CodeOrigin(BytecodeIndex(origin->m_callSiteIndex.bits())));
        } else
            builder.appendItem(originRange.label, PCToCodeOriginMapBuilder::defaultCodeOrigin());
    }
    auto map = Box<PCToCodeOriginMap>::create(WTF::move(builder), linkBuffer);
    WTF::storeStoreFence();
    m_callSiteIndexMap = WTF::move(map);

    if (Options::useSamplingProfiler()) {
        PCToCodeOriginMapBuilder samplingProfilerBuilder(shouldBuildMapping);
        for (const B3::PCToOriginMap::OriginRange& originRange : originMap.ranges()) {
            B3::Origin b3Origin = originRange.origin;
            if (auto* origin = b3Origin.maybeWasmOrigin()) {
                // We stash the location into a BytecodeIndex.
                samplingProfilerBuilder.appendItem(originRange.label, CodeOrigin(BytecodeIndex(origin->m_opcodeOrigin.location())));
            } else
                samplingProfilerBuilder.appendItem(originRange.label, PCToCodeOriginMapBuilder::defaultCodeOrigin());
        }
        return Box<PCToCodeOriginMap>::create(WTF::move(samplingProfilerBuilder), linkBuffer);
    }
    return nullptr;
}

#endif

JSToWasmCallee::JSToWasmCallee(TypeIndex typeIndex, bool)
    : Callee(Wasm::CompilationMode::JSToWasmMode)
    , m_typeIndex(typeIndex)
{
    const TypeDefinition& signature = TypeInformation::get(typeIndex).expand();
    CallInformation wasmFrameConvention = wasmCallingConvention().callInformationFor(signature, CallRole::Caller);

    RegisterAtOffsetList savedResultRegisters = wasmFrameConvention.computeResultsOffsetList();
    size_t totalFrameSize = wasmFrameConvention.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.sizeOfAreaInBytes();
    totalFrameSize += JSToWasmCallee::RegisterStackSpaceAligned;
    totalFrameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(totalFrameSize);
    m_frameSize = totalFrameSize;
}

CodePtr<WasmEntryPtrTag> JSToWasmCallee::entrypointImpl() const
{
#if ENABLE(JIT)
    if (Options::useJIT())
        return createJSToWasmJITShared().retaggedCode<WasmEntryPtrTag>();
#endif
    return LLInt::getCodeFunctionPtr<CFunctionPtrTag>(js_to_wasm_wrapper_entry);
}

const RegisterAtOffsetList* JSToWasmCallee::calleeSaveRegistersImpl()
{
    // This must be the same to JSToWasm's callee save registers.
    // The reason is that we may use m_replacementCallee which can be set at any time.
    // So, we must store the same callee save registers at the same location to the JIT version.
#if CPU(X86_64) || CPU(ARM64) || CPU(RISCV64)
    ASSERT(RegisterAtOffsetList::wasmPinnedRegisters().registerCount() == 3);
#elif CPU(ARM)
#else
#error Unsupported architecture.
#endif
    ASSERT(WTF::roundUpToMultipleOf<stackAlignmentBytes()>(RegisterAtOffsetList::wasmPinnedRegisters().sizeOfAreaInBytes()) == SpillStackSpaceAligned);
    return &RegisterAtOffsetList::wasmPinnedRegisters();
}

#if ENABLE(WEBASSEMBLY_BBQJIT)

void OptimizingJITCallee::linkExceptionHandlers(Vector<UnlinkedHandlerInfo> unlinkedExceptionHandlers, Vector<CodeLocationLabel<ExceptionHandlerPtrTag>> exceptionHandlerLocations)
{
    size_t count = unlinkedExceptionHandlers.size();
    m_exceptionHandlers = FixedVector<HandlerInfo>(count);
    for (size_t i = 0; i < count; i++) {
        HandlerInfo& handler = m_exceptionHandlers[i];
        const UnlinkedHandlerInfo& unlinkedHandler = unlinkedExceptionHandlers[i];
        CodeLocationLabel<ExceptionHandlerPtrTag> location = exceptionHandlerLocations[i];
        handler.initialize(unlinkedHandler, location);
    }
}

unsigned OptimizingJITCallee::computeCodeHashImpl() const
{
    return m_profiledCallee->computeCodeHashImpl();
}

BBQCallee::~BBQCallee()
{
    if (Options::freeRetiredWasmCode() && m_osrEntryCallee) {
        ASSERT(m_osrEntryCallee->hasOneRef());
        m_osrEntryCallee->reportToVMsForDestruction();
    }
}

const RegisterAtOffsetList* BBQCallee::calleeSaveRegistersImpl()
{
    return &RegisterAtOffsetList::bbqCalleeSaveRegisters();
}

#endif

WasmBuiltinCallee::WasmBuiltinCallee(const WebAssemblyBuiltin* builtin, std::pair<const Name*, RefPtr<NameSection>>&& name)
    : Callee(Wasm::CompilationMode::WasmBuiltinMode, Wasm::FunctionSpaceIndex(0xDEAD), WTF::move(name))
    , m_builtin(builtin, { })
{
#if ENABLE(JIT_CAGE)
    if (Options::useJITCage()) {
        auto jitCode = generateWasmBuiltinTrampoline(*builtin);
        RELEASE_ASSERT(!!jitCode);
        m_code = jitCode.value(); // hold onto it to retain the code
        m_trampoline = m_code.code();
        return;
    }
#endif
    m_trampoline = m_builtin->wasmTrampoline();
}

} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
