/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include "LowLevelInterpreter.h"

#include "LLIntOfflineAsmConfig.h"
#include <wtf/InlineASM.h>

#if ENABLE(C_LOOP)
#include "Bytecodes.h"
#include "CLoopStackInlines.h"
#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "Interpreter.h"
#include "LLIntCLoop.h"
#include "LLIntData.h"
#include "LLIntSlowPaths.h"
#include "JSCInlines.h"
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

using namespace JSC::LLInt;

// LLInt C Loop opcodes
// ====================
// In the implementation of the C loop, the LLint trampoline glue functions
// (e.g. llint_program_prologue, llint_eval_prologue, etc) are addressed as
// if they are bytecode handlers. That means the names of the trampoline
// functions will be added to the OpcodeID list via the
// FOR_EACH_LLINT_OPCODE_EXTENSION() macro that FOR_EACH_OPCODE_ID()
// includes.
//
// In addition, some JIT trampoline functions which are needed by LLInt
// (e.g. getHostCallReturnValue, ctiOpThrowNotCaught) are also added as
// bytecodes, and the CLoop will provide bytecode handlers for them.
//
// In the CLoop, we can only dispatch indirectly to these bytecodes
// (including the LLInt and JIT extensions). All other dispatches
// (i.e. goto's) must be to a known label (i.e. local / global labels).


// How are the opcodes named?
// ==========================
// Here is a table to show examples of how each of the manifestation of the
// opcodes are named:
//
//   Type:                        Opcode            Trampoline Glue
//                                ======            ===============
//   [In the llint .asm files]
//   llint labels:                llint_op_enter    llint_program_prologue
//
//   OpcodeID:                    op_enter          llint_program
//                                [in Opcode.h]     [in LLIntOpcode.h]
//
//   When using a switch statement dispatch in the CLoop, each "opcode" is
//   a case statement:
//   Opcode:                      case op_enter:    case llint_program_prologue:
//
//   When using a computed goto dispatch in the CLoop, each opcode is a label:
//   Opcode:                      op_enter:         llint_program_prologue:


//============================================================================
// Define the opcode dispatch mechanism when using the C loop:
//

// These are for building a C Loop interpreter:
#define OFFLINE_ASM_BEGIN
#define OFFLINE_ASM_END

#if ENABLE(OPCODE_TRACING)
#define TRACE_OPCODE(opcode) dataLogF("   op %s\n", #opcode)
#else
#define TRACE_OPCODE(opcode)
#endif

// To keep compilers happy in case of unused labels, force usage of the label:
#define USE_LABEL(label) \
    do { \
        if (false) \
            goto label; \
    } while (false)

#define OFFLINE_ASM_OPCODE_LABEL(opcode) DEFINE_OPCODE(opcode) USE_LABEL(opcode); TRACE_OPCODE(opcode);

#define OFFLINE_ASM_GLOBAL_LABEL(label)  label: USE_LABEL(label);

#if ENABLE(LABEL_TRACING)
#define TRACE_LABEL(prefix, label) dataLog(#prefix, ": ", #label, "\n")
#else
#define TRACE_LABEL(prefix, label) do { } while (false);
#endif


#if ENABLE(COMPUTED_GOTO_OPCODES)
#define OFFLINE_ASM_GLUE_LABEL(label) label: TRACE_LABEL("OFFLINE_ASM_GLUE_LABEL", label); USE_LABEL(label);
#else
#define OFFLINE_ASM_GLUE_LABEL(label)  case label: label: USE_LABEL(label);
#endif

#define OFFLINE_ASM_LOCAL_LABEL(label) label: TRACE_LABEL("OFFLINE_ASM_LOCAL_LABEL", #label); USE_LABEL(label);

namespace JSC {

//============================================================================
// CLoopRegister is the storage for an emulated CPU register.
// It defines the policy of how ints smaller than intptr_t are packed into the
// pseudo register, as well as hides endianness differences.

class CLoopRegister {
public:
    ALWAYS_INLINE intptr_t i() const { return m_value; };
    ALWAYS_INLINE uintptr_t u() const { return m_value; }
    ALWAYS_INLINE int32_t i32() const { return m_value; }
    ALWAYS_INLINE uint32_t u32() const { return m_value; }
    ALWAYS_INLINE int8_t i8() const { return m_value; }
    ALWAYS_INLINE uint8_t u8() const { return m_value; }

    ALWAYS_INLINE intptr_t* ip() const { return bitwise_cast<intptr_t*>(m_value); }
    ALWAYS_INLINE int8_t* i8p() const { return bitwise_cast<int8_t*>(m_value); }
    ALWAYS_INLINE void* vp() const { return bitwise_cast<void*>(m_value); }
    ALWAYS_INLINE const void* cvp() const { return bitwise_cast<const void*>(m_value); }
    ALWAYS_INLINE CallFrame* callFrame() const { return bitwise_cast<CallFrame*>(m_value); }
    ALWAYS_INLINE ExecState* execState() const { return bitwise_cast<ExecState*>(m_value); }
    ALWAYS_INLINE const void* instruction() const { return bitwise_cast<const void*>(m_value); }
    ALWAYS_INLINE VM* vm() const { return bitwise_cast<VM*>(m_value); }
    ALWAYS_INLINE JSCell* cell() const { return bitwise_cast<JSCell*>(m_value); }
    ALWAYS_INLINE ProtoCallFrame* protoCallFrame() const { return bitwise_cast<ProtoCallFrame*>(m_value); }
    ALWAYS_INLINE NativeFunction nativeFunc() const { return bitwise_cast<NativeFunction>(m_value); }
#if USE(JSVALUE64)
    ALWAYS_INLINE int64_t i64() const { return m_value; }
    ALWAYS_INLINE uint64_t u64() const { return m_value; }
    ALWAYS_INLINE EncodedJSValue encodedJSValue() const { return bitwise_cast<EncodedJSValue>(m_value); }
#endif
    ALWAYS_INLINE Opcode opcode() const { return bitwise_cast<Opcode>(m_value); }

    operator ExecState*() { return bitwise_cast<ExecState*>(m_value); }
    operator const Instruction*() { return bitwise_cast<const Instruction*>(m_value); }
    operator JSCell*() { return bitwise_cast<JSCell*>(m_value); }
    operator ProtoCallFrame*() { return bitwise_cast<ProtoCallFrame*>(m_value); }
    operator Register*() { return bitwise_cast<Register*>(m_value); }
    operator VM*() { return bitwise_cast<VM*>(m_value); }

    template<typename T, typename = std::enable_if_t<sizeof(T) == sizeof(uintptr_t)>>
    ALWAYS_INLINE void operator=(T value) { m_value = bitwise_cast<uintptr_t>(value); }
#if USE(JSVALUE64)
    ALWAYS_INLINE void operator=(int32_t value) { m_value = static_cast<intptr_t>(value); }
    ALWAYS_INLINE void operator=(uint32_t value) { m_value = static_cast<uintptr_t>(value); }
#endif
    ALWAYS_INLINE void operator=(int16_t value) { m_value = static_cast<intptr_t>(value); }
    ALWAYS_INLINE void operator=(uint16_t value) { m_value = static_cast<uintptr_t>(value); }
    ALWAYS_INLINE void operator=(int8_t value) { m_value = static_cast<intptr_t>(value); }
    ALWAYS_INLINE void operator=(uint8_t value) { m_value = static_cast<uintptr_t>(value); }
    ALWAYS_INLINE void operator=(bool value) { m_value = static_cast<uintptr_t>(value); }

#if USE(JSVALUE64)
    ALWAYS_INLINE double bitsAsDouble() const { return bitwise_cast<double>(m_value); }
    ALWAYS_INLINE int64_t bitsAsInt64() const { return bitwise_cast<int64_t>(m_value); }
#endif

private:
    uintptr_t m_value { static_cast<uintptr_t>(0xbadbeef0baddbeef) };
};

class CLoopDoubleRegister {
public:
    template<typename T>
    explicit operator T() const { return bitwise_cast<T>(m_value); }

    ALWAYS_INLINE double d() const { return m_value; }
    ALWAYS_INLINE int64_t bitsAsInt64() const { return bitwise_cast<int64_t>(m_value); }

    ALWAYS_INLINE void operator=(double value) { m_value = value; }

    template<typename T, typename = std::enable_if_t<sizeof(T) == sizeof(uintptr_t) && std::is_integral<T>::value>>
    ALWAYS_INLINE void operator=(T value) { m_value = bitwise_cast<double>(value); }

private:
    double m_value;
};

//============================================================================
// Some utilities:
//

namespace LLInt {

#if USE(JSVALUE32_64)
static double ints2Double(uint32_t lo, uint32_t hi)
{
    uint64_t value = (static_cast<uint64_t>(hi) << 32) | lo;
    return bitwise_cast<double>(value);
}

static void double2Ints(double val, CLoopRegister& lo, CLoopRegister& hi)
{
    uint64_t value = bitwise_cast<uint64_t>(val);
    hi = static_cast<uint32_t>(value >> 32);
    lo = static_cast<uint32_t>(value);
}
#endif // USE(JSVALUE32_64)

static void decodeResult(SlowPathReturnType result, CLoopRegister& t0, CLoopRegister& t1)
{
    const void* t0Result;
    const void* t1Result;
    JSC::decodeResult(result, t0Result, t1Result);
    t0 = t0Result;
    t1 = t1Result;
}

} // namespace LLint

//============================================================================
// The llint C++ interpreter loop:
//

JSValue CLoop::execute(OpcodeID entryOpcodeID, void* executableAddress, VM* vm, ProtoCallFrame* protoCallFrame, bool isInitializationPass)
{
#define CAST bitwise_cast

    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function. The
    // caller (or one of its ancestors) is responsible for ensuring that this
    // is only called once during the initialization of the VM before threads
    // are at play.
    if (UNLIKELY(isInitializationPass)) {
        Opcode* opcodeMap = LLInt::opcodeMap();
        Opcode* opcodeMapWide = LLInt::opcodeMapWide();

#if ENABLE(COMPUTED_GOTO_OPCODES)
        #define OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = bitwise_cast<void*>(&&__opcode); \
            opcodeMapWide[__opcode] = bitwise_cast<void*>(&&__opcode##_wide);

        #define LLINT_OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = bitwise_cast<void*>(&&__opcode);
#else
        // FIXME: this mapping is unnecessarily expensive in the absence of COMPUTED_GOTO
        //   narrow opcodes don't need any mapping and wide opcodes just need to add numOpcodeIDs
        #define OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = __opcode; \
            opcodeMapWide[__opcode] = static_cast<OpcodeID>(__opcode##_wide);

        #define LLINT_OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = __opcode;
#endif
        FOR_EACH_BYTECODE_ID(OPCODE_ENTRY)
        FOR_EACH_CLOOP_BYTECODE_HELPER_ID(LLINT_OPCODE_ENTRY)
        FOR_EACH_LLINT_NATIVE_HELPER(LLINT_OPCODE_ENTRY)
        #undef OPCODE_ENTRY
        #undef LLINT_OPCODE_ENTRY

        // Note: we can only set the exceptionInstructions after we have
        // initialized the opcodeMap above. This is because getCodePtr()
        // can depend on the opcodeMap.
        uint8_t* exceptionInstructions = reinterpret_cast<uint8_t*>(LLInt::exceptionInstructions());
        for (int i = 0; i < maxOpcodeLength + 1; ++i)
            exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;

        return JSValue();
    }

    // Define the pseudo registers used by the LLINT C Loop backend:
    ASSERT(sizeof(CLoopRegister) == sizeof(intptr_t));

    // The CLoop llint backend is initially based on the ARMv7 backend, and
    // then further enhanced with a few instructions from the x86 backend to
    // support building for X64 targets. Hence, the shape of the generated
    // code and the usage convention of registers will look a lot like the
    // ARMv7 backend's.
    //
    // For example, on a 32-bit build:
    // 1. Outgoing args will be set up as follows:
    //    arg1 in t0 (r0 on ARM)
    //    arg2 in t1 (r1 on ARM)
    // 2. 32 bit return values will be in t0 (r0 on ARM).
    // 3. 64 bit return values (e.g. doubles) will be in t0,t1 (r0,r1 on ARM).
    //
    // But instead of naming these simulator registers based on their ARM
    // counterparts, we'll name them based on their original llint asm names.
    // This will make it easier to correlate the generated code with the
    // original llint asm code.
    //
    // On a 64-bit build, it more like x64 in that the registers are 64 bit.
    // Hence:
    // 1. Outgoing args are still the same: arg1 in t0, arg2 in t1, etc.
    // 2. 32 bit result values will be in the low 32-bit of t0.
    // 3. 64 bit result values will be in t0.

    CLoopRegister t0, t1, t2, t3, t5, sp, cfr, lr, pc;
#if USE(JSVALUE64)
    CLoopRegister pcBase, tagTypeNumber, tagMask;
#endif
    CLoopRegister metadataTable;
    CLoopDoubleRegister d0, d1;

    struct StackPointerScope {
        StackPointerScope(CLoopStack& stack)
            : m_stack(stack)
            , m_originalStackPointer(stack.currentStackPointer())
        { }

        ~StackPointerScope()
        {
            m_stack.setCurrentStackPointer(m_originalStackPointer);
        }

    private:
        CLoopStack& m_stack;
        void* m_originalStackPointer;
    };

    CLoopStack& cloopStack = vm->interpreter->cloopStack();
    StackPointerScope stackPointerScope(cloopStack);

    lr = getOpcode(llint_return_to_host);
    sp = cloopStack.currentStackPointer();
    cfr = vm->topCallFrame;
#ifndef NDEBUG
    void* startSP = sp.vp();
    CallFrame* startCFR = cfr.callFrame();
#endif

    // Initialize the incoming args for doVMEntryToJavaScript:
    t0 = executableAddress;
    t1 = vm;
    t2 = protoCallFrame;

#if USE(JSVALUE64)
    // For the ASM llint, JITStubs takes care of this initialization. We do
    // it explicitly here for the C loop:
    tagTypeNumber = 0xFFFF000000000000;
    tagMask = 0xFFFF000000000002;
#endif // USE(JSVALUE64)

    // Interpreter variables for value passing between opcodes and/or helpers:
    NativeFunction nativeFunc = nullptr;
    JSValue functionReturnValue;
    Opcode opcode = getOpcode(entryOpcodeID);

#define PUSH(cloopReg) \
    do { \
        sp = sp.ip() - 1; \
        *sp.ip() = cloopReg.i(); \
    } while (false)

#define POP(cloopReg) \
    do { \
        cloopReg = *sp.ip(); \
        sp = sp.ip() + 1; \
    } while (false)

#if ENABLE(OPCODE_STATS)
#define RECORD_OPCODE_STATS(__opcode) OpcodeStats::recordInstruction(__opcode)
#else
#define RECORD_OPCODE_STATS(__opcode)
#endif

#if USE(JSVALUE32_64)
#define FETCH_OPCODE() *pc.i8p
#else // USE(JSVALUE64)
#define FETCH_OPCODE() *bitwise_cast<OpcodeID*>(pcBase.i8p + pc.i)
#endif // USE(JSVALUE64)

#define NEXT_INSTRUCTION() \
    do {                         \
        opcode = FETCH_OPCODE(); \
        DISPATCH_OPCODE();       \
    } while (false)

#if ENABLE(COMPUTED_GOTO_OPCODES)

    //========================================================================
    // Loop dispatch mechanism using computed goto statements:

    #define DISPATCH_OPCODE() goto *opcode

    #define DEFINE_OPCODE(__opcode) \
        __opcode: \
            RECORD_OPCODE_STATS(__opcode);

    // Dispatch to the current PC's bytecode:
    DISPATCH_OPCODE();

#else // !ENABLE(COMPUTED_GOTO_OPCODES)
    //========================================================================
    // Loop dispatch mechanism using a C switch statement:

    #define DISPATCH_OPCODE() goto dispatchOpcode

    #define DEFINE_OPCODE(__opcode) \
        case __opcode: \
        __opcode: \
            RECORD_OPCODE_STATS(__opcode);

    // Dispatch to the current PC's bytecode:
    dispatchOpcode:
    switch (static_cast<unsigned>(opcode))

#endif // !ENABLE(COMPUTED_GOTO_OPCODES)

    //========================================================================
    // Bytecode handlers:
    {
        // This is the file generated by offlineasm, which contains all of the
        // bytecode handlers for the interpreter, as compiled from
        // LowLevelInterpreter.asm and its peers.

        IGNORE_CLANG_WARNINGS_BEGIN("unreachable-code")
        #include "LLIntAssembly.h"
        IGNORE_CLANG_WARNINGS_END

        OFFLINE_ASM_GLUE_LABEL(llint_return_to_host)
        {
            ASSERT(startSP == sp.vp());
            ASSERT(startCFR == cfr.callFrame());
#if USE(JSVALUE32_64)
            return JSValue(t1.i(), t0.i()); // returning JSValue(tag, payload);
#else
            return JSValue::decode(t0.encodedJSValue());
#endif
        }

        // In the ASM llint, getHostCallReturnValue() is a piece of glue
        // function provided by the JIT (see jit/JITOperations.cpp).
        // We simulate it here with a pseduo-opcode handler.
        OFFLINE_ASM_GLUE_LABEL(getHostCallReturnValue)
        {
            // The part in getHostCallReturnValueWithExecState():
            JSValue result = vm->hostCallReturnValue;
#if USE(JSVALUE32_64)
            t1 = result.tag();
            t0 = result.payload();
#else
            t0 = JSValue::encode(result);
#endif
            opcode = lr.opcode();
            DISPATCH_OPCODE();
        }

#if !ENABLE(COMPUTED_GOTO_OPCODES)
    default:
        ASSERT(false);
#endif

    } // END bytecode handler cases.

#if ENABLE(COMPUTED_GOTO_OPCODES)
    // Keep the compiler happy so that it doesn't complain about unused
    // labels for the LLInt trampoline glue. The labels are automatically
    // emitted by label macros above, and some of them are referenced by
    // the llint generated code. Since we can't tell ahead of time which
    // will be referenced and which will be not, we'll just passify the
    // compiler on all such labels:
    #define LLINT_OPCODE_ENTRY(__opcode, length) \
        UNUSED_LABEL(__opcode);
        FOR_EACH_OPCODE_ID(LLINT_OPCODE_ENTRY);
    #undef LLINT_OPCODE_ENTRY
#endif

    #undef NEXT_INSTRUCTION
    #undef DEFINE_OPCODE
    #undef CHECK_FOR_TIMEOUT
    #undef CAST

    return JSValue(); // to suppress a compiler warning.
} // Interpreter::llintCLoopExecute()

} // namespace JSC

#elif !COMPILER(MSVC)

//============================================================================
// Define the opcode dispatch mechanism when using an ASM loop:
//

// These are for building an interpreter from generated assembly code:
#define OFFLINE_ASM_BEGIN   asm (
#define OFFLINE_ASM_END     );

#if USE(LLINT_EMBEDDED_OPCODE_ID)
#define EMBED_OPCODE_ID_IF_NEEDED(__opcode) ".int " __opcode##_value_string "\n"
#else
#define EMBED_OPCODE_ID_IF_NEEDED(__opcode)
#endif

#define OFFLINE_ASM_OPCODE_LABEL(__opcode) \
    EMBED_OPCODE_ID_IF_NEEDED(__opcode) \
    OFFLINE_ASM_OPCODE_DEBUG_LABEL(llint_##__opcode) \
    OFFLINE_ASM_LOCAL_LABEL(llint_##__opcode)

#define OFFLINE_ASM_GLUE_LABEL(__opcode)   OFFLINE_ASM_LOCAL_LABEL(__opcode)

#if CPU(ARM_THUMB2)
#define OFFLINE_ASM_GLOBAL_LABEL(label)          \
    ".text\n"                                    \
    ".align 4\n"                                 \
    ".globl " SYMBOL_STRING(label) "\n"          \
    HIDE_SYMBOL(label) "\n"                      \
    ".thumb\n"                                   \
    ".thumb_func " THUMB_FUNC_PARAM(label) "\n"  \
    SYMBOL_STRING(label) ":\n"
#elif CPU(ARM64)
#define OFFLINE_ASM_GLOBAL_LABEL(label)         \
    ".text\n"                                   \
    ".align 4\n"                                \
    ".globl " SYMBOL_STRING(label) "\n"         \
    HIDE_SYMBOL(label) "\n"                     \
    SYMBOL_STRING(label) ":\n"
#else
#define OFFLINE_ASM_GLOBAL_LABEL(label)         \
    ".text\n"                                   \
    ".globl " SYMBOL_STRING(label) "\n"         \
    HIDE_SYMBOL(label) "\n"                     \
    SYMBOL_STRING(label) ":\n"
#endif

#define OFFLINE_ASM_LOCAL_LABEL(label)   LOCAL_LABEL_STRING(label) ":\n"

#if OS(LINUX)
#define OFFLINE_ASM_OPCODE_DEBUG_LABEL(label)  #label ":\n"
#else
#define OFFLINE_ASM_OPCODE_DEBUG_LABEL(label)
#endif

// This is a file generated by offlineasm, which contains all of the assembly code
// for the interpreter, as compiled from LowLevelInterpreter.asm.
#include "LLIntAssembly.h"

#endif // ENABLE(C_LOOP)
