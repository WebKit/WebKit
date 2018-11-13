/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "ArrayConstructor.h"
#include "CallFrame.h"
#include "CommonSlowPaths.h"
#include "Error.h"
#include "ErrorHandlingScope.h"
#include "EvalCodeBlock.h"
#include "Exception.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "FunctionCodeBlock.h"
#include "FunctionWhitelist.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "InterpreterInlines.h"
#include "IteratorOperations.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JITWorklist.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLexicalEnvironment.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LLIntCommon.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "LLIntPrototypeLoadAdaptiveStructureWatchpoint.h"
#include "LowLevelInterpreter.h"
#include "ModuleProgramCodeBlock.h"
#include "ObjectConstructor.h"
#include "ObjectPropertyConditionSet.h"
#include "OpcodeInlines.h"
#include "ProgramCodeBlock.h"
#include "ProtoCallFrame.h"
#include "RegExpObject.h"
#include "ShadowChicken.h"
#include "StructureRareDataInlines.h"
#include "SuperSampler.h"
#include "VMInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>

namespace JSC { namespace LLInt {

#define LLINT_BEGIN_NO_SET_PC() \
    VM& vm = exec->vm();      \
    NativeCallFrameTracer tracer(&vm, exec); \
    auto throwScope = DECLARE_THROW_SCOPE(vm)

#ifndef NDEBUG
#define LLINT_SET_PC_FOR_STUBS() do { \
        exec->codeBlock()->bytecodeOffset(pc); \
        exec->setCurrentVPC(pc); \
    } while (false)
#else
#define LLINT_SET_PC_FOR_STUBS() do { \
        exec->setCurrentVPC(pc); \
    } while (false)
#endif

#define LLINT_BEGIN()                           \
    LLINT_BEGIN_NO_SET_PC();                    \
    LLINT_SET_PC_FOR_STUBS()

#define LLINT_OP(__r) (exec->uncheckedR(__r.offset()))
#define LLINT_OP_C(__r) (exec->r(__r.offset()))

#define LLINT_RETURN_TWO(first, second) do {       \
        return encodeResult(first, second);        \
    } while (false)

#define LLINT_END_IMPL() LLINT_RETURN_TWO(pc, 0)

#define LLINT_THROW(exceptionToThrow) do {                        \
        throwException(exec, throwScope, exceptionToThrow);       \
        pc = returnToThrow(exec);                                 \
        LLINT_END_IMPL();                                         \
    } while (false)

#define LLINT_CHECK_EXCEPTION() do {                    \
        doExceptionFuzzingIfEnabled(exec, throwScope, "LLIntSlowPaths", pc);    \
        if (UNLIKELY(throwScope.exception())) {         \
            pc = returnToThrow(exec);                   \
            LLINT_END_IMPL();                           \
        }                                               \
    } while (false)

#define LLINT_END() do {                        \
        LLINT_CHECK_EXCEPTION();                \
        LLINT_END_IMPL();                       \
    } while (false)

#define JUMP_OFFSET(target) \
    ((target) ? (target) : exec->codeBlock()->outOfLineJumpOffset(pc))

#define JUMP_TO(target) do { \
        pc = reinterpret_cast<const Instruction*>(reinterpret_cast<const uint8_t*>(pc) + (target)); \
    } while (false)

#define LLINT_BRANCH(condition) do {                  \
        bool __b_condition = (condition);                         \
        LLINT_CHECK_EXCEPTION();                                  \
        if (__b_condition)                                        \
            JUMP_TO(JUMP_OFFSET(bytecode.target)); \
        else                                                      \
            JUMP_TO(pc->size()); \
        LLINT_END_IMPL();                                         \
    } while (false)

#define LLINT_RETURN(value) do {                \
        JSValue __r_returnValue = (value);      \
        LLINT_CHECK_EXCEPTION();                \
        LLINT_OP(bytecode.dst) = __r_returnValue;          \
        LLINT_END_IMPL();                       \
    } while (false)

#define LLINT_RETURN_PROFILED(value) do {               \
        JSValue __rp_returnValue = (value);                     \
        LLINT_CHECK_EXCEPTION();                                \
        LLINT_OP(bytecode.dst) = __rp_returnValue;                         \
        LLINT_PROFILE_VALUE(__rp_returnValue);          \
        LLINT_END_IMPL();                                       \
    } while (false)

#define LLINT_PROFILE_VALUE(value) do { \
        bytecode.metadata(exec).profile.m_buckets[0] = JSValue::encode(value); \
    } while (false)

#define LLINT_CALL_END_IMPL(exec, callTarget, callTargetTag) \
    LLINT_RETURN_TWO(retagCodePtr((callTarget), callTargetTag, SlowPathPtrTag), (exec))

#define LLINT_CALL_THROW(exec, exceptionToThrow) do {                   \
        ExecState* __ct_exec = (exec);                                  \
        throwException(__ct_exec, throwScope, exceptionToThrow);        \
        LLINT_CALL_END_IMPL(0, callToThrow(__ct_exec), ExceptionHandlerPtrTag);                 \
    } while (false)

#define LLINT_CALL_CHECK_EXCEPTION(exec, execCallee) do {               \
        ExecState* __cce_exec = (exec);                                 \
        ExecState* __cce_execCallee = (execCallee);                     \
        doExceptionFuzzingIfEnabled(__cce_exec, throwScope, "LLIntSlowPaths/call", nullptr); \
        if (UNLIKELY(throwScope.exception()))                           \
            LLINT_CALL_END_IMPL(0, callToThrow(__cce_execCallee), ExceptionHandlerPtrTag); \
    } while (false)

#define LLINT_CALL_RETURN(exec, execCallee, callTarget, callTargetTag) do { \
        ExecState* __cr_exec = (exec);                                  \
        ExecState* __cr_execCallee = (execCallee);                      \
        void* __cr_callTarget = (callTarget);                           \
        LLINT_CALL_CHECK_EXCEPTION(__cr_exec, __cr_execCallee);         \
        LLINT_CALL_END_IMPL(__cr_execCallee, __cr_callTarget, callTargetTag); \
    } while (false)

#define LLINT_RETURN_CALLEE_FRAME(execCallee) do {                      \
        ExecState* __rcf_exec = (execCallee);                           \
        LLINT_RETURN_TWO(pc, __rcf_exec);                               \
    } while (false)

#if LLINT_TRACING

template<typename... Types>
void slowPathLog(const Types&... values)
{
    dataLogIf(Options::traceLLIntSlowPath(), values...);
}

template<typename... Types>
void slowPathLn(const Types&... values)
{
    dataLogLnIf(Options::traceLLIntSlowPath(), values...);
}

template<typename... Types>
void slowPathLogF(const char* format, const Types&... values)
{
    ALLOW_NONLITERAL_FORMAT_BEGIN
    IGNORE_WARNINGS_BEGIN("format-security")
    if (Options::traceLLIntSlowPath())
        dataLogF(format, values...);
    IGNORE_WARNINGS_END
    ALLOW_NONLITERAL_FORMAT_END
}

#else // not LLINT_TRACING

template<typename... Types> void slowPathLog(const Types&...) { }
template<typename... Types> void slowPathLogLn(const Types&...) { }
template<typename... Types> void slowPathLogF(const char*, const Types&...) { }

#endif // LLINT_TRACING

extern "C" SlowPathReturnType llint_trace_operand(ExecState* exec, const Instruction* pc, int fromWhere, int operand)
{
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    LLINT_BEGIN();
    dataLogF(
        "<%p> %p / %p: executing bc#%zu, op#%u: Trace(%d): %d\n",
        &Thread::current(),
        exec->codeBlock(),
        exec,
        static_cast<intptr_t>(exec->codeBlock()->bytecodeOffset(pc)),
        pc->opcodeID(),
        fromWhere,
        operand);
    LLINT_END();
}

extern "C" SlowPathReturnType llint_trace_value(ExecState* exec, const Instruction* pc, int fromWhere, VirtualRegister operand)
{
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    JSValue value = LLINT_OP_C(operand).jsValue();
    union {
        struct {
            uint32_t tag;
            uint32_t payload;
        } bits;
        EncodedJSValue asValue;
    } u;
    u.asValue = JSValue::encode(value);
    dataLogF(
        "<%p> %p / %p: executing bc#%zu, op#%u: Trace(%d): %d: %08x:%08x: %s\n",
        &Thread::current(),
        exec->codeBlock(),
        exec,
        static_cast<intptr_t>(exec->codeBlock()->bytecodeOffset(pc)),
        pc->opcodeID(),
        fromWhere,
        operand.offset(),
        u.bits.tag,
        u.bits.payload,
        toCString(value).data());
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_prologue)
{
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    dataLogF("<%p> %p / %p: in prologue of ", &Thread::current(), exec->codeBlock(), exec);
    dataLog(exec->codeBlock(), "\n");
    LLINT_END_IMPL();
}

static void traceFunctionPrologue(ExecState* exec, const char* comment, CodeSpecializationKind kind)
{
    if (!Options::traceLLIntExecution())
        return;

    JSFunction* callee = jsCast<JSFunction*>(exec->jsCallee());
    FunctionExecutable* executable = callee->jsExecutable();
    CodeBlock* codeBlock = executable->codeBlockFor(kind);
    dataLogF("<%p> %p / %p: in %s of ", &Thread::current(), codeBlock, exec, comment);
    dataLog(codeBlock);
    dataLogF(" function %p, executable %p; numVars = %u, numParameters = %u, numCalleeLocals = %u, caller = %p.\n",
        callee, executable, codeBlock->numVars(), codeBlock->numParameters(), codeBlock->numCalleeLocals(), exec->callerFrame());
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
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    OpcodeID opcodeID = pc->opcodeID();
    dataLogF("<%p> %p / %p: executing bc#%zu, %s, pc = %p\n",
            &Thread::current(),
            exec->codeBlock(),
            exec,
            static_cast<intptr_t>(exec->codeBlock()->bytecodeOffset(pc)),
            pc->name(),
            pc);
    if (opcodeID == op_enter) {
        dataLogF("Frame will eventually return to %p\n", exec->returnPC().value());
        *removeCodePtrTag<volatile char*>(exec->returnPC().value());
    }
    if (opcodeID == op_ret) {
        dataLogF("Will be returning to %p\n", exec->returnPC().value());
        dataLogF("The new cfr will be %p\n", exec->callerFrame());
    }
    LLINT_END_IMPL();
}

enum EntryKind { Prologue, ArityCheck };

#if ENABLE(JIT)
static FunctionWhitelist& ensureGlobalJITWhitelist()
{
    static LazyNeverDestroyed<FunctionWhitelist> baselineWhitelist;
    static std::once_flag initializeWhitelistFlag;
    std::call_once(initializeWhitelistFlag, [] {
        const char* functionWhitelistFile = Options::jitWhitelist();
        baselineWhitelist.construct(functionWhitelistFile);
    });
    return baselineWhitelist;
}

inline bool shouldJIT(CodeBlock* codeBlock)
{
    if (!Options::bytecodeRangeToJITCompile().isInRange(codeBlock->instructionCount())
        || !ensureGlobalJITWhitelist().contains(codeBlock))
        return false;

    return VM::canUseJIT() && Options::useBaselineJIT();
}

// Returns true if we should try to OSR.
inline bool jitCompileAndSetHeuristics(CodeBlock* codeBlock, ExecState* exec, unsigned loopOSREntryBytecodeOffset = 0)
{
    VM& vm = exec->vm();
    DeferGCForAWhile deferGC(vm.heap); // My callers don't set top callframe, so we don't want to GC here at all.
    
    codeBlock->updateAllValueProfilePredictions();

    if (!codeBlock->checkIfJITThresholdReached()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayJITCompile", ("threshold not reached, counter = ", codeBlock->llintExecuteCounter()));
        if (Options::verboseOSR())
            dataLogF("    JIT threshold should be lifted.\n");
        return false;
    }
    
    JITWorklist::instance()->poll(vm);
    
    switch (codeBlock->jitType()) {
    case JITCode::BaselineJIT: {
        if (Options::verboseOSR())
            dataLogF("    Code was already compiled.\n");
        codeBlock->jitSoon();
        return true;
    }
    case JITCode::InterpreterThunk: {
        JITWorklist::instance()->compileLater(codeBlock, loopOSREntryBytecodeOffset);
        return codeBlock->jitType() == JITCode::BaselineJIT;
    }
    default:
        dataLog("Unexpected code block in LLInt: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
}

static SlowPathReturnType entryOSR(ExecState* exec, const Instruction*, CodeBlock* codeBlock, const char *name, EntryKind kind)
{
    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered ", name, " with executeCounter = ",
            codeBlock->llintExecuteCounter(), "\n");
    }
    
    if (!shouldJIT(codeBlock)) {
        codeBlock->dontJITAnytimeSoon();
        LLINT_RETURN_TWO(0, 0);
    }
    if (!jitCompileAndSetHeuristics(codeBlock, exec))
        LLINT_RETURN_TWO(0, 0);
    
    CODEBLOCK_LOG_EVENT(codeBlock, "OSR entry", ("in prologue"));
    
    if (kind == Prologue)
        LLINT_RETURN_TWO(codeBlock->jitCode()->executableAddress(), 0);
    ASSERT(kind == ArityCheck);
    LLINT_RETURN_TWO(codeBlock->jitCode()->addressForCall(MustCheckArity).executableAddress(), 0);
}
#else // ENABLE(JIT)
static SlowPathReturnType entryOSR(ExecState* exec, const Instruction*, CodeBlock* codeBlock, const char*, EntryKind)
{
    codeBlock->dontJITAnytimeSoon();
    LLINT_RETURN_TWO(0, exec);
}
#endif // ENABLE(JIT)

LLINT_SLOW_PATH_DECL(entry_osr)
{
    return entryOSR(exec, pc, exec->codeBlock(), "entry_osr", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_call)
{
    return entryOSR(exec, pc, jsCast<JSFunction*>(exec->jsCallee())->jsExecutable()->codeBlockForCall(), "entry_osr_function_for_call", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_construct)
{
    return entryOSR(exec, pc, jsCast<JSFunction*>(exec->jsCallee())->jsExecutable()->codeBlockForConstruct(), "entry_osr_function_for_construct", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_call_arityCheck)
{
    return entryOSR(exec, pc, jsCast<JSFunction*>(exec->jsCallee())->jsExecutable()->codeBlockForCall(), "entry_osr_function_for_call_arityCheck", ArityCheck);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_construct_arityCheck)
{
    return entryOSR(exec, pc, jsCast<JSFunction*>(exec->jsCallee())->jsExecutable()->codeBlockForConstruct(), "entry_osr_function_for_construct_arityCheck", ArityCheck);
}

LLINT_SLOW_PATH_DECL(loop_osr)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(throwScope);
    CodeBlock* codeBlock = exec->codeBlock();

#if ENABLE(JIT)
    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered loop_osr with executeCounter = ",
            codeBlock->llintExecuteCounter(), "\n");
    }
    
    unsigned loopOSREntryBytecodeOffset = codeBlock->bytecodeOffset(pc);

    if (!shouldJIT(codeBlock)) {
        codeBlock->dontJITAnytimeSoon();
        LLINT_RETURN_TWO(0, 0);
    }
    
    if (!jitCompileAndSetHeuristics(codeBlock, exec, loopOSREntryBytecodeOffset))
        LLINT_RETURN_TWO(0, 0);
    
    CODEBLOCK_LOG_EVENT(codeBlock, "osrEntry", ("at bc#", loopOSREntryBytecodeOffset));

    ASSERT(codeBlock->jitType() == JITCode::BaselineJIT);

    const JITCodeMap& codeMap = codeBlock->jitCodeMap();
    CodeLocationLabel<JSEntryPtrTag> codeLocation = codeMap.find(loopOSREntryBytecodeOffset);
    ASSERT(codeLocation);

    void* jumpTarget = codeLocation.executableAddress();
    ASSERT(jumpTarget);
    
    LLINT_RETURN_TWO(jumpTarget, exec->topOfFrame());
#else // ENABLE(JIT)
    UNUSED_PARAM(pc);
    codeBlock->dontJITAnytimeSoon();
    LLINT_RETURN_TWO(0, 0);
#endif // ENABLE(JIT)
}

LLINT_SLOW_PATH_DECL(replace)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(throwScope);
    CodeBlock* codeBlock = exec->codeBlock();

#if ENABLE(JIT)
    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered replace with executeCounter = ",
            codeBlock->llintExecuteCounter(), "\n");
    }
    
    if (shouldJIT(codeBlock))
        jitCompileAndSetHeuristics(codeBlock, exec);
    else
        codeBlock->dontJITAnytimeSoon();
    LLINT_END_IMPL();
#else // ENABLE(JIT)
    codeBlock->dontJITAnytimeSoon();
    LLINT_END_IMPL();
#endif // ENABLE(JIT)
}

LLINT_SLOW_PATH_DECL(stack_check)
{
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    // It's ok to create the NativeCallFrameTracer here before we
    // convertToStackOverflowFrame() because this function is always called
    // after the frame has been propulated with a proper CodeBlock and callee.
    NativeCallFrameTracer tracer(&vm, exec);

    LLINT_SET_PC_FOR_STUBS();

    CodeBlock* codeBlock = exec->codeBlock();
    slowPathLogF("Checking stack height with exec = %p.\n", exec);
    slowPathLog("CodeBlock = ", codeBlock, "\n");
    if (codeBlock) {
        slowPathLogF("Num callee registers = %u.\n", codeBlock->numCalleeLocals());
        slowPathLogF("Num vars = %u.\n", codeBlock->numVars());
    }
    slowPathLogF("Current OS stack end is at %p.\n", vm.softStackLimit());
#if ENABLE(C_LOOP)
    slowPathLogF("Current C Loop stack end is at %p.\n", vm.cloopStackLimit());
#endif

    // If the stack check succeeds and we don't need to throw the error, then
    // we'll return 0 instead. The prologue will check for a non-zero value
    // when determining whether to set the callFrame or not.

    // For JIT enabled builds which uses the C stack, the stack is not growable.
    // Hence, if we get here, then we know a stack overflow is imminent. So, just
    // throw the StackOverflowError unconditionally.
#if ENABLE(C_LOOP)
    Register* topOfFrame = exec->topOfFrame();
    if (LIKELY(topOfFrame < reinterpret_cast<Register*>(exec))) {
        ASSERT(!vm.interpreter->cloopStack().containsAddress(topOfFrame));
        if (LIKELY(vm.ensureStackCapacityFor(topOfFrame)))
            LLINT_RETURN_TWO(pc, 0);
    }
#endif

    exec->convertToStackOverflowFrame(vm);
    ErrorHandlingScope errorScope(vm);
    throwStackOverflowError(exec, throwScope);
    pc = returnToThrow(exec);
    LLINT_RETURN_TWO(pc, exec);
}

LLINT_SLOW_PATH_DECL(slow_path_new_object)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewObject>();
    auto& metadata = bytecode.metadata(exec);
    LLINT_RETURN(constructEmptyObject(exec, metadata.objectAllocationProfile.structure()));
}

LLINT_SLOW_PATH_DECL(slow_path_new_array)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewArray>();
    auto& metadata = bytecode.metadata(exec);
    LLINT_RETURN(constructArrayNegativeIndexed(exec, &metadata.arrayAllocationProfile, bitwise_cast<JSValue*>(&LLINT_OP(bytecode.argv)), bytecode.argc));
}

LLINT_SLOW_PATH_DECL(slow_path_new_array_with_size)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewArrayWithSize>();
    auto& metadata = bytecode.metadata(exec);
    LLINT_RETURN(constructArrayWithSizeQuirk(exec, &metadata.arrayAllocationProfile, exec->lexicalGlobalObject(), LLINT_OP_C(bytecode.length).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_new_regexp)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewRegexp>();
    RegExp* regExp = jsCast<RegExp*>(LLINT_OP_C(bytecode.regexp).jsValue());
    ASSERT(regExp->isValid());
    LLINT_RETURN(RegExpObject::create(vm, exec->lexicalGlobalObject()->regExpStructure(), regExp));
}

LLINT_SLOW_PATH_DECL(slow_path_instanceof)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpInstanceof>();
    JSValue value = LLINT_OP_C(bytecode.value).jsValue();
    JSValue proto = LLINT_OP_C(bytecode.prototype).jsValue();
    LLINT_RETURN(jsBoolean(JSObject::defaultHasInstance(exec, value, proto)));
}

LLINT_SLOW_PATH_DECL(slow_path_instanceof_custom)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpInstanceofCustom>();
    JSValue value = LLINT_OP_C(bytecode.value).jsValue();
    JSValue constructor = LLINT_OP_C(bytecode.constructor).jsValue();
    JSValue hasInstanceValue = LLINT_OP_C(bytecode.hasInstanceValue).jsValue();

    ASSERT(constructor.isObject());
    ASSERT(hasInstanceValue != exec->lexicalGlobalObject()->functionProtoHasInstanceSymbolFunction() || !constructor.getObject()->structure(vm)->typeInfo().implementsDefaultHasInstance());

    JSValue result = jsBoolean(constructor.getObject()->hasInstance(exec, value, hasInstanceValue));
    LLINT_RETURN(result);
}

LLINT_SLOW_PATH_DECL(slow_path_try_get_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpTryGetById>();
    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(bytecode.property);
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    PropertySlot slot(baseValue, PropertySlot::PropertySlot::InternalMethodType::VMInquiry);

    baseValue.getPropertySlot(exec, ident, slot);
    JSValue result = slot.getPureResult();

    LLINT_RETURN_PROFILED(result);
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_id_direct)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetByIdDirect>();
    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(bytecode.property);
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    PropertySlot slot(baseValue, PropertySlot::PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(exec, ident, slot);
    LLINT_CHECK_EXCEPTION();
    JSValue result = found ? slot.getValue(exec, ident) : jsUndefined();
    LLINT_CHECK_EXCEPTION();

    if (!LLINT_ALWAYS_ACCESS_SLOW && slot.isCacheable()) {
        auto& metadata = bytecode.metadata(exec);
        {
            StructureID oldStructureID = metadata.structure;
            if (oldStructureID) {
                Structure* a = vm.heap.structureIDTable().get(oldStructureID);
                Structure* b = baseValue.asCell()->structure(vm);

                if (Structure::shouldConvertToPolyProto(a, b)) {
                    ASSERT(a->rareData()->sharedPolyProtoWatchpoint().get() == b->rareData()->sharedPolyProtoWatchpoint().get());
                    a->rareData()->sharedPolyProtoWatchpoint()->invalidate(vm, StringFireDetail("Detected poly proto opportunity."));
                }
            }
        }

        JSCell* baseCell = baseValue.asCell();
        Structure* structure = baseCell->structure(vm);
        if (slot.isValue()) {
            // Start out by clearing out the old cache.
            metadata.structure = 0;
            metadata.offset = 0;

            if (structure->propertyAccessesAreCacheable()
                && !structure->needImpurePropertyWatchpoint()) {
                vm.heap.writeBarrier(codeBlock);

                ConcurrentJSLocker locker(codeBlock->m_lock);

                metadata.structure = structure->id();
                metadata.offset = slot.cachedOffset();
            }
        }
    }

    LLINT_RETURN_PROFILED(result);
}


static void setupGetByIdPrototypeCache(ExecState* exec, VM& vm, const Instruction* pc, OpGetById::Metadata& metadata, JSCell* baseCell, PropertySlot& slot, const Identifier& ident)
{
    CodeBlock* codeBlock = exec->codeBlock();
    Structure* structure = baseCell->structure(vm);

    if (structure->typeInfo().prohibitsPropertyCaching())
        return;
    
    if (structure->needImpurePropertyWatchpoint())
        return;

    if (structure->isDictionary()) {
        if (structure->hasBeenFlattenedBefore())
            return;
        structure->flattenDictionaryStructure(vm, jsCast<JSObject*>(baseCell));
    }

    ObjectPropertyConditionSet conditions;
    if (slot.isUnset())
        conditions = generateConditionsForPropertyMiss(vm, codeBlock, exec, structure, ident.impl());
    else
        conditions = generateConditionsForPrototypePropertyHit(vm, codeBlock, exec, structure, slot.slotBase(), ident.impl());

    if (!conditions.isValid())
        return;

    PropertyOffset offset = invalidOffset;
    CodeBlock::StructureWatchpointMap& watchpointMap = codeBlock->llintGetByIdWatchpointMap();
    Bag<LLIntPrototypeLoadAdaptiveStructureWatchpoint> watchpoints;
    for (ObjectPropertyCondition condition : conditions) {
        if (!condition.isWatchable())
            return;
        if (condition.condition().kind() == PropertyCondition::Presence)
            offset = condition.condition().offset();
        watchpoints.add(condition, metadata)->install(vm);
    }

    ASSERT((offset == invalidOffset) == slot.isUnset());
    auto result = watchpointMap.add(std::make_tuple(structure, pc), WTFMove(watchpoints));
    ASSERT_UNUSED(result, result.isNewEntry);

    ConcurrentJSLocker locker(codeBlock->m_lock);

    if (slot.isUnset()) {
        metadata.mode = GetByIdMode::Unset;
        metadata.modeMetadata.unsetMode.structure = structure->id();
        return;
    }
    ASSERT(slot.isValue());

    metadata.mode = GetByIdMode::ProtoLoad;
    metadata.modeMetadata.protoLoadMode.structure = structure->id();
    metadata.modeMetadata.protoLoadMode.cachedOffset = offset;
    metadata.modeMetadata.protoLoadMode.cachedSlot = slot.slotBase();
    // We know that this pointer will remain valid because it will be cleared by either a watchpoint fire or
    // during GC when we clear the LLInt caches.
    metadata.modeMetadata.protoLoadMode.cachedSlot = slot.slotBase();
}


LLINT_SLOW_PATH_DECL(slow_path_get_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetById>();
    auto& metadata = bytecode.metadata(exec);
    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(bytecode.property);
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    PropertySlot slot(baseValue, PropertySlot::PropertySlot::InternalMethodType::Get);

    JSValue result = baseValue.get(exec, ident, slot);
    LLINT_CHECK_EXCEPTION();
    LLINT_OP(bytecode.dst) = result;
    
    if (!LLINT_ALWAYS_ACCESS_SLOW
        && baseValue.isCell()
        && slot.isCacheable()) {
        {
            StructureID oldStructureID;
            auto mode = metadata.mode;
            switch (mode) {
            case GetByIdMode::Default:
                oldStructureID = metadata.modeMetadata.defaultMode.structure;
                break;
            case GetByIdMode::Unset:
                oldStructureID = metadata.modeMetadata.unsetMode.structure;
                break;
            case GetByIdMode::ProtoLoad:
                oldStructureID = metadata.modeMetadata.protoLoadMode.structure;
                break;
            default:
                oldStructureID = 0;
            }
            if (oldStructureID) {
                Structure* a = vm.heap.structureIDTable().get(oldStructureID);
                Structure* b = baseValue.asCell()->structure(vm);

                if (Structure::shouldConvertToPolyProto(a, b)) {
                    ASSERT(a->rareData()->sharedPolyProtoWatchpoint().get() == b->rareData()->sharedPolyProtoWatchpoint().get());
                    a->rareData()->sharedPolyProtoWatchpoint()->invalidate(vm, StringFireDetail("Detected poly proto opportunity."));
                }
            }
        }

        JSCell* baseCell = baseValue.asCell();
        Structure* structure = baseCell->structure(vm);
        if (slot.isValue() && slot.slotBase() == baseValue) {
            // Start out by clearing out the old cache.
            metadata.mode = GetByIdMode::Default;
            metadata.modeMetadata.defaultMode.structure = 0;
            metadata.modeMetadata.defaultMode.cachedOffset = 0;

            // Prevent the prototype cache from ever happening.
            metadata.hitCountForLLIntCaching = 0;
        
            if (structure->propertyAccessesAreCacheable()
                && !structure->needImpurePropertyWatchpoint()) {
                vm.heap.writeBarrier(codeBlock);
                
                ConcurrentJSLocker locker(codeBlock->m_lock);

                metadata.modeMetadata.defaultMode.structure = structure->id();
                metadata.modeMetadata.defaultMode.cachedOffset = slot.cachedOffset();
            }
        } else if (UNLIKELY(metadata.hitCountForLLIntCaching && (slot.isValue() || slot.isUnset()))) {
            ASSERT(slot.slotBase() != baseValue);

            if (!(--metadata.hitCountForLLIntCaching))
                setupGetByIdPrototypeCache(exec, vm, pc, metadata, baseCell, slot, ident);
        }
    } else if (!LLINT_ALWAYS_ACCESS_SLOW
        && isJSArray(baseValue)
        && ident == vm.propertyNames->length) {
        metadata.mode = GetByIdMode::ArrayLength;
        metadata.modeMetadata.arrayLengthMode.arrayProfile.observeStructure(baseValue.asCell()->structure(vm));

        // Prevent the prototype cache from ever happening.
        metadata.hitCountForLLIntCaching = 0;
    }

    LLINT_PROFILE_VALUE(result);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutById>();
    auto& metadata = bytecode.metadata(exec);
    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(bytecode.property);
    
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    PutPropertySlot slot(baseValue, codeBlock->isStrictMode(), codeBlock->putByIdContext());
    if (metadata.flags & PutByIdIsDirect)
        CommonSlowPaths::putDirectWithReify(vm, exec, asObject(baseValue), ident, LLINT_OP_C(bytecode.value).jsValue(), slot);
    else
        baseValue.putInline(exec, ident, LLINT_OP_C(bytecode.value).jsValue(), slot);
    LLINT_CHECK_EXCEPTION();
    
    if (!LLINT_ALWAYS_ACCESS_SLOW
        && baseValue.isCell()
        && slot.isCacheablePut()) {

        {
            StructureID oldStructureID = metadata.oldStructure;
            if (oldStructureID) {
                Structure* a = vm.heap.structureIDTable().get(oldStructureID);
                Structure* b = baseValue.asCell()->structure(vm);
                if (slot.type() == PutPropertySlot::NewProperty)
                    b = b->previousID();

                if (Structure::shouldConvertToPolyProto(a, b)) {
                    a->rareData()->sharedPolyProtoWatchpoint()->invalidate(vm, StringFireDetail("Detected poly proto opportunity."));
                    b->rareData()->sharedPolyProtoWatchpoint()->invalidate(vm, StringFireDetail("Detected poly proto opportunity."));
                }
            }
        }

        // Start out by clearing out the old cache.
        metadata.oldStructure = 0;
        metadata.offset = 0;
        metadata.newStructure = 0;
        metadata.structureChain.clear();
        metadata.flags = static_cast<PutByIdFlags>(metadata.flags & PutByIdPersistentFlagsMask);
        
        JSCell* baseCell = baseValue.asCell();
        Structure* structure = baseCell->structure(vm);
        
        if (!structure->isUncacheableDictionary()
            && !structure->typeInfo().prohibitsPropertyCaching()
            && baseCell == slot.base()) {

            vm.heap.writeBarrier(codeBlock);
            
            if (slot.type() == PutPropertySlot::NewProperty) {
                GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
            
                if (!structure->isDictionary() && structure->previousID()->outOfLineCapacity() == structure->outOfLineCapacity()) {
                    ASSERT(structure->previousID()->transitionWatchpointSetHasBeenInvalidated());

                    bool sawPolyProto = false;
                    auto result = normalizePrototypeChain(exec, baseCell, sawPolyProto);
                    if (result != InvalidPrototypeChain && !sawPolyProto) {
                        ASSERT(structure->previousID()->isObject());
                        metadata.oldStructure = structure->previousID()->id();
                        metadata.offset = slot.cachedOffset();
                        metadata.newStructure = structure->id();
                        if (!(metadata.flags & PutByIdIsDirect)) {
                            StructureChain* chain = structure->prototypeChain(exec, asObject(baseCell));
                            ASSERT(chain);
                            metadata.structureChain.set(vm, codeBlock, chain);
                        }
                        metadata.flags = static_cast<PutByIdFlags>(
                            metadata.flags |
                            structure->inferredTypeDescriptorFor(ident.impl()).putByIdFlags());
                    }
                }
            } else {
                structure->didCachePropertyReplacement(vm, slot.cachedOffset());
                metadata.oldStructure = structure->id();
                metadata.offset = slot.cachedOffset();
                metadata.flags = static_cast<PutByIdFlags>(
                    metadata.flags |
                    structure->inferredTypeDescriptorFor(ident.impl()).putByIdFlags());
            }
        }
    }
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_del_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpDelById>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSObject* baseObject = LLINT_OP_C(bytecode.base).jsValue().toObject(exec);
    LLINT_CHECK_EXCEPTION();
    bool couldDelete = baseObject->methodTable(vm)->deleteProperty(baseObject, exec, codeBlock->identifier(bytecode.property));
    LLINT_CHECK_EXCEPTION();
    if (!couldDelete && codeBlock->isStrictMode())
        LLINT_THROW(createTypeError(exec, UnableToDeletePropertyError));
    LLINT_RETURN(jsBoolean(couldDelete));
}

static ALWAYS_INLINE JSValue getByVal(VM& vm, ExecState* exec, OpGetByVal bytecode)
{
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    JSValue subscript = LLINT_OP_C(bytecode.property).jsValue();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            if (RefPtr<AtomicStringImpl> existingAtomicString = asString(subscript)->toExistingAtomicString(exec)) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomicString.get()))
                    return result;
            }
        }
    }
    
    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        auto& metadata = bytecode.metadata(exec);
        ArrayProfile* arrayProfile = &metadata.arrayProfile;

        if (isJSString(baseValue)) {
            if (asString(baseValue)->canGetIndex(i)) {
                scope.release();
                return asString(baseValue)->getIndex(exec, i);
            }
            arrayProfile->setOutOfBounds();
        } else if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canGetIndexQuickly(i))
                return object->getIndexQuickly(i);

            bool skipMarkingOutOfBounds = false;

            if (object->indexingType() == ArrayWithContiguous && i < object->butterfly()->publicLength()) {
                // FIXME: expand this to ArrayStorage, Int32, and maybe Double:
                // https://bugs.webkit.org/show_bug.cgi?id=182940
                auto* globalObject = object->globalObject(vm);
                skipMarkingOutOfBounds = globalObject->isOriginalArrayStructure(object->structure(vm)) && globalObject->arrayPrototypeChainIsSane();
            }

            if (!skipMarkingOutOfBounds && !CommonSlowPaths::canAccessArgumentIndexQuickly(*object, i))
                arrayProfile->setOutOfBounds();
        }

        scope.release();
        return baseValue.get(exec, i);
    }

    baseValue.requireObjectCoercible(exec);
    RETURN_IF_EXCEPTION(scope, JSValue());
    auto property = subscript.toPropertyKey(exec);
    RETURN_IF_EXCEPTION(scope, JSValue());
    scope.release();
    return baseValue.get(exec, property);
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetByVal>();
    LLINT_RETURN_PROFILED(getByVal(vm, exec, bytecode));
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_val)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpPutByVal>();
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    JSValue subscript = LLINT_OP_C(bytecode.property).jsValue();
    JSValue value = LLINT_OP_C(bytecode.value).jsValue();
    bool isStrictMode = exec->codeBlock()->isStrictMode();
    
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canSetIndexQuickly(i))
                object->setIndexQuickly(vm, i, value);
            else
                object->methodTable(vm)->putByIndex(object, exec, i, value, isStrictMode);
            LLINT_END();
        }
        baseValue.putByIndex(exec, i, value, isStrictMode);
        LLINT_END();
    }

    auto property = subscript.toPropertyKey(exec);
    LLINT_CHECK_EXCEPTION();
    PutPropertySlot slot(baseValue, isStrictMode);
    baseValue.put(exec, property, value, slot);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_val_direct)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpPutByValDirect>();
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    JSValue subscript = LLINT_OP_C(bytecode.property).jsValue();
    JSValue value = LLINT_OP_C(bytecode.value).jsValue();
    RELEASE_ASSERT(baseValue.isObject());
    JSObject* baseObject = asObject(baseValue);
    bool isStrictMode = exec->codeBlock()->isStrictMode();
    if (LIKELY(subscript.isUInt32())) {
        // Despite its name, JSValue::isUInt32 will return true only for positive boxed int32_t; all those values are valid array indices.
        ASSERT(isIndex(subscript.asUInt32()));
        baseObject->putDirectIndex(exec, subscript.asUInt32(), value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        LLINT_END();
    }

    if (subscript.isDouble()) {
        double subscriptAsDouble = subscript.asDouble();
        uint32_t subscriptAsUInt32 = static_cast<uint32_t>(subscriptAsDouble);
        if (subscriptAsDouble == subscriptAsUInt32 && isIndex(subscriptAsUInt32)) {
            baseObject->putDirectIndex(exec, subscriptAsUInt32, value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
            LLINT_END();
        }
    }

    // Don't put to an object if toString threw an exception.
    auto property = subscript.toPropertyKey(exec);
    if (UNLIKELY(throwScope.exception()))
        LLINT_END();

    if (std::optional<uint32_t> index = parseIndex(property))
        baseObject->putDirectIndex(exec, index.value(), value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
    else {
        PutPropertySlot slot(baseObject, isStrictMode);
        CommonSlowPaths::putDirectWithReify(vm, exec, baseObject, property, value, slot);
    }
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_del_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpDelByVal>();
    JSValue baseValue = LLINT_OP_C(bytecode.base).jsValue();
    JSObject* baseObject = baseValue.toObject(exec);
    LLINT_CHECK_EXCEPTION();

    JSValue subscript = LLINT_OP_C(bytecode.property).jsValue();
    
    bool couldDelete;
    
    uint32_t i;
    if (subscript.getUInt32(i))
        couldDelete = baseObject->methodTable(vm)->deletePropertyByIndex(baseObject, exec, i);
    else {
        LLINT_CHECK_EXCEPTION();
        auto property = subscript.toPropertyKey(exec);
        LLINT_CHECK_EXCEPTION();
        couldDelete = baseObject->methodTable(vm)->deleteProperty(baseObject, exec, property);
    }
    LLINT_CHECK_EXCEPTION();

    if (!couldDelete && exec->codeBlock()->isStrictMode())
        LLINT_THROW(createTypeError(exec, UnableToDeletePropertyError));
    
    LLINT_RETURN(jsBoolean(couldDelete));
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutGetterById>();
    ASSERT(LLINT_OP(bytecode.base).jsValue().isObject());
    JSObject* baseObj = asObject(LLINT_OP(bytecode.base).jsValue());

    unsigned options = bytecode.attributes;

    JSValue getter = LLINT_OP(bytecode.accessor).jsValue();
    ASSERT(getter.isObject());

    baseObj->putGetter(exec, exec->codeBlock()->identifier(bytecode.property), asObject(getter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_setter_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutSetterById>();
    ASSERT(LLINT_OP(bytecode.base).jsValue().isObject());
    JSObject* baseObj = asObject(LLINT_OP(bytecode.base).jsValue());

    unsigned options = bytecode.attributes;

    JSValue setter = LLINT_OP(bytecode.accessor).jsValue();
    ASSERT(setter.isObject());

    baseObj->putSetter(exec, exec->codeBlock()->identifier(bytecode.property), asObject(setter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_setter_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutGetterSetterById>();
    ASSERT(LLINT_OP(bytecode.base).jsValue().isObject());
    JSObject* baseObject = asObject(LLINT_OP(bytecode.base).jsValue());

    JSValue getter = LLINT_OP(bytecode.getter).jsValue();
    JSValue setter = LLINT_OP(bytecode.setter).jsValue();
    ASSERT(getter.isObject() || setter.isObject());
    GetterSetter* accessor = GetterSetter::create(vm, exec->lexicalGlobalObject(), getter, setter);

    CommonSlowPaths::putDirectAccessorWithReify(vm, exec, baseObject, exec->codeBlock()->identifier(bytecode.property), accessor, bytecode.attributes);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutGetterByVal>();
    ASSERT(LLINT_OP(bytecode.base).jsValue().isObject());
    JSObject* baseObj = asObject(LLINT_OP(bytecode.base).jsValue());
    JSValue subscript = LLINT_OP_C(bytecode.property).jsValue();

    unsigned options = bytecode.attributes;

    JSValue getter = LLINT_OP(bytecode.accessor).jsValue();
    ASSERT(getter.isObject());

    auto property = subscript.toPropertyKey(exec);
    LLINT_CHECK_EXCEPTION();

    baseObj->putGetter(exec, property, asObject(getter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_setter_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutSetterByVal>();
    ASSERT(LLINT_OP(bytecode.base).jsValue().isObject());
    JSObject* baseObj = asObject(LLINT_OP(bytecode.base).jsValue());
    JSValue subscript = LLINT_OP_C(bytecode.property).jsValue();

    unsigned options = bytecode.attributes;

    JSValue setter = LLINT_OP(bytecode.accessor).jsValue();
    ASSERT(setter.isObject());

    auto property = subscript.toPropertyKey(exec);
    LLINT_CHECK_EXCEPTION();

    baseObj->putSetter(exec, property, asObject(setter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_jtrue)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJtrue>();
    LLINT_BRANCH(LLINT_OP_C(bytecode.condition).jsValue().toBoolean(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_jfalse)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJfalse>();
    LLINT_BRANCH(!LLINT_OP_C(bytecode.condition).jsValue().toBoolean(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_jless)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJless>();
    LLINT_BRANCH(jsLess<true>(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jnless)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJnless>();
    LLINT_BRANCH(!jsLess<true>(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jgreater)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJgreater>();
    LLINT_BRANCH(jsLess<false>(exec, LLINT_OP_C(bytecode.rhs).jsValue(), LLINT_OP_C(bytecode.lhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jngreater)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJngreater>();
    LLINT_BRANCH(!jsLess<false>(exec, LLINT_OP_C(bytecode.rhs).jsValue(), LLINT_OP_C(bytecode.lhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jlesseq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJlesseq>();
    LLINT_BRANCH(jsLessEq<true>(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jnlesseq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJnlesseq>();
    LLINT_BRANCH(!jsLessEq<true>(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jgreatereq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJgreatereq>();
    LLINT_BRANCH(jsLessEq<false>(exec, LLINT_OP_C(bytecode.rhs).jsValue(), LLINT_OP_C(bytecode.lhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jngreatereq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJngreatereq>();
    LLINT_BRANCH(!jsLessEq<false>(exec, LLINT_OP_C(bytecode.rhs).jsValue(), LLINT_OP_C(bytecode.lhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jeq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJeq>();
    LLINT_BRANCH(JSValue::equal(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jneq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJneq>();
    LLINT_BRANCH(!JSValue::equal(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jstricteq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJstricteq>();
    LLINT_BRANCH(JSValue::strictEqual(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_jnstricteq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJnstricteq>();
    LLINT_BRANCH(!JSValue::strictEqual(exec, LLINT_OP_C(bytecode.lhs).jsValue(), LLINT_OP_C(bytecode.rhs).jsValue()));
}

LLINT_SLOW_PATH_DECL(slow_path_switch_imm)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSwitchImm>();
    JSValue scrutinee = LLINT_OP_C(bytecode.scrutinee).jsValue();
    ASSERT(scrutinee.isDouble());
    double value = scrutinee.asDouble();
    int32_t intValue = static_cast<int32_t>(value);
    int defaultOffset = JUMP_OFFSET(bytecode.defaultOffset);
    if (value == intValue) {
        CodeBlock* codeBlock = exec->codeBlock();
        JUMP_TO(codeBlock->switchJumpTable(bytecode.tableIndex).offsetForValue(intValue, defaultOffset));
    } else
        JUMP_TO(defaultOffset);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_switch_char)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSwitchChar>();
    JSValue scrutinee = LLINT_OP_C(bytecode.scrutinee).jsValue();
    ASSERT(scrutinee.isString());
    JSString* string = asString(scrutinee);
    ASSERT(string->length() == 1);
    int defaultOffset = JUMP_OFFSET(bytecode.defaultOffset);
    StringImpl* impl = string->value(exec).impl();
    CodeBlock* codeBlock = exec->codeBlock();
    JUMP_TO(codeBlock->switchJumpTable(bytecode.tableIndex).offsetForValue((*impl)[0], defaultOffset));
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_switch_string)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSwitchString>();
    JSValue scrutinee = LLINT_OP_C(bytecode.scrutinee).jsValue();
    int defaultOffset = JUMP_OFFSET(bytecode.defaultOffset);
    if (!scrutinee.isString())
        JUMP_TO(defaultOffset);
    else {
        CodeBlock* codeBlock = exec->codeBlock();
        JUMP_TO(codeBlock->stringSwitchJumpTable(bytecode.tableIndex).offsetForValue(asString(scrutinee)->value(exec).impl(), defaultOffset));
    }
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_new_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewFunc>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    slowPathLogF("Creating function!\n");
    LLINT_RETURN(JSFunction::create(vm, codeBlock->functionDecl(bytecode.functionDecl), scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_generator_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewGeneratorFunc>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    slowPathLogF("Creating function!\n");
    LLINT_RETURN(JSGeneratorFunction::create(vm, codeBlock->functionDecl(bytecode.functionDecl), scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_async_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewAsyncFunc>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    slowPathLogF("Creating async function!\n");
    LLINT_RETURN(JSAsyncFunction::create(vm, codeBlock->functionDecl(bytecode.functionDecl), scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_async_generator_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewAsyncGeneratorFunc>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    slowPathLogF("Creating async generator function!\n");
    LLINT_RETURN(JSAsyncGeneratorFunction::create(vm, codeBlock->functionDecl(bytecode.functionDecl), scope));
}
    
LLINT_SLOW_PATH_DECL(slow_path_new_func_exp)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpNewFuncExp>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.functionDecl);
    
    LLINT_RETURN(JSFunction::create(vm, executable, scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_generator_func_exp)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpNewGeneratorFuncExp>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.functionDecl);

    LLINT_RETURN(JSGeneratorFunction::create(vm, executable, scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_async_func_exp)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpNewAsyncFuncExp>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.functionDecl);
    
    LLINT_RETURN(JSAsyncFunction::create(vm, executable, scope));
}
    
LLINT_SLOW_PATH_DECL(slow_path_new_async_generator_func_exp)
{
    LLINT_BEGIN();
        
    auto bytecode = pc->as<OpNewAsyncGeneratorFuncExp>();
    CodeBlock* codeBlock = exec->codeBlock();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.functionDecl);
        
    LLINT_RETURN(JSAsyncGeneratorFunction::create(vm, executable, scope));
}

LLINT_SLOW_PATH_DECL(slow_path_set_function_name)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSetFunctionName>();
    JSFunction* func = jsCast<JSFunction*>(LLINT_OP(bytecode.function).Register::unboxedCell());
    JSValue name = LLINT_OP_C(bytecode.name).Register::jsValue();
    func->setFunctionName(exec, name);
    LLINT_END();
}

static SlowPathReturnType handleHostCall(ExecState* execCallee, JSValue callee, CodeSpecializationKind kind)
{
    slowPathLog("Performing host call.\n");
    
    ExecState* exec = execCallee->callerFrame();
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    execCallee->setCodeBlock(0);
    execCallee->clearReturnPC();

    if (kind == CodeForCall) {
        CallData callData;
        CallType callType = getCallData(vm, callee, callData);
    
        ASSERT(callType != CallType::JS);
    
        if (callType == CallType::Host) {
            NativeCallFrameTracer tracer(&vm, execCallee);
            execCallee->setCallee(asObject(callee));
            vm.hostCallReturnValue = JSValue::decode(callData.native.function(execCallee));
            
            PoisonedMasmPtr::assertIsNotPoisoned(LLInt::getCodePtr(getHostCallReturnValue));
            LLINT_CALL_RETURN(execCallee, execCallee, LLInt::getCodePtr(getHostCallReturnValue), CFunctionPtrTag);
        }
        
        slowPathLog("Call callee is not a function: ", callee, "\n");

        ASSERT(callType == CallType::None);
        LLINT_CALL_THROW(exec, createNotAFunctionError(exec, callee));
    }

    ASSERT(kind == CodeForConstruct);
    
    ConstructData constructData;
    ConstructType constructType = getConstructData(vm, callee, constructData);
    
    ASSERT(constructType != ConstructType::JS);
    
    if (constructType == ConstructType::Host) {
        NativeCallFrameTracer tracer(&vm, execCallee);
        execCallee->setCallee(asObject(callee));
        vm.hostCallReturnValue = JSValue::decode(constructData.native.function(execCallee));

        PoisonedMasmPtr::assertIsNotPoisoned(LLInt::getCodePtr(getHostCallReturnValue));
        LLINT_CALL_RETURN(execCallee, execCallee, LLInt::getCodePtr(getHostCallReturnValue), CFunctionPtrTag);
    }
    
    slowPathLog("Constructor callee is not a function: ", callee, "\n");

    ASSERT(constructType == ConstructType::None);
    LLINT_CALL_THROW(exec, createNotAConstructorError(exec, callee));
}

inline SlowPathReturnType setUpCall(ExecState* execCallee, CodeSpecializationKind kind, JSValue calleeAsValue, LLIntCallLinkInfo* callLinkInfo = nullptr)
{
    ExecState* exec = execCallee->callerFrame();
    VM& vm = exec->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    slowPathLogF("Performing call with recorded PC = %p\n", exec->currentVPC());
    
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell) {
        if (auto* internalFunction = jsDynamicCast<InternalFunction*>(vm, calleeAsValue)) {
            MacroAssemblerCodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            ASSERT(!!codePtr);

            if (!LLINT_ALWAYS_ACCESS_SLOW && callLinkInfo) {
                CodeBlock* callerCodeBlock = exec->codeBlock();

                ConcurrentJSLocker locker(callerCodeBlock->m_lock);

                if (callLinkInfo->isOnList())
                    callLinkInfo->remove();
                callLinkInfo->callee.set(vm, callerCodeBlock, internalFunction);
                callLinkInfo->lastSeenCallee.set(vm, callerCodeBlock, internalFunction);
                callLinkInfo->machineCodeTarget = codePtr;
            }

            assertIsTaggedWith(codePtr.executableAddress(), JSEntryPtrTag);
            PoisonedMasmPtr::assertIsNotPoisoned(codePtr.executableAddress());
            LLINT_CALL_RETURN(exec, execCallee, codePtr.executableAddress(), JSEntryPtrTag);
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(execCallee, calleeAsValue, kind));
    }
    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
    CodeBlock* codeBlock = 0;
    if (executable->isHostFunction())
        codePtr = executable->entrypointFor(kind, MustCheckArity);
    else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct)
            LLINT_CALL_THROW(exec, createNotAConstructorError(exec, callee));

        CodeBlock** codeBlockSlot = execCallee->addressOfCodeBlock();
        JSObject* error = functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, scope, kind, *codeBlockSlot);
        EXCEPTION_ASSERT(throwScope.exception() == error);
        if (UNLIKELY(error))
            LLINT_CALL_THROW(exec, error);
        codeBlock = *codeBlockSlot;
        ASSERT(codeBlock);
        ArityCheckMode arity;
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()))
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = functionExecutable->entrypointFor(kind, arity);
    }

    ASSERT(!!codePtr);
    
    if (!LLINT_ALWAYS_ACCESS_SLOW && callLinkInfo) {
        CodeBlock* callerCodeBlock = exec->codeBlock();

        ConcurrentJSLocker locker(callerCodeBlock->m_lock);
        
        if (callLinkInfo->isOnList())
            callLinkInfo->remove();
        callLinkInfo->callee.set(vm, callerCodeBlock, callee);
        callLinkInfo->lastSeenCallee.set(vm, callerCodeBlock, callee);
        callLinkInfo->machineCodeTarget = codePtr;
        if (codeBlock)
            codeBlock->linkIncomingCall(exec, callLinkInfo);
    }

    assertIsTaggedWith(codePtr.executableAddress(), JSEntryPtrTag);
    PoisonedMasmPtr::assertIsNotPoisoned(codePtr.executableAddress());
    LLINT_CALL_RETURN(exec, execCallee, codePtr.executableAddress(), JSEntryPtrTag);
}

template<typename Op>
inline SlowPathReturnType genericCall(ExecState* exec, Op&& bytecode, CodeSpecializationKind kind)
{
    // This needs to:
    // - Set up a call frame.
    // - Figure out what to call and compile it if necessary.
    // - If possible, link the call's inline cache.
    // - Return a tuple of machine code address to call and the new call frame.
    
    JSValue calleeAsValue = LLINT_OP_C(bytecode.callee).jsValue();
    
    ExecState* execCallee = exec - bytecode.argv;
    
    execCallee->setArgumentCountIncludingThis(bytecode.argc);
    execCallee->uncheckedR(CallFrameSlot::callee) = calleeAsValue;
    execCallee->setCallerFrame(exec);
    
    auto& metadata = bytecode.metadata(exec);
    return setUpCall(execCallee, kind, calleeAsValue, &metadata.callLinkInfo);
}

LLINT_SLOW_PATH_DECL(slow_path_call)
{
    LLINT_BEGIN_NO_SET_PC();
    RELEASE_AND_RETURN(throwScope, genericCall(exec, pc->as<OpCall>(), CodeForCall));
}

LLINT_SLOW_PATH_DECL(slow_path_tail_call)
{
    LLINT_BEGIN_NO_SET_PC();
    RELEASE_AND_RETURN(throwScope, genericCall(exec, pc->as<OpTailCall>(), CodeForCall));
}

LLINT_SLOW_PATH_DECL(slow_path_construct)
{
    LLINT_BEGIN_NO_SET_PC();
    RELEASE_AND_RETURN(throwScope, genericCall(exec, pc->as<OpConstruct>(), CodeForConstruct));
}

LLINT_SLOW_PATH_DECL(slow_path_size_frame_for_varargs)
{
    LLINT_BEGIN();
    // This needs to:
    // - Set up a call frame while respecting the variable arguments.
    
    unsigned numUsedStackSlots;
    JSValue arguments;
    int firstVarArg;
    switch (pc->opcodeID()) {
    case op_call_varargs: {
        auto bytecode = pc->as<OpCallVarargs>();
        numUsedStackSlots = -bytecode.firstFree.offset();
        arguments = LLINT_OP_C(bytecode.arguments).jsValue();
        firstVarArg = bytecode.firstVarArg;
        break;
    }
    case op_tail_call_varargs: {
        auto bytecode = pc->as<OpTailCallVarargs>();
        numUsedStackSlots = -bytecode.firstFree.offset();
        arguments = LLINT_OP_C(bytecode.arguments).jsValue();
        firstVarArg = bytecode.firstVarArg;
        break;
    }
    case op_construct_varargs: {
        auto bytecode = pc->as<OpConstructVarargs>();
        numUsedStackSlots = -bytecode.firstFree.offset();
        arguments = LLINT_OP_C(bytecode.arguments).jsValue();
        firstVarArg = bytecode.firstVarArg;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    unsigned length = sizeFrameForVarargs(exec, vm, arguments, numUsedStackSlots, firstVarArg);
    LLINT_CALL_CHECK_EXCEPTION(exec, exec);
    
    ExecState* execCallee = calleeFrameForVarargs(exec, numUsedStackSlots, length + 1);
    vm.varargsLength = length;
    vm.newCallFrameReturnValue = execCallee;

    LLINT_RETURN_CALLEE_FRAME(execCallee);
}

LLINT_SLOW_PATH_DECL(slow_path_size_frame_for_forward_arguments)
{
    LLINT_BEGIN();
    // This needs to:
    // - Set up a call frame with the same arguments as the current frame.

    auto bytecode = pc->as<OpTailCallForwardArguments>();
    unsigned numUsedStackSlots = -bytecode.firstFree.offset();

    unsigned arguments = sizeFrameForForwardArguments(exec, vm, numUsedStackSlots);
    LLINT_CALL_CHECK_EXCEPTION(exec, exec);

    ExecState* execCallee = calleeFrameForVarargs(exec, numUsedStackSlots, arguments + 1);

    vm.varargsLength = arguments;
    vm.newCallFrameReturnValue = execCallee;

    LLINT_RETURN_CALLEE_FRAME(execCallee);
}

enum class SetArgumentsWith {
    Object,
    CurrentArguments
};

template<typename Op>
inline SlowPathReturnType varargsSetup(ExecState* exec, const Instruction* pc, CodeSpecializationKind kind, SetArgumentsWith set)
{
    LLINT_BEGIN_NO_SET_PC();
    // This needs to:
    // - Figure out what to call and compile it if necessary.
    // - Return a tuple of machine code address to call and the new call frame.

    auto bytecode = pc->as<Op>();
    JSValue calleeAsValue = LLINT_OP_C(bytecode.callee).jsValue();

    ExecState* execCallee = vm.newCallFrameReturnValue;

    if (set == SetArgumentsWith::Object) {
        setupVarargsFrameAndSetThis(exec, execCallee, LLINT_OP_C(bytecode.thisValue).jsValue(), LLINT_OP_C(bytecode.arguments).jsValue(), bytecode.firstVarArg, vm.varargsLength);
        LLINT_CALL_CHECK_EXCEPTION(exec, exec);
    } else
        setupForwardArgumentsFrameAndSetThis(exec, execCallee, LLINT_OP_C(bytecode.thisValue).jsValue(), vm.varargsLength);

    execCallee->setCallerFrame(exec);
    execCallee->uncheckedR(CallFrameSlot::callee) = calleeAsValue;
    exec->setCurrentVPC(pc);

    RELEASE_AND_RETURN(throwScope, setUpCall(execCallee, kind, calleeAsValue));
}

LLINT_SLOW_PATH_DECL(slow_path_call_varargs)
{
    return varargsSetup<OpCallVarargs>(exec, pc, CodeForCall, SetArgumentsWith::Object);
}

LLINT_SLOW_PATH_DECL(slow_path_tail_call_varargs)
{
    return varargsSetup<OpTailCallVarargs>(exec, pc, CodeForCall, SetArgumentsWith::Object);
}

LLINT_SLOW_PATH_DECL(slow_path_tail_call_forward_arguments)
{
    return varargsSetup<OpTailCallForwardArguments>(exec, pc, CodeForCall, SetArgumentsWith::CurrentArguments);
}

LLINT_SLOW_PATH_DECL(slow_path_construct_varargs)
{
    return varargsSetup<OpConstructVarargs>(exec, pc, CodeForConstruct, SetArgumentsWith::Object);
}

inline SlowPathReturnType commonCallEval(ExecState* exec, const Instruction* pc, MacroAssemblerCodePtr<JSEntryPtrTag> returnPoint)
{
    LLINT_BEGIN_NO_SET_PC();
    auto bytecode = pc->as<OpCallEval>();
    JSValue calleeAsValue = LLINT_OP(bytecode.callee).jsValue();
    
    ExecState* execCallee = exec - bytecode.argv;
    
    execCallee->setArgumentCountIncludingThis(bytecode.argc);
    execCallee->setCallerFrame(exec);
    execCallee->uncheckedR(CallFrameSlot::callee) = calleeAsValue;
    execCallee->setReturnPC(returnPoint.executableAddress());
    execCallee->setCodeBlock(0);
    exec->setCurrentVPC(pc);
    
    if (!isHostFunction(calleeAsValue, globalFuncEval))
        RELEASE_AND_RETURN(throwScope, setUpCall(execCallee, CodeForCall, calleeAsValue));
    
    vm.hostCallReturnValue = eval(execCallee);
    LLINT_CALL_RETURN(exec, execCallee, LLInt::getCodePtr(getHostCallReturnValue), CFunctionPtrTag);
}
    
LLINT_SLOW_PATH_DECL(slow_path_call_eval)
{
    return commonCallEval(exec, pc, LLInt::getCodePtr<JSEntryPtrTag>(llint_generic_return_point));
}

LLINT_SLOW_PATH_DECL(slow_path_call_eval_wide)
{
    return commonCallEval(exec, pc, LLInt::getWideCodePtr<JSEntryPtrTag>(llint_generic_return_point));
}

LLINT_SLOW_PATH_DECL(slow_path_strcat)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpStrcat>();
    LLINT_RETURN(jsStringFromRegisterArray(exec, &LLINT_OP(bytecode.src), bytecode.count));
}

LLINT_SLOW_PATH_DECL(slow_path_to_primitive)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpToPrimitive>();
    LLINT_RETURN(LLINT_OP_C(bytecode.src).jsValue().toPrimitive(exec));
}

LLINT_SLOW_PATH_DECL(slow_path_throw)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpThrow>();
    LLINT_THROW(LLINT_OP_C(bytecode.value).jsValue());
}

LLINT_SLOW_PATH_DECL(slow_path_handle_traps)
{
    LLINT_BEGIN_NO_SET_PC();
    ASSERT(vm.needTrapHandling());
    vm.handleTraps(exec);
    UNUSED_PARAM(pc);
    LLINT_RETURN_TWO(throwScope.exception(), exec);
}

LLINT_SLOW_PATH_DECL(slow_path_debug)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpDebug>();
    vm.interpreter->debug(exec, bytecode.debugHookType);
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_handle_exception)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(throwScope);
    genericUnwind(&vm, exec);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_get_from_scope)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetFromScope>();
    auto& metadata = bytecode.metadata(exec);
    const Identifier& ident = exec->codeBlock()->identifier(bytecode.var);
    JSObject* scope = jsCast<JSObject*>(LLINT_OP(bytecode.scope).jsValue());

    // ModuleVar is always converted to ClosureVar for get_from_scope.
    ASSERT(metadata.getPutInfo.resolveType() != ModuleVar);

    LLINT_RETURN(scope->getPropertySlot(exec, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            if (metadata.getPutInfo.resolveMode() == ThrowIfNotFound)
                return throwException(exec, throwScope, createUndefinedVariableError(exec, ident));
            return jsUndefined();
        }

        JSValue result = JSValue();
        if (scope->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            result = slot.getValue(exec, ident);
            if (result == jsTDZValue())
                return throwException(exec, throwScope, createTDZError(exec));
        }

        CommonSlowPaths::tryCacheGetFromScopeGlobal(exec, vm, bytecode, scope, slot, ident);

        if (!result)
            return slot.getValue(exec, ident);
        return result;
    }));
}

LLINT_SLOW_PATH_DECL(slow_path_put_to_scope)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(exec);
    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(bytecode.var);
    JSObject* scope = jsCast<JSObject*>(LLINT_OP(bytecode.scope).jsValue());
    JSValue value = LLINT_OP_C(bytecode.value).jsValue();
    if (metadata.getPutInfo.resolveType() == LocalClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(scope);
        environment->variableAt(ScopeOffset(metadata.operand)).set(vm, environment, value);
        
        // Have to do this *after* the write, because if this puts the set into IsWatched, then we need
        // to have already changed the value of the variable. Otherwise we might watch and constant-fold
        // to the Undefined value from before the assignment.
        if (metadata.watchpointSet)
            metadata.watchpointSet->touch(vm, "Executed op_put_scope<LocalClosureVar>");
        LLINT_END();
    }

    bool hasProperty = scope->hasProperty(exec, ident);
    LLINT_CHECK_EXCEPTION();
    if (hasProperty
        && scope->isGlobalLexicalEnvironment()
        && !isInitialization(metadata.getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, exec, ident, slot);
        if (slot.getValue(exec, ident) == jsTDZValue())
            LLINT_THROW(createTDZError(exec));
    }

    if (metadata.getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty)
        LLINT_THROW(createUndefinedVariableError(exec, ident));

    PutPropertySlot slot(scope, codeBlock->isStrictMode(), PutPropertySlot::UnknownContext, isInitialization(metadata.getPutInfo.initializationMode()));
    scope->methodTable(vm)->put(scope, exec, ident, value, slot);
    
    CommonSlowPaths::tryCachePutToScopeGlobal(exec, codeBlock, bytecode, scope, slot, ident);

    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_check_if_exception_is_uncatchable_and_notify_profiler)
{
    LLINT_BEGIN();
    RELEASE_ASSERT(!!throwScope.exception());

    if (isTerminatedExecutionException(vm, throwScope.exception()))
        LLINT_RETURN_TWO(pc, bitwise_cast<void*>(static_cast<uintptr_t>(1)));
    LLINT_RETURN_TWO(pc, 0);
}

LLINT_SLOW_PATH_DECL(slow_path_log_shadow_chicken_prologue)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpLogShadowChickenPrologue>();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    vm.shadowChicken().log(vm, exec, ShadowChicken::Packet::prologue(exec->jsCallee(), exec, exec->callerFrame(), scope));
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_log_shadow_chicken_tail)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpLogShadowChickenTail>();
    JSValue thisValue = LLINT_OP(bytecode.thisValue).jsValue();
    JSScope* scope = exec->uncheckedR(bytecode.scope).Register::scope();
    
#if USE(JSVALUE64)
    CallSiteIndex callSiteIndex(exec->codeBlock()->bytecodeOffset(pc));
#else
    CallSiteIndex callSiteIndex(pc);
#endif
    vm.shadowChicken().log(vm, exec, ShadowChicken::Packet::tail(exec, thisValue, scope, exec->codeBlock(), callSiteIndex));
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_profile_catch)
{
    LLINT_BEGIN();

    exec->codeBlock()->ensureCatchLivenessIsComputedForBytecodeOffset(exec->bytecodeOffset());

    auto bytecode = pc->as<OpCatch>();
    auto& metadata = bytecode.metadata(exec);
    metadata.buffer->forEach([&] (ValueProfileAndOperand& profile) {
        profile.m_profile.m_buckets[0] = JSValue::encode(exec->uncheckedR(profile.m_operand).jsValue());
    });

    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_super_sampler_begin)
{
    // FIXME: It seems like we should be able to do this in asm but llint doesn't seem to like global variables.
    // See: https://bugs.webkit.org/show_bug.cgi?id=179438
    UNUSED_PARAM(exec);
    g_superSamplerCount++;
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_super_sampler_end)
{
    // FIXME: It seems like we should be able to do this in asm but llint doesn't seem to like global variables.
    // See: https://bugs.webkit.org/show_bug.cgi?id=179438
    UNUSED_PARAM(exec);
    g_superSamplerCount--;
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_out_of_line_jump_target)
{
    CodeBlock* codeBlock = exec->codeBlock();
    pc = codeBlock->outOfLineJumpTarget(pc);
    LLINT_END_IMPL();
}

extern "C" SlowPathReturnType llint_throw_stack_overflow_error(VM* vm, ProtoCallFrame* protoFrame)
{
    ExecState* exec = vm->topCallFrame;
    auto scope = DECLARE_THROW_SCOPE(*vm);

    if (!exec)
        exec = protoFrame->callee()->globalObject(*vm)->globalExec();
    throwStackOverflowError(exec, scope);
    return encodeResult(0, 0);
}

#if ENABLE(C_LOOP)
extern "C" SlowPathReturnType llint_stack_check_at_vm_entry(VM* vm, Register* newTopOfStack)
{
    bool success = vm->ensureStackCapacityFor(newTopOfStack);
    return encodeResult(reinterpret_cast<void*>(success), 0);
}
#endif

extern "C" void llint_write_barrier_slow(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    vm.heap.writeBarrier(cell);
}

extern "C" NO_RETURN_DUE_TO_CRASH void llint_crash()
{
    CRASH();
}

} } // namespace JSC::LLInt
