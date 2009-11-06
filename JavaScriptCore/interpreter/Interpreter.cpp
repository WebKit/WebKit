/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "Collector.h"
#include "Debugger.h"
#include "DebuggerCallFrame.h"
#include "EvalCodeCache.h"
#include "ExceptionHelpers.h"
#include "GlobalEvalFunction.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSByteArray.h"
#include "JSFunction.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "LiteralParser.h"
#include "JSStaticScopeObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include "Parser.h"
#include "Profiler.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "SamplingTool.h"
#include <limits.h>
#include <stdio.h>
#include <wtf/Threading.h>

#if ENABLE(JIT)
#include "JIT.h"
#endif

using namespace std;

namespace JSC {

static ALWAYS_INLINE unsigned bytecodeOffsetForPC(CallFrame* callFrame, CodeBlock* codeBlock, void* pc)
{
#if ENABLE(JIT)
    return codeBlock->getBytecodeIndex(callFrame, ReturnAddressPtr(pc));
#else
    UNUSED_PARAM(callFrame);
    return static_cast<Instruction*>(pc) - codeBlock->instructions().begin();
#endif
}

// Returns the depth of the scope chain within a given call frame.
static int depth(CodeBlock* codeBlock, ScopeChain& sc)
{
    if (!codeBlock->needsFullScopeChain())
        return 0;
    return sc.localDepth();
}

#if USE(INTERPRETER)
NEVER_INLINE bool Interpreter::resolve(CallFrame* callFrame, Instruction* vPC, JSValue& exceptionValue)
{
    int dst = vPC[1].u.operand;
    int property = vPC[2].u.operand;

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifier(property);
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame->r(dst) = JSValue(result);
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveSkip(CallFrame* callFrame, Instruction* vPC, JSValue& exceptionValue)
{
    CodeBlock* codeBlock = callFrame->codeBlock();

    int dst = vPC[1].u.operand;
    int property = vPC[2].u.operand;
    int skip = vPC[3].u.operand + codeBlock->needsFullScopeChain();

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = codeBlock->identifier(property);
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame->r(dst) = JSValue(result);
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveGlobal(CallFrame* callFrame, Instruction* vPC, JSValue& exceptionValue)
{
    int dst = vPC[1].u.operand;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(vPC[2].u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int property = vPC[3].u.operand;
    Structure* structure = vPC[4].u.structure;
    int offset = vPC[5].u.operand;

    if (structure == globalObject->structure()) {
        callFrame->r(dst) = JSValue(globalObject->getDirectOffset(offset));
        return true;
    }

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifier(property);
    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValue result = slot.getValue(callFrame, ident);
        if (slot.isCacheable() && !globalObject->structure()->isUncacheableDictionary() && slot.slotBase() == globalObject) {
            if (vPC[4].u.structure)
                vPC[4].u.structure->deref();
            globalObject->structure()->ref();
            vPC[4] = globalObject->structure();
            vPC[5] = slot.cachedOffset();
            callFrame->r(dst) = JSValue(result);
            return true;
        }

        exceptionValue = callFrame->globalData().exception;
        if (exceptionValue)
            return false;
        callFrame->r(dst) = JSValue(result);
        return true;
    }

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

NEVER_INLINE void Interpreter::resolveBase(CallFrame* callFrame, Instruction* vPC)
{
    int dst = vPC[1].u.operand;
    int property = vPC[2].u.operand;
    callFrame->r(dst) = JSValue(JSC::resolveBase(callFrame, callFrame->codeBlock()->identifier(property), callFrame->scopeChain()));
}

NEVER_INLINE bool Interpreter::resolveBaseAndProperty(CallFrame* callFrame, Instruction* vPC, JSValue& exceptionValue)
{
    int baseDst = vPC[1].u.operand;
    int propDst = vPC[2].u.operand;
    int property = vPC[3].u.operand;

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifier(property);
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {
            JSValue result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame->r(propDst) = JSValue(result);
            callFrame->r(baseDst) = JSValue(base);
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

#endif // USE(INTERPRETER)

ALWAYS_INLINE CallFrame* Interpreter::slideRegisterWindowForCall(CodeBlock* newCodeBlock, RegisterFile* registerFile, CallFrame* callFrame, size_t registerOffset, int argc)
{
    Register* r = callFrame->registers();
    Register* newEnd = r + registerOffset + newCodeBlock->m_numCalleeRegisters;

    if (LIKELY(argc == newCodeBlock->m_numParameters)) { // correct number of arguments
        if (UNLIKELY(!registerFile->grow(newEnd)))
            return 0;
        r += registerOffset;
    } else if (argc < newCodeBlock->m_numParameters) { // too few arguments -- fill in the blanks
        size_t omittedArgCount = newCodeBlock->m_numParameters - argc;
        registerOffset += omittedArgCount;
        newEnd += omittedArgCount;
        if (!registerFile->grow(newEnd))
            return 0;
        r += registerOffset;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();
    } else { // too many arguments -- copy expected arguments, leaving the extra arguments behind
        size_t numParameters = newCodeBlock->m_numParameters;
        registerOffset += numParameters;
        newEnd += numParameters;

        if (!registerFile->grow(newEnd))
            return 0;
        r += registerOffset;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argc;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argc] = argv[i];
    }

    return CallFrame::create(r);
}

#if USE(INTERPRETER)
static NEVER_INLINE bool isInvalidParamForIn(CallFrame* callFrame, CodeBlock* codeBlock, const Instruction* vPC, JSValue value, JSValue& exceptionData)
{
    if (value.isObject())
        return false;
    exceptionData = createInvalidParamError(callFrame, "in" , value, vPC - codeBlock->instructions().begin(), codeBlock);
    return true;
}

static NEVER_INLINE bool isInvalidParamForInstanceOf(CallFrame* callFrame, CodeBlock* codeBlock, const Instruction* vPC, JSValue value, JSValue& exceptionData)
{
    if (value.isObject() && asObject(value)->structure()->typeInfo().implementsHasInstance())
        return false;
    exceptionData = createInvalidParamError(callFrame, "instanceof" , value, vPC - codeBlock->instructions().begin(), codeBlock);
    return true;
}
#endif

NEVER_INLINE JSValue Interpreter::callEval(CallFrame* callFrame, RegisterFile* registerFile, Register* argv, int argc, int registerOffset, JSValue& exceptionValue)
{
    if (argc < 2)
        return jsUndefined();

    JSValue program = argv[1].jsValue();

    if (!program.isString())
        return program;

    UString programSource = asString(program)->value();

    LiteralParser preparser(callFrame, programSource, LiteralParser::NonStrictJSON);
    if (JSValue parsedObject = preparser.tryLiteralParse())
        return parsedObject;

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    CodeBlock* codeBlock = callFrame->codeBlock();
    RefPtr<EvalExecutable> eval = codeBlock->evalCodeCache().get(callFrame, programSource, scopeChain, exceptionValue);

    JSValue result = jsUndefined();
    if (eval)
        result = callFrame->globalData().interpreter->execute(eval.get(), callFrame, callFrame->thisValue().toThisObject(callFrame), callFrame->registers() - registerFile->start() + registerOffset, scopeChain, &exceptionValue);

    return result;
}

Interpreter::Interpreter()
    : m_sampleEntryDepth(0)
    , m_reentryDepth(0)
{
    privateExecute(InitializeAndReturn, 0, 0, 0);
#if ENABLE(OPCODE_SAMPLING)
    enableSampler();
#endif
}

#ifndef NDEBUG

void Interpreter::dumpCallFrame(CallFrame* callFrame)
{
    callFrame->codeBlock()->dump(callFrame);
    dumpRegisters(callFrame);
}

void Interpreter::dumpRegisters(CallFrame* callFrame)
{
    printf("Register frame: \n\n");
    printf("-----------------------------------------------------------------------------\n");
    printf("            use            |   address  |                value               \n");
    printf("-----------------------------------------------------------------------------\n");

    CodeBlock* codeBlock = callFrame->codeBlock();
    RegisterFile* registerFile = &callFrame->scopeChain()->globalObject->globalData()->interpreter->registerFile();
    const Register* it;
    const Register* end;
    JSValue v;

    if (codeBlock->codeType() == GlobalCode) {
        it = registerFile->lastGlobal();
        end = it + registerFile->numGlobals();
        while (it != end) {
            v = (*it).jsValue();
#if USE(JSVALUE32_64)
            printf("[global var]               | %10p | %-16s 0x%llx \n", it, v.description(), JSValue::encode(v));
#else
            printf("[global var]               | %10p | %-16s %p \n", it, v.description(), JSValue::encode(v));
#endif
            ++it;
        }
        printf("-----------------------------------------------------------------------------\n");
    }
    
    it = callFrame->registers() - RegisterFile::CallFrameHeaderSize - codeBlock->m_numParameters;
    v = (*it).jsValue();
#if USE(JSVALUE32_64)
    printf("[this]                     | %10p | %-16s 0x%llx \n", it, v.description(), JSValue::encode(v)); ++it;
#else
    printf("[this]                     | %10p | %-16s %p \n", it, v.description(), JSValue::encode(v)); ++it;
#endif
    end = it + max(codeBlock->m_numParameters - 1, 0); // - 1 to skip "this"
    if (it != end) {
        do {
            v = (*it).jsValue();
#if USE(JSVALUE32_64)
            printf("[param]                    | %10p | %-16s 0x%llx \n", it, v.description(), JSValue::encode(v));
#else
            printf("[param]                    | %10p | %-16s %p \n", it, v.description(), JSValue::encode(v));
#endif
            ++it;
        } while (it != end);
    }
    printf("-----------------------------------------------------------------------------\n");
    printf("[CodeBlock]                | %10p | %p \n", it, (*it).codeBlock()); ++it;
    printf("[ScopeChain]               | %10p | %p \n", it, (*it).scopeChain()); ++it;
    printf("[CallerRegisters]          | %10p | %d \n", it, (*it).i()); ++it;
    printf("[ReturnPC]                 | %10p | %p \n", it, (*it).vPC()); ++it;
    printf("[ReturnValueRegister]      | %10p | %d \n", it, (*it).i()); ++it;
    printf("[ArgumentCount]            | %10p | %d \n", it, (*it).i()); ++it;
    printf("[Callee]                   | %10p | %p \n", it, (*it).function()); ++it;
    printf("[OptionalCalleeArguments]  | %10p | %p \n", it, (*it).arguments()); ++it;
    printf("-----------------------------------------------------------------------------\n");

    int registerCount = 0;

    end = it + codeBlock->m_numVars;
    if (it != end) {
        do {
            v = (*it).jsValue();
#if USE(JSVALUE32_64)
            printf("[r%2d]                      | %10p | %-16s 0x%llx \n", registerCount, it, v.description(), JSValue::encode(v));
#else
            printf("[r%2d]                      | %10p | %-16s %p \n", registerCount, it, v.description(), JSValue::encode(v));
#endif
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("-----------------------------------------------------------------------------\n");

    end = it + codeBlock->m_numCalleeRegisters - codeBlock->m_numVars;
    if (it != end) {
        do {
            v = (*it).jsValue();
#if USE(JSVALUE32_64)
            printf("[r%2d]                      | %10p | %-16s 0x%llx \n", registerCount, it, v.description(), JSValue::encode(v));
#else
            printf("[r%2d]                      | %10p | %-16s %p \n", registerCount, it, v.description(), JSValue::encode(v));
#endif
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("-----------------------------------------------------------------------------\n");
}

#endif

bool Interpreter::isOpcode(Opcode opcode)
{
#if HAVE(COMPUTED_GOTO)
    return opcode != HashTraits<Opcode>::emptyValue()
        && !HashTraits<Opcode>::isDeletedValue(opcode)
        && m_opcodeIDTable.contains(opcode);
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}

NEVER_INLINE bool Interpreter::unwindCallFrame(CallFrame*& callFrame, JSValue exceptionValue, unsigned& bytecodeOffset, CodeBlock*& codeBlock)
{
    CodeBlock* oldCodeBlock = codeBlock;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        if (callFrame->callee())
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerExecutable()->sourceID(), codeBlock->ownerExecutable()->lastLine());
        else
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerExecutable()->sourceID(), codeBlock->ownerExecutable()->lastLine());
    }

    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
        if (callFrame->callee())
            profiler->didExecute(callFrame, callFrame->callee());
        else
            profiler->didExecute(callFrame, codeBlock->ownerExecutable()->sourceURL(), codeBlock->ownerExecutable()->lineNo());
    }

    // If this call frame created an activation or an 'arguments' object, tear it off.
    if (oldCodeBlock->codeType() == FunctionCode && oldCodeBlock->needsFullScopeChain()) {
        while (!scopeChain->object->inherits(&JSActivation::info))
            scopeChain = scopeChain->pop();
        static_cast<JSActivation*>(scopeChain->object)->copyRegisters(callFrame->optionalCalleeArguments());
    } else if (Arguments* arguments = callFrame->optionalCalleeArguments()) {
        if (!arguments->isTornOff())
            arguments->copyRegisters();
    }

    if (oldCodeBlock->needsFullScopeChain())
        scopeChain->deref();

    void* returnPC = callFrame->returnPC();
    callFrame = callFrame->callerFrame();
    if (callFrame->hasHostCallFrameFlag())
        return false;

    codeBlock = callFrame->codeBlock();
    bytecodeOffset = bytecodeOffsetForPC(callFrame, codeBlock, returnPC);
    return true;
}

NEVER_INLINE HandlerInfo* Interpreter::throwException(CallFrame*& callFrame, JSValue& exceptionValue, unsigned bytecodeOffset, bool explicitThrow)
{
    // Set up the exception object

    CodeBlock* codeBlock = callFrame->codeBlock();
    if (exceptionValue.isObject()) {
        JSObject* exception = asObject(exceptionValue);
        if (exception->isNotAnObjectErrorStub()) {
            exception = createNotAnObjectError(callFrame, static_cast<JSNotAnObjectErrorStub*>(exception), bytecodeOffset, codeBlock);
            exceptionValue = exception;
        } else {
            if (!exception->hasProperty(callFrame, Identifier(callFrame, "line")) && 
                !exception->hasProperty(callFrame, Identifier(callFrame, "sourceId")) && 
                !exception->hasProperty(callFrame, Identifier(callFrame, "sourceURL")) && 
                !exception->hasProperty(callFrame, Identifier(callFrame, expressionBeginOffsetPropertyName)) && 
                !exception->hasProperty(callFrame, Identifier(callFrame, expressionCaretOffsetPropertyName)) && 
                !exception->hasProperty(callFrame, Identifier(callFrame, expressionEndOffsetPropertyName))) {
                if (explicitThrow) {
                    int startOffset = 0;
                    int endOffset = 0;
                    int divotPoint = 0;
                    int line = codeBlock->expressionRangeForBytecodeOffset(callFrame, bytecodeOffset, divotPoint, startOffset, endOffset);
                    exception->putWithAttributes(callFrame, Identifier(callFrame, "line"), jsNumber(callFrame, line), ReadOnly | DontDelete);
                    
                    // We only hit this path for error messages and throw statements, which don't have a specific failure position
                    // So we just give the full range of the error/throw statement.
                    exception->putWithAttributes(callFrame, Identifier(callFrame, expressionBeginOffsetPropertyName), jsNumber(callFrame, divotPoint - startOffset), ReadOnly | DontDelete);
                    exception->putWithAttributes(callFrame, Identifier(callFrame, expressionEndOffsetPropertyName), jsNumber(callFrame, divotPoint + endOffset), ReadOnly | DontDelete);
                } else
                    exception->putWithAttributes(callFrame, Identifier(callFrame, "line"), jsNumber(callFrame, codeBlock->lineNumberForBytecodeOffset(callFrame, bytecodeOffset)), ReadOnly | DontDelete);
                exception->putWithAttributes(callFrame, Identifier(callFrame, "sourceId"), jsNumber(callFrame, codeBlock->ownerExecutable()->sourceID()), ReadOnly | DontDelete);
                exception->putWithAttributes(callFrame, Identifier(callFrame, "sourceURL"), jsOwnedString(callFrame, codeBlock->ownerExecutable()->sourceURL()), ReadOnly | DontDelete);
            }
            
            if (exception->isWatchdogException()) {
                while (unwindCallFrame(callFrame, exceptionValue, bytecodeOffset, codeBlock)) {
                    // Don't need handler checks or anything, we just want to unroll all the JS callframes possible.
                }
                return 0;
            }
        }
    }

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        debugger->exception(debuggerCallFrame, codeBlock->ownerExecutable()->sourceID(), codeBlock->lineNumberForBytecodeOffset(callFrame, bytecodeOffset));
    }

    // If we throw in the middle of a call instruction, we need to notify
    // the profiler manually that the call instruction has returned, since
    // we'll never reach the relevant op_profile_did_call.
    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
#if !ENABLE(JIT)
        if (isCallBytecode(codeBlock->instructions()[bytecodeOffset].u.opcode))
            profiler->didExecute(callFrame, callFrame->r(codeBlock->instructions()[bytecodeOffset + 2].u.operand).jsValue());
        else if (codeBlock->instructions()[bytecodeOffset + 8].u.opcode == getOpcode(op_construct))
            profiler->didExecute(callFrame, callFrame->r(codeBlock->instructions()[bytecodeOffset + 10].u.operand).jsValue());
#else
        int functionRegisterIndex;
        if (codeBlock->functionRegisterForBytecodeOffset(bytecodeOffset, functionRegisterIndex))
            profiler->didExecute(callFrame, callFrame->r(functionRegisterIndex).jsValue());
#endif
    }

    // Calculate an exception handler vPC, unwinding call frames as necessary.

    HandlerInfo* handler = 0;
    while (!(handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset))) {
        if (!unwindCallFrame(callFrame, exceptionValue, bytecodeOffset, codeBlock))
            return 0;
    }

    // Now unwind the scope chain within the exception handler's call frame.

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ScopeChain sc(scopeChain);
    int scopeDelta = depth(codeBlock, sc) - handler->scopeDepth;
    ASSERT(scopeDelta >= 0);
    while (scopeDelta--)
        scopeChain = scopeChain->pop();
    callFrame->setScopeChain(scopeChain);

    return handler;
}

JSValue Interpreter::execute(ProgramExecutable* program, CallFrame* callFrame, ScopeChainNode* scopeChain, JSObject* thisObj, JSValue* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || m_reentryDepth >= MaxMainThreadReentryDepth) {
            *exception = createStackOverflowError(callFrame);
            return jsNull();
        }
    }

    CodeBlock* codeBlock = &program->bytecode(callFrame, scopeChain);

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = oldEnd + codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, scopeChain->globalObject);

    JSGlobalObject* lastGlobalObject = m_registerFile.globalObject();
    JSGlobalObject* globalObject = callFrame->dynamicGlobalObject();
    globalObject->copyGlobalsTo(m_registerFile);

    CallFrame* newCallFrame = CallFrame::create(oldEnd + codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize);
    newCallFrame->r(codeBlock->thisRegister()) = JSValue(thisObj);
    newCallFrame->init(codeBlock, 0, scopeChain, CallFrame::noCaller(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain())
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, program->sourceURL(), program->lineNo());

    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());

        m_reentryDepth++;
#if ENABLE(JIT)
        result = program->jitCode(newCallFrame, scopeChain).execute(&m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
        m_reentryDepth--;
    }

    if (*profiler)
        (*profiler)->didExecute(callFrame, program->sourceURL(), program->lineNo());

    if (m_reentryDepth && lastGlobalObject && globalObject != lastGlobalObject)
        lastGlobalObject->copyGlobalsTo(m_registerFile);

    m_registerFile.shrink(oldEnd);

    return result;
}

JSValue Interpreter::execute(FunctionExecutable* functionExecutable, CallFrame* callFrame, JSFunction* function, JSObject* thisObj, const ArgList& args, ScopeChainNode* scopeChain, JSValue* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || m_reentryDepth >= MaxMainThreadReentryDepth) {
            *exception = createStackOverflowError(callFrame);
            return jsNull();
        }
    }

    Register* oldEnd = m_registerFile.end();
    int argc = 1 + args.size(); // implicit "this" parameter

    if (!m_registerFile.grow(oldEnd + argc)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, scopeChain->globalObject);

    CallFrame* newCallFrame = CallFrame::create(oldEnd);
    size_t dst = 0;
    newCallFrame->r(0) = JSValue(thisObj);
    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it)
        newCallFrame->r(++dst) = *it;

    CodeBlock* codeBlock = &functionExecutable->bytecode(callFrame, scopeChain);
    newCallFrame = slideRegisterWindowForCall(codeBlock, &m_registerFile, newCallFrame, argc + RegisterFile::CallFrameHeaderSize, argc);
    if (UNLIKELY(!newCallFrame)) {
        *exception = createStackOverflowError(callFrame);
        m_registerFile.shrink(oldEnd);
        return jsNull();
    }
    // a 0 codeBlock indicates a built-in caller
    newCallFrame->init(codeBlock, 0, scopeChain, callFrame->addHostCallFrameFlag(), 0, argc, function);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(callFrame, function);

    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());

        m_reentryDepth++;
#if ENABLE(JIT)
        result = functionExecutable->jitCode(newCallFrame, scopeChain).execute(&m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
        m_reentryDepth--;
    }

    if (*profiler)
        (*profiler)->didExecute(callFrame, function);

    m_registerFile.shrink(oldEnd);
    return result;
}

CallFrameClosure Interpreter::prepareForRepeatCall(FunctionExecutable* FunctionExecutable, CallFrame* callFrame, JSFunction* function, int argCount, ScopeChainNode* scopeChain, JSValue* exception)
{
    ASSERT(!scopeChain->globalData->exception);
    
    if (m_reentryDepth >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || m_reentryDepth >= MaxMainThreadReentryDepth) {
            *exception = createStackOverflowError(callFrame);
            return CallFrameClosure();
        }
    }
    
    Register* oldEnd = m_registerFile.end();
    int argc = 1 + argCount; // implicit "this" parameter
    
    if (!m_registerFile.grow(oldEnd + argc)) {
        *exception = createStackOverflowError(callFrame);
        return CallFrameClosure();
    }

    CallFrame* newCallFrame = CallFrame::create(oldEnd);
    size_t dst = 0;
    for (int i = 0; i < argc; ++i)
        newCallFrame->r(++dst) = jsUndefined();
    
    CodeBlock* codeBlock = &FunctionExecutable->bytecode(callFrame, scopeChain);
    newCallFrame = slideRegisterWindowForCall(codeBlock, &m_registerFile, newCallFrame, argc + RegisterFile::CallFrameHeaderSize, argc);
    if (UNLIKELY(!newCallFrame)) {
        *exception = createStackOverflowError(callFrame);
        m_registerFile.shrink(oldEnd);
        return CallFrameClosure();
    }
    // a 0 codeBlock indicates a built-in caller
    newCallFrame->init(codeBlock, 0, scopeChain, callFrame->addHostCallFrameFlag(), 0, argc, function);
#if ENABLE(JIT)
    FunctionExecutable->jitCode(newCallFrame, scopeChain);
#endif

    CallFrameClosure result = { callFrame, newCallFrame, function, FunctionExecutable, scopeChain->globalData, oldEnd, scopeChain, codeBlock->m_numParameters, argc };
    return result;
}

JSValue Interpreter::execute(CallFrameClosure& closure, JSValue* exception) 
{
    closure.resetCallFrame();
    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(closure.oldCallFrame, closure.function);
    
    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());
        
        m_reentryDepth++;
#if ENABLE(JIT)
        result = closure.functionExecutable->generatedJITCode().execute(&m_registerFile, closure.newCallFrame, closure.globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, closure.newCallFrame, exception);
#endif
        m_reentryDepth--;
    }
    
    if (*profiler)
        (*profiler)->didExecute(closure.oldCallFrame, closure.function);
    return result;
}

void Interpreter::endRepeatCall(CallFrameClosure& closure)
{
    m_registerFile.shrink(closure.oldEnd);
}

JSValue Interpreter::execute(EvalExecutable* eval, CallFrame* callFrame, JSObject* thisObj, ScopeChainNode* scopeChain, JSValue* exception)
{
    return execute(eval, callFrame, thisObj, m_registerFile.size() + eval->bytecode(callFrame, scopeChain).m_numParameters + RegisterFile::CallFrameHeaderSize, scopeChain, exception);
}

JSValue Interpreter::execute(EvalExecutable* eval, CallFrame* callFrame, JSObject* thisObj, int globalRegisterOffset, ScopeChainNode* scopeChain, JSValue* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxSecondaryThreadReentryDepth) {
        if (!isMainThread() || m_reentryDepth >= MaxMainThreadReentryDepth) {
            *exception = createStackOverflowError(callFrame);
            return jsNull();
        }
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, scopeChain->globalObject);

    EvalCodeBlock* codeBlock = &eval->bytecode(callFrame, scopeChain);

    JSVariableObject* variableObject;
    for (ScopeChainNode* node = scopeChain; ; node = node->next) {
        ASSERT(node);
        if (node->object->isVariableObject()) {
            variableObject = static_cast<JSVariableObject*>(node->object);
            break;
        }
    }

    { // Scope for BatchedTransitionOptimizer

        BatchedTransitionOptimizer optimizer(variableObject);

        unsigned numVariables = codeBlock->numVariables();
        for (unsigned i = 0; i < numVariables; ++i) {
            const Identifier& ident = codeBlock->variable(i);
            if (!variableObject->hasProperty(callFrame, ident)) {
                PutPropertySlot slot;
                variableObject->put(callFrame, ident, jsUndefined(), slot);
            }
        }

        int numFunctions = codeBlock->numberOfFunctionDecls();
        for (int i = 0; i < numFunctions; ++i) {
            FunctionExecutable* function = codeBlock->functionDecl(i);
            PutPropertySlot slot;
            variableObject->put(callFrame, function->name(), function->make(callFrame, scopeChain), slot);
        }

    }

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = m_registerFile.start() + globalRegisterOffset + codeBlock->m_numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    CallFrame* newCallFrame = CallFrame::create(m_registerFile.start() + globalRegisterOffset);

    // a 0 codeBlock indicates a built-in caller
    newCallFrame->r(codeBlock->thisRegister()) = JSValue(thisObj);
    newCallFrame->init(codeBlock, 0, scopeChain, callFrame->addHostCallFrameFlag(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain())
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, eval->sourceURL(), eval->lineNo());

    JSValue result;
    {
        SamplingTool::CallRecord callRecord(m_sampler.get());

        m_reentryDepth++;
#if ENABLE(JIT)
        result = eval->jitCode(newCallFrame, scopeChain).execute(&m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
        m_reentryDepth--;
    }

    if (*profiler)
        (*profiler)->didExecute(callFrame, eval->sourceURL(), eval->lineNo());

    m_registerFile.shrink(oldEnd);
    return result;
}

NEVER_INLINE void Interpreter::debug(CallFrame* callFrame, DebugHookID debugHookID, int firstLine, int lastLine)
{
    Debugger* debugger = callFrame->dynamicGlobalObject()->debugger();
    if (!debugger)
        return;

    switch (debugHookID) {
        case DidEnterCallFrame:
            debugger->callEvent(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), firstLine);
            return;
        case WillLeaveCallFrame:
            debugger->returnEvent(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), lastLine);
            return;
        case WillExecuteStatement:
            debugger->atStatement(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), firstLine);
            return;
        case WillExecuteProgram:
            debugger->willExecuteProgram(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), firstLine);
            return;
        case DidExecuteProgram:
            debugger->didExecuteProgram(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), lastLine);
            return;
        case DidReachBreakpoint:
            debugger->didReachBreakpoint(callFrame, callFrame->codeBlock()->ownerExecutable()->sourceID(), lastLine);
            return;
    }
}
    
#if USE(INTERPRETER)
NEVER_INLINE ScopeChainNode* Interpreter::createExceptionScope(CallFrame* callFrame, const Instruction* vPC)
{
    int dst = vPC[1].u.operand;
    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& property = codeBlock->identifier(vPC[2].u.operand);
    JSValue value = callFrame->r(vPC[3].u.operand).jsValue();
    JSObject* scope = new (callFrame) JSStaticScopeObject(callFrame, property, value, DontDelete);
    callFrame->r(dst) = JSValue(scope);

    return callFrame->scopeChain()->push(scope);
}

NEVER_INLINE void Interpreter::tryCachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValue baseValue, const PutPropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_put_by_id))
        return;

    if (!baseValue.isCell())
        return;

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }
    
    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isUncacheableDictionary()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Cache miss: record Structure to compare against next time.
    Structure* lastStructure = vPC[4].u.structure;
    if (structure != lastStructure) {
        // First miss: record Structure to compare against next time.
        if (!lastStructure) {
            vPC[4] = structure;
            return;
        }

        // Second miss: give up.
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Cache hit: Specialize instruction and ref Structures.

    // If baseCell != slot.base(), then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Structure transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        if (structure->isDictionary()) {
            vPC[0] = getOpcode(op_put_by_id_generic);
            return;
        }

        // put_by_id_transition checks the prototype chain for setters.
        normalizePrototypeChain(callFrame, baseCell);

        vPC[0] = getOpcode(op_put_by_id_transition);
        vPC[4] = structure->previousID();
        vPC[5] = structure;
        vPC[6] = structure->prototypeChain(callFrame);
        vPC[7] = slot.cachedOffset();
        codeBlock->refStructures(vPC);
        return;
    }

    vPC[0] = getOpcode(op_put_by_id_replace);
    vPC[5] = slot.cachedOffset();
    codeBlock->refStructures(vPC);
}

NEVER_INLINE void Interpreter::uncachePutByID(CodeBlock* codeBlock, Instruction* vPC)
{
    codeBlock->derefStructures(vPC);
    vPC[0] = getOpcode(op_put_by_id);
    vPC[4] = 0;
}

NEVER_INLINE void Interpreter::tryCacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_get_by_id))
        return;

    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    JSGlobalData* globalData = &callFrame->globalData();
    if (isJSArray(globalData, baseValue) && propertyName == callFrame->propertyNames().length) {
        vPC[0] = getOpcode(op_get_array_length);
        return;
    }

    if (isJSString(globalData, baseValue) && propertyName == callFrame->propertyNames().length) {
        vPC[0] = getOpcode(op_get_string_length);
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    Structure* structure = asCell(baseValue)->structure();

    if (structure->isUncacheableDictionary()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    // Cache miss
    Structure* lastStructure = vPC[4].u.structure;
    if (structure != lastStructure) {
        // First miss: record Structure to compare against next time.
        if (!lastStructure) {
            vPC[4] = structure;
            return;
        }

        // Second miss: give up.
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    // Cache hit: Specialize instruction and ref Structures.

    if (slot.slotBase() == baseValue) {
        vPC[0] = getOpcode(op_get_by_id_self);
        vPC[5] = slot.cachedOffset();

        codeBlock->refStructures(vPC);
        return;
    }

    if (structure->isDictionary()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    if (slot.slotBase() == structure->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase().isObject());

        JSObject* baseObject = asObject(slot.slotBase());

        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (baseObject->structure()->isDictionary())
            baseObject->setStructure(Structure::fromDictionaryTransition(baseObject->structure()));

        ASSERT(!baseObject->structure()->isUncacheableDictionary());

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[5] = baseObject->structure();
        vPC[6] = slot.cachedOffset();

        codeBlock->refStructures(vPC);
        return;
    }

    size_t count = normalizePrototypeChain(callFrame, baseValue, slot.slotBase());
    if (!count) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    vPC[0] = getOpcode(op_get_by_id_chain);
    vPC[4] = structure;
    vPC[5] = structure->prototypeChain(callFrame);
    vPC[6] = count;
    vPC[7] = slot.cachedOffset();
    codeBlock->refStructures(vPC);
}

NEVER_INLINE void Interpreter::uncacheGetByID(CodeBlock* codeBlock, Instruction* vPC)
{
    codeBlock->derefStructures(vPC);
    vPC[0] = getOpcode(op_get_by_id);
    vPC[4] = 0;
}

#endif // USE(INTERPRETER)

JSValue Interpreter::privateExecute(ExecutionFlag flag, RegisterFile* registerFile, CallFrame* callFrame, JSValue* exception)
{
    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function.
    if (flag == InitializeAndReturn) {
        #if HAVE(COMPUTED_GOTO)
            #define ADD_BYTECODE(id, length) m_opcodeTable[id] = &&id;
                FOR_EACH_OPCODE_ID(ADD_BYTECODE);
            #undef ADD_BYTECODE

            #define ADD_OPCODE_ID(id, length) m_opcodeIDTable.add(&&id, id);
                FOR_EACH_OPCODE_ID(ADD_OPCODE_ID);
            #undef ADD_OPCODE_ID
            ASSERT(m_opcodeIDTable.size() == numOpcodeIDs);
        #endif // HAVE(COMPUTED_GOTO)
        return JSValue();
    }

#if ENABLE(JIT)
    // Mixing Interpreter + JIT is not supported.
    ASSERT_NOT_REACHED();
#endif
#if !USE(INTERPRETER)
    UNUSED_PARAM(registerFile);
    UNUSED_PARAM(callFrame);
    UNUSED_PARAM(exception);
    return JSValue();
#else

    JSGlobalData* globalData = &callFrame->globalData();
    JSValue exceptionValue;
    HandlerInfo* handler = 0;

    Instruction* vPC = callFrame->codeBlock()->instructions().begin();
    Profiler** enabledProfilerReference = Profiler::enabledProfilerReference();
    unsigned tickCount = globalData->timeoutChecker.ticksUntilNextCheck();

#define CHECK_FOR_EXCEPTION() \
    do { \
        if (UNLIKELY(globalData->exception != JSValue())) { \
            exceptionValue = globalData->exception; \
            goto vm_throw; \
        } \
    } while (0)

#if ENABLE(OPCODE_STATS)
    OpcodeStats::resetLastInstruction();
#endif

#define CHECK_FOR_TIMEOUT() \
    if (!--tickCount) { \
        if (globalData->timeoutChecker.didTimeOut(callFrame)) { \
            exceptionValue = jsNull(); \
            goto vm_throw; \
        } \
        tickCount = globalData->timeoutChecker.ticksUntilNextCheck(); \
    }
    
#if ENABLE(OPCODE_SAMPLING)
    #define SAMPLE(codeBlock, vPC) m_sampler->sample(codeBlock, vPC)
#else
    #define SAMPLE(codeBlock, vPC)
#endif

#if HAVE(COMPUTED_GOTO)
    #define NEXT_INSTRUCTION() SAMPLE(callFrame->codeBlock(), vPC); goto *vPC->u.opcode
#if ENABLE(OPCODE_STATS)
    #define DEFINE_OPCODE(opcode) opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define DEFINE_OPCODE(opcode) opcode:
#endif
    NEXT_INSTRUCTION();
#else
    #define NEXT_INSTRUCTION() SAMPLE(callFrame->codeBlock(), vPC); goto interpreterLoopStart
#if ENABLE(OPCODE_STATS)
    #define DEFINE_OPCODE(opcode) case opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define DEFINE_OPCODE(opcode) case opcode:
#endif
    while (1) { // iterator loop begins
    interpreterLoopStart:;
    switch (vPC->u.opcode)
#endif
    {
    DEFINE_OPCODE(op_new_object) {
        /* new_object dst(r)

           Constructs a new empty Object instance using the original
           constructor, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        callFrame->r(dst) = JSValue(constructEmptyObject(callFrame));

        vPC += OPCODE_LENGTH(op_new_object);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_array) {
        /* new_array dst(r) firstArg(r) argCount(n)

           Constructs a new Array instance using the original
           constructor, and puts the result in register dst.
           The array will contain argCount elements with values
           taken from registers starting at register firstArg.
        */
        int dst = vPC[1].u.operand;
        int firstArg = vPC[2].u.operand;
        int argCount = vPC[3].u.operand;
        ArgList args(callFrame->registers() + firstArg, argCount);
        callFrame->r(dst) = JSValue(constructArray(callFrame, args));

        vPC += OPCODE_LENGTH(op_new_array);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = vPC[1].u.operand;
        int regExp = vPC[2].u.operand;
        callFrame->r(dst) = JSValue(new (globalData) RegExpObject(callFrame->scopeChain()->globalObject->regExpStructure(), callFrame->codeBlock()->regexp(regExp)));

        vPC += OPCODE_LENGTH(op_new_regexp);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = callFrame->r(src);

        vPC += OPCODE_LENGTH(op_mov);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32())
            callFrame->r(dst) = jsBoolean(src1.asInt32() == src2.asInt32());
        else {
            JSValue result = jsBoolean(JSValue::equalSlowCase(callFrame, src1, src2));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_eq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_eq_null) {
        /* eq_null dst(r) src(r)

           Checks whether register src is null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src = callFrame->r(vPC[2].u.operand).jsValue();

        if (src.isUndefinedOrNull()) {
            callFrame->r(dst) = jsBoolean(true);
            vPC += OPCODE_LENGTH(op_eq_null);
            NEXT_INSTRUCTION();
        }
        
        callFrame->r(dst) = jsBoolean(src.isCell() && src.asCell()->structure()->typeInfo().masqueradesAsUndefined());
        vPC += OPCODE_LENGTH(op_eq_null);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32())
            callFrame->r(dst) = jsBoolean(src1.asInt32() != src2.asInt32());
        else {
            JSValue result = jsBoolean(!JSValue::equalSlowCase(callFrame, src1, src2));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_neq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_neq_null) {
        /* neq_null dst(r) src(r)

           Checks whether register src is not null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src = callFrame->r(vPC[2].u.operand).jsValue();

        if (src.isUndefinedOrNull()) {
            callFrame->r(dst) = jsBoolean(false);
            vPC += OPCODE_LENGTH(op_neq_null);
            NEXT_INSTRUCTION();
        }
        
        callFrame->r(dst) = jsBoolean(!src.isCell() || !asCell(src)->structure()->typeInfo().masqueradesAsUndefined());
        vPC += OPCODE_LENGTH(op_neq_null);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        callFrame->r(dst) = jsBoolean(JSValue::strictEqual(src1, src2));

        vPC += OPCODE_LENGTH(op_stricteq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        callFrame->r(dst) = jsBoolean(!JSValue::strictEqual(src1, src2));

        vPC += OPCODE_LENGTH(op_nstricteq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        JSValue result = jsBoolean(jsLess(callFrame, src1, src2));
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;

        vPC += OPCODE_LENGTH(op_less);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        JSValue result = jsBoolean(jsLessEq(callFrame, src1, src2));
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;

        vPC += OPCODE_LENGTH(op_lesseq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = vPC[1].u.operand;
        JSValue v = callFrame->r(srcDst).jsValue();
        if (v.isInt32() && v.asInt32() < INT_MAX)
            callFrame->r(srcDst) = jsNumber(callFrame, v.asInt32() + 1);
        else {
            JSValue result = jsNumber(callFrame, v.toNumber(callFrame) + 1);
            CHECK_FOR_EXCEPTION();
            callFrame->r(srcDst) = result;
        }

        vPC += OPCODE_LENGTH(op_pre_inc);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = vPC[1].u.operand;
        JSValue v = callFrame->r(srcDst).jsValue();
        if (v.isInt32() && v.asInt32() > INT_MIN)
            callFrame->r(srcDst) = jsNumber(callFrame, v.asInt32() - 1);
        else {
            JSValue result = jsNumber(callFrame, v.toNumber(callFrame) - 1);
            CHECK_FOR_EXCEPTION();
            callFrame->r(srcDst) = result;
        }

        vPC += OPCODE_LENGTH(op_pre_dec);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = vPC[1].u.operand;
        int srcDst = vPC[2].u.operand;
        JSValue v = callFrame->r(srcDst).jsValue();
        if (v.isInt32() && v.asInt32() < INT_MAX) {
            callFrame->r(srcDst) = jsNumber(callFrame, v.asInt32() + 1);
            callFrame->r(dst) = v;
        } else {
            JSValue number = callFrame->r(srcDst).jsValue().toJSNumber(callFrame);
            CHECK_FOR_EXCEPTION();
            callFrame->r(srcDst) = jsNumber(callFrame, number.uncheckedGetNumber() + 1);
            callFrame->r(dst) = number;
        }

        vPC += OPCODE_LENGTH(op_post_inc);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = vPC[1].u.operand;
        int srcDst = vPC[2].u.operand;
        JSValue v = callFrame->r(srcDst).jsValue();
        if (v.isInt32() && v.asInt32() > INT_MIN) {
            callFrame->r(srcDst) = jsNumber(callFrame, v.asInt32() - 1);
            callFrame->r(dst) = v;
        } else {
            JSValue number = callFrame->r(srcDst).jsValue().toJSNumber(callFrame);
            CHECK_FOR_EXCEPTION();
            callFrame->r(srcDst) = jsNumber(callFrame, number.uncheckedGetNumber() - 1);
            callFrame->r(dst) = number;
        }

        vPC += OPCODE_LENGTH(op_post_dec);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;

        JSValue srcVal = callFrame->r(src).jsValue();

        if (LIKELY(srcVal.isNumber()))
            callFrame->r(dst) = callFrame->r(src);
        else {
            JSValue result = srcVal.toJSNumber(callFrame);
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_to_jsnumber);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src = callFrame->r(vPC[2].u.operand).jsValue();
        if (src.isInt32() && src.asInt32())
            callFrame->r(dst) = jsNumber(callFrame, -src.asInt32());
        else {
            JSValue result = jsNumber(callFrame, -src.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_negate);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32() && !(src1.asInt32() | src2.asInt32() & 0xc0000000)) // no overflow
            callFrame->r(dst) = jsNumber(callFrame, src1.asInt32() + src2.asInt32());
        else {
            JSValue result = jsAdd(callFrame, src1, src2);
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }
        vPC += OPCODE_LENGTH(op_add);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32() && !(src1.asInt32() | src2.asInt32() >> 15)) // no overflow
                callFrame->r(dst) = jsNumber(callFrame, src1.asInt32() * src2.asInt32());
        else {
            JSValue result = jsNumber(callFrame, src1.toNumber(callFrame) * src2.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_mul);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_div) {
        /* div dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by the
           register divisor (converted to number), and puts the
           quotient in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue dividend = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue divisor = callFrame->r(vPC[3].u.operand).jsValue();

        JSValue result = jsNumber(callFrame, dividend.toNumber(callFrame) / divisor.toNumber(callFrame));
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;

        vPC += OPCODE_LENGTH(op_div);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_mod) {
        /* mod dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by
           register divisor (converted to number), and puts the
           remainder in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue dividend = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue divisor = callFrame->r(vPC[3].u.operand).jsValue();

        if (dividend.isInt32() && divisor.isInt32() && divisor.asInt32() != 0) {
            JSValue result = jsNumber(callFrame, dividend.asInt32() % divisor.asInt32());
            ASSERT(result);
            callFrame->r(dst) = result;
            vPC += OPCODE_LENGTH(op_mod);
            NEXT_INSTRUCTION();
        }

        // Conversion to double must happen outside the call to fmod since the
        // order of argument evaluation is not guaranteed.
        double d1 = dividend.toNumber(callFrame);
        double d2 = divisor.toNumber(callFrame);
        JSValue result = jsNumber(callFrame, fmod(d1, d2));
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_mod);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_sub) {
        /* sub dst(r) src1(r) src2(r)

           Subtracts register src2 (converted to number) from register
           src1 (converted to number), and puts the difference in
           register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32() && !(src1.asInt32() | src2.asInt32() & 0xc0000000)) // no overflow
            callFrame->r(dst) = jsNumber(callFrame, src1.asInt32() - src2.asInt32());
        else {
            JSValue result = jsNumber(callFrame, src1.toNumber(callFrame) - src2.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }
        vPC += OPCODE_LENGTH(op_sub);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_lshift) {
        /* lshift dst(r) val(r) shift(r)

           Performs left shift of register val (converted to int32) by
           register shift (converted to uint32), and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue val = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue shift = callFrame->r(vPC[3].u.operand).jsValue();

        if (val.isInt32() && shift.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, val.asInt32() << (shift.asInt32() & 0x1f));
        else {
            JSValue result = jsNumber(callFrame, (val.toInt32(callFrame)) << (shift.toUInt32(callFrame) & 0x1f));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_lshift);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_rshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs arithmetic right shift of register val (converted
           to int32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue val = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue shift = callFrame->r(vPC[3].u.operand).jsValue();

        if (val.isInt32() && shift.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, val.asInt32() >> (shift.asInt32() & 0x1f));
        else {
            JSValue result = jsNumber(callFrame, (val.toInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_rshift);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue val = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue shift = callFrame->r(vPC[3].u.operand).jsValue();
        if (val.isUInt32() && shift.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, val.asInt32() >> (shift.asInt32() & 0x1f));
        else {
            JSValue result = jsNumber(callFrame, (val.toUInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_urshift);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitand) {
        /* bitand dst(r) src1(r) src2(r)

           Computes bitwise AND of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, src1.asInt32() & src2.asInt32());
        else {
            JSValue result = jsNumber(callFrame, src1.toInt32(callFrame) & src2.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_bitand);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitxor) {
        /* bitxor dst(r) src1(r) src2(r)

           Computes bitwise XOR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, src1.asInt32() ^ src2.asInt32());
        else {
            JSValue result = jsNumber(callFrame, src1.toInt32(callFrame) ^ src2.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_bitxor);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitor) {
        /* bitor dst(r) src1(r) src2(r)

           Computes bitwise OR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the
           result in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src1 = callFrame->r(vPC[2].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[3].u.operand).jsValue();
        if (src1.isInt32() && src2.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, src1.asInt32() | src2.asInt32());
        else {
            JSValue result = jsNumber(callFrame, src1.toInt32(callFrame) | src2.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }

        vPC += OPCODE_LENGTH(op_bitor);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        JSValue src = callFrame->r(vPC[2].u.operand).jsValue();
        if (src.isInt32())
            callFrame->r(dst) = jsNumber(callFrame, ~src.asInt32());
        else {
            JSValue result = jsNumber(callFrame, ~src.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = result;
        }
        vPC += OPCODE_LENGTH(op_bitnot);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_not) {
        /* not dst(r) src(r)

           Computes logical NOT of register src (converted to
           boolean), and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        JSValue result = jsBoolean(!callFrame->r(src).jsValue().toBoolean(callFrame));
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;

        vPC += OPCODE_LENGTH(op_not);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_instanceof) {
        /* instanceof dst(r) value(r) constructor(r) constructorProto(r)

           Tests whether register value is an instance of register
           constructor, and puts the boolean result in register
           dst. Register constructorProto must contain the "prototype"
           property (not the actual prototype) of the object in
           register constructor. This lookup is separated so that
           polymorphic inline caching can apply.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = vPC[1].u.operand;
        int value = vPC[2].u.operand;
        int base = vPC[3].u.operand;
        int baseProto = vPC[4].u.operand;

        JSValue baseVal = callFrame->r(base).jsValue();

        if (isInvalidParamForInstanceOf(callFrame, callFrame->codeBlock(), vPC, baseVal, exceptionValue))
            goto vm_throw;

        bool result = asObject(baseVal)->hasInstance(callFrame, callFrame->r(value).jsValue(), callFrame->r(baseProto).jsValue());
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = jsBoolean(result);

        vPC += OPCODE_LENGTH(op_instanceof);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = JSValue(jsTypeStringForValue(callFrame, callFrame->r(src).jsValue()));

        vPC += OPCODE_LENGTH(op_typeof);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_undefined) {
        /* is_undefined dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "undefined", and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        JSValue v = callFrame->r(src).jsValue();
        callFrame->r(dst) = jsBoolean(v.isCell() ? v.asCell()->structure()->typeInfo().masqueradesAsUndefined() : v.isUndefined());

        vPC += OPCODE_LENGTH(op_is_undefined);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_boolean) {
        /* is_boolean dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "boolean", and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = jsBoolean(callFrame->r(src).jsValue().isBoolean());

        vPC += OPCODE_LENGTH(op_is_boolean);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_number) {
        /* is_number dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "number", and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = jsBoolean(callFrame->r(src).jsValue().isNumber());

        vPC += OPCODE_LENGTH(op_is_number);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_string) {
        /* is_string dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "string", and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = jsBoolean(callFrame->r(src).jsValue().isString());

        vPC += OPCODE_LENGTH(op_is_string);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_object) {
        /* is_object dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "object", and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = jsBoolean(jsIsObjectType(callFrame->r(src).jsValue()));

        vPC += OPCODE_LENGTH(op_is_object);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_function) {
        /* is_function dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "function", and puts the result
           in register dst.
        */
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        callFrame->r(dst) = jsBoolean(jsIsFunctionType(callFrame->r(src).jsValue()));

        vPC += OPCODE_LENGTH(op_is_function);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_in) {
        /* in dst(r) property(r) base(r)

           Tests whether register base has a property named register
           property, and puts the boolean result in register dst.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int base = vPC[3].u.operand;

        JSValue baseVal = callFrame->r(base).jsValue();
        if (isInvalidParamForIn(callFrame, callFrame->codeBlock(), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = asObject(baseVal);

        JSValue propName = callFrame->r(property).jsValue();

        uint32_t i;
        if (propName.getUInt32(i))
            callFrame->r(dst) = jsBoolean(baseObj->hasProperty(callFrame, i));
        else {
            Identifier property(callFrame, propName.toString(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = jsBoolean(baseObj->hasProperty(callFrame, property));
        }

        vPC += OPCODE_LENGTH(op_in);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_resolve) {
        /* resolve dst(r) property(id)

           Looks up the property named by identifier property in the
           scope chain, and writes the resulting value to register
           dst. If the property is not found, raises an exception.
        */
        if (UNLIKELY(!resolve(callFrame, vPC, exceptionValue)))
            goto vm_throw;

        vPC += OPCODE_LENGTH(op_resolve);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_resolve_skip) {
        /* resolve_skip dst(r) property(id) skip(n)

         Looks up the property named by identifier property in the
         scope chain skipping the top 'skip' levels, and writes the resulting
         value to register dst. If the property is not found, raises an exception.
         */
        if (UNLIKELY(!resolveSkip(callFrame, vPC, exceptionValue)))
            goto vm_throw;

        vPC += OPCODE_LENGTH(op_resolve_skip);

        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_resolve_global) {
        /* resolve_skip dst(r) globalObject(c) property(id) structure(sID) offset(n)
         
           Performs a dynamic property lookup for the given property, on the provided
           global object.  If structure matches the Structure of the global then perform
           a fast lookup using the case offset, otherwise fall back to a full resolve and
           cache the new structure and offset
         */
        if (UNLIKELY(!resolveGlobal(callFrame, vPC, exceptionValue)))
            goto vm_throw;
        
        vPC += OPCODE_LENGTH(op_resolve_global);
        
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_global_var) {
        /* get_global_var dst(r) globalObject(c) index(n)

           Gets the global var at global slot index and places it in register dst.
         */
        int dst = vPC[1].u.operand;
        JSGlobalObject* scope = static_cast<JSGlobalObject*>(vPC[2].u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = vPC[3].u.operand;

        callFrame->r(dst) = scope->registerAt(index);
        vPC += OPCODE_LENGTH(op_get_global_var);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_global_var) {
        /* put_global_var globalObject(c) index(n) value(r)
         
           Puts value into global slot index.
         */
        JSGlobalObject* scope = static_cast<JSGlobalObject*>(vPC[1].u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = vPC[2].u.operand;
        int value = vPC[3].u.operand;
        
        scope->registerAt(index) = JSValue(callFrame->r(value).jsValue());
        vPC += OPCODE_LENGTH(op_put_global_var);
        NEXT_INSTRUCTION();
    }            
    DEFINE_OPCODE(op_get_scoped_var) {
        /* get_scoped_var dst(r) index(n) skip(n)

         Loads the contents of the index-th local from the scope skip nodes from
         the top of the scope chain, and places it in register dst
         */
        int dst = vPC[1].u.operand;
        int index = vPC[2].u.operand;
        int skip = vPC[3].u.operand + callFrame->codeBlock()->needsFullScopeChain();

        ScopeChainNode* scopeChain = callFrame->scopeChain();
        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);
        while (skip--) {
            ++iter;
            ASSERT(iter != end);
        }

        ASSERT((*iter)->isVariableObject());
        JSVariableObject* scope = static_cast<JSVariableObject*>(*iter);
        callFrame->r(dst) = scope->registerAt(index);
        vPC += OPCODE_LENGTH(op_get_scoped_var);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_scoped_var) {
        /* put_scoped_var index(n) skip(n) value(r)

         */
        int index = vPC[1].u.operand;
        int skip = vPC[2].u.operand + callFrame->codeBlock()->needsFullScopeChain();
        int value = vPC[3].u.operand;

        ScopeChainNode* scopeChain = callFrame->scopeChain();
        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);
        while (skip--) {
            ++iter;
            ASSERT(iter != end);
        }

        ASSERT((*iter)->isVariableObject());
        JSVariableObject* scope = static_cast<JSVariableObject*>(*iter);
        scope->registerAt(index) = JSValue(callFrame->r(value).jsValue());
        vPC += OPCODE_LENGTH(op_put_scoped_var);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_resolve_base) {
        /* resolve_base dst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register dst. If none is found, the outermost scope (which
           will be the global object) is stored in register dst.
        */
        resolveBase(callFrame, vPC);

        vPC += OPCODE_LENGTH(op_resolve_base);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_resolve_with_base) {
        /* resolve_with_base baseDst(r) propDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register srcDst, and the retrieved property value to register
           propDst. If the property is not found, raises an exception.

           This is more efficient than doing resolve_base followed by
           resolve, or resolve_base followed by get_by_id, as it
           avoids duplicate hash lookups.
        */
        if (UNLIKELY(!resolveBaseAndProperty(callFrame, vPC, exceptionValue)))
            goto vm_throw;

        vPC += OPCODE_LENGTH(op_resolve_with_base);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id) {
        /* get_by_id dst(r) base(r) property(id) structure(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        CodeBlock* codeBlock = callFrame->codeBlock();
        Identifier& ident = codeBlock->identifier(property);
        JSValue baseValue = callFrame->r(base).jsValue();
        PropertySlot slot(baseValue);
        JSValue result = baseValue.get(callFrame, ident, slot);
        CHECK_FOR_EXCEPTION();

        tryCacheGetByID(callFrame, codeBlock, vPC, baseValue, ident, slot);

        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_get_by_id);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_self) {
        /* op_get_by_id_self dst(r) base(r) property(id) structure(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base. If the cache misses, op_get_by_id_self reverts to
           op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();

        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int dst = vPC[1].u.operand;
                int offset = vPC[5].u.operand;

                ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifier(vPC[3].u.operand)) == baseObject->getDirectOffset(offset));
                callFrame->r(dst) = JSValue(baseObject->getDirectOffset(offset));

                vPC += OPCODE_LENGTH(op_get_by_id_self);
                NEXT_INSTRUCTION();
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_proto) {
        /* op_get_by_id_proto dst(r) base(r) property(id) structure(sID) prototypeStructure(sID) offset(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype. If the cache misses, op_get_by_id_proto
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();

        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(structure->prototypeForLookup(callFrame).isObject());
                JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
                Structure* prototypeStructure = vPC[5].u.structure;

                if (LIKELY(protoObject->structure() == prototypeStructure)) {
                    int dst = vPC[1].u.operand;
                    int offset = vPC[6].u.operand;

                    ASSERT(protoObject->get(callFrame, callFrame->codeBlock()->identifier(vPC[3].u.operand)) == protoObject->getDirectOffset(offset));
                    ASSERT(baseValue.get(callFrame, callFrame->codeBlock()->identifier(vPC[3].u.operand)) == protoObject->getDirectOffset(offset));
                    callFrame->r(dst) = JSValue(protoObject->getDirectOffset(offset));

                    vPC += OPCODE_LENGTH(op_get_by_id_proto);
                    NEXT_INSTRUCTION();
                }
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_self_list) {
        // Polymorphic self access caching currently only supported when JITting.
        ASSERT_NOT_REACHED();
        // This case of the switch must not be empty, else (op_get_by_id_self_list == op_get_by_id_chain)!
        vPC += OPCODE_LENGTH(op_get_by_id_self_list);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_proto_list) {
        // Polymorphic prototype access caching currently only supported when JITting.
        ASSERT_NOT_REACHED();
        // This case of the switch must not be empty, else (op_get_by_id_proto_list == op_get_by_id_chain)!
        vPC += OPCODE_LENGTH(op_get_by_id_proto_list);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_chain) {
        /* op_get_by_id_chain dst(r) base(r) property(id) structure(sID) structureChain(chain) count(n) offset(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype chain. If the cache misses, op_get_by_id_chain
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();

        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                RefPtr<Structure>* it = vPC[5].u.structureChain->head();
                size_t count = vPC[6].u.operand;
                RefPtr<Structure>* end = it + count;

                while (true) {
                    JSObject* baseObject = asObject(baseCell->structure()->prototypeForLookup(callFrame));

                    if (UNLIKELY(baseObject->structure() != (*it).get()))
                        break;

                    if (++it == end) {
                        int dst = vPC[1].u.operand;
                        int offset = vPC[7].u.operand;

                        ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifier(vPC[3].u.operand)) == baseObject->getDirectOffset(offset));
                        ASSERT(baseValue.get(callFrame, callFrame->codeBlock()->identifier(vPC[3].u.operand)) == baseObject->getDirectOffset(offset));
                        callFrame->r(dst) = JSValue(baseObject->getDirectOffset(offset));

                        vPC += OPCODE_LENGTH(op_get_by_id_chain);
                        NEXT_INSTRUCTION();
                    }

                    // Update baseCell, so that next time around the loop we'll pick up the prototype's prototype.
                    baseCell = baseObject;
                }
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_generic) {
        /* op_get_by_id_generic dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        Identifier& ident = callFrame->codeBlock()->identifier(property);
        JSValue baseValue = callFrame->r(base).jsValue();
        PropertySlot slot(baseValue);
        JSValue result = baseValue.get(callFrame, ident, slot);
        CHECK_FOR_EXCEPTION();

        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_get_by_id_generic);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_array_length) {
        /* op_get_array_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the array in register base,
           and puts the result in register dst. If register base does not hold
           an array, op_get_array_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();
        if (LIKELY(isJSArray(globalData, baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame->r(dst) = jsNumber(callFrame, asArray(baseValue)->length());
            vPC += OPCODE_LENGTH(op_get_array_length);
            NEXT_INSTRUCTION();
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_string_length) {
        /* op_get_string_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the string in register base,
           and puts the result in register dst. If register base does not hold
           a string, op_get_string_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();
        if (LIKELY(isJSString(globalData, baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame->r(dst) = jsNumber(callFrame, asString(baseValue)->value().size());
            vPC += OPCODE_LENGTH(op_get_string_length);
            NEXT_INSTRUCTION();
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_by_id) {
        /* put_by_id base(r) property(id) value(r) nop(n) nop(n) nop(n) nop(n)

           Generic property access: Sets the property named by identifier
           property, belonging to register base, to register value.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */

        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int value = vPC[3].u.operand;

        CodeBlock* codeBlock = callFrame->codeBlock();
        JSValue baseValue = callFrame->r(base).jsValue();
        Identifier& ident = codeBlock->identifier(property);
        PutPropertySlot slot;
        baseValue.put(callFrame, ident, callFrame->r(value).jsValue(), slot);
        CHECK_FOR_EXCEPTION();

        tryCachePutByID(callFrame, codeBlock, vPC, baseValue, slot);

        vPC += OPCODE_LENGTH(op_put_by_id);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_by_id_transition) {
        /* op_put_by_id_transition base(r) property(id) value(r) oldStructure(sID) newStructure(sID) structureChain(chain) offset(n)
         
           Cached property access: Attempts to set a new property with a cached transition
           property named by identifier property, belonging to register base,
           to register value. If the cache misses, op_put_by_id_transition
           reverts to op_put_by_id_generic.
         
           Unlike many opcodes, this one does not write any output to
           the register file.
         */
        int base = vPC[1].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();
        
        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* oldStructure = vPC[4].u.structure;
            Structure* newStructure = vPC[5].u.structure;
            
            if (LIKELY(baseCell->structure() == oldStructure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);

                RefPtr<Structure>* it = vPC[6].u.structureChain->head();

                JSValue proto = baseObject->structure()->prototypeForLookup(callFrame);
                while (!proto.isNull()) {
                    if (UNLIKELY(asObject(proto)->structure() != (*it).get())) {
                        uncachePutByID(callFrame->codeBlock(), vPC);
                        NEXT_INSTRUCTION();
                    }
                    ++it;
                    proto = asObject(proto)->structure()->prototypeForLookup(callFrame);
                }

                baseObject->transitionTo(newStructure);

                int value = vPC[3].u.operand;
                unsigned offset = vPC[7].u.operand;
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifier(vPC[2].u.operand))) == offset);
                baseObject->putDirectOffset(offset, callFrame->r(value).jsValue());

                vPC += OPCODE_LENGTH(op_put_by_id_transition);
                NEXT_INSTRUCTION();
            }
        }
        
        uncachePutByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_by_id_replace) {
        /* op_put_by_id_replace base(r) property(id) value(r) structure(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to set a pre-existing, cached
           property named by identifier property, belonging to register base,
           to register value. If the cache misses, op_put_by_id_replace
           reverts to op_put_by_id.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        JSValue baseValue = callFrame->r(base).jsValue();

        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int value = vPC[3].u.operand;
                unsigned offset = vPC[5].u.operand;
                
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifier(vPC[2].u.operand))) == offset);
                baseObject->putDirectOffset(offset, callFrame->r(value).jsValue());

                vPC += OPCODE_LENGTH(op_put_by_id_replace);
                NEXT_INSTRUCTION();
            }
        }

        uncachePutByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_by_id_generic) {
        /* op_put_by_id_generic base(r) property(id) value(r) nop(n) nop(n) nop(n) nop(n)

           Generic property access: Sets the property named by identifier
           property, belonging to register base, to register value.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int value = vPC[3].u.operand;

        JSValue baseValue = callFrame->r(base).jsValue();
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        PutPropertySlot slot;
        baseValue.put(callFrame, ident, callFrame->r(value).jsValue(), slot);
        CHECK_FOR_EXCEPTION();

        vPC += OPCODE_LENGTH(op_put_by_id_generic);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_del_by_id) {
        /* del_by_id dst(r) base(r) property(id)

           Converts register base to Object, deletes the property
           named by identifier property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        JSObject* baseObj = callFrame->r(base).jsValue().toObject(callFrame);
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        JSValue result = jsBoolean(baseObj->deleteProperty(callFrame, ident));
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_del_by_id);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_pname) {
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;
        int expected = vPC[4].u.operand;
        int iter = vPC[5].u.operand;
        int i = vPC[6].u.operand;

        JSValue baseValue = callFrame->r(base).jsValue();
        JSPropertyNameIterator* it = callFrame->r(iter).propertyNameIterator();
        JSValue subscript = callFrame->r(property).jsValue();
        JSValue expectedSubscript = callFrame->r(expected).jsValue();
        int index = callFrame->r(i).i() - 1;
        JSValue result;
        int offset = 0;
        if (subscript == expectedSubscript && baseValue.isCell() && (baseValue.asCell()->structure() == it->cachedStructure()) && it->getOffset(index, offset)) {
            callFrame->r(dst) = asObject(baseValue)->getDirectOffset(offset);
            vPC += OPCODE_LENGTH(op_get_by_pname);
            NEXT_INSTRUCTION();
        }
        Identifier propertyName(callFrame, subscript.toString(callFrame));
        result = baseValue.get(callFrame, propertyName);
        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_get_by_pname);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_val) {
        /* get_by_val dst(r) base(r) property(r)

           Converts register base to Object, gets the property named
           by register property from the object, and puts the result
           in register dst. property is nominally converted to string
           but numbers are treated more efficiently.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;
        
        JSValue baseValue = callFrame->r(base).jsValue();
        JSValue subscript = callFrame->r(property).jsValue();

        JSValue result;

        if (LIKELY(subscript.isUInt32())) {
            uint32_t i = subscript.asUInt32();
            if (isJSArray(globalData, baseValue)) {
                JSArray* jsArray = asArray(baseValue);
                if (jsArray->canGetIndex(i))
                    result = jsArray->getIndex(i);
                else
                    result = jsArray->JSArray::get(callFrame, i);
            } else if (isJSString(globalData, baseValue) && asString(baseValue)->canGetIndex(i))
                result = asString(baseValue)->getIndex(&callFrame->globalData(), i);
            else if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(i))
                result = asByteArray(baseValue)->getIndex(callFrame, i);
            else
                result = baseValue.get(callFrame, i);
        } else {
            Identifier property(callFrame, subscript.toString(callFrame));
            result = baseValue.get(callFrame, property);
        }

        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_get_by_val);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_by_val) {
        /* put_by_val base(r) property(r) value(r)

           Sets register value on register base as the property named
           by register property. Base is converted to object
           first. register property is nominally converted to string
           but numbers are treated more efficiently.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int value = vPC[3].u.operand;

        JSValue baseValue = callFrame->r(base).jsValue();
        JSValue subscript = callFrame->r(property).jsValue();

        if (LIKELY(subscript.isUInt32())) {
            uint32_t i = subscript.asUInt32();
            if (isJSArray(globalData, baseValue)) {
                JSArray* jsArray = asArray(baseValue);
                if (jsArray->canSetIndex(i))
                    jsArray->setIndex(i, callFrame->r(value).jsValue());
                else
                    jsArray->JSArray::put(callFrame, i, callFrame->r(value).jsValue());
            } else if (isJSByteArray(globalData, baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
                JSByteArray* jsByteArray = asByteArray(baseValue);
                double dValue = 0;
                JSValue jsValue = callFrame->r(value).jsValue();
                if (jsValue.isInt32())
                    jsByteArray->setIndex(i, jsValue.asInt32());
                else if (jsValue.getNumber(dValue))
                    jsByteArray->setIndex(i, dValue);
                else
                    baseValue.put(callFrame, i, jsValue);
            } else
                baseValue.put(callFrame, i, callFrame->r(value).jsValue());
        } else {
            Identifier property(callFrame, subscript.toString(callFrame));
            if (!globalData->exception) { // Don't put to an object if toString threw an exception.
                PutPropertySlot slot;
                baseValue.put(callFrame, property, callFrame->r(value).jsValue(), slot);
            }
        }

        CHECK_FOR_EXCEPTION();
        vPC += OPCODE_LENGTH(op_put_by_val);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_del_by_val) {
        /* del_by_val dst(r) base(r) property(r)

           Converts register base to Object, deletes the property
           named by register property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        JSObject* baseObj = callFrame->r(base).jsValue().toObject(callFrame); // may throw

        JSValue subscript = callFrame->r(property).jsValue();
        JSValue result;
        uint32_t i;
        if (subscript.getUInt32(i))
            result = jsBoolean(baseObj->deleteProperty(callFrame, i));
        else {
            CHECK_FOR_EXCEPTION();
            Identifier property(callFrame, subscript.toString(callFrame));
            CHECK_FOR_EXCEPTION();
            result = jsBoolean(baseObj->deleteProperty(callFrame, property));
        }

        CHECK_FOR_EXCEPTION();
        callFrame->r(dst) = result;
        vPC += OPCODE_LENGTH(op_del_by_val);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_by_index) {
        /* put_by_index base(r) property(n) value(r)

           Sets register value on register base as the property named
           by the immediate number property. Base is converted to
           object first.

           Unlike many opcodes, this one does not write any output to
           the register file.

           This opcode is mainly used to initialize array literals.
        */
        int base = vPC[1].u.operand;
        unsigned property = vPC[2].u.operand;
        int value = vPC[3].u.operand;

        callFrame->r(base).jsValue().put(callFrame, property, callFrame->r(value).jsValue());

        vPC += OPCODE_LENGTH(op_put_by_index);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_loop) {
        /* loop target(offset)
         
           Jumps unconditionally to offset target from the current
           instruction.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
#if ENABLE(OPCODE_STATS)
        OpcodeStats::resetLastInstruction();
#endif
        int target = vPC[1].u.operand;
        CHECK_FOR_TIMEOUT();
        vPC += target;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jmp) {
        /* jmp target(offset)

           Jumps unconditionally to offset target from the current
           instruction.
        */
#if ENABLE(OPCODE_STATS)
        OpcodeStats::resetLastInstruction();
#endif
        int target = vPC[1].u.operand;

        vPC += target;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_loop_if_true) {
        /* loop_if_true cond(r) target(offset)
         
           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
        int cond = vPC[1].u.operand;
        int target = vPC[2].u.operand;
        if (callFrame->r(cond).jsValue().toBoolean(callFrame)) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION();
        }
        
        vPC += OPCODE_LENGTH(op_loop_if_true);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = vPC[1].u.operand;
        int target = vPC[2].u.operand;
        if (callFrame->r(cond).jsValue().toBoolean(callFrame)) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jtrue);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = vPC[1].u.operand;
        int target = vPC[2].u.operand;
        if (!callFrame->r(cond).jsValue().toBoolean(callFrame)) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jfalse);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jeq_null) {
        /* jeq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is null.
        */
        int src = vPC[1].u.operand;
        int target = vPC[2].u.operand;
        JSValue srcValue = callFrame->r(src).jsValue();

        if (srcValue.isUndefinedOrNull() || (srcValue.isCell() && srcValue.asCell()->structure()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jeq_null);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jneq_null) {
        /* jneq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is not null.
        */
        int src = vPC[1].u.operand;
        int target = vPC[2].u.operand;
        JSValue srcValue = callFrame->r(src).jsValue();

        if (!srcValue.isUndefinedOrNull() || (srcValue.isCell() && !srcValue.asCell()->structure()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jneq_null);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jneq_ptr) {
        /* jneq_ptr src(r) ptr(jsCell) target(offset)
         
           Jumps to offset target from the current instruction, if the value r is equal
           to ptr, using pointer equality.
         */
        int src = vPC[1].u.operand;
        JSValue ptr = JSValue(vPC[2].u.jsCell);
        int target = vPC[3].u.operand;
        JSValue srcValue = callFrame->r(src).jsValue();
        if (srcValue != ptr) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jneq_ptr);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_loop_if_less) {
        /* loop_if_less src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
        JSValue src1 = callFrame->r(vPC[1].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[2].u.operand).jsValue();
        int target = vPC[3].u.operand;
        
        bool result = jsLess(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION();
        }
        
        vPC += OPCODE_LENGTH(op_loop_if_less);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_loop_if_lesseq) {
        /* loop_if_lesseq src1(r) src2(r) target(offset)

           Checks whether register src1 is less than or equal to register
           src2, as with the ECMAScript '<=' operator, and then jumps to
           offset target from the current instruction, if and only if the 
           result of the comparison is true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
        */
        JSValue src1 = callFrame->r(vPC[1].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[2].u.operand).jsValue();
        int target = vPC[3].u.operand;
        
        bool result = jsLessEq(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION();
        }
        
        vPC += OPCODE_LENGTH(op_loop_if_lesseq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jnless) {
        /* jnless src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is false.
        */
        JSValue src1 = callFrame->r(vPC[1].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[2].u.operand).jsValue();
        int target = vPC[3].u.operand;

        bool result = jsLess(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jnless);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jnlesseq) {
        /* jnlesseq src1(r) src2(r) target(offset)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator,
           and then jumps to offset target from the current instruction,
           if and only if theresult of the comparison is false.
        */
        JSValue src1 = callFrame->r(vPC[1].u.operand).jsValue();
        JSValue src2 = callFrame->r(vPC[2].u.operand).jsValue();
        int target = vPC[3].u.operand;

        bool result = jsLessEq(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        vPC += OPCODE_LENGTH(op_jnlesseq);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_switch_imm) {
        /* switch_imm tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a range checked switch on the scrutinee value, using
           the tableIndex-th immediate switch jump table.  If the scrutinee value
           is an immediate number in the range covered by the referenced jump
           table, and the value at jumpTable[scrutinee value] is non-zero, then
           that value is used as the jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = vPC[1].u.operand;
        int defaultOffset = vPC[2].u.operand;
        JSValue scrutinee = callFrame->r(vPC[3].u.operand).jsValue();
        if (scrutinee.isInt32())
            vPC += callFrame->codeBlock()->immediateSwitchJumpTable(tableIndex).offsetForValue(scrutinee.asInt32(), defaultOffset);
        else {
            double value;
            int32_t intValue;
            if (scrutinee.getNumber(value) && ((intValue = static_cast<int32_t>(value)) == value))
                vPC += callFrame->codeBlock()->immediateSwitchJumpTable(tableIndex).offsetForValue(intValue, defaultOffset);
            else
                vPC += defaultOffset;
        }
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_switch_char) {
        /* switch_char tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a range checked switch on the scrutinee value, using
           the tableIndex-th character switch jump table.  If the scrutinee value
           is a single character string in the range covered by the referenced jump
           table, and the value at jumpTable[scrutinee value] is non-zero, then
           that value is used as the jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = vPC[1].u.operand;
        int defaultOffset = vPC[2].u.operand;
        JSValue scrutinee = callFrame->r(vPC[3].u.operand).jsValue();
        if (!scrutinee.isString())
            vPC += defaultOffset;
        else {
            UString::Rep* value = asString(scrutinee)->value().rep();
            if (value->size() != 1)
                vPC += defaultOffset;
            else
                vPC += callFrame->codeBlock()->characterSwitchJumpTable(tableIndex).offsetForValue(value->data()[0], defaultOffset);
        }
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_switch_string) {
        /* switch_string tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a sparse hashmap based switch on the value in the scrutinee
           register, using the tableIndex-th string switch jump table.  If the 
           scrutinee value is a string that exists as a key in the referenced 
           jump table, then the value associated with the string is used as the 
           jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = vPC[1].u.operand;
        int defaultOffset = vPC[2].u.operand;
        JSValue scrutinee = callFrame->r(vPC[3].u.operand).jsValue();
        if (!scrutinee.isString())
            vPC += defaultOffset;
        else 
            vPC += callFrame->codeBlock()->stringSwitchJumpTable(tableIndex).offsetForValue(asString(scrutinee)->value().rep(), defaultOffset);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_func) {
        /* new_func dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function declarations, and
           puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int func = vPC[2].u.operand;

        callFrame->r(dst) = JSValue(callFrame->codeBlock()->functionDecl(func)->make(callFrame, callFrame->scopeChain()));

        vPC += OPCODE_LENGTH(op_new_func);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_func_exp) {
        /* new_func_exp dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function expressions, and
           puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int funcIndex = vPC[2].u.operand;

        FunctionExecutable* function = callFrame->codeBlock()->functionExpr(funcIndex);
        JSFunction* func = function->make(callFrame, callFrame->scopeChain());

        /* 
            The Identifier in a FunctionExpression can be referenced from inside
            the FunctionExpression's FunctionBody to allow the function to call
            itself recursively. However, unlike in a FunctionDeclaration, the
            Identifier in a FunctionExpression cannot be referenced from and
            does not affect the scope enclosing the FunctionExpression.
         */
        if (!function->name().isNull()) {
            JSStaticScopeObject* functionScopeObject = new (callFrame) JSStaticScopeObject(callFrame, function->name(), func, ReadOnly | DontDelete);
            func->scope().push(functionScopeObject);
        }

        callFrame->r(dst) = JSValue(func);

        vPC += OPCODE_LENGTH(op_new_func_exp);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_call_eval) {
        /* call_eval dst(r) func(r) argCount(n) registerOffset(n)

           Call a function named "eval" with no explicit "this" value
           (which may therefore be the eval operator). If register
           thisVal is the global object, and register func contains
           that global object's original global eval function, then
           perform the eval operator in local scope (interpreting
           the argument registers as for the "call"
           opcode). Otherwise, act exactly as the "call" opcode would.
         */

        int dst = vPC[1].u.operand;
        int func = vPC[2].u.operand;
        int argCount = vPC[3].u.operand;
        int registerOffset = vPC[4].u.operand;

        JSValue funcVal = callFrame->r(func).jsValue();

        Register* newCallFrame = callFrame->registers() + registerOffset;
        Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
        JSValue thisValue = argv[0].jsValue();
        JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject;

        if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
            JSValue result = callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
            if (exceptionValue)
                goto vm_throw;
            callFrame->r(dst) = result;

            vPC += OPCODE_LENGTH(op_call_eval);
            NEXT_INSTRUCTION();
        }

        // We didn't find the blessed version of eval, so process this
        // instruction as a normal function call.
        // fall through to op_call
    }
    DEFINE_OPCODE(op_call) {
        /* call dst(r) func(r) argCount(n) registerOffset(n)

           Perform a function call.
           
           registerOffset is the distance the callFrame pointer should move
           before the VM initializes the new call frame's header.
           
           dst is where op_ret should store its result.
         */

        int dst = vPC[1].u.operand;
        int func = vPC[2].u.operand;
        int argCount = vPC[3].u.operand;
        int registerOffset = vPC[4].u.operand;

        JSValue v = callFrame->r(func).jsValue();

        CallData callData;
        CallType callType = v.getCallData(callData);

        if (callType == CallTypeJS) {
            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            CodeBlock* newCodeBlock = &callData.js.functionExecutable->bytecode(callFrame, callDataScopeChain);

            CallFrame* previousCallFrame = callFrame;

            callFrame = slideRegisterWindowForCall(newCodeBlock, registerFile, callFrame, registerOffset, argCount);
            if (UNLIKELY(!callFrame)) {
                callFrame = previousCallFrame;
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }

            callFrame->init(newCodeBlock, vPC + 5, callDataScopeChain, previousCallFrame, dst, argCount, asFunction(v));
            vPC = newCodeBlock->instructions().begin();

#if ENABLE(OPCODE_STATS)
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_INSTRUCTION();
        }

        if (callType == CallTypeHost) {
            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 5, scopeChain, callFrame, dst, argCount, 0);

            Register* thisRegister = newCallFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
            ArgList args(thisRegister + 1, argCount - 1);

            // FIXME: All host methods should be calling toThisObject, but this is not presently the case.
            JSValue thisValue = thisRegister->jsValue();
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();

            JSValue returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler.get());
                returnValue = callData.native.function(newCallFrame, asObject(v), thisValue, args);
            }
            CHECK_FOR_EXCEPTION();

            callFrame->r(dst) = returnValue;

            vPC += OPCODE_LENGTH(op_call);
            NEXT_INSTRUCTION();
        }

        ASSERT(callType == CallTypeNone);

        exceptionValue = createNotAFunctionError(callFrame, v, vPC - callFrame->codeBlock()->instructions().begin(), callFrame->codeBlock());
        goto vm_throw;
    }
    DEFINE_OPCODE(op_load_varargs) {
        int argCountDst = vPC[1].u.operand;
        int argsOffset = vPC[2].u.operand;
        
        JSValue arguments = callFrame->r(argsOffset).jsValue();
        int32_t argCount = 0;
        if (!arguments) {
            argCount = (uint32_t)(callFrame->argumentCount()) - 1;
            int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
            Register* newEnd = callFrame->registers() + sizeDelta;
            if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }
            ASSERT(!callFrame->callee()->isHostFunction());
            int32_t expectedParams = callFrame->callee()->jsExecutable()->parameterCount();
            int32_t inplaceArgs = min(argCount, expectedParams);
            int32_t i = 0;
            Register* argStore = callFrame->registers() + argsOffset;

            // First step is to copy the "expected" parameters from their normal location relative to the callframe
            for (; i < inplaceArgs; i++)
                argStore[i] = callFrame->registers()[i - RegisterFile::CallFrameHeaderSize - expectedParams];
            // Then we copy any additional arguments that may be further up the stack ('-1' to account for 'this')
            for (; i < argCount; i++)
                argStore[i] = callFrame->registers()[i - RegisterFile::CallFrameHeaderSize - expectedParams - argCount - 1];
        } else if (!arguments.isUndefinedOrNull()) {
            if (!arguments.isObject()) {
                exceptionValue = createInvalidParamError(callFrame, "Function.prototype.apply", arguments, vPC - callFrame->codeBlock()->instructions().begin(), callFrame->codeBlock());
                goto vm_throw;
            }
            if (asObject(arguments)->classInfo() == &Arguments::info) {
                Arguments* args = asArguments(arguments);
                argCount = args->numProvidedArguments(callFrame);
                int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
                Register* newEnd = callFrame->registers() + sizeDelta;
                if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                    exceptionValue = createStackOverflowError(callFrame);
                    goto vm_throw;
                }
                args->copyToRegisters(callFrame, callFrame->registers() + argsOffset, argCount);
            } else if (isJSArray(&callFrame->globalData(), arguments)) {
                JSArray* array = asArray(arguments);
                argCount = array->length();
                int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
                Register* newEnd = callFrame->registers() + sizeDelta;
                if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                    exceptionValue = createStackOverflowError(callFrame);
                    goto vm_throw;
                }
                array->copyToRegisters(callFrame, callFrame->registers() + argsOffset, argCount);
            } else if (asObject(arguments)->inherits(&JSArray::info)) {
                JSObject* argObject = asObject(arguments);
                argCount = argObject->get(callFrame, callFrame->propertyNames().length).toUInt32(callFrame);
                int32_t sizeDelta = argsOffset + argCount + RegisterFile::CallFrameHeaderSize;
                Register* newEnd = callFrame->registers() + sizeDelta;
                if (!registerFile->grow(newEnd) || ((newEnd - callFrame->registers()) != sizeDelta)) {
                    exceptionValue = createStackOverflowError(callFrame);
                    goto vm_throw;
                }
                Register* argsBuffer = callFrame->registers() + argsOffset;
                for (int32_t i = 0; i < argCount; ++i) {
                    argsBuffer[i] = asObject(arguments)->get(callFrame, i);
                    CHECK_FOR_EXCEPTION();
                }
            } else {
                if (!arguments.isObject()) {
                    exceptionValue = createInvalidParamError(callFrame, "Function.prototype.apply", arguments, vPC - callFrame->codeBlock()->instructions().begin(), callFrame->codeBlock());
                    goto vm_throw;
                }
            }
        }
        CHECK_FOR_EXCEPTION();
        callFrame->r(argCountDst) = Register::withInt(argCount + 1);
        vPC += OPCODE_LENGTH(op_load_varargs);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_call_varargs) {
        /* call_varargs dst(r) func(r) argCountReg(r) baseRegisterOffset(n)
         
         Perform a function call with a dynamic set of arguments.
         
         registerOffset is the distance the callFrame pointer should move
         before the VM initializes the new call frame's header, excluding
         space for arguments.
         
         dst is where op_ret should store its result.
         */
        
        int dst = vPC[1].u.operand;
        int func = vPC[2].u.operand;
        int argCountReg = vPC[3].u.operand;
        int registerOffset = vPC[4].u.operand;
        
        JSValue v = callFrame->r(func).jsValue();
        int argCount = callFrame->r(argCountReg).i();
        registerOffset += argCount;
        CallData callData;
        CallType callType = v.getCallData(callData);
        
        if (callType == CallTypeJS) {
            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            CodeBlock* newCodeBlock = &callData.js.functionExecutable->bytecode(callFrame, callDataScopeChain);
            
            CallFrame* previousCallFrame = callFrame;
            
            callFrame = slideRegisterWindowForCall(newCodeBlock, registerFile, callFrame, registerOffset, argCount);
            if (UNLIKELY(!callFrame)) {
                callFrame = previousCallFrame;
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }
            
            callFrame->init(newCodeBlock, vPC + 5, callDataScopeChain, previousCallFrame, dst, argCount, asFunction(v));
            vPC = newCodeBlock->instructions().begin();
            
#if ENABLE(OPCODE_STATS)
            OpcodeStats::resetLastInstruction();
#endif
            
            NEXT_INSTRUCTION();
        }
        
        if (callType == CallTypeHost) {
            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 5, scopeChain, callFrame, dst, argCount, 0);
            
            Register* thisRegister = newCallFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
            ArgList args(thisRegister + 1, argCount - 1);
            
            // FIXME: All host methods should be calling toThisObject, but this is not presently the case.
            JSValue thisValue = thisRegister->jsValue();
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();
            
            JSValue returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler.get());
                returnValue = callData.native.function(newCallFrame, asObject(v), thisValue, args);
            }
            CHECK_FOR_EXCEPTION();
            
            callFrame->r(dst) = returnValue;
            
            vPC += OPCODE_LENGTH(op_call_varargs);
            NEXT_INSTRUCTION();
        }
        
        ASSERT(callType == CallTypeNone);
        
        exceptionValue = createNotAFunctionError(callFrame, v, vPC - callFrame->codeBlock()->instructions().begin(), callFrame->codeBlock());
        goto vm_throw;
    }
    DEFINE_OPCODE(op_tear_off_activation) {
        /* tear_off_activation activation(r)

           Copy all locals and parameters to new memory allocated on
           the heap, and make the passed activation use this memory
           in the future when looking up entries in the symbol table.
           If there is an 'arguments' object, then it will also use
           this memory for storing the named parameters, but not any
           extra arguments.

           This opcode should only be used immediately before op_ret.
        */

        int src = vPC[1].u.operand;
        ASSERT(callFrame->codeBlock()->needsFullScopeChain());

        asActivation(callFrame->r(src).jsValue())->copyRegisters(callFrame->optionalCalleeArguments());

        vPC += OPCODE_LENGTH(op_tear_off_activation);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_tear_off_arguments) {
        /* tear_off_arguments

           Copy all arguments to new memory allocated on the heap,
           and make the 'arguments' object use this memory in the
           future when looking up named parameters, but not any
           extra arguments. If an activation object exists for the
           current function context, then the tear_off_activation
           opcode should be used instead.

           This opcode should only be used immediately before op_ret.
        */

        ASSERT(callFrame->codeBlock()->usesArguments() && !callFrame->codeBlock()->needsFullScopeChain());

        if (callFrame->optionalCalleeArguments())
            callFrame->optionalCalleeArguments()->copyRegisters();

        vPC += OPCODE_LENGTH(op_tear_off_arguments);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_ret) {
        /* ret result(r)
           
           Return register result as the return value of the current
           function call, writing it into the caller's expected return
           value register. In addition, unwind one call frame and
           restore the scope chain, code block instruction pointer and
           register base to those of the calling function.
        */

        int result = vPC[1].u.operand;

        if (callFrame->codeBlock()->needsFullScopeChain())
            callFrame->scopeChain()->deref();

        JSValue returnValue = callFrame->r(result).jsValue();

        vPC = callFrame->returnPC();
        int dst = callFrame->returnValueRegister();
        callFrame = callFrame->callerFrame();
        
        if (callFrame->hasHostCallFrameFlag())
            return returnValue;

        callFrame->r(dst) = returnValue;

        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_enter) {
        /* enter

           Initializes local variables to undefined and fills constant
           registers with their values. If the code block requires an
           activation, enter_with_activation should be used instead.

           This opcode should only be used at the beginning of a code
           block.
        */

        size_t i = 0;
        CodeBlock* codeBlock = callFrame->codeBlock();
        
        for (size_t count = codeBlock->m_numVars; i < count; ++i)
            callFrame->r(i) = jsUndefined();

        vPC += OPCODE_LENGTH(op_enter);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_enter_with_activation) {
        /* enter_with_activation dst(r)

           Initializes local variables to undefined, fills constant
           registers with their values, creates an activation object,
           and places the new activation both in dst and at the top
           of the scope chain. If the code block does not require an
           activation, enter should be used instead.

           This opcode should only be used at the beginning of a code
           block.
        */

        size_t i = 0;
        CodeBlock* codeBlock = callFrame->codeBlock();

        for (size_t count = codeBlock->m_numVars; i < count; ++i)
            callFrame->r(i) = jsUndefined();

        int dst = vPC[1].u.operand;
        JSActivation* activation = new (globalData) JSActivation(callFrame, static_cast<FunctionExecutable*>(codeBlock->ownerExecutable()));
        callFrame->r(dst) = JSValue(activation);
        callFrame->setScopeChain(callFrame->scopeChain()->copy()->push(activation));

        vPC += OPCODE_LENGTH(op_enter_with_activation);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_convert_this) {
        /* convert_this this(r)

           Takes the value in the 'this' register, converts it to a
           value that is suitable for use as the 'this' value, and
           stores it in the 'this' register. This opcode is emitted
           to avoid doing the conversion in the caller unnecessarily.

           This opcode should only be used at the beginning of a code
           block.
        */

        int thisRegister = vPC[1].u.operand;
        JSValue thisVal = callFrame->r(thisRegister).jsValue();
        if (thisVal.needsThisConversion())
            callFrame->r(thisRegister) = JSValue(thisVal.toThisObject(callFrame));

        vPC += OPCODE_LENGTH(op_convert_this);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_init_arguments) {
        /* create_arguments

           Initialises the arguments object reference to null to ensure
           we can correctly detect that we need to create it later (or
           avoid creating it altogether).

           This opcode should only be used at the beginning of a code
           block.
         */
        callFrame->r(RegisterFile::ArgumentsRegister) = JSValue();
        vPC += OPCODE_LENGTH(op_init_arguments);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_create_arguments) {
        /* create_arguments

           Creates the 'arguments' object and places it in both the
           'arguments' call frame slot and the local 'arguments'
           register, if it has not already been initialised.
         */
        
         if (!callFrame->r(RegisterFile::ArgumentsRegister).jsValue()) {
             Arguments* arguments = new (globalData) Arguments(callFrame);
             callFrame->setCalleeArguments(arguments);
             callFrame->r(RegisterFile::ArgumentsRegister) = JSValue(arguments);
         }
        vPC += OPCODE_LENGTH(op_create_arguments);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_construct) {
        /* construct dst(r) func(r) argCount(n) registerOffset(n) proto(r) thisRegister(r)

           Invoke register "func" as a constructor. For JS
           functions, the calling convention is exactly as for the
           "call" opcode, except that the "this" value is a newly
           created Object. For native constructors, no "this"
           value is passed. In either case, the argCount and registerOffset
           registers are interpreted as for the "call" opcode.

           Register proto must contain the prototype property of
           register func. This is to enable polymorphic inline
           caching of this lookup.
        */

        int dst = vPC[1].u.operand;
        int func = vPC[2].u.operand;
        int argCount = vPC[3].u.operand;
        int registerOffset = vPC[4].u.operand;
        int proto = vPC[5].u.operand;
        int thisRegister = vPC[6].u.operand;

        JSValue v = callFrame->r(func).jsValue();

        ConstructData constructData;
        ConstructType constructType = v.getConstructData(constructData);

        if (constructType == ConstructTypeJS) {
            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            CodeBlock* newCodeBlock = &constructData.js.functionExecutable->bytecode(callFrame, callDataScopeChain);

            Structure* structure;
            JSValue prototype = callFrame->r(proto).jsValue();
            if (prototype.isObject())
                structure = asObject(prototype)->inheritorID();
            else
                structure = callDataScopeChain->globalObject->emptyObjectStructure();
            JSObject* newObject = new (globalData) JSObject(structure);

            callFrame->r(thisRegister) = JSValue(newObject); // "this" value

            CallFrame* previousCallFrame = callFrame;

            callFrame = slideRegisterWindowForCall(newCodeBlock, registerFile, callFrame, registerOffset, argCount);
            if (UNLIKELY(!callFrame)) {
                callFrame = previousCallFrame;
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }

            callFrame->init(newCodeBlock, vPC + 7, callDataScopeChain, previousCallFrame, dst, argCount, asFunction(v));
            vPC = newCodeBlock->instructions().begin();

#if ENABLE(OPCODE_STATS)
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_INSTRUCTION();
        }

        if (constructType == ConstructTypeHost) {
            ArgList args(callFrame->registers() + thisRegister + 1, argCount - 1);

            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 7, scopeChain, callFrame, dst, argCount, 0);

            JSValue returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler.get());
                returnValue = constructData.native.function(newCallFrame, asObject(v), args);
            }
            CHECK_FOR_EXCEPTION();
            callFrame->r(dst) = JSValue(returnValue);

            vPC += OPCODE_LENGTH(op_construct);
            NEXT_INSTRUCTION();
        }

        ASSERT(constructType == ConstructTypeNone);

        exceptionValue = createNotAConstructorError(callFrame, v, vPC - callFrame->codeBlock()->instructions().begin(), callFrame->codeBlock());
        goto vm_throw;
    }
    DEFINE_OPCODE(op_construct_verify) {
        /* construct_verify dst(r) override(r)

           Verifies that register dst holds an object. If not, moves
           the object in register override to register dst.
        */

        int dst = vPC[1].u.operand;
        if (LIKELY(callFrame->r(dst).jsValue().isObject())) {
            vPC += OPCODE_LENGTH(op_construct_verify);
            NEXT_INSTRUCTION();
        }

        int override = vPC[2].u.operand;
        callFrame->r(dst) = callFrame->r(override);

        vPC += OPCODE_LENGTH(op_construct_verify);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_strcat) {
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;
        int count = vPC[3].u.operand;

        callFrame->r(dst) = concatenateStrings(callFrame, &callFrame->registers()[src], count);
        vPC += OPCODE_LENGTH(op_strcat);

        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_to_primitive) {
        int dst = vPC[1].u.operand;
        int src = vPC[2].u.operand;

        callFrame->r(dst) = callFrame->r(src).jsValue().toPrimitive(callFrame);
        vPC += OPCODE_LENGTH(op_to_primitive);

        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.  The contents of the register scope
           are replaced by the result of toObject conversion of the scope.
        */
        int scope = vPC[1].u.operand;
        JSValue v = callFrame->r(scope).jsValue();
        JSObject* o = v.toObject(callFrame);
        CHECK_FOR_EXCEPTION();

        callFrame->r(scope) = JSValue(o);
        callFrame->setScopeChain(callFrame->scopeChain()->push(o));

        vPC += OPCODE_LENGTH(op_push_scope);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        callFrame->setScopeChain(callFrame->scopeChain()->pop());

        vPC += OPCODE_LENGTH(op_pop_scope);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_pnames) {
        /* get_pnames dst(r) base(r) i(n) size(n) breakTarget(offset)

           Creates a property name list for register base and puts it
           in register dst, initializing i and size for iteration. If
           base is undefined or null, jumps to breakTarget.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int i = vPC[3].u.operand;
        int size = vPC[4].u.operand;
        int breakTarget = vPC[5].u.operand;

        JSValue v = callFrame->r(base).jsValue();
        if (v.isUndefinedOrNull()) {
            vPC += breakTarget;
            NEXT_INSTRUCTION();
        }

        JSObject* o = v.toObject(callFrame);
        Structure* structure = o->structure();
        JSPropertyNameIterator* jsPropertyNameIterator = structure->enumerationCache();
        if (!jsPropertyNameIterator || jsPropertyNameIterator->cachedPrototypeChain() != structure->prototypeChain(callFrame))
            jsPropertyNameIterator = JSPropertyNameIterator::create(callFrame, o);

        callFrame->r(dst) = jsPropertyNameIterator;
        callFrame->r(base) = JSValue(o);
        callFrame->r(i) = Register::withInt(0);
        callFrame->r(size) = Register::withInt(jsPropertyNameIterator->size());
        vPC += OPCODE_LENGTH(op_get_pnames);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_next_pname) {
        /* next_pname dst(r) base(r) i(n) size(n) iter(r) target(offset)

           Copies the next name from the property name list in
           register iter to dst, then jumps to offset target. If there are no
           names left, invalidates the iterator and continues to the next
           instruction.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int i = vPC[3].u.operand;
        int size = vPC[4].u.operand;
        int iter = vPC[5].u.operand;
        int target = vPC[6].u.operand;

        JSPropertyNameIterator* it = callFrame->r(iter).propertyNameIterator();
        while (callFrame->r(i).i() != callFrame->r(size).i()) {
            JSValue key = it->get(callFrame, asObject(callFrame->r(base).jsValue()), callFrame->r(i).i());
            callFrame->r(i) = Register::withInt(callFrame->r(i).i() + 1);
            if (key) {
                CHECK_FOR_TIMEOUT();
                callFrame->r(dst) = key;
                vPC += target;
                NEXT_INSTRUCTION();
            }
        }

        vPC += OPCODE_LENGTH(op_next_pname);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jmp_scopes) {
        /* jmp_scopes count(n) target(offset)

           Removes the a number of items from the current scope chain
           specified by immediate number count, then jumps to offset
           target.
        */
        int count = vPC[1].u.operand;
        int target = vPC[2].u.operand;

        ScopeChainNode* tmp = callFrame->scopeChain();
        while (count--)
            tmp = tmp->pop();
        callFrame->setScopeChain(tmp);

        vPC += target;
        NEXT_INSTRUCTION();
    }
#if HAVE(COMPUTED_GOTO)
    // Appease GCC
    goto *(&&skip_new_scope);
#endif
    DEFINE_OPCODE(op_push_new_scope) {
        /* new_scope dst(r) property(id) value(r)
         
           Constructs a new StaticScopeObject with property set to value.  That scope
           object is then pushed onto the ScopeChain.  The scope object is then stored
           in dst for GC.
         */
        callFrame->setScopeChain(createExceptionScope(callFrame, vPC));

        vPC += OPCODE_LENGTH(op_push_new_scope);
        NEXT_INSTRUCTION();
    }
#if HAVE(COMPUTED_GOTO)
    skip_new_scope:
#endif
    DEFINE_OPCODE(op_catch) {
        /* catch ex(r)

           Retrieves the VM's current exception and puts it in register
           ex. This is only valid after an exception has been raised,
           and usually forms the beginning of an exception handler.
        */
        ASSERT(exceptionValue);
        ASSERT(!globalData->exception);
        int ex = vPC[1].u.operand;
        callFrame->r(ex) = exceptionValue;
        exceptionValue = JSValue();

        vPC += OPCODE_LENGTH(op_catch);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_throw) {
        /* throw ex(r)

           Throws register ex as an exception. This involves three
           steps: first, it is set as the current exception in the
           VM's internal state, then the stack is unwound until an
           exception handler or a native code boundary is found, and
           then control resumes at the exception handler if any or
           else the script returns control to the nearest native caller.
        */

        int ex = vPC[1].u.operand;
        exceptionValue = callFrame->r(ex).jsValue();

        handler = throwException(callFrame, exceptionValue, vPC - callFrame->codeBlock()->instructions().begin(), true);
        if (!handler) {
            *exception = exceptionValue;
            return jsNull();
        }

        vPC = callFrame->codeBlock()->instructions().begin() + handler->target;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_error) {
        /* new_error dst(r) type(n) message(k)

           Constructs a new Error instance using the original
           constructor, using immediate number n as the type and
           constant message as the message string. The result is
           written to register dst.
        */
        int dst = vPC[1].u.operand;
        int type = vPC[2].u.operand;
        int message = vPC[3].u.operand;

        CodeBlock* codeBlock = callFrame->codeBlock();
        callFrame->r(dst) = JSValue(Error::create(callFrame, (ErrorType)type, callFrame->r(message).jsValue().toString(callFrame), codeBlock->lineNumberForBytecodeOffset(callFrame, vPC - codeBlock->instructions().begin()), codeBlock->ownerExecutable()->sourceID(), codeBlock->ownerExecutable()->sourceURL()));

        vPC += OPCODE_LENGTH(op_new_error);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_end) {
        /* end result(r)
           
           Return register result as the value of a global or eval
           program. Return control to the calling native code.
        */

        if (callFrame->codeBlock()->needsFullScopeChain()) {
            ScopeChainNode* scopeChain = callFrame->scopeChain();
            ASSERT(scopeChain->refCount > 1);
            scopeChain->deref();
        }
        int result = vPC[1].u.operand;
        return callFrame->r(result).jsValue();
    }
    DEFINE_OPCODE(op_put_getter) {
        /* put_getter base(r) property(id) function(r)

           Sets register function on register base as the getter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for getters defined
           in object literal form.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int function = vPC[3].u.operand;

        ASSERT(callFrame->r(base).jsValue().isObject());
        JSObject* baseObj = asObject(callFrame->r(base).jsValue());
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        ASSERT(callFrame->r(function).jsValue().isObject());
        baseObj->defineGetter(callFrame, ident, asObject(callFrame->r(function).jsValue()));

        vPC += OPCODE_LENGTH(op_put_getter);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_setter) {
        /* put_setter base(r) property(id) function(r)

           Sets register function on register base as the setter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for setters defined
           in object literal form.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        int property = vPC[2].u.operand;
        int function = vPC[3].u.operand;

        ASSERT(callFrame->r(base).jsValue().isObject());
        JSObject* baseObj = asObject(callFrame->r(base).jsValue());
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        ASSERT(callFrame->r(function).jsValue().isObject());
        baseObj->defineSetter(callFrame, ident, asObject(callFrame->r(function).jsValue()), 0);

        vPC += OPCODE_LENGTH(op_put_setter);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_method_check) {
        vPC++;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jsr) {
        /* jsr retAddrDst(r) target(offset)

           Places the address of the next instruction into the retAddrDst
           register and jumps to offset target from the current instruction.
        */
        int retAddrDst = vPC[1].u.operand;
        int target = vPC[2].u.operand;
        callFrame->r(retAddrDst) = vPC + OPCODE_LENGTH(op_jsr);

        vPC += target;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_sret) {
        /* sret retAddrSrc(r)

         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = vPC[1].u.operand;
        vPC = callFrame->r(retAddrSrc).vPC();
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_debug) {
        /* debug debugHookID(n) firstLine(n) lastLine(n)

         Notifies the debugger of the current state of execution. This opcode
         is only generated while the debugger is attached.
        */
        int debugHookID = vPC[1].u.operand;
        int firstLine = vPC[2].u.operand;
        int lastLine = vPC[3].u.operand;

        debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);

        vPC += OPCODE_LENGTH(op_debug);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_profile_will_call) {
        /* op_profile_will_call function(r)

         Notifies the profiler of the beginning of a function call. This opcode
         is only generated if developer tools are enabled.
        */
        int function = vPC[1].u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->willExecute(callFrame, callFrame->r(function).jsValue());

        vPC += OPCODE_LENGTH(op_profile_will_call);
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_profile_did_call) {
        /* op_profile_did_call function(r)

         Notifies the profiler of the end of a function call. This opcode
         is only generated if developer tools are enabled.
        */
        int function = vPC[1].u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->didExecute(callFrame, callFrame->r(function).jsValue());

        vPC += OPCODE_LENGTH(op_profile_did_call);
        NEXT_INSTRUCTION();
    }
    vm_throw: {
        globalData->exception = JSValue();
        if (!tickCount) {
            // The exceptionValue is a lie! (GCC produces bad code for reasons I 
            // cannot fathom if we don't assign to the exceptionValue before branching)
            exceptionValue = createInterruptedExecutionException(globalData);
        }
        handler = throwException(callFrame, exceptionValue, vPC - callFrame->codeBlock()->instructions().begin(), false);
        if (!handler) {
            *exception = exceptionValue;
            return jsNull();
        }

        vPC = callFrame->codeBlock()->instructions().begin() + handler->target;
        NEXT_INSTRUCTION();
    }
    }
#if !HAVE(COMPUTED_GOTO)
    } // iterator loop ends
#endif
#endif // USE(INTERPRETER)
    #undef NEXT_INSTRUCTION
    #undef DEFINE_OPCODE
    #undef CHECK_FOR_EXCEPTION
    #undef CHECK_FOR_TIMEOUT
}

JSValue Interpreter::retrieveArguments(CallFrame* callFrame, JSFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CodeBlock* codeBlock = functionCallFrame->codeBlock();
    if (codeBlock->usesArguments()) {
        ASSERT(codeBlock->codeType() == FunctionCode);
        SymbolTable& symbolTable = *codeBlock->symbolTable();
        int argumentsIndex = symbolTable.get(functionCallFrame->propertyNames().arguments.ustring().rep()).getIndex();
        if (!functionCallFrame->r(argumentsIndex).jsValue()) {
            Arguments* arguments = new (callFrame) Arguments(functionCallFrame);
            functionCallFrame->setCalleeArguments(arguments);
            functionCallFrame->r(RegisterFile::ArgumentsRegister) = JSValue(arguments);
        }
        return functionCallFrame->r(argumentsIndex).jsValue();
    }

    Arguments* arguments = functionCallFrame->optionalCalleeArguments();
    if (!arguments) {
        arguments = new (functionCallFrame) Arguments(functionCallFrame);
        arguments->copyRegisters();
        callFrame->setCalleeArguments(arguments);
    }

    return arguments;
}

JSValue Interpreter::retrieveCaller(CallFrame* callFrame, InternalFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CallFrame* callerFrame = functionCallFrame->callerFrame();
    if (callerFrame->hasHostCallFrameFlag())
        return jsNull();

    JSValue caller = callerFrame->callee();
    if (!caller)
        return jsNull();

    return caller;
}

void Interpreter::retrieveLastCaller(CallFrame* callFrame, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValue& function) const
{
    function = JSValue();
    lineNumber = -1;
    sourceURL = UString();

    CallFrame* callerFrame = callFrame->callerFrame();
    if (callerFrame->hasHostCallFrameFlag())
        return;

    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    if (!callerCodeBlock)
        return;

    unsigned bytecodeOffset = bytecodeOffsetForPC(callerFrame, callerCodeBlock, callFrame->returnPC());
    lineNumber = callerCodeBlock->lineNumberForBytecodeOffset(callerFrame, bytecodeOffset - 1);
    sourceID = callerCodeBlock->ownerExecutable()->sourceID();
    sourceURL = callerCodeBlock->ownerExecutable()->sourceURL();
    function = callerFrame->callee();
}

CallFrame* Interpreter::findFunctionCallFrame(CallFrame* callFrame, InternalFunction* function)
{
    for (CallFrame* candidate = callFrame; candidate; candidate = candidate->callerFrame()->removeHostCallFrameFlag()) {
        if (candidate->callee() == function)
            return candidate;
    }
    return 0;
}

void Interpreter::enableSampler()
{
#if ENABLE(OPCODE_SAMPLING)
    if (!m_sampler) {
        m_sampler.set(new SamplingTool(this));
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
