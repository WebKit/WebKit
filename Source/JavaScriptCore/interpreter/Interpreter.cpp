/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#include "BatchedTransitionOptimizer.h"
#include "Bytecodes.h"
#include "CallFrameClosure.h"
#include "CatchScope.h"
#include "CodeBlock.h"
#include "DirectArguments.h"
#include "Heap.h"
#include "Debugger.h"
#include "DebuggerCallFrame.h"
#include "DirectEvalCodeCache.h"
#include "ErrorInstance.h"
#include "EvalCodeBlock.h"
#include "Exception.h"
#include "ExceptionHelpers.h"
#include "FrameTracers.h"
#include "FunctionCodeBlock.h"
#include "InterpreterInlines.h"
#include "JITCodeInlines.h"
#include "JSArrayInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSFixedArray.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LLIntCLoop.h"
#include "LLIntThunks.h"
#include "LiteralParser.h"
#include "ModuleProgramCodeBlock.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "ProgramCodeBlock.h"
#include "ProtoCallFrame.h"
#include "RegExpObject.h"
#include "Register.h"
#include "RegisterAtOffsetList.h"
#include "ScopedArguments.h"
#include "StackAlignment.h"
#include "StackFrame.h"
#include "StackVisitor.h"
#include "StrictEvalActivation.h"
#include "StrongInlines.h"
#include "Symbol.h"
#include "VMEntryScope.h"
#include "VMInlines.h"
#include "VMInspector.h"
#include "VirtualRegister.h"

#include <limits.h>
#include <stdio.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StackStats.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/Threading.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(JIT)
#include "JIT.h"
#endif

namespace JSC {

JSValue eval(CallFrame* callFrame)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!callFrame->argumentCount())
        return jsUndefined();

    JSValue program = callFrame->argument(0);
    if (!program.isString())
        return program;

    TopCallFrameSetter topCallFrame(vm, callFrame);
    JSGlobalObject* globalObject = callFrame->lexicalGlobalObject();
    if (!globalObject->evalEnabled()) {
        throwException(callFrame, scope, createEvalError(callFrame, globalObject->evalDisabledErrorMessage()));
        return jsUndefined();
    }
    String programSource = asString(program)->value(callFrame);
    RETURN_IF_EXCEPTION(scope, JSValue());
    
    CallFrame* callerFrame = callFrame->callerFrame();
    CallSiteIndex callerCallSiteIndex = callerFrame->callSiteIndex();
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    JSScope* callerScopeChain = callerFrame->uncheckedR(callerCodeBlock->scopeRegister().offset()).Register::scope();
    UnlinkedCodeBlock* callerUnlinkedCodeBlock = callerCodeBlock->unlinkedCodeBlock();

    bool isArrowFunctionContext = callerUnlinkedCodeBlock->isArrowFunction() || callerUnlinkedCodeBlock->isArrowFunctionContext();

    DerivedContextType derivedContextType = callerUnlinkedCodeBlock->derivedContextType();
    if (!isArrowFunctionContext && callerUnlinkedCodeBlock->isClassContext()) {
        derivedContextType = callerUnlinkedCodeBlock->isConstructor()
            ? DerivedContextType::DerivedConstructorContext
            : DerivedContextType::DerivedMethodContext;
    }

    EvalContextType evalContextType;
    if (isFunctionParseMode(callerUnlinkedCodeBlock->parseMode()))
        evalContextType = EvalContextType::FunctionEvalContext;
    else if (callerUnlinkedCodeBlock->codeType() == EvalCode)
        evalContextType = callerUnlinkedCodeBlock->evalContextType();
    else
        evalContextType = EvalContextType::None;

    DirectEvalExecutable* eval = callerCodeBlock->directEvalCodeCache().tryGet(programSource, callerCallSiteIndex);
    if (!eval) {
        if (!callerCodeBlock->isStrictMode()) {
            if (programSource.is8Bit()) {
                LiteralParser<LChar> preparser(callFrame, programSource.characters8(), programSource.length(), NonStrictJSON);
                if (JSValue parsedObject = preparser.tryLiteralParse())
                    RELEASE_AND_RETURN(scope, parsedObject);

            } else {
                LiteralParser<UChar> preparser(callFrame, programSource.characters16(), programSource.length(), NonStrictJSON);
                if (JSValue parsedObject = preparser.tryLiteralParse())
                    RELEASE_AND_RETURN(scope, parsedObject);

            }
            RETURN_IF_EXCEPTION(scope, JSValue());
        }
        
        VariableEnvironment variablesUnderTDZ;
        JSScope::collectClosureVariablesUnderTDZ(callerScopeChain, variablesUnderTDZ);
        eval = DirectEvalExecutable::create(callFrame, makeSource(programSource, callerCodeBlock->source()->sourceOrigin()), callerCodeBlock->isStrictMode(), derivedContextType, isArrowFunctionContext, evalContextType, &variablesUnderTDZ);
        EXCEPTION_ASSERT(!!scope.exception() == !eval);
        if (!eval)
            return jsUndefined();

        callerCodeBlock->directEvalCodeCache().set(callFrame, callerCodeBlock, programSource, callerCallSiteIndex, eval);
    }

    JSValue thisValue = callerFrame->thisValue();
    Interpreter* interpreter = vm.interpreter;
    RELEASE_AND_RETURN(scope, interpreter->execute(eval, callFrame, thisValue, callerScopeChain));
}

unsigned sizeOfVarargs(CallFrame* callFrame, JSValue arguments, uint32_t firstVarArgOffset)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!arguments.isCell())) {
        if (arguments.isUndefinedOrNull())
            return 0;
        
        throwException(callFrame, scope, createInvalidFunctionApplyParameterError(callFrame, arguments));
        return 0;
    }
    
    JSCell* cell = arguments.asCell();
    unsigned length;
    switch (cell->type()) {
    case DirectArgumentsType:
        length = jsCast<DirectArguments*>(cell)->length(callFrame);
        break;
    case ScopedArgumentsType:
        length = jsCast<ScopedArguments*>(cell)->length(callFrame);
        break;
    case JSFixedArrayType:
        length = jsCast<JSFixedArray*>(cell)->size();
        break;
    case JSImmutableButterflyType:
        length = jsCast<JSImmutableButterfly*>(cell)->length();
        break;
    case StringType:
    case SymbolType:
    case BigIntType:
        throwException(callFrame, scope, createInvalidFunctionApplyParameterError(callFrame,  arguments));
        return 0;
        
    default:
        RELEASE_ASSERT(arguments.isObject());
        length = clampToUnsigned(toLength(callFrame, jsCast<JSObject*>(cell)));
        break;
    }
    RETURN_IF_EXCEPTION(scope, 0);
    
    if (length >= firstVarArgOffset)
        length -= firstVarArgOffset;
    else
        length = 0;
    
    return length;
}

unsigned sizeFrameForForwardArguments(CallFrame* callFrame, VM& vm, unsigned numUsedStackSlots)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = callFrame->argumentCount();
    CallFrame* calleeFrame = calleeFrameForVarargs(callFrame, numUsedStackSlots, length + 1);
    if (UNLIKELY(!vm.ensureStackCapacityFor(calleeFrame->registers())))
        throwStackOverflowError(callFrame, scope);

    return length;
}

unsigned sizeFrameForVarargs(CallFrame* callFrame, VM& vm, JSValue arguments, unsigned numUsedStackSlots, uint32_t firstVarArgOffset)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = sizeOfVarargs(callFrame, arguments, firstVarArgOffset);
    RETURN_IF_EXCEPTION(scope, 0);

    CallFrame* calleeFrame = calleeFrameForVarargs(callFrame, numUsedStackSlots, length + 1);
    if (UNLIKELY(length > maxArguments || !vm.ensureStackCapacityFor(calleeFrame->registers()))) {
        throwStackOverflowError(callFrame, scope);
        return 0;
    }
    
    return length;
}

void loadVarargs(CallFrame* callFrame, VirtualRegister firstElementDest, JSValue arguments, uint32_t offset, uint32_t length)
{
    if (UNLIKELY(!arguments.isCell()) || !length)
        return;

    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSCell* cell = arguments.asCell();

    switch (cell->type()) {
    case DirectArgumentsType:
        scope.release();
        jsCast<DirectArguments*>(cell)->copyToArguments(callFrame, firstElementDest, offset, length);
        return;
    case ScopedArgumentsType:
        scope.release();
        jsCast<ScopedArguments*>(cell)->copyToArguments(callFrame, firstElementDest, offset, length);
        return;
    case JSFixedArrayType:
        scope.release();
        jsCast<JSFixedArray*>(cell)->copyToArguments(callFrame, firstElementDest, offset, length);
        return;
    case JSImmutableButterflyType:
        scope.release();
        jsCast<JSImmutableButterfly*>(cell)->copyToArguments(callFrame, firstElementDest, offset, length);
        return; 
    default: {
        ASSERT(arguments.isObject());
        JSObject* object = jsCast<JSObject*>(cell);
        if (isJSArray(object)) {
            scope.release();
            jsCast<JSArray*>(object)->copyToArguments(callFrame, firstElementDest, offset, length);
            return;
        }
        unsigned i;
        for (i = 0; i < length && object->canGetIndexQuickly(i + offset); ++i)
            callFrame->r(firstElementDest + i) = object->getIndexQuickly(i + offset);
        for (; i < length; ++i) {
            JSValue value = object->get(callFrame, i + offset);
            RETURN_IF_EXCEPTION(scope, void());
            callFrame->r(firstElementDest + i) = value;
        }
        return;
    } }
}

void setupVarargsFrame(CallFrame* callFrame, CallFrame* newCallFrame, JSValue arguments, uint32_t offset, uint32_t length)
{
    VirtualRegister calleeFrameOffset(newCallFrame - callFrame);
    
    loadVarargs(
        callFrame,
        calleeFrameOffset + CallFrame::argumentOffset(0),
        arguments, offset, length);
    
    newCallFrame->setArgumentCountIncludingThis(length + 1);
}

void setupVarargsFrameAndSetThis(CallFrame* callFrame, CallFrame* newCallFrame, JSValue thisValue, JSValue arguments, uint32_t firstVarArgOffset, uint32_t length)
{
    setupVarargsFrame(callFrame, newCallFrame, arguments, firstVarArgOffset, length);
    newCallFrame->setThisValue(thisValue);
}

void setupForwardArgumentsFrame(CallFrame* execCaller, CallFrame* execCallee, uint32_t length)
{
    ASSERT(length == execCaller->argumentCount());
    unsigned offset = execCaller->argumentOffset(0) * sizeof(Register);
    memcpy(reinterpret_cast<char*>(execCallee) + offset, reinterpret_cast<char*>(execCaller) + offset, length * sizeof(Register));
    execCallee->setArgumentCountIncludingThis(length + 1);
}

void setupForwardArgumentsFrameAndSetThis(CallFrame* execCaller, CallFrame* execCallee, JSValue thisValue, uint32_t length)
{
    setupForwardArgumentsFrame(execCaller, execCallee, length);
    execCallee->setThisValue(thisValue);
}

    

Interpreter::Interpreter(VM& vm)
    : m_vm(vm)
#if ENABLE(C_LOOP)
    , m_cloopStack(vm)
#endif
{
#if !ASSERT_DISABLED
    static std::once_flag assertOnceKey;
    std::call_once(assertOnceKey, [] {
        for (unsigned i = 0; i < NUMBER_OF_BYTECODE_IDS; ++i) {
            OpcodeID opcodeID = static_cast<OpcodeID>(i);
            RELEASE_ASSERT(getOpcodeID(getOpcode(opcodeID)) == opcodeID);
        }
    });
#endif // USE(LLINT_EMBEDDED_OPCODE_ID)
}

Interpreter::~Interpreter()
{
}

#if ENABLE(COMPUTED_GOTO_OPCODES)
#if !USE(LLINT_EMBEDDED_OPCODE_ID) || !ASSERT_DISABLED
HashMap<Opcode, OpcodeID>& Interpreter::opcodeIDTable()
{
    static NeverDestroyed<HashMap<Opcode, OpcodeID>> opcodeIDTable;

    static std::once_flag initializeKey;
    std::call_once(initializeKey, [&] {
        const Opcode* opcodeTable = LLInt::opcodeMap();
        for (unsigned i = 0; i < NUMBER_OF_BYTECODE_IDS; ++i)
            opcodeIDTable.get().add(opcodeTable[i], static_cast<OpcodeID>(i));
    });

    return opcodeIDTable;
}
#endif // !USE(LLINT_EMBEDDED_OPCODE_ID) || !ASSERT_DISABLED
#endif // ENABLE(COMPUTED_GOTO_OPCODES)

#if !ASSERT_DISABLED
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
#endif // !ASSERT_DISABLED

class GetStackTraceFunctor {
public:
    GetStackTraceFunctor(VM& vm, JSCell* owner, Vector<StackFrame>& results, size_t framesToSkip, size_t capacity)
        : m_vm(vm)
        , m_owner(owner)
        , m_results(results)
        , m_framesToSkip(framesToSkip)
        , m_remainingCapacityForFrameCapture(capacity)
    {
        m_results.reserveInitialCapacity(capacity);
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (m_framesToSkip > 0) {
            m_framesToSkip--;
            return StackVisitor::Continue;
        }

        if (m_remainingCapacityForFrameCapture) {
            if (visitor->isWasmFrame()) {
                m_results.append(StackFrame(visitor->wasmFunctionIndexOrName()));
            } else if (!!visitor->codeBlock() && !visitor->codeBlock()->unlinkedCodeBlock()->isBuiltinFunction()) {
                m_results.append(
                    StackFrame(m_vm, m_owner, visitor->callee().asCell(), visitor->codeBlock(), visitor->bytecodeOffset()));
            } else {
                m_results.append(
                    StackFrame(m_vm, m_owner, visitor->callee().asCell()));
            }
    
            m_remainingCapacityForFrameCapture--;
            return StackVisitor::Continue;
        }
        return StackVisitor::Done;
    }

private:
    VM& m_vm;
    JSCell* m_owner;
    Vector<StackFrame>& m_results;
    mutable size_t m_framesToSkip;
    mutable size_t m_remainingCapacityForFrameCapture;
};

void Interpreter::getStackTrace(JSCell* owner, Vector<StackFrame>& results, size_t framesToSkip, size_t maxStackSize)
{
    DisallowGC disallowGC;
    VM& vm = m_vm;
    CallFrame* callFrame = vm.topCallFrame;
    if (!callFrame || !maxStackSize)
        return;

    size_t framesCount = 0;
    size_t maxFramesCountNeeded = maxStackSize + framesToSkip;
    StackVisitor::visit(callFrame, &vm, [&] (StackVisitor&) -> StackVisitor::Status {
        if (++framesCount < maxFramesCountNeeded)
            return StackVisitor::Continue;
        return StackVisitor::Done;
    });
    if (framesCount <= framesToSkip)
        return;

    framesCount -= framesToSkip;
    framesCount = std::min(maxStackSize, framesCount);

    GetStackTraceFunctor functor(vm, owner, results, framesToSkip, framesCount);
    StackVisitor::visit(callFrame, &vm, functor);
    ASSERT(results.size() == results.capacity());
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
    ASSERT(!visitor->isInlinedFrame());
#endif

    CallFrame* callFrame = visitor->callFrame();
    unsigned exceptionHandlerIndex;
    if (JITCode::isOptimizingJIT(codeBlock->jitType()))
        exceptionHandlerIndex = callFrame->callSiteIndex().bits();
    else
        exceptionHandlerIndex = callFrame->bytecodeOffset();

    return codeBlock->handlerForIndex(exceptionHandlerIndex, requiredHandler);
}

class GetCatchHandlerFunctor {
public:
    GetCatchHandlerFunctor()
        : m_handler(0)
    {
    }

    HandlerInfo* handler() { return m_handler; }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        visitor.unwindToMachineCodeBlockFrame();

        CodeBlock* codeBlock = visitor->codeBlock();
        if (!codeBlock)
            return StackVisitor::Continue;

        m_handler = findExceptionHandler(visitor, codeBlock, RequiredHandler::CatchHandler);
        if (m_handler)
            return StackVisitor::Done;

        return StackVisitor::Continue;
    }

private:
    mutable HandlerInfo* m_handler;
};

ALWAYS_INLINE static void notifyDebuggerOfUnwinding(VM& vm, CallFrame* callFrame)
{
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    if (Debugger* debugger = vm.vmEntryGlobalObject(callFrame)->debugger()) {
        SuspendExceptionScope scope(&vm);
        if (callFrame->isAnyWasmCallee()
            || (callFrame->callee().isCell() && callFrame->callee().asCell()->inherits<JSFunction>(vm)))
            debugger->unwindEvent(callFrame);
        else
            debugger->didExecuteProgram(callFrame);
        catchScope.assertNoException();
    }
}

class UnwindFunctor {
public:
    UnwindFunctor(VM& vm, CallFrame*& callFrame, bool isTermination, CodeBlock*& codeBlock, HandlerInfo*& handler)
        : m_vm(vm)
        , m_callFrame(callFrame)
        , m_isTermination(isTermination)
        , m_codeBlock(codeBlock)
        , m_handler(handler)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        visitor.unwindToMachineCodeBlockFrame();
        m_callFrame = visitor->callFrame();
        m_codeBlock = visitor->codeBlock();

        m_handler = nullptr;
        if (!m_isTermination) {
            if (m_codeBlock) {
                m_handler = findExceptionHandler(visitor, m_codeBlock, RequiredHandler::AnyHandler);
                if (m_handler)
                    return StackVisitor::Done;
            }
        }

        notifyDebuggerOfUnwinding(m_vm, m_callFrame);

        copyCalleeSavesToEntryFrameCalleeSavesBuffer(visitor);

        bool shouldStopUnwinding = visitor->callerIsEntryFrame();
        if (shouldStopUnwinding)
            return StackVisitor::Done;

        return StackVisitor::Continue;
    }

private:
    void copyCalleeSavesToEntryFrameCalleeSavesBuffer(StackVisitor& visitor) const
    {
#if !ENABLE(C_LOOP) && NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        RegisterAtOffsetList* currentCalleeSaves = visitor->calleeSaveRegisters();

        if (!currentCalleeSaves)
            return;

        RegisterAtOffsetList* allCalleeSaves = RegisterSet::vmCalleeSaveRegisterOffsets();
        RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
        intptr_t* frame = reinterpret_cast<intptr_t*>(m_callFrame->registers());

        unsigned registerCount = currentCalleeSaves->size();
        VMEntryRecord* record = vmEntryRecord(m_vm.topEntryFrame);
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset currentEntry = currentCalleeSaves->at(i);
            if (dontCopyRegisters.get(currentEntry.reg()))
                continue;
            RegisterAtOffset* calleeSavesEntry = allCalleeSaves->find(currentEntry.reg());
            
            record->calleeSaveRegistersBuffer[calleeSavesEntry->offsetAsIndex()] = *(frame + currentEntry.offsetAsIndex());
        }
#else
        UNUSED_PARAM(visitor);
#endif
    }

    VM& m_vm;
    CallFrame*& m_callFrame;
    bool m_isTermination;
    CodeBlock*& m_codeBlock;
    HandlerInfo*& m_handler;
};

NEVER_INLINE HandlerInfo* Interpreter::unwind(VM& vm, CallFrame*& callFrame, Exception* exception)
{
    auto scope = DECLARE_CATCH_SCOPE(vm);

    ASSERT(reinterpret_cast<void*>(callFrame) != vm.topEntryFrame);
    CodeBlock* codeBlock = callFrame->codeBlock();

    JSValue exceptionValue = exception->value();
    ASSERT(!exceptionValue.isEmpty());
    ASSERT(!exceptionValue.isCell() || exceptionValue.asCell());
    // This shouldn't be possible (hence the assertions), but we're already in the slowest of
    // slow cases, so let's harden against it anyway to be safe.
    if (exceptionValue.isEmpty() || (exceptionValue.isCell() && !exceptionValue.asCell()))
        exceptionValue = jsNull();

    EXCEPTION_ASSERT_UNUSED(scope, scope.exception());

    // Calculate an exception handler vPC, unwinding call frames as necessary.
    HandlerInfo* handler = nullptr;
    UnwindFunctor functor(vm, callFrame, isTerminatedExecutionException(vm, exception), codeBlock, handler);
    StackVisitor::visit<StackVisitor::TerminateIfTopEntryFrameIsEmpty>(callFrame, &vm, functor);
    if (!handler)
        return nullptr;

    return handler;
}

void Interpreter::notifyDebuggerOfExceptionToBeThrown(VM& vm, CallFrame* callFrame, Exception* exception)
{
    Debugger* debugger = vm.vmEntryGlobalObject(callFrame)->debugger();
    if (debugger && debugger->needsExceptionCallbacks() && !exception->didNotifyInspectorOfThrow()) {
        // This code assumes that if the debugger is enabled then there is no inlining.
        // If that assumption turns out to be false then we'll ignore the inlined call
        // frames.
        // https://bugs.webkit.org/show_bug.cgi?id=121754

        bool hasCatchHandler;
        bool isTermination = isTerminatedExecutionException(vm, exception);
        if (isTermination)
            hasCatchHandler = false;
        else {
            GetCatchHandlerFunctor functor;
            StackVisitor::visit(callFrame, &vm, functor);
            HandlerInfo* handler = functor.handler();
            ASSERT(!handler || handler->isCatchHandler());
            hasCatchHandler = !!handler;
        }

        debugger->exception(callFrame, exception->value(), hasCatchHandler);
    }
    exception->setDidNotifyInspectorOfThrow();
}

JSValue Interpreter::executeProgram(const SourceCode& source, CallFrame* callFrame, JSObject* thisObj)
{
    JSScope* scope = thisObj->globalObject()->globalScope();
    VM& vm = *scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ProgramExecutable* program = ProgramExecutable::create(callFrame, source);
    EXCEPTION_ASSERT(throwScope.exception() || program);
    RETURN_IF_EXCEPTION(throwScope, { });

    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    if (vm.isCollectorBusyOnCurrentThread())
        return jsNull();

    if (UNLIKELY(!vm.isSafeToRecurseSoft()))
        return checkedReturn(throwStackOverflowError(callFrame, throwScope));

    // First check if the "program" is actually just a JSON object. If so,
    // we'll handle the JSON object here. Else, we'll handle real JS code
    // below at failedJSONP.

    JSGlobalObject* globalObject = scope->globalObject(vm);
    Vector<JSONPData> JSONPData;
    bool parseResult;
    StringView programSource = program->source().view();
    if (programSource.isNull())
        return jsUndefined();
    if (programSource.is8Bit()) {
        LiteralParser<LChar> literalParser(callFrame, programSource.characters8(), programSource.length(), JSONP);
        parseResult = literalParser.tryJSONPParse(JSONPData, globalObject->globalObjectMethodTable()->supportsRichSourceInfo(globalObject));
    } else {
        LiteralParser<UChar> literalParser(callFrame, programSource.characters16(), programSource.length(), JSONP);
        parseResult = literalParser.tryJSONPParse(JSONPData, globalObject->globalObjectMethodTable()->supportsRichSourceInfo(globalObject));
    }

    RETURN_IF_EXCEPTION(throwScope, { });
    if (parseResult) {
        JSValue result;
        for (unsigned entry = 0; entry < JSONPData.size(); entry++) {
            Vector<JSONPPathEntry> JSONPPath;
            JSONPPath.swap(JSONPData[entry].m_path);
            JSValue JSONPValue = JSONPData[entry].m_value.get();
            if (JSONPPath.size() == 1 && JSONPPath[0].m_type == JSONPPathEntryTypeDeclareVar) {
                globalObject->addVar(callFrame, JSONPPath[0].m_pathEntryName);
                RETURN_IF_EXCEPTION(throwScope, { });
                PutPropertySlot slot(globalObject);
                globalObject->methodTable(vm)->put(globalObject, callFrame, JSONPPath[0].m_pathEntryName, JSONPValue, slot);
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
                            if (scope->getPropertySlot(callFrame, JSONPPath[i].m_pathEntryName, slot))
                                return slot.getValue(callFrame, JSONPPath[i].m_pathEntryName);
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
                            return throwException(callFrame, throwScope, createUndefinedVariableError(callFrame, JSONPPath[i].m_pathEntryName));
                        goto failedJSONP;
                    }

                    baseObject = baseObject.get(callFrame, JSONPPath[i].m_pathEntryName);
                    RETURN_IF_EXCEPTION(throwScope, JSValue());
                    continue;
                }
                case JSONPPathEntryTypeLookup: {
                    baseObject = baseObject.get(callFrame, static_cast<unsigned>(JSONPPath[i].m_pathIndex));
                    RETURN_IF_EXCEPTION(throwScope, JSValue());
                    continue;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    return jsUndefined();
                }
            }

            if (JSONPPath.size() == 1 && JSONPPath.last().m_type != JSONPPathEntryTypeLookup) {
                RELEASE_ASSERT(baseObject == globalObject);
                JSGlobalLexicalEnvironment* scope = globalObject->globalLexicalEnvironment();
                if (scope->hasProperty(callFrame, JSONPPath.last().m_pathEntryName))
                    baseObject = scope;
                RETURN_IF_EXCEPTION(throwScope, JSValue());
            }

            PutPropertySlot slot(baseObject);
            switch (JSONPPath.last().m_type) {
            case JSONPPathEntryTypeCall: {
                JSValue function = baseObject.get(callFrame, JSONPPath.last().m_pathEntryName);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                CallData callData;
                CallType callType = getCallData(vm, function, callData);
                if (callType == CallType::None)
                    return throwException(callFrame, throwScope, createNotAFunctionError(callFrame, function));
                MarkedArgumentBuffer jsonArg;
                jsonArg.append(JSONPValue);
                ASSERT(!jsonArg.hasOverflowed());
                JSValue thisValue = JSONPPath.size() == 1 ? jsUndefined() : baseObject;
                JSONPValue = JSC::call(callFrame, function, callType, callData, thisValue, jsonArg);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                break;
            }
            case JSONPPathEntryTypeDot: {
                baseObject.put(callFrame, JSONPPath.last().m_pathEntryName, JSONPValue, slot);
                RETURN_IF_EXCEPTION(throwScope, JSValue());
                break;
            }
            case JSONPPathEntryTypeLookup: {
                baseObject.putByIndex(callFrame, JSONPPath.last().m_pathIndex, JSONPValue, slot.isStrictMode());
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

    VMEntryScope entryScope(vm, globalObject);

    // Compile source to bytecode if necessary:
    JSObject* error = program->initializeGlobalProperties(vm, callFrame, scope);
    EXCEPTION_ASSERT(!throwScope.exception() || !error);
    if (UNLIKELY(error))
        return checkedReturn(throwException(callFrame, throwScope, error));

    ProgramCodeBlock* codeBlock;
    {
        CodeBlock* tempCodeBlock;
        std::optional<Exception*> error = program->prepareForExecution<ProgramExecutable>(vm, nullptr, scope, CodeForCall, tempCodeBlock);
        EXCEPTION_ASSERT(throwScope.exception() == error.value_or(nullptr));

        if (UNLIKELY(error))
            return checkedReturn(*error);
        codeBlock = jsCast<ProgramCodeBlock*>(tempCodeBlock);
    }

    VMTraps::Mask mask(VMTraps::NeedTermination, VMTraps::NeedWatchdogCheck);
    if (UNLIKELY(vm.needTrapHandling(mask))) {
        vm.handleTraps(callFrame, mask);
        RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
    }

    if (scope->structure(vm)->isUncacheableDictionary())
        scope->flattenDictionaryObject(vm);

    ASSERT(codeBlock->numParameters() == 1); // 1 parameter for 'this'.

    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(codeBlock, JSCallee::create(vm, globalObject, scope), thisObj, 1);

    // Execute the code:
    throwScope.release();
    JSValue result = program->generatedJITCode()->execute(&vm, &protoCallFrame);
    return checkedReturn(result);
}

JSValue Interpreter::executeCall(CallFrame* callFrame, JSObject* function, CallType callType, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = callFrame->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    if (vm.isCollectorBusyOnCurrentThread())
        return jsNull();

    bool isJSCall = (callType == CallType::JS);
    JSScope* scope = nullptr;
    CodeBlock* newCodeBlock;
    size_t argsCount = 1 + args.size(); // implicit "this" parameter

    JSGlobalObject* globalObject;

    if (isJSCall) {
        scope = callData.js.scope;
        globalObject = scope->globalObject(vm);
    } else {
        ASSERT(callType == CallType::Host);
        globalObject = function->globalObject(vm);
    }

    VMEntryScope entryScope(vm, globalObject);
    if (UNLIKELY(!vm.isSafeToRecurseSoft()))
        return checkedReturn(throwStackOverflowError(callFrame, throwScope));

    if (isJSCall) {
        // Compile the callee:
        std::optional<Exception*> compileError = callData.js.functionExecutable->prepareForExecution<FunctionExecutable>(vm, jsCast<JSFunction*>(function), scope, CodeForCall, newCodeBlock);
        EXCEPTION_ASSERT(throwScope.exception() == compileError.value_or(nullptr));
        if (UNLIKELY(!!compileError))
            return checkedReturn(*compileError);

        ASSERT(!!newCodeBlock);
        newCodeBlock->m_shouldAlwaysBeInlined = false;
    } else
        newCodeBlock = 0;

    VMTraps::Mask mask(VMTraps::NeedTermination, VMTraps::NeedWatchdogCheck);
    if (UNLIKELY(vm.needTrapHandling(mask))) {
        vm.handleTraps(callFrame, mask);
        RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
    }

    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(newCodeBlock, function, thisValue, argsCount, args.data());

    JSValue result;
    {
        // Execute the code:
        if (isJSCall) {
            throwScope.release();
            result = callData.js.functionExecutable->generatedJITCodeForCall()->execute(&vm, &protoCallFrame);
        } else {
            result = JSValue::decode(vmEntryToNative(callData.native.function.rawPointer(), &vm, &protoCallFrame));
            RETURN_IF_EXCEPTION(throwScope, JSValue());
        }
    }

    return checkedReturn(result);
}

JSObject* Interpreter::executeConstruct(CallFrame* callFrame, JSObject* constructor, ConstructType constructType, const ConstructData& constructData, const ArgList& args, JSValue newTarget)
{
    VM& vm = callFrame->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    // We throw in this case because we have to return something "valid" but we're
    // already in an invalid state.
    if (vm.isCollectorBusyOnCurrentThread())
        return checkedReturn(throwStackOverflowError(callFrame, throwScope));

    bool isJSConstruct = (constructType == ConstructType::JS);
    JSScope* scope = nullptr;
    CodeBlock* newCodeBlock;
    size_t argsCount = 1 + args.size(); // implicit "this" parameter

    JSGlobalObject* globalObject;

    if (isJSConstruct) {
        scope = constructData.js.scope;
        globalObject = scope->globalObject(vm);
    } else {
        ASSERT(constructType == ConstructType::Host);
        globalObject = constructor->globalObject(vm);
    }

    VMEntryScope entryScope(vm, globalObject);
    if (UNLIKELY(!vm.isSafeToRecurseSoft()))
        return checkedReturn(throwStackOverflowError(callFrame, throwScope));

    if (isJSConstruct) {
        // Compile the callee:
        std::optional<Exception*> compileError = constructData.js.functionExecutable->prepareForExecution<FunctionExecutable>(vm, jsCast<JSFunction*>(constructor), scope, CodeForConstruct, newCodeBlock);
        EXCEPTION_ASSERT(throwScope.exception() == compileError.value_or(nullptr));
        if (UNLIKELY(!!compileError))
            return checkedReturn(*compileError);

        ASSERT(!!newCodeBlock);
        newCodeBlock->m_shouldAlwaysBeInlined = false;
    } else
        newCodeBlock = 0;

    VMTraps::Mask mask(VMTraps::NeedTermination, VMTraps::NeedWatchdogCheck);
    if (UNLIKELY(vm.needTrapHandling(mask))) {
        vm.handleTraps(callFrame, mask);
        RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
    }

    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(newCodeBlock, constructor, newTarget, argsCount, args.data());

    JSValue result;
    {
        // Execute the code.
        if (isJSConstruct)
            result = constructData.js.functionExecutable->generatedJITCodeForConstruct()->execute(&vm, &protoCallFrame);
        else {
            result = JSValue::decode(vmEntryToNative(constructData.native.function.rawPointer(), &vm, &protoCallFrame));

            if (LIKELY(!throwScope.exception()))
                RELEASE_ASSERT(result.isObject());
        }
    }

    RETURN_IF_EXCEPTION(throwScope, 0);
    ASSERT(result.isObject());
    return checkedReturn(asObject(result));
}

CallFrameClosure Interpreter::prepareForRepeatCall(FunctionExecutable* functionExecutable, CallFrame* callFrame, ProtoCallFrame* protoCallFrame, JSFunction* function, int argumentCountIncludingThis, JSScope* scope, const ArgList& args)
{
    VM& vm = *scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    throwScope.assertNoException();
    
    if (vm.isCollectorBusyOnCurrentThread())
        return CallFrameClosure();

    // Compile the callee:
    CodeBlock* newCodeBlock;
    std::optional<Exception*> error = functionExecutable->prepareForExecution<FunctionExecutable>(vm, function, scope, CodeForCall, newCodeBlock);
    EXCEPTION_ASSERT(throwScope.exception() == error.value_or(nullptr));
    if (UNLIKELY(error))
        return CallFrameClosure();
    newCodeBlock->m_shouldAlwaysBeInlined = false;

    size_t argsCount = argumentCountIncludingThis;

    protoCallFrame->init(newCodeBlock, function, jsUndefined(), argsCount, args.data());
    // Return the successful closure:
    CallFrameClosure result = { callFrame, protoCallFrame, function, functionExecutable, &vm, scope, newCodeBlock->numParameters(), argumentCountIncludingThis };
    return result;
}

JSValue Interpreter::execute(EvalExecutable* eval, CallFrame* callFrame, JSValue thisValue, JSScope* scope)
{
    VM& vm = *scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ASSERT(&vm == &callFrame->vm());
    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    if (vm.isCollectorBusyOnCurrentThread())
        return jsNull();

    VMEntryScope entryScope(vm, scope->globalObject(vm));
    if (UNLIKELY(!vm.isSafeToRecurseSoft()))
        return checkedReturn(throwStackOverflowError(callFrame, throwScope));

    unsigned numVariables = eval->numVariables();
    unsigned numTopLevelFunctionDecls = eval->numTopLevelFunctionDecls();
    unsigned numFunctionHoistingCandidates = eval->numFunctionHoistingCandidates();

    JSScope* variableObject;
    if ((numVariables || numTopLevelFunctionDecls) && eval->isStrictMode()) {
        scope = StrictEvalActivation::create(callFrame, scope);
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
    }

    EvalCodeBlock* codeBlock;
    {
        CodeBlock* tempCodeBlock;
        std::optional<Exception*> compileError = eval->prepareForExecution<EvalExecutable>(vm, nullptr, scope, CodeForCall, tempCodeBlock);
        EXCEPTION_ASSERT(throwScope.exception() == compileError.value_or(nullptr));
        if (UNLIKELY(!!compileError))
            return checkedReturn(*compileError);
        codeBlock = jsCast<EvalCodeBlock*>(tempCodeBlock);
    }

    // We can't declare a "var"/"function" that overwrites a global "let"/"const"/"class" in a sloppy-mode eval.
    if (variableObject->isGlobalObject() && !eval->isStrictMode() && (numVariables || numTopLevelFunctionDecls)) {
        JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalObject*>(variableObject)->globalLexicalEnvironment();
        for (unsigned i = 0; i < numVariables; ++i) {
            const Identifier& ident = codeBlock->variable(i);
            PropertySlot slot(globalLexicalEnvironment, PropertySlot::InternalMethodType::VMInquiry);
            if (JSGlobalLexicalEnvironment::getOwnPropertySlot(globalLexicalEnvironment, callFrame, ident, slot)) {
                return checkedReturn(throwTypeError(callFrame, throwScope, makeString("Can't create duplicate global variable in eval: '", String(ident.impl()), "'")));
            }
        }

        for (unsigned i = 0; i < numTopLevelFunctionDecls; ++i) {
            FunctionExecutable* function = codeBlock->functionDecl(i);
            PropertySlot slot(globalLexicalEnvironment, PropertySlot::InternalMethodType::VMInquiry);
            if (JSGlobalLexicalEnvironment::getOwnPropertySlot(globalLexicalEnvironment, callFrame, function->name(), slot)) {
                return checkedReturn(throwTypeError(callFrame, throwScope, makeString("Can't create duplicate global variable in eval: '", String(function->name().impl()), "'")));
            }
        }
    }

    if (variableObject->structure(vm)->isUncacheableDictionary())
        variableObject->flattenDictionaryObject(vm);

    if (numVariables || numTopLevelFunctionDecls || numFunctionHoistingCandidates) {
        BatchedTransitionOptimizer optimizer(vm, variableObject);
        if (variableObject->next() && !eval->isStrictMode())
            variableObject->globalObject(vm)->varInjectionWatchpoint()->fireAll(vm, "Executed eval, fired VarInjection watchpoint");

        for (unsigned i = 0; i < numVariables; ++i) {
            const Identifier& ident = codeBlock->variable(i);
            bool hasProperty = variableObject->hasProperty(callFrame, ident);
            RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
            if (!hasProperty) {
                PutPropertySlot slot(variableObject);
                if (!variableObject->isExtensible(callFrame))
                    return checkedReturn(throwTypeError(callFrame, throwScope, NonExtensibleObjectPropertyDefineError));
                variableObject->methodTable(vm)->put(variableObject, callFrame, ident, jsUndefined(), slot);
                RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
            }
        }
        
        if (eval->isStrictMode()) {
            for (unsigned i = 0; i < numTopLevelFunctionDecls; ++i) {
                FunctionExecutable* function = codeBlock->functionDecl(i);
                PutPropertySlot slot(variableObject);
                // We need create this variables because it will be used to emits code by bytecode generator
                variableObject->methodTable(vm)->put(variableObject, callFrame, function->name(), jsUndefined(), slot);
                RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
            }
        } else {
            for (unsigned i = 0; i < numTopLevelFunctionDecls; ++i) {
                FunctionExecutable* function = codeBlock->functionDecl(i);
                JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(callFrame, scope, function->name());
                RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
                if (resolvedScope.isUndefined())
                    return checkedReturn(throwSyntaxError(callFrame, throwScope, makeString("Can't create duplicate variable in eval: '", String(function->name().impl()), "'")));
                PutPropertySlot slot(variableObject);
                // We need create this variables because it will be used to emits code by bytecode generator
                variableObject->methodTable(vm)->put(variableObject, callFrame, function->name(), jsUndefined(), slot);
                RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
            }

            for (unsigned i = 0; i < numFunctionHoistingCandidates; ++i) {
                const Identifier& ident = codeBlock->functionHoistingCandidate(i);
                JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(callFrame, scope, ident);
                RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
                if (!resolvedScope.isUndefined()) {
                    bool hasProperty = variableObject->hasProperty(callFrame, ident);
                    RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
                    if (!hasProperty) {
                        PutPropertySlot slot(variableObject);
                        variableObject->methodTable(vm)->put(variableObject, callFrame, ident, jsUndefined(), slot);
                        RETURN_IF_EXCEPTION(throwScope, checkedReturn(throwScope.exception()));
                    }
                }
            }
        }
    }

    VMTraps::Mask mask(VMTraps::NeedTermination, VMTraps::NeedWatchdogCheck);
    if (UNLIKELY(vm.needTrapHandling(mask))) {
        vm.handleTraps(callFrame, mask);
        RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
    }

    ASSERT(codeBlock->numParameters() == 1); // 1 parameter for 'this'.

    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(codeBlock, JSCallee::create(vm, scope->globalObject(vm), scope), thisValue, 1);

    // Execute the code:
    throwScope.release();
    JSValue result = eval->generatedJITCode()->execute(&vm, &protoCallFrame);

    return checkedReturn(result);
}

JSValue Interpreter::executeModuleProgram(ModuleProgramExecutable* executable, CallFrame* callFrame, JSModuleEnvironment* scope)
{
    VM& vm = *scope->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    ASSERT(&vm == &callFrame->vm());
    throwScope.assertNoException();
    ASSERT(!vm.isCollectorBusyOnCurrentThread());
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    if (vm.isCollectorBusyOnCurrentThread())
        return jsNull();

    VMEntryScope entryScope(vm, scope->globalObject(vm));
    if (UNLIKELY(!vm.isSafeToRecurseSoft()))
        return checkedReturn(throwStackOverflowError(callFrame, throwScope));

    ModuleProgramCodeBlock* codeBlock;
    {
        CodeBlock* tempCodeBlock;
        std::optional<Exception*> compileError = executable->prepareForExecution<ModuleProgramExecutable>(vm, nullptr, scope, CodeForCall, tempCodeBlock);
        EXCEPTION_ASSERT(throwScope.exception() == compileError.value_or(nullptr));
        if (UNLIKELY(!!compileError))
            return checkedReturn(*compileError);
        codeBlock = jsCast<ModuleProgramCodeBlock*>(tempCodeBlock);
    }

    VMTraps::Mask mask(VMTraps::NeedTermination, VMTraps::NeedWatchdogCheck);
    if (UNLIKELY(vm.needTrapHandling(mask))) {
        vm.handleTraps(callFrame, mask);
        RETURN_IF_EXCEPTION(throwScope, throwScope.exception());
    }

    if (scope->structure(vm)->isUncacheableDictionary())
        scope->flattenDictionaryObject(vm);

    ASSERT(codeBlock->numParameters() == 1); // 1 parameter for 'this'.

    // The |this| of the module is always `undefined`.
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-environment-records-hasthisbinding
    // http://www.ecma-international.org/ecma-262/6.0/#sec-module-environment-records-getthisbinding
    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(codeBlock, JSCallee::create(vm, scope->globalObject(vm), scope), jsUndefined(), 1);

    // Execute the code:
    throwScope.release();
    JSValue result = executable->generatedJITCode()->execute(&vm, &protoCallFrame);

    return checkedReturn(result);
}

NEVER_INLINE void Interpreter::debug(CallFrame* callFrame, DebugHookType debugHookType)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    Debugger* debugger = vm.vmEntryGlobalObject(callFrame)->debugger();
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
        case WillExecuteProgram:
            debugger->willExecuteProgram(callFrame);
            break;
        case DidExecuteProgram:
            debugger->didExecuteProgram(callFrame);
            break;
        case DidReachBreakpoint:
            debugger->didReachBreakpoint(callFrame);
            break;
    }
    scope.assertNoException();
}

} // namespace JSC
