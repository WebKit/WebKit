/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "Machine.h"

#include "Arguments.h"
#include "BatchedTransitionOptimizer.h"
#include "CodeBlock.h"
#include "DebuggerCallFrame.h"
#include "EvalCodeCache.h"
#include "ExceptionHelpers.h"
#include "ExecState.h"
#include "GlobalEvalFunction.h"
#include "JSActivation.h"
#include "JSArray.h"
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
#include "CTI.h"
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

#if HAVE(COMPUTED_GOTO)
static void* op_throw_end_indirect;
static void* op_call_indirect;
#endif

#if ENABLE(JIT)

static ALWAYS_INLINE Instruction* vPCForPC(CodeBlock* codeBlock, void* pc)
{
    if (pc >= codeBlock->instructions.begin() && pc < codeBlock->instructions.end())
        return static_cast<Instruction*>(pc);

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(pc));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(pc);
    return codeBlock->instructions.begin() + vPCIndex;
}

#else // ENABLE(JIT)

static ALWAYS_INLINE Instruction* vPCForPC(CodeBlock*, void* pc)
{
    return static_cast<Instruction*>(pc);
}

#endif // ENABLE(JIT)

// Returns the depth of the scope chain within a given call frame.
static int depth(CodeBlock* codeBlock, ScopeChain& sc)
{
    if (!codeBlock->needsFullScopeChain)
        return 0;
    int scopeDepth = 0;
    ScopeChainIterator iter = sc.begin();
    ScopeChainIterator end = sc.end();
    while (!(*iter)->isObject(&JSActivation::info)) {
        ++iter;
        if (iter == end)
            break;
        ++scopeDepth;
    }
    return scopeDepth;
}

// FIXME: This operation should be called "getNumber", not "isNumber" (as it is in JSValue.h).
// FIXME: There's no need to have a "slow" version of this. All versions should be fast.
static ALWAYS_INLINE bool fastIsNumber(JSValue* value, double& arg)
{
    if (JSImmediate::isNumber(value))
        arg = JSImmediate::getTruncatedInt32(value);
    else if (LIKELY(!JSImmediate::isImmediate(value)) && LIKELY(Heap::isNumber(asCell(value))))
        arg = asNumberCell(value)->value();
    else
        return false;
    return true;
}

// FIXME: Why doesn't JSValue*::toInt32 have the Heap::isNumber optimization?
static bool fastToInt32(JSValue* value, int32_t& arg)
{
    if (JSImmediate::isNumber(value))
        arg = JSImmediate::getTruncatedInt32(value);
    else if (LIKELY(!JSImmediate::isImmediate(value)) && LIKELY(Heap::isNumber(asCell(value))))
        arg = asNumberCell(value)->toInt32();
    else
        return false;
    return true;
}

static ALWAYS_INLINE bool fastToUInt32(JSValue* value, uint32_t& arg)
{
    if (JSImmediate::isNumber(value)) {
        if (JSImmediate::getTruncatedUInt32(value, arg))
            return true;
        bool scratch;
        arg = toUInt32SlowCase(JSImmediate::getTruncatedInt32(value), scratch);
        return true;
    } else if (!JSImmediate::isImmediate(value) && Heap::isNumber(asCell(value)))
        arg = asNumberCell(value)->toUInt32();
    else
        return false;
    return true;
}

static inline bool jsLess(CallFrame* callFrame, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) < JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    if (fastIsNumber(v1, n1) && fastIsNumber(v2, n2))
        return n1 < n2;

    Interpreter* interpreter = callFrame->interpreter();
    if (interpreter->isJSString(v1) && interpreter->isJSString(v2))
        return asString(v1)->value() < asString(v2)->value();

    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(callFrame, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(callFrame, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return asString(p1)->value() < asString(p2)->value();
}

static inline bool jsLessEq(CallFrame* callFrame, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) <= JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    if (fastIsNumber(v1, n1) && fastIsNumber(v2, n2))
        return n1 <= n2;

    Interpreter* interpreter = callFrame->interpreter();
    if (interpreter->isJSString(v1) && interpreter->isJSString(v2))
        return !(asString(v2)->value() < asString(v1)->value());

    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(callFrame, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(callFrame, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(asString(p2)->value() < asString(p1)->value());
}

static NEVER_INLINE JSValue* jsAddSlowCase(CallFrame* callFrame, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue* p1 = v1->toPrimitive(callFrame);
    JSValue* p2 = v2->toPrimitive(callFrame);

    if (p1->isString() || p2->isString()) {
        RefPtr<UString::Rep> value = concatenate(p1->toString(callFrame).rep(), p2->toString(callFrame).rep());
        if (!value)
            return throwOutOfMemoryError(callFrame);
        return jsString(callFrame, value.release());
    }

    return jsNumber(callFrame, p1->toNumber(callFrame) + p2->toNumber(callFrame));
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5626160 Add case: 3 3 (of these, 3637690 are for immediate values)
//    247412  Add case: 5 5
//    20900   Add case: 5 6
//    13962   Add case: 5 3
//    4000    Add case: 3 5

static ALWAYS_INLINE JSValue* jsAdd(CallFrame* callFrame, JSValue* v1, JSValue* v2)
{
    double left;
    double right = 0.0;

    bool rightIsNumber = fastIsNumber(v2, right);
    if (rightIsNumber && fastIsNumber(v1, left))
        return jsNumber(callFrame, left + right);
    
    bool leftIsString = v1->isString();
    if (leftIsString && v2->isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (!value)
            return throwOutOfMemoryError(callFrame);
        return jsString(callFrame, value.release());
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = JSImmediate::isImmediate(v2) ?
            concatenate(asString(v1)->value().rep(), JSImmediate::getTruncatedInt32(v2)) :
            concatenate(asString(v1)->value().rep(), right);

        if (!value)
            return throwOutOfMemoryError(callFrame);
        return jsString(callFrame, value.release());
    }

    // All other cases are pretty uncommon
    return jsAddSlowCase(callFrame, v1, v2);
}

static JSValue* jsTypeStringForValue(CallFrame* callFrame, JSValue* v)
{
    if (v->isUndefined())
        return jsNontrivialString(callFrame, "undefined");
    if (v->isBoolean())
        return jsNontrivialString(callFrame, "boolean");
    if (v->isNumber())
        return jsNontrivialString(callFrame, "number");
    if (v->isString())
        return jsNontrivialString(callFrame, "string");
    if (v->isObject()) {
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

static bool jsIsObjectType(JSValue* v)
{
    if (JSImmediate::isImmediate(v))
        return v->isNull();

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

static bool jsIsFunctionType(JSValue* v)
{
    if (v->isObject()) {
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return true;
    }
    return false;
}

NEVER_INLINE bool Interpreter::resolve(CallFrame* callFrame, Instruction* vPC, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValue* result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame[dst] = result;
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC, codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveSkip(CallFrame* callFrame, Instruction* vPC, JSValue*& exceptionValue)
{
    CodeBlock* codeBlock = callFrame->codeBlock();

    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    int skip = (vPC + 3)->u.operand + codeBlock->needsFullScopeChain;

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(callFrame, ident, slot)) {
            JSValue* result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame[dst] = result;
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC, codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveGlobal(CallFrame* callFrame, Instruction* vPC, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>((vPC + 2)->u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int property = (vPC + 3)->u.operand;
    Structure* structure = (vPC + 4)->u.structure;
    int offset = (vPC + 5)->u.operand;

    if (structure == globalObject->structure()) {
        callFrame[dst] = globalObject->getDirectOffset(offset);
        return true;
    }

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifiers[property];
    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValue* result = slot.getValue(callFrame, ident);
        if (slot.isCacheable()) {
            if (vPC[4].u.structure)
                vPC[4].u.structure->deref();
            globalObject->structure()->ref();
            vPC[4] = globalObject->structure();
            vPC[5] = slot.cachedOffset();
            callFrame[dst] = result;
            return true;
        }

        exceptionValue = callFrame->globalData().exception;
        if (exceptionValue)
            return false;
        callFrame[dst] = result;
        return true;
    }

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC, codeBlock);
    return false;
}

static ALWAYS_INLINE JSValue* inlineResolveBase(CallFrame* callFrame, Identifier& property, ScopeChainNode* scopeChain)
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
    callFrame[dst] = inlineResolveBase(callFrame, callFrame->codeBlock()->identifiers[property], callFrame->scopeChain());
}

NEVER_INLINE bool Interpreter::resolveBaseAndProperty(CallFrame* callFrame, Instruction* vPC, JSValue*& exceptionValue)
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
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(callFrame, ident, slot)) {
            JSValue* result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;
            callFrame[propDst] = result;
            callFrame[baseDst] = base;
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC, codeBlock);
    return false;
}

NEVER_INLINE bool Interpreter::resolveBaseAndFunc(CallFrame* callFrame, Instruction* vPC, JSValue*& exceptionValue)
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
    Identifier& ident = codeBlock->identifiers[property];
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
            JSValue* result = slot.getValue(callFrame, ident);
            exceptionValue = callFrame->globalData().exception;
            if (exceptionValue)
                return false;

            callFrame[baseDst] = thisObj;
            callFrame[funcDst] = result;
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(callFrame, ident, vPC, codeBlock);
    return false;
}

ALWAYS_INLINE CallFrame* Interpreter::slideRegisterWindowForCall(CodeBlock* newCodeBlock, RegisterFile* registerFile, CallFrame* callFrame, size_t registerOffset, int argc)
{
    Register* r = callFrame->registers();
    Register* newEnd = r + registerOffset + newCodeBlock->numCalleeRegisters;

    if (LIKELY(argc == newCodeBlock->numParameters)) { // correct number of arguments
        if (UNLIKELY(!registerFile->grow(newEnd)))
            return 0;
        r += registerOffset;
    } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
        size_t omittedArgCount = newCodeBlock->numParameters - argc;
        registerOffset += omittedArgCount;
        newEnd += omittedArgCount;
        if (!registerFile->grow(newEnd))
            return 0;
        r += registerOffset;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();
    } else { // too many arguments -- copy expected arguments, leaving the extra arguments behind
        size_t numParameters = newCodeBlock->numParameters;
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

static NEVER_INLINE bool isNotObject(CallFrame* callFrame, bool forInstanceOf, CodeBlock* codeBlock, const Instruction* vPC, JSValue* value, JSValue*& exceptionData)
{
    if (value->isObject())
        return false;
    exceptionData = createInvalidParamError(callFrame, forInstanceOf ? "instanceof" : "in" , value, vPC, codeBlock);
    return true;
}

NEVER_INLINE JSValue* Interpreter::callEval(CallFrame* callFrame, RegisterFile* registerFile, Register* argv, int argc, int registerOffset, JSValue*& exceptionValue)
{
    if (argc < 2)
        return jsUndefined();

    JSValue* program = argv[1].jsValue(callFrame);

    if (!program->isString())
        return program;

    UString programSource = asString(program)->value();

    ScopeChainNode* scopeChain = callFrame->scopeChain();
    CodeBlock* codeBlock = callFrame->codeBlock();
    RefPtr<EvalNode> evalNode = codeBlock->evalCodeCache.get(callFrame, programSource, scopeChain, exceptionValue);

    JSValue* result = jsUndefined();
    if (evalNode)
        result = callFrame->globalData().interpreter->execute(evalNode.get(), callFrame, callFrame->thisValue()->toThisObject(callFrame), callFrame->registers() - registerFile->start() + registerOffset, scopeChain, &exceptionValue);

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
    , m_assemblerBuffer(new AssemblerBuffer(1024 * 1024))
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
#if ENABLE(JIT)
    JIT::freeCTIMachineTrampolines(this);
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
    printf("----------------------------------------------------\n");
    printf("            use            |   address  |   value   \n");
    printf("----------------------------------------------------\n");

    CodeBlock* codeBlock = callFrame->codeBlock();
    RegisterFile* registerFile = &callFrame->scopeChain()->globalObject()->globalData()->interpreter->registerFile();
    const Register* it;
    const Register* end;

    if (codeBlock->codeType == GlobalCode) {
        it = registerFile->lastGlobal();
        end = it + registerFile->numGlobals();
        while (it != end) {
            printf("[global var]               | %10p | %10p \n", it, (*it).v());
            ++it;
        }
        printf("----------------------------------------------------\n");
    }
    
    it = callFrame->registers() - RegisterFile::CallFrameHeaderSize - codeBlock->numParameters;
    printf("[this]                     | %10p | %10p \n", it, (*it).v()); ++it;
    end = it + max(codeBlock->numParameters - 1, 0); // - 1 to skip "this"
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

    end = it + codeBlock->numVars;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    end = it + codeBlock->numConstants;
    if (it != end) {
        do {
            printf("[r%2d]                      | %10p | %10p \n", registerCount, it, (*it).v());
            ++it;
            ++registerCount;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    end = it + codeBlock->numCalleeRegisters - codeBlock->numConstants - codeBlock->numVars;
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

NEVER_INLINE bool Interpreter::unwindCallFrame(CallFrame*& callFrame, JSValue* exceptionValue, const Instruction*& vPC, CodeBlock*& codeBlock)
{
    CodeBlock* oldCodeBlock = codeBlock;
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        if (callFrame->callee())
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->lastLine());
        else
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->lastLine());
    }

    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
        if (callFrame->callee())
            profiler->didExecute(callFrame, callFrame->callee());
        else
            profiler->didExecute(callFrame, codeBlock->ownerNode->sourceURL(), codeBlock->ownerNode->lineNo());
    }

    // If this call frame created an activation or an 'arguments' object, tear it off.
    if (oldCodeBlock->codeType == FunctionCode && oldCodeBlock->needsFullScopeChain) {
        while (!scopeChain->object->isObject(&JSActivation::info))
            scopeChain = scopeChain->pop();
        static_cast<JSActivation*>(scopeChain->object)->copyRegisters(callFrame->optionalCalleeArguments());
    } else if (Arguments* arguments = callFrame->optionalCalleeArguments()) {
        if (!arguments->isTornOff())
            arguments->copyRegisters();
    }

    if (oldCodeBlock->needsFullScopeChain)
        scopeChain->deref();

    void* returnPC = callFrame->returnPC();
    callFrame = callFrame->callerFrame();
    if (callFrame->hasHostCallFrameFlag())
        return false;

    codeBlock = callFrame->codeBlock();
    vPC = vPCForPC(codeBlock, returnPC);
    return true;
}

NEVER_INLINE Instruction* Interpreter::throwException(CallFrame*& callFrame, JSValue*& exceptionValue, const Instruction* vPC, bool explicitThrow)
{
    // Set up the exception object
    
    CodeBlock* codeBlock = callFrame->codeBlock();
    if (exceptionValue->isObject()) {
        JSObject* exception = asObject(exceptionValue);
        if (exception->isNotAnObjectErrorStub()) {
            exception = createNotAnObjectError(callFrame, static_cast<JSNotAnObjectErrorStub*>(exception), vPC, codeBlock);
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
                    int line = codeBlock->expressionRangeForVPC(vPC, divotPoint, startOffset, endOffset);
                    exception->putWithAttributes(callFrame, Identifier(callFrame, "line"), jsNumber(callFrame, line), ReadOnly | DontDelete);
                    
                    // We only hit this path for error messages and throw statements, which don't have a specific failure position
                    // So we just give the full range of the error/throw statement.
                    exception->putWithAttributes(callFrame, Identifier(callFrame, expressionBeginOffsetPropertyName), jsNumber(callFrame, divotPoint - startOffset), ReadOnly | DontDelete);
                    exception->putWithAttributes(callFrame, Identifier(callFrame, expressionEndOffsetPropertyName), jsNumber(callFrame, divotPoint + endOffset), ReadOnly | DontDelete);
                } else
                    exception->putWithAttributes(callFrame, Identifier(callFrame, "line"), jsNumber(callFrame, codeBlock->lineNumberForVPC(vPC)), ReadOnly | DontDelete);
                exception->putWithAttributes(callFrame, Identifier(callFrame, "sourceId"), jsNumber(callFrame, codeBlock->ownerNode->sourceID()), ReadOnly | DontDelete);
                exception->putWithAttributes(callFrame, Identifier(callFrame, "sourceURL"), jsOwnedString(callFrame, codeBlock->ownerNode->sourceURL()), ReadOnly | DontDelete);
            }
            
            if (exception->isWatchdogException()) {
                while (unwindCallFrame(callFrame, exceptionValue, vPC, codeBlock)) {
                    // Don't need handler checks or anything, we just want to unroll all the JS callframes possible.
                }
                return 0;
            }
        }
    }

    if (Debugger* debugger = callFrame->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(callFrame, exceptionValue);
        debugger->exception(debuggerCallFrame, codeBlock->ownerNode->sourceID(), codeBlock->lineNumberForVPC(vPC));
    }

    // If we throw in the middle of a call instruction, we need to notify
    // the profiler manually that the call instruction has returned, since
    // we'll never reach the relevant op_profile_did_call.
    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
        if (isCallBytecode(vPC[0].u.opcode))
            profiler->didExecute(callFrame, callFrame[vPC[2].u.operand].jsValue(callFrame));
        else if (vPC[8].u.opcode == getOpcode(op_construct))
            profiler->didExecute(callFrame, callFrame[vPC[10].u.operand].jsValue(callFrame));
    }

    // Calculate an exception handler vPC, unwinding call frames as necessary.

    int scopeDepth;
    Instruction* handlerVPC;

    while (!codeBlock->getHandlerForVPC(vPC, handlerVPC, scopeDepth)) {
        if (!unwindCallFrame(callFrame, exceptionValue, vPC, codeBlock))
            return 0;
    }

    // Now unwind the scope chain within the exception handler's call frame.

    ScopeChain sc(callFrame->scopeChain());
    int scopeDelta = depth(codeBlock, sc) - scopeDepth;
    ASSERT(scopeDelta >= 0);
    while (scopeDelta--)
        sc.pop();
    callFrame->setScopeChain(sc.node());

    return handlerVPC;
}

class DynamicGlobalObjectScope : Noncopyable {
public:
    DynamicGlobalObjectScope(CallFrame* callFrame, JSGlobalObject* dynamicGlobalObject) 
        : m_dynamicGlobalObjectSlot(callFrame->globalData().dynamicGlobalObject)
        , m_savedDynamicGlobalObject(m_dynamicGlobalObjectSlot)
    {
        m_dynamicGlobalObjectSlot = dynamicGlobalObject;
    }

    ~DynamicGlobalObjectScope()
    {
        m_dynamicGlobalObjectSlot = m_savedDynamicGlobalObject;
    }

private:
    JSGlobalObject*& m_dynamicGlobalObjectSlot;
    JSGlobalObject* m_savedDynamicGlobalObject;
};

JSValue* Interpreter::execute(ProgramNode* programNode, CallFrame* callFrame, ScopeChainNode* scopeChain, JSObject* thisObj, JSValue** exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    CodeBlock* codeBlock = &programNode->bytecode(scopeChain);

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = oldEnd + codeBlock->numParameters + RegisterFile::CallFrameHeaderSize + codeBlock->numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, scopeChain->globalObject());

    JSGlobalObject* lastGlobalObject = m_registerFile.globalObject();
    JSGlobalObject* globalObject = callFrame->dynamicGlobalObject();
    globalObject->copyGlobalsTo(m_registerFile);

    CallFrame* newCallFrame = CallFrame::create(oldEnd + codeBlock->numParameters + RegisterFile::CallFrameHeaderSize);
    newCallFrame[codeBlock->thisRegister] = thisObj;
    newCallFrame->init(codeBlock, 0, scopeChain, CallFrame::noCaller(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain)
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, programNode->sourceURL(), programNode->lineNo());

    JSValue* result;
    {
        SamplingTool::CallRecord callRecord(m_sampler);

        m_reentryDepth++;
#if ENABLE(JIT)
        if (!codeBlock->ctiCode)
            JIT::compile(scopeChain->globalData, codeBlock);
        result = JIT::execute(codeBlock->ctiCode, &m_registerFile, newCallFrame, scopeChain->globalData, exception);
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

JSValue* Interpreter::execute(FunctionBodyNode* functionBodyNode, CallFrame* callFrame, JSFunction* function, JSObject* thisObj, const ArgList& args, ScopeChainNode* scopeChain, JSValue** exception)
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
    newCallFrame[0] = thisObj;
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

    JSValue* result;
    {
        SamplingTool::CallRecord callRecord(m_sampler);

        m_reentryDepth++;
#if ENABLE(JIT)
        if (!codeBlock->ctiCode)
            JIT::compile(scopeChain->globalData, codeBlock);
        result = JIT::execute(codeBlock->ctiCode, &m_registerFile, newCallFrame, scopeChain->globalData, exception);
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

JSValue* Interpreter::execute(EvalNode* evalNode, CallFrame* callFrame, JSObject* thisObj, ScopeChainNode* scopeChain, JSValue** exception)
{
    return execute(evalNode, callFrame, thisObj, m_registerFile.size() + evalNode->bytecode(scopeChain).numParameters + RegisterFile::CallFrameHeaderSize, scopeChain, exception);
}

JSValue* Interpreter::execute(EvalNode* evalNode, CallFrame* callFrame, JSObject* thisObj, int globalRegisterOffset, ScopeChainNode* scopeChain, JSValue** exception)
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

        const DeclarationStacks::VarStack& varStack = codeBlock->ownerNode->varStack();
        DeclarationStacks::VarStack::const_iterator varStackEnd = varStack.end();
        for (DeclarationStacks::VarStack::const_iterator it = varStack.begin(); it != varStackEnd; ++it) {
            const Identifier& ident = (*it).first;
            if (!variableObject->hasProperty(callFrame, ident)) {
                PutPropertySlot slot;
                variableObject->put(callFrame, ident, jsUndefined(), slot);
            }
        }

        const DeclarationStacks::FunctionStack& functionStack = codeBlock->ownerNode->functionStack();
        DeclarationStacks::FunctionStack::const_iterator functionStackEnd = functionStack.end();
        for (DeclarationStacks::FunctionStack::const_iterator it = functionStack.begin(); it != functionStackEnd; ++it) {
            PutPropertySlot slot;
            variableObject->put(callFrame, (*it)->m_ident, (*it)->makeFunction(callFrame, scopeChain), slot);
        }

    }

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = m_registerFile.start() + globalRegisterOffset + codeBlock->numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    CallFrame* newCallFrame = CallFrame::create(m_registerFile.start() + globalRegisterOffset);

    // a 0 codeBlock indicates a built-in caller
    newCallFrame[codeBlock->thisRegister] = thisObj;
    newCallFrame->init(codeBlock, 0, scopeChain, callFrame->addHostCallFrameFlag(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain)
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, evalNode->sourceURL(), evalNode->lineNo());

    JSValue* result;
    {
        SamplingTool::CallRecord callRecord(m_sampler);

        m_reentryDepth++;
#if ENABLE(JIT)
        if (!codeBlock->ctiCode)
            JIT::compile(scopeChain->globalData, codeBlock);
        result = JIT::execute(codeBlock->ctiCode, &m_registerFile, newCallFrame, scopeChain->globalData, exception);
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
            debugger->callEvent(callFrame, callFrame->codeBlock()->ownerNode->sourceID(), firstLine);
            return;
        case WillLeaveCallFrame:
            debugger->returnEvent(callFrame, callFrame->codeBlock()->ownerNode->sourceID(), lastLine);
            return;
        case WillExecuteStatement:
            debugger->atStatement(callFrame, callFrame->codeBlock()->ownerNode->sourceID(), firstLine);
            return;
        case WillExecuteProgram:
            debugger->willExecuteProgram(callFrame, callFrame->codeBlock()->ownerNode->sourceID(), firstLine);
            return;
        case DidExecuteProgram:
            debugger->didExecuteProgram(callFrame, callFrame->codeBlock()->ownerNode->sourceID(), lastLine);
            return;
        case DidReachBreakpoint:
            debugger->didReachBreakpoint(callFrame, callFrame->codeBlock()->ownerNode->sourceID(), lastLine);
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
    thread_info(mach_thread_self(), THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &infoCount);
    
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
ALWAYS_INLINE JSValue* Interpreter::checkTimeout(JSGlobalObject* globalObject)
{
    unsigned currentTime = getCPUTime();
    
    if (!m_timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        m_timeAtLastCheckTimeout = currentTime;
        return noValue();
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
            return jsNull(); // Appeasing GCC, all we need is a non-null js value.
        
        resetTimeoutCheck();
    }
    
    return noValue();
}

NEVER_INLINE ScopeChainNode* Interpreter::createExceptionScope(CallFrame* callFrame, const Instruction* vPC)
{
    int dst = (++vPC)->u.operand;
    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& property = codeBlock->identifiers[(++vPC)->u.operand];
    JSValue* value = callFrame[(++vPC)->u.operand].jsValue(callFrame);
    JSObject* scope = new (callFrame) JSStaticScopeObject(callFrame, property, value, DontDelete);
    callFrame[dst] = scope;

    return callFrame->scopeChain()->push(scope);
}

static StructureChain* cachePrototypeChain(CallFrame* callFrame, Structure* structure)
{
    JSValue* prototype = structure->prototypeForLookup(callFrame);
    if (JSImmediate::isImmediate(prototype))
        return 0;
    RefPtr<StructureChain> chain = StructureChain::create(asObject(prototype)->structure());
    structure->setCachedPrototypeChain(chain.release());
    return structure->cachedPrototypeChain();
}

NEVER_INLINE void Interpreter::tryCachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValue* baseValue, const PutPropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_put_by_id))
        return;

    if (JSImmediate::isImmediate(baseValue))
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

NEVER_INLINE void Interpreter::tryCacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValue* baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // Recursive invocation may already have specialized this instruction.
    if (vPC[0].u.opcode != getOpcode(op_get_by_id))
        return;

    // FIXME: Cache property access for immediates.
    if (JSImmediate::isImmediate(baseValue)) {
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
        ASSERT(slot.slotBase()->isObject());

        JSObject* baseObject = asObject(slot.slotBase());

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
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

    size_t count = 0;
    JSObject* o = asObject(baseValue);
    while (slot.slotBase() != o) {
        JSValue* v = o->structure()->prototypeForLookup(callFrame);

        // If we didn't find base in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.
        if (v->isNull()) {
            vPC[0] = getOpcode(op_get_by_id_generic);
            return;
        }

        o = asObject(v);

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (o->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(o->structure());
            o->setStructure(transition.release());
            asObject(baseValue)->structure()->setCachedPrototypeChain(0);
        }

        ++count;
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

JSValue* Interpreter::privateExecute(ExecutionFlag flag, RegisterFile* registerFile, CallFrame* callFrame, JSValue** exception)
{
    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function.
    if (flag == InitializeAndReturn) {
        #if HAVE(COMPUTED_GOTO)
            #define ADD_BYTECODE(id) m_opcodeTable[id] = &&id;
                FOR_EACH_OPCODE_ID(ADD_BYTECODE);
            #undef ADD_BYTECODE

            #define ADD_OPCODE_ID(id) m_opcodeIDTable.add(&&id, id);
                FOR_EACH_OPCODE_ID(ADD_OPCODE_ID);
            #undef ADD_OPCODE_ID
            ASSERT(m_opcodeIDTable.size() == numOpcodeIDs);
            op_throw_end_indirect = &&op_throw_end;
            op_call_indirect = &&op_call;
        #endif // HAVE(COMPUTED_GOTO)
        return noValue();
    }

#if ENABLE(JIT)
    // Currently with CTI enabled we never interpret functions
    ASSERT_NOT_REACHED();
#endif

    JSGlobalData* globalData = &callFrame->globalData();
    JSValue* exceptionValue = noValue();
    Instruction* handlerVPC = 0;

    Instruction* vPC = callFrame->codeBlock()->instructions.begin();
    Profiler** enabledProfilerReference = Profiler::enabledProfilerReference();
    unsigned tickCount = m_ticksUntilNextTimeoutCheck + 1;

#define VM_CHECK_EXCEPTION() \
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
        if ((exceptionValue = checkTimeout(callFrame->dynamicGlobalObject()))) \
            goto vm_throw; \
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
    #define NEXT_INSTRUCTION SAMPLE(callFrame->codeBlock(), vPC); goto *vPC->u.opcode
#if ENABLE(OPCODE_STATS)
    #define DEFINE_OPCODE(opcode) opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define DEFINE_OPCODE(opcode) opcode:
#endif
    NEXT_INSTRUCTION;
#else
    #define NEXT_INSTRUCTION SAMPLE(callFrame->codeBlock(), vPC); goto interpreterLoopStart
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
        callFrame[dst] = constructEmptyObject(callFrame);

        ++vPC;
        NEXT_INSTRUCTION;
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
        callFrame[dst] = constructArray(callFrame, args);

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int regExp = (++vPC)->u.operand;
        callFrame[dst] = new (globalData) RegExpObject(callFrame->scopeChain()->globalObject()->regExpStructure(), callFrame->codeBlock()->regexps[regExp]);

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = callFrame[src];

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = jsBoolean(src1 == src2);
        else {
            JSValue* result = jsBoolean(equalSlowCase(callFrame, src1, src2));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_eq_null) {
        /* eq_null dst(r) src(r)

           Checks whether register src is null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (src->isUndefinedOrNull()) {
            callFrame[dst] = jsBoolean(true);
            ++vPC;
            NEXT_INSTRUCTION;
        }
        
        callFrame[dst] = jsBoolean(!JSImmediate::isImmediate(src) && src->asCell()->structure()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = jsBoolean(src1 != src2);
        else {
            JSValue* result = jsBoolean(!equalSlowCase(callFrame, src1, src2));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_neq_null) {
        /* neq_null dst(r) src(r)

           Checks whether register src is not null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (src->isUndefinedOrNull()) {
            callFrame[dst] = jsBoolean(false);
            ++vPC;
            NEXT_INSTRUCTION;
        }
        
        callFrame[dst] = jsBoolean(JSImmediate::isImmediate(src) || !asCell(src)->structure()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediate(src1, src2))
            callFrame[dst] = jsBoolean(src1 == src2);
        else if (JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate()))
            callFrame[dst] = jsBoolean(false);
        else
            callFrame[dst] = jsBoolean(strictEqualSlowCase(src1, src2));

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (JSImmediate::areBothImmediate(src1, src2))
            callFrame[dst] = jsBoolean(src1 != src2);
        else if (JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate()))
            callFrame[dst] = jsBoolean(true);
        else
            callFrame[dst] = jsBoolean(!strictEqualSlowCase(src1, src2));

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* result = jsBoolean(jsLess(callFrame, src1, src2));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* result = jsBoolean(jsLessEq(callFrame, src1, src2));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValue* v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            callFrame[srcDst] = JSImmediate::incImmediateNumber(v);
        else {
            JSValue* result = jsNumber(callFrame, v->toNumber(callFrame) + 1);
            VM_CHECK_EXCEPTION();
            callFrame[srcDst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValue* v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            callFrame[srcDst] = JSImmediate::decImmediateNumber(v);
        else {
            JSValue* result = jsNumber(callFrame, v->toNumber(callFrame) - 1);
            VM_CHECK_EXCEPTION();
            callFrame[srcDst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValue* v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            callFrame[dst] = v;
            callFrame[srcDst] = JSImmediate::incImmediateNumber(v);
        } else {
            JSValue* number = callFrame[srcDst].jsValue(callFrame)->toJSNumber(callFrame);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = number;
            callFrame[srcDst] = jsNumber(callFrame, number->uncheckedGetNumber() + 1);
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValue* v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            callFrame[dst] = v;
            callFrame[srcDst] = JSImmediate::decImmediateNumber(v);
        } else {
            JSValue* number = callFrame[srcDst].jsValue(callFrame)->toJSNumber(callFrame);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = number;
            callFrame[srcDst] = jsNumber(callFrame, number->uncheckedGetNumber() - 1);
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;

        JSValue* srcVal = callFrame[src].jsValue(callFrame);

        if (LIKELY(srcVal->isNumber()))
            callFrame[dst] = callFrame[src];
        else {
            JSValue* result = srcVal->toJSNumber(callFrame);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        ++vPC;
        double v;
        if (fastIsNumber(src, v))
            callFrame[dst] = jsNumber(callFrame, -v);
        else {
            JSValue* result = jsNumber(callFrame, -src->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            callFrame[dst] = JSImmediate::addImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsAdd(callFrame, src1, src2);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        vPC += 2;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double left;
        double right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2)) {
            int32_t left = JSImmediate::getTruncatedInt32(src1);
            int32_t right = JSImmediate::getTruncatedInt32(src2);
            if ((left | right) >> 15 == 0)
                callFrame[dst] = jsNumber(callFrame, left * right);
            else
                callFrame[dst] = jsNumber(callFrame, static_cast<double>(left) * static_cast<double>(right));
        } else if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
            callFrame[dst] = jsNumber(callFrame, left * right);
        else {
            JSValue* result = jsNumber(callFrame, src1->toNumber(callFrame) * src2->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_div) {
        /* div dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by the
           register divisor (converted to number), and puts the
           quotient in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* dividend = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* divisor = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double left;
        double right;
        if (fastIsNumber(dividend, left) && fastIsNumber(divisor, right))
            callFrame[dst] = jsNumber(callFrame, left / right);
        else {
            JSValue* result = jsNumber(callFrame, dividend->toNumber(callFrame) / divisor->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        ++vPC;
        NEXT_INSTRUCTION;
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

        JSValue* dividendValue = callFrame[dividend].jsValue(callFrame);
        JSValue* divisorValue = callFrame[divisor].jsValue(callFrame);

        if (JSImmediate::areBothImmediateNumbers(dividendValue, divisorValue) && divisorValue != JSImmediate::from(0)) {
            callFrame[dst] = JSImmediate::from(JSImmediate::getTruncatedInt32(dividendValue) % JSImmediate::getTruncatedInt32(divisorValue));
            ++vPC;
            NEXT_INSTRUCTION;
        }

        double d = dividendValue->toNumber(callFrame);
        JSValue* result = jsNumber(callFrame, fmod(d, divisorValue->toNumber(callFrame)));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_sub) {
        /* sub dst(r) src1(r) src2(r)

           Subtracts register src2 (converted to number) from register
           src1 (converted to number), and puts the difference in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double left;
        double right;
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            callFrame[dst] = JSImmediate::subImmediateNumbers(src1, src2);
        else if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
            callFrame[dst] = jsNumber(callFrame, left - right);
        else {
            JSValue* result = jsNumber(callFrame, src1->toNumber(callFrame) - src2->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        vPC += 2;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_lshift) {
        /* lshift dst(r) val(r) shift(r)

           Performs left shift of register val (converted to int32) by
           register shift (converted to uint32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        uint32_t right;
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            callFrame[dst] = jsNumber(callFrame, JSImmediate::getTruncatedInt32(val) << (JSImmediate::getTruncatedUInt32(shift) & 0x1f));
        else if (fastToInt32(val, left) && fastToUInt32(shift, right))
            callFrame[dst] = jsNumber(callFrame, left << (right & 0x1f));
        else {
            JSValue* result = jsNumber(callFrame, (val->toInt32(callFrame)) << (shift->toUInt32(callFrame) & 0x1f));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_rshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs arithmetic right shift of register val (converted
           to int32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        uint32_t right;
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            callFrame[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else if (fastToInt32(val, left) && fastToUInt32(shift, right))
            callFrame[dst] = jsNumber(callFrame, left >> (right & 0x1f));
        else {
            JSValue* result = jsNumber(callFrame, (val->toInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
            callFrame[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else {
            JSValue* result = jsNumber(callFrame, (val->toUInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_bitand) {
        /* bitand dst(r) src1(r) src2(r)

           Computes bitwise AND of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        int32_t right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = JSImmediate::andImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            callFrame[dst] = jsNumber(callFrame, left & right);
        else {
            JSValue* result = jsNumber(callFrame, src1->toInt32(callFrame) & src2->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_bitxor) {
        /* bitxor dst(r) src1(r) src2(r)

           Computes bitwise XOR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        int32_t right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = JSImmediate::xorImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            callFrame[dst] = jsNumber(callFrame, left ^ right);
        else {
            JSValue* result = jsNumber(callFrame, src1->toInt32(callFrame) ^ src2->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_bitor) {
        /* bitor dst(r) src1(r) src2(r)

           Computes bitwise OR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t left;
        int32_t right;
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = JSImmediate::orImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            callFrame[dst] = jsNumber(callFrame, left | right);
        else {
            JSValue* result = jsNumber(callFrame, src1->toInt32(callFrame) | src2->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t value;
        if (fastToInt32(src, value))
            callFrame[dst] = jsNumber(callFrame, ~value);
        else {
            JSValue* result = jsNumber(callFrame, ~src->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_not) {
        /* not dst(r) src(r)

           Computes logical NOT of register src (converted to
           boolean), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = jsBoolean(!callFrame[src].jsValue(callFrame)->toBoolean(callFrame));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_INSTRUCTION;
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

        JSValue* baseVal = callFrame[base].jsValue(callFrame);

        if (isNotObject(callFrame, true, callFrame->codeBlock(), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = asObject(baseVal);
        callFrame[dst] = jsBoolean(baseObj->structure()->typeInfo().implementsHasInstance() ? baseObj->hasInstance(callFrame, callFrame[value].jsValue(callFrame), callFrame[baseProto].jsValue(callFrame)) : false);

        vPC += 5;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsTypeStringForValue(callFrame, callFrame[src].jsValue(callFrame));

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_is_undefined) {
        /* is_undefined dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "undefined", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* v = callFrame[src].jsValue(callFrame);
        callFrame[dst] = jsBoolean(JSImmediate::isImmediate(v) ? v->isUndefined() : v->asCell()->structure()->typeInfo().masqueradesAsUndefined());

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_is_boolean) {
        /* is_boolean dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "boolean", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame)->isBoolean());

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_is_number) {
        /* is_number dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "number", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame)->isNumber());

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_is_string) {
        /* is_string dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "string", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame)->isString());

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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

        JSValue* baseVal = callFrame[base].jsValue(callFrame);
        if (isNotObject(callFrame, false, callFrame->codeBlock(), vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = asObject(baseVal);

        JSValue* propName = callFrame[property].jsValue(callFrame);

        uint32_t i;
        if (propName->getUInt32(i))
            callFrame[dst] = jsBoolean(baseObj->hasProperty(callFrame, i));
        else {
            Identifier property(callFrame, propName->toString(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = jsBoolean(baseObj->hasProperty(callFrame, property));
        }

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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

        NEXT_INSTRUCTION;
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
        
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_put_global_var) {
        /* put_global_var globalObject(c) index(n) value(r)
         
           Puts value into global slot index.
         */
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        
        scope->registerAt(index) = callFrame[value].jsValue(callFrame);
        ++vPC;
        NEXT_INSTRUCTION;
    }            
    DEFINE_OPCODE(op_get_scoped_var) {
        /* get_scoped_var dst(r) index(n) skip(n)

         Loads the contents of the index-th local from the scope skip nodes from
         the top of the scope chain, and places it in register dst
         */
        int dst = (++vPC)->u.operand;
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + callFrame->codeBlock()->needsFullScopeChain;

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
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_put_scoped_var) {
        /* put_scoped_var index(n) skip(n) value(r)

         */
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + callFrame->codeBlock()->needsFullScopeChain;
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
        scope->registerAt(index) = callFrame[value].jsValue(callFrame);
        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        Identifier& ident = codeBlock->identifiers[property];
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        PropertySlot slot(baseValue);
        JSValue* result = baseValue->get(callFrame, ident, slot);
        VM_CHECK_EXCEPTION();

        tryCacheGetByID(callFrame, codeBlock, vPC, baseValue, ident, slot);

        callFrame[dst] = result;
        vPC += 8;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_get_by_id_self) {
        /* op_get_by_id_self dst(r) base(r) property(id) structure(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base. If the cache misses, op_get_by_id_self reverts to
           op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue* baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int dst = vPC[1].u.operand;
                int offset = vPC[5].u.operand;

                ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifiers[vPC[3].u.operand]) == baseObject->getDirectOffset(offset));
                callFrame[dst] = baseObject->getDirectOffset(offset);

                vPC += 8;
                NEXT_INSTRUCTION;
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_get_by_id_proto) {
        /* op_get_by_id_proto dst(r) base(r) property(id) structure(sID) prototypeStructure(sID) offset(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype. If the cache misses, op_get_by_id_proto
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue* baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(structure->prototypeForLookup(callFrame)->isObject());
                JSObject* protoObject = asObject(structure->prototypeForLookup(callFrame));
                Structure* prototypeStructure = vPC[5].u.structure;

                if (LIKELY(protoObject->structure() == prototypeStructure)) {
                    int dst = vPC[1].u.operand;
                    int offset = vPC[6].u.operand;

                    ASSERT(protoObject->get(callFrame, callFrame->codeBlock()->identifiers[vPC[3].u.operand]) == protoObject->getDirectOffset(offset));
                    callFrame[dst] = protoObject->getDirectOffset(offset);

                    vPC += 8;
                    NEXT_INSTRUCTION;
                }
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_get_by_id_chain) {
        /* op_get_by_id_chain dst(r) base(r) property(id) structure(sID) structureChain(chain) count(n) offset(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype chain. If the cache misses, op_get_by_id_chain
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValue* baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                RefPtr<Structure>* it = vPC[5].u.structureChain->head();
                size_t count = vPC[6].u.operand;
                RefPtr<Structure>* end = it + count;

                JSObject* baseObject = asObject(baseCell);
                while (1) {
                    baseObject = asObject(baseObject->structure()->prototypeForLookup(callFrame));
                    if (UNLIKELY(baseObject->structure() != (*it).get()))
                        break;

                    if (++it == end) {
                        int dst = vPC[1].u.operand;
                        int offset = vPC[7].u.operand;

                        ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifiers[vPC[3].u.operand]) == baseObject->getDirectOffset(offset));
                        callFrame[dst] = baseObject->getDirectOffset(offset);

                        vPC += 8;
                        NEXT_INSTRUCTION;
                    }
                }
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_get_by_id_generic) {
        /* op_get_by_id_generic dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        PropertySlot slot(baseValue);
        JSValue* result = baseValue->get(callFrame, ident, slot);
        VM_CHECK_EXCEPTION();

        callFrame[dst] = result;
        vPC += 8;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_get_array_length) {
        /* op_get_array_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the array in register base,
           and puts the result in register dst. If register base does not hold
           an array, op_get_array_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        if (LIKELY(isJSArray(baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame[dst] = jsNumber(callFrame, asArray(baseValue)->length());
            vPC += 8;
            NEXT_INSTRUCTION;
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_get_string_length) {
        /* op_get_string_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the string in register base,
           and puts the result in register dst. If register base does not hold
           a string, op_get_string_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        if (LIKELY(isJSString(baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame[dst] = jsNumber(callFrame, asString(baseValue)->value().size());
            vPC += 8;
            NEXT_INSTRUCTION;
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
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
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        Identifier& ident = codeBlock->identifiers[property];
        PutPropertySlot slot;
        baseValue->put(callFrame, ident, callFrame[value].jsValue(callFrame), slot);
        VM_CHECK_EXCEPTION();

        tryCachePutByID(callFrame, codeBlock, vPC, baseValue, slot);

        vPC += 8;
        NEXT_INSTRUCTION;
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
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        
        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            Structure* oldStructure = vPC[4].u.structure;
            Structure* newStructure = vPC[5].u.structure;
            
            if (LIKELY(baseCell->structure() == oldStructure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);

                RefPtr<Structure>* it = vPC[6].u.structureChain->head();

                JSValue* proto = baseObject->structure()->prototypeForLookup(callFrame);
                while (!proto->isNull()) {
                    if (UNLIKELY(asObject(proto)->structure() != (*it).get())) {
                        uncachePutByID(callFrame->codeBlock(), vPC);
                        NEXT_INSTRUCTION;
                    }
                    ++it;
                    proto = asObject(proto)->structure()->prototypeForLookup(callFrame);
                }

                baseObject->transitionTo(newStructure);

                int value = vPC[3].u.operand;
                unsigned offset = vPC[7].u.operand;
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifiers[vPC[2].u.operand])) == offset);
                baseObject->putDirectOffset(offset, callFrame[value].jsValue(callFrame));

                vPC += 8;
                NEXT_INSTRUCTION;
            }
        }
        
        uncachePutByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
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
        JSValue* baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            Structure* structure = vPC[4].u.structure;

            if (LIKELY(baseCell->structure() == structure)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int value = vPC[3].u.operand;
                unsigned offset = vPC[5].u.operand;
                
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifiers[vPC[2].u.operand])) == offset);
                baseObject->putDirectOffset(offset, callFrame[value].jsValue(callFrame));

                vPC += 8;
                NEXT_INSTRUCTION;
            }
        }

        uncachePutByID(callFrame->codeBlock(), vPC);
        NEXT_INSTRUCTION;
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

        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        PutPropertySlot slot;
        baseValue->put(callFrame, ident, callFrame[value].jsValue(callFrame), slot);
        VM_CHECK_EXCEPTION();

        vPC += 8;
        NEXT_INSTRUCTION;
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

        JSObject* baseObj = callFrame[base].jsValue(callFrame)->toObject(callFrame);
        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        JSValue* result = jsBoolean(baseObj->deleteProperty(callFrame, ident));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_INSTRUCTION;
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
        
        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        JSValue* subscript = callFrame[property].jsValue(callFrame);

        JSValue* result;
        unsigned i;

        bool isUInt32 = JSImmediate::getUInt32(subscript, i);
        if (LIKELY(isUInt32)) {
            if (isJSArray(baseValue)) {
                JSArray* jsArray = asArray(baseValue);
                if (jsArray->canGetIndex(i))
                    result = jsArray->getIndex(i);
                else
                    result = jsArray->JSArray::get(callFrame, i);
            } else if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
                result = asString(baseValue)->getIndex(&callFrame->globalData(), i);
            else
                result = baseValue->get(callFrame, i);
        } else {
            Identifier property(callFrame, subscript->toString(callFrame));
            result = baseValue->get(callFrame, property);
        }

        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_INSTRUCTION;
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

        JSValue* baseValue = callFrame[base].jsValue(callFrame);
        JSValue* subscript = callFrame[property].jsValue(callFrame);

        unsigned i;

        bool isUInt32 = JSImmediate::getUInt32(subscript, i);
        if (LIKELY(isUInt32)) {
            if (isJSArray(baseValue)) {
                JSArray* jsArray = asArray(baseValue);
                if (jsArray->canSetIndex(i))
                    jsArray->setIndex(i, callFrame[value].jsValue(callFrame));
                else
                    jsArray->JSArray::put(callFrame, i, callFrame[value].jsValue(callFrame));
            } else
                baseValue->put(callFrame, i, callFrame[value].jsValue(callFrame));
        } else {
            Identifier property(callFrame, subscript->toString(callFrame));
            if (!globalData->exception) { // Don't put to an object if toString threw an exception.
                PutPropertySlot slot;
                baseValue->put(callFrame, property, callFrame[value].jsValue(callFrame), slot);
            }
        }

        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_INSTRUCTION;
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

        JSObject* baseObj = callFrame[base].jsValue(callFrame)->toObject(callFrame); // may throw

        JSValue* subscript = callFrame[property].jsValue(callFrame);
        JSValue* result;
        uint32_t i;
        if (subscript->getUInt32(i))
            result = jsBoolean(baseObj->deleteProperty(callFrame, i));
        else {
            VM_CHECK_EXCEPTION();
            Identifier property(callFrame, subscript->toString(callFrame));
            VM_CHECK_EXCEPTION();
            result = jsBoolean(baseObj->deleteProperty(callFrame, property));
        }

        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_INSTRUCTION;
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

        callFrame[base].jsValue(callFrame)->put(callFrame, property, callFrame[value].jsValue(callFrame));

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        if (callFrame[cond].jsValue(callFrame)->toBoolean(callFrame)) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION;
        }
        
        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (callFrame[cond].jsValue(callFrame)->toBoolean(callFrame)) {
            vPC += target;
            NEXT_INSTRUCTION;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (!callFrame[cond].jsValue(callFrame)->toBoolean(callFrame)) {
            vPC += target;
            NEXT_INSTRUCTION;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_jeq_null) {
        /* jeq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is null.
        */
        int src = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        JSValue* srcValue = callFrame[src].jsValue(callFrame);

        if (srcValue->isUndefinedOrNull() || (!JSImmediate::isImmediate(srcValue) && srcValue->asCell()->structure()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_INSTRUCTION;
        }

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_jneq_null) {
        /* jneq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is not null.
        */
        int src = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        JSValue* srcValue = callFrame[src].jsValue(callFrame);

        if (!srcValue->isUndefinedOrNull() || (!JSImmediate::isImmediate(srcValue) && !srcValue->asCell()->structure()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_INSTRUCTION;
        }

        ++vPC;
        NEXT_INSTRUCTION;
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
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int target = (++vPC)->u.operand;
        
        bool result = jsLess(callFrame, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION;
        }
        
        ++vPC;
        NEXT_INSTRUCTION;
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
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int target = (++vPC)->u.operand;
        
        bool result = jsLessEq(callFrame, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_INSTRUCTION;
        }
        
        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_jnless) {
        /* jnless src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is false.
        */
        JSValue* src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValue* src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int target = (++vPC)->u.operand;

        bool result = jsLess(callFrame, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_INSTRUCTION;
        }

        ++vPC;
        NEXT_INSTRUCTION;
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
        JSValue* scrutinee = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (!JSImmediate::isNumber(scrutinee))
            vPC += defaultOffset;
        else {
            int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
            vPC += callFrame->codeBlock()->immediateSwitchJumpTables[tableIndex].offsetForValue(value, defaultOffset);
        }
        NEXT_INSTRUCTION;
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
        JSValue* scrutinee = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (!scrutinee->isString())
            vPC += defaultOffset;
        else {
            UString::Rep* value = asString(scrutinee)->value().rep();
            if (value->size() != 1)
                vPC += defaultOffset;
            else
                vPC += callFrame->codeBlock()->characterSwitchJumpTables[tableIndex].offsetForValue(value->data()[0], defaultOffset);
        }
        NEXT_INSTRUCTION;
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
        JSValue* scrutinee = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (!scrutinee->isString())
            vPC += defaultOffset;
        else 
            vPC += callFrame->codeBlock()->stringSwitchJumpTables[tableIndex].offsetForValue(asString(scrutinee)->value().rep(), defaultOffset);
        NEXT_INSTRUCTION;
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

        callFrame[dst] = callFrame->codeBlock()->functions[func]->makeFunction(callFrame, callFrame->scopeChain());

        ++vPC;
        NEXT_INSTRUCTION;
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

        callFrame[dst] = callFrame->codeBlock()->functionExpressions[func]->makeFunction(callFrame, callFrame->scopeChain());

        ++vPC;
        NEXT_INSTRUCTION;
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

        JSValue* funcVal = callFrame[func].jsValue(callFrame);

        Register* newCallFrame = callFrame->registers() + registerOffset;
        Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
        JSValue* thisValue = argv[0].jsValue(callFrame);
        JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

        if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
            JSValue* result = callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
            if (exceptionValue)
                goto vm_throw;
            callFrame[dst] = result;

            vPC += 5;
            NEXT_INSTRUCTION;
        }

        // We didn't find the blessed version of eval, so process this
        // instruction as a normal function call.

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_call_indirect; // indirect goto -> op_call
#endif
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

        JSValue* v = callFrame[func].jsValue(callFrame);

        CallData callData;
        CallType callType = v->getCallData(callData);

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
            vPC = newCodeBlock->instructions.begin();

#if ENABLE(OPCODE_STATS)
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_INSTRUCTION;
        }

        if (callType == CallTypeHost) {
            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 5, scopeChain, callFrame, dst, argCount, 0);

            Register* thisRegister = newCallFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
            ArgList args(thisRegister + 1, argCount - 1);

            // FIXME: All host methods should be calling toThisObject, but this is not presently the case.
            JSValue* thisValue = thisRegister->jsValue(callFrame);
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();

            JSValue* returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler);
                returnValue = callData.native.function(newCallFrame, asObject(v), thisValue, args);
            }
            VM_CHECK_EXCEPTION();

            callFrame[dst] = returnValue;

            vPC += 5;
            NEXT_INSTRUCTION;
        }

        ASSERT(callType == CallTypeNone);

        exceptionValue = createNotAFunctionError(callFrame, v, vPC, callFrame->codeBlock());
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
        ASSERT(callFrame->codeBlock()->needsFullScopeChain);

        asActivation(callFrame[src].getJSValue())->copyRegisters(callFrame->optionalCalleeArguments());

        ++vPC;
        NEXT_INSTRUCTION;
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

        ASSERT(callFrame->codeBlock()->usesArguments && !callFrame->codeBlock()->needsFullScopeChain);

        callFrame->optionalCalleeArguments()->copyRegisters();

        ++vPC;
        NEXT_INSTRUCTION;
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

        if (callFrame->codeBlock()->needsFullScopeChain)
            callFrame->scopeChain()->deref();

        JSValue* returnValue = callFrame[result].jsValue(callFrame);

        vPC = callFrame->returnPC();
        int dst = callFrame->returnValueRegister();
        callFrame = callFrame->callerFrame();
        
        if (callFrame->hasHostCallFrameFlag())
            return returnValue;

        callFrame[dst] = returnValue;

        NEXT_INSTRUCTION;
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
        
        for (size_t count = codeBlock->numVars; i < count; ++i)
            callFrame[i] = jsUndefined();

        for (size_t count = codeBlock->constantRegisters.size(), j = 0; j < count; ++i, ++j)
            callFrame[i] = codeBlock->constantRegisters[j];

        ++vPC;
        NEXT_INSTRUCTION;
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

        for (size_t count = codeBlock->numVars; i < count; ++i)
            callFrame[i] = jsUndefined();

        for (size_t count = codeBlock->constantRegisters.size(), j = 0; j < count; ++i, ++j)
            callFrame[i] = codeBlock->constantRegisters[j];

        int dst = (++vPC)->u.operand;
        JSActivation* activation = new (globalData) JSActivation(callFrame, static_cast<FunctionBodyNode*>(codeBlock->ownerNode));
        callFrame[dst] = activation;
        callFrame->setScopeChain(callFrame->scopeChain()->copy()->push(activation));

        ++vPC;
        NEXT_INSTRUCTION;
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
        JSValue* thisVal = callFrame[thisRegister].getJSValue();
        if (thisVal->needsThisConversion())
            callFrame[thisRegister] = thisVal->toThisObject(callFrame);

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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

        JSValue* v = callFrame[func].jsValue(callFrame);

        ConstructData constructData;
        ConstructType constructType = v->getConstructData(constructData);

        if (constructType == ConstructTypeJS) {
            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->bytecode(callDataScopeChain);

            Structure* structure;
            JSValue* prototype = callFrame[proto].jsValue(callFrame);
            if (prototype->isObject())
                structure = asObject(prototype)->inheritorID();
            else
                structure = callDataScopeChain->globalObject()->emptyObjectStructure();
            JSObject* newObject = new (globalData) JSObject(structure);

            callFrame[thisRegister] = newObject; // "this" value

            CallFrame* previousCallFrame = callFrame;

            callFrame = slideRegisterWindowForCall(newCodeBlock, registerFile, callFrame, registerOffset, argCount);
            if (UNLIKELY(!callFrame)) {
                callFrame = previousCallFrame;
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }

            callFrame->init(newCodeBlock, vPC + 7, callDataScopeChain, previousCallFrame, dst, argCount, asFunction(v));
            vPC = newCodeBlock->instructions.begin();

#if ENABLE(OPCODE_STATS)
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_INSTRUCTION;
        }

        if (constructType == ConstructTypeHost) {
            ArgList args(callFrame->registers() + thisRegister + 1, argCount - 1);

            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 7, scopeChain, callFrame, dst, argCount, 0);

            JSValue* returnValue;
            {
                SamplingTool::HostCallRecord callRecord(m_sampler);
                returnValue = constructData.native.function(newCallFrame, asObject(v), args);
            }
            VM_CHECK_EXCEPTION();
            callFrame[dst] = returnValue;

            vPC += 7;
            NEXT_INSTRUCTION;
        }

        ASSERT(constructType == ConstructTypeNone);

        exceptionValue = createNotAConstructorError(callFrame, v, vPC, callFrame->codeBlock());
        goto vm_throw;
    }
    DEFINE_OPCODE(op_construct_verify) {
        /* construct_verify dst(r) override(r)

           Verifies that register dst holds an object. If not, moves
           the object in register override to register dst.
        */

        int dst = vPC[1].u.operand;;
        if (LIKELY(callFrame[dst].jsValue(callFrame)->isObject())) {
            vPC += 3;
            NEXT_INSTRUCTION;
        }

        int override = vPC[2].u.operand;
        callFrame[dst] = callFrame[override];

        vPC += 3;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.
        */
        int scope = (++vPC)->u.operand;
        JSValue* v = callFrame[scope].jsValue(callFrame);
        JSObject* o = v->toObject(callFrame);
        VM_CHECK_EXCEPTION();

        callFrame->setScopeChain(callFrame->scopeChain()->push(o));

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        callFrame->setScopeChain(callFrame->scopeChain()->pop());

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        if (JSValue* temp = it->next(callFrame)) {
            CHECK_FOR_TIMEOUT();
            callFrame[dst] = temp;
            vPC += target;
            NEXT_INSTRUCTION;
        }
        it->invalidate();

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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

        handlerVPC = throwException(callFrame, exceptionValue, vPC, true);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_throw_end_indirect; // indirect goto -> op_throw_end
    }
    op_throw_end: {
#endif

        vPC = handlerVPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_unexpected_load) {
        /* unexpected_load load dst(r) src(k)

           Copies constant src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = callFrame->codeBlock()->unexpectedConstants[src];

        ++vPC;
        NEXT_INSTRUCTION;
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
        callFrame[dst] = Error::create(callFrame, (ErrorType)type, codeBlock->unexpectedConstants[message]->toString(callFrame), codeBlock->lineNumberForVPC(vPC), codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->sourceURL());

        ++vPC;
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_end) {
        /* end result(r)
           
           Return register result as the value of a global or eval
           program. Return control to the calling native code.
        */

        if (callFrame->codeBlock()->needsFullScopeChain) {
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

        ASSERT(callFrame[base].jsValue(callFrame)->isObject());
        JSObject* baseObj = asObject(callFrame[base].jsValue(callFrame));
        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        ASSERT(callFrame[function].jsValue(callFrame)->isObject());
        baseObj->defineGetter(callFrame, ident, asObject(callFrame[function].jsValue(callFrame)));

        ++vPC;
        NEXT_INSTRUCTION;
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

        ASSERT(callFrame[base].jsValue(callFrame)->isObject());
        JSObject* baseObj = asObject(callFrame[base].jsValue(callFrame));
        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        ASSERT(callFrame[function].jsValue(callFrame)->isObject());
        baseObj->defineSetter(callFrame, ident, asObject(callFrame[function].jsValue(callFrame)));

        ++vPC;
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
    }
    DEFINE_OPCODE(op_sret) {
        /* sret retAddrSrc(r)

         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = (++vPC)->u.operand;
        vPC = callFrame[retAddrSrc].vPC();
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
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
        NEXT_INSTRUCTION;
    }
    vm_throw: {
        globalData->exception = noValue();
        if (!tickCount) {
            // The exceptionValue is a lie! (GCC produces bad code for reasons I 
            // cannot fathom if we don't assign to the exceptionValue before branching)
            exceptionValue = createInterruptedExecutionException(globalData);
        }
        handlerVPC = throwException(callFrame, exceptionValue, vPC, false);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }
        vPC = handlerVPC;
        NEXT_INSTRUCTION;
    }
    }
#if !HAVE(COMPUTED_GOTO)
    } // iterator loop ends
#endif
    #undef NEXT_INSTRUCTION
    #undef DEFINE_OPCODE
    #undef VM_CHECK_EXCEPTION
    #undef CHECK_FOR_TIMEOUT
}

JSValue* Interpreter::retrieveArguments(CallFrame* callFrame, JSFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CodeBlock* codeBlock = functionCallFrame->codeBlock();
    if (codeBlock->usesArguments) {
        ASSERT(codeBlock->codeType == FunctionCode);
        SymbolTable& symbolTable = codeBlock->symbolTable;
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

JSValue* Interpreter::retrieveCaller(CallFrame* callFrame, InternalFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CallFrame* callerFrame = functionCallFrame->callerFrame();
    if (callerFrame->hasHostCallFrameFlag())
        return jsNull();

    JSValue* caller = callerFrame->callee();
    if (!caller)
        return jsNull();

    return caller;
}

void Interpreter::retrieveLastCaller(CallFrame* callFrame, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValue*& function) const
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

    Instruction* vPC = vPCForPC(callerCodeBlock, callFrame->returnPC());
    lineNumber = callerCodeBlock->lineNumberForVPC(vPC - 1);
    sourceID = callerCodeBlock->ownerNode->sourceID();
    sourceURL = callerCodeBlock->ownerNode->sourceURL();
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

NEVER_INLINE void Interpreter::tryCTICachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValue* baseValue, const PutPropertySlot& slot)
{
    // The interpreter checks for recursion here; I do not believe this can occur in CTI.

    if (JSImmediate::isImmediate(baseValue))
        return;

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }
    
    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isDictionary()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(returnAddress);
    Instruction* vPC = codeBlock->instructions.begin() + vPCIndex;

    // Cache hit: Specialize instruction and ref Structures.

    // If baseCell != base, then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
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
        JIT::compilePutByIdTransition(callFrame->scopeChain()->globalData, codeBlock, structure->previousID(), structure, slot.cachedOffset(), chain, returnAddress);
        return;
    }
    
    vPC[0] = getOpcode(op_put_by_id_replace);
    vPC[4] = structure;
    vPC[5] = slot.cachedOffset();
    codeBlock->refStructures(vPC);

#if USE(CTI_REPATCH_PIC)
    UNUSED_PARAM(callFrame);
    JIT::patchPutByIdReplace(codeBlock, structure, slot.cachedOffset(), returnAddress);
#else
    JIT::compilePutByIdReplace(callFrame->scopeChain()->globalData, callFrame, codeBlock, structure, slot.cachedOffset(), returnAddress);
#endif
}

NEVER_INLINE void Interpreter::tryCTICacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValue* baseValue, const Identifier& propertyName, const PropertySlot& slot)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    // FIXME: Cache property access for immediates.
    if (JSImmediate::isImmediate(baseValue)) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    if (isJSArray(baseValue) && propertyName == callFrame->propertyNames().length) {
#if USE(CTI_REPATCH_PIC)
        JIT::compilePatchGetArrayLength(callFrame->scopeChain()->globalData, codeBlock, returnAddress);
#else
        ctiRepatchCallByReturnAddress(returnAddress, m_ctiArrayLengthTrampoline);
#endif
        return;
    }
    if (isJSString(baseValue) && propertyName == callFrame->propertyNames().length) {
        // The tradeoff of compiling an repatched inline string length access routine does not seem
        // to pay off, so we currently only do this for arrays.
        ctiRepatchCallByReturnAddress(returnAddress, m_ctiStringLengthTrampoline);
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    JSCell* baseCell = asCell(baseValue);
    Structure* structure = baseCell->structure();

    if (structure->isDictionary()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(returnAddress);
    Instruction* vPC = codeBlock->instructions.begin() + vPCIndex;

    // Cache hit: Specialize instruction and ref Structures.

    if (slot.slotBase() == baseValue) {
        // set this up, so derefStructures can do it's job.
        vPC[0] = getOpcode(op_get_by_id_self);
        vPC[4] = structure;
        vPC[5] = slot.cachedOffset();
        codeBlock->refStructures(vPC);
        
#if USE(CTI_REPATCH_PIC)
        JIT::patchGetByIdSelf(codeBlock, structure, slot.cachedOffset(), returnAddress);
#else
        JIT::compileGetByIdSelf(callFrame->scopeChain()->globalData, callFrame, codeBlock, structure, slot.cachedOffset(), returnAddress);
#endif
        return;
    }

    if (slot.slotBase() == structure->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase()->isObject());

        JSObject* slotBaseObject = asObject(slot.slotBase());

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (slotBaseObject->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(slotBaseObject->structure());
            slotBaseObject->setStructure(transition.release());
            asObject(baseValue)->structure()->setCachedPrototypeChain(0);
        }

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[4] = structure;
        vPC[5] = slotBaseObject->structure();
        vPC[6] = slot.cachedOffset();
        codeBlock->refStructures(vPC);

        JIT::compileGetByIdProto(callFrame->scopeChain()->globalData, callFrame, codeBlock, structure, slotBaseObject->structure(), slot.cachedOffset(), returnAddress);
        return;
    }

    size_t count = 0;
    JSObject* o = asObject(baseValue);
    while (slot.slotBase() != o) {
        JSValue* v = o->structure()->prototypeForLookup(callFrame);

        // If we didn't find slotBase in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.

        if (v->isNull()) {
            vPC[0] = getOpcode(op_get_by_id_generic);
            return;
        }

        o = asObject(v);

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (o->structure()->isDictionary()) {
            RefPtr<Structure> transition = Structure::fromDictionaryTransition(o->structure());
            o->setStructure(transition.release());
            asObject(baseValue)->structure()->setCachedPrototypeChain(0);
        }

        ++count;
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

    JIT::compileGetByIdChain(callFrame->scopeChain()->globalData, callFrame, codeBlock, structure, chain, count, slot.cachedOffset(), returnAddress);
}

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

#define CTI_STACK_HACK() StackHack stackHack(&CTI_RETURN_ADDRESS_SLOT)
#define CTI_SET_RETURN_ADDRESS(address) stackHack.savedReturnAddress = address
#define CTI_RETURN_ADDRESS stackHack.savedReturnAddress

#else

#define CTI_STACK_HACK() (void)0
#define CTI_SET_RETURN_ADDRESS(address) ctiSetReturnAddress(&CTI_RETURN_ADDRESS_SLOT, address);
#define CTI_RETURN_ADDRESS CTI_RETURN_ADDRESS_SLOT

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
        VoidPtrPairValue pair = {{ 0, 0 }}; \
        return pair.i; \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    returnToThrowTrampoline(ARG_globalData, CTI_RETURN_ADDRESS, CTI_RETURN_ADDRESS)

#define VM_CHECK_EXCEPTION() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != noValue())) \
            VM_THROW_EXCEPTION(); \
    } while (0)
#define VM_CHECK_EXCEPTION_AT_END() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != noValue())) \
            VM_THROW_EXCEPTION_AT_END(); \
    } while (0)
#define VM_CHECK_EXCEPTION_VOID() \
    do { \
        if (UNLIKELY(ARG_globalData->exception != noValue())) { \
            VM_THROW_EXCEPTION_AT_END(); \
            return; \
        } \
    } while (0)

JSObject* Interpreter::cti_op_convert_this(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v1 = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSObject* result = v1->toThisObject(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Interpreter::cti_op_end(CTI_ARGS)
{
    CTI_STACK_HACK();

    ScopeChainNode* scopeChain = ARG_callFrame->scopeChain();
    ASSERT(scopeChain->refCount > 1);
    scopeChain->deref();
}

JSValue* Interpreter::cti_op_add(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v1 = ARG_src1;
    JSValue* v2 = ARG_src2;

    double left;
    double right = 0.0;

    bool rightIsNumber = fastIsNumber(v2, right);
    if (rightIsNumber && fastIsNumber(v1, left))
        return jsNumber(ARG_globalData, left + right);
    
    CallFrame* callFrame = ARG_callFrame;

    bool leftIsString = v1->isString();
    if (leftIsString && v2->isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }

        return jsString(ARG_globalData, value.release());
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = JSImmediate::isImmediate(v2) ?
            concatenate(asString(v1)->value().rep(), JSImmediate::getTruncatedInt32(v2)) :
            concatenate(asString(v1)->value().rep(), right);

        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }
        return jsString(ARG_globalData, value.release());
    }

    // All other cases are pretty uncommon
    JSValue* result = jsAddSlowCase(callFrame, v1, v2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_pre_inc(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, v->toNumber(callFrame) + 1);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Interpreter::cti_timeout_check(CTI_ARGS)
{
    CTI_STACK_HACK();

    if (ARG_globalData->interpreter->checkTimeout(ARG_callFrame->dynamicGlobalObject())) {
        ARG_globalData->exception = createInterruptedExecutionException(ARG_globalData);
        VM_THROW_EXCEPTION_AT_END();
    }
}

void Interpreter::cti_register_file_check(CTI_ARGS)
{
    CTI_STACK_HACK();

    if (LIKELY(ARG_registerFile->grow(ARG_callFrame + ARG_callFrame->codeBlock()->numCalleeRegisters)))
        return;

    // Rewind to the previous call frame because op_call already optimistically
    // moved the call frame forward.
    CallFrame* oldCallFrame = ARG_callFrame->callerFrame();
    ARG_setCallFrame(oldCallFrame);
    throwStackOverflowError(oldCallFrame, ARG_globalData, oldCallFrame->returnPC(), CTI_RETURN_ADDRESS);
}

int Interpreter::cti_op_loop_if_less(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Interpreter::cti_op_loop_if_lesseq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLessEq(callFrame, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSObject* Interpreter::cti_op_new_object(CTI_ARGS)
{
    CTI_STACK_HACK();

    return constructEmptyObject(ARG_callFrame);
}

void Interpreter::cti_op_put_by_id(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1->put(callFrame, ident, ARG_src3, slot);

    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_id_second));

    VM_CHECK_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_id_second(CTI_ARGS)
{
    CTI_STACK_HACK();

    PutPropertySlot slot;
    ARG_src1->put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    ARG_globalData->interpreter->tryCTICachePutByID(ARG_callFrame, ARG_callFrame->codeBlock(), CTI_RETURN_ADDRESS, ARG_src1, slot);
    VM_CHECK_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_id_generic(CTI_ARGS)
{
    CTI_STACK_HACK();

    PutPropertySlot slot;
    ARG_src1->put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    VM_CHECK_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_id_fail(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1->put(callFrame, ident, ARG_src3, slot);

    // should probably uncachePutByID() ... this would mean doing a vPC lookup - might be worth just bleeding this until the end.
    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_id_generic));

    VM_CHECK_EXCEPTION_AT_END();
}

JSValue* Interpreter::cti_op_get_by_id(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(callFrame, ident, slot);

    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_second));

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_get_by_id_second(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(callFrame, ident, slot);

    ARG_globalData->interpreter->tryCTICacheGetByID(callFrame, callFrame->codeBlock(), CTI_RETURN_ADDRESS, baseValue, ident, slot);

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_get_by_id_generic(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(callFrame, ident, slot);

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_get_by_id_fail(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValue* baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValue* result = baseValue->get(callFrame, ident, slot);

    // should probably uncacheGetByID() ... this would mean doing a vPC lookup - might be worth just bleeding this until the end.
    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_generic));

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_instanceof(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValue* value = ARG_src1;
    JSValue* baseVal = ARG_src2;
    JSValue* proto = ARG_src3;

    // at least one of these checks must have failed to get to the slow case
    ASSERT(JSImmediate::isAnyImmediate(value, baseVal, proto) 
           || !value->isObject() || !baseVal->isObject() || !proto->isObject() 
           || (asObject(baseVal)->structure()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);

    if (!baseVal->isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
        unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "instanceof", baseVal, codeBlock->instructions.begin() + vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    if (!asObject(baseVal)->structure()->typeInfo().implementsHasInstance())
        return jsBoolean(false);

    if (!proto->isObject()) {
        throwError(callFrame, TypeError, "instanceof called on an object with an invalid prototype property.");
        VM_THROW_EXCEPTION();
    }
        
    if (!value->isObject())
        return jsBoolean(false);

    JSValue* result = jsBoolean(asObject(baseVal)->hasInstance(callFrame, value, proto));
    VM_CHECK_EXCEPTION_AT_END();

    return result;
}

JSValue* Interpreter::cti_op_del_by_id(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;
    
    JSObject* baseObj = ARG_src1->toObject(callFrame);

    JSValue* result = jsBoolean(baseObj->deleteProperty(callFrame, ident));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_mul(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left * right);

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, src1->toNumber(callFrame) * src2->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSObject* Interpreter::cti_op_new_func(CTI_ARGS)
{
    CTI_STACK_HACK();

    return ARG_func1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

void* Interpreter::cti_op_call_JSFunction(CTI_ARGS)
{
    CTI_STACK_HACK();

#ifndef NDEBUG
    CallData callData;
    ASSERT(ARG_src1->getCallData(callData) == CallTypeJS);
#endif

    ScopeChainNode* callDataScopeChain = asFunction(ARG_src1)->m_scopeChain.node();
    CodeBlock* newCodeBlock = &asFunction(ARG_src1)->m_body->bytecode(callDataScopeChain);

    if (!newCodeBlock->ctiCode)
        JIT::compile(ARG_globalData, newCodeBlock);

    return newCodeBlock;
}

VoidPtrPair Interpreter::cti_op_call_arityCheck(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* newCodeBlock = ARG_codeBlock4;
    int argCount = ARG_int3;

    ASSERT(argCount != newCodeBlock->numParameters);

    CallFrame* oldCallFrame = callFrame->callerFrame();

    if (argCount > newCodeBlock->numParameters) {
        size_t numParameters = newCodeBlock->numParameters;
        Register* r = callFrame->registers() + numParameters;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argCount;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argCount] = argv[i];

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    } else {
        size_t omittedArgCount = newCodeBlock->numParameters - argCount;
        Register* r = callFrame->registers() + omittedArgCount;
        Register* newEnd = r + newCodeBlock->numCalleeRegisters;
        if (!ARG_registerFile->grow(newEnd)) {
            // Rewind to the previous call frame because op_call already optimistically
            // moved the call frame forward.
            ARG_setCallFrame(oldCallFrame);
            throwStackOverflowError(oldCallFrame, ARG_globalData, ARG_returnAddress2, CTI_RETURN_ADDRESS);
            VoidPtrPairValue pair = {{ 0, 0 }};
            return pair.i;
        }

        Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
        for (size_t i = 0; i < omittedArgCount; ++i)
            argv[i] = jsUndefined();

        callFrame = CallFrame::create(r);
        callFrame->setCallerFrame(oldCallFrame);
    }

    VoidPtrPairValue pair = {{ newCodeBlock, callFrame }};
    return pair.i;
}

void* Interpreter::cti_vm_dontLazyLinkCall(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSFunction* callee = asFunction(ARG_src1);
    CodeBlock* codeBlock = &callee->m_body->bytecode(callee->m_scopeChain.node());
    if (!codeBlock->ctiCode)
        JIT::compile(ARG_globalData, codeBlock);

    ctiRepatchCallByReturnAddress(ARG_returnAddress2, ARG_globalData->interpreter->m_ctiVirtualCallLink);

    return codeBlock->ctiCode;
}

void* Interpreter::cti_vm_lazyLinkCall(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSFunction* callee = asFunction(ARG_src1);
    CodeBlock* codeBlock = &callee->m_body->bytecode(callee->m_scopeChain.node());
    if (!codeBlock->ctiCode)
        JIT::compile(ARG_globalData, codeBlock);

    CallLinkInfo* callLinkInfo = &ARG_callFrame->callerFrame()->codeBlock()->getCallLinkInfo(ARG_returnAddress2);
    JIT::linkCall(callee, codeBlock, codeBlock->ctiCode, callLinkInfo, ARG_int3);

    return codeBlock->ctiCode;
}

JSObject* Interpreter::cti_op_push_activation(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSActivation* activation = new (ARG_globalData) JSActivation(ARG_callFrame, static_cast<FunctionBodyNode*>(ARG_callFrame->codeBlock()->ownerNode));
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->copy()->push(activation));
    return activation;
}

JSValue* Interpreter::cti_op_call_NotJSFunction(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* funcVal = ARG_src1;

    CallData callData;
    CallType callType = funcVal->getCallData(callData);

    ASSERT(callType != CallTypeJS);

    if (callType == CallTypeHost) {
        int registerOffset = ARG_int2;
        int argCount = ARG_int3;
        CallFrame* previousCallFrame = ARG_callFrame;
        CallFrame* callFrame = CallFrame::create(previousCallFrame->registers() + registerOffset);

        callFrame->init(0, ARG_instr4 + 1, previousCallFrame->scopeChain(), previousCallFrame, 0, argCount, 0);
        ARG_setCallFrame(callFrame);

        Register* argv = ARG_callFrame->registers() - RegisterFile::CallFrameHeaderSize - argCount;
        ArgList argList(argv + 1, argCount - 1);

        JSValue* returnValue;
        {
            SamplingTool::HostCallRecord callRecord(CTI_SAMPLER);

            // FIXME: All host methods should be calling toThisObject, but this is not presently the case.
            JSValue* thisValue = argv[0].jsValue(callFrame);
            if (thisValue == jsNull())
                thisValue = callFrame->globalThisValue();

            returnValue = callData.native.function(callFrame, asObject(funcVal), thisValue, argList);
        }
        ARG_setCallFrame(previousCallFrame);
        VM_CHECK_EXCEPTION();

        return returnValue;
    }

    ASSERT(callType == CallTypeNone);

    ARG_globalData->exception = createNotAFunctionError(ARG_callFrame, funcVal, ARG_instr4, ARG_callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

void Interpreter::cti_op_create_arguments(CTI_ARGS)
{
    CTI_STACK_HACK();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void Interpreter::cti_op_create_arguments_no_params(CTI_ARGS)
{
    CTI_STACK_HACK();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame, Arguments::NoParameters);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void Interpreter::cti_op_tear_off_activation(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain);
    asActivation(ARG_src1)->copyRegisters(ARG_callFrame->optionalCalleeArguments());
}

void Interpreter::cti_op_tear_off_arguments(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(ARG_callFrame->codeBlock()->usesArguments && !ARG_callFrame->codeBlock()->needsFullScopeChain);
    ARG_callFrame->optionalCalleeArguments()->copyRegisters();
}

void Interpreter::cti_op_profile_will_call(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->willExecute(ARG_callFrame, ARG_src1);
}

void Interpreter::cti_op_profile_did_call(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->didExecute(ARG_callFrame, ARG_src1);
}

void Interpreter::cti_op_ret_scopeChain(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain);
    ARG_callFrame->scopeChain()->deref();
}

JSObject* Interpreter::cti_op_new_array(CTI_ARGS)
{
    CTI_STACK_HACK();

    ArgList argList(ARG_registers1, ARG_int2);
    return constructArray(ARG_callFrame, argList);
}

JSValue* Interpreter::cti_op_resolve(CTI_ARGS)
{
    CTI_STACK_HACK();

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
            JSValue* result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();
            return result;
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSObject* Interpreter::cti_op_construct_JSConstruct(CTI_ARGS)
{
    CTI_STACK_HACK();

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(asFunction(ARG_src1)->getConstructData(constructData) == ConstructTypeJS);
#endif

    Structure* structure;
    if (ARG_src4->isObject())
        structure = asObject(ARG_src4)->inheritorID();
    else
        structure = asFunction(ARG_src1)->m_scopeChain.node()->globalObject()->emptyObjectStructure();
    return new (ARG_globalData) JSObject(structure);
}

JSValue* Interpreter::cti_op_construct_NotJSConstruct(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSValue* constrVal = ARG_src1;
    int argCount = ARG_int3;
    int thisRegister = ARG_int5;

    ConstructData constructData;
    ConstructType constructType = constrVal->getConstructData(constructData);

    if (constructType == ConstructTypeHost) {
        ArgList argList(callFrame->registers() + thisRegister + 1, argCount - 1);

        JSValue* returnValue;
        {
            SamplingTool::HostCallRecord callRecord(CTI_SAMPLER);
            returnValue = constructData.native.function(callFrame, asObject(constrVal), argList);
        }
        VM_CHECK_EXCEPTION();

        return returnValue;
    }

    ASSERT(constructType == ConstructTypeNone);

    ARG_globalData->exception = createNotAConstructorError(callFrame, constrVal, ARG_instr6, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

JSValue* Interpreter::cti_op_get_by_val(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Interpreter* interpreter = ARG_globalData->interpreter;

    JSValue* baseValue = ARG_src1;
    JSValue* subscript = ARG_src2;

    JSValue* result;
    unsigned i;

    bool isUInt32 = JSImmediate::getUInt32(subscript, i);
    if (LIKELY(isUInt32)) {
        if (interpreter->isJSArray(baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canGetIndex(i))
                result = jsArray->getIndex(i);
            else
                result = jsArray->JSArray::get(callFrame, i);
        } else if (interpreter->isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            result = asString(baseValue)->getIndex(ARG_globalData, i);
        else
            result = baseValue->get(callFrame, i);
    } else {
        Identifier property(callFrame, subscript->toString(callFrame));
        result = baseValue->get(callFrame, property);
    }

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair Interpreter::cti_op_resolve_func(CTI_ARGS)
{
    CTI_STACK_HACK();

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
            JSValue* result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();

            VoidPtrPairValue pair = {{ thisObj, asPointer(result) }};
            return pair.i;
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSValue* Interpreter::cti_op_sub(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left - right);

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, src1->toNumber(callFrame) - src2->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Interpreter::cti_op_put_by_val(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Interpreter* interpreter = ARG_globalData->interpreter;

    JSValue* baseValue = ARG_src1;
    JSValue* subscript = ARG_src2;
    JSValue* value = ARG_src3;

    unsigned i;

    bool isUInt32 = JSImmediate::getUInt32(subscript, i);
    if (LIKELY(isUInt32)) {
        if (interpreter->isJSArray(baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canSetIndex(i))
                jsArray->setIndex(i, value);
            else
                jsArray->JSArray::put(callFrame, i, value);
        } else
            baseValue->put(callFrame, i, value);
    } else {
        Identifier property(callFrame, subscript->toString(callFrame));
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue->put(callFrame, property, value, slot);
        }
    }

    VM_CHECK_EXCEPTION_AT_END();
}

void Interpreter::cti_op_put_by_val_array(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSValue* baseValue = ARG_src1;
    int i = ARG_int2;
    JSValue* value = ARG_src3;

    ASSERT(ARG_globalData->interpreter->isJSArray(baseValue));

    if (LIKELY(i >= 0))
        asArray(baseValue)->JSArray::put(callFrame, i, value);
    else {
        Identifier property(callFrame, JSImmediate::from(i)->toString(callFrame));
        // FIXME: can toString throw an exception here?
        if (!ARG_globalData->exception) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot;
            baseValue->put(callFrame, property, value, slot);
        }
    }

    VM_CHECK_EXCEPTION_AT_END();
}

JSValue* Interpreter::cti_op_lesseq(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsBoolean(jsLessEq(callFrame, ARG_src1, ARG_src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Interpreter::cti_op_loop_if_true(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1->toBoolean(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_negate(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src = ARG_src1;

    double v;
    if (fastIsNumber(src, v))
        return jsNumber(ARG_globalData, -v);

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, -src->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_resolve_base(CTI_ARGS)
{
    CTI_STACK_HACK();

    return inlineResolveBase(ARG_callFrame, *ARG_id1, ARG_callFrame->scopeChain());
}

JSValue* Interpreter::cti_op_resolve_skip(CTI_ARGS)
{
    CTI_STACK_HACK();

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
            JSValue* result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();
            return result;
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSValue* Interpreter::cti_op_resolve_global(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSGlobalObject* globalObject = asGlobalObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    Instruction* vPC = ARG_instr3;
    ASSERT(globalObject->isGlobalObject());

    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValue* result = slot.getValue(callFrame, ident);
        if (slot.isCacheable()) {
            if (vPC[4].u.structure)
                vPC[4].u.structure->deref();
            globalObject->structure()->ref();
            vPC[4] = globalObject->structure();
            vPC[5] = slot.cachedOffset();
            return result;
        }

        VM_CHECK_EXCEPTION_AT_END();
        return result;
    }
    
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPC, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

JSValue* Interpreter::cti_op_div(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left / right);

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, src1->toNumber(callFrame) / src2->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_pre_dec(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, v->toNumber(callFrame) - 1);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Interpreter::cti_op_jless(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_not(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValue* result = jsBoolean(!src->toBoolean(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int SFX_CALL Interpreter::cti_op_jtrue(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1->toBoolean(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair Interpreter::cti_op_post_inc(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValue* number = v->toJSNumber(callFrame);
    VM_CHECK_EXCEPTION_AT_END();

    VoidPtrPairValue pair = {{ asPointer(number), asPointer(jsNumber(ARG_globalData, number->uncheckedGetNumber() + 1)) }};
    return pair.i;
}

JSValue* Interpreter::cti_op_eq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(!JSImmediate::areBothImmediateNumbers(src1, src2));
    JSValue* result = jsBoolean(equalSlowCaseInline(callFrame, src1, src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_lshift(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* val = ARG_src1;
    JSValue* shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSImmediate::areBothImmediateNumbers(val, shift))
        return jsNumber(ARG_globalData, JSImmediate::getTruncatedInt32(val) << (JSImmediate::getTruncatedUInt32(shift) & 0x1f));
    if (fastToInt32(val, left) && fastToUInt32(shift, right))
        return jsNumber(ARG_globalData, left << (right & 0x1f));

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, (val->toInt32(callFrame)) << (shift->toUInt32(callFrame) & 0x1f));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_bitand(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    int32_t left;
    int32_t right;
    if (fastToInt32(src1, left) && fastToInt32(src2, right))
        return jsNumber(ARG_globalData, left & right);

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, src1->toInt32(callFrame) & src2->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_rshift(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* val = ARG_src1;
    JSValue* shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSImmediate::areBothImmediateNumbers(val, shift))
        return JSImmediate::rightShiftImmediateNumbers(val, shift);
    if (fastToInt32(val, left) && fastToUInt32(shift, right))
        return jsNumber(ARG_globalData, left >> (right & 0x1f));

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, (val->toInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_bitnot(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src = ARG_src1;

    int value;
    if (fastToInt32(src, value))
        return jsNumber(ARG_globalData, ~value);
            
    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsNumber(ARG_globalData, ~src->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair Interpreter::cti_op_resolve_with_base(CTI_ARGS)
{
    CTI_STACK_HACK();

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
            JSValue* result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();

            VoidPtrPairValue pair = {{ base, asPointer(result) }};
            return pair.i;
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSObject* Interpreter::cti_op_new_func_exp(CTI_ARGS)
{
    CTI_STACK_HACK();

    return ARG_funcexp1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

JSValue* Interpreter::cti_op_mod(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* dividendValue = ARG_src1;
    JSValue* divisorValue = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;
    double d = dividendValue->toNumber(callFrame);
    JSValue* result = jsNumber(ARG_globalData, fmod(d, divisorValue->toNumber(callFrame)));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_less(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsBoolean(jsLess(callFrame, ARG_src1, ARG_src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_neq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    ASSERT(!JSImmediate::areBothImmediateNumbers(src1, src2));

    CallFrame* callFrame = ARG_callFrame;
    JSValue* result = jsBoolean(!equalSlowCaseInline(callFrame, src1, src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair Interpreter::cti_op_post_dec(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValue* number = v->toJSNumber(callFrame);
    VM_CHECK_EXCEPTION_AT_END();

    VoidPtrPairValue pair = {{ asPointer(number), asPointer(jsNumber(ARG_globalData, number->uncheckedGetNumber() - 1)) }};
    return pair.i;
}

JSValue* Interpreter::cti_op_urshift(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* val = ARG_src1;
    JSValue* shift = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
        return JSImmediate::rightShiftImmediateNumbers(val, shift);
    else {
        JSValue* result = jsNumber(ARG_globalData, (val->toUInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
        VM_CHECK_EXCEPTION_AT_END();
        return result;
    }
}

JSValue* Interpreter::cti_op_bitxor(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValue* result = jsNumber(ARG_globalData, src1->toInt32(callFrame) ^ src2->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSObject* Interpreter::cti_op_new_regexp(CTI_ARGS)
{
    CTI_STACK_HACK();

    return new (ARG_globalData) RegExpObject(ARG_callFrame->lexicalGlobalObject()->regExpStructure(), ARG_regexp1);
}

JSValue* Interpreter::cti_op_bitor(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValue* result = jsNumber(ARG_globalData, src1->toInt32(callFrame) | src2->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_call_eval(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    RegisterFile* registerFile = ARG_registerFile;

    Interpreter* interpreter = ARG_globalData->interpreter;
    
    JSValue* funcVal = ARG_src1;
    int registerOffset = ARG_int2;
    int argCount = ARG_int3;

    Register* newCallFrame = callFrame->registers() + registerOffset;
    Register* argv = newCallFrame - RegisterFile::CallFrameHeaderSize - argCount;
    JSValue* thisValue = argv[0].jsValue(callFrame);
    JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

    if (thisValue == globalObject && funcVal == globalObject->evalFunction()) {
        JSValue* exceptionValue = noValue();
        JSValue* result = interpreter->callEval(callFrame, registerFile, argv, argCount, registerOffset, exceptionValue);
        if (UNLIKELY(exceptionValue != noValue())) {
            ARG_globalData->exception = exceptionValue;
            VM_THROW_EXCEPTION_AT_END();
        }
        return result;
    }

    return JSImmediate::impossibleValue();
}

JSValue* Interpreter::cti_op_throw(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);

    JSValue* exceptionValue = ARG_src1;
    ASSERT(exceptionValue);

    Instruction* handlerVPC = ARG_globalData->interpreter->throwException(callFrame, exceptionValue, codeBlock->instructions.begin() + vPCIndex, true);

    if (!handlerVPC) {
        *ARG_exception = exceptionValue;
        return JSImmediate::nullImmediate();
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = callFrame->codeBlock()->nativeExceptionCodeForHandlerVPC(handlerVPC);
    ASSERT(catchRoutine);
    CTI_SET_RETURN_ADDRESS(catchRoutine);
    return exceptionValue;
}

JSPropertyNameIterator* Interpreter::cti_op_get_pnames(CTI_ARGS)
{
    CTI_STACK_HACK();

    return JSPropertyNameIterator::create(ARG_callFrame, ARG_src1);
}

JSValue* Interpreter::cti_op_next_pname(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSPropertyNameIterator* it = ARG_pni1;
    JSValue* temp = it->next(ARG_callFrame);
    if (!temp)
        it->invalidate();
    return temp;
}

void Interpreter::cti_op_push_scope(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSObject* o = ARG_src1->toObject(ARG_callFrame);
    VM_CHECK_EXCEPTION_VOID();
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->push(o));
}

void Interpreter::cti_op_pop_scope(CTI_ARGS)
{
    CTI_STACK_HACK();

    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->pop());
}

JSValue* Interpreter::cti_op_typeof(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsTypeStringForValue(ARG_callFrame, ARG_src1);
}

JSValue* Interpreter::cti_op_is_undefined(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* v = ARG_src1;
    return jsBoolean(JSImmediate::isImmediate(v) ? v->isUndefined() : v->asCell()->structure()->typeInfo().masqueradesAsUndefined());
}

JSValue* Interpreter::cti_op_is_boolean(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(ARG_src1->isBoolean());
}

JSValue* Interpreter::cti_op_is_number(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(ARG_src1->isNumber());
}

JSValue* Interpreter::cti_op_is_string(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(ARG_globalData->interpreter->isJSString(ARG_src1));
}

JSValue* Interpreter::cti_op_is_object(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(jsIsObjectType(ARG_src1));
}

JSValue* Interpreter::cti_op_is_function(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(jsIsFunctionType(ARG_src1));
}

JSValue* Interpreter::cti_op_stricteq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    // handled inline as fast cases
    ASSERT(!JSImmediate::areBothImmediate(src1, src2));
    ASSERT(!(JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate())));

    return jsBoolean(strictEqualSlowCaseInline(src1, src2));
}

JSValue* Interpreter::cti_op_nstricteq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src1 = ARG_src1;
    JSValue* src2 = ARG_src2;

    // handled inline as fast cases
    ASSERT(!JSImmediate::areBothImmediate(src1, src2));
    ASSERT(!(JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate())));
    
    return jsBoolean(!strictEqualSlowCaseInline(src1, src2));
}

JSValue* Interpreter::cti_op_to_jsnumber(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* src = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSValue* result = src->toJSNumber(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Interpreter::cti_op_in(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValue* baseVal = ARG_src2;

    if (!baseVal->isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
        unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "in", baseVal, codeBlock->instructions.begin() + vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSValue* propName = ARG_src1;
    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName->getUInt32(i))
        return jsBoolean(baseObj->hasProperty(callFrame, i));

    Identifier property(callFrame, propName->toString(callFrame));
    VM_CHECK_EXCEPTION();
    return jsBoolean(baseObj->hasProperty(callFrame, property));
}

JSObject* Interpreter::cti_op_push_new_scope(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSObject* scope = new (ARG_globalData) JSStaticScopeObject(ARG_callFrame, *ARG_id1, ARG_src2, DontDelete);

    CallFrame* callFrame = ARG_callFrame;
    callFrame->setScopeChain(callFrame->scopeChain()->push(scope));
    return scope;
}

void Interpreter::cti_op_jmp_scopes(CTI_ARGS)
{
    CTI_STACK_HACK();

    unsigned count = ARG_int1;
    CallFrame* callFrame = ARG_callFrame;

    ScopeChainNode* tmp = callFrame->scopeChain();
    while (count--)
        tmp = tmp->pop();
    callFrame->setScopeChain(tmp);
}

void Interpreter::cti_op_put_by_index(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    unsigned property = ARG_int2;

    ARG_src1->put(callFrame, property, ARG_src3);
}

void* Interpreter::cti_op_switch_imm(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    if (JSImmediate::isNumber(scrutinee)) {
        int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
        return codeBlock->immediateSwitchJumpTables[tableIndex].ctiForValue(value);
    }

    return codeBlock->immediateSwitchJumpTables[tableIndex].ctiDefault;
}

void* Interpreter::cti_op_switch_char(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->characterSwitchJumpTables[tableIndex].ctiDefault;

    if (scrutinee->isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        if (value->size() == 1)
            result = codeBlock->characterSwitchJumpTables[tableIndex].ctiForValue(value->data()[0]);
    }

    return result;
}

void* Interpreter::cti_op_switch_string(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValue* scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    void* result = codeBlock->stringSwitchJumpTables[tableIndex].ctiDefault;

    if (scrutinee->isString()) {
        UString::Rep* value = asString(scrutinee)->value().rep();
        result = codeBlock->stringSwitchJumpTables[tableIndex].ctiForValue(value);
    }

    return result;
}

JSValue* Interpreter::cti_op_del_by_val(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSValue* baseValue = ARG_src1;
    JSObject* baseObj = baseValue->toObject(callFrame); // may throw

    JSValue* subscript = ARG_src2;
    JSValue* result;
    uint32_t i;
    if (subscript->getUInt32(i))
        result = jsBoolean(baseObj->deleteProperty(callFrame, i));
    else {
        VM_CHECK_EXCEPTION();
        Identifier property(callFrame, subscript->toString(callFrame));
        VM_CHECK_EXCEPTION();
        result = jsBoolean(baseObj->deleteProperty(callFrame, property));
    }

    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Interpreter::cti_op_put_getter(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1->isObject());
    JSObject* baseObj = asObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    ASSERT(ARG_src3->isObject());
    baseObj->defineGetter(callFrame, ident, asObject(ARG_src3));
}

void Interpreter::cti_op_put_setter(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1->isObject());
    JSObject* baseObj = asObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    ASSERT(ARG_src3->isObject());
    baseObj->defineSetter(callFrame, ident, asObject(ARG_src3));
}

JSObject* Interpreter::cti_op_new_error(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned type = ARG_int1;
    JSValue* message = ARG_src2;
    unsigned lineNumber = ARG_int3;

    return Error::create(callFrame, static_cast<ErrorType>(type), message->toString(callFrame), lineNumber, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->sourceURL());
}

void Interpreter::cti_op_debug(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    int debugHookID = ARG_int1;
    int firstLine = ARG_int2;
    int lastLine = ARG_int3;

    ARG_globalData->interpreter->debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
}

JSValue* Interpreter::cti_vm_throw(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    JSGlobalData* globalData = ARG_globalData;

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(globalData->exceptionLocation));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(globalData->exceptionLocation);

    JSValue* exceptionValue = globalData->exception;
    ASSERT(exceptionValue);
    globalData->exception = noValue();

    Instruction* handlerVPC = globalData->interpreter->throwException(callFrame, exceptionValue, codeBlock->instructions.begin() + vPCIndex, false);

    if (!handlerVPC) {
        *ARG_exception = exceptionValue;
        return JSImmediate::nullImmediate();
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = callFrame->codeBlock()->nativeExceptionCodeForHandlerVPC(handlerVPC);
    ASSERT(catchRoutine);
    CTI_SET_RETURN_ADDRESS(catchRoutine);
    return exceptionValue;
}

#undef CTI_RETURN_ADDRESS
#undef CTI_SET_RETURN_ADDRESS
#undef CTI_STACK_HACK
#undef VM_CHECK_EXCEPTION
#undef VM_CHECK_EXCEPTION_AT_END
#undef VM_CHECK_EXCEPTION_VOID
#undef VM_THROW_EXCEPTION
#undef VM_THROW_EXCEPTION_2
#undef VM_THROW_EXCEPTION_AT_END

#endif // ENABLE(JIT)

} // namespace JSC
