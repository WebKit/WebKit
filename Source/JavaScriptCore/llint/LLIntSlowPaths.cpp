/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "LLIntSlowPaths.h"

#if ENABLE(LLINT)

#include "Arguments.h"
#include "CallFrame.h"
#include "CommonSlowPaths.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITDriver.h"
#include "JSActivation.h"
#include "JSGlobalObjectFunctions.h"
#include "JSPropertyNameIterator.h"
#include "JSStaticScopeObject.h"
#include "JSString.h"
#include "JSValue.h"
#include "LLIntCommon.h"
#include "LLIntExceptions.h"
#include "LowLevelInterpreter.h"
#include "Operations.h"

namespace JSC { namespace LLInt {

#define LLINT_BEGIN_NO_SET_PC() \
    JSGlobalData& globalData = exec->globalData();      \
    NativeCallFrameTracer tracer(&globalData, exec)

#ifndef NDEBUG
#define LLINT_SET_PC_FOR_STUBS() do { \
        exec->codeBlock()->bytecodeOffset(pc); \
        exec->setCurrentVPC(pc + 1); \
    } while (false)
#else
#define LLINT_SET_PC_FOR_STUBS() do { \
        exec->setCurrentVPC(pc + 1); \
    } while (false)
#endif

#define LLINT_BEGIN()                           \
    LLINT_BEGIN_NO_SET_PC();                    \
    LLINT_SET_PC_FOR_STUBS()

#define LLINT_OP(index) (exec->uncheckedR(pc[index].u.operand))
#define LLINT_OP_C(index) (exec->r(pc[index].u.operand))

#define LLINT_RETURN_TWO(first, second) do {       \
        return encodeResult(first, second);        \
    } while (false)

#define LLINT_END_IMPL() LLINT_RETURN_TWO(pc, exec)

#define LLINT_THROW(exceptionToThrow) do {                        \
        globalData.exception = (exceptionToThrow);                \
        pc = returnToThrow(exec, pc);                             \
        LLINT_END_IMPL();                                         \
    } while (false)

#define LLINT_CHECK_EXCEPTION() do {                    \
        if (UNLIKELY(globalData.exception)) {           \
            pc = returnToThrow(exec, pc);               \
            LLINT_END_IMPL();                           \
        }                                               \
    } while (false)

#define LLINT_END() do {                        \
        LLINT_CHECK_EXCEPTION();                \
        LLINT_END_IMPL();                       \
    } while (false)

#define LLINT_BRANCH(opcode, condition) do {                      \
        bool __b_condition = (condition);                         \
        LLINT_CHECK_EXCEPTION();                                  \
        if (__b_condition)                                        \
            pc += pc[OPCODE_LENGTH(opcode) - 1].u.operand;        \
        else                                                      \
            pc += OPCODE_LENGTH(opcode);                          \
        LLINT_END_IMPL();                                         \
    } while (false)

#define LLINT_RETURN(value) do {                \
        JSValue __r_returnValue = (value);      \
        LLINT_CHECK_EXCEPTION();                \
        LLINT_OP(1) = __r_returnValue;          \
        LLINT_END_IMPL();                       \
    } while (false)

#if ENABLE(VALUE_PROFILER)
#define LLINT_RETURN_PROFILED(opcode, value) do {               \
        JSValue __rp_returnValue = (value);                     \
        LLINT_CHECK_EXCEPTION();                                \
        LLINT_OP(1) = __rp_returnValue;                         \
        pc[OPCODE_LENGTH(opcode) - 1].u.profile->m_buckets[0] = \
            JSValue::encode(__rp_returnValue);                  \
        LLINT_END_IMPL();                                       \
    } while (false)
#else // ENABLE(VALUE_PROFILER)
#define LLINT_RETURN_PROFILED(opcode, value) LLINT_RETURN(value)
#endif // ENABLE(VALUE_PROFILER)

#define LLINT_CALL_END_IMPL(exec, callTarget) LLINT_RETURN_TWO((callTarget), (exec))

#define LLINT_CALL_THROW(exec, pc, exceptionToThrow) do {               \
        ExecState* __ct_exec = (exec);                                  \
        Instruction* __ct_pc = (pc);                                    \
        globalData.exception = (exceptionToThrow);                      \
        LLINT_CALL_END_IMPL(__ct_exec, callToThrow(__ct_exec, __ct_pc)); \
    } while (false)

#define LLINT_CALL_CHECK_EXCEPTION(exec, pc) do {                       \
        ExecState* __cce_exec = (exec);                                 \
        Instruction* __cce_pc = (pc);                                   \
        if (UNLIKELY(globalData.exception))                              \
            LLINT_CALL_END_IMPL(__cce_exec, callToThrow(__cce_exec, __cce_pc)); \
    } while (false)

#define LLINT_CALL_RETURN(exec, pc, callTarget) do {                    \
        ExecState* __cr_exec = (exec);                                  \
        Instruction* __cr_pc = (pc);                                    \
        void* __cr_callTarget = (callTarget);                           \
        LLINT_CALL_CHECK_EXCEPTION(__cr_exec->callerFrame(), __cr_pc);  \
        LLINT_CALL_END_IMPL(__cr_exec, __cr_callTarget);                \
    } while (false)

extern "C" SlowPathReturnType llint_trace_operand(ExecState* exec, Instruction* pc, int fromWhere, int operand)
{
    LLINT_BEGIN();
    dataLog("%p / %p: executing bc#%zu, op#%u: Trace(%d): %d: %d\n",
            exec->codeBlock(),
            exec,
            static_cast<intptr_t>(pc - exec->codeBlock()->instructions().begin()),
            exec->globalData().interpreter->getOpcodeID(pc[0].u.opcode),
            fromWhere,
            operand,
            pc[operand].u.operand);
    LLINT_END();
}

extern "C" SlowPathReturnType llint_trace_value(ExecState* exec, Instruction* pc, int fromWhere, int operand)
{
    JSValue value = LLINT_OP_C(operand).jsValue();
    union {
        struct {
            uint32_t tag;
            uint32_t payload;
        } bits;
        EncodedJSValue asValue;
    } u;
    u.asValue = JSValue::encode(value);
    dataLog("%p / %p: executing bc#%zu, op#%u: Trace(%d): %d: %d: %08x:%08x: %s\n",
            exec->codeBlock(),
            exec,
            static_cast<intptr_t>(pc - exec->codeBlock()->instructions().begin()),
            exec->globalData().interpreter->getOpcodeID(pc[0].u.opcode),
            fromWhere,
            operand,
            pc[operand].u.operand,
            u.bits.tag,
            u.bits.payload,
            value.description());
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_prologue)
{
    dataLog("%p / %p: in prologue.\n", exec->codeBlock(), exec);
    LLINT_END_IMPL();
}

static void traceFunctionPrologue(ExecState* exec, const char* comment, CodeSpecializationKind kind)
{
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    FunctionExecutable* executable = callee->jsExecutable();
    CodeBlock* codeBlock = &executable->generatedBytecodeFor(kind);
    dataLog("%p / %p: in %s of function %p, executable %p; numVars = %u, numParameters = %u, numCalleeRegisters = %u, caller = %p.\n",
            codeBlock, exec, comment, callee, executable,
            codeBlock->m_numVars, codeBlock->numParameters(), codeBlock->m_numCalleeRegisters,
            exec->callerFrame());
}

LLINT_SLOW_PATH_DECL(trace_prologue_function_for_call)
{
    traceFunctionPrologue(exec, "call prologue", CodeForCall);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_prologue_function_for_construct)
{
    traceFunctionPrologue(exec, "construct prologue", CodeForConstruct);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_arityCheck_for_call)
{
    traceFunctionPrologue(exec, "call arity check", CodeForCall);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_arityCheck_for_construct)
{
    traceFunctionPrologue(exec, "construct arity check", CodeForConstruct);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace)
{
    dataLog("%p / %p: executing bc#%zu, %s, scope %p\n",
            exec->codeBlock(),
            exec,
            static_cast<intptr_t>(pc - exec->codeBlock()->instructions().begin()),
            opcodeNames[exec->globalData().interpreter->getOpcodeID(pc[0].u.opcode)],
            exec->scopeChain());
    if (exec->globalData().interpreter->getOpcodeID(pc[0].u.opcode) == op_ret) {
        dataLog("Will be returning to %p\n", exec->returnPC().value());
        dataLog("The new cfr will be %p\n", exec->callerFrame());
    }
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(special_trace)
{
    dataLog("%p / %p: executing special case bc#%zu, op#%u, return PC is %p\n",
            exec->codeBlock(),
            exec,
            static_cast<intptr_t>(pc - exec->codeBlock()->instructions().begin()),
            exec->globalData().interpreter->getOpcodeID(pc[0].u.opcode),
            exec->returnPC().value());
    LLINT_END_IMPL();
}

inline bool shouldJIT(ExecState* exec)
{
    // You can modify this to turn off JITting without rebuilding the world.
    return exec->globalData().canUseJIT();
}

// Returns true if we should try to OSR.
inline bool jitCompileAndSetHeuristics(CodeBlock* codeBlock, ExecState* exec)
{
    if (!codeBlock->checkIfJITThresholdReached()) {
#if ENABLE(JIT_VERBOSE_OSR)
        dataLog("    JIT threshold should be lifted.\n");
#endif
        return false;
    }
        
    CodeBlock::JITCompilationResult result = codeBlock->jitCompile(exec->globalData());
    switch (result) {
    case CodeBlock::AlreadyCompiled:
#if ENABLE(JIT_VERBOSE_OSR)
        dataLog("    Code was already compiled.\n");
#endif
        codeBlock->jitSoon();
        return true;
    case CodeBlock::CouldNotCompile:
#if ENABLE(JIT_VERBOSE_OSR)
        dataLog("    JIT compilation failed.\n");
#endif
        codeBlock->dontJITAnytimeSoon();
        return false;
    case CodeBlock::CompiledSuccessfully:
#if ENABLE(JIT_VERBOSE_OSR)
        dataLog("    JIT compilation successful.\n");
#endif
        codeBlock->jitSoon();
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

enum EntryKind { Prologue, ArityCheck };
static SlowPathReturnType entryOSR(ExecState* exec, Instruction* pc, CodeBlock* codeBlock, const char *name, EntryKind kind)
{
#if ENABLE(JIT_VERBOSE_OSR)
    dataLog("%p: Entered %s with executeCounter = %d\n", codeBlock, name, codeBlock->llintExecuteCounter());
#endif
    
    if (!shouldJIT(exec)) {
        codeBlock->dontJITAnytimeSoon();
        LLINT_RETURN_TWO(0, exec);
    }
    if (!jitCompileAndSetHeuristics(codeBlock, exec))
        LLINT_RETURN_TWO(0, exec);
    
    if (kind == Prologue)
        LLINT_RETURN_TWO(codeBlock->getJITCode().executableAddressAtOffset(0), exec);
    ASSERT(kind == ArityCheck);
    LLINT_RETURN_TWO(codeBlock->getJITCodeWithArityCheck().executableAddress(), exec);
}

LLINT_SLOW_PATH_DECL(entry_osr)
{
    return entryOSR(exec, pc, exec->codeBlock(), "entry_osr", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_call)
{
    return entryOSR(exec, pc, &jsCast<JSFunction*>(exec->callee())->jsExecutable()->generatedBytecodeFor(CodeForCall), "entry_osr_function_for_call", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_construct)
{
    return entryOSR(exec, pc, &jsCast<JSFunction*>(exec->callee())->jsExecutable()->generatedBytecodeFor(CodeForConstruct), "entry_osr_function_for_construct", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_call_arityCheck)
{
    return entryOSR(exec, pc, &jsCast<JSFunction*>(exec->callee())->jsExecutable()->generatedBytecodeFor(CodeForCall), "entry_osr_function_for_call_arityCheck", ArityCheck);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_construct_arityCheck)
{
    return entryOSR(exec, pc, &jsCast<JSFunction*>(exec->callee())->jsExecutable()->generatedBytecodeFor(CodeForConstruct), "entry_osr_function_for_construct_arityCheck", ArityCheck);
}

LLINT_SLOW_PATH_DECL(loop_osr)
{
    CodeBlock* codeBlock = exec->codeBlock();
    
#if ENABLE(JIT_VERBOSE_OSR)
    dataLog("%p: Entered loop_osr with executeCounter = %d\n", codeBlock, codeBlock->llintExecuteCounter());
#endif
    
    if (!shouldJIT(exec)) {
        codeBlock->dontJITAnytimeSoon();
        LLINT_RETURN_TWO(0, exec);
    }
    
    if (!jitCompileAndSetHeuristics(codeBlock, exec))
        LLINT_RETURN_TWO(0, exec);
    
    ASSERT(codeBlock->getJITType() == JITCode::BaselineJIT);
    
    Vector<BytecodeAndMachineOffset> map;
    codeBlock->jitCodeMap()->decode(map);
    BytecodeAndMachineOffset* mapping = binarySearch<BytecodeAndMachineOffset, unsigned, BytecodeAndMachineOffset::getBytecodeIndex>(map.begin(), map.size(), pc - codeBlock->instructions().begin());
    ASSERT(mapping);
    ASSERT(mapping->m_bytecodeIndex == static_cast<unsigned>(pc - codeBlock->instructions().begin()));
    
    void* jumpTarget = codeBlock->getJITCode().executableAddressAtOffset(mapping->m_machineCodeOffset);
    ASSERT(jumpTarget);
    
    LLINT_RETURN_TWO(jumpTarget, exec);
}

LLINT_SLOW_PATH_DECL(replace)
{
    CodeBlock* codeBlock = exec->codeBlock();
    
#if ENABLE(JIT_VERBOSE_OSR)
    dataLog("%p: Entered replace with executeCounter = %d\n", codeBlock, codeBlock->llintExecuteCounter());
#endif
    
    if (shouldJIT(exec))
        jitCompileAndSetHeuristics(codeBlock, exec);
    else
        codeBlock->dontJITAnytimeSoon();
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(register_file_check)
{
    LLINT_BEGIN();
#if LLINT_SLOW_PATH_TRACING
    dataLog("Checking stack height with exec = %p.\n", exec);
    dataLog("CodeBlock = %p.\n", exec->codeBlock());
    dataLog("Num callee registers = %u.\n", exec->codeBlock()->m_numCalleeRegisters);
    dataLog("Num vars = %u.\n", exec->codeBlock()->m_numVars);
    dataLog("Current end is at %p.\n", exec->globalData().interpreter->registerFile().end());
#endif
    ASSERT(&exec->registers()[exec->codeBlock()->m_numCalleeRegisters] > exec->globalData().interpreter->registerFile().end());
    if (UNLIKELY(!globalData.interpreter->registerFile().grow(&exec->registers()[exec->codeBlock()->m_numCalleeRegisters]))) {
        ReturnAddressPtr returnPC = exec->returnPC();
        exec = exec->callerFrame();
        globalData.exception = createStackOverflowError(exec);
        interpreterThrowInCaller(exec, returnPC);
        pc = returnToThrowForThrownException(exec);
    }
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_call_arityCheck)
{
    LLINT_BEGIN();
    ExecState* newExec = CommonSlowPaths::arityCheckFor(exec, &globalData.interpreter->registerFile(), CodeForCall);
    if (!newExec) {
        ReturnAddressPtr returnPC = exec->returnPC();
        exec = exec->callerFrame();
        globalData.exception = createStackOverflowError(exec);
        interpreterThrowInCaller(exec, returnPC);
        LLINT_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), exec);
    }
    LLINT_RETURN_TWO(0, newExec);
}

LLINT_SLOW_PATH_DECL(slow_path_construct_arityCheck)
{
    LLINT_BEGIN();
    ExecState* newExec = CommonSlowPaths::arityCheckFor(exec, &globalData.interpreter->registerFile(), CodeForConstruct);
    if (!newExec) {
        ReturnAddressPtr returnPC = exec->returnPC();
        exec = exec->callerFrame();
        globalData.exception = createStackOverflowError(exec);
        interpreterThrowInCaller(exec, returnPC);
        LLINT_RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), exec);
    }
    LLINT_RETURN_TWO(0, newExec);
}

LLINT_SLOW_PATH_DECL(slow_path_create_activation)
{
    LLINT_BEGIN();
#if LLINT_SLOW_PATH_TRACING
    dataLog("Creating an activation, exec = %p!\n", exec);
#endif
    JSActivation* activation = JSActivation::create(globalData, exec, static_cast<FunctionExecutable*>(exec->codeBlock()->ownerExecutable()));
    exec->setScopeChain(exec->scopeChain()->push(activation));
    LLINT_RETURN(JSValue(activation));
}

LLINT_SLOW_PATH_DECL(slow_path_create_arguments)
{
    LLINT_BEGIN();
    JSValue arguments = JSValue(Arguments::create(globalData, exec));
    LLINT_CHECK_EXCEPTION();
    exec->uncheckedR(pc[1].u.operand) = arguments;
    exec->uncheckedR(unmodifiedArgumentsRegister(pc[1].u.operand)) = arguments;
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_create_this)
{
    LLINT_BEGIN();
    JSFunction* constructor = jsCast<JSFunction*>(exec->callee());
    
#if !ASSERT_DISABLED
    ConstructData constructData;
    ASSERT(constructor->methodTable()->getConstructData(constructor, constructData) == ConstructTypeJS);
#endif
    
    Structure* structure;
    JSValue proto = LLINT_OP(2).jsValue();
    if (proto.isObject())
        structure = asObject(proto)->inheritorID(globalData);
    else
        structure = constructor->scope()->globalObject->emptyObjectStructure();
    
    LLINT_RETURN(constructEmptyObject(exec, structure));
}

LLINT_SLOW_PATH_DECL(slow_path_convert_this)
{
    LLINT_BEGIN();
    JSValue v1 = LLINT_OP(1).jsValue();
    ASSERT(v1.isPrimitive());
    LLINT_RETURN(v1.toThisObject(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_new_object)
{
    LLINT_BEGIN();
    LLINT_RETURN(constructEmptyObject(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_new_array)
{
    LLINT_BEGIN();
    LLINT_RETURN(constructArray(exec, bitwise_cast<JSValue*>(&LLINT_OP(2)), pc[3].u.operand));
}

LLINT_SLOW_PATH_DECL(slow_path_new_array_buffer)
{
    LLINT_BEGIN();
    LLINT_RETURN(constructArray(exec, exec->codeBlock()->constantBuffer(pc[2].u.operand), pc[3].u.operand));
}

LLINT_SLOW_PATH_DECL(slow_path_new_regexp)
{
    LLINT_BEGIN();
    RegExp* regExp = exec->codeBlock()->regexp(pc[2].u.operand);
    if (!regExp->isValid())
        LLINT_THROW(createSyntaxError(exec, "Invalid flag supplied to RegExp constructor."));
    LLINT_RETURN(RegExpObject::create(globalData, exec->lexicalGlobalObject(), exec->lexicalGlobalObject()->regExpStructure(), regExp));
}

LLINT_SLOW_PATH_DECL(slow_path_not)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(!LLINT_OP_C(2).jsValue().toBoolean(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_eq)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(JSValue::equal(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_neq)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(!JSValue::equal(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_stricteq)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(JSValue::strictEqual(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_nstricteq)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(!JSValue::strictEqual(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_less)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(jsLess<true>(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_lesseq)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(jsLessEq<true>(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_greater)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(jsLess<false>(exec, LLINT_OP_C(3).jsValue(), LLINT_OP_C(2).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_greatereq)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(jsLessEq<false>(exec, LLINT_OP_C(3).jsValue(), LLINT_OP_C(2).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_pre_inc)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP(1).jsValue().toNumber(exec) + 1));
}

LLINT_SLOW_PATH_DECL(slow_path_pre_dec)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP(1).jsValue().toNumber(exec) - 1));
}

LLINT_SLOW_PATH_DECL(slow_path_post_inc)
{
    LLINT_BEGIN();
    double result = LLINT_OP(2).jsValue().toNumber(exec);
    LLINT_OP(2) = jsNumber(result + 1);
    LLINT_RETURN(jsNumber(result));
}

LLINT_SLOW_PATH_DECL(slow_path_post_dec)
{
    LLINT_BEGIN();
    double result = LLINT_OP(2).jsValue().toNumber(exec);
    LLINT_OP(2) = jsNumber(result - 1);
    LLINT_RETURN(jsNumber(result));
}

LLINT_SLOW_PATH_DECL(slow_path_to_jsnumber)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toNumber(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_negate)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(-LLINT_OP_C(2).jsValue().toNumber(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_add)
{
    LLINT_BEGIN();
    JSValue v1 = LLINT_OP_C(2).jsValue();
    JSValue v2 = LLINT_OP_C(3).jsValue();
    
#if LLINT_SLOW_PATH_TRACING
    dataLog("Trying to add %s", v1.description());
    dataLog(" to %s.\n", v2.description());
#endif
    
    if (v1.isString() && !v2.isObject())
        LLINT_RETURN(jsString(exec, asString(v1), v2.toString(exec)));
    
    if (v1.isNumber() && v2.isNumber())
        LLINT_RETURN(jsNumber(v1.asNumber() + v2.asNumber()));
    
    LLINT_RETURN(jsAddSlowCase(exec, v1, v2));
}

LLINT_SLOW_PATH_DECL(slow_path_mul)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toNumber(exec) * LLINT_OP_C(3).jsValue().toNumber(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_sub)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toNumber(exec) - LLINT_OP_C(3).jsValue().toNumber(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_div)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toNumber(exec) / LLINT_OP_C(3).jsValue().toNumber(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_mod)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(fmod(LLINT_OP_C(2).jsValue().toNumber(exec), LLINT_OP_C(3).jsValue().toNumber(exec))));
}

LLINT_SLOW_PATH_DECL(slow_path_lshift)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toInt32(exec) << (LLINT_OP_C(3).jsValue().toUInt32(exec) & 31)));
}

LLINT_SLOW_PATH_DECL(slow_path_rshift)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toInt32(exec) >> (LLINT_OP_C(3).jsValue().toUInt32(exec) & 31)));
}

LLINT_SLOW_PATH_DECL(slow_path_urshift)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toUInt32(exec) >> (LLINT_OP_C(3).jsValue().toUInt32(exec) & 31)));
}

LLINT_SLOW_PATH_DECL(slow_path_bitand)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toInt32(exec) & LLINT_OP_C(3).jsValue().toInt32(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_bitor)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toInt32(exec) | LLINT_OP_C(3).jsValue().toInt32(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_bitxor)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsNumber(LLINT_OP_C(2).jsValue().toInt32(exec) ^ LLINT_OP_C(3).jsValue().toInt32(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_check_has_instance)
{
    LLINT_BEGIN();
    JSValue baseVal = LLINT_OP_C(1).jsValue();
#ifndef NDEBUG
    TypeInfo typeInfo(UnspecifiedType);
    ASSERT(!baseVal.isObject()
           || !(typeInfo = asObject(baseVal)->structure()->typeInfo()).implementsHasInstance());
#endif
    LLINT_THROW(createInvalidParamError(exec, "instanceof", baseVal));
}

LLINT_SLOW_PATH_DECL(slow_path_instanceof)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(CommonSlowPaths::opInstanceOfSlow(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue(), LLINT_OP_C(4).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_typeof)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsTypeStringForValue(exec, LLINT_OP_C(2).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_is_object)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(jsIsObjectType(LLINT_OP_C(2).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_is_function)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(jsIsFunctionType(LLINT_OP_C(2).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_in)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsBoolean(CommonSlowPaths::opIn(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue())));
}

LLINT_SLOW_PATH_DECL(slow_path_resolve)
{
    LLINT_BEGIN();
    LLINT_RETURN_PROFILED(op_resolve, CommonSlowPaths::opResolve(exec, exec->codeBlock()->identifier(pc[2].u.operand)));
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_skip)
{
    LLINT_BEGIN();
    LLINT_RETURN_PROFILED(
        op_resolve_skip,
        CommonSlowPaths::opResolveSkip(
            exec,
            exec->codeBlock()->identifier(pc[2].u.operand),
            pc[3].u.operand));
}

static JSValue resolveGlobal(ExecState* exec, Instruction* pc)
{
    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    ASSERT(globalObject->isGlobalObject());
    int property = pc[2].u.operand;
    Structure* structure = pc[3].u.structure.get();
    
    ASSERT_UNUSED(structure, structure != globalObject->structure());
    
    Identifier& ident = codeBlock->identifier(property);
    PropertySlot slot(globalObject);
    
    if (globalObject->getPropertySlot(exec, ident, slot)) {
        JSValue result = slot.getValue(exec, ident);
        if (slot.isCacheableValue() && !globalObject->structure()->isUncacheableDictionary()
            && slot.slotBase() == globalObject) {
            pc[3].u.structure.set(
                exec->globalData(), codeBlock->ownerExecutable(), globalObject->structure());
            pc[4] = slot.cachedOffset();
        }
        
        return result;
    }
    
    exec->globalData().exception = createUndefinedVariableError(exec, ident);
    return JSValue();
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_global)
{
    LLINT_BEGIN();
    LLINT_RETURN_PROFILED(op_resolve_global, resolveGlobal(exec, pc));
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_global_dynamic)
{
    LLINT_BEGIN();
    LLINT_RETURN_PROFILED(op_resolve_global_dynamic, resolveGlobal(exec, pc));
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_for_resolve_global_dynamic)
{
    LLINT_BEGIN();
    LLINT_RETURN_PROFILED(op_resolve_global_dynamic, CommonSlowPaths::opResolve(exec, exec->codeBlock()->identifier(pc[2].u.operand)));
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_base)
{
    LLINT_BEGIN();
    Identifier& ident = exec->codeBlock()->identifier(pc[2].u.operand);
    if (pc[3].u.operand) {
        JSValue base = JSC::resolveBase(exec, ident, exec->scopeChain(), true);
        if (!base)
            LLINT_THROW(createErrorForInvalidGlobalAssignment(exec, ident.ustring()));
        LLINT_RETURN(base);
    }
    
    LLINT_RETURN_PROFILED(op_resolve_base, JSC::resolveBase(exec, ident, exec->scopeChain(), false));
}

LLINT_SLOW_PATH_DECL(slow_path_ensure_property_exists)
{
    LLINT_BEGIN();
    JSObject* object = asObject(LLINT_OP(1).jsValue());
    PropertySlot slot(object);
    Identifier& ident = exec->codeBlock()->identifier(pc[2].u.operand);
    if (!object->getPropertySlot(exec, ident, slot))
        LLINT_THROW(createErrorForInvalidGlobalAssignment(exec, ident.ustring()));
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_with_base)
{
    LLINT_BEGIN();
    JSValue result = CommonSlowPaths::opResolveWithBase(exec, exec->codeBlock()->identifier(pc[3].u.operand), LLINT_OP(1));
    LLINT_CHECK_EXCEPTION();
    LLINT_OP(2) = result;
    // FIXME: technically should have profiling, but we don't do it because the DFG won't use it.
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_resolve_with_this)
{
    LLINT_BEGIN();
    JSValue result = CommonSlowPaths::opResolveWithThis(exec, exec->codeBlock()->identifier(pc[3].u.operand), LLINT_OP(1));
    LLINT_CHECK_EXCEPTION();
    LLINT_OP(2) = result;
    // FIXME: technically should have profiling, but we don't do it because the DFG won't use it.
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_id)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    Identifier& ident = codeBlock->identifier(pc[3].u.operand);
    JSValue baseValue = LLINT_OP_C(2).jsValue();
    PropertySlot slot(baseValue);

    JSValue result = baseValue.get(exec, ident, slot);
    LLINT_CHECK_EXCEPTION();
    LLINT_OP(1) = result;

    if (baseValue.isCell()
        && slot.isCacheable()
        && slot.slotBase() == baseValue
        && slot.cachedPropertyType() == PropertySlot::Value) {
        
        JSCell* baseCell = baseValue.asCell();
        Structure* structure = baseCell->structure();
        
        if (!structure->isUncacheableDictionary()
            && !structure->typeInfo().prohibitsPropertyCaching()) {
            pc[4].u.structure.set(
                globalData, codeBlock->ownerExecutable(), structure);
            pc[5].u.operand = slot.cachedOffset() * sizeof(JSValue);
        }
    }

#if ENABLE(VALUE_PROFILER)    
    pc[OPCODE_LENGTH(op_get_by_id) - 1].u.profile->m_buckets[0] = JSValue::encode(result);
#endif
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_get_arguments_length)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    Identifier& ident = codeBlock->identifier(pc[3].u.operand);
    JSValue baseValue = LLINT_OP(2).jsValue();
    PropertySlot slot(baseValue);
    LLINT_RETURN(baseValue.get(exec, ident, slot));
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_id)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    Identifier& ident = codeBlock->identifier(pc[2].u.operand);
    
    JSValue baseValue = LLINT_OP_C(1).jsValue();
    PutPropertySlot slot(codeBlock->isStrictMode());
    if (pc[8].u.operand)
        asObject(baseValue)->putDirect(globalData, ident, LLINT_OP_C(3).jsValue(), slot);
    else
        baseValue.put(exec, ident, LLINT_OP_C(3).jsValue(), slot);
    LLINT_CHECK_EXCEPTION();
    
    if (baseValue.isCell()
        && slot.isCacheable()) {
        
        JSCell* baseCell = baseValue.asCell();
        Structure* structure = baseCell->structure();
        
        if (!structure->isUncacheableDictionary()
            && !structure->typeInfo().prohibitsPropertyCaching()
            && baseCell == slot.base()) {
            
            if (slot.type() == PutPropertySlot::NewProperty) {
                if (!structure->isDictionary() && structure->previousID()->propertyStorageCapacity() == structure->propertyStorageCapacity()) {
                    // This is needed because some of the methods we call
                    // below may GC.
                    pc[0].u.opcode = bitwise_cast<void*>(&llint_op_put_by_id);

                    normalizePrototypeChain(exec, baseCell);
                    
                    ASSERT(structure->previousID()->isObject());
                    pc[4].u.structure.set(
                        globalData, codeBlock->ownerExecutable(), structure->previousID());
                    pc[5].u.operand = slot.cachedOffset() * sizeof(JSValue);
                    pc[6].u.structure.set(
                        globalData, codeBlock->ownerExecutable(), structure);
                    StructureChain* chain = structure->prototypeChain(exec);
                    ASSERT(chain);
                    pc[7].u.structureChain.set(
                        globalData, codeBlock->ownerExecutable(), chain);
                    
                    if (pc[8].u.operand)
                        pc[0].u.opcode = bitwise_cast<void*>(&llint_op_put_by_id_transition_direct);
                    else
                        pc[0].u.opcode = bitwise_cast<void*>(&llint_op_put_by_id_transition_normal);
                }
            } else {
                pc[0].u.opcode = bitwise_cast<void*>(&llint_op_put_by_id);
                pc[4].u.structure.set(
                    globalData, codeBlock->ownerExecutable(), structure);
                pc[5].u.operand = slot.cachedOffset() * sizeof(JSValue);
            }
        }
    }
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_del_by_id)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    JSObject* baseObject = LLINT_OP_C(2).jsValue().toObject(exec);
    bool couldDelete = baseObject->methodTable()->deleteProperty(baseObject, exec, codeBlock->identifier(pc[3].u.operand));
    LLINT_CHECK_EXCEPTION();
    if (!couldDelete && codeBlock->isStrictMode())
        LLINT_THROW(createTypeError(exec, "Unable to delete property."));
    LLINT_RETURN(jsBoolean(couldDelete));
}

inline JSValue getByVal(ExecState* exec, JSValue baseValue, JSValue subscript)
{
    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        if (JSValue result = baseValue.asCell()->fastGetOwnProperty(exec, asString(subscript)->value(exec)))
            return result;
    }
    
    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            return asString(baseValue)->getIndex(exec, i);
        
        return baseValue.get(exec, i);
    }
    
    Identifier property(exec, subscript.toString(exec)->value(exec));
    return baseValue.get(exec, property);
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_val)
{
    LLINT_BEGIN();
    LLINT_RETURN_PROFILED(op_get_by_val, getByVal(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_get_argument_by_val)
{
    LLINT_BEGIN();
    JSValue arguments = LLINT_OP(2).jsValue();
    if (!arguments) {
        arguments = Arguments::create(globalData, exec);
        LLINT_CHECK_EXCEPTION();
        LLINT_OP(2) = arguments;
        exec->uncheckedR(unmodifiedArgumentsRegister(pc[2].u.operand)) = arguments;
    }
    
    LLINT_RETURN(getByVal(exec, arguments, LLINT_OP_C(3).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_pname)
{
    LLINT_BEGIN();
    LLINT_RETURN(getByVal(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_val)
{
    LLINT_BEGIN();
    
    JSValue baseValue = LLINT_OP_C(1).jsValue();
    JSValue subscript = LLINT_OP_C(2).jsValue();
    JSValue value = LLINT_OP_C(3).jsValue();
    
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        if (isJSArray(baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canSetIndex(i))
                jsArray->setIndex(globalData, i, value);
            else
                JSArray::putByIndex(jsArray, exec, i, value, exec->codeBlock()->isStrictMode());
            LLINT_END();
        }
        baseValue.putByIndex(exec, i, value, exec->codeBlock()->isStrictMode());
        LLINT_END();
    }
    
    Identifier property(exec, subscript.toString(exec)->value(exec));
    LLINT_CHECK_EXCEPTION();
    PutPropertySlot slot(exec->codeBlock()->isStrictMode());
    baseValue.put(exec, property, value, slot);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_del_by_val)
{
    LLINT_BEGIN();
    JSValue baseValue = LLINT_OP_C(2).jsValue();
    JSObject* baseObject = baseValue.toObject(exec);
    
    JSValue subscript = LLINT_OP_C(3).jsValue();
    
    bool couldDelete;
    
    uint32_t i;
    if (subscript.getUInt32(i))
        couldDelete = baseObject->methodTable()->deletePropertyByIndex(baseObject, exec, i);
    else {
        LLINT_CHECK_EXCEPTION();
        Identifier property(exec, subscript.toString(exec)->value(exec));
        LLINT_CHECK_EXCEPTION();
        couldDelete = baseObject->methodTable()->deleteProperty(baseObject, exec, property);
    }
    
    if (!couldDelete && exec->codeBlock()->isStrictMode())
        LLINT_THROW(createTypeError(exec, "Unable to delete property."));
    
    LLINT_RETURN(jsBoolean(couldDelete));
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_index)
{
    LLINT_BEGIN();
    JSValue arrayValue = LLINT_OP_C(1).jsValue();
    ASSERT(isJSArray(arrayValue));
    asArray(arrayValue)->putDirectIndex(exec, pc[2].u.operand, LLINT_OP_C(3).jsValue(), false);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_setter)
{
    LLINT_BEGIN();
    ASSERT(LLINT_OP(1).jsValue().isObject());
    JSObject* baseObj = asObject(LLINT_OP(1).jsValue());
    
    GetterSetter* accessor = GetterSetter::create(exec);
    LLINT_CHECK_EXCEPTION();
    
    JSValue getter = LLINT_OP(3).jsValue();
    JSValue setter = LLINT_OP(4).jsValue();
    ASSERT(getter.isObject() || getter.isUndefined());
    ASSERT(setter.isObject() || setter.isUndefined());
    ASSERT(getter.isObject() || setter.isObject());
    
    if (!getter.isUndefined())
        accessor->setGetter(globalData, asObject(getter));
    if (!setter.isUndefined())
        accessor->setSetter(globalData, asObject(setter));
    baseObj->putDirectAccessor(
        globalData,
        exec->codeBlock()->identifier(pc[2].u.operand),
        accessor, Accessor);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_jmp_scopes)
{
    LLINT_BEGIN();
    unsigned count = pc[1].u.operand;
    ScopeChainNode* tmp = exec->scopeChain();
    while (count--)
        tmp = tmp->pop();
    exec->setScopeChain(tmp);
    pc += pc[2].u.operand;
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_jtrue)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jtrue, LLINT_OP_C(1).jsValue().toBoolean(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_jfalse)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jfalse, !LLINT_OP_C(1).jsValue().toBoolean(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_jless)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jless, jsLess<true>(exec, LLINT_OP_C(1).jsValue(), LLINT_OP_C(2).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jnless)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jnless, !jsLess<true>(exec, LLINT_OP_C(1).jsValue(), LLINT_OP_C(2).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jgreater)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jgreater, jsLess<false>(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(1).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jngreater)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jngreater, !jsLess<false>(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(1).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jlesseq)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jlesseq, jsLessEq<true>(exec, LLINT_OP_C(1).jsValue(), LLINT_OP_C(2).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jnlesseq)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jnlesseq, !jsLessEq<true>(exec, LLINT_OP_C(1).jsValue(), LLINT_OP_C(2).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jgreatereq)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jgreatereq, jsLessEq<false>(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(1).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jngreatereq)
{
    LLINT_BEGIN();
    LLINT_BRANCH(op_jngreatereq, !jsLessEq<false>(exec, LLINT_OP_C(2).jsValue(), LLINT_OP_C(1).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_switch_imm)
{
    LLINT_BEGIN();
    JSValue scrutinee = LLINT_OP_C(3).jsValue();
    ASSERT(scrutinee.isDouble());
    double value = scrutinee.asDouble();
    int32_t intValue = static_cast<int32_t>(value);
    int defaultOffset = pc[2].u.operand;
    if (value == intValue) {
        CodeBlock* codeBlock = exec->codeBlock();
        pc += codeBlock->immediateSwitchJumpTable(pc[1].u.operand).offsetForValue(intValue, defaultOffset);
    } else
        pc += defaultOffset;
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_switch_char)
{
    LLINT_BEGIN();
    JSValue scrutinee = LLINT_OP_C(3).jsValue();
    ASSERT(scrutinee.isString());
    JSString* string = asString(scrutinee);
    ASSERT(string->length() == 1);
    int defaultOffset = pc[2].u.operand;
    StringImpl* impl = string->value(exec).impl();
    CodeBlock* codeBlock = exec->codeBlock();
    pc += codeBlock->characterSwitchJumpTable(pc[1].u.operand).offsetForValue((*impl)[0], defaultOffset);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_switch_string)
{
    LLINT_BEGIN();
    JSValue scrutinee = LLINT_OP_C(3).jsValue();
    int defaultOffset = pc[2].u.operand;
    if (!scrutinee.isString())
        pc += defaultOffset;
    else {
        CodeBlock* codeBlock = exec->codeBlock();
        pc += codeBlock->stringSwitchJumpTable(pc[1].u.operand).offsetForValue(asString(scrutinee)->value(exec).impl(), defaultOffset);
    }
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_new_func)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    ASSERT(codeBlock->codeType() != FunctionCode
           || !codeBlock->needsFullScopeChain()
           || exec->uncheckedR(codeBlock->activationRegister()).jsValue());
#if LLINT_SLOW_PATH_TRACING
    dataLog("Creating function!\n");
#endif
    LLINT_RETURN(codeBlock->functionDecl(pc[2].u.operand)->make(exec, exec->scopeChain()));
}

LLINT_SLOW_PATH_DECL(slow_path_new_func_exp)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    FunctionExecutable* function = codeBlock->functionExpr(pc[2].u.operand);
    JSFunction* func = function->make(exec, exec->scopeChain());
    
    if (!function->name().isNull()) {
        JSStaticScopeObject* functionScopeObject = JSStaticScopeObject::create(exec, function->name(), func, ReadOnly | DontDelete);
        func->setScope(globalData, func->scope()->push(functionScopeObject));
    }
    
    LLINT_RETURN(func);
}

static SlowPathReturnType handleHostCall(ExecState* execCallee, Instruction* pc, JSValue callee, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    JSGlobalData& globalData = exec->globalData();

    execCallee->setScopeChain(exec->scopeChain());
    execCallee->setCodeBlock(0);
    execCallee->clearReturnPC();

    if (kind == CodeForCall) {
        CallData callData;
        CallType callType = getCallData(callee, callData);
    
        ASSERT(callType != CallTypeJS);
    
        if (callType == CallTypeHost) {
            NativeCallFrameTracer tracer(&globalData, execCallee);
            execCallee->setCallee(asObject(callee));
            globalData.hostCallReturnValue = JSValue::decode(callData.native.function(execCallee));
            
            LLINT_CALL_RETURN(execCallee, pc, reinterpret_cast<void*>(getHostCallReturnValue));
        }
        
#if LLINT_SLOW_PATH_TRACING
        dataLog("Call callee is not a function: %s\n", callee.description());
#endif

        ASSERT(callType == CallTypeNone);
        LLINT_CALL_THROW(exec, pc, createNotAFunctionError(exec, callee));
    }

    ASSERT(kind == CodeForConstruct);
    
    ConstructData constructData;
    ConstructType constructType = getConstructData(callee, constructData);
    
    ASSERT(constructType != ConstructTypeJS);
    
    if (constructType == ConstructTypeHost) {
        NativeCallFrameTracer tracer(&globalData, execCallee);
        execCallee->setCallee(asObject(callee));
        globalData.hostCallReturnValue = JSValue::decode(constructData.native.function(execCallee));

        LLINT_CALL_RETURN(execCallee, pc, reinterpret_cast<void*>(getHostCallReturnValue));
    }
    
#if LLINT_SLOW_PATH_TRACING
    dataLog("Constructor callee is not a function: %s\n", callee.description());
#endif

    ASSERT(constructType == ConstructTypeNone);
    LLINT_CALL_THROW(exec, pc, createNotAConstructorError(exec, callee));
}

inline SlowPathReturnType setUpCall(ExecState* execCallee, Instruction* pc, CodeSpecializationKind kind, JSValue calleeAsValue, LLIntCallLinkInfo* callLinkInfo = 0)
{
#if LLINT_SLOW_PATH_TRACING
    dataLog("Performing call with recorded PC = %p\n", execCallee->callerFrame()->currentVPC());
#endif

    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell)
        return handleHostCall(execCallee, pc, calleeAsValue, kind);
    
    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    ScopeChainNode* scope = callee->scopeUnchecked();
    JSGlobalData& globalData = *scope->globalData;
    execCallee->setScopeChain(scope);
    ExecutableBase* executable = callee->executable();
    
    MacroAssemblerCodePtr codePtr;
    CodeBlock* codeBlock = 0;
    if (executable->isHostFunction())
        codePtr = executable->generatedJITCodeFor(kind).addressForCall();
    else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->compileFor(execCallee, callee->scope(), kind);
        if (error)
            LLINT_CALL_THROW(execCallee->callerFrame(), pc, error);
        codeBlock = &functionExecutable->generatedBytecodeFor(kind);
        ASSERT(codeBlock);
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()))
            codePtr = functionExecutable->generatedJITCodeWithArityCheckFor(kind);
        else
            codePtr = functionExecutable->generatedJITCodeFor(kind).addressForCall();
    }
    
    if (callLinkInfo) {
        if (callLinkInfo->isOnList())
            callLinkInfo->remove();
        ExecState* execCaller = execCallee->callerFrame();
        callLinkInfo->callee.set(globalData, execCaller->codeBlock()->ownerExecutable(), callee);
        callLinkInfo->lastSeenCallee.set(globalData, execCaller->codeBlock()->ownerExecutable(), callee);
        callLinkInfo->machineCodeTarget = codePtr;
        if (codeBlock)
            codeBlock->linkIncomingCall(callLinkInfo);
    }
    
    LLINT_CALL_RETURN(execCallee, pc, codePtr.executableAddress());
}

inline SlowPathReturnType genericCall(ExecState* exec, Instruction* pc, CodeSpecializationKind kind)
{
    // This needs to:
    // - Set up a call frame.
    // - Figure out what to call and compile it if necessary.
    // - If possible, link the call's inline cache.
    // - Return a tuple of machine code address to call and the new call frame.
    
    JSValue calleeAsValue = LLINT_OP_C(1).jsValue();
    
    ExecState* execCallee = exec + pc[3].u.operand;
    
    execCallee->setArgumentCountIncludingThis(pc[2].u.operand);
    execCallee->uncheckedR(RegisterFile::Callee) = calleeAsValue;
    execCallee->setCallerFrame(exec);
    
    ASSERT(pc[4].u.callLinkInfo);
    return setUpCall(execCallee, pc, kind, calleeAsValue, pc[4].u.callLinkInfo);
}

LLINT_SLOW_PATH_DECL(slow_path_call)
{
    LLINT_BEGIN_NO_SET_PC();
    return genericCall(exec, pc, CodeForCall);
}

LLINT_SLOW_PATH_DECL(slow_path_construct)
{
    LLINT_BEGIN_NO_SET_PC();
    return genericCall(exec, pc, CodeForConstruct);
}

LLINT_SLOW_PATH_DECL(slow_path_call_varargs)
{
    LLINT_BEGIN();
    // This needs to:
    // - Set up a call frame while respecting the variable arguments.
    // - Figure out what to call and compile it if necessary.
    // - Return a tuple of machine code address to call and the new call frame.
    
    JSValue calleeAsValue = LLINT_OP_C(1).jsValue();
    
    ExecState* execCallee = loadVarargs(
        exec, &globalData.interpreter->registerFile(),
        LLINT_OP_C(2).jsValue(), LLINT_OP_C(3).jsValue(), pc[4].u.operand);
    LLINT_CALL_CHECK_EXCEPTION(exec, pc);
    
    execCallee->uncheckedR(RegisterFile::Callee) = calleeAsValue;
    execCallee->setCallerFrame(exec);
    exec->setCurrentVPC(pc + OPCODE_LENGTH(op_call_varargs));
    
    return setUpCall(execCallee, pc, CodeForCall, calleeAsValue);
}

LLINT_SLOW_PATH_DECL(slow_path_call_eval)
{
    LLINT_BEGIN_NO_SET_PC();
    JSValue calleeAsValue = LLINT_OP(1).jsValue();
    
    ExecState* execCallee = exec + pc[3].u.operand;
    
    execCallee->setArgumentCountIncludingThis(pc[2].u.operand);
    execCallee->setCallerFrame(exec);
    execCallee->uncheckedR(RegisterFile::Callee) = calleeAsValue;
    execCallee->setScopeChain(exec->scopeChain());
    execCallee->setReturnPC(bitwise_cast<Instruction*>(&llint_generic_return_point));
    execCallee->setCodeBlock(0);
    exec->setCurrentVPC(pc + OPCODE_LENGTH(op_call_eval));
    
    if (!isHostFunction(calleeAsValue, globalFuncEval))
        return setUpCall(execCallee, pc, CodeForCall, calleeAsValue);
    
    globalData.hostCallReturnValue = eval(execCallee);
    LLINT_CALL_RETURN(execCallee, pc, reinterpret_cast<void*>(getHostCallReturnValue));
}

LLINT_SLOW_PATH_DECL(slow_path_tear_off_activation)
{
    LLINT_BEGIN();
    ASSERT(exec->codeBlock()->needsFullScopeChain());
    JSValue activationValue = LLINT_OP(1).jsValue();
    if (!activationValue) {
        if (JSValue v = exec->uncheckedR(unmodifiedArgumentsRegister(pc[2].u.operand)).jsValue()) {
            if (!exec->codeBlock()->isStrictMode())
                asArguments(v)->tearOff(exec);
        }
        LLINT_END();
    }
    JSActivation* activation = asActivation(activationValue);
    activation->tearOff(globalData);
    if (JSValue v = exec->uncheckedR(unmodifiedArgumentsRegister(pc[2].u.operand)).jsValue())
        asArguments(v)->didTearOffActivation(globalData, activation);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_tear_off_arguments)
{
    LLINT_BEGIN();
    ASSERT(exec->codeBlock()->usesArguments() && !exec->codeBlock()->needsFullScopeChain());
    asArguments(exec->uncheckedR(unmodifiedArgumentsRegister(pc[1].u.operand)).jsValue())->tearOff(exec);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_strcat)
{
    LLINT_BEGIN();
    LLINT_RETURN(jsString(exec, &LLINT_OP(2), pc[3].u.operand));
}

LLINT_SLOW_PATH_DECL(slow_path_to_primitive)
{
    LLINT_BEGIN();
    LLINT_RETURN(LLINT_OP_C(2).jsValue().toPrimitive(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_get_pnames)
{
    LLINT_BEGIN();
    JSValue v = LLINT_OP(2).jsValue();
    if (v.isUndefinedOrNull()) {
        pc += pc[5].u.operand;
        LLINT_END();
    }
    
    JSObject* o = v.toObject(exec);
    Structure* structure = o->structure();
    JSPropertyNameIterator* jsPropertyNameIterator = structure->enumerationCache();
    if (!jsPropertyNameIterator || jsPropertyNameIterator->cachedPrototypeChain() != structure->prototypeChain(exec))
        jsPropertyNameIterator = JSPropertyNameIterator::create(exec, o);
    
    LLINT_OP(1) = JSValue(jsPropertyNameIterator);
    LLINT_OP(2) = JSValue(o);
    LLINT_OP(3) = Register::withInt(0);
    LLINT_OP(4) = Register::withInt(jsPropertyNameIterator->size());
    
    pc += OPCODE_LENGTH(op_get_pnames);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_next_pname)
{
    LLINT_BEGIN();
    JSObject* base = asObject(LLINT_OP(2).jsValue());
    JSString* property = asString(LLINT_OP(1).jsValue());
    if (base->hasProperty(exec, Identifier(exec, property->value(exec)))) {
        // Go to target.
        pc += pc[6].u.operand;
    } // Else, don't change the PC, so the interpreter will reloop.
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_push_scope)
{
    LLINT_BEGIN();
    JSValue v = LLINT_OP(1).jsValue();
    JSObject* o = v.toObject(exec);
    LLINT_CHECK_EXCEPTION();
    
    LLINT_OP(1) = o;
    exec->setScopeChain(exec->scopeChain()->push(o));
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_pop_scope)
{
    LLINT_BEGIN();
    exec->setScopeChain(exec->scopeChain()->pop());
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_push_new_scope)
{
    LLINT_BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    JSObject* scope = JSStaticScopeObject::create(exec, codeBlock->identifier(pc[2].u.operand), LLINT_OP(3).jsValue(), DontDelete);
    exec->setScopeChain(exec->scopeChain()->push(scope));
    LLINT_RETURN(scope);
}

LLINT_SLOW_PATH_DECL(slow_path_throw)
{
    LLINT_BEGIN();
    LLINT_THROW(LLINT_OP_C(1).jsValue());
}

LLINT_SLOW_PATH_DECL(slow_path_throw_reference_error)
{
    LLINT_BEGIN();
    LLINT_THROW(createReferenceError(exec, LLINT_OP_C(1).jsValue().toString(exec)->value(exec)));
}

LLINT_SLOW_PATH_DECL(slow_path_debug)
{
    LLINT_BEGIN();
    int debugHookID = pc[1].u.operand;
    int firstLine = pc[2].u.operand;
    int lastLine = pc[3].u.operand;
    
    globalData.interpreter->debug(exec, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_profile_will_call)
{
    LLINT_BEGIN();
    (*Profiler::enabledProfilerReference())->willExecute(exec, LLINT_OP(1).jsValue());
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_profile_did_call)
{
    LLINT_BEGIN();
    (*Profiler::enabledProfilerReference())->didExecute(exec, LLINT_OP(1).jsValue());
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(throw_from_native_call)
{
    LLINT_BEGIN();
    ASSERT(globalData.exception);
    LLINT_END();
}

} } // namespace JSC::LLInt

#endif // ENABLE(LLINT)
