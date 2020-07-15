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

#include "JSCJSValueInlines.h"
#include "JSObject.h"
#include "JSObjectInlines.h"
#include "JSToWasm.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyMemory.h"
#include "JSWebAssemblyRuntimeError.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "ProtoCallFrameInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmContextInlines.h"
#include "WasmFormat.h"
#include "WasmMemory.h"
#include "WasmMemoryInformation.h"
#include "WasmModuleInformation.h"
#include "WasmSignatureInlines.h"
#include <wtf/StackPointer.h>
#include <wtf/SystemTracing.h>

namespace JSC {

const ClassInfo WebAssemblyFunction::s_info = { "WebAssemblyFunction", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunction) };

static EncodedJSValue JSC_HOST_CALL callWebAssemblyFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    WebAssemblyFunction* wasmFunction = jsCast<WebAssemblyFunction*>(callFrame->jsCallee());
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

    for (unsigned argIndex = 0; argIndex < signature.argumentCount(); ++argIndex) {
        JSValue arg = callFrame->argument(argIndex);
        switch (signature.argument(argIndex)) {
        case Wasm::I32:
            arg = JSValue::decode(arg.toInt32(globalObject));
            break;
        case Wasm::Funcref: {
            if (!isWebAssemblyHostFunction(vm, arg) && !arg.isNull())
                return JSValue::encode(throwException(globalObject, scope, createJSWebAssemblyRuntimeError(globalObject, vm, "Funcref must be an exported wasm function")));
            break;
        }
        case Wasm::Anyref:
            break;
        case Wasm::I64:
            arg = JSValue();
            break;
        case Wasm::F32:
            arg = JSValue::decode(bitwise_cast<uint32_t>(arg.toFloat(globalObject)));
            break;
        case Wasm::F64:
            arg = JSValue::decode(bitwise_cast<uint64_t>(arg.toNumber(globalObject)));
            break;
        case Wasm::Void:
        case Wasm::Func:
            RELEASE_ASSERT_NOT_REACHED();
        }
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        boxedArgs.append(arg);
    }

    // When we don't use fast TLS to store the context, the JS
    // entry wrapper expects a JSWebAssemblyInstance as the |this| value argument.
    JSValue context = Wasm::Context::useFastTLS() ? JSValue() : instance;
    JSValue* args = boxedArgs.data();
    int argCount = boxedArgs.size() + 1;

    // Note: we specifically use the WebAssemblyFunction as the callee to begin with in the ProtoCallFrame.
    // The reason for this is that calling into the llint may stack overflow, and the stack overflow
    // handler might read the global object from the callee.
    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(nullptr, globalObject, wasmFunction, context, argCount, args);

    // FIXME Do away with this entire function, and only use the entrypoint generated by B3. https://bugs.webkit.org/show_bug.cgi?id=166486
    Wasm::Instance* prevWasmInstance = vm.wasmContext.load();
    {
        // We do the stack check here for the wrapper function because we don't
        // want to emit a stack check inside every wrapper function.
        const uintptr_t sp = bitwise_cast<uintptr_t>(currentStackPointer());
        const uintptr_t frameSize = (boxedArgs.size() + CallFrame::headerSizeInRegisters) * sizeof(Register);
        const uintptr_t stackSpaceUsed = 2 * frameSize; // We're making two calls. One to the wrapper, and one to the actual wasm code.
        if (UNLIKELY((sp < stackSpaceUsed) || ((sp - stackSpaceUsed) < bitwise_cast<uintptr_t>(vm.softStackLimit()))))
            return JSValue::encode(throwException(globalObject, scope, createStackOverflowError(globalObject)));
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
    return signature.argumentCount() || !signature.returnsVoid();
}

RegisterSet WebAssemblyFunction::calleeSaves() const
{
    // Pessimistically save callee saves in BoundsChecking mode since the LLInt always bounds checks
    return Wasm::PinnedRegisterInfo::get().toSave(Wasm::MemoryMode::BoundsChecking);
}

RegisterAtOffsetList WebAssemblyFunction::usedCalleeSaveRegisters() const
{
    return RegisterAtOffsetList { calleeSaves(), RegisterAtOffsetList::OffsetBaseType::FramePointerBased };
}

ptrdiff_t WebAssemblyFunction::previousInstanceOffset() const
{
    ptrdiff_t result = calleeSaves().numberOfSetRegisters() * sizeof(CPURegister);
    result = -result - sizeof(CPURegister);
#if ASSERT_ENABLED
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

    const Wasm::WasmCallingConvention& wasmCC = Wasm::wasmCallingConvention();
    Wasm::CallInformation wasmCallInfo = wasmCC.callInformationFor(signature);
    Wasm::CallInformation jsCallInfo = Wasm::jsCallingConvention().callInformationFor(signature, Wasm::CallRole::Callee);
    RegisterAtOffsetList savedResultRegisters = wasmCallInfo.computeResultsOffsetList();

    unsigned totalFrameSize = registersToSpill.size() * sizeof(CPURegister);
    totalFrameSize += sizeof(CPURegister); // Slot for the VM's previous wasm instance.
    totalFrameSize += wasmCallInfo.headerAndArgumentStackSizeInBytes;
    totalFrameSize += savedResultRegisters.size() * sizeof(CPURegister);

    if (wasmCallInfo.argumentsIncludeI64 || wasmCallInfo.resultsIncludeI64)
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

    GPRReg scratchGPR = Wasm::wasmCallingConvention().prologueScratchGPRs[1];
    bool stackLimitGPRIsClobbered = false;
    GPRReg stackLimitGPR = Wasm::wasmCallingConvention().prologueScratchGPRs[0];
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
        CCallHelpers::payloadFor(CallFrameSlot::argumentCountIncludingThis), CCallHelpers::TrustedImm32(signature.argumentCount() + 1)));

    if (useTagRegisters())
        jit.emitMaterializeTagCheckRegisters();

    // Loop backwards so we can use the first floating point argument as a scratch.
    FPRReg scratchFPR = Wasm::wasmCallingConvention().fprArgs[0].fpr();
    for (unsigned i = signature.argumentCount(); i--;) {
        CCallHelpers::Address calleeFrame = CCallHelpers::Address(MacroAssembler::stackPointerRegister, 0);
        CCallHelpers::Address jsParam(GPRInfo::callFrameRegister, jsCallInfo.params[i].offsetFromFP());
        bool isStack = wasmCallInfo.params[i].isStackArgument();

        auto type = signature.argument(i);
        switch (type) {
        case Wasm::I32: {
            jit.load64(jsParam, scratchGPR);
            slowPath.append(jit.branchIfNotInt32(scratchGPR));
            if (isStack)
                jit.store32(scratchGPR, calleeFrame.withOffset(wasmCallInfo.params[i].offsetFromSP()));
            else
                jit.zeroExtend32ToWord(scratchGPR, wasmCallInfo.params[i].gpr());
            break;
        }
        case Wasm::Funcref: {
            // Ensure we have a WASM exported function.
            jit.load64(jsParam, scratchGPR);
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
            if (isStack) {
                jit.load64(jsParam, scratchGPR);
                jit.store64(scratchGPR, calleeFrame.withOffset(wasmCallInfo.params[i].offsetFromSP()));
            } else
                jit.load64(jsParam, wasmCallInfo.params[i].gpr());
            break;
        }
        case Wasm::F32:
        case Wasm::F64: {
            if (!isStack)
                scratchFPR = wasmCallInfo.params[i].fpr();
            auto moveToDestination = [&] () {
                if (isStack) {
                    if (signature.argument(i) == Wasm::F32)
                        jit.storeFloat(scratchFPR, calleeFrame.withOffset(wasmCallInfo.params[i].offsetFromSP()));
                    else
                        jit.storeDouble(scratchFPR, calleeFrame.withOffset(wasmCallInfo.params[i].offsetFromSP()));
                }
            };

            jit.load64(jsParam, scratchGPR);
            slowPath.append(jit.branchIfNotNumber(scratchGPR));
            auto isInt32 = jit.branchIfInt32(scratchGPR);

            jit.unboxDouble(scratchGPR, scratchGPR, scratchFPR);
            if (signature.argument(i) == Wasm::F32)
                jit.convertDoubleToFloat(scratchFPR, scratchFPR);
            moveToDestination();
            auto done = jit.jump();

            isInt32.link(&jit);
            if (signature.argument(i) == Wasm::F32) {
                jit.convertInt32ToFloat(scratchGPR, scratchFPR);
                moveToDestination();
            } else {
                jit.convertInt32ToDouble(scratchGPR, scratchFPR);
                moveToDestination();
            }
            done.link(&jit);

            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
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

    marshallJSResult(jit, signature, wasmCallInfo, savedResultRegisters);

    ASSERT(!RegisterSet::runtimeTagRegisters().contains(GPRInfo::nonPreservedNonReturnGPR));
    jit.loadPtr(CCallHelpers::Address(GPRInfo::callFrameRegister, previousInstanceOffset), GPRInfo::nonPreservedNonReturnGPR);
    if (Wasm::Context::useFastTLS())
        jit.storeWasmContextInstance(GPRInfo::nonPreservedNonReturnGPR);
    else
        jit.storePtr(GPRInfo::nonPreservedNonReturnGPR, vm.wasmContext.pointerToInstance());

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
    m_jsCallEntrypoint = FINALIZE_WASM_CODE(linkBuffer, WasmEntryPtrTag, "JS->Wasm IC");
    return m_jsCallEntrypoint.code();
}

WebAssemblyFunction* WebAssemblyFunction::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, unsigned length, const String& name, JSWebAssemblyInstance* instance, Wasm::Callee& jsEntrypoint, Wasm::WasmToWasmImportableFunction::LoadLocation wasmToWasmEntrypointLoadLocation, Wasm::SignatureIndex signatureIndex)
{
    NativeExecutable* executable = vm.getHostFunction(callWebAssemblyFunction, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    WebAssemblyFunction* function = new (NotNull, allocateCell<WebAssemblyFunction>(vm.heap)) WebAssemblyFunction(vm, executable, globalObject, structure, jsEntrypoint, wasmToWasmEntrypointLoadLocation, signatureIndex);
    function->finishCreation(vm, executable, length, name, instance);
    return function;
}

Structure* WebAssemblyFunction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    ASSERT(globalObject);
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

WebAssemblyFunction::WebAssemblyFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, Wasm::Callee& jsEntrypoint, Wasm::WasmToWasmImportableFunction::LoadLocation wasmToWasmEntrypointLoadLocation, Wasm::SignatureIndex signatureIndex)
    : Base { vm, executable, globalObject, structure }
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
