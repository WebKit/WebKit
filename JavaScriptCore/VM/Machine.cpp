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
#include "collector.h"
#include "debugger.h"
#include "operations.h"
#include "SamplingTool.h"
#include <stdio.h>

#if ENABLE(CTI)
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

#if ENABLE(CTI)

static ALWAYS_INLINE Instruction* vPCForPC(CodeBlock* codeBlock, void* pc)
{
    if (pc >= codeBlock->instructions.begin() && pc < codeBlock->instructions.end())
        return static_cast<Instruction*>(pc);

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(pc));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(pc);
    return codeBlock->instructions.begin() + vPCIndex;
}

#else // ENABLE(CTI)

static ALWAYS_INLINE Instruction* vPCForPC(CodeBlock*, void* pc)
{
    return static_cast<Instruction*>(pc);
}

#endif // ENABLE(CTI)

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
static ALWAYS_INLINE bool fastIsNumber(JSValuePtr value, double& arg)
{
    if (JSImmediate::isNumber(value))
        arg = JSImmediate::getTruncatedInt32(value);
    else if (LIKELY(!JSImmediate::isImmediate(value)) && LIKELY(Heap::isNumber(asCell(value))))
        arg = asNumberCell(value)->value();
    else
        return false;
    return true;
}

// FIXME: Why doesn't JSValuePtr::toInt32 have the Heap::isNumber optimization?
static bool fastToInt32(JSValuePtr value, int32_t& arg)
{
    if (JSImmediate::isNumber(value))
        arg = JSImmediate::getTruncatedInt32(value);
    else if (LIKELY(!JSImmediate::isImmediate(value)) && LIKELY(Heap::isNumber(asCell(value))))
        arg = asNumberCell(value)->toInt32();
    else
        return false;
    return true;
}

static ALWAYS_INLINE bool fastToUInt32(JSValuePtr value, uint32_t& arg)
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

static inline bool jsLess(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) < JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    if (fastIsNumber(v1, n1) && fastIsNumber(v2, n2))
        return n1 < n2;

    Machine* machine = callFrame->machine();
    if (machine->isJSString(v1) && machine->isJSString(v2))
        return asString(v1)->value() < asString(v2)->value();

    JSValuePtr p1;
    JSValuePtr p2;
    bool wasNotString1 = v1->getPrimitiveNumber(callFrame, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(callFrame, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return asString(p1)->value() < asString(p2)->value();
}

static inline bool jsLessEq(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) <= JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    if (fastIsNumber(v1, n1) && fastIsNumber(v2, n2))
        return n1 <= n2;

    Machine* machine = callFrame->machine();
    if (machine->isJSString(v1) && machine->isJSString(v2))
        return !(asString(v2)->value() < asString(v1)->value());

    JSValuePtr p1;
    JSValuePtr p2;
    bool wasNotString1 = v1->getPrimitiveNumber(callFrame, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(callFrame, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(asString(p2)->value() < asString(p1)->value());
}

static NEVER_INLINE JSValuePtr jsAddSlowCase(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
{
    // exception for the Date exception in defaultValue()
    JSValuePtr p1 = v1->toPrimitive(callFrame);
    JSValuePtr p2 = v2->toPrimitive(callFrame);

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

static ALWAYS_INLINE JSValuePtr jsAdd(CallFrame* callFrame, JSValuePtr v1, JSValuePtr v2)
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

static JSValuePtr jsTypeStringForValue(CallFrame* callFrame, JSValuePtr v)
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
        if (asObject(v)->structureID()->typeInfo().masqueradesAsUndefined())
            return jsNontrivialString(callFrame, "undefined");
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return jsNontrivialString(callFrame, "function");
    }
    return jsNontrivialString(callFrame, "object");
}

static bool jsIsObjectType(JSValuePtr v)
{
    if (JSImmediate::isImmediate(v))
        return v->isNull();

    JSType type = asCell(v)->structureID()->typeInfo().type();
    if (type == NumberType || type == StringType)
        return false;
    if (type == ObjectType) {
        if (asObject(v)->structureID()->typeInfo().masqueradesAsUndefined())
            return false;
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return false;
    }
    return true;
}

static bool jsIsFunctionType(JSValuePtr v)
{
    if (v->isObject()) {
        CallData callData;
        if (asObject(v)->getCallData(callData) != CallTypeNone)
            return true;
    }
    return false;
}

NEVER_INLINE bool Machine::resolve(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
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

NEVER_INLINE bool Machine::resolveSkip(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
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

NEVER_INLINE bool Machine::resolveGlobal(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>((vPC + 2)->u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int property = (vPC + 3)->u.operand;
    StructureID* structureID = (vPC + 4)->u.structureID;
    int offset = (vPC + 5)->u.operand;

    if (structureID == globalObject->structureID()) {
        callFrame[dst] = globalObject->getDirectOffset(offset);
        return true;
    }

    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& ident = codeBlock->identifiers[property];
    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValuePtr result = slot.getValue(callFrame, ident);
        if (slot.isCacheable()) {
            if (vPC[4].u.structureID)
                vPC[4].u.structureID->deref();
            globalObject->structureID()->ref();
            vPC[4] = globalObject->structureID();
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

NEVER_INLINE void Machine::resolveBase(CallFrame* callFrame, Instruction* vPC)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    callFrame[dst] = inlineResolveBase(callFrame, callFrame->codeBlock()->identifiers[property], callFrame->scopeChain());
}

NEVER_INLINE bool Machine::resolveBaseAndProperty(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
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

NEVER_INLINE bool Machine::resolveBaseAndFunc(CallFrame* callFrame, Instruction* vPC, JSValuePtr& exceptionValue)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
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

ALWAYS_INLINE CallFrame* Machine::slideRegisterWindowForCall(CodeBlock* newCodeBlock, RegisterFile* registerFile, CallFrame* callFrame, size_t registerOffset, int argc)
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

static NEVER_INLINE bool isNotObject(CallFrame* callFrame, bool forInstanceOf, CodeBlock* codeBlock, const Instruction* vPC, JSValuePtr value, JSValuePtr& exceptionData)
{
    if (value->isObject())
        return false;
    exceptionData = createInvalidParamError(callFrame, forInstanceOf ? "instanceof" : "in" , value, vPC, codeBlock);
    return true;
}

NEVER_INLINE JSValuePtr Machine::callEval(CallFrame* callFrame, JSObject* thisObj, ScopeChainNode* scopeChain, RegisterFile* registerFile, int argv, int argc, JSValuePtr& exceptionValue)
{
    if (argc < 2)
        return jsUndefined();

    JSValuePtr program = callFrame[argv + 1].jsValue(callFrame);

    if (!program->isString())
        return program;

    UString programSource = asString(program)->value();

    CodeBlock* codeBlock = callFrame->codeBlock();
    RefPtr<EvalNode> evalNode = codeBlock->evalCodeCache.get(callFrame, programSource, scopeChain, exceptionValue);

    JSValuePtr result = jsUndefined();
    if (evalNode)
        result = callFrame->globalData().machine->execute(evalNode.get(), callFrame, thisObj, callFrame->registers() - registerFile->start() + argv + 1 + RegisterFile::CallFrameHeaderSize, scopeChain, &exceptionValue);

    return result;
}

Machine::Machine()
    : m_sampler(0)
#if ENABLE(CTI)
    , m_ctiArrayLengthTrampoline(0)
    , m_ctiStringLengthTrampoline(0)
    , m_jitCodeBuffer(new JITCodeBuffer(1024 * 1024))
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

    JSCell* jsArray = new (storage) JSArray(JSArray::createStructureID(jsNull()));
    m_jsArrayVptr = jsArray->vptr();
    jsArray->~JSCell();

    JSCell* jsString = new (storage) JSString(JSString::VPtrStealingHack);
    m_jsStringVptr = jsString->vptr();
    jsString->~JSCell();

    JSCell* jsFunction = new (storage) JSFunction(JSFunction::createStructureID(jsNull()));
    m_jsFunctionVptr = jsFunction->vptr();
    jsFunction->~JSCell();
    
    fastFree(storage);
}

Machine::~Machine()
{
#if ENABLE(CTI)
    if (m_ctiArrayLengthTrampoline)
        fastFree(m_ctiArrayLengthTrampoline);
    if (m_ctiStringLengthTrampoline)
        fastFree(m_ctiStringLengthTrampoline);
#endif
}

#ifndef NDEBUG

void Machine::dumpCallFrame(const RegisterFile* registerFile, CallFrame* callFrame)
{
    JSGlobalObject* globalObject = callFrame->scopeChain()->globalObject();

    CodeBlock* codeBlock = callFrame->codeBlock();
    codeBlock->dump(globalObject->globalExec());

    dumpRegisters(registerFile, callFrame);
}

void Machine::dumpRegisters(const RegisterFile* registerFile, CallFrame* callFrame)
{
    printf("Register frame: \n\n");
    printf("----------------------------------------------------\n");
    printf("            use            |   address  |   value   \n");
    printf("----------------------------------------------------\n");

    CodeBlock* codeBlock = callFrame->codeBlock();
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

bool Machine::isOpcode(Opcode opcode)
{
#if HAVE(COMPUTED_GOTO)
    return opcode != HashTraits<Opcode>::emptyValue()
        && !HashTraits<Opcode>::isDeletedValue(opcode)
        && m_opcodeIDTable.contains(opcode);
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}

NEVER_INLINE bool Machine::unwindCallFrame(CallFrame*& callFrame, JSValuePtr exceptionValue, const Instruction*& vPC, CodeBlock*& codeBlock)
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

NEVER_INLINE Instruction* Machine::throwException(CallFrame*& callFrame, JSValuePtr& exceptionValue, const Instruction* vPC, bool explicitThrow)
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
        if (isCallOpcode(vPC[0].u.opcode))
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

JSValuePtr Machine::execute(ProgramNode* programNode, CallFrame* callFrame, ScopeChainNode* scopeChain, JSObject* thisObj, JSValuePtr* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    CodeBlock* codeBlock = &programNode->byteCode(scopeChain);

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

    m_reentryDepth++;
#if ENABLE(CTI)
    if (!codeBlock->ctiCode)
        CTI::compile(this, newCallFrame, codeBlock);
    JSValuePtr result = CTI::execute(codeBlock->ctiCode, &m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
    JSValuePtr result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler)
        (*profiler)->didExecute(callFrame, programNode->sourceURL(), programNode->lineNo());

    if (m_reentryDepth && lastGlobalObject && globalObject != lastGlobalObject)
        lastGlobalObject->copyGlobalsTo(m_registerFile);

    m_registerFile.shrink(oldEnd);

    return result;
}

JSValuePtr Machine::execute(FunctionBodyNode* functionBodyNode, CallFrame* callFrame, JSFunction* function, JSObject* thisObj, const ArgList& args, ScopeChainNode* scopeChain, JSValuePtr* exception)
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

    CodeBlock* codeBlock = &functionBodyNode->byteCode(scopeChain);
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

    m_reentryDepth++;
#if ENABLE(CTI)
    if (!codeBlock->ctiCode)
        CTI::compile(this, newCallFrame, codeBlock);
    JSValuePtr result = CTI::execute(codeBlock->ctiCode, &m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
    JSValuePtr result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
    m_reentryDepth--;

    if (*profiler)
        (*profiler)->didExecute(newCallFrame, function);

    MACHINE_SAMPLING_privateExecuteReturned();

    m_registerFile.shrink(oldEnd);
    return result;
}

JSValuePtr Machine::execute(EvalNode* evalNode, CallFrame* callFrame, JSObject* thisObj, ScopeChainNode* scopeChain, JSValuePtr* exception)
{
    return execute(evalNode, callFrame, thisObj, m_registerFile.size() + evalNode->byteCode(scopeChain).numParameters + RegisterFile::CallFrameHeaderSize, scopeChain, exception);
}

JSValuePtr Machine::execute(EvalNode* evalNode, CallFrame* callFrame, JSObject* thisObj, int registerOffset, ScopeChainNode* scopeChain, JSValuePtr* exception)
{
    ASSERT(!scopeChain->globalData->exception);

    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    DynamicGlobalObjectScope globalObjectScope(callFrame, callFrame->globalData().dynamicGlobalObject ? callFrame->globalData().dynamicGlobalObject : scopeChain->globalObject());

    EvalCodeBlock* codeBlock = &evalNode->byteCode(scopeChain);

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

        const Node::VarStack& varStack = codeBlock->ownerNode->varStack();
        Node::VarStack::const_iterator varStackEnd = varStack.end();
        for (Node::VarStack::const_iterator it = varStack.begin(); it != varStackEnd; ++it) {
            const Identifier& ident = (*it).first;
            if (!variableObject->hasProperty(callFrame, ident)) {
                PutPropertySlot slot;
                variableObject->put(callFrame, ident, jsUndefined(), slot);
            }
        }

        const Node::FunctionStack& functionStack = codeBlock->ownerNode->functionStack();
        Node::FunctionStack::const_iterator functionStackEnd = functionStack.end();
        for (Node::FunctionStack::const_iterator it = functionStack.begin(); it != functionStackEnd; ++it) {
            PutPropertySlot slot;
            variableObject->put(callFrame, (*it)->m_ident, (*it)->makeFunction(callFrame, scopeChain), slot);
        }

    }

    Register* oldEnd = m_registerFile.end();
    Register* newEnd = m_registerFile.start() + registerOffset + codeBlock->numCalleeRegisters;
    if (!m_registerFile.grow(newEnd)) {
        *exception = createStackOverflowError(callFrame);
        return jsNull();
    }

    CallFrame* newCallFrame = CallFrame::create(m_registerFile.start() + registerOffset);

    // a 0 codeBlock indicates a built-in caller
    newCallFrame[codeBlock->thisRegister] = thisObj;
    newCallFrame->init(codeBlock, 0, scopeChain, callFrame->addHostCallFrameFlag(), 0, 0, 0);

    if (codeBlock->needsFullScopeChain)
        scopeChain->ref();

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(newCallFrame, evalNode->sourceURL(), evalNode->lineNo());

    m_reentryDepth++;
#if ENABLE(CTI)
    if (!codeBlock->ctiCode)
        CTI::compile(this, newCallFrame, codeBlock);
    JSValuePtr result = CTI::execute(codeBlock->ctiCode, &m_registerFile, newCallFrame, scopeChain->globalData, exception);
#else
    JSValuePtr result = privateExecute(Normal, &m_registerFile, newCallFrame, exception);
#endif
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler)
        (*profiler)->didExecute(callFrame, evalNode->sourceURL(), evalNode->lineNo());

    m_registerFile.shrink(oldEnd);
    return result;
}

NEVER_INLINE void Machine::debug(CallFrame* callFrame, DebugHookID debugHookID, int firstLine, int lastLine)
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

void Machine::resetTimeoutCheck()
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
ALWAYS_INLINE JSValuePtr Machine::checkTimeout(JSGlobalObject* globalObject)
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

NEVER_INLINE ScopeChainNode* Machine::createExceptionScope(CallFrame* callFrame, const Instruction* vPC)
{
    int dst = (++vPC)->u.operand;
    CodeBlock* codeBlock = callFrame->codeBlock();
    Identifier& property = codeBlock->identifiers[(++vPC)->u.operand];
    JSValuePtr value = callFrame[(++vPC)->u.operand].jsValue(callFrame);
    JSObject* scope = new (callFrame) JSStaticScopeObject(callFrame, property, value, DontDelete);
    callFrame[dst] = scope;

    return callFrame->scopeChain()->push(scope);
}

static StructureIDChain* cachePrototypeChain(CallFrame* callFrame, StructureID* structureID)
{
    JSValuePtr prototype = structureID->prototypeForLookup(callFrame);
    if (JSImmediate::isImmediate(prototype))
        return 0;
    RefPtr<StructureIDChain> chain = StructureIDChain::create(asObject(prototype)->structureID());
    structureID->setCachedPrototypeChain(chain.release());
    return structureID->cachedPrototypeChain();
}

NEVER_INLINE void Machine::tryCachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValuePtr baseValue, const PutPropertySlot& slot)
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
    StructureID* structureID = baseCell->structureID();

    if (structureID->isDictionary()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Cache miss: record StructureID to compare against next time.
    StructureID* lastStructureID = vPC[4].u.structureID;
    if (structureID != lastStructureID) {
        // First miss: record StructureID to compare against next time.
        if (!lastStructureID) {
            vPC[4] = structureID;
            return;
        }

        // Second miss: give up.
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // Cache hit: Specialize instruction and ref StructureIDs.

    // If baseCell != slot.base(), then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        vPC[0] = getOpcode(op_put_by_id_generic);
        return;
    }

    // StructureID transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        vPC[0] = getOpcode(op_put_by_id_transition);
        vPC[4] = structureID->previousID();
        vPC[5] = structureID;
        StructureIDChain* chain = structureID->cachedPrototypeChain();
        if (!chain) {
            chain = cachePrototypeChain(callFrame, structureID);
            if (!chain) {
                // This happens if someone has manually inserted null into the prototype chain
                vPC[0] = getOpcode(op_put_by_id_generic);
                return;
            }
        }
        vPC[6] = chain;
        vPC[7] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);
        return;
    }

    vPC[0] = getOpcode(op_put_by_id_replace);
    vPC[5] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);
}

NEVER_INLINE void Machine::uncachePutByID(CodeBlock* codeBlock, Instruction* vPC)
{
    codeBlock->derefStructureIDs(vPC);
    vPC[0] = getOpcode(op_put_by_id);
    vPC[4] = 0;
}

NEVER_INLINE void Machine::tryCacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, Instruction* vPC, JSValuePtr baseValue, const Identifier& propertyName, const PropertySlot& slot)
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

    StructureID* structureID = asCell(baseValue)->structureID();

    if (structureID->isDictionary()) {
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    // Cache miss
    StructureID* lastStructureID = vPC[4].u.structureID;
    if (structureID != lastStructureID) {
        // First miss: record StructureID to compare against next time.
        if (!lastStructureID) {
            vPC[4] = structureID;
            return;
        }

        // Second miss: give up.
        vPC[0] = getOpcode(op_get_by_id_generic);
        return;
    }

    // Cache hit: Specialize instruction and ref StructureIDs.

    if (slot.slotBase() == baseValue) {
        vPC[0] = getOpcode(op_get_by_id_self);
        vPC[5] = slot.cachedOffset();

        codeBlock->refStructureIDs(vPC);
        return;
    }

    if (slot.slotBase() == structureID->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase()->isObject());

        JSObject* baseObject = asObject(slot.slotBase());

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (baseObject->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(baseObject->structureID());
            baseObject->setStructureID(transition.release());
            asCell(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[5] = baseObject->structureID();
        vPC[6] = slot.cachedOffset();

        codeBlock->refStructureIDs(vPC);
        return;
    }

    size_t count = 0;
    JSObject* o = asObject(baseValue);
    while (slot.slotBase() != o) {
        JSValuePtr v = o->structureID()->prototypeForLookup(callFrame);

        // If we didn't find base in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.
        if (v->isNull()) {
            vPC[0] = getOpcode(op_get_by_id_generic);
            return;
        }

        o = asObject(v);

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (o->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(o->structureID());
            o->setStructureID(transition.release());
            asObject(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        ++count;
    }

    StructureIDChain* chain = structureID->cachedPrototypeChain();
    if (!chain)
        chain = cachePrototypeChain(callFrame, structureID);
    ASSERT(chain);

    vPC[0] = getOpcode(op_get_by_id_chain);
    vPC[4] = structureID;
    vPC[5] = chain;
    vPC[6] = count;
    vPC[7] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);
}

NEVER_INLINE void Machine::uncacheGetByID(CodeBlock* codeBlock, Instruction* vPC)
{
    codeBlock->derefStructureIDs(vPC);
    vPC[0] = getOpcode(op_get_by_id);
    vPC[4] = 0;
}

JSValuePtr Machine::privateExecute(ExecutionFlag flag, RegisterFile* registerFile, CallFrame* callFrame, JSValuePtr* exception)
{
    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function.
    if (flag == InitializeAndReturn) {
        #if HAVE(COMPUTED_GOTO)
            #define ADD_OPCODE(id) m_opcodeTable[id] = &&id;
                FOR_EACH_OPCODE_ID(ADD_OPCODE);
            #undef ADD_OPCODE

            #define ADD_OPCODE_ID(id) m_opcodeIDTable.add(&&id, id);
                FOR_EACH_OPCODE_ID(ADD_OPCODE_ID);
            #undef ADD_OPCODE_ID
            ASSERT(m_opcodeIDTable.size() == numOpcodeIDs);
            op_throw_end_indirect = &&op_throw_end;
            op_call_indirect = &&op_call;
        #endif // HAVE(COMPUTED_GOTO)
        return noValue();
    }

#if ENABLE(CTI)
    // Currently with CTI enabled we never interpret functions
    ASSERT_NOT_REACHED();
#endif

    JSGlobalData* globalData = &callFrame->globalData();
    JSValuePtr exceptionValue = noValue();
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

#if DUMP_OPCODE_STATS
    OpcodeStats::resetLastInstruction();
#endif

#define CHECK_FOR_TIMEOUT() \
    if (!--tickCount) { \
        if ((exceptionValue = checkTimeout(callFrame->dynamicGlobalObject()))) \
            goto vm_throw; \
        tickCount = m_ticksUntilNextTimeoutCheck; \
    }

#if HAVE(COMPUTED_GOTO)
    #define NEXT_OPCODE MACHINE_SAMPLING_sample(callFrame->codeBlock(), vPC); goto *vPC->u.opcode
#if DUMP_OPCODE_STATS
    #define BEGIN_OPCODE(opcode) opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define BEGIN_OPCODE(opcode) opcode:
#endif
    NEXT_OPCODE;
#else
    #define NEXT_OPCODE MACHINE_SAMPLING_sample(callFrame->codeBlock(), vPC); goto interpreterLoopStart
#if DUMP_OPCODE_STATS
    #define BEGIN_OPCODE(opcode) case opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define BEGIN_OPCODE(opcode) case opcode:
#endif
    while (1) { // iterator loop begins
    interpreterLoopStart:;
    switch (vPC->u.opcode)
#endif
    {
    BEGIN_OPCODE(op_new_object) {
        /* new_object dst(r)

           Constructs a new empty Object instance using the original
           constructor, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        callFrame[dst] = constructEmptyObject(callFrame);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_array) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int regExp = (++vPC)->u.operand;
        callFrame[dst] = new (globalData) RegExpObject(callFrame->scopeChain()->globalObject()->regExpStructure(), callFrame->codeBlock()->regexps[regExp]);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = callFrame[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = jsBoolean(src1 == src2);
        else {
            JSValuePtr result = jsBoolean(equalSlowCase(callFrame, src1, src2));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_eq_null) {
        /* eq_null dst(r) src(r)

           Checks whether register src is null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (src->isUndefinedOrNull()) {
            callFrame[dst] = jsBoolean(true);
            ++vPC;
            NEXT_OPCODE;
        }
        
        callFrame[dst] = jsBoolean(!JSImmediate::isImmediate(src) && src->asCell()->structureID()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = jsBoolean(src1 != src2);
        else {
            JSValuePtr result = jsBoolean(!equalSlowCase(callFrame, src1, src2));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_neq_null) {
        /* neq_null dst(r) src(r)

           Checks whether register src is not null, as with the ECMAScript '!='
           operator, and puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (src->isUndefinedOrNull()) {
            callFrame[dst] = jsBoolean(false);
            ++vPC;
            NEXT_OPCODE;
        }
        
        callFrame[dst] = jsBoolean(JSImmediate::isImmediate(src) || !asCell(src)->structureID()->typeInfo().masqueradesAsUndefined());
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediate(src1, src2))
            callFrame[dst] = jsBoolean(src1 == src2);
        else if (JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate()))
            callFrame[dst] = jsBoolean(false);
        else
            callFrame[dst] = jsBoolean(strictEqualSlowCase(src1, src2));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);

        if (JSImmediate::areBothImmediate(src1, src2))
            callFrame[dst] = jsBoolean(src1 != src2);
        else if (JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate()))
            callFrame[dst] = jsBoolean(true);
        else
            callFrame[dst] = jsBoolean(!strictEqualSlowCase(src1, src2));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr result = jsBoolean(jsLess(callFrame, src1, src2));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr result = jsBoolean(jsLessEq(callFrame, src1, src2));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            callFrame[srcDst] = JSImmediate::incImmediateNumber(v);
        else {
            JSValuePtr result = jsNumber(callFrame, v->toNumber(callFrame) + 1);
            VM_CHECK_EXCEPTION();
            callFrame[srcDst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            callFrame[srcDst] = JSImmediate::decImmediateNumber(v);
        else {
            JSValuePtr result = jsNumber(callFrame, v->toNumber(callFrame) - 1);
            VM_CHECK_EXCEPTION();
            callFrame[srcDst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            callFrame[dst] = v;
            callFrame[srcDst] = JSImmediate::incImmediateNumber(v);
        } else {
            JSValuePtr number = callFrame[srcDst].jsValue(callFrame)->toJSNumber(callFrame);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = number;
            callFrame[srcDst] = jsNumber(callFrame, number->uncheckedGetNumber() + 1);
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValuePtr v = callFrame[srcDst].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            callFrame[dst] = v;
            callFrame[srcDst] = JSImmediate::decImmediateNumber(v);
        } else {
            JSValuePtr number = callFrame[srcDst].jsValue(callFrame)->toJSNumber(callFrame);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = number;
            callFrame[srcDst] = jsNumber(callFrame, number->uncheckedGetNumber() - 1);
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;

        JSValuePtr srcVal = callFrame[src].jsValue(callFrame);

        if (LIKELY(srcVal->isNumber()))
            callFrame[dst] = callFrame[src];
        else {
            JSValuePtr result = srcVal->toJSNumber(callFrame);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        double v;
        if (fastIsNumber(src, v))
            callFrame[dst] = jsNumber(callFrame, -v);
        else {
            JSValuePtr result = jsNumber(callFrame, -src->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            callFrame[dst] = JSImmediate::addImmediateNumbers(src1, src2);
        else {
            JSValuePtr result = jsAdd(callFrame, src1, src2);
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src1 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr src2 = callFrame[(++vPC)->u.operand].jsValue(callFrame);
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
            JSValuePtr result = jsNumber(callFrame, src1->toNumber(callFrame) * src2->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_div) {
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
        if (fastIsNumber(dividend, left) && fastIsNumber(divisor, right))
            callFrame[dst] = jsNumber(callFrame, left / right);
        else {
            JSValuePtr result = jsNumber(callFrame, dividend->toNumber(callFrame) / divisor->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mod) {
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

        if (JSImmediate::areBothImmediateNumbers(dividendValue, divisorValue) && divisorValue != JSImmediate::from(0)) {
            callFrame[dst] = JSImmediate::from(JSImmediate::getTruncatedInt32(dividendValue) % JSImmediate::getTruncatedInt32(divisorValue));
            ++vPC;
            NEXT_OPCODE;
        }

        double d = dividendValue->toNumber(callFrame);
        JSValuePtr result = jsNumber(callFrame, fmod(d, divisorValue->toNumber(callFrame)));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sub) {
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
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            callFrame[dst] = JSImmediate::subImmediateNumbers(src1, src2);
        else if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
            callFrame[dst] = jsNumber(callFrame, left - right);
        else {
            JSValuePtr result = jsNumber(callFrame, src1->toNumber(callFrame) - src2->toNumber(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lshift) {
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
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            callFrame[dst] = jsNumber(callFrame, JSImmediate::getTruncatedInt32(val) << (JSImmediate::getTruncatedUInt32(shift) & 0x1f));
        else if (fastToInt32(val, left) && fastToUInt32(shift, right))
            callFrame[dst] = jsNumber(callFrame, left << (right & 0x1f));
        else {
            JSValuePtr result = jsNumber(callFrame, (val->toInt32(callFrame)) << (shift->toUInt32(callFrame) & 0x1f));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_rshift) {
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
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            callFrame[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else if (fastToInt32(val, left) && fastToUInt32(shift, right))
            callFrame[dst] = jsNumber(callFrame, left >> (right & 0x1f));
        else {
            JSValuePtr result = jsNumber(callFrame, (val->toInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr val = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        JSValuePtr shift = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
            callFrame[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else {
            JSValuePtr result = jsNumber(callFrame, (val->toUInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitand) {
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
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = JSImmediate::andImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            callFrame[dst] = jsNumber(callFrame, left & right);
        else {
            JSValuePtr result = jsNumber(callFrame, src1->toInt32(callFrame) & src2->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitxor) {
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
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = JSImmediate::xorImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            callFrame[dst] = jsNumber(callFrame, left ^ right);
        else {
            JSValuePtr result = jsNumber(callFrame, src1->toInt32(callFrame) ^ src2->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitor) {
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
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            callFrame[dst] = JSImmediate::orImmediateNumbers(src1, src2);
        else if (fastToInt32(src1, left) && fastToInt32(src2, right))
            callFrame[dst] = jsNumber(callFrame, left | right);
        else {
            JSValuePtr result = jsNumber(callFrame, src1->toInt32(callFrame) | src2->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValuePtr src = callFrame[(++vPC)->u.operand].jsValue(callFrame);
        int32_t value;
        if (fastToInt32(src, value))
            callFrame[dst] = jsNumber(callFrame, ~value);
        else {
            JSValuePtr result = jsNumber(callFrame, ~src->toInt32(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = result;
        }
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_not) {
        /* not dst(r) src(r)

           Computes logical NOT of register src (converted to
           boolean), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValuePtr result = jsBoolean(!callFrame[src].jsValue(callFrame)->toBoolean(callFrame));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_instanceof) {
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
        callFrame[dst] = jsBoolean(baseObj->structureID()->typeInfo().implementsHasInstance() ? baseObj->hasInstance(callFrame, callFrame[value].jsValue(callFrame), callFrame[baseProto].jsValue(callFrame)) : false);

        vPC += 5;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsTypeStringForValue(callFrame, callFrame[src].jsValue(callFrame));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_undefined) {
        /* is_undefined dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "undefined", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValuePtr v = callFrame[src].jsValue(callFrame);
        callFrame[dst] = jsBoolean(JSImmediate::isImmediate(v) ? v->isUndefined() : v->asCell()->structureID()->typeInfo().masqueradesAsUndefined());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_boolean) {
        /* is_boolean dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "boolean", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame)->isBoolean());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_number) {
        /* is_number dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "number", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame)->isNumber());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_string) {
        /* is_string dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "string", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(callFrame[src].jsValue(callFrame)->isString());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_object) {
        /* is_object dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "object", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(jsIsObjectType(callFrame[src].jsValue(callFrame)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_is_function) {
        /* is_function dst(r) src(r)

           Determines whether the type string for src according to
           the ECMAScript rules is "function", and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = jsBoolean(jsIsFunctionType(callFrame[src].jsValue(callFrame)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_in) {
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
        if (propName->getUInt32(i))
            callFrame[dst] = jsBoolean(baseObj->hasProperty(callFrame, i));
        else {
            Identifier property(callFrame, propName->toString(callFrame));
            VM_CHECK_EXCEPTION();
            callFrame[dst] = jsBoolean(baseObj->hasProperty(callFrame, property));
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve) {
        /* resolve dst(r) property(id)

           Looks up the property named by identifier property in the
           scope chain, and writes the resulting value to register
           dst. If the property is not found, raises an exception.
        */
        if (UNLIKELY(!resolve(callFrame, vPC, exceptionValue)))
            goto vm_throw;

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_skip) {
        /* resolve_skip dst(r) property(id) skip(n)

         Looks up the property named by identifier property in the
         scope chain skipping the top 'skip' levels, and writes the resulting
         value to register dst. If the property is not found, raises an exception.
         */
        if (UNLIKELY(!resolveSkip(callFrame, vPC, exceptionValue)))
            goto vm_throw;

        vPC += 4;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_global) {
        /* resolve_skip dst(r) globalObject(c) property(id) structureID(sID) offset(n)
         
           Performs a dynamic property lookup for the given property, on the provided
           global object.  If structureID matches the StructureID of the global then perform
           a fast lookup using the case offset, otherwise fall back to a full resolve and
           cache the new structureID and offset
         */
        if (UNLIKELY(!resolveGlobal(callFrame, vPC, exceptionValue)))
            goto vm_throw;
        
        vPC += 6;
        
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_global_var) {
        /* get_global_var dst(r) globalObject(c) index(n)

           Gets the global var at global slot index and places it in register dst.
         */
        int dst = (++vPC)->u.operand;
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;

        callFrame[dst] = scope->registerAt(index);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_global_var) {
        /* put_global_var globalObject(c) index(n) value(r)
         
           Puts value into global slot index.
         */
        JSGlobalObject* scope = static_cast<JSGlobalObject*>((++vPC)->u.jsCell);
        ASSERT(scope->isGlobalObject());
        int index = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        
        scope->registerAt(index) = callFrame[value].jsValue(callFrame);
        ++vPC;
        NEXT_OPCODE;
    }            
    BEGIN_OPCODE(op_get_scoped_var) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_scoped_var) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base) {
        /* resolve_base dst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register dst. If none is found, the outermost scope (which
           will be the global object) is stored in register dst.
        */
        resolveBase(callFrame, vPC);

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_with_base) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_func) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id) {
        /* get_by_id dst(r) base(r) property(id) structureID(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        CodeBlock* codeBlock = callFrame->codeBlock();
        Identifier& ident = codeBlock->identifiers[property];
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        PropertySlot slot(baseValue);
        JSValuePtr result = baseValue->get(callFrame, ident, slot);
        VM_CHECK_EXCEPTION();

        tryCacheGetByID(callFrame, codeBlock, vPC, baseValue, ident, slot);

        callFrame[dst] = result;
        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_self) {
        /* op_get_by_id_self dst(r) base(r) property(id) structureID(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base. If the cache misses, op_get_by_id_self reverts to
           op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int dst = vPC[1].u.operand;
                int offset = vPC[5].u.operand;

                ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifiers[vPC[3].u.operand]) == baseObject->getDirectOffset(offset));
                callFrame[dst] = baseObject->getDirectOffset(offset);

                vPC += 8;
                NEXT_OPCODE;
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_proto) {
        /* op_get_by_id_proto dst(r) base(r) property(id) structureID(sID) protoStructureID(sID) offset(n) nop(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype. If the cache misses, op_get_by_id_proto
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                ASSERT(structureID->prototypeForLookup(callFrame)->isObject());
                JSObject* protoObject = asObject(structureID->prototypeForLookup(callFrame));
                StructureID* protoStructureID = vPC[5].u.structureID;

                if (LIKELY(protoObject->structureID() == protoStructureID)) {
                    int dst = vPC[1].u.operand;
                    int offset = vPC[6].u.operand;

                    ASSERT(protoObject->get(callFrame, callFrame->codeBlock()->identifiers[vPC[3].u.operand]) == protoObject->getDirectOffset(offset));
                    callFrame[dst] = protoObject->getDirectOffset(offset);

                    vPC += 8;
                    NEXT_OPCODE;
                }
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_chain) {
        /* op_get_by_id_chain dst(r) base(r) property(id) structureID(sID) structureIDChain(sIDc) count(n) offset(n)

           Cached property access: Attempts to get a cached property from the
           value base's prototype chain. If the cache misses, op_get_by_id_chain
           reverts to op_get_by_id.
        */
        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                RefPtr<StructureID>* it = vPC[5].u.structureIDChain->head();
                size_t count = vPC[6].u.operand;
                RefPtr<StructureID>* end = it + count;

                JSObject* baseObject = asObject(baseCell);
                while (1) {
                    baseObject = asObject(baseObject->structureID()->prototypeForLookup(callFrame));
                    if (UNLIKELY(baseObject->structureID() != (*it).get()))
                        break;

                    if (++it == end) {
                        int dst = vPC[1].u.operand;
                        int offset = vPC[7].u.operand;

                        ASSERT(baseObject->get(callFrame, callFrame->codeBlock()->identifiers[vPC[3].u.operand]) == baseObject->getDirectOffset(offset));
                        callFrame[dst] = baseObject->getDirectOffset(offset);

                        vPC += 8;
                        NEXT_OPCODE;
                    }
                }
            }
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id_generic) {
        /* op_get_by_id_generic dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Generic property access: Gets the property named by identifier
           property from the value base, and puts the result in register dst.
        */
        int dst = vPC[1].u.operand;
        int base = vPC[2].u.operand;
        int property = vPC[3].u.operand;

        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        PropertySlot slot(baseValue);
        JSValuePtr result = baseValue->get(callFrame, ident, slot);
        VM_CHECK_EXCEPTION();

        callFrame[dst] = result;
        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_array_length) {
        /* op_get_array_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the array in register base,
           and puts the result in register dst. If register base does not hold
           an array, op_get_array_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        if (LIKELY(isJSArray(baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame[dst] = jsNumber(callFrame, asArray(baseValue)->length());
            vPC += 8;
            NEXT_OPCODE;
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_string_length) {
        /* op_get_string_length dst(r) base(r) property(id) nop(sID) nop(n) nop(n) nop(n)

           Cached property access: Gets the length of the string in register base,
           and puts the result in register dst. If register base does not hold
           a string, op_get_string_length reverts to op_get_by_id.
        */

        int base = vPC[2].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        if (LIKELY(isJSString(baseValue))) {
            int dst = vPC[1].u.operand;
            callFrame[dst] = jsNumber(callFrame, asString(baseValue)->value().size());
            vPC += 8;
            NEXT_OPCODE;
        }

        uncacheGetByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id) {
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
        Identifier& ident = codeBlock->identifiers[property];
        PutPropertySlot slot;
        baseValue->put(callFrame, ident, callFrame[value].jsValue(callFrame), slot);
        VM_CHECK_EXCEPTION();

        tryCachePutByID(callFrame, codeBlock, vPC, baseValue, slot);

        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id_transition) {
        /* op_put_by_id_transition base(r) property(id) value(r) oldStructureID(sID) newStructureID(sID) structureIDChain(sIDc) offset(n)
         
           Cached property access: Attempts to set a new property with a cached transition
           property named by identifier property, belonging to register base,
           to register value. If the cache misses, op_put_by_id_transition
           reverts to op_put_by_id_generic.
         
           Unlike many opcodes, this one does not write any output to
           the register file.
         */
        int base = vPC[1].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);
        
        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            StructureID* oldStructureID = vPC[4].u.structureID;
            StructureID* newStructureID = vPC[5].u.structureID;
            
            if (LIKELY(baseCell->structureID() == oldStructureID)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);

                RefPtr<StructureID>* it = vPC[6].u.structureIDChain->head();

                JSValuePtr proto = baseObject->structureID()->prototypeForLookup(callFrame);
                while (!proto->isNull()) {
                    if (UNLIKELY(asObject(proto)->structureID() != (*it).get())) {
                        uncachePutByID(callFrame->codeBlock(), vPC);
                        NEXT_OPCODE;
                    }
                    ++it;
                    proto = asObject(proto)->structureID()->prototypeForLookup(callFrame);
                }

                baseObject->transitionTo(newStructureID);

                int value = vPC[3].u.operand;
                unsigned offset = vPC[7].u.operand;
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifiers[vPC[2].u.operand])) == offset);
                baseObject->putDirectOffset(offset, callFrame[value].jsValue(callFrame));

                vPC += 8;
                NEXT_OPCODE;
            }
        }
        
        uncachePutByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id_replace) {
        /* op_put_by_id_replace base(r) property(id) value(r) structureID(sID) offset(n) nop(n) nop(n)

           Cached property access: Attempts to set a pre-existing, cached
           property named by identifier property, belonging to register base,
           to register value. If the cache misses, op_put_by_id_replace
           reverts to op_put_by_id.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = vPC[1].u.operand;
        JSValuePtr baseValue = callFrame[base].jsValue(callFrame);

        if (LIKELY(!JSImmediate::isImmediate(baseValue))) {
            JSCell* baseCell = asCell(baseValue);
            StructureID* structureID = vPC[4].u.structureID;

            if (LIKELY(baseCell->structureID() == structureID)) {
                ASSERT(baseCell->isObject());
                JSObject* baseObject = asObject(baseCell);
                int value = vPC[3].u.operand;
                unsigned offset = vPC[5].u.operand;
                
                ASSERT(baseObject->offsetForLocation(baseObject->getDirectLocation(callFrame->codeBlock()->identifiers[vPC[2].u.operand])) == offset);
                baseObject->putDirectOffset(offset, callFrame[value].jsValue(callFrame));

                vPC += 8;
                NEXT_OPCODE;
            }
        }

        uncachePutByID(callFrame->codeBlock(), vPC);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id_generic) {
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
        Identifier& ident = callFrame->codeBlock()->identifiers[property];
        PutPropertySlot slot;
        baseValue->put(callFrame, ident, callFrame[value].jsValue(callFrame), slot);
        VM_CHECK_EXCEPTION();

        vPC += 8;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_id) {
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
        JSValuePtr result = jsBoolean(baseObj->deleteProperty(callFrame, ident));
        VM_CHECK_EXCEPTION();
        callFrame[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_val) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_val) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_val) {
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

        JSValuePtr subscript = callFrame[property].jsValue(callFrame);
        JSValuePtr result;
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_index) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop) {
        /* loop target(offset)
         
           Jumps unconditionally to offset target from the current
           instruction.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
#if DUMP_OPCODE_STATS
        OpcodeStats::resetLastInstruction();
#endif
        int target = (++vPC)->u.operand;
        CHECK_FOR_TIMEOUT();
        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp) {
        /* jmp target(offset)

           Jumps unconditionally to offset target from the current
           instruction.
        */
#if DUMP_OPCODE_STATS
        OpcodeStats::resetLastInstruction();
#endif
        int target = (++vPC)->u.operand;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_true) {
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
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (callFrame[cond].jsValue(callFrame)->toBoolean(callFrame)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (!callFrame[cond].jsValue(callFrame)->toBoolean(callFrame)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jeq_null) {
        /* jeq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is null.
        */
        int src = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        JSValuePtr srcValue = callFrame[src].jsValue(callFrame);

        if (srcValue->isUndefinedOrNull() || (!JSImmediate::isImmediate(srcValue) && srcValue->asCell()->structureID()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jneq_null) {
        /* jneq_null src(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register src is not null.
        */
        int src = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        JSValuePtr srcValue = callFrame[src].jsValue(callFrame);

        if (!srcValue->isUndefinedOrNull() || (!JSImmediate::isImmediate(srcValue) && !srcValue->asCell()->structureID()->typeInfo().masqueradesAsUndefined())) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_less) {
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
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_lesseq) {
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
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jnless) {
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
        VM_CHECK_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_imm) {
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
        if (!JSImmediate::isNumber(scrutinee))
            vPC += defaultOffset;
        else {
            int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
            vPC += callFrame->codeBlock()->immediateSwitchJumpTables[tableIndex].offsetForValue(value, defaultOffset);
        }
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_char) {
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
        if (!scrutinee->isString())
            vPC += defaultOffset;
        else {
            UString::Rep* value = asString(scrutinee)->value().rep();
            if (value->size() != 1)
                vPC += defaultOffset;
            else
                vPC += callFrame->codeBlock()->characterSwitchJumpTables[tableIndex].offsetForValue(value->data()[0], defaultOffset);
        }
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_string) {
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
        if (!scrutinee->isString())
            vPC += defaultOffset;
        else 
            vPC += callFrame->codeBlock()->stringSwitchJumpTables[tableIndex].offsetForValue(asString(scrutinee)->value().rep(), defaultOffset);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func_exp) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_call_eval) {
        /* call_eval dst(r) func(r) thisVal(r) firstArg(r) argCount(n)

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
        int thisVal = vPC[3].u.operand;
        int firstArg = vPC[4].u.operand;
        int argCount = vPC[5].u.operand;

        JSValuePtr funcVal = callFrame[func].jsValue(callFrame);
        JSValuePtr baseVal = callFrame[thisVal].jsValue(callFrame);

        ScopeChainNode* scopeChain = callFrame->scopeChain();
        if (baseVal == scopeChain->globalObject() && funcVal == scopeChain->globalObject()->evalFunction()) {
            JSObject* thisObject = asObject(callFrame[callFrame->codeBlock()->thisRegister].jsValue(callFrame));
            JSValuePtr result = callEval(callFrame, thisObject, scopeChain, registerFile, firstArg, argCount, exceptionValue);
            if (exceptionValue)
                goto vm_throw;

            callFrame[dst] = result;

            vPC += 7;
            NEXT_OPCODE;
        }

        // We didn't find the blessed version of eval, so reset vPC and process
        // this instruction as a normal function call, supplying the proper 'this'
        // value.
        callFrame[thisVal] = baseVal->toThisObject(callFrame);

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_call_indirect; // indirect goto -> op_call
#endif
        // fall through to op_call
    }
    BEGIN_OPCODE(op_call) {
        /* call dst(r) func(r) thisVal(r) firstArg(r) argCount(n) registerOffset(n)

           Perform a function call. Specifically, call register func
           with a "this" value of register thisVal, and put the result
           in register dst.

           The arguments start at register firstArg and go up to
           argCount, but the "this" value is considered an implicit
           first argument, so the argCount should be one greater than
           the number of explicit arguments passed, and the register
           after firstArg should contain the actual first
           argument. This opcode will copy from the thisVal register
           to the firstArg register, unless the register index of
           thisVal is the special missing this object marker, which is
           2^31-1; in that case, the global object will be used as the
           "this" value.

           If func is a native code function, then this opcode calls
           it and returns the value immediately. 

           But if it is a JS function, then the current scope chain
           and code block is set to the function's, and we slide the
           register window so that the arguments would form the first
           few local registers of the called function's register
           window. In addition, a call frame header is written
           immediately before the arguments; see the call frame
           documentation for an explanation of how many registers a
           call frame takes and what they contain. That many registers
           before the firstArg register will be overwritten by the
           call. In addition, any registers higher than firstArg +
           argCount may be overwritten. Once this setup is complete,
           execution continues from the called function's first
           argument, and does not return until a "ret" opcode is
           encountered.
         */

        int dst = vPC[1].u.operand;
        int func = vPC[2].u.operand;
        int thisVal = vPC[3].u.operand;
        int firstArg = vPC[4].u.operand;
        int argCount = vPC[5].u.operand;
        int registerOffset = vPC[6].u.operand;

        JSValuePtr v = callFrame[func].jsValue(callFrame);

        CallData callData;
        CallType callType = v->getCallData(callData);

        if (callType == CallTypeJS) {
            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = callData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

            callFrame[firstArg] = thisVal == missingThisObjectMarker() ? callFrame->globalThisValue() : callFrame[thisVal].jsValue(callFrame);
            
            CallFrame* previousCallFrame = callFrame;

            callFrame = slideRegisterWindowForCall(newCodeBlock, registerFile, callFrame, registerOffset, argCount);
            if (UNLIKELY(!callFrame)) {
                callFrame = previousCallFrame;
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }

            callFrame->init(newCodeBlock, vPC + 7, callDataScopeChain, previousCallFrame, dst, argCount, asFunction(v));
            vPC = newCodeBlock->instructions.begin();

#if DUMP_OPCODE_STATS
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_OPCODE;
        }

        if (callType == CallTypeHost) {
            JSValuePtr thisValue = thisVal == missingThisObjectMarker() ? callFrame->globalThisValue() : callFrame[thisVal].jsValue(callFrame);
            ArgList args(callFrame->registers() + firstArg + 1, argCount - 1);

            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 7, scopeChain, callFrame, dst, argCount, 0);

            MACHINE_SAMPLING_callingHostFunction();

            JSValuePtr returnValue = callData.native.function(newCallFrame, asObject(v), thisValue, args);
            VM_CHECK_EXCEPTION();

            callFrame[dst] = returnValue;

            vPC += 7;
            NEXT_OPCODE;
        }

        ASSERT(callType == CallTypeNone);

        exceptionValue = createNotAFunctionError(callFrame, v, vPC, callFrame->codeBlock());
        goto vm_throw;
    }
    BEGIN_OPCODE(op_tear_off_activation) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_tear_off_arguments) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_ret) {
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

        JSValuePtr returnValue = callFrame[result].jsValue(callFrame);

        vPC = callFrame->returnPC();
        int dst = callFrame->returnValueRegister();
        callFrame = callFrame->callerFrame();
        
        if (callFrame->hasHostCallFrameFlag())
            return returnValue;

        callFrame[dst] = returnValue;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_enter) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_enter_with_activation) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_convert_this) {
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
        if (thisVal->needsThisConversion())
            callFrame[thisRegister] = thisVal->toThisObject(callFrame);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_create_arguments) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_construct) {
        /* construct dst(r) constr(r) constrProto(r) firstArg(r) argCount(n) registerOffset(n)

           Invoke register "constr" as a constructor. For JS
           functions, the calling convention is exactly as for the
           "call" opcode, except that the "this" value is a newly
           created Object. For native constructors, a null "this"
           value is passed. In either case, the firstArg and argCount
           registers are interpreted as for the "call" opcode.

           Register constrProto must contain the prototype property of
           register constsr. This is to enable polymorphic inline
           caching of this lookup.
        */

        int dst = vPC[1].u.operand;
        int constr = vPC[2].u.operand;
        int constrProto = vPC[3].u.operand;
        int firstArg = vPC[4].u.operand;
        int argCount = vPC[5].u.operand;
        int registerOffset = vPC[6].u.operand;

        JSValuePtr v = callFrame[constr].jsValue(callFrame);

        ConstructData constructData;
        ConstructType constructType = v->getConstructData(constructData);

        if (constructType == ConstructTypeJS) {
            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

            StructureID* structure;
            JSValuePtr prototype = callFrame[constrProto].jsValue(callFrame);
            if (prototype->isObject())
                structure = asObject(prototype)->inheritorID();
            else
                structure = callDataScopeChain->globalObject()->emptyObjectStructure();
            JSObject* newObject = new (globalData) JSObject(structure);

            callFrame[firstArg] = newObject; // "this" value

            CallFrame* previousCallFrame = callFrame;

            callFrame = slideRegisterWindowForCall(newCodeBlock, registerFile, callFrame, registerOffset, argCount);
            if (UNLIKELY(!callFrame)) {
                callFrame = previousCallFrame;
                exceptionValue = createStackOverflowError(callFrame);
                goto vm_throw;
            }

            callFrame->init(newCodeBlock, vPC + 7, callDataScopeChain, previousCallFrame, dst, argCount, asFunction(v));
            vPC = newCodeBlock->instructions.begin();

#if DUMP_OPCODE_STATS
            OpcodeStats::resetLastInstruction();
#endif

            NEXT_OPCODE;
        }

        if (constructType == ConstructTypeHost) {
            ArgList args(callFrame->registers() + firstArg + 1, argCount - 1);

            ScopeChainNode* scopeChain = callFrame->scopeChain();
            CallFrame* newCallFrame = CallFrame::create(callFrame->registers() + registerOffset);
            newCallFrame->init(0, vPC + 7, scopeChain, callFrame, dst, argCount, 0);

            MACHINE_SAMPLING_callingHostFunction();

            JSValuePtr returnValue = constructData.native.function(newCallFrame, asObject(v), args);

            VM_CHECK_EXCEPTION();
            callFrame[dst] = returnValue;

            vPC += 7;
            NEXT_OPCODE;
        }

        ASSERT(constructType == ConstructTypeNone);

        exceptionValue = createNotAConstructorError(callFrame, v, vPC, callFrame->codeBlock());
        goto vm_throw;
    }
    BEGIN_OPCODE(op_construct_verify) {
        /* construct_verify dst(r) override(r)

           Verifies that register dst holds an object. If not, moves
           the object in register override to register dst.
        */

        int dst = vPC[1].u.operand;;
        if (LIKELY(callFrame[dst].jsValue(callFrame)->isObject())) {
            vPC += 3;
            NEXT_OPCODE;
        }

        int override = vPC[2].u.operand;
        callFrame[dst] = callFrame[override];

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.
        */
        int scope = (++vPC)->u.operand;
        JSValuePtr v = callFrame[scope].jsValue(callFrame);
        JSObject* o = v->toObject(callFrame);
        VM_CHECK_EXCEPTION();

        callFrame->setScopeChain(callFrame->scopeChain()->push(o));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        callFrame->setScopeChain(callFrame->scopeChain()->pop());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_pnames) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_next_pname) {
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
            callFrame[dst] = temp;
            vPC += target;
            NEXT_OPCODE;
        }
        it->invalidate();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp_scopes) {
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
        NEXT_OPCODE;
    }
#if HAVE(COMPUTED_GOTO)
    // Appease GCC
    goto *(&&skip_new_scope);
#endif
    BEGIN_OPCODE(op_push_new_scope) {
        /* new_scope dst(r) property(id) value(r)
         
           Constructs a new StaticScopeObject with property set to value.  That scope
           object is then pushed onto the ScopeChain.  The scope object is then stored
           in dst for GC.
         */
        callFrame->setScopeChain(createExceptionScope(callFrame, vPC));

        vPC += 4;
        NEXT_OPCODE;
    }
#if HAVE(COMPUTED_GOTO)
    skip_new_scope:
#endif
    BEGIN_OPCODE(op_catch) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_throw) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_unexpected_load) {
        /* unexpected_load load dst(r) src(k)

           Copies constant src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        callFrame[dst] = callFrame->codeBlock()->unexpectedConstants[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_error) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_end) {
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
    BEGIN_OPCODE(op_put_getter) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_setter) {
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
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jsr) {
        /* jsr retAddrDst(r) target(offset)

           Places the address of the next instruction into the retAddrDst
           register and jumps to offset target from the current instruction.
        */
        int retAddrDst = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        callFrame[retAddrDst] = vPC + 1;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sret) {
        /* sret retAddrSrc(r)

         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = (++vPC)->u.operand;
        vPC = callFrame[retAddrSrc].vPC();
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_debug) {
        /* debug debugHookID(n) firstLine(n) lastLine(n)

         Notifies the debugger of the current state of execution. This opcode
         is only generated while the debugger is attached.
        */
        int debugHookID = (++vPC)->u.operand;
        int firstLine = (++vPC)->u.operand;
        int lastLine = (++vPC)->u.operand;

        debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_profile_will_call) {
        /* op_profile_will_call function(r)

         Notifies the profiler of the beginning of a function call. This opcode
         is only generated if developer tools are enabled.
        */
        int function = vPC[1].u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->willExecute(callFrame, callFrame[function].jsValue(callFrame));

        vPC += 2;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_profile_did_call) {
        /* op_profile_did_call function(r)

         Notifies the profiler of the end of a function call. This opcode
         is only generated if developer tools are enabled.
        */
        int function = vPC[1].u.operand;

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->didExecute(callFrame, callFrame[function].jsValue(callFrame));

        vPC += 2;
        NEXT_OPCODE;
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
        NEXT_OPCODE;
    }
    }
#if !HAVE(COMPUTED_GOTO)
    } // iterator loop ends
#endif
    #undef NEXT_OPCODE
    #undef BEGIN_OPCODE
    #undef VM_CHECK_EXCEPTION
    #undef CHECK_FOR_TIMEOUT
}

JSValuePtr Machine::retrieveArguments(CallFrame* callFrame, JSFunction* function) const
{
    CallFrame* functionCallFrame = findFunctionCallFrame(callFrame, function);
    if (!functionCallFrame)
        return jsNull();

    CodeBlock* codeBlock = functionCallFrame->codeBlock();
    if (codeBlock->usesArguments) {
        ASSERT(codeBlock->codeType == FunctionCode);
        SymbolTable& symbolTable = static_cast<FunctionBodyNode*>(codeBlock->ownerNode)->symbolTable();
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

JSValuePtr Machine::retrieveCaller(CallFrame* callFrame, InternalFunction* function) const
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

void Machine::retrieveLastCaller(CallFrame* callFrame, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValuePtr& function) const
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

CallFrame* Machine::findFunctionCallFrame(CallFrame* callFrame, InternalFunction* function)
{
    for (CallFrame* candidate = callFrame; candidate; candidate = candidate->callerFrame()->removeHostCallFrameFlag()) {
        if (candidate->callee() == function)
            return candidate;
    }
    return 0;
}

#if ENABLE(CTI)

NEVER_INLINE void Machine::tryCTICachePutByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValuePtr baseValue, const PutPropertySlot& slot)
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
    StructureID* structureID = baseCell->structureID();

    if (structureID->isDictionary()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(returnAddress);
    Instruction* vPC = codeBlock->instructions.begin() + vPCIndex;

    // Cache hit: Specialize instruction and ref StructureIDs.

    // If baseCell != base, then baseCell must be a proxy for another object.
    if (baseCell != slot.base()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_put_by_id_generic));
        return;
    }

    // StructureID transition, cache transition info
    if (slot.type() == PutPropertySlot::NewProperty) {
        vPC[0] = getOpcode(op_put_by_id_transition);
        vPC[4] = structureID->previousID();
        vPC[5] = structureID;
        StructureIDChain* chain = structureID->cachedPrototypeChain();
        if (!chain) {
            chain = cachePrototypeChain(callFrame, structureID);
            if (!chain) {
                // This happens if someone has manually inserted null into the prototype chain
                vPC[0] = getOpcode(op_put_by_id_generic);
                return;
            }
        }
        vPC[6] = chain;
        vPC[7] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);
        CTI::compilePutByIdTransition(this, callFrame, codeBlock, structureID->previousID(), structureID, slot.cachedOffset(), chain, returnAddress);
        return;
    }
    
    vPC[0] = getOpcode(op_put_by_id_replace);
    vPC[4] = structureID;
    vPC[5] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);

#if USE(CTI_REPATCH_PIC)
    UNUSED_PARAM(callFrame);
    CTI::patchPutByIdReplace(codeBlock, structureID, slot.cachedOffset(), returnAddress);
#else
    CTI::compilePutByIdReplace(this, callFrame, codeBlock, structureID, slot.cachedOffset(), returnAddress);
#endif
}

void* Machine::getCTIArrayLengthTrampoline(CallFrame* callFrame, CodeBlock* codeBlock)
{
    if (!m_ctiArrayLengthTrampoline)
        m_ctiArrayLengthTrampoline = CTI::compileArrayLengthTrampoline(this, callFrame, codeBlock);
        
    return m_ctiArrayLengthTrampoline;
}

void* Machine::getCTIStringLengthTrampoline(CallFrame* callFrame, CodeBlock* codeBlock)
{
    if (!m_ctiStringLengthTrampoline)
        m_ctiStringLengthTrampoline = CTI::compileStringLengthTrampoline(this, callFrame, codeBlock);
        
    return m_ctiStringLengthTrampoline;
}

NEVER_INLINE void Machine::tryCTICacheGetByID(CallFrame* callFrame, CodeBlock* codeBlock, void* returnAddress, JSValuePtr baseValue, const Identifier& propertyName, const PropertySlot& slot)
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
        CTI::compilePatchGetArrayLength(this, callFrame, codeBlock, returnAddress);
#else
        ctiRepatchCallByReturnAddress(returnAddress, getCTIArrayLengthTrampoline(callFrame, codeBlock));
#endif
        return;
    }
    if (isJSString(baseValue) && propertyName == callFrame->propertyNames().length) {
        // The tradeoff of compiling an repatched inline string length access routine does not seem
        // to pay off, so we currently only do this for arrays.
        ctiRepatchCallByReturnAddress(returnAddress, getCTIStringLengthTrampoline(callFrame, codeBlock));
        return;
    }

    // Uncacheable: give up.
    if (!slot.isCacheable()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    JSCell* baseCell = asCell(baseValue);
    StructureID* structureID = baseCell->structureID();

    if (structureID->isDictionary()) {
        ctiRepatchCallByReturnAddress(returnAddress, reinterpret_cast<void*>(cti_op_get_by_id_generic));
        return;
    }

    // In the interpreter the last structure is trapped here; in CTI we use the
    // *_second method to achieve a similar (but not quite the same) effect.

    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(returnAddress);
    Instruction* vPC = codeBlock->instructions.begin() + vPCIndex;

    // Cache hit: Specialize instruction and ref StructureIDs.

    if (slot.slotBase() == baseValue) {
        // set this up, so derefStructureIDs can do it's job.
        vPC[0] = getOpcode(op_get_by_id_self);
        vPC[4] = structureID;
        vPC[5] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);
        
#if USE(CTI_REPATCH_PIC)
        CTI::patchGetByIdSelf(codeBlock, structureID, slot.cachedOffset(), returnAddress);
#else
        CTI::compileGetByIdSelf(this, callFrame, codeBlock, structureID, slot.cachedOffset(), returnAddress);
#endif
        return;
    }

    if (slot.slotBase() == structureID->prototypeForLookup(callFrame)) {
        ASSERT(slot.slotBase()->isObject());

        JSObject* slotBaseObject = asObject(slot.slotBase());

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (slotBaseObject->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(slotBaseObject->structureID());
            slotBaseObject->setStructureID(transition.release());
            asObject(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        vPC[0] = getOpcode(op_get_by_id_proto);
        vPC[4] = structureID;
        vPC[5] = slotBaseObject->structureID();
        vPC[6] = slot.cachedOffset();
        codeBlock->refStructureIDs(vPC);

        CTI::compileGetByIdProto(this, callFrame, codeBlock, structureID, slotBaseObject->structureID(), slot.cachedOffset(), returnAddress);
        return;
    }

    size_t count = 0;
    JSObject* o = asObject(baseValue);
    while (slot.slotBase() != o) {
        JSValuePtr v = o->structureID()->prototypeForLookup(callFrame);

        // If we didn't find slotBase in baseValue's prototype chain, then baseValue
        // must be a proxy for another object.

        if (v->isNull()) {
            vPC[0] = getOpcode(op_get_by_id_generic);
            return;
        }

        o = asObject(v);

        // Heavy access to a prototype is a good indication that it's not being
        // used as a dictionary.
        if (o->structureID()->isDictionary()) {
            RefPtr<StructureID> transition = StructureID::fromDictionaryTransition(o->structureID());
            o->setStructureID(transition.release());
            asObject(baseValue)->structureID()->setCachedPrototypeChain(0);
        }

        ++count;
    }

    StructureIDChain* chain = structureID->cachedPrototypeChain();
    if (!chain)
        chain = cachePrototypeChain(callFrame, structureID);

    ASSERT(chain);
    vPC[0] = getOpcode(op_get_by_id_chain);
    vPC[4] = structureID;
    vPC[5] = chain;
    vPC[6] = count;
    vPC[7] = slot.cachedOffset();
    codeBlock->refStructureIDs(vPC);

    CTI::compileGetByIdChain(this, callFrame, codeBlock, structureID, chain, count, slot.cachedOffset(), returnAddress);
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
static NEVER_INLINE void setUpThrowTrampolineReturnAddress(JSGlobalData* globalData, void*& returnAddress)
{
    ASSERT(globalData->exception);
    globalData->throwReturnAddress = returnAddress;
    ctiSetReturnAddress(&returnAddress, reinterpret_cast<void*>(ctiVMThrowTrampoline));
}

#define VM_THROW_EXCEPTION() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        return 0; \
    } while (0)
#define VM_THROW_EXCEPTION_2() \
    do { \
        VM_THROW_EXCEPTION_AT_END(); \
        VoidPtrPair pair = { 0, 0 }; \
        return pair; \
    } while (0)
#define VM_THROW_EXCEPTION_AT_END() \
    setUpThrowTrampolineReturnAddress(ARG_globalData, CTI_RETURN_ADDRESS)

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

JSObject* Machine::cti_op_convert_this(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v1 = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSObject* result = v1->toThisObject(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

void Machine::cti_op_end(CTI_ARGS)
{
    CTI_STACK_HACK();

    ScopeChainNode* scopeChain = ARG_callFrame->scopeChain();
    ASSERT(scopeChain->refCount > 1);
    scopeChain->deref();
}

JSValue* Machine::cti_op_add(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v1 = ARG_src1;
    JSValuePtr v2 = ARG_src2;

    double left;
    double right = 0.0;

    bool rightIsNumber = fastIsNumber(v2, right);
    if (rightIsNumber && fastIsNumber(v1, left))
        return jsNumber(ARG_globalData, left + right).payload();
    
    CallFrame* callFrame = ARG_callFrame;

    bool leftIsString = v1->isString();
    if (leftIsString && v2->isString()) {
        RefPtr<UString::Rep> value = concatenate(asString(v1)->value().rep(), asString(v2)->value().rep());
        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }

        return JSValuePtr(jsString(ARG_globalData, value.release())).payload();
    }

    if (rightIsNumber & leftIsString) {
        RefPtr<UString::Rep> value = JSImmediate::isImmediate(v2) ?
            concatenate(asString(v1)->value().rep(), JSImmediate::getTruncatedInt32(v2)) :
            concatenate(asString(v1)->value().rep(), right);

        if (UNLIKELY(!value)) {
            throwOutOfMemoryError(callFrame);
            VM_THROW_EXCEPTION();
        }
        return JSValuePtr(jsString(ARG_globalData, value.release())).payload();
    }

    // All other cases are pretty uncommon
    JSValuePtr result = jsAddSlowCase(callFrame, v1, v2);
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_pre_inc(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, v->toNumber(callFrame) + 1);
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

void Machine::cti_timeout_check(CTI_ARGS)
{
    CTI_STACK_HACK();

    if (ARG_globalData->machine->checkTimeout(ARG_callFrame->dynamicGlobalObject())) {
        ARG_globalData->exception = createInterruptedExecutionException(ARG_globalData);
        VM_THROW_EXCEPTION_AT_END();
    }
}

NEVER_INLINE void Machine::throwStackOverflowPreviousFrame(CallFrame* callFrame, JSGlobalData* globalData, void*& returnAddress)
{
    globalData->exception = createStackOverflowError(callFrame->callerFrame());
    globalData->throwReturnAddress = callFrame->returnPC();
    ctiSetReturnAddress(&returnAddress, reinterpret_cast<void*>(ctiVMThrowTrampoline));
}

void Machine::cti_register_file_check(CTI_ARGS)
{
    CTI_STACK_HACK();

    if (LIKELY(ARG_registerFile->grow(ARG_callFrame + ARG_callFrame->codeBlock()->numCalleeRegisters)))
        return;

    ARG_setCallFrame(ARG_callFrame->callerFrame());
    throwStackOverflowPreviousFrame(ARG_callFrame, ARG_globalData, CTI_RETURN_ADDRESS);
}

int Machine::cti_op_loop_if_less(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

int Machine::cti_op_loop_if_lesseq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLessEq(callFrame, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSObject* Machine::cti_op_new_object(CTI_ARGS)
{
    CTI_STACK_HACK();

    return constructEmptyObject(ARG_callFrame);
}

void Machine::cti_op_put_by_id(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    PutPropertySlot slot;
    ARG_src1->put(callFrame, ident, ARG_src3, slot);

    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_put_by_id_second));

    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_id_second(CTI_ARGS)
{
    CTI_STACK_HACK();

    PutPropertySlot slot;
    ARG_src1->put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    ARG_globalData->machine->tryCTICachePutByID(ARG_callFrame, ARG_callFrame->codeBlock(), CTI_RETURN_ADDRESS, ARG_src1, slot);
    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_id_generic(CTI_ARGS)
{
    CTI_STACK_HACK();

    PutPropertySlot slot;
    ARG_src1->put(ARG_callFrame, *ARG_id2, ARG_src3, slot);
    VM_CHECK_EXCEPTION_AT_END();
}

void Machine::cti_op_put_by_id_fail(CTI_ARGS)
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

JSValue* Machine::cti_op_get_by_id(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue->get(callFrame, ident, slot);

    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_second));

    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_get_by_id_second(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue->get(callFrame, ident, slot);

    ARG_globalData->machine->tryCTICacheGetByID(callFrame, callFrame->codeBlock(), CTI_RETURN_ADDRESS, baseValue, ident, slot);

    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_get_by_id_generic(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue->get(callFrame, ident, slot);

    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_get_by_id_fail(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;

    JSValuePtr baseValue = ARG_src1;
    PropertySlot slot(baseValue);
    JSValuePtr result = baseValue->get(callFrame, ident, slot);

    // should probably uncacheGetByID() ... this would mean doing a vPC lookup - might be worth just bleeding this until the end.
    ctiRepatchCallByReturnAddress(CTI_RETURN_ADDRESS, reinterpret_cast<void*>(cti_op_get_by_id_generic));

    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_instanceof(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr value = ARG_src1;
    JSValuePtr baseVal = ARG_src2;
    JSValuePtr proto = ARG_src3;

    // at least one of these checks must have failed to get to the slow case
    ASSERT(JSImmediate::isAnyImmediate(value, baseVal, proto) 
           || !value->isObject() || !baseVal->isObject() || !proto->isObject() 
           || (asObject(baseVal)->structureID()->typeInfo().flags() & (ImplementsHasInstance | OverridesHasInstance)) != ImplementsHasInstance);

    if (!baseVal->isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
        unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "instanceof", baseVal, codeBlock->instructions.begin() + vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    if (!asObject(baseVal)->structureID()->typeInfo().implementsHasInstance())
        return JSValuePtr(jsBoolean(false)).payload();

    if (!proto->isObject()) {
        throwError(callFrame, TypeError, "instanceof called on an object with an invalid prototype property.");
        VM_THROW_EXCEPTION();
    }
        
    if (!value->isObject())
        return JSValuePtr(jsBoolean(false)).payload();

    JSValuePtr result = jsBoolean(asObject(baseVal)->hasInstance(callFrame, value, proto));
    VM_CHECK_EXCEPTION_AT_END();

    return result.payload();
}

JSValue* Machine::cti_op_del_by_id(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Identifier& ident = *ARG_id2;
    
    JSObject* baseObj = ARG_src1->toObject(callFrame);

    JSValuePtr result = jsBoolean(baseObj->deleteProperty(callFrame, ident));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_mul(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left * right).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1->toNumber(callFrame) * src2->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSObject* Machine::cti_op_new_func(CTI_ARGS)
{
    CTI_STACK_HACK();

    return ARG_func1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

VoidPtrPair Machine::cti_op_call_JSFunction(CTI_ARGS)
{
    CTI_STACK_HACK();

#ifndef NDEBUG
    CallData callData;
    ASSERT(ARG_src1->getCallData(callData) == CallTypeJS);
#endif

    ScopeChainNode* callDataScopeChain = asFunction(ARG_src1)->m_scopeChain.node();
    CodeBlock* newCodeBlock = &asFunction(ARG_src1)->m_body->byteCode(callDataScopeChain);
    CallFrame* callFrame = ARG_callFrame;
    size_t registerOffset = ARG_int2;
    int argCount = ARG_int3;

    if (LIKELY(argCount == newCodeBlock->numParameters)) {
        VoidPtrPair pair = { newCodeBlock, CallFrame::create(callFrame->registers() + registerOffset) };
        return pair;
    }

    if (argCount > newCodeBlock->numParameters) {
        size_t numParameters = newCodeBlock->numParameters;
        Register* r = callFrame->registers() + registerOffset + numParameters;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argCount;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argCount] = argv[i];

        VoidPtrPair pair = { newCodeBlock, CallFrame::create(r) };
        return pair;
    }

    size_t omittedArgCount = newCodeBlock->numParameters - argCount;
    Register* r = callFrame->registers() + registerOffset + omittedArgCount;
    Register* newEnd = r + newCodeBlock->numCalleeRegisters;
    if (!ARG_registerFile->grow(newEnd)) {
        ARG_globalData->exception = createStackOverflowError(callFrame);
        VM_THROW_EXCEPTION_2();
    }

    Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
    for (size_t i = 0; i < omittedArgCount; ++i)
        argv[i] = jsUndefined();

    VoidPtrPair pair = { newCodeBlock, CallFrame::create(r) };
    return pair;
}

void* Machine::cti_vm_lazyLinkCall(CTI_ARGS)
{
    CTI_STACK_HACK();

    Machine* machine = ARG_globalData->machine;
    CallFrame* callFrame = CallFrame::create(ARG_callFrame);
    CallFrame* callerCallFrame = callFrame->callerFrame();
    CodeBlock* callerCodeBlock = callerCallFrame->codeBlock();

    JSFunction* callee = asFunction(ARG_src1);
    CodeBlock* codeBlock = &callee->m_body->byteCode(callee->m_scopeChain.node());
    if (!codeBlock->ctiCode)
        CTI::compile(machine, callFrame, codeBlock);

    int argCount = ARG_int3;
    CTI::linkCall(callerCodeBlock, callee, codeBlock, codeBlock->ctiCode, CTI_RETURN_ADDRESS, argCount);

    return codeBlock->ctiCode;
}

void* Machine::cti_vm_compile(CTI_ARGS)
{
    CTI_STACK_HACK();

    CodeBlock* codeBlock = ARG_callFrame->codeBlock();
    if (!codeBlock->ctiCode)
        CTI::compile(ARG_globalData->machine, ARG_callFrame, codeBlock);
    return codeBlock->ctiCode;
}

JSObject* Machine::cti_op_push_activation(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSActivation* activation = new (ARG_globalData) JSActivation(ARG_callFrame, static_cast<FunctionBodyNode*>(ARG_callFrame->codeBlock()->ownerNode));
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->copy()->push(activation));
    return activation;
}

JSValue* Machine::cti_op_call_NotJSFunction(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr funcVal = ARG_src1;

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

        CTI_MACHINE_SAMPLING_callingHostFunction();

        JSValuePtr returnValue = callData.native.function(callFrame, asObject(funcVal), argv[0].jsValue(callFrame), argList);
        ARG_setCallFrame(previousCallFrame);
        VM_CHECK_EXCEPTION();

        return returnValue.payload();
    }

    ASSERT(callType == CallTypeNone);

    ARG_globalData->exception = createNotAFunctionError(ARG_callFrame, funcVal, ARG_instr4, ARG_callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

void Machine::cti_op_create_arguments(CTI_ARGS)
{
    CTI_STACK_HACK();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void Machine::cti_op_create_arguments_no_params(CTI_ARGS)
{
    CTI_STACK_HACK();

    Arguments* arguments = new (ARG_globalData) Arguments(ARG_callFrame, Arguments::ArgumentsNoParameters);
    ARG_callFrame->setCalleeArguments(arguments);
    ARG_callFrame[RegisterFile::ArgumentsRegister] = arguments;
}

void Machine::cti_op_tear_off_activation(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain);
    asActivation(ARG_src1)->copyRegisters(ARG_callFrame->optionalCalleeArguments());
}

void Machine::cti_op_tear_off_arguments(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(ARG_callFrame->codeBlock()->usesArguments && !ARG_callFrame->codeBlock()->needsFullScopeChain);
    ARG_callFrame->optionalCalleeArguments()->copyRegisters();
}

void Machine::cti_op_profile_will_call(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->willExecute(ARG_callFrame, ARG_src1);
}

void Machine::cti_op_profile_did_call(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(*ARG_profilerReference);
    (*ARG_profilerReference)->didExecute(ARG_callFrame, ARG_src1);
}

void Machine::cti_op_ret_scopeChain(CTI_ARGS)
{
    CTI_STACK_HACK();

    ASSERT(ARG_callFrame->codeBlock()->needsFullScopeChain);
    ARG_callFrame->scopeChain()->deref();
}

JSObject* Machine::cti_op_new_array(CTI_ARGS)
{
    CTI_STACK_HACK();

    ArgList argList(ARG_registers1, ARG_int2);
    return constructArray(ARG_callFrame, argList);
}

JSValue* Machine::cti_op_resolve(CTI_ARGS)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();
            return result.payload();
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSObject* Machine::cti_op_construct_JSConstructFast(CTI_ARGS)
{
    CTI_STACK_HACK();

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(asFunction(ARG_src1)->getConstructData(constructData) == ConstructTypeJS);
#endif

    StructureID* structure;
    if (ARG_src2->isObject())
        structure = asObject(ARG_src2)->inheritorID();
    else
        structure = asFunction(ARG_src1)->m_scopeChain.node()->globalObject()->emptyObjectStructure();
    return new (ARG_globalData) JSObject(structure);
}

VoidPtrPair Machine::cti_op_construct_JSConstruct(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSFunction* constructor = asFunction(ARG_src1);
    int registerOffset = ARG_int2;
    int argCount = ARG_int3;
    JSValuePtr constrProtoVal = ARG_src5;
    int firstArg = ARG_int6;

#ifndef NDEBUG
    ConstructData constructData;
    ASSERT(constructor->getConstructData(constructData) == ConstructTypeJS);
#endif

    ScopeChainNode* callDataScopeChain = constructor->m_scopeChain.node();
    FunctionBodyNode* functionBodyNode = constructor->m_body.get();
    CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

    StructureID* structure;
    if (constrProtoVal->isObject())
        structure = asObject(constrProtoVal)->inheritorID();
    else
        structure = callDataScopeChain->globalObject()->emptyObjectStructure();
    JSObject* newObject = new (ARG_globalData) JSObject(structure);
    callFrame[firstArg] = newObject; // "this" value

    if (LIKELY(argCount == newCodeBlock->numParameters)) {
        VoidPtrPair pair = { newCodeBlock, CallFrame::create(callFrame->registers() + registerOffset) };
        return pair;
    }

    if (argCount > newCodeBlock->numParameters) {
        size_t numParameters = newCodeBlock->numParameters;
        Register* r = callFrame->registers() + registerOffset + numParameters;

        Register* argv = r - RegisterFile::CallFrameHeaderSize - numParameters - argCount;
        for (size_t i = 0; i < numParameters; ++i)
            argv[i + argCount] = argv[i];

        VoidPtrPair pair = { newCodeBlock, CallFrame::create(r) };
        return pair;
    }

    size_t omittedArgCount = newCodeBlock->numParameters - argCount;
    Register* r = callFrame->registers() + registerOffset + omittedArgCount;
    Register* newEnd = r + newCodeBlock->numCalleeRegisters;
    if (!ARG_registerFile->grow(newEnd)) {
        ARG_globalData->exception = createStackOverflowError(callFrame);
        VM_THROW_EXCEPTION_2();
    }

    Register* argv = r - RegisterFile::CallFrameHeaderSize - omittedArgCount;
    for (size_t i = 0; i < omittedArgCount; ++i)
        argv[i] = jsUndefined();

    VoidPtrPair pair = { newCodeBlock, CallFrame::create(r) };
    return pair;
}

JSValue* Machine::cti_op_construct_NotJSConstruct(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr constrVal = ARG_src1;
    int argCount = ARG_int3;
    int firstArg = ARG_int6;

    ConstructData constructData;
    ConstructType constructType = constrVal->getConstructData(constructData);

    if (constructType == ConstructTypeHost) {
        ArgList argList(callFrame->registers() + firstArg + 1, argCount - 1);

        CTI_MACHINE_SAMPLING_callingHostFunction();

        JSValuePtr returnValue = constructData.native.function(callFrame, asObject(constrVal), argList);
        VM_CHECK_EXCEPTION();

        return returnValue.payload();
    }

    ASSERT(constructType == ConstructTypeNone);

    ARG_globalData->exception = createNotAConstructorError(callFrame, constrVal, ARG_instr4, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_get_by_val(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Machine* machine = ARG_globalData->machine;

    JSValuePtr baseValue = ARG_src1;
    JSValuePtr subscript = ARG_src2;

    JSValuePtr result;
    unsigned i;

    bool isUInt32 = JSImmediate::getUInt32(subscript, i);
    if (LIKELY(isUInt32)) {
        if (machine->isJSArray(baseValue)) {
            JSArray* jsArray = asArray(baseValue);
            if (jsArray->canGetIndex(i))
                result = jsArray->getIndex(i);
            else
                result = jsArray->JSArray::get(callFrame, i);
        } else if (machine->isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            result = asString(baseValue)->getIndex(ARG_globalData, i);
        else
            result = baseValue->get(callFrame, i);
    } else {
        Identifier property(callFrame, subscript->toString(callFrame));
        result = baseValue->get(callFrame, property);
    }

    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

VoidPtrPair Machine::cti_op_resolve_func(CTI_ARGS)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();

            VoidPtrPair pair = { thisObj, asPointer(result) };
            return pair;
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSValue* Machine::cti_op_sub(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left - right).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1->toNumber(callFrame) - src2->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

void Machine::cti_op_put_by_val(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    Machine* machine = ARG_globalData->machine;

    JSValuePtr baseValue = ARG_src1;
    JSValuePtr subscript = ARG_src2;
    JSValuePtr value = ARG_src3;

    unsigned i;

    bool isUInt32 = JSImmediate::getUInt32(subscript, i);
    if (LIKELY(isUInt32)) {
        if (machine->isJSArray(baseValue)) {
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

void Machine::cti_op_put_by_val_array(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr baseValue = ARG_src1;
    int i = ARG_int2;
    JSValuePtr value = ARG_src3;

    ASSERT(ARG_globalData->machine->isJSArray(baseValue));

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

JSValue* Machine::cti_op_lesseq(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsBoolean(jsLessEq(callFrame, ARG_src1, ARG_src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

int Machine::cti_op_loop_if_true(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1->toBoolean(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_negate(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src = ARG_src1;

    double v;
    if (fastIsNumber(src, v))
        return jsNumber(ARG_globalData, -v).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, -src->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_resolve_base(CTI_ARGS)
{
    CTI_STACK_HACK();

    return inlineResolveBase(ARG_callFrame, *ARG_id1, ARG_callFrame->scopeChain()).payload();
}

JSValue* Machine::cti_op_resolve_skip(CTI_ARGS)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();
            return result.payload();
        }
    } while (++iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_resolve_global(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSGlobalObject* globalObject = asGlobalObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    Instruction* vPC = ARG_instr3;
    ASSERT(globalObject->isGlobalObject());

    PropertySlot slot(globalObject);
    if (globalObject->getPropertySlot(callFrame, ident, slot)) {
        JSValuePtr result = slot.getValue(callFrame, ident);
        if (slot.isCacheable()) {
            if (vPC[4].u.structureID)
                vPC[4].u.structureID->deref();
            globalObject->structureID()->ref();
            vPC[4] = globalObject->structureID();
            vPC[5] = slot.cachedOffset();
            return result.payload();
        }

        VM_CHECK_EXCEPTION_AT_END();
        return result.payload();
    }
    
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, vPC, callFrame->codeBlock());
    VM_THROW_EXCEPTION();
}

JSValue* Machine::cti_op_div(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    double left;
    double right;
    if (fastIsNumber(src1, left) && fastIsNumber(src2, right))
        return jsNumber(ARG_globalData, left / right).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1->toNumber(callFrame) / src2->toNumber(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_pre_dec(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, v->toNumber(callFrame) - 1);
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

int Machine::cti_op_jless(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;
    CallFrame* callFrame = ARG_callFrame;

    bool result = jsLess(callFrame, src1, src2);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

JSValue* Machine::cti_op_not(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = jsBoolean(!src->toBoolean(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

int SFX_CALL Machine::cti_op_jtrue(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    bool result = src1->toBoolean(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result;
}

VoidPtrPair Machine::cti_op_post_inc(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr number = v->toJSNumber(callFrame);
    VM_CHECK_EXCEPTION_AT_END();

    VoidPtrPair pair = { asPointer(number), asPointer(jsNumber(ARG_globalData, number->uncheckedGetNumber() + 1)) };
    return pair;
}

JSValue* Machine::cti_op_eq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(!JSImmediate::areBothImmediateNumbers(src1, src2));
    JSValuePtr result = jsBoolean(equalSlowCaseInline(callFrame, src1, src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_lshift(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr val = ARG_src1;
    JSValuePtr shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSImmediate::areBothImmediateNumbers(val, shift))
        return jsNumber(ARG_globalData, JSImmediate::getTruncatedInt32(val) << (JSImmediate::getTruncatedUInt32(shift) & 0x1f)).payload();
    if (fastToInt32(val, left) && fastToUInt32(shift, right))
        return jsNumber(ARG_globalData, left << (right & 0x1f)).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, (val->toInt32(callFrame)) << (shift->toUInt32(callFrame) & 0x1f));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_bitand(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    int32_t left;
    int32_t right;
    if (fastToInt32(src1, left) && fastToInt32(src2, right))
        return jsNumber(ARG_globalData, left & right).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, src1->toInt32(callFrame) & src2->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_rshift(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr val = ARG_src1;
    JSValuePtr shift = ARG_src2;

    int32_t left;
    uint32_t right;
    if (JSImmediate::areBothImmediateNumbers(val, shift))
        return JSImmediate::rightShiftImmediateNumbers(val, shift).payload();
    if (fastToInt32(val, left) && fastToUInt32(shift, right))
        return jsNumber(ARG_globalData, left >> (right & 0x1f)).payload();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, (val->toInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_bitnot(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src = ARG_src1;

    int value;
    if (fastToInt32(src, value))
        return jsNumber(ARG_globalData, ~value).payload();
            
    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsNumber(ARG_globalData, ~src->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

VoidPtrPair Machine::cti_op_resolve_with_base(CTI_ARGS)
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
            JSValuePtr result = slot.getValue(callFrame, ident);
            VM_CHECK_EXCEPTION_AT_END();

            VoidPtrPair pair = { base, asPointer(result) };
            return pair;
        }
        ++iter;
    } while (iter != end);

    CodeBlock* codeBlock = callFrame->codeBlock();
    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
    ARG_globalData->exception = createUndefinedVariableError(callFrame, ident, codeBlock->instructions.begin() + vPCIndex, codeBlock);
    VM_THROW_EXCEPTION_2();
}

JSObject* Machine::cti_op_new_func_exp(CTI_ARGS)
{
    CTI_STACK_HACK();

    return ARG_funcexp1->makeFunction(ARG_callFrame, ARG_callFrame->scopeChain());
}

JSValue* Machine::cti_op_mod(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr dividendValue = ARG_src1;
    JSValuePtr divisorValue = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;
    double d = dividendValue->toNumber(callFrame);
    JSValuePtr result = jsNumber(ARG_globalData, fmod(d, divisorValue->toNumber(callFrame)));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_less(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsBoolean(jsLess(callFrame, ARG_src1, ARG_src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_neq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    ASSERT(!JSImmediate::areBothImmediateNumbers(src1, src2));

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr result = jsBoolean(!equalSlowCaseInline(callFrame, src1, src2));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

VoidPtrPair Machine::cti_op_post_dec(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v = ARG_src1;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr number = v->toJSNumber(callFrame);
    VM_CHECK_EXCEPTION_AT_END();

    VoidPtrPair pair = { asPointer(number), asPointer(jsNumber(ARG_globalData, number->uncheckedGetNumber() - 1)) };
    return pair;
}

JSValue* Machine::cti_op_urshift(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr val = ARG_src1;
    JSValuePtr shift = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
        return JSImmediate::rightShiftImmediateNumbers(val, shift).payload();
    else {
        JSValuePtr result = jsNumber(ARG_globalData, (val->toUInt32(callFrame)) >> (shift->toUInt32(callFrame) & 0x1f));
        VM_CHECK_EXCEPTION_AT_END();
        return result.payload();
    }
}

JSValue* Machine::cti_op_bitxor(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = jsNumber(ARG_globalData, src1->toInt32(callFrame) ^ src2->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSObject* Machine::cti_op_new_regexp(CTI_ARGS)
{
    CTI_STACK_HACK();

    return new (ARG_globalData) RegExpObject(ARG_callFrame->lexicalGlobalObject()->regExpStructure(), ARG_regexp1);
}

JSValue* Machine::cti_op_bitor(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = jsNumber(ARG_globalData, src1->toInt32(callFrame) | src2->toInt32(callFrame));
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_call_eval(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    RegisterFile* registerFile = ARG_registerFile;
    CodeBlock* codeBlock = callFrame->codeBlock();
    ScopeChainNode* scopeChain = callFrame->scopeChain();

    Machine* machine = ARG_globalData->machine;
    
    JSValuePtr funcVal = ARG_src1;
    int registerOffset = ARG_int2;
    int argCount = ARG_int3;
    JSValuePtr baseVal = ARG_src5;

    if (baseVal == scopeChain->globalObject() && funcVal == scopeChain->globalObject()->evalFunction()) {
        JSObject* thisObject = asObject(callFrame[codeBlock->thisRegister].jsValue(callFrame));
        JSValuePtr exceptionValue = noValue();
        JSValuePtr result = machine->callEval(callFrame, thisObject, scopeChain, registerFile, registerOffset - RegisterFile::CallFrameHeaderSize - argCount, argCount, exceptionValue);
        if (UNLIKELY(exceptionValue != noValue())) {
            ARG_globalData->exception = exceptionValue;
            VM_THROW_EXCEPTION_AT_END();
        }
        return result.payload();
    }

    return JSImmediate::impossibleValue().payload();
}

JSValue* Machine::cti_op_throw(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);

    JSValuePtr exceptionValue = ARG_src1;
    ASSERT(exceptionValue);

    Instruction* handlerVPC = ARG_globalData->machine->throwException(callFrame, exceptionValue, codeBlock->instructions.begin() + vPCIndex, true);

    if (!handlerVPC) {
        *ARG_exception = exceptionValue;
        return JSImmediate::nullImmediate().payload();
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = callFrame->codeBlock()->nativeExceptionCodeForHandlerVPC(handlerVPC);
    ASSERT(catchRoutine);
    CTI_SET_RETURN_ADDRESS(catchRoutine);
    return exceptionValue.payload();
}

JSPropertyNameIterator* Machine::cti_op_get_pnames(CTI_ARGS)
{
    CTI_STACK_HACK();

    return JSPropertyNameIterator::create(ARG_callFrame, ARG_src1);
}

JSValue* Machine::cti_op_next_pname(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSPropertyNameIterator* it = ARG_pni1;
    JSValuePtr temp = it->next(ARG_callFrame);
    if (!temp)
        it->invalidate();
    return temp.payload();
}

void Machine::cti_op_push_scope(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSObject* o = ARG_src1->toObject(ARG_callFrame);
    VM_CHECK_EXCEPTION_VOID();
    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->push(o));
}

void Machine::cti_op_pop_scope(CTI_ARGS)
{
    CTI_STACK_HACK();

    ARG_callFrame->setScopeChain(ARG_callFrame->scopeChain()->pop());
}

JSValue* Machine::cti_op_typeof(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsTypeStringForValue(ARG_callFrame, ARG_src1).payload();
}

JSValue* Machine::cti_op_is_undefined(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr v = ARG_src1;
    return jsBoolean(JSImmediate::isImmediate(v) ? v->isUndefined() : v->asCell()->structureID()->typeInfo().masqueradesAsUndefined()).payload();
}

JSValue* Machine::cti_op_is_boolean(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(ARG_src1->isBoolean()).payload();
}

JSValue* Machine::cti_op_is_number(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(ARG_src1->isNumber()).payload();
}

JSValue* Machine::cti_op_is_string(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(ARG_globalData->machine->isJSString(ARG_src1)).payload();
}

JSValue* Machine::cti_op_is_object(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(jsIsObjectType(ARG_src1)).payload();
}

JSValue* Machine::cti_op_is_function(CTI_ARGS)
{
    CTI_STACK_HACK();

    return jsBoolean(jsIsFunctionType(ARG_src1)).payload();
}

JSValue* Machine::cti_op_stricteq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    // handled inline as fast cases
    ASSERT(!JSImmediate::areBothImmediate(src1, src2));
    ASSERT(!(JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate())));

    return jsBoolean(strictEqualSlowCaseInline(src1, src2)).payload();
}

JSValue* Machine::cti_op_nstricteq(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src1 = ARG_src1;
    JSValuePtr src2 = ARG_src2;

    // handled inline as fast cases
    ASSERT(!JSImmediate::areBothImmediate(src1, src2));
    ASSERT(!(JSImmediate::isEitherImmediate(src1, src2) & (src1 != JSImmediate::zeroImmediate()) & (src2 != JSImmediate::zeroImmediate())));
    
    return jsBoolean(!strictEqualSlowCaseInline(src1, src2)).payload();
}

JSValue* Machine::cti_op_to_jsnumber(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr src = ARG_src1;
    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr result = src->toJSNumber(callFrame);
    VM_CHECK_EXCEPTION_AT_END();
    return result.payload();
}

JSValue* Machine::cti_op_in(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    JSValuePtr baseVal = ARG_src2;

    if (!baseVal->isObject()) {
        CallFrame* callFrame = ARG_callFrame;
        CodeBlock* codeBlock = callFrame->codeBlock();
        ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(CTI_RETURN_ADDRESS));
        unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(CTI_RETURN_ADDRESS);
        ARG_globalData->exception = createInvalidParamError(callFrame, "in", baseVal, codeBlock->instructions.begin() + vPCIndex, codeBlock);
        VM_THROW_EXCEPTION();
    }

    JSValuePtr propName = ARG_src1;
    JSObject* baseObj = asObject(baseVal);

    uint32_t i;
    if (propName->getUInt32(i))
        return jsBoolean(baseObj->hasProperty(callFrame, i)).payload();

    Identifier property(callFrame, propName->toString(callFrame));
    VM_CHECK_EXCEPTION();
    return jsBoolean(baseObj->hasProperty(callFrame, property)).payload();
}

JSObject* Machine::cti_op_push_new_scope(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSObject* scope = new (ARG_globalData) JSStaticScopeObject(ARG_callFrame, *ARG_id1, ARG_src2, DontDelete);

    CallFrame* callFrame = ARG_callFrame;
    callFrame->setScopeChain(callFrame->scopeChain()->push(scope));
    return scope;
}

void Machine::cti_op_jmp_scopes(CTI_ARGS)
{
    CTI_STACK_HACK();

    unsigned count = ARG_int1;
    CallFrame* callFrame = ARG_callFrame;

    ScopeChainNode* tmp = callFrame->scopeChain();
    while (count--)
        tmp = tmp->pop();
    callFrame->setScopeChain(tmp);
}

void Machine::cti_op_put_by_index(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    unsigned property = ARG_int2;

    ARG_src1->put(callFrame, property, ARG_src3);
}

void* Machine::cti_op_switch_imm(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr scrutinee = ARG_src1;
    unsigned tableIndex = ARG_int2;
    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    if (JSImmediate::isNumber(scrutinee)) {
        int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
        return codeBlock->immediateSwitchJumpTables[tableIndex].ctiForValue(value);
    }

    return codeBlock->immediateSwitchJumpTables[tableIndex].ctiDefault;
}

void* Machine::cti_op_switch_char(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr scrutinee = ARG_src1;
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

void* Machine::cti_op_switch_string(CTI_ARGS)
{
    CTI_STACK_HACK();

    JSValuePtr scrutinee = ARG_src1;
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

JSValue* Machine::cti_op_del_by_val(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    JSValuePtr baseValue = ARG_src1;
    JSObject* baseObj = baseValue->toObject(callFrame); // may throw

    JSValuePtr subscript = ARG_src2;
    JSValuePtr result;
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
    return result.payload();
}

void Machine::cti_op_put_getter(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1->isObject());
    JSObject* baseObj = asObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    ASSERT(ARG_src3->isObject());
    baseObj->defineGetter(callFrame, ident, asObject(ARG_src3));
}

void Machine::cti_op_put_setter(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    ASSERT(ARG_src1->isObject());
    JSObject* baseObj = asObject(ARG_src1);
    Identifier& ident = *ARG_id2;
    ASSERT(ARG_src3->isObject());
    baseObj->defineSetter(callFrame, ident, asObject(ARG_src3));
}

JSObject* Machine::cti_op_new_error(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();
    unsigned type = ARG_int1;
    JSValuePtr message = ARG_src2;
    unsigned lineNumber = ARG_int3;

    return Error::create(callFrame, static_cast<ErrorType>(type), message->toString(callFrame), lineNumber, codeBlock->ownerNode->sourceID(), codeBlock->ownerNode->sourceURL());
}

void Machine::cti_op_debug(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;

    int debugHookID = ARG_int1;
    int firstLine = ARG_int2;
    int lastLine = ARG_int3;

    ARG_globalData->machine->debug(callFrame, static_cast<DebugHookID>(debugHookID), firstLine, lastLine);
}

JSValue* Machine::cti_vm_throw(CTI_ARGS)
{
    CTI_STACK_HACK();

    CallFrame* callFrame = ARG_callFrame;
    CodeBlock* codeBlock = callFrame->codeBlock();

    ASSERT(codeBlock->ctiReturnAddressVPCMap.contains(ARG_globalData->throwReturnAddress));
    unsigned vPCIndex = codeBlock->ctiReturnAddressVPCMap.get(ARG_globalData->throwReturnAddress);

    JSValuePtr exceptionValue = ARG_globalData->exception;
    ASSERT(exceptionValue);
    ARG_globalData->exception = noValue();

    Instruction* handlerVPC = ARG_globalData->machine->throwException(callFrame, exceptionValue, codeBlock->instructions.begin() + vPCIndex, false);

    if (!handlerVPC) {
        *ARG_exception = exceptionValue;
        return JSImmediate::nullImmediate().payload();
    }

    ARG_setCallFrame(callFrame);
    void* catchRoutine = callFrame->codeBlock()->nativeExceptionCodeForHandlerVPC(handlerVPC);
    ASSERT(catchRoutine);
    CTI_SET_RETURN_ADDRESS(catchRoutine);
    return exceptionValue.payload();
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

#endif // ENABLE(CTI)

} // namespace JSC
