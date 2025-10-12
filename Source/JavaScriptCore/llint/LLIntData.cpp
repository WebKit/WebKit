/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#include "InPlaceInterpreter.h"
#include "JSCConfig.h"
#include "LLIntCLoop.h"
#include "LLIntEntrypoint.h"
#include "LLIntSlowPaths.h"
#include "LLIntThunks.h"
#include "Opcode.h"

#if PLATFORM(COCOA)
#include <wtf/cocoa/Entitlements.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace LLInt {

#if COMPILER(CLANG)
// The purpose of applying this attribute is to move the g_opcodeConfigStorage away from other
// global variables, and allow the linker to pack them in more efficiently. Without this, the
// linker currently leaves multiple KBs of unused padding before this page though there are many
// other variables that come afterwards that would have fit in there. Adding this attribute was
// shown to resolve a memory regression on our small memory footprint benchmark.
#define LLINT_OPCODE_CONFIG_SECTION __attribute__((used, section("__DATA,__jsc_opcodes")))
#else
#define LLINT_OPCODE_CONFIG_SECTION
#endif

alignas(OpcodeConfigAlignment) Opcode LLINT_OPCODE_CONFIG_SECTION g_opcodeConfigStorage[OpcodeConfigSizeToProtect / sizeof(Opcode)];
static_assert(sizeof(OpcodeConfig) <= OpcodeConfigSizeToProtect);

#if !ENABLE(C_LOOP)
extern "C" void SYSV_ABI llint_entry(void*, void*, void*);

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

#if PLATFORM(COCOA)
static bool scriptingIsForbidden()
{
    return processHasEntitlement("com.apple.security.script-restrictions"_s);
}
#else
static constexpr bool scriptingIsForbidden() { return false; }
#endif


void initialize()
{
    WTF::makePagesFreezable(&g_opcodeConfigStorage, OpcodeConfigSizeToProtect);

    if (g_jscConfig.vmEntryDisallowed || scriptingIsForbidden()) [[unlikely]] {
        // FIXME: Check if we can do this in a more performant way. See rdar://158509720.
        g_jscConfig.vmEntryDisallowed = true;
        WTF::permanentlyFreezePages(&g_opcodeConfigStorage, OpcodeConfigSizeToProtect, WTF::FreezePagePermission::None);
        return;
    }
    WTF::compilerFence();

#if ENABLE(C_LOOP)
    CLoop::initialize();

#else // !ENABLE(C_LOOP)

    static_assert(numOpcodeIDs >= 256, "nextInstruction() relies on this for bounding the dispatch");

#if CPU(ARM64E)
    RELEASE_ASSERT(!g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::vmEntryToJavaScript)]);

    JSC::Opcode tempOpcodeMap[numOpcodeIDs];
    JSC::Opcode tempOpcodeMapWide16[numOpcodeIDs];
    JSC::Opcode tempOpcodeMapWide32[numOpcodeIDs];
    JSC::Opcode* opcodeMap = tempOpcodeMap;
    JSC::Opcode* opcodeMapWide16 = tempOpcodeMapWide16;
    JSC::Opcode* opcodeMapWide32 = tempOpcodeMapWide32;
#else
    JSC::Opcode* opcodeMap = g_opcodeMap;
    JSC::Opcode* opcodeMapWide16 = g_opcodeMapWide16;
    JSC::Opcode* opcodeMapWide32 = g_opcodeMapWide32;
#endif

    // Step 1: fill in opcodeMaps.
    llint_entry(opcodeMap, opcodeMapWide16, opcodeMapWide32);

#if ENABLE(WEBASSEMBLY)
    if (Options::useWasm())
        IPInt::initialize();
#endif

#if CPU(ARM64E)
    for (size_t i = 0; i < numOpcodeIDs; ++i) {
        g_opcodeMap[i] = removeCodePtrTag(opcodeMap[i]);
        g_opcodeMapWide16[i] = removeCodePtrTag(opcodeMapWide16[i]);
        g_opcodeMapWide32[i] = removeCodePtrTag(opcodeMapWide32[i]);
    }
#endif

    // Step 2: freeze opcodeMaps.
    WTF::compilerFence();
    WTF::permanentlyFreezePages(&g_opcodeConfigStorage, OpcodeConfigSizeToProtect, WTF::FreezePagePermission::ReadOnly);
    WTF::compilerFence();

    // Step 3: verify that the opcodeMap is expected after freezing.
#if CPU(ARM64E)
    for (size_t i = 0; i < numOpcodeIDs; ++i) {
        uintptr_t tag = (static_cast<uintptr_t>(BytecodePtrTag) << 48) | std::bit_cast<uintptr_t>(&opcodeMap[i]);
        uintptr_t tag16 = (static_cast<uintptr_t>(BytecodePtrTag) << 48) | std::bit_cast<uintptr_t>(&opcodeMapWide16[i]);
        uintptr_t tag32 = (static_cast<uintptr_t>(BytecodePtrTag) << 48) | std::bit_cast<uintptr_t>(&opcodeMapWide32[i]);

        RELEASE_ASSERT(g_opcodeMap[i] == __builtin_ptrauth_auth(opcodeMap[i], ptrauth_key_process_dependent_code, tag));
        RELEASE_ASSERT(g_opcodeMapWide16[i] == __builtin_ptrauth_auth(opcodeMapWide16[i], ptrauth_key_process_dependent_code, tag16));
        RELEASE_ASSERT(g_opcodeMapWide32[i] == __builtin_ptrauth_auth(opcodeMapWide32[i], ptrauth_key_process_dependent_code, tag32));
    }
#endif

#if ENABLE(WEBASSEMBLY)
    if (Options::useWasm())
        IPInt::verifyInitialization();
#endif

    static_assert(llint_throw_from_slow_path_trampoline < UINT8_MAX);
    for (unsigned i = 0; i < maxBytecodeStructLength + 1; ++i)
        g_jscConfig.llint.exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;

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

    // Step 4: Initialize g_jscConfig.llint.gateMap[Gate::vmEntryToJavaScript].
    // This is key to entering the interpreter.
    {
        static LazyNeverDestroyed<MacroAssemblerCodeRef<VMEntryToJITGatePtrTag>> codeRef;
        if (Options::useJIT()) {
            auto gateCodeRef = createJSGateThunk(retagCodePtr<void*, CFunctionPtrTag, OperationPtrTag>(&vmEntryToJavaScriptGateAfter), JSEntryPtrTag, "vmEntryToJavaScript");
            codeRef.construct(gateCodeRef.retagged<VMEntryToJITGatePtrTag>());
        } else
            codeRef.construct(MacroAssemblerCodeRef<VMEntryToJITGatePtrTag>::createSelfManagedCodeRef(CodePtr<VMEntryToJITGatePtrTag>::fromTaggedPtr(retagCodePtr<void*, CFunctionPtrTag, VMEntryToJITGatePtrTag>(&vmEntryToJavaScriptTrampoline))));
        g_jscConfig.llint.gateMap[static_cast<unsigned>(Gate::vmEntryToJavaScript)] = codeRef.get().code().taggedPtr();
    }
    // We want to make sure that we didn't inadvertantly authorize entry into the
    // LLInt unintentionally (due to corrupted jumps that skipped the check at the
    // top, or otherwise). So, verify again that we are allowed to enter the LLInt.
    WTF::compilerFence();
    RELEASE_ASSERT(!scriptingIsForbidden());

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

    WTF::compilerFence();
    RELEASE_ASSERT(!scriptingIsForbidden());
}

} } // namespace JSC::LLInt

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
