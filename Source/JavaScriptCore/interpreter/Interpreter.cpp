/*
 * Copyright (C) 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "Arguments.h"
#include "BatchedTransitionOptimizer.h"
#include "CallFrame.h"
#include "CallFrameClosure.h"
#include "CodeBlock.h"
#include "Heap.h"
#include "Debugger.h"
#include "DebuggerCallFrame.h"
#include "ErrorInstance.h"
#include "EvalCodeCache.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSBoundFunction.h"
#include "JSNameScope.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LLIntCLoop.h"
#include "LiteralParser.h"
#include "NameInstance.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include "Parser.h"
#include "Profiler.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "SamplingTool.h"
#include "StrictEvalActivation.h"
#include "StrongInlines.h"
#include <limits.h>
#include <stdio.h>
#include <wtf/StackStats.h>
#include <wtf/Threading.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(JIT)
#include "JIT.h"
#endif

#define WTF_USE_GCC_COMPUTED_GOTO_WORKAROUND (ENABLE(LLINT) && !defined(__llvm__))

using namespace std;

namespace JSC {

static CallFrame* getCallerInfo(JSGlobalData*, CallFrame*, int& lineNumber, unsigned& bytecodeOffset);

// Returns the depth of the scope chain within a given call frame.
static int depth(CodeBlock* codeBlock, JSScope* sc)
{
    if (!codeBlock->needsFullScopeChain())
        return 0;
    return sc->localDepth();
}

ALWAYS_INLINE CallFrame* Interpreter::slideRegisterWindowForCall(CodeBlock* newCodeBlock, JSStack* stack, CallFrame* callFrame, size_t registerOffset, int argumentCountIncludingThis)
{
    // This ensures enough space for the worst case scenario of zero arguments passed by the caller.
    if (!stack->grow(callFrame->registers() + registerOffset + newCodeBlock->numParameters() + newCodeBlock->m_numCalleeRegisters))
        return 0;

    if (argumentCountIncludingThis >= newCodeBlock->numParameters()) {
        Register* newCallFrame = callFrame->registers() + registerOffset;
        return CallFrame::create(newCallFrame);
    }

    // Too few arguments -- copy arguments, then fill in missing arguments with undefined.
    size_t delta = newCodeBlock->numParameters() - argumentCountIncludingThis;
    CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset + delta);

    Register* dst = &newCallFrame->uncheckedR(CallFrame::thisArgumentOffset());
    Register* end = dst - argumentCountIncludingThis;
    for ( ; dst != end; --dst)
        *dst = *(dst - delta);

    end -= delta;
    for ( ; dst != end; --dst)
        *dst = jsUndefined();

    return newCallFrame;
}

JSValue eval(CallFrame* callFrame)
{
    if (!callFrame->argumentCount())
        return jsUndefined();

    JSValue program = callFrame->argument(0);
    if (!program.isString())
        return program;
    
    TopCallFrameSetter topCallFrame(callFrame->globalData(), callFrame);
    String programSource = asString(program)->value(callFrame);
    if (callFrame->hadException())
        return JSValue();
    
    CallFrame* callerFrame = callFrame->callerFrame();
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    JSScope* callerScopeChain = callerFrame->scope();
    EvalExecutable* eval = callerCodeBlock->evalCodeCache().tryGet(callerCodeBlock->isStrictMode(), programSource, callerScopeChain);

    if (!eval) {
        if (!callerCodeBlock->isStrictMode()) {
            // FIXME: We can use the preparser in strict mode, we just need additional logic
            // to prevent duplicates.
            if (programSource.is8Bit()) {
                LiteralParser<LChar> preparser(callFrame, programSource.characters8(), programSource.length(), NonStrictJSON);
                if (JSValue parsedObject = preparser.tryLiteralParse())
                    return parsedObject;
            } else {
                LiteralParser<UChar> preparser(callFrame, programSource.characters16(), programSource.length(), NonStrictJSON);
                if (JSValue parsedObject = preparser.tryLiteralParse())
                    return parsedObject;                
            }
        }
        
        // If the literal parser bailed, it should not have thrown exceptions.
        ASSERT(!callFrame->globalData().exception);

        JSValue exceptionValue;
        eval = callerCodeBlock->evalCodeCache().getSlow(callFrame, callerCodeBlock->ownerExecutable(), callerCodeBlock->isStrictMode(), programSource, callerScopeChain, exceptionValue);
        
        ASSERT(!eval == exceptionValue);
        if (UNLIKELY(!eval))
            return throwError(callFrame, exceptionValue);
    }

    JSValue thisValue = callerFrame->thisValue();
    ASSERT(isValidThisObject(thisValue, callFrame));
    Interpreter* interpreter = callFrame->globalData().interpreter;
    return interpreter->execute(eval, callFrame, thisValue, callerScopeChain, callFrame->registers() - interpreter->stack().begin() + 1 + JSStack::CallFrameHeaderSize);
}

CallFrame* loadVarargs(CallFrame* callFrame, JSStack* stack, JSValue thisValue, JSValue arguments, int firstFreeRegister)
{
    if (!arguments) { // f.apply(x, arguments), with arguments unmodified.
        unsigned argumentCountIncludingThis = callFrame->argumentCountIncludingThis();
        CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + firstFreeRegister + argumentCountIncludingThis + JSStack::CallFrameHeaderSize);
        if (argumentCountIncludingThis > Arguments::MaxArguments + 1 || !stack->grow(newCallFrame->registers())) {
            callFrame->globalData().exception = createStackOverflowError(callFrame);
            return 0;
        }

        newCallFrame->setArgumentCountIncludingThis(argumentCountIncludingThis);
        newCallFrame->setThisValue(thisValue);
        for (size_t i = 0; i < callFrame->argumentCount(); ++i)
            newCallFrame->setArgument(i, callFrame->argumentAfterCapture(i));
        return newCallFrame;
    }

    if (arguments.isUndefinedOrNull()) {
        CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + firstFreeRegister + 1 + JSStack::CallFrameHeaderSize);
        if (!stack->grow(newCallFrame->registers())) {
            callFrame->globalData().exception = createStackOverflowError(callFrame);
            return 0;
        }
        newCallFrame->setArgumentCountIncludingThis(1);
        newCallFrame->setThisValue(thisValue);
        return newCallFrame;
    }

    if (!arguments.isObject()) {
        callFrame->globalData().exception = createInvalidParamError(callFrame, "Function.prototype.apply", arguments);
        return 0;
    }

    if (asObject(arguments)->classInfo() == &Arguments::s_info) {
        Arguments* argsObject = asArguments(arguments);
        unsigned argCount = argsObject->length(callFrame);
        CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + firstFreeRegister + CallFrame::offsetFor(argCount + 1));
        if (argCount > Arguments::MaxArguments || !stack->grow(newCallFrame->registers())) {
            callFrame->globalData().exception = createStackOverflowError(callFrame);
            return 0;
        }
        newCallFrame->setArgumentCountIncludingThis(argCount + 1);
        newCallFrame->setThisValue(thisValue);
        argsObject->copyToArguments(callFrame, newCallFrame, argCount);
        return newCallFrame;
    }

    if (isJSArray(arguments)) {
        JSArray* array = asArray(arguments);
        unsigned argCount = array->length();
        CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + firstFreeRegister + CallFrame::offsetFor(argCount + 1));
        if (argCount > Arguments::MaxArguments || !stack->grow(newCallFrame->registers())) {
            callFrame->globalData().exception = createStackOverflowError(callFrame);
            return 0;
        }
        newCallFrame->setArgumentCountIncludingThis(argCount + 1);
        newCallFrame->setThisValue(thisValue);
        array->copyToArguments(callFrame, newCallFrame, argCount);
        return newCallFrame;
    }

    JSObject* argObject = asObject(arguments);
    unsigned argCount = argObject->get(callFrame, callFrame->propertyNames().length).toUInt32(callFrame);
    CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + firstFreeRegister + CallFrame::offsetFor(argCount + 1));
    if (argCount > Arguments::MaxArguments || !stack->grow(newCallFrame->registers())) {
        callFrame->globalData().exception = createStackOverflowError(callFrame);
        return 0;
    }
    newCallFrame->setArgumentCountIncludingThis(argCount + 1);
    newCallFrame->setThisValue(thisValue);
    for (size_t i = 0; i < argCount; ++i) {
        newCallFrame->setArgument(i, asObject(arguments)->get(callFrame, i));
        if (UNLIKELY(callFrame->globalData().exception))
            return 0;
    }
    return newCallFrame;
}

Interpreter::Interpreter()
    : m_sampleEntryDepth(0)
    , m_reentryDepth(0)
#if !ASSERT_DISABLED
    , m_initialized(false)
#endif
{
}

Interpreter::~Interpreter()
{
}

void Interpreter::initialize(bool canUseJIT)
{
    UNUSED_PARAM(canUseJIT);

#if ENABLE(COMPUTED_GOTO_OPCODES) && ENABLE(LLINT)
    m_opcodeTable = LLInt::opcodeMap();
    for (int i = 0; i < numOpcodeIDs; ++i)
        m_opcodeIDTable.add(m_opcodeTable[i], static_cast<OpcodeID>(i));
#endif

#if !ASSERT_DISABLED
    m_initialized = true;
#endif

#if ENABLE(OPCODE_SAMPLING)
    enableSampler();
#endif
}

#ifdef NDEBUG

void Interpreter::dumpCallFrame(CallFrame*)
{
}

#else

void Interpreter::dumpCallFrame(CallFrame* callFrame)
{
    callFrame->codeBlock()->dump(callFrame);
    dumpRegisters(callFrame);
}

void Interpreter::dumpRegisters(CallFrame* callFrame)
{
    dataLog("Register frame: \n\n");
    dataLog("-----------------------------------------------------------------------------\n");
    dataLog("            use            |   address  |                value               \n");
    dataLog("-----------------------------------------------------------------------------\n");

    CodeBlock* codeBlock = callFrame->codeBlock();
    const Register* it;
    const Register* end;

    it = callFrame->registers() - JSStack::CallFrameHeaderSize - callFrame->argumentCountIncludingThis();
    end = callFrame->registers() - JSStack::CallFrameHeaderSize;
    while (it < end) {
        JSValue v = it->jsValue();
        int registerNumber = it - callFrame->registers();
        String name = codeBlock->nameForRegister(registerNumber);
        dataLog("[r% 3d %14s]      | %10p | %-16s 0x%lld \n", registerNumber, name.ascii().data(), it, v.description(), (long long)JSValue::encode(v));
        it++;
    }
    
    dataLog("-----------------------------------------------------------------------------\n");
    dataLog("[ArgumentCount]            | %10p | %lu \n", it, (unsigned long) callFrame->argumentCount());
    ++it;
    dataLog("[CallerFrame]              | %10p | %p \n", it, callFrame->callerFrame());
    ++it;
    dataLog("[Callee]                   | %10p | %p \n", it, callFrame->callee());
    ++it;
    dataLog("[ScopeChain]               | %10p | %p \n", it, callFrame->scope());
    ++it;
#if ENABLE(JIT)
    AbstractPC pc = callFrame->abstractReturnPC(callFrame->globalData());
    if (pc.hasJITReturnAddress())
        dataLog("[ReturnJITPC]              | %10p | %p \n", it, pc.jitReturnAddress().value());
#endif
    unsigned bytecodeOffset = 0;
    int line = 0;
    getCallerInfo(&callFrame->globalData(), callFrame, line, bytecodeOffset);
    dataLog("[ReturnVPC]                | %10p | %d (line %d)\n", it, bytecodeOffset, line);
    ++it;
    dataLog("[CodeBlock]                | %10p | %p \n", it, callFrame->codeBlock());
    ++it;
    dataLog("-----------------------------------------------------------------------------\n");

    int registerCount = 0;

    end = it + codeBlock->m_numVars;
    if (it != end) {
        do {
            JSValue v = it->jsValue();
            int registerNumber = it - callFrame->registers();
            String name = codeBlock->nameForRegister(registerNumber);
            dataLog("[r% 3d %14s]      | %10p | %-16s 0x%lld \n", registerNumber, name.ascii().data(), it, v.description(), (long long)JSValue::encode(v));
            ++it;
            ++registerCount;
        } while (it != end);
    }
    dataLog("-----------------------------------------------------------------------------\n");

    end = it + codeBlock->m_numCalleeRegisters - codeBlock->m_numVars;
    if (it != end) {
        do {
            JSValue v = (*it).jsValue();
            dataLog("[r% 3d]                     | %10p | %-16s 0x%lld \n", registerCount, it, v.description(), (long long)JSValue::encode(v));
            ++it;
            ++registerCount;
        } while (it != end);
    }
    dataLog("-----------------------------------------------------------------------------\n");
}

#endif

bool Interpreter::isOpcode(Opcode opcode)
{
#if ENABLE(COMPUTED_GOTO_OPCODES)
#if !ENABLE(LLINT)
    return static_cast<OpcodeID>(bitwise_cast<uintptr_t>(opcode)) <= op_end;
#else
    return opcode != HashTraits<Opcode>::emptyValue()
        && !HashTraits<Opcode>::isDeletedValue(opcode)
        && m_opcodeIDTable.contains(opcode);
#endif
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}

NEVER_INLINE bool Interpreter::unwindCallFrame(CallFrame*& callFrame, JSValue exceptionValue, unsigned& bytecodeOffset, CodeBlock*& codeBlock)
{
    CodeBlock* oldCodeBlock = codeBlock;
    JSScope* scope = callFrame->scope();

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        if (callFrame->callee())
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerExecutable()->sourceID(), codeBlock->ownerExecutable()->lastLine(), 0);
        else
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerExecutable()->sourceID(), codeBlock->ownerExecutable()->lastLine(), 0);
    }

    JSValue activation;
    if (oldCodeBlock->codeType() == FunctionCode && oldCodeBlock->needsActivation()) {
        activation = callFrame->uncheckedR(oldCodeBlock->activationRegister()).jsValue();
        if (activation)
            jsCast<JSActivation*>(activation)->tearOff(*scope->globalData());
    }

    if (oldCodeBlock->codeType() == FunctionCode && oldCodeBlock->usesArguments()) {
        if (JSValue arguments = callFrame->uncheckedR(unmodifiedArgumentsRegister(oldCodeBlock->argumentsRegister())).jsValue()) {
            if (activation)
                jsCast<Arguments*>(arguments)->didTearOffActivation(callFrame, jsCast<JSActivation*>(activation));
            else
                jsCast<Arguments*>(arguments)->tearOff(callFrame);
        }
    }

    CallFrame* callerFrame = callFrame->callerFrame();
    callFrame->globalData().topCallFrame = callerFrame;
    if (callerFrame->hasHostCallFrameFlag())
        return false;

    codeBlock = callerFrame->codeBlock();
    
    // Because of how the JIT records call site->bytecode offset
    // information the JIT reports the bytecodeOffset for the returnPC
    // to be at the beginning of the opcode that has caused the call.
#if ENABLE(JIT) || ENABLE(LLINT)
    bytecodeOffset = codeBlock->bytecodeOffset(callerFrame, callFrame->returnPC());
#endif

    callFrame = callerFrame;
    return true;
}

static void appendSourceToError(CallFrame* callFrame, ErrorInstance* exception, unsigned bytecodeOffset)
{
    exception->clearAppendSourceToMessage();

    if (!callFrame->codeBlock()->hasExpressionInfo())
        return;

    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;

    CodeBlock* codeBlock = callFrame->codeBlock();
    codeBlock->expressionRangeForBytecodeOffset(bytecodeOffset, divotPoint, startOffset, endOffset);

    int expressionStart = divotPoint - startOffset;
    int expressionStop = divotPoint + endOffset;

    const String& sourceString = codeBlock->source()->source();
    if (!expressionStop || expressionStart > static_cast<int>(sourceString.length()))
        return;

    JSGlobalData* globalData = &callFrame->globalData();
    JSValue jsMessage = exception->getDirect(*globalData, globalData->propertyNames->message);
    if (!jsMessage || !jsMessage.isString())
        return;

    String message = asString(jsMessage)->value(callFrame);

    if (expressionStart < expressionStop)
        message =  makeString(message, " (evaluating '", codeBlock->source()->getRange(expressionStart, expressionStop), "')");
    else {
        // No range information, so give a few characters of context
        const StringImpl* data = sourceString.impl();
        int dataLength = sourceString.length();
        int start = expressionStart;
        int stop = expressionStart;
        // Get up to 20 characters of context to the left and right of the divot, clamping to the line.
        // then strip whitespace.
        while (start > 0 && (expressionStart - start < 20) && (*data)[start - 1] != '\n')
            start--;
        while (start < (expressionStart - 1) && isStrWhiteSpace((*data)[start]))
            start++;
        while (stop < dataLength && (stop - expressionStart < 20) && (*data)[stop] != '\n')
            stop++;
        while (stop > expressionStart && isStrWhiteSpace((*data)[stop - 1]))
            stop--;
        message = makeString(message, " (near '...", codeBlock->source()->getRange(start, stop), "...')");
    }

    exception->putDirect(*globalData, globalData->propertyNames->message, jsString(globalData, message));
}

static int getLineNumberForCallFrame(JSGlobalData* globalData, CallFrame* callFrame)
{
    UNUSED_PARAM(globalData);
    callFrame = callFrame->removeHostCallFrameFlag();
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (!codeBlock)
        return -1;
#if ENABLE(JIT) || ENABLE(LLINT)
#if ENABLE(DFG_JIT)
    if (codeBlock->getJITType() == JITCode::DFGJIT)
        return codeBlock->lineNumberForBytecodeOffset(codeBlock->codeOrigin(callFrame->codeOriginIndexForDFG()).bytecodeIndex);
#endif
    return codeBlock->lineNumberForBytecodeOffset(callFrame->bytecodeOffsetForNonDFGCode());
#endif
}

static CallFrame* getCallerInfo(JSGlobalData* globalData, CallFrame* callFrame, int& lineNumber, unsigned& bytecodeOffset)
{
    UNUSED_PARAM(globalData);
    bytecodeOffset = 0;
    lineNumber = -1;
    ASSERT(!callFrame->hasHostCallFrameFlag());
    CallFrame* callerFrame = callFrame->codeBlock() ? callFrame->trueCallerFrame() : callFrame->callerFrame()->removeHostCallFrameFlag();
    bool callframeIsHost = callerFrame->addHostCallFrameFlag() == callFrame->callerFrame();
    ASSERT(!callerFrame->hasHostCallFrameFlag());

    if (callerFrame == CallFrame::noCaller() || !callerFrame || !callerFrame->codeBlock())
        return callerFrame;
    
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    
#if ENABLE(JIT) || ENABLE(LLINT)
    if (!callFrame->hasReturnPC())
        callframeIsHost = true;
#endif
#if ENABLE(DFG_JIT)
    if (callFrame->isInlineCallFrame())
        callframeIsHost = false;
#endif

    if (callframeIsHost) {
        // Don't need to deal with inline callframes here as by definition we haven't
        // inlined a call with an intervening native call frame.
#if ENABLE(JIT) || ENABLE(LLINT)
#if ENABLE(DFG_JIT)
        if (callerCodeBlock && callerCodeBlock->getJITType() == JITCode::DFGJIT) {
            unsigned codeOriginIndex = callerFrame->codeOriginIndexForDFG();
            bytecodeOffset = callerCodeBlock->codeOrigin(codeOriginIndex).bytecodeIndex;
        } else
#endif
            bytecodeOffset = callerFrame->bytecodeOffsetForNonDFGCode();
#endif
    } else {
#if ENABLE(JIT) || ENABLE(LLINT)
    #if ENABLE(DFG_JIT)
        if (callFrame->isInlineCallFrame()) {
            InlineCallFrame* icf = callFrame->inlineCallFrame();
            bytecodeOffset = icf->caller.bytecodeIndex;
            if (InlineCallFrame* parentCallFrame = icf->caller.inlineCallFrame) {
                FunctionExecutable* executable = static_cast<FunctionExecutable*>(parentCallFrame->executable.get());
                CodeBlock* newCodeBlock = executable->baselineCodeBlockFor(parentCallFrame->isCall ? CodeForCall : CodeForConstruct);
                ASSERT(newCodeBlock);
                ASSERT(newCodeBlock->instructionCount() > bytecodeOffset);
                callerCodeBlock = newCodeBlock;
            }
        } else if (callerCodeBlock && callerCodeBlock->getJITType() == JITCode::DFGJIT) {
            CodeOrigin origin;
            if (!callerCodeBlock->codeOriginForReturn(callFrame->returnPC(), origin))
                ASSERT_NOT_REACHED();
            bytecodeOffset = origin.bytecodeIndex;
            if (InlineCallFrame* icf = origin.inlineCallFrame) {
                FunctionExecutable* executable = static_cast<FunctionExecutable*>(icf->executable.get());
                CodeBlock* newCodeBlock = executable->baselineCodeBlockFor(icf->isCall ? CodeForCall : CodeForConstruct);
                ASSERT(newCodeBlock);
                ASSERT(newCodeBlock->instructionCount() > bytecodeOffset);
                callerCodeBlock = newCodeBlock;
            }
        } else
    #endif
            bytecodeOffset = callerCodeBlock->bytecodeOffset(callerFrame, callFrame->returnPC());
#endif
    }

    lineNumber = callerCodeBlock->lineNumberForBytecodeOffset(bytecodeOffset);
    return callerFrame;
}

static ALWAYS_INLINE const String getSourceURLFromCallFrame(CallFrame* callFrame)
{
    ASSERT(!callFrame->hasHostCallFrameFlag());
    return callFrame->codeBlock()->ownerExecutable()->sourceURL();
}

static StackFrameCodeType getStackFrameCodeType(CallFrame* callFrame)
{
    ASSERT(!callFrame->hasHostCallFrameFlag());

    switch (callFrame->codeBlock()->codeType()) {
    case EvalCode:
        return StackFrameEvalCode;
    case FunctionCode:
        return StackFrameFunctionCode;
    case GlobalCode:
        return StackFrameGlobalCode;
    }
    ASSERT_NOT_REACHED();
    return StackFrameGlobalCode;
}

void Interpreter::getStackTrace(JSGlobalData* globalData, Vector<StackFrame>& results)
{
    CallFrame* callFrame = globalData->topCallFrame->removeHostCallFrameFlag();
    if (!callFrame || callFrame == CallFrame::noCaller()) 
        return;
    int line = getLineNumberForCallFrame(globalData, callFrame);

    callFrame = callFrame->trueCallFrameFromVMCode();

    while (callFrame && callFrame != CallFrame::noCaller()) {
        String sourceURL;
        if (callFrame->codeBlock()) {
            sourceURL = getSourceURLFromCallFrame(callFrame);
            StackFrame s = { Strong<JSObject>(*globalData, callFrame->callee()), getStackFrameCodeType(callFrame), Strong<ExecutableBase>(*globalData, callFrame->codeBlock()->ownerExecutable()), line, sourceURL};
            results.append(s);
        } else {
            StackFrame s = { Strong<JSObject>(*globalData, callFrame->callee()), StackFrameNativeCode, Strong<ExecutableBase>(), -1, String()};
            results.append(s);
        }
        unsigned unusedBytecodeOffset = 0;
        callFrame = getCallerInfo(globalData, callFrame, line, unusedBytecodeOffset);
    }
}

void Interpreter::addStackTraceIfNecessary(CallFrame* callFrame, JSObject* error)
{
    JSGlobalData* globalData = &callFrame->globalData();
    ASSERT(callFrame == globalData->topCallFrame || callFrame == callFrame->lexicalGlobalObject()->globalExec() || callFrame == callFrame->dynamicGlobalObject()->globalExec());
    if (error->hasProperty(callFrame, globalData->propertyNames->stack))
        return;

    Vector<StackFrame> stackTrace;
    getStackTrace(&callFrame->globalData(), stackTrace);
    
    if (stackTrace.isEmpty())
        return;
    
    JSGlobalObject* globalObject = 0;
    if (isTerminatedExecutionException(error) || isInterruptedExecutionException(error))
        globalObject = globalData->dynamicGlobalObject;
    else
        globalObject = error->globalObject();

    // FIXME: JSStringJoiner could be more efficient than StringBuilder here.
    StringBuilder builder;
    for (unsigned i = 0; i < stackTrace.size(); i++) {
        builder.append(String(stackTrace[i].toString(globalObject->globalExec()).impl()));
        if (i != stackTrace.size() - 1)
            builder.append('\n');
    }
    
    error->putDirect(*globalData, globalData->propertyNames->stack, jsString(globalData, builder.toString()), ReadOnly | DontDelete);
}

NEVER_INLINE HandlerInfo* Interpreter::throwException(CallFrame*& callFrame, JSValue& exceptionValue, unsigned bytecodeOffset)
{
    CodeBlock* codeBlock = callFrame->codeBlock();
    bool isInterrupt = false;

    ASSERT(!exceptionValue.isEmpty());
    ASSERT(!exceptionValue.isCell() || exceptionValue.asCell());
    // This shouldn't be possible (hence the assertions), but we're already in the slowest of
    // slow cases, so let's harden against it anyway to be safe.
    if (exceptionValue.isEmpty() || (exceptionValue.isCell() && !exceptionValue.asCell()))
        exceptionValue = jsNull();

    // Set up the exception object
    if (exceptionValue.isObject()) {
        JSObject* exception = asObject(exceptionValue);

        if (exception->isErrorInstance() && static_cast<ErrorInstance*>(exception)->appendSourceToMessage())
            appendSourceToError(callFrame, static_cast<ErrorInstance*>(exception), bytecodeOffset);

        if (!hasErrorInfo(callFrame, exception)) {
            // FIXME: should only really be adding these properties to VM generated exceptions,
            // but the inspector currently requires these for all thrown objects.
            addErrorInfo(callFrame, exception, codeBlock->lineNumberForBytecodeOffset(bytecodeOffset), codeBlock->ownerExecutable()->source());
        }

        isInterrupt = isInterruptedExecutionException(exception) || isTerminatedExecutionException(exception);
    }

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        bool hasHandler = codeBlock->handlerForBytecodeOffset(bytecodeOffset);
        debugger->exception(debuggerCallFrame, codeBlock->ownerExecutable()->sourceID(), codeBlock->lineNumberForBytecodeOffset(bytecodeOffset), 0, hasHandler);
    }

    // Calculate an exception handler vPC, unwinding call frames as necessary.
    HandlerInfo* handler = 0;
    while (isInterrupt || !(handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset))) {
        if (!unwindCallFrame(callFrame, exceptionValue, bytecodeOffset, codeBlock)) {
            if (Profiler* profiler = callFrame->globalData().enabledProfiler())
                profiler->exceptionUnwind(callFrame);
            callFrame->globalData().topCallFrame = callFrame;
            return 0;
        }
    }
    callFrame->globalData().topCallFrame = callFrame;

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->exceptionUnwind(callFrame);

    // Shrink the JS stack, in case stack overflow made it huge.
    Register* highWaterMark = 0;
    for (CallFrame* callerFrame = callFrame; callerFrame; callerFrame = callerFrame->callerFrame()->removeHostCallFrameFlag()) {
        CodeBlock* codeBlock = callerFrame->codeBlock();
        if (!codeBlock)
            continue;
        Register* callerHighWaterMark = callerFrame->registers() + codeBlock->m_numCalleeRegisters;
        highWaterMark = max(highWaterMark, callerHighWaterMark);
    }
    m_stack.shrink(highWaterMark);

    // Unwind the scope chain within the exception handler's call frame.
    JSScope* scope = callFrame->scope();
    int scopeDelta = 0;
    if (!codeBlock->needsFullScopeChain() || codeBlock->codeType() != FunctionCode 
        || callFrame->uncheckedR(codeBlock->activationRegister()).jsValue())
        scopeDelta = depth(codeBlock, scope) - handler->scopeDepth;
    ASSERT(scopeDelta >= 0);
    while (scopeDelta--)
        scope = scope->next();
    callFrame->setScope(scope);

    return handler;
}

static inline JSValue checkedReturn(JSValue returnValue)
{
    ASSERT(returnValue);
    return returnValue;
}

static inline JSObject* checkedReturn(JSObject* returnValue)
{
    ASSERT(returnValue);
    return returnValue;
}

class SamplingScope {
public:
    SamplingScope(Interpreter* interpreter)
        : m_interpreter(interpreter)
    {
        interpreter->startSampling();
    }
    ~SamplingScope()
    {
        m_interpreter->stopSampling();
    }
private:
    Interpreter* m_interpreter;
};

JSValue Interpreter::execute(ProgramExecutable* program, CallFrame* callFrame, JSObject* thisObj)
{
    SamplingScope samplingScope(this);
    
    JSScope* scope = callFrame->scope();
    ASSERT(isValidThisObject(thisObj, callFrame));
    ASSERT(!scope->globalData()->exception);
    ASSERT(!callFrame->globalData().isCollectorBusy());
    if (callFrame->globalData().isCollectorBusy())
        CRASH();

    StackStats::CheckPoint stackCheckPoint;
    if (m_reentryDepth >= MaxSmallThreadReentryDepth && m_reentryDepth >= callFrame->globalData().maxReentryDepth)
        return checkedReturn(throwStackOverflowError(callFrame));

    // First check if the "program" is actually just a JSON object. If so,
    // we'll handle the JSON object here. Else, we'll handle real JS code
    // below at failedJSONP.
    DynamicGlobalObjectScope globalObjectScope(*scope->globalData(), scope->globalObject());
    Vector<JSONPData> JSONPData;
    bool parseResult;
    const String programSource = program->source().toString();
    if (programSource.isNull())
        return jsUndefined();
    if (programSource.is8Bit()) {
        LiteralParser<LChar> literalParser(callFrame, programSource.characters8(), programSource.length(), JSONP);
        parseResult = literalParser.tryJSONPParse(JSONPData, scope->globalObject()->globalObjectMethodTable()->supportsRichSourceInfo(scope->globalObject()));
    } else {
        LiteralParser<UChar> literalParser(callFrame, programSource.characters16(), programSource.length(), JSONP);
        parseResult = literalParser.tryJSONPParse(JSONPData, scope->globalObject()->globalObjectMethodTable()->supportsRichSourceInfo(scope->globalObject()));
    }

    if (parseResult) {
        JSGlobalObject* globalObject = scope->globalObject();
        JSValue result;
        for (unsigned entry = 0; entry < JSONPData.size(); entry++) {
            Vector<JSONPPathEntry> JSONPPath;
            JSONPPath.swap(JSONPData[entry].m_path);
            JSValue JSONPValue = JSONPData[entry].m_value.get();
            if (JSONPPath.size() == 1 && JSONPPath[0].m_type == JSONPPathEntryTypeDeclare) {
                if (globalObject->hasProperty(callFrame, JSONPPath[0].m_pathEntryName)) {
                    PutPropertySlot slot;
                    globalObject->methodTable()->put(globalObject, callFrame, JSONPPath[0].m_pathEntryName, JSONPValue, slot);
                } else
                    globalObject->methodTable()->putDirectVirtual(globalObject, callFrame, JSONPPath[0].m_pathEntryName, JSONPValue, DontEnum | DontDelete);
                // var declarations return undefined
                result = jsUndefined();
                continue;
            }
            JSValue baseObject(globalObject);
            for (unsigned i = 0; i < JSONPPath.size() - 1; i++) {
                ASSERT(JSONPPath[i].m_type != JSONPPathEntryTypeDeclare);
                switch (JSONPPath[i].m_type) {
                case JSONPPathEntryTypeDot: {
                    if (i == 0) {
                        PropertySlot slot(globalObject);
                        if (!globalObject->getPropertySlot(callFrame, JSONPPath[i].m_pathEntryName, slot)) {
                            if (entry)
                                return throwError(callFrame, createUndefinedVariableError(globalObject->globalExec(), JSONPPath[i].m_pathEntryName));
                            goto failedJSONP;
                        }
                        baseObject = slot.getValue(callFrame, JSONPPath[i].m_pathEntryName);
                    } else
                        baseObject = baseObject.get(callFrame, JSONPPath[i].m_pathEntryName);
                    if (callFrame->hadException())
                        return jsUndefined();
                    continue;
                }
                case JSONPPathEntryTypeLookup: {
                    baseObject = baseObject.get(callFrame, JSONPPath[i].m_pathIndex);
                    if (callFrame->hadException())
                        return jsUndefined();
                    continue;
                }
                default:
                    ASSERT_NOT_REACHED();
                    return jsUndefined();
                }
            }
            PutPropertySlot slot;
            switch (JSONPPath.last().m_type) {
            case JSONPPathEntryTypeCall: {
                JSValue function = baseObject.get(callFrame, JSONPPath.last().m_pathEntryName);
                if (callFrame->hadException())
                    return jsUndefined();
                CallData callData;
                CallType callType = getCallData(function, callData);
                if (callType == CallTypeNone)
                    return throwError(callFrame, createNotAFunctionError(callFrame, function));
                MarkedArgumentBuffer jsonArg;
                jsonArg.append(JSONPValue);
                JSValue thisValue = JSONPPath.size() == 1 ? jsUndefined(): baseObject;
                JSONPValue = JSC::call(callFrame, function, callType, callData, thisValue, jsonArg);
                if (callFrame->hadException())
                    return jsUndefined();
                break;
            }
            case JSONPPathEntryTypeDot: {
                baseObject.put(callFrame, JSONPPath.last().m_pathEntryName, JSONPValue, slot);
                if (callFrame->hadException())
                    return jsUndefined();
                break;
            }
            case JSONPPathEntryTypeLookup: {
                baseObject.putByIndex(callFrame, JSONPPath.last().m_pathIndex, JSONPValue, slot.isStrictMode());
                if (callFrame->hadException())
                    return jsUndefined();
                break;
            }
            default:
                ASSERT_NOT_REACHED();
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
    JSObject* error = program->compile(callFrame, scope);
    if (error)
        return checkedReturn(throwError(callFrame, error));
    CodeBlock* codeBlock = &program->generatedBytecode();

    // Reserve stack space for this invocation:
    Register* oldEnd = m_stack.end();
    Register* newEnd = oldEnd + codeBlock->numParameters() + JSStack::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters;
    if (!m_stack.grow(newEnd))
        return checkedReturn(throwStackOverflowError(callFrame));

    // Push the call frame for this invocation:
    CallFrame* newCallFrame = CallFrame::create(oldEnd + codeBlock->numParameters() + JSStack::CallFrameHeaderSize);
    ASSERT(codeBlock->numParameters() == 1); // 1 parameter for 'this'.
    newCallFrame->init(codeBlock, 0, scope, CallFrame::noCaller(), codeBlock->numParameters(), 0);
    newCallFrame->setThisValue(thisObj);
    TopCallFrameSetter topCallFrame(callFrame->globalData(), newCallFrame);

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->willExecute(callFrame, program->sourceURL(), program->lineNo());

    // Execute the code:
    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());

        m_reentryDepth++;
#if ENABLE(LLINT_C_LOOP)
        result = LLInt::CLoop::execute(newCallFrame, llint_program_prologue);
#elif ENABLE(JIT)
        result = program->generatedJITCode().execute(&m_stack, newCallFrame, scope->globalData());
#endif // ENABLE(JIT)

        m_reentryDepth--;
    }

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->didExecute(callFrame, program->sourceURL(), program->lineNo());

    m_stack.shrink(oldEnd);

    return checkedReturn(result);
}

JSValue Interpreter::executeCall(CallFrame* callFrame, JSObject* function, CallType callType, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    ASSERT(isValidThisObject(thisValue, callFrame));
    ASSERT(!callFrame->hadException());
    ASSERT(!callFrame->globalData().isCollectorBusy());
    if (callFrame->globalData().isCollectorBusy())
        return jsNull();

    StackStats::CheckPoint stackCheckPoint;
    if (m_reentryDepth >= MaxSmallThreadReentryDepth && m_reentryDepth >= callFrame->globalData().maxReentryDepth)
        return checkedReturn(throwStackOverflowError(callFrame));

    Register* oldEnd = m_stack.end();
    ASSERT(callFrame->frameExtent() <= oldEnd || callFrame == callFrame->scope()->globalObject()->globalExec());
    int argCount = 1 + args.size(); // implicit "this" parameter
    size_t registerOffset = argCount + JSStack::CallFrameHeaderSize;

    CallFrame* newCallFrame = CallFrame::create(oldEnd + registerOffset);
    if (!m_stack.grow(newCallFrame->registers()))
        return checkedReturn(throwStackOverflowError(callFrame));

    newCallFrame->setThisValue(thisValue);
    for (size_t i = 0; i < args.size(); ++i)
        newCallFrame->setArgument(i, args.at(i));

    if (callType == CallTypeJS) {
        JSScope* callDataScope = callData.js.scope;

        DynamicGlobalObjectScope globalObjectScope(*callDataScope->globalData(), callDataScope->globalObject());

        JSObject* compileError = callData.js.functionExecutable->compileForCall(callFrame, callDataScope);
        if (UNLIKELY(!!compileError)) {
            m_stack.shrink(oldEnd);
            return checkedReturn(throwError(callFrame, compileError));
        }

        CodeBlock* newCodeBlock = &callData.js.functionExecutable->generatedBytecodeForCall();
        newCallFrame = slideRegisterWindowForCall(newCodeBlock, &m_stack, newCallFrame, 0, argCount);
        if (UNLIKELY(!newCallFrame)) {
            m_stack.shrink(oldEnd);
            return checkedReturn(throwStackOverflowError(callFrame));
        }

        newCallFrame->init(newCodeBlock, 0, callDataScope, callFrame->addHostCallFrameFlag(), argCount, function);

        TopCallFrameSetter topCallFrame(callFrame->globalData(), newCallFrame);

        if (Profiler* profiler = callFrame->globalData().enabledProfiler())
            profiler->willExecute(callFrame, function);

        JSValue result;
        {
            SamplingTool::CallRecord callRecord(m_sampler.get());

            m_reentryDepth++;  
#if ENABLE(LLINT_C_LOOP)
            result = LLInt::CLoop::execute(newCallFrame, llint_function_for_call_prologue);
#elif ENABLE(JIT)
            result = callData.js.functionExecutable->generatedJITCodeForCall().execute(&m_stack, newCallFrame, callDataScope->globalData());
#endif // ENABLE(JIT)

            m_reentryDepth--;
        }

        if (Profiler* profiler = callFrame->globalData().enabledProfiler())
            profiler->didExecute(callFrame, function);

        m_stack.shrink(oldEnd);
        return checkedReturn(result);
    }

    ASSERT(callType == CallTypeHost);
    JSScope* scope = callFrame->scope();
    newCallFrame->init(0, 0, scope, callFrame->addHostCallFrameFlag(), argCount, function);

    TopCallFrameSetter topCallFrame(callFrame->globalData(), newCallFrame);

    DynamicGlobalObjectScope globalObjectScope(*scope->globalData(), scope->globalObject());

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->willExecute(callFrame, function);

    JSValue result;
    {
        SamplingTool::HostCallRecord callRecord(m_sampler.get());
        result = JSValue::decode(callData.native.function(newCallFrame));
    }

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->didExecute(callFrame, function);

    m_stack.shrink(oldEnd);
    return checkedReturn(result);
}

JSObject* Interpreter::executeConstruct(CallFrame* callFrame, JSObject* constructor, ConstructType constructType, const ConstructData& constructData, const ArgList& args)
{
    ASSERT(!callFrame->hadException());
    ASSERT(!callFrame->globalData().isCollectorBusy());
    // We throw in this case because we have to return something "valid" but we're
    // already in an invalid state.
    if (callFrame->globalData().isCollectorBusy())
        return checkedReturn(throwStackOverflowError(callFrame));

    StackStats::CheckPoint stackCheckPoint;
    if (m_reentryDepth >= MaxSmallThreadReentryDepth && m_reentryDepth >= callFrame->globalData().maxReentryDepth)
        return checkedReturn(throwStackOverflowError(callFrame));

    Register* oldEnd = m_stack.end();
    int argCount = 1 + args.size(); // implicit "this" parameter
    size_t registerOffset = argCount + JSStack::CallFrameHeaderSize;

    if (!m_stack.grow(oldEnd + registerOffset))
        return checkedReturn(throwStackOverflowError(callFrame));

    CallFrame* newCallFrame = CallFrame::create(oldEnd + registerOffset);
    newCallFrame->setThisValue(jsUndefined());
    for (size_t i = 0; i < args.size(); ++i)
        newCallFrame->setArgument(i, args.at(i));

    if (constructType == ConstructTypeJS) {
        JSScope* constructDataScope = constructData.js.scope;

        DynamicGlobalObjectScope globalObjectScope(*constructDataScope->globalData(), constructDataScope->globalObject());

        JSObject* compileError = constructData.js.functionExecutable->compileForConstruct(callFrame, constructDataScope);
        if (UNLIKELY(!!compileError)) {
            m_stack.shrink(oldEnd);
            return checkedReturn(throwError(callFrame, compileError));
        }

        CodeBlock* newCodeBlock = &constructData.js.functionExecutable->generatedBytecodeForConstruct();
        newCallFrame = slideRegisterWindowForCall(newCodeBlock, &m_stack, newCallFrame, 0, argCount);
        if (UNLIKELY(!newCallFrame)) {
            m_stack.shrink(oldEnd);
            return checkedReturn(throwStackOverflowError(callFrame));
        }

        newCallFrame->init(newCodeBlock, 0, constructDataScope, callFrame->addHostCallFrameFlag(), argCount, constructor);

        TopCallFrameSetter topCallFrame(callFrame->globalData(), newCallFrame);

        if (Profiler* profiler = callFrame->globalData().enabledProfiler())
            profiler->willExecute(callFrame, constructor);

        JSValue result;
        {
            SamplingTool::CallRecord callRecord(m_sampler.get());

            m_reentryDepth++;  
#if ENABLE(LLINT_C_LOOP)
            result = LLInt::CLoop::execute(newCallFrame, llint_function_for_construct_prologue);
#elif ENABLE(JIT)
            result = constructData.js.functionExecutable->generatedJITCodeForConstruct().execute(&m_stack, newCallFrame, constructDataScope->globalData());
#endif // ENABLE(JIT)
            m_reentryDepth--;
        }

        if (Profiler* profiler = callFrame->globalData().enabledProfiler())
            profiler->didExecute(callFrame, constructor);

        m_stack.shrink(oldEnd);
        if (callFrame->hadException())
            return 0;
        ASSERT(result.isObject());
        return checkedReturn(asObject(result));
    }

    ASSERT(constructType == ConstructTypeHost);
    JSScope* scope = callFrame->scope();
    newCallFrame->init(0, 0, scope, callFrame->addHostCallFrameFlag(), argCount, constructor);

    TopCallFrameSetter topCallFrame(callFrame->globalData(), newCallFrame);

    DynamicGlobalObjectScope globalObjectScope(*scope->globalData(), scope->globalObject());

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->willExecute(callFrame, constructor);

    JSValue result;
    {
        SamplingTool::HostCallRecord callRecord(m_sampler.get());
        result = JSValue::decode(constructData.native.function(newCallFrame));
    }

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->didExecute(callFrame, constructor);

    m_stack.shrink(oldEnd);
    if (callFrame->hadException())
        return 0;
    ASSERT(result.isObject());
    return checkedReturn(asObject(result));
}

CallFrameClosure Interpreter::prepareForRepeatCall(FunctionExecutable* functionExecutable, CallFrame* callFrame, JSFunction* function, int argumentCountIncludingThis, JSScope* scope)
{
    ASSERT(!scope->globalData()->exception);
    
    if (callFrame->globalData().isCollectorBusy())
        return CallFrameClosure();

    StackStats::CheckPoint stackCheckPoint;
    if (m_reentryDepth >= MaxSmallThreadReentryDepth && m_reentryDepth >= callFrame->globalData().maxReentryDepth) {
        throwStackOverflowError(callFrame);
        return CallFrameClosure();
    }

    Register* oldEnd = m_stack.end();
    size_t registerOffset = argumentCountIncludingThis + JSStack::CallFrameHeaderSize;

    CallFrame* newCallFrame = CallFrame::create(oldEnd + registerOffset);
    if (!m_stack.grow(newCallFrame->registers())) {
        throwStackOverflowError(callFrame);
        return CallFrameClosure();
    }

    JSObject* error = functionExecutable->compileForCall(callFrame, scope);
    if (error) {
        throwError(callFrame, error);
        m_stack.shrink(oldEnd);
        return CallFrameClosure();
    }
    CodeBlock* codeBlock = &functionExecutable->generatedBytecodeForCall();

    newCallFrame = slideRegisterWindowForCall(codeBlock, &m_stack, newCallFrame, 0, argumentCountIncludingThis);
    if (UNLIKELY(!newCallFrame)) {
        throwStackOverflowError(callFrame);
        m_stack.shrink(oldEnd);
        return CallFrameClosure();
    }
    newCallFrame->init(codeBlock, 0, scope, callFrame->addHostCallFrameFlag(), argumentCountIncludingThis, function);  
    scope->globalData()->topCallFrame = newCallFrame;
    CallFrameClosure result = { callFrame, newCallFrame, function, functionExecutable, scope->globalData(), oldEnd, scope, codeBlock->numParameters(), argumentCountIncludingThis };
    return result;
}

JSValue Interpreter::execute(CallFrameClosure& closure) 
{
    SamplingScope samplingScope(this);
    
    ASSERT(!closure.oldCallFrame->globalData().isCollectorBusy());
    if (closure.oldCallFrame->globalData().isCollectorBusy())
        return jsNull();

    StackStats::CheckPoint stackCheckPoint;
    closure.resetCallFrame();
    if (Profiler* profiler = closure.oldCallFrame->globalData().enabledProfiler())
        profiler->willExecute(closure.oldCallFrame, closure.function);

    TopCallFrameSetter topCallFrame(*closure.globalData, closure.newCallFrame);

    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());
        
        m_reentryDepth++;  
#if ENABLE(LLINT_C_LOOP)
        result = LLInt::CLoop::execute(closure.newCallFrame, llint_function_for_call_prologue);
#elif ENABLE(JIT)
        result = closure.functionExecutable->generatedJITCodeForCall().execute(&m_stack, closure.newCallFrame, closure.globalData);
#endif // ENABLE(JIT)
        m_reentryDepth--;
    }

    if (Profiler* profiler = closure.oldCallFrame->globalData().enabledProfiler())
        profiler->didExecute(closure.oldCallFrame, closure.function);
    return checkedReturn(result);
}

void Interpreter::endRepeatCall(CallFrameClosure& closure)
{
    closure.globalData->topCallFrame = closure.oldCallFrame;
    m_stack.shrink(closure.oldEnd);
}

JSValue Interpreter::execute(EvalExecutable* eval, CallFrame* callFrame, JSValue thisValue, JSScope* scope, int globalRegisterOffset)
{
    SamplingScope samplingScope(this);
    
    ASSERT(isValidThisObject(thisValue, callFrame));
    ASSERT(!scope->globalData()->exception);
    ASSERT(!callFrame->globalData().isCollectorBusy());
    if (callFrame->globalData().isCollectorBusy())
        return jsNull();

    DynamicGlobalObjectScope globalObjectScope(*scope->globalData(), scope->globalObject());

    StackStats::CheckPoint stackCheckPoint;
    if (m_reentryDepth >= MaxSmallThreadReentryDepth && m_reentryDepth >= callFrame->globalData().maxReentryDepth)
        return checkedReturn(throwStackOverflowError(callFrame));

    JSObject* compileError = eval->compile(callFrame, scope);
    if (UNLIKELY(!!compileError))
        return checkedReturn(throwError(callFrame, compileError));
    EvalCodeBlock* codeBlock = &eval->generatedBytecode();

    JSObject* variableObject;
    for (JSScope* node = scope; ; node = node->next()) {
        ASSERT(node);
        if (node->isVariableObject() && !node->isNameScopeObject()) {
            variableObject = node;
            break;
        }
    }

    unsigned numVariables = codeBlock->numVariables();
    int numFunctions = codeBlock->numberOfFunctionDecls();
    if (numVariables || numFunctions) {
        if (codeBlock->isStrictMode()) {
            scope = StrictEvalActivation::create(callFrame);
            variableObject = scope;
        }
        // Scope for BatchedTransitionOptimizer
        BatchedTransitionOptimizer optimizer(callFrame->globalData(), variableObject);

        for (unsigned i = 0; i < numVariables; ++i) {
            const Identifier& ident = codeBlock->variable(i);
            if (!variableObject->hasProperty(callFrame, ident)) {
                PutPropertySlot slot;
                variableObject->methodTable()->put(variableObject, callFrame, ident, jsUndefined(), slot);
            }
        }

        for (int i = 0; i < numFunctions; ++i) {
            FunctionExecutable* function = codeBlock->functionDecl(i);
            PutPropertySlot slot;
            variableObject->methodTable()->put(variableObject, callFrame, function->name(), JSFunction::create(callFrame, function, scope), slot);
        }
    }

    Register* oldEnd = m_stack.end();
    Register* newEnd = m_stack.begin() + globalRegisterOffset + codeBlock->m_numCalleeRegisters;
    if (!m_stack.grow(newEnd))
        return checkedReturn(throwStackOverflowError(callFrame));

    CallFrame* newCallFrame = CallFrame::create(m_stack.begin() + globalRegisterOffset);

    ASSERT(codeBlock->numParameters() == 1); // 1 parameter for 'this'.
    newCallFrame->init(codeBlock, 0, scope, callFrame->addHostCallFrameFlag(), codeBlock->numParameters(), 0);
    newCallFrame->setThisValue(thisValue);

    TopCallFrameSetter topCallFrame(callFrame->globalData(), newCallFrame);

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->willExecute(callFrame, eval->sourceURL(), eval->lineNo());

    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());

        m_reentryDepth++;
        
#if ENABLE(LLINT_C_LOOP)
        result = LLInt::CLoop::execute(newCallFrame, llint_eval_prologue);
#elif ENABLE(JIT)
        result = eval->generatedJITCode().execute(&m_stack, newCallFrame, scope->globalData());
#endif // ENABLE(JIT)
        m_reentryDepth--;
    }

    if (Profiler* profiler = callFrame->globalData().enabledProfiler())
        profiler->didExecute(callFrame, eval->sourceURL(), eval->lineNo());

    m_stack.shrink(oldEnd);
    return checkedReturn(result);
}

NEVER_INLINE void Interpreter::debug(CallFrame* callFrame, DebugHookID debugHookID, int firstLine, int lastLine, int column)
{
    Debugger* debugger = callFrame->dynamicGlobalObject()->debugger();
    if (!debugger)
        return;

    switch (debugHookID) {
        case DidEnterCallFrame:
            debugger->callEvent(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), firstLine, column);
            return;
        case WillLeaveCallFrame:
            debugger->returnEvent(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), lastLine, column);
            return;
        case WillExecuteStatement:
            debugger->atStatement(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), firstLine, column);
            return;
        case WillExecuteProgram:
            debugger->willExecuteProgram(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), firstLine, column);
            return;
        case DidExecuteProgram:
            debugger->didExecuteProgram(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), lastLine, column);
            return;
        case DidReachBreakpoint:
            debugger->didReachBreakpoint(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), lastLine, column);
            return;
    }
}
    
JSValue Interpreter::retrieveArgumentsFromVMCode(CallFrame* callFrame, JSFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrameFromVMCode(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    Arguments* arguments = Arguments::create(functionCallFrame->globalData(), functionCallFrame);
    arguments->tearOff(functionCallFrame);
    return JSValue(arguments);
}

JSValue Interpreter::retrieveCallerFromVMCode(CallFrame* callFrame, JSFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrameFromVMCode(callFrame, function);

    if (!functionCallFrame)
        return jsNull();
    
    int lineNumber;
    unsigned bytecodeOffset;
    CallFrame* callerFrame = getCallerInfo(&callFrame->globalData(), functionCallFrame, lineNumber, bytecodeOffset);
    if (!callerFrame)
        return jsNull();
    JSValue caller = callerFrame->callee();
    if (!caller)
        return jsNull();

    // Skip over function bindings.
    ASSERT(caller.isObject());
    while (asObject(caller)->inherits(&JSBoundFunction::s_info)) {
        callerFrame = getCallerInfo(&callFrame->globalData(), callerFrame, lineNumber, bytecodeOffset);
        if (!callerFrame)
            return jsNull();
        caller = callerFrame->callee();
        if (!caller)
            return jsNull();
    }

    return caller;
}

void Interpreter::retrieveLastCaller(CallFrame* callFrame, int& lineNumber, intptr_t& sourceID, String& sourceURL, JSValue& function) const
{
    function = JSValue();
    lineNumber = -1;
    sourceURL = String();

    CallFrame* callerFrame = callFrame->callerFrame();
    if (callerFrame->hasHostCallFrameFlag())
        return;

    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    if (!callerCodeBlock)
        return;
    unsigned bytecodeOffset = 0;
    bytecodeOffset = callerCodeBlock->bytecodeOffset(callerFrame, callFrame->returnPC());
    lineNumber = callerCodeBlock->lineNumberForBytecodeOffset(bytecodeOffset - 1);
    sourceID = callerCodeBlock->ownerExecutable()->sourceID();
    sourceURL = callerCodeBlock->ownerExecutable()->sourceURL();
    function = callerFrame->callee();
}

CallFrame* Interpreter::findFunctionCallFrameFromVMCode(CallFrame* callFrame, JSFunction* function)
{
    for (CallFrame* candidate = callFrame->trueCallFrameFromVMCode(); candidate; candidate = candidate->trueCallerFrame()) {
        if (candidate->callee() == function)
            return candidate;
    }
    return 0;
}

void Interpreter::enableSampler()
{
#if ENABLE(OPCODE_SAMPLING)
    if (!m_sampler) {
        m_sampler = adoptPtr(new SamplingTool(this));
        m_sampler->setup();
    }
#endif
}
void Interpreter::dumpSampleData(ExecState* exec)
{
#if ENABLE(OPCODE_SAMPLING)
    if (m_sampler)
        m_sampler->dump(exec);
#else
    UNUSED_PARAM(exec);
#endif
}
void Interpreter::startSampling()
{
#if ENABLE(SAMPLING_THREAD)
    if (!m_sampleEntryDepth)
        SamplingThread::start();

    m_sampleEntryDepth++;
#endif
}
void Interpreter::stopSampling()
{
#if ENABLE(SAMPLING_THREAD)
    m_sampleEntryDepth--;
    if (!m_sampleEntryDepth)
        SamplingThread::stop();
#endif
}

} // namespace JSC
