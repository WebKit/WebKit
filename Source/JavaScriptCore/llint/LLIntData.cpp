/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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

#include "JSCConfig.h"
#include "LLIntCLoop.h"
#include "LLIntEntrypoint.h"
#include "LLIntSlowPaths.h"
#include "LLIntThunks.h"
#include "Opcode.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace LLInt {

Opcode g_opcodeMap[numOpcodeIDs + numWasmOpcodeIDs] = { };
Opcode g_opcodeMapWide16[numOpcodeIDs + numWasmOpcodeIDs] = { };
Opcode g_opcodeMapWide32[numOpcodeIDs + numWasmOpcodeIDs] = { };

#if !ENABLE(C_LOOP)
extern "C" void SYSV_ABI llint_entry(void*, void*, void*);

#if ENABLE(WEBASSEMBLY)
extern "C" void SYSV_ABI wasm_entry(void*, void*, void*);
#endif // ENABLE(WEBASSEMBLY)

#endif // !ENABLE(C_LOOP)

#if CPU(ARM64E)
extern "C" void vmEntryToJavaScriptTrampoline(void);
extern "C" void tailCallJSEntryTrampoline(void);
extern "C" void tailCallJSEntrySlowPathTrampoline(void);
extern "C" void tailCallWithoutUntagJSEntryTrampoline(void);
extern "C" void wasmTailCallTrampoline(void);
extern "C" void exceptionHandlerTrampoline(void);
extern "C" void returnFromLLIntTrampoline(void);
#endif

#if ENABLE(CSS_SELECTOR_JIT) && CPU(ARM64E) && !ENABLE(C_LOOP)
extern "C" void SYSV_ABI vmEntryToCSSJITAfter(void);
JSC_ANNOTATE_JIT_OPERATION_RETURN(vmEntryToCSSJITAfter);
#endif

#if !ENABLE(C_LOOP)
static void neuterOpcodeMaps()
{
#if CPU(ARM64E)
#define SET_CRASH_TARGET(entry) do { \
        void* crashTarget = reinterpret_cast<void*>(llint_check_vm_entry_permission); \
        uint64_t address = std::bit_cast<uint64_t>(&entry); \
        uint64_t newTag = (std::bit_cast<uint64_t>(BytecodePtrTag) << 48) | address; \
        void* signedTarget = ptrauth_auth_and_resign(crashTarget, ptrauth_key_function_pointer, 0, ptrauth_key_process_dependent_code, newTag); \
        entry = std::bit_cast<Opcode>(signedTarget); \
    } while (false)
#else
#define SET_CRASH_TARGET(entry) do { \
        entry = reinterpret_cast<Opcode>(llint_check_vm_entry_permission); \
    } while (false)
#endif
    for (unsigned i = 0; i < numOpcodeIDs + numWasmOpcodeIDs; ++i) {
        SET_CRASH_TARGET(g_opcodeMap[i]);
        SET_CRASH_TARGET(g_opcodeMapWide16[i]);
        SET_CRASH_TARGET(g_opcodeMapWide32[i]);
    }
#undef SET_CRASH_TARGET
}
#endif

void initialize()
{
#if ENABLE(C_LOOP)
    CLoop::initialize();

#else // !ENABLE(C_LOOP)

    if (UNLIKELY(g_jscConfig.vmEntryDisallowed))
        neuterOpcodeMaps();
    else {
        llint_entry(&g_opcodeMap, &g_opcodeMapWide16, &g_opcodeMapWide32);
    
#if ENABLE(WEBASSEMBLY)
        wasm_entry(&g_opcodeMap[numOpcodeIDs], &g_opcodeMapWide16[numOpcodeIDs], &g_opcodeMapWide32[numOpcodeIDs]);
#endif // ENABLE(WEBASSEMBLY)
    }

    static_assert(llint_throw_from_slow_path_trampoline < UINT8_MAX);
    static_assert(wasm_throw_from_slow_path_trampoline < UINT8_MAX);
    for (unsigned i = 0; i < maxBytecodeStructLength + 1; ++i) {
        g_jscConfig.llint.exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;
        g_jscConfig.llint.wasmExceptionInstructions[i] = wasm_throw_from_slow_path_trampoline;
    }

    JITOperationList::populatePointersInJavaScriptCoreForLLInt();

#if CPU(ARM64E)

#if ENABLE(JIT_CAGE)
    if (Options::useJITCage())
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::jitCagePtr)] = jitCagePtrThunk().code().taggedPtr();
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
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name)] = codeRef8.get().code().taggedPtr(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide16)] = codeRef16.get().code().taggedPtr(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide32)] = codeRef32.get().code().taggedPtr(); \
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
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name)] = codeRef8.get().code().taggedPtr(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide16)] = codeRef16.get().code().taggedPtr(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##_wide32)] = codeRef32.get().code().taggedPtr(); \
    } while (0);

    JSC_WASM_GATE_OPCODES(INITIALIZE_WASM_GATE)

#endif // ENABLE(WEBASSEMBLY)

    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createJSGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(&vmEntryToJavaScriptGateAfter), JSEntryPtrTag, "vmEntryToJavaScript"));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&vmEntryToJavaScriptTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::vmEntryToJavaScript)] = codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createTailCallGate(JSEntryPtrTag, true));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&tailCallJSEntryTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::tailCallJSEntryPtrTag)]= codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createTailCallGate(JSEntryPtrTag, true));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&tailCallJSEntrySlowPathTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::tailCallJSEntrySlowPathPtrTag)] = codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createTailCallGate(JSEntryPtrTag, false));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&tailCallWithoutUntagJSEntryTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::tailCallWithoutUntagJSEntryPtrTag)]= codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createWasmTailCallGate(WasmEntryPtrTag));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&wasmTailCallTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::wasmTailCallWasmEntryPtrTag)]= codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(createWasmTailCallGate(WasmEntryPtrTag));
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&wasmTailCallTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::wasmIPIntTailCallWasmEntryPtrTag)]= codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(exceptionHandlerGateThunk());
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&exceptionHandlerTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::exceptionHandler)] = codeRef.get().code().taggedPtr();
    }
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<NativeToJITGatePtrTag>> codeRef;
        if (Options::useJIT())
            codeRef.construct(returnFromLLIntGateThunk());
        else
            codeRef.construct(MacroAssemblerCodeRef<NativeToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<NativeToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, NativeToJITGatePtrTag>(&returnFromLLIntTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::returnFromLLInt)] = codeRef.get().code().taggedPtr();
    }

    if (Options::useJIT()) {
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::loopOSREntry)] = loopOSREntryGateThunk().code().taggedPtr();
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::entryOSREntry)] = entryOSREntryGateThunk().code().taggedPtr();
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::wasmOSREntry)] = wasmOSREntryGateThunk().code().taggedPtr();
    } else {
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::loopOSREntry)] = LLInt::getCodeRef<NativeToJITGatePtrTag>(loop_osr_entry_gate).code().taggedPtr();
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::entryOSREntry)] = nullptr;
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::wasmOSREntry)] = nullptr;
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
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##Tag)] = tagCodeRef.get().code().taggedPtr(); \
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::name##Untag)] = untagCodeRef.get().code().taggedPtr(); \
    } while (0);

    INITIALIZE_TAG_AND_UNTAG_THUNKS(llint_function_for_call_arity_check);
    INITIALIZE_TAG_AND_UNTAG_THUNKS(llint_function_for_construct_arity_check);
#endif // CPU(ARM64E)
#endif // ENABLE(C_LOOP)
    g_jscConfig.defaultCallThunk = defaultCall().code().taggedPtr();
#if ENABLE(JIT)
    if (Options::useJIT())
        g_jscConfig.arityFixupThunk = arityFixupThunk().code().taggedPtr();
#endif
}

} } // namespace JSC::LLInt

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
