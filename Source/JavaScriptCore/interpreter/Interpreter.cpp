/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Interpreter.h"

#include "AbortReason.h"
#include "AbstractModuleRecord.h"
#include "ArgList.h"
#include "BatchedTransitionOptimizer.h"
#include "Bytecodes.h"
#include "CallLinkInfo.h"
#include "CatchScope.h"
#include "CheckpointOSRExitSideState.h"
#include "CodeBlock.h"
#include "Debugger.h"
#include "DirectArguments.h"
#include "DirectEvalCodeCache.h"
#include "EvalCodeBlock.h"
#include "ExecutableBaseInlines.h"
#include "FrameTracers.h"
#include "GlobalObjectMethodTable.h"
#include "InlineCallFrame.h"
#include "InterpreterInlines.h"
#include "JITCode.h"
#include "JSArrayInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSModuleRecord.h"
#include "JSObject.h"
#include "JSRemoteFunction.h"
#include "JSString.h"
#include "JSWebAssemblyException.h"
#include "LLIntThunks.h"
#include "LiteralParser.h"
#include "ModuleProgramCodeBlock.h"
#include "NativeCallee.h"
#include "ProgramCodeBlock.h"
#include "ProtoCallFrameInlines.h"
#include "Register.h"
#include "RegisterAtOffsetList.h"
#include "ScopedArguments.h"
#include "SourceProfiler.h"
#include "StackFrame.h"
#include "StackVisitor.h"
#include "StrictEvalActivation.h"
#include "VMEntryScopeInlines.h"
#include "VMInlines.h"
#include "VMTrapsInlines.h"
#include "VirtualRegister.h"
#include "WasmThunks.h"
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(WEBASSEMBLY)
#include "JSWebAssemblyInstance.h"
#include "WasmContext.h"
#include "WebAssemblyFunction.h"
#endif

namespace JSC {

static inline DirectEvalCodeCache::CacheLookupKey directEvalCacheKey(JSGlobalObject* globalObject, JSString* string, BytecodeIndex bytecodeIndex)
{
    if (string->isRope()) {
        auto rope = string->asRope();
        if (auto source = rope->tryGetLHS("()"_s))
            return DirectEvalCodeCache::CacheLookupKey(source, bytecodeIndex, DirectEvalCodeCache::RopeSuffix::FunctionCall);
        return DirectEvalCodeCache::CacheLookupKey(rope->resolveRope(globalObject).impl(), bytecodeIndex, DirectEvalCodeCache::RopeSuffix::None);
    }
    return DirectEvalCodeCache::CacheLookupKey(string->getValueImpl(), bytecodeIndex, DirectEvalCodeCache::RopeSuffix::None);
}

JSValue eval(CallFrame* callFrame, JSValue thisValue, JSScope* callerScopeChain, CodeBlock* callerBaselineCodeBlock, BytecodeIndex bytecodeIndex, LexicallyScopedFeatures lexicallyScopedFeatures)
{
    JSGlobalObject* globalObject = callerBaselineCodeBlock->globalObject();

    if (callFrame->guaranteedJSValueCallee() != globalObject->evalFunction()) [[unlikely]]
        return { };

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    if (!callFrame->argumentCount())
        return jsUndefined();

    JSValue program = callFrame->uncheckedArgument(0);
    JSString* programString = nullptr;
    bool isTrusted = false;
    if (program.isString()) [[likely]]
        programString = asString(program);
    else {
        if (Options::useTrustedTypes() && program.isObject()) {
            auto* structure = globalObject->trustedScriptStructure();
            if (structure == asObject(program)->structure()) {
                programString = program.toString(globalObject);
                RETURN_IF_EXCEPTION(scope, { });
                isTrusted = true;
            } else {
                auto code = globalObject->globalObjectMethodTable()->codeForEval(globalObject, program);
                RETURN_IF_EXCEPTION(scope, { });
                if (!code.isNull()) {
                    programString = jsString(vm, code);
                    isTrusted = true;
                }
            }
        }

        if (!programString) [[unlikely]]
            return program;
    }

    if (globalObject->trustedTypesEnforcement() != TrustedTypesEnforcement::None && !isTrusted) [[unlikely]] {
        bool canCompileStrings = globalObject->globalObjectMethodTable()->canCompileStrings(globalObject, CompilationType::DirectEval, programString->value(globalObject).data, *vm.emptyList);
        RETURN_IF_EXCEPTION(scope, { });
        if (!canCompileStrings) [[unlikely]] {
            throwException(globalObject, scope, createEvalError(globalObject, "Refused to evaluate a string as JavaScript because this document requires a 'Trusted Type' assignment."_s));
            return { };
        }
    }

    TopCallFrameSetter topCallFrame(vm, callFrame);
    if (!globalObject->evalEnabled() && globalObject->trustedTypesEnforcement() != TrustedTypesEnforcement::EnforcedWithEvalEnabled) [[unlikely]] {
        globalObject->globalObjectMethodTable()->reportViolationForUnsafeEval(globalObject, programString->value(globalObject).data);
        throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
        return { };
    }

    auto cacheKey = directEvalCacheKey(globalObject, programString, bytecodeIndex);
    RETURN_IF_EXCEPTION(scope, { });
    DirectEvalExecutable* eval = callerBaselineCodeBlock->directEvalCodeCache().get(cacheKey);
    if (!eval) {
        auto programSource = programString->value(globalObject).data;
        if (SourceProfiler::g_profilerHook) [[unlikely]] {
            SourceTaintedOrigin sourceTaintedOrigin = computeNewSourceTaintedOriginFromStack(vm, callFrame);
            auto source = makeSource(programSource, callerBaselineCodeBlock->source().provider()->sourceOrigin(), sourceTaintedOrigin);
            SourceProfiler::profile(SourceProfiler::Type::Eval, source);
        }

        if (!(lexicallyScopedFeatures & StrictModeLexicallyScopedFeature)) {
            JSValue parsedValue;
            if (programSource.is8Bit()) {
                LiteralParser<LChar, JSONReviverMode::Disabled> preparser(globalObject, programSource.span8(), SloppyJSON, callerBaselineCodeBlock);
                parsedValue = preparser.tryEval();
            } else {
                LiteralParser<char16_t, JSONReviverMode::Disabled> preparser(globalObject, programSource.span16(), SloppyJSON, callerBaselineCodeBlock);
                parsedValue = preparser.tryEval();

            }
            RETURN_IF_EXCEPTION(scope, { });
            if (parsedValue)
                RELEASE_AND_RETURN(scope, parsedValue);
        }
        
        TDZEnvironment variablesUnderTDZ;
        PrivateNameEnvironment privateNameEnvironment;
        JSScope::collectClosureVariablesUnderTDZ(callerScopeChain, variablesUnderTDZ, privateNameEnvironment);
        SourceTaintedOrigin sourceTaintedOrigin = computeNewSourceTaintedOriginFromStack(vm, callFrame);

        UnlinkedCodeBlock* callerUnlinkedCodeBlock = callerBaselineCodeBlock->unlinkedCodeBlock();

        bool isArrowFunctionContext = callerUnlinkedCodeBlock->isArrowFunction() || callerUnlinkedCodeBlock->isArrowFunctionContext();

        DerivedContextType derivedContextType = callerUnlinkedCodeBlock->derivedContextType();
        if (!isArrowFunctionContext && callerUnlinkedCodeBlock->isClassContext()) {
            derivedContextType = callerUnlinkedCodeBlock->isConstructor()
                ? DerivedContextType::DerivedConstructorContext
                : DerivedContextType::DerivedMethodContext;
        }

        EvalContextType evalContextType;
        if (callerUnlinkedCodeBlock->parseMode() == SourceParseMode::ClassFieldInitializerMode)
            evalContextType = EvalContextType::InstanceFieldEvalContext;
        else if (isFunctionParseMode(callerUnlinkedCodeBlock->parseMode()))
            evalContextType = EvalContextType::FunctionEvalContext;
        else if (callerUnlinkedCodeBlock->codeType() == EvalCode)
            evalContextType = callerUnlinkedCodeBlock->evalContextType();
        else
            evalContextType = EvalContextType::None;

        eval = DirectEvalExecutable::create(globalObject, makeSource(programSource, callerBaselineCodeBlock->source().provider()->sourceOrigin(), sourceTaintedOrigin), lexicallyScopedFeatures, derivedContextType, callerUnlinkedCodeBlock->needsClassFieldInitializer(), callerUnlinkedCodeBlock->privateBrandRequirement(), isArrowFunctionContext, callerBaselineCodeBlock->ownerExecutable()->isInsideOrdinaryFunction(), evalContextType, &variablesUnderTDZ, &privateNameEnvironment);
        EXCEPTION_ASSERT(!!scope.exception() == !eval);
        if (!eval) [[unlikely]]
            return { };

        // Skip the eval cache if tainted since another eval call could have a different taintedness.
        if (sourceTaintedOrigin == SourceTaintedOrigin::Untainted)
            callerBaselineCodeBlock->directEvalCodeCache().set(globalObject, callerBaselineCodeBlock, cacheKey, eval);
    }

    RELEASE_AND_RETURN(scope, vm.interpreter.executeEval(eval, thisValue, callerScopeChain));
}

unsigned sizeOfVarargs(JSGlobalObject* globalObject, JSValue arguments, uint32_t firstVarArgOffset)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!arguments.isCell()) [[unlikely]] {
        if (arguments.isUndefinedOrNull())
            return 0;
        
        throwException(globalObject, scope, createInvalidFunctionApplyParameterError(globalObject, arguments));
        return 0;
    }
    
    JSCell* cell = arguments.asCell();
    unsigned length;
    switch (cell->type()) {
    case DirectArgumentsType:
        length = jsCast<DirectArguments*>(cell)->length(globalObject);
        break;
    case ScopedArgumentsType:
        length = jsCast<ScopedArguments*>(cell)->length(globalObject);
        break;
    case ClonedArgumentsType:
        length = jsCast<ClonedArguments*>(cell)->length(globalObject);
        break;
    case JSImmutableButterflyType:
        length = jsCast<JSImmutableButterfly*>(cell)->length();
        break;
    case StringType:
    case SymbolType:
    case HeapBigIntType:
        throwException(globalObject, scope, createInvalidFunctionApplyParameterError(globalObject,  arguments));
        return 0;
        
    default:
        RELEASE_ASSERT(arguments.isObject());
        length = clampToUnsigned(toLength(globalObject, jsCast<JSObject*>(cell)));
        break;
    }
    RETURN_IF_EXCEPTION(scope, 0);
    
    if (length > maxArguments)
        throwStackOverflowError(globalObject, scope);

    if (length >= firstVarArgOffset)
        length -= firstVarArgOffset;
    else
        length = 0;
    
    return length;
}

unsigned sizeFrameForForwardArguments(JSGlobalObject* globalObject, CallFrame* callFrame, VM& vm, unsigned numUsedStackSlots)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = callFrame->argumentCount();
    CallFrame* calleeFrame = calleeFrameForVarargs(callFrame, numUsedStackSlots, length + 1);
    if (!vm.ensureStackCapacityFor(calleeFrame->registers())) [[unlikely]]
        throwStackOverflowError(globalObject, scope);

    return length;
}

unsigned sizeFrameForVarargs(JSGlobalObject* globalObject, CallFrame* callFrame, VM& vm, JSValue arguments, unsigned numUsedStackSlots, uint32_t firstVarArgOffset)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = sizeOfVarargs(globalObject, arguments, firstVarArgOffset);
    RETURN_IF_EXCEPTION(scope, 0);

    CallFrame* calleeFrame = calleeFrameForVarargs(callFrame, numUsedStackSlots, length + 1);
    if (length > maxArguments || !vm.ensureStackCapacityFor(calleeFrame->registers())) [[unlikely]] {
        throwStackOverflowError(globalObject, scope);
        return 0;
    }
    
    return length;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

void loadVarargs(JSGlobalObject* globalObject, JSValue* firstElementDest, JSValue arguments, uint32_t offset, uint32_t length)
{
    if (!arguments.isCell()) [[unlikely]]
        return;
    if (!length)
        return;

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSCell* cell = arguments.asCell();

    switch (cell->type()) {
    case DirectArgumentsType:
        scope.release();
        jsCast<DirectArguments*>(cell)->copyToArguments(globalObject, firstElementDest, offset, length);
        return;
    case ScopedArgumentsType:
        scope.release();
        jsCast<ScopedArguments*>(cell)->copyToArguments(globalObject, firstElementDest, offset, length);
        return;
    case ClonedArgumentsType:
        scope.release();
        jsCast<ClonedArguments*>(cell)->copyToArguments(globalObject, firstElementDest, offset, length);
        return;
    case JSImmutableButterflyType:
        scope.release();
        jsCast<JSImmutableButterfly*>(cell)->copyToArguments(globalObject, firstElementDest, offset, length);
        return;
    default: {
        ASSERT(arguments.isObject());
        JSObject* object = jsCast<JSObject*>(cell);
        if (isJSArray(object)) {
            scope.release();
            jsCast<JSArray*>(object)->copyToArguments(globalObject, firstElementDest, offset, length);
            return;
        }
        unsigned i;
        for (i = 0; i < length && object->canGetIndexQuickly(i + offset); ++i)
            firstElementDest[i] = object->getIndexQuickly(i + offset);
        for (; i < length; ++i) {
            JSValue value = object->get(globalObject, i + offset);
            RETURN_IF_EXCEPTION(scope, void());
            firstElementDest[i] = value;
        }
        return;
    }
    }
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

void setupVarargsFrame(JSGlobalObject* globalObject, CallFrame* callFrame, CallFrame* newCallFrame, JSValue arguments, uint32_t offset, uint32_t length)
{
    VirtualRegister calleeFrameOffset(newCallFrame - callFrame);
    
    loadVarargs(
        globalObject,
        std::bit_cast<JSValue*>(&callFrame->r(calleeFrameOffset + CallFrame::argumentOffset(0))),
        arguments, offset, length);
    
    newCallFrame->setArgumentCountIncludingThis(length + 1);
}

void setupVarargsFrameAndSetThis(JSGlobalObject* globalObject, CallFrame* callFrame, CallFrame* newCallFrame, JSValue thisValue, JSValue arguments, uint32_t firstVarArgOffset, uint32_t length)
{
    setupVarargsFrame(globalObject, callFrame, newCallFrame, arguments, firstVarArgOffset, length);
    newCallFrame->setThisValue(thisValue);
}

void setupForwardArgumentsFrame(JSGlobalObject*, CallFrame* execCaller, CallFrame* execCallee, uint32_t length)
{
    ASSERT(length == execCaller->argumentCount());
    unsigned offset = execCaller->argumentOffset(0) * sizeof(Register);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memcpy(reinterpret_cast<char*>(execCallee) + offset, reinterpret_cast<char*>(execCaller) + offset, length * sizeof(Register));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    execCallee->setArgumentCountIncludingThis(length + 1);
}

void setupForwardArgumentsFrameAndSetThis(JSGlobalObject* globalObject, CallFrame* execCaller, CallFrame* execCallee, JSValue thisValue, uint32_t length)
{
    setupForwardArgumentsFrame(globalObject, execCaller, execCallee, length);
    execCallee->setThisValue(thisValue);
}

Interpreter::Interpreter()
#if ENABLE(C_LOOP)
    : m_cloopStack(vm())
#endif
{
#if ASSERT_ENABLED
    static std::once_flag assertOnceKey;
    std::call_once(assertOnceKey, [] {
        if (g_jscConfig.vmEntryDisallowed)
            return;
        for (unsigned i = 0; i < NUMBER_OF_BYTECODE_IDS; ++i) {
            OpcodeID opcodeID = static_cast<OpcodeID>(i);
            RELEASE_ASSERT(getOpcodeID(getOpcode(opcodeID)) == opcodeID);
        }
    });
#endif // ASSERT_ENABLED
}

Interpreter::~Interpreter() = default;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(COMPUTED_GOTO_OPCODES)
#if !ENABLE(LLINT_EMBEDDED_OPCODE_ID) || ASSERT_ENABLED
UncheckedKeyHashMap<Opcode, OpcodeID>& Interpreter::opcodeIDTable()
{
    static LazyNeverDestroyed<UncheckedKeyHashMap<Opcode, OpcodeID>> opcodeIDTable;

    static std::once_flag initializeKey;
    std::call_once(initializeKey, [&] {
        opcodeIDTable.construct();
        const Opcode* opcodeTable = LLInt::opcodeMap();
        for (unsigned i = 0; i < NUMBER_OF_BYTECODE_IDS; ++i)
            opcodeIDTable->add(opcodeTable[i], static_cast<OpcodeID>(i));
    });

    return opcodeIDTable;
}
#endif // !ENABLE(LLINT_EMBEDDED_OPCODE_ID) || ASSERT_ENABLED
#endif // ENABLE(COMPUTED_GOTO_OPCODES)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#if ASSERT_ENABLED
bool Interpreter::isOpcode(Opcode opcode)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
    return opcode != HashTraits<Opcode>::emptyValue()
        && !HashTraits<Opcode>::isDeletedValue(opcode)
        && opcodeIDTable().contains(opcode);
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}
#endif // ASSERT_ENABLED

void Interpreter::getStackTrace(JSCell* owner, Vector<StackFrame>& results, size_t framesToSkip, size_t maxStackSize, JSCell* caller, JSCell* ownerOfCallLinkInfo, CallLinkInfo* callLinkInfo)
{
    AssertNoGC assertNoGC;
    VM& vm = this->vm();
    CallFrame* callFrame = vm.topCallFrame;
    if (!callFrame || !maxStackSize)
        return;

    size_t skippedFrames = 0;

    auto isImplementationVisibilityPrivate = [&](CodeBlock* codeBlock) {
        if (auto* executable = codeBlock->ownerExecutable())
            return executable->implementationVisibility() != ImplementationVisibility::Public;
        return false;
    };

    // This is OK since we never cause GC inside it (see AssertNoGC).
    auto appendFrame = [&](CodeBlock* codeBlock, BytecodeIndex bytecodeIndex) {
        if (results.size() >= maxStackSize)
            return IterationStatus::Done;

        if (skippedFrames < framesToSkip) {
            skippedFrames++;
            return IterationStatus::Continue;
        }
        if (isImplementationVisibilityPrivate(codeBlock))
            return IterationStatus::Continue;

        results.append(StackFrame(vm, owner, codeBlock, bytecodeIndex));
        return IterationStatus::Continue;
    };

    if (!caller && ownerOfCallLinkInfo && callLinkInfo && callLinkInfo->isTailCall()) {
        // Reconstruct the top frame from CallLinkInfo*
        CodeBlock* codeBlock = jsDynamicCast<CodeBlock*>(ownerOfCallLinkInfo);
        if (codeBlock) {
            CodeOrigin codeOrigin = callLinkInfo->codeOrigin();
            if (codeOrigin.inlineCallFrame()) {
                for (auto currentCodeOrigin = &codeOrigin; currentCodeOrigin && currentCodeOrigin->inlineCallFrame(); currentCodeOrigin = currentCodeOrigin->inlineCallFrame()->getCallerSkippingTailCalls()) {
                    if (appendFrame(baselineCodeBlockForInlineCallFrame(currentCodeOrigin->inlineCallFrame()), currentCodeOrigin->bytecodeIndex()) == IterationStatus::Done)
                        return;
                }
            } else
                if (appendFrame(codeBlock, codeOrigin.bytecodeIndex())  == IterationStatus::Done)
                    return;
        }
    }

    bool foundCaller = !caller;
    StackVisitor::visit(callFrame, vm, [&] (StackVisitor& visitor) ALWAYS_INLINE_LAMBDA {
        if (results.size() >= maxStackSize)
            return IterationStatus::Done;

        if (skippedFrames < framesToSkip) {
            skippedFrames++;
            return IterationStatus::Continue;
        }

        if (!foundCaller) {
            if (!visitor->callee().isNativeCallee() && visitor->callee().asCell() == caller)
                foundCaller = true;
            skippedFrames++;
            return IterationStatus::Continue;
        }

        if (visitor->isImplementationVisibilityPrivate())
            return IterationStatus::Continue;

        if (visitor->isNativeCalleeFrame()) {
            auto* nativeCallee = visitor->callee().asNativeCallee();
            switch (nativeCallee->category()) {
            case NativeCallee::Category::Wasm: {
                results.append(StackFrame(visitor->wasmFunctionIndexOrName()));
                break;
            }
            case NativeCallee::Category::InlineCache: {
                break;
            }
            }
        } else if (!!visitor->codeBlock() && !visitor->codeBlock()->unlinkedCodeBlock()->isBuiltinFunction())
            results.append(StackFrame(vm, owner, visitor->callee().asCell(), visitor->codeBlock(), visitor->bytecodeIndex()));
        else
            results.append(StackFrame(vm, owner, visitor->callee().asCell()));
        return IterationStatus::Continue;
    });
}

String Interpreter::stackTraceAsString(VM& vm, const Vector<StackFrame>& stackTrace)
{
    // FIXME: JSStringJoiner could be more efficient than StringBuilder here.
    StringBuilder builder;
    for (unsigned i = 0; i < stackTrace.size(); i++) {
        builder.append(String(stackTrace[i].toString(vm)));
        if (i != stackTrace.size() - 1)
            builder.append('\n');
    }
    return builder.toString();
}

ALWAYS_INLINE static HandlerInfo* findExceptionHandler(StackVisitor& visitor, CodeBlock* codeBlock, RequiredHandler requiredHandler)
{
    ASSERT(codeBlock);
#if ENABLE(DFG_JIT)
    ASSERT(!visitor->isInlinedDFGFrame());
#endif

    CallFrame* callFrame = visitor->callFrame();
    unsigned exceptionHandlerIndex;
    if (JSC::JITCode::isOptimizingJIT(codeBlock->jitType()))
        exceptionHandlerIndex = callFrame->callSiteIndex().bits();
    else
        exceptionHandlerIndex = callFrame->bytecodeIndex().offset();

    return codeBlock->handlerForIndex(exceptionHandlerIndex, requiredHandler);
}

class GetCatchHandlerFunctor {
public:
    GetCatchHandlerFunctor()
        : m_handler(nullptr)
    {
    }

    HandlerInfo* handler() { return m_handler; }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        visitor.unwindToMachineCodeBlockFrame();

        CodeBlock* codeBlock = visitor->codeBlock();
        if (!codeBlock)
            return IterationStatus::Continue;

        m_handler = findExceptionHandler(visitor, codeBlock, RequiredHandler::CatchHandler);
        if (m_handler)
            return IterationStatus::Done;

        return IterationStatus::Continue;
    }

private:
    mutable HandlerInfo* m_handler;
};

CatchInfo::CatchInfo(const HandlerInfo* handler, CodeBlock* codeBlock)
{
    m_valid = !!handler;
    if (m_valid) {
        m_type = handler->type();
#if ENABLE(JIT)
        m_nativeCode = handler->nativeCode;
#endif

        // handler->target is meaningless for getting a code offset when catching
        // the exception in a DFG/FTL frame. This bytecode target offset could be
        // something that's in an inlined frame, which means an array access
        // with this bytecode offset in the machine frame is utterly meaningless
        // and can cause an overflow. OSR exit properly exits to handler->target
        // in the proper frame.
        if (!JSC::JITCode::isOptimizingJIT(codeBlock->jitType()))
            m_catchPCForInterpreter = { codeBlock->instructions().at(handler->target).ptr() };
        else
            m_catchPCForInterpreter = { static_cast<JSInstruction*>(nullptr) };
    }
}

#if ENABLE(WEBASSEMBLY)
CatchInfo::CatchInfo(const Wasm::HandlerInfo* handler, const Wasm::Callee* callee)
{
    m_valid = !!handler;
    if (m_valid) {
        m_type = HandlerType::Catch;
#if ENABLE(JIT)
        m_nativeCode = handler->m_nativeCode;
        m_nativeCodeForDispatchAndCatch = nullptr;
#endif
        m_catchPCForInterpreter = { static_cast<WasmInstruction*>(nullptr) };
        if (callee->compilationMode() == Wasm::CompilationMode::LLIntMode)
            m_catchPCForInterpreter = { static_cast<const Wasm::LLIntCallee*>(callee)->instructions().at(handler->m_target).ptr() };
        else if (callee->compilationMode() == Wasm::CompilationMode::IPIntMode) {
            m_catchPCForInterpreter = handler->m_target;
            m_catchMetadataPCForInterpreter = handler->m_targetMetadata;
            m_tryDepthForThrow = handler->m_tryDepth;
        } else {
#if ENABLE(JIT)
            m_nativeCode = Wasm::Thunks::singleton().stub(Wasm::catchInWasmThunkGenerator).template retagged<ExceptionHandlerPtrTag>().code();
            m_nativeCodeForDispatchAndCatch = handler->m_nativeCode;
#endif
        }
    }
}
#endif

class UnwindFunctor {
public:
    UnwindFunctor(VM& vm, CallFrame*& callFrame, Exception* exception, JSValue thrownValue, CodeBlock*& codeBlock, CatchInfo& handler, JSRemoteFunction*& seenRemoteFunction)
        : m_vm(vm)
        , m_callFrame(callFrame)
        , m_exception(exception)
        , m_isTermination(vm.isTerminationException(exception))
        , m_codeBlock(codeBlock)
        , m_handler(handler)
        , m_seenRemoteFunction(seenRemoteFunction)
    {
#if ENABLE(WEBASSEMBLY)
        if (!m_isTermination) {
            if (JSWebAssemblyException* wasmException = jsDynamicCast<JSWebAssemblyException*>(thrownValue)) {
                m_catchableFromWasm = true;
                m_wasmTag = &wasmException->tag();
            } else if (ErrorInstance* error = jsDynamicCast<ErrorInstance*>(thrownValue))
                m_catchableFromWasm = error->isCatchableFromWasm();
            else
                m_catchableFromWasm = true;

            // https://webassembly.github.io/exception-handling/js-api/#create-a-host-function
            if (!m_wasmTag)
                m_wasmTag = &Wasm::Tag::jsExceptionTag();
        }
#else
        UNUSED_PARAM(thrownValue);
#endif
    }

    IterationStatus operator()(StackVisitor& visitor) const
    {
        visitor.unwindToMachineCodeBlockFrame();
        m_callFrame = visitor->callFrame();
        m_codeBlock = visitor->codeBlock();

        m_handler.m_valid = false;
        if (m_codeBlock) {
            if (!m_isTermination) {
                m_handler = { findExceptionHandler(visitor, m_codeBlock, RequiredHandler::AnyHandler), m_codeBlock };
                if (m_handler.m_valid)
                    return IterationStatus::Done;
            }
        }

        CalleeBits callee = visitor->callee();
        if (callee.isNativeCallee()) {
            NativeCallee* nativeCallee = callee.asNativeCallee();
            switch (nativeCallee->category()) {
            case NativeCallee::Category::Wasm: {
#if ENABLE(WEBASSEMBLY)
                if (m_catchableFromWasm) {
                    auto* wasmCallee = static_cast<Wasm::Callee*>(nativeCallee);
                    if (wasmCallee->hasExceptionHandlers()) {
                        JSWebAssemblyInstance* instance = m_callFrame->wasmInstance();
                        unsigned exceptionHandlerIndex = m_callFrame->callSiteIndex().bits();
                        auto* wasmHandler = wasmCallee->handlerForIndex(*instance, exceptionHandlerIndex, m_wasmTag.get());
                        m_handler = { wasmHandler, wasmCallee };
                        if (m_handler.m_valid) {
                            if (m_wasmTag == &Wasm::Tag::jsExceptionTag())
                                m_exception->wrapValueForJSTag(instance->globalObject());
                            return IterationStatus::Done;
                        }
                    }
                }
#endif
                break;
            }
            case NativeCallee::Category::InlineCache: {
                break;
            }
            }
        }

        if (!m_callFrame->isNativeCalleeFrame() && JSC::isRemoteFunction(m_callFrame->jsCallee()) && !m_isTermination) {
            // Continue searching for a handler, but mark that a marshalling function was on the stack so that we can
            // translate the exception before jumping to the handler.
            m_seenRemoteFunction = jsCast<JSRemoteFunction*>(m_callFrame->jsCallee());
        }

        JSGlobalObject* globalObject = m_callFrame->lexicalGlobalObject(m_vm);
        notifyDebuggerOfUnwinding(globalObject, m_vm, m_callFrame);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(visitor);

        bool shouldStopUnwinding = visitor->callerIsEntryFrame();
        if (shouldStopUnwinding)
            return IterationStatus::Done;

        return IterationStatus::Continue;
    }

private:
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

    void copyCalleeSavesToEntryFrameCalleeSavesBuffer(StackVisitor& visitor) const
    {
#if ENABLE(ASSEMBLER)
        std::optional<RegisterAtOffsetList> currentCalleeSaves = visitor->calleeSaveRegistersForUnwinding();

        if (!currentCalleeSaves)
            return;

        RegisterAtOffsetList* allCalleeSaves = RegisterSetBuilder::vmCalleeSaveRegisterOffsets();
        auto dontCopyRegisters = RegisterSetBuilder::stackRegisters();
        CPURegister* frame = reinterpret_cast<CPURegister*>(m_callFrame->registers());

        unsigned registerCount = currentCalleeSaves->registerCount();
        VMEntryRecord* record = vmEntryRecord(m_vm.topEntryFrame);
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset currentEntry = currentCalleeSaves->at(i);
            if (dontCopyRegisters.contains(currentEntry.reg(), IgnoreVectors))
                continue;
            RegisterAtOffset* calleeSavesEntry = allCalleeSaves->find(currentEntry.reg());

            if (!calleeSavesEntry) {
                if constexpr (!isARM_THUMB2())
                    RELEASE_ASSERT_NOT_REACHED();
                // This can happen on ARMv7, because there are more callee save
                // registers in the system convention than in the VM convention,
                // so frames generated by Air callees might restore any system
                // callee-save registers and we don't know the correct offset to
                // restore them to in the destination record if the register is
                // not callee-save in the VM convention.

                // Luckily, it is correct for us to drop these--since the
                // Air-generated callee is only expected to preserve the VM
                // callee registers (when called from the VM), it doesn't need
                // to appear to preserve the non-VM-callee-saves if we unwind
                // its frame.
                continue;
            }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
            record->calleeSaveRegistersBuffer[calleeSavesEntry->offsetAsIndex()] = *(frame + currentEntry.offsetAsIndex());
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        }
#else
        UNUSED_PARAM(visitor);
#endif
    }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    ALWAYS_INLINE static void notifyDebuggerOfUnwinding(JSGlobalObject* globalObject, VM& vm, CallFrame* callFrame)
    {
        Debugger* debugger = globalObject->debugger();
        if (!debugger) [[likely]]
            return;

        DeferTermination deferScope(vm);
        auto catchScope = DECLARE_CATCH_SCOPE(vm);

        SuspendExceptionScope scope(vm);
        if (callFrame->isNativeCalleeFrame()
            || (callFrame->callee().isCell() && callFrame->callee().asCell()->inherits<JSFunction>()))
            debugger->unwindEvent(callFrame);
        else
            debugger->didExecuteProgram(callFrame);
        catchScope.assertNoException();
    }

    VM& m_vm;
    CallFrame*& m_callFrame;
    Exception* m_exception;
    bool m_isTermination;
    CodeBlock*& m_codeBlock;
    CatchInfo& m_handler;
#if ENABLE(WEBASSEMBLY)
    mutable RefPtr<const Wasm::Tag> m_wasmTag;
    bool m_catchableFromWasm { false };
#endif

    JSRemoteFunction*& m_seenRemoteFunction;
};

// Replace an exception which passes across a marshalling boundary with a TypeError for its handler's global object.
static void sanitizeRemoteFunctionException(VM& vm, JSRemoteFunction* remoteFunction, Exception* exception)
{
    ASSERT(vm.traps().isDeferringTermination());
    auto scope = DECLARE_THROW_SCOPE(vm);
    ASSERT(exception);
    ASSERT(!vm.isTerminationException(exception));

    JSGlobalObject* globalObject = remoteFunction->globalObject();
    JSValue exceptionValue = exception->value();
    scope.clearException();

    // Avoid user-observable ToString()
    String exceptionString;
    if (exceptionValue.isPrimitive())
        exceptionString = exceptionValue.toWTFString(globalObject);
    else if (exceptionValue.asCell()->inherits<ErrorInstance>())
        exceptionString = static_cast<ErrorInstance*>(exceptionValue.asCell())->sanitizedMessageString(globalObject);

    EXCEPTION_ASSERT(!scope.exception()); // We must not have entered JS at this point

    if (exceptionString.length()) {
        throwVMTypeError(globalObject, scope, exceptionString);
        return;
    }

    throwVMTypeError(globalObject, scope);
}

NEVER_INLINE CatchInfo Interpreter::unwind(VM& vm, CallFrame*& callFrame, Exception* exception)
{
    // If we're unwinding the stack due to a regular exception (not a TerminationException), then
    // we want to use a DeferTerminationForAWhile scope. This is because we want to avoid a
    // TerminationException being raised (due to a concurrent termination request) in the middle
    // of unwinding. The unwinding code only checks if we're handling a TerminationException before
    // it starts unwinding and is not expecting this status to change in the middle. Without the
    // DeferTerminationForAWhile scope, control flow may end up in an exception handler, and effectively
    // "catch" the newly raised TerminationException, which should not be catchable.
    //
    // On the other hand, if we're unwinding the stack due to a TerminationException, we do not need
    // nor want the DeferTerminationForAWhile scope. This is because on exit, DeferTerminationForAWhile
    // will set the VMTraps NeedTermination bit if termination is in progress. The system expects the
    // NeedTermination bit to be have been cleared by VMTraps::handleTraps() once the TerminationException
    // has been raised. Some legacy client apps relies on this and expects to be able to re-enter the
    // VM after it exits due to termination. If the NeedTermination bit is set, upon re-entry, the
    // VM will behave as if a termination request is pending and terminate almost immediately, thereby
    // breaking the legacy client apps.
    //
    // FIXME: Revisit this once we can deprecate this legacy behavior of being able to re-enter the VM
    // after termination.
    std::optional<DeferTerminationForAWhile> deferScope;
    if (!vm.isTerminationException(exception))
        deferScope.emplace(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    ASSERT(reinterpret_cast<void*>(callFrame) != vm.topEntryFrame);
    CodeBlock* codeBlock = callFrame->isNativeCalleeFrame() ? nullptr : callFrame->codeBlock();

    JSValue exceptionValue = exception->value();
    ASSERT(!exceptionValue.isEmpty());
    ASSERT(!exceptionValue.isCell() || exceptionValue.asCell());
    // This shouldn't be possible (hence the assertions), but we're already in the slowest of
    // slow cases, so let's harden against it anyway to be safe.
    if (exceptionValue.isEmpty() || (exceptionValue.isCell() && !exceptionValue.asCell()))
        exceptionValue = jsNull();

    EXCEPTION_ASSERT_UNUSED(scope, scope.exception());

    // Calculate an exception handler vPC, unwinding call frames as necessary.
    CatchInfo catchInfo;
    JSRemoteFunction* seenRemoteFunction = nullptr;
    UnwindFunctor functor(vm, callFrame, exception, exceptionValue, codeBlock, catchInfo, seenRemoteFunction);
    StackVisitor::visit<StackVisitor::TerminateIfTopEntryFrameIsEmpty>(callFrame, vm, functor);

    if (seenRemoteFunction) {
        ASSERT(!vm.isTerminationException(exception));
        sanitizeRemoteFunctionException(vm, seenRemoteFunction, exception);
        exception = scope.exception(); // clear m_needExceptionCheck
    }

    if (vm.hasCheckpointOSRSideState())
        vm.popAllCheckpointOSRSideStateUntil(callFrame);

    return catchInfo;
}

void Interpreter::notifyDebuggerOfExceptionToBeThrown(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame, Exception* exception)
{
    ASSERT(!vm.isTerminationException(exception));

    Debugger* debugger = globalObject->debugger();
    if (debugger && debugger->needsExceptionCallbacks() && !exception->didNotifyInspectorOfThrow()) {
        // This code assumes that if the debugger is enabled then there is no inlining.
        // If that assumption turns out to be false then we'll ignore the inlined call
        // frames.
        // https://bugs.webkit.org/show_bug.cgi?id=121754

        GetCatchHandlerFunctor functor;
        if (callFrame)
            StackVisitor::visit(callFrame, vm, functor);
        HandlerInfo* handler = functor.handler();
        ASSERT(!handler || handler->isCatchHandler());
        bool hasCatchHandler = !!handler;

        debugger->exception(globalObject, callFrame, exception->value(), hasCatchHandler);
    }
    exception->setDidNotifyInspectorOfThrow();
}

NEVER_INLINE JSValue Interpreter::checkVMEntryPermission()
{
    if (Options::crashOnDisallowedVMEntry() || g_jscConfig.vmEntryDisallowed)
        CRASH_WITH_EXTRA_SECURITY_IMPLICATION_AND_INFO(VMEntryDisallowed, "VM entry disallowed"_s);
    return jsUndefined();
}

JSValue Interpreter::executeProgram(const SourceCode& source, JSGlobalObject*, JSObject* thisObj)
{
    VM& vm = this->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    JSScope* scope = thisObj->globalObject()->globalScope();
    JSGlobalObject* globalObject = scope->globalObject();
    JSCallee* globalCallee = globalObject->globalCallee();

    VMEntryScope entryScope(vm, globalObject);

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    if (SourceProfiler::g_profilerHook) [[unlikely]]
        SourceProfiler::profile(SourceProfiler::Type::Program, source);

    ProgramExecutable* program = ProgramExecutable::create(globalObject, source);
    EXCEPTION_ASSERT(throwScope.exception() || program);
    RETURN_IF_EXCEPTION(throwScope, { });

    if (globalObject->globalScopeExtension())
        program->setTaintedByWithScope();

    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());

    if (!vm.isSafeToRecurseSoft()) [[unlikely]]
        return throwStackOverflowError(globalObject, throwScope);

    if (vm.disallowVMEntryCount) [[unlikely]]
        return checkVMEntryPermission();

    // First check if the "program" is actually just a JSON object. If so,
    // we'll handle the JSON object here. Else, we'll handle real JS code
    // below at failedJSONP.

    Vector<JSONPData> JSONPData;
    bool parseResult;
    StringView programSource = program->source().view();
    // Skip JSONP if the program is tainted. We want there to be a tainted
    // frame on the stack in case the program does an eval via a setter.
    if (source.provider()->sourceTaintedOrigin() != SourceTaintedOrigin::Untainted)
        goto failedJSONP;

    if (programSource.isNull())
        return jsUndefined();
    if (programSource.is8Bit()) {
        LiteralParser<LChar, JSONReviverMode::Disabled> literalParser(globalObject, programSource.span8(), JSONP);
        parseResult = literalParser.tryJSONPParse(JSONPData, globalObject->globalObjectMethodTable()->supportsRichSourceInfo(globalObject));
    } else {
        LiteralParser<char16_t, JSONReviverMode::Disabled> literalParser(globalObject, programSource.span16(), JSONP);
        parseResult = literalParser.tryJSONPParse(JSONPData, globalObject->globalObjectMethodTable()->supportsRichSourceInfo(globalObject));
    }

    // FIXME: The patterns to trigger JSONP fast path should be more idiomatic.
    // https://bugs.webkit.org/show_bug.cgi?id=243578
    RETURN_IF_EXCEPTION(throwScope, { });
    if (parseResult) {
        JSValue result;
        for (unsigned entry = 0; entry < JSONPData.size(); entry++) {
            Vector<JSONPPathEntry> JSONPPath;
            JSONPPath.swap(JSONPData[entry].m_path);
            JSValue JSONPValue = JSONPData[entry].m_value.get();
            if (JSONPPath.size() == 1 && JSONPPath[0].m_type == JSONPPathEntryTypeDeclareVar) {
                if (!globalObject->isStructureExtensible()) [[unlikely]]
                    goto failedJSONP;
                globalObject->createGlobalVarBinding<BindingCreationContext::Global>(JSONPPath[0].m_pathEntryName);
                RETURN_IF_EXCEPTION(throwScope, { });
                PutPropertySlot slot(globalObject);
                globalObject->methodTable()->put(globalObject, globalObject, JSONPPath[0].m_pathEntryName, JSONPValue, slot);
                RETURN_IF_EXCEPTION(throwScope, { });
                result = jsUndefined();
                continue;
            }
            JSValue baseObject(globalObject);
            for (unsigned i = 0; i < JSONPPath.size() - 1; i++) {
                ASSERT(JSONPPath[i].m_type != JSONPPathEntryTypeDeclareVar);
                switch (JSONPPath[i].m_type) {
                case JSONPPathEntryTypeDot: {
                    if (i == 0) {
                        RELEASE_ASSERT(baseObject == globalObject);

                        auto doGet = [&] (JSSegmentedVariableObject* scope) {
                            PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
                            if (scope->getPropertySlot(globalObject, JSONPPath[i].m_pathEntryName, slot))
                                return slot.getValue(globalObject, JSONPPath[i].m_pathEntryName);
                            return JSValue();
                        };

                        JSValue result = doGet(globalObject->globalLexicalEnvironment());
                        RETURN_IF_EXCEPTION(throwScope, JSValue());
                        if (result) {
                            baseObject = result;
                            continue;
                        }

                        result = doGet(globalObject);
                        RETURN_IF_EXCEPTION(throwScope, JSValue());
                        if (result) {
                            baseObject = result;
                            continue;
                        }

                        if (entry)
                            return throwException(globalObject, throwScope, createUndefinedVariableError(globalObject, JSONPPath[i].m_pathEntryName));
                        goto failedJSONP;
                    }

                    baseObject = baseObject.get(globalObject, JSONPPath[i].m_pathEntryName);
                    RETURN_IF_EXCEPTION(throwScope, JSValue());
                    continue;
                }
                case JSONPPathEntryTypeLookup: {
                    baseObject = baseObject.get(globalObject, static_cast<unsigned>(JSONPPath[i].m_pathIndex));
                    RETURN_IF_EXCEPTION(throwScope, JSValue());
                    continue;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    return jsUndefined();
                }
            }

            const Identifier& ident = JSONPPath.last().m_pathEntryName;
            if (JSONPPath.size() == 1 && JSONPPath.last().m_type != JSONPPathEntryTypeLookup) {
                RELEASE_ASSERT(baseObject == globalObject);
                JSGlobalLexicalEnvironment* scope = globalObject->globalLexicalEnvironment();
                bool hasProperty = scope->hasProperty(globalObject, ident);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                if (hasProperty) {
                    PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
                    JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, globalObject, ident, slot);
                    if (slot.getValue(globalObject, ident) == jsTDZValue())
                        return throwException(globalObject, throwScope, createTDZError(globalObject));
                    baseObject = scope;
                }
            }

            PutPropertySlot slot(baseObject);
            switch (JSONPPath.last().m_type) {
            case JSONPPathEntryTypeCall: {
                JSValue function = baseObject.get(globalObject, ident);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                auto callData = JSC::getCallData(function);
                if (callData.type == CallData::Type::None)
                    return throwException(globalObject, throwScope, createNotAFunctionError(globalObject, function));
                MarkedArgumentBuffer jsonArg;
                jsonArg.append(JSONPValue);
                ASSERT(!jsonArg.hasOverflowed());
                JSValue thisValue = JSONPPath.size() == 1 ? jsUndefined() : baseObject;
                JSONPValue = JSC::call(globalObject, function, callData, thisValue, jsonArg);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                break;
            }
            case JSONPPathEntryTypeDot: {
                baseObject.put(globalObject, ident, JSONPValue, slot);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                break;
            }
            case JSONPPathEntryTypeLookup: {
                baseObject.putByIndex(globalObject, JSONPPath.last().m_pathIndex, JSONPValue, slot.isStrictMode());
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
                return jsUndefined();
            }
            result = JSONPValue;
        }
        return result;
    }
failedJSONP:
    // If we get here, then we have already proven that the script is not a JSON
    // object.

    // Compile source to bytecode if necessary:
    JSObject* error = program->initializeGlobalProperties(vm, globalObject, scope);
    EXCEPTION_ASSERT(!throwScope.exception() || !error || vm.hasPendingTerminationException());
    RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
    if (error) [[unlikely]]
        return throwException(globalObject, throwScope, error);

    if (vm.traps().needHandling(VMTraps::NonDebuggerAsyncEvents)) [[unlikely]] {
        if (vm.hasExceptionsAfterHandlingTraps())
            return throwScope.exception();
    }

    if (scope->structure()->isUncacheableDictionary())
        scope->flattenDictionaryObject(vm);

    RefPtr<JSC::JITCode> jitCode;
    ProtoCallFrame protoCallFrame;
    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        ProgramCodeBlock* codeBlock;
        {
            CodeBlock* tempCodeBlock;
            program->prepareForExecution<ProgramExecutable>(vm, nullptr, scope, CodeSpecializationKind::CodeForCall, tempCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(throwScope, throwScope.exception());
            codeBlock = jsCast<ProgramCodeBlock*>(tempCodeBlock);
            ASSERT(codeBlock && codeBlock->numParameters() == 1); // 1 parameter for 'this'.
        }

        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            jitCode = program->generatedJITCode();
            protoCallFrame.init(codeBlock, globalObject, globalCallee, thisObj, 1);
        }
    }

    // Execute the code:
    throwScope.release();
    ASSERT(jitCode == program->generatedJITCode().ptr());
    return JSValue::decode(vmEntryToJavaScript(jitCode->addressForCall(), &vm, &protoCallFrame));
}

JSValue Interpreter::executeBoundCall(VM& vm, JSBoundFunction* function, const ArgList& args)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(function->boundArgsLength());

    MarkedArgumentBuffer combinedArgs;
    combinedArgs.ensureCapacity(function->boundArgsLength() + args.size());
    function->forEachBoundArg([&](JSValue argument) -> IterationStatus {
        combinedArgs.append(argument);
        return IterationStatus::Continue;
    });
    for (unsigned i = 0; i < args.size(); ++i)
        combinedArgs.append(args.at(i));

    if (combinedArgs.hasOverflowed()) [[unlikely]]
        return throwStackOverflowError(function->globalObject(), scope);

    JSObject* targetFunction = function->targetFunction();
    JSValue boundThis = function->boundThis();
    auto callData = JSC::getCallData(targetFunction);
    ASSERT(callData.type != CallData::Type::None);

    RELEASE_AND_RETURN(scope, executeCallImpl(vm, targetFunction, callData, boundThis, combinedArgs));
}

ALWAYS_INLINE JSValue Interpreter::executeCallImpl(VM& vm, JSObject* function, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    auto scope = DECLARE_THROW_SCOPE(vm);

    scope.assertNoException();

    ASSERT(!vm.isCollectorBusyOnCurrentThread());

    bool isJSCall = callData.type == CallData::Type::JS;
    JSScope* functionScope = nullptr;
    FunctionExecutable* functionExecutable = nullptr;
    TaggedNativeFunction nativeFunction;
    JSGlobalObject* globalObject = nullptr;

    if (isJSCall) {
        functionScope = callData.js.scope;
        functionExecutable = callData.js.functionExecutable;
        globalObject = functionScope->globalObject();
    } else {
        ASSERT(callData.type == CallData::Type::Native);
        nativeFunction = callData.native.function;
        globalObject = function->globalObject();
    }

    size_t argsCount = 1 + args.size(); // implicit "this" parameter

    VMEntryScope entryScope(vm, globalObject);
    if (!vm.isSafeToRecurseSoft() || args.size() > maxArguments) [[unlikely]]
        return throwStackOverflowError(globalObject, scope);

    if (vm.disallowVMEntryCount) [[unlikely]]
        return checkVMEntryPermission();

    if (vm.traps().needHandling(VMTraps::NonDebuggerAsyncEvents)) [[unlikely]] {
        if (vm.hasExceptionsAfterHandlingTraps())
            return scope.exception();
    }

    RefPtr<JSC::JITCode> jitCode;
    ProtoCallFrame protoCallFrame;
    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        CodeBlock* newCodeBlock = nullptr;
        if (isJSCall) {
            // Compile the callee:
            functionExecutable->prepareForExecution<FunctionExecutable>(vm, jsCast<JSFunction*>(function), functionScope, CodeSpecializationKind::CodeForCall, newCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, scope.exception());
            ASSERT(newCodeBlock);
            newCodeBlock->m_shouldAlwaysBeInlined = false;
        }

        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            if (isJSCall)
                jitCode = functionExecutable->generatedJITCodeForCall();
            protoCallFrame.init(newCodeBlock, globalObject, function, thisValue, argsCount, args.data());
        }
    }

    // Execute the code:
    scope.release();
    if (isJSCall) {
        ASSERT(jitCode == functionExecutable->generatedJITCodeForCall().ptr());
        return JSValue::decode(vmEntryToJavaScript(jitCode->addressForCall(), &vm, &protoCallFrame));
    }

#if ENABLE(WEBASSEMBLY)
    if (callData.native.isWasm)
        return JSValue::decode(vmEntryToWasm(jsCast<WebAssemblyFunction*>(function)->jsEntrypoint(ArityCheckMode::MustCheckArity).taggedPtr(), &vm, &protoCallFrame));
#endif

    return JSValue::decode(vmEntryToNative(nativeFunction.taggedPtr(), &vm, &protoCallFrame));
}

JSValue Interpreter::executeCall(JSObject* function, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = this->vm();
    if (callData.type == CallData::Type::JS || !callData.native.isBoundFunction)
        return executeCallImpl(vm, function, callData, thisValue, args);

    // Only one-level unwrap is enough! We already made JSBoundFunction's nest smaller.
    auto* boundFunction = jsCast<JSBoundFunction*>(function);
    if (boundFunction->m_isTainted)
        vm.setMightBeExecutingTaintedCode();
    if (!boundFunction->boundArgsLength()) {
        // This is the simplest path, just replacing |this|. We do not need to go to executeBoundCall.
        // Let's just replace and get unwrapped functions again.
        JSObject* targetFunction = boundFunction->targetFunction();
        JSValue boundThis = boundFunction->boundThis();
        auto targetFunctionCallData = JSC::getCallData(targetFunction);
        ASSERT(targetFunctionCallData.type != CallData::Type::None);
        return executeCallImpl(vm, targetFunction, targetFunctionCallData, boundThis, args);
    }
    return executeBoundCall(vm, boundFunction, args);
}

JSObject* Interpreter::executeConstruct(JSObject* constructor, const CallData& constructData, const ArgList& args, JSValue newTarget)
{
    VM& vm = this->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());

    bool isJSConstruct = (constructData.type == CallData::Type::JS);
    JSScope* scope = nullptr;
    size_t argsCount = 1 + args.size(); // implicit "this" parameter

    JSGlobalObject* globalObject;

    if (isJSConstruct) {
        scope = constructData.js.scope;
        globalObject = scope->globalObject();
    } else {
        ASSERT(constructData.type == CallData::Type::Native);
        globalObject = constructor->globalObject();
    }

    VMEntryScope entryScope(vm, globalObject);
    if (!vm.isSafeToRecurseSoft() || args.size() > maxArguments) [[unlikely]] {
        throwStackOverflowError(globalObject, throwScope);
        return nullptr;
    }

    if (vm.disallowVMEntryCount) [[unlikely]] {
        checkVMEntryPermission();
        return globalObject->globalThis();
    }

    if (vm.traps().needHandling(VMTraps::NonDebuggerAsyncEvents)) [[unlikely]] {
        if (vm.hasExceptionsAfterHandlingTraps())
            return nullptr;
    }

    RefPtr<JSC::JITCode> jitCode;
    ProtoCallFrame protoCallFrame;
    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        CodeBlock* newCodeBlock = nullptr;
        if (isJSConstruct) {
            // Compile the callee:
            constructData.js.functionExecutable->prepareForExecution<FunctionExecutable>(vm, jsCast<JSFunction*>(constructor), scope, CodeSpecializationKind::CodeForConstruct, newCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(throwScope, nullptr);
            ASSERT(newCodeBlock);
            newCodeBlock->m_shouldAlwaysBeInlined = false;
        }

        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            if (isJSConstruct)
                jitCode = constructData.js.functionExecutable->generatedJITCodeForConstruct();
            protoCallFrame.init(newCodeBlock, globalObject, constructor, newTarget, argsCount, args.data());
        }
    }

    EncodedJSValue result;
    // Execute the code.
    if (isJSConstruct) {
        ASSERT(jitCode == constructData.js.functionExecutable->generatedJITCodeForConstruct().ptr());
        result = vmEntryToJavaScript(jitCode->addressForCall(), &vm, &protoCallFrame);
    } else
        result = vmEntryToNative(constructData.native.function.taggedPtr(), &vm, &protoCallFrame);

    // We need to do an explicit exception check so that we don't return a non-null JSObject*
    // if an exception was thrown.
    RETURN_IF_EXCEPTION(throwScope, nullptr);
    return asObject(JSValue::decode(result));
}

CodeBlock* Interpreter::prepareForCachedCall(CachedCall& cachedCall, JSFunction* function)
{
    VM& vm = this->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    throwScope.assertNoException();

    // Compile the callee:
    CodeBlock* newCodeBlock;
    cachedCall.functionExecutable()->prepareForExecution<FunctionExecutable>(vm, function, cachedCall.scope(), CodeSpecializationKind::CodeForCall, newCodeBlock);
    RETURN_IF_EXCEPTION(throwScope, { });

    ASSERT(newCodeBlock);
    newCodeBlock->m_shouldAlwaysBeInlined = false;

    cachedCall.m_addressForCall = newCodeBlock->jitCode()->addressForCall();
    newCodeBlock->linkIncomingCall(nullptr, &cachedCall);
    return newCodeBlock;
}

JSValue Interpreter::executeEval(EvalExecutable* eval, JSValue thisValue, JSScope* scope)
{
    VM& vm = this->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    ASSERT(&vm == &scope->vm());
    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    ASSERT(vm.currentThreadIsHoldingAPILock());

    JSGlobalObject* globalObject = scope->globalObject();
    if (!vm.isSafeToRecurseSoft()) [[unlikely]]
        return throwStackOverflowError(globalObject, throwScope);

    if (vm.traps().needHandling(VMTraps::NonDebuggerAsyncEvents)) [[unlikely]] {
        if (vm.hasExceptionsAfterHandlingTraps())
            return throwScope.exception();
    }

    auto topLevelFunctionDecls = eval->topLevelFunctionDecls();
    auto variables = eval->variables();
    auto functionHoistingCandidates = eval->functionHoistingCandidates();

    if (!variables.empty() || !topLevelFunctionDecls.empty() || !functionHoistingCandidates.empty()) {
        JSScope* variableObject = nullptr;
        if ((!variables.empty() || !topLevelFunctionDecls.empty()) && eval->isInStrictContext()) {
            scope = StrictEvalActivation::create(vm, globalObject->strictEvalActivationStructure(), scope);
            variableObject = scope;
        } else {
            for (JSScope* node = scope; ; node = node->next()) {
                RELEASE_ASSERT(node);
                if (node->isGlobalObject()) {
                    variableObject = node;
                    break;
                }
                if (node->isJSLexicalEnvironment()) {
                    JSLexicalEnvironment* lexicalEnvironment = jsCast<JSLexicalEnvironment*>(node);
                    if (lexicalEnvironment->symbolTable()->scopeType() == SymbolTable::ScopeType::VarScope) {
                        variableObject = node;
                        break;
                    }
                }
            }
            if (variableObject->structure()->isUncacheableDictionary())
                variableObject->flattenDictionaryObject(vm);
        }

        EvalCodeBlock* codeBlock = nullptr;
        {
            CodeBlock* tempCodeBlock;
            eval->prepareForExecution<EvalExecutable>(vm, nullptr, scope, CodeSpecializationKind::CodeForCall, tempCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(throwScope, throwScope.exception());
            codeBlock = jsCast<EvalCodeBlock*>(tempCodeBlock);
            ASSERT(codeBlock && codeBlock->numParameters() == 1); // 1 parameter for 'this'.
        }

        auto functionDecls = codeBlock->functionDecls();
        BatchedTransitionOptimizer optimizer(vm, variableObject);
        if (variableObject->next() && !eval->isInStrictContext())
            variableObject->globalObject()->varInjectionWatchpointSet().fireAll(vm, "Executed eval, fired VarInjection watchpoint");

        if (!eval->isInStrictContext()) {
            for (auto& ident : variables) {
                JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(globalObject, scope, ident);
                RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
                if (resolvedScope.isUndefined())
                    return throwSyntaxError(globalObject, throwScope, makeString("Can't create duplicate variable in eval: '"_s, StringView(ident.impl()), '\''));
            }

            for (auto& slot : functionDecls) {
                FunctionExecutable* function = slot.get();
                JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(globalObject, scope, function->name());
                RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
                if (resolvedScope.isUndefined())
                    return throwSyntaxError(globalObject, throwScope, makeString("Can't create duplicate variable in eval: '"_s, StringView(function->name().impl()), '\''));
            }
        }

        bool isGlobalVariableEnvironment = variableObject->isGlobalObject();
        if (isGlobalVariableEnvironment) {
            for (auto& slot : functionDecls) {
                FunctionExecutable* function = slot.get();
                bool canDeclare = jsCast<JSGlobalObject*>(variableObject)->canDeclareGlobalFunction(function->name());
                throwScope.assertNoExceptionExceptTermination();
                if (!canDeclare)
                    return throwException(globalObject, throwScope, createErrorForInvalidGlobalFunctionDeclaration(globalObject, function->name()));
            }

            if (!variableObject->isStructureExtensible()) {
                for (auto& ident : variables) {
                    bool canDeclare = jsCast<JSGlobalObject*>(variableObject)->canDeclareGlobalVar(ident);
                    throwScope.assertNoExceptionExceptTermination();
                    if (!canDeclare)
                        return throwException(globalObject, throwScope, createErrorForInvalidGlobalVarDeclaration(globalObject, ident));
                }
            }
        }

        auto ensureBindingExists = [&](const Identifier& ident) {
            bool hasProperty = variableObject->hasOwnProperty(globalObject, ident);
            throwScope.assertNoExceptionExceptTermination();
            if (!hasProperty) {
                bool shouldThrow = true;
                PutPropertySlot slot(variableObject, shouldThrow);
                variableObject->methodTable()->put(variableObject, globalObject, ident, jsUndefined(), slot);
                throwScope.assertNoExceptionExceptTermination();
            }
        };

        if (!eval->isInStrictContext()) {
            for (auto& ident : functionHoistingCandidates) {
                JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(globalObject, scope, ident);
                RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
                if (!resolvedScope.isUndefined()) {
                    if (isGlobalVariableEnvironment) {
                        bool canDeclare = jsCast<JSGlobalObject*>(variableObject)->canDeclareGlobalVar(ident);
                        throwScope.assertNoExceptionExceptTermination();
                        if (canDeclare) {
                            jsCast<JSGlobalObject*>(variableObject)->createGlobalVarBinding<BindingCreationContext::Eval>(ident);
                            throwScope.assertNoExceptionExceptTermination();
                        }
                    } else
                        ensureBindingExists(ident);
                }
            }
        }

        for (auto& slot : functionDecls) {
            FunctionExecutable* function = slot.get();
            if (isGlobalVariableEnvironment) {
                jsCast<JSGlobalObject*>(variableObject)->createGlobalFunctionBinding<BindingCreationContext::Eval>(function->name());
                throwScope.assertNoExceptionExceptTermination();
            } else
                ensureBindingExists(function->name());
        }

        for (auto& ident : variables) {
            if (isGlobalVariableEnvironment) {
                jsCast<JSGlobalObject*>(variableObject)->createGlobalVarBinding<BindingCreationContext::Eval>(ident);
                throwScope.assertNoExceptionExceptTermination();
            } else
                ensureBindingExists(ident);
        }

        ensureStillAliveHere(codeBlock);
    }

    EvalCodeBlock* codeBlock = nullptr;
    JSCallee* callee = globalObject->evalCallee();
#if CPU(ARM64) && CPU(ADDRESS64) && !ENABLE(C_LOOP)
    void* entry = nullptr;
    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        // Reload CodeBlock. It is possible that we replaced CodeBlock while setting up the environment.
        {
            CodeBlock* tempCodeBlock;
            eval->prepareForExecution<EvalExecutable>(vm, nullptr, scope, CodeSpecializationKind::CodeForCall, tempCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(throwScope, throwScope.exception());
            codeBlock = jsCast<EvalCodeBlock*>(tempCodeBlock);
            entry = codeBlock->jitCode()->addressForCall();
            ASSERT(codeBlock && codeBlock->numParameters() == 1); // 1 parameter for 'this'.
        }
    }
    callee->setScope(vm, scope);
    EncodedJSValue result = vmEntryToJavaScriptWith0Arguments(entry, &vm, codeBlock, callee, thisValue);
    callee->setScope(vm, nullptr);
#else
    RefPtr<JSC::JITCode> jitCode;
    ProtoCallFrame protoCallFrame;
    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        // Reload CodeBlock. It is possible that we replaced CodeBlock while setting up the environment.
        {
            CodeBlock* tempCodeBlock;
            eval->prepareForExecution<EvalExecutable>(vm, nullptr, scope, CodeSpecializationKind::CodeForCall, tempCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(throwScope, throwScope.exception());
            codeBlock = jsCast<EvalCodeBlock*>(tempCodeBlock);
            ASSERT(codeBlock && codeBlock->numParameters() == 1); // 1 parameter for 'this'.
        }

        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            jitCode = eval->generatedJITCode();
            protoCallFrame.init(codeBlock, globalObject, callee, thisValue, 1);
        }
    }

    // Execute the code:
    throwScope.release();
    ASSERT(jitCode == eval->generatedJITCode().ptr());
    // eval code only uses scope at the beginning (op_enter).
    // We can replace the current scope for the subsequent run.
    callee->setScope(vm, scope);
    EncodedJSValue result = vmEntryToJavaScript(jitCode->addressForCall(), &vm, &protoCallFrame);
    callee->setScope(vm, nullptr);
#endif
    ensureStillAliveHere(eval);
    ensureStillAliveHere(codeBlock);
    return JSValue::decode(result);
}

JSValue Interpreter::executeModuleProgram(JSModuleRecord* record, ModuleProgramExecutable* executable, JSGlobalObject* lexicalGlobalObject, JSModuleEnvironment* scope, JSValue sentValue, JSValue resumeMode)
{
    VM& vm = this->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto clobberizeValidator = makeScopeExit([&] {
        vm.didEnterVM = true;
    });

    ASSERT_UNUSED(lexicalGlobalObject, &vm == &lexicalGlobalObject->vm());
    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());

    JSGlobalObject* globalObject = scope->globalObject();
    VMEntryScope entryScope(vm, scope->globalObject());
    if (!vm.isSafeToRecurseSoft()) [[unlikely]]
        return throwStackOverflowError(globalObject, throwScope);

    if (vm.disallowVMEntryCount) [[unlikely]]
        return checkVMEntryPermission();

    if (vm.traps().needHandling(VMTraps::NonDebuggerAsyncEvents)) [[unlikely]] {
        if (vm.hasExceptionsAfterHandlingTraps())
            return throwScope.exception();
    }

    if (scope->structure()->isUncacheableDictionary())
        scope->flattenDictionaryObject(vm);

    const unsigned numberOfArguments = static_cast<unsigned>(AbstractModuleRecord::Argument::NumberOfArguments);
    JSCallee* callee = JSCallee::create(vm, globalObject, scope);
    RefPtr<JSC::JITCode> jitCode;

    ProtoCallFrame protoCallFrame;
    EncodedJSValue args[numberOfArguments] = {
        JSValue::encode(record),
        JSValue::encode(record->internalField(JSModuleRecord::Field::State).get()),
        JSValue::encode(sentValue),
        JSValue::encode(resumeMode),
        JSValue::encode(scope),
    };

    {
        DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

        ModuleProgramCodeBlock* codeBlock;
        {
            CodeBlock* tempCodeBlock;
            executable->prepareForExecution<ModuleProgramExecutable>(vm, nullptr, scope, CodeSpecializationKind::CodeForCall, tempCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(throwScope, throwScope.exception());
            codeBlock = jsCast<ModuleProgramCodeBlock*>(tempCodeBlock);
            ASSERT(codeBlock && codeBlock->numParameters() == numberOfArguments + 1);
        }

        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            jitCode = executable->generatedJITCode();

            // The |this| of the module is always `undefined`.
            // http://www.ecma-international.org/ecma-262/6.0/#sec-module-environment-records-hasthisbinding
            // http://www.ecma-international.org/ecma-262/6.0/#sec-module-environment-records-getthisbinding
            protoCallFrame.init(codeBlock, globalObject, callee, jsUndefined(), numberOfArguments + 1, args);
        }

        record->internalField(JSModuleRecord::Field::State).set(vm, record, jsNumber(static_cast<int>(JSModuleRecord::State::Executing)));
    }

    // Execute the code:
    throwScope.release();
    ASSERT(jitCode == executable->generatedJITCode().ptr());
    return JSValue::decode(vmEntryToJavaScript(jitCode->addressForCall(), &vm, &protoCallFrame));
}

NEVER_INLINE void Interpreter::debug(CallFrame* callFrame, DebugHookType debugHookType, JSValue data)
{
    VM& vm = this->vm();
    DeferTermination deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (Options::debuggerTriggersBreakpointException()) [[unlikely]] {
        if (debugHookType == DidReachDebuggerStatement)
            WTFBreakpointTrap();
    }

    Debugger* debugger = callFrame->lexicalGlobalObject(vm)->debugger();
    if (!debugger)
        return;

    ASSERT(callFrame->codeBlock()->hasDebuggerRequests());
    scope.assertNoException();

    switch (debugHookType) {
        case DidEnterCallFrame:
            debugger->callEvent(callFrame);
            break;
        case WillLeaveCallFrame:
            debugger->returnEvent(callFrame);
            break;
        case WillExecuteStatement:
            debugger->atStatement(callFrame);
            break;
        case WillExecuteExpression:
            debugger->atExpression(callFrame);
            break;
        case WillAwait:
            debugger->willAwait(callFrame, data);
            break;
        case DidAwait:
            debugger->didAwait(callFrame, data);
            break;
        case WillExecuteProgram:
            debugger->willExecuteProgram(callFrame);
            break;
        case DidExecuteProgram:
            debugger->didExecuteProgram(callFrame);
            break;
        case DidReachDebuggerStatement:
            debugger->didReachDebuggerStatement(callFrame);
            break;
    }
    scope.assertNoException();
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::DebugHookType type)
{
    switch (type) {
    case JSC::WillExecuteProgram:
        out.print("WillExecuteProgram");
        return;
    case JSC::DidExecuteProgram:
        out.print("DidExecuteProgram");
        return;
    case JSC::DidEnterCallFrame:
        out.print("DidEnterCallFrame");
        return;
    case JSC::DidReachDebuggerStatement:
        out.print("DidReachDebuggerStatement");
        return;
    case JSC::WillLeaveCallFrame:
        out.print("WillLeaveCallFrame");
        return;
    case JSC::WillExecuteStatement:
        out.print("WillExecuteStatement");
        return;
    case JSC::WillExecuteExpression:
        out.print("WillExecuteExpression");
        return;
    case JSC::WillAwait:
        out.print("WillAwait");
        return;
    case JSC::DidAwait:
        out.print("DidAwait");
        return;
    }
}

} // namespace WTF
