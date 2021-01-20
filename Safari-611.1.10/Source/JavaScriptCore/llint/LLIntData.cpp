/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#include "LLIntData.h"

#include "ArithProfile.h"
#include "CodeBlock.h"
#include "JSCConfig.h"
#include "LLIntCLoop.h"
#include "LLIntPCRanges.h"
#include "LLIntThunks.h"
#include "Opcode.h"
#include "WriteBarrier.h"

#define STATIC_ASSERT(cond) static_assert(cond, "LLInt assumes " #cond)

namespace JSC {

namespace LLInt {

Opcode g_opcodeMap[numOpcodeIDs + numWasmOpcodeIDs] = { };
Opcode g_opcodeMapWide16[numOpcodeIDs + numWasmOpcodeIDs] = { };
Opcode g_opcodeMapWide32[numOpcodeIDs + numWasmOpcodeIDs] = { };

#if !ENABLE(C_LOOP)
extern "C" void llint_entry(void*, void*, void*);

#if ENABLE(WEBASSEMBLY)
extern "C" void wasm_entry(void*, void*, void*);
#endif // ENABLE(WEBASSEMBLY)

#endif // !ENABLE(C_LOOP)

#if CPU(ARM64E)
extern "C" void vmEntryToJavaScriptTrampoline(void);
extern "C" void tailCallJSEntryTrampoline(void);
extern "C" void tailCallJSEntrySlowPathTrampoline(void);
extern "C" void exceptionHandlerTrampoline(void);
extern "C" void returnFromLLIntTrampoline(void);
#endif

#if ENABLE(CSS_SELECTOR_JIT) && CPU(ARM64E)
extern "C" void vmEntryToCSSJITAfter(void);
JSC_ANNOTATE_JIT_OPERATION(_JITTarget_vmEntryToCSSJITAfter, vmEntryToCSSJITAfter);
#endif

void initialize()
{
#if ENABLE(C_LOOP)
    CLoop::initialize();

#else // !ENABLE(C_LOOP)

    llint_entry(&g_opcodeMap, &g_opcodeMapWide16, &g_opcodeMapWide32);

#if ENABLE(WEBASSEMBLY)
    wasm_entry(&g_opcodeMap[numOpcodeIDs], &g_opcodeMapWide16[numOpcodeIDs], &g_opcodeMapWide32[numOpcodeIDs]);
#endif // ENABLE(WEBASSEMBLY)

    static_assert(llint_throw_from_slow_path_trampoline < UINT8_MAX);
    static_assert(wasm_throw_from_slow_path_trampoline < UINT8_MAX);
    for (unsigned i = 0; i < maxOpcodeLength + 1; ++i) {
        g_jscConfig.llint.exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;
        g_jscConfig.llint.wasmExceptionInstructions[i] = wasm_throw_from_slow_path_trampoline;
    }

    JITOperationList::populatePointersInJavaScriptCoreForLLInt();

#if CPU(ARM64E)

#if ENABLE(JIT_CAGE)
    if (Options::useJITCage())
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::jitCagePtr)] = jitCagePtrThunk().code().executableAddress();
#endif

#define INITIALIZE_JS_GATE(name, tag) \
    do { \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef8; \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef16; \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef32; \
        if (Options::useJIT()) { \
            codeRef8.construct(createJSGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(name##_return_location)), tag, #name)); \
            codeRef16.construct(createJSGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(LLInt::getWide16CodeFunctionPtr<CFunctionPtrTag>(name##_return_location)), tag, #name "_wide16")); \
            codeRef32.construct(createJSGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(LLInt::getWide32CodeFunctionPtr<CFunctionPtrTag>(name##_return_location)), tag, #name "_wide32")); \
        } else { \
            codeRef8.construct(LLInt::getCodeRef<NativeToJITGatePtrTag>(js_trampoline_##name)); \
            codeRef16.construct(LLInt::getWide16CodeRef<NativeToJITGatePtrTag>(js_trampoline_##name)); \
            codeRef32.construct(LLInt::getWide32CodeRef<NativeToJITGatePtrTag>(js_trampoline_##name)); \
        } \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name)] = codeRef8.get().code().executableAddress(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide16)] = codeRef16.get().code().executableAddress(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide32)] = codeRef32.get().code().executableAddress(); \
    } while (0);

    JSC_JS_GATE_OPCODES(INITIALIZE_JS_GATE)

#if ENABLE(WEBASSEMBLY)

#define INITIALIZE_WASM_GATE(name, tag) \
    do { \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef8; \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef16; \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef32; \
        if (Options::useJIT()) { \
            codeRef8.construct(createWasmGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(LLInt::getCodeFunctionPtr<CFunctionPtrTag>(name##_return_location)), tag, #name)); \
            codeRef16.construct(createWasmGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(LLInt::getWide16CodeFunctionPtr<CFunctionPtrTag>(name##_return_location)), tag, #name "_wide16")); \
            codeRef32.construct(createWasmGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(LLInt::getWide32CodeFunctionPtr<CFunctionPtrTag>(name##_return_location)), tag, #name "_wide32")); \
        } else { \
            codeRef8.construct(LLInt::getCodeRef<NativeToJITGatePtrTag>(wasm_trampoline_##name)); \
            codeRef16.construct(LLInt::getWide16CodeRef<NativeToJITGatePtrTag>(wasm_trampoline_##name)); \
            codeRef32.construct(LLInt::getWide32CodeRef<NativeToJITGatePtrTag>(wasm_trampoline_##name)); \
        } \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name)] = codeRef8.get().code().executableAddress(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide16)] = codeRef16.get().code().executableAddress(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide32)] = codeRef32.get().code().executableAddress(); \
    } while (0);

    JSC_WASM_GATE_OPCODES(INITIALIZE_WASM_GATE)

#endif // ENABLE(WEBASSEMBLY)

    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createJSGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(&vmEntryToJavaScriptGateAfter), JSEntryPtrTag, "vmEntryToJavaScript"));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(MacroAssemblerCodePtr<NativeToJITGatePtrTag>::createFromExecutableAddress(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&vmEntryToJavaScriptTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::vmEntryToJavaScript)] = codeRef.get().code().executableAddress();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createTailCallGate(JSEntryPtrTag));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(MacroAssemblerCodePtr<NativeToJITGatePtrTag>::createFromExecutableAddress(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&tailCallJSEntryTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::tailCallJSEntryPtrTag)]= codeRef.get().code().executableAddress();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createTailCallGate(JSEntrySlowPathPtrTag));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(MacroAssemblerCodePtr<NativeToJITGatePtrTag>::createFromExecutableAddress(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&tailCallJSEntrySlowPathTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::tailCallJSEntrySlowPathPtrTag)] = codeRef.get().code().executableAddress();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(exceptionHandlerGateThunk());
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(MacroAssemblerCodePtr<NativeToJITGatePtrTag>::createFromExecutableAddress(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&exceptionHandlerTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::exceptionHandler)] = codeRef.get().code().executableAddress();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(returnFromLLIntGateThunk());
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(MacroAssemblerCodePtr<NativeToJITGatePtrTag>::createFromExecutableAddress(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&returnFromLLIntTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::returnFromLLInt)] = codeRef.get().code().executableAddress();
    }

    if (Options::useJIT()) {
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::loopOSREntry)] = loopOSREntryGateThunk().code().executableAddress();
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::entryOSREntry)] = entryOSREntryGateThunk().code().executableAddress();
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::wasmOSREntry)] = wasmOSREntryGateThunk().code().executableAddress();
    }

#define INITIALIZE_TAG_AND_UNTAG_THUNKS(name) \
    do { \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> tagCodeRef; \
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> untagCodeRef; \
        if (Options::useJIT()) { \
            tagCodeRef.construct(tagGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(name##TagGateAfter))); \
            untagCodeRef.construct(untagGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(name##UntagGateAfter))); \
        } else { \
            tagCodeRef.construct(LLInt::getCodeRef<NativeToJITGatePtrTag>(js_trampoline_##name##_tag)); \
            untagCodeRef.construct(LLInt::getCodeRef<NativeToJITGatePtrTag>(js_trampoline_##name##_untag)); \
        } \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##Tag)] = tagCodeRef.get().code().executableAddress(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##Untag)] = untagCodeRef.get().code().executableAddress(); \
    } while (0);

    INITIALIZE_TAG_AND_UNTAG_THUNKS(llint_function_for_call_arity_check);
    INITIALIZE_TAG_AND_UNTAG_THUNKS(llint_function_for_construct_arity_check);
#endif // CPU(ARM64E)
#endif // ENABLE(C_LOOP)
}

IGNORE_WARNINGS_BEGIN("missing-noreturn")
void Data::performAssertions(VM& vm)
{
    UNUSED_PARAM(vm);
    
    // Assertions to match LowLevelInterpreter.asm.  If you change any of this code, be
    // prepared to change LowLevelInterpreter.asm as well!!

#if USE(JSVALUE64)
    const ptrdiff_t CallFrameHeaderSlots = 5;
#else // USE(JSVALUE64) // i.e. 32-bit version
    const ptrdiff_t CallFrameHeaderSlots = 4;
#endif
    const ptrdiff_t MachineRegisterSize = sizeof(CPURegister);
    const ptrdiff_t SlotSize = 8;

    STATIC_ASSERT(sizeof(Register) == SlotSize);
    STATIC_ASSERT(CallFrame::headerSizeInRegisters == CallFrameHeaderSlots);

    ASSERT(!CallFrame::callerFrameOffset());
    STATIC_ASSERT(CallerFrameAndPC::sizeInRegisters == (MachineRegisterSize * 2) / SlotSize);
    ASSERT(CallFrame::returnPCOffset() == CallFrame::callerFrameOffset() + MachineRegisterSize);
    ASSERT(static_cast<std::underlying_type_t<CallFrameSlot>>(CallFrameSlot::codeBlock) * sizeof(Register) == CallFrame::returnPCOffset() + MachineRegisterSize);
    STATIC_ASSERT(CallFrameSlot::callee * sizeof(Register) == CallFrameSlot::codeBlock * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) == CallFrameSlot::callee * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrameSlot::thisArgument * sizeof(Register) == CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + SlotSize);
    STATIC_ASSERT(CallFrame::headerSizeInRegisters == CallFrameSlot::thisArgument);

    ASSERT(CallFrame::argumentOffsetIncludingThis(0) == CallFrameSlot::thisArgument);

#if CPU(BIG_ENDIAN)
    STATIC_ASSERT(TagOffset == 0);
    STATIC_ASSERT(PayloadOffset == 4);
#else
    STATIC_ASSERT(TagOffset == 4);
    STATIC_ASSERT(PayloadOffset == 0);
#endif

#if ENABLE(C_LOOP)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif USE(JSVALUE32_64)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 1);
#elif (CPU(X86_64) && !OS(WINDOWS))  || CPU(ARM64)
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#elif (CPU(X86_64) && OS(WINDOWS))
    ASSERT(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters() == 4);
#endif

    ASSERT(!(reinterpret_cast<ptrdiff_t>((reinterpret_cast<WriteBarrier<JSCell>*>(0x4000)->slot())) - 0x4000));

    // FIXME: make these assertions less horrible.
#if ASSERT_ENABLED
    Vector<int> testVector;
    testVector.resize(42);
    ASSERT(bitwise_cast<uint32_t*>(&testVector)[sizeof(void*)/sizeof(uint32_t) + 1] == 42);
    ASSERT(bitwise_cast<int**>(&testVector)[0] == testVector.begin());
#endif

    {
        UnaryArithProfile arithProfile;
        arithProfile.argSawInt32();
        ASSERT(arithProfile.bits() == UnaryArithProfile::observedIntBits());
        ASSERT(arithProfile.argObservedType().isOnlyInt32());
    }
    {
        UnaryArithProfile arithProfile;
        arithProfile.argSawNumber();
        ASSERT(arithProfile.bits() == UnaryArithProfile::observedNumberBits());
        ASSERT(arithProfile.argObservedType().isOnlyNumber());
    }

    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawInt32();
        arithProfile.rhsSawInt32();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedIntIntBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyInt32());
        ASSERT(arithProfile.rhsObservedType().isOnlyInt32());
    }
    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawNumber();
        arithProfile.rhsSawInt32();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedNumberIntBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyNumber());
        ASSERT(arithProfile.rhsObservedType().isOnlyInt32());
    }
    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawNumber();
        arithProfile.rhsSawNumber();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedNumberNumberBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyNumber());
        ASSERT(arithProfile.rhsObservedType().isOnlyNumber());
    }
    {
        BinaryArithProfile arithProfile;
        arithProfile.lhsSawInt32();
        arithProfile.rhsSawNumber();
        ASSERT(arithProfile.bits() == BinaryArithProfile::observedIntNumberBits());
        ASSERT(arithProfile.lhsObservedType().isOnlyInt32());
        ASSERT(arithProfile.rhsObservedType().isOnlyNumber());
    }
}
IGNORE_WARNINGS_END

} } // namespace JSC::LLInt
