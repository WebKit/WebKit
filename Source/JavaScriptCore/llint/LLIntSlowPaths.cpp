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
#include "LLIntSlowPaths.h"

#include "ArrayConstructor.h"
#include "BytecodeGenerator.h"
#include "BytecodeOperandsForCheckpoint.h"
#include "CallFrame.h"
#include "CheckpointOSRExitSideState.h"
#include "CommonSlowPathsInlines.h"
#include "Error.h"
#include "ErrorHandlingScope.h"
#include "Exception.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "FunctionAllowlist.h"
#include "FunctionCodeBlock.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
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
#include "LLIntCommon.h"
#include "LLIntData.h"
#include "LLIntExceptions.h"
#include "LLIntPrototypeLoadAdaptiveStructureWatchpoint.h"
#include "ObjectConstructor.h"
#include "ObjectPropertyConditionSet.h"
#include "ProtoCallFrameInlines.h"
#include "RegExpObject.h"
#include "ShadowChicken.h"
#include "SuperSampler.h"
#include "VMInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>

namespace JSC { namespace LLInt {

#define LLINT_BEGIN_NO_SET_PC() \
    CodeBlock* codeBlock = callFrame->codeBlock(); \
    JSGlobalObject* globalObject = codeBlock->globalObject(); \
    VM& vm = codeBlock->vm(); \
    SlowPathFrameTracer tracer(vm, callFrame); \
    dataLogLnIf(LLINT_TRACING && Options::traceLLIntSlowPath(), "Calling slow path ", WTF_PRETTY_FUNCTION); \
    auto throwScope = DECLARE_THROW_SCOPE(vm)

#ifndef NDEBUG
#define LLINT_SET_PC_FOR_STUBS() do { \
        codeBlock->bytecodeOffset(pc); \
        callFrame->setCurrentVPC(pc); \
    } while (false)
#else
#define LLINT_SET_PC_FOR_STUBS() do { \
        callFrame->setCurrentVPC(pc); \
    } while (false)
#endif

#define LLINT_BEGIN()                           \
    LLINT_BEGIN_NO_SET_PC();                    \
    LLINT_SET_PC_FOR_STUBS()

inline JSValue getNonConstantOperand(CallFrame* callFrame, VirtualRegister operand) { return callFrame->uncheckedR(operand).jsValue(); }
inline JSValue getOperand(CallFrame* callFrame, VirtualRegister operand) { return callFrame->r(operand).jsValue(); }

#define LLINT_RETURN_TWO(first, second) do {       \
        return encodeResult(first, second);        \
    } while (false)

#define LLINT_END_IMPL() LLINT_RETURN_TWO(pc, nullptr)

#define LLINT_THROW(exceptionToThrow) do {                        \
        throwException(globalObject, throwScope, exceptionToThrow);       \
        pc = returnToThrow(vm);                                 \
        LLINT_END_IMPL();                                         \
    } while (false)

#define LLINT_CHECK_EXCEPTION() do {                    \
        doExceptionFuzzingIfEnabled(globalObject, throwScope, "LLIntSlowPaths", pc);    \
        if (UNLIKELY(throwScope.exception())) {         \
            pc = returnToThrow(vm);                   \
            LLINT_END_IMPL();                           \
        }                                               \
    } while (false)

#define LLINT_END() do {                        \
        LLINT_CHECK_EXCEPTION();                \
        LLINT_END_IMPL();                       \
    } while (false)

#define JUMP_OFFSET(targetOffset) \
    ((targetOffset) ? (targetOffset) : codeBlock->outOfLineJumpOffset(pc))

#define JUMP_TO(target) do { \
        pc = reinterpret_cast<const Instruction*>(reinterpret_cast<const uint8_t*>(pc) + (target)); \
    } while (false)

#define LLINT_BRANCH(condition) do {                  \
        bool __b_condition = (condition);                         \
        LLINT_CHECK_EXCEPTION();                                  \
        if (__b_condition)                                        \
            JUMP_TO(JUMP_OFFSET(bytecode.m_targetLabel));         \
        else                                                      \
            JUMP_TO(pc->size()); \
        LLINT_END_IMPL();                                         \
    } while (false)

#define LLINT_RETURN(value) do {                \
        JSValue __r_returnValue = (value);      \
        LLINT_CHECK_EXCEPTION();                \
        callFrame->uncheckedR(bytecode.m_dst) = __r_returnValue;          \
        LLINT_END_IMPL();                       \
    } while (false)

#define LLINT_RETURN_PROFILED(value) do {               \
        JSValue __rp_returnValue = (value);                     \
        LLINT_CHECK_EXCEPTION();                                \
        callFrame->uncheckedR(bytecode.m_dst) = __rp_returnValue;                         \
        LLINT_PROFILE_VALUE(__rp_returnValue);          \
        LLINT_END_IMPL();                                       \
    } while (false)

#define LLINT_PROFILE_VALUE(value) do { \
        bytecode.metadata(codeBlock).m_profile.m_buckets[0] = JSValue::encode(value); \
    } while (false)

#define LLINT_CALL_END_IMPL(callFrame, callTarget, callTargetTag) \
    LLINT_RETURN_TWO(retagCodePtr((callTarget), callTargetTag, SlowPathPtrTag), (callFrame))

#define LLINT_CALL_THROW(globalObject, exceptionToThrow) do {                   \
        JSGlobalObject* __ct_globalObject = (globalObject);                                  \
        throwException(__ct_globalObject, throwScope, exceptionToThrow);        \
        LLINT_CALL_END_IMPL(nullptr, callToThrow(vm), ExceptionHandlerPtrTag);                 \
    } while (false)

#define LLINT_CALL_CHECK_EXCEPTION(globalObject) do {               \
        JSGlobalObject* __cce_globalObject = (globalObject);                                 \
        doExceptionFuzzingIfEnabled(__cce_globalObject, throwScope, "LLIntSlowPaths/call", nullptr); \
        if (UNLIKELY(throwScope.exception()))                           \
            LLINT_CALL_END_IMPL(nullptr, callToThrow(vm), ExceptionHandlerPtrTag); \
    } while (false)

#define LLINT_CALL_RETURN(globalObject, calleeFrame, callTarget, callTargetTag) do { \
        JSGlobalObject* __cr_globalObject = (globalObject); \
        CallFrame* __cr_calleeFrame = (calleeFrame); \
        void* __cr_callTarget = (callTarget); \
        LLINT_CALL_CHECK_EXCEPTION(__cr_globalObject);         \
        LLINT_CALL_END_IMPL(__cr_calleeFrame, __cr_callTarget, callTargetTag); \
    } while (false)

#define LLINT_RETURN_CALLEE_FRAME(calleeFrame) do { \
        CallFrame* __rcf_calleeFrame = (calleeFrame); \
        LLINT_RETURN_TWO(pc, __rcf_calleeFrame); \
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

extern "C" SlowPathReturnType llint_trace_operand(CallFrame* callFrame, const Instruction* pc, int fromWhere, int operand)
{
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    LLINT_BEGIN();
    dataLogF(
        "<%p> %p / %p: executing bc#%zu, op#%u: Trace(%d): %d\n",
        &Thread::current(),
        callFrame->codeBlock(),
        globalObject,
        static_cast<intptr_t>(callFrame->codeBlock()->bytecodeOffset(pc)),
        pc->opcodeID(),
        fromWhere,
        operand);
    LLINT_END();
}

extern "C" SlowPathReturnType llint_trace_value(CallFrame* callFrame, const Instruction* pc, int fromWhere, VirtualRegister operand)
{
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    JSValue value = getOperand(callFrame, operand);
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
        callFrame->codeBlock(),
        callFrame,
        static_cast<intptr_t>(callFrame->codeBlock()->bytecodeOffset(pc)),
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

    CodeBlock* codeBlock = callFrame->codeBlock();
    dataLogF("<%p> %p / %p: in prologue of ", &Thread::current(), codeBlock, callFrame);
    dataLog(codeBlock, "\n");
    LLINT_END_IMPL();
}

static void traceFunctionPrologue(CallFrame* callFrame, const char* comment, CodeSpecializationKind kind)
{
    if (!Options::traceLLIntExecution())
        return;

    JSFunction* callee = jsCast<JSFunction*>(callFrame->jsCallee());
    FunctionExecutable* executable = callee->jsExecutable();
    CodeBlock* codeBlock = executable->codeBlockFor(kind);
    dataLogF("<%p> %p / %p: in %s of ", &Thread::current(), codeBlock, callFrame, comment);
    dataLog(codeBlock);
    dataLogF(" function %p, executable %p; numVars = %u, numParameters = %u, numCalleeLocals = %u, caller = %p.\n",
        callee, executable, codeBlock->numVars(), codeBlock->numParameters(), codeBlock->numCalleeLocals(), callFrame->callerFrame());
}

LLINT_SLOW_PATH_DECL(trace_prologue_function_for_call)
{
    traceFunctionPrologue(callFrame, "call prologue", CodeForCall);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_prologue_function_for_construct)
{
    traceFunctionPrologue(callFrame, "construct prologue", CodeForConstruct);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_arityCheck_for_call)
{
    traceFunctionPrologue(callFrame, "call arity check", CodeForCall);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace_arityCheck_for_construct)
{
    traceFunctionPrologue(callFrame, "construct arity check", CodeForConstruct);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(trace)
{
    if (!Options::traceLLIntExecution())
        LLINT_END_IMPL();

    CodeBlock* codeBlock = callFrame->codeBlock();
    OpcodeID opcodeID = pc->opcodeID();
    dataLogF("<%p> %p / %p: executing bc#%zu, %s, pc = %p\n",
            &Thread::current(),
            codeBlock,
            callFrame,
            static_cast<intptr_t>(codeBlock->bytecodeOffset(pc)),
            pc->name(),
            pc);
    if (opcodeID == op_enter) {
        dataLogF("Frame will eventually return to %p\n", callFrame->returnPC().value());
        *removeCodePtrTag<volatile char*>(callFrame->returnPC().value());
    }
    if (opcodeID == op_ret) {
        dataLogF("Will be returning to %p\n", callFrame->returnPC().value());
        dataLogF("The new cfr will be %p\n", callFrame->callerFrame());
    }
    LLINT_END_IMPL();
}

enum EntryKind { Prologue, ArityCheck };

#if ENABLE(JIT)
static FunctionAllowlist& ensureGlobalJITAllowlist()
{
    static LazyNeverDestroyed<FunctionAllowlist> baselineAllowlist;
    static std::once_flag initializeAllowlistFlag;
    std::call_once(initializeAllowlistFlag, [] {
        const char* functionAllowlistFile = Options::jitAllowlist();
        baselineAllowlist.construct(functionAllowlistFile);
    });
    return baselineAllowlist;
}

inline bool shouldJIT(CodeBlock* codeBlock)
{
    if (!Options::bytecodeRangeToJITCompile().isInRange(codeBlock->instructionsSize())
        || !ensureGlobalJITAllowlist().contains(codeBlock))
        return false;

    return Options::useBaselineJIT();
}

// Returns true if we should try to OSR.
inline bool jitCompileAndSetHeuristics(VM& vm, CodeBlock* codeBlock, BytecodeIndex loopOSREntryBytecodeIndex = BytecodeIndex(0))
{
    DeferGCForAWhile deferGC(vm.heap); // My callers don't set top callframe, so we don't want to GC here at all.
    ASSERT(Options::useJIT());
    
    codeBlock->updateAllValueProfilePredictions();

    if (!codeBlock->checkIfJITThresholdReached()) {
        CODEBLOCK_LOG_EVENT(codeBlock, "delayJITCompile", ("threshold not reached, counter = ", codeBlock->llintExecuteCounter()));
        dataLogLnIf(Options::verboseOSR(), "    JIT threshold should be lifted.");
        return false;
    }
    
    JITWorklist::ensureGlobalWorklist().poll(vm);
    
    switch (codeBlock->jitType()) {
    case JITType::BaselineJIT: {
        dataLogLnIf(Options::verboseOSR(), "    Code was already compiled.");
        codeBlock->jitSoon();
        return true;
    }
    case JITType::InterpreterThunk: {
        JITWorklist::ensureGlobalWorklist().compileLater(codeBlock, loopOSREntryBytecodeIndex);
        return codeBlock->jitType() == JITType::BaselineJIT;
    }
    default:
        dataLog("Unexpected code block in LLInt: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
}

static SlowPathReturnType entryOSR(CodeBlock* codeBlock, const char *name, EntryKind kind)
{
    dataLogLnIf(Options::verboseOSR(),
        *codeBlock, ": Entered ", name, " with executeCounter = ",
        codeBlock->llintExecuteCounter());
    
    if (!shouldJIT(codeBlock)) {
        codeBlock->dontJITAnytimeSoon();
        LLINT_RETURN_TWO(nullptr, nullptr);
    }
    VM& vm = codeBlock->vm();
    if (!jitCompileAndSetHeuristics(vm, codeBlock))
        LLINT_RETURN_TWO(nullptr, nullptr);
    
    CODEBLOCK_LOG_EVENT(codeBlock, "OSR entry", ("in prologue"));
    
    if (kind == Prologue)
        LLINT_RETURN_TWO(codeBlock->jitCode()->executableAddress(), nullptr);
    ASSERT(kind == ArityCheck);
    LLINT_RETURN_TWO(codeBlock->jitCode()->addressForCall(MustCheckArity).executableAddress(), nullptr);
}
#else // ENABLE(JIT)
static SlowPathReturnType entryOSR(CodeBlock* codeBlock, const char*, EntryKind)
{
    codeBlock->dontJITAnytimeSoon();
    LLINT_RETURN_TWO(nullptr, nullptr);
}
#endif // ENABLE(JIT)

LLINT_SLOW_PATH_DECL(entry_osr)
{
    UNUSED_PARAM(pc);
    return entryOSR(callFrame->codeBlock(), "entry_osr", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_call)
{
    UNUSED_PARAM(pc);
    return entryOSR(jsCast<JSFunction*>(callFrame->jsCallee())->jsExecutable()->codeBlockForCall(), "entry_osr_function_for_call", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_construct)
{
    UNUSED_PARAM(pc);
    return entryOSR(jsCast<JSFunction*>(callFrame->jsCallee())->jsExecutable()->codeBlockForConstruct(), "entry_osr_function_for_construct", Prologue);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_call_arityCheck)
{
    UNUSED_PARAM(pc);
    return entryOSR(jsCast<JSFunction*>(callFrame->jsCallee())->jsExecutable()->codeBlockForCall(), "entry_osr_function_for_call_arityCheck", ArityCheck);
}

LLINT_SLOW_PATH_DECL(entry_osr_function_for_construct_arityCheck)
{
    UNUSED_PARAM(pc);
    return entryOSR(jsCast<JSFunction*>(callFrame->jsCallee())->jsExecutable()->codeBlockForConstruct(), "entry_osr_function_for_construct_arityCheck", ArityCheck);
}

LLINT_SLOW_PATH_DECL(loop_osr)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(throwScope);
    UNUSED_PARAM(globalObject);

#if ENABLE(JIT)
    dataLogLnIf(Options::verboseOSR(),
            *codeBlock, ": Entered loop_osr with executeCounter = ",
            codeBlock->llintExecuteCounter());

    if (UNLIKELY(Options::returnEarlyFromInfiniteLoopsForFuzzing() && codeBlock->loopHintsAreEligibleForFuzzingEarlyReturn())) {
        uint64_t* ptr = vm.getLoopHintExecutionCounter(pc);
        *ptr += codeBlock->llintExecuteCounter().m_activeThreshold;
        if (*ptr >= Options::earlyReturnFromInfiniteLoopsLimit())
            LLINT_RETURN_TWO(LLInt::getCodePtr<JSEntryPtrTag>(fuzzer_return_early_from_loop_hint).executableAddress(), callFrame->topOfFrame());
    }
    
    
    auto loopOSREntryBytecodeIndex = BytecodeIndex(codeBlock->bytecodeOffset(pc));

    if (!shouldJIT(codeBlock)) {
        codeBlock->dontJITAnytimeSoon();
        LLINT_RETURN_TWO(nullptr, nullptr);
    }
    
    if (!jitCompileAndSetHeuristics(vm, codeBlock, loopOSREntryBytecodeIndex))
        LLINT_RETURN_TWO(nullptr, nullptr);
    
    CODEBLOCK_LOG_EVENT(codeBlock, "osrEntry", ("at ", loopOSREntryBytecodeIndex));

    ASSERT(codeBlock->jitType() == JITType::BaselineJIT);

    const JITCodeMap& codeMap = codeBlock->jitCodeMap();
    CodeLocationLabel<JSEntryPtrTag> codeLocation = codeMap.find(loopOSREntryBytecodeIndex);
    ASSERT(codeLocation);

    void* jumpTarget = codeLocation.executableAddress();
    ASSERT(jumpTarget);
    
    LLINT_RETURN_TWO(jumpTarget, callFrame->topOfFrame());
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
    UNUSED_PARAM(globalObject);

#if ENABLE(JIT)
    dataLogLnIf(Options::verboseOSR(),
        *codeBlock, ": Entered replace with executeCounter = ",
        codeBlock->llintExecuteCounter());
    
    if (shouldJIT(codeBlock))
        jitCompileAndSetHeuristics(vm, codeBlock);
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
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalObject* globalObject = codeBlock->globalObject();
    VM& vm = codeBlock->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    // It's ok to create the SlowPathFrameTracer here before we
    // convertToStackOverflowFrame() because this function is always called
    // after the frame has been propulated with a proper CodeBlock and callee.
    SlowPathFrameTracer tracer(vm, callFrame);

    LLINT_SET_PC_FOR_STUBS();

    slowPathLogF("Checking stack height with callFrame = %p.\n", callFrame);
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
    Register* topOfFrame = callFrame->topOfFrame();
    if (LIKELY(topOfFrame < reinterpret_cast<Register*>(callFrame))) {
        ASSERT(!vm.interpreter->cloopStack().containsAddress(topOfFrame));
        if (LIKELY(vm.ensureStackCapacityFor(topOfFrame)))
            LLINT_RETURN_TWO(pc, 0);
    }
#endif

    callFrame->convertToStackOverflowFrame(vm, codeBlock);
    ErrorHandlingScope errorScope(vm);
    throwStackOverflowError(globalObject, throwScope);
    pc = returnToThrow(vm);
    LLINT_RETURN_TWO(pc, callFrame);
}

LLINT_SLOW_PATH_DECL(slow_path_new_object)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewObject>();
    auto& metadata = bytecode.metadata(codeBlock);
    LLINT_RETURN(constructEmptyObject(vm, metadata.m_objectAllocationProfile.structure()));
}

LLINT_SLOW_PATH_DECL(slow_path_new_array)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewArray>();
    auto& metadata = bytecode.metadata(codeBlock);
    LLINT_RETURN(constructArrayNegativeIndexed(globalObject, &metadata.m_arrayAllocationProfile, bitwise_cast<JSValue*>(&callFrame->uncheckedR(bytecode.m_argv)), bytecode.m_argc));
}

LLINT_SLOW_PATH_DECL(slow_path_new_array_with_size)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewArrayWithSize>();
    auto& metadata = bytecode.metadata(codeBlock);
    LLINT_RETURN(constructArrayWithSizeQuirk(globalObject, &metadata.m_arrayAllocationProfile, getOperand(callFrame, bytecode.m_length)));
}

LLINT_SLOW_PATH_DECL(slow_path_new_regexp)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewRegexp>();
    RegExp* regExp = jsCast<RegExp*>(getOperand(callFrame, bytecode.m_regexp));
    ASSERT(regExp->isValid());
    LLINT_RETURN(RegExpObject::create(vm, globalObject->regExpStructure(), regExp));
}

LLINT_SLOW_PATH_DECL(slow_path_instanceof)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpInstanceof>();
    JSValue value = getOperand(callFrame, bytecode.m_value);
    JSValue proto = getOperand(callFrame, bytecode.m_prototype);
    LLINT_RETURN(jsBoolean(JSObject::defaultHasInstance(globalObject, value, proto)));
}

LLINT_SLOW_PATH_DECL(slow_path_instanceof_custom)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpInstanceofCustom>();
    JSValue value = getOperand(callFrame, bytecode.m_value);
    JSValue constructor = getOperand(callFrame, bytecode.m_constructor);
    JSValue hasInstanceValue = getOperand(callFrame, bytecode.m_hasInstanceValue);

    ASSERT(constructor.isObject());
    ASSERT(hasInstanceValue != globalObject->functionProtoHasInstanceSymbolFunction() || !constructor.getObject()->structure(vm)->typeInfo().implementsDefaultHasInstance());

    JSValue result = jsBoolean(constructor.getObject()->hasInstance(globalObject, value, hasInstanceValue));
    LLINT_RETURN(result);
}

LLINT_SLOW_PATH_DECL(slow_path_try_get_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpTryGetById>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    PropertySlot slot(baseValue, PropertySlot::PropertySlot::InternalMethodType::VMInquiry, &vm);

    baseValue.getPropertySlot(globalObject, ident, slot);
    JSValue result = slot.getPureResult();

    LLINT_RETURN_PROFILED(result);
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_id_direct)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetByIdDirect>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    PropertySlot slot(baseValue, PropertySlot::PropertySlot::InternalMethodType::GetOwnProperty);

    bool found = baseValue.getOwnPropertySlot(globalObject, ident, slot);
    LLINT_CHECK_EXCEPTION();
    JSValue result = found ? slot.getValue(globalObject, ident) : jsUndefined();
    LLINT_CHECK_EXCEPTION();

    if (!LLINT_ALWAYS_ACCESS_SLOW && slot.isCacheable() && !slot.isUnset()) {
        auto& metadata = bytecode.metadata(codeBlock);
        {
            StructureID oldStructureID = metadata.m_structureID;
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
            metadata.m_structureID = 0;
            metadata.m_offset = 0;

            if (structure->propertyAccessesAreCacheable() && !structure->needImpurePropertyWatchpoint()) {
                {
                    ConcurrentJSLocker locker(codeBlock->m_lock);
                    metadata.m_structureID = structure->id();
                    metadata.m_offset = slot.cachedOffset();
                }
                vm.heap.writeBarrier(codeBlock);
            }
        }
    }

    LLINT_RETURN_PROFILED(result);
}


static void setupGetByIdPrototypeCache(JSGlobalObject* globalObject, VM& vm, CodeBlock* codeBlock, const Instruction* pc, GetByIdModeMetadata& metadata, JSCell* baseCell, PropertySlot& slot, const Identifier& ident)
{
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

    prepareChainForCaching(globalObject, baseCell, slot);

    ObjectPropertyConditionSet conditions;
    if (slot.isUnset())
        conditions = generateConditionsForPropertyMiss(vm, codeBlock, globalObject, structure, ident.impl());
    else
        conditions = generateConditionsForPrototypePropertyHit(vm, codeBlock, globalObject, structure, slot.slotBase(), ident.impl());

    if (!conditions.isValid())
        return;

    unsigned bytecodeOffset = codeBlock->bytecodeOffset(pc);
    PropertyOffset offset = invalidOffset;
    CodeBlock::StructureWatchpointMap& watchpointMap = codeBlock->llintGetByIdWatchpointMap();
    Vector<LLIntPrototypeLoadAdaptiveStructureWatchpoint> watchpoints;
    watchpoints.reserveInitialCapacity(conditions.size());
    for (ObjectPropertyCondition condition : conditions) {
        if (!condition.isWatchable())
            return;
        if (condition.condition().kind() == PropertyCondition::Presence)
            offset = condition.condition().offset();
        watchpoints.uncheckedConstructAndAppend(codeBlock, condition, bytecodeOffset);
        watchpoints.last().install(vm);
    }

    ASSERT((offset == invalidOffset) == slot.isUnset());
    auto result = watchpointMap.add(std::make_tuple(structure->id(), bytecodeOffset), WTFMove(watchpoints));
    ASSERT_UNUSED(result, result.isNewEntry);

    {
        ConcurrentJSLocker locker(codeBlock->m_lock);
        if (slot.isUnset())
            metadata.setUnsetMode(structure);
        else {
            ASSERT(slot.isValue());
            metadata.setProtoLoadMode(structure, offset, slot.slotBase());
        }
    }
    vm.heap.writeBarrier(codeBlock);
}

static JSValue performLLIntGetByID(const Instruction* pc, CodeBlock* codeBlock, JSGlobalObject* globalObject, JSValue baseValue, const Identifier& ident, GetByIdModeMetadata& metadata)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    PropertySlot slot(baseValue, PropertySlot::PropertySlot::InternalMethodType::Get);

    JSValue result = baseValue.get(globalObject, ident, slot);
    RETURN_IF_EXCEPTION(throwScope, { });

    if (!LLINT_ALWAYS_ACCESS_SLOW
        && baseValue.isCell()
        && slot.isCacheable()
        && !slot.isUnset()) {
        {
            StructureID oldStructureID;
            switch (metadata.mode) {
            case GetByIdMode::Default:
                oldStructureID = metadata.defaultMode.structureID;
                break;
            case GetByIdMode::Unset:
                oldStructureID = metadata.unsetMode.structureID;
                break;
            case GetByIdMode::ProtoLoad:
                oldStructureID = metadata.protoLoadMode.structureID;
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
            ConcurrentJSLocker locker(codeBlock->m_lock);
            // Start out by clearing out the old cache.
            metadata.clearToDefaultModeWithoutCache();

            // Prevent the prototype cache from ever happening.
            metadata.hitCountForLLIntCaching = 0;
        
            if (structure->propertyAccessesAreCacheable() && !structure->needImpurePropertyWatchpoint()) {
                metadata.defaultMode.structureID = structure->id();
                metadata.defaultMode.cachedOffset = slot.cachedOffset();
                vm.heap.writeBarrier(codeBlock);
            }
        } else if (UNLIKELY(metadata.hitCountForLLIntCaching && slot.isValue())) {
            ASSERT(slot.slotBase() != baseValue);

            if (!(--metadata.hitCountForLLIntCaching))
                setupGetByIdPrototypeCache(globalObject, vm, codeBlock, pc, metadata, baseCell, slot, ident);
        }
    } else if (!LLINT_ALWAYS_ACCESS_SLOW && isJSArray(baseValue) && ident == vm.propertyNames->length) {
        {
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.setArrayLengthMode();
            metadata.arrayLengthMode.arrayProfile.observeStructure(baseValue.asCell()->structure(vm));
        }
        vm.heap.writeBarrier(codeBlock);
    }

    return result;
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetById>();
    auto& metadata = bytecode.metadata(codeBlock);
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);

    JSValue result = performLLIntGetByID(pc, codeBlock, globalObject, baseValue, ident, metadata.m_modeMetadata);
    LLINT_RETURN_PROFILED(result);
}

LLINT_SLOW_PATH_DECL(slow_path_iterator_open_get_next)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpIteratorOpen>();
    auto& metadata = bytecode.metadata(codeBlock);
    JSValue iterator = getOperand(callFrame, bytecode.m_iterator);
    Register& nextRegister = callFrame->uncheckedR(bytecode.m_next);

    if (!iterator.isObject())
        LLINT_THROW(createTypeError(globalObject, "Iterator result interface is not an object."_s));

    JSValue result = performLLIntGetByID(pc, codeBlock, globalObject, iterator, vm.propertyNames->next, metadata.m_modeMetadata);
    LLINT_CHECK_EXCEPTION();
    nextRegister = result;
    bytecode.metadata(codeBlock).m_nextProfile.m_buckets[0] = JSValue::encode(result);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_iterator_next_get_done)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpIteratorNext>();
    auto& metadata = bytecode.metadata(codeBlock);
    // We use m_value to hold the iterator return value since it's either not live past this bytecode or it's going to be filled later.
    JSValue iteratorReturn = getOperand(callFrame, bytecode.m_value);
    Register& doneRegister = callFrame->uncheckedR(bytecode.m_done);

    if (!iteratorReturn.isObject())
        LLINT_THROW(createTypeError(globalObject, "Iterator result interface is not an object."_s));

    JSValue result = performLLIntGetByID(pc, codeBlock, globalObject, iteratorReturn, vm.propertyNames->done, metadata.m_doneModeMetadata);
    LLINT_CHECK_EXCEPTION();
    doneRegister = result;
    bytecode.metadata(codeBlock).m_doneProfile.m_buckets[0] = JSValue::encode(result);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_iterator_next_get_value)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpIteratorNext>();
    auto& metadata = bytecode.metadata(codeBlock);
    // We use m_value to hold the iterator return value tmp since it's either not live past this bytecode or it's going to be filled later.
    Register& valueRegister = callFrame->uncheckedR(bytecode.m_value);
    JSValue iteratorReturn = valueRegister.jsValue();

    JSValue result = performLLIntGetByID(pc, codeBlock, globalObject, iteratorReturn, vm.propertyNames->value, metadata.m_valueModeMetadata);
    LLINT_CHECK_EXCEPTION();
    valueRegister = result;
    bytecode.metadata(codeBlock).m_valueProfile.m_buckets[0] = JSValue::encode(result);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutById>();
    auto& metadata = bytecode.metadata(codeBlock);
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    PutPropertySlot slot(baseValue, bytecode.m_flags.ecmaMode().isStrict(), codeBlock->putByIdContext());

    Structure* oldStructure = baseValue.isCell() ? baseValue.asCell()->structure(vm) : nullptr;
    if (bytecode.m_flags.isDirect())
        CommonSlowPaths::putDirectWithReify(vm, globalObject, asObject(baseValue), ident, getOperand(callFrame, bytecode.m_value), slot);
    else
        baseValue.putInline(globalObject, ident, getOperand(callFrame, bytecode.m_value), slot);
    LLINT_CHECK_EXCEPTION();
    
    if (!LLINT_ALWAYS_ACCESS_SLOW
        && baseValue.isCell()
        && slot.isCacheablePut()
        && oldStructure->propertyAccessesAreCacheable()) {
        {
            StructureID oldStructureID = metadata.m_oldStructureID;
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
        metadata.m_oldStructureID = 0;
        metadata.m_offset = 0;
        metadata.m_newStructureID = 0;
        metadata.m_structureChain.clear();
        
        JSCell* baseCell = baseValue.asCell();
        Structure* newStructure = baseCell->structure(vm);
        
        if (newStructure->propertyAccessesAreCacheable() && baseCell == slot.base()) {
            if (slot.type() == PutPropertySlot::NewProperty) {
                GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
                if (!newStructure->isDictionary() && newStructure->previousID()->outOfLineCapacity() == newStructure->outOfLineCapacity()) {
                    ASSERT(oldStructure == newStructure->previousID());
                    if (oldStructure == newStructure->previousID()) {
                        ASSERT(oldStructure->transitionWatchpointSetHasBeenInvalidated());

                        bool sawPolyProto = false;
                        auto result = normalizePrototypeChain(globalObject, baseCell, sawPolyProto);
                        if (result != InvalidPrototypeChain && !sawPolyProto) {
                            ASSERT(oldStructure->isObject());
                            metadata.m_oldStructureID = oldStructure->id();
                            metadata.m_offset = slot.cachedOffset();
                            metadata.m_newStructureID = newStructure->id();
                            if (!(bytecode.m_flags.isDirect())) {
                                StructureChain* chain = newStructure->prototypeChain(globalObject, asObject(baseCell));
                                ASSERT(chain);
                                metadata.m_structureChain.set(vm, codeBlock, chain);
                            }
                            vm.heap.writeBarrier(codeBlock);
                        }
                    }
                }
            } else {
                // This assert helps catch bugs if we accidentally forget to disable caching
                // when we transition then store to an existing property. This is common among
                // paths that reify lazy properties. If we reify a lazy property and forget
                // to disable caching, we may come down this path. The Replace IC does not
                // know how to model these types of structure transitions (or any structure
                // transition for that matter).
                RELEASE_ASSERT(newStructure == oldStructure);
                newStructure->didCachePropertyReplacement(vm, slot.cachedOffset());
                {
                    ConcurrentJSLocker locker(codeBlock->m_lock);
                    metadata.m_oldStructureID = newStructure->id();
                    metadata.m_offset = slot.cachedOffset();
                }
                vm.heap.writeBarrier(codeBlock);
            }
        }
    }
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_del_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpDelById>();
    JSObject* baseObject = getOperand(callFrame, bytecode.m_base).toObject(globalObject);
    LLINT_CHECK_EXCEPTION();
    bool couldDelete = JSCell::deleteProperty(baseObject, globalObject, codeBlock->identifier(bytecode.m_property));
    LLINT_CHECK_EXCEPTION();
    if (!couldDelete && bytecode.m_ecmaMode.isStrict())
        LLINT_THROW(createTypeError(globalObject, UnableToDeletePropertyError));
    LLINT_RETURN(jsBoolean(couldDelete));
}

static ALWAYS_INLINE JSValue getByVal(VM& vm, JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue subscript, OpGetByVal bytecode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            RefPtr<AtomStringImpl> existingAtomString = asString(subscript)->toExistingAtomString(globalObject);
            RETURN_IF_EXCEPTION(scope, JSValue());
            if (existingAtomString) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomString.get()))
                    return result;
            }
        }
    }
    
    if (Optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        uint32_t i = *index;
        auto& metadata = bytecode.metadata(codeBlock);
        ArrayProfile* arrayProfile = &metadata.m_arrayProfile;

        if (isJSString(baseValue)) {
            if (asString(baseValue)->canGetIndex(i)) {
                scope.release();
                return asString(baseValue)->getIndex(globalObject, i);
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
        return baseValue.get(globalObject, i);
    }

    baseValue.requireObjectCoercible(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());
    auto property = subscript.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, JSValue());
    scope.release();
    return baseValue.get(globalObject, property);
}

LLINT_SLOW_PATH_DECL(slow_path_get_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetByVal>();
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    JSValue subscript = getOperand(callFrame, bytecode.m_property);

    if (subscript.isString() || subscript.isSymbol()) {
        auto& metadata = bytecode.metadata(codeBlock);
        if (metadata.m_seenIdentifiers.count() <= Options::getByValICMaxNumberOfIdentifiers()) {
            const UniquedStringImpl* impl = nullptr;
            if (subscript.isSymbol())
                impl = &jsCast<Symbol*>(subscript)->privateName().uid();
            else {
                JSString* string = asString(subscript);
                if (auto* maybeUID = string->tryGetValueImpl()) {
                    if (maybeUID->isAtom())
                        impl = static_cast<const UniquedStringImpl*>(maybeUID);
                }
            }

            metadata.m_seenIdentifiers.observe(impl);
        }
    }
    
    LLINT_RETURN_PROFILED(getByVal(vm, globalObject, codeBlock, baseValue, subscript, bytecode));
}

LLINT_SLOW_PATH_DECL(slow_path_get_private_name)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpGetPrivateName>();
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    JSValue subscript = getOperand(callFrame, bytecode.m_property);
    ASSERT(subscript.isSymbol());

    baseValue.requireObjectCoercible(globalObject);
    LLINT_CHECK_EXCEPTION();
    auto property = subscript.toPropertyKey(globalObject);
    LLINT_CHECK_EXCEPTION();
    ASSERT(property.isPrivateName());

    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);
    asObject(baseValue)->getPrivateField(globalObject, property, slot);
    LLINT_CHECK_EXCEPTION();

    if (!LLINT_ALWAYS_ACCESS_SLOW && slot.isCacheable() && !slot.isUnset()) {
        auto& metadata = bytecode.metadata(codeBlock);
        {
            StructureID oldStructureID = metadata.m_structureID;
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
            metadata.m_structureID = 0;
            metadata.m_offset = 0;

            if (!structure->isUncacheableDictionary()) {
                {
                    ConcurrentJSLocker locker(codeBlock->m_lock);
                    metadata.m_structureID = structure->id();
                    metadata.m_offset = slot.cachedOffset();

                    //  Update the cached private symbol
                    metadata.m_property.set(vm, codeBlock, subscript.asCell());
                }
                vm.heap.writeBarrier(codeBlock);
            }
        }
    }

    LLINT_RETURN_PROFILED(slot.getValue(globalObject, property));
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_val)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpPutByVal>();
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    JSValue subscript = getOperand(callFrame, bytecode.m_property);
    JSValue value = getOperand(callFrame, bytecode.m_value);
    bool isStrictMode = bytecode.m_ecmaMode.isStrict();
    
    if (Optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        uint32_t i = *index;
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canSetIndexQuickly(i, value))
                object->setIndexQuickly(vm, i, value);
            else
                object->methodTable(vm)->putByIndex(object, globalObject, i, value, isStrictMode);
            LLINT_END();
        }
        baseValue.putByIndex(globalObject, i, value, isStrictMode);
        LLINT_END();
    }

    auto property = subscript.toPropertyKey(globalObject);
    LLINT_CHECK_EXCEPTION();
    PutPropertySlot slot(baseValue, isStrictMode);
    baseValue.put(globalObject, property, value, slot);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_by_val_direct)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpPutByValDirect>();
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    JSValue subscript = getOperand(callFrame, bytecode.m_property);
    JSValue value = getOperand(callFrame, bytecode.m_value);
    RELEASE_ASSERT(baseValue.isObject());
    JSObject* baseObject = asObject(baseValue);
    bool isStrictMode = bytecode.m_ecmaMode.isStrict();
    if (Optional<uint32_t> index = subscript.tryGetAsUint32Index()) {
        baseObject->putDirectIndex(globalObject, *index, value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        LLINT_END();
    }

    // Don't put to an object if toString threw an exception.
    auto property = subscript.toPropertyKey(globalObject);
    if (UNLIKELY(throwScope.exception()))
        LLINT_END();

    if (Optional<uint32_t> index = parseIndex(property))
        baseObject->putDirectIndex(globalObject, index.value(), value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
    else {
        PutPropertySlot slot(baseObject, isStrictMode);
        CommonSlowPaths::putDirectWithReify(vm, globalObject, baseObject, property, value, slot);
    }
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_private_name)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpPutPrivateName>();
    auto& metadata = bytecode.metadata(codeBlock);
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    JSValue subscript = getOperand(callFrame, bytecode.m_property);
    JSValue value = getOperand(callFrame, bytecode.m_value);

    auto property = subscript.toPropertyKey(globalObject);
    LLINT_CHECK_EXCEPTION();
    ASSERT(property.isPrivateName());

    Structure* oldStructure = baseValue.isCell() ? baseValue.asCell()->structure(vm) : nullptr;

    // Private fields can only be accessed within class lexical scope
    // and class methods are always in strict mode
    const bool isStrictMode = true;
    auto baseObject = asObject(baseValue);
    PutPropertySlot slot(baseObject, isStrictMode);
    if (bytecode.m_putKind.isDefine())
        baseObject->definePrivateField(globalObject, property, value, slot);
    else {
        ASSERT(bytecode.m_putKind.isSet());
        baseObject->setPrivateField(globalObject, property, value, slot);
    }
    LLINT_CHECK_EXCEPTION();

    if (!LLINT_ALWAYS_ACCESS_SLOW
        && baseValue.isCell()
        && slot.isCacheablePut()
        && subscript.isCell()
        && oldStructure->propertyAccessesAreCacheable()) {
        {
            StructureID oldStructureID = metadata.m_oldStructureID;
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
        metadata.m_oldStructureID = 0;
        metadata.m_offset = 0;
        metadata.m_newStructureID = 0;
        metadata.m_property.clear();
        
        JSCell* baseCell = baseValue.asCell();
        Structure* newStructure = baseCell->structure(vm);
        
        if (newStructure->propertyAccessesAreCacheable() && baseCell == slot.base()) {
            if (slot.type() == PutPropertySlot::NewProperty) {
                GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
                if (!newStructure->isDictionary() && newStructure->previousID()->outOfLineCapacity() == newStructure->outOfLineCapacity()) {
                    ASSERT(oldStructure == newStructure->previousID());
                    if (oldStructure == newStructure->previousID()) {
                        ASSERT(oldStructure->transitionWatchpointSetHasBeenInvalidated());

                        bool sawPolyProto = false;
                        auto result = normalizePrototypeChain(globalObject, baseCell, sawPolyProto);
                        if (result != InvalidPrototypeChain && !sawPolyProto) {
                            ASSERT(oldStructure->isObject());
                            metadata.m_oldStructureID = oldStructure->id();
                            metadata.m_offset = slot.cachedOffset();
                            metadata.m_newStructureID = newStructure->id();
                            metadata.m_property.set(vm, codeBlock, subscript.asCell());
                            vm.heap.writeBarrier(codeBlock);
                        }
                    }
                }
            } else {
                // This assert helps catch bugs if we accidentally forget to disable caching
                // when we transition then store to an existing property. This is common among
                // paths that reify lazy properties. If we reify a lazy property and forget
                // to disable caching, we may come down this path. The Replace IC does not
                // know how to model these types of structure transitions (or any structure
                // transition for that matter).
                RELEASE_ASSERT(newStructure == oldStructure);
                newStructure->didCachePropertyReplacement(vm, slot.cachedOffset());
                {
                    ConcurrentJSLocker locker(codeBlock->m_lock);
                    metadata.m_oldStructureID = newStructure->id();
                    metadata.m_offset = slot.cachedOffset();
                    metadata.m_property.set(vm, codeBlock, subscript.asCell());
                }
                vm.heap.writeBarrier(codeBlock);
            }
        }
    }

    LLINT_END();    
}

LLINT_SLOW_PATH_DECL(slow_path_del_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpDelByVal>();
    JSValue baseValue = getOperand(callFrame, bytecode.m_base);
    JSObject* baseObject = baseValue.toObject(globalObject);
    LLINT_CHECK_EXCEPTION();

    JSValue subscript = getOperand(callFrame, bytecode.m_property);
    
    bool couldDelete;
    
    uint32_t i;
    if (subscript.getUInt32(i))
        couldDelete = baseObject->methodTable(vm)->deletePropertyByIndex(baseObject, globalObject, i);
    else {
        LLINT_CHECK_EXCEPTION();
        auto property = subscript.toPropertyKey(globalObject);
        LLINT_CHECK_EXCEPTION();
        couldDelete = JSCell::deleteProperty(baseObject, globalObject, property);
    }
    LLINT_CHECK_EXCEPTION();

    if (!couldDelete && bytecode.m_ecmaMode.isStrict())
        LLINT_THROW(createTypeError(globalObject, UnableToDeletePropertyError));
    
    LLINT_RETURN(jsBoolean(couldDelete));
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutGetterById>();
    ASSERT(getNonConstantOperand(callFrame, bytecode.m_base).isObject());
    JSObject* baseObj = asObject(getNonConstantOperand(callFrame, bytecode.m_base));

    unsigned options = bytecode.m_attributes;

    JSValue getter = getNonConstantOperand(callFrame, bytecode.m_accessor);
    ASSERT(getter.isObject());

    baseObj->putGetter(globalObject, codeBlock->identifier(bytecode.m_property), asObject(getter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_setter_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutSetterById>();
    ASSERT(getNonConstantOperand(callFrame, bytecode.m_base).isObject());
    JSObject* baseObj = asObject(getNonConstantOperand(callFrame, bytecode.m_base));

    unsigned options = bytecode.m_attributes;

    JSValue setter = getNonConstantOperand(callFrame, bytecode.m_accessor);
    ASSERT(setter.isObject());

    baseObj->putSetter(globalObject, codeBlock->identifier(bytecode.m_property), asObject(setter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_setter_by_id)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutGetterSetterById>();
    ASSERT(getNonConstantOperand(callFrame, bytecode.m_base).isObject());
    JSObject* baseObject = asObject(getNonConstantOperand(callFrame, bytecode.m_base));

    JSValue getter = getNonConstantOperand(callFrame, bytecode.m_getter);
    JSValue setter = getNonConstantOperand(callFrame, bytecode.m_setter);
    ASSERT(getter.isObject() || setter.isObject());
    GetterSetter* accessor = GetterSetter::create(vm, globalObject, getter, setter);

    CommonSlowPaths::putDirectAccessorWithReify(vm, globalObject, baseObject, codeBlock->identifier(bytecode.m_property), accessor, bytecode.m_attributes);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_getter_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutGetterByVal>();
    ASSERT(getNonConstantOperand(callFrame, bytecode.m_base).isObject());
    JSObject* baseObj = asObject(getNonConstantOperand(callFrame, bytecode.m_base));
    JSValue subscript = getOperand(callFrame, bytecode.m_property);

    unsigned options = bytecode.m_attributes;

    JSValue getter = getNonConstantOperand(callFrame, bytecode.m_accessor);
    ASSERT(getter.isObject());

    auto property = subscript.toPropertyKey(globalObject);
    LLINT_CHECK_EXCEPTION();

    baseObj->putGetter(globalObject, property, asObject(getter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_put_setter_by_val)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpPutSetterByVal>();
    ASSERT(getNonConstantOperand(callFrame, bytecode.m_base).isObject());
    JSObject* baseObj = asObject(getNonConstantOperand(callFrame, bytecode.m_base));
    JSValue subscript = getOperand(callFrame, bytecode.m_property);

    unsigned options = bytecode.m_attributes;

    JSValue setter = getNonConstantOperand(callFrame, bytecode.m_accessor);
    ASSERT(setter.isObject());

    auto property = subscript.toPropertyKey(globalObject);
    LLINT_CHECK_EXCEPTION();

    baseObj->putSetter(globalObject, property, asObject(setter), options);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_jtrue)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJtrue>();
    LLINT_BRANCH(getOperand(callFrame, bytecode.m_condition).toBoolean(globalObject));
}

LLINT_SLOW_PATH_DECL(slow_path_jfalse)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJfalse>();
    LLINT_BRANCH(!getOperand(callFrame, bytecode.m_condition).toBoolean(globalObject));
}

LLINT_SLOW_PATH_DECL(slow_path_jless)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJless>();
    LLINT_BRANCH(jsLess<true>(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jnless)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJnless>();
    LLINT_BRANCH(!jsLess<true>(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jgreater)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJgreater>();
    LLINT_BRANCH(jsLess<false>(globalObject, getOperand(callFrame, bytecode.m_rhs), getOperand(callFrame, bytecode.m_lhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jngreater)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJngreater>();
    LLINT_BRANCH(!jsLess<false>(globalObject, getOperand(callFrame, bytecode.m_rhs), getOperand(callFrame, bytecode.m_lhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jlesseq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJlesseq>();
    LLINT_BRANCH(jsLessEq<true>(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jnlesseq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJnlesseq>();
    LLINT_BRANCH(!jsLessEq<true>(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jgreatereq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJgreatereq>();
    LLINT_BRANCH(jsLessEq<false>(globalObject, getOperand(callFrame, bytecode.m_rhs), getOperand(callFrame, bytecode.m_lhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jngreatereq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJngreatereq>();
    LLINT_BRANCH(!jsLessEq<false>(globalObject, getOperand(callFrame, bytecode.m_rhs), getOperand(callFrame, bytecode.m_lhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jeq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJeq>();
    LLINT_BRANCH(JSValue::equal(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jneq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJneq>();
    LLINT_BRANCH(!JSValue::equal(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jstricteq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJstricteq>();
    LLINT_BRANCH(JSValue::strictEqual(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_jnstricteq)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpJnstricteq>();
    LLINT_BRANCH(!JSValue::strictEqual(globalObject, getOperand(callFrame, bytecode.m_lhs), getOperand(callFrame, bytecode.m_rhs)));
}

LLINT_SLOW_PATH_DECL(slow_path_switch_imm)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSwitchImm>();
    JSValue scrutinee = getOperand(callFrame, bytecode.m_scrutinee);
    ASSERT(scrutinee.isDouble());
    double value = scrutinee.asDouble();
    int32_t intValue = static_cast<int32_t>(value);
    int defaultOffset = JUMP_OFFSET(bytecode.m_defaultOffset);
    if (value == intValue)
        JUMP_TO(codeBlock->switchJumpTable(bytecode.m_tableIndex).offsetForValue(intValue, defaultOffset));
    else
        JUMP_TO(defaultOffset);
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_switch_char)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSwitchChar>();
    JSValue scrutinee = getOperand(callFrame, bytecode.m_scrutinee);
    ASSERT(scrutinee.isString());
    JSString* string = asString(scrutinee);
    ASSERT(string->length() == 1);
    int defaultOffset = JUMP_OFFSET(bytecode.m_defaultOffset);
    StringImpl* impl = string->value(globalObject).impl();
    JUMP_TO(codeBlock->switchJumpTable(bytecode.m_tableIndex).offsetForValue((*impl)[0], defaultOffset));
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_switch_string)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSwitchString>();
    JSValue scrutinee = getOperand(callFrame, bytecode.m_scrutinee);
    int defaultOffset = JUMP_OFFSET(bytecode.m_defaultOffset);
    if (!scrutinee.isString())
        JUMP_TO(defaultOffset);
    else {
        StringImpl* scrutineeStringImpl = asString(scrutinee)->value(globalObject).impl();

        LLINT_CHECK_EXCEPTION();

        JUMP_TO(codeBlock->stringSwitchJumpTable(bytecode.m_tableIndex).offsetForValue(scrutineeStringImpl, defaultOffset));
    }
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_new_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewFunc>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    slowPathLogF("Creating function!\n");
    LLINT_RETURN(JSFunction::create(vm, codeBlock->functionDecl(bytecode.m_functionDecl), scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_generator_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewGeneratorFunc>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    slowPathLogF("Creating function!\n");
    LLINT_RETURN(JSGeneratorFunction::create(vm, codeBlock->functionDecl(bytecode.m_functionDecl), scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_async_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewAsyncFunc>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    slowPathLogF("Creating async function!\n");
    LLINT_RETURN(JSAsyncFunction::create(vm, codeBlock->functionDecl(bytecode.m_functionDecl), scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_async_generator_func)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpNewAsyncGeneratorFunc>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    slowPathLogF("Creating async generator function!\n");
    LLINT_RETURN(JSAsyncGeneratorFunction::create(vm, codeBlock->functionDecl(bytecode.m_functionDecl), scope));
}
    
LLINT_SLOW_PATH_DECL(slow_path_new_func_exp)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpNewFuncExp>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.m_functionDecl);
    
    LLINT_RETURN(JSFunction::create(vm, executable, scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_generator_func_exp)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpNewGeneratorFuncExp>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.m_functionDecl);

    LLINT_RETURN(JSGeneratorFunction::create(vm, executable, scope));
}

LLINT_SLOW_PATH_DECL(slow_path_new_async_func_exp)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpNewAsyncFuncExp>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.m_functionDecl);
    
    LLINT_RETURN(JSAsyncFunction::create(vm, executable, scope));
}
    
LLINT_SLOW_PATH_DECL(slow_path_new_async_generator_func_exp)
{
    LLINT_BEGIN();
        
    auto bytecode = pc->as<OpNewAsyncGeneratorFuncExp>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    FunctionExecutable* executable = codeBlock->functionExpr(bytecode.m_functionDecl);
        
    LLINT_RETURN(JSAsyncGeneratorFunction::create(vm, executable, scope));
}

LLINT_SLOW_PATH_DECL(slow_path_set_function_name)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpSetFunctionName>();
    JSFunction* func = jsCast<JSFunction*>(getNonConstantOperand(callFrame, bytecode.m_function));
    JSValue name = getOperand(callFrame, bytecode.m_name);
    func->setFunctionName(globalObject, name);
    LLINT_END();
}

static SlowPathReturnType handleHostCall(CallFrame* calleeFrame, JSValue callee, CodeSpecializationKind kind)
{
    slowPathLog("Performing host call.\n");
    
    CallFrame* callFrame = calleeFrame->callerFrame();
    CodeBlock* callerCodeBlock = callFrame->codeBlock();
    JSGlobalObject* globalObject = callerCodeBlock->globalObject();
    VM& vm = callerCodeBlock->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    calleeFrame->setCodeBlock(nullptr);
    calleeFrame->clearReturnPC();

    if (kind == CodeForCall) {
        auto callData = getCallData(vm, callee);
        ASSERT(callData.type != CallData::Type::JS);

        if (callData.type == CallData::Type::Native) {
            SlowPathFrameTracer tracer(vm, calleeFrame);
            calleeFrame->setCallee(asObject(callee));
            vm.hostCallReturnValue = JSValue::decode(callData.native.function(asObject(callee)->globalObject(vm), calleeFrame));
            LLINT_CALL_RETURN(globalObject, calleeFrame, LLInt::getCodePtr(getHostCallReturnValue), CFunctionPtrTag);
        }
        
        slowPathLog("Call callee is not a function: ", callee, "\n");

        ASSERT(callData.type == CallData::Type::None);
        LLINT_CALL_THROW(globalObject, createNotAFunctionError(globalObject, callee));
    }

    ASSERT(kind == CodeForConstruct);

    auto constructData = getConstructData(vm, callee);
    ASSERT(constructData.type != CallData::Type::JS);

    if (constructData.type == CallData::Type::Native) {
        SlowPathFrameTracer tracer(vm, calleeFrame);
        calleeFrame->setCallee(asObject(callee));
        vm.hostCallReturnValue = JSValue::decode(constructData.native.function(asObject(callee)->globalObject(vm), calleeFrame));
        LLINT_CALL_RETURN(globalObject, calleeFrame, LLInt::getCodePtr(getHostCallReturnValue), CFunctionPtrTag);
    }
    
    slowPathLog("Constructor callee is not a function: ", callee, "\n");

    ASSERT(constructData.type == CallData::Type::None);
    LLINT_CALL_THROW(globalObject, createNotAConstructorError(globalObject, callee));
}

inline SlowPathReturnType setUpCall(CallFrame* calleeFrame, CodeSpecializationKind kind, JSValue calleeAsValue, LLIntCallLinkInfo* callLinkInfo = nullptr)
{
    CallFrame* callFrame = calleeFrame->callerFrame();
    CodeBlock* callerCodeBlock = callFrame->codeBlock();
    JSGlobalObject* globalObject = callerCodeBlock->globalObject();
    VM& vm = callerCodeBlock->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    slowPathLogF("Performing call with recorded PC = %p\n", callFrame->currentVPC());
    
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell) {
        if (auto* internalFunction = jsDynamicCast<InternalFunction*>(vm, calleeAsValue)) {
            MacroAssemblerCodePtr<JSEntryPtrTag> codePtr = vm.getCTIInternalFunctionTrampolineFor(kind);
            ASSERT(!!codePtr);

            if (!LLINT_ALWAYS_ACCESS_SLOW && callLinkInfo) {
                ConcurrentJSLocker locker(callerCodeBlock->m_lock);
                callLinkInfo->link(vm, callerCodeBlock, internalFunction, codePtr);
            }

            assertIsTaggedWith(codePtr.executableAddress(), JSEntryPtrTag);
            LLINT_CALL_RETURN(globalObject, calleeFrame, codePtr.executableAddress(), JSEntryPtrTag);
        }
        RELEASE_AND_RETURN(throwScope, handleHostCall(calleeFrame, calleeAsValue, kind));
    }
    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
    CodeBlock* codeBlock = nullptr;
    if (executable->isHostFunction())
        codePtr = executable->entrypointFor(kind, MustCheckArity);
    else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct)
            LLINT_CALL_THROW(globalObject, createNotAConstructorError(globalObject, callee));

        CodeBlock** codeBlockSlot = calleeFrame->addressOfCodeBlock();
        Exception* error = functionExecutable->prepareForExecution<FunctionExecutable>(vm, callee, scope, kind, *codeBlockSlot);
        EXCEPTION_ASSERT(throwScope.exception() == error);
        if (UNLIKELY(error))
            LLINT_CALL_THROW(globalObject, error);
        codeBlock = *codeBlockSlot;
        ASSERT(codeBlock);
        ArityCheckMode arity;
        if (calleeFrame->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()))
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = functionExecutable->entrypointFor(kind, arity);
    }

    ASSERT(!!codePtr);
    
    if (!LLINT_ALWAYS_ACCESS_SLOW && callLinkInfo) {
        ConcurrentJSLocker locker(callerCodeBlock->m_lock);
        callLinkInfo->link(vm, callerCodeBlock, callee, codePtr);
        if (codeBlock)
            codeBlock->linkIncomingCall(callFrame, callLinkInfo);
    }

    assertIsTaggedWith(codePtr.executableAddress(), JSEntryPtrTag);
    LLINT_CALL_RETURN(globalObject, calleeFrame, codePtr.executableAddress(), JSEntryPtrTag);
}

template<typename Op>
inline SlowPathReturnType genericCall(CodeBlock* codeBlock, CallFrame* callFrame, Op&& bytecode, CodeSpecializationKind kind, unsigned checkpointIndex = 0)
{
    // This needs to:
    // - Set up a call frame.
    // - Figure out what to call and compile it if necessary.
    // - If possible, link the call's inline cache.
    // - Return a tuple of machine code address to call and the new call frame.
    
    JSValue calleeAsValue = getOperand(callFrame, calleeFor(bytecode, checkpointIndex));
    
    CallFrame* calleeFrame = callFrame - stackOffsetInRegistersForCall(bytecode, checkpointIndex);
    
    calleeFrame->setArgumentCountIncludingThis(argumentCountIncludingThisFor(bytecode, checkpointIndex));
    calleeFrame->uncheckedR(VirtualRegister(CallFrameSlot::callee)) = calleeAsValue;
    calleeFrame->setCallerFrame(callFrame);
    
    auto& metadata = bytecode.metadata(codeBlock);
    return setUpCall(calleeFrame, kind, calleeAsValue, &callLinkInfoFor(metadata, checkpointIndex));
}

LLINT_SLOW_PATH_DECL(slow_path_call)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(globalObject);
    RELEASE_AND_RETURN(throwScope, genericCall(codeBlock, callFrame, pc->as<OpCall>(), CodeForCall));
}

LLINT_SLOW_PATH_DECL(slow_path_tail_call)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(globalObject);
    RELEASE_AND_RETURN(throwScope, genericCall(codeBlock, callFrame, pc->as<OpTailCall>(), CodeForCall));
}

LLINT_SLOW_PATH_DECL(slow_path_construct)
{
    LLINT_BEGIN_NO_SET_PC();
    UNUSED_PARAM(globalObject);
    RELEASE_AND_RETURN(throwScope, genericCall(codeBlock, callFrame, pc->as<OpConstruct>(), CodeForConstruct));
}

LLINT_SLOW_PATH_DECL(slow_path_iterator_open_call)
{
    LLINT_BEGIN();
    UNUSED_PARAM(globalObject);

    RELEASE_AND_RETURN(throwScope, genericCall(codeBlock, callFrame, pc->as<OpIteratorOpen>(), CodeForCall, OpIteratorOpen::symbolCall));
}

LLINT_SLOW_PATH_DECL(slow_path_iterator_next_call)
{
    LLINT_BEGIN();
    UNUSED_PARAM(globalObject);

    RELEASE_AND_RETURN(throwScope, genericCall(codeBlock, callFrame, pc->as<OpIteratorNext>(), CodeForCall, OpIteratorNext::computeNext));
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
        numUsedStackSlots = -bytecode.m_firstFree.offset();
        arguments = getOperand(callFrame, bytecode.m_arguments);
        firstVarArg = bytecode.m_firstVarArg;
        break;
    }
    case op_tail_call_varargs: {
        auto bytecode = pc->as<OpTailCallVarargs>();
        numUsedStackSlots = -bytecode.m_firstFree.offset();
        arguments = getOperand(callFrame, bytecode.m_arguments);
        firstVarArg = bytecode.m_firstVarArg;
        break;
    }
    case op_construct_varargs: {
        auto bytecode = pc->as<OpConstructVarargs>();
        numUsedStackSlots = -bytecode.m_firstFree.offset();
        arguments = getOperand(callFrame, bytecode.m_arguments);
        firstVarArg = bytecode.m_firstVarArg;
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    unsigned length = sizeFrameForVarargs(globalObject, callFrame, vm, arguments, numUsedStackSlots, firstVarArg);
    LLINT_CALL_CHECK_EXCEPTION(globalObject);
    
    CallFrame* calleeFrame = calleeFrameForVarargs(callFrame, numUsedStackSlots, length + 1);
    vm.varargsLength = length;
    vm.newCallFrameReturnValue = calleeFrame;

    LLINT_RETURN_CALLEE_FRAME(calleeFrame);
}

LLINT_SLOW_PATH_DECL(slow_path_size_frame_for_forward_arguments)
{
    LLINT_BEGIN();
    // This needs to:
    // - Set up a call frame with the same arguments as the current frame.

    auto bytecode = pc->as<OpTailCallForwardArguments>();
    unsigned numUsedStackSlots = -bytecode.m_firstFree.offset();

    unsigned arguments = sizeFrameForForwardArguments(globalObject, callFrame, vm, numUsedStackSlots);
    LLINT_CALL_CHECK_EXCEPTION(globalObject);

    CallFrame* calleeFrame = calleeFrameForVarargs(callFrame, numUsedStackSlots, arguments + 1);

    vm.varargsLength = arguments;
    vm.newCallFrameReturnValue = calleeFrame;

    LLINT_RETURN_CALLEE_FRAME(calleeFrame);
}

enum class SetArgumentsWith {
    Object,
    CurrentArguments
};

template<typename Op>
inline SlowPathReturnType varargsSetup(CallFrame* callFrame, const Instruction* pc, CodeSpecializationKind kind, SetArgumentsWith set)
{
    LLINT_BEGIN_NO_SET_PC();

    // This needs to:
    // - Figure out what to call and compile it if necessary.
    // - Return a tuple of machine code address to call and the new call frame.

    auto bytecode = pc->as<Op>();
    JSValue calleeAsValue = getOperand(callFrame, bytecode.m_callee);

    CallFrame* calleeFrame = vm.newCallFrameReturnValue;

    if (set == SetArgumentsWith::Object) {
        setupVarargsFrameAndSetThis(globalObject, callFrame, calleeFrame, getOperand(callFrame, bytecode.m_thisValue), getOperand(callFrame, bytecode.m_arguments), bytecode.m_firstVarArg, vm.varargsLength);
        LLINT_CALL_CHECK_EXCEPTION(globalObject);
    } else
        setupForwardArgumentsFrameAndSetThis(globalObject, callFrame, calleeFrame, getOperand(callFrame, bytecode.m_thisValue), vm.varargsLength);

    calleeFrame->setCallerFrame(callFrame);
    calleeFrame->uncheckedR(VirtualRegister(CallFrameSlot::callee)) = calleeAsValue;
    callFrame->setCurrentVPC(pc);

    RELEASE_AND_RETURN(throwScope, setUpCall(calleeFrame, kind, calleeAsValue));
}

LLINT_SLOW_PATH_DECL(slow_path_call_varargs)
{
    return varargsSetup<OpCallVarargs>(callFrame, pc, CodeForCall, SetArgumentsWith::Object);
}

LLINT_SLOW_PATH_DECL(slow_path_tail_call_varargs)
{
    return varargsSetup<OpTailCallVarargs>(callFrame, pc, CodeForCall, SetArgumentsWith::Object);
}

LLINT_SLOW_PATH_DECL(slow_path_tail_call_forward_arguments)
{
    return varargsSetup<OpTailCallForwardArguments>(callFrame, pc, CodeForCall, SetArgumentsWith::CurrentArguments);
}

LLINT_SLOW_PATH_DECL(slow_path_construct_varargs)
{
    return varargsSetup<OpConstructVarargs>(callFrame, pc, CodeForConstruct, SetArgumentsWith::Object);
}

inline SlowPathReturnType commonCallEval(CallFrame* callFrame, const Instruction* pc, MacroAssemblerCodePtr<JSEntryPtrTag> returnPoint)
{
    LLINT_BEGIN_NO_SET_PC();
    auto bytecode = pc->as<OpCallEval>();
    JSValue calleeAsValue = getNonConstantOperand(callFrame, bytecode.m_callee);
    
    CallFrame* calleeFrame = callFrame - bytecode.m_argv;
    
    calleeFrame->setArgumentCountIncludingThis(bytecode.m_argc);
    calleeFrame->setCallerFrame(callFrame);
    calleeFrame->uncheckedR(VirtualRegister(CallFrameSlot::callee)) = calleeAsValue;
    calleeFrame->setReturnPC(returnPoint.executableAddress());
    calleeFrame->setCodeBlock(nullptr);
    callFrame->setCurrentVPC(pc);
    
    if (!isHostFunction(calleeAsValue, globalFuncEval))
        RELEASE_AND_RETURN(throwScope, setUpCall(calleeFrame, CodeForCall, calleeAsValue));
    
    vm.hostCallReturnValue = eval(globalObject, calleeFrame, bytecode.m_ecmaMode);
    LLINT_CALL_RETURN(globalObject, calleeFrame, LLInt::getCodePtr(getHostCallReturnValue), CFunctionPtrTag);
}
    
LLINT_SLOW_PATH_DECL(slow_path_call_eval)
{
    return commonCallEval(callFrame, pc, LLInt::getCodePtr<JSEntryPtrTag>(llint_generic_return_point));
}

LLINT_SLOW_PATH_DECL(slow_path_call_eval_wide16)
{
    return commonCallEval(callFrame, pc, LLInt::getWide16CodePtr<JSEntryPtrTag>(llint_generic_return_point));
}

LLINT_SLOW_PATH_DECL(slow_path_call_eval_wide32)
{
    return commonCallEval(callFrame, pc, LLInt::getWide32CodePtr<JSEntryPtrTag>(llint_generic_return_point));
}

LLINT_SLOW_PATH_DECL(slow_path_strcat)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpStrcat>();
    LLINT_RETURN(jsStringFromRegisterArray(globalObject, &callFrame->uncheckedR(bytecode.m_src), bytecode.m_count));
}

LLINT_SLOW_PATH_DECL(slow_path_to_primitive)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpToPrimitive>();
    LLINT_RETURN(getOperand(callFrame, bytecode.m_src).toPrimitive(globalObject));
}

LLINT_SLOW_PATH_DECL(slow_path_throw)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpThrow>();
    LLINT_THROW(getOperand(callFrame, bytecode.m_value));
}

LLINT_SLOW_PATH_DECL(slow_path_handle_traps)
{
    LLINT_BEGIN_NO_SET_PC();
    ASSERT(vm.needTrapHandling());
    vm.handleTraps(globalObject, callFrame);
    UNUSED_PARAM(pc);
    LLINT_RETURN_TWO(throwScope.exception(), globalObject);
}

LLINT_SLOW_PATH_DECL(slow_path_debug)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpDebug>();
    vm.interpreter->debug(callFrame, bytecode.m_debugHookType);
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_handle_exception)
{
    VM& vm = callFrame->deprecatedVM();
    SlowPathFrameTracer tracer(vm, callFrame);
    genericUnwind(vm, callFrame);
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_get_from_scope)
{
    LLINT_BEGIN();
    auto bytecode = pc->as<OpGetFromScope>();
    auto& metadata = bytecode.metadata(codeBlock);
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* scope = jsCast<JSObject*>(getNonConstantOperand(callFrame, bytecode.m_scope));

    // ModuleVar is always converted to ClosureVar for get_from_scope.
    ASSERT(metadata.m_getPutInfo.resolveType() != ModuleVar);

    LLINT_RETURN(scope->getPropertySlot(globalObject, ident, [&] (bool found, PropertySlot& slot) -> JSValue {
        if (!found) {
            if (metadata.m_getPutInfo.resolveMode() == ThrowIfNotFound)
                return throwException(globalObject, throwScope, createUndefinedVariableError(globalObject, ident));
            return jsUndefined();
        }

        JSValue result = JSValue();
        if (scope->isGlobalLexicalEnvironment()) {
            // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
            result = slot.getValue(globalObject, ident);
            if (result == jsTDZValue())
                return throwException(globalObject, throwScope, createTDZError(globalObject));
        }

        CommonSlowPaths::tryCacheGetFromScopeGlobal(globalObject, codeBlock, vm, bytecode, scope, slot, ident);

        if (!result)
            return slot.getValue(globalObject, ident);
        return result;
    }));
}

LLINT_SLOW_PATH_DECL(slow_path_put_to_scope)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(codeBlock);
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSObject* scope = jsCast<JSObject*>(getNonConstantOperand(callFrame, bytecode.m_scope));
    JSValue value = getOperand(callFrame, bytecode.m_value);
    if (metadata.m_getPutInfo.resolveType() == LocalClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(scope);
        environment->variableAt(ScopeOffset(metadata.m_operand)).set(vm, environment, value);
        
        // Have to do this *after* the write, because if this puts the set into IsWatched, then we need
        // to have already changed the value of the variable. Otherwise we might watch and constant-fold
        // to the Undefined value from before the assignment.
        if (metadata.m_watchpointSet)
            metadata.m_watchpointSet->touch(vm, "Executed op_put_scope<LocalClosureVar>");
        LLINT_END();
    }

    bool hasProperty = scope->hasProperty(globalObject, ident);
    LLINT_CHECK_EXCEPTION();
    if (hasProperty
        && scope->isGlobalLexicalEnvironment()
        && !isInitialization(metadata.m_getPutInfo.initializationMode())) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, globalObject, ident, slot);
        if (slot.getValue(globalObject, ident) == jsTDZValue())
            LLINT_THROW(createTDZError(globalObject));
    }

    if (metadata.m_getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty)
        LLINT_THROW(createUndefinedVariableError(globalObject, ident));

    PutPropertySlot slot(scope, metadata.m_getPutInfo.ecmaMode().isStrict(), PutPropertySlot::UnknownContext, isInitialization(metadata.m_getPutInfo.initializationMode()));
    scope->methodTable(vm)->put(scope, globalObject, ident, value, slot);
    
    CommonSlowPaths::tryCachePutToScopeGlobal(globalObject, codeBlock, bytecode, scope, slot, ident);

    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_check_if_exception_is_uncatchable_and_notify_profiler)
{
    LLINT_BEGIN();
    UNUSED_PARAM(globalObject);
    RELEASE_ASSERT(!!throwScope.exception());

    if (isTerminatedExecutionException(vm, throwScope.exception()))
        LLINT_RETURN_TWO(pc, bitwise_cast<void*>(static_cast<uintptr_t>(1)));
    LLINT_RETURN_TWO(pc, nullptr);
}

LLINT_SLOW_PATH_DECL(slow_path_log_shadow_chicken_prologue)
{
    LLINT_BEGIN();
    
    auto bytecode = pc->as<OpLogShadowChickenPrologue>();
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    ShadowChicken* shadowChicken = vm.shadowChicken();
    RELEASE_ASSERT(shadowChicken);
    shadowChicken->log(vm, callFrame, ShadowChicken::Packet::prologue(callFrame->jsCallee(), callFrame, callFrame->callerFrame(), scope));
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_log_shadow_chicken_tail)
{
    LLINT_BEGIN();

    auto bytecode = pc->as<OpLogShadowChickenTail>();
    JSValue thisValue = getNonConstantOperand(callFrame, bytecode.m_thisValue);
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    
    CallSiteIndex callSiteIndex(BytecodeIndex(codeBlock->bytecodeOffset(pc)));

    ShadowChicken* shadowChicken = vm.shadowChicken();
    RELEASE_ASSERT(shadowChicken);
    shadowChicken->log(vm, callFrame, ShadowChicken::Packet::tail(callFrame, thisValue, scope, codeBlock, callSiteIndex));
    
    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_profile_catch)
{
    LLINT_BEGIN();

    codeBlock->ensureCatchLivenessIsComputedForBytecodeIndex(callFrame->bytecodeIndex());

    auto bytecode = pc->as<OpCatch>();
    auto& metadata = bytecode.metadata(codeBlock);
    metadata.m_buffer->forEach([&] (ValueProfileAndVirtualRegister& profile) {
        profile.m_buckets[0] = JSValue::encode(callFrame->uncheckedR(profile.m_operand).jsValue());
    });

    LLINT_END();
}

LLINT_SLOW_PATH_DECL(slow_path_super_sampler_begin)
{
    // FIXME: It seems like we should be able to do this in asm but llint doesn't seem to like global variables.
    // See: https://bugs.webkit.org/show_bug.cgi?id=179438
    UNUSED_PARAM(callFrame);
    g_superSamplerCount++;
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_super_sampler_end)
{
    // FIXME: It seems like we should be able to do this in asm but llint doesn't seem to like global variables.
    // See: https://bugs.webkit.org/show_bug.cgi?id=179438
    UNUSED_PARAM(callFrame);
    g_superSamplerCount--;
    LLINT_END_IMPL();
}

LLINT_SLOW_PATH_DECL(slow_path_out_of_line_jump_target)
{
    pc = callFrame->codeBlock()->outOfLineJumpTarget(pc);
    LLINT_END_IMPL();
}

template<typename Opcode>
static void handleVarargsCheckpoint(VM& vm, CallFrame* callFrame, JSGlobalObject* globalObject, const Opcode& bytecode, CheckpointOSRExitSideState& sideState)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned argumentCountIncludingThis = sideState.tmps[Opcode::argCountIncludingThis].asUInt32();
    unsigned firstVarArg = bytecode.m_firstVarArg;

    MarkedArgumentBuffer args;
    args.fill(argumentCountIncludingThis - 1, [&] (JSValue* buffer) {
        loadVarargs(globalObject, buffer, callFrame->r(bytecode.m_arguments).jsValue(), firstVarArg, argumentCountIncludingThis - 1);
    });
    if (args.hasOverflowed()) {
        throwStackOverflowError(globalObject, scope);
        return;
    }
    
    RETURN_IF_EXCEPTION(scope, void());

    JSValue result;
    if (Opcode::opcodeID != op_construct_varargs)
        result = call(globalObject, getOperand(callFrame, bytecode.m_callee), getOperand(callFrame, bytecode.m_thisValue), args, "");
    else
        result = construct(globalObject, getOperand(callFrame, bytecode.m_callee), getOperand(callFrame, bytecode.m_thisValue), args, "");

    RETURN_IF_EXCEPTION(scope, void());
    callFrame->uncheckedR(bytecode.m_dst) = result;
}

static void handleIteratorOpenCheckpoint(VM& vm, CallFrame* callFrame, JSGlobalObject* globalObject, const OpIteratorOpen& bytecode)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue iterator = callFrame->uncheckedR(bytecode.m_iterator).jsValue();
    if (!iterator.isObject()) {
        throwVMTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
        return;
    }

    JSValue next = iterator.get(globalObject, vm.propertyNames->next);
    RETURN_IF_EXCEPTION(scope, void());
    callFrame->uncheckedR(bytecode.m_next) = next;
}

static void handleIteratorNextCheckpoint(VM& vm, CallFrame* callFrame, JSGlobalObject* globalObject, const OpIteratorNext& bytecode, CheckpointOSRExitSideState& sideState)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned checkpointIndex = sideState.bytecodeIndex.checkpoint();

    auto& valueRegister = callFrame->uncheckedR(bytecode.m_value);
    auto iteratorResultObject = sideState.tmps[OpIteratorNext::nextResult];
    auto next = callFrame->uncheckedR(bytecode.m_next).jsValue();    
    RELEASE_ASSERT_WITH_MESSAGE(next, "We should not OSR exit to a checkpoint for fast cases.");

    if (!iteratorResultObject.isObject()) {
        throwVMTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
        return;
    }

    auto& doneRegister = callFrame->uncheckedR(bytecode.m_done);
    if (checkpointIndex == OpIteratorNext::getDone) {
        doneRegister = iteratorResultObject.get(globalObject, vm.propertyNames->done);
        RETURN_IF_EXCEPTION(scope, void());
    }

    scope.release();
    if (doneRegister.jsValue().toBoolean(globalObject))
        valueRegister = jsUndefined();
    else
        valueRegister = iteratorResultObject.get(globalObject, vm.propertyNames->value);
}

inline SlowPathReturnType dispatchToNextInstruction(ThrowScope& scope, CodeBlock* codeBlock, InstructionStream::Ref pc)
{
    if (scope.exception())
        return encodeResult(returnToThrow(scope.vm()), nullptr);

    if (Options::forceOSRExitToLLInt() || codeBlock->jitType() == JITType::InterpreterThunk) {
        const Instruction* nextPC = pc.next().ptr();
        auto nextBytecode = LLInt::getCodePtr<JSEntryPtrTag>(*pc.next().ptr());
        return encodeResult(nextPC, nextBytecode.executableAddress());
    }

#if ENABLE(JIT)
    ASSERT(codeBlock->jitType() == JITType::BaselineJIT);
    BytecodeIndex nextBytecodeIndex = pc.next().index();
    auto nextBytecode = codeBlock->jitCodeMap().find(nextBytecodeIndex);
    return encodeResult(nullptr, nextBytecode.executableAddress());
#endif
    RELEASE_ASSERT_NOT_REACHED();
}

extern "C" SlowPathReturnType llint_slow_path_checkpoint_osr_exit_from_inlined_call(CallFrame* callFrame, EncodedJSValue result)
{
    // Since all our calling checkpoints do right now is move result into our dest we can just do that here and return.
    CodeBlock* codeBlock = callFrame->codeBlock();
    VM& vm = codeBlock->vm();
    SlowPathFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::unique_ptr<CheckpointOSRExitSideState> sideState = vm.popCheckpointOSRSideState(callFrame);
    BytecodeIndex bytecodeIndex = sideState->bytecodeIndex;
    ASSERT(bytecodeIndex.checkpoint());

    auto pc = codeBlock->instructions().at(bytecodeIndex);
    JSGlobalObject* globalObject = codeBlock->globalObject();

    auto opcode = pc->opcodeID();
    switch (opcode) {
    case op_call_varargs: {
        callFrame->uncheckedR(pc->as<OpCallVarargs>().m_dst) = JSValue::decode(result);
        break;
    }
    case op_construct_varargs: {
        callFrame->uncheckedR(pc->as<OpConstructVarargs>().m_dst) = JSValue::decode(result);
        break;
    }
    // op_tail_call_varargs should never return if the thing it was calling was inlined.

    case op_iterator_open: {
        ASSERT(bytecodeIndex.checkpoint() == OpIteratorOpen::getNext);
        callFrame->uncheckedR(destinationFor(pc->as<OpIteratorOpen>(), bytecodeIndex.checkpoint()).virtualRegister()) = JSValue::decode(result);
        break;
    }
    case op_iterator_next: {
        callFrame->uncheckedR(destinationFor(pc->as<OpIteratorNext>(), bytecodeIndex.checkpoint()).virtualRegister()) = JSValue::decode(result);
        if (bytecodeIndex.checkpoint() == OpIteratorNext::getValue)
            break;
        ASSERT(bytecodeIndex.checkpoint() == OpIteratorNext::getDone);
        sideState->bytecodeIndex = bytecodeIndex.withCheckpoint(OpIteratorNext::getValue);
        handleIteratorNextCheckpoint(vm, callFrame, globalObject, pc->as<OpIteratorNext>(), *sideState.get());
        break;
    }

    default:
        CRASH_WITH_INFO(opcode);
        break;
    }

    return dispatchToNextInstruction(scope, codeBlock, pc);
}

extern "C" SlowPathReturnType llint_slow_path_checkpoint_osr_exit(CallFrame* callFrame, EncodedJSValue /* needed for cCall2 in CLoop */)
{
    CodeBlock* codeBlock = callFrame->codeBlock();
    VM& vm = codeBlock->vm();
    SlowPathFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = codeBlock->globalObject();

    std::unique_ptr<CheckpointOSRExitSideState> sideState = vm.popCheckpointOSRSideState(callFrame);
    BytecodeIndex bytecodeIndex = sideState->bytecodeIndex;
    ASSERT(bytecodeIndex.checkpoint());

    auto pc = codeBlock->instructions().at(bytecodeIndex);

    auto opcode = pc->opcodeID();
    switch (opcode) {
    case op_call_varargs:
        handleVarargsCheckpoint(vm, callFrame, globalObject, pc->as<OpCallVarargs>(), *sideState.get());
        break;
    case op_construct_varargs:
        handleVarargsCheckpoint(vm, callFrame, globalObject, pc->as<OpConstructVarargs>(), *sideState.get());
        break;
    case op_tail_call_varargs:
        ASSERT_WITH_MESSAGE(pc.next()->opcodeID() == op_ret || pc.next()->opcodeID() == op_jmp, "We strongly assume all tail calls are followed by an op_ret (or sometimes a jmp to a ret).");
        handleVarargsCheckpoint(vm, callFrame, globalObject, pc->as<OpTailCallVarargs>(), *sideState.get());
        break;

    case op_iterator_open: {
        handleIteratorOpenCheckpoint(vm, callFrame, globalObject, pc->as<OpIteratorOpen>());
        break;
    }
    case op_iterator_next: {
        handleIteratorNextCheckpoint(vm, callFrame, globalObject, pc->as<OpIteratorNext>(), *sideState.get());
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return dispatchToNextInstruction(scope, codeBlock, pc);
}

extern "C" SlowPathReturnType llint_throw_stack_overflow_error(VM* vm, ProtoCallFrame* protoFrame)
{
    CallFrame* callFrame = vm->topCallFrame;
    auto scope = DECLARE_THROW_SCOPE(*vm);
    JSGlobalObject* globalObject = nullptr;
    if (callFrame)
        globalObject = callFrame->lexicalGlobalObject(*vm);
    else
        globalObject = protoFrame->callee()->globalObject(*vm);
    throwStackOverflowError(globalObject, scope);
    return encodeResult(nullptr, nullptr);
}

#if ENABLE(C_LOOP)
extern "C" SlowPathReturnType llint_stack_check_at_vm_entry(VM* vm, Register* newTopOfStack)
{
    bool success = vm->ensureStackCapacityFor(newTopOfStack);
    return encodeResult(reinterpret_cast<void*>(success), 0);
}
#endif

extern "C" void llint_write_barrier_slow(CallFrame* callFrame, JSCell* cell)
{
    VM& vm = callFrame->codeBlock()->vm();
    vm.heap.writeBarrier(cell);
}

extern "C" SlowPathReturnType llint_check_vm_entry_permission(VM* vm, ProtoCallFrame*)
{
    ASSERT_UNUSED(vm, vm->disallowVMEntryCount);
    if (Options::crashOnDisallowedVMEntry())
        CRASH();

    // Else return, and let doVMEntry return undefined.
    return encodeResult(nullptr, nullptr);
}

extern "C" void llint_dump_value(EncodedJSValue value);
extern "C" void llint_dump_value(EncodedJSValue value)
{
    dataLogLn(JSValue::decode(value));
}

extern "C" NO_RETURN_DUE_TO_CRASH void llint_crash()
{
    CRASH();
}

} } // namespace JSC::LLInt
