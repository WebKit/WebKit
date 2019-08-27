/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WebAssemblyFunction.h"

#if ENABLE(WEBASSEMBLY)

#include "B3Compilation.h"
#include "JSCInlines.h"
#include "JSFunctionInlines.h"
#include "JSObject.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyRuntimeError.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "ProtoCallFrame.h"
#include "VM.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmFormat.h"
#include "WasmMemory.h"
#include "WasmMemoryInformation.h"
#include "WasmModuleInformation.h"
#include "WasmSignatureInlines.h"
#include <wtf/FastTLS.h>
#include <wtf/StackPointer.h>
#include <wtf/SystemTracing.h>

namespace JSC {

const ClassInfo WebAssemblyFunction::s_info = { "WebAssemblyFunction", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunction) };

static EncodedJSValue JSC_HOST_CALL callWebAssemblyFunction(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    WebAssemblyFunction* wasmFunction = jsCast<WebAssemblyFunction*>(exec->jsCallee());
    Wasm::SignatureIndex signatureIndex = wasmFunction->signatureIndex();
    const Wasm::Signature& signature = Wasm::SignatureInformation::get(signatureIndex);

    // Make sure that the memory we think we are going to run with matches the one we expect.
    ASSERT(wasmFunction->instance()->instance().codeBlock()->isSafeToRun(wasmFunction->instance()->memory()->memory().mode()));

    Optional<TraceScope> traceScope;
    if (Options::useTracePoints())
        traceScope.emplace(WebAssemblyExecuteStart, WebAssemblyExecuteEnd);

    Vector<JSValue, MarkedArgumentBuffer::inlineCapacity> boxedArgs;
    JSWebAssemblyInstance* instance = wasmFunction->instance();
    Wasm::Instance* wasmInstance = &instance->instance();
    // When we don't use fast TLS to store the context, the JS
    // entry wrapper expects a JSWebAssemblyInstance as the first argument.
    if (!Wasm::Context::useFastTLS())
        boxedArgs.append(instance);

    for (unsigned argIndex = 0; argIndex < signature.argumentCount(); ++argIndex) {
        JSValue arg = exec->argument(argIndex);
        switch (signature.argument(argIndex)) {
        case Wasm::I32:
            arg = JSValue::decode(arg.toInt32(exec));
            break;
        case Wasm::Funcref: {
            if (!isWebAssemblyHostFunction(vm, arg) && !arg.isNull())
                return JSValue::encode(throwException(exec, scope, createJSWebAssemblyRuntimeError(exec, vm, "Funcref must be an exported wasm function")));
            break;
        }
        case Wasm::Anyref:
            break;
        case Wasm::I64:
            arg = JSValue();
            break;
        case Wasm::F32:
            arg = JSValue::decode(bitwise_cast<uint32_t>(arg.toFloat(exec)));
            break;
        case Wasm::F64:
            arg = JSValue::decode(bitwise_cast<uint64_t>(arg.toNumber(exec)));
            break;
        case Wasm::Void:
        case Wasm::Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        boxedArgs.append(arg);
    }

    JSValue firstArgument = JSValue();
    int argCount = 1;
    JSValue* remainingArgs = nullptr;
    if (boxedArgs.size()) {
        remainingArgs = boxedArgs.data();
        firstArgument = *remainingArgs;
        remainingArgs++;
        argCount = boxedArgs.size();
    }

    // Note: we specifically use the WebAssemblyFunction as the callee to begin with in the ProtoCallFrame.
    // The reason for this is that calling into the llint may stack overflow, and the stack overflow
    // handler might read the global object from the callee.
    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(nullptr, wasmFunction, firstArgument, argCount, remainingArgs);

    // FIXME Do away with this entire function, and only use the entrypoint generated by B3. https://bugs.webkit.org/show_bug.cgi?id=166486
    Wasm::Instance* prevWasmInstance = vm.wasmContext.load();
    {
        // We do the stack check here for the wrapper function because we don't
        // want to emit a stack check inside every wrapper function.
        const intptr_t sp = bitwise_cast<intptr_t>(currentStackPointer());
        const intptr_t frameSize = (boxedArgs.size() + CallFrame::headerSizeInRegisters) * sizeof(Register);
        const intptr_t stackSpaceUsed = 2 * frameSize; // We're making two calls. One to the wrapper, and one to the actual wasm code.
        if (UNLIKELY((sp < stackSpaceUsed) || ((sp - stackSpaceUsed) < bitwise_cast<intptr_t>(vm.softStackLimit()))))
            return JSValue::encode(throwException(exec, scope, createStackOverflowError(exec)));
    }
    vm.wasmContext.store(wasmInstance, vm.softStackLimit());
    ASSERT(wasmFunction->instance());
    ASSERT(&wasmFunction->instance()->instance() == vm.wasmContext.load());
    EncodedJSValue rawResult = vmEntryToWasm(wasmFunction->jsEntrypoint(MustCheckArity).executableAddress(), &vm, &protoCallFrame);
    // We need to make sure this is in a register or on the stack since it's stored in Vector<JSValue>.
    // This probably isn't strictly necessary, since the WebAssemblyFunction* should keep the instance
    // alive. But it's good hygiene.
    instance->use();
    if (prevWasmInstance != wasmInstance) {
        // This is just for some extra safety instead of leaving a cached
        // value in there. If we ever forget to set the value to be a real
        // bounds, this will force every stack overflow check to immediately
        // fire. The stack limit never changes while executing except when
        // WebAssembly is used through the JSC API: API users can ask the code
        // to migrate threads.
        wasmInstance->setCachedStackLimit(bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()));
    }
    vm.wasmContext.store(prevWasmInstance, vm.softStackLimit());
    RETURN_IF_EXCEPTION(scope, { });

    return rawResult;
}

bool WebAssemblyFunction::useTagRegisters() const
{
    const auto& signature = Wasm::SignatureInformation::get(signatureIndex());
    return signature.argumentCount() || signature.returnType() != Wasm::Void;
}

RegisterSet WebAssemblyFunction::calleeSaves() const
{
    RegisterSet toSave = Wasm::PinnedRegisterInfo::get().toSave(instance()->memoryMode());
    if (useTagRegisters()) {
        RegisterSet tagRegisters = RegisterSet::runtimeTagRegisters();
        // We rely on these being disjoint sets.
#if !ASSERT_DISABLED
        for (Reg reg : tagRegisters)
            ASSERT(!toSave.contains(reg));
#endif
        toSave.merge(tagRegisters);
    }
    return toSave;
}

RegisterAtOffsetList WebAssemblyFunction::usedCalleeSaveRegisters() const
{
    return RegisterAtOffsetList { calleeSaves(), RegisterAtOffsetList::OffsetBaseType::FramePointerBased };
}

ptrdiff_t WebAssemblyFunction::previousInstanceOffset() const
{
    ptrdiff_t result = calleeSaves().numberOfSetRegisters() * sizeof(CPURegister);
    result = -result - sizeof(CPURegister);
#if !ASSERT_DISABLED
    ptrdiff_t minOffset = 1;
    for (const RegisterAtOffset& regAtOffset : usedCalleeSaveRegisters()) {
        ptrdiff_t offset = regAtOffset.offset();
        ASSERT(offset < 0);
        minOffset = std::min(offset, minOffset);
    }
    ASSERT(minOffset - static_cast<ptrdiff_t>(sizeof(CPURegister)) == result);
#endif
    return result;
}

Wasm::Instance* WebAssemblyFunction::previousInstance(CallFrame* callFrame)
{
    ASSERT(callFrame->callee().rawPtr() == m_jsToWasmICCallee.get());
    auto* result = *bitwise_cast<Wasm::Instance**>(bitwise_cast<char*>(callFrame) + previousInstanceOffset());
    return result;
}

MacroAssemblerCodePtr<JSEntryPtrTag> WebAssemblyFunction::jsCallEntrypointSlow()
{
    VM& vm = this->vm();
    CCallHelpers jit;

    const auto& signature = Wasm::SignatureInformation::get(signatureIndex());
    const auto& pinnedRegs = Wasm::PinnedRegisterInfo::get();
    RegisterAtOffsetList registersToSpill = usedCalleeSaveRegisters();

    auto& moduleInformation = instance()->instance().module().moduleInformation();

    unsigned totalFrameSize = registersToSpill.size() * sizeof(CPURegister);
    totalFrameSize += sizeof(CPURegister); // Slot for the VM's previous wasm instance.
    totalFrameSize += Wasm::WasmCallingConvention::headerSizeInBytes();
    totalFrameSize -= sizeof(CallerFrameAndPC);

    unsigned numGPRs = 0;
    unsigned numFPRs = 0;
    bool argumentsIncludeI64 = false;
    for (unsigned i = 0; i < signature.argumentCount(); i++) {
        switch (signature.argument(i)) {
        case Wasm::I64:
            argumentsIncludeI64 = true;
            break;
        case Wasm::Anyref:
        case Wasm::Funcref:
        case Wasm::I32:
            if (numGPRs >= Wasm::wasmCallingConvention().m_gprArgs.size())
                totalFrameSize += sizeof(CPURegister);
            ++numGPRs;
            break;
        case Wasm::F32:
        case Wasm::F64:
            if (numFPRs >= Wasm::wasmCallingConvention().m_fprArgs.size())
                totalFrameSize += sizeof(CPURegister);
            ++numFPRs;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    if (argumentsIncludeI64)
        return nullptr;

    totalFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), totalFrameSize);

    jit.emitFunctionPrologue();
    jit.subPtr(MacroAssembler::TrustedImm32(totalFrameSize), MacroAssembler::stackPointerRegister);
    jit.store64(CCallHelpers::TrustedImm64(0), CCallHelpers::addressFor(CallFrameSlot::codeBlock));

    for (const RegisterAtOffset& regAtOffset : registersToSpill) {
        GPRReg reg = regAtOffset.reg().gpr();
        ptrdiff_t offset = regAtOffset.offset();
        jit.storePtr(reg, CCallHelpers::Address(GPRInfo::callFrameRegister, offset));
    }

    GPRReg scratchGPR = Wasm::wasmCallingConventionAir().prologueScratch(1);
    bool stackLimitGPRIsClobbered = false;
    GPRReg stackLimitGPR = Wasm::wasmCallingConventionAir().prologueScratch(0);
    jit.loadPtr(vm.addressOfSoftStackLimit(), stackLimitGPR);

    CCallHelpers::JumpList slowPath;
    slowPath.append(jit.branchPtr(CCallHelpers::Above, MacroAssembler::stackPointerRegister, GPRInfo::callFrameRegister));
    slowPath.append(jit.branchPtr(CCallHelpers::Below, MacroAssembler::stackPointerRegister, stackLimitGPR));

    // Ensure:
    // argCountPlusThis - 1 >= signature.argumentCount()
    // argCountPlusThis >= signature.argumentCount() + 1
    // FIXME: We should handle mismatched arity
    // https://bugs.webkit.org/show_bug.cgi?id=196564
    slowPath.append(jit.branch32(CCallHelpers::Below,
        CCallHelpers::payloadFor(CallFrameSlot::argumentCount), CCallHelpers::TrustedImm32(signature.argumentCount() + 1)));

    if (useTagRegisters())
        jit.emitMaterializeTagCheckRegisters();

    // First we do stack slots for FPRs so we can use FPR argument registers as scratch.
    // After that, we handle FPR argument registers.
    // We also handle all GPR types here as we have GPR scratch registers.
    {
        CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));
        numGPRs = 0;
        numFPRs = 0;
        FPRReg scratchFPR = Wasm::wasmCallingConvention().m_fprArgs[0].fpr();

        ptrdiff_t jsOffset = CallFrameSlot::firstArgument * sizeof(EncodedJSValue);

        ptrdiff_t wasmOffset = CallFrame::headerSizeInRegisters * sizeof(CPURegister);
        for (unsigned i = 0; i < signature.argumentCount(); i++) {
            switch (signature.argument(i)) {
            case Wasm::I32:
                jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchGPR);
                slowPath.append(jit.branchIfNotInt32(scratchGPR));
                if (numGPRs >= Wasm::wasmCallingConvention().m_gprArgs.size()) {
                    jit.store32(scratchGPR, calleeFrame.withOffset(wasmOffset));
                    wasmOffset += sizeof(CPURegister);
                } else {
                    jit.zeroExtend32ToPtr(scratchGPR, Wasm::wasmCallingConvention().m_gprArgs[numGPRs].gpr());
                    ++numGPRs;
                }
                break;
            case Wasm::Funcref: {
                // Ensure we have a WASM exported function.
                jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchGPR);
                auto isNull = jit.branchIfNull(scratchGPR);
                slowPath.append(jit.branchIfNotCell(scratchGPR));

                stackLimitGPRIsClobbered = true;
                jit.emitLoadStructure(vm, scratchGPR, scratchGPR, stackLimitGPR);
                jit.loadPtr(CCallHelpers::Address(scratchGPR, Structure::classInfoOffset()), scratchGPR);

                static_assert(std::is_final<WebAssemblyFunction>::value, "We do not check for subtypes below");
                static_assert(std::is_final<WebAssemblyWrapperFunction>::value, "We do not check for subtypes below");

                auto isWasmFunction = jit.branchPtr(CCallHelpers::Equal, scratchGPR, CCallHelpers::TrustedImmPtr(WebAssemblyFunction::info()));
                slowPath.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(WebAssemblyWrapperFunction::info())));

                isWasmFunction.link(&jit);
                isNull.link(&jit);
                FALLTHROUGH;
            }
            case Wasm::Anyref: {
                jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchGPR);

                if (numGPRs >= Wasm::wasmCallingConvention().m_gprArgs.size()) {
                    jit.store64(scratchGPR, calleeFrame.withOffset(wasmOffset));
                    wasmOffset += sizeof(CPURegister);
                } else {
                    jit.move(scratchGPR, Wasm::wasmCallingConvention().m_gprArgs[numGPRs].gpr());
                    ++numGPRs;
                }
                break;
            }
            case Wasm::F32:
            case Wasm::F64:
                if (numFPRs >= Wasm::wasmCallingConvention().m_fprArgs.size()) {
                    jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchGPR);
                    slowPath.append(jit.branchIfNotNumber(scratchGPR));
                    auto isInt32 = jit.branchIfInt32(scratchGPR);
                    if (signature.argument(i) == Wasm::F32) {
                        jit.unboxDouble(scratchGPR, scratchGPR, scratchFPR);
                        jit.convertDoubleToFloat(scratchFPR, scratchFPR);
                        jit.storeFloat(scratchFPR, calleeFrame.withOffset(wasmOffset));
                    } else {
                        jit.add64(GPRInfo::tagTypeNumberRegister, scratchGPR, scratchGPR);
                        jit.store64(scratchGPR, calleeFrame.withOffset(wasmOffset));
                    }
                    auto done = jit.jump();

                    isInt32.link(&jit);
                    if (signature.argument(i) == Wasm::F32) {
                        jit.convertInt32ToFloat(scratchGPR, scratchFPR);
                        jit.storeFloat(scratchFPR, calleeFrame.withOffset(wasmOffset));
                    } else {
                        jit.convertInt32ToDouble(scratchGPR, scratchFPR);
                        jit.storeDouble(scratchFPR, calleeFrame.withOffset(wasmOffset));
                    }
                    done.link(&jit);

                    wasmOffset += sizeof(CPURegister);
                } else
                    ++numFPRs;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            jsOffset += sizeof(EncodedJSValue);
        }
    }

    // Now handle FPR arguments in registers.
    {
        numFPRs = 0;
        ptrdiff_t jsOffset = CallFrameSlot::firstArgument * sizeof(EncodedJSValue);
        for (unsigned i = 0; i < signature.argumentCount(); i++) {
            switch (signature.argument(i)) {
            case Wasm::F32:
            case Wasm::F64:
                if (numFPRs < Wasm::wasmCallingConvention().m_fprArgs.size()) {
                    FPRReg argFPR = Wasm::wasmCallingConvention().m_fprArgs[numFPRs].fpr();
                    jit.load64(CCallHelpers::Address(GPRInfo::callFrameRegister, jsOffset), scratchGPR);
                    slowPath.append(jit.branchIfNotNumber(scratchGPR));
                    auto isInt32 = jit.branchIfInt32(scratchGPR);
                    jit.unboxDouble(scratchGPR, scratchGPR, argFPR);
                    if (signature.argument(i) == Wasm::F32)
                        jit.convertDoubleToFloat(argFPR, argFPR);
                    auto done = jit.jump();

                    isInt32.link(&jit);
                    if (signature.argument(i) == Wasm::F32)
                        jit.convertInt32ToFloat(scratchGPR, argFPR);
                    else
                        jit.convertInt32ToDouble(scratchGPR, argFPR);

                    done.link(&jit);
                    ++numFPRs;
                }
                break;
            default:
                break;
            }

            jsOffset += sizeof(EncodedJSValue);
        }
    }

    // At this point, we're committed to doing a fast call.

    if (Wasm::Context::useFastTLS()) 
        jit.loadWasmContextInstance(scratchGPR);
    else
        jit.loadPtr(vm.wasmContext.pointerToInstance(), scratchGPR);
    ptrdiff_t previousInstanceOffset = this->previousInstanceOffset();
    jit.storePtr(scratchGPR, CCallHelpers::Address(GPRInfo::callFrameRegister, previousInstanceOffset));

    jit.move(CCallHelpers::TrustedImmPtr(&instance()->instance()), scratchGPR);
    if (Wasm::Context::useFastTLS()) 
        jit.storeWasmContextInstance(scratchGPR);
    else {
        jit.move(scratchGPR, pinnedRegs.wasmContextInstancePointer);
        jit.storePtr(scratchGPR, vm.wasmContext.pointerToInstance());
    }
    if (stackLimitGPRIsClobbered)
        jit.loadPtr(vm.addressOfSoftStackLimit(), stackLimitGPR);
    jit.storePtr(stackLimitGPR, CCallHelpers::Address(scratchGPR, Wasm::Instance::offsetOfCachedStackLimit()));

    if (!!moduleInformation.memory) {
        GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
        GPRReg scratchOrSize = stackLimitGPR;
        auto mode = instance()->memoryMode();

        if (isARM64E()) {
            if (mode != Wasm::MemoryMode::Signaling)
                scratchOrSize = pinnedRegs.sizeRegister;
            jit.loadPtr(CCallHelpers::Address(scratchGPR, Wasm::Instance::offsetOfCachedMemorySize()), scratchOrSize);
        } else {
            if (mode != Wasm::MemoryMode::Signaling)
                jit.loadPtr(CCallHelpers::Address(scratchGPR, Wasm::Instance::offsetOfCachedMemorySize()), pinnedRegs.sizeRegister);
        }

        jit.loadPtr(CCallHelpers::Address(scratchGPR, Wasm::Instance::offsetOfCachedMemory()), baseMemory);
        jit.cageConditionally(Gigacage::Primitive, baseMemory, scratchOrSize, scratchOrSize);
    }

    // We use this callee to indicate how to unwind past these types of frames:
    // 1. We need to know where to get callee saves.
    // 2. We need to know to restore the previous wasm context.
    if (!m_jsToWasmICCallee)
        m_jsToWasmICCallee.set(vm, this, JSToWasmICCallee::create(vm, globalObject(), this));
    jit.storePtr(CCallHelpers::TrustedImmPtr(m_jsToWasmICCallee.get()), CCallHelpers::addressFor(CallFrameSlot::callee));

    {
        // FIXME: Currently we just do an indirect jump. But we should teach the Module
        // how to repatch us:
        // https://bugs.webkit.org/show_bug.cgi?id=196570
        jit.loadPtr(entrypointLoadLocation(), scratchGPR);
        jit.call(scratchGPR, WasmEntryPtrTag);
    }

    ASSERT(!RegisterSet::runtimeTagRegisters().contains(GPRInfo::nonPreservedNonReturnGPR));
    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, previousInstanceOffset), GPRInfo::nonPreservedNonReturnGPR);
    if (Wasm::Context::useFastTLS())
        jit.storeWasmContextInstance(GPRInfo::nonPreservedNonReturnGPR);
    else
        jit.storePtr(GPRInfo::nonPreservedNonReturnGPR, vm.wasmContext.pointerToInstance());

    switch (signature.returnType()) {
    case Wasm::Void:
        jit.moveTrustedValue(jsUndefined(), JSValueRegs { GPRInfo::returnValueGPR });
        break;
    case Wasm::I32:
        jit.zeroExtend32ToPtr(GPRInfo::returnValueGPR, GPRInfo::returnValueGPR);
        jit.boxInt32(GPRInfo::returnValueGPR, JSValueRegs { GPRInfo::returnValueGPR });
        break;
    case Wasm::F32:
        jit.convertFloatToDouble(FPRInfo::returnValueFPR, FPRInfo::returnValueFPR);
        FALLTHROUGH;
    case Wasm::F64: {
        jit.moveTrustedValue(jsNumber(pureNaN()), JSValueRegs { GPRInfo::returnValueGPR });
        auto isNaN = jit.branchIfNaN(FPRInfo::returnValueFPR);
        jit.boxDouble(FPRInfo::returnValueFPR, JSValueRegs { GPRInfo::returnValueGPR });
        isNaN.link(&jit);
        break;
    }
    case Wasm::Funcref:
    case Wasm::Anyref:
        break;
    case Wasm::I64:
    case Wasm::Func:
        return nullptr;
    default:
        break;
    }

    auto emitRestoreCalleeSaves = [&] {
        for (const RegisterAtOffset& regAtOffset : registersToSpill) {
            GPRReg reg = regAtOffset.reg().gpr();
            ASSERT(reg != GPRInfo::returnValueGPR);
            ptrdiff_t offset = regAtOffset.offset();
            jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, offset), reg);
        }
    };

    emitRestoreCalleeSaves();

    jit.emitFunctionEpilogue();
    jit.ret();

    slowPath.link(&jit);
    emitRestoreCalleeSaves();
    jit.move(CCallHelpers::TrustedImmPtr(this), GPRInfo::regT0);
    jit.emitFunctionEpilogue();
#if CPU(ARM64E)
    jit.untagReturnAddress();
#endif
    auto jumpToHostCallThunk = jit.jump();

    LinkBuffer linkBuffer(jit, nullptr, JITCompilationCanFail);
    if (UNLIKELY(linkBuffer.didFailToAllocate()))
        return nullptr;

    linkBuffer.link(jumpToHostCallThunk, CodeLocationLabel<JSEntryPtrTag>(executable()->entrypointFor(CodeForCall, MustCheckArity).executableAddress()));
    m_jsCallEntrypoint = FINALIZE_CODE(linkBuffer, WasmEntryPtrTag, "JS->Wasm IC");
    return m_jsCallEntrypoint.code();
}

WebAssemblyFunction* WebAssemblyFunction::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, unsigned length, const String& name, JSWebAssemblyInstance* instance, Wasm::Callee& jsEntrypoint, Wasm::WasmToWasmImportableFunction::LoadLocation wasmToWasmEntrypointLoadLocation, Wasm::SignatureIndex signatureIndex)
{
    NativeExecutable* executable = vm.getHostFunction(callWebAssemblyFunction, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    WebAssemblyFunction* function = new (NotNull, allocateCell<WebAssemblyFunction>(vm.heap)) WebAssemblyFunction(vm, globalObject, structure, jsEntrypoint, wasmToWasmEntrypointLoadLocation, signatureIndex);
    function->finishCreation(vm, executable, length, name, instance);
    ASSERT_WITH_MESSAGE(!function->isLargeAllocation(), "WebAssemblyFunction should be allocated not in large allocation since it is JSCallee.");
    return function;
}

Structure* WebAssemblyFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

WebAssemblyFunction::WebAssemblyFunction(VM& vm, JSGlobalObject* globalObject, Structure* structure, Wasm::Callee& jsEntrypoint, Wasm::WasmToWasmImportableFunction::LoadLocation wasmToWasmEntrypointLoadLocation, Wasm::SignatureIndex signatureIndex)
    : Base { vm, globalObject, structure }
    , m_jsEntrypoint { jsEntrypoint.entrypoint() }
    , m_importableFunction { signatureIndex, wasmToWasmEntrypointLoadLocation }
{ }

void WebAssemblyFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    WebAssemblyFunction* thisObject = jsCast<WebAssemblyFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_jsToWasmICCallee);
}

void WebAssemblyFunction::destroy(JSCell* cell)
{
    static_cast<WebAssemblyFunction*>(cell)->WebAssemblyFunction::~WebAssemblyFunction();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
