/*
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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
#include "SuperSampler.h"
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

using namespace JSC::LLInt;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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
// (e.g. ctiOpThrowNotCaught) are also added as
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

using WebConfig::g_config;

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

class CallLinkInfo;

//============================================================================
// CLoopRegister is the storage for an emulated CPU register.
// It defines the policy of how ints smaller than intptr_t are packed into the
// pseudo register, as well as hides endianness differences.

// The emulated register is always 64 bits wide so it can hold an EncodedJSValue,
// even on 32-bit targets where pointers occupy only the low half.
class CLoopRegister {
public:
    ALWAYS_INLINE intptr_t i() const { return static_cast<intptr_t>(m_value); };
    ALWAYS_INLINE uintptr_t u() const { return static_cast<uintptr_t>(m_value); }
    ALWAYS_INLINE int32_t i32() const { return static_cast<int32_t>(m_value); }
    ALWAYS_INLINE uint32_t u32() const { return static_cast<uint32_t>(m_value); }
    ALWAYS_INLINE int16_t i16() const { return static_cast<int16_t>(m_value); }
    ALWAYS_INLINE uint16_t u16() const { return static_cast<uint16_t>(m_value); }
    ALWAYS_INLINE int8_t i8() const { return static_cast<int8_t>(m_value); }
    ALWAYS_INLINE uint8_t u8() const { return static_cast<uint8_t>(m_value); }

    ALWAYS_INLINE intptr_t* ip() const { return std::bit_cast<intptr_t*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE int8_t* i8p() const { return std::bit_cast<int8_t*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE void* vp() const { return std::bit_cast<void*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE const void* cvp() const { return std::bit_cast<const void*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE CallFrame* callFrame() const { return std::bit_cast<CallFrame*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE const void* instruction() const { return std::bit_cast<const void*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE VM* vm() const { return std::bit_cast<VM*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE JSCell* cell() const { return std::bit_cast<JSCell*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE ProtoCallFrame* protoCallFrame() const { return std::bit_cast<ProtoCallFrame*>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE NativeFunction nativeFunc() const { return std::bit_cast<NativeFunction>(static_cast<uintptr_t>(m_value)); }
    ALWAYS_INLINE int64_t i64() const { return static_cast<int64_t>(m_value); }
    ALWAYS_INLINE uint64_t u64() const { return m_value; }
    ALWAYS_INLINE EncodedJSValue encodedJSValue() const { return std::bit_cast<EncodedJSValue>(m_value); }
    ALWAYS_INLINE Opcode opcode() const { return std::bit_cast<Opcode>(static_cast<uintptr_t>(m_value)); }

    operator CallFrame*() { return std::bit_cast<CallFrame*>(static_cast<uintptr_t>(m_value)); }
    operator const JSInstruction*() { return std::bit_cast<const JSInstruction*>(static_cast<uintptr_t>(m_value)); }
    operator JSCell*() { return std::bit_cast<JSCell*>(static_cast<uintptr_t>(m_value)); }
    operator ProtoCallFrame*() { return std::bit_cast<ProtoCallFrame*>(static_cast<uintptr_t>(m_value)); }
    operator Register*() { return std::bit_cast<Register*>(static_cast<uintptr_t>(m_value)); }
    operator VM*() { return std::bit_cast<VM*>(static_cast<uintptr_t>(m_value)); }
    operator CallLinkInfo*() { return std::bit_cast<CallLinkInfo*>(static_cast<uintptr_t>(m_value)); }
    operator void*() { return std::bit_cast<void*>(static_cast<uintptr_t>(m_value)); }

    // Assignment must stay templated: intptr_t is a distinct type from every
    // int<N>_t on 32-bit Darwin, so a fixed-width overload set is ambiguous for it.
    template<typename T>
        requires (std::is_pointer_v<T>)
    ALWAYS_INLINE void operator=(T value) { m_value = std::bit_cast<uintptr_t>(value); }

    template<typename T>
        requires (std::is_integral_v<T> && std::is_signed_v<T>)
    ALWAYS_INLINE void operator=(T value) { m_value = static_cast<uint64_t>(static_cast<int64_t>(value)); }

    template<typename T>
        requires (std::is_integral_v<T> && !std::is_signed_v<T>)
    ALWAYS_INLINE void operator=(T value) { m_value = static_cast<uint64_t>(value); }

    ALWAYS_INLINE double bitsAsDouble() const { return std::bit_cast<double>(m_value); }
    ALWAYS_INLINE int64_t bitsAsInt64() const { return std::bit_cast<int64_t>(m_value); }

private:
    uint64_t m_value { 0xbadbeef0baddbeefull };
};

class CLoopDoubleRegister {
public:
    template<typename T>
    explicit operator T() const { return std::bit_cast<T>(m_value); }

    ALWAYS_INLINE double d() const { return m_value; }
    ALWAYS_INLINE int64_t bitsAsInt64() const { return std::bit_cast<int64_t>(m_value); }

    ALWAYS_INLINE void operator=(double value) { m_value = value; }

    template<typename T>
        requires (sizeof(T) == sizeof(uintptr_t) && std::integral<T>)
    ALWAYS_INLINE void operator=(T value) { m_value = std::bit_cast<double>(value); }

private:
    double m_value;
};

//============================================================================
// Some utilities:
//

namespace LLInt {

static void decodeResult(UGPRPair result, CLoopRegister& t0, CLoopRegister& t1)
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
    // WARNING: it is critical to avoid 'std::bit_cast' in the CAST macro that follows and in
    // the OPCODE_ENTRY() and LLINT_OPCODE_ENTRY() macros below. Using it dramatically increases the
    // frame size of this function in debug builds, potentially causing stack overflows in tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=295796 for details.
#define CAST reinterpret_cast

    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function. The
    // caller (or one of its ancestors) is responsible for ensuring that this
    // is only called once during the initialization of the VM before threads
    // are at play.
    if (isInitializationPass) [[unlikely]] {
        Opcode* opcodeMap = LLInt::opcodeMap();
        Opcode* opcodeMapWide16 = LLInt::opcodeMapWide16();
        Opcode* opcodeMapWide32 = LLInt::opcodeMapWide32();

#if ENABLE(COMPUTED_GOTO_OPCODES)
        #define OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = static_cast<void*>(&&__opcode); \
            opcodeMapWide16[__opcode] = static_cast<void*>(&&__opcode##_wide16); \
            opcodeMapWide32[__opcode] = static_cast<void*>(&&__opcode##_wide32);

        #define LLINT_OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = static_cast<void*>(&&__opcode);
#else
        // FIXME: this mapping is unnecessarily expensive in the absence of COMPUTED_GOTO
        //   narrow opcodes don't need any mapping and wide opcodes just need to add numOpcodeIDs
        #define OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = __opcode; \
            opcodeMapWide16[__opcode] = static_cast<OpcodeID>(__opcode##_wide16); \
            opcodeMapWide32[__opcode] = static_cast<OpcodeID>(__opcode##_wide32);

        #define LLINT_OPCODE_ENTRY(__opcode, length) \
            opcodeMap[__opcode] = __opcode;
#endif
        FOR_EACH_BYTECODE_ID(OPCODE_ENTRY)
        FOR_EACH_BYTECODE_HELPER_ID(OPCODE_ENTRY)
        FOR_EACH_CLOOP_BYTECODE_HELPER_ID(LLINT_OPCODE_ENTRY)
        FOR_EACH_LLINT_NATIVE_HELPER(LLINT_OPCODE_ENTRY)
        FOR_EACH_CLOOP_RETURN_HELPER_ID(LLINT_OPCODE_ENTRY)
        #undef OPCODE_ENTRY
        #undef LLINT_OPCODE_ENTRY

        // Note: we can only set the exceptionInstructions after we have
        // initialized the opcodeMap above. This is because getCodePtr()
        // can depend on the opcodeMap.
        uint8_t* exceptionInstructions = reinterpret_cast<uint8_t*>(LLInt::exceptionInstructions());
        for (unsigned i = 0; i < maxBytecodeStructLength + 1; ++i)
            exceptionInstructions[i] = llint_throw_from_slow_path_trampoline;

        return JSValue();
    }

    // Define the pseudo registers used by the LLINT C Loop backend. Each is 64 bits
    // wide so it can hold an EncodedJSValue.
    static_assert(sizeof(CLoopRegister) == sizeof(EncodedJSValue));

    // The registers are named after their original llint asm names to make it easier
    // to correlate the generated code with the llint asm. The calling convention:
    // 1. Outgoing args are arg1 in t0, arg2 in t1, etc.
    // 2. 32 bit result values are in the low 32-bit of t0.
    // 3. 64 bit result values are in t0.

    CLoopRegister t0, t1, t2, t3, t5, t6, t7, sp, cfr, lr, pc;
    CLoopRegister numberTag, notCellMask;
    CLoopRegister pcBase;
    CLoopRegister metadataTable;
    CLoopDoubleRegister d0, d1;

    UNUSED_VARIABLE(t0);
    UNUSED_VARIABLE(t1);
    UNUSED_VARIABLE(t2);
    UNUSED_VARIABLE(t3);
    UNUSED_VARIABLE(t5);
    UNUSED_VARIABLE(t6);
    UNUSED_VARIABLE(t7);

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

    CLoopStack& cloopStack = vm->cloopStack();
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

    // For the ASM llint, JITStubs takes care of this initialization. We do
    // it explicitly here for the C loop:
    numberTag = JSValue::NumberTag;
    notCellMask = JSValue::NotCellMask;

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
            return JSValue::decode(t0.encodedJSValue());
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

    #undef DEFINE_OPCODE
    #undef CHECK_FOR_TIMEOUT
    #undef CAST

    return JSValue(); // to suppress a compiler warning.
} // Interpreter::llintCLoopExecute()

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#else // !ENABLE(C_LOOP)

//============================================================================
// Define the opcode dispatch mechanism when using an ASM loop:
//

// We need an OFFLINE_ASM_BEGIN_SPACER because we'll be declaring every OFFLINE_ASM_GLOBAL_LABEL
// as an alt entry. However, Clang will error out if the first global label is also an alt entry.
// To work around this, we'll make OFFLINE_ASM_BEGIN emit an unused global label (which will now
// be the first) that is not an alt entry, and insert a spacer instruction between it and the
// actual first global label emitted by the offlineasm. Clang also requires that these 2 labels
// not point to the same spot in memory; hence, the need for the spacer.
//
// For the spacer instruction, we'll choose a breakpoint instruction. However, we can
// also just emit an unused piece of data. A breakpoint instruction is preferable.

#if CPU(ARM64)
#define OFFLINE_ASM_BEGIN_SPACER "brk #" STRINGIZE_VALUE_OF(WTF_FATAL_CRASH_CODE) "\n"
#elif CPU(X86_64)
#define OFFLINE_ASM_BEGIN_SPACER "int3\n"
#else
#define OFFLINE_ASM_BEGIN_SPACER ".int 0xbadbeef0\n"
#endif

// These are for building an interpreter from generated assembly code:
// the jsc_llint_begin and jsc_llint_end labels help lldb_webkit.py find the
// start and end of the llint instruction range quickly.

#define OFFLINE_ASM_BEGIN   __asm__( \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(jsc_llint_begin, OFFLINE_ASM_NO_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_ALIGN4B, HIDE_SYMBOL) \
    OFFLINE_ASM_BEGIN_SPACER

#define OFFLINE_ASM_END \
    OFFLINE_ASM_BEGIN_SPACER \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(jsc_llint_end, OFFLINE_ASM_NO_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_ALIGN4B, HIDE_SYMBOL) \
);

#if ENABLE(LLINT_EMBEDDED_OPCODE_ID)
#define EMBED_OPCODE_ID_IF_NEEDED(__opcode) ".int " __opcode##_value_string "\n"
#else
#define EMBED_OPCODE_ID_IF_NEEDED(__opcode)
#endif

#define OFFLINE_ASM_OPCODE_LABEL(__opcode) \
    EMBED_OPCODE_ID_IF_NEEDED(__opcode) \
    OFFLINE_ASM_OPCODE_DEBUG_LABEL(llint_##__opcode) \
    OFFLINE_ASM_LOCAL_LABEL(llint_##__opcode)

#define OFFLINE_ASM_GLUE_LABEL(__opcode) \
    OFFLINE_ASM_OPCODE_DEBUG_LABEL(__opcode) \
    OFFLINE_ASM_LOCAL_LABEL(__opcode)

#define OFFLINE_ASM_NO_ALT_ENTRY_DIRECTIVE(label)

#if COMPILER(CLANG) && OS(DARWIN) && ENABLE(OFFLINE_ASM_ALT_ENTRY)
#define OFFLINE_ASM_ALT_ENTRY_DIRECTIVE(label) \
    ".alt_entry " SYMBOL_STRING(label) "\n"
#else
#define OFFLINE_ASM_ALT_ENTRY_DIRECTIVE(label)
#endif

#if OS(DARWIN)
#define OFFLINE_ASM_TEXT_SECTION ".section __TEXT,__jsc_int,regular,pure_instructions\n"
#else
#define OFFLINE_ASM_TEXT_SECTION ".text\n"
#endif

#if CPU(RISCV64)
#define OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, ALT_ENTRY, ALIGNMENT, VISIBILITY) \
    OFFLINE_ASM_TEXT_SECTION                    \
    ALIGNMENT                                   \
    ALT_ENTRY(label)                            \
    ".globl " SYMBOL_STRING(label) "\n"         \
    ".attribute arch, \"rv64gc\"" "\n"          \
    VISIBILITY(label) "\n"                      \
    SYMBOL_STRING(label) ":\n"
#else
#define OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, ALT_ENTRY, ALIGNMENT, VISIBILITY) \
    OFFLINE_ASM_TEXT_SECTION                    \
    ALIGNMENT                                   \
    ALT_ENTRY(label)                            \
    ".globl " SYMBOL_STRING(label) "\n"         \
    VISIBILITY(label) "\n"                      \
    SYMBOL_STRING(label) ":\n"
#endif

#if OS(WINDOWS) && CPU(ARM64)
#define OFFLINE_ASM_ALIGN4B ".align 2\n"
#else
#define OFFLINE_ASM_ALIGN4B ".balign 4\n"
#endif
#define OFFLINE_ASM_NOALIGN ""

#if CPU(ARM64) && OS(WINDOWS)
// COFF uses power-of-two alignment: .align N means 2^N bytes
#define OFFLINE_ASM_ALIGN_TRAP_256 "\n .align 8\n"
#define OFFLINE_ASM_ALIGN_TRAP_64 "\n .align 6\n"
#define OFFLINE_ASM_ALIGN_TRAP(align) OFFLINE_ASM_ALIGN_TRAP_##align
#elif CPU(ARM64) || CPU(ARM64E)
#define OFFLINE_ASM_ALIGN_TRAP(align) OFFLINE_ASM_BEGIN_SPACER "\n .balignl " #align ", 0xd4388e20\n" // pad with brk instructions
#elif CPU(X86_64)
#define OFFLINE_ASM_ALIGN_TRAP(align) OFFLINE_ASM_BEGIN_SPACER "\n .balign " #align ", 0xcc\n" // pad with int 3 instructions
#elif CPU(RISCV64)
#define OFFLINE_ASM_ALIGN_TRAP(align) OFFLINE_ASM_BEGIN_SPACER "\n .balignw " #align ", 0x9002\n" // pad with c.ebreak instructions
#endif

#define OFFLINE_ASM_EXPORT_SYMBOL(symbol)

#define OFFLINE_ASM_GLOBAL_LABEL(label) \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, OFFLINE_ASM_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_ALIGN4B, HIDE_SYMBOL)
#define OFFLINE_ASM_UNALIGNED_GLOBAL_LABEL(label) \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, OFFLINE_ASM_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_NOALIGN, HIDE_SYMBOL)
#define OFFLINE_ASM_ALIGNED_GLOBAL_LABEL(label, align) \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, OFFLINE_ASM_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_ALIGN_TRAP(align), HIDE_SYMBOL)
#define OFFLINE_ASM_GLOBAL_EXPORT_LABEL(label) \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, OFFLINE_ASM_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_ALIGN4B, OFFLINE_ASM_EXPORT_SYMBOL)
#define OFFLINE_ASM_UNALIGNED_GLOBAL_EXPORT_LABEL(label) \
    OFFLINE_ASM_GLOBAL_LABEL_IMPL(label, OFFLINE_ASM_ALT_ENTRY_DIRECTIVE, OFFLINE_ASM_NOALIGN, OFFLINE_ASM_EXPORT_SYMBOL)

#if COMPILER(CLANG) && ENABLE(OFFLINE_ASM_ALT_ENTRY)
#define OFFLINE_ASM_ALT_GLOBAL_LABEL(label) OFFLINE_ASM_GLOBAL_LABEL(label)
#else
#define OFFLINE_ASM_ALT_GLOBAL_LABEL(label)
#endif

#define OFFLINE_ASM_LOCAL_LABEL(label) \
    LOCAL_LABEL_STRING(label) ":\n" \
    OFFLINE_ASM_ALT_GLOBAL_LABEL(label)

#if OS(LINUX)
#define OFFLINE_ASM_OPCODE_DEBUG_LABEL(label)  #label ":\n"
#else
#define OFFLINE_ASM_OPCODE_DEBUG_LABEL(label)
#endif

#include "WasmCallee.h"

// This works around a bug in GDB where, if the compilation unit
// doesn't have any address range information, its line table won't
// even be consulted. Emit {before,after}_llint_asm so that the code
// emitted in the top level inline asm statement is within functions
// visible to the compiler. This way, GDB can resolve a PC in the
// llint asm code to this compilation unit and the successfully look
// up the line number information.
DEBUGGER_ANNOTATION_MARKER(before_llint_asm)


// We do not set them on Darwin since Mach-O does not support nested cfi_startproc & global symbols.
// https://github.com/llvm/llvm-project/issues/72802
// Similarly, GCC complains about implicit endproc instructions.
//
// This may seem strange; We duplicate these table entries because
// different lldb versions seem to sometimes have off-by-one errors otherwise.
// See GdbJIT.cpp for a detailed explanation of how these DWARF directives work.
#if !OS(DARWIN) && COMPILER(CLANG)
#if CPU(ARM64)
__asm__(
    ".cfi_startproc\n"
    ".cfi_def_cfa fp, 16\n"
    ".cfi_offset lr, -8\n"
    ".cfi_offset fp, -16\n"
    OFFLINE_ASM_BEGIN_SPACER
    ".cfi_def_cfa fp, 0\n"
    ".cfi_offset lr, 0\n"
    ".cfi_offset fp, 0\n"
    OFFLINE_ASM_BEGIN_SPACER
    ".cfi_def_cfa fp, 16\n"
    ".cfi_offset lr, -8\n"
    ".cfi_offset fp, -16\n"
    OFFLINE_ASM_BEGIN_SPACER
);
#endif
#endif

// This is a file generated by offlineasm, which contains all of the assembly code
// for the interpreter, as compiled from LowLevelInterpreter.asm.
#include "LLIntAssembly.h"

// See GdbJIT.cpp for a detailed explanation.
#if !OS(DARWIN) && COMPILER(CLANG)
#if CPU(ARM64)
__asm__(
    ".cfi_endproc\n"
);
#endif
#endif

DEBUGGER_ANNOTATION_MARKER(after_llint_asm)

#endif // ENABLE(C_LOOP)
