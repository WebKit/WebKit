/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY_B3JIT)

#include "AirCode.h"
#include "B3StackmapGenerationParams.h"
#include "B3StackmapValue.h"
#include "CCallHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "LinkBuffer.h"
#include "ProbeContext.h"
#include "WasmInstance.h"
#include "WasmOperations.h"

namespace JSC { namespace Wasm {

struct PatchpointExceptionHandle {
    PatchpointExceptionHandle(std::optional<bool> hasExceptionHandlers)
        : m_hasExceptionHandlers(hasExceptionHandlers)
    { }

    PatchpointExceptionHandle(std::optional<bool> hasExceptionHandlers, unsigned callSiteIndex, unsigned numLiveValues)
        : m_hasExceptionHandlers(hasExceptionHandlers)
        , m_callSiteIndex(callSiteIndex)
        , m_numLiveValues(numLiveValues)
    { }

    template <typename Generator>
    void generate(CCallHelpers& jit, const B3::StackmapGenerationParams& params, Generator* generator) const
    {
        if (m_callSiteIndex == s_invalidCallSiteIndex) {
            if (!m_hasExceptionHandlers || m_hasExceptionHandlers.value())
                jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
            return;
        }

        StackMap values(m_numLiveValues);
        unsigned paramsOffset = params.size() - m_numLiveValues;
        unsigned childrenOffset = params.value()->numChildren() - m_numLiveValues;
        for (unsigned i = 0; i < m_numLiveValues; ++i)
            values[i] = OSREntryValue(params[i + paramsOffset], params.value()->child(i + childrenOffset)->type());

        generator->addStackMap(m_callSiteIndex, WTFMove(values));
        jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    }

    static constexpr unsigned s_invalidCallSiteIndex = std::numeric_limits<unsigned>::max();

    std::optional<bool> m_hasExceptionHandlers;
    unsigned m_callSiteIndex { s_invalidCallSiteIndex };
    unsigned m_numLiveValues;
};


static inline void computeExceptionHandlerAndLoopEntrypointLocations(Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>& handlers, Vector<CodeLocationLabel<WasmEntryPtrTag>>& loopEntrypoints, const InternalFunction* function, const CompilationContext& context, LinkBuffer& linkBuffer)
{
    if (!context.procedure)
        return;

    unsigned entrypointIndex = 1;
    unsigned numEntrypoints = context.procedure->numEntrypoints();
    for (const UnlinkedHandlerInfo& handlerInfo : function->exceptionHandlers) {
        if (handlerInfo.m_type == HandlerType::Delegate) {
            handlers.append({ });
            continue;
        }

        RELEASE_ASSERT(entrypointIndex < numEntrypoints);
        handlers.append(linkBuffer.locationOf<ExceptionHandlerPtrTag>(context.procedure->code().entrypointLabel(entrypointIndex)));
        ++entrypointIndex;
    }

    for (; entrypointIndex < numEntrypoints; ++entrypointIndex)
        loopEntrypoints.append(linkBuffer.locationOf<WasmEntryPtrTag>(context.procedure->code().entrypointLabel(entrypointIndex)));
}

static inline void computeExceptionHandlerLocations(Vector<CodeLocationLabel<ExceptionHandlerPtrTag>>& handlers, const InternalFunction* function, const CompilationContext& context, LinkBuffer& linkBuffer)
{
    Vector<CodeLocationLabel<WasmEntryPtrTag>> ignored;
    computeExceptionHandlerAndLoopEntrypointLocations(handlers, ignored, function, context, linkBuffer);
}

static inline void emitRethrowImpl(CCallHelpers& jit)
{
    // Instance in argumentGPR0
    // exception pointer in argumentGPR1

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfVM()), scratch);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
        linkBuffer.link<OperationPtrTag>(call, operationWasmRethrow);
    });
}

static inline void emitThrowImpl(CCallHelpers& jit, unsigned exceptionIndex)
{
    // Instance in argumentGPR0
    // arguments to the exception off of stack pointer

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, Instance::offsetOfVM()), scratch);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(scratch);

    jit.move(MacroAssembler::TrustedImm32(exceptionIndex), GPRInfo::argumentGPR1);
    jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR2);
    jit.prepareWasmCallOperation(GPRInfo::argumentGPR0);
    CCallHelpers::Call call = jit.call(OperationPtrTag);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);
    jit.addLinkTask([call] (LinkBuffer& linkBuffer) {
        linkBuffer.link<OperationPtrTag>(call, operationWasmThrow);
    });
}

template<SavedFPWidth savedFPWidth>
static inline void buildEntryBufferForCatch(Probe::Context& context)
{
    unsigned valueSize = (savedFPWidth == SavedFPWidth::SaveVectors) ? 2 : 1;
    CallFrame* callFrame = context.fp<CallFrame*>();
    CallSiteIndex callSiteIndex = callFrame->callSiteIndex();
    OptimizingJITCallee* callee = bitwise_cast<OptimizingJITCallee*>(callFrame->callee().asWasmCallee());
    const StackMap& stackmap = callee->stackmap(callSiteIndex);
    VM* vm = context.gpr<VM*>(GPRInfo::regT0);
    uint64_t* buffer = vm->wasmContext.scratchBufferForSize(stackmap.size() * valueSize * 8);
    loadValuesIntoBuffer(context, stackmap, buffer, savedFPWidth);

    context.gpr(GPRInfo::argumentGPR0) = bitwise_cast<uintptr_t>(buffer);
}

static inline void buildEntryBufferForCatchSIMD(Probe::Context& context) { buildEntryBufferForCatch<SavedFPWidth::SaveVectors>(context); }
static inline void buildEntryBufferForCatchNoSIMD(Probe::Context& context) { buildEntryBufferForCatch<SavedFPWidth::DontSaveVectors>(context); }

static inline void emitCatchPrologueShared(B3::Air::Code& code, CCallHelpers& jit)
{
    JIT_COMMENT(jit, "shared catch prologue");
    jit.emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, GPRInfo::regT0);
    {
        // FIXME: Handling precise allocations in WasmB3IRGenerator catch entrypoints might be unnecessary
        // https://bugs.webkit.org/show_bug.cgi?id=231213
        auto preciseAllocationCase = jit.branchTestPtr(CCallHelpers::NonZero, GPRInfo::regT0, CCallHelpers::TrustedImm32(PreciseAllocation::halfAlignment));
        jit.andPtr(CCallHelpers::TrustedImmPtr(MarkedBlock::blockMask), GPRInfo::regT0);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, MarkedBlock::offsetOfHeader + MarkedBlock::Header::offsetOfVM()), GPRInfo::regT0);
        auto loadedCase = jit.jump();

        preciseAllocationCase.link(&jit);
        jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, PreciseAllocation::offsetOfWeakSet() + WeakSet::offsetOfVM() - PreciseAllocation::headerSize()), GPRInfo::regT0);

        loadedCase.link(&jit);
    }
    JIT_COMMENT(jit, "restore callee saves from vm entry buffer");
    jit.restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(GPRInfo::regT0, GPRInfo::regT3);

    {
        CCallHelpers::Address calleeForWasmCatch { GPRInfo::regT0, static_cast<int32_t>(VM::calleeForWasmCatchOffset()) };
        CCallHelpers::Address calleeSlot { GPRInfo::callFrameRegister, CallFrameSlot::callee * sizeof(Register) };
        JSValueRegs tmp {
#if USE(JSVALUE32_64)
            GPRInfo::nonPreservedNonArgumentGPR0,
#endif
            GPRInfo::regT3
        };
        jit.loadValue(calleeForWasmCatch, tmp);
        jit.storeValue(tmp, calleeSlot);
        jit.storeValue(JSValue { }, calleeForWasmCatch, tmp);
    }

    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT0, VM::callFrameForCatchOffset()), GPRInfo::callFrameRegister);
    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::Address(GPRInfo::regT0, VM::callFrameForCatchOffset()));

    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, CallFrameSlot::thisArgument * sizeof(Register)), GPRInfo::regT3);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::regT3, JSWebAssemblyInstance::offsetOfInstance()), GPRInfo::regT3);
    jit.storeWasmContextInstance(GPRInfo::regT3);

    jit.probe(tagCFunction<JITProbePtrTag>(code.usesSIMD() ? buildEntryBufferForCatchSIMD : buildEntryBufferForCatchNoSIMD), nullptr,
        code.usesSIMD() ? SavedFPWidth::SaveVectors : SavedFPWidth::DontSaveVectors);

    jit.addPtr(CCallHelpers::TrustedImm32(-code.frameSize()), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
}

// See AirIRGenerator64::emitTailCallPatchpoint for the setup before this.
static inline void prepareForTailCallImpl(CCallHelpers& jit, const B3::StackmapGenerationParams& params, CallInformation wasmCalleeInfoAsCallee, unsigned firstPatchArg, unsigned lastPatchArg, int32_t newFPOffsetFromFP)
{
    constexpr bool tailCallVerbose = false;
    const bool mightClobberLocals = newFPOffsetFromFP < 0;
    
    AllowMacroScratchRegisterUsage allowScratch(jit);
    JIT_COMMENT(jit, "Set up tail call, might clobber locals: ", mightClobberLocals);

    // Note: we must only use the memory temp register inside the instructions below.
    auto tmp = jit.scratchRegister();

    // Set up a valid frame so that we can clobber this one.
    RegisterAtOffsetList calleeSaves = params.code().calleeSaveRegisterAtOffsetList();
    jit.emitRestore(calleeSaves);

    const unsigned frameSize = params.code().frameSize();

    auto fpOffsetToSPOffset = [frameSize](unsigned offset) {
        return checkedSum<intptr_t>(frameSize + offset).value();
    };
    auto acit = params[lastPatchArg];
    auto retPc = params[lastPatchArg + 1];
    ASSERT(acit.isStack());
    ASSERT(retPc.isStack());

    JIT_COMMENT(jit, "Let's use the caller's frame, so that we always have a valid frame.");
    jit.probeDebugIf<tailCallVerbose>([frameSize, fpOffsetToSPOffset, newFPOffsetFromFP, acit, retPc] (Probe::Context& context) {
        uint64_t sp = context.gpr<uint64_t>(MacroAssembler::stackPointerRegister);
        uint64_t fp = context.gpr<uint64_t>(GPRInfo::callFrameRegister);
        dataLogLn("Before changing anything: FP: ", RawHex(fp), " SP: ", RawHex(sp));
        dataLogLn("New FP will be at ", RawHex(sp + fpOffsetToSPOffset(newFPOffsetFromFP)));
        dataLogLn("ACIT temp original: ", RawHex(context.gpr<uint64_t*>(GPRInfo::callFrameRegister)[acit.offsetFromFP()/sizeof(uint64_t)]));
        dataLogLn("retPc temp original: ", RawHex(context.gpr<uint64_t*>(GPRInfo::callFrameRegister)[retPc.offsetFromFP()/sizeof(uint64_t)]));
        ASSERT(sp + frameSize == fp);
    });
    jit.loadPtr(CCallHelpers::Address(MacroAssembler::framePointerRegister, CallFrame::callerFrameOffset()), MacroAssembler::framePointerRegister);
    jit.probeDebugIf<tailCallVerbose>([fpOffsetToSPOffset, acit, retPc] (Probe::Context& context) {
        uint64_t sp = context.gpr<uint64_t>(MacroAssembler::stackPointerRegister);
        uint64_t fp = context.gpr<uint64_t>(GPRInfo::callFrameRegister);
        dataLogLn("Used old frame: FP: ", RawHex(fp), " SP: ", RawHex(sp));
        dataLogLn("ACIT temp off sp: ", RawHex(context.gpr<uint64_t*>(MacroAssembler::stackPointerRegister)[fpOffsetToSPOffset(acit.offsetFromFP())/sizeof(uint64_t)]));
        dataLogLn("retPc temp off sp: ", RawHex(context.gpr<uint64_t*>(MacroAssembler::stackPointerRegister)[fpOffsetToSPOffset(retPc.offsetFromFP())/sizeof(uint64_t)]));

    });

    intptr_t lastStackArgOfset = std::numeric_limits<intptr_t>::min();
    JIT_COMMENT(jit, "Copy over args if needed into their final position, clobbering everything.");
    
    for (unsigned i = 0; i < wasmCalleeInfoAsCallee.params.size(); ++i) {
        auto dst = wasmCalleeInfoAsCallee.params[wasmCalleeInfoAsCallee.params.size() - i - 1];
        ASSERT(!dst.location.isStackArgument());
        if (!dst.location.isStack() || !mightClobberLocals)
            continue;
        auto src = params[firstPatchArg + wasmCalleeInfoAsCallee.params.size() - i - 1];
        ASSERT(src.isStack());
        intptr_t srcOffset = fpOffsetToSPOffset(src.offsetFromFP());
        intptr_t dstOffset = fpOffsetToSPOffset(checkedSum<int32_t>(dst.location.offsetFromFP(), newFPOffsetFromFP).value());
        ASSERT(srcOffset >= 0);
        ASSERT(dstOffset >= 0);

        if (tailCallVerbose)
            dataLogLn("[static] Arg ", i, " sp[", dstOffset, "]", " = sp[", srcOffset, "]; Final location is newFp[", dst.location.offsetFromFP(), "]; newFPOffsetFromFP = ", newFPOffsetFromFP, "; frameSize: ", params.code().frameSize());

        // Assert monotonic continuity.
        if (lastStackArgOfset == std::numeric_limits<intptr_t>::min())
            lastStackArgOfset = srcOffset;
        else
            lastStackArgOfset -= bytesForWidth(dst.width);
        ASSERT(lastStackArgOfset == srcOffset);
        ASSERT(dstOffset >= srcOffset);

        if (dst.width <= Width64)
            ASSERT(dst.width == Width64);
        jit.load64(CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset), tmp);
        jit.probeDebugIf<tailCallVerbose>([i, src, tmp, dstOffset, srcOffset, dst, newFPOffsetFromFP, frameSize = params.code().frameSize()] (Probe::Context& context) {
            dataLogLn("Temp stack arg copy: ", i, " patch arg: ", src, " value: ", context.gpr<uint64_t>(tmp));
            dataLogLn("sp[", dstOffset, "]", " = sp[", srcOffset, "]; Final location is newFp[", dst.location.offsetFromFP(), "]; newFPOffsetFromFP = ", newFPOffsetFromFP, "; frameSize: ", frameSize);
        });
        jit.store64(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, dstOffset));
        if (dst.width > Width64) {
            JIT_COMMENT(jit, "other 64 bits");
            jit.load64(CCallHelpers::Address(MacroAssembler::stackPointerRegister, srcOffset + sizeof(Register)), tmp);
            jit.store64(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister, dstOffset + sizeof(Register)));
        }
    }

    RELEASE_ASSERT(!mightClobberLocals || firstPatchArg + wasmCalleeInfoAsCallee.params.size() + 2 >= params.size());

    JIT_COMMENT(jit, "Now we can restore / resign lr and the argument count including this, which in wasm stores exception info.");

    jit.load64(CCallHelpers::Address(MacroAssembler::stackPointerRegister,
        fpOffsetToSPOffset(acit.offsetFromFP())), tmp);
    jit.probeDebugIf<tailCallVerbose>([tmp, fpOffsetToSPOffset, newFPOffsetFromFP, acit] (Probe::Context& context) {
        uint64_t sp = context.gpr<uint64_t>(MacroAssembler::stackPointerRegister);
        intptr_t fpOffset = CallFrameSlot::argumentCountIncludingThis * sizeof(Register);
        intptr_t newFpOffset = checkedSum<intptr_t>(fpOffset, newFPOffsetFromFP).value();
        dataLogLn("ArgumentCountIncludingThis fp[", acit.offsetFromFP(), "] or sp[", fpOffsetToSPOffset(acit.offsetFromFP()), "] value: ", RawHex(context.gpr<uint64_t>(tmp)), " overwrites: ", RawHex(bitwise_cast<uint64_t*>(sp)[fpOffsetToSPOffset(newFpOffset)/sizeof(uint64_t)]));
    });
    jit.store64(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister,
        fpOffsetToSPOffset(checkedSum<intptr_t>(CallFrameSlot::argumentCountIncludingThis * sizeof(Register), newFPOffsetFromFP).value())));

    jit.load64(CCallHelpers::Address(MacroAssembler::stackPointerRegister,
        fpOffsetToSPOffset(retPc.offsetFromFP())), tmp);
    jit.probeDebugIf<tailCallVerbose>([tmp, fpOffsetToSPOffset, newFPOffsetFromFP, retPc] (Probe::Context& context) {
        uint64_t sp = context.gpr<uint64_t>(MacroAssembler::stackPointerRegister);
        intptr_t fpOffset = CallFrame::returnPCOffset();
        intptr_t newFpOffset = checkedSum<intptr_t>(fpOffset, newFPOffsetFromFP).value();
        dataLogLn("return pc fp[", retPc.offsetFromFP(), "] or sp[", fpOffsetToSPOffset(retPc.offsetFromFP()), "] value: ", RawHex(context.gpr<uint64_t>(tmp)), " overwrites: ", RawHex(bitwise_cast<uint64_t*>(sp)[fpOffsetToSPOffset(newFpOffset)/sizeof(uint64_t)]));
    });
    jit.store64(tmp, CCallHelpers::Address(MacroAssembler::stackPointerRegister,
        fpOffsetToSPOffset(checkedSum<intptr_t>(CallFrame::returnPCOffset(), newFPOffsetFromFP).value())));

    // Pop our locals, leaving only the new frame behind as though our original caller had called the callee.
    // Also pop callee.
    auto newFPOffsetFromSP = fpOffsetToSPOffset(newFPOffsetFromFP);
    ASSERT(newFPOffsetFromSP > 0);

#if CPU(X86_64)
    jit.addPtr(MacroAssembler::TrustedImm32(newFPOffsetFromSP + CallerFrameAndPC::returnPCOffset()), MacroAssembler::stackPointerRegister);
    // Return PC is now at the top of the stack
#elif CPU(ARM) || CPU(ARM64) || CPU(RISCV64)
    JIT_COMMENT(jit, "The return pointer was loaded above.");
    jit.move(tmp, MacroAssembler::linkRegister);
#if CPU(ARM64E)
    JIT_COMMENT(jit, "The return pointer was signed with the stack height before we pushed lr, fp, see emitFunctionPrologue. newFPOffsetFromSP: ", newFPOffsetFromSP, " newFPOffsetFromFP ", newFPOffsetFromFP);
    jit.addPtr(MacroAssembler::TrustedImm32(fpOffsetToSPOffset(0) + sizeof(CallerFrameAndPC)), MacroAssembler::stackPointerRegister, tmp);
    jit.untagPtr(tmp, MacroAssembler::linkRegister);
    jit.probeDebugIf<tailCallVerbose>([] (Probe::Context& context) {
        dataLogLn("untagged return pc: ", RawHex(context.gpr<uint64_t>(MacroAssembler::linkRegister)));
    });
    JIT_COMMENT(jit, "Validate untagged pointer");
    jit.validateUntaggedPtr(MacroAssembler::linkRegister);
    jit.addPtr(MacroAssembler::TrustedImm32(newFPOffsetFromSP + sizeof(CallerFrameAndPC)), MacroAssembler::stackPointerRegister);
#endif
#else
    UNREACHABLE_FOR_PLATFORM();
#endif

#ifndef NDEBUG
    for (unsigned i = 0; i < 50; ++i) {
        // Everthing after sp might be overwritten anyway.
        jit.store64(MacroAssembler::TrustedImm32(0), CCallHelpers::Address(MacroAssembler::stackPointerRegister, -i * sizeof(uint64_t)));
    }
#endif

    JIT_COMMENT(jit, "OK, now we can jump.");
    jit.probeDebugIf<tailCallVerbose>([wasmCalleeInfoAsCallee] (Probe::Context& context) {
        dataLogLn("Can now jump: FP: ", RawHex(context.gpr<uint64_t>(GPRInfo::callFrameRegister)), " SP: ", RawHex(context.gpr<uint64_t>(MacroAssembler::stackPointerRegister)));
        auto* newFP = context.gpr<uint64_t*>(MacroAssembler::stackPointerRegister) - sizeof(CallerFrameAndPC) / sizeof(uint64_t);
        dataLogLn("New FP will be at ", RawPointer(newFP));

        for (unsigned i = 0; i < wasmCalleeInfoAsCallee.params.size(); ++i) {
            auto arg = wasmCalleeInfoAsCallee.params[i];
            dataLog("Arg ", i, " located at ", arg.location, " = ");
            if (arg.location.isGPR())
                dataLog(context.gpr(arg.location.jsr().gpr()));
            else if (arg.location.isFPR())
                dataLog(context.fpr(arg.location.fpr()));
            else
                dataLog(newFP[arg.location.offsetFromFP() / sizeof(uint64_t)]);
            dataLogLn();
        }
    });
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_B3JIT)
