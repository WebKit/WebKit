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
#include "CodeBlock.h"
#include "DebuggerCallFrame.h"
#include "EvalCodeCache.h"
#include "ExceptionHelpers.h"
#include "CallFrame.h"
#include "GlobalEvalFunction.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSByteArray.h"
#include "JSFunction.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSStaticScopeObject.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "Profiler.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "Collector.h"
#include "Debugger.h"
#include "Operations.h"
#include "SamplingTool.h"
#include <stdio.h>

#if ENABLE(JIT)
#include "JIT.h"
#endif

#if ENABLE(ASSEMBLER)
#include "AssemblerBuffer.h"
#endif

#if PLATFORM(DARWIN)
#include <mach/mach.h>
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QDateTime>
#endif

using namespace std;

namespace JSC {

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

static ALWAYS_INLINE unsigned bytecodeOffsetForPC(CallFrame* callFrame, CodeBlock* codeBlock, void* pc)
{
#if ENABLE(JIT)
    return codeBlock->getBytecodeIndex(callFrame, pc);
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

static inline bool jsLess(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    if (JSValuePtr::areBothInt32Fast(v1, v2))
        return v1.getInt32Fast() < v2.getInt32Fast();

    double n1;
    double n2;
    if (v1.getNumber(n1) && v2.getNumber(n2))
        return n1 < n2;

    Interpreter* interpreter = callFrame->interpreter();
    if (interpreter->isJSString(v1) && interpreter->isJSString(v2))
        return asString(v1)->value() < asString(v2)->value();

    JSValuePtr p1;
    JSValuePtr p2;
    bool wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
    bool wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return asString(p1)->value() < asString(p2)->value();
}

static inline bool jsLessEq(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    if (JSValuePtr::areBothInt32Fast(v1, v2))
        return v1.getInt32Fast() <= v2.getInt32Fast();

    double n1;
    double n2;
    if (v1.getNumber(n1) && v2.getNumber(n2))
        return n1 <= n2;

    Interpreter* interpreter = callFrame->interpreter();
    if (interpreter->isJSString(v1) && interpreter->isJSString(v2))
        return !(asString(v2)->value() < asString(v1)->value());

    JSValuePtr p1;
    JSValuePtr p2;
    bool wasNotString1 = v1.getPrimitiveNumber(callFrame, n1, p1);
    bool wasNotString2 = v2.getPrimitiveNumber(callFrame, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(asString(p2)->value() < asString(p1)->value());
}

static NEVER_INLINE JSValuePtr jsAddSlowCase(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    // exception for the Date exception in defaultValue()
    JSValuePtr p1 = v1.toPrimitive(callFrame);
    JSValuePtr p2 = v2.toPrimitive(callFrame);

    if (p1.isString() || p2.isString()) {
        RefPtr<UString::Rep> value = concatenate(p1.toString(callFrame).rep(), p2.toString(callFrame).rep());
        if (!value)
            return throwOutOfMemoryError(callFrame);
        return jsString(callFrame, value.release());
    }

    return jsNumber(callFrame, p1.toNumber(callFrame) + p2.toNumber(callFrame));
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5626160 Add case: 3 3 (of these, 3637690 are for immediate values)
//    247412  Add case: 5 5
//    20900   Add case: 5 6
//    13962   Add case: 5 3
//    4000    Add case: 3 5

static ALWAYS_INLINE JSValuePtr jsAdd(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    double left;
    double right = 0.0;

    bool rightIsNumber = v2.getNumber(right);
    if (rightIsNumber && v1.getNumber(left))
        return jsNumber(callFrame, left + right);
    
    bool leftIsString = v1.isString();
    if (leftIsString && v2.isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (!value)
            return throwOutOfMemoryError(callFrame);
        return jsString(callFrame, value.release());
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = v2.isInt32Fast() ?
            concatenate(asString(v1)->value().rep(), v2.getInt32Fast()) :
            concatenate(asString(v1)->value().rep(), right);

        if (!value)
            return throwOutOfMemoryError(callFrame);
        return jsString(callFrame, value.release());
    }

    // All other cases are pretty uncommon
    return jsAddSlowCase(callFrame, v1, v2);
}

static JSValuePtr jsTypeStringForValue(CallFrame* callFrame, JSValuePtr v)
{
    if (v.isUndefined())
        return jsNontrivialString(callFrame, "undefined");
    if (v.isBoolean())
        return jsNontrivialString(callFrame, "boolean");
    if (v.isNumber())
        return jsNontrivialString(callFrame, "number");
    if (v.isString())
        return jsNontrivialString(callFrame, "string");
    if (v.isObject()) {
        // Return "undefined" for objects that should be treated
        // as null when doing comparisons.
        if (asObject(v)->structure()->typeInfo().masqueradesAsUndefined())
            return jsNontrivialString(callFrame, "undefined");
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return jsNontrivialString(callFrame, "function");
    }
    return jsNontrivialString(callFrame, "object");
}

static bool jsIsObjectType(JSValuePtr v)
{
    if (!v.isCell())
        return v.isNull();

    JSType type = asCell(v)->structure()->typeInfo().type();
    if (type == NumberType || type == StringType)
        return false;
    if (type == ObjectType) {
        if (asObject(v)->structure()->typeInfo().masqueradesAsUndefined())
            return false;
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return false;
    }
    return true;
}

static bool jsIsFunctionType(JSValuePtr v)
{
    if (v.isObject()) {
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return true;
    }
    return false;
}

NEVER_INLINE bool Interpreter::resolve(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

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
            JSValuePtr result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame[dst] = JSValuePtr(result);
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveSkip(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
{
    CodeBlock* codeBlock = callFrame->codeBlock();

    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    int skip = (vPC + 3)->u.operand + codeBlock->needsFullScopeChain();

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
            JSValuePtr result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame[dst] = JSValuePtr(result);
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveGlobal(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>((vPC + 2)->u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int property = (vPC + 3)->u.operand;
    Structure* structure = (vPC + 4)->u.structure;
    int offset = (vPC + 5)->u.operand;

    if (structure == globalObject->structure()) {
        callFrame[dst] = JSValuePtr(globalObject->getDirectOffset(offset));
        return true;
    }

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifier(property);
    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValuePtr result = slot.getValue(callFrame, ident);
        if (slot.isCacheable() && !globalObject->structure()->isDictionary()) {
            if (vPC[4].u.structure)
                vPC[4].u.structure->deref();
            globalObject->structure()->ref();
            vPC[4] = globalObject->structure();
            vPC[5] = slot.cachedOffset();
            callFrame[dst] = JSValuePtr(result);
            return true;
        }

        exceptionValue = callFrame->globalData().exception;
        if (exceptionValue)
            return false;
        callFrame[dst] = JSValuePtr(result);
        return true;
    }

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

static ALWAYS_INLINE JSValuePtr inlineResolveBase(CallFrame* callFrame, Identifier& property, ScopeChainNode* scopeChain)
{
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator next = iter;
    ++next;
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    JSObject* base;
    while (true) {
        base = *iter;
        if (next == end || base->getPropertySlot(callFrame, property, slot))
            return base;

        iter = next;
        ++next;
    }

    ASSERT_NOT_REACHED();
    return noValue();
}

NEVER_INLINE void Interpreter::resolveBase(CallFrame* callFrame, Instruction* vPC)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    callFrame[dst] = JSValuePtr(inlineResolveBase(callFrame, callFrame->codeBlock()->identifier(property), callFrame->scopeChain()));
}

NEVER_INLINE bool Interpreter::resolveBaseAndProperty(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int propDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

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
            JSValuePtr result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame[propDst] = JSValuePtr(result);
            callFrame[baseDst] = JSValuePtr(base);
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveBaseAndFunc(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int funcDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

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
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(callFrame);
            JSValuePtr result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;

            callFrame[baseDst] = JSValuePtr(thisObj);
            callFrame[funcDst] = JSValuePtr(result);
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC - codeBlock->instructions().begin(), codeBlock);
    return false;
}

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

static NEVER_INLINE bool isNotObject(CallFrame* callFrame, bool forInstanceOf, CodeBlock* codeBlock, const Instruction* vPC, JSValuePtr value, JSValuePtr& exceptionData)
{
    if (value.isObject())
        return false;
    exceptionData = createInvalidParamError(callFrame, forInstanceOf ? "instanceof" : "in" , value, vPC - codeBlock->instructions().begin(), codeBlock);
    return true;
}

NEVER_INLINE JSValuePtr Interpreter::callEval(CallFrame* callFrame, RegisterFile* registerFile, Register* argv, int argc, int registerOffset, JSValuePtr& exceptionValue)
{
    if (argc < 2)
        return jsUndefined();

    JSValuePtr program = argv[1].jsValue(callFrame);

    if (!program.isString())
        return program;

    UString programSource = asString(program)->value();

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    CodeBlock* codeBlock = callFrame->codeBlock();
    RefPtr<EvalNode> evalNode = codeBlock->evalCodeCache().get(callFrame, programSource, scopeChain, exceptionValue);

    JSValuePtr result = jsUndefined();
    if (evalNode)
        result = callFrame->globalData().interpreter->execute(evalNode.get(), callFrame, callFrame->thisValue().toThisObject(callFrame), callFrame->registers() - registerFile->start() + registerOffset, scopeChain, &exceptionValue);

    return result;
}

Interpreter::Interpreter()
    : m_sampler(0)
#if ENABLE(JIT)
    , m_ctiArrayLengthTrampoline(0)
    , m_ctiStringLengthTrampoline(0)
    , m_ctiVirtualCallPreLink(0)
    , m_ctiVirtualCallLink(0)
    , m_ctiVirtualCall(0)
#endif
    , m_reentryDepth(0)
    , m_timeoutTime(0)
    , m_timeAtLastCheckTimeout(0)
    , m_timeExecuting(0)
    , m_timeoutCheckCount(0)
    , m_ticksUntilNextTimeoutCheck(initialTickCountThreshold)
{
    initTimeout();
    privateExecute(InitializeAndReturn, 0, 0, 0);
    
    // Bizarrely, calling fastMalloc here is faster than allocating space on the stack.
    void* storage = fastMalloc(sizeof(CollectorBlock));

    JSCell* jsArray = new (storage) JSArray(JSArray::createStructure(jsNull()));
    m_jsArrayVptr = jsArray->vptr();
    jsArray->~JSCell();

    JSCell* jsByteArray = new (storage) JSByteArray(JSByteArray::VPtrStealingHack);
    m_jsByteArrayVptr = jsByteArray->vptr();
    jsByteArray->~JSCell();

    JSCell* jsString = new (storage) JSString(JSString::VPtrStealingHack);
    m_jsStringVptr = jsString->vptr();
    jsString->~JSCell();

    JSCell* jsFunction = new (storage) JSFunction(JSFunction::createStructure(jsNull()));
    m_jsFunctionVptr = jsFunction->vptr();
    jsFunction->~JSCell();
    
    fastFree(storage);
}

void Interpreter::initialize(JSGlobalData* globalData)
{
#if ENABLE(JIT)
    JIT::compileCTIMachineTrampolines(globalData);
#else
    UNUSED_PARAM(globalData);
#endif
}

Interpreter::~Interpreter()
{
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
    printf("----------------------------------------------------\n");
    printf("            use            |   address  |   value   \n");
    printf("----------------------------------------------------\n");

    CodeBlock* codeBlock = callFrame->codeBlock();
    RegisterFile* registerFile = &callFrame->scopeChain()->globalObject()->globalData()->interpreter->registerFile();
    const Register* it;
    const Register* end;

    if (codeBlock->codeType() == GlobalCode) {
        it = registerFile->lastGlobal();
        end = it + registerFile->numGlobals();
        while (it != end) {
            printf("[global var]               | %10p | %10p \n", it, (*it).v());
            ++it;
        }
        printf("----------------------------------------------------\n");
    }
    
    it = callFrame->registers() - RegisterFile::CallFrameHeaderSize - codeBlock->m_numParameters;
    printf("[this]                     | %10p | %10p \n", it, (*it).v()); ++it;
    end = it + max(codeBlock->m_numParameters - 1, 0); // - 1 to skip "this"
    if (it != end) {
        do {
            printf("[param]                    | %10p | %10p \n", it, (*it).v());
            ++it;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    printf("[CodeBlock]                | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ScopeChain]               | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[CallerRegisters]          | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ReturnPC]                 | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ReturnValueRegister]      | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ArgumentCount]            | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[Callee]                   | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[OptionalCalleeArguments]  | %10p | %10p \n", it, (*it).v()); ++it;
    printf("----------------------------------------------------\n");

    int registerCount = 0;

    end = it + codeBlock->m_numVars;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    end = it + codeBlock->m_numConstants;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    end = it + codeBlock->m_numCalleeRegisters - codeBlock->m_numConstants - codeBlock->m_numVars;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");
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

NEVER_INLINE bool Interpreter::unwindCallFrame(CallFrame*& callFrame, JSValuePtr exceptionValue, unsigned& bytecodeOffset, CodeBlock*& codeBlock)
{
    CodeBlock* oldCodeBlock = codeBlock;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        if (callFrame->callee())
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerNode()->sourceID(), codeBlock->ownerNode()->lastLine());
        else
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerNode()->sourceID(), codeBlock->ownerNode()->lastLine());
    }

    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
        if (callFrame->callee())
            profiler->didExecute(callFrame, callFrame->callee());
        else
            profiler->didExecute(callFrame, codeBlock->ownerNode()->sourceURL(), codeBlock->ownerNode()->lineNo());
    }

    // If this call frame created an activation or an 'arguments' object, tear it off.
    if (oldCodeBlock->codeType() == FunctionCode && oldCodeBlock->needsFullScopeChain()) {
        while (!scopeChain->object->isObject(&JSActivation::info))
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

NEVER_INLINE HandlerInfo* Interpreter::throwException(CallFrame*& callFrame, JSValuePtr& exceptionValue, unsigned bytecodeOffset, bool explicitThrow)
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
                exception->putWithAttributes(callFrame, Identifier(callFrame, "sourceId"), jsNumber(callFrame, codeBlock->ownerNode()->sourceID()), ReadOnly | DontDelete);
                exception->putWithAttributes(callFrame, Identifier(callFrame, "sourceURL"), jsOwnedString(callFrame, codeBlock->ownerNode()->sourceURL()), ReadOnly | DontDelete);
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
        debugger->exception(debuggerCallFrame, codeBlock->ownerNode()->sourceID(), codeBlock->lineNumberForBytecodeOffset(callFrame, bytecodeOffset));
    }

    // If we throw in the middle of a call instruction, we need to notify
    // the profiler manually that the call instruction has returned, since
    // we'll never reach the relevant op_profile_did_call.
    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
#if !ENABLE(JIT)
        if (isCallBytecode(codeBlock->instructions()[bytecodeOffset].u.opcode))
            profiler->didExecute(callFrame, callFrame[codeBlock->instructions()[bytecodeOffset + 2].u.operand].jsValue(callFrame));
        else if (codeBlock->instructions()[bytecodeOffset + 8].u.opcode == getOpcode(op_construct))
            profiler->didExecute(callFrame, callFrame[codeBlock->instructions()[bytecodeOffset + 10].u.operand].jsValue(callFrame));
#else
        int functionRegisterIndex;
        if (codeBlock->functionRegisterForBytecodeOffset(bytecodeOffset, functionRegisterIndex))
            profiler->didExecute(callFrame, callFrame[functionRegisterIndex].jsValue(callFrame));
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

JSValuePtr Interpreter::execute(ProgramNode* programNode, CallFrame* callFrame, ScopeChainNode* scopeChain, JSObject* thisObj, JSValuePtr* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    CodeBlock* codeBlock = &programNode->bytecode(scopeChain);

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = oldEnd + codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize + codeBlock->m_numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, scopeChain->globalObject());

    JSGlobalObject* lastGlobalObject = m_registerFile.globalObject();
    JSGlobalObject* globalObject = callFrame->dynamicGlobalObject();
    globalObject->copyGlobalsTo(m_registerFile);

    CallFrame* newCallFrame = CallFrame::create(oldEnd + codeBlock->m_numParameters + RegisterFile::CallFrameHeaderSize);
    newCallFrame[codeBlock->thisRegister()] = JSValuePtr(thisObj);
    newCallFrame->init(codeBlock, 0, scopeChain, CallFrame::noCaller(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain())
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, programNode->sourceURL(), programNode->lineNo());

    JSValuePtr result;
    {
        SamplingTool::CallRecord callRecord(m_sampler);

        m_reentryDepth++;
#if ENABLE(JIT)
        if (!codeBlock->jitCode())
            JIT::compile(scopeChain->globalData, codeBlock);
        result = JIT::execute(codeBlock->jitCode(), &m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
        m_reentryDepth--;
    }

    if (*profiler)
        (*profiler)->didExecute(callFrame, programNode->sourceURL(), programNode->lineNo());

    if (m_reentryDepth && lastGlobalObject && globalObject != lastGlobalObject)
        lastGlobalObject->copyGlobalsTo(m_registerFile);

    m_registerFile.shrink(oldEnd);

    return result;
}

JSValuePtr Interpreter::execute(FunctionBodyNode* functionBodyNode, CallFrame* callFrame, JSFunction* function, JSObject* thisObj, const ArgList& args, ScopeChainNode* scopeChain, JSValuePtr* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    Register* oldEnd = m_registerFile.end();
    int argc = 1 + args.size(); // implicit "this" parameter

    if (!m_registerFile.grow(oldEnd + argc)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, callFrame->globalData().dynamicGlobalObject ? callFrame->globalData().dynamicGlobalObject : scopeChain->globalObject());

    CallFrame* newCallFrame = CallFrame::create(oldEnd);
    size_t dst = 0;
    newCallFrame[0] = JSValuePtr(thisObj);
    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it)
        newCallFrame[++dst] = *it;

    CodeBlock* codeBlock = &functionBodyNode->bytecode(scopeChain);
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
        (*profiler)->willExecute(newCallFrame, function);

    JSValuePtr result;
    {
        SamplingTool::CallRecord callRecord(m_sampler);

        m_reentryDepth++;
#if ENABLE(JIT)
        if (!codeBlock->jitCode())
            JIT::compile(scopeChain->globalData, codeBlock);
        result = JIT::execute(codeBlock->jitCode(), &m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
        m_reentryDepth--;
    }

    if (*profiler)
        (*profiler)->didExecute(newCallFrame, function);

    m_registerFile.shrink(oldEnd);
    return result;
}

JSValuePtr Interpreter::execute(EvalNode* evalNode, CallFrame* callFrame, JSObject* thisObj, ScopeChainNode* scopeChain, JSValuePtr* exception)
{
    return execute(evalNode, callFrame, thisObj, m_registerFile.size() + evalNode->bytecode(scopeChain).m_numParameters + RegisterFile::CallFrameHeaderSize, scopeChain, exception);
}

JSValuePtr Interpreter::execute(EvalNode* evalNode, CallFrame* callFrame, JSObject* thisObj, int globalRegisterOffset, ScopeChainNode* scopeChain, JSValuePtr* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, callFrame->globalData().dynamicGlobalObject ? callFrame->globalData().dynamicGlobalObject : scopeChain->globalObject());

    EvalCodeBlock* codeBlock = &evalNode->bytecode(scopeChain);

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

        const DeclarationStacks::VarStack& varStack = codeBlock->ownerNode()->varStack();
        DeclarationStacks::VarStack::const_iterator varStackEnd = varStack.end();
        for (DeclarationStacks::VarStack::const_iterator it = varStack.begin(); it != varStackEnd; ++it) {
            const Identifier& ident = (*it).first;
            if (!variableObject->hasProperty(callFrame, ident)) {
                PutPropertySlot slot;
                variableObject->put(callFrame, ident, jsUndefined(), slot);
            }
        }

        const DeclarationStacks::FunctionStack& functionStack = codeBlock->ownerNode()->functionStack();
        DeclarationStacks::FunctionStack::const_iterator functionStackEnd = functionStack.end();
        for (DeclarationStacks::FunctionStack::const_iterator it = functionStack.begin(); it != functionStackEnd; ++it) {
            PutPropertySlot slot;
            variableObject->put(callFrame, (*it)->m_ident, (*it)->makeFunction(callFrame, scopeChain), slot);
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
    newCallFrame[codeBlock->thisRegister()] = JSValuePtr(thisObj);
    newCallFrame->init(codeBlock, 0, scopeChain, callFrame->addHostCallFrameFlag(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain())
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, evalNode->sourceURL(), evalNode->lineNo());

    JSValuePtr result;
    {
        SamplingTool::CallRecord callRecord(m_sampler);

        m_reentryDepth++;
#if ENABLE(JIT)
        if (!codeBlock->jitCode())
            JIT::compile(scopeChain->globalData, codeBlock);
        result = JIT::execute(codeBlock->jitCode(), &m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
        result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
        m_reentryDepth--;
    }

    if (*profiler)
        (*profiler)->didExecute(callFrame, evalNode->sourceURL(), evalNode->lineNo());

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
            debugger->callEvent(callFrame, callFrame->codeBlock()->ownerNode()->sourceID(), firstLine);
            return;
        case WillLeaveCallFrame:
            debugger->returnEvent(callFrame, callFrame->codeBlock()->ownerNode()->sourceID(), lastLine);
            return;
        case WillExecuteStatement:
            debugger->atStatement(callFrame, callFrame->codeBlock()->ownerNode()->sourceID(), firstLine);
            return;
        case WillExecuteProgram:
            debugger->willExecuteProgram(callFrame, callFrame->codeBlock()->ownerNode()->sourceID(), firstLine);
            return;
        case DidExecuteProgram:
            debugger->didExecuteProgram(callFrame, callFrame->codeBlock()->ownerNode()->sourceID(), lastLine);
            return;
        case DidReachBreakpoint:
            debugger->didReachBreakpoint(callFrame, callFrame->codeBlock()->ownerNode()->sourceID(), lastLine);
            return;
    }
}

void Interpreter::resetTimeoutCheck()
{
    m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    m_timeAtLastCheckTimeout = 0;
    m_timeExecuting = 0;
}

// Returns the time the current thread has spent executing, in milliseconds.
static inline unsigned getCPUTime()
{
#if PLATFORM(DARWIN)
    mach_msg_type_number_t infoCount = THREAD_BASIC_INFO_COUNT;
    thread_basic_info_data_t info;

    // Get thread information
    mach_port_t threadPort = mach_thread_self();
    thread_info(threadPort, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &infoCount);
    mach_port_deallocate(mach_task_self(), threadPort);
    
    unsigned time = info.user_time.seconds * 1000 + info.user_time.microseconds / 1000;
    time += info.system_time.seconds * 1000 + info.system_time.microseconds / 1000;
    
    return time;
#elif HAVE(SYS_TIME_H)
    // FIXME: This should probably use getrusage with the RUSAGE_THREAD flag.
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    return t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    union {
        FILETIME fileTime;
        unsigned long long fileTimeAsLong;
    } userTime, kernelTime;
    
    // GetThreadTimes won't accept NULL arguments so we pass these even though
    // they're not used.
    FILETIME creationTime, exitTime;
    
    GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime.fileTime, &userTime.fileTime);
    
    return userTime.fileTimeAsLong / 10000 + kernelTime.fileTimeAsLong / 10000;
#else
#error Platform does not have getCurrentTime function
#endif
}

// We have to return a JSValue here, gcc seems to produce worse code if 
// we attempt to return a bool
ALWAYS_INLINE bool Interpreter::checkTimeout(JSGlobalObject* globalObject)
{
    unsigned currentTime = getCPUTime();
    
    if (!m_timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        m_timeAtLastCheckTimeout = currentTime;
        return false;
    }
    
    unsigned timeDiff = currentTime - m_timeAtLastCheckTimeout;
    
    if (timeDiff == 0)
        timeDiff = 1;
    
    m_timeExecuting += timeDiff;
    m_timeAtLastCheckTimeout = currentTime;
    
    // Adjust the tick threshold so we get the next checkTimeout call in the interval specified in 
    // preferredScriptCheckTimeInterval
    m_ticksUntilNextTimeoutCheck = static_cast<unsigned>((static_cast<float>(preferredScriptCheckTimeInterval) / timeDiff) * m_ticksUntilNextTimeoutCheck);
    // If the new threshold is 0 reset it to the default threshold. This can happen if the timeDiff is higher than the
    // preferred script check time interval.
    if (m_ticksUntilNextTimeoutCheck == 0)
        m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    
    if (m_timeoutTime && m_timeExecuting > m_timeoutTime) {
        if (globalObject->shouldInterruptScript())
            return true;
        
        resetTimeoutCheck();
    }
    
    return false;
}

NEVER_INLINE ScopeChainNode* Interpreter::createExceptionScope(CallFrame* callFrame, const Instruction* vPC)
{
    int dst = (++vPC)->u.operand;
    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& property = codeBlock->identifier((++vPC)->u.operand);
    JSValuePtr value = callFrame[(++vPC)->u.operand].jsValue(callFrame);
    JSObject* scope = new (callFrame) JSStaticScopeObject(callFrame, property, value, DontDelete);
    callFrame[dst] = JSValuePtr(scope);

    return callFrame->scopeChain()->push(scope);
}

static StructureChain* cachePrototypeChain(CallFrame* callFrame, Structure* structure)
{
    JSValuePtr prototype = structure->prototypeForLookup(callFrame);
    if (!prototype.isCell())
        return 0;
    RefPtr<StructureChain> chain = StructureChain::create(asObject(prototype)->structure());
    structure->setCachedPrototypeChain(chain.release());
    return structure->cachedPrototypeChain();
}

NEVER_INLINE void Interpreter::tryCachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValuePtr baseValue, const PutPropertySlot& slot)
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

    if (structure->isDictionary()) {
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
        vPC[0] = getOpcode(op_put_by_id_transition);
        vPC[4] = structure->previousID();
        vPC[5] = structure;
        StructureChain* chain = structure->cachedPrototypeChain();
        if (!chain) {
            chain = cachePrototypeChain(callFrame, structure);
            if (!chain) {
                // This happens if someone has manually inserted null into the prototype chain
                vPC[0] = getOpcode(op_put_by_id_generic);
                return;
            }
        }
        vPC[6] = chain;
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

static size_t countPrototypeChainEntriesAndCheckForProxies(CallFrame* callFrame, JSValuePtr baseValue, const PropertySlot& slot)
{
    JSCell* cell = asCell(baseValue);
    size_t count = 0;

    while (slot.slotBase() != cell) {
        JSValuePtr v = cell->structure()->prototypeForLookup(callFrame);

        // If we didn't find slotBase in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.

        if (v.isNull())
            return 0;

        cell = asCell(v);

        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (cell->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(cell->structure());
            asObject(cell)->setStructure(transition.release());
            cell->structure()->setCachedPrototypeChain(0);
        }

        ++count;
    }
    
    ASSERT(count);
    return count;
}

NEVER_INLINE void Interpreter::tryCacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValuePtr baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_get_by_id))
        return;

    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    if (isJSArray(baseValue) && propertyName == callFrame->propertyNames().length) {
        vPC[0] = getOpcode(op_get_array_length);
        return;
    }

    if (isJSString(baseValue) && propertyName == callFrame->propertyNames().length) {
        vPC[0] = getOpcode(op_get_string_length);
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    Structure* structure = asCell(baseValue)->structure();

    if (structure->isDictionary()) {
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

    if (slot.slotBase() == structure->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase().isObject());

        JSObject* baseObject = asObject(slot.slotBase());

        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (baseObject->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(baseObject->structure());
            baseObject->setStructure(transition.release());
            asCell(baseValue)->structure()->setCachedPrototypeChain(0);
        }

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[5] = baseObject->structure();
        vPC[6] = slot.cachedOffset();

        codeBlock->refStructures(vPC);
        return;
    }

    size_t count = countPrototypeChainEntriesAndCheckForProxies(callFrame, baseValue, slot);
    if (!count) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    StructureChain* chain = structure->cachedPrototypeChain();
    if (!chain)
        chain = cachePrototypeChain(callFrame, structure);
    ASSERT(chain);

    vPC[0] = getOpcode(op_get_by_id_chain);
    vPC[4] = structure;
    vPC[5] = chain;
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

JSValuePtr Interpreter::privateExecute(ExecutionFlag flag, RegisterFile* registerFile, CallFrame* callFrame, JSValuePtr* exception)
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
        return noValue();
    }

#if ENABLE(JIT)
    // Currently with CTI enabled we never interpret functions
    ASSERT_NOT_REACHED();
#endif

    JSGlobalData* globalData = &callFrame->globalData();
    JSValuePtr exceptionValue = noValue();
    HandlerInfo* handler = 0;

    Instruction* vPC = callFrame->codeBlock()->instructions().begin();
    Profiler** enabledProfilerReference = Profiler::enabledProfilerReference();
    unsigned tickCount = m_ticksUntilNextTimeoutCheck + 1;

#define CHECK_FOR_EXCEPTION() \
    do { \
        if (UNLIKELY(globalData->exception != noValue())) { \
            exceptionValue = globalData->exception; \
            goto vm_throw; \
        } \
    } while (0)

#if ENABLE(OPCODE_STATS)
    OpcodeStats::resetLastInstruction();
#endif

#define CHECK_FOR_TIMEOUT() \
    if (!--tickCount) { \
        if (checkTimeout(callFrame->dynamicGlobalObject())) { \
            exceptionValue = jsNull(); \
            goto vm_throw; \
        } \
        tickCount = m_ticksUntilNextTimeoutCheck; \
    }
    
#if ENABLE(OPCODE_SAMPLING)
    #define SAMPLE(codeBlock, vPC) m_sampler->sample(codeBlock, vPC)
    #define CTI_SAMPLER ARG_globalData->interpreter->sampler()
#else
    #define SAMPLE(codeBlock, vPC)
    #define CTI_SAMPLER 0
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
        int dst = (++vPC)->u.operand;
        callFrame[dst] = JSValuePtr(constructEmptyObject(callFrame));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_array) {
        /* new_array dst(r) firstArg(r) argCount(n)

           Constructs a new Array instance using the original
           constructor, and puts the result in register dst.
           The array will contain argCount elements with values
           taken from registers starting at register firstArg.
        */
        int dst = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;
        ArgList args(callFrame->registers() + firstArg, argCount);
        callFrame[dst] = JSValuePtr(constructArray(callFrame, args));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int regExp = (++vPC)->u.operand;
        callFrame[dst] = JSValuePtr(new (globalData) RegExpObject(callFrame->scopeChain()->globalObject()->regExpStructure(), callFrame->codeBlock()->regexp(regExp)));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = callFrame[src];

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSFastMath::canDoFastBitwiseOperations(src1, src2))
            callFrame[dst] = JSFastMath::equal(src1, src2);
        else {
            JSValuePtr result = jsBoolean(JSValuePtr::equalSlowCase(callFrame, src1, src2));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_eq_null) {
        /* eq_null dst(r) src(r)

           Checks whether register src is null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (src.isUndefinedOrNull()) {
            callFrame[dst] = jsBoolean(true);
            ++vPC;
            NEXT_INSTRUCTION();
        }
        
        callFrame[dst] = jsBoolean(src.isCell() && src.asCell()->structure()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSFastMath::canDoFastBitwiseOperations(src1, src2))
            callFrame[dst] = JSFastMath::notEqual(src1, src2);
        else {
            JSValuePtr result = jsBoolean(!JSValuePtr::equalSlowCase(callFrame, src1, src2));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_neq_null) {
        /* neq_null dst(r) src(r)

           Checks whether register src is not null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (src.isUndefinedOrNull()) {
            callFrame[dst] = jsBoolean(false);
            ++vPC;
            NEXT_INSTRUCTION();
        }
        
        callFrame[dst] = jsBoolean(!src.isCell() || !asCell(src)->structure()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        callFrame[dst] = jsBoolean(JSValuePtr::strictEqual(src1, src2));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        callFrame[dst] = jsBoolean(!JSValuePtr::strictEqual(src1, src2));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr result = jsBoolean(jsLess(callFrame, src1, src2));
        CHECK_FOR_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr result = jsBoolean(jsLessEq(callFrame, src1, src2));
        CHECK_FOR_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSFastMath::canDoFastAdditiveOperations(v))
            callFrame[srcDst] = JSValuePtr(JSFastMath::incImmediateNumber(v));
        else {
            JSValuePtr result = jsNumber(callFrame, v.toNumber(callFrame) + 1);
            CHECK_FOR_EXCEPTION();
            callFrame[srcDst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSFastMath::canDoFastAdditiveOperations(v))
            callFrame[srcDst] = JSValuePtr(JSFastMath::decImmediateNumber(v));
        else {
            JSValuePtr result = jsNumber(callFrame, v.toNumber(callFrame) - 1);
            CHECK_FOR_EXCEPTION();
            callFrame[srcDst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSFastMath::canDoFastAdditiveOperations(v)) {
            callFrame[dst] = v;
            callFrame[srcDst] = JSValuePtr(JSFastMath::incImmediateNumber(v));
        } else {
            JSValuePtr number = callFrame[srcDst].jsValue(callFrame).toJSNumber(callFrame);
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = number;
            callFrame[srcDst] = JSValuePtr(jsNumber(callFrame, number.uncheckedGetNumber() + 1));
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSFastMath::canDoFastAdditiveOperations(v)) {
            callFrame[dst] = v;
            callFrame[srcDst] = JSValuePtr(JSFastMath::decImmediateNumber(v));
        } else {
            JSValuePtr number = callFrame[srcDst].jsValue(callFrame).toJSNumber(callFrame);
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = number;
            callFrame[srcDst] = JSValuePtr(jsNumber(callFrame, number.uncheckedGetNumber() - 1));
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;

        JSValuePtr srcVal = callFrame[src].jsValue(callFrame);

        if (LIKELY(srcVal.isNumber()))
            callFrame[dst] = callFrame[src];
        else {
            JSValuePtr result = srcVal.toJSNumber(callFrame);
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        ++vPC;
        double v;
        if (src.getNumber(v))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, -v));
        else {
            JSValuePtr result = jsNumber(callFrame, -src.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSFastMath::canDoFastAdditiveOperations(src1, src2))
            callFrame[dst] = JSValuePtr(JSFastMath::addImmediateNumbers(src1, src2));
        else {
            JSValuePtr result = jsAdd(callFrame, src1, src2);
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }
        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double left;
        double right;
        if (JSValuePtr::areBothInt32Fast(src1, src2)) {
            int32_t left = src1.getInt32Fast();
            int32_t right = src2.getInt32Fast();
            if ((left | right) >> 15 == 0)
                callFrame[dst] = JSValuePtr(jsNumber(callFrame, left * right));
            else
                callFrame[dst] = JSValuePtr(jsNumber(callFrame, static_cast<double>(left) * static_cast<double>(right)));
        } else if (src1.getNumber(left) && src2.getNumber(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left * right));
        else {
            JSValuePtr result = jsNumber(callFrame, src1.toNumber(callFrame) * src2.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_div) {
        /* div dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by the
           register divisor (converted to number), and puts the
           quotient in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr dividend = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr divisor = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double left;
        double right;
        if (dividend.getNumber(left) && divisor.getNumber(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left / right));
        else {
            JSValuePtr result = jsNumber(callFrame, dividend.toNumber(callFrame) / divisor.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_mod) {
        /* mod dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by
           register divisor (converted to number), and puts the
           remainder in register dst.
        */
        int dst = (++vPC)->u.operand;
        int dividend = (++vPC)->u.operand;
        int divisor = (++vPC)->u.operand;

        JSValuePtr dividendValue = callFrame[dividend].jsValue(callFrame);
        JSValuePtr divisorValue = callFrame[divisor].jsValue(callFrame);

        if (JSValuePtr::areBothInt32Fast(dividendValue, divisorValue) && divisorValue != js0()) {
            // We expect the result of the modulus of a number that was representable as an int32 to also be representable
            // as an int32.
            JSValuePtr result = JSValuePtr::makeInt32Fast(dividendValue.getInt32Fast() % divisorValue.getInt32Fast());
            ASSERT(result);
            callFrame[dst] = result;
            ++vPC;
            NEXT_INSTRUCTION();
        }

        double d = dividendValue.toNumber(callFrame);
        JSValuePtr result = jsNumber(callFrame, fmod(d, divisorValue.toNumber(callFrame)));
        CHECK_FOR_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_sub) {
        /* sub dst(r) src1(r) src2(r)

           Subtracts register src2 (converted to number) from register
           src1 (converted to number), and puts the difference in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double left;
        double right;
        if (JSFastMath::canDoFastAdditiveOperations(src1, src2))
            callFrame[dst] = JSValuePtr(JSFastMath::subImmediateNumbers(src1, src2));
        else if (src1.getNumber(left) && src2.getNumber(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left - right));
        else {
            JSValuePtr result = jsNumber(callFrame, src1.toNumber(callFrame) - src2.toNumber(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }
        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_lshift) {
        /* lshift dst(r) val(r) shift(r)

           Performs left shift of register val (converted to int32) by
           register shift (converted to uint32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        uint32_t right;
        if (JSValuePtr::areBothInt32Fast(val, shift))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, val.getInt32Fast() << (shift.getInt32Fast() & 0x1f)));
        else if (val.numberToInt32(left) && shift.numberToUInt32(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left << (right & 0x1f)));
        else {
            JSValuePtr result = jsNumber(callFrame, (val.toInt32(callFrame)) << (shift.toUInt32(callFrame) & 0x1f));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_rshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs arithmetic right shift of register val (converted
           to int32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        uint32_t right;
        if (JSFastMath::canDoFastRshift(val, shift))
            callFrame[dst] = JSValuePtr(JSFastMath::rightShiftImmediateNumbers(val, shift));
        else if (val.numberToInt32(left) && shift.numberToUInt32(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left >> (right & 0x1f)));
        else {
            JSValuePtr result = jsNumber(callFrame, (val.toInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSFastMath::canDoFastUrshift(val, shift))
            callFrame[dst] = JSValuePtr(JSFastMath::rightShiftImmediateNumbers(val, shift));
        else {
            JSValuePtr result = jsNumber(callFrame, (val.toUInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitand) {
        /* bitand dst(r) src1(r) src2(r)

           Computes bitwise AND of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        int32_t right;
        if (JSFastMath::canDoFastBitwiseOperations(src1, src2))
            callFrame[dst] = JSValuePtr(JSFastMath::andImmediateNumbers(src1, src2));
        else if (src1.numberToInt32(left) && src2.numberToInt32(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left & right));
        else {
            JSValuePtr result = jsNumber(callFrame, src1.toInt32(callFrame) & src2.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitxor) {
        /* bitxor dst(r) src1(r) src2(r)

           Computes bitwise XOR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        int32_t right;
        if (JSFastMath::canDoFastBitwiseOperations(src1, src2))
            callFrame[dst] = JSValuePtr(JSFastMath::xorImmediateNumbers(src1, src2));
        else if (src1.numberToInt32(left) && src2.numberToInt32(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left ^ right));
        else {
            JSValuePtr result = jsNumber(callFrame, src1.toInt32(callFrame) ^ src2.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitor) {
        /* bitor dst(r) src1(r) src2(r)

           Computes bitwise OR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        int32_t right;
        if (JSFastMath::canDoFastBitwiseOperations(src1, src2))
            callFrame[dst] = JSValuePtr(JSFastMath::orImmediateNumbers(src1, src2));
        else if (src1.numberToInt32(left) && src2.numberToInt32(right))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, left | right));
        else {
            JSValuePtr result = jsNumber(callFrame, src1.toInt32(callFrame) | src2.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t value;
        if (src.numberToInt32(value))
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, ~value));
        else {
            JSValuePtr result = jsNumber(callFrame, ~src.toInt32(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = result;
        }
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_not) {
        /* not dst(r) src(r)

           Computes logical NOT of register src (converted to
           boolean), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValuePtr result = jsBoolean(!callFrame[src].jsValue(callFrame).toBoolean(callFrame));
        CHECK_FOR_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
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

        JSValuePtr baseVal = callFrame[base].jsValue(callFrame);

        if (isNotObject(callFrame, true, callFrame->codeBlock(), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = asObject(baseVal);
        callFrame[dst] = jsBoolean(baseObj->structure()->typeInfo().implementsHasInstance() ? baseObj->hasInstance(callFrame, callFrame[value].jsValue(callFrame), callFrame[baseProto].jsValue(callFrame)) : false);

        vPC += 5;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = JSValuePtr(jsTypeStringForValue(callFrame, callFrame[src].jsValue(callFrame)));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_undefined) {
        /* is_undefined dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "undefined", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValuePtr v = callFrame[src].jsValue(callFrame);
        callFrame[dst] = jsBoolean(v.isCell() ? v.asCell()->structure()->typeInfo().masqueradesAsUndefined() : v.isUndefined());

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_boolean) {
        /* is_boolean dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "boolean", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame).isBoolean());

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_number) {
        /* is_number dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "number", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame).isNumber());

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_string) {
        /* is_string dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "string", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame).isString());

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_object) {
        /* is_object dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "object", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(jsIsObjectType(callFrame[src].jsValue(callFrame)));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_is_function) {
        /* is_function dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "function", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(jsIsFunctionType(callFrame[src].jsValue(callFrame)));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_in) {
        /* in dst(r) property(r) base(r)

           Tests whether register base has a property named register
           property, and puts the boolean result in register dst.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        JSValuePtr baseVal = callFrame[base].jsValue(callFrame);
        if (isNotObject(callFrame, false, callFrame->codeBlock(), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = asObject(baseVal);

        JSValuePtr propName = callFrame[property].jsValue(callFrame);

        uint32_t i;
        if (propName.getUInt32(i))
            callFrame[dst] = jsBoolean(baseObj->hasProperty(callFrame, i));
        else {
            Identifier property(callFrame, propName.toString(callFrame));
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = jsBoolean(baseObj->hasProperty(callFrame, property));
        }

        ++vPC;
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

        vPC += 3;
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

        vPC += 4;

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
        
        vPC += 6;
        
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_global_var) {
        /* get_global_var dst(r) globalObject(c) index(n)

           Gets the global var at global slot index and places it in register dst.
         */
        int dst = (++vPC)->u.operand;
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;

        callFrame[dst] = scope->registerAt(index);
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_global_var) {
        /* put_global_var globalObject(c) index(n) value(r)
         
           Puts value into global slot index.
         */
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        
        scope->registerAt(index) = JSValuePtr(callFrame[value].jsValue(callFrame));
        ++vPC;
        NEXT_INSTRUCTION();
    }            
    DEFINE_OPCODE(op_get_scoped_var) {
        /* get_scoped_var dst(r) index(n) skip(n)

         Loads the contents of the index-th local from the scope skip nodes from
         the top of the scope chain, and places it in register dst
         */
        int dst = (++vPC)->u.operand;
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + callFrame->codeBlock()->needsFullScopeChain();

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
        callFrame[dst] = scope->registerAt(index);
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_put_scoped_var) {
        /* put_scoped_var index(n) skip(n) value(r)

         */
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + callFrame->codeBlock()->needsFullScopeChain();
        int value = (++vPC)->u.operand;

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
        scope->registerAt(index) = JSValuePtr(callFrame[value].jsValue(callFrame));
        ++vPC;
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

        vPC += 3;
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

        vPC += 4;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_resolve_func) {
        /* resolve_func baseDst(r) funcDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes the
           appropriate object to use as "this" when calling its
           properties to register baseDst; and the retrieved property
           value to register propDst. If the property is not found,
           raises an exception.

           This differs from resolve_with_base, because the
           global this value will be substituted for activations or
           the global object, which is the right behavior for function
           calls but not for other property lookup.
        */
        if (UNLIKELY(!resolveBaseAndFunc(callFrame, vPC, exceptionValue)))
            goto vm_throw;

        vPC += 4;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        PropertySlot slot(baseValue);
        JSValuePtr result = baseValue.get(callFrame, ident, slot);
        CHECK_FOR_EXCEPTION();

        tryCacheGetByID(callFrame, codeBlock, vPC, baseValue, ident, slot);

        callFrame[dst] = result;
        vPC += 8;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_self) {
        /* op_get_by_id_self dst(r) base(r) property(id) structure(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base. If the cache misses, op_get_by_id_self reverts to
           op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int dst = vPC[1].u.operand;
                int offset = vPC[5].u.operand;

                ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifier(vPC[3].u.operand)) == baseObject->getDirectOffset(offset));
                callFrame[dst] = JSValuePtr(baseObject->getDirectOffset(offset));

                vPC += 8;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

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
                    callFrame[dst] = JSValuePtr(protoObject->getDirectOffset(offset));

                    vPC += 8;
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
        vPC += 8;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_proto_list) {
        // Polymorphic prototype access caching currently only supported when JITting.
        ASSERT_NOT_REACHED();
        // This case of the switch must not be empty, else (op_get_by_id_proto_list == op_get_by_id_chain)!
        vPC += 8;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_id_chain) {
        /* op_get_by_id_chain dst(r) base(r) property(id) structure(sID) structureChain(chain) count(n) offset(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype chain. If the cache misses, op_get_by_id_chain
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

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
                        callFrame[dst] = JSValuePtr(baseObject->getDirectOffset(offset));

                        vPC += 8;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        PropertySlot slot(baseValue);
        JSValuePtr result = baseValue.get(callFrame, ident, slot);
        CHECK_FOR_EXCEPTION();

        callFrame[dst] = result;
        vPC += 8;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_array_length) {
        /* op_get_array_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the array in register base,
           and puts the result in register dst. If register base does not hold
           an array, op_get_array_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        if (LIKELY(isJSArray(baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, asArray(baseValue)->length()));
            vPC += 8;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        if (LIKELY(isJSString(baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame[dst] = JSValuePtr(jsNumber(callFrame, asString(baseValue)->value().size()));
            vPC += 8;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        Identifier& ident = codeBlock->identifier(property);
        PutPropertySlot slot;
        baseValue.put(callFrame, ident, callFrame[value].jsValue(callFrame), slot);
        CHECK_FOR_EXCEPTION();

        tryCachePutByID(callFrame, codeBlock, vPC, baseValue, slot);

        vPC += 8;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        
        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* oldStructure = vPC[4].u.structure;
            Structure* newStructure = vPC[5].u.structure;
            
            if (LIKELY(baseCell->structure() == oldStructure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);

                RefPtr<Structure>* it = vPC[6].u.structureChain->head();

                JSValuePtr proto = baseObject->structure()->prototypeForLookup(callFrame);
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
                baseObject->putDirectOffset(offset, callFrame[value].jsValue(callFrame));

                vPC += 8;
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
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(baseValue.isCell())) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int value = vPC[3].u.operand;
                unsigned offset = vPC[5].u.operand;
                
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifier(vPC[2].u.operand))) == offset);
                baseObject->putDirectOffset(offset, callFrame[value].jsValue(callFrame));

                vPC += 8;
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

        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        PutPropertySlot slot;
        baseValue.put(callFrame, ident, callFrame[value].jsValue(callFrame), slot);
        CHECK_FOR_EXCEPTION();

        vPC += 8;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_del_by_id) {
        /* del_by_id dst(r) base(r) property(id)

           Converts register base to Object, deletes the property
           named by identifier property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = callFrame[base].jsValue(callFrame).toObject(callFrame);
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        JSValuePtr result = jsBoolean(baseObj->deleteProperty(callFrame, ident));
        CHECK_FOR_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_by_val) {
        /* get_by_val dst(r) base(r) property(r)

           Converts register base to Object, gets the property named
           by register property from the object, and puts the result
           in register dst. property is nominally converted to string
           but numbers are treated more efficiently.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        JSValuePtr subscript = callFrame[property].jsValue(callFrame);

        JSValuePtr result;

        if (LIKELY(subscript.isUInt32Fast())) {
            uint32_t i = subscript.getUInt32Fast();
            if (isJSArray(baseValue)) {
                JSArray* jsArray = asArray(baseValue);
                if (jsArray->canGetIndex(i))
                    result = jsArray->getIndex(i);
                else
                    result = jsArray->JSArray::get(callFrame, i);
            } else if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
                result = asString(baseValue)->getIndex(&callFrame->globalData(), i);
            else if (isJSByteArray(baseValue) && asByteArray(baseValue)->canAccessIndex(i))
                result = asByteArray(baseValue)->getIndex(callFrame, i);
            else
                result = baseValue.get(callFrame, i);
        } else {
            Identifier property(callFrame, subscript.toString(callFrame));
            result = baseValue.get(callFrame, property);
        }

        CHECK_FOR_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
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
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        JSValuePtr subscript = callFrame[property].jsValue(callFrame);

        if (LIKELY(subscript.isUInt32Fast())) {
            uint32_t i = subscript.getUInt32Fast();
            if (isJSArray(baseValue)) {
                JSArray* jsArray = asArray(baseValue);
                if (jsArray->canSetIndex(i))
                    jsArray->setIndex(i, callFrame[value].jsValue(callFrame));
                else
                    jsArray->JSArray::put(callFrame, i, callFrame[value].jsValue(callFrame));
            } else if (isJSByteArray(baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
                JSByteArray* jsByteArray = asByteArray(baseValue);
                double dValue = 0;
                JSValuePtr jsValue = callFrame[value].jsValue(callFrame);
                if (jsValue.isInt32Fast())
                    jsByteArray->setIndex(i, jsValue.getInt32Fast());
                else if (jsValue.getNumber(dValue))
                    jsByteArray->setIndex(i, dValue);
                else
                    baseValue.put(callFrame, i, jsValue);
            } else
                baseValue.put(callFrame, i, callFrame[value].jsValue(callFrame));
        } else {
            Identifier property(callFrame, subscript.toString(callFrame));
            if (!globalData->exception) { // Don't put to an object if toString threw an exception.
                PutPropertySlot slot;
                baseValue.put(callFrame, property, callFrame[value].jsValue(callFrame), slot);
            }
        }

        CHECK_FOR_EXCEPTION();
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_del_by_val) {
        /* del_by_val dst(r) base(r) property(r)

           Converts register base to Object, deletes the property
           named by register property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = callFrame[base].jsValue(callFrame).toObject(callFrame); // may throw

        JSValuePtr subscript = callFrame[property].jsValue(callFrame);
        JSValuePtr result;
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
        callFrame[dst] = result;
        ++vPC;
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
        int base = (++vPC)->u.operand;
        unsigned property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        callFrame[base].jsValue(callFrame).put(callFrame, property, callFrame[value].jsValue(callFrame));

        ++vPC;
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
        int target = (++vPC)->u.operand;
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
        int target = (++vPC)->u.operand;

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
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (callFrame[cond].jsValue(callFrame).toBoolean(callFrame)) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION();
        }
        
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (callFrame[cond].jsValue(callFrame).toBoolean(callFrame)) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (!callFrame[cond].jsValue(callFrame).toBoolean(callFrame)) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jeq_null) {
        /* jeq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is null.
        */
        int src = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        JSValuePtr srcValue = callFrame[src].jsValue(callFrame);

        if (srcValue.isUndefinedOrNull() || (srcValue.isCell() && srcValue.asCell()->structure()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jneq_null) {
        /* jneq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is not null.
        */
        int src = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        JSValuePtr srcValue = callFrame[src].jsValue(callFrame);

        if (!srcValue.isUndefinedOrNull() || (srcValue.isCell() && !srcValue.asCell()->structure()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        ++vPC;
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
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int target = (++vPC)->u.operand;
        
        bool result = jsLess(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION();
        }
        
        ++vPC;
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
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int target = (++vPC)->u.operand;
        
        bool result = jsLessEq(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION();
        }
        
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jnless) {
        /* jnless src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is false.
        */
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int target = (++vPC)->u.operand;

        bool result = jsLess(callFrame, src1, src2);
        CHECK_FOR_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_INSTRUCTION();
        }

        ++vPC;
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
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValuePtr scrutinee = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (scrutinee.isInt32Fast())
            vPC += callFrame->codeBlock()->immediateSwitchJumpTable(tableIndex).offsetForValue(scrutinee.getInt32Fast(), defaultOffset);
        else
            vPC += defaultOffset;
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
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValuePtr scrutinee = callFrame[(++vPC)->u.operand].jsValue(callFrame);
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
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValuePtr scrutinee = callFrame[(++vPC)->u.operand].jsValue(callFrame);
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
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        callFrame[dst] = callFrame->codeBlock()->function(func)->makeFunction(callFrame, callFrame->scopeChain());

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_func_exp) {
        /* new_func_exp dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function expressions, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        callFrame[dst] = callFrame->codeBlock()->functionExpression(func)->makeFunction(callFrame, callFrame->scopeChain());

        ++vPC;
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

        JSValuePtr funcVal = callFrame[func].jsValue(callFrame);

        Register* newCallFrame = callFrame->registers() + registerOffset;
        Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
        JSValuePtr thisValue = argv[0].jsValue(callFrame);
        JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

        if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
            JSValuePtr result = callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
            if (exceptionValue)
                goto vm_throw;
            callFrame[dst] = result;

            vPC += 5;
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

        JSValuePtr v = callFrame[func].jsValue(callFrame);

        CallData callData;
        CallType callType = v.getCallData(callData);

        if (callType == CallTypeJS) {
            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = callData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->bytecode(callDataScopeChain);

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
            JSValuePtr thisValue = thisRegister->jsValue(callFrame);
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();

            JSValuePtr returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler);
                returnValue = callData.native.function(newCallFrame, asObject(v), thisValue, args);
            }
            CHECK_FOR_EXCEPTION();

            callFrame[dst] = JSValuePtr(returnValue);

            vPC += 5;
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

        int src = (++vPC)->u.operand;
        ASSERT(callFrame->codeBlock()->needsFullScopeChain());

        asActivation(callFrame[src].getJSValue())->copyRegisters(callFrame->optionalCalleeArguments());

        ++vPC;
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

        callFrame->optionalCalleeArguments()->copyRegisters();

        ++vPC;
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

        int result = (++vPC)->u.operand;

        if (callFrame->codeBlock()->needsFullScopeChain())
            callFrame->scopeChain()->deref();

        JSValuePtr returnValue = callFrame[result].jsValue(callFrame);

        vPC = callFrame->returnPC();
        int dst = callFrame->returnValueRegister();
        callFrame = callFrame->callerFrame();
        
        if (callFrame->hasHostCallFrameFlag())
            return returnValue;

        callFrame[dst] = JSValuePtr(returnValue);

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
            callFrame[i] = jsUndefined();

        for (size_t count = codeBlock->numberOfConstantRegisters(), j = 0; j < count; ++i, ++j)
            callFrame[i] = codeBlock->constantRegister(j);

        ++vPC;
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
            callFrame[i] = jsUndefined();

        for (size_t count = codeBlock->numberOfConstantRegisters(), j = 0; j < count; ++i, ++j)
            callFrame[i] = codeBlock->constantRegister(j);

        int dst = (++vPC)->u.operand;
        JSActivation* activation = new (globalData) JSActivation(callFrame, static_cast<FunctionBodyNode*>(codeBlock->ownerNode()));
        callFrame[dst] = activation;
        callFrame->setScopeChain(callFrame->scopeChain()->copy()->push(activation));

        ++vPC;
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

        int thisRegister = (++vPC)->u.operand;
        JSValuePtr thisVal = callFrame[thisRegister].getJSValue();
        if (thisVal.needsThisConversion())
            callFrame[thisRegister] = JSValuePtr(thisVal.toThisObject(callFrame));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_create_arguments) {
        /* create_arguments

           Creates the 'arguments' object and places it in both the
           'arguments' call frame slot and the local 'arguments'
           register.

           This opcode should only be used at the beginning of a code
           block.
        */

        Arguments* arguments = new (globalData) Arguments(callFrame);
        callFrame->setCalleeArguments(arguments);
        callFrame[RegisterFile::ArgumentsRegister] = arguments;
        
        ++vPC;
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

        JSValuePtr v = callFrame[func].jsValue(callFrame);

        ConstructData constructData;
        ConstructType constructType = v.getConstructData(constructData);

        if (constructType == ConstructTypeJS) {
            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->bytecode(callDataScopeChain);

            Structure* structure;
            JSValuePtr prototype = callFrame[proto].jsValue(callFrame);
            if (prototype.isObject())
                structure = asObject(prototype)->inheritorID();
            else
                structure = callDataScopeChain->globalObject()->emptyObjectStructure();
            JSObject* newObject = new (globalData) JSObject(structure);

            callFrame[thisRegister] = JSValuePtr(newObject); // "this" value

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

            JSValuePtr returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler);
                returnValue = constructData.native.function(newCallFrame, asObject(v), args);
            }
            CHECK_FOR_EXCEPTION();
            callFrame[dst] = JSValuePtr(returnValue);

            vPC += 7;
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

        int dst = vPC[1].u.operand;;
        if (LIKELY(callFrame[dst].jsValue(callFrame).isObject())) {
            vPC += 3;
            NEXT_INSTRUCTION();
        }

        int override = vPC[2].u.operand;
        callFrame[dst] = callFrame[override];

        vPC += 3;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.  The contents of the register scope
           are replaced by the result of toObject conversion of the scope.
        */
        int scope = (++vPC)->u.operand;
        JSValuePtr v = callFrame[scope].jsValue(callFrame);
        JSObject* o = v.toObject(callFrame);
        CHECK_FOR_EXCEPTION();

        callFrame[scope] = JSValuePtr(o);
        callFrame->setScopeChain(callFrame->scopeChain()->push(o));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        callFrame->setScopeChain(callFrame->scopeChain()->pop());

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_get_pnames) {
        /* get_pnames dst(r) base(r)

           Creates a property name list for register base and puts it
           in register dst. This is not a true JavaScript value, just
           a synthetic value used to keep the iteration state in a
           register.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        callFrame[dst] = JSPropertyNameIterator::create(callFrame, callFrame[base].jsValue(callFrame));
        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_next_pname) {
        /* next_pname dst(r) iter(r) target(offset)

           Tries to copies the next name from property name list in
           register iter. If there are names left, then copies one to
           register dst, and jumps to offset target. If there are none
           left, invalidates the iterator and continues to the next
           instruction.
        */
        int dst = (++vPC)->u.operand;
        int iter = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

        JSPropertyNameIterator* it = callFrame[iter].propertyNameIterator();
        if (JSValuePtr temp = it->next(callFrame)) {
            CHECK_FOR_TIMEOUT();
            callFrame[dst] = JSValuePtr(temp);
            vPC += target;
            NEXT_INSTRUCTION();
        }
        it->invalidate();

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jmp_scopes) {
        /* jmp_scopes count(n) target(offset)

           Removes the a number of items from the current scope chain
           specified by immediate number count, then jumps to offset
           target.
        */
        int count = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

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

        vPC += 4;
        NEXT_INSTRUCTION();
    }
#if HAVE(COMPUTED_GOTO)
    skip_new_scope:
#endif
    DEFINE_OPCODE(op_catch) {
        /* catch ex(r)

           Retrieves the VMs current exception and puts it in register
           ex. This is only valid after an exception has been raised,
           and usually forms the beginning of an exception handler.
        */
        ASSERT(exceptionValue);
        ASSERT(!globalData->exception);
        int ex = (++vPC)->u.operand;
        callFrame[ex] = exceptionValue;
        exceptionValue = noValue();

        ++vPC;
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

        int ex = (++vPC)->u.operand;
        exceptionValue = callFrame[ex].jsValue(callFrame);

        handler = throwException(callFrame, exceptionValue, vPC - callFrame->codeBlock()->instructions().begin(), true);
        if (!handler) {
            *exception = exceptionValue;
            return jsNull();
        }

        vPC = callFrame->codeBlock()->instructions().begin() + handler->target;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_unexpected_load) {
        /* unexpected_load load dst(r) src(k)

           Copies constant src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = JSValuePtr(callFrame->codeBlock()->unexpectedConstant(src));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_new_error) {
        /* new_error dst(r) type(n) message(k)

           Constructs a new Error instance using the original
           constructor, using immediate number n as the type and
           constant message as the message string. The result is
           written to register dst.
        */
        int dst = (++vPC)->u.operand;
        int type = (++vPC)->u.operand;
        int message = (++vPC)->u.operand;

        CodeBlock* codeBlock = callFrame->codeBlock();
        callFrame[dst] = JSValuePtr(Error::create(callFrame, (ErrorType)type, codeBlock->unexpectedConstant(message).toString(callFrame), codeBlock->lineNumberForBytecodeOffset(callFrame, vPC - codeBlock->instructions().begin()), codeBlock->ownerNode()->sourceID(), codeBlock->ownerNode()->sourceURL()));

        ++vPC;
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
        int result = (++vPC)->u.operand;
        return callFrame[result].jsValue(callFrame);
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
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;

        ASSERT(callFrame[base].jsValue(callFrame).isObject());
        JSObject* baseObj = asObject(callFrame[base].jsValue(callFrame));
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        ASSERT(callFrame[function].jsValue(callFrame).isObject());
        baseObj->defineGetter(callFrame, ident, asObject(callFrame[function].jsValue(callFrame)));

        ++vPC;
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
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;

        ASSERT(callFrame[base].jsValue(callFrame).isObject());
        JSObject* baseObj = asObject(callFrame[base].jsValue(callFrame));
        Identifier& ident = callFrame->codeBlock()->identifier(property);
        ASSERT(callFrame[function].jsValue(callFrame).isObject());
        baseObj->defineSetter(callFrame, ident, asObject(callFrame[function].jsValue(callFrame)));

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_jsr) {
        /* jsr retAddrDst(r) target(offset)

           Places the address of the next instruction into the retAddrDst
           register and jumps to offset target from the current instruction.
        */
        int retAddrDst = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        callFrame[retAddrDst] = vPC + 1;

        vPC += target;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_sret) {
        /* sret retAddrSrc(r)

         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = (++vPC)->u.operand;
        vPC = callFrame[retAddrSrc].vPC();
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_debug) {
        /* debug debugHookID(n) firstLine(n) lastLine(n)

         Notifies the debugger of the current state of execution. This opcode
         is only generated while the debugger is attached.
        */
        int debugHookID = (++vPC)->u.operand;
        int firstLine = (++vPC)->u.operand;
        int lastLine = (++vPC)->u.operand;

        debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);

        ++vPC;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_profile_will_call) {
        /* op_profile_will_call function(r)

         Notifies the profiler of the beginning of a function call. This opcode
         is only generated if developer tools are enabled.
        */
        int function = vPC[1].u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->willExecute(callFrame, callFrame[function].jsValue(callFrame));

        vPC += 2;
        NEXT_INSTRUCTION();
    }
    DEFINE_OPCODE(op_profile_did_call) {
        /* op_profile_did_call function(r)

         Notifies the profiler of the end of a function call. This opcode
         is only generated if developer tools are enabled.
        */
        int function = vPC[1].u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->didExecute(callFrame, callFrame[function].jsValue(callFrame));

        vPC += 2;
        NEXT_INSTRUCTION();
    }
    vm_throw: {
        globalData->exception = noValue();
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
    #undef NEXT_INSTRUCTION
    #undef DEFINE_OPCODE
    #undef CHECK_FOR_EXCEPTION
    #undef CHECK_FOR_TIMEOUT
}

JSValuePtr Interpreter::retrieveArguments(CallFrame* callFrame, JSFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CodeBlock* codeBlock = functionCallFrame->codeBlock();
    if (codeBlock->usesArguments()) {
        ASSERT(codeBlock->codeType() == FunctionCode);
        SymbolTable& symbolTable = codeBlock->symbolTable();
        int argumentsIndex = symbolTable.get(functionCallFrame->propertyNames().arguments.ustring().rep()).getIndex();
        return functionCallFrame[argumentsIndex].jsValue(callFrame);
    }

    Arguments* arguments = functionCallFrame->optionalCalleeArguments();
    if (!arguments) {
        arguments = new (functionCallFrame) Arguments(functionCallFrame);
        arguments->copyRegisters();
        callFrame->setCalleeArguments(arguments);
    }

    return arguments;
}

JSValuePtr Interpreter::retrieveCaller(CallFrame* callFrame, InternalFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CallFrame* callerFrame = functionCallFrame->callerFrame();
    if (callerFrame->hasHostCallFrameFlag())
        return jsNull();

    JSValuePtr caller = callerFrame->callee();
    if (!caller)
        return jsNull();

    return caller;
}

void Interpreter::retrieveLastCaller(CallFrame* callFrame, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValuePtr& function) const
{
    function = noValue();
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
    sourceID = callerCodeBlock->ownerNode()->sourceID();
    sourceURL = callerCodeBlock->ownerNode()->sourceURL();
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

#if ENABLE(JIT)

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

NEVER_INLINE void Interpreter::tryCTICachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValuePtr baseValue, const PutPropertySlot& slot)
{
    // The interpreter checks for recursion here; I do not believe this can occur in CTI.

    if (!baseValue.isCell())
        return;

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }
    
    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isDictionary()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }

    // If baseCell != base, then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }

    StructureStubInfo* stubInfo = &codeBlock->getStubInfo(returnAddress);

    // Cache hit: Specialize instruction and ref Structures.

    // Structure transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        StructureChain* chain = structure->cachedPrototypeChain();
        if (!chain) {
            chain = cachePrototypeChain(callFrame, structure);
            if (!chain) {
                // This happens if someone has manually inserted null into the prototype chain 
                stubInfo->opcodeID = op_put_by_id_generic;
                return;
            }
        }
        stubInfo->initPutByIdTransition(structure->previousID(), structure, chain);
        JIT::compilePutByIdTransition(callFrame->scopeChain()->globalData, codeBlock, stubInfo, structure->previousID(), structure, slot.cachedOffset(), chain, returnAddress);
        return;
    }
    
    stubInfo->initPutByIdReplace(structure);

#if USE(CTI_REPATCH_PIC)
    UNUSED_PARAM(callFrame);
    JIT::patchPutByIdReplace(stubInfo, structure, slot.cachedOffset(), returnAddress);
#else
    JIT::compilePutByIdReplace(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, slot.cachedOffset(), returnAddress);
#endif
}

NEVER_INLINE void Interpreter::tryCTICacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValuePtr baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    if (isJSArray(baseValue) && propertyName == callFrame->propertyNames().length) {
#if USE(CTI_REPATCH_PIC)
        JIT::compilePatchGetArrayLength(callFrame->scopeChain()->globalData, codeBlock, returnAddress);
#else
        ctiPatchCallByReturnAddress(returnAddress, m_ctiArrayLengthTrampoline);
#endif
        return;
    }
    if (isJSString(baseValue) && propertyName == callFrame->propertyNames().length) {
        // The tradeoff of compiling an patched inline string length access routine does not seem
        // to pay off, so we currently only do this for arrays.
        ctiPatchCallByReturnAddress(returnAddress, m_ctiStringLengthTrampoline);
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isDictionary()) {
        ctiPatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    StructureStubInfo* stubInfo = &codeBlock->getStubInfo(returnAddress);

    // Cache hit: Specialize instruction and ref Structures.

    if (slot.slotBase() == baseValue) {
        // set this up, so derefStructures can do it's job.
        stubInfo->initGetByIdSelf(structure);
        
#if USE(CTI_REPATCH_PIC)
        JIT::patchGetByIdSelf(stubInfo, structure, slot.cachedOffset(), returnAddress);
#else
        JIT::compileGetByIdSelf(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, slot.cachedOffset(), returnAddress);
#endif
        return;
    }

    if (slot.slotBase() == structure->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase().isObject());

        JSObject* slotBaseObject = asObject(slot.slotBase());

        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (slotBaseObject->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(slotBaseObject->structure());
            slotBaseObject->setStructure(transition.release());
            asCell(baseValue)->structure()->setCachedPrototypeChain(0);
        }
        
        stubInfo->initGetByIdProto(structure, slotBaseObject->structure());

        JIT::compileGetByIdProto(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, slotBaseObject->structure(), slot.cachedOffset(), returnAddress);
        return;
    }

    size_t count = countPrototypeChainEntriesAndCheckForProxies(callFrame, baseValue, slot);
    if (!count) {
        stubInfo->opcodeID = op_get_by_id_generic;
        return;
    }

    StructureChain* chain = structure->cachedPrototypeChain();
    if (!chain)
        chain = cachePrototypeChain(callFrame, structure);
    ASSERT(chain);

    stubInfo->initGetByIdChain(structure, chain);

    JIT::compileGetByIdChain(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, structure, chain, count, slot.cachedOffset(), returnAddress);
}

#endif

#if USE(JIT_STUB_ARGUMENT_VA_LIST)
#define SETUP_VA_LISTL_ARGS va_list vl_args; va_start(vl_args, args)
#else // JIT_STUB_ARGUMENT_REGISTER or JIT_STUB_ARGUMENT_STACK
#define SETUP_VA_LISTL_ARGS
#endif

#ifndef NDEBUG

extern "C" {

static void jscGeneratedNativeCode() 
{
    // When executing a CTI function (which might do an allocation), we hack the return address
    // to pretend to be executing this function, to keep stack logging tools from blowing out
    // memory.
}

}

struct StackHack {
    ALWAYS_INLINE StackHack(void** location) 
    { 
        returnAddressLocation = location;
        savedReturnAddress = *returnAddressLocation;
        ctiSetReturnAddress(returnAddressLocation, reinterpret_cast<void*>(jscGeneratedNativeCode));
    }
    ALWAYS_INLINE ~StackHack() 
    { 
        ctiSetReturnAddress(returnAddressLocation, savedReturnAddress);
    }

    void** returnAddressLocation;
    void* savedReturnAddress;
};

#define BEGIN_STUB_FUNCTION() SETUP_VA_LISTL_ARGS; StackHack stackHack(&STUB_RETURN_ADDRESS_SLOT)
#define STUB_SET_RETURN_ADDRESS(address) stackHack.savedReturnAddress = address
#define STUB_RETURN_ADDRESS stackHack.savedReturnAddress

#else

#define BEGIN_STUB_FUNCTION() SETUP_VA_LISTL_ARGS
#define STUB_SET_RETURN_ADDRESS(address) ctiSetReturnAddress(&STUB_RETURN_ADDRESS_SLOT, address);
#define STUB_RETURN_ADDRESS STUB_RETURN_ADDRESS_SLOT

#endif

// The reason this is not inlined is to avoid having to do a PIC branch
// to get the address of the ctiVMThrowTrampoline function. It's also
// good to keep the code size down by leaving as much of the exception
// handling code out of line as possible.
static NEVER_INLINE void returnToThrowTrampoline(JSGlobalData* globalData, void* exceptionLocation, void*& returnAddressSlot)
{
    ASSERT(globalData->exception);
    globalData->exceptionLocation = exceptionLocation;
    ctiSetReturnAddress(&returnAddressSlot, reinterpret_cast<void*>(ctiVMThrowTrampoline));
}

static NEVER_INLINE void throwStackOverflowError(CallFrame* callFrame, JSGlobalData* globalData, void* exceptionLocation, void*& returnAddressSlot)
{
    globalData->exception = createStackOverflowError(callFrame);
    returnToThrowTrampoline(globalData, exceptionLocation, returnAddressSlot);
}

#define VM_THROW_EXCEPTION() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        return 0; \
    } while (0)
#define VM_THROW_EXCEPTION_2() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        RETURN_PAIR(0, 0); \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    returnToThrowTrampoline(ARG_globalData, STUB_RETURN_ADDRESS, STUB_RETURN_ADDRESS)

#define CHECK_FOR_EXCEPTION() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != noValue())) \
            VM_THROW_EXCEPTION(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_AT_END() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != noValue())) \
            VM_THROW_EXCEPTION_AT_END(); \
    } while (0)
#define CHECK_FOR_EXCEPTION_VOID() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != noValue())) { \
            VM_THROW_EXCEPTION_AT_END(); \
            return; \
        } \
    } while (0)

JSObject* Interpreter::cti_op_convert_this(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v1 = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSObject* result = v1.toThisObject(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

void Interpreter::cti_op_end(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ScopeChainNode* scopeChain = ARG_callFrame->scopeChain();
    ASSERT(scopeChain->refCount > 1);
    scopeChain->deref();
}

JSValueEncodedAsPointer* Interpreter::cti_op_add(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v1 = ARG_src1;
    JSValuePtr v2 = ARG_src2;

    double left;
    double right = 0.0;

    bool rightIsNumber = v2.getNumber(right);
    if (rightIsNumber && v1.getNumber(left))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left + right));
    
    CallFrame* callFrame = ARG_callFrame;

    bool leftIsString = v1.isString();
    if (leftIsString && v2.isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }

        return JSValuePtr::encode(jsString(ARG_globalData, value.release()));
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = v2.isInt32Fast() ?
            concatenate(asString(v1)->value().rep(), v2.getInt32Fast()) :
            concatenate(asString(v1)->value().rep(), right);

        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }
        return JSValuePtr::encode(jsString(ARG_globalData, value.release()));
    }

    // All other cases are pretty uncommon
    JSValuePtr result = jsAddSlowCase(callFrame, v1, v2);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_pre_inc(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, v.toNumber(callFrame) + 1);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

int Interpreter::cti_timeout_check(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    Interpreter* interpreter = ARG_globalData->interpreter;

    if (interpreter->checkTimeout(ARG_callFrame->dynamicGlobalObject())) {
        ARG_globalData->exception = createInterruptedExecutionException(ARG_globalData);
        VM_THROW_EXCEPTION_AT_END();
    }
    
    return interpreter->m_ticksUntilNextTimeoutCheck;
}

void Interpreter::cti_register_file_check(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    if (LIKELY(ARG_registerFile->grow(ARG_callFrame + ARG_callFrame->codeBlock()->m_numCalleeRegisters)))
        return;

    // Rewind to the previous call frame because op_call already optimistically
    // moved the call frame forward.
    CallFrame* oldCallFrame = ARG_callFrame->callerFrame();
    ARG_setCallFrame(oldCallFrame);
    throwStackOverflowError(oldCallFrame, ARG_globalData, oldCallFrame->returnPC(), STUB_RETURN_ADDRESS);
}

int Interpreter::cti_op_loop_if_less(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

int Interpreter::cti_op_loop_if_lesseq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLessEq(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

JSObject* Interpreter::cti_op_new_object(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return constructEmptyObject(ARG_callFrame);
}

void Interpreter::cti_op_put_by_id_generic(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    PutPropertySlot slot;
    ARG_src1.put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    CHECK_FOR_EXCEPTION_AT_END();
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_generic(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(callFrame, ident, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)

void Interpreter::cti_op_put_by_id(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1.put(callFrame, ident, ARG_src3, slot);

    ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_id_second));

    CHECK_FOR_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_id_second(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    PutPropertySlot slot;
    ARG_src1.put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    ARG_globalData->interpreter->tryCTICachePutByID(ARG_callFrame, ARG_callFrame->codeBlock(), STUB_RETURN_ADDRESS, ARG_src1, slot);
    CHECK_FOR_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_id_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1.put(callFrame, ident, ARG_src3, slot);

    CHECK_FOR_EXCEPTION_AT_END();
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(callFrame, ident, slot);

    ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_second));

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_second(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(callFrame, ident, slot);

    ARG_globalData->interpreter->tryCTICacheGetByID(callFrame, callFrame->codeBlock(), STUB_RETURN_ADDRESS, baseValue, ident, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_self_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(callFrame, ident, slot);

    CHECK_FOR_EXCEPTION();

    if (baseValue.isCell()
        && slot.isCacheable()
        && !asCell(baseValue)->structure()->isDictionary()
        && slot.slotBase() == baseValue) {

        CodeBlock* codeBlock = callFrame->codeBlock();
        StructureStubInfo* stubInfo = &codeBlock->getStubInfo(STUB_RETURN_ADDRESS);

        ASSERT(slot.slotBase().isObject());

        PolymorphicAccessStructureList* polymorphicStructureList;
        int listIndex = 1;

        if (stubInfo->opcodeID == op_get_by_id_self) {
            ASSERT(!stubInfo->stubRoutine);
            polymorphicStructureList = new PolymorphicAccessStructureList(0, stubInfo->u.getByIdSelf.baseObjectStructure);
            stubInfo->initGetByIdSelfList(polymorphicStructureList, 2);
        } else {
            polymorphicStructureList = stubInfo->u.getByIdSelfList.structureList;
            listIndex = stubInfo->u.getByIdSelfList.listSize;
            stubInfo->u.getByIdSelfList.listSize++;
        }

        JIT::compileGetByIdSelfList(callFrame->scopeChain()->globalData, codeBlock, stubInfo, polymorphicStructureList, listIndex, asCell(baseValue)->structure(), slot.cachedOffset());

        if (listIndex == (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_generic));
    } else {
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_generic));
    }
    return JSValuePtr::encode(result);
}

static PolymorphicAccessStructureList* getPolymorphicAccessStructureListSlot(StructureStubInfo* stubInfo, int& listIndex)
{
    PolymorphicAccessStructureList* prototypeStructureList = 0;
    listIndex = 1;

    switch (stubInfo->opcodeID) {
    case op_get_by_id_proto:
        prototypeStructureList = new PolymorphicAccessStructureList(stubInfo->stubRoutine, stubInfo->u.getByIdProto.baseObjectStructure, stubInfo->u.getByIdProto.prototypeStructure);
        stubInfo->stubRoutine = 0;
        stubInfo->initGetByIdProtoList(prototypeStructureList, 2);
        break;
    case op_get_by_id_chain:
        prototypeStructureList = new PolymorphicAccessStructureList(stubInfo->stubRoutine, stubInfo->u.getByIdChain.baseObjectStructure, stubInfo->u.getByIdChain.chain);
        stubInfo->stubRoutine = 0;
        stubInfo->initGetByIdProtoList(prototypeStructureList, 2);
        break;
    case op_get_by_id_proto_list:
        prototypeStructureList = stubInfo->u.getByIdProtoList.structureList;
        listIndex = stubInfo->u.getByIdProtoList.listSize;
        stubInfo->u.getByIdProtoList.listSize++;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    ASSERT(listIndex < POLYMORPHIC_LIST_CACHE_SIZE);
    return prototypeStructureList;
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_proto_list(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION();

    if (!baseValue.isCell() || !slot.isCacheable() || asCell(baseValue)->structure()->isDictionary()) {
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_fail));
        return JSValuePtr::encode(result);
    }

    Structure* structure = asCell(baseValue)->structure();
    CodeBlock* codeBlock = callFrame->codeBlock();
    StructureStubInfo* stubInfo = &codeBlock->getStubInfo(STUB_RETURN_ADDRESS);

    ASSERT(slot.slotBase().isObject());
    JSObject* slotBaseObject = asObject(slot.slotBase());

    if (slot.slotBase() == baseValue)
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_fail));
    else if (slot.slotBase() == asCell(baseValue)->structure()->prototypeForLookup(callFrame)) {
        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (slotBaseObject->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(slotBaseObject->structure());
            slotBaseObject->setStructure(transition.release());
            asCell(baseValue)->structure()->setCachedPrototypeChain(0);
        }

        int listIndex;
        PolymorphicAccessStructureList* prototypeStructureList = getPolymorphicAccessStructureListSlot(stubInfo, listIndex);

        JIT::compileGetByIdProtoList(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, prototypeStructureList, listIndex, structure, slotBaseObject->structure(), slot.cachedOffset());

        if (listIndex == (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_list_full));
    } else if (size_t count = countPrototypeChainEntriesAndCheckForProxies(callFrame, baseValue, slot)) {
        StructureChain* chain = structure->cachedPrototypeChain();
        if (!chain)
            chain = cachePrototypeChain(callFrame, structure);
        ASSERT(chain);

        int listIndex;
        PolymorphicAccessStructureList* prototypeStructureList = getPolymorphicAccessStructureListSlot(stubInfo, listIndex);

        JIT::compileGetByIdChainList(callFrame->scopeChain()->globalData, callFrame, codeBlock, stubInfo, prototypeStructureList, listIndex, structure, chain, count, slot.cachedOffset());

        if (listIndex == (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_list_full));
    } else
        ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_proto_fail));

    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_proto_list_full(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_proto_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_array_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_id_string_fail(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue.get(ARG_callFrame, *ARG_id2, slot);

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

#endif

JSValueEncodedAsPointer* Interpreter::cti_op_instanceof(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr value = ARG_src1;
    JSValuePtr baseVal = ARG_src2;
    JSValuePtr proto = ARG_src3;

    // at least one of these checks must have failed to get to the slow case
    ASSERT(!value.isCell() || !baseVal.isCell() || !proto.isCell()
           || !value.isObject() || !baseVal.isObject() || !proto.isObject() 
           || (asObject(baseVal)->structure()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);

    if (!baseVal.isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "instanceof", baseVal, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    if (!asObject(baseVal)->structure()->typeInfo().implementsHasInstance())
        return JSValuePtr::encode(jsBoolean(false));

    if (!proto.isObject()) {
        throwError(callFrame, TypeError, "instanceof called on an object with an invalid prototype property.");
        VM_THROW_EXCEPTION();
    }
        
    if (!value.isObject())
        return JSValuePtr::encode(jsBoolean(false));

    JSValuePtr result = jsBoolean(asObject(baseVal)->hasInstance(callFrame, value, proto));
    CHECK_FOR_EXCEPTION_AT_END();

    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_del_by_id(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    
    JSObject* baseObj = ARG_src1.toObject(callFrame);

    JSValuePtr result = jsBoolean(baseObj->deleteProperty(callFrame, *ARG_id2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_mul(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left * right));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1.toNumber(callFrame) * src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSObject* Interpreter::cti_op_new_func(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return ARG_func1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

void* Interpreter::cti_op_call_JSFunction(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

#ifndef NDEBUG
    CallData callData;
    ASSERT(ARG_src1.getCallData(callData) == CallTypeJS);
#endif

    ScopeChainNode* callDataScopeChain = asFunction(ARG_src1)->m_scopeChain.node();
    CodeBlock* newCodeBlock = &asFunction(ARG_src1)->body()->bytecode(callDataScopeChain);

    if (!newCodeBlock->jitCode())
        JIT::compile(ARG_globalData, newCodeBlock);

    return newCodeBlock;
}

VoidPtrPair Interpreter::cti_op_call_arityCheck(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* newCodeBlock = ARG_codeBlock4;
    int argCount = ARG_int3;

    ASSERT(argCount != newCodeBlock->m_numParameters);

    CallFrame* oldCallFrame = callFrame->callerFrame();

    if (argCount > newCodeBlock->m_numParameters) {
        size_t numParameters = newCodeBlock->m_numParameters;
        Register* r = callFrame->registers() + numParameters;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argCount;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argCount] = argv[i];

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    } else {
        size_t omittedArgCount = newCodeBlock->m_numParameters - argCount;
        Register* r = callFrame->registers() + omittedArgCount;
        Register* newEnd = r + newCodeBlock->m_numCalleeRegisters;
        if (!ARG_registerFile->grow(newEnd)) {
            // Rewind to the previous call frame because op_call already optimistically
            // moved the call frame forward.
            ARG_setCallFrame(oldCallFrame);
            throwStackOverflowError(oldCallFrame, ARG_globalData, ARG_returnAddress2, STUB_RETURN_ADDRESS);
            RETURN_PAIR(0, 0);
        }

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    }

    RETURN_PAIR(newCodeBlock, callFrame);
}

void* Interpreter::cti_vm_dontLazyLinkCall(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSFunction* callee = asFunction(ARG_src1);
    CodeBlock* codeBlock = &callee->body()->bytecode(callee->m_scopeChain.node());
    if (!codeBlock->jitCode())
        JIT::compile(ARG_globalData, codeBlock);

    ctiPatchCallByReturnAddress(ARG_returnAddress2, ARG_globalData->interpreter->m_ctiVirtualCallLink);

    return codeBlock->jitCode();
}

void* Interpreter::cti_vm_lazyLinkCall(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSFunction* callee = asFunction(ARG_src1);
    CodeBlock* codeBlock = &callee->body()->bytecode(callee->m_scopeChain.node());
    if (!codeBlock->jitCode())
        JIT::compile(ARG_globalData, codeBlock);

    CallLinkInfo* callLinkInfo = &ARG_callFrame->callerFrame()->codeBlock()->getCallLinkInfo(ARG_returnAddress2);
    JIT::linkCall(callee, codeBlock, codeBlock->jitCode(), callLinkInfo, ARG_int3);

    return codeBlock->jitCode();
}

JSObject* Interpreter::cti_op_push_activation(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSActivation* activation = new (ARG_globalData) JSActivation(ARG_callFrame, static_cast<FunctionBodyNode*>(ARG_callFrame->codeBlock()->ownerNode()));
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->copy()->push(activation));
    return activation;
}

JSValueEncodedAsPointer* Interpreter::cti_op_call_NotJSFunction(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr funcVal = ARG_src1;

    CallData callData;
    CallType callType = funcVal.getCallData(callData);

    ASSERT(callType != CallTypeJS);

    if (callType == CallTypeHost) {
        int registerOffset = ARG_int2;
        int argCount = ARG_int3;
        CallFrame* previousCallFrame = ARG_callFrame;
        CallFrame* callFrame = CallFrame::create(previousCallFrame->registers() + registerOffset);

        callFrame->init(0, static_cast<Instruction*>(STUB_RETURN_ADDRESS), previousCallFrame->scopeChain(), previousCallFrame, 0, argCount, 0);
        ARG_setCallFrame(callFrame);

        Register* argv = ARG_callFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
        ArgList argList(argv + 1, argCount - 1);

        JSValuePtr returnValue;
        {
            SamplingTool::HostCallRecord callRecord(CTI_SAMPLER);

            // FIXME: All host methods should be calling toThisObject, but this is not presently the case.
            JSValuePtr thisValue = argv[0].jsValue(callFrame);
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();

            returnValue = callData.native.function(callFrame, asObject(funcVal), thisValue, argList);
        }
        ARG_setCallFrame(previousCallFrame);
        CHECK_FOR_EXCEPTION();

        return JSValuePtr::encode(returnValue);
    }

    ASSERT(callType == CallTypeNone);

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createNotAFunctionError(ARG_callFrame, funcVal, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

void Interpreter::cti_op_create_arguments(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void Interpreter::cti_op_create_arguments_no_params(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame, Arguments::NoParameters);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void Interpreter::cti_op_tear_off_activation(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain());
    asActivation(ARG_src1)->copyRegisters(ARG_callFrame->optionalCalleeArguments());
}

void Interpreter::cti_op_tear_off_arguments(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(ARG_callFrame->codeBlock()->usesArguments() && !ARG_callFrame->codeBlock()->needsFullScopeChain());
    ARG_callFrame->optionalCalleeArguments()->copyRegisters();
}

void Interpreter::cti_op_profile_will_call(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->willExecute(ARG_callFrame, ARG_src1);
}

void Interpreter::cti_op_profile_did_call(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->didExecute(ARG_callFrame, ARG_src1);
}

void Interpreter::cti_op_ret_scopeChain(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain());
    ARG_callFrame->scopeChain()->deref();
}

JSObject* Interpreter::cti_op_new_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ArgList argList(&ARG_callFrame->registers()[ARG_int1], ARG_int2);
    return constructArray(ARG_callFrame, argList);
}

JSValueEncodedAsPointer* Interpreter::cti_op_resolve(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValuePtr result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();
            return JSValuePtr::encode(result);
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSObject* Interpreter::cti_op_construct_JSConstruct(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(asFunction(ARG_src1)->getConstructData(constructData) == ConstructTypeJS);
#endif

    Structure* structure;
    if (ARG_src4.isObject())
        structure = asObject(ARG_src4)->inheritorID();
    else
        structure = asFunction(ARG_src1)->m_scopeChain.node()->globalObject()->emptyObjectStructure();
    return new (ARG_globalData) JSObject(structure);
}

JSValueEncodedAsPointer* Interpreter::cti_op_construct_NotJSConstruct(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr constrVal = ARG_src1;
    int argCount = ARG_int3;
    int thisRegister = ARG_int5;

    ConstructData constructData;
    ConstructType constructType = constrVal.getConstructData(constructData);

    if (constructType == ConstructTypeHost) {
        ArgList argList(callFrame->registers() + thisRegister + 1, argCount - 1);

        JSValuePtr returnValue;
        {
            SamplingTool::HostCallRecord callRecord(CTI_SAMPLER);
            returnValue = constructData.native.function(callFrame, asObject(constrVal), argList);
        }
        CHECK_FOR_EXCEPTION();

        return JSValuePtr::encode(returnValue);
    }

    ASSERT(constructType == ConstructTypeNone);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createNotAConstructorError(callFrame, constrVal, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_val(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Interpreter* interpreter = ARG_globalData->interpreter;

    JSValuePtr baseValue = ARG_src1;
    JSValuePtr subscript = ARG_src2;

    JSValuePtr result;

    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (interpreter->isJSArray(baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canGetIndex(i))
                result = jsArray->getIndex(i);
            else
                result = jsArray->JSArray::get(callFrame, i);
        } else if (interpreter->isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            result = asString(baseValue)->getIndex(ARG_globalData, i);
        else if (interpreter->isJSByteArray(baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_val_byte_array));
            return JSValuePtr::encode(asByteArray(baseValue)->getIndex(callFrame, i));
        } else
            result = baseValue.get(callFrame, i);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        result = baseValue.get(callFrame, property);
    }

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_get_by_val_byte_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    
    CallFrame* callFrame = ARG_callFrame;
    Interpreter* interpreter = ARG_globalData->interpreter;
    
    JSValuePtr baseValue = ARG_src1;
    JSValuePtr subscript = ARG_src2;
    
    JSValuePtr result;

    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (interpreter->isJSByteArray(baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            return JSValuePtr::encode(asByteArray(baseValue)->getIndex(callFrame, i));
        }

        result = baseValue.get(callFrame, i);
        if (!interpreter->isJSByteArray(baseValue))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_val));
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        result = baseValue.get(callFrame, property);
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

VoidPtrPair Interpreter::cti_op_resolve_func(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {            
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(callFrame);
            JSValuePtr result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();

            RETURN_PAIR(thisObj, JSValuePtr::encode(result));
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSValueEncodedAsPointer* Interpreter::cti_op_sub(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left - right));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1.toNumber(callFrame) - src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

void Interpreter::cti_op_put_by_val(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    Interpreter* interpreter = ARG_globalData->interpreter;

    JSValuePtr baseValue = ARG_src1;
    JSValuePtr subscript = ARG_src2;
    JSValuePtr value = ARG_src3;

    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (interpreter->isJSArray(baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canSetIndex(i))
                jsArray->setIndex(i, value);
            else
                jsArray->JSArray::put(callFrame, i, value);
        } else if (interpreter->isJSByteArray(baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            JSByteArray* jsByteArray = asByteArray(baseValue);
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_val_byte_array));
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            if (value.isInt32Fast()) {
                jsByteArray->setIndex(i, value.getInt32Fast());
                return;
            } else {
                double dValue = 0;
                if (value.getNumber(dValue)) {
                    jsByteArray->setIndex(i, dValue);
                    return;
                }
            }

            baseValue.put(callFrame, i, value);
        } else
            baseValue.put(callFrame, i, value);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }

    CHECK_FOR_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_val_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr baseValue = ARG_src1;
    int i = ARG_int2;
    JSValuePtr value = ARG_src3;

    ASSERT(ARG_globalData->interpreter->isJSArray(baseValue));

    if (LIKELY(i >= 0))
        asArray(baseValue)->JSArray::put(callFrame, i, value);
    else {
        // This should work since we're re-boxing an immediate unboxed in JIT code.
        ASSERT(JSValuePtr::makeInt32Fast(i));
        Identifier property(callFrame, JSValuePtr::makeInt32Fast(i).toString(callFrame));
        // FIXME: can toString throw an exception here?
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }

    CHECK_FOR_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_val_byte_array(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();
    
    CallFrame* callFrame = ARG_callFrame;
    Interpreter* interpreter = ARG_globalData->interpreter;
    
    JSValuePtr baseValue = ARG_src1;
    JSValuePtr subscript = ARG_src2;
    JSValuePtr value = ARG_src3;
    
    if (LIKELY(subscript.isUInt32Fast())) {
        uint32_t i = subscript.getUInt32Fast();
        if (interpreter->isJSByteArray(baseValue) && asByteArray(baseValue)->canAccessIndex(i)) {
            JSByteArray* jsByteArray = asByteArray(baseValue);
            
            // All fast byte array accesses are safe from exceptions so return immediately to avoid exception checks.
            if (value.isInt32Fast()) {
                jsByteArray->setIndex(i, value.getInt32Fast());
                return;
            } else {
                double dValue = 0;                
                if (value.getNumber(dValue)) {
                    jsByteArray->setIndex(i, dValue);
                    return;
                }
            }
        }

        if (!interpreter->isJSByteArray(baseValue))
            ctiPatchCallByReturnAddress(STUB_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_val));
        baseValue.put(callFrame, i, value);
    } else {
        Identifier property(callFrame, subscript.toString(callFrame));
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue.put(callFrame, property, value, slot);
        }
    }
    
    CHECK_FOR_EXCEPTION_AT_END();
}

JSValueEncodedAsPointer* Interpreter::cti_op_lesseq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsBoolean(jsLessEq(callFrame, ARG_src1, ARG_src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

int Interpreter::cti_op_loop_if_true(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1.toBoolean(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

JSValueEncodedAsPointer* Interpreter::cti_op_negate(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src = ARG_src1;

    double v;
    if (src.getNumber(v))
        return JSValuePtr::encode(jsNumber(ARG_globalData, -v));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, -src.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_resolve_base(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(inlineResolveBase(ARG_callFrame, *ARG_id1, ARG_callFrame->scopeChain()));
}

JSValueEncodedAsPointer* Interpreter::cti_op_resolve_skip(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    int skip = ARG_int2;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = *ARG_id1;
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValuePtr result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();
            return JSValuePtr::encode(result);
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSValueEncodedAsPointer* Interpreter::cti_op_resolve_global(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSGlobalObject* globalObject = asGlobalObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    unsigned globalResolveInfoIndex = ARG_int3;
    ASSERT(globalObject->isGlobalObject());

    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValuePtr result = slot.getValue(callFrame, ident);
        if (slot.isCacheable() && !globalObject->structure()->isDictionary()) {
            GlobalResolveInfo& globalResolveInfo = callFrame->codeBlock()->globalResolveInfo(globalResolveInfoIndex);
            if (globalResolveInfo.structure)
                globalResolveInfo.structure->deref();
            globalObject->structure()->ref();
            globalResolveInfo.structure = globalObject->structure();
            globalResolveInfo.offset = slot.cachedOffset();
            return JSValuePtr::encode(result);
        }

        CHECK_FOR_EXCEPTION_AT_END();
        return JSValuePtr::encode(result);
    }

    unsigned vPCIndex = callFrame->codeBlock()->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

JSValueEncodedAsPointer* Interpreter::cti_op_div(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    double left;
    double right;
    if (src1.getNumber(left) && src2.getNumber(right))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left / right));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1.toNumber(callFrame) / src2.toNumber(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_pre_dec(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, v.toNumber(callFrame) - 1);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

int Interpreter::cti_op_jless(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

JSValueEncodedAsPointer* Interpreter::cti_op_not(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = jsBoolean(!src.toBoolean(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

int Interpreter::cti_op_jtrue(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1.toBoolean(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair Interpreter::cti_op_post_inc(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr number = v.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();

    RETURN_PAIR(JSValuePtr::encode(number), JSValuePtr::encode(jsNumber(ARG_globalData, number.uncheckedGetNumber() + 1)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_eq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(!JSValuePtr::areBothInt32Fast(src1, src2));
    JSValuePtr result = jsBoolean(JSValuePtr::equalSlowCaseInline(callFrame, src1, src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_lshift(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr val = ARG_src1;
    JSValuePtr shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSValuePtr::areBothInt32Fast(val, shift))
        return JSValuePtr::encode(jsNumber(ARG_globalData, val.getInt32Fast() << (shift.getInt32Fast() & 0x1f)));
    if (val.numberToInt32(left) && shift.numberToUInt32(right))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left << (right & 0x1f)));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, (val.toInt32(callFrame)) << (shift.toUInt32(callFrame) & 0x1f));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_bitand(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    int32_t left;
    int32_t right;
    if (src1.numberToInt32(left) && src2.numberToInt32(right))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left & right));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1.toInt32(callFrame) & src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_rshift(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr val = ARG_src1;
    JSValuePtr shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSFastMath::canDoFastRshift(val, shift))
        return JSValuePtr::encode(JSFastMath::rightShiftImmediateNumbers(val, shift));
    if (val.numberToInt32(left) && shift.numberToUInt32(right))
        return JSValuePtr::encode(jsNumber(ARG_globalData, left >> (right & 0x1f)));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, (val.toInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_bitnot(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src = ARG_src1;

    int value;
    if (src.numberToInt32(value))
        return JSValuePtr::encode(jsNumber(ARG_globalData, ~value));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, ~src.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

VoidPtrPair Interpreter::cti_op_resolve_with_base(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = *ARG_id1;
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {
            JSValuePtr result = slot.getValue(callFrame, ident);
            CHECK_FOR_EXCEPTION_AT_END();

            RETURN_PAIR(base, JSValuePtr::encode(result));
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSObject* Interpreter::cti_op_new_func_exp(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return ARG_funcexp1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

JSValueEncodedAsPointer* Interpreter::cti_op_mod(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr dividendValue = ARG_src1;
    JSValuePtr divisorValue = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;
    double d = dividendValue.toNumber(callFrame);
    JSValuePtr result = jsNumber(ARG_globalData, fmod(d, divisorValue.toNumber(callFrame)));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_less(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsBoolean(jsLess(callFrame, ARG_src1, ARG_src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_neq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    ASSERT(!JSValuePtr::areBothInt32Fast(src1, src2));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsBoolean(!JSValuePtr::equalSlowCaseInline(callFrame, src1, src2));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

VoidPtrPair Interpreter::cti_op_post_dec(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr number = v.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();

    RETURN_PAIR(JSValuePtr::encode(number), JSValuePtr::encode(jsNumber(ARG_globalData, number.uncheckedGetNumber() - 1)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_urshift(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr val = ARG_src1;
    JSValuePtr shift = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    if (JSFastMath::canDoFastUrshift(val, shift))
        return JSValuePtr::encode(JSFastMath::rightShiftImmediateNumbers(val, shift));
    else {
        JSValuePtr result = jsNumber(ARG_globalData, (val.toUInt32(callFrame)) >> (shift.toUInt32(callFrame) & 0x1f));
        CHECK_FOR_EXCEPTION_AT_END();
        return JSValuePtr::encode(result);
    }
}

JSValueEncodedAsPointer* Interpreter::cti_op_bitxor(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = jsNumber(ARG_globalData, src1.toInt32(callFrame) ^ src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSObject* Interpreter::cti_op_new_regexp(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return new (ARG_globalData) RegExpObject(ARG_callFrame->lexicalGlobalObject()->regExpStructure(), ARG_regexp1);
}

JSValueEncodedAsPointer* Interpreter::cti_op_bitor(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = jsNumber(ARG_globalData, src1.toInt32(callFrame) | src2.toInt32(callFrame));
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_call_eval(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    RegisterFile* registerFile = ARG_registerFile;

    Interpreter* interpreter = ARG_globalData->interpreter;
    
    JSValuePtr funcVal = ARG_src1;
    int registerOffset = ARG_int2;
    int argCount = ARG_int3;

    Register* newCallFrame = callFrame->registers() + registerOffset;
    Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
    JSValuePtr thisValue = argv[0].jsValue(callFrame);
    JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

    if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
        JSValuePtr exceptionValue = noValue();
        JSValuePtr result = interpreter->callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
        if (UNLIKELY(exceptionValue != noValue())) {
            ARG_globalData->exception = exceptionValue;
            VM_THROW_EXCEPTION_AT_END();
        }
        return JSValuePtr::encode(result);
    }

    return JSValuePtr::encode(jsImpossibleValue());
}

JSValueEncodedAsPointer* Interpreter::cti_op_throw(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);

    JSValuePtr exceptionValue = ARG_src1;
    ASSERT(exceptionValue);

    HandlerInfo* handler = ARG_globalData->interpreter->throwException(callFrame, exceptionValue, vPCIndex, true);

    if (!handler) {
        *ARG_exception = exceptionValue;
        return JSValuePtr::encode(jsNull());
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = handler->nativeCode;
    ASSERT(catchRoutine);
    STUB_SET_RETURN_ADDRESS(catchRoutine);
    return JSValuePtr::encode(exceptionValue);
}

JSPropertyNameIterator* Interpreter::cti_op_get_pnames(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSPropertyNameIterator::create(ARG_callFrame, ARG_src1);
}

JSValueEncodedAsPointer* Interpreter::cti_op_next_pname(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSPropertyNameIterator* it = ARG_pni1;
    JSValuePtr temp = it->next(ARG_callFrame);
    if (!temp)
        it->invalidate();
    return JSValuePtr::encode(temp);
}

JSObject* Interpreter::cti_op_push_scope(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSObject* o = ARG_src1.toObject(ARG_callFrame);
    CHECK_FOR_EXCEPTION();
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->push(o));
    return o;
}

void Interpreter::cti_op_pop_scope(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->pop());
}

JSValueEncodedAsPointer* Interpreter::cti_op_typeof(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(jsTypeStringForValue(ARG_callFrame, ARG_src1));
}

JSValueEncodedAsPointer* Interpreter::cti_op_is_undefined(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr v = ARG_src1;
    return JSValuePtr::encode(jsBoolean(v.isCell() ? v.asCell()->structure()->typeInfo().masqueradesAsUndefined() : v.isUndefined()));
}

JSValueEncodedAsPointer* Interpreter::cti_op_is_boolean(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(jsBoolean(ARG_src1.isBoolean()));
}

JSValueEncodedAsPointer* Interpreter::cti_op_is_number(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(jsBoolean(ARG_src1.isNumber()));
}

JSValueEncodedAsPointer* Interpreter::cti_op_is_string(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(jsBoolean(ARG_globalData->interpreter->isJSString(ARG_src1)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_is_object(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(jsBoolean(jsIsObjectType(ARG_src1)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_is_function(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    return JSValuePtr::encode(jsBoolean(jsIsFunctionType(ARG_src1)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_stricteq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    return JSValuePtr::encode(jsBoolean(JSValuePtr::strictEqual(src1, src2)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_nstricteq(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    return JSValuePtr::encode(jsBoolean(!JSValuePtr::strictEqual(src1, src2)));
}

JSValueEncodedAsPointer* Interpreter::cti_op_to_jsnumber(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr src = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = src.toJSNumber(callFrame);
    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

JSValueEncodedAsPointer* Interpreter::cti_op_in(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr baseVal = ARG_src2;

    if (!baseVal.isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, STUB_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "in", baseVal, vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSValuePtr propName = ARG_src1;
    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName.getUInt32(i))
        return JSValuePtr::encode(jsBoolean(baseObj->hasProperty(callFrame, i)));

    Identifier property(callFrame, propName.toString(callFrame));
    CHECK_FOR_EXCEPTION();
    return JSValuePtr::encode(jsBoolean(baseObj->hasProperty(callFrame, property)));
}

JSObject* Interpreter::cti_op_push_new_scope(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSObject* scope = new (ARG_globalData) JSStaticScopeObject(ARG_callFrame, *ARG_id1, ARG_src2, DontDelete);

    CallFrame* callFrame = ARG_callFrame;
    callFrame->setScopeChain(callFrame->scopeChain()->push(scope));
    return scope;
}

void Interpreter::cti_op_jmp_scopes(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    unsigned count = ARG_int1;
    CallFrame* callFrame = ARG_callFrame;

    ScopeChainNode* tmp = callFrame->scopeChain();
    while (count--)
        tmp = tmp->pop();
    callFrame->setScopeChain(tmp);
}

void Interpreter::cti_op_put_by_index(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    unsigned property = ARG_int2;

    ARG_src1.put(callFrame, property, ARG_src3);
}

void* Interpreter::cti_op_switch_imm(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    if (scrutinee.isInt32Fast())
        return codeBlock->immediateSwitchJumpTable(tableIndex).ctiForValue(scrutinee.getInt32Fast());

    return codeBlock->immediateSwitchJumpTable(tableIndex).ctiDefault;
}

void* Interpreter::cti_op_switch_char(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->characterSwitchJumpTable(tableIndex).ctiDefault;

    if (scrutinee.isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        if (value->size() == 1)
            result = codeBlock->characterSwitchJumpTable(tableIndex).ctiForValue(value->data()[0]);
    }

    return result;
}

void* Interpreter::cti_op_switch_string(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    JSValuePtr scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->stringSwitchJumpTable(tableIndex).ctiDefault;

    if (scrutinee.isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        result = codeBlock->stringSwitchJumpTable(tableIndex).ctiForValue(value);
    }

    return result;
}

JSValueEncodedAsPointer* Interpreter::cti_op_del_by_val(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr baseValue = ARG_src1;
    JSObject* baseObj = baseValue.toObject(callFrame); // may throw

    JSValuePtr subscript = ARG_src2;
    JSValuePtr result;
    uint32_t i;
    if (subscript.getUInt32(i))
        result = jsBoolean(baseObj->deleteProperty(callFrame, i));
    else {
        CHECK_FOR_EXCEPTION();
        Identifier property(callFrame, subscript.toString(callFrame));
        CHECK_FOR_EXCEPTION();
        result = jsBoolean(baseObj->deleteProperty(callFrame, property));
    }

    CHECK_FOR_EXCEPTION_AT_END();
    return JSValuePtr::encode(result);
}

void Interpreter::cti_op_put_getter(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1.isObject());
    JSObject* baseObj = asObject(ARG_src1);
    ASSERT(ARG_src3.isObject());
    baseObj->defineGetter(callFrame, *ARG_id2, asObject(ARG_src3));
}

void Interpreter::cti_op_put_setter(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1.isObject());
    JSObject* baseObj = asObject(ARG_src1);
    ASSERT(ARG_src3.isObject());
    baseObj->defineSetter(callFrame, *ARG_id2, asObject(ARG_src3));
}

JSObject* Interpreter::cti_op_new_error(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned type = ARG_int1;
    JSValuePtr message = ARG_src2;
    unsigned bytecodeOffset = ARG_int3;

    unsigned lineNumber = codeBlock->lineNumberForBytecodeOffset(callFrame, bytecodeOffset);
    return Error::create(callFrame, static_cast<ErrorType>(type), message.toString(callFrame), lineNumber, codeBlock->ownerNode()->sourceID(), codeBlock->ownerNode()->sourceURL());
}

void Interpreter::cti_op_debug(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;

    int debugHookID = ARG_int1;
    int firstLine = ARG_int2;
    int lastLine = ARG_int3;

    ARG_globalData->interpreter->debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
}

JSValueEncodedAsPointer* Interpreter::cti_vm_throw(STUB_ARGS)
{
    BEGIN_STUB_FUNCTION();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalData* globalData = ARG_globalData;

    unsigned vPCIndex = codeBlock->getBytecodeIndex(callFrame, globalData->exceptionLocation);

    JSValuePtr exceptionValue = globalData->exception;
    ASSERT(exceptionValue);
    globalData->exception = noValue();

    HandlerInfo* handler = globalData->interpreter->throwException(callFrame, exceptionValue, vPCIndex, false);

    if (!handler) {
        *ARG_exception = exceptionValue;
        return JSValuePtr::encode(jsNull());
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = handler->nativeCode;
    ASSERT(catchRoutine);
    STUB_SET_RETURN_ADDRESS(catchRoutine);
    return JSValuePtr::encode(exceptionValue);
}

#undef STUB_RETURN_ADDRESS
#undef STUB_SET_RETURN_ADDRESS
#undef BEGIN_STUB_FUNCTION
#undef CHECK_FOR_EXCEPTION
#undef CHECK_FOR_EXCEPTION_AT_END
#undef CHECK_FOR_EXCEPTION_VOID
#undef VM_THROW_EXCEPTION
#undef VM_THROW_EXCEPTION_2
#undef VM_THROW_EXCEPTION_AT_END

#endif // ENABLE(JIT)

} // namespace JSC
